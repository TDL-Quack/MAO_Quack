#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <vector>
#include <cmath>
#include <complex>

namespace te = tracktion;

/** Texte français (avec accents) -> juce::String. Les sources sont en UTF-8. */
inline juce::String fr (const char* utf8)   { return juce::String::fromUTF8 (utf8); }

//==============================================================================
namespace Couleurs
{
    const juce::Colour fond         { 0xff2b2b2b };
    const juce::Colour panneau      { 0xff3a3a3a };
    const juce::Colour panneauClair { 0xff4a4a4a };
    const juce::Colour bordure      { 0xff1e1e1e };
    const juce::Colour piste        { 0xff333333 };
    const juce::Colour pisteAlt     { 0xff303030 };
    const juce::Colour grille       { 0xff444444 };
    const juce::Colour grilleMesure { 0xff5a5a5a };
    const juce::Colour texte        { 0xffe0e0e0 };
    const juce::Colour texteDoux    { 0xff9a9a9a };
    const juce::Colour accent       { 0xffffb532 };   // orange "actif"
    const juce::Colour lecture      { 0xff5ad15a };   // vert lecture
    const juce::Colour solo         { 0xff4aa3ff };

    inline juce::Colour dePiste (int index)
    {
        static const juce::Colour palette[] = {
            juce::Colour (0xffff6b5e), juce::Colour (0xffffb532), juce::Colour (0xfffff06a),
            juce::Colour (0xff7ee06b), juce::Colour (0xff4fd8c9), juce::Colour (0xff5aa8ff),
            juce::Colour (0xffa788ff), juce::Colour (0xffff7fc8)
        };
        return palette[((index % 8) + 8) % 8];
    }
}

/** La couleur d'une piste (celle enregistrée dans le morceau, sinon une couleur par défaut). */
inline juce::Colour couleurPiste (const te::Track& piste, int index)
{
    const auto c = piste.getColour();
    return c.isTransparent() ? Couleurs::dePiste (index) : c;
}

//==============================================================================
/** Apparence sombre de toute l'application. */
class ApparenceQuack : public juce::LookAndFeel_V4
{
public:
    ApparenceQuack()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Couleurs::fond);
        setColour (juce::DocumentWindow::textColourId, Couleurs::texte);
        setColour (juce::Label::textColourId, Couleurs::texte);
        setColour (juce::TextButton::buttonColourId, Couleurs::panneauClair);
        setColour (juce::TextButton::buttonOnColourId, Couleurs::accent);
        setColour (juce::TextButton::textColourOffId, Couleurs::texte);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Slider::backgroundColourId, Couleurs::bordure);
        setColour (juce::Slider::trackColourId, Couleurs::accent);
        setColour (juce::Slider::thumbColourId, Couleurs::texte);
        setColour (juce::Slider::textBoxTextColourId, Couleurs::texte);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId, Couleurs::bordure);
        setColour (juce::TextEditor::textColourId, Couleurs::texte);
        setColour (juce::PopupMenu::backgroundColourId, Couleurs::panneau);
        setColour (juce::PopupMenu::textColourId, Couleurs::texte);
        setColour (juce::ComboBox::backgroundColourId, Couleurs::panneauClair);
        setColour (juce::ComboBox::textColourId, Couleurs::texte);
        setColour (juce::ListBox::backgroundColourId, Couleurs::panneau);
    }
};

//==============================================================================
/** Attache un texte d'aide à un composant : il s'affiche dans la zone d'aide
    (en bas à gauche) quand la souris passe dessus. */
inline void definirAide (juce::Component& c, const juce::String& titre, const juce::String& texte)
{
    c.getProperties().set ("aideTitre", titre);
    c.getProperties().set ("aideTexte", texte);
}

/** La zone d'aide, comme l'« Info View » d'Ableton. */
class ZoneAide : public juce::Component,
                 private juce::Timer
{
public:
    ZoneAide()              { startTimerHz (8); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);
        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());

        auto zone = getLocalBounds().reduced (10, 8);

        g.setColour (Couleurs::accent);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (titre.isEmpty() ? fr ("Aide") : titre, zone.removeFromTop (22),
                    juce::Justification::centredLeft, true);

        g.setColour (Couleurs::texte);
        g.setFont (juce::FontOptions (13.5f));
        g.drawFittedText (texte.isEmpty() ? fr ("Passe la souris sur un bouton ou une zone : "
                                                "son explication apparaît ici.")
                                          : texte,
                          zone, juce::Justification::topLeft, 12, 1.0f);
    }

private:
    juce::String titre, texte;

    void timerCallback() override
    {
        juce::String t, x;

        for (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
             c != nullptr; c = c->getParentComponent())
        {
            if (c->getProperties().contains ("aideTitre"))
            {
                t = c->getProperties()["aideTitre"].toString();
                x = c->getProperties()["aideTexte"].toString();
                break;
            }
        }

        if (t != titre || x != texte)
        {
            titre = t;
            texte = x;
            repaint();
        }
    }
};

//==============================================================================
/** Une piste est soit MIDI (des notes jouées par un instrument), soit audio (des sons enregistrés). */
namespace TypePiste
{
    inline const juce::Identifier& id()
    {
        static const juce::Identifier i ("quackType");
        return i;
    }

    inline bool estAudio (const te::Track& piste)       { return piste.state.getProperty (id()).toString() == "audio"; }
    inline void definirAudio (te::Track& piste, bool a) { piste.state.setProperty (id(), a ? "audio" : "midi", nullptr); }
}

/** Les identifiants des éléments du navigateur : « inst:... » et « preset:... » (instruments fournis),
    « fx:... » (effets fournis), « vsti:... » et « vstfx:... » (plugins VST instruments et effets). */
namespace IdNav
{
    inline bool estEffet (const juce::String& id)       { return id.startsWith ("fx:") || id.startsWith ("vstfx:"); }
    inline bool estInstrument (const juce::String& id)  { return id.startsWith ("inst:") || id.startsWith ("preset:") || id.startsWith ("vsti:"); }
    inline bool estConnu (const juce::String& id)       { return estEffet (id) || estInstrument (id); }
}

/** Fabrique la petite étiquette qui suit la souris pendant un glisser-déposer. */
inline juce::ScaledImage imageGlisser (const juce::String& texte, juce::Colour couleur)
{
    const juce::Font police { juce::FontOptions (14.0f, juce::Font::bold) };
    const int largeur = juce::jlimit (60, 260, juce::roundToInt (juce::GlyphArrangement::getStringWidth (police, texte)) + 24);
    juce::Image image (juce::Image::ARGB, largeur, 24, true);

    juce::Graphics g (image);
    g.setColour (couleur.withAlpha (0.9f));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) largeur, 24.0f, 5.0f);
    g.setColour (juce::Colours::black);
    g.setFont (police);
    g.drawText (texte, 0, 0, largeur, 24, juce::Justification::centred, true);

    return juce::ScaledImage (image);
}

//==============================================================================
/** Le clip et la piste choisis par l'utilisateur, partagés entre toutes les zones. */
class Selection : public juce::ChangeBroadcaster
{
public:
    Selection (te::Edit& e) : edit (e) {}

    void choisir (te::Clip* c)
    {
        if (clip.get() != c)
        {
            clip = c;

            if (c != nullptr)
            {
                const int i = indexPiste (edit, *c);
                if (i >= 0)
                    piste = te::getAudioTracks (edit)[i];
            }

            sendChangeMessage();
        }
    }

    void choisirPiste (te::AudioTrack* p)
    {
        if (piste.get() != p)
        {
            piste = p;
            sendChangeMessage();
        }
    }

    /** Le clip choisi (MIDI ou audio), ou nullptr s'il n'existe plus (effacé, annulé...). */
    te::Clip* clipValide()
    {
        if (clip != nullptr)
        {
            for (auto* p : te::getAudioTracks (edit))
                if (p->getClips().contains (clip.get()))
                    return clip.get();

            clip = nullptr;
            sendChangeMessage();
        }

        return nullptr;
    }

    /** Le clip choisi si c'est un clip de notes, sinon nullptr. */
    te::MidiClip* clipMidiValide()      { return dynamic_cast<te::MidiClip*> (clipValide()); }

    /** La piste choisie (ou la première s'il n'y en a pas), nullptr si le morceau est vide. */
    te::AudioTrack* pisteValide()
    {
        auto pistes = te::getAudioTracks (edit);

        if (piste != nullptr && pistes.contains (piste.get()))
            return piste.get();

        piste = pistes.isEmpty() ? nullptr : pistes.getFirst();
        return piste.get();
    }

    /** Numéro de la piste (0, 1, 2...) qui contient le clip, ou -1. */
    static int indexPiste (te::Edit& e, const te::Clip& c)
    {
        int i = 0;
        for (auto* p : te::getAudioTracks (e))
        {
            if (p->getClips().contains (const_cast<te::Clip*> (&c)))
                return i;
            ++i;
        }
        return -1;
    }

    te::Edit& edit;

private:
    te::Clip::Ptr clip;
    te::AudioTrack::Ptr piste;
};

//==============================================================================
/** Petits réglages gardés d'une fois sur l'autre (les favoris du navigateur). */
namespace Reglages
{
    /** Le dossier de l'utilisateur : Documents/MAO Quack (morceaux, exports, et la bibliothèque). */
    inline juce::File dossierUtilisateur()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("MAO Quack");
        d.createDirectory();
        return d;
    }

    /** L'ancien emplacement des données (versions avant la 0.9), dans AppData. */
    inline juce::File ancienDossierDonnees()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("MAO Quack");
    }

    /** La bibliothèque : tout ce que le logiciel range pour lui-même (sons, préréglages, favoris,
        réglages, journal). Elle est dans Documents/MAO Quack, comme la « User Library » d'Ableton :
        rien n'est laissé ailleurs sur l'ordinateur. */
    inline juce::File dossierDonnees()
    {
        auto d = dossierUtilisateur().getChildFile (fr ("Bibliothèque"));
        d.createDirectory();
        return d;
    }

    /** Au premier lancement de cette version : reprend les préréglages, favoris et réglages de
        l'ancien dossier (AppData), puis supprime celui-ci. */
    inline void reprendreAncienDossier()
    {
        const auto ancien = ancienDossierDonnees();
        if (! ancien.isDirectory())
            return;

        const auto nouveau = dossierDonnees();

        for (auto nom : { "favoris.txt", "categories_vst.txt" })
            if (ancien.getChildFile (nom).existsAsFile() && ! nouveau.getChildFile (nom).existsAsFile())
                ancien.getChildFile (nom).copyFileTo (nouveau.getChildFile (nom));

        // Les réglages du moteur (dont la liste des plugins VST déjà trouvés)
        auto reglages = nouveau.getChildFile (fr ("Réglages"));
        reglages.createDirectory();
        if (ancien.getChildFile ("Settings.xml").existsAsFile() && ! reglages.getChildFile ("Settings.xml").existsAsFile())
            ancien.getChildFile ("Settings.xml").copyFileTo (reglages.getChildFile ("Settings.xml"));

        auto presets = nouveau.getChildFile ("Presets");
        presets.createDirectory();
        for (auto& f : ancien.getChildFile ("Presets").findChildFiles (juce::File::findFiles, false, "*.quackpreset"))
            if (! presets.getChildFile (f.getFileName()).existsAsFile())
                f.copyFileTo (presets.getChildFile (f.getFileName()));

        ancien.deleteRecursively();
    }

    inline juce::File fichierFavoris()      { return dossierDonnees().getChildFile ("favoris.txt"); }

    inline juce::StringArray lireFavoris()
    {
        juce::StringArray f;
        f.addLines (fichierFavoris().loadFileAsString());
        f.removeEmptyStrings();
        return f;
    }

    inline void ecrireFavoris (const juce::StringArray& f)
    {
        fichierFavoris().replaceWithText (f.joinIntoString ("\n"));
    }
}

//==============================================================================
/** Journal : un fichier texte où le logiciel note ce qu'il fait d'important et ses erreurs.
    Il sert à comprendre un plantage après coup (menu Aide > Ouvrir le journal). */
namespace Journal
{
    inline juce::File fichier()     { return Reglages::dossierDonnees().getChildFile ("journal.txt"); }

    inline void ecrire (const juce::String& ligne)
    {
        auto f = fichier();

        if (f.getSize() > 512 * 1024)       // on repart de zéro quand il devient trop gros
            f.deleteFile();

        f.appendText (juce::Time::getCurrentTime().toString (true, true, true, true) + "  " + ligne + "\r\n",
                      false, false, "\r\n");
    }

    inline void gestionnaireDePlantage (void*)
    {
        ecrire ("*** PLANTAGE ***");
        ecrire (juce::SystemStats::getStackBacktrace());
    }

    inline void demarrer (const juce::String& version)
    {
        ecrire ("=== Demarrage de MAO Quack " + version + " - " + juce::SystemStats::getOperatingSystemName() + " ===");
        juce::SystemStats::setApplicationCrashHandler (gestionnaireDePlantage);
    }
}
