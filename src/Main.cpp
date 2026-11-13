/*
    MAO Quack — un petit logiciel de musique pour débuter.
*/

#include "Commun.h"
#include "BarreTransport.h"
#include "VueArrangement.h"
#include "Panneaux.h"
#include "Instruments.h"
#include "PianoRoll.h"

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
class ComposantPrincipal : public juce::Component
{
public:
    ComposantPrincipal (te::Engine& e, std::unique_ptr<te::Edit> morceau, const juce::File& fichierDuMorceau)
        : engine (e),
          edit (std::move (morceau)),
          fichier (fichierDuMorceau),
          selection (*edit),
          transport (*edit),
          arrangement (*edit, selection),
          pianoRoll (*edit, selection)
    {
        addAndMakeVisible (transport);
        addAndMakeVisible (navigateur);
        addAndMakeVisible (arrangement);
        addAndMakeVisible (aide);
        addAndMakeVisible (pianoRoll);

        transport.surReglagesAudio = [this] { ouvrirReglagesAudio(); };
        transport.surFichier = [this] { menuFichier(); };
        transport.definirTitre (nomDuMorceau());

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

        transport.setBounds (r.removeFromTop (46));

        auto bas = r.removeFromBottom (juce::jlimit (230, 380, (int) (getHeight() * 0.4)));
        aide.setBounds (bas.removeFromLeft (230));
        pianoRoll.setBounds (bas);

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

        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
        {
            // (Le piano roll traite lui-même ses notes quand on travaille dedans.)
            if (! pianoRoll.hasKeyboardFocus (true))
                arrangement.supprimerClipChoisi();
            return true;
        }

        const auto mods = k.getModifiers();
        if (! mods.isCommandDown())
            return false;

        const bool dansPianoRoll = pianoRoll.hasKeyboardFocus (true);

        switch (k.getKeyCode())
        {
            case 'S':   enregistrer (mods.isShiftDown(), {});                   return true;
            case 'O':   ouvrir();                                               return true;
            case 'N':   nouveau();                                              return true;
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

    BarreTransport transport;
    Navigateur navigateur;
    VueArrangement arrangement;
    ZoneAide aide;
    PianoRoll pianoRoll;

    std::shared_ptr<juce::FileChooser> selecteur;

    //==========================================================================
    static juce::File dossierMorceaux()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("MAO Quack");
        d.createDirectory();
        return d;
    }

    void menuFichier()
    {
        juce::PopupMenu m;
        m.addItem (1, fr ("Nouveau morceau          Ctrl+N"));
        m.addItem (2, fr ("Ouvrir...                         Ctrl+O"));
        m.addSeparator();
        m.addItem (3, fr ("Enregistrer                    Ctrl+S"));
        m.addItem (4, fr ("Enregistrer sous..."));

        juce::Component::SafePointer<ComposantPrincipal> moi (this);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&transport.boutonFichier()),
                         [moi] (int choix)
                         {
                             if (moi == nullptr)
                                 return;

                             switch (choix)
                             {
                                 case 1:  moi->nouveau();                  break;
                                 case 2:  moi->ouvrir();                   break;
                                 case 3:  moi->enregistrer (false, {});    break;
                                 case 4:  moi->enregistrer (true, {});     break;
                                 default: break;
                             }
                         });
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
            transport.definirTitre (nomDuMorceau());

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
        Instruments::preparerSonsBatterie();
        engine = std::make_unique<te::Engine> (getApplicationName());
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
