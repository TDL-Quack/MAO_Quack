#pragma once

#include "Commun.h"
#include "Instruments.h"
#include "Effets.h"

//==============================================================================
/** Le navigateur (à gauche) : instruments, effets et favoris.
    Double-clic : mettre l'élément sur la piste choisie. On peut aussi le glisser sur une piste. */
class Navigateur : public juce::Component,
                   private juce::Timer
{
public:
    Navigateur()
    {
        favoris = Reglages::lireFavoris();
        recharger();
        definirAideParDefaut();
        startTimer (1500);
    }

    /** Appelé au double-clic sur un élément (identifiant du catalogue). */
    std::function<void (const juce::String&)> surChoix;

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);

        auto r = getLocalBounds().reduced (1);

        // Catégories
        for (int i = 0; i < 3; ++i)
        {
            auto ligne = zoneCategorie (i);
            const bool active = (i == categorie);

            g.setColour (active ? Couleurs::panneauClair.brighter (0.1f) : Couleurs::panneau);
            g.fillRect (ligne);

            auto icone = ligne.removeFromLeft (30).withSizeKeepingCentre (14, 14).toFloat();

            if (i == 0)
            {
                g.setColour (Couleurs::accent);
                g.fillPath (etoile (icone));
            }
            else
            {
                g.setColour (i == 1 ? Couleurs::dePiste (5) : Couleurs::dePiste (3));
                g.fillRoundedRectangle (icone.reduced (1.0f), 3.0f);
            }

            g.setColour (active ? juce::Colours::white : Couleurs::texte);
            g.setFont (juce::FontOptions (14.5f, active ? juce::Font::bold : juce::Font::plain));
            g.drawText (nomCategorie (i), ligne, juce::Justification::centredLeft, true);
        }

        g.setColour (Couleurs::bordure);
        g.drawHorizontalLine (zoneListe().getY() - 1, 0.0f, (float) getWidth());

        // Éléments
        const auto zone = zoneListe();
        g.saveState();
        g.reduceClipRegion (zone);

        const auto visibles = elementsVisibles();

        if (visibles.isEmpty())
        {
            g.setColour (Couleurs::texteDoux);
            g.setFont (juce::FontOptions (13.0f));
            g.drawFittedText (categorie == 0 ? fr ("Pas encore de favori.\nClique sur l'étoile d'un instrument ou d'un effet pour le retrouver ici.")
                                             : fr ("Rien à afficher."),
                              zone.reduced (12, 10), juce::Justification::topLeft, 5);
        }

        for (int i = 0; i < visibles.size(); ++i)
        {
            auto ligne = zoneElement (i);
            if (! ligne.intersects (zone))
                continue;

            const auto& e = visibles.getReference (i);

            if (i == survol)
            {
                g.setColour (Couleurs::panneauClair);
                g.fillRect (ligne);
            }

            auto zoneEtoile = ligne.removeFromRight (30).withSizeKeepingCentre (15, 15).toFloat();
            const bool fav = favoris.contains (e.id);

            if (fav)
            {
                g.setColour (Couleurs::accent);
                g.fillPath (etoile (zoneEtoile));
            }
            else if (i == survol)
            {
                g.setColour (Couleurs::texteDoux);
                g.strokePath (etoile (zoneEtoile), juce::PathStrokeType (1.0f));
            }

            g.setColour (e.effet ? Couleurs::dePiste (3) : Couleurs::dePiste (5));
            g.fillRoundedRectangle (ligne.removeFromLeft (26).withSizeKeepingCentre (8, 8).toFloat(), 2.0f);

            g.setColour (Couleurs::texte);
            g.setFont (juce::FontOptions (14.0f));
            g.drawText (e.nom, ligne, juce::Justification::centredLeft, true);
        }

        g.restoreState();

        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());
        juce::ignoreUnused (r);
    }

    //==========================================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int i = elementEnPosition (e.getPosition());

        if (i != survol)
        {
            survol = i;
            const auto visibles = elementsVisibles();

            if (i >= 0 && i < visibles.size())
                definirAide (*this, visibles[i].nom,
                             visibles[i].description + fr (" Double-clic : le mettre sur la piste choisie. "
                                                           "Tu peux aussi le glisser sur une piste. L'étoile l'ajoute aux favoris."));
            else
                definirAideParDefaut();

            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        survol = -1;
        definirAideParDefaut();
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
        {
            if (zoneCategorie (i).contains (e.getPosition()))
            {
                categorie = i;
                defilement = 0;
                survol = -1;
                repaint();
                return;
            }
        }

        const int i = elementEnPosition (e.getPosition());
        const auto visibles = elementsVisibles();

        if (i < 0 || i >= visibles.size())
            return;

        const auto element = visibles[i];

        if (e.x > getWidth() - 32)
        {
            // L'étoile
            if (favoris.contains (element.id))   favoris.removeString (element.id);
            else                                 favoris.add (element.id);

            Reglages::ecrireFavoris (favoris);
            repaint();
            return;
        }

        if (e.mods.isPopupMenu() && element.utilisateur)
        {
            juce::PopupMenu m;
            m.addItem (1, fr ("Supprimer ce préréglage"));
            juce::Component::SafePointer<Navigateur> moi (this);

            m.showMenuAsync (juce::PopupMenu::Options(), [moi, element] (int choix)
            {
                if (moi != nullptr && choix == 1)
                {
                    Instruments::fichierPreset (element.nom).deleteFile();
                    moi->favoris.removeString (element.id);
                    Reglages::ecrireFavoris (moi->favoris);
                    moi->recharger();
                    moi->repaint();
                }
            });
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 6 || e.getMouseDownX() > getWidth() - 32)
            return;

        const int i = elementEnPosition (e.getMouseDownPosition());
        const auto visibles = elementsVisibles();

        if (i >= 0 && i < visibles.size())
            if (auto* conteneur = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! conteneur->isDragAndDropActive())
                    conteneur->startDragging (visibles[i].id, this);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x > getWidth() - 32)
            return;

        const int i = elementEnPosition (e.getPosition());
        const auto visibles = elementsVisibles();

        if (i >= 0 && i < visibles.size() && surChoix)
            surChoix (visibles[i].id);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int maxi = juce::jmax (0, elementsVisibles().size() * hauteurLigne - zoneListe().getHeight());
        defilement = juce::jlimit (0, maxi, defilement - juce::roundToInt (w.deltaY * 200.0f));
        repaint();
    }

private:
    struct Element
    {
        juce::String id, nom, description;
        bool effet = false, utilisateur = false;
    };

    static constexpr int hauteurLigne = 28;

    juce::Array<Element> elements;
    juce::StringArray favoris;
    int categorie = 1;          // 0 favoris, 1 instruments, 2 effets
    int survol = -1;
    int defilement = 0;

    //==========================================================================
    static juce::String nomCategorie (int i)
    {
        switch (i)
        {
            case 0:  return "Favoris";
            case 1:  return "Instruments";
            default: return "Effets";
        }
    }

    static juce::String descriptionInstrument (const juce::String& id)
    {
        if (id == "inst:batterie")  return fr ("Une batterie électronique de 8 sons : grosse caisse, caisse claire, charleys, clap, toms, cymbale.");
        if (id == "inst:basse")     return fr ("Une basse ronde et profonde, pour les notes graves.");
        if (id == "inst:melodie")   return fr ("Un son brillant pour les mélodies.");
        if (id == "inst:accords")   return fr ("Une nappe douce et large, idéale pour les accords tenus.");
        if (id == "inst:cloches")   return fr ("Un son de cloche qui s'éteint doucement.");
        if (id == "inst:orgue")     return fr ("Un orgue, au son plein et constant.");
        if (id == "inst:jeuvideo")  return fr ("Un son carré, comme dans les vieux jeux vidéo.");
        return fr ("Un de tes préréglages personnels. Clic droit : le supprimer.");
    }

    void recharger()
    {
        elements.clear();

        for (auto& e : Instruments::catalogue())
            elements.add ({ e.id, e.nom, descriptionInstrument (e.id), false, e.utilisateur });

        for (auto& e : Effets::tous())
            elements.add ({ e.id, e.nom, fr (e.description), true, false });
    }

    juce::Array<Element> elementsVisibles() const
    {
        juce::Array<Element> liste;

        for (auto& e : elements)
            if ((categorie == 0 && favoris.contains (e.id)) || (categorie == 1 && ! e.effet) || (categorie == 2 && e.effet))
                liste.add (e);

        return liste;
    }

    juce::Rectangle<int> zoneCategorie (int i) const    { return { 1, 4 + i * 30, getWidth() - 2, 30 }; }
    juce::Rectangle<int> zoneListe() const              { return getLocalBounds().withTrimmedTop (4 + 3 * 30 + 6).reduced (1); }

    juce::Rectangle<int> zoneElement (int i) const
    {
        const auto z = zoneListe();
        return { z.getX(), z.getY() + i * hauteurLigne - defilement, z.getWidth(), hauteurLigne };
    }

    int elementEnPosition (juce::Point<int> p) const
    {
        const auto z = zoneListe();
        if (! z.contains (p))
            return -1;

        const int i = (p.y - z.getY() + defilement) / hauteurLigne;
        return i < elementsVisibles().size() ? i : -1;
    }

    static juce::Path etoile (juce::Rectangle<float> r)
    {
        juce::Path p;
        p.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
        return p;
    }

    void definirAideParDefaut()
    {
        definirAide (*this, fr ("Navigateur"),
                     fr ("La réserve d'instruments et d'effets. Double-clique sur un élément pour le mettre sur la piste "
                         "choisie, ou glisse-le sur une piste. Clique sur l'étoile pour le ranger dans tes favoris."));
    }

    void timerCallback() override
    {
        // Un préréglage a pu être enregistré depuis le synthé
        const int avant = elements.size();
        recharger();

        if (elements.size() != avant)
            repaint();
    }
};
