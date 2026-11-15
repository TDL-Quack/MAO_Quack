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
#include "BinaryData.h"

static const char* const extensionMorceau = ".quack";

//==============================================================================
/** Un morceau vierge : deux pistes MIDI et deux pistes audio, prêtes à recevoir instruments et sons. */
static std::unique_ptr<te::Edit> creerNouveauMorceau (te::Engine& e)
{
    auto fichier = e.getTemporaryFileManager().getTempFile ("sans_titre.tracktionedit");
    fichier.getParentDirectory().createDirectory();
    fichier.deleteFile();

    auto ed = te::createEmptyEdit (e, fichier);
    ed->ensureNumberOfAudioTracks (4);
    auto pistes = te::getAudioTracks (*ed);

    for (int i = 0; i < juce::jmin (4, pistes.size()); ++i)
    {
        const bool audio = i >= 2;
        pistes[i]->setName ((audio ? "Audio " : "MIDI ") + juce::String (i + 1));
        pistes[i]->setColour (Couleurs::dePiste (i));
        TypePiste::definirAudio (*pistes[i], audio);
    }

    ed->tempoSequence.getTempo (0)->setBpm (120.0);
    ed->clickTrackEnabled = false;
    ed->getUndoManager().clearUndoHistory();
    ed->resetChangedStatus();

    return ed;
}

/** Le morceau de démonstration, montré une seule fois : au tout premier lancement. */
static std::unique_ptr<te::Edit> creerMorceauDeDemo (te::Engine& e)
{
    auto fichier = e.getTemporaryFileManager().getTempFile ("demo.tracktionedit");
    fichier.getParentDirectory().createDirectory();

    if (! fichier.replaceWithData (BinaryData::demo_quack, (size_t) BinaryData::demo_quackSize))
        return {};

    auto ed = te::loadEditFromFile (e, fichier);

    if (ed == nullptr || te::getAudioTracks (*ed).isEmpty())
        return {};

    Instruments::reparerSonsBatterie (*ed);     // les sons de batterie sont rangés dans la bibliothèque de cet ordinateur
    Effets::preparerOscillos (*ed);
    ed->getTransport().setPosition (te::TimePosition());
    ed->getUndoManager().clearUndoHistory();
    ed->resetChangedStatus();
    return ed;
}

/** Ouvre un morceau enregistré (fichier .quack). Renvoie nullptr si le fichier n'est pas lisible. */
static std::unique_ptr<te::Edit> chargerMorceau (te::Engine& e, const juce::File& fichier)
{
    auto ed = te::loadEditFromFile (e, fichier);

    if (ed == nullptr || te::getAudioTracks (*ed).isEmpty())
        return {};

    Instruments::reparerSonsBatterie (*ed);
    Effets::preparerOscillos (*ed);
    ed->getUndoManager().clearUndoHistory();
    ed->resetChangedStatus();
    return ed;
}

/** Le morceau affiché au lancement : la démo la toute première fois, un morceau vierge ensuite. */
static std::unique_ptr<te::Edit> creerMorceauDAccueil (te::Engine& e)
{
    const auto temoin = Reglages::dossierDonnees().getChildFile ("demo_deja_vue.txt");

    if (! temoin.existsAsFile())
    {
        temoin.replaceWithText ("La demo de MAO Quack a deja ete montree sur cet ordinateur.");

        if (auto demo = creerMorceauDeDemo (e))
            return demo;
    }

    return creerNouveauMorceau (e);
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
          navigateur (e),
          arrangement (*edit, selection),
          bas (*edit, selection)
    {
        addAndMakeVisible (barreMenus);
        addAndMakeVisible (transport);
        addAndMakeVisible (navigateur);
        addAndMakeVisible (arrangement);
        addAndMakeVisible (aide);
        addAndMakeVisible (bas);

        transport.surAutomation = [this] (bool active) { arrangement.definirModeAutomation (active); };

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
        // On ne pose la question que si l'utilisateur a vraiment fait quelque chose :
        // un morceau tout juste ouvert (la démo, un morceau vierge) se referme sans rien demander.
        if (! edit->hasChangedSinceSaved() || ! edit->getUndoManager().canUndo())
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

        if (! k.getModifiers().isCommandDown() && (k.getKeyCode() == 'A' || k.getKeyCode() == 'a'))
        {
            basculerAutomation();
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
            case 'L':   arrangement.bouclerSurClip();                           return true;
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
        cmdAnnuler = 20, cmdRetablir, cmdDupliquerClip, cmdSupprimerClip, cmdCopierClip, cmdCollerClip, cmdBouclerClip,
        cmdVoirNotes = 30, cmdVoirInstrument, cmdAutomation,
        cmdReglagesAudio = 40, cmdPlugins,
        cmdRaccourcis = 50, cmdJournal, cmdAPropos, cmdGuide, cmdDemo
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
                m.addSeparator();
                ajouter (cmdBouclerClip, fr ("Boucler sur le clip choisi"), "Ctrl+L", selection.clipValide() != nullptr);
                break;

            case 2:
                ajouter (cmdVoirNotes, fr ("Notes du clip"), "Tab");
                ajouter (cmdVoirInstrument, fr ("Instrument et effets de la piste"), "Tab");
                m.addSeparator();
                ajouter (cmdAutomation, arrangement.estEnModeAutomation() ? fr ("Quitter le mode automation")
                                                                          : fr ("Mode automation (courbes de réglages)"), "A");
                break;

            case 3:
                ajouter (cmdReglagesAudio, fr ("Préférences audio (sortie du son, micro)..."));
                ajouter (cmdPlugins, fr ("Plugins VST (recherche et liste)..."));
                break;

            case 4:
                ajouter (cmdGuide, fr ("Guide : mon premier morceau, Quack Synth..."));
                ajouter (cmdDemo, fr ("Ouvrir le morceau de démonstration"));
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
            case cmdBouclerClip:        arrangement.bouclerSurClip();               break;
            case cmdVoirNotes:          bas.montrer (0);                            break;
            case cmdVoirInstrument:     bas.montrer (1);                            break;
            case cmdAutomation:         basculerAutomation();                       break;
            case cmdPlugins:            ouvrirPlugins();                            break;
            case cmdReglagesAudio:      ouvrirReglagesAudio();                      break;
            case cmdRaccourcis:         montrerRaccourcis();                        break;
            case cmdJournal:            Journal::ecrire ("Journal ouvert depuis le menu Aide");
                                        Journal::fichier().startAsProcess();        break;
            case cmdAPropos:            montrerAPropos();                           break;
            case cmdGuide:              ouvrirGuide();                              break;
            case cmdDemo:               verifierPuis ([this] { if (auto demo = creerMorceauDeDemo (engine)) remplacer (std::move (demo), {}); });  break;
            default:                    break;
        }
    }

private:
    void basculerAutomation()
    {
        const bool actif = ! arrangement.estEnModeAutomation();
        arrangement.definirModeAutomation (actif);
        transport.definirAutomation (actif);
    }

    /** La fenêtre de gestion des plugins VST : recherche sur le disque et liste de ce qui a été trouvé. */
    void ouvrirPlugins()
    {
        auto& gestionnaire = engine.getPluginManager();

        // Un cadre qui possède l'explication et la liste des plugins
        struct Cadre : public juce::Component
        {
            juce::Label explication;
            std::unique_ptr<juce::PluginListComponent> liste;

            void resized() override
            {
                explication.setBounds (10, 8, getWidth() - 20, 64);

                if (liste != nullptr)
                    liste->setBounds (0, 76, getWidth(), getHeight() - 76);
            }
        };

        auto* cadre = new Cadre();
        cadre->explication.setText (fr ("Pour chercher tes plugins : bouton « Options... » en bas, puis « Scan for new or updated VST3 "
                                        "plug-ins ». Choisis le dossier (le plus souvent C:\\Program Files\\Common Files\\VST3) et valide.\n"
                                        "Les plugins trouvés apparaissent ensuite dans le navigateur, dossier « Plugins VST »."),
                                    juce::dontSendNotification);
        cadre->explication.setFont (juce::FontOptions (13.5f));
        cadre->explication.setColour (juce::Label::textColourId, Couleurs::texte);
        cadre->explication.setJustificationType (juce::Justification::topLeft);
        cadre->liste = std::make_unique<juce::PluginListComponent> (gestionnaire.pluginFormatManager, gestionnaire.knownPluginList,
                                                                    engine.getTemporaryFileManager().getTempFile ("RecherchePlugins"),
                                                                    nullptr);
        cadre->liste->setNumberOfThreadsForScanning (1);    // la recherche tourne à part : la fenêtre reste vivante
        cadre->addAndMakeVisible (cadre->explication);
        cadre->addAndMakeVisible (*cadre->liste);
        cadre->setSize (760, 520);

        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Plugins VST";
        o.dialogBackgroundColour = Couleurs::panneau;
        o.content.setOwned (cadre);
        o.componentToCentreAround = this;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void montrerRaccourcis()
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("Raccourcis clavier")
                .withMessage (fr ("Espace : lecture / stop\n"
                                  "Tab : passer des notes à l'instrument     A : mode automation\n"
                                  "Ctrl+Z / Ctrl+Y : annuler / rétablir\n"
                                  "Ctrl+S : enregistrer     Ctrl+E : exporter\n\n"
                                  "Dans l'arrangement :\n"
                                  "Double-clic : créer un clip     Suppr : l'effacer\n"
                                  "Ctrl+C / Ctrl+V / Ctrl+D : copier / coller / dupliquer le clip\n"
                                  "Ctrl + glisser : dupliquer un clip (ou des notes)     Ctrl+L : boucler sur le clip\n"
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

    /** La fenêtre « À propos » : version, crédits, et les liens vers la musique de l'auteur. */
    void montrerAPropos()
    {
        struct Contenu : public juce::Component
        {
            Contenu()
            {
                logo = juce::ImageCache::getFromMemory (BinaryData::icone_png, BinaryData::icone_pngSize);

                const std::pair<const char*, const char*> liens[] = {
                    { "Deezer",     "https://link.deezer.com/s/34XGhMuZrCJyDe47ulB1D" },
                    { "Spotify",    "https://open.spotify.com/intl-fr/artist/2EffRdlYAAfxNy519PifEV?si=tZ8bNbpHQwSa4BOFDz3FIA" },
                    { "SoundCloud", "https://soundcloud.com/alpha-quack-773871380" }
                };

                for (auto& l : liens)
                {
                    auto* b = boutons.add (new juce::HyperlinkButton (l.first, juce::URL (l.second)));
                    b->setFont (juce::FontOptions (15.0f, juce::Font::bold), false);
                    b->setColour (juce::HyperlinkButton::textColourId, Couleurs::accent);
                    addAndMakeVisible (b);
                }

                setSize (460, 330);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (Couleurs::panneau);
                g.drawImage (logo, juce::Rectangle<float> (20.0f, 18.0f, 96.0f, 96.0f));

                g.setColour (Couleurs::accent);
                g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
                g.drawText ("MAO Quack", 132, 24, 300, 32, juce::Justification::centredLeft);

                g.setColour (Couleurs::texte);
                g.setFont (juce::FontOptions (14.0f));
                g.drawText ("Version " + juce::String (MAOQUACK_VERSION), 132, 58, 300, 20, juce::Justification::centredLeft);
                g.drawText (fr ("Un petit logiciel de musique pour débuter."), 132, 80, 320, 20, juce::Justification::centredLeft);

                g.setColour (Couleurs::texteDoux);
                g.setFont (juce::FontOptions (12.5f));
                g.drawFittedText (fr ("Construit avec JUCE et Tracktion Engine (logiciels libres). Export MP3 : encodeur LAME.\n"
                                      "VST est une marque de Steinberg Media Technologies GmbH."),
                                  20, 128, getWidth() - 40, 40, juce::Justification::topLeft, 3);

                g.setColour (Couleurs::texte);
                g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
                g.drawText (fr ("Écouter la musique de l'auteur, Alpha Quack :"), 20, 186, getWidth() - 40, 22, juce::Justification::centredLeft);
            }

            void resized() override
            {
                int x = 20;
                for (auto* b : boutons)
                {
                    b->setBounds (x, 216, 130, 28);
                    x += 140;
                }
            }

            juce::Image logo;
            juce::OwnedArray<juce::HyperlinkButton> boutons;
        };

        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = fr ("À propos de MAO Quack");
        o.dialogBackgroundColour = Couleurs::panneau;
        o.content.setOwned (new Contenu());
        o.componentToCentreAround = this;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    /** Ouvre le guide (une page web rangée dans la bibliothèque) dans le navigateur de l'ordinateur. */
    void ouvrirGuide()
    {
        const auto guide = Reglages::dossierDonnees().getChildFile ("Guide MAO Quack.html");
        guide.replaceWithData (BinaryData::Guide_html, (size_t) BinaryData::Guide_htmlSize);
        guide.startAsProcess();
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
                                        auto charge = chargerMorceau (moi->engine, f);

                                        if (charge == nullptr)
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
    /** `fichierAOuvrir` : un morceau sur lequel l'utilisateur a double-cliqué dans l'explorateur (sinon vide). */
    FenetrePrincipale (te::Engine& e, const juce::File& fichierAOuvrir)
        : juce::DocumentWindow ("MAO Quack", Couleurs::fond, juce::DocumentWindow::allButtons),
          engine (e)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (900, 560, 10000, 10000);
        setIcon (juce::ImageCache::getFromMemory (BinaryData::icone_png, BinaryData::icone_pngSize));
        std::unique_ptr<te::Edit> depart;
        juce::File fichierDepart;

        if (fichierAOuvrir.existsAsFile())
        {
            depart = chargerMorceau (engine, fichierAOuvrir);
            if (depart != nullptr)
                fichierDepart = fichierAOuvrir;
        }

        if (depart == nullptr)
            depart = creerMorceauDAccueil (engine);

        afficher (std::move (depart), fichierDepart, true);
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
    bool moreThanOneInstanceAllowed() override              { return true; }     // la recherche de plugins relance le programme

    void initialise (const juce::String& ligneDeCommande) override
    {
        // Ce lancement n'est peut-être qu'un assistant chargé d'examiner un plugin VST :
        // dans ce cas il fait son travail en silence, sans ouvrir de fenêtre.
        if (te::PluginManager::startChildProcessPluginScan (ligneDeCommande))
            return;

        juce::LookAndFeel::setDefaultLookAndFeel (&apparence);
        Reglages::reprendreAncienDossier();     // les données quittent AppData pour Documents/MAO Quack
        Journal::demarrer (MAOQUACK_VERSION);
        Instruments::preparerSonsBatterie();
        engine = std::make_unique<te::Engine> (std::make_unique<RangementMoteur>(), std::make_unique<ComportementQuack>(), std::make_unique<ReglagesMoteur>());
        Effets::declarerTypes (*engine);
        // Lancé par un double-clic sur un morceau : son chemin arrive ici, parfois entre guillemets
        const juce::File demande (ligneDeCommande.trim().unquoted());
        const bool estUnMorceau = juce::File::isAbsolutePath (ligneDeCommande.trim().unquoted())
                                    && demande.hasFileExtension (extensionMorceau);

        fenetre = std::make_unique<FenetrePrincipale> (*engine, estUnMorceau ? demande : juce::File());
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
