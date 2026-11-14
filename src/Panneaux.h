#pragma once

#include "Commun.h"
#include "Instruments.h"
#include "Effets.h"

//==============================================================================
/** Le navigateur (à gauche), en arborescence :
        Favoris
        Instruments > Quack Synth > les préréglages
                    > Quack Drums > les kits
        Effets      > EQ, Compressor...
    Double-clic : mettre l'élément sur la piste choisie. On peut aussi le glisser sur une piste
    ou dans la chaîne d'effets, à l'endroit voulu. L'étoile range un élément dans les favoris. */
class Navigateur : public juce::Component,
                   private juce::Timer
{
public:
    Navigateur (te::Engine& e) : engine (e)
    {
        favoris = Reglages::lireFavoris();
        ouverts = { "Favoris", "Instruments", "Quack Synth", "Effets", fr ("Plugins VST à ranger") };
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

        const auto zone = getLocalBounds().reduced (1);
        g.saveState();
        g.reduceClipRegion (zone);

        const auto lignes = lignesVisibles();

        for (int i = 0; i < lignes.size(); ++i)
        {
            auto r = zoneLigne (i);
            if (! r.intersects (zone))
                continue;

            const auto& l = lignes.getReference (i);

            if (i == survol)
            {
                g.setColour (Couleurs::panneauClair);
                g.fillRect (r);
            }

            r.removeFromLeft (6 + l.profondeur * 14);

            if (l.dossier)
            {
                // Petit triangle : vers la droite si fermé, vers le bas si ouvert
                auto t = r.removeFromLeft (16).withSizeKeepingCentre (9, 9).toFloat();
                juce::Path p;

                if (ouverts.contains (l.nom))
                    p.addTriangle (t.getX(), t.getY() + 1.0f, t.getRight(), t.getY() + 1.0f, t.getCentreX(), t.getBottom() - 1.0f);
                else
                    p.addTriangle (t.getX() + 1.0f, t.getY(), t.getX() + 1.0f, t.getBottom(), t.getRight() - 1.0f, t.getCentreY());

                g.setColour (Couleurs::texteDoux);
                g.fillPath (p);

                if (l.nom == "Favoris")
                {
                    g.setColour (Couleurs::accent);
                    g.fillPath (etoile (r.removeFromLeft (18).withSizeKeepingCentre (13, 13).toFloat()));
                }

                g.setColour (l.profondeur == 0 ? juce::Colours::white : Couleurs::texte);
                g.setFont (juce::FontOptions (l.profondeur == 0 ? 14.5f : 14.0f, juce::Font::bold));
                g.drawText (l.nom, r.withTrimmedLeft (2), juce::Justification::centredLeft, true);
            }
            else if (l.id.isEmpty())
            {
                // Une simple ligne d'information
                g.setColour (Couleurs::texteDoux);
                g.setFont (juce::FontOptions (12.5f));
                g.drawText (l.nom, r.withTrimmedLeft (4), juce::Justification::centredLeft, true);
            }
            else
            {
                auto zoneEtoile = r.removeFromRight (28).withSizeKeepingCentre (15, 15).toFloat();

                if (favoris.contains (l.id))
                {
                    g.setColour (Couleurs::accent);
                    g.fillPath (etoile (zoneEtoile));
                }
                else if (i == survol)
                {
                    g.setColour (Couleurs::texteDoux);
                    g.strokePath (etoile (zoneEtoile), juce::PathStrokeType (1.0f));
                }

                g.setColour (l.effet ? Couleurs::dePiste (3) : Couleurs::dePiste (5));
                g.fillRoundedRectangle (r.removeFromLeft (18).withSizeKeepingCentre (8, 8).toFloat(), 2.0f);

                g.setColour (Couleurs::texte);
                g.setFont (juce::FontOptions (13.5f));
                g.drawText (l.nom, r, juce::Justification::centredLeft, true);
            }
        }

        g.restoreState();
        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());
    }

    //==========================================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int i = ligneEnPosition (e.getPosition());

        if (i != survol)
        {
            survol = i;
            const auto lignes = lignesVisibles();

            if (i >= 0 && i < lignes.size() && lignes[i].dossier && lignes[i].id.isNotEmpty())
                definirAide (*this, lignes[i].nom,
                             lignes[i].description + fr (" Clique pour ouvrir ou fermer le dossier. Tu peux aussi le glisser sur "
                                                         "une piste (ou double-cliquer) pour poser l'instrument avec son son de base."));
            else if (i >= 0 && i < lignes.size() && ! lignes[i].dossier && lignes[i].id.isNotEmpty())
                definirAide (*this, lignes[i].nom,
                             lignes[i].description + fr (" Double-clic : le mettre sur la piste choisie. Tu peux aussi le glisser "
                                                         "sur une piste ou dans la chaîne d'effets. L'étoile l'ajoute aux favoris."));
            else if (i >= 0 && i < lignes.size())
                definirAide (*this, lignes[i].nom, lignes[i].description + fr (" Clique pour ouvrir ou fermer ce dossier."));
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
        const int i = ligneEnPosition (e.getPosition());
        const auto lignes = lignesVisibles();

        if (i < 0 || i >= lignes.size())
            return;

        const auto ligne = lignes[i];

        if (ligne.dossier)
        {
            if (ouverts.contains (ligne.nom))   ouverts.removeString (ligne.nom);
            else                                ouverts.add (ligne.nom);

            borner();
            repaint();
            return;
        }

        if (e.x > getWidth() - 30 && ligne.id.isNotEmpty())
        {
            // L'étoile
            if (favoris.contains (ligne.id))   favoris.removeString (ligne.id);
            else                               favoris.add (ligne.id);

            Reglages::ecrireFavoris (favoris);
            repaint();
            return;
        }

        if (ligne.id.isEmpty())
            return;

        if (e.mods.isPopupMenu() && Vst::estIdVst (ligne.id))
        {
            menuRangement (ligne);
            return;
        }

        if (e.mods.isPopupMenu() && ligne.utilisateur)
        {
            juce::PopupMenu m;
            m.addItem (1, fr ("Supprimer ce préréglage"));
            juce::Component::SafePointer<Navigateur> moi (this);

            m.showMenuAsync (juce::PopupMenu::Options(), [moi, ligne] (int choix)
            {
                if (moi != nullptr && choix == 1)
                {
                    Instruments::fichierPreset (ligne.nom).deleteFile();
                    moi->favoris.removeString (ligne.id);
                    Reglages::ecrireFavoris (moi->favoris);
                    moi->recharger();
                    moi->repaint();
                }
            });
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 6 || e.getMouseDownX() > getWidth() - 30)
            return;

        const int i = ligneEnPosition (e.getMouseDownPosition());
        const auto lignes = lignesVisibles();

        // On emporte seulement une petite étiquette, pas tout le navigateur
        if (i >= 0 && i < lignes.size() && lignes[i].id.isNotEmpty())
            if (auto* conteneur = juce::DragAndDropContainer::findParentDragContainerFor (this))
                if (! conteneur->isDragAndDropActive())
                    conteneur->startDragging (lignes[i].id, this,
                                              imageGlisser (lignes[i].nom, lignes[i].effet ? Couleurs::dePiste (3) : Couleurs::dePiste (5)));
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x > getWidth() - 30)
            return;

        const int i = ligneEnPosition (e.getPosition());
        const auto lignes = lignesVisibles();

        if (i >= 0 && i < lignes.size() && lignes[i].id.isNotEmpty() && surChoix)
            surChoix (lignes[i].id);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        defilement -= juce::roundToInt (w.deltaY * 200.0f);
        borner();
        repaint();
    }

private:
    struct Ligne
    {
        int profondeur = 0;
        bool dossier = false;
        juce::String id, nom, description;
        bool effet = false, utilisateur = false;
    };

    static constexpr int hauteurLigne = 26;

    te::Engine& engine;
    juce::Array<Ligne> elements;        // tous les instruments et effets fournis (sans les dossiers)
    juce::Array<Ligne> plugins;         // les plugins VST
    juce::StringArray favoris, ouverts;
    int survol = -1;
    int defilement = 0;

    //==========================================================================
    static juce::String descriptionInstrument (const juce::String& id)
    {
        if (id == "inst:batterie")  return fr ("Une batterie électronique de 8 sons : grosse caisse, caisse claire, charleys, clap, toms, cymbale.");
        if (id == "inst:basse")     return fr ("Une basse ronde et profonde, pour les notes graves.");
        if (id == "inst:melodie")   return fr ("Un son brillant pour les mélodies.");
        if (id == "inst:accords")   return fr ("Une nappe douce et large, idéale pour les accords tenus.");
        if (id == "inst:cloches")   return fr ("Un son de cloche qui s'éteint doucement.");
        if (id == "inst:orgue")     return fr ("Un orgue, au son plein et constant.");
        if (id == "inst:jeuvideo")  return fr ("Un son carré, comme dans les vieux jeux vidéo.");
        if (id == "inst:sampler")   return fr ("Le sampler : il joue un son enregistré comme un instrument, au clavier ou découpé en tranches. Glisse-y ensuite un fichier son.");
        if (id == "inst:init")      return fr ("Le son de base du synthé, tout simple : le point de départ pour fabriquer ton propre son.");
        return fr ("Un de tes préréglages personnels. Clic droit : le supprimer.");
    }

    void recharger()
    {
        elements.clear();

        for (auto& e : Instruments::catalogue())
        {
            Ligne l;
            l.id = e.id;
            l.nom = e.id == "inst:batterie" ? juce::String ("Quack Drums") : e.nom;
            l.description = descriptionInstrument (e.id);
            l.utilisateur = e.utilisateur;
            elements.add (l);
        }

        for (auto& e : Effets::tous())
        {
            Ligne l;
            l.id = e.id;
            l.nom = e.nom;
            l.description = fr (e.description);
            l.effet = true;
            elements.add (l);
        }

        // Les plugins VST trouvés sur l'ordinateur
        plugins.clear();

        for (auto& d : Vst::liste (engine))
        {
            Ligne l;
            l.id = Vst::idDe (d);
            l.nom = d.name;
            l.description = (d.isInstrument ? fr ("Instrument VST de ") : fr ("Effet VST de ")) + d.manufacturerName
                              + fr (". Clic droit : le ranger dans une catégorie.");
            l.effet = ! d.isInstrument;
            plugins.add (l);
        }
    }

    /** Où un plugin est rangé : « » (à ranger), « Instruments », « Effets », ou « Instruments/Dossier »...
        Les catégories créées avec une version plus ancienne (un simple nom) deviennent des sous-dossiers. */
    static juce::String categorieNormalisee (const Ligne& plugin)
    {
        const auto c = Vst::categories()[plugin.id];

        if (c.isEmpty() || c == Vst::sansCategorie())
            return {};

        if (c == "Instruments" || c == "Effets" || c.startsWith ("Instruments/") || c.startsWith ("Effets/"))
            return c;

        return juce::String (plugin.effet ? "Effets/" : "Instruments/") + c;
    }

    /** Menu « Ranger dans... » d'un plugin VST. */
    void menuRangement (const Ligne& ligne)
    {
        const auto actuelle = categorieNormalisee (ligne);
        const juce::String racine = ligne.effet ? "Effets" : "Instruments";

        // Les sous-dossiers déjà créés
        juce::StringArray dossiers;
        for (auto& e : plugins)
        {
            const auto c = categorieNormalisee (e);
            if (c.contains ("/"))
                dossiers.addIfNotAlreadyThere (c);
        }
        dossiers.sort (true);

        juce::PopupMenu m;
        m.addSectionHeader (fr ("Ranger « ") + ligne.nom + fr (" » dans :"));
        m.addItem (1, "Instruments", true, actuelle == "Instruments");
        m.addItem (2, "Effets", true, actuelle == "Effets");

        if (! dossiers.isEmpty())
            m.addSeparator();

        int id = 10;
        for (auto& d : dossiers)
            m.addItem (id++, d.replace ("/", "  >  "), true, d == actuelle);

        m.addSeparator();
        m.addItem (500, fr ("Nouveau dossier dans ") + racine + "...");
        m.addItem (501, fr ("Retirer du rangement (à ranger)"), actuelle.isNotEmpty());

        juce::Component::SafePointer<Navigateur> moi (this);
        const auto idPlugin = ligne.id;

        m.showMenuAsync (juce::PopupMenu::Options(), [moi, idPlugin, dossiers, racine] (int choix)
        {
            if (moi == nullptr || choix <= 0)
                return;

            if (choix == 500)
            {
                moi->demanderNouvelleCategorie (idPlugin, racine);
                return;
            }

            juce::String categorie;
            if (choix == 1)                                         categorie = "Instruments";
            else if (choix == 2)                                    categorie = "Effets";
            else if (choix >= 10 && choix - 10 < dossiers.size())   categorie = dossiers[choix - 10];

            Vst::definirCategorie (idPlugin, categorie);
            moi->ouverts.addIfNotAlreadyThere (categorie.upToFirstOccurrenceOf ("/", false, false));
            moi->ouverts.addIfNotAlreadyThere (categorie.fromFirstOccurrenceOf ("/", false, false));
            moi->repaint();
        });
    }

    void demanderNouvelleCategorie (const juce::String& idPlugin, const juce::String& racine)
    {
        auto* fenetre = new juce::AlertWindow (fr ("Nouveau dossier dans ") + racine,
                                               fr ("Par exemple : Synthés, Pianos, Réverbes, Distorsions..."),
                                               juce::MessageBoxIconType::NoIcon, this);
        fenetre->addTextEditor ("nom", "");
        fenetre->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        fenetre->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<Navigateur> moi (this);

        fenetre->enterModalState (true, juce::ModalCallbackFunction::create ([moi, fenetre, idPlugin, racine] (int resultat)
        {
            const auto nom = fenetre->getTextEditorContents ("nom").trim().replace ("/", "-");

            if (moi != nullptr && resultat == 1 && nom.isNotEmpty())
            {
                Vst::definirCategorie (idPlugin, racine + "/" + nom);
                moi->ouverts.addIfNotAlreadyThere (racine);
                moi->ouverts.addIfNotAlreadyThere (nom);
                moi->repaint();
            }
        }), true);
    }

    /** Un dossier. S'il a un identifiant, on peut aussi le glisser comme un instrument (son de base). */
    static Ligne dossier (int profondeur, const juce::String& nom, const juce::String& description, const juce::String& id = {})
    {
        Ligne l;
        l.profondeur = profondeur;
        l.dossier = true;
        l.nom = nom;
        l.description = description;
        l.id = id;
        return l;
    }

    /** L'arborescence telle qu'elle est affichée, dossiers fermés exclus. */
    juce::Array<Ligne> lignesVisibles() const
    {
        juce::Array<Ligne> lignes;
        auto ajouterElement = [&lignes] (Ligne l, int profondeur)
        {
            l.profondeur = profondeur;
            lignes.add (l);
        };

        lignes.add (dossier (0, "Favoris", fr ("Tes instruments et effets préférés, marqués d'une étoile.")));
        if (ouverts.contains ("Favoris"))
        {
            for (auto& e : elements)
                if (favoris.contains (e.id))
                    ajouterElement (e, 1);

            for (auto& e : plugins)
                if (favoris.contains (e.id))
                    ajouterElement (e, 1);
        }

        // Les plugins VST se rangent avec le reste : sous « Instruments » ou « Effets », directement
        // ou dans un sous-dossier. Leur catégorie s'écrit « Instruments », « Effets/Réverbes », etc.
        auto ajouterPlugins = [&] (const juce::String& racine)
        {
            for (auto& e : plugins)
                if (categorieNormalisee (e) == racine)
                    ajouterElement (e, 1);

            juce::StringArray sousDossiers;
            for (auto& e : plugins)
            {
                const auto c = categorieNormalisee (e);
                if (c.startsWith (racine + "/"))
                    sousDossiers.addIfNotAlreadyThere (c);
            }

            sousDossiers.sort (true);

            for (auto& chemin : sousDossiers)
            {
                const auto nom = chemin.fromFirstOccurrenceOf ("/", false, false);
                lignes.add (dossier (1, nom, fr ("Un de tes dossiers de plugins. Clic droit sur un plugin pour le changer de place.")));

                if (ouverts.contains (nom))
                    for (auto& e : plugins)
                        if (categorieNormalisee (e) == chemin)
                            ajouterElement (e, 2);
            }
        };

        lignes.add (dossier (0, "Instruments", fr ("Ce qui fabrique le son d'une piste.")));
        if (ouverts.contains ("Instruments"))
        {
            lignes.add (dossier (1, "Quack Synth", fr ("Le synthétiseur : chaque préréglage est un son tout prêt, que tu peux ensuite modifier."), "inst:init"));
            if (ouverts.contains ("Quack Synth"))
                for (auto& e : elements)
                    if (! e.effet && e.id != "inst:batterie" && e.id != "inst:sampler")
                        ajouterElement (e, 2);

            for (auto& e : elements)
                if (e.id == "inst:batterie" || e.id == "inst:sampler")
                    ajouterElement (e, 1);

            ajouterPlugins ("Instruments");
        }

        lignes.add (dossier (0, "Effets", fr ("Ce qui transforme le son d'une piste, après l'instrument.")));
        if (ouverts.contains ("Effets"))
        {
            for (auto& e : elements)
                if (e.effet)
                    ajouterElement (e, 1);

            ajouterPlugins ("Effets");
        }

        // Les plugins VST fraîchement trouvés attendent ici d'être rangés
        const auto aRanger = fr ("Plugins VST à ranger");
        bool resteARanger = false;
        for (auto& e : plugins)
            resteARanger = resteARanger || categorieNormalisee (e).isEmpty();

        if (resteARanger || plugins.isEmpty())
        {
            lignes.add (dossier (0, aRanger, fr ("Les plugins VST3 trouvés sur l'ordinateur et pas encore rangés. Clic droit sur un plugin "
                                                 "pour le ranger dans Instruments ou Effets. Pour chercher tes plugins : menu Options > Plugins VST.")));
            if (ouverts.contains (aRanger))
            {
                if (plugins.isEmpty())
                {
                    Ligne info;
                    info.profondeur = 1;
                    info.nom = fr ("Aucun : menu Options > Plugins VST");
                    lignes.add (info);
                }

                for (auto& e : plugins)
                    if (categorieNormalisee (e).isEmpty())
                        ajouterElement (e, 1);
            }
        }

        return lignes;
    }

    juce::Rectangle<int> zoneLigne (int i) const
    {
        return { 1, 4 + i * hauteurLigne - defilement, getWidth() - 2, hauteurLigne };
    }

    int ligneEnPosition (juce::Point<int> p) const
    {
        if (p.y < 4 || p.x < 0 || p.x >= getWidth())
            return -1;

        const int i = (p.y - 4 + defilement) / hauteurLigne;
        return i < lignesVisibles().size() ? i : -1;
    }

    void borner()
    {
        const int maxi = juce::jmax (0, lignesVisibles().size() * hauteurLigne + 8 - getHeight());
        defilement = juce::jlimit (0, maxi, defilement);
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
                     fr ("La réserve d'instruments et d'effets, rangée en dossiers. Double-clique sur un élément pour le mettre "
                         "sur la piste choisie, ou glisse-le sur une piste ou dans la chaîne d'effets. "
                         "Clique sur l'étoile pour le ranger dans tes favoris."));
    }

    void timerCallback() override
    {
        // Un préréglage a pu être enregistré depuis le synthé
        const int avant = elements.size() + plugins.size();
        recharger();

        if (elements.size() + plugins.size() != avant)
            repaint();
    }
};
