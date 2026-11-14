#pragma once

#include "Commun.h"
#include "PianoRoll.h"
#include "Peripheriques.h"

//==============================================================================
/** La zone du bas, avec deux onglets comme dans Ableton :
    les notes du clip choisi, ou l'instrument et les effets de la piste choisie. */
class PanneauBas : public juce::Component,
                   private juce::ChangeListener
{
public:
    static constexpr int hauteurOnglets = 20;

    PanneauBas (te::Edit& e, Selection& sel)
        : edit (e), selection (sel), pianoRoll (e, sel), chaine (e, sel)
    {
        addAndMakeVisible (pianoRoll);
        addChildComponent (chaine);

        auto preparer = [this] (juce::TextButton& b, const juce::String& texte, int index, const juce::String& aide)
        {
            addAndMakeVisible (b);
            b.setButtonText (texte);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (77);
            b.setWantsKeyboardFocus (false);
            b.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
            b.setColour (juce::TextButton::buttonColourId, Couleurs::bordure);
            b.setColour (juce::TextButton::buttonOnColourId, Couleurs::panneauClair);
            b.setColour (juce::TextButton::textColourOffId, Couleurs::texteDoux);
            b.setColour (juce::TextButton::textColourOnId, Couleurs::accent);
            b.onClick = [this, index, &b] { if (b.getToggleState()) montrer (index); };
            definirAide (b, texte, aide);
        };

        preparer (ongletNotes, "Notes", 0,
                  fr ("Affiche le piano roll : les notes du clip choisi. Raccourci pour changer d'onglet : Tab."));
        preparer (ongletInstrument, "Instrument", 1,
                  fr ("Affiche l'instrument de la piste choisie et ses effets, avec tous leurs réglages. "
                      "Raccourci pour changer d'onglet : Tab."));

        ongletNotes.setToggleState (true, juce::dontSendNotification);
        dernierePiste = selection.pisteValide();
        selection.addChangeListener (this);
    }

    ~PanneauBas() override      { selection.removeChangeListener (this); }

    PianoRoll& getPianoRoll()   { return pianoRoll; }

    void montrer (int index)
    {
        onglet = index;
        pianoRoll.setVisible (index == 0);
        chaine.setVisible (index == 1);
        ongletNotes.setToggleState (index == 0, juce::dontSendNotification);
        ongletInstrument.setToggleState (index == 1, juce::dontSendNotification);
        repaint();
    }

    void basculer()             { montrer (onglet == 0 ? 1 : 0); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::bordure);

        if (auto* piste = selection.pisteValide())
        {
            auto r = getLocalBounds().removeFromTop (hauteurOnglets).withTrimmedRight (160).reduced (8, 0);
            const int numPiste = te::getAudioTracks (edit).indexOf (piste);

            g.setColour (couleurPiste (*piste, numPiste));
            g.fillRoundedRectangle (r.removeFromLeft (12).withSizeKeepingCentre (10, 10).toFloat(), 2.0f);
            g.setColour (Couleurs::texte);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (piste->getName(), r.withTrimmedLeft (6), juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        // De petits onglets discrets, à droite, comme les sélecteurs de vue d'Ableton
        auto barre = r.removeFromTop (hauteurOnglets).reduced (4, 2);
        ongletInstrument.setBounds (barre.removeFromRight (84));
        ongletNotes.setBounds (barre.removeFromRight (60));
        pianoRoll.setBounds (r);
        chaine.setBounds (r);
    }

private:
    te::Edit& edit;
    Selection& selection;
    PianoRoll pianoRoll;
    ChainePeripheriques chaine;
    juce::TextButton ongletNotes, ongletInstrument;
    int onglet = 0;

    te::MidiClip* dernierClip = nullptr;
    te::AudioTrack* dernierePiste = nullptr;

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        auto* clip = selection.clipValide();
        auto* piste = selection.pisteValide();

        // Un clip vient d'être choisi : on montre ses notes. Une piste : son instrument.
        if (clip != nullptr && clip != dernierClip)
            montrer (0);
        else if (piste != dernierePiste)
            montrer (1);

        dernierClip = clip;
        dernierePiste = piste;
        repaint();
    }
};
