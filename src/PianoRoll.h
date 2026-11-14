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
                     fr ("Les notes du clip. Double-clic : poser ou effacer une note. "
                         "Glisse dans le vide pour entourer plusieurs notes (Maj+clic pour en ajouter). "
                         "Ctrl+C copier, Ctrl+V coller, Ctrl+X couper, Ctrl+D dupliquer, Ctrl+A tout choisir. "
                         "Flèches : déplacer. Suppr : effacer."));

        addChildComponent (menuLongueur);
        menuLongueur.addItemList ({ "1 mesure", "2 mesures", "4 mesures", "8 mesures", "16 mesures" }, 1);
        menuLongueur.setWantsKeyboardFocus (false);
        menuLongueur.onChange = [this]
        {
            static const int mesures[] = { 1, 2, 4, 8, 16 };
            const int i = menuLongueur.getSelectedItemIndex();

            if (auto* clip = selection.clipMidiValide(); clip != nullptr && i >= 0 && i < 5)
            {
                edit.getUndoManager().beginNewTransaction();
                clip->setEnd (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (debutClip (*clip) + mesures[i] * 4.0)), false);
            }
        };
        definirAide (menuLongueur, fr ("Longueur du clip"),
                     fr ("Choisis la longueur du clip, en mesures (une mesure = 4 temps). Si tu raccourcis, les notes qui "
                         "dépassent ne sont pas effacées : elles reviennent quand tu rallonges."));

        addChildComponent (crayon);
        crayon.setButtonText (fr ("Crayon (B)"));
        crayon.setClickingTogglesState (true);
        crayon.setWantsKeyboardFocus (false);
        definirAide (crayon, fr ("Crayon"),
                     fr ("Quand le crayon est allumé, un simple clic pose une note et un clic sur "
                         "une note l'efface. Éteins-le pour sélectionner et déplacer. Raccourci : B."));

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

        auto* clip = selection.clipMidiValide();
        auto titre = getLocalBounds().removeFromTop (hauteurTitre);

        crayon.setVisible (clip != nullptr);
        menuLongueur.setVisible (clip != nullptr);

        if (auto* son = dynamic_cast<te::WaveAudioClip*> (selection.clipValide()))
        {
            // Un clip audio n'a pas de notes : on affiche simplement de quoi il s'agit
            g.setColour (Couleurs::texte);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawFittedText (fr ("Clip audio : ") + son->getName(), getLocalBounds().reduced (20).removeFromTop (60),
                              juce::Justification::centred, 2);
            g.setColour (Couleurs::texteDoux);
            g.setFont (juce::FontOptions (13.5f));
            g.drawFittedText (fr ("Un clip audio joue un son enregistré : il n'a pas de notes à modifier.\n"
                                  "Dans l'arrangement, glisse-le pour le déplacer et tire son bord droit pour le raccourcir."),
                              getLocalBounds().reduced (20).withTrimmedTop (60), juce::Justification::centredTop, 4);
            g.setColour (Couleurs::bordure);
            g.drawRect (getLocalBounds());
            return;
        }

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
        const auto couleur = numPiste >= 0 ? couleurPiste (*te::getAudioTracks (edit)[numPiste], numPiste)
                                           : Couleurs::accent;
        const auto zone = zoneGrille();
        const auto clavier = zoneClavier();
        const double longueur = longueurClip (*clip);
        const double px = pxParTemps (*clip);

        // Titre
        g.setColour (couleur);
        g.fillRect (titre.removeFromLeft (6));
        g.setColour (Couleurs::texte);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        titre.removeFromLeft (132);     // la place du menu « longueur »
        g.drawText (clip->getName(), titre.reduced (8, 0).removeFromLeft (150), juce::Justification::centredLeft, true);

        if (ligneSurvolee >= 0 && ligneSurvolee < (int) lignes.size())
        {
            // La note sous la souris
            const auto nomNote = estBatterie ? nomsLignes[ligneSurvolee]
                                             : nomDeNote (lignes[(size_t) ligneSurvolee]);
            g.setColour (Couleurs::accent);
            g.drawText (nomNote, titre.reduced (8, 0).withTrimmedLeft (156).removeFromLeft (170),
                        juce::Justification::centredLeft, true);
        }

        // Le menu de longueur reflète la longueur réelle du clip
        {
            const double mesures = longueur_ (*clip) / 4.0;
            const juce::String texte = (std::abs (mesures - std::round (mesures)) < 0.01 ? juce::String (juce::roundToInt (mesures))
                                                                                          : juce::String (mesures, 2))
                                         + (mesures > 1.01 ? " mesures" : " mesure");
            if (! menuLongueur.isPopupActive() && menuLongueur.getText() != texte)
                menuLongueur.setText (texte, juce::dontSendNotification);
        }

        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (crayon.getToggleState()
                        ? fr ("Clic : poser une note   •   Clic sur une note : l'effacer")
                        : fr ("Double-clic : poser / effacer   •   Glisser : sélectionner   •   Ctrl+C / Ctrl+V : copier / coller"),
                    titre.withTrimmedRight (110).withTrimmedLeft (330).reduced (8, 0), juce::Justification::centredRight, true);

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

            const bool survolee = (i == ligneSurvolee);

            if (survolee)
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (zone.getX(), y, zone.getWidth(), hauteurLigne);
            }

            if (estBatterie)
            {
                g.setColour (note == noteApercu ? couleur : survolee ? Couleurs::panneauClair.brighter (0.2f) : Couleurs::panneauClair);
                g.fillRect (touche);
                g.setColour (note == noteApercu ? juce::Colours::black : Couleurs::texte);
                g.setFont (juce::FontOptions (12.5f));
                g.drawText (nomsLignes[i], touche.reduced (6, 0), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour (note == noteApercu ? couleur : (noire ? juce::Colour (0xff222222) : juce::Colour (0xffe8e8e8)));
                g.fillRect (touche);

                if (survolee)
                {
                    g.setColour (Couleurs::accent.withAlpha (0.55f));
                    g.fillRect (touche);
                }

                if (estDo || survolee)
                {
                    g.setColour (noire && ! survolee ? juce::Colours::white : juce::Colours::black);
                    g.setFont (juce::FontOptions (10.5f, survolee ? juce::Font::bold : juce::Font::plain));
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
        validerChoix (*clip);

        for (auto* n : clip->getSequence().getNotes())
        {
            auto r = rectangleNote (*clip, *n);
            if (r.isEmpty())
                continue;

            const bool choisie = notesChoisies.contains (n);
            g.setColour (choisie ? couleur.brighter (0.9f) : couleur);
            g.fillRoundedRectangle (r.toFloat(), 2.0f);
            g.setColour (choisie ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (r.toFloat().reduced (choisie ? 1.0f : 0.5f), 2.0f, choisie ? 2.0f : 1.0f);
        }

        // Point d'insertion (là où Ctrl+V colle)
        if (pointInsertion >= 0.0 && pointInsertion <= longueur)
        {
            g.setColour (Couleurs::solo);
            g.drawVerticalLine (zone.getX() + juce::roundToInt (pointInsertion * px), (float) zone.getY(), (float) zone.getBottom());
        }

        // Rectangle de sélection
        if (mode == Mode::lasso && ! lasso.isEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (lasso);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawRect (lasso);
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

    void resized() override
    {
        crayon.setBounds (getWidth() - 104, 3, 98, hauteurTitre - 6);
        menuLongueur.setBounds (10, 3, 122, hauteurTitre - 6);
        ajusterLignes();
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = Mode::rien;
        auto* clip = selection.clipMidiValide();
        if (clip == nullptr)
            return;

        const int ligne = ligneEnY (e.y);

        if (zoneClavier().contains (e.getPosition()))
        {
            if (ligne >= 0)
                jouerApercu (*clip, lignes[(size_t) ligne]);
            return;
        }

        if (! zoneGrille().contains (e.getPosition()) || ligne < 0)
            return;

        auto& um = edit.getUndoManager();
        um.beginNewTransaction();
        validerChoix (*clip);

        auto* n = noteSous (*clip, e.getPosition());
        const bool ajout = e.mods.isShiftDown();
        duplicationFaite = false;

        if (n != nullptr && (e.mods.isPopupMenu() || crayon.getToggleState()))
        {
            // Clic droit (ou crayon) sur une note : on l'efface
            notesChoisies.removeAllInstancesOf (n);
            clip->getSequence().removeNote (*n, &um);
        }
        else if (n != nullptr)
        {
            if (ajout)
            {
                if (notesChoisies.contains (n))   notesChoisies.removeAllInstancesOf (n);
                else                              notesChoisies.add (n);
            }
            else if (! notesChoisies.contains (n))
            {
                notesChoisies.clearQuick();
                notesChoisies.add (n);
            }

            if (notesChoisies.contains (n))
            {
                const auto r = rectangleNote (*clip, *n);
                commencerGlisser (*clip, e, (e.x > r.getRight() - 6 && r.getWidth() > 12) ? Mode::redimensionner
                                                                                          : Mode::deplacer);
                jouerApercu (*clip, n->getNoteNumber());
            }
        }
        else if (crayon.getToggleState() && ! e.mods.isPopupMenu())
        {
            if (auto* nouvelle = poserNote (*clip, e))
            {
                notesChoisies.clearQuick();
                notesChoisies.add (nouvelle);
                commencerGlisser (*clip, e, Mode::deplacer);
            }
        }
        else if (! e.mods.isPopupMenu())
        {
            // Dans le vide : début d'un rectangle de sélection
            if (! ajout)
                notesChoisies.clearQuick();

            choixAvantLasso = notesChoisies;
            lasso = {};
            mode = Mode::lasso;
            pointInsertion = juce::jlimit (0.0, longueurClip (*clip),
                                           std::round (tempsEnX (*clip, e.x) / grille) * grille);
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* clip = selection.clipMidiValide();
        if (clip == nullptr || mode == Mode::rien)
            return;

        if (mode == Mode::lasso)
        {
            lasso = juce::Rectangle<int> (e.getMouseDownPosition(), e.getPosition()).getIntersection (zoneGrille());
            notesChoisies = choixAvantLasso;

            for (auto* n : clip->getSequence().getNotes())
                if (rectangleNote (*clip, *n).intersects (lasso))
                    notesChoisies.addIfNotAlreadyThere (n);

            repaint();
            return;
        }

        auto& um = edit.getUndoManager();
        auto& notes = clip->getSequence().getNotes();
        const double longueur = longueurClip (*clip);
        const double deltaBrut = std::round ((tempsEnX (*clip, e.x) - tempsPrise) / grille) * grille;

        if (mode == Mode::deplacer)
        {
            double minDebut = 1.0e9, maxFin = 0.0;
            int minLigne = 100000, maxLigne = -1;

            for (auto& o : origines)
            {
                minDebut = juce::jmin (minDebut, o.debut);
                maxFin   = juce::jmax (maxFin, o.debut + o.duree);
                minLigne = juce::jmin (minLigne, o.ligne);
                maxLigne = juce::jmax (maxLigne, o.ligne);
            }

            const double dT = juce::jlimit (-minDebut, juce::jmax (-minDebut, longueur - maxFin), deltaBrut);
            const int dL = juce::jlimit (-minLigne, juce::jmax (-minLigne, (int) lignes.size() - 1 - maxLigne),
                                         ligneSansBorne (e.y) - lignePrise);

            // Ctrl + glisser : on laisse une copie des notes à leur place d'origine, comme dans Ableton
            if (e.mods.isCommandDown() && ! duplicationFaite && (dT != 0.0 || dL != 0))
            {
                duplicationFaite = true;

                for (auto& o : origines)
                    if (notes.contains (o.note))
                        clip->getSequence().addNote (lignes[(size_t) o.ligne], te::BeatPosition::fromBeats (o.debut),
                                                     te::BeatDuration::fromBeats (o.duree), o.note->getVelocity(), 0, &um);
            }

            for (auto& o : origines)
            {
                if (! notes.contains (o.note))
                    continue;

                if (o.debut + dT != o.note->getStartBeat().inBeats())
                    o.note->setStartAndLength (te::BeatPosition::fromBeats (o.debut + dT),
                                               te::BeatDuration::fromBeats (o.duree), &um);

                const int hauteur = lignes[(size_t) (o.ligne + dL)];
                if (hauteur != o.note->getNoteNumber())
                    o.note->setNoteNumber (hauteur, &um);
            }

            if (dL != dernierDeltaLigne && ! origines.empty() && notes.contains (origines.front().note))
            {
                dernierDeltaLigne = dL;
                jouerApercu (*clip, origines.front().note->getNoteNumber());
            }
        }
        else if (mode == Mode::redimensionner)
        {
            for (auto& o : origines)
            {
                if (! notes.contains (o.note))
                    continue;

                const double duree = juce::jlimit (grille, juce::jmax (grille, longueur - o.debut), o.duree + deltaBrut);

                if (duree != o.note->getLengthBeats().inBeats())
                    o.note->setStartAndLength (te::BeatPosition::fromBeats (o.debut),
                                               te::BeatDuration::fromBeats (duree), &um);

                if (! estBatterie && origines.size() == 1)
                    longueurParDefaut = duree;
            }
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        mode = Mode::rien;
        lasso = {};
        origines.clear();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        auto* clip = selection.clipMidiValide();
        if (clip == nullptr || crayon.getToggleState() || ! zoneGrille().contains (e.getPosition()))
            return;

        auto& um = edit.getUndoManager();
        um.beginNewTransaction();

        if (auto* n = noteSous (*clip, e.getPosition()))
        {
            notesChoisies.removeAllInstancesOf (n);
            clip->getSequence().removeNote (*n, &um);
        }
        else if (auto* nouvelle = poserNote (*clip, e))
        {
            notesChoisies.clearQuick();
            notesChoisies.add (nouvelle);
        }

        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        ligneSurvolee = -1;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool bord = false;
        ligneSurvolee = ligneEnY (e.y);

        if (auto* clip = selection.clipMidiValide())
            if (auto* n = noteSous (*clip, e.getPosition()))
            {
                const auto r = rectangleNote (*clip, *n);
                bord = ! crayon.getToggleState() && e.x > r.getRight() - 6 && r.getWidth() > 12;
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

    //==========================================================================
    bool keyPressed (const juce::KeyPress& k) override
    {
        auto* clip = selection.clipMidiValide();
        if (clip == nullptr)
            return false;

        const auto mods = k.getModifiers();
        const int code = k.getKeyCode();

        if (! mods.isCommandDown() && (code == 'B' || code == 'b'))
        {
            crayon.setToggleState (! crayon.getToggleState(), juce::dontSendNotification);
            repaint();
            return true;
        }

        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
            return supprimerNotesChoisies();

        if (mods.isCommandDown())
        {
            switch (code)
            {
                case 'A':   toutChoisir (*clip);                          return true;
                case 'C':   return copier (*clip);
                case 'X':   if (copier (*clip)) supprimerNotesChoisies(); return true;
                case 'V':   coller (*clip);                               return true;
                case 'D':   return dupliquer (*clip);
                default:    return false;
            }
        }

        if (code == juce::KeyPress::leftKey)    return decalerChoix (*clip, -grille, 0);
        if (code == juce::KeyPress::rightKey)   return decalerChoix (*clip, grille, 0);
        if (code == juce::KeyPress::upKey)      return decalerChoix (*clip, 0.0, (mods.isShiftDown() && ! estBatterie) ? -12 : -1);
        if (code == juce::KeyPress::downKey)    return decalerChoix (*clip, 0.0, (mods.isShiftDown() && ! estBatterie) ? 12 : 1);

        return false;
    }

    /** Efface les notes choisies. Renvoie faux s'il n'y en avait pas. */
    bool supprimerNotesChoisies()
    {
        auto* clip = selection.clipMidiValide();
        if (clip == nullptr)
            return false;

        validerChoix (*clip);
        if (notesChoisies.isEmpty())
            return false;

        auto& um = edit.getUndoManager();
        um.beginNewTransaction();

        for (auto* n : notesChoisies)
            clip->getSequence().removeNote (*n, &um);

        notesChoisies.clearQuick();
        repaint();
        return true;
    }

private:
    enum class Mode { rien, deplacer, redimensionner, lasso };

    struct Origine      { te::MidiNote* note; double debut, duree; int ligne; };
    struct NoteCopiee   { int hauteur; double debut, duree; int velocite; };

    te::Edit& edit;
    Selection& selection;

    std::vector<int> lignes;            // numéro de note de chaque ligne, de haut en bas
    juce::StringArray nomsLignes;
    bool estBatterie = false;
    int hauteurLigne = 16;
    double defilement = 0.0;
    double longueurParDefaut = 0.5;

    juce::TextButton crayon;
    juce::ComboBox menuLongueur;
    int ligneSurvolee = -1;
    bool duplicationFaite = false;

    /** Le nom d'une note, à l'anglaise et à la française (C3 / Do). */
    static juce::String nomDeNote (int note)
    {
        static const char* const francais[] = { "Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
        return juce::MidiMessage::getMidiNoteName (note, true, true, 3) + "  (" + fr (francais[note % 12]) + ")";
    }

    double longueur_ (const te::Clip& c) const   { return longueurClip (c); }
    juce::Array<te::MidiNote*> notesChoisies, choixAvantLasso;
    std::vector<Origine> origines;
    std::vector<NoteCopiee> pressePapiers;
    double longueurPressePapiers = 0.0;
    juce::Rectangle<int> lasso;
    Mode mode = Mode::rien;
    double tempsPrise = 0.0;
    int lignePrise = 0, dernierDeltaLigne = 0;
    double pointInsertion = -1.0;

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

    /** Retire du choix les notes qui n'existent plus (après un « annuler » par exemple). */
    void validerChoix (te::MidiClip& c)
    {
        auto& notes = c.getSequence().getNotes();

        for (int i = notesChoisies.size(); --i >= 0;)
            if (! notes.contains (notesChoisies[i]))
                notesChoisies.remove (i);
    }

    int ligneSansBorne (int y) const
    {
        const int ligne = (int) std::floor ((y - zoneGrille().getY() + defilement) / (double) hauteurLigne);
        return juce::jlimit (0, juce::jmax (0, (int) lignes.size() - 1), ligne);
    }

    void commencerGlisser (te::MidiClip& c, const juce::MouseEvent& e, Mode m)
    {
        origines.clear();

        for (auto* n : notesChoisies)
            origines.push_back ({ n, n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(),
                                  juce::jmax (0, ligneDeNote (n->getNoteNumber())) });

        tempsPrise = tempsEnX (c, e.x);
        lignePrise = ligneSansBorne (e.y);
        dernierDeltaLigne = 0;
        mode = m;
    }

    te::MidiNote* poserNote (te::MidiClip& c, const juce::MouseEvent& e)
    {
        const int ligne = ligneEnY (e.y);
        if (ligne < 0)
            return nullptr;

        const int hauteur = lignes[(size_t) ligne];
        const double longueur = longueurClip (c);
        const double debut = juce::jlimit (0.0, juce::jmax (0.0, longueur - grille),
                                           std::floor (tempsEnX (c, e.x) / grille) * grille);
        const double duree = juce::jmin (estBatterie ? grille : longueurParDefaut, longueur - debut);

        jouerApercu (c, hauteur);
        return c.getSequence().addNote (hauteur, te::BeatPosition::fromBeats (debut),
                                        te::BeatDuration::fromBeats (duree), 100, 0, &edit.getUndoManager());
    }

    void toutChoisir (te::MidiClip& c)
    {
        notesChoisies = c.getSequence().getNotes();
        repaint();
    }

    bool copier (te::MidiClip& c)
    {
        validerChoix (c);
        if (notesChoisies.isEmpty())
            return false;

        double debut = 1.0e9, fin = 0.0;
        for (auto* n : notesChoisies)
        {
            debut = juce::jmin (debut, n->getStartBeat().inBeats());
            fin   = juce::jmax (fin, n->getEndBeat().inBeats());
        }

        pressePapiers.clear();
        for (auto* n : notesChoisies)
            pressePapiers.push_back ({ n->getNoteNumber(), n->getStartBeat().inBeats() - debut,
                                       n->getLengthBeats().inBeats(), n->getVelocity() });

        // Longueur arrondie au temps supérieur : pratique pour répéter un motif
        longueurPressePapiers = juce::jmax (grille, std::ceil ((fin - debut) - 1.0e-6));
        pointInsertion = debut + longueurPressePapiers;
        repaint();
        return true;
    }

    /** Colle les notes copiées au point d'insertion ; allonge le clip si besoin. */
    void coller (te::MidiClip& c)
    {
        if (pressePapiers.empty())
            return;

        auto& um = edit.getUndoManager();
        um.beginNewTransaction();

        const double position = juce::jmax (0.0, pointInsertion);
        double fin = 0.0;
        for (auto& n : pressePapiers)
            fin = juce::jmax (fin, position + n.debut + n.duree);

        if (fin > longueurClip (c) + 1.0e-6)
        {
            const double nouvelleFin = debutClip (c) + std::ceil (fin / 4.0) * 4.0;
            c.setEnd (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (nouvelleFin)), false);
        }

        notesChoisies.clearQuick();

        for (auto& n : pressePapiers)
            if (auto* nouvelle = c.getSequence().addNote (n.hauteur, te::BeatPosition::fromBeats (position + n.debut),
                                                          te::BeatDuration::fromBeats (n.duree), n.velocite, 0, &um))
                notesChoisies.add (nouvelle);

        pointInsertion = position + longueurPressePapiers;
        repaint();
    }

    bool dupliquer (te::MidiClip& c)
    {
        const auto ancien = pressePapiers;
        const double ancienneLongueur = longueurPressePapiers;

        if (! copier (c))
            return false;

        coller (c);
        pressePapiers = ancien;
        longueurPressePapiers = ancienneLongueur;
        return true;
    }

    bool decalerChoix (te::MidiClip& c, double deltaTemps, int deltaLignes)
    {
        validerChoix (c);
        if (notesChoisies.isEmpty())
            return false;

        const double longueur = longueurClip (c);

        // On ne bouge que si toutes les notes restent dans le clip et dans la grille
        for (auto* n : notesChoisies)
        {
            const double debut = n->getStartBeat().inBeats() + deltaTemps;
            const int ligne = ligneDeNote (n->getNoteNumber()) + deltaLignes;

            if (debut < -1.0e-6 || debut + n->getLengthBeats().inBeats() > longueur + 1.0e-6
                || ligne < 0 || ligne >= (int) lignes.size())
                return true;
        }

        auto& um = edit.getUndoManager();
        um.beginNewTransaction();

        for (auto* n : notesChoisies)
        {
            if (deltaTemps != 0.0)
                n->setStartAndLength (te::BeatPosition::fromBeats (n->getStartBeat().inBeats() + deltaTemps),
                                      n->getLengthBeats(), &um);
            if (deltaLignes != 0)
                n->setNoteNumber (lignes[(size_t) (ligneDeNote (n->getNoteNumber()) + deltaLignes)], &um);
        }

        if (deltaLignes != 0)
            jouerApercu (c, notesChoisies.getFirst()->getNoteNumber());

        repaint();
        return true;
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

        auto* clip = selection.clipMidiValide();
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
                else if (piste != nullptr && piste->getName().containsIgnoreCase ("basse"))
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
        auto* clip = selection.clipMidiValide();
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
        notesChoisies.clearQuick();
        pointInsertion = -1.0;
        auto* clip = selection.clipMidiValide();
        dernierePiste = clip != nullptr ? Selection::indexPiste (edit, *clip) : -1;
        ajusterLignes (true);
        repaint();
    }
};
