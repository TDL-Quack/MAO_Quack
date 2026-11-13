#pragma once

#include "Commun.h"

//==============================================================================
/** Le navigateur (à gauche) : pour l'instant, seulement les catégories. */
class Navigateur : public juce::Component
{
public:
    Navigateur()
    {
        definirAide (*this, fr ("Navigateur"),
                     fr ("La réserve de sons : batteries, instruments, boucles et effets. "
                         "Bientôt, tu pourras les faire glisser sur une piste. "
                         "Pour l'instant, chaque piste a déjà son instrument."));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);
        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());

        auto r = getLocalBounds().reduced (10, 8);

        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (fr ("NAVIGATEUR"), r.removeFromTop (20), juce::Justification::centredLeft, false);
        r.removeFromTop (4);

        static const char* const categories[] = { "Batteries", "Instruments", "Boucles et sons",
                                                  "Effets", "Plugins VST" };
        int i = 0;
        for (auto* c : categories)
        {
            auto ligne = r.removeFromTop (28);
            g.setColour (Couleurs::dePiste (i++ * 2));
            g.fillRoundedRectangle (ligne.removeFromLeft (10).withSizeKeepingCentre (10, 10).toFloat(), 2.0f);
            ligne.removeFromLeft (8);
            g.setColour (Couleurs::texte);
            g.setFont (juce::FontOptions (14.5f));
            g.drawText (fr (c), ligne, juce::Justification::centredLeft, true);
        }

        r.removeFromTop (12);
        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.5f));
        g.drawFittedText (fr ("Les sons arrivent dans les prochaines étapes."),
                          r.removeFromTop (40), juce::Justification::topLeft, 3);
    }
};

//==============================================================================
/** Le panneau du bas : les instruments et effets de la piste (à venir). */
class PanneauDetail : public juce::Component
{
public:
    PanneauDetail()
    {
        definirAide (*this, fr ("Instruments et effets"),
                     fr ("Ici s'afficheront l'instrument et les effets de la piste choisie, "
                         "avec leurs boutons et leurs dessins animés."));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);
        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());

        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (fr ("Instruments et effets de la piste — bientôt"),
                    getLocalBounds(), juce::Justification::centred, true);
    }
};
