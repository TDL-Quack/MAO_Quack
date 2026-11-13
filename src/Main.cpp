/*
    MAO Quack — un petit logiciel de musique pour débuter.
    Étape 2 : clips, piano roll, batterie et synthés intégrés.
*/

#include "Commun.h"
#include "BarreTransport.h"
#include "VueArrangement.h"
#include "Panneaux.h"
#include "Instruments.h"
#include "PianoRoll.h"

//==============================================================================
/** Tout le contenu de la fenêtre. */
class ComposantPrincipal : public juce::Component
{
public:
    ComposantPrincipal (te::Engine& e)
        : engine (e),
          edit (creerMorceauVide (e)),
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

        edit->getTransport().ensureContextAllocated();

        setWantsKeyboardFocus (true);
        setSize (1280, 800);
    }

    ~ComposantPrincipal() override
    {
        edit->getTransport().stop (false, true);
    }

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
            // Dans le piano roll on efface une note, ailleurs le clip choisi
            if (pianoRoll.hasKeyboardFocus (true))
                pianoRoll.supprimerNoteChoisie();
            else
                arrangement.supprimerClipChoisi();
            return true;
        }

        const auto mods = k.getModifiers();

        if (mods.isCommandDown() && k.getKeyCode() == 'D')
        {
            arrangement.dupliquerClipChoisi();
            return true;
        }

        if (mods.isCommandDown() && (k.getKeyCode() == 'Y' || (k.getKeyCode() == 'Z' && mods.isShiftDown())))
        {
            edit->getUndoManager().redo();
            return true;
        }

        if (mods.isCommandDown() && k.getKeyCode() == 'Z')
        {
            edit->getUndoManager().undo();
            return true;
        }

        return false;
    }

    void mouseDown (const juce::MouseEvent&) override   { grabKeyboardFocus(); }

private:
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;

    Selection selection;

    BarreTransport transport;
    Navigateur navigateur;
    VueArrangement arrangement;
    ZoneAide aide;
    PianoRoll pianoRoll;

    static std::unique_ptr<te::Edit> creerMorceauVide (te::Engine& e)
    {
        auto fichier = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("MAOQuack")
                           .getChildFile ("sans_titre.tracktionedit");
        fichier.getParentDirectory().createDirectory();
        fichier.deleteFile();

        auto ed = te::createEmptyEdit (e, fichier);

        static const char* const noms[] = { "Batterie", "Basse", "Mélodie", "Accords" };
        ed->ensureNumberOfAudioTracks (4);
        auto pistes = te::getAudioTracks (*ed);

        for (int i = 0; i < juce::jmin (4, pistes.size()); ++i)
            pistes[i]->setName (fr (noms[i]));

        if (pistes.size() >= 4)
        {
            Instruments::installerBatterie (*pistes[0]);
            Instruments::installerSynthe (*pistes[1], Instruments::Synthe::basse);
            Instruments::installerSynthe (*pistes[2], Instruments::Synthe::melodie);
            Instruments::installerSynthe (*pistes[3], Instruments::Synthe::accords);

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

        return ed;
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
    FenetrePrincipale (te::Engine& engine)
        : juce::DocumentWindow ("MAO Quack", Couleurs::fond, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new ComposantPrincipal (engine), true);
        setResizable (true, false);
        setResizeLimits (900, 560, 10000, 10000);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
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
        engine = std::make_unique<te::Engine> (getApplicationName());
        fenetre = std::make_unique<FenetrePrincipale> (*engine);
    }

    void shutdown() override
    {
        fenetre.reset();
        engine.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override     { quit(); }

private:
    ApparenceQuack apparence;
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<FenetrePrincipale> fenetre;
};

START_JUCE_APPLICATION (ApplicationQuack)
