#pragma once

#include "Commun.h"
#include "FormesOnde.h"

/** Le « warp » des clips audio : caler une boucle enregistrée sur le tempo du morceau. */
namespace Warp
{
    /** Devine le tempo d'origine d'une boucle d'après sa durée, en supposant qu'elle dure
        un nombre rond de temps (1, 2, 4, 8, 16... temps). */
    inline double devinerBpm (double dureeEnSecondes)
    {
        if (dureeEnSecondes <= 0.0)
            return 120.0;

        for (double temps = 1.0; temps <= 256.0; temps *= 2.0)
        {
            const double bpm = temps * 60.0 / dureeEnSecondes;
            if (bpm >= 70.0 && bpm < 180.0)
                return bpm;
        }

        return 120.0;
    }

    inline double bpmOrigine (te::WaveAudioClip& clip)
    {
        const double bpm = clip.getLoopInfo().getBpm (clip.getAudioFile().getInfo());
        return bpm > 1.0 ? bpm : devinerBpm (FormesOnde::obtenir (clip.getOriginalFile()).duree);
    }

    /** Indique le tempo d'origine du son ; si le warp est actif, le clip reprend la bonne longueur. */
    inline void definirBpmOrigine (te::WaveAudioClip& clip, double bpm)
    {
        bpm = juce::jlimit (30.0, 300.0, bpm);

        auto infoBoucle = clip.getLoopInfo();
        infoBoucle.setBpm (bpm, clip.getAudioFile().getInfo());
        clip.setLoopInfo (infoBoucle);

        if (clip.getAutoTempo())
        {
            auto& edit = clip.edit;
            const double duree = FormesOnde::obtenir (clip.getOriginalFile()).duree;
            const double debut = edit.tempoSequence.toBeats (clip.getPosition().time.getStart()).inBeats();
            const double temps = duree * bpm / 60.0;
            clip.setEnd (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (debut + temps)), true);
        }
    }

    /** Active ou coupe le calage sur le tempo. */
    inline void activer (te::WaveAudioClip& clip, bool actif)
    {
        const double duree = FormesOnde::obtenir (clip.getOriginalFile()).duree;
        const auto debut = clip.getPosition().time.getStart();

        if (actif)
        {
            const double bpm = bpmOrigine (clip);
            clip.setSyncType (te::Clip::syncBarsBeats);
            clip.setTimeStretchMode (te::TimeStretcher::defaultMode);
            clip.setAutoTempo (true);
            definirBpmOrigine (clip, bpm);
        }
        else
        {
            clip.setAutoTempo (false);
            clip.setEnd (debut + te::TimeDuration::fromSeconds (duree), true);      // retour à la durée réelle du son
        }
    }
}

//==============================================================================
/** Le panneau d'un clip audio (dans la zone du bas) : sa forme d'onde, le warp et le volume. */
class PanneauClipAudio : public juce::Component,
                         private juce::Timer
{
public:
    PanneauClipAudio (te::Edit& e, Selection& sel) : edit (e), selection (sel)
    {
        addAndMakeVisible (warp);
        warp.setButtonText ("Warp");
        warp.setClickingTogglesState (true);
        warp.setWantsKeyboardFocus (false);
        warp.setColour (juce::TextButton::buttonOnColourId, Couleurs::accent);
        warp.onClick = [this]
        {
            if (auto* clip = clipAudio())
            {
                edit.getUndoManager().beginNewTransaction();
                Warp::activer (*clip, warp.getToggleState());
            }
        };
        definirAide (warp, "Warp",
                     fr ("Cale ce son sur le tempo du morceau : une boucle enregistrée plus vite ou plus lentement est étirée "
                         "pour tomber juste, sans changer de hauteur. Il faut pour cela indiquer son tempo d'origine (BPM) à côté."));

        addAndMakeVisible (bpm);
        bpm.setSliderStyle (juce::Slider::LinearBar);
        bpm.setRange (40.0, 240.0, 0.01);
        bpm.setNumDecimalPlacesToDisplay (2);
        bpm.setTextValueSuffix (" BPM");
        bpm.setWantsKeyboardFocus (false);
        bpm.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); };
        bpm.onValueChange = [this]
        {
            if (auto* clip = clipAudio())
                Warp::definirBpmOrigine (*clip, bpm.getValue());
        };
        definirAide (bpm, fr ("Tempo d'origine"),
                     fr ("Le tempo auquel ce son a été enregistré. MAO Quack le devine d'après sa durée ; corrige-le si la boucle "
                         "ne tombe pas juste. Les boutons /2 et x2 servent quand il s'est trompé du simple au double."));

        auto preparer = [this] (juce::TextButton& b, const juce::String& texte, double facteur)
        {
            addAndMakeVisible (b);
            b.setButtonText (texte);
            b.setWantsKeyboardFocus (false);
            b.onClick = [this, facteur]
            {
                if (auto* clip = clipAudio())
                {
                    edit.getUndoManager().beginNewTransaction();
                    Warp::definirBpmOrigine (*clip, Warp::bpmOrigine (*clip) * facteur);
                }
            };
            definirAide (b, texte, fr ("Divise ou multiplie par deux le tempo d'origine indiqué."));
        };

        preparer (moitie, "/2", 0.5);
        preparer (dobl, "x2", 2.0);

        addAndMakeVisible (gain);
        gain.setSliderStyle (juce::Slider::LinearBar);
        gain.setRange (-36.0, 12.0, 0.1);
        gain.setTextValueSuffix (" dB");
        gain.setDoubleClickReturnValue (true, 0.0);
        gain.setWantsKeyboardFocus (false);
        gain.onDragStart = [this] { edit.getUndoManager().beginNewTransaction(); };
        gain.onValueChange = [this]
        {
            if (auto* clip = clipAudio())
                clip->setGainDB ((float) gain.getValue());
        };
        definirAide (gain, "Gain", fr ("Le volume de ce clip audio seulement. Double-clic : retour à 0 dB."));

        definirAide (*this, fr ("Clip audio"),
                     fr ("Un clip audio joue un son enregistré. Ici : son dessin, son volume, et le Warp pour le caler sur le tempo."));

        startTimerHz (10);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);

        auto* clip = clipAudio();
        if (clip == nullptr)
            return;

        g.setColour (Couleurs::texte);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (fr ("Clip audio : ") + clip->getName(), 12, 4, getWidth() - 24, 20, juce::Justification::centredLeft, true);

        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("Volume", 12, 30, 60, 22, juce::Justification::centredLeft);
        g.drawText (fr ("Tempo d'origine"), 330, 30, 100, 22, juce::Justification::centredLeft);

        // La forme d'onde du fichier entier
        auto zone = getLocalBounds().withTrimmedTop (64).reduced (12, 8);
        g.setColour (Couleurs::bordure);
        g.fillRoundedRectangle (zone.toFloat(), 3.0f);

        const juce::File fichier = clip->getOriginalFile();
        const auto& forme = FormesOnde::obtenir (fichier);

        if (forme.duree > 0.0 && zone.getWidth() > 4)
        {
            const int numPiste = Selection::indexPiste (edit, *clip);
            g.setColour (numPiste >= 0 ? couleurPiste (*te::getAudioTracks (edit)[numPiste], numPiste) : Couleurs::accent);

            const double parPixel = forme.duree / (double) zone.getWidth();
            const float milieu = (float) zone.getCentreY(), demi = (float) zone.getHeight() * 0.46f;

            for (int px = 0; px < zone.getWidth(); ++px)
            {
                const float c = juce::jmin (1.0f, forme.crete (px * parPixel, (px + 1) * parPixel));
                g.drawVerticalLine (zone.getX() + px, milieu - juce::jmax (0.5f, c * demi), milieu + juce::jmax (0.5f, c * demi));
            }
        }
        else
        {
            g.setColour (Couleurs::accent);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText (fr ("Fichier introuvable : ") + fichier.getFullPathName(), zone, juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        gain.setBounds (64, 30, 130, 22);
        warp.setBounds (214, 30, 96, 22);
        bpm.setBounds (432, 30, 130, 22);
        moitie.setBounds (568, 30, 34, 22);
        dobl.setBounds (606, 30, 34, 22);
    }

private:
    te::Edit& edit;
    Selection& selection;
    juce::TextButton warp, moitie, dobl;
    juce::Slider bpm, gain;

    te::WaveAudioClip* clipAudio()      { return dynamic_cast<te::WaveAudioClip*> (selection.clipValide()); }

    void timerCallback() override
    {
        if (! isShowing())
            return;

        if (auto* clip = clipAudio())
        {
            warp.setToggleState (clip->getAutoTempo(), juce::dontSendNotification);

            if (! bpm.isMouseButtonDown())
                bpm.setValue (Warp::bpmOrigine (*clip), juce::dontSendNotification);

            if (! gain.isMouseButtonDown())
                gain.setValue (clip->getGainDB(), juce::dontSendNotification);
        }

        repaint();
    }
};
