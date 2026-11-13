/*
    MAO Quack — un petit logiciel de musique pour débuter.
    Étape 1 : fenêtre principale, moteur audio, transport, pistes.
*/

#include "Commun.h"
#include "BarreTransport.h"
#include "VueArrangement.h"
#include "Panneaux.h"

//==============================================================================
/** Tout le contenu de la fenêtre. */
class ComposantPrincipal : public juce::Component
{
public:
    ComposantPrincipal (te::Engine& e)
        : engine (e),
          edit (creerMorceauVide (e)),
          transport (*edit),
          arrangement (*edit)
    {
        addAndMakeVisible (transport);
        addAndMakeVisible (navigateur);
        addAndMakeVisible (arrangement);
        addAndMakeVisible (aide);
        addAndMakeVisible (detail);

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

        auto bas = r.removeFromBottom (170);
        aide.setBounds (bas.removeFromLeft (230));
        detail.setBounds (bas);

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

        return false;
    }

    void mouseDown (const juce::MouseEvent&) override   { grabKeyboardFocus(); }

private:
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;

    BarreTransport transport;
    Navigateur navigateur;
    VueArrangement arrangement;
    ZoneAide aide;
    PanneauDetail detail;

    static std::unique_ptr<te::Edit> creerMorceauVide (te::Engine& e)
    {
        auto fichier = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("MAOQuack")
                           .getChildFile ("sans_titre.tracktionedit");
        fichier.getParentDirectory().createDirectory();
        fichier.deleteFile();

        auto ed = te::createEmptyEdit (e, fichier);

        static const char* const noms[] = { "Batterie", "Basse", "Mélodie", "Micro / Sons" };
        ed->ensureNumberOfAudioTracks (4);

        int i = 0;
        for (auto* t : te::getAudioTracks (*ed))
            if (i < 4)
                t->setName (fr (noms[i++]));

        ed->tempoSequence.getTempo (0)->setBpm (120.0);
        ed->clickTrackEnabled = true;   // le métronome, pour entendre que le son marche

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
