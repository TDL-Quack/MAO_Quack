#pragma once

#include "Commun.h"
#include "Instruments.h"

//==============================================================================
/** Le piano roll (en bas) : on y dessine les notes du clip choisi. */
class PianoRoll : public juce::Component,
                  private juce::Timer,
                  private juce::ChangeListener
{
public:
    static constexpr int hauteurTitre = 26;
    static constexpr double grille = 0.25;      // un quart de temps (double-croche)

    PianoRoll (te::Edit& e, Selection& sel) : edit (e), selection (sel)
    {
        setWantsKeyboardFocus (true);
        selection.addChangeListener (this);

        definirAide (*this, fr ("Piano roll"),
                     fr ("Les notes du clip choisi. Clique dans la grille pour poser une note, "
                         "glisse-la pour la déplacer, tire son bord droit pour l'allonger. "
                         "Double-clic ou clic droit sur une note : l'effacer. "
                         "Plus haut = plus aigu, plus bas = plus grave."));
        startTimerHz (20);
    }

    ~PianoRoll() override
    {
        arreterApercu();
        selection.removeChangeListener (this);
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);

        auto* clip = selection.clipValide();
        auto titre = getLocalBounds().removeFromTop (hauteurTitre);

        if (clip == nullptr)
        {
            g.setColour (Couleurs::texteDoux);
            g.setFont (juce::FontOptions (14.5f));
            g.drawFittedText (fr ("Double-clique sur une piste, en haut, pour créer un clip.\n"
                                  "Clique ensuite sur le clip : ses notes s'affichent ici."),
                              getLocalBounds().reduced (20), juce::Justification::centred, 3);
            g.setColour (Couleurs::bordure);
            g.drawRect (getLocalBounds());
            return;
        }

        const int numPiste = Selection::indexPiste (edit, *clip);
        const auto couleur = Couleurs::dePiste (numPiste);
        const auto zone = zoneGrille();
        const auto clavier = zoneClavier();
        const double longueur = longueurClip (*clip);
        const double px = pxParTemps (*clip);

        // Titre
        g.setColour (couleur);
        g.fillRect (titre.removeFromLeft (6));
        g.setColour (Couleurs::texte);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (clip->getName(), titre.reduced (8, 0), juce::Justification::centredLeft, true);
        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (fr ("Clic : poser une note   •   Double-clic : l'effacer"),
                    titre.reduced (8, 0), juce::Justification::centredRight, true);

        g.saveState();
        g.reduceClipRegion (zone.getUnion (clavier));

        // Lignes (une par note)
        for (int i = 0; i < (int) lignes.size(); ++i)
        {
            const int note = lignes[(size_t) i];
            const int y = yLigne (i);
            if (y + hauteurLigne < zone.getY() || y > zone.getBottom())
                continue;

            const bool noire = ! estBatterie && juce::MidiMessage::isMidiNoteBlack (note);
            const bool estDo = ! estBatterie && note % 12 == 0;

            g.setColour (noire ? Couleurs::pisteAlt.darker (0.25f) : Couleurs::piste.brighter (0.06f));
            g.fillRect (zone.getX(), y, zone.getWidth(), hauteurLigne);
            g.setColour (Couleurs::bordure.withAlpha (0.6f));
            g.drawHorizontalLine (y + hauteurLigne - 1, (float) zone.getX(), (float) zone.getRight());

            // Clavier / noms des sons
            auto touche = juce::Rectangle<int> (clavier.getX(), y, clavier.getWidth(), hauteurLigne - 1);

            if (estBatterie)
            {
                g.setColour (note == noteApercu ? couleur : Couleurs::panneauClair);
                g.fillRect (touche);
                g.setColour (note == noteApercu ? juce::Colours::black : Couleurs::texte);
                g.setFont (juce::FontOptions (12.5f));
                g.drawText (nomsLignes[i], touche.reduced (6, 0), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour (note == noteApercu ? couleur : (noire ? juce::Colour (0xff222222) : juce::Colour (0xffe8e8e8)));
                g.fillRect (touche);

                if (estDo)
                {
                    g.setColour (juce::Colours::black);
                    g.setFont (juce::FontOptions (10.5f));
                    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                                touche.reduced (4, 0), juce::Justification::centredRight, false);
                }
            }
        }

        g.restoreState();
        g.saveState();
        g.reduceClipRegion (zone);

        // Lignes verticales
        const int nbPas = (int) std::ceil (longueur / grille);
        for (int p = 0; p <= nbPas; ++p)
        {
            const int x = zone.getX() + juce::roundToInt (p * grille * px);
            const bool temps = (p % 4) == 0;
            const bool mesure = (p % 16) == 0;

            if (! temps && px * grille < 6.0)
                continue;

            g.setColour (mesure ? Couleurs::texteDoux : temps ? Couleurs::grilleMesure : Couleurs::grille);
            g.drawVerticalLine (x, (float) zone.getY(), (float) zone.getBottom());
        }

        // Notes
        auto* choisie = noteValide (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            auto r = rectangleNote (*clip, *n);
            if (r.isEmpty())
                continue;

            g.setColour (n == choisie ? couleur.brighter (0.5f) : couleur);
            g.fillRoundedRectangle (r.toFloat(), 2.0f);
            g.setColour (n == choisie ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 2.0f, 1.0f);
        }

        // Trait de lecture
        const double tempsClip = edit.tempoSequence.toBeats (edit.getTransport().getPosition()).inBeats()
                                    - debutClip (*clip);
        if (tempsClip >= 0.0 && tempsClip <= longueur)
        {
            g.setColour (Couleurs::accent);
            g.fillRect (zone.getX() + juce::roundToInt (tempsClip * px), zone.getY(), 2, zone.getHeight());
        }

        g.restoreState();

        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());
        g.drawHorizontalLine (hauteurTitre - 1, 0.0f, (float) getWidth());
    }

    void resized() override     { ajusterLignes(); }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = Mode::rien;
        auto* clip = selection.clipValide();
        if (clip == nullptr)
            return;

        const int ligne = ligneEnY (e.y);
        if (ligne < 0)
            return;

        const int note = lignes[(size_t) ligne];

        if (zoneClavier().contains (e.getPosition()))
        {
            jouerApercu (*clip, note);
            return;
        }

        if (! zoneGrille().contains (e.getPosition()))
            return;

        auto& um = edit.getUndoManager();
        um.beginNewTransaction();

        if (auto* n = noteSous (*clip, e.getPosition()))
        {
            if (e.mods.isPopupMenu())
            {
                clip->getSequence().removeNote (*n, &um);
                noteChoisie = nullptr;
                repaint();
                return;
            }

            noteChoisie = n;
            const auto r = rectangleNote (*clip, *n);
            mode = (e.x > r.getRight() - 6 && r.getWidth() > 12) ? Mode::redimensionner : Mode::deplacer;
            decalagePrise = tempsEnX (*clip, e.x) - n->getStartBeat().inBeats();
            jouerApercu (*clip, n->getNoteNumber());
        }
        else if (! e.mods.isPopupMenu())
        {
            const double longueur = longueurClip (*clip);
            const double debut = juce::jlimit (0.0, juce::jmax (0.0, longueur - grille),
                                               std::floor (tempsEnX (*clip, e.x) / grille) * grille);
            const double duree = juce::jmin (estBatterie ? grille : longueurParDefaut, longueur - debut);

            noteChoisie = clip->getSequence().addNote (note, te::BeatPosition::fromBeats (debut),
                                                       te::BeatDuration::fromBeats (duree), 100, 0, &um);
            mode = Mode::deplacer;
            decalagePrise = tempsEnX (*clip, e.x) - debut;
            jouerApercu (*clip, note);
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* clip = selection.clipValide();
        auto* n = clip != nullptr ? noteValide (*clip) : nullptr;

        if (n == nullptr || mode == Mode::rien)
            return;

        auto& um = edit.getUndoManager();
        const double longueur = longueurClip (*clip);
        const double duree = n->getLengthBeats().inBeats();

        if (mode == Mode::deplacer)
        {
            const double debut = juce::jlimit (0.0, juce::jmax (0.0, longueur - duree),
                                               std::round ((tempsEnX (*clip, e.x) - decalagePrise) / grille) * grille);

            if (debut != n->getStartBeat().inBeats())
                n->setStartAndLength (te::BeatPosition::fromBeats (debut), te::BeatDuration::fromBeats (duree), &um);

            const int ligne = ligneEnY (e.y);
            if (ligne >= 0 && lignes[(size_t) ligne] != n->getNoteNumber())
            {
                n->setNoteNumber (lignes[(size_t) ligne], &um);
                jouerApercu (*clip, n->getNoteNumber());
            }
        }
        else
        {
            const double debut = n->getStartBeat().inBeats();
            const double fin = juce::jlimit (debut + grille, juce::jmax (debut + grille, longueur),
                                             std::round (tempsEnX (*clip, e.x) / grille) * grille);

            if (fin - debut != duree)
            {
                n->setStartAndLength (te::BeatPosition::fromBeats (debut), te::BeatDuration::fromBeats (fin - debut), &um);
                if (! estBatterie)
                    longueurParDefaut = fin - debut;
            }
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override     { mode = Mode::rien; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* clip = selection.clipValide())
        {
            if (auto* n = noteSous (*clip, e.getPosition()))
            {
                clip->getSequence().removeNote (*n, &edit.getUndoManager());
                noteChoisie = nullptr;
                repaint();
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool bord = false;

        if (auto* clip = selection.clipValide())
            if (auto* n = noteSous (*clip, e.getPosition()))
            {
                const auto r = rectangleNote (*clip, *n);
                bord = e.x > r.getRight() - 6 && r.getWidth() > 12;
            }

        setMouseCursor (bord ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (estBatterie)
            return;

        defilement -= w.deltaY * 200.0;
        borneDefilement();
        repaint();
    }

    /** Efface la note choisie. Renvoie faux s'il n'y en avait pas. */
    bool supprimerNoteChoisie()
    {
        if (auto* clip = selection.clipValide())
        {
            if (auto* n = noteValide (*clip))
            {
                edit.getUndoManager().beginNewTransaction();
                clip->getSequence().removeNote (*n, &edit.getUndoManager());
                noteChoisie = nullptr;
                repaint();
                return true;
            }
        }

        return false;
    }

private:
    enum class Mode { rien, deplacer, redimensionner };

    te::Edit& edit;
    Selection& selection;

    std::vector<int> lignes;            // numéro de note de chaque ligne, de haut en bas
    juce::StringArray nomsLignes;
    bool estBatterie = false;
    int hauteurLigne = 16;
    double defilement = 0.0;
    double longueurParDefaut = 0.5;

    te::MidiNote* noteChoisie = nullptr;
    Mode mode = Mode::rien;
    double decalagePrise = 0.0;

    te::MPESourceID sourceMidi = te::createUniqueMPESourceID();
    te::AudioTrack::Ptr pisteApercu;
    int noteApercu = -1;
    juce::uint32 finApercu = 0;
    int dernierePiste = -1;

    //==========================================================================
    juce::Rectangle<int> zoneClavier() const
    {
        return getLocalBounds().withTrimmedTop (hauteurTitre).removeFromLeft (estBatterie ? 124 : 56).reduced (1, 1);
    }

    juce::Rectangle<int> zoneGrille() const
    {
        return getLocalBounds().withTrimmedTop (hauteurTitre).withTrimmedLeft (estBatterie ? 124 : 56).reduced (1, 1);
    }

    double debutClip (const te::Clip& c) const
    {
        return edit.tempoSequence.toBeats (c.getPosition().time).getStart().inBeats();
    }

    double longueurClip (const te::Clip& c) const
    {
        return juce::jmax (grille, edit.tempoSequence.toBeats (c.getPosition().time).getLength().inBeats());
    }

    double pxParTemps (const te::Clip& c) const
    {
        return zoneGrille().getWidth() / longueurClip (c);
    }

    double tempsEnX (const te::Clip& c, int x) const
    {
        return (x - zoneGrille().getX()) / pxParTemps (c);
    }

    int yLigne (int ligne) const
    {
        return zoneGrille().getY() + ligne * hauteurLigne - juce::roundToInt (defilement);
    }

    int ligneEnY (int y) const
    {
        const auto zone = zoneGrille();
        if (y < zone.getY() || y >= zone.getBottom())
            return -1;

        const int ligne = (y - zone.getY() + juce::roundToInt (defilement)) / hauteurLigne;
        return ligne >= 0 && ligne < (int) lignes.size() ? ligne : -1;
    }

    int ligneDeNote (int note) const
    {
        for (int i = 0; i < (int) lignes.size(); ++i)
            if (lignes[(size_t) i] == note)
                return i;
        return -1;
    }

    juce::Rectangle<int> rectangleNote (const te::Clip& c, const te::MidiNote& n) const
    {
        const int ligne = ligneDeNote (n.getNoteNumber());
        if (ligne < 0)
            return {};

        const double px = pxParTemps (c);
        const int x = zoneGrille().getX() + juce::roundToInt (n.getStartBeat().inBeats() * px);
        const int w = juce::jmax (4, juce::roundToInt (n.getLengthBeats().inBeats() * px) - 1);
        return { x, yLigne (ligne) + 1, w, hauteurLigne - 2 };
    }

    te::MidiNote* noteSous (te::MidiClip& c, juce::Point<int> p) const
    {
        if (! zoneGrille().contains (p))
            return nullptr;

        auto& notes = c.getSequence().getNotes();
        for (int i = notes.size(); --i >= 0;)
            if (rectangleNote (c, *notes[i]).contains (p))
                return notes[i];

        return nullptr;
    }

    te::MidiNote* noteValide (te::MidiClip& c)
    {
        if (noteChoisie != nullptr && ! c.getSequence().getNotes().contains (noteChoisie))
            noteChoisie = nullptr;

        return noteChoisie;
    }

    //==========================================================================
    void borneDefilement()
    {
        const double maxi = juce::jmax (0.0, (double) lignes.size() * hauteurLigne - zoneGrille().getHeight());
        defilement = juce::jlimit (0.0, maxi, defilement);
    }

    /** Prépare les lignes selon l'instrument de la piste (batterie ou notes). */
    void ajusterLignes (bool recentrer = false)
    {
        lignes.clear();
        nomsLignes.clear();
        estBatterie = false;

        auto* clip = selection.clipValide();
        if (clip == nullptr)
            return;

        const int numPiste = Selection::indexPiste (edit, *clip);
        auto* piste = numPiste >= 0 ? te::getAudioTracks (edit)[numPiste] : nullptr;
        auto* batterie = piste != nullptr ? Instruments::batterieDe (*piste) : nullptr;

        if (batterie != nullptr && batterie->getNumSounds() > 0)
        {
            estBatterie = true;

            for (int i = batterie->getNumSounds(); --i >= 0;)
            {
                lignes.push_back (batterie->getKeyNote (i));
                nomsLignes.add (batterie->getSoundName (i));
            }

            hauteurLigne = juce::jlimit (18, 40, zoneGrille().getHeight() / juce::jmax (1, (int) lignes.size()));
            defilement = 0.0;
        }
        else
        {
            for (int note = 96; note >= 24; --note)
                lignes.push_back (note);

            hauteurLigne = 16;

            if (recentrer)
            {
                // On se place autour des notes du clip, sinon autour du do du milieu
                int centre = 60;
                auto& notes = clip->getSequence().getNotes();

                if (! notes.isEmpty())
                {
                    int somme = 0;
                    for (auto* n : notes)
                        somme += n->getNoteNumber();
                    centre = somme / notes.size();
                }
                else if (numPiste == 1)
                {
                    centre = 43;    // piste de basse : plus grave
                }

                defilement = (96 - centre) * hauteurLigne - zoneGrille().getHeight() / 2.0;
            }
        }

        borneDefilement();
    }

    //==========================================================================
    void jouerApercu (te::MidiClip& c, int note)
    {
        arreterApercu();

        const int numPiste = Selection::indexPiste (edit, c);
        if (numPiste < 0)
            return;

        pisteApercu = te::getAudioTracks (edit)[numPiste];
        noteApercu = note;
        finApercu = juce::Time::getMillisecondCounter() + 220;
        pisteApercu->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), sourceMidi);
    }

    void arreterApercu()
    {
        if (pisteApercu != nullptr && noteApercu >= 0)
            pisteApercu->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, noteApercu), sourceMidi);

        noteApercu = -1;
        pisteApercu = nullptr;
    }

    void timerCallback() override
    {
        if (noteApercu >= 0 && juce::Time::getMillisecondCounter() > finApercu)
            arreterApercu();

        // Le clip a pu changer de piste (donc d'instrument)
        auto* clip = selection.clipValide();
        const int numPiste = clip != nullptr ? Selection::indexPiste (edit, *clip) : -1;

        if (numPiste != dernierePiste)
        {
            dernierePiste = numPiste;
            ajusterLignes (true);
        }

        repaint();
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        noteChoisie = nullptr;
        auto* clip = selection.clipValide();
        dernierePiste = clip != nullptr ? Selection::indexPiste (edit, *clip) : -1;
        ajusterLignes (true);
        repaint();
    }
};
