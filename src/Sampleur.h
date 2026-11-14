#pragma once

#include "Commun.h"
#include "FormesOnde.h"

//==============================================================================
/** Quack Sampler : joue un son enregistré comme un instrument.
    - Mode « Clavier » : le son est joué plus aigu ou plus grave selon la note (jeu chromatique).
    - Mode « Tranches » : le son est découpé en parts égales, une par touche à partir de C1
      (pratique pour rejouer une boucle de batterie morceau par morceau). */
class SampleurQuack : public te::Plugin
{
public:
    static const char* getPluginName()      { return "Quack Sampler"; }
    static const char* xmlTypeName;

    static constexpr int premiereNoteTranches = 36;
    static constexpr int maxVoix = 16;

    SampleurQuack (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();

        fichierValue.referTo  (state, "fichier", um, juce::String());
        noteBaseValue.referTo (state, "noteBase", um, 60);
        inverseValue.referTo  (state, "inverse", um, false);
        boucleValue.referTo   (state, "boucle", um, false);
        modeValue.referTo     (state, "mode", um, 0);
        tranchesValue.referTo (state, "tranches", um, 8);

        auto creer = [this, um] (juce::CachedValue<float>& valeur, te::AutomatableParameter::Ptr& param,
                                 const char* id, const char* nom, juce::NormalisableRange<float> plage, float defaut)
        {
            valeur.referTo (state, id, um, defaut);
            param = addParam (id, nom, plage);
            param->attachToCurrentValue (valeur);
        };

        creer (gainValue,    gain,    "gain",    "Gain",    { -36.0f, 12.0f }, 0.0f);
        creer (pitchValue,   pitch,   "pitch",   "Pitch",   { -24.0f, 24.0f, 1.0f }, 0.0f);
        creer (fineValue,    fine,    "fine",    "Fine",    { -100.0f, 100.0f }, 0.0f);
        creer (debutValue,   debut,   "debut",   "Start",   { 0.0f, 1.0f }, 0.0f);
        creer (finValue,     fin,     "fin",     "End",     { 0.0f, 1.0f }, 1.0f);
        creer (attackValue,  attack,  "attack",  "Attack",  { 0.0f, 5.0f, 0.0f, 0.3f }, 0.002f);
        creer (decayValue,   decay,   "decay",   "Decay",   { 0.0f, 5.0f, 0.0f, 0.3f }, 0.2f);
        creer (sustainValue, sustain, "sustain", "Sustain", { 0.0f, 1.0f }, 1.0f);
        creer (releaseValue, release, "release", "Release", { 0.0f, 5.0f, 0.0f, 0.3f }, 0.05f);

        recharger();
    }

    ~SampleurQuack() override
    {
        notifyListenersOfDeletion();

        for (auto p : getAutomatableParameters())
            p->detachFromCurrentValue();
    }

    //==========================================================================
    juce::String getName() const override                   { return getPluginName(); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getShortName (int) override                { return "Sampler"; }
    juce::String getSelectableDescription() override        { return getName(); }
    bool isSynth() override                                 { return true; }
    bool takesMidiInput() override                          { return true; }
    bool takesAudioInput() override                         { return true; }
    bool producesAudioWhenNoAudioInput() override           { return true; }
    int getNumOutputChannelsGivenInputs (int) override      { return 2; }
    BusLayout getBusses() const override                    { return BusLayout::singleStereoInOut(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        frequenceSortie = info.sampleRate > 0.0 ? info.sampleRate : 44100.0;

        const juce::SpinLock::ScopedLockType verrou (verrouVoix);
        for (auto& v : voix)
            v.active = false;
    }

    void deinitialise() override {}

    //==========================================================================
    /** Le son chargé (échantillons + fréquence d'origine). */
    struct Son
    {
        juce::AudioBuffer<float> donnees;
        double frequence = 44100.0;
        juce::File fichier;
    };

    /** Choisit le fichier son à jouer. */
    void definirFichier (const juce::File& f)
    {
        fichierValue = f.getFullPathName();
        recharger();
    }

    juce::File getFichier() const       { return juce::File (fichierValue.get()); }

    /** À appeler de temps en temps par l'interface : recharge le son si le fichier a changé
        (après un « annuler » ou l'ouverture d'un morceau). */
    void verifierFichier()
    {
        auto actuel = sonCharge();
        const auto voulu = fichierValue.get();

        if ((actuel == nullptr && voulu.isNotEmpty() && ! chargementEchoue)
            || (actuel != nullptr && actuel->fichier.getFullPathName() != voulu))
            recharger();
    }

    std::shared_ptr<Son> sonCharge() const
    {
        const juce::SpinLock::ScopedLockType verrou (verrouSon);
        return son;
    }

    int nombreDeTranches() const        { return juce::jlimit (2, 64, tranchesValue.get()); }
    bool modeTranches() const           { return modeValue.get() == 1; }

    /** Position de lecture de la voix la plus récente (0..1 dans le fichier), ou négatif si rien ne joue. */
    float positionDeLecture() const     { return positionAffichee.load (std::memory_order_relaxed); }

    //==========================================================================
    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr)
            return;

        te::clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

        // On prend le son sans jamais bloquer : s'il est en cours de remplacement, on saute ce bloc
        std::shared_ptr<Son> local;
        {
            const juce::SpinLock::ScopedTryLockType verrou (verrouSon);
            if (verrou.isLocked())
                local = son;
        }

        const juce::SpinLock::ScopedTryLockType verrou (verrouVoix);
        if (! verrou.isLocked())
            return;

        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff)
                for (auto& v : voix)
                    if (v.active)
                        v.enveloppe.noteOff();

            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn() && local != nullptr)
                    demarrerNote (*local, m.getNoteNumber(), m.getVelocity() / 127.0f);
                else if (m.isNoteOff())
                    arreterNote (m.getNoteNumber());
                else if (m.isAllNotesOff() || m.isAllSoundOff())
                    for (auto& v : voix)
                        if (v.active)
                            v.enveloppe.noteOff();
            }
        }

        if (local == nullptr || local->donnees.getNumSamples() < 2)
        {
            positionAffichee.store (-1.0f, std::memory_order_relaxed);
            return;
        }

        const int canauxSortie = juce::jmin (2, fc.destBuffer->getNumChannels());
        const int canauxSon = local->donnees.getNumChannels();
        const int total = local->donnees.getNumSamples();
        const float volume = juce::Decibels::decibelsToGain (gain->getCurrentValue());
        float affichage = -1.0f;
        juce::uint32 plusRecente = 0;

        for (auto& v : voix)
        {
            if (! v.active)
                continue;

            const float* gauche = local->donnees.getReadPointer (0);
            const float* droite = local->donnees.getReadPointer (canauxSon > 1 ? 1 : 0);
            float* sortieG = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);
            float* sortieD = canauxSortie > 1 ? fc.destBuffer->getWritePointer (1, fc.bufferStartSample) : nullptr;

            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                // Arrivé au bout de sa zone : on reboucle ou on s'arrête
                if (v.position >= v.fin || v.position < v.debut)
                {
                    if (v.boucle && v.fin - v.debut > 4.0)
                    {
                        v.position = v.pas > 0.0 ? v.debut : v.fin - 1.0;
                    }
                    else
                    {
                        v.active = false;
                        break;
                    }
                }

                const float niveau = v.enveloppe.getNextSample() * v.velocite * volume;

                if (! v.enveloppe.isActive())
                {
                    v.active = false;
                    break;
                }

                // Lecture avec interpolation entre deux échantillons voisins
                const int index = juce::jlimit (0, total - 2, (int) v.position);
                const float fraction = (float) (v.position - (double) index);
                const float g = gauche[index] + (gauche[index + 1] - gauche[index]) * fraction;
                const float d = droite[index] + (droite[index + 1] - droite[index]) * fraction;

                sortieG[i] += g * niveau;
                if (sortieD != nullptr)
                    sortieD[i] += d * niveau;

                v.position += v.pas;
            }

            if (v.active && v.numero >= plusRecente)
            {
                plusRecente = v.numero;
                affichage = (float) (v.position / (double) total);
            }
        }

        positionAffichee.store (affichage, std::memory_order_relaxed);
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, fichierValue, noteBaseValue, inverseValue, boucleValue, modeValue, tranchesValue,
                                          gainValue, pitchValue, fineValue, debutValue, finValue,
                                          attackValue, decayValue, sustainValue, releaseValue);

        for (auto p : getAutomatableParameters())
            p->updateFromAttachedValue();

        recharger();
    }

    //==========================================================================
    juce::CachedValue<juce::String> fichierValue;
    juce::CachedValue<int> noteBaseValue, modeValue, tranchesValue;
    juce::CachedValue<bool> inverseValue, boucleValue;
    juce::CachedValue<float> gainValue, pitchValue, fineValue, debutValue, finValue,
                             attackValue, decayValue, sustainValue, releaseValue;
    te::AutomatableParameter::Ptr gain, pitch, fine, debut, fin, attack, decay, sustain, release;

private:
    struct Voix
    {
        bool active = false;
        int note = 0;
        juce::uint32 numero = 0;
        double position = 0.0, pas = 1.0, debut = 0.0, fin = 0.0;
        float velocite = 1.0f;
        bool boucle = false;
        bool unCoup = false;        // joue sa tranche jusqu'au bout, même si la touche est relâchée
        juce::ADSR enveloppe;
    };

    std::array<Voix, (size_t) maxVoix> voix;
    juce::SpinLock verrouVoix;
    mutable juce::SpinLock verrouSon;
    std::shared_ptr<Son> son;
    bool chargementEchoue = false;
    double frequenceSortie = 44100.0;
    juce::uint32 compteurNotes = 0;
    std::atomic<float> positionAffichee { -1.0f };

    //==========================================================================
    void recharger()
    {
        const juce::File f (fichierValue.get());
        std::shared_ptr<Son> nouveau;
        chargementEchoue = false;

        if (fichierValue.get().isNotEmpty())
        {
            if (std::unique_ptr<juce::AudioFormatReader> lecteur { FormesOnde::formats().createReaderFor (f) })
            {
                // On garde au plus 2 minutes de son en mémoire
                const auto longueur = (int) juce::jmin (lecteur->lengthInSamples, (juce::int64) (lecteur->sampleRate * 120.0));

                nouveau = std::make_shared<Son>();
                nouveau->donnees.setSize ((int) juce::jlimit (1u, 2u, lecteur->numChannels), juce::jmax (2, longueur));
                nouveau->donnees.clear();
                lecteur->read (&nouveau->donnees, 0, longueur, 0, true, true);
                nouveau->frequence = lecteur->sampleRate;
                nouveau->fichier = f;
            }
            else
            {
                chargementEchoue = true;
                Journal::ecrire ("Sampler : son illisible : " + f.getFullPathName());
            }
        }

        {
            const juce::SpinLock::ScopedLockType verrou (verrouVoix);
            for (auto& v : voix)
                v.active = false;
        }

        const juce::SpinLock::ScopedLockType verrou (verrouSon);
        son = nouveau;
    }

    void demarrerNote (const Son& s, int note, float velocite)
    {
        const int total = s.donnees.getNumSamples();
        double zoneDebut = juce::jlimit (0.0f, 1.0f, debut->getCurrentValue()) * total;
        double zoneFin = juce::jlimit (0.0f, 1.0f, fin->getCurrentValue()) * total;

        if (zoneFin < zoneDebut)
            std::swap (zoneDebut, zoneFin);

        double demiTons = pitch->getCurrentValue() + fine->getCurrentValue() / 100.0;

        if (modeTranches())
        {
            // Chaque touche à partir de C1 joue une part du son, à sa hauteur d'origine
            const int nombre = nombreDeTranches();
            const int tranche = note - premiereNoteTranches;

            if (tranche < 0 || tranche >= nombre)
                return;

            const double part = (zoneFin - zoneDebut) / nombre;
            zoneFin = zoneDebut + part * (tranche + 1);
            zoneDebut = zoneDebut + part * tranche;
        }
        else
        {
            demiTons += note - noteBaseValue.get();
        }

        if (zoneFin - zoneDebut < 2.0)
            return;

        // On prend une voix libre, sinon la plus ancienne
        Voix* choisie = nullptr;
        for (auto& v : voix)
        {
            if (! v.active)
            {
                choisie = &v;
                break;
            }

            if (choisie == nullptr || v.numero < choisie->numero)
                choisie = &v;
        }

        const double vitesse = (s.frequence / frequenceSortie) * std::pow (2.0, demiTons / 12.0);
        const bool alEnvers = inverseValue.get();

        choisie->active = true;
        choisie->note = note;
        choisie->numero = ++compteurNotes;
        choisie->debut = zoneDebut;
        choisie->fin = zoneFin;
        choisie->pas = alEnvers ? -vitesse : vitesse;
        choisie->position = alEnvers ? zoneFin - 1.0 : zoneDebut;
        choisie->velocite = velocite;
        choisie->boucle = boucleValue.get() && ! modeTranches();
        choisie->unCoup = modeTranches();

        juce::ADSR::Parameters p;
        p.attack  = juce::jmax (0.001f, attack->getCurrentValue());
        p.decay   = juce::jmax (0.001f, decay->getCurrentValue());
        p.sustain = sustain->getCurrentValue();
        p.release = juce::jmax (0.003f, release->getCurrentValue());
        choisie->enveloppe.setSampleRate (frequenceSortie);
        choisie->enveloppe.setParameters (p);
        choisie->enveloppe.reset();
        choisie->enveloppe.noteOn();
    }

    void arreterNote (int note)
    {
        for (auto& v : voix)
            if (v.active && v.note == note && ! v.unCoup)
                v.enveloppe.noteOff();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleurQuack)
};

inline const char* SampleurQuack::xmlTypeName = "quackSampler";
