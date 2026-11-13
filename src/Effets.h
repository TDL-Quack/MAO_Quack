#pragma once

#include "Commun.h"

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
              te::CompressorPlugin::xmlTypeName },
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
    }

    inline bool estInstrument (te::Plugin& p)
    {
        return dynamic_cast<te::SamplerPlugin*> (&p) != nullptr || dynamic_cast<te::FourOscPlugin*> (&p) != nullptr;
    }

    inline bool estEffetConnu (te::Plugin& p)
    {
        const auto type = p.getPluginType();
        for (auto& e : tous())
            if (type == e.type)
                return true;
        return false;
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

    /** Ajoute un effet à la fin de la chaîne de la piste (juste avant son volume). */
    inline te::Plugin::Ptr ajouter (te::AudioTrack& piste, const juce::String& id)
    {
        const auto* info = trouver (id);
        if (info == nullptr)
            return {};

        auto plugin = piste.edit.getPluginCache().createNewPlugin (info->type, {});
        if (plugin == nullptr)
            return {};

        int index = piste.pluginList.size();
        if (auto* vol = piste.getVolumePlugin())
            index = juce::jmax (0, piste.pluginList.indexOf (vol));

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
