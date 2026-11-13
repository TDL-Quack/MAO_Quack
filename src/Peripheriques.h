#pragma once

#include "Commun.h"
#include "Widgets.h"
#include "Instruments.h"
#include "Effets.h"

//==============================================================================
/** Un périphérique (instrument ou effet) affiché dans la chaîne du bas, comme dans Ableton. */
class Peripherique : public juce::Component
{
public:
    static constexpr int hauteurEntete = 24;
    static constexpr int hauteurTotale = 298;

    Peripherique (te::Plugin& p, const juce::String& titreAffiche, int largeurVoulue, bool supprimable, juce::Colour c)
        : plugin (&p), titre (titreAffiche), largeur (largeurVoulue), couleur (c)
    {
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

        if (supprimable)
        {
            addAndMakeVisible (supprimer);
            supprimer.setButtonText ("X");
            supprimer.setWantsKeyboardFocus (false);
            supprimer.onClick = [this] { if (surSupprimer) surSupprimer(); };
            definirAide (supprimer, fr ("Supprimer l'effet"), fr ("Retire cet effet de la piste."));
        }
    }

    te::Plugin& getPlugin()         { return *plugin; }
    int getLargeur() const          { return largeur; }

    std::function<void()> surSupprimer;

    void rafraichirTout()
    {
        marche.setToggleState (plugin->isEnabled(), juce::dontSendNotification);

        for (auto* r : aRafraichir)
            r->rafraichir();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneauClair.darker (0.25f));

        auto entete = getLocalBounds().removeFromTop (hauteurEntete);
        g.setColour (couleur.withMultipliedSaturation (0.75f).darker (0.15f));
        g.fillRect (entete);

        g.setColour (juce::Colours::black);
        g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
        g.drawText (titre, entete.withTrimmedLeft (44).withTrimmedRight (30), juce::Justification::centredLeft, true);

        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        for (auto& e : etiquettes)
            g.drawText (e.second, e.first, juce::Justification::centredLeft, false);

        g.setColour (Couleurs::bordure);
        for (int x : separateurs)
            g.drawVerticalLine (x, (float) hauteurEntete + 4.0f, (float) getHeight() - 4.0f);

        g.drawRect (getLocalBounds());
    }

    void resized() override
    {
        marche.setBounds (4, 3, 36, hauteurEntete - 6);
        supprimer.setBounds (getWidth() - 26, 3, 22, hauteurEntete - 6);
    }

protected:
    te::Plugin::Ptr plugin;
    juce::String titre;
    int largeur;
    juce::Colour couleur;

    /** Crée un composant, le garde, l'affiche et le place (y compté sous l'en-tête). */
    template <typename Type, typename... Args>
    Type& ajouter (juce::Rectangle<int> place, Args&&... args)
    {
        auto* c = new Type (std::forward<Args> (args)...);
        possede.add (c);
        addAndMakeVisible (c);
        c->setBounds (place.translated (0, hauteurEntete));

        if (auto* r = dynamic_cast<Rafraichissable*> (c))
            aRafraichir.push_back (r);

        return *c;
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

private:
    juce::TextButton marche, supprimer;
    juce::OwnedArray<juce::Component> possede;
    std::vector<Rafraichissable*> aRafraichir;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> etiquettes;
    std::vector<int> separateurs;
};

//==============================================================================
/** Le synthétiseur : 4 oscillateurs, filtre, deux enveloppes, 2 LFO et des effets. */
class PeripheriqueSynthe : public Peripherique
{
public:
    PeripheriqueSynthe (te::FourOscPlugin& s, juce::Colour c)
        : Peripherique (s, "Quack Synth", 1390, false, c), synthe (s)
    {
        construireOscillateurs (8);
        separateur (402);
        construireFiltre (410);
        separateur (702);
        construireAmpli (710);
        separateur (902);
        construireLfos (910);
        separateur (1118);
        construireEffets (1126);

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
    }

    /** Appelé quand l'utilisateur choisit un préréglage (identifiant du catalogue). */
    std::function<void (juce::String)> surPreset;

    void resized() override
    {
        Peripherique::resized();
        presets.setBounds (150, 3, 190, hauteurEntete - 6);
        enregistrer.setBounds (346, 3, 50, hauteurEntete - 6);
    }

private:
    te::FourOscPlugin& synthe;
    juce::ComboBox presets;
    juce::TextButton enregistrer;
    juce::StringArray idsPresets;

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
    void construireOscillateurs (int x0)
    {
        etiquette (x0, 2, "OSCILLATORS");

        for (int i = 0; i < 4; ++i)
        {
            auto* o = synthe.oscParams[i];
            const int y = 18 + i * 64;
            const juce::String n (i + 1);

            ajouter<EcranOnde> (juce::Rectangle<int> (x0, y + 2, 72, 50),
                                [o] { return o->waveShapeValue.get(); },
                                [this, o] (int f) { debutGeste(); o->waveShapeValue = f; },
                                couleur);

            int x = x0 + 78;
            potard (x, y, o->tune, "Tune", fr ("La hauteur de l'oscillateur ") + n + fr (", en demi-tons. +12 = une octave plus haut, -12 = une octave plus bas."));
            potard (x += lp, y, o->fineTune, "Fine", fr ("Accordage très fin. Un tout petit décalage entre deux oscillateurs donne un son plus large."));
            potard (x += lp, y, o->level, "Level", fr ("Le volume de l'oscillateur ") + n + ".");
            potard (x += lp, y, o->pulseWidth, "Width", fr ("Pour l'onde Square seulement : la largeur du carré. Au milieu : son creux. Sur les côtés : son plus nasillard."));

            Potard::Lien voix;
            voix.plage = juce::NormalisableRange<double> (1.0, 8.0, 1.0);
            voix.lire = [o] { return (float) o->voicesValue.get(); };
            voix.ecrire = [o] (float v) { o->voicesValue = juce::roundToInt (v); };
            voix.texte = [] (float v) { return juce::String (juce::roundToInt (v)) + " voix"; };
            voix.debutGeste = [this] { debutGeste(); };
            voix.defaut = 1.0;
            voix.aUnDefaut = true;
            potardLibre (x += lp, y, "Voices", voix, fr ("Nombre de copies de l'oscillateur jouées ensemble (unisson). Plusieurs voix + Detune = un son épais."));

            potard (x += lp, y, o->detune, "Detune", fr ("Désaccorde légèrement les voix entre elles. Ne s'entend qu'avec 2 voix ou plus."));
            potard (x += lp, y, o->spread, "Spread", fr ("Répartit les voix entre la gauche et la droite."));
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

    void construireFiltre (int x0)
    {
        etiquette (x0, 2, "FILTER");

        auto& ecran = ajouter<EcranCourbe> (juce::Rectangle<int> (x0, 18, 284, 74), -36.0f, 24.0f, couleur);
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
                     fr ("Le filtre enlève une partie du son. La courbe montre ce qui passe : à gauche les graves, à droite "
                         "les aigus. Tire le point : gauche/droite pour la fréquence de coupure, haut/bas pour la résonance."));

        ajouter<ChoixListe> (juce::Rectangle<int> (x0, 96, 136, 22),
                             juce::StringArray { "Off", "Lowpass", "Highpass", "Bandpass", "Notch" },
                             [this] { return synthe.filterTypeValue.get(); },
                             [this] (int i) { debutGeste(); synthe.filterTypeValue = i; },
                             "Filter Type",
                             fr ("Lowpass : garde les graves, coupe les aigus (le plus utilisé). Highpass : l'inverse. "
                                 "Bandpass : ne garde qu'une bande au milieu. Notch : creuse un trou. Off : pas de filtre."));

        ajouter<ChoixListe> (juce::Rectangle<int> (x0 + 142, 96, 90, 22),
                             juce::StringArray { "12 dB", "24 dB" },
                             [this] { return synthe.filterSlopeValue.get() == 24 ? 1 : 0; },
                             [this] (int i) { debutGeste(); synthe.filterSlopeValue = (i == 1 ? 24 : 12); },
                             "Filter Slope",
                             fr ("La pente du filtre : 24 dB coupe plus franchement que 12 dB."));

        int x = x0 + 4;
        const int y = 122;
        potard (x, y, synthe.filterFreq, "Freq", fr ("La fréquence de coupure : là où le filtre commence à couper."));
        potard (x += lp, y, synthe.filterResonance, "Res", fr ("La résonance : fait ressortir (« siffler ») le son juste à la fréquence de coupure."));
        potard (x += lp, y, synthe.filterAmount, "Env", fr ("De combien l'enveloppe du filtre (juste en dessous) ouvre ou ferme le filtre à chaque note."));
        potard (x += lp, y, synthe.filterKey, "Key", fr ("Fait suivre le filtre avec la hauteur de la note : les notes aiguës sont alors plus brillantes."));
        potard (x += lp, y, synthe.filterVelocity, "Vel", fr ("Les notes jouées fort ouvrent davantage le filtre."));

        etiquette (x0, 176, "FILTER ENVELOPE");
        ajouter<EcranEnveloppe> (juce::Rectangle<int> (x0, 192, 104, 76),
                                 synthe.filterAttack, synthe.filterDecay, synthe.filterSustain, synthe.filterRelease,
                                 couleur, "Filter Envelope");

        x = x0 + 108;
        potard (x, 202, synthe.filterAttack, "Attack", fr ("Le temps que met le filtre à s'ouvrir au début de la note."));
        potard (x += lp, 202, synthe.filterDecay, "Decay", fr ("Le temps que met le filtre à se refermer jusqu'au niveau Sustain."));
        potard (x += lp, 202, synthe.filterSustain, "Sustain", fr ("L'ouverture du filtre tant que la note reste tenue."));
        potard (x += lp, 202, synthe.filterRelease, "Release", fr ("Le temps que met le filtre à se refermer quand la note s'arrête."));
    }

    //==========================================================================
    void construireAmpli (int x0)
    {
        etiquette (x0, 2, "AMP ENVELOPE");

        ajouter<EcranEnveloppe> (juce::Rectangle<int> (x0, 18, 184, 100),
                                 synthe.ampAttack, synthe.ampDecay, synthe.ampSustain, synthe.ampRelease,
                                 Couleurs::accent, "Amp Envelope");

        int x = x0 + 4;
        potard (x, 122, synthe.ampAttack, "Attack", fr ("Le temps que met le son pour arriver à son maximum. Court : son qui claque. Long : son qui gonfle doucement."));
        potard (x += lp, 122, synthe.ampDecay, "Decay", fr ("Le temps pour redescendre du maximum jusqu'au niveau Sustain."));
        potard (x += lp, 122, synthe.ampSustain, "Sustain", fr ("Le niveau du son tant que la note reste tenue. À zéro, le son s'éteint tout seul (piano, cloche)."));
        potard (x += lp, 122, synthe.ampRelease, "Release", fr ("Le temps que met le son à s'éteindre quand la note s'arrête."));

        etiquette (x0, 180, "GLOBAL");
        x = x0 + 4;
        potard (x, 196, synthe.ampVelocity, "Vel", fr ("De combien la force de la note change son volume."));
        potard (x += lp, 196, synthe.masterLevel, "Level", fr ("Le volume général du synthétiseur."));
        potard (x += lp, 196, synthe.legato, "Glide", fr ("En mode Mono ou Legato : le temps pour glisser d'une note à la suivante."));

        ajouter<ChoixListe> (juce::Rectangle<int> (x0 + 4 + lp * 3 + 2, 208, 54, 22),
                             juce::StringArray { "Mono", "Legato", "Poly" },
                             [this] { return synthe.voiceModeValue.get(); },
                             [this] (int i) { debutGeste(); synthe.voiceModeValue = i; },
                             "Voice Mode",
                             fr ("Poly : plusieurs notes à la fois (accords). Mono : une seule note à la fois (basses, solos). "
                                 "Legato : comme Mono, mais les notes liées ne redémarrent pas l'enveloppe."));
    }

    //==========================================================================
    juce::Array<te::AutomatableParameter::Ptr> ciblesLfo (int destination)
    {
        juce::Array<te::AutomatableParameter::Ptr> cibles;

        switch (destination)
        {
            case 1:  for (auto* o : synthe.oscParams) cibles.add (o->fineTune);     break;     // Pitch
            case 2:  cibles.add (synthe.filterFreq);                                break;     // Filter
            case 3:  for (auto* o : synthe.oscParams) cibles.add (o->level);        break;     // Volume
            case 4:  for (auto* o : synthe.oscParams) cibles.add (o->pulseWidth);   break;     // Pulse width
            case 5:  for (auto* o : synthe.oscParams) cibles.add (o->pan);          break;     // Pan
            default: break;
        }

        return cibles;
    }

    int destinationLfo (te::FourOscPlugin::ModSource source)
    {
        for (int d = 1; d <= 5; ++d)
            if (synthe.getModulationDepth (source, ciblesLfo (d).getFirst()) >= -1.0f)
                return d;
        return 0;
    }

    float quantiteLfo (te::FourOscPlugin::ModSource source)
    {
        const int d = destinationLfo (source);
        return d == 0 ? 0.0f : juce::jmax (0.0f, synthe.getModulationDepth (source, ciblesLfo (d).getFirst()));
    }

    void reglerLfo (te::FourOscPlugin::ModSource source, int destination, float quantite)
    {
        for (int d = 1; d <= 5; ++d)
            for (auto& p : ciblesLfo (d))
                synthe.clearModulation (source, p);

        for (auto& p : ciblesLfo (destination))
            synthe.setModulationDepth (source, p, quantite);

        static_cast<te::Plugin&> (synthe).flushPluginStateToValueTree();
        synthe.edit.markAsChanged();
    }

    void construireLfos (int x0)
    {
        for (int i = 0; i < 2; ++i)
        {
            auto* l = synthe.lfoParams[i];
            const auto source = i == 0 ? te::FourOscPlugin::lfo1 : te::FourOscPlugin::lfo2;
            const int y = i * 136;

            etiquette (x0, y + 2, "LFO " + juce::String (i + 1));

            ajouter<ChoixListe> (juce::Rectangle<int> (x0, y + 20, 98, 22),
                                 juce::StringArray { "Off", "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" },
                                 [l] { return l->waveShapeValue.get(); },
                                 [this, l] (int f) { debutGeste(); l->waveShapeValue = f; },
                                 "LFO Wave",
                                 fr ("Un LFO est un oscillateur très lent : on ne l'entend pas, il fait bouger un autre réglage "
                                     "tout seul (vibrato, wah-wah, trémolo...). Ici, la forme de ce mouvement."));

            ajouter<ChoixListe> (juce::Rectangle<int> (x0 + 102, y + 20, 98, 22),
                                 juce::StringArray { fr ("-> Rien"), "-> Pitch", "-> Filter", "-> Volume", "-> Width", "-> Pan" },
                                 [this, source] { return destinationLfo (source); },
                                 [this, source] (int d)
                                 {
                                     const float q = quantiteLfo (source);
                                     reglerLfo (source, d, q > 0.001f ? q : 0.3f);
                                 },
                                 "LFO Destination",
                                 fr ("Ce que le LFO fait bouger. Pitch : la hauteur (vibrato). Filter : le filtre (wah-wah). "
                                     "Volume : le niveau (trémolo). Width : la largeur de l'onde carrée. Pan : gauche/droite."));

            potard (x0 + 4, y + 48, l->rate, "Rate", fr ("La vitesse du LFO, en battements par seconde (Hz)."));

            Potard::Lien quantite;
            quantite.plage = juce::NormalisableRange<double> (0.0, 1.0);
            quantite.lire = [this, source] { return quantiteLfo (source); };
            quantite.ecrire = [this, source] (float v)
            {
                const int d = destinationLfo (source);
                if (d != 0)
                    reglerLfo (source, d, v);
            };
            quantite.texte = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
            potardLibre (x0 + 4 + lp, y + 48, "Amount", quantite,
                         fr ("La force du mouvement. Choisis d'abord une destination dans la liste au-dessus."));

            potard (x0 + 4 + lp * 2, y + 48, l->depth, "Depth", fr ("L'amplitude du LFO lui-même."));

            ajouter<Interrupteur> (juce::Rectangle<int> (x0 + 4 + lp * 3 + 6, y + 60, 52, 22), "Sync",
                                   [l] { return l->syncValue.get(); },
                                   [this, l] (bool b) { debutGeste(); l->syncValue = b; },
                                   "LFO Sync",
                                   fr ("Cale la vitesse du LFO sur le tempo du morceau au lieu du bouton Rate."));
        }
    }

    //==========================================================================
    void construireEffets (int x0)
    {
        etiquette (x0, 2, "EFFECTS");

        auto ligne = [this, x0] (int numero, const juce::String& nom, juce::CachedValue<bool>& actif, const juce::String& aide)
        {
            const int y = 18 + numero * 64;
            auto* a = &actif;
            ajouter<Interrupteur> (juce::Rectangle<int> (x0, y + 14, 74, 24), nom,
                                   [a] { return a->get(); },
                                   [this, a] (bool b) { debutGeste(); *a = b; },
                                   nom, aide);
            return y;
        };

        int y = ligne (0, "Distortion", synthe.distortionOnValue, fr ("Salit le son du synthé. Clique pour l'allumer."));
        potard (x0 + 80, y, synthe.distortion, "Amount", fr ("La quantité de distorsion."));

        y = ligne (1, "Chorus", synthe.chorusOnValue, fr ("Épaissit le son, comme si plusieurs instruments jouaient ensemble."));
        int x = x0 + 80;
        potard (x, y, synthe.chorusMix, "Mix", fr ("Le dosage entre le son normal et le son avec chorus."));
        potard (x += lp, y, synthe.chorusSpeed, "Speed", fr ("La vitesse du mouvement du chorus."));
        potard (x += lp, y, synthe.chorusDepth, "Depth", fr ("La profondeur du mouvement du chorus."));
        potard (x += lp, y, synthe.chorusWidth, "Width", fr ("La largeur stéréo du chorus."));

        y = ligne (2, "Delay", synthe.delayOnValue, fr ("Un écho : le son se répète, de moins en moins fort."));
        x = x0 + 80;
        potard (x, y, synthe.delayMix, "Mix", fr ("Le dosage entre le son normal et l'écho."));
        potard (x += lp, y, synthe.delayFeedback, "Feedb", fr ("Feedback : combien de fois l'écho se répète."));

        Potard::Lien temps;
        temps.plage = juce::NormalisableRange<double> (0.25, 4.0, 0.25);
        temps.lire = [this] { return synthe.delayValue.get(); };
        temps.ecrire = [this] (float v) { synthe.delayValue = v; };
        temps.texte = [] (float v) { return juce::String (v, 2) + " temps"; };
        temps.debutGeste = [this] { debutGeste(); };
        temps.defaut = 1.0;
        temps.aUnDefaut = true;
        potardLibre (x += lp, y, "Time", temps, fr ("L'écart entre deux répétitions, en temps (calé sur le tempo). 0,5 = une croche."));
        potard (x += lp, y, synthe.delayCrossfeed, "Cross", fr ("Fait rebondir l'écho entre la gauche et la droite."));

        y = ligne (3, "Reverb", synthe.reverbOnValue, fr ("Donne l'impression de jouer dans une grande salle."));
        x = x0 + 80;
        potard (x, y, synthe.reverbMix, "Mix", fr ("Le dosage entre le son normal et la réverbération."));
        potard (x += lp, y, synthe.reverbSize, "Size", fr ("La taille de la salle : petite pièce ou cathédrale."));
        potard (x += lp, y, synthe.reverbDamping, "Damp", fr ("Étouffe les aigus de la réverbération, comme une salle avec des rideaux."));
        potard (x += lp, y, synthe.reverbWidth, "Width", fr ("La largeur stéréo de la réverbération."));
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
/** La chaîne de périphériques de la piste choisie : instrument, puis effets, de gauche à droite. */
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

        definirAide (*this, fr ("Instrument et effets"),
                     fr ("L'instrument de la piste choisie, suivi de ses effets. Le son va de gauche à droite. "
                         "Tu peux glisser ici un instrument ou un effet depuis le navigateur."));

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
        return id.startsWith ("fx:") || id.startsWith ("inst:") || id.startsWith ("preset:");
    }

    void itemDropped (const SourceDetails& d) override
    {
        if (auto* piste = selection.pisteValide())
            appliquerElement (*piste, d.description.toString());
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
    te::Edit& edit;
    Selection& selection;

    juce::Viewport vue;
    juce::Component contenu;
    juce::OwnedArray<Peripherique> peripheriques;
    juce::TextButton ajouterEffet;

    te::AudioTrack::Ptr pisteAffichee;
    juce::Array<te::Plugin*> chaineAffichee;

    //==========================================================================
    void timerCallback() override
    {
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
                    auto* s = new PeripheriqueSynthe (*synthe, couleur);
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

                peripheriques.add (p);
                contenu.addAndMakeVisible (p);
            }
        }

        // Mise en place, de gauche à droite
        int x = 6;
        for (auto* p : peripheriques)
        {
            p->setBounds (x, 4, p->getLargeur(), Peripherique::hauteurTotale);
            x += p->getLargeur() + 8;
        }

        ajouterEffet.setVisible (piste != nullptr);
        ajouterEffet.setBounds (x, 4, 84, 28);
        contenu.setSize (x + 96, Peripherique::hauteurTotale + 8);
        vue.setViewPosition (0, 0);
        repaint();
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
