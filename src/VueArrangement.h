#pragma once

#include "Commun.h"

//==============================================================================
/** L'en-tête d'une piste (à droite, comme dans Ableton) : nom, muet, solo, volume. */
class EnTetePiste : public juce::Component
{
public:
    EnTetePiste (te::AudioTrack& t, int index)
        : piste (t), couleur (Couleurs::dePiste (index))
    {
        piste.setColour (couleur);

        addAndMakeVisible (nom);
        nom.setText (piste.getName(), juce::dontSendNotification);
        nom.setEditable (false, true);
        nom.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        nom.setColour (juce::Label::textColourId, juce::Colours::black);
        nom.setColour (juce::Label::textWhenEditingColourId, juce::Colours::black);
        nom.setColour (juce::Label::backgroundWhenEditingColourId, couleur.brighter (0.4f));
        nom.onTextChange = [this] { piste.setName (nom.getText()); };
        definirAide (nom, fr ("Nom de la piste"),
                     fr ("Une piste, c'est un instrument ou un son de ton morceau. "
                         "Double-clique sur le nom pour le changer."));

        addAndMakeVisible (muet);
        muet.setButtonText ("M");
        muet.setClickingTogglesState (true);
        muet.setColour (juce::TextButton::buttonOnColourId, Couleurs::accent);
        muet.onClick = [this] { piste.setMute (muet.getToggleState()); };
        definirAide (muet, fr ("Muet"),
                     fr ("Coupe le son de cette piste sans rien effacer. "
                         "Pratique pour écouter le morceau sans elle."));

        addAndMakeVisible (solo);
        solo.setButtonText ("S");
        solo.setClickingTogglesState (true);
        solo.setColour (juce::TextButton::buttonOnColourId, Couleurs::solo);
        solo.onClick = [this] { piste.setSolo (solo.getToggleState()); };
        definirAide (solo, fr ("Solo"),
                     fr ("Fait entendre cette piste toute seule : "
                         "toutes les autres se taisent."));

        addAndMakeVisible (volume);
        volume.setSliderStyle (juce::Slider::LinearBar);
        volume.setRange (-60.0, 6.0, 0.1);
        volume.setTextValueSuffix (" dB");
        volume.setDoubleClickReturnValue (true, 0.0);
        volume.setColour (juce::Slider::trackColourId, couleur.withMultipliedSaturation (0.7f).darker (0.2f));

        if (auto* vp = piste.getVolumePlugin())
            volume.setValue (vp->getVolumeDb(), juce::dontSendNotification);

        volume.onValueChange = [this]
        {
            if (auto* vp = piste.getVolumePlugin())
                vp->setVolumeDb ((float) volume.getValue());
        };
        definirAide (volume, fr ("Volume"),
                     fr ("Le niveau sonore de la piste. Glisse à droite pour monter, "
                         "à gauche pour baisser. Double-clique pour revenir à 0 dB."));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (Couleurs::panneau);
        g.fillRect (r);
        g.setColour (couleur);
        g.fillRect (r.removeFromTop (24).reduced (0, 1));
        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1);
        nom.setBounds (r.removeFromTop (24).reduced (4, 0));
        r.reduce (6, 6);

        auto ligne = r.removeFromTop (juce::jmin (24, r.getHeight()));
        muet.setBounds (ligne.removeFromLeft (28));
        ligne.removeFromLeft (4);
        solo.setBounds (ligne.removeFromLeft (28));
        ligne.removeFromLeft (8);
        volume.setBounds (ligne);
    }

private:
    te::AudioTrack& piste;
    juce::Colour couleur;
    juce::Label nom;
    juce::TextButton muet, solo;
    juce::Slider volume;
};

//==============================================================================
/** La vue Arrangement : la règle du temps, les pistes et le trait de lecture. */
class VueArrangement : public juce::Component,
                       private juce::Timer
{
public:
    static constexpr int hauteurRegle   = 26;
    static constexpr int hauteurPiste   = 72;
    static constexpr int largeurEnTete  = 210;
    static constexpr int tempsParMesure = 4;

    VueArrangement (te::Edit& e) : edit (e)
    {
        int i = 0;
        for (auto* t : te::getAudioTracks (edit))
            addAndMakeVisible (enTetes.add (new EnTetePiste (*t, i++)));

        definirAide (*this, fr ("Arrangement"),
                     fr ("C'est ici que tu construis ton morceau, de gauche à droite, comme une frise. "
                         "Clique dans la règle du haut pour déplacer le trait de lecture. "
                         "Molette : se déplacer. Ctrl + molette : zoomer."));

        startTimerHz (30);
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::fond);

        const auto zone   = zonePistes();
        const auto regle  = zoneRegle();
        const int nbPistes = enTetes.size();

        // Fond des pistes
        for (int i = 0; i < nbPistes; ++i)
        {
            g.setColour (i % 2 == 0 ? Couleurs::piste : Couleurs::pisteAlt);
            g.fillRect (zone.getX(), zone.getY() + i * hauteurPiste, zone.getWidth(), hauteurPiste);
        }

        // Règle
        g.setColour (Couleurs::panneau);
        g.fillRect (regle);

        // Grille
        const int basPistes = zone.getY() + nbPistes * hauteurPiste;
        const int premierTemps = juce::jmax (0, (int) std::floor (debutTemps));
        const int dernierTemps = (int) std::ceil (debutTemps + zone.getWidth() / pxParTemps);

        // Écart entre les numéros de mesure pour qu'ils restent lisibles
        int pasMesures = 1;
        while (pasMesures * tempsParMesure * pxParTemps < 48.0)
            pasMesures *= 2;

        g.setFont (juce::FontOptions (12.0f));

        for (int t = premierTemps; t <= dernierTemps; ++t)
        {
            const int x = tempsVersX ((double) t);
            if (x < zone.getX() || x > zone.getRight())
                continue;

            const bool debutMesure = (t % tempsParMesure) == 0;
            const int mesure = t / tempsParMesure;

            if (debutMesure)
            {
                g.setColour (Couleurs::grilleMesure);
                g.drawVerticalLine (x, (float) regle.getY(), (float) basPistes);

                if (mesure % pasMesures == 0)
                {
                    g.setColour (Couleurs::texte);
                    g.drawText (juce::String (mesure + 1), x + 4, regle.getY(), 40, regle.getHeight(),
                                juce::Justification::centredLeft, false);
                }
            }
            else if (pxParTemps >= 10.0)
            {
                g.setColour (Couleurs::grille);
                g.drawVerticalLine (x, (float) zone.getY(), (float) basPistes);
            }
        }

        // Séparations horizontales
        g.setColour (Couleurs::bordure);
        for (int i = 0; i <= nbPistes; ++i)
            g.drawHorizontalLine (zone.getY() + i * hauteurPiste, (float) zone.getX(), (float) zone.getRight());

        g.drawHorizontalLine (regle.getBottom() - 1, (float) regle.getX(), (float) regle.getRight());

        // Message d'attente
        if (basPistes + 40 < zone.getBottom())
        {
            g.setColour (Couleurs::texteDoux);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (fr ("Les clips (notes et sons) arriveront ici à la prochaine étape."),
                        zone.withTop (basPistes + 12).withHeight (24), juce::Justification::centred, true);
        }

        // Trait de lecture
        const int xLecture = tempsVersX (positionEnTemps());
        if (xLecture >= zone.getX() && xLecture <= zone.getRight())
        {
            g.setColour (Couleurs::accent);
            g.fillRect (xLecture, regle.getY(), 2, basPistes - regle.getY());

            juce::Path tri;
            tri.addTriangle ((float) xLecture - 5.0f, (float) regle.getY(),
                             (float) xLecture + 7.0f, (float) regle.getY(),
                             (float) xLecture + 1.0f, (float) regle.getY() + 9.0f);
            g.fillPath (tri);
        }

        // Coin au-dessus des en-têtes
        auto coin = getLocalBounds().removeFromRight (largeurEnTete).removeFromTop (hauteurRegle);
        g.setColour (Couleurs::panneau);
        g.fillRect (coin);
        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (fr ("Pistes"), coin.reduced (8, 0), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().removeFromRight (largeurEnTete);
        r.removeFromTop (hauteurRegle);

        for (auto* e : enTetes)
            e->setBounds (r.removeFromTop (hauteurPiste));
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override     { deplacerLecture (e); }
    void mouseDrag (const juce::MouseEvent& e) override     { deplacerLecture (e); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown())
        {
            // Zoom centré sur la souris
            const double tempsSouris = xVersTemps (e.x);
            pxParTemps = juce::jlimit (3.0, 160.0, pxParTemps * std::pow (1.6, (double) w.deltaY * 3.0));
            debutTemps = juce::jmax (0.0, tempsSouris - (e.x - zonePistes().getX()) / pxParTemps);
        }
        else
        {
            const double delta = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
            debutTemps = juce::jmax (0.0, debutTemps - delta * 400.0 / pxParTemps);
        }

        repaint();
    }

private:
    te::Edit& edit;
    juce::OwnedArray<EnTetePiste> enTetes;

    double pxParTemps = 28.0;   // zoom horizontal : pixels pour un temps
    double debutTemps = 0.0;    // temps affiché tout à gauche
    int dernierXLecture = -1;

    juce::Rectangle<int> zoneRegle() const
    {
        return getLocalBounds().withTrimmedRight (largeurEnTete).removeFromTop (hauteurRegle);
    }

    juce::Rectangle<int> zonePistes() const
    {
        return getLocalBounds().withTrimmedRight (largeurEnTete).withTrimmedTop (hauteurRegle);
    }

    int tempsVersX (double temps) const
    {
        return zonePistes().getX() + juce::roundToInt ((temps - debutTemps) * pxParTemps);
    }

    double xVersTemps (int x) const
    {
        return debutTemps + (x - zonePistes().getX()) / pxParTemps;
    }

    double positionEnTemps() const
    {
        return edit.tempoSequence.toBeats (edit.getTransport().getPosition()).inBeats();
    }

    void deplacerLecture (const juce::MouseEvent& e)
    {
        if (e.getMouseDownY() >= hauteurRegle || e.getMouseDownX() >= zonePistes().getRight())
            return;

        // Aimanté sur le temps le plus proche
        const double temps = juce::jmax (0.0, std::round (xVersTemps (e.x)));
        edit.getTransport().setPosition (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (temps)));
        repaint();
    }

    void timerCallback() override
    {
        const auto zone = zonePistes();
        int x = tempsVersX (positionEnTemps());

        // Pendant la lecture, la vue suit le trait de lecture
        if (edit.getTransport().isPlaying() && x > zone.getRight() - 20)
        {
            debutTemps += (zone.getWidth() * 0.8) / pxParTemps;
            x = tempsVersX (positionEnTemps());
            repaint();
        }

        if (x != dernierXLecture)
        {
            dernierXLecture = x;
            repaint();
        }
    }
};
