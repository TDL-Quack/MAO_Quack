#pragma once

#include "Commun.h"

//==============================================================================
/** Petit bouton dessiné (lecture, stop, métronome). */
class BoutonForme : public juce::Button
{
public:
    enum class Forme { lecture, stop, metronome };

    BoutonForme (const juce::String& nom, Forme f, juce::Colour couleurActive)
        : juce::Button (nom), forme (f), active (couleurActive) {}

    void paintButton (juce::Graphics& g, bool survol, bool enfonce) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool allume = getToggleState();

        g.setColour (allume ? active : (enfonce ? Couleurs::bordure
                                                : survol ? Couleurs::panneauClair.brighter (0.15f)
                                                         : Couleurs::panneauClair));
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (allume ? juce::Colours::black : Couleurs::texte);
        auto i = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) * 0.5f,
                                          juce::jmin (r.getWidth(), r.getHeight()) * 0.5f);
        juce::Path p;

        switch (forme)
        {
            case Forme::lecture:
                p.addTriangle (i.getX(), i.getY(), i.getX(), i.getBottom(), i.getRight(), i.getCentreY());
                g.fillPath (p);
                break;

            case Forme::stop:
                g.fillRect (i);
                break;

            case Forme::metronome:
                g.fillEllipse (i.getX(), i.getCentreY() - i.getHeight() * 0.25f,
                               i.getHeight() * 0.5f, i.getHeight() * 0.5f);
                g.drawEllipse (i.getCentreX(), i.getCentreY() - i.getHeight() * 0.25f,
                               i.getHeight() * 0.5f, i.getHeight() * 0.5f, 1.5f);
                break;
        }
    }

private:
    Forme forme;
    juce::Colour active;
};

//==============================================================================
/** La barre du haut : tempo, métronome, lecture, stop, position. */
class BarreTransport : public juce::Component,
                       private juce::Timer
{
public:
    BarreTransport (te::Edit& e) : edit (e)
    {
        addAndMakeVisible (tempo);
        tempo.setSliderStyle (juce::Slider::LinearBar);
        tempo.setRange (60.0, 200.0, 1.0);
        tempo.setTextValueSuffix (" BPM");
        tempo.setValue (edit.tempoSequence.getTempo (0)->getBpm(), juce::dontSendNotification);
        tempo.onValueChange = [this] { edit.tempoSequence.getTempo (0)->setBpm (tempo.getValue()); };
        definirAide (tempo, fr ("Tempo"),
                     fr ("La vitesse du morceau, en battements par minute (BPM). "
                         "Glisse vers la droite pour aller plus vite, vers la gauche pour ralentir. "
                         "120 est un bon départ."));

        addAndMakeVisible (metronome);
        metronome.setClickingTogglesState (true);
        metronome.setToggleState (edit.clickTrackEnabled.get(), juce::dontSendNotification);
        metronome.onClick = [this] { edit.clickTrackEnabled = metronome.getToggleState(); };
        definirAide (metronome, fr ("Métronome"),
                     fr ("Fait « tic, tic, tic » pendant la lecture pour t'aider à rester en rythme. "
                         "Clique pour l'allumer ou l'éteindre."));

        addAndMakeVisible (lecture);
        lecture.onClick = [this] { lire(); };
        definirAide (lecture, fr ("Lecture"),
                     fr ("Lance le morceau à partir du trait de lecture. "
                         "Raccourci : la barre d'espace."));

        addAndMakeVisible (stop);
        stop.onClick = [this] { arreter(); };
        definirAide (stop, fr ("Stop"),
                     fr ("Arrête le morceau. Clique une deuxième fois pour revenir au début."));

        addAndMakeVisible (position);
        position.setJustificationType (juce::Justification::centred);
        position.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::bold));
        position.setColour (juce::Label::backgroundColourId, Couleurs::bordure);
        definirAide (position, fr ("Position"),
                     fr ("Où on en est dans le morceau : mesure . temps. "
                         "Une mesure contient 4 temps."));

        startTimerHz (20);
    }


    void lire()
    {
        auto& t = edit.getTransport();
        if (! t.isPlaying())
            t.play (false);
    }

    void arreter()
    {
        auto& t = edit.getTransport();
        if (t.isPlaying())
            t.stop (false, false);
        else
            t.setPosition (te::TimePosition());
    }

    void basculerLecture()
    {
        if (edit.getTransport().isPlaying())
            edit.getTransport().stop (false, false);
        else
            lire();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);
        g.setColour (Couleurs::bordure);
        g.fillRect (getLocalBounds().removeFromBottom (1));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 5);

        // Bloc central
        const int largeurBloc = 110 + 8 + 34 + 24 + 40 + 6 + 40 + 16 + 110;
        auto c = r.withSizeKeepingCentre (juce::jmin (largeurBloc, r.getWidth()), r.getHeight());

        tempo.setBounds (c.removeFromLeft (110));
        c.removeFromLeft (8);
        metronome.setBounds (c.removeFromLeft (34));
        c.removeFromLeft (24);
        lecture.setBounds (c.removeFromLeft (40));
        c.removeFromLeft (6);
        stop.setBounds (c.removeFromLeft (40));
        c.removeFromLeft (16);
        position.setBounds (c.removeFromLeft (110));
    }

private:
    te::Edit& edit;

    juce::Label position;
    juce::Slider tempo;
    BoutonForme metronome { "metronome", BoutonForme::Forme::metronome, Couleurs::accent };
    BoutonForme lecture   { "lecture",   BoutonForme::Forme::lecture,   Couleurs::lecture };
    BoutonForme stop      { "stop",      BoutonForme::Forme::stop,      Couleurs::accent };

    void timerCallback() override
    {
        auto& t = edit.getTransport();
        lecture.setToggleState (t.isPlaying(), juce::dontSendNotification);

        const auto bb = edit.tempoSequence.toBarsAndBeats (t.getPosition());
        const int temps = (int) std::floor (bb.beats.inBeats()) + 1;
        position.setText (juce::String (bb.bars + 1) + " . " + juce::String (temps),
                          juce::dontSendNotification);
    }
};
