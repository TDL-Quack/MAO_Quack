#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <vector>
#include <cmath>

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
/** Le clip choisi par l'utilisateur, partagé entre l'arrangement et le piano roll. */
class Selection : public juce::ChangeBroadcaster
{
public:
    Selection (te::Edit& e) : edit (e) {}

    void choisir (te::MidiClip* c)
    {
        if (clip.get() != c)
        {
            clip = c;
            sendChangeMessage();
        }
    }

    /** Le clip choisi, ou nullptr s'il n'existe plus (effacé, annulé...). */
    te::MidiClip* clipValide()
    {
        if (clip != nullptr)
        {
            for (auto* piste : te::getAudioTracks (edit))
                if (piste->getClips().contains (clip.get()))
                    return clip.get();

            clip = nullptr;
            sendChangeMessage();
        }

        return nullptr;
    }

    /** Numéro de la piste (0, 1, 2...) qui contient le clip, ou -1. */
    static int indexPiste (te::Edit& e, const te::Clip& c)
    {
        int i = 0;
        for (auto* piste : te::getAudioTracks (e))
        {
            if (piste->getClips().contains (const_cast<te::Clip*> (&c)))
                return i;
            ++i;
        }
        return -1;
    }

private:
    te::Edit& edit;
    te::MidiClip::Ptr clip;
};
