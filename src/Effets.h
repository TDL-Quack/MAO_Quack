#pragma once

#include "Commun.h"
#include "Vst.h"
#include "Instruments.h"

//==============================================================================
/** Distorsion : pousse le son dans une courbe qui l'écrase (saturation). */
class DistorsionQuack : public te::Plugin
{
public:
    static const char* getPluginName()      { return "Distortion"; }
    static const char* xmlTypeName;

    DistorsionQuack (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        driveValue.referTo  (state, "drive", um, 12.0f);
        toneValue.referTo   (state, "tone", um, 8000.0f);
        mixValue.referTo    (state, "mix", um, 1.0f);
        outputValue.referTo (state, "output", um, -6.0f);

        drive  = addParam ("drive",  "Drive",   { 0.0f, 40.0f });
        tone   = addParam ("tone",   "Tone",    { 200.0f, 20000.0f, 0.0f, 0.3f });
        mix    = addParam ("mix",    "Dry/Wet", { 0.0f, 1.0f });
        output = addParam ("output", "Output",  { -24.0f, 6.0f });

        drive->attachToCurrentValue (driveValue);
        tone->attachToCurrentValue (toneValue);
        mix->attachToCurrentValue (mixValue);
        output->attachToCurrentValue (outputValue);
    }

    ~DistorsionQuack() override
    {
        notifyListenersOfDeletion();
        drive->detachFromCurrentValue();
        tone->detachFromCurrentValue();
        mix->detachFromCurrentValue();
        output->detachFromCurrentValue();
    }

    juce::String getName() const override                   { return getPluginName(); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getSelectableDescription() override        { return getName(); }
    BusLayout getBusses() const override                    { return BusLayout::singlePassThrough(); }

    void initialise (const te::PluginInitialisationInfo& info) override
    {
        frequence = info.sampleRate > 0.0 ? info.sampleRate : 44100.0;
        filtre[0] = filtre[1] = 0.0f;
    }

    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (! isEnabled() || fc.destBuffer == nullptr)
            return;

        const float gain   = juce::Decibels::decibelsToGain (drive->getCurrentValue());
        const float sortie = juce::Decibels::decibelsToGain (output->getCurrentValue());
        const float dose   = mix->getCurrentValue();
        const float coef   = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * tone->getCurrentValue() / (float) frequence);

        for (int canal = 0; canal < juce::jmin (2, fc.destBuffer->getNumChannels()); ++canal)
        {
            auto* d = fc.destBuffer->getWritePointer (canal, fc.bufferStartSample);
            float etat = filtre[canal];

            for (int i = 0; i < fc.bufferNumSamples; ++i)
            {
                const float sec = d[i];
                etat += coef * (std::tanh (sec * gain) - etat);
                d[i] = (sec * (1.0f - dose) + etat * dose) * sortie;
            }

            filtre[canal] = etat;
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, driveValue, toneValue, mixValue, outputValue);

        for (auto p : getAutomatableParameters())
            p->updateFromAttachedValue();
    }

    juce::CachedValue<float> driveValue, toneValue, mixValue, outputValue;
    te::AutomatableParameter::Ptr drive, tone, mix, output;

private:
    double frequence = 44100.0;
    float filtre[2] = { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistorsionQuack)
};

inline const char* DistorsionQuack::xmlTypeName = "quackDistorsion";

//==============================================================================
/** Volume : un simple bouton de niveau (et de gauche/droite) à placer où l'on veut. */
class VolumeQuack : public te::Plugin
{
public:
    static const char* getPluginName()      { return "Volume"; }
    static const char* xmlTypeName;

    VolumeQuack (te::PluginCreationInfo info) : te::Plugin (info)
    {
        auto um = getUndoManager();
        gainValue.referTo (state, "gain", um, 0.0f);
        panValue.referTo  (state, "pan", um, 0.0f);

        gain = addParam ("gain", "Gain", { -60.0f, 24.0f, 0.0f, 2.5f });
        pan  = addParam ("pan",  "Pan",  { -1.0f, 1.0f });

        gain->attachToCurrentValue (gainValue);
        pan->attachToCurrentValue (panValue);
    }

    ~VolumeQuack() override
    {
        notifyListenersOfDeletion();
        gain->detachFromCurrentValue();
        pan->detachFromCurrentValue();
    }

    juce::String getName() const override                   { return getPluginName(); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getSelectableDescription() override        { return getName(); }
    BusLayout getBusses() const override                    { return BusLayout::singlePassThrough(); }

    void initialise (const te::PluginInitialisationInfo&) override     { dernier[0] = dernier[1] = -1.0f; }
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (! isEnabled() || fc.destBuffer == nullptr)
            return;

        const float g = gain->getCurrentValue() <= -59.9f ? 0.0f
                                                          : juce::Decibels::decibelsToGain (gain->getCurrentValue());
        const float p = pan->getCurrentValue();
        const float cible[2] = { g * juce::jmin (1.0f, 1.0f - p), g * juce::jmin (1.0f, 1.0f + p) };

        for (int canal = 0; canal < juce::jmin (2, fc.destBuffer->getNumChannels()); ++canal)
        {
            const float depart = dernier[canal] < 0.0f ? cible[canal] : dernier[canal];
            fc.destBuffer->applyGainRamp (canal, fc.bufferStartSample, fc.bufferNumSamples, depart, cible[canal]);
            dernier[canal] = cible[canal];
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        te::copyPropertiesToCachedValues (v, gainValue, panValue);

        for (auto p : getAutomatableParameters())
            p->updateFromAttachedValue();
    }

    juce::CachedValue<float> gainValue, panValue;
    te::AutomatableParameter::Ptr gain, pan;

private:
    float dernier[2] = { -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VolumeQuack)
};

inline const char* VolumeQuack::xmlTypeName = "quackVolume";

//==============================================================================
/** Compresseur : celui du moteur, auquel on ajoute une mesure de son action
    (de combien il baisse le son à chaque instant) pour pouvoir l'afficher. */
class CompresseurQuack : public te::CompressorPlugin
{
public:
    static const char* getPluginName()      { return "Compressor"; }
    static const char* xmlTypeName;

    CompresseurQuack (te::PluginCreationInfo info) : te::CompressorPlugin (info) {}

    juce::String getPluginType() override   { return xmlTypeName; }

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
            return;

        const int canaux = juce::jmin (2, fc.destBuffer->getNumChannels());
        float avant = 0.0f, apres = 0.0f;

        for (int c = 0; c < canaux; ++c)
            avant = juce::jmax (avant, fc.destBuffer->getMagnitude (c, fc.bufferStartSample, fc.bufferNumSamples));

        te::CompressorPlugin::applyToBuffer (fc);

        for (int c = 0; c < canaux; ++c)
            apres = juce::jmax (apres, fc.destBuffer->getMagnitude (c, fc.bufferStartSample, fc.bufferNumSamples));

        // La réduction = ce qui sort par rapport à ce qui entre, sans compter le gain de sortie
        float reduction = 0.0f;

        if (avant > 0.0005f && isEnabled())
        {
            const float sortie = juce::Decibels::decibelsToGain (outputDb.getCurrentValue());
            reduction = juce::jlimit (-40.0f, 0.0f, juce::Decibels::gainToDecibels (apres / (avant * sortie), -40.0f));
        }

        // Descente rapide, remontée douce : plus lisible à l'écran
        const float precedente = reductionDb.load (std::memory_order_relaxed);
        reductionDb.store (reduction < precedente ? reduction : precedente + (reduction - precedente) * 0.25f,
                           std::memory_order_relaxed);
        niveauEntree.store (avant, std::memory_order_relaxed);
    }

    std::atomic<float> reductionDb { 0.0f };       ///< de combien le son est baissé, en dB (0 = pas du tout)
    std::atomic<float> niveauEntree { 0.0f };      ///< le niveau qui entre (0..1)

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompresseurQuack)
};

inline const char* CompresseurQuack::xmlTypeName = "quackCompresseur";

//==============================================================================
/** Oscilloscope : ne change pas le son, il en garde une copie pour l'afficher en temps réel.
    Il est placé automatiquement juste après le synthé et n'apparaît pas dans la chaîne. */
class OscilloQuack : public te::Plugin
{
public:
    static const char* getPluginName()      { return "Oscilloscope"; }
    static const char* xmlTypeName;
    static constexpr int taille = 4096;

    OscilloQuack (te::PluginCreationInfo info) : te::Plugin (info) {}
    ~OscilloQuack() override                                { notifyListenersOfDeletion(); }

    juce::String getName() const override                   { return getPluginName(); }
    juce::String getPluginType() override                   { return xmlTypeName; }
    juce::String getSelectableDescription() override        { return getName(); }
    BusLayout getBusses() const override                    { return BusLayout::singlePassThrough(); }

    void initialise (const te::PluginInitialisationInfo&) override {}
    void deinitialise() override {}

    void applyToBuffer (const te::PluginRenderContext& fc) override
    {
        if (fc.destBuffer == nullptr || fc.destBuffer->getNumChannels() < 1)
            return;

        const auto* g = fc.destBuffer->getReadPointer (0, fc.bufferStartSample);
        const auto* d = fc.destBuffer->getNumChannels() > 1 ? fc.destBuffer->getReadPointer (1, fc.bufferStartSample) : g;
        int pos = ecriture.load (std::memory_order_relaxed);

        for (int i = 0; i < fc.bufferNumSamples; ++i)
        {
            tampon[(size_t) pos] = 0.5f * (g[i] + d[i]);
            pos = (pos + 1) & (taille - 1);
        }

        ecriture.store (pos, std::memory_order_relaxed);
    }

    /** Copie les `nombre` derniers échantillons (du plus ancien au plus récent). */
    void lire (float* destination, int nombre) const
    {
        const int pos = ecriture.load (std::memory_order_relaxed);

        for (int i = 0; i < nombre; ++i)
            destination[i] = tampon[(size_t) ((pos - nombre + i + taille * 2) & (taille - 1))];
    }

private:
    std::array<float, (size_t) taille> tampon {};
    std::atomic<int> ecriture { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloQuack)
};

inline const char* OscilloQuack::xmlTypeName = "quackOscillo";

//==============================================================================
/** Le catalogue des effets fournis. */
namespace Effets
{
    struct Info
    {
        const char* id;             // identifiant dans le navigateur
        const char* nom;            // nom affiché (comme dans Ableton)
        const char* description;    // explication pour la zone d'aide
        const char* type;           // type de plugin dans le moteur
    };

    inline const std::array<Info, 6>& tous()
    {
        static const std::array<Info, 6> liste {{
            { "fx:eq",     "EQ",         "Égaliseur : monte ou baisse les graves, les médiums et les aigus d'un son.",
              te::EqualiserPlugin::xmlTypeName },
            { "fx:volume", "Volume",     "Un bouton de volume et de gauche/droite, à placer où tu veux dans la chaîne.",
              VolumeQuack::xmlTypeName },
            { "fx:comp",   "Compressor", "Compresseur : rapproche les sons forts et les sons faibles. Avec le sidechain, "
                                         "une autre piste (la grosse caisse par exemple) fait baisser celle-ci : l'effet « pompe ».",
              CompresseurQuack::xmlTypeName },
            { "fx:delay",  "Delay",      "Écho : le son se répète, de moins en moins fort.",
              te::DelayPlugin::xmlTypeName },
            { "fx:reverb", "Reverb",     "Réverbération : donne l'impression de jouer dans une grande salle.",
              te::ReverbPlugin::xmlTypeName },
            { "fx:disto",  "Distortion", "Distorsion : salit et durcit le son, comme une guitare électrique saturée.",
              DistorsionQuack::xmlTypeName }
        }};
        return liste;
    }

    inline const Info* trouver (const juce::String& id)
    {
        for (auto& e : tous())
            if (id == e.id)
                return &e;
        return nullptr;
    }

    /** À appeler une fois au démarrage : déclare nos effets maison au moteur. */
    inline void declarerTypes (te::Engine& engine)
    {
        engine.getPluginManager().createBuiltInType<DistorsionQuack>();
        engine.getPluginManager().createBuiltInType<VolumeQuack>();
        engine.getPluginManager().createBuiltInType<OscilloQuack>();
        engine.getPluginManager().createBuiltInType<CompresseurQuack>();
        engine.getPluginManager().createBuiltInType<SampleurQuack>();
    }

    inline bool estInstrument (te::Plugin& p)       { return Instruments::estUnInstrument (p); }

    inline bool estEffetConnu (te::Plugin& p)
    {
        const auto type = p.getPluginType();
        for (auto& e : tous())
            if (type == e.type)
                return true;

        // Les effets VST
        if (auto* externe = dynamic_cast<te::ExternalPlugin*> (&p))
            return ! externe->isSynth();

        // Les compresseurs des morceaux enregistrés avec une version plus ancienne
        return type == te::CompressorPlugin::xmlTypeName;
    }

    /** Les instruments et effets de la piste, dans l'ordre où le son les traverse. */
    inline juce::Array<te::Plugin*> chaineDe (te::AudioTrack& piste)
    {
        juce::Array<te::Plugin*> chaine;

        for (auto* p : piste.pluginList.getPlugins())
            if (estInstrument (*p) || estEffetConnu (*p))
                chaine.add (p);

        return chaine;
    }

    /** L'oscilloscope placé après l'instrument de la piste ; le crée s'il manque. */
    inline OscilloQuack* oscilloDe (te::AudioTrack& piste, bool creerSiAbsent)
    {
        if (auto* o = piste.pluginList.findFirstPluginOfType<OscilloQuack>())
            return o;

        if (! creerSiAbsent)
            return nullptr;

        int index = 0;
        for (auto* p : piste.pluginList.getPlugins())
        {
            if (estInstrument (*p))
            {
                index = piste.pluginList.indexOf (p) + 1;
                break;
            }
        }

        if (auto plugin = piste.edit.getPluginCache().createNewPlugin (OscilloQuack::xmlTypeName, {}))
        {
            piste.pluginList.insertPlugin (plugin, index, nullptr);
            return dynamic_cast<OscilloQuack*> (plugin.get());
        }

        return nullptr;
    }

    /** Place un oscilloscope derrière chaque synthé du morceau qui n'en a pas encore.
        À faire juste après l'ouverture d'un morceau, pour que le simple fait de regarder
        un synthé ne compte pas comme une modification. */
    inline void preparerOscillos (te::Edit& edit)
    {
        for (auto* piste : te::getAudioTracks (edit))
            if (piste->pluginList.findFirstPluginOfType<te::FourOscPlugin>() != nullptr)
                oscilloDe (*piste, true);
    }

    /** La place, dans la liste complète des plugins de la piste, qui correspond à la position
        `positionDansChaine` de la chaîne affichée (-1 = à la fin, juste avant le volume de la piste). */
    inline int indexReel (te::AudioTrack& piste, int positionDansChaine)
    {
        const auto chaine = chaineDe (piste);

        if (positionDansChaine >= 0 && positionDansChaine < chaine.size())
        {
            // Jamais avant l'instrument (ni avant son oscilloscope)
            int index = piste.pluginList.indexOf (chaine[positionDansChaine]);

            for (auto* p : piste.pluginList.getPlugins())
                if (estInstrument (*p) || dynamic_cast<OscilloQuack*> (p) != nullptr)
                    index = juce::jmax (index, piste.pluginList.indexOf (p) + 1);

            return index;
        }

        if (auto* vol = piste.getVolumePlugin())
            return juce::jmax (0, piste.pluginList.indexOf (vol));

        return piste.pluginList.size();
    }

    /** Ajoute un effet dans la chaîne de la piste, à la position voulue (-1 = à la fin). */
    inline te::Plugin::Ptr ajouter (te::AudioTrack& piste, const juce::String& id, int positionDansChaine = -1)
    {
        te::Plugin::Ptr plugin;

        if (id.startsWith ("vstfx:"))
        {
            plugin = Vst::creer (piste.edit, id);       // un effet VST
        }
        else if (const auto* info = trouver (id))
        {
            plugin = piste.edit.getPluginCache().createNewPlugin (info->type, {});
        }

        if (plugin == nullptr)
            return {};

        const int index = indexReel (piste, positionDansChaine);
        piste.pluginList.insertPlugin (plugin, index, nullptr);

        // Quelques réglages de départ agréables
        if (auto* reverb = dynamic_cast<te::ReverbPlugin*> (plugin.get()))
        {
            reverb->setRoomSize (0.6f);
            reverb->setWet (0.25f);
            reverb->setDry (0.8f);
        }
        else if (auto* delay = dynamic_cast<te::DelayPlugin*> (plugin.get()))
        {
            const double bpm = piste.edit.tempoSequence.getTempo (0)->getBpm();
            delay->lengthMs = juce::roundToInt (60000.0 / bpm * 0.75);     // croche pointée
            delay->mixProportion->setParameter (0.3f, juce::dontSendNotification);
            delay->feedbackDb->setParameter (-8.0f, juce::dontSendNotification);
        }
        else if (auto* comp = dynamic_cast<te::CompressorPlugin*> (plugin.get()))
        {
            comp->thresholdGain.setParameter (juce::Decibels::decibelsToGain (-18.0f), juce::dontSendNotification);
            comp->ratio.setParameter (0.25f, juce::dontSendNotification);       // 4:1
            comp->attackMs.setParameter (10.0f, juce::dontSendNotification);
            comp->releaseMs.setParameter (120.0f, juce::dontSendNotification);
        }

        return plugin;
    }
}

namespace Effets
{
    /** Déplace un effet déjà présent vers une autre position de la chaîne affichée. */
    inline void deplacer (te::AudioTrack& piste, te::Plugin& plugin, int positionDansChaine)
    {
        if (estInstrument (plugin))
            return;

        auto parent = plugin.state.getParent();
        const int ancien = parent.indexOf (plugin.state);
        if (! parent.isValid() || ancien < 0)
            return;

        // La cible est exprimée par rapport à un autre plugin : on la retrouve dans l'arbre d'état
        const auto chaine = chaineDe (piste);
        int cible = -1;

        if (positionDansChaine >= 0 && positionDansChaine < chaine.size())
        {
            if (chaine[positionDansChaine] == &plugin)
                return;

            cible = parent.indexOf (chaine[positionDansChaine]->state);
        }
        else if (auto* vol = piste.getVolumePlugin())
        {
            cible = parent.indexOf (vol->state);
        }

        if (cible < 0)
            cible = parent.getNumChildren();

        // Jamais avant l'instrument ni son oscilloscope
        for (auto* p : piste.pluginList.getPlugins())
            if (estInstrument (*p) || dynamic_cast<OscilloQuack*> (p) != nullptr)
                cible = juce::jmax (cible, parent.indexOf (p->state) + 1);

        if (cible > ancien)
            --cible;        // on retire d'abord l'élément : tout ce qui suit recule d'un cran

        if (cible != ancien)
        {
            parent.moveChild (ancien, cible, &piste.edit.getUndoManager());
            piste.edit.restartPlayback();
        }
    }
}
