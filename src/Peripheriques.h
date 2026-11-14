#pragma once

#include "Commun.h"
#include "Widgets.h"
#include "Instruments.h"
#include "Effets.h"

//==============================================================================
/** Un petit texte de titre de section, posé comme un composant (pour pouvoir le cacher avec sa page). */
class TexteSection : public juce::Component
{
public:
    TexteSection (const juce::String& t) : texte (t)    { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (texte, getLocalBounds(), juce::Justification::centredLeft, false);
    }

private:
    juce::String texte;
};

//==============================================================================
/** Un périphérique (instrument ou effet) affiché dans la chaîne du bas, comme dans Ableton.
    Son en-tête permet de l'éteindre, de le réduire, de le déplacer (en le glissant) ou de le supprimer. */
class Peripherique : public juce::Component
{
public:
    static constexpr int hauteurEntete = 24;
    static constexpr int hauteurTotale = 298;
    static constexpr int largeurReduite = 30;

    Peripherique (te::Plugin& p, const juce::String& titreAffiche, int largeurVoulue, bool estUnEffet, juce::Colour c)
        : plugin (&p), titre (titreAffiche), largeur (largeurVoulue), couleur (c), effet (estUnEffet)
    {
        reduit = (bool) plugin->state.getProperty (idReduit(), false);

        addAndMakeVisible (marche);
        marche.setButtonText ("On");
        marche.setClickingTogglesState (true);
        marche.setWantsKeyboardFocus (false);
        marche.setColour (juce::TextButton::buttonOnColourId, Couleurs::accent);
        marche.onClick = [this]
        {
            plugin->edit.getUndoManager().beginNewTransaction();
            plugin->setEnabled (marche.getToggleState());
        };
        definirAide (marche, fr ("Activer / désactiver"),
                     fr ("Allume ou éteint ce périphérique sans le supprimer. Pratique pour comparer avec et sans."));

        addAndMakeVisible (reduire);
        reduire.setWantsKeyboardFocus (false);
        reduire.onClick = [this] { definirReduit (! reduit); };
        definirAide (reduire, fr ("Réduire / agrandir"),
                     fr ("Replie ce périphérique en une fine bande pour gagner de la place, ou le rouvre. "
                         "Un double-clic sur sa barre de titre fait la même chose."));

        if (effet)
        {
            addAndMakeVisible (supprimer);
            supprimer.setButtonText ("X");
            supprimer.setWantsKeyboardFocus (false);
            supprimer.onClick = [this] { if (surSupprimer) surSupprimer(); };
            definirAide (supprimer, fr ("Supprimer l'effet"), fr ("Retire cet effet de la piste."));
        }

        definirAide (*this, titre,
                     effet ? fr ("Un effet de la chaîne. Attrape sa barre de titre et glisse-la pour le déplacer avant ou après "
                                 "un autre effet. Double-clic sur la barre : réduire.")
                           : fr ("L'instrument de la piste : c'est lui qui fabrique le son, toujours en premier dans la chaîne. "
                                 "Double-clic sur la barre de titre : réduire."));
    }

    te::Plugin& getPlugin()         { return *plugin; }
    bool estEffet() const           { return effet; }
    bool estReduit() const          { return reduit; }
    int getLargeur() const          { return reduit ? largeurReduite : largeur; }

    std::function<void()> surSupprimer;
    std::function<void()> surMiseEnPage;       // la largeur a changé : la chaîne doit se replacer

    void definirReduit (bool r)
    {
        reduit = r;
        plugin->state.setProperty (idReduit(), reduit, nullptr);
        appliquerVisibilite();
        resized();
        repaint();

        if (surMiseEnPage)
            surMiseEnPage();
    }

    void rafraichirTout()
    {
        marche.setToggleState (plugin->isEnabled(), juce::dontSendNotification);

        if (reduit)
            return;

        for (auto* r : aRafraichir)
            r->rafraichir();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneauClair.darker (0.25f));
        const auto teinte = couleur.withMultipliedSaturation (0.75f).darker (0.15f);

        if (reduit)
        {
            g.fillAll (teinte);
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));

            // Le titre écrit de bas en haut
            juce::Graphics::ScopedSaveState etat (g);
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                             (float) getWidth() * 0.5f, (float) getHeight() * 0.5f));
            g.drawText (titre, juce::Rectangle<int> (0, 0, getHeight() - 80, getWidth())
                                   .withCentre ({ getWidth() / 2, getHeight() / 2 + 30 }),
                        juce::Justification::centredLeft, true);
        }
        else
        {
            auto entete = getLocalBounds().removeFromTop (hauteurEntete);
            g.setColour (teinte);
            g.fillRect (entete);

            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
            g.drawText (titre, entete.withTrimmedLeft (44).withTrimmedRight (56), juce::Justification::centredLeft, true);

            g.setColour (Couleurs::texteDoux);
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            for (auto& e : etiquettes)
                g.drawText (e.second, e.first, juce::Justification::centredLeft, false);

            g.setColour (Couleurs::bordure);
            for (int x : separateurs)
                g.drawVerticalLine (x, (float) hauteurEntete + 4.0f, (float) getHeight() - 4.0f);
        }

        g.setColour (Couleurs::bordure);
        g.drawRect (getLocalBounds());
    }

    void resized() override
    {
        reduire.setButtonText (reduit ? "+" : "-");

        if (reduit)
        {
            marche.setBounds (2, 3, largeurReduite - 4, 18);
            reduire.setBounds (2, 24, largeurReduite - 4, 18);
            supprimer.setBounds (2, 45, largeurReduite - 4, 18);
        }
        else
        {
            marche.setBounds (4, 3, 36, hauteurEntete - 6);
            supprimer.setBounds (getWidth() - 26, 3, 22, hauteurEntete - 6);
            reduire.setBounds (getWidth() - (effet ? 50 : 26), 3, 22, hauteurEntete - 6);
        }
    }

    //==========================================================================
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (reduit || e.y < hauteurEntete)
            definirReduit (! reduit);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Glisser la barre de titre d'un effet pour le déplacer dans la chaîne
        if (! effet || e.getDistanceFromDragStart() < 6 || (! reduit && e.getMouseDownY() >= hauteurEntete))
            return;

        if (auto* conteneur = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! conteneur->isDragAndDropActive())
                conteneur->startDragging ("peripherique", this);
    }

protected:
    te::Plugin::Ptr plugin;
    juce::String titre;
    int largeur;
    juce::Colour couleur;
    bool effet = false, reduit = false;

    static const juce::Identifier& idReduit()
    {
        static const juce::Identifier id ("quackReduit");
        return id;
    }

    /** Crée un composant, le garde, l'affiche et le place (y compté sous l'en-tête). */
    template <typename Type, typename... Args>
    Type& ajouter (juce::Rectangle<int> place, Args&&... args)
    {
        auto* c = new Type (std::forward<Args> (args)...);
        possede.add (c);
        addChildComponent (c);
        c->setBounds (place.translated (0, hauteurEntete));
        c->setVisible (! reduit);

        if (auto* r = dynamic_cast<Rafraichissable*> (c))
            aRafraichir.push_back (r);

        composantAjoute (*c);
        return *c;
    }

    /** Appelé à chaque composant créé (le synthé s'en sert pour ranger ses composants par page). */
    virtual void composantAjoute (juce::Component&) {}

    /** Montre ou cache le contenu selon que le périphérique est réduit ou non. */
    virtual void appliquerVisibilite()
    {
        for (auto* c : possede)
            c->setVisible (! reduit);
    }

    static constexpr int lp = 44, hp = 54;      // taille d'un potard

    Potard& potard (int x, int y, te::AutomatableParameter::Ptr p, const juce::String& nom, const juce::String& aide)
    {
        return ajouter<Potard> (juce::Rectangle<int> (x, y, lp, hp), nom, Potard::versParametre (p), aide);
    }

    Potard& potardLibre (int x, int y, const juce::String& nom, Potard::Lien lien, const juce::String& aide)
    {
        return ajouter<Potard> (juce::Rectangle<int> (x, y, lp, hp), nom, std::move (lien), aide);
    }

    void etiquette (int x, int y, const juce::String& texte, int w = 120)
    {
        etiquettes.push_back ({ juce::Rectangle<int> (x, y + hauteurEntete, w, 14), texte });
    }

    void separateur (int x)     { separateurs.push_back (x); }

    void debutGeste()           { plugin->edit.getUndoManager().beginNewTransaction(); }

    juce::OwnedArray<juce::Component> possede;

private:
    juce::TextButton marche, reduire, supprimer;
    std::vector<Rafraichissable*> aRafraichir;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> etiquettes;
    std::vector<int> separateurs;
};

//==============================================================================
/** Un onglet qui s'ouvre tout seul quand on passe dessus en glissant une modulation. */
class OngletSynthe : public juce::TextButton,
                     public juce::DragAndDropTarget
{
public:
    using juce::TextButton::TextButton;

    bool isInterestedInDragSource (const SourceDetails& d) override     { return d.description.toString().startsWith ("mod:"); }
    void itemDragEnter (const SourceDetails&) override                  { if (onClick) onClick(); }
    void itemDropped (const SourceDetails&) override                    {}
};

//==============================================================================
/** Le synthétiseur, en trois onglets : OSC (les 4 oscillateurs), FILTER / MOD (filtre, enveloppes, LFO)
    et FX (effets). À gauche, toujours visibles : l'oscilloscope et les réglages généraux.
    Le son traverse dans cet ordre : oscillateurs -> filtre -> effets. */
class PeripheriqueSynthe : public Peripherique
{
public:
    PeripheriqueSynthe (te::FourOscPlugin& s, std::function<void (float*, int)> lectureOscillo, juce::Colour c)
        : Peripherique (s, "Quack Synth", 652, false, c), synthe (s)
    {
        construireGauche (std::move (lectureOscillo));
        construireOnglets();

        etiquettePage = 0;   construireOscillateurs (160, 26);
        etiquettePage = 1;   construireFiltre (160, 26);
                             construireEnveloppes (320, 26);
                             construireLfos (506, 26);
        etiquettePage = 2;   construireEffets (160, 26);
        etiquettePage = -1;

        // Préréglages (dans l'en-tête)
        addAndMakeVisible (presets);
        presets.setWantsKeyboardFocus (false);
        presets.setTextWhenNothingSelected (fr ("Préréglage..."));
        presets.onChange = [this]
        {
            const int i = presets.getSelectedItemIndex();
            if (i >= 0 && i < idsPresets.size() && surPreset)
                surPreset (idsPresets[i]);
        };
        definirAide (presets, fr ("Préréglages"),
                     fr ("Des sons tout prêts. Choisis-en un comme point de départ, puis modifie-le à ton goût."));

        addAndMakeVisible (enregistrer);
        enregistrer.setButtonText ("Save");
        enregistrer.setWantsKeyboardFocus (false);
        enregistrer.onClick = [this] { demanderNomPreset(); };
        definirAide (enregistrer, fr ("Enregistrer le préréglage"),
                     fr ("Garde tes réglages sous un nom : tu retrouveras ce son dans la liste et dans le navigateur."));

        remplirPresets();
        page = juce::jlimit (0, 2, (int) synthe.state.getProperty ("quackPage", 0));
        appliquerVisibilite();
    }

    /** Appelé quand l'utilisateur choisit un préréglage (identifiant du catalogue). */
    std::function<void (juce::String)> surPreset;

    void resized() override
    {
        Peripherique::resized();
        presets.setBounds (140, 3, 190, hauteurEntete - 6);
        enregistrer.setBounds (336, 3, 50, hauteurEntete - 6);
    }

protected:
    void composantAjoute (juce::Component& c) override
    {
        rangement.push_back ({ &c, etiquettePage, etiquetteEnv, etiquetteLfo });
    }

    void appliquerVisibilite() override
    {
        for (auto& r : rangement)
            r.composant->setVisible (! reduit
                                      && (r.page < 0 || r.page == page)
                                      && (r.env < 0 || r.env == envChoisie)
                                      && (r.lfo < 0 || r.lfo == lfoChoisi));

        presets.setVisible (! reduit);
        enregistrer.setVisible (! reduit);

        for (int i = 0; i < onglets.size(); ++i)
            onglets[i]->setToggleState (i == page, juce::dontSendNotification);

        for (int i = 0; i < ongletsEnv.size(); ++i)
            ongletsEnv[i]->setToggleState (i == envChoisie, juce::dontSendNotification);

        for (int i = 0; i < ongletsLfo.size(); ++i)
            ongletsLfo[i]->setToggleState (i == lfoChoisi, juce::dontSendNotification);
    }

private:
    struct Rangement { juce::Component* composant; int page, env, lfo; };

    te::FourOscPlugin& synthe;
    juce::ComboBox presets;
    juce::TextButton enregistrer;
    juce::StringArray idsPresets;

    std::vector<Rangement> rangement;
    juce::Array<juce::TextButton*> onglets, ongletsEnv, ongletsLfo;
    int etiquettePage = -1, etiquetteEnv = -1, etiquetteLfo = -1;      // où ranger les prochains composants créés
    int page = 0, envChoisie = 0, lfoChoisi = 0;

    //==========================================================================
    // Les sources de modulation, dans l'ordre des pastilles : LFO 1, LFO 2, ENV 2, ENV 3

    static constexpr int nombreSources = 4;

    static te::FourOscPlugin::ModSource sourceMoteur (int i)
    {
        static const te::FourOscPlugin::ModSource s[] = { te::FourOscPlugin::lfo1, te::FourOscPlugin::lfo2,
                                                          te::FourOscPlugin::env1, te::FourOscPlugin::env2 };
        return s[juce::jlimit (0, 3, i)];
    }

    static juce::String nomSource (int i)
    {
        static const char* const n[] = { "LFO 1", "LFO 2", "ENV 2", "ENV 3" };
        return n[juce::jlimit (0, 3, i)];
    }

    static juce::Colour couleurSource (int i)
    {
        static const juce::Colour c[] = { juce::Colour (0xff5ab8ff), juce::Colour (0xffb08cff),
                                          juce::Colour (0xff6fe07a), juce::Colour (0xffff8ac4) };
        return c[juce::jlimit (0, 3, i)];
    }

    /** Un potard du synthé : on peut y glisser un LFO ou une enveloppe. */
    Potard& potardMod (int x, int y, te::AutomatableParameter::Ptr p, const juce::String& nom, const juce::String& aide)
    {
        auto& pot = potard (x, y, p, nom, aide);

        Potard::Modulation m;
        m.nombreSources = nombreSources;
        m.lire = [this, p] (int i) { return synthe.getModulationDepth (sourceMoteur (i), p); };
        m.ecrire = [this, p] (int i, float force)
        {
            synthe.setModulationDepth (sourceMoteur (i), p, force);
            noterChangement();
        };
        m.effacer = [this, p] (int i)
        {
            debutGeste();
            synthe.clearModulation (sourceMoteur (i), p);
            noterChangement();
        };
        m.couleur = [] (int i) { return couleurSource (i); };
        m.nom = [] (int i) { return nomSource (i); };
        m.positionVivante = [this, p]
        {
            const auto positions = synthe.getLiveModulationPositions (p);
            return positions.isEmpty() ? -1.0f : positions.getFirst();
        };
        pot.definirModulation (std::move (m));
        return pot;
    }

    void noterChangement()
    {
        static_cast<te::Plugin&> (synthe).flushPluginStateToValueTree();
        synthe.edit.markAsChanged();
    }

    void texte (int x, int y, const juce::String& t, int w = 150)
    {
        ajouter<TexteSection> (juce::Rectangle<int> (x, y, w, 14), t);
    }

    //==========================================================================
    void remplirPresets()
    {
        presets.clear (juce::dontSendNotification);
        idsPresets.clear();

        const auto actuel = Instruments::nomPresetDe (synthe);
        int id = 1, choisi = 0;

        for (auto& e : Instruments::catalogue())
        {
            if (e.id == Instruments::idDe (Instruments::Type::batterie))
                continue;

            presets.addItem (e.utilisateur ? e.nom + fr ("  (perso)") : e.nom, id);
            idsPresets.add (e.id);

            if (e.nom == actuel)
                choisi = id;
            ++id;
        }

        if (choisi > 0)
            presets.setSelectedId (choisi, juce::dontSendNotification);
    }

    void demanderNomPreset()
    {
        auto* fenetre = new juce::AlertWindow (fr ("Enregistrer le préréglage"),
                                               fr ("Quel nom veux-tu donner à ce son ?"),
                                               juce::MessageBoxIconType::NoIcon, this);
        fenetre->addTextEditor ("nom", fr ("Mon son"));
        fenetre->addButton ("Enregistrer", 1, juce::KeyPress (juce::KeyPress::returnKey));
        fenetre->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PeripheriqueSynthe> moi (this);

        fenetre->enterModalState (true, juce::ModalCallbackFunction::create ([moi, fenetre] (int resultat)
        {
            const auto nom = fenetre->getTextEditorContents ("nom").trim();

            if (moi != nullptr && resultat == 1 && nom.isNotEmpty())
            {
                Instruments::enregistrerPreset (moi->synthe, nom);
                moi->remplirPresets();
            }
        }), true);
    }

    //==========================================================================
    void construireGauche (std::function<void (float*, int)> lectureOscillo)
    {
        ajouter<EcranOscillo> (juce::Rectangle<int> (6, 4, 144, 96), std::move (lectureOscillo), couleur);

        int x = 8;
        potardMod (x, 106, synthe.masterLevel, "Level", fr ("Le volume général du synthétiseur."));
        potardMod (x += lp, 106, synthe.ampVelocity, "Vel", fr ("De combien la force de la note change son volume."));
        potard (x += lp, 106, synthe.legato, "Glide", fr ("En mode Mono ou Legato : le temps pour glisser d'une note à la suivante."));

        ajouter<ChoixListe> (juce::Rectangle<int> (8, 166, 140, 22),
                             juce::StringArray { "Mono", "Legato", "Poly" },
                             [this] { return synthe.voiceModeValue.get(); },
                             [this] (int i) { debutGeste(); synthe.voiceModeValue = i; },
                             "Voice Mode",
                             fr ("Poly : plusieurs notes à la fois (accords). Mono : une seule note à la fois (basses, solos). "
                                 "Legato : comme Mono, mais les notes liées ne redémarrent pas l'enveloppe."));

        auto& chemin = ajouter<juce::Label> (juce::Rectangle<int> (6, 196, 146, 34));
        chemin.setText (fr ("Trajet du son :\nOSC  >  FILTER  >  FX"), juce::dontSendNotification);
        chemin.setFont (juce::FontOptions (11.0f));
        chemin.setColour (juce::Label::textColourId, Couleurs::texteDoux);
        chemin.setJustificationType (juce::Justification::centred);
        chemin.setInterceptsMouseClicks (false, false);
    }

    void construireOnglets()
    {
        const char* const noms[] = { "OSC", "FILTER / MOD", "FX" };
        const char* const aides[] = {
            "Les 4 oscillateurs : ce sont eux qui fabriquent le son de départ.",
            "Le filtre, qui sculpte le son, puis les enveloppes et les LFO, qui font bouger les réglages dans le temps.",
            "Les effets du synthé, appliqués tout à la fin, après le filtre : distorsion, chorus, écho, réverbération."
        };

        for (int i = 0; i < 3; ++i)
        {
            auto& b = ajouter<OngletSynthe> (juce::Rectangle<int> (160 + i * 112, 2, 110, 20), noms[i]);
            b.setWantsKeyboardFocus (false);
            b.setClickingTogglesState (false);
            b.setColour (juce::TextButton::buttonOnColourId, Couleurs::accent);
            b.onClick = [this, i]
            {
                page = i;
                synthe.state.setProperty ("quackPage", page, nullptr);
                appliquerVisibilite();
            };
            definirAide (b, noms[i], fr (aides[i]));
            onglets.add (&b);
        }
    }

    //==========================================================================
    void construireOscillateurs (int x0, int y0)
    {
        for (int i = 0; i < 4; ++i)
        {
            auto* o = synthe.oscParams[i];
            const int y = y0 + i * 61;
            const juce::String n (i + 1);

            ajouter<EcranOnde> (juce::Rectangle<int> (x0, y + 2, 72, 50),
                                [o] { return o->waveShapeValue.get(); },
                                [this, o] (int f) { debutGeste(); o->waveShapeValue = f; },
                                couleur);

            int x = x0 + 78;
            potardMod (x, y, o->tune, "Tune", fr ("La hauteur de l'oscillateur ") + n + fr (", en demi-tons. +12 = une octave plus haut, -12 = une octave plus bas."));
            potardMod (x += lp, y, o->fineTune, "Fine", fr ("Accordage très fin. Un tout petit décalage entre deux oscillateurs donne un son plus large. Avec un LFO dessus : vibrato."));
            potardMod (x += lp, y, o->level, "Level", fr ("Le volume de l'oscillateur ") + n + ".");
            potardMod (x += lp, y, o->pulseWidth, "Width", fr ("Pour l'onde Square seulement : la largeur du carré. Au milieu : son creux. Sur les côtés : son plus nasillard."));

            Potard::Lien voix;
            voix.plage = juce::NormalisableRange<double> (1.0, 8.0, 1.0);
            voix.lire = [o] { return (float) o->voicesValue.get(); };
            voix.ecrire = [o] (float v) { o->voicesValue = juce::roundToInt (v); };
            voix.texte = [] (float v) { return juce::String (juce::roundToInt (v)) + " voix"; };
            voix.debutGeste = [this] { debutGeste(); };
            voix.defaut = 1.0;
            voix.aUnDefaut = true;
            potardLibre (x += lp, y, "Voices", voix, fr ("Nombre de copies de l'oscillateur jouées ensemble (unisson). Plusieurs voix + Detune = un son épais."));

            potardMod (x += lp, y, o->detune, "Detune", fr ("Désaccorde légèrement les voix entre elles. Ne s'entend qu'avec 2 voix ou plus."));
            potardMod (x += lp, y, o->spread, "Spread", fr ("Répartit les voix entre la gauche et la droite."));
            potardMod (x += lp, y, o->pan, "Pan", fr ("Place cet oscillateur plus à gauche ou plus à droite."));
        }
    }

    //==========================================================================
    static float magnitude (const juce::IIRCoefficients& c, double hz, double fe)
    {
        const double w = juce::MathConstants<double>::twoPi * hz / fe;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        const auto num = (double) c.coefficients[0] + (double) c.coefficients[1] * z1 + (double) c.coefficients[2] * z2;
        const auto den = 1.0 + (double) c.coefficients[3] * z1 + (double) c.coefficients[4] * z2;
        return (float) std::abs (num / den);
    }

    static float noteVersHz (float note)        { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
    static float hzVersNote (float hz)          { return 69.0f + 12.0f * std::log2 (juce::jmax (1.0f, hz) / 440.0f); }

    void construireFiltre (int x0, int y0)
    {
        texte (x0, y0, "FILTER");

        auto& ecran = ajouter<EcranCourbe> (juce::Rectangle<int> (x0, y0 + 16, 152, 84), -36.0f, 24.0f, couleur);
        ecran.tracerZero = false;
        ecran.debutGeste = [this] { debutGeste(); };
        ecran.courbe = [this] (float hz)
        {
            const int type = synthe.filterTypeValue.get();
            if (type == 0)
                return 0.0f;

            const double fe = 44100.0;
            const double coupure = juce::jlimit (20.0, 20000.0, (double) noteVersHz (synthe.filterFreq->getCurrentValue()));
            const double q = 0.70710678 / (1.0 - (synthe.filterResonance->getCurrentValue() / 100.0) * 0.99);
            juce::IIRCoefficients c;

            switch (type)
            {
                case 1:  c = juce::IIRCoefficients::makeLowPass (fe, coupure, q);       break;
                case 2:  c = juce::IIRCoefficients::makeHighPass (fe, coupure, q);      break;
                case 3:  c = juce::IIRCoefficients::makeBandPass (fe, coupure, q);      break;
                default: c = juce::IIRCoefficients::makeNotchFilter (fe, coupure, q);   break;
            }

            float m = magnitude (c, hz, fe);
            if (synthe.filterSlopeValue.get() == 24)
                m *= m;

            return juce::Decibels::gainToDecibels (m, -60.0f);
        };

        EcranCourbe::Point pt;
        pt.couleur = Couleurs::accent;
        pt.lireFrequence = [this] { return noteVersHz (synthe.filterFreq->getCurrentValue()); };
        pt.lireHauteur = [this] { return -36.0f + synthe.filterResonance->getCurrentValue() / 100.0f * 60.0f; };
        pt.ecrire = [this] (float hz, float h)
        {
            synthe.filterFreq->setParameter (hzVersNote (hz), juce::sendNotification);
            synthe.filterResonance->setParameter ((h + 36.0f) / 60.0f * 100.0f, juce::sendNotification);
        };
        ecran.points.push_back (pt);
        definirAide (ecran, "Filter",
                     fr ("Le filtre enlève une partie du son, juste après les oscillateurs et avant les effets. La courbe montre "
                         "ce qui passe : à gauche les graves, à droite les aigus. Tire le point : gauche/droite pour la "
                         "fréquence de coupure, haut/bas pour la résonance."));

        ajouter<ChoixListe> (juce::Rectangle<int> (x0, y0 + 104, 90, 22),
                             juce::StringArray { "Off", "Lowpass", "Highpass", "Bandpass", "Notch" },
                             [this] { return synthe.filterTypeValue.get(); },
                             [this] (int i) { debutGeste(); synthe.filterTypeValue = i; },
                             "Filter Type",
                             fr ("Lowpass : garde les graves, coupe les aigus (le plus utilisé). Highpass : l'inverse. "
                                 "Bandpass : ne garde qu'une bande au milieu. Notch : creuse un trou. Off : pas de filtre."));

        ajouter<ChoixListe> (juce::Rectangle<int> (x0 + 94, y0 + 104, 58, 22),
                             juce::StringArray { "12 dB", "24 dB" },
                             [this] { return synthe.filterSlopeValue.get() == 24 ? 1 : 0; },
                             [this] (int i) { debutGeste(); synthe.filterSlopeValue = (i == 1 ? 24 : 12); },
                             "Filter Slope",
                             fr ("La pente du filtre : 24 dB coupe plus franchement que 12 dB."));

        int x = x0 + 10;
        const int y = y0 + 132;
        potardMod (x, y, synthe.filterFreq, "Freq", fr ("La fréquence de coupure : là où le filtre commence à couper. Glisse ENV 2 dessus pour que le filtre s'ouvre à chaque note, ou un LFO pour un effet wah-wah."));
        potardMod (x += lp, y, synthe.filterResonance, "Res", fr ("La résonance : fait ressortir (« siffler ») le son juste à la fréquence de coupure."));
        potard (x += lp, y, synthe.filterKey, "Key", fr ("Fait suivre le filtre avec la hauteur de la note : les notes aiguës sont alors plus brillantes."));
    }

    //==========================================================================
    void construireEnveloppes (int x0, int y0)
    {
        const char* const noms[] = { "ENV 1", "ENV 2", "ENV 3" };

        for (int i = 0; i < 3; ++i)
        {
            auto& b = ajouter<juce::TextButton> (juce::Rectangle<int> (x0 + i * 60, y0, 58, 18), noms[i]);
            b.setWantsKeyboardFocus (false);
            b.setColour (juce::TextButton::buttonOnColourId, i == 0 ? Couleurs::accent : couleurSource (i + 1));
            b.onClick = [this, i] { envChoisie = i; appliquerVisibilite(); };
            definirAide (b, noms[i],
                         i == 0 ? fr ("ENV 1 est l'enveloppe du volume : elle dessine la vie de chaque note (comment elle démarre, tient et s'éteint).")
                                : fr ("Une enveloppe libre : elle ne fait rien tant que tu ne l'as pas glissée sur un potard. "
                                      "Glissée sur Freq du filtre, elle ouvre et referme le filtre à chaque note."));
            ongletsEnv.add (&b);
        }

        for (int i = 0; i < 3; ++i)
        {
            etiquetteEnv = i;

            te::AutomatableParameter::Ptr a, d, s, r;
            if (i == 0)
            {
                a = synthe.ampAttack;  d = synthe.ampDecay;  s = synthe.ampSustain;  r = synthe.ampRelease;
            }
            else
            {
                auto* m = synthe.modEnvParams[i - 1];
                a = m->modAttack;  d = m->modDecay;  s = m->modSustain;  r = m->modRelease;
            }

            const auto teinte = i == 0 ? Couleurs::accent : couleurSource (i + 1);
            ajouter<EcranEnveloppe> (juce::Rectangle<int> (x0, y0 + 22, 178, 84), a, d, s, r, teinte, noms[i]);

            int x = x0 + 1;
            const int y = y0 + 110;
            potard (x, y, a, "Attack", fr ("Le temps pour arriver au maximum. Court : ça claque. Long : ça gonfle doucement."));
            potard (x += lp, y, d, "Decay", fr ("Le temps pour redescendre du maximum jusqu'au niveau Sustain."));
            potard (x += lp, y, s, "Sustain", fr ("Le niveau tant que la note reste tenue. À zéro, ça s'éteint tout seul (piano, cloche)."));
            potard (x += lp, y, r, "Release", fr ("Le temps pour s'éteindre quand la note s'arrête."));

            if (i == 0)
            {
                auto& info = ajouter<juce::Label> (juce::Rectangle<int> (x0, y0 + 170, 178, 34));
                info.setText (fr ("ENV 1 règle toujours le volume de la note."), juce::dontSendNotification);
                info.setFont (juce::FontOptions (11.0f));
                info.setColour (juce::Label::textColourId, Couleurs::texteDoux);
                info.setJustificationType (juce::Justification::centred);
                info.setInterceptsMouseClicks (false, false);
            }
            else
            {
                ajouter<PoigneeModulation> (juce::Rectangle<int> (x0, y0 + 174, 178, 24), i + 1, noms[i], teinte);
            }
        }

        etiquetteEnv = -1;
    }

    //==========================================================================
    void construireLfos (int x0, int y0)
    {
        const char* const noms[] = { "LFO 1", "LFO 2" };

        for (int i = 0; i < 2; ++i)
        {
            auto& b = ajouter<juce::TextButton> (juce::Rectangle<int> (x0 + i * 70, y0, 68, 18), noms[i]);
            b.setWantsKeyboardFocus (false);
            b.setColour (juce::TextButton::buttonOnColourId, couleurSource (i));
            b.onClick = [this, i] { lfoChoisi = i; appliquerVisibilite(); };
            definirAide (b, noms[i],
                         fr ("Un LFO fait bouger un réglage tout seul, en boucle. Il ne fait rien tant que tu ne l'as pas "
                             "glissé sur un potard avec la poignée du bas."));
            ongletsLfo.add (&b);
        }

        for (int i = 0; i < 2; ++i)
        {
            etiquetteLfo = i;
            auto* l = synthe.lfoParams[i];
            const auto teinte = couleurSource (i);

            ajouter<EcranLfo> (juce::Rectangle<int> (x0, y0 + 22, 138, 62),
                               [l] { return l->waveShapeValue.get(); },
                               [l] { return l->rate->getCurrentValue(); },
                               teinte);

            ajouter<ChoixListe> (juce::Rectangle<int> (x0, y0 + 88, 138, 20),
                                 juce::StringArray { "Off", "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" },
                                 [l] { return l->waveShapeValue.get(); },
                                 [this, l] (int f) { debutGeste(); l->waveShapeValue = f; },
                                 "LFO Wave",
                                 fr ("La forme du mouvement du LFO. Sine : va-et-vient doux. Square : saute entre deux valeurs. "
                                     "Saw : monte (ou descend) puis recommence. Random : valeurs au hasard."));

            potardMod (x0, y0 + 112, l->rate, "Rate", fr ("La vitesse du LFO, en battements par seconde (Hz)."));
            potardMod (x0 + lp, y0 + 112, l->depth, "Depth", fr ("L'amplitude générale du LFO."));

            ajouter<Interrupteur> (juce::Rectangle<int> (x0 + lp * 2 + 4, y0 + 126, 46, 22), "Sync",
                                   [l] { return l->syncValue.get(); },
                                   [this, l] (bool etat) { debutGeste(); l->syncValue = etat; },
                                   "LFO Sync",
                                   fr ("Cale la vitesse du LFO sur le tempo du morceau au lieu du bouton Rate."));

            ajouter<PoigneeModulation> (juce::Rectangle<int> (x0, y0 + 174, 138, 24), i, noms[i], teinte);
        }

        etiquetteLfo = -1;
    }

    //==========================================================================
    void construireEffets (int x0, int y0)
    {
        auto ligne = [this, x0, y0] (int numero, const juce::String& nom, juce::CachedValue<bool>& actif, const juce::String& aide)
        {
            const int y = y0 + numero * 61;
            auto* a = &actif;
            ajouter<Interrupteur> (juce::Rectangle<int> (x0, y + 14, 80, 24), nom,
                                   [a] { return a->get(); },
                                   [this, a] (bool etat) { debutGeste(); *a = etat; },
                                   nom, aide);
            return y;
        };

        int y = ligne (0, "Distortion", synthe.distortionOnValue, fr ("Salit le son du synthé, après le filtre. Clique pour l'allumer."));
        potardMod (x0 + 90, y, synthe.distortion, "Amount", fr ("La quantité de distorsion."));

        y = ligne (1, "Chorus", synthe.chorusOnValue, fr ("Épaissit le son, comme si plusieurs instruments jouaient ensemble."));
        int x = x0 + 90;
        potardMod (x, y, synthe.chorusMix, "Mix", fr ("Le dosage entre le son normal et le son avec chorus."));
        potardMod (x += lp, y, synthe.chorusSpeed, "Speed", fr ("La vitesse du mouvement du chorus."));
        potardMod (x += lp, y, synthe.chorusDepth, "Depth", fr ("La profondeur du mouvement du chorus."));
        potardMod (x += lp, y, synthe.chorusWidth, "Width", fr ("La largeur stéréo du chorus."));

        y = ligne (2, "Delay", synthe.delayOnValue, fr ("Un écho : le son se répète, de moins en moins fort."));
        x = x0 + 90;
        potardMod (x, y, synthe.delayMix, "Mix", fr ("Le dosage entre le son normal et l'écho."));
        potardMod (x += lp, y, synthe.delayFeedback, "Feedb", fr ("Feedback : combien de fois l'écho se répète."));

        Potard::Lien temps;
        temps.plage = juce::NormalisableRange<double> (0.25, 4.0, 0.25);
        temps.lire = [this] { return synthe.delayValue.get(); };
        temps.ecrire = [this] (float v) { synthe.delayValue = v; };
        temps.texte = [] (float v) { return juce::String (v, 2) + " temps"; };
        temps.debutGeste = [this] { debutGeste(); };
        temps.defaut = 1.0;
        temps.aUnDefaut = true;
        potardLibre (x += lp, y, "Time", temps, fr ("L'écart entre deux répétitions, en temps (calé sur le tempo). 0,5 = une croche."));
        potardMod (x += lp, y, synthe.delayCrossfeed, "Cross", fr ("Fait rebondir l'écho entre la gauche et la droite."));

        y = ligne (3, "Reverb", synthe.reverbOnValue, fr ("Donne l'impression de jouer dans une grande salle."));
        x = x0 + 90;
        potardMod (x, y, synthe.reverbMix, "Mix", fr ("Le dosage entre le son normal et la réverbération."));
        potardMod (x += lp, y, synthe.reverbSize, "Size", fr ("La taille de la salle : petite pièce ou cathédrale."));
        potardMod (x += lp, y, synthe.reverbDamping, "Damp", fr ("Étouffe les aigus de la réverbération, comme une salle avec des rideaux."));
        potardMod (x += lp, y, synthe.reverbWidth, "Width", fr ("La largeur stéréo de la réverbération."));
    }
};

//==============================================================================
/** La batterie : 8 pads, chacun avec son volume et son placement gauche/droite. */
class PeripheriqueBatterie : public Peripherique
{
public:
    PeripheriqueBatterie (te::SamplerPlugin& s, te::AudioTrack& t, juce::Colour c)
        : Peripherique (s, "Quack Drums", 8 + 4 * 112, false, c), batterie (s), piste (&t)
    {
        for (int i = 0; i < juce::jmin (8, batterie.getNumSounds()); ++i)
        {
            const int x = 8 + (i % 4) * 112;
            const int y = 6 + (1 - i / 4) * 134;        // les sons graves en bas, comme un Drum Rack

            auto& pad = ajouter<juce::TextButton> (juce::Rectangle<int> (x, y, 104, 62), batterie.getSoundName (i));
            pad.setWantsKeyboardFocus (false);
            pad.setColour (juce::TextButton::buttonColourId, c.withMultipliedBrightness (0.55f));
            pad.setTriggeredOnMouseDown (true);
            pad.onClick = [this, i] { jouer (batterie.getKeyNote (i)); };
            definirAide (pad, batterie.getSoundName (i), fr ("Clique pour écouter ce son de batterie."));

            Potard::Lien volume;
            volume.plage = juce::NormalisableRange<double> (-36.0, 12.0, 0.1);
            volume.lire = [this, i] { return batterie.getSoundGainDb (i); };
            volume.ecrire = [this, i] (float v) { batterie.setSoundGains (i, v, batterie.getSoundPan (i)); };
            volume.texte = [] (float v) { return juce::String (v, 1) + " dB"; };
            volume.debutGeste = [this] { debutGeste(); };
            volume.aUnDefaut = true;
            potardLibre (x + 6, y + 66, "Volume", volume, fr ("Le volume de ce son de batterie."));

            Potard::Lien pan;
            pan.plage = juce::NormalisableRange<double> (-1.0, 1.0, 0.01);
            pan.lire = [this, i] { return batterie.getSoundPan (i); };
            pan.ecrire = [this, i] (float v) { batterie.setSoundGains (i, batterie.getSoundGainDb (i), v); };
            pan.texte = [] (float v)
            {
                const int p = juce::roundToInt (v * 50.0f);
                return p == 0 ? juce::String ("C") : juce::String (std::abs (p)) + (p < 0 ? "L" : "R");
            };
            pan.debutGeste = [this] { debutGeste(); };
            pan.aUnDefaut = true;
            potardLibre (x + 54, y + 66, "Pan", pan, fr ("Place ce son plus à gauche ou plus à droite."));
        }
    }

private:
    te::SamplerPlugin& batterie;
    te::AudioTrack::Ptr piste;
    te::MPESourceID source = te::createUniqueMPESourceID();

    void jouer (int note)
    {
        piste->injectLiveMidiMessage (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), source);
        piste->injectLiveMidiMessage (juce::MidiMessage::noteOff (1, note), source);
    }
};

//==============================================================================
/** Égaliseur 4 bandes. */
class PeripheriqueEQ : public Peripherique
{
public:
    PeripheriqueEQ (te::EqualiserPlugin& e, juce::Colour c)
        : Peripherique (e, "EQ", 16 + 4 * 62, true, c), eq (e)
    {
        auto& ecran = ajouter<EcranCourbe> (juce::Rectangle<int> (8, 6, 4 * 62, 96), -20.0f, 20.0f, c);
        ecran.debutGeste = [this] { debutGeste(); };
        ecran.courbe = [this] (float hz) { return eq.getDBGainAtFrequency (hz); };
        definirAide (ecran, "EQ",
                     fr ("L'égaliseur change l'équilibre entre graves (à gauche) et aigus (à droite). Au-dessus de la ligne : "
                         "plus fort. En dessous : moins fort. Tire les 4 points pour sculpter le son."));

        struct Bande { const char* nom; const char* lettre; te::AutomatableParameter::Ptr freq, gain, q; const char* aide; };
        const Bande bandes[] = {
            { "LOW",   "L", eq.loFreq,   eq.loGain,   eq.loQ,   "Les graves (basse, grosse caisse)." },
            { "MID 1", "1", eq.midFreq1, eq.midGain1, eq.midQ1, "Les bas-médiums (le corps du son)." },
            { "MID 2", "2", eq.midFreq2, eq.midGain2, eq.midQ2, "Les hauts-médiums (la présence, la voix)." },
            { "HIGH",  "H", eq.hiFreq,   eq.hiGain,   eq.hiQ,   "Les aigus (cymbales, brillance)." }
        };

        int i = 0;
        for (auto& b : bandes)
        {
            const int x = 8 + i * 62;
            etiquette (x + 8, 104, b.nom, 54);

            auto freq = b.freq;
            auto gain = b.gain;

            EcranCourbe::Point pt;
            pt.couleur = Couleurs::dePiste (i * 2 + 1);
            pt.etiquette = b.lettre;
            pt.lireFrequence = [freq] { return freq->getCurrentValue(); };
            pt.lireHauteur = [gain] { return gain->getCurrentValue(); };
            pt.ecrire = [freq, gain] (float hz, float db)
            {
                freq->setParameter (hz, juce::sendNotification);
                gain->setParameter (db, juce::sendNotification);
            };
            ecran.points.push_back (pt);

            potard (x + 9, 116, b.gain, "Gain", fr (b.aide) + fr (" Gain : monte ou baisse cette zone."));
            potard (x + 9, 168, b.freq, "Freq", fr (b.aide) + fr (" Freq : l'endroit précis où cette bande agit."));
            potard (x + 9, 220, b.q, "Q", fr ("La largeur de la bande : petit Q = zone large et douce, grand Q = zone étroite et précise."));
            ++i;
        }
    }

private:
    te::EqualiserPlugin& eq;
};

//==============================================================================
/** Compresseur avec sidechain. */
class PeripheriqueCompresseur : public Peripherique
{
public:
    PeripheriqueCompresseur (te::CompressorPlugin& p, juce::Colour c)
        : Peripherique (p, "Compressor", 296, true, c), comp (p)
    {
        auto& ecran = ajouter<EcranTransfert> (juce::Rectangle<int> (8, 6, 110, 110), c);
        ecran.transfert = [this] (float entree)
        {
            const float seuil = comp.thresholdGain.getCurrentValue();
            return entree <= seuil ? entree : seuil + (entree - seuil) * comp.ratio.getCurrentValue();
        };
        definirAide (ecran, "Compressor",
                     fr ("De gauche à droite : le niveau qui entre. De bas en haut : le niveau qui sort. Au-dessus du seuil "
                         "(Threshold), la courbe se couche : les sons forts sont rabaissés."));

        int x = 126;
        potard (x, 6, comp.thresholdGain.parameter, "Thresh", fr ("Threshold (seuil) : le niveau à partir duquel le compresseur commence à baisser le son."));
        potard (x += lp, 6, comp.ratio.parameter, "Ratio", fr ("La force de la compression. 2:1 = douce, 10:1 = très forte."));
        potard (x += lp, 6, comp.outputDb.parameter, "Output", fr ("Remonte le volume après la compression."));

        x = 126;
        potard (x, 62, comp.attackMs.parameter, "Attack", fr ("Le temps de réaction du compresseur. Court : il attrape tout de suite. Long : il laisse passer le début du son."));
        potard (x += lp, 62, comp.releaseMs.parameter, "Release", fr ("Le temps que met le compresseur à relâcher. Pour l'effet « pompe », essaie entre 100 et 300 ms."));

        etiquette (8, 124, "SIDECHAIN");

        ajouter<Interrupteur> (juce::Rectangle<int> (8, 142, 84, 24), "Sidechain",
                               [this] { return comp.useSidechainTrigger.get(); },
                               [this] (bool b)
                               {
                                   debutGeste();
                                   comp.useSidechainTrigger = b;
                                   comp.edit.restartPlayback();
                               },
                               "Sidechain",
                               fr ("Quand il est allumé, c'est une AUTRE piste qui déclenche le compresseur. Exemple classique : "
                                   "la grosse caisse fait baisser la basse ou les accords à chaque coup (effet « pompe »)."));

        sources = &ajouter<ChoixListe> (juce::Rectangle<int> (98, 142, 136, 24), nomsSources(),
                                        [this] { return juce::jmax (0, nomsSources().indexOf (comp.getSidechainSourceName())); },
                                        [this] (int i)
                                        {
                                            debutGeste();
                                            const auto noms = nomsSources();
                                            comp.setSidechainSourceByName (i > 0 && i < noms.size() ? noms[i] : juce::String());
                                            comp.useSidechainTrigger = (i > 0);
                                            comp.edit.restartPlayback();
                                        },
                                        "Sidechain Source",
                                        fr ("La piste qui déclenche le compresseur. Choisis par exemple la batterie."));

        potard (242, 130, comp.sidechainDb.parameter, "SC Gain", fr ("Rend le signal de la piste source plus ou moins fort avant qu'il ne déclenche le compresseur."));

        dernieresSources = nomsSources();
    }

    /** Les noms des pistes changent : on met la liste à jour. */
    void verifierSources()
    {
        const auto noms = nomsSources();
        if (noms != dernieresSources)
        {
            dernieresSources = noms;
            sources->definirChoix (noms);
        }
    }

private:
    te::CompressorPlugin& comp;
    ChoixListe* sources = nullptr;
    juce::StringArray dernieresSources;

    juce::StringArray nomsSources()
    {
        auto noms = comp.getSidechainSourceNames (false);
        noms.insert (0, fr ("Aucune"));
        return noms;
    }
};

//==============================================================================
/** Delay (écho). */
class PeripheriqueDelay : public Peripherique
{
public:
    PeripheriqueDelay (te::DelayPlugin& p, juce::Colour c)
        : Peripherique (p, "Delay", 16 + 3 * lp + 8, true, c), delay (p)
    {
        Potard::Lien temps;
        temps.plage = juce::NormalisableRange<double> (10.0, 2000.0, 1.0, 0.4);
        temps.lire = [this] { return (float) delay.lengthMs.get(); };
        temps.ecrire = [this] (float v) { delay.lengthMs = juce::roundToInt (v); };
        temps.texte = [] (float v) { return juce::String (juce::roundToInt (v)) + " ms"; };
        temps.debutGeste = [this] { debutGeste(); };
        potardLibre (12, 8, "Time", temps, fr ("L'écart entre deux répétitions, en millisecondes. Les boutons en dessous le calent sur le tempo."));

        potard (12 + lp, 8, delay.feedbackDb, "Feedb", fr ("Feedback : combien de fois l'écho se répète avant de s'éteindre."));
        potard (12 + lp * 2, 8, delay.mixProportion, "Dry/Wet", fr ("Le dosage : à gauche le son normal seul, à droite l'écho seul."));

        etiquette (12, 72, "SYNC");

        const std::pair<const char*, double> durees[] = { { "1/4", 1.0 }, { "1/8.", 0.75 }, { "1/8", 0.5 }, { "1/16", 0.25 } };
        int i = 0;

        for (auto& d : durees)
        {
            auto& b = ajouter<juce::TextButton> (juce::Rectangle<int> (12 + (i % 2) * 68, 90 + (i / 2) * 28, 64, 24), d.first);
            b.setWantsKeyboardFocus (false);
            const double temps_ = d.second;
            b.onClick = [this, temps_]
            {
                debutGeste();
                const double bpm = delay.edit.tempoSequence.getTempo (0)->getBpm();
                delay.lengthMs = juce::roundToInt (60000.0 / bpm * temps_);
            };
            definirAide (b, "Sync " + juce::String (d.first),
                         fr ("Cale l'écho sur le tempo du morceau. 1/4 = un temps (noire), 1/8 = une croche, "
                             "1/8. = une croche pointée (très utilisé), 1/16 = une double-croche."));
            ++i;
        }
    }

private:
    te::DelayPlugin& delay;
};

//==============================================================================
/** Reverb. */
class PeripheriqueReverb : public Peripherique
{
public:
    PeripheriqueReverb (te::ReverbPlugin& p, juce::Colour c)
        : Peripherique (p, "Reverb", 16 + 3 * lp + 8, true, c)
    {
        potard (12, 8, p.roomSizeParam, "Size", fr ("La taille de la salle : petite pièce ou cathédrale."));
        potard (12 + lp, 8, p.dampParam, "Damp", fr ("Étouffe les aigus de la réverbération, comme une salle avec des rideaux."));
        potard (12 + lp * 2, 8, p.widthParam, "Width", fr ("La largeur stéréo de la réverbération."));
        potard (12 + lp / 2, 70, p.dryParam, "Dry", fr ("Le niveau du son d'origine (sans effet)."));
        potard (12 + lp / 2 + lp, 70, p.wetParam, "Wet", fr ("Le niveau de la réverbération ajoutée."));
    }
};

//==============================================================================
/** Distorsion. */
class PeripheriqueDistorsion : public Peripherique
{
public:
    PeripheriqueDistorsion (DistorsionQuack& p, juce::Colour c)
        : Peripherique (p, "Distortion", 16 + 110 + 8 + 2 * lp, true, c), disto (p)
    {
        auto& ecran = ajouter<EcranTransfert> (juce::Rectangle<int> (8, 6, 110, 110), c);
        ecran.bipolaire = true;
        ecran.transfert = [this] (float entree)
        {
            return std::tanh (entree * juce::Decibels::decibelsToGain (disto.drive->getCurrentValue()));
        };
        definirAide (ecran, "Distortion",
                     fr ("La courbe montre comment le son est écrasé. Une ligne droite en diagonale = son intact. "
                         "Plus la courbe s'aplatit en haut et en bas, plus le son est saturé."));

        potard (126, 6, disto.drive, "Drive", fr ("Pousse le son dans la saturation : plus on monte, plus ça salit."));
        potard (126 + lp, 6, disto.tone, "Tone", fr ("Adoucit les aigus de la distorsion. À droite : son brillant et agressif."));
        potard (126, 62, disto.mix, "Dry/Wet", fr ("Le dosage entre le son propre et le son saturé."));
        potard (126 + lp, 62, disto.output, "Output", fr ("Le volume de sortie. La distorsion augmente le volume : baisse-le ici."));
    }

private:
    DistorsionQuack& disto;
};

//==============================================================================
/** Volume. */
class PeripheriqueVolume : public Peripherique
{
public:
    PeripheriqueVolume (VolumeQuack& p, juce::Colour c)
        : Peripherique (p, "Volume", 16 + 2 * lp + 30, true, c)
    {
        potard (22, 8, p.gain, "Gain", fr ("Monte ou baisse le volume à cet endroit de la chaîne."));
        potard (22 + lp, 8, p.pan, "Pan", fr ("Place le son plus à gauche ou plus à droite."));
    }
};

//==============================================================================
/** La chaîne de périphériques de la piste choisie : instrument, puis effets, de gauche à droite.
    On peut y glisser un effet ou un instrument depuis le navigateur, à l'endroit précis voulu,
    et déplacer un effet en glissant sa barre de titre. */
class ChainePeripheriques : public juce::Component,
                            public juce::DragAndDropTarget,
                            private juce::Timer
{
public:
    ChainePeripheriques (te::Edit& e, Selection& sel) : edit (e), selection (sel)
    {
        addAndMakeVisible (vue);
        vue.setViewedComponent (&contenu, false);
        vue.setScrollBarsShown (true, true);
        vue.setScrollBarThickness (10);

        contenu.addAndMakeVisible (ajouterEffet);
        ajouterEffet.setButtonText ("+ Effet");
        ajouterEffet.setWantsKeyboardFocus (false);
        ajouterEffet.onClick = [this] { menuEffets(); };
        definirAide (ajouterEffet, fr ("Ajouter un effet"),
                     fr ("Ajoute un effet à la fin de la chaîne. Le son traverse les périphériques de gauche à droite."));

        contenu.addChildComponent (marqueur);
        marqueur.setInterceptsMouseClicks (false, false);

        definirAide (*this, fr ("Instrument et effets"),
                     fr ("L'instrument de la piste choisie, suivi de ses effets. Le son va de gauche à droite. "
                         "Glisse ici un instrument ou un effet depuis le navigateur : un trait blanc montre où il se posera."));

        startTimerHz (12);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);

        if (peripheriques.isEmpty())
        {
            g.setColour (Couleurs::texteDoux);
            g.setFont (juce::FontOptions (14.5f));
            g.drawFittedText (fr ("Clique sur une piste pour voir son instrument et ses effets."),
                              getLocalBounds().reduced (20), juce::Justification::centred, 2);
        }
    }

    void resized() override     { vue.setBounds (getLocalBounds()); }

    //==========================================================================
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        const auto id = d.description.toString();
        return id.startsWith ("fx:") || id.startsWith ("inst:") || id.startsWith ("preset:") || id == "peripherique";
    }

    void itemDragMove (const SourceDetails& d) override
    {
        const auto id = d.description.toString();

        if (id.startsWith ("fx:") || id == "peripherique")
        {
            const int position = positionPour (d.localPosition.x);
            marqueur.setBounds (xDePosition (position) - 3, 4, 4, Peripherique::hauteurTotale);
            marqueur.setVisible (true);
            marqueur.toFront (false);
        }
    }

    void itemDragExit (const SourceDetails&) override    { marqueur.setVisible (false); }

    void itemDropped (const SourceDetails& d) override
    {
        marqueur.setVisible (false);

        auto* piste = selection.pisteValide();
        if (piste == nullptr)
            return;

        const auto id = d.description.toString();
        const int position = positionPour (d.localPosition.x);
        edit.getUndoManager().beginNewTransaction();

        if (id == "peripherique")
        {
            if (auto* source = dynamic_cast<Peripherique*> (d.sourceComponent.get()))
                Effets::deplacer (*piste, source->getPlugin(), position);
        }
        else if (id.startsWith ("fx:"))
        {
            Effets::ajouter (*piste, id, position);
        }
        else
        {
            Instruments::appliquer (*piste, id);
        }
    }

    /** Applique un élément du navigateur (instrument ou effet) à une piste. */
    static void appliquerElement (te::AudioTrack& piste, const juce::String& id)
    {
        piste.edit.getUndoManager().beginNewTransaction();

        if (id.startsWith ("fx:"))
            Effets::ajouter (piste, id);
        else
            Instruments::appliquer (piste, id);
    }

private:
    struct Marqueur : public juce::Component
    {
        void paint (juce::Graphics& g) override     { g.fillAll (juce::Colours::white); }
    };

    te::Edit& edit;
    Selection& selection;

    juce::Viewport vue;
    juce::Component contenu;
    juce::OwnedArray<Peripherique> peripheriques;
    juce::TextButton ajouterEffet;
    Marqueur marqueur;

    te::AudioTrack::Ptr pisteAffichee;
    juce::Array<te::Plugin*> chaineAffichee;

    //==========================================================================
    /** À quelle position de la chaîne correspond ce point (x mesuré dans la zone visible) ? */
    int positionPour (int xVisible) const
    {
        const int x = xVisible + vue.getViewPositionX();

        for (int i = 0; i < peripheriques.size(); ++i)
            if (x < peripheriques[i]->getBounds().getCentreX())
                return juce::jmax (premierePositionEffet(), i);

        return peripheriques.size();
    }

    int premierePositionEffet() const
    {
        for (int i = 0; i < peripheriques.size(); ++i)
            if (peripheriques[i]->estEffet())
                return i;

        return peripheriques.size();
    }

    int xDePosition (int position) const
    {
        if (position < peripheriques.size())
            return peripheriques[position]->getX() - 2;

        return peripheriques.isEmpty() ? 6 : peripheriques.getLast()->getRight() + 5;
    }

    void timerCallback() override
    {
        if (edit.isRendering())     // pendant un export, on ne touche pas aux instruments
            return;

        auto* piste = selection.pisteValide();
        const auto chaine = piste != nullptr ? Effets::chaineDe (*piste) : juce::Array<te::Plugin*>();

        if (piste != pisteAffichee.get() || chaine != chaineAffichee)
            reconstruire (piste, chaine);

        for (auto* p : peripheriques)
        {
            p->rafraichirTout();

            if (auto* comp = dynamic_cast<PeripheriqueCompresseur*> (p))
                comp->verifierSources();
        }
    }

    void reconstruire (te::AudioTrack* piste, const juce::Array<te::Plugin*>& chaine)
    {
        const bool memePiste = (piste == pisteAffichee.get());
        const auto vuePrecedente = vue.getViewPosition();

        peripheriques.clear();
        pisteAffichee = piste;
        chaineAffichee = chaine;

        if (piste != nullptr)
        {
            const int numPiste = te::getAudioTracks (edit).indexOf (piste);
            const auto couleur = couleurPiste (*piste, numPiste);

            for (auto* plugin : chaine)
            {
                Peripherique* p = nullptr;

                if (auto* synthe = dynamic_cast<te::FourOscPlugin*> (plugin))
                {
                    // L'oscilloscope est un petit module placé juste après le synthé
                    te::Plugin::Ptr oscillo (Effets::oscilloDe (*piste, true));

                    auto* s = new PeripheriqueSynthe (*synthe,
                                                      [oscillo] (float* destination, int nombre)
                                                      {
                                                          if (auto* o = dynamic_cast<OscilloQuack*> (oscillo.get()))
                                                              o->lire (destination, nombre);
                                                          else
                                                              juce::FloatVectorOperations::clear (destination, nombre);
                                                      },
                                                      couleur);
                    s->surPreset = [this] (juce::String id) { differer ([id] (te::AudioTrack& t) { Instruments::appliquer (t, id); }); };
                    p = s;
                }
                else if (auto* batterie = dynamic_cast<te::SamplerPlugin*> (plugin))    p = new PeripheriqueBatterie (*batterie, *piste, couleur);
                else if (auto* eq = dynamic_cast<te::EqualiserPlugin*> (plugin))        p = new PeripheriqueEQ (*eq, couleur);
                else if (auto* comp = dynamic_cast<te::CompressorPlugin*> (plugin))     p = new PeripheriqueCompresseur (*comp, couleur);
                else if (auto* delay = dynamic_cast<te::DelayPlugin*> (plugin))         p = new PeripheriqueDelay (*delay, couleur);
                else if (auto* reverb = dynamic_cast<te::ReverbPlugin*> (plugin))       p = new PeripheriqueReverb (*reverb, couleur);
                else if (auto* disto = dynamic_cast<DistorsionQuack*> (plugin))         p = new PeripheriqueDistorsion (*disto, couleur);
                else if (auto* volume = dynamic_cast<VolumeQuack*> (plugin))            p = new PeripheriqueVolume (*volume, couleur);

                if (p == nullptr)
                    continue;

                te::Plugin::Ptr garde (plugin);
                p->surSupprimer = [this, garde]
                {
                    differer ([garde] (te::AudioTrack&) { garde->deleteFromParent(); });
                };
                p->surMiseEnPage = [this] { placer(); };

                peripheriques.add (p);
                contenu.addAndMakeVisible (p);
            }

            // L'ajout de l'oscilloscope a pu modifier la liste : on la relit pour ne pas reconstruire en boucle
            chaineAffichee = Effets::chaineDe (*piste);
        }

        ajouterEffet.setVisible (piste != nullptr);
        placer();
        vue.setViewPosition (memePiste ? vuePrecedente : juce::Point<int>());
        repaint();
    }

    /** Range les périphériques de gauche à droite. */
    void placer()
    {
        int x = 6;
        for (auto* p : peripheriques)
        {
            p->setBounds (x, 4, p->getLargeur(), Peripherique::hauteurTotale);
            x += p->getLargeur() + 8;
        }

        ajouterEffet.setBounds (x, 4, 84, 28);
        contenu.setSize (x + 96, Peripherique::hauteurTotale + 8);
    }

    /** Lance une action sur la piste un peu plus tard (pour ne pas détruire un bouton pendant son clic). */
    void differer (std::function<void (te::AudioTrack&)> action)
    {
        juce::Component::SafePointer<ChainePeripheriques> moi (this);

        juce::MessageManager::callAsync ([moi, action]
        {
            if (moi == nullptr)
                return;

            if (auto* piste = moi->selection.pisteValide())
            {
                moi->edit.getUndoManager().beginNewTransaction();
                action (*piste);
            }
        });
    }

    void menuEffets()
    {
        juce::PopupMenu m;
        int id = 1;
        for (auto& e : Effets::tous())
            m.addItem (id++, e.nom);

        juce::Component::SafePointer<ChainePeripheriques> moi (this);

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&ajouterEffet),
                         [moi] (int choix)
                         {
                             if (moi == nullptr || choix <= 0)
                                 return;

                             if (auto* piste = moi->selection.pisteValide())
                                 appliquerElement (*piste, Effets::tous()[(size_t) (choix - 1)].id);
                         });
    }
};
