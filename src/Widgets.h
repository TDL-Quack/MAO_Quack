#pragma once

#include "Commun.h"

/** Tout ce qui doit se remettre à jour quand un réglage change (annuler, preset...). */
struct Rafraichissable
{
    virtual ~Rafraichissable() = default;
    virtual void rafraichir() = 0;
};

//==============================================================================
/** Un potard (bouton rotatif) avec son nom dessous. Au survol, le nom laisse place à la valeur. */
class Potard : public juce::Component,
               public Rafraichissable
{
public:
    struct Lien
    {
        std::function<float()> lire;
        std::function<void (float)> ecrire;
        std::function<juce::String (float)> texte;
        std::function<void()> debutGeste;
        juce::NormalisableRange<double> plage { 0.0, 1.0 };
        double defaut = 0.0;
        bool aUnDefaut = false;
    };

    /** Lien vers un paramètre du moteur audio. */
    static Lien versParametre (te::AutomatableParameter::Ptr p)
    {
        Lien l;
        const auto v = p->valueRange;
        l.plage = juce::NormalisableRange<double> ((double) v.start, (double) v.end, (double) v.interval,
                                                   (double) v.skew, v.symmetricSkew);
        l.lire = [p] { return p->getCurrentValue(); };
        l.ecrire = [p] (float x) { p->setParameter (x, juce::sendNotification); };
        l.texte = [p] (float x)
        {
            auto t = p->valueToString (x);
            return t.isNotEmpty() ? t : juce::String (x, 2);
        };
        l.debutGeste = [p] { p->getEdit().getUndoManager().beginNewTransaction(); };

        if (auto d = p->getDefaultValue())
        {
            l.defaut = (double) *d;
            l.aUnDefaut = true;
        }

        return l;
    }

    Potard (const juce::String& nomAffiche, Lien l, const juce::String& aide)
        : lien (std::move (l)), nom (nomAffiche)
    {
        addAndMakeVisible (bouton);
        bouton.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        bouton.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        bouton.setNormalisableRange (lien.plage);
        bouton.setColour (juce::Slider::rotarySliderFillColourId, Couleurs::accent);
        bouton.setColour (juce::Slider::rotarySliderOutlineColourId, Couleurs::bordure);
        bouton.setColour (juce::Slider::thumbColourId, Couleurs::texte);
        bouton.setWantsKeyboardFocus (false);

        if (lien.aUnDefaut)
            bouton.setDoubleClickReturnValue (true, lien.defaut);

        bouton.onDragStart = [this] { if (lien.debutGeste) lien.debutGeste(); };
        bouton.onValueChange = [this]
        {
            if (lien.ecrire)
                lien.ecrire ((float) bouton.getValue());
            repaint();
        };

        definirAide (*this, nomAffiche, aide + fr (" Glisse vers le haut ou le bas pour régler ; double-clic : valeur de départ."));
        rafraichir();
    }

    void rafraichir() override
    {
        if (lien.lire && ! bouton.isMouseButtonDown())
        {
            const double v = (double) lien.lire();
            if (std::abs (v - bouton.getValue()) > 1.0e-6)
                bouton.setValue (v, juce::dontSendNotification);
        }

        const bool survol = isMouseOver (true) || bouton.isMouseButtonDown();
        if (survol != survolPrecedent)
        {
            survolPrecedent = survol;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const bool survol = isMouseOver (true) || bouton.isMouseButtonDown();
        g.setFont (juce::FontOptions (11.0f));
        g.setColour (survol ? Couleurs::accent : Couleurs::texteDoux);

        juce::String t = nom;
        if (survol)
            t = lien.texte ? lien.texte ((float) bouton.getValue()) : juce::String (bouton.getValue(), 2);

        g.drawText (t, getLocalBounds().removeFromBottom (13), juce::Justification::centred, false);
    }

    void resized() override
    {
        bouton.setBounds (getLocalBounds().withTrimmedBottom (12));
    }

private:
    Lien lien;
    juce::String nom;
    juce::Slider bouton;
    bool survolPrecedent = false;
};

//==============================================================================
/** Une liste déroulante liée à un réglage entier. */
class ChoixListe : public juce::Component,
                   public Rafraichissable
{
public:
    ChoixListe (const juce::StringArray& choix, std::function<int()> lireIndex, std::function<void (int)> ecrireIndex,
                const juce::String& titreAide, const juce::String& aide)
        : lire (std::move (lireIndex)), ecrire (std::move (ecrireIndex))
    {
        addAndMakeVisible (liste);
        liste.addItemList (choix, 1);
        liste.setWantsKeyboardFocus (false);
        liste.onChange = [this] { if (ecrire) ecrire (liste.getSelectedItemIndex()); };
        definirAide (*this, titreAide, aide);
        rafraichir();
    }

    void definirChoix (const juce::StringArray& choix)
    {
        liste.clear (juce::dontSendNotification);
        liste.addItemList (choix, 1);
        rafraichir();
    }

    void rafraichir() override
    {
        if (lire && ! liste.isPopupActive())
        {
            const int i = lire();
            if (i != liste.getSelectedItemIndex())
                liste.setSelectedItemIndex (i, juce::dontSendNotification);
        }
    }

    void resized() override     { liste.setBounds (getLocalBounds()); }

private:
    std::function<int()> lire;
    std::function<void (int)> ecrire;
    juce::ComboBox liste;
};

//==============================================================================
/** Un interrupteur marche/arrêt. */
class Interrupteur : public juce::TextButton,
                     public Rafraichissable
{
public:
    Interrupteur (const juce::String& texte, std::function<bool()> lireEtat, std::function<void (bool)> ecrireEtat,
                  const juce::String& titreAide, const juce::String& aide)
        : juce::TextButton (texte), lire (std::move (lireEtat)), ecrire (std::move (ecrireEtat))
    {
        setClickingTogglesState (true);
        setWantsKeyboardFocus (false);
        onClick = [this] { if (ecrire) ecrire (getToggleState()); };
        definirAide (*this, titreAide, aide);
        rafraichir();
    }

    void rafraichir() override
    {
        if (lire)
            setToggleState (lire(), juce::dontSendNotification);
    }

private:
    std::function<bool()> lire;
    std::function<void (bool)> ecrire;
};

//==============================================================================
/** Petit écran qui montre une forme d'onde ; un clic ouvre la liste des formes. */
class EcranOnde : public juce::Component,
                  public Rafraichissable
{
public:
    /** Formes : 0 aucune, 1 sinus, 2 carré, 3 scie, 4 triangle, 5 bruit */
    EcranOnde (std::function<int()> lireForme, std::function<void (int)> ecrireForme, juce::Colour c)
        : lire (std::move (lireForme)), ecrire (std::move (ecrireForme)), couleur (c)
    {
        definirAide (*this, "Wave",
                     fr ("La forme de l'onde, qui donne sa couleur au son. Sine : doux et rond. Triangle : doux avec un peu "
                         "de brillance. Saw (scie) : riche et brillant. Square (carré) : creux, façon jeu vidéo. "
                         "Noise : du souffle. Off : oscillateur éteint. Clique pour choisir."));
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        rafraichir();
    }

    static juce::StringArray noms()     { return { "Off", "Sine", "Square", "Saw", "Triangle", "Noise" }; }

    static float valeur (int forme, float phase, juce::Random& alea)
    {
        switch (forme)
        {
            case 1:  return std::sin (phase * juce::MathConstants<float>::twoPi);
            case 2:  return phase < 0.5f ? 1.0f : -1.0f;
            case 3:  return 1.0f - 2.0f * phase;
            case 4:  return phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
            case 5:  return alea.nextFloat() * 2.0f - 1.0f;
            default: return 0.0f;
        }
    }

    void rafraichir() override
    {
        const int f = lire ? lire() : 0;
        if (f != forme)
        {
            forme = f;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Couleurs::bordure);
        g.fillRoundedRectangle (r, 3.0f);

        const auto z = r.reduced (4.0f, 5.0f);
        juce::Random alea (7);
        juce::Path p;

        for (int i = 0; i <= 64; ++i)
        {
            const float phase = std::fmod ((float) i / 32.0f, 1.0f);       // deux périodes
            const float x = z.getX() + z.getWidth() * (float) i / 64.0f;
            const float y = z.getCentreY() - valeur (forme, i == 64 ? 0.999f : phase, alea) * z.getHeight() * 0.5f;

            if (i == 0)   p.startNewSubPath (x, y);
            else          p.lineTo (x, y);
        }

        g.setColour (forme == 0 ? Couleurs::texteDoux.withAlpha (0.4f) : couleur);
        g.strokePath (p, juce::PathStrokeType (1.6f));

        g.setColour (Couleurs::texteDoux);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (noms()[forme], getLocalBounds().reduced (4, 1), juce::Justification::topLeft, false);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        juce::PopupMenu m;
        int i = 0;
        for (auto& n : noms())
        {
            m.addItem (i + 1, n, true, i == forme);
            ++i;
        }

        juce::Component::SafePointer<EcranOnde> moi (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [moi] (int choix)
                         {
                             if (moi != nullptr && choix > 0 && moi->ecrire)
                             {
                                 moi->ecrire (choix - 1);
                                 moi->rafraichir();
                             }
                         });
    }

private:
    std::function<int()> lire;
    std::function<void (int)> ecrire;
    juce::Colour couleur;
    int forme = -1;
};

//==============================================================================
/** Écran d'enveloppe ADSR : la courbe se redessine en direct et ses points se tirent à la souris. */
class EcranEnveloppe : public juce::Component,
                       public Rafraichissable
{
public:
    EcranEnveloppe (te::AutomatableParameter::Ptr a, te::AutomatableParameter::Ptr d,
                    te::AutomatableParameter::Ptr s, te::AutomatableParameter::Ptr r,
                    juce::Colour c, const juce::String& titre)
        : attack (a), decay (d), sustain (s), release (r), couleur (c)
    {
        definirAide (*this, titre,
                     fr ("La vie d'une note. Attack : le temps pour arriver au maximum. Decay : le temps pour redescendre. "
                         "Sustain : le niveau tant que la touche reste enfoncée. Release : le temps pour s'éteindre "
                         "quand on lâche. Tire les points pour changer la forme."));
    }

    void rafraichir() override
    {
        const float v[] = { attack->getCurrentValue(), decay->getCurrentValue(),
                            sustain->getCurrentValue(), release->getCurrentValue() };

        if (v[0] != vu[0] || v[1] != vu[1] || v[2] != vu[2] || v[3] != vu[3])
        {
            for (int i = 0; i < 4; ++i)
                vu[i] = v[i];
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Couleurs::bordure);
        g.fillRoundedRectangle (r, 3.0f);

        const auto pts = points();
        juce::Path p;
        p.startNewSubPath (pts[0]);
        for (int i = 1; i < 5; ++i)
            p.lineTo (pts[(size_t) i]);

        juce::Path rempli (p);
        rempli.lineTo (pts[4].x, zone().getBottom());
        rempli.lineTo (pts[0].x, zone().getBottom());
        rempli.closeSubPath();

        g.setColour (couleur.withAlpha (0.2f));
        g.fillPath (rempli);
        g.setColour (couleur);
        g.strokePath (p, juce::PathStrokeType (1.6f));

        for (int i = 1; i <= 3; ++i)
        {
            const auto pt = i == 3 ? pts[4] : pts[(size_t) i];
            g.setColour (i == poignee ? juce::Colours::white : couleur.brighter (0.5f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pts = points();
        const juce::Point<float> cibles[] = { pts[1], pts[2], pts[4] };
        poignee = 0;
        float meilleur = 1.0e9f;

        for (int i = 0; i < 3; ++i)
        {
            const float d = cibles[i].getDistanceFrom (e.position);
            if (d < meilleur)
            {
                meilleur = d;
                poignee = i + 1;
            }
        }

        attack->getEdit().getUndoManager().beginNewTransaction();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto z = zone();
        const float segment = z.getWidth() / 4.0f;
        auto versTemps = [segment] (float px) { return tempsDepuis (juce::jlimit (0.0f, 1.0f, px / segment)); };
        const float niveau = juce::jlimit (0.0f, 100.0f, (z.getBottom() - e.position.y) / z.getHeight() * 100.0f);

        if (poignee == 1)
        {
            attack->setParameter (versTemps (e.position.x - z.getX()), juce::sendNotification);
        }
        else if (poignee == 2)
        {
            decay->setParameter (versTemps (e.position.x - points()[1].x), juce::sendNotification);
            sustain->setParameter (niveau, juce::sendNotification);
        }
        else if (poignee == 3)
        {
            release->setParameter (versTemps (e.position.x - points()[3].x), juce::sendNotification);
        }

        rafraichir();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        poignee = 0;
        repaint();
    }

private:
    te::AutomatableParameter::Ptr attack, decay, sustain, release;
    juce::Colour couleur;
    float vu[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
    int poignee = 0;

    // Échelle des temps : 0..1 sur l'écran <-> 0..5 secondes, plus précise pour les temps courts
    static float proportion (float secondes)    { return std::pow (juce::jlimit (0.0f, 1.0f, secondes / 5.0f), 0.4f); }
    static float tempsDepuis (float prop)       { return 5.0f * std::pow (prop, 2.5f); }

    juce::Rectangle<float> zone() const         { return getLocalBounds().toFloat().reduced (6.0f, 7.0f); }

    /** Départ, fin d'attaque, fin de chute, fin du maintien, fin du relâchement. */
    std::array<juce::Point<float>, 5> points() const
    {
        const auto z = zone();
        const float segment = z.getWidth() / 4.0f;
        const float ySustain = z.getBottom() - z.getHeight() * juce::jlimit (0.0f, 1.0f, sustain->getCurrentValue() / 100.0f);

        const float x1 = z.getX() + segment * proportion (attack->getCurrentValue());
        const float x2 = x1 + segment * proportion (decay->getCurrentValue());
        const float x3 = x2 + segment * 0.6f;
        const float x4 = x3 + segment * proportion (release->getCurrentValue());

        return {{ { z.getX(), z.getBottom() }, { x1, z.getY() }, { x2, ySustain }, { x3, ySustain }, { x4, z.getBottom() } }};
    }
};

//==============================================================================
/** Écran générique : dessine une courbe fournie par une fonction, sur une échelle de fréquences.
    Sert au filtre du synthé et à l'égaliseur. On peut y poser des points à tirer. */
class EcranCourbe : public juce::Component,
                    public Rafraichissable
{
public:
    struct Point
    {
        std::function<float()> lireFrequence, lireHauteur;              // hauteur : valeur entre plageBas et plageHaut
        std::function<void (float, float)> ecrire;                       // (fréquence, hauteur)
        juce::Colour couleur;
        juce::String etiquette;
    };

    EcranCourbe (float bas, float haut, juce::Colour c) : plageBas (bas), plageHaut (haut), couleur (c) {}

    std::function<float (float)> courbe;            // fréquence (Hz) -> valeur (entre bas et haut)
    std::function<void()> debutGeste;
    std::vector<Point> points;
    bool tracerZero = true;

    static float frequenceDepuis (float prop)   { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, prop)); }
    static float proportionDe (float hz)        { return std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f); }

    void rafraichir() override      { repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Couleurs::bordure);
        g.fillRoundedRectangle (r, 3.0f);

        const auto z = zone();

        // Repères : 100 Hz, 1 kHz, 10 kHz
        g.setFont (juce::FontOptions (9.5f));
        const std::pair<float, const char*> reperes[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };

        for (auto& rep : reperes)
        {
            const float x = z.getX() + z.getWidth() * proportionDe (rep.first);
            g.setColour (Couleurs::grille);
            g.drawVerticalLine ((int) x, z.getY(), z.getBottom());
            g.setColour (Couleurs::texteDoux);
            g.drawText (rep.second, (int) x + 2, (int) z.getBottom() - 11, 30, 11, juce::Justification::left, false);
        }

        if (tracerZero && plageBas < 0.0f && plageHaut > 0.0f)
        {
            g.setColour (Couleurs::grilleMesure);
            g.drawHorizontalLine ((int) yDe (0.0f), z.getX(), z.getRight());
        }

        if (courbe)
        {
            juce::Path p;
            const int n = juce::jmax (2, (int) z.getWidth() / 2);

            for (int i = 0; i <= n; ++i)
            {
                const float prop = (float) i / (float) n;
                const float x = z.getX() + z.getWidth() * prop;
                const float y = yDe (courbe (frequenceDepuis (prop)));

                if (i == 0)   p.startNewSubPath (x, y);
                else          p.lineTo (x, y);
            }

            juce::Path rempli (p);
            const float base = (plageBas < 0.0f && plageHaut > 0.0f) ? yDe (0.0f) : z.getBottom();
            rempli.lineTo (z.getRight(), base);
            rempli.lineTo (z.getX(), base);
            rempli.closeSubPath();

            g.saveState();
            g.reduceClipRegion (z.toNearestInt());
            g.setColour (couleur.withAlpha (0.18f));
            g.fillPath (rempli);
            g.setColour (couleur);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            g.restoreState();
        }

        int i = 0;
        for (auto& pt : points)
        {
            const auto pos = positionDe (pt);
            g.setColour (i == poignee ? juce::Colours::white : pt.couleur);
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (pos));
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText (pt.etiquette, juce::Rectangle<float> (13.0f, 13.0f).withCentre (pos),
                        juce::Justification::centred, false);
            ++i;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        poignee = -1;
        float meilleur = 1.0e9f;

        for (int i = 0; i < (int) points.size(); ++i)
        {
            const float d = positionDe (points[(size_t) i]).getDistanceFrom (e.position);
            if (d < meilleur)
            {
                meilleur = d;
                poignee = i;
            }
        }

        if (poignee >= 0 && debutGeste)
            debutGeste();

        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (poignee < 0 || poignee >= (int) points.size())
            return;

        const auto z = zone();
        const float hz = frequenceDepuis ((e.position.x - z.getX()) / z.getWidth());
        const float h = plageBas + (plageHaut - plageBas)
                                     * juce::jlimit (0.0f, 1.0f, (z.getBottom() - e.position.y) / z.getHeight());

        if (points[(size_t) poignee].ecrire)
            points[(size_t) poignee].ecrire (hz, h);

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        poignee = -1;
        repaint();
    }

private:
    float plageBas, plageHaut;
    juce::Colour couleur;
    int poignee = -1;

    juce::Rectangle<float> zone() const     { return getLocalBounds().toFloat().reduced (5.0f, 6.0f); }

    float yDe (float valeur) const
    {
        const auto z = zone();
        return z.getBottom() - z.getHeight() * juce::jlimit (0.0f, 1.0f, (valeur - plageBas) / (plageHaut - plageBas));
    }

    juce::Point<float> positionDe (const Point& pt) const
    {
        const auto z = zone();
        return { z.getX() + z.getWidth() * proportionDe (pt.lireFrequence ? pt.lireFrequence() : 1000.0f),
                 yDe (pt.lireHauteur ? pt.lireHauteur() : 0.0f) };
    }
};

//==============================================================================
/** Écran « entrée -> sortie » : montre comment un effet transforme le niveau du son
    (compresseur, distorsion). */
class EcranTransfert : public juce::Component,
                       public Rafraichissable
{
public:
    EcranTransfert (juce::Colour c) : couleur (c) {}

    std::function<float (float)> transfert;     // entrée 0..1 (ou -1..1 si bipolaire) -> sortie
    bool bipolaire = false;

    void rafraichir() override      { repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Couleurs::bordure);
        g.fillRoundedRectangle (r, 3.0f);

        const auto z = r.reduced (5.0f);
        const float mini = bipolaire ? -1.0f : 0.0f;

        g.setColour (Couleurs::grille);
        g.drawLine (z.getX(), z.getBottom(), z.getRight(), z.getY(), 1.0f);      // la diagonale = son inchangé

        if (bipolaire)
        {
            g.drawHorizontalLine ((int) z.getCentreY(), z.getX(), z.getRight());
            g.drawVerticalLine ((int) z.getCentreX(), z.getY(), z.getBottom());
        }

        if (! transfert)
            return;

        juce::Path p;
        for (int i = 0; i <= 60; ++i)
        {
            const float prop = (float) i / 60.0f;
            const float entree = mini + (1.0f - mini) * prop;
            const float sortie = juce::jlimit (mini, 1.0f, transfert (entree));
            const float x = z.getX() + z.getWidth() * prop;
            const float y = z.getBottom() - z.getHeight() * (sortie - mini) / (1.0f - mini);

            if (i == 0)   p.startNewSubPath (x, y);
            else          p.lineTo (x, y);
        }

        g.setColour (couleur);
        g.strokePath (p, juce::PathStrokeType (1.8f));
    }

private:
    juce::Colour couleur;
};
