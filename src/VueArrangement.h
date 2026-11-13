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

    VueArrangement (te::Edit& e, Selection& sel) : edit (e), selection (sel)
    {
        setWantsKeyboardFocus (true);

        int i = 0;
        for (auto* t : te::getAudioTracks (edit))
            addAndMakeVisible (enTetes.add (new EnTetePiste (*t, i++)));

        definirAide (*this, fr ("Arrangement"),
                     fr ("Ton morceau, de gauche à droite. Double-clique sur une piste pour créer un clip "
                         "(un bloc de notes). Glisse un clip pour le déplacer, tire son bord droit pour "
                         "l'allonger. Suppr : effacer. Ctrl+D : dupliquer. Ctrl+C puis Ctrl+V : coller au trait "
                         "de lecture. Ctrl+Z : annuler. "
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

        // Clips
        auto* choisi = selection.clipValide();
        int numPiste = 0;

        for (auto* piste : te::getAudioTracks (edit))
        {
            for (auto* clip : piste->getClips())
            {
                auto r = rectangleClip (*clip, numPiste).getIntersection (zone.expanded (4, 0));
                if (r.isEmpty())
                    continue;

                auto full = rectangleClip (*clip, numPiste);
                const auto couleur = Couleurs::dePiste (numPiste);
                const bool estChoisi = (clip == choisi);

                g.saveState();
                g.reduceClipRegion (zone);

                g.setColour (couleur.withMultipliedBrightness (estChoisi ? 0.75f : 0.5f));
                g.fillRoundedRectangle (full.toFloat(), 3.0f);
                g.setColour (couleur);
                g.fillRoundedRectangle (full.withHeight (16).toFloat(), 3.0f);

                g.setColour (juce::Colours::black);
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                g.drawText (clip->getName(), full.withHeight (16).reduced (5, 0),
                            juce::Justification::centredLeft, true);

                // Aperçu des notes
                if (auto* mc = dynamic_cast<te::MidiClip*> (clip))
                {
                    auto& notes = mc->getSequence().getNotes();
                    int bas = 127, haut = 0;
                    for (auto* n : notes)
                    {
                        bas = juce::jmin (bas, n->getNoteNumber());
                        haut = juce::jmax (haut, n->getNoteNumber());
                    }

                    auto zoneNotes = full.withTrimmedTop (18).reduced (0, 3);
                    const double etendue = juce::jmax (7, haut - bas + 1);
                    g.setColour (estChoisi ? juce::Colours::white : couleur.brighter (0.6f));

                    for (auto* n : notes)
                    {
                        const double y = zoneNotes.getBottom() - (n->getNoteNumber() - bas + 1) / etendue * zoneNotes.getHeight();
                        const float x1 = (float) (full.getX() + n->getStartBeat().inBeats() * pxParTemps);
                        const float w = (float) juce::jmax (2.0, n->getLengthBeats().inBeats() * pxParTemps - 1.0);
                        g.fillRect (x1, (float) y, juce::jmin (w, (float) full.getRight() - x1),
                                    (float) juce::jmax (2.0, zoneNotes.getHeight() / etendue - 1.0));
                    }
                }

                if (estChoisi)
                {
                    g.setColour (juce::Colours::white);
                    g.drawRoundedRectangle (full.toFloat().reduced (0.5f), 3.0f, 1.5f);
                }

                g.restoreState();
            }

            ++numPiste;
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
    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = Mode::rien;

        if (e.x >= zonePistes().getRight())
            return;

        if (e.y < hauteurRegle)
        {
            mode = Mode::lecture;
            deplacerLecture (e);
            return;
        }

        if (auto* clip = clipSous (e.getPosition()))
        {
            selection.choisir (clip);
            edit.getUndoManager().beginNewTransaction();

            const auto r = rectangleClip (*clip, Selection::indexPiste (edit, *clip));
            const auto temps = tempsDuClip (*clip);
            mode = (e.x > r.getRight() - 8 && r.getWidth() > 16) ? Mode::redimensionner : Mode::deplacer;
            decalagePrise = xVersTemps (e.x) - temps.getStart().inBeats();
        }
        else
        {
            selection.choisir (nullptr);
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mode == Mode::lecture)
        {
            deplacerLecture (e);
            return;
        }

        auto* clip = selection.clipValide();
        if (clip == nullptr || mode == Mode::rien || e.getDistanceFromDragStart() < 3)
            return;

        const auto temps = tempsDuClip (*clip);

        if (mode == Mode::deplacer)
        {
            const double debut = juce::jmax (0.0, std::round (xVersTemps (e.x) - decalagePrise));
            if (debut != temps.getStart().inBeats())
                clip->setStart (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (debut)), false, true);

            const int cible = (e.y - zonePistes().getY()) / hauteurPiste;
            auto pistes = te::getAudioTracks (edit);

            if (e.y >= zonePistes().getY() && cible < pistes.size()
                && cible != Selection::indexPiste (edit, *clip))
                clip->moveTo (*pistes[cible]);
        }
        else if (mode == Mode::redimensionner)
        {
            const double fin = juce::jmax (temps.getStart().inBeats() + 1.0, std::round (xVersTemps (e.x)));
            if (fin != temps.getEnd().inBeats())
                clip->setEnd (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (fin)), false);
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override     { mode = Mode::rien; }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool bord = false;

        if (auto* clip = clipSous (e.getPosition()))
        {
            const auto r = rectangleClip (*clip, Selection::indexPiste (edit, *clip));
            bord = e.x > r.getRight() - 8 && r.getWidth() > 16;
        }

        setMouseCursor (bord ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto zone = zonePistes();
        auto pistes = te::getAudioTracks (edit);
        const int numPiste = (e.y - zone.getY()) / hauteurPiste;

        if (e.y < zone.getY() || e.x >= zone.getRight() || numPiste >= pistes.size()
            || clipSous (e.getPosition()) != nullptr)
            return;

        // Un nouveau clip d'une mesure, calé sur le début de la mesure
        const double debut = std::floor (juce::jmax (0.0, xVersTemps (e.x)) / tempsParMesure) * tempsParMesure;
        creerClip (*pistes[numPiste], debut, (double) tempsParMesure);
    }

    //==========================================================================
    te::MidiClip* creerClip (te::AudioTrack& piste, double debutEnTemps, double longueurEnTemps)
    {
        edit.getUndoManager().beginNewTransaction();

        const te::BeatRange temps (te::BeatPosition::fromBeats (debutEnTemps),
                                   te::BeatPosition::fromBeats (debutEnTemps + longueurEnTemps));

        auto clip = piste.insertMIDIClip (piste.getName(), edit.tempoSequence.toTime (temps), nullptr);
        selection.choisir (clip.get());
        repaint();
        return clip.get();
    }

    bool supprimerClipChoisi()
    {
        if (auto* clip = selection.clipValide())
        {
            edit.getUndoManager().beginNewTransaction();
            te::Clip::Ptr garde (clip);
            selection.choisir (nullptr);
            garde->removeFromParent();
            repaint();
            return true;
        }

        return false;
    }

    void copierClipChoisi()
    {
        clipCopie = selection.clipValide();
    }

    /** Colle le clip copié sur sa piste, à l'endroit du trait de lecture. */
    void collerClip()
    {
        if (clipCopie == nullptr)
            return;

        int numPiste = Selection::indexPiste (edit, *clipCopie);
        if (auto* choisi = selection.clipValide())
            numPiste = Selection::indexPiste (edit, *choisi);

        auto pistes = te::getAudioTracks (edit);
        if (numPiste < 0 || numPiste >= pistes.size())
            numPiste = 0;

        const double longueur = edit.tempoSequence.toBeats (clipCopie->getPosition().time).getLength().inBeats();
        const juce::String nom = clipCopie->getName();

        if (auto* copie = creerClip (*pistes[numPiste], std::round (positionEnTemps()), longueur))
        {
            copie->setName (nom);
            copie->getSequence().copyFrom (clipCopie->getSequence(), &edit.getUndoManager());
        }
    }

    void dupliquerClipChoisi()
    {
        auto* clip = selection.clipValide();
        const int numPiste = clip != nullptr ? Selection::indexPiste (edit, *clip) : -1;

        if (numPiste < 0)
            return;

        const auto temps = tempsDuClip (*clip);
        const juce::String nom = clip->getName();
        te::MidiClip::Ptr source (clip);

        if (auto* copie = creerClip (*te::getAudioTracks (edit)[numPiste],
                                     temps.getEnd().inBeats(), temps.getLength().inBeats()))
        {
            copie->setName (nom);
            copie->getSequence().copyFrom (source->getSequence(), &edit.getUndoManager());
        }
    }

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
    enum class Mode { rien, lecture, deplacer, redimensionner };

    te::Edit& edit;
    Selection& selection;
    juce::OwnedArray<EnTetePiste> enTetes;
    Mode mode = Mode::rien;
    double decalagePrise = 0.0;
    int compteur = 0;
    te::MidiClip::Ptr clipCopie;

    te::BeatRange tempsDuClip (const te::Clip& c) const
    {
        return edit.tempoSequence.toBeats (c.getPosition().time);
    }

    juce::Rectangle<int> rectangleClip (const te::Clip& c, int numPiste) const
    {
        const auto t = tempsDuClip (c);
        const int x1 = tempsVersX (t.getStart().inBeats());
        const int x2 = tempsVersX (t.getEnd().inBeats());
        return { x1, zonePistes().getY() + numPiste * hauteurPiste + 2, juce::jmax (4, x2 - x1 - 1), hauteurPiste - 4 };
    }

    te::MidiClip* clipSous (juce::Point<int> p) const
    {
        const auto zone = zonePistes();
        if (! zone.contains (p))
            return nullptr;

        auto pistes = te::getAudioTracks (edit);
        const int numPiste = (p.y - zone.getY()) / hauteurPiste;

        if (numPiste >= 0 && numPiste < pistes.size())
        {
            auto& clips = pistes[numPiste]->getClips();
            for (int i = clips.size(); --i >= 0;)
                if (rectangleClip (*clips[i], numPiste).contains (p))
                    return dynamic_cast<te::MidiClip*> (clips[i]);
        }

        return nullptr;
    }

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

        // Rafraîchissement régulier (les clips peuvent changer depuis le piano roll)
        if (x != dernierXLecture || ++compteur % 5 == 0)
        {
            dernierXLecture = x;
            repaint();
        }
    }
};
