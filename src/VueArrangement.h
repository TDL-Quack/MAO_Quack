#pragma once

#include "Commun.h"
#include "Instruments.h"
#include "Effets.h"
#include "FormesOnde.h"
#include "ClipAudio.h"

//==============================================================================
/** L'en-tête d'une piste (à droite, comme dans Ableton) : nom, muet, solo, volume. */
class EnTetePiste : public juce::Component
{
public:
    EnTetePiste (te::AudioTrack& t, int index)
        : garde (&t), piste (t), couleur (couleurPiste (t, index))
    {
        addAndMakeVisible (menu);
        menu.setButtonText ("...");
        menu.setColour (juce::TextButton::buttonColourId, couleur.darker (0.25f));
        menu.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        menu.onClick = [this] { if (surMenu) surMenu (piste, menu); };
        definirAide (menu, fr ("Menu de la piste"),
                     fr ("Pour changer l'instrument d'une piste MIDI, importer un son sur une piste audio, ou supprimer la piste."));

        addAndMakeVisible (nom);
        nom.addMouseListener (this, false);     // un clic sur le nom choisit aussi la piste
        nom.setText (piste.getName(), juce::dontSendNotification);
        nom.setEditable (false, true);
        nom.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        nom.setColour (juce::Label::textColourId, juce::Colours::black);
        nom.setColour (juce::Label::textWhenEditingColourId, juce::Colours::black);
        nom.setColour (juce::Label::backgroundWhenEditingColourId, couleur.brighter (0.4f));
        nom.onTextChange = [this] { piste.setName (nom.getText()); };
        definirAide (nom, fr ("Nom de la piste"),
                     fr ("Une piste, c'est un instrument de ton morceau. Clique pour la choisir : son instrument "
                         "et ses effets s'affichent en bas. Double-clique sur le nom pour le changer."));

        addAndMakeVisible (muet);
        muet.setButtonText ("M");
        muet.setClickingTogglesState (true);
        muet.setColour (juce::TextButton::buttonOnColourId, Couleurs::accent);
        muet.setToggleState (piste.isMuted (false), juce::dontSendNotification);
        muet.onClick = [this] { piste.setMute (muet.getToggleState()); };
        definirAide (muet, fr ("Muet"),
                     fr ("Coupe le son de cette piste sans rien effacer. "
                         "Pratique pour écouter le morceau sans elle."));

        addAndMakeVisible (solo);
        solo.setButtonText ("S");
        solo.setClickingTogglesState (true);
        solo.setColour (juce::TextButton::buttonOnColourId, Couleurs::solo);
        solo.setToggleState (piste.isSolo (false), juce::dontSendNotification);
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
        addChildComponent (parametre);
        parametre.setWantsKeyboardFocus (false);
        parametre.setColour (juce::TextButton::buttonColourId, Couleurs::solo.darker (0.3f));
        parametre.onClick = [this] { if (surParametre) surParametre (piste, parametre); };
        definirAide (parametre, fr ("Réglage automatisé"),
                     fr ("Choisis quel réglage tu dessines sur cette piste : son volume, son panoramique, ou n'importe quel "
                         "potard de son instrument ou de ses effets."));

        for (auto* enfant : std::initializer_list<juce::Component*> { &muet, &solo, &volume, &menu, &parametre })
            enfant->addMouseListener (this, false);

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
        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (TypePiste::estAudio (piste) ? "AUDIO" : "MIDI", getLocalBounds().reduced (8, 3),
                    juce::Justification::bottomLeft, false);

        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());

        if (choisie)
        {
            g.setColour (juce::Colours::white);
            g.drawRect (getLocalBounds(), 2);
        }
    }

    void definirChoisie (bool c)
    {
        if (c != choisie)
        {
            choisie = c;
            repaint();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (1);
        auto bandeau = r.removeFromTop (24);
        menu.setBounds (bandeau.removeFromRight (30).reduced (2, 3));
        nom.setBounds (bandeau.reduced (4, 0));
        r.reduce (6, 6);

        auto ligne = r.removeFromTop (juce::jmin (24, r.getHeight()));
        muet.setBounds (ligne.removeFromLeft (28));
        ligne.removeFromLeft (4);
        solo.setBounds (ligne.removeFromLeft (28));
        ligne.removeFromLeft (8);
        volume.setBounds (ligne);
        parametre.setBounds (ligne);
    }

    std::function<void (te::AudioTrack&, juce::Component&)> surMenu;
    std::function<void (te::AudioTrack&)> surChoix;
    std::function<void (te::AudioTrack&, juce::Component&)> surParametre;

    /** En mode automation, le volume laisse place au bouton qui choisit le réglage à automatiser. */
    void definirAutomation (bool active, const juce::String& nomDuReglage)
    {
        volume.setVisible (! active);
        parametre.setVisible (active);
        parametre.setButtonText (nomDuReglage);
    }

    te::AudioTrack& getPiste()      { return piste; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Un clic n'importe où sur l'en-tête (même sur un bouton) choisit la piste
        if (surChoix)
            surChoix (piste);

        if (e.mods.isPopupMenu() && surMenu && (e.eventComponent == this || e.eventComponent == &nom))
            surMenu (piste, menu);
    }

private:
    te::AudioTrack::Ptr garde;      // garde la piste en vie tant que l'en-tête existe
    te::AudioTrack& piste;
    juce::Colour couleur;
    juce::Label nom;
    juce::TextButton muet, solo, menu;
    bool choisie = false;
    juce::Slider volume;
    juce::TextButton parametre;
};

//==============================================================================
/** La vue Arrangement : la règle du temps (avec la boucle et les repères), les pistes et leurs clips. */
class VueArrangement : public juce::Component,
                       public juce::DragAndDropTarget,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    static constexpr int bandeBoucle    = 13;       // le haut de la règle : la zone de boucle
    static constexpr int hauteurRegle   = 40;
    static constexpr int hauteurPiste   = 72;
    static constexpr int largeurEnTete  = 210;
    static constexpr int tempsParMesure = 4;

    VueArrangement (te::Edit& e, Selection& sel) : edit (e), selection (sel)
    {
        setWantsKeyboardFocus (true);

        addAndMakeVisible (conteneur);
        conteneur.addAndMakeVisible (ajouter);
        ajouter.setButtonText ("+ Ajouter une piste");
        ajouter.onClick = [this] { menuAjouterPiste(); };
        definirAide (ajouter, fr ("Ajouter une piste"),
                     fr ("Ajoute une piste MIDI (des notes jouées par un instrument, à choisir ensuite dans le navigateur) "
                         "ou une piste audio (des sons enregistrés : boucles, voix, effets...)."));

        reconstruireEnTetes();

        definirAide (*this, fr ("Arrangement"),
                     fr ("Ton morceau, de gauche à droite. Double-clic sur une piste MIDI : créer un clip de notes ; sur une "
                         "piste audio : importer un son. Glisse un clip pour le déplacer (Ctrl + glisser : le dupliquer), tire "
                         "son bord droit pour l'allonger. Tout en haut de la règle : glisse pour définir la boucle. "
                         "Double-clic dans la règle : poser un repère. Ctrl + molette : zoomer."));

        startTimerHz (30);
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::fond);

        const auto zone   = zonePistes();
        const auto regle  = zoneRegle();
        const auto pistes = te::getAudioTracks (edit);
        const int nbPistes = pistes.size();
        const int basPistes = juce::jmin (zone.getBottom(), yPiste (nbPistes));
        const int premierTemps = juce::jmax (0, (int) std::floor (debutTemps));
        const int dernierTemps = (int) std::ceil (debutTemps + zone.getWidth() / pxParTemps);
        const auto plageBoucle = boucleEnTemps();
        const double debutBoucle = plageBoucle.getStart().inBeats(), finBoucle = plageBoucle.getEnd().inBeats();
        const bool boucleActive = edit.getTransport().looping.get();

        g.saveState();
        g.reduceClipRegion (zone);

        // Fond des pistes
        for (int i = 0; i < nbPistes; ++i)
        {
            g.setColour (i % 2 == 0 ? Couleurs::piste : Couleurs::pisteAlt);
            g.fillRect (zone.getX(), yPiste (i), zone.getWidth(), hauteurPiste);
        }

        // La zone bouclée, légèrement éclaircie
        if (boucleActive && finBoucle > debutBoucle)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (tempsVersX (debutBoucle), zone.getY(),
                        tempsVersX (finBoucle) - tempsVersX (debutBoucle), basPistes - zone.getY());
        }

        // Grille
        for (int t = premierTemps; t <= dernierTemps; ++t)
        {
            const bool debutMesure = (t % tempsParMesure) == 0;
            if (! debutMesure && pxParTemps < 10.0)
                continue;

            g.setColour (debutMesure ? Couleurs::grilleMesure : Couleurs::grille);
            g.drawVerticalLine (tempsVersX ((double) t), (float) zone.getY(), (float) basPistes);
        }

        g.setColour (Couleurs::bordure);
        for (int i = 0; i <= nbPistes; ++i)
            g.drawHorizontalLine (yPiste (i), (float) zone.getX(), (float) zone.getRight());

        // Clips
        auto* choisi = selection.clipValide();

        for (int numPiste = 0; numPiste < nbPistes; ++numPiste)
        {
            auto* piste = pistes[numPiste];

            for (auto* clip : piste->getClips())
            {
                const auto plein = rectangleClip (*clip, numPiste);
                if (! plein.intersects (zone))
                    continue;

                dessinerClip (g, *clip, plein, couleurPiste (*piste, numPiste), clip == choisi && ! modeAutomation);
            }

            if (modeAutomation)
            {
                // Les clips passent à l'arrière-plan, la courbe d'automation au premier plan
                g.setColour (Couleurs::fond.withAlpha (0.55f));
                g.fillRect (zone.getX(), yPiste (numPiste) + 1, zone.getWidth(), hauteurPiste - 1);
                dessinerAutomation (g, *piste, numPiste);
            }
        }

        // Piste visée pendant un glisser-déposer
        if (pisteSurvolee >= 0 && pisteSurvolee < nbPistes)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (zone.getX(), yPiste (pisteSurvolee), zone.getWidth(), hauteurPiste);
        }

        // Traits des repères
        for (int i = 0; i < Reperes::nombre (edit); ++i)
        {
            g.setColour (Couleurs::solo.withAlpha (0.45f));
            g.drawVerticalLine (tempsVersX (Reperes::temps (edit, i)), (float) zone.getY(), (float) basPistes);
        }

        g.restoreState();

        //======================================================================
        // Règle
        g.setColour (Couleurs::panneau);
        g.fillRect (regle);
        g.setColour (Couleurs::bordure.withAlpha (0.6f));
        g.fillRect (regle.withHeight (bandeBoucle));

        g.saveState();
        g.reduceClipRegion (regle);

        // La boucle : une barre tout en haut
        if (finBoucle > debutBoucle)
        {
            const int x1 = tempsVersX (debutBoucle), x2 = tempsVersX (finBoucle);
            g.setColour (boucleActive ? Couleurs::accent : Couleurs::texteDoux.withAlpha (0.6f));
            g.fillRoundedRectangle ((float) x1, (float) regle.getY() + 2.0f, (float) juce::jmax (4, x2 - x1), (float) bandeBoucle - 4.0f, 3.0f);

            // Les deux curseurs, début et fin, qu'on attrape séparément
            g.setColour (juce::Colours::white);
            g.fillRect (x1, regle.getY() + 1, 3, bandeBoucle - 2);
            g.fillRect (x2 - 3, regle.getY() + 1, 3, bandeBoucle - 2);
        }

        int pasMesures = 1;     // écart entre les numéros pour qu'ils restent lisibles
        while (pasMesures * tempsParMesure * pxParTemps < 48.0)
            pasMesures *= 2;

        g.setFont (juce::FontOptions (12.0f));

        for (int t = premierTemps - (premierTemps % tempsParMesure); t <= dernierTemps; t += tempsParMesure)
        {
            const int x = tempsVersX ((double) t);
            const int mesure = t / tempsParMesure;

            g.setColour (Couleurs::grilleMesure);
            g.drawVerticalLine (x, (float) regle.getY() + bandeBoucle, (float) regle.getBottom());

            if (mesure % pasMesures == 0)
            {
                g.setColour (Couleurs::texte);
                g.drawText (juce::String (mesure + 1), x + 4, regle.getY() + bandeBoucle, 40, 14,
                            juce::Justification::centredLeft, false);
            }
        }

        // Repères : un petit drapeau avec son nom
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (int i = 0; i < Reperes::nombre (edit); ++i)
        {
            const auto r = rectangleRepere (i);
            g.setColour (Couleurs::solo);
            g.fillRoundedRectangle (r.toFloat(), 2.0f);
            g.setColour (juce::Colours::black);
            g.drawText (Reperes::nom (edit, i), r.reduced (4, 0), juce::Justification::centredLeft, true);
        }

        g.restoreState();

        g.setColour (Couleurs::bordure);
        g.drawHorizontalLine (regle.getBottom() - 1, (float) regle.getX(), (float) regle.getRight());

        // Trait de lecture
        const int xLecture = tempsVersX (positionEnTemps());
        if (xLecture >= zone.getX() && xLecture <= zone.getRight())
        {
            g.setColour (juce::Colours::white);
            g.fillRect (xLecture, regle.getY() + bandeBoucle, 2, basPistes - regle.getY() - bandeBoucle);

            juce::Path tri;
            tri.addTriangle ((float) xLecture - 5.0f, (float) regle.getY() + bandeBoucle,
                             (float) xLecture + 7.0f, (float) regle.getY() + bandeBoucle,
                             (float) xLecture + 1.0f, (float) regle.getY() + bandeBoucle + 9.0f);
            g.fillPath (tri);
        }

        // Coin au-dessus des en-têtes
        auto coin = getLocalBounds().removeFromRight (largeurEnTete).removeFromTop (hauteurRegle);
        g.setColour (Couleurs::panneau);
        g.fillRect (coin);
        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (modeAutomation ? fr ("Pistes  -  mode automation") : fr ("Pistes"),
                    coin.reduced (8, 0), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        conteneur.setBounds (getLocalBounds().removeFromRight (largeurEnTete).withTrimmedTop (hauteurRegle));
        placerEnTetes();
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = Mode::rien;
        duplicationFaite = false;

        if (e.x >= zonePistes().getRight())
            return;

        // Tout en haut de la règle : la boucle
        if (e.y < bandeBoucle)
        {
            const auto plage = boucleEnTemps();
            const int x1 = tempsVersX (plage.getStart().inBeats()), x2 = tempsVersX (plage.getEnd().inBeats());
            const bool existe = x2 > x1;

            // Près d'un curseur : on ne bouge que lui. Au milieu : toute la boucle. Ailleurs : une nouvelle boucle.
            if (existe && std::abs (e.x - x1) <= 6)             mode = Mode::boucleDebut;
            else if (existe && std::abs (e.x - x2) <= 6)        mode = Mode::boucleFin;
            else if (existe && e.x > x1 && e.x < x2)            mode = Mode::boucleEntiere;
            else                                                mode = Mode::boucle;

            departBoucle = juce::jmax (0.0, std::round (xVersTemps (e.x)));
            decalagePrise = xVersTemps (e.x) - plage.getStart().inBeats();
            return;
        }

        // Le reste de la règle : les repères et le trait de lecture
        if (e.y < hauteurRegle)
        {
            const int repere = repereSous (e.getPosition());

            if (repere >= 0)
            {
                if (e.mods.isPopupMenu())
                {
                    menuRepere (repere);
                    return;
                }

                edit.getUndoManager().beginNewTransaction();
                repereGlisse = repere;
                mode = Mode::repere;
                return;
            }

            mode = Mode::lecture;
            deplacerLecture (e);
            return;
        }

        auto pistes = te::getAudioTracks (edit);
        const int numPiste = pisteEnY (e.y);

        if (numPiste >= 0 && numPiste < pistes.size())
            selection.choisirPiste (pistes[numPiste]);

        if (modeAutomation)
        {
            if (numPiste >= 0 && numPiste < pistes.size())
                cliquerAutomation (e, *pistes[numPiste], numPiste);

            repaint();
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

        if (mode == Mode::boucle)
        {
            const double t = juce::jmax (0.0, std::round (xVersTemps (e.x)));
            const double debut = juce::jmin (departBoucle, t);
            const double fin = juce::jmax (juce::jmax (departBoucle, t), debut + 1.0);
            definirBoucle (debut, fin, true);
            return;
        }

        if (mode == Mode::boucleDebut || mode == Mode::boucleFin || mode == Mode::boucleEntiere)
        {
            const auto plage = boucleEnTemps();
            const double debut = plage.getStart().inBeats(), fin = plage.getEnd().inBeats();
            const double t = juce::jmax (0.0, std::round (xVersTemps (e.x)));

            if (mode == Mode::boucleDebut)
                definirBoucle (juce::jmin (t, fin - 1.0), fin, false);
            else if (mode == Mode::boucleFin)
                definirBoucle (debut, juce::jmax (t, debut + 1.0), false);
            else
            {
                const double nouveauDebut = juce::jmax (0.0, std::round (xVersTemps (e.x) - decalagePrise));
                definirBoucle (nouveauDebut, nouveauDebut + (fin - debut), false);
            }

            return;
        }

        if (mode == Mode::automation)
        {
            glisserPointAutomation (e);
            return;
        }

        if (mode == Mode::repere)
        {
            if (repereGlisse >= 0 && repereGlisse < Reperes::nombre (edit))
                Reperes::deplacer (edit, repereGlisse, juce::jmax (0.0, std::round (xVersTemps (e.x))));

            repaint();
            return;
        }

        auto* clip = selection.clipValide();
        if (clip == nullptr || mode == Mode::rien || e.getDistanceFromDragStart() < 3)
            return;

        const auto temps = tempsDuClip (*clip);

        if (mode == Mode::deplacer)
        {
            // Ctrl + glisser : on laisse une copie à la place d'origine, comme dans Ableton
            if (e.mods.isCommandDown() && ! duplicationFaite)
            {
                duplicationFaite = true;
                te::duplicateClip (*clip);
            }

            const double debut = juce::jmax (0.0, std::round (xVersTemps (e.x) - decalagePrise));
            if (debut != temps.getStart().inBeats())
                clip->setStart (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (debut)), false, true);

            const int cible = pisteEnY (e.y);
            auto pistes = te::getAudioTracks (edit);

            // Un clip de notes ne va que sur une piste MIDI, un clip de son que sur une piste audio
            if (cible >= 0 && cible < pistes.size()
                && cible != Selection::indexPiste (edit, *clip)
                && TypePiste::estAudio (*pistes[cible]) == estClipAudio (*clip))
                clip->moveTo (*pistes[cible]);
        }
        else if (mode == Mode::redimensionner)
        {
            double fin = juce::jmax (temps.getStart().inBeats() + 1.0, std::round (xVersTemps (e.x)));

            // Un clip audio ne peut pas être plus long que son fichier
            if (auto* son = dynamic_cast<te::WaveAudioClip*> (clip); son != nullptr && ! son->getAutoTempo())
            {
                const double reste = FormesOnde::obtenir (son->getOriginalFile()).duree - clip->getPosition().offset.inSeconds();
                const auto finMax = edit.tempoSequence.toBeats (clip->getPosition().time.getStart() + te::TimeDuration::fromSeconds (reste));
                fin = juce::jmin (fin, finMax.inBeats());
            }

            if (fin != temps.getEnd().inBeats() && fin > temps.getStart().inBeats())
                clip->setEnd (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (fin)), true);
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        // Un clip posé sur un autre l'écrase, comme dans Ableton
        if (mode == Mode::deplacer || mode == Mode::redimensionner)
            if (auto* clip = selection.clipValide())
                ecraserSous (*clip);

        mode = Mode::rien;
        repereGlisse = -1;
        pointGlisse = -1;
        parametreGlisse = nullptr;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool bord = false;

        if (e.y < bandeBoucle)
        {
            // Sur un des deux curseurs de la boucle
            const auto plage = boucleEnTemps();
            const int x1 = tempsVersX (plage.getStart().inBeats()), x2 = tempsVersX (plage.getEnd().inBeats());
            bord = x2 > x1 && (std::abs (e.x - x1) <= 6 || std::abs (e.x - x2) <= 6);
        }
        else if (auto* clip = modeAutomation ? nullptr : clipSous (e.getPosition()))
        {
            const auto r = rectangleClip (*clip, Selection::indexPiste (edit, *clip));
            bord = e.x > r.getRight() - 8 && r.getWidth() > 16;
        }

        setMouseCursor (bord ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto zone = zonePistes();
        if (e.x >= zone.getRight())
            return;

        // Dans la règle : poser un repère, ou renommer celui sur lequel on clique
        if (e.y >= bandeBoucle && e.y < hauteurRegle)
        {
            const int repere = repereSous (e.getPosition());

            if (repere >= 0)
            {
                renommerRepere (repere);
            }
            else
            {
                edit.getUndoManager().beginNewTransaction();
                Reperes::ajouter (edit, juce::jmax (0.0, std::round (xVersTemps (e.x))),
                                  fr ("Repère ") + juce::String (Reperes::nombre (edit) + 1));
                renommerRepere (Reperes::nombre (edit) - 1);
            }

            repaint();
            return;
        }

        auto pistes = te::getAudioTracks (edit);
        const int numPiste = pisteEnY (e.y);

        if (modeAutomation)
        {
            // Double-clic sur un point d'automation : on l'efface
            if (numPiste >= 0 && numPiste < pistes.size())
                effacerPointAutomation (e, *pistes[numPiste], numPiste);

            return;
        }

        if (numPiste < 0 || numPiste >= pistes.size() || clipSous (e.getPosition()) != nullptr)
            return;

        const double debutMesure = std::floor (juce::jmax (0.0, xVersTemps (e.x)) / tempsParMesure) * tempsParMesure;

        if (TypePiste::estAudio (*pistes[numPiste]))
            importerSon (*pistes[numPiste], debutMesure);
        else
            creerClip (*pistes[numPiste], debutMesure, (double) tempsParMesure);     // un clip d'une mesure
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
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY) || defilementMax() <= 0)
        {
            const double delta = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
            debutTemps = juce::jmax (0.0, debutTemps - delta * 400.0 / pxParTemps);
        }
        else
        {
            defilementY = juce::jlimit (0, defilementMax(), defilementY - juce::roundToInt (w.deltaY * 250.0f));
            placerEnTetes();
        }

        repaint();
    }

    //==========================================================================
    // Glisser-déposer depuis le navigateur

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        const auto id = d.description.toString();
        return IdNav::estConnu (id);
    }

    void itemDragMove (const SourceDetails& d) override     { survoler (pisteEnY (d.localPosition.y)); }
    void itemDragExit (const SourceDetails&) override       { survoler (-100); }

    void itemDropped (const SourceDetails& d) override
    {
        const auto id = d.description.toString();
        const int cible = pisteEnY (d.localPosition.y);
        auto pistes = te::getAudioTracks (edit);
        survoler (-100);

        if (cible >= 0 && cible < pistes.size())
            appliquerSurPiste (*pistes[cible], id);
        else if (IdNav::estInstrument (id))
            ajouterPiste (false, id);       // lâché sous les pistes : une nouvelle piste MIDI avec cet instrument
    }

    //==========================================================================
    // Fichiers son glissés depuis l'explorateur de Windows

    bool isInterestedInFileDrag (const juce::StringArray& fichiers) override
    {
        for (auto& f : fichiers)
            if (FormesOnde::estFichierSon (juce::File (f)))
                return true;

        return false;
    }

    void fileDragMove (const juce::StringArray&, int, int y) override   { survoler (pisteEnY (y)); }
    void fileDragExit (const juce::StringArray&) override               { survoler (-100); }

    void filesDropped (const juce::StringArray& fichiers, int x, int y) override
    {
        survoler (-100);
        auto pistes = te::getAudioTracks (edit);
        int cible = pisteEnY (y);
        double debut = juce::jmax (0.0, std::round (xVersTemps (x)));

        edit.getUndoManager().beginNewTransaction();

        for (auto& chemin : fichiers)
        {
            const juce::File f (chemin);
            if (! FormesOnde::estFichierSon (f))
                continue;

            te::AudioTrack* piste = (cible >= 0 && cible < pistes.size()) ? pistes[cible] : nullptr;

            // Lâché sur une piste MIDI ou sous les pistes : on crée une piste audio pour ce son
            if (piste == nullptr || ! TypePiste::estAudio (*piste))
                piste = ajouterPiste (true, {});

            if (piste != nullptr)
                if (auto clip = insererSon (*piste, f, debut))
                    debut = tempsDuClip (*clip).getEnd().inBeats();
        }

        repaint();
    }

    //==========================================================================
    /** Met un instrument ou un effet (identifiant du navigateur) sur une piste. */
    void appliquerSurPiste (te::AudioTrack& piste, const juce::String& id)
    {
        edit.getUndoManager().beginNewTransaction();
        selection.choisirPiste (&piste);

        if (IdNav::estEffet (id))
        {
            Effets::ajouter (piste, id);
        }
        else if (! TypePiste::estAudio (piste))     // pas d'instrument sur une piste audio
        {
            const auto nouveauNom = Instruments::nomDeLId (id);

            // Si la piste porte encore un nom par défaut, on lui donne celui de l'instrument
            bool nomParDefaut = piste.getName().startsWith ("MIDI ");
            for (auto& e : Instruments::catalogue())
                nomParDefaut = nomParDefaut || piste.getName() == e.nom;

            if (nomParDefaut && nouveauNom.isNotEmpty())
                piste.setName (nouveauNom);

            Instruments::appliquer (piste, id);
            reconstruireEnTetes();
        }

        selection.sendChangeMessage();
    }

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

    /** Pose un fichier son sur une piste audio, à l'endroit voulu. */
    te::WaveAudioClip::Ptr insererSon (te::AudioTrack& piste, const juce::File& fichier, double debutEnTemps)
    {
        const double duree = FormesOnde::obtenir (fichier).duree;

        if (duree <= 0.0)
        {
            Journal::ecrire ("Son illisible : " + fichier.getFullPathName());
            return {};
        }

        const auto debut = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (debutEnTemps));
        const te::ClipPosition position { { debut, debut + te::TimeDuration::fromSeconds (duree) }, {} };

        auto clip = piste.insertWaveClip (fichier.getFileNameWithoutExtension(), fichier, position, false);

        if (clip != nullptr)
        {
            selection.choisir (clip.get());
            ecraserSous (*clip);
        }

        repaint();
        return clip;
    }

    /** Ouvre une fenêtre pour choisir un fichier son et le pose sur la piste. */
    void importerSon (te::AudioTrack& piste, double debutEnTemps)
    {
        selecteur = std::make_shared<juce::FileChooser> (fr ("Choisir un son"),
                                                         juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                         "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<VueArrangement> moi (this);
        te::AudioTrack::Ptr garde (&piste);

        selecteur->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                [moi, garde, debutEnTemps] (const juce::FileChooser& fc)
                                {
                                    const auto f = fc.getResult();

                                    if (moi != nullptr && f.existsAsFile() && te::getAudioTracks (moi->edit).contains (garde.get()))
                                    {
                                        moi->edit.getUndoManager().beginNewTransaction();
                                        moi->insererSon (*garde, f, debutEnTemps);
                                    }
                                });
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

    void copierClipChoisi()         { clipCopie = selection.clipValide(); }

    /** Colle le clip copié à l'endroit du trait de lecture. */
    void collerClip()
    {
        if (clipCopie == nullptr || Selection::indexPiste (edit, *clipCopie) < 0)
            return;

        edit.getUndoManager().beginNewTransaction();

        if (auto copie = te::duplicateClip (*clipCopie))
        {
            copie->setStart (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (std::round (positionEnTemps()))), false, true);

            // Sur la piste choisie, si elle est du même type que le clip
            if (auto* piste = selection.pisteValide())
                if (TypePiste::estAudio (*piste) == estClipAudio (*copie) && ! piste->getClips().contains (copie.get()))
                    copie->moveTo (*piste);

            selection.choisir (copie.get());
            ecraserSous (*copie);
        }

        repaint();
    }

    /** Recopie le clip choisi juste après lui-même. */
    void dupliquerClipChoisi()
    {
        auto* clip = selection.clipValide();
        if (clip == nullptr)
            return;

        edit.getUndoManager().beginNewTransaction();
        const auto fin = clip->getPosition().time.getEnd();

        if (auto copie = te::duplicateClip (*clip))
        {
            copie->setStart (fin, false, true);
            selection.choisir (copie.get());
            ecraserSous (*copie);
        }

        repaint();
    }

    //==========================================================================
    // La boucle

    te::BeatRange boucleEnTemps() const
    {
        return edit.tempoSequence.toBeats (edit.getTransport().getLoopRange());
    }

    void definirBoucle (double debutEnTemps, double finEnTemps, bool activer)
    {
        auto& transport = edit.getTransport();
        transport.setLoopRange (edit.tempoSequence.toTime (te::BeatRange (te::BeatPosition::fromBeats (debutEnTemps),
                                                                          te::BeatPosition::fromBeats (finEnTemps))));
        if (activer)
            transport.looping = true;

        repaint();
    }

    /** Fait tourner en boucle la partie du morceau occupée par le clip choisi. */
    void bouclerSurClip()
    {
        if (auto* clip = selection.clipValide())
        {
            const auto t = tempsDuClip (*clip);
            definirBoucle (t.getStart().inBeats(), t.getEnd().inBeats(), true);
        }
    }

    //==========================================================================
    // Mode automation

    bool estEnModeAutomation() const        { return modeAutomation; }

    void definirModeAutomation (bool actif)
    {
        modeAutomation = actif;
        mettreAJourEnTetesAutomation();
        repaint();
    }

private:
    enum class Mode { rien, lecture, boucle, boucleDebut, boucleFin, boucleEntiere, repere, deplacer, redimensionner, automation };

    te::Edit& edit;
    Selection& selection;
    juce::Component conteneur;              // contient les en-têtes (et les coupe sous la règle)
    juce::OwnedArray<EnTetePiste> enTetes;
    juce::TextButton ajouter;
    std::shared_ptr<juce::FileChooser> selecteur;

    double pxParTemps = 28.0;   // zoom horizontal : pixels pour un temps
    double debutTemps = 0.0;    // temps affiché tout à gauche
    int defilementY = 0;
    int pisteSurvolee = -100;
    int dernierXLecture = -1;
    int compteur = 0;

    Mode mode = Mode::rien;
    double decalagePrise = 0.0, departBoucle = 0.0;
    bool duplicationFaite = false;
    int repereGlisse = -1;
    te::Clip::Ptr clipCopie;

    static constexpr int maxPistes = 16;

    bool modeAutomation = false;
    std::map<juce::uint64, te::AutomatableParameter::Ptr> reglagesAutomatises;     // par piste : le réglage affiché
    te::AutomatableParameter::Ptr parametreGlisse;
    int pointGlisse = -1;

    //==========================================================================
    // Écrasement des clips

    /** Retire ce qui se trouve sous un clip sur sa piste : les autres clips sont coupés ou effacés. */
    void ecraserSous (te::Clip& dessus)
    {
        auto* piste = dessus.getClipTrack();
        if (piste == nullptr)
            return;

        const auto zone = dessus.getPosition().time;
        juce::Array<te::Clip::Ptr> autres;

        for (auto* c : piste->getClips())
            if (c != &dessus)
                autres.add (c);

        for (auto& c : autres)
        {
            const auto t = c->getPosition().time;

            if (t.getEnd() <= zone.getStart() || t.getStart() >= zone.getEnd())
                continue;       // ne se touchent pas

            const bool debutCouvert = t.getStart() >= zone.getStart();
            const bool finCouverte = t.getEnd() <= zone.getEnd();

            if (debutCouvert && finCouverte)
            {
                c->removeFromParent();                  // entièrement recouvert
            }
            else if (debutCouvert)
            {
                rognerDebut (*c, zone.getEnd());        // son début est recouvert
            }
            else if (finCouverte)
            {
                c->setEnd (zone.getStart(), true);     // sa fin est recouverte
            }
            else
            {
                // Recouvert au milieu : il est coupé en deux morceaux
                if (auto droite = te::duplicateClip (*c))
                    rognerDebut (*droite, zone.getEnd());

                c->setEnd (zone.getStart(), true);
            }
        }
    }

    /** Fait commencer un clip plus tard, sans déplacer ce qu'il contient. */
    void rognerDebut (te::Clip& c, te::TimePosition nouveauDebut)
    {
        if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            auto* um = &edit.getUndoManager();
            const double delta = edit.tempoSequence.toBeats (nouveauDebut).inBeats() - tempsDuClip (c).getStart().inBeats();
            auto& sequence = mc->getSequence();
            const juce::Array<te::MidiNote*> notes (sequence.getNotes());

            // Les notes sont comptées depuis le début du clip : on les recale
            for (auto* n : notes)
            {
                const double debut = n->getStartBeat().inBeats(), fin = n->getEndBeat().inBeats();

                if (fin <= delta)
                    sequence.removeNote (*n, um);
                else if (debut < delta)
                    n->setStartAndLength (te::BeatPosition::fromBeats (0.0), te::BeatDuration::fromBeats (fin - delta), um);
                else
                    n->setStartAndLength (te::BeatPosition::fromBeats (debut - delta), n->getLengthBeats(), um);
            }

            c.setStart (nouveauDebut, false, false);
        }
        else
        {
            c.setStart (nouveauDebut, true, false);     // un son : on saute simplement le début du fichier
        }
    }

    //==========================================================================
    // Automation : une courbe par piste, pour le réglage choisi dans l'en-tête

    te::AutomatableParameter::Ptr reglageDe (te::AudioTrack& piste)
    {
        auto& choisi = reglagesAutomatises[piste.itemID.getRawID()];

        // Le réglage choisi a pu disparaître avec son effet
        if (choisi != nullptr)
        {
            bool existe = false;

            for (auto* p : piste.pluginList.getPlugins())
                existe = existe || p->getAutomatableParameters().contains (choisi.get());

            if (! existe)
                choisi = nullptr;
        }

        if (choisi == nullptr)
            if (auto* volume = piste.getVolumePlugin())
                choisi = volume->volParam;

        return choisi;
    }

    juce::String nomDuReglage (te::AudioTrack& piste)
    {
        auto p = reglageDe (piste);
        if (p == nullptr)
            return "-";

        if (auto* volume = piste.getVolumePlugin())
        {
            if (p == volume->volParam)   return "Volume";
            if (p == volume->panParam)   return "Pan";
        }

        return p->getParameterName();
    }

    void mettreAJourEnTetesAutomation()
    {
        for (auto* e : enTetes)
            e->definirAutomation (modeAutomation, modeAutomation ? nomDuReglage (e->getPiste()) : juce::String());
    }

    juce::Rectangle<int> zoneAutomation (int numPiste) const
    {
        return { zonePistes().getX(), yPiste (numPiste) + 7, zonePistes().getWidth(), hauteurPiste - 14 };
    }

    int yDeValeur (te::AutomatableParameter& p, float valeur, int numPiste) const
    {
        const auto z = zoneAutomation (numPiste);
        return z.getBottom() - juce::roundToInt (p.valueRange.convertTo0to1 (p.valueRange.snapToLegalValue (valeur)) * (float) z.getHeight());
    }

    float valeurEnY (te::AutomatableParameter& p, int y, int numPiste) const
    {
        const auto z = zoneAutomation (numPiste);
        return p.valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, (float) (z.getBottom() - y) / (float) z.getHeight()));
    }

    int xDuPoint (te::AutomationCurve& courbe, int index) const
    {
        return tempsVersX (edit.tempoSequence.toBeats (courbe.getPointTime (index)).inBeats());
    }

    int pointSous (te::AutomatableParameter& p, juce::Point<int> pos, int numPiste) const
    {
        auto& courbe = p.getCurve();

        for (int i = courbe.getNumPoints(); --i >= 0;)
            if (juce::Point<int> (xDuPoint (courbe, i), yDeValeur (p, courbe.getPointValue (i), numPiste)).getDistanceFrom (pos) <= 8)
                return i;

        return -1;
    }

    void dessinerAutomation (juce::Graphics& g, te::AudioTrack& piste, int numPiste)
    {
        auto p = reglageDe (piste);
        if (p == nullptr)
            return;

        auto& courbe = p->getCurve();
        const auto zone = zonePistes();
        const int n = courbe.getNumPoints();
        const auto couleur = Couleurs::solo;

        if (n == 0)
        {
            // Pas encore de courbe : un trait fin à la valeur actuelle du réglage
            g.setColour (couleur.withAlpha (0.5f));
            g.drawHorizontalLine (yDeValeur (*p, p->getCurrentValue(), numPiste), (float) zone.getX(), (float) zone.getRight());
            return;
        }

        juce::Path trace;
        trace.startNewSubPath ((float) zone.getX(), (float) yDeValeur (*p, courbe.getPointValue (0), numPiste));

        for (int i = 0; i < n; ++i)
            trace.lineTo ((float) xDuPoint (courbe, i), (float) yDeValeur (*p, courbe.getPointValue (i), numPiste));

        trace.lineTo ((float) zone.getRight(), (float) yDeValeur (*p, courbe.getPointValue (n - 1), numPiste));

        g.setColour (couleur);
        g.strokePath (trace, juce::PathStrokeType (2.0f));

        for (int i = 0; i < n; ++i)
        {
            const juce::Point<float> centre ((float) xDuPoint (courbe, i), (float) yDeValeur (*p, courbe.getPointValue (i), numPiste));
            const bool glisse = (p == parametreGlisse && i == pointGlisse);

            g.setColour (glisse ? juce::Colours::white : couleur.brighter (0.4f));
            g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (centre));
            g.setColour (juce::Colours::black);
            g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (centre), 1.0f);

            if (glisse)
            {
                // La valeur du point qu'on est en train de bouger
                g.setColour (juce::Colours::white);
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                g.drawText (p->valueToString (courbe.getPointValue (i)) + " " + p->getLabel(),
                            (int) centre.x + 8, (int) centre.y - 18, 120, 16, juce::Justification::centredLeft, false);
            }
        }
    }

    void cliquerAutomation (const juce::MouseEvent& e, te::AudioTrack& piste, int numPiste)
    {
        auto p = reglageDe (piste);
        if (p == nullptr)
            return;

        auto& courbe = p->getCurve();
        auto* um = &edit.getUndoManager();
        um->beginNewTransaction();

        int index = pointSous (*p, e.getPosition(), numPiste);

        if (index >= 0 && e.mods.isPopupMenu())
        {
            courbe.removePoint (index, um);     // clic droit : effacer le point
            return;
        }

        if (e.mods.isPopupMenu())
            return;

        if (index < 0)
        {
            // Un nouveau point, aimanté sur le quart de temps
            const double temps = juce::jmax (0.0, std::round (xVersTemps (e.x) * 4.0) / 4.0);
            index = courbe.addPoint (te::EditPosition (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (temps))),
                                     valeurEnY (*p, e.y, numPiste), 0.0f, um);
        }

        parametreGlisse = p;
        pointGlisse = index;
        pisteGlissee = numPiste;
        mode = Mode::automation;
    }

    void glisserPointAutomation (const juce::MouseEvent& e)
    {
        if (parametreGlisse == nullptr)
            return;

        auto& courbe = parametreGlisse->getCurve();
        if (pointGlisse < 0 || pointGlisse >= courbe.getNumPoints())
            return;

        const double temps = juce::jmax (0.0, std::round (xVersTemps (e.x) * 4.0) / 4.0);
        pointGlisse = courbe.movePoint (pointGlisse,
                                        te::EditPosition (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (temps))),
                                        valeurEnY (*parametreGlisse, e.y, pisteGlissee), std::nullopt, false,
                                        &edit.getUndoManager());
        repaint();
    }

    void effacerPointAutomation (const juce::MouseEvent& e, te::AudioTrack& piste, int numPiste)
    {
        if (auto p = reglageDe (piste))
        {
            const int index = pointSous (*p, e.getPosition(), numPiste);

            if (index >= 0)
            {
                edit.getUndoManager().beginNewTransaction();
                p->getCurve().removePoint (index, &edit.getUndoManager());
                repaint();
            }
        }
    }

    /** Le menu qui choisit le réglage automatisé d'une piste. */
    void menuReglage (te::AudioTrack& piste, juce::Component& cible)
    {
        auto liste = std::make_shared<juce::Array<te::AutomatableParameter::Ptr>>();
        const auto actuel = reglageDe (piste);
        juce::PopupMenu m;

        auto ajouterReglage = [&] (juce::PopupMenu& menu, te::AutomatableParameter::Ptr p, const juce::String& nom)
        {
            liste->add (p);
            // Une puce signale les réglages qui ont déjà une courbe
            menu.addItem (liste->size(), (p->getCurve().getNumPoints() > 0 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f ")) : juce::String()) + nom,
                          true, p == actuel);
        };

        if (auto* volume = piste.getVolumePlugin())
        {
            m.addSectionHeader ("Piste");
            ajouterReglage (m, volume->volParam, "Volume");
            ajouterReglage (m, volume->panParam, "Pan");
        }

        for (auto* plugin : Effets::chaineDe (piste))
        {
            juce::PopupMenu sous;
            int n = 0;

            for (auto p : plugin->getAutomatableParameters())
            {
                if (++n > 80)
                    break;

                ajouterReglage (sous, p, p->getParameterName());
            }

            if (n > 0)
                m.addSubMenu (plugin->getName(), sous);
        }

        te::AudioTrack::Ptr garde (&piste);
        juce::Component::SafePointer<VueArrangement> moi (this);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&cible),
                         [moi, garde, liste] (int choix)
                         {
                             if (moi != nullptr && choix > 0 && choix <= liste->size())
                             {
                                 moi->reglagesAutomatises[garde->itemID.getRawID()] = (*liste)[choix - 1];
                                 moi->mettreAJourEnTetesAutomation();
                                 moi->repaint();
                             }
                         });
    }

    int pisteGlissee = 0;

    //==========================================================================
    // Géométrie

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

    int yPiste (int index) const        { return zonePistes().getY() + index * hauteurPiste - defilementY; }

    int pisteEnY (int y) const
    {
        if (y < zonePistes().getY())
            return -1;

        return (y - zonePistes().getY() + defilementY) / hauteurPiste;
    }

    int defilementMax() const
    {
        return juce::jmax (0, enTetes.size() * hauteurPiste + 44 - zonePistes().getHeight());
    }

    double positionEnTemps() const
    {
        return edit.tempoSequence.toBeats (edit.getTransport().getPosition()).inBeats();
    }

    te::BeatRange tempsDuClip (const te::Clip& c) const
    {
        return edit.tempoSequence.toBeats (c.getPosition().time);
    }

    static bool estClipAudio (const te::Clip& c)
    {
        return dynamic_cast<const te::WaveAudioClip*> (&c) != nullptr;
    }

    juce::Rectangle<int> rectangleClip (const te::Clip& c, int numPiste) const
    {
        const auto t = tempsDuClip (c);
        const int x1 = tempsVersX (t.getStart().inBeats());
        const int x2 = tempsVersX (t.getEnd().inBeats());
        return { x1, yPiste (numPiste) + 2, juce::jmax (4, x2 - x1 - 1), hauteurPiste - 4 };
    }

    te::Clip* clipSous (juce::Point<int> p) const
    {
        if (! zonePistes().contains (p))
            return nullptr;

        auto pistes = te::getAudioTracks (edit);
        const int numPiste = pisteEnY (p.y);

        if (numPiste >= 0 && numPiste < pistes.size())
        {
            auto& clips = pistes[numPiste]->getClips();
            for (int i = clips.size(); --i >= 0;)
                if (rectangleClip (*clips[i], numPiste).contains (p))
                    return clips[i];
        }

        return nullptr;
    }

    juce::Rectangle<int> rectangleRepere (int i) const
    {
        const juce::Font police { juce::FontOptions (11.0f, juce::Font::bold) };
        const int largeur = juce::roundToInt (juce::GlyphArrangement::getStringWidth (police, Reperes::nom (edit, i))) + 10;
        return { tempsVersX (Reperes::temps (edit, i)), zoneRegle().getY() + bandeBoucle + 13, juce::jmax (16, largeur), 13 };
    }

    int repereSous (juce::Point<int> p) const
    {
        for (int i = Reperes::nombre (edit); --i >= 0;)
            if (rectangleRepere (i).contains (p))
                return i;

        return -1;
    }

    void survoler (int numPiste)
    {
        if (numPiste != pisteSurvolee)
        {
            pisteSurvolee = numPiste;
            repaint();
        }
    }

    //==========================================================================
    void dessinerClip (juce::Graphics& g, te::Clip& clip, juce::Rectangle<int> plein, juce::Colour couleur, bool estChoisi)
    {
        const auto zone = zonePistes();

        g.setColour (couleur.withMultipliedBrightness (estChoisi ? 0.75f : 0.5f));
        g.fillRoundedRectangle (plein.toFloat(), 3.0f);
        g.setColour (couleur);
        g.fillRoundedRectangle (plein.withHeight (16).toFloat(), 3.0f);

        g.setColour (juce::Colours::black);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (clip.getName(), plein.withHeight (16).reduced (5, 0), juce::Justification::centredLeft, true);

        const auto contenu = plein.withTrimmedTop (18).reduced (0, 3);
        g.setColour (estChoisi ? juce::Colours::white : couleur.brighter (0.6f));

        if (auto* mc = dynamic_cast<te::MidiClip*> (&clip))
        {
            // Aperçu des notes
            auto& notes = mc->getSequence().getNotes();
            int bas = 127, haut = 0;
            for (auto* n : notes)
            {
                bas = juce::jmin (bas, n->getNoteNumber());
                haut = juce::jmax (haut, n->getNoteNumber());
            }

            const double etendue = juce::jmax (7, haut - bas + 1);

            for (auto* n : notes)
            {
                const double y = contenu.getBottom() - (n->getNoteNumber() - bas + 1) / etendue * contenu.getHeight();
                const float x1 = (float) (plein.getX() + n->getStartBeat().inBeats() * pxParTemps);
                const float w = (float) juce::jmax (2.0, n->getLengthBeats().inBeats() * pxParTemps - 1.0);

                if (x1 < (float) plein.getRight())
                    g.fillRect (x1, (float) y, juce::jmin (w, (float) plein.getRight() - x1),
                                (float) juce::jmax (2.0, contenu.getHeight() / etendue - 1.0));
            }
        }
        else if (auto* son = dynamic_cast<te::WaveAudioClip*> (&clip))
        {
            // Forme d'onde du fichier son
            const juce::File fichierSon = son->getOriginalFile();
            const auto& forme = FormesOnde::obtenir (fichierSon);
            const double bpm = edit.tempoSequence.getTempo (0)->getBpm();
            // Un clip « warpé » suit le tempo du morceau : son dessin est étiré de la même façon
            const double secondesParPixel = 60.0 / (son->getAutoTempo() ? Warp::bpmOrigine (*son) : bpm) / pxParTemps;
            const double decalage = clip.getPosition().offset.inSeconds();
            const float milieu = (float) contenu.getCentreY();
            const float demi = (float) contenu.getHeight() * 0.5f;

            const int x1 = juce::jmax (plein.getX(), zone.getX());
            const int x2 = juce::jmin (plein.getRight(), zone.getRight());

            for (int x = x1; x < x2; ++x)
            {
                const double t = decalage + (x - plein.getX()) * secondesParPixel;
                const float c = juce::jmin (1.0f, forme.crete (t, t + secondesParPixel));
                g.drawVerticalLine (x, milieu - juce::jmax (0.5f, c * demi), milieu + juce::jmax (0.5f, c * demi));
            }
        }

        if (estChoisi)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (plein.toFloat().reduced (0.5f), 3.0f, 1.5f);
        }
    }

    void deplacerLecture (const juce::MouseEvent& e)
    {
        // Aimanté sur le temps le plus proche
        const double temps = juce::jmax (0.0, std::round (xVersTemps (e.x)));
        edit.getTransport().setPosition (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (temps)));
        repaint();
    }

    //==========================================================================
    // Repères

    void menuRepere (int index)
    {
        juce::PopupMenu m;
        m.addItem (1, fr ("Renommer le repère"));
        m.addItem (2, fr ("Supprimer le repère"));

        juce::Component::SafePointer<VueArrangement> moi (this);

        m.showMenuAsync (juce::PopupMenu::Options(), [moi, index] (int choix)
        {
            if (moi == nullptr || index >= Reperes::nombre (moi->edit))
                return;

            if (choix == 1)
            {
                moi->renommerRepere (index);
            }
            else if (choix == 2)
            {
                moi->edit.getUndoManager().beginNewTransaction();
                Reperes::supprimer (moi->edit, index);
                moi->repaint();
            }
        });
    }

    void renommerRepere (int index)
    {
        auto* fenetre = new juce::AlertWindow (fr ("Nom du repère"),
                                               fr ("Par exemple : Intro, Couplet, Refrain..."),
                                               juce::MessageBoxIconType::NoIcon, this);
        fenetre->addTextEditor ("nom", Reperes::nom (edit, index));
        fenetre->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        fenetre->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<VueArrangement> moi (this);

        fenetre->enterModalState (true, juce::ModalCallbackFunction::create ([moi, fenetre, index] (int resultat)
        {
            const auto nom = fenetre->getTextEditorContents ("nom").trim();

            if (moi != nullptr && resultat == 1 && nom.isNotEmpty() && index < Reperes::nombre (moi->edit))
            {
                Reperes::renommer (moi->edit, index, nom);
                moi->repaint();
            }
        }), true);
    }

    //==========================================================================
    // Pistes

    void reconstruireEnTetes()
    {
        enTetes.clear();

        int i = 0;
        for (auto* t : te::getAudioTracks (edit))
        {
            if (t->getColour().isTransparent())
                t->setColour (Couleurs::dePiste (i));

            auto* e = enTetes.add (new EnTetePiste (*t, i++));
            e->surMenu = [this] (te::AudioTrack& piste, juce::Component& cible) { menuPiste (piste, cible); };
            e->surChoix = [this] (te::AudioTrack& piste) { selection.choisirPiste (&piste); };
            e->surParametre = [this] (te::AudioTrack& piste, juce::Component& cible) { menuReglage (piste, cible); };
            conteneur.addAndMakeVisible (e);
        }

        ajouter.setEnabled (enTetes.size() < maxPistes);
        mettreAJourEnTetesAutomation();
        placerEnTetes();
    }

    void placerEnTetes()
    {
        defilementY = juce::jlimit (0, defilementMax(), defilementY);
        int y = -defilementY;

        for (auto* e : enTetes)
        {
            e->setBounds (0, y, conteneur.getWidth(), hauteurPiste);
            y += hauteurPiste;
        }

        ajouter.setBounds (8, y + 8, conteneur.getWidth() - 16, 28);
        repaint();
    }

    bool pistesOntChange() const
    {
        auto pistes = te::getAudioTracks (edit);
        if (pistes.size() != enTetes.size())
            return true;

        for (int i = 0; i < pistes.size(); ++i)
            if (pistes[i] != &enTetes[i]->getPiste())
                return true;

        return false;
    }

    void menuAjouterPiste()
    {
        juce::PopupMenu m;
        m.addItem (1, fr ("Piste MIDI  -  des notes jouées par un instrument"));
        m.addItem (2, fr ("Piste audio  -  des sons enregistrés (boucles, voix, effets...)"));

        juce::Component::SafePointer<VueArrangement> moi (this);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&ajouter),
                         [moi] (int choix)
                         {
                             if (moi != nullptr && choix > 0)
                             {
                                 moi->edit.getUndoManager().beginNewTransaction();
                                 moi->ajouterPiste (choix == 2, {});
                             }
                         });
    }

    /** Crée une piste MIDI (avec l'instrument `idInstrument` s'il est donné) ou une piste audio. */
    te::AudioTrack* ajouterPiste (bool audio, const juce::String& idInstrument)
    {
        auto pistes = te::getAudioTracks (edit);
        if (pistes.size() >= maxPistes)
            return nullptr;

        // La première couleur pas encore utilisée
        int numCouleur = pistes.size();
        for (int c = 0; c < 8; ++c)
        {
            bool prise = false;
            for (auto* p : pistes)
                prise = prise || p->getColour() == Couleurs::dePiste (c);

            if (! prise)
            {
                numCouleur = c;
                break;
            }
        }

        te::Track* derniere = pistes.isEmpty() ? nullptr : pistes.getLast();
        auto piste = edit.insertNewAudioTrack (te::TrackInsertPoint (nullptr, derniere), nullptr);

        if (piste == nullptr)
            return nullptr;

        const juce::String numero (pistes.size() + 1);
        TypePiste::definirAudio (*piste, audio);
        piste->setColour (Couleurs::dePiste (numCouleur));

        if (audio)
        {
            piste->setName ("Audio " + numero);
        }
        else if (idInstrument.isNotEmpty())
        {
            piste->setName (Instruments::nomDeLId (idInstrument));
            Instruments::appliquer (*piste, idInstrument);
        }
        else
        {
            piste->setName ("MIDI " + numero);
        }

        selection.choisirPiste (piste.get());
        reconstruireEnTetes();
        defilementY = defilementMax();
        placerEnTetes();
        return piste.get();
    }

    /** Menu des instruments (fournis + préréglages personnels). Renvoie les identifiants dans l'ordre. */
    static juce::StringArray remplirMenuInstruments (juce::PopupMenu& m, int premierId)
    {
        juce::StringArray ids;
        bool separateurPose = false;

        for (auto& e : Instruments::catalogue())
        {
            if (e.utilisateur && ! separateurPose)
            {
                m.addSeparator();
                separateurPose = true;
            }

            m.addItem (premierId + ids.size(), e.nom);
            ids.add (e.id);
        }

        return ids;
    }

    void menuPiste (te::AudioTrack& piste, juce::Component& cible)
    {
        juce::PopupMenu m;
        juce::StringArray ids;

        if (TypePiste::estAudio (piste))
        {
            m.addItem (900, fr ("Importer un son..."));
        }
        else
        {
            juce::PopupMenu instruments;
            ids = remplirMenuInstruments (instruments, 1);
            m.addSubMenu (fr ("Changer d'instrument"), instruments);
        }

        m.addSeparator();
        m.addItem (1000, fr ("Supprimer la piste"), te::getAudioTracks (edit).size() > 1);

        te::AudioTrack::Ptr garde (&piste);
        juce::Component::SafePointer<VueArrangement> moi (this);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&cible),
                         [moi, garde, ids] (int choix)
                         {
                             if (moi == nullptr || choix <= 0 || ! te::getAudioTracks (moi->edit).contains (garde.get()))
                                 return;

                             if (choix == 1000)
                             {
                                 moi->edit.getUndoManager().beginNewTransaction();
                                 moi->selection.choisir (nullptr);
                                 moi->edit.deleteTrack (garde.get());
                                 moi->reconstruireEnTetes();
                             }
                             else if (choix == 900)
                             {
                                 moi->importerSon (*garde, std::round (moi->positionEnTemps()));
                             }
                             else if (choix <= ids.size())
                             {
                                 moi->appliquerSurPiste (*garde, ids[choix - 1]);
                             }
                         });
    }

    //==========================================================================
    void timerCallback() override
    {
        if (pistesOntChange())      // après un « annuler », par exemple
            reconstruireEnTetes();

        {
            auto* choisie = selection.pisteValide();
            for (auto* e : enTetes)
                e->definirChoisie (&e->getPiste() == choisie);
        }

        const auto zone = zonePistes();
        int x = tempsVersX (positionEnTemps());

        // Pendant la lecture, la vue suit le trait de lecture
        if (edit.getTransport().isPlaying() && (x > zone.getRight() - 20 || x < zone.getX()))
        {
            debutTemps = juce::jmax (0.0, positionEnTemps() - (zone.getWidth() * 0.1) / pxParTemps);
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
