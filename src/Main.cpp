/*
    MAO Quack — un petit logiciel de musique pour débuter.
*/

#include "Commun.h"
#include "BarreTransport.h"
#include "VueArrangement.h"
#include "Panneaux.h"
#include "Instruments.h"
#include "Effets.h"
#include "PanneauBas.h"
#include "Export.h"

static const char* const extensionMorceau = ".quack";

//==============================================================================
/** Un morceau tout neuf, avec quatre pistes et un petit rythme. */
static std::unique_ptr<te::Edit> creerNouveauMorceau (te::Engine& e)
{
    auto fichier = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("MAOQuack")
                       .getChildFile ("sans_titre.tracktionedit");
    fichier.getParentDirectory().createDirectory();
    fichier.deleteFile();

    auto ed = te::createEmptyEdit (e, fichier);

    const Instruments::Type types[] = { Instruments::Type::batterie, Instruments::Type::basse,
                                        Instruments::Type::melodie, Instruments::Type::accords };
    ed->ensureNumberOfAudioTracks (4);
    auto pistes = te::getAudioTracks (*ed);

    for (int i = 0; i < juce::jmin (4, pistes.size()); ++i)
    {
        pistes[i]->setName (Instruments::nomDe (types[i]));
        pistes[i]->setColour (Couleurs::dePiste (i));
        Instruments::installer (*pistes[i], types[i]);
    }

    if (! pistes.isEmpty())
    {
        // Un petit rythme pour ne pas partir d'une page blanche
        const te::BeatRange mesure (te::BeatPosition::fromBeats (0.0), te::BeatPosition::fromBeats (4.0));

        if (auto clip = pistes[0]->insertMIDIClip ("Rythme", ed->tempoSequence.toTime (mesure), nullptr))
        {
            auto& seq = clip->getSequence();
            const int gc = Instruments::premiereNoteBatterie, cc = gc + 1, ch = gc + 2;
            auto poser = [&seq] (int note, double temps)
            {
                seq.addNote (note, te::BeatPosition::fromBeats (temps), te::BeatDuration::fromBeats (0.25), 100, 0, nullptr);
            };

            for (double t : { 0.0, 2.0, 2.5 })   poser (gc, t);
            for (double t : { 1.0, 3.0 })        poser (cc, t);
            for (int i = 0; i < 8; ++i)          poser (ch, i * 0.5);
        }
    }

    ed->tempoSequence.getTempo (0)->setBpm (120.0);
    ed->clickTrackEnabled = false;
    ed->getUndoManager().clearUndoHistory();
    ed->resetChangedStatus();

    return ed;
}

//==============================================================================
/** Tout le contenu de la fenêtre, pour un morceau donné. */
class ComposantPrincipal : public juce::Component,
                           public juce::DragAndDropContainer,
                           public juce::MenuBarModel
{
public:
    ComposantPrincipal (te::Engine& e, std::unique_ptr<te::Edit> morceau, const juce::File& fichierDuMorceau)
        : engine (e),
          edit (std::move (morceau)),
          fichier (fichierDuMorceau),
          selection (*edit),
          transport (*edit),
          arrangement (*edit, selection),
          bas (*edit, selection)
    {
        addAndMakeVisible (barreMenus);
        addAndMakeVisible (transport);
        addAndMakeVisible (navigateur);
        addAndMakeVisible (arrangement);
        addAndMakeVisible (aide);
        addAndMakeVisible (bas);

        navigateur.surChoix = [this] (const juce::String& id)
        {
            if (auto* piste = selection.pisteValide())
            {
                arrangement.appliquerSurPiste (*piste, id);
                bas.montrer (1);
            }
        };

        edit->getTransport().ensureContextAllocated();

        setWantsKeyboardFocus (true);
        setSize (1280, 800);
    }

    ~ComposantPrincipal() override
    {
        edit->getTransport().stop (false, true);
    }

    /** Appelé quand il faut afficher un autre morceau (nouveau ou ouvert). */
    std::function<void (std::shared_ptr<std::unique_ptr<te::Edit>>, juce::File)> surRemplacer;

    juce::String nomDuMorceau() const
    {
        return fichier == juce::File() ? fr ("Nouveau morceau") : fichier.getFileNameWithoutExtension();
    }

    /** Si le morceau a changé, propose de l'enregistrer, puis continue. */
    void verifierPuis (std::function<void()> suite)
    {
        if (! edit->hasChangedSinceSaved())
        {
            suite();
            return;
        }

        juce::Component::SafePointer<ComposantPrincipal> moi (this);

        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle (fr ("Enregistrer le morceau ?"))
                .withMessage (fr ("Tu as fait des changements dans « ") + nomDuMorceau()
                                  + fr (" ». Veux-tu les enregistrer ?"))
                .withButton ("Enregistrer", 1)
                .withButton ("Ne pas enregistrer", 2)
                .withButton ("Annuler", 0)
                .withAssociatedComponent (this),
            [moi, suite] (int reponse)
            {
                if (moi == nullptr || reponse == 0)
                    return;

                if (reponse == 1)
                    moi->enregistrer (false, suite);
                else
                    suite();
            });
    }

    //==========================================================================
    void paint (juce::Graphics& g) override     { g.fillAll (Couleurs::bordure); }

    void resized() override
    {
        auto r = getLocalBounds();

        barreMenus.setBounds (r.removeFromTop (24));
        transport.setBounds (r.removeFromTop (38));

        auto zoneBas = r.removeFromBottom (juce::jlimit (260, 346, (int) (getHeight() * 0.44)));
        aide.setBounds (zoneBas.removeFromLeft (230));
        bas.setBounds (zoneBas);

        navigateur.setBounds (r.removeFromLeft (230));
        arrangement.setBounds (r);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::spaceKey)
        {
            transport.basculerLecture();
            return true;
        }

        if (k == juce::KeyPress::tabKey)
        {
            bas.basculer();
            return true;
        }

        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
        {
            // (Le piano roll traite lui-même ses notes quand on travaille dedans.)
            if (! bas.getPianoRoll().hasKeyboardFocus (true))
                arrangement.supprimerClipChoisi();
            return true;
        }

        const auto mods = k.getModifiers();
        if (! mods.isCommandDown())
            return false;

        const bool dansPianoRoll = bas.getPianoRoll().hasKeyboardFocus (true);

        switch (k.getKeyCode())
        {
            case 'S':   enregistrer (mods.isShiftDown(), {});                   return true;
            case 'O':   ouvrir();                                               return true;
            case 'N':   nouveau();                                              return true;
            case 'E':   exporter();                                             return true;
            case 'D':   arrangement.dupliquerClipChoisi();                      return true;
            case 'C':   if (! dansPianoRoll) arrangement.copierClipChoisi();    return true;
            case 'V':   if (! dansPianoRoll) arrangement.collerClip();          return true;
            case 'Y':   edit->getUndoManager().redo();                          return true;
            case 'Z':
                if (mods.isShiftDown())   edit->getUndoManager().redo();
                else                      edit->getUndoManager().undo();
                return true;
            default:    break;
        }

        return false;
    }

    void mouseDown (const juce::MouseEvent&) override   { grabKeyboardFocus(); }

private:
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;
    juce::File fichier;
    Selection selection;

    juce::MenuBarComponent barreMenus { this };
    BarreTransport transport;
    Navigateur navigateur;
    VueArrangement arrangement;
    ZoneAide aide;
    PanneauBas bas;

    std::shared_ptr<juce::FileChooser> selecteur;

    //==========================================================================
    static juce::File dossierMorceaux()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("MAO Quack");
        d.createDirectory();
        return d;
    }

public:
    //==========================================================================
    // La barre de menus

    enum Commande
    {
        cmdNouveau = 1, cmdOuvrir, cmdEnregistrer, cmdEnregistrerSous, cmdExporter, cmdQuitter,
        cmdAnnuler = 20, cmdRetablir, cmdDupliquerClip, cmdSupprimerClip, cmdCopierClip, cmdCollerClip,
        cmdVoirNotes = 30, cmdVoirInstrument,
        cmdReglagesAudio = 40,
        cmdRaccourcis = 50, cmdJournal, cmdAPropos
    };

    juce::StringArray getMenuBarNames() override
    {
        return { "Fichier", fr ("Édition"), "Affichage", "Options", "Aide" };
    }

    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override
    {
        juce::PopupMenu m;
        auto ajouter = [&m] (int id, const juce::String& texte, const juce::String& raccourci = {}, bool actif = true)
        {
            juce::PopupMenu::Item item (texte);
            item.itemID = id;
            item.shortcutKeyDescription = raccourci;
            item.isEnabled = actif;
            m.addItem (item);
        };

        switch (index)
        {
            case 0:
                ajouter (cmdNouveau, fr ("Nouveau morceau"), "Ctrl+N");
                ajouter (cmdOuvrir, fr ("Ouvrir..."), "Ctrl+O");
                m.addSeparator();
                ajouter (cmdEnregistrer, fr ("Enregistrer"), "Ctrl+S");
                ajouter (cmdEnregistrerSous, fr ("Enregistrer sous..."), "Ctrl+Maj+S");
                m.addSeparator();
                ajouter (cmdExporter, fr ("Exporter (WAV, MP3, MIDI)..."), "Ctrl+E");
                m.addSeparator();
                ajouter (cmdQuitter, fr ("Quitter"));
                break;

            case 1:
                ajouter (cmdAnnuler, fr ("Annuler"), "Ctrl+Z", edit->getUndoManager().canUndo());
                ajouter (cmdRetablir, fr ("Rétablir"), "Ctrl+Y", edit->getUndoManager().canRedo());
                m.addSeparator();
                ajouter (cmdCopierClip, fr ("Copier le clip"), "Ctrl+C", selection.clipValide() != nullptr);
                ajouter (cmdCollerClip, fr ("Coller le clip au trait de lecture"), "Ctrl+V");
                ajouter (cmdDupliquerClip, fr ("Dupliquer le clip"), "Ctrl+D", selection.clipValide() != nullptr);
                ajouter (cmdSupprimerClip, fr ("Supprimer le clip"), "Suppr", selection.clipValide() != nullptr);
                break;

            case 2:
                ajouter (cmdVoirNotes, fr ("Notes du clip"), "Tab");
                ajouter (cmdVoirInstrument, fr ("Instrument et effets de la piste"), "Tab");
                break;

            case 3:
                ajouter (cmdReglagesAudio, fr ("Préférences audio (sortie du son, micro)..."));
                break;

            case 4:
                ajouter (cmdRaccourcis, fr ("Raccourcis clavier..."));
                ajouter (cmdJournal, fr ("Ouvrir le journal d'erreurs"));
                m.addSeparator();
                ajouter (cmdAPropos, fr ("À propos de MAO Quack"));
                break;

            default: break;
        }

        return m;
    }

    void menuItemSelected (int id, int) override
    {
        switch (id)
        {
            case cmdNouveau:            nouveau();                                  break;
            case cmdOuvrir:             ouvrir();                                   break;
            case cmdEnregistrer:        enregistrer (false, {});                    break;
            case cmdEnregistrerSous:    enregistrer (true, {});                     break;
            case cmdExporter:           exporter();                                 break;
            case cmdQuitter:            juce::JUCEApplication::getInstance()->systemRequestedQuit();  break;
            case cmdAnnuler:            edit->getUndoManager().undo();              break;
            case cmdRetablir:           edit->getUndoManager().redo();              break;
            case cmdCopierClip:         arrangement.copierClipChoisi();             break;
            case cmdCollerClip:         arrangement.collerClip();                   break;
            case cmdDupliquerClip:      arrangement.dupliquerClipChoisi();          break;
            case cmdSupprimerClip:      arrangement.supprimerClipChoisi();          break;
            case cmdVoirNotes:          bas.montrer (0);                            break;
            case cmdVoirInstrument:     bas.montrer (1);                            break;
            case cmdReglagesAudio:      ouvrirReglagesAudio();                      break;
            case cmdRaccourcis:         montrerRaccourcis();                        break;
            case cmdJournal:            Journal::ecrire ("Journal ouvert depuis le menu Aide");
                                        Journal::fichier().startAsProcess();        break;
            case cmdAPropos:            montrerAPropos();                           break;
            default:                    break;
        }
    }

private:
    void montrerRaccourcis()
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("Raccourcis clavier")
                .withMessage (fr ("Espace : lecture / stop\n"
                                  "Tab : passer des notes à l'instrument\n"
                                  "Ctrl+Z / Ctrl+Y : annuler / rétablir\n"
                                  "Ctrl+S : enregistrer     Ctrl+E : exporter\n\n"
                                  "Dans l'arrangement :\n"
                                  "Double-clic : créer un clip     Suppr : l'effacer\n"
                                  "Ctrl+C / Ctrl+V / Ctrl+D : copier / coller / dupliquer le clip\n"
                                  "Molette : monter et descendre     Maj+molette : avancer et reculer\n"
                                  "Ctrl+molette : zoomer\n\n"
                                  "Dans le piano roll :\n"
                                  "Double-clic : poser ou effacer une note     B : crayon\n"
                                  "Ctrl+A : tout sélectionner     Flèches : déplacer les notes\n"
                                  "Ctrl+C / Ctrl+X / Ctrl+V / Ctrl+D : copier / couper / coller / dupliquer"))
                .withButton ("OK", 1)
                .withAssociatedComponent (this),
            nullptr);
    }

    void montrerAPropos()
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("MAO Quack")
                .withMessage (fr ("Version ") + juce::String (MAOQUACK_VERSION)
                                  + fr ("\n\nUn petit logiciel de musique pour débuter.\n"
                                        "Construit avec JUCE et Tracktion Engine (logiciels libres, licence GPL).\n"
                                        "Export MP3 : encodeur LAME."))
                .withButton ("OK", 1)
                .withAssociatedComponent (this),
            nullptr);
    }

    void remplacer (std::unique_ptr<te::Edit> nouveauMorceau, const juce::File& f)
    {
        if (surRemplacer)
            surRemplacer (std::make_shared<std::unique_ptr<te::Edit>> (std::move (nouveauMorceau)), f);
    }

    void nouveau()
    {
        verifierPuis ([this] { remplacer (creerNouveauMorceau (engine), {}); });
    }

    void ouvrir()
    {
        verifierPuis ([this]
        {
            selecteur = std::make_shared<juce::FileChooser> (fr ("Ouvrir un morceau"), dossierMorceaux(),
                                                             juce::String ("*") + extensionMorceau);
            juce::Component::SafePointer<ComposantPrincipal> moi (this);

            selecteur->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                    [moi] (const juce::FileChooser& fc)
                                    {
                                        const auto f = fc.getResult();
                                        if (moi == nullptr || ! f.existsAsFile())
                                            return;

                                        moi->edit->getTransport().stop (false, false);
                                        auto charge = te::loadEditFromFile (moi->engine, f);

                                        if (charge == nullptr || te::getAudioTracks (*charge).isEmpty())
                                        {
                                            juce::AlertWindow::showAsync (
                                                juce::MessageBoxOptions()
                                                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                    .withTitle (fr ("Impossible d'ouvrir ce fichier"))
                                                    .withMessage (fr ("Ce fichier n'est pas un morceau MAO Quack, ou il est abîmé."))
                                                    .withButton ("OK", 1)
                                                    .withAssociatedComponent (moi.getComponent()),
                                                nullptr);
                                            return;
                                        }

                                        charge->getUndoManager().clearUndoHistory();
                                        charge->resetChangedStatus();
                                        moi->remplacer (std::move (charge), f);
                                    });
        });
    }

    bool ecrire (const juce::File& f)
    {
        const bool ok = te::EditFileOperations (*edit).writeToFile (f, false);

        if (ok)
        {
            fichier = f;
            edit->resetChangedStatus();

            if (auto* fenetre = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
                fenetre->setName ("MAO Quack - " + nomDuMorceau());
        }
        else
        {
            juce::AlertWindow::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle (fr ("L'enregistrement a échoué"))
                    .withMessage (fr ("Le morceau n'a pas pu être enregistré à cet endroit. Essaie un autre dossier."))
                    .withButton ("OK", 1)
                    .withAssociatedComponent (this),
                nullptr);
        }

        return ok;
    }

    /** Enregistre le morceau (en demandant où si besoin), puis continue. */
    void enregistrer (bool demanderOu, std::function<void()> suite)
    {
        if (! demanderOu && fichier != juce::File())
        {
            if (ecrire (fichier) && suite)
                suite();
            return;
        }

        const auto depart = fichier != juce::File() ? fichier
                                                    : dossierMorceaux().getChildFile (fr ("Mon morceau") + extensionMorceau);

        selecteur = std::make_shared<juce::FileChooser> (fr ("Enregistrer le morceau"), depart,
                                                         juce::String ("*") + extensionMorceau);
        juce::Component::SafePointer<ComposantPrincipal> moi (this);

        selecteur->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [moi, suite] (const juce::FileChooser& fc)
                                {
                                    auto f = fc.getResult();
                                    if (moi == nullptr || f == juce::File())
                                        return;

                                    if (! f.hasFileExtension (extensionMorceau))
                                        f = f.withFileExtension (extensionMorceau);

                                    if (moi->ecrire (f) && suite)
                                        suite();
                                });
    }

    void exporter()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Exporter";
        o.dialogBackgroundColour = Couleurs::panneau;
        o.content.setOwned (new DialogueExport (*edit, nomDuMorceau()));
        o.componentToCentreAround = this;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void ouvrirReglagesAudio()
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = fr ("Réglages audio");
        o.dialogBackgroundColour = Couleurs::panneau;
        o.content.setOwned (new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager,
                                                                    0, 2, 1, 2, false, false, true, false));
        o.content->setSize (480, 420);
        o.componentToCentreAround = this;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }
};

//==============================================================================
class FenetrePrincipale : public juce::DocumentWindow
{
public:
    FenetrePrincipale (te::Engine& e)
        : juce::DocumentWindow ("MAO Quack", Couleurs::fond, juce::DocumentWindow::allButtons),
          engine (e)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (900, 560, 10000, 10000);
        afficher (creerNouveauMorceau (engine), {}, true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    /** Vérifie qu'on ne perd rien avant de quitter. */
    void demanderAQuitter()
    {
        if (auto* c = dynamic_cast<ComposantPrincipal*> (getContentComponent()))
            c->verifierPuis ([] { juce::JUCEApplication::quit(); });
        else
            juce::JUCEApplication::quit();
    }

private:
    te::Engine& engine;

    void afficher (std::unique_ptr<te::Edit> morceau, const juce::File& f, bool ajusterTaille)
    {
        clearContentComponent();    // l'ancien morceau est fermé avant d'ouvrir le nouveau

        auto* c = new ComposantPrincipal (engine, std::move (morceau), f);
        juce::Component::SafePointer<FenetrePrincipale> moi (this);

        c->surRemplacer = [moi] (std::shared_ptr<std::unique_ptr<te::Edit>> suivant, juce::File fichier)
        {
            // On attend la fin du clic en cours avant de remplacer le contenu de la fenêtre
            juce::MessageManager::callAsync ([moi, suivant, fichier]
            {
                if (moi != nullptr && *suivant != nullptr)
                    moi->afficher (std::move (*suivant), fichier, false);
            });
        };

        setName ("MAO Quack - " + c->nomDuMorceau());
        setContentOwned (c, ajusterTaille);
    }
};

//==============================================================================
class ApplicationQuack : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override        { return "MAO Quack"; }
    const juce::String getApplicationVersion() override     { return MAOQUACK_VERSION; }
    bool moreThanOneInstanceAllowed() override              { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&apparence);
        Journal::demarrer (MAOQUACK_VERSION);
        Instruments::preparerSonsBatterie();
        engine = std::make_unique<te::Engine> (getApplicationName(), std::make_unique<ComportementQuack>(), nullptr);
        Effets::declarerTypes (*engine);
        fenetre = std::make_unique<FenetrePrincipale> (*engine);
    }

    void shutdown() override
    {
        fenetre.reset();
        engine.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (fenetre != nullptr)
            fenetre->demanderAQuitter();
        else
            quit();
    }

private:
    ApparenceQuack apparence;
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<FenetrePrincipale> fenetre;
};

START_JUCE_APPLICATION (ApplicationQuack)
