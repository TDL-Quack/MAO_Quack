#pragma once

#include "Commun.h"

/** Les instruments fournis avec le logiciel : une batterie (sons fabriqués
    par le programme lui-même) et des synthés avec quelques réglages tout prêts. */
namespace Instruments
{
    constexpr int premiereNoteBatterie = 36;

    struct SonBatterie { const char* nom; const char* fichier; };

    inline const std::array<SonBatterie, 8>& sonsBatterie()
    {
        static const std::array<SonBatterie, 8> sons {{
            { "Grosse caisse",  "grosse_caisse.wav" },
            { "Caisse claire",  "caisse_claire.wav" },
            { "Charley fermé",  "charley_ferme.wav" },
            { "Charley ouvert", "charley_ouvert.wav" },
            { "Clap",           "clap.wav" },
            { "Tom grave",      "tom_grave.wav" },
            { "Tom aigu",       "tom_aigu.wav" },
            { "Cymbale",        "cymbale.wav" }
        }};
        return sons;
    }

    //==========================================================================
    inline bool ecrireWav (const juce::File& fichier, const std::vector<float>& echantillons, double frequence)
    {
        fichier.deleteFile();
        std::unique_ptr<juce::OutputStream> flux (fichier.createOutputStream().release());

        if (flux == nullptr)
            return false;

        juce::WavAudioFormat wav;
        auto ecrivain = wav.createWriterFor (flux, juce::AudioFormatWriterOptions()
                                                       .withSampleRate (frequence)
                                                       .withNumChannels (1)
                                                       .withBitsPerSample (16));
        if (ecrivain == nullptr)
            return false;

        juce::AudioBuffer<float> tampon (1, (int) echantillons.size());
        tampon.copyFrom (0, 0, echantillons.data(), (int) echantillons.size());
        return ecrivain->writeFromAudioSampleBuffer (tampon, 0, tampon.getNumSamples());
    }

    /** Fabrique un son de batterie par synthèse. */
    inline std::vector<float> fabriquerSon (int index, double fe)
    {
        const double duree[] = { 0.45, 0.25, 0.07, 0.40, 0.25, 0.40, 0.32, 1.10 };
        const int n = (int) (duree[index] * fe);
        std::vector<float> s ((size_t) n, 0.0f);
        juce::Random alea (1234 + index);
        const double deuxPi = juce::MathConstants<double>::twoPi;

        double phase = 0.0, passeBas = 0.0, passeBas2 = 0.0;

        auto balayage = [&] (double f0, double f1, double vitesse, double chute, double niveau)
        {
            for (int i = 0; i < n; ++i)
            {
                const double t = i / fe;
                const double f = f1 + (f0 - f1) * std::exp (-t * vitesse);
                phase += deuxPi * f / fe;
                s[(size_t) i] += (float) (niveau * std::sin (phase) * std::exp (-t * chute));
            }
        };

        // Bruit filtré passe-haut (coef proche de 0 = bruit complet, proche de 1 = très aigu)
        auto bruit = [&] (double coefAigu, double chute, double niveau, double debut = 0.0)
        {
            for (int i = (int) (debut * fe); i < n; ++i)
            {
                const double t = i / fe - debut;
                const double b = alea.nextDouble() * 2.0 - 1.0;
                passeBas += (1.0 - coefAigu) * (b - passeBas);
                const double aigu = b - passeBas * coefAigu;
                s[(size_t) i] += (float) (niveau * aigu * std::exp (-t * chute));
            }
        };

        switch (index)
        {
            case 0:  balayage (160.0, 45.0, 28.0, 9.0, 0.95);  bruit (0.2, 200.0, 0.25);                 break; // grosse caisse
            case 1:  balayage (240.0, 180.0, 30.0, 22.0, 0.5); bruit (0.6, 20.0, 0.6);                   break; // caisse claire
            case 2:  bruit (0.97, 70.0, 0.55);                                                           break; // charley fermé
            case 3:  bruit (0.97, 9.0, 0.5);                                                             break; // charley ouvert
            case 4:  bruit (0.8, 160.0, 0.6); bruit (0.8, 160.0, 0.6, 0.012);
                     bruit (0.8, 160.0, 0.6, 0.024); bruit (0.8, 18.0, 0.5, 0.036);                      break; // clap
            case 5:  balayage (150.0, 85.0, 12.0, 9.0, 0.85);                                            break; // tom grave
            case 6:  balayage (250.0, 150.0, 12.0, 11.0, 0.8);                                           break; // tom aigu
            case 7:
            {
                bruit (0.96, 3.5, 0.4);
                const double partiels[] = { 3140.0, 4321.0, 5587.0, 6843.0, 8210.0 };
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / fe;
                    double m = 0.0;
                    for (double f : partiels)
                        m += std::sin (deuxPi * f * t) > 0.0 ? 1.0 : -1.0;
                    passeBas2 += 0.3 * (m - passeBas2);
                    s[(size_t) i] += (float) (0.05 * (m - passeBas2) * std::exp (-t * 4.0));
                }
                break;
            }
            default: break;
        }

        // Petit fondu à la fin pour éviter un « clic », et limitation douce
        const int fondu = juce::jmin (n, (int) (0.01 * fe));
        for (int i = 0; i < fondu; ++i)
            s[(size_t) (n - 1 - i)] *= (float) i / (float) fondu;

        for (auto& v : s)
            v = std::tanh (v * 1.2f) * 0.9f;

        return s;
    }

    /** Le dossier des sons de batterie ; les fabrique au premier lancement. */
    inline juce::File preparerSonsBatterie()
    {
        auto dossier = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("MAO Quack").getChildFile ("Sons").getChildFile ("Batterie v1");
        dossier.createDirectory();

        int i = 0;
        for (auto& son : sonsBatterie())
        {
            auto f = dossier.getChildFile (son.fichier);
            if (! f.existsAsFile())
                ecrireWav (f, fabriquerSon (i, 44100.0), 44100.0);
            ++i;
        }

        return dossier;
    }

    //==========================================================================
    inline void installerBatterie (te::AudioTrack& piste)
    {
        auto& edit = piste.edit;
        auto dossier = preparerSonsBatterie();

        if (auto plugin = edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}))
        {
            piste.pluginList.insertPlugin (plugin, 0, nullptr);

            if (auto* sampler = dynamic_cast<te::SamplerPlugin*> (plugin.get()))
            {
                int i = 0;
                for (auto& son : sonsBatterie())
                {
                    auto f = dossier.getChildFile (son.fichier);
                    const int note = premiereNoteBatterie + i;

                    if (sampler->addSound (f.getFullPathName(), fr (son.nom), 0.0, 0.0, 0.0f).isEmpty())
                    {
                        const int idx = sampler->getNumSounds() - 1;
                        sampler->setSoundParams (idx, note, note, note);
                        sampler->setSoundOpenEnded (idx, true);
                    }
                    ++i;
                }
            }
        }
    }

    enum class Type { batterie, basse, melodie, accords, cloches, orgue, jeuVideo };

    inline const std::array<Type, 7>& tous()
    {
        static const std::array<Type, 7> t {{ Type::batterie, Type::basse, Type::melodie, Type::accords,
                                              Type::cloches, Type::orgue, Type::jeuVideo }};
        return t;
    }

    inline juce::String nomDe (Type t)
    {
        switch (t)
        {
            case Type::batterie:    return "Batterie";
            case Type::basse:       return "Basse";
            case Type::melodie:     return fr ("Mélodie");
            case Type::accords:     return "Accords";
            case Type::cloches:     return "Cloches";
            case Type::orgue:       return "Orgue";
            case Type::jeuVideo:    return fr ("Jeu vidéo");
        }
        return {};
    }

    inline void installerSynthe (te::AudioTrack& piste, Type type)
    {
        auto plugin = piste.edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {});
        if (plugin == nullptr)
            return;

        piste.pluginList.insertPlugin (plugin, 0, nullptr);

        auto* s = dynamic_cast<te::FourOscPlugin*> (plugin.get());
        if (s == nullptr)
            return;

        auto regler = [] (te::AutomatableParameter::Ptr& p, float v)
        {
            if (p != nullptr)
                p->setParameter (v, juce::dontSendNotification);
        };

        enum { aucune = 0, sinus, carre, scie, triangle };
        auto& o1 = *s->oscParams[0];
        auto& o2 = *s->oscParams[1];

        s->filterTypeValue = 1;     // passe-bas
        s->ampAnalogValue = true;

        switch (type)
        {
            case Type::basse:
                o1.waveShapeValue = scie;
                o2.waveShapeValue = carre;
                regler (o2.tune, -12.0f);
                regler (o2.level, -6.0f);
                regler (s->filterFreq, 62.0f);
                regler (s->filterAmount, 0.35f);
                regler (s->filterDecay, 0.25f);
                regler (s->filterSustain, 20.0f);
                regler (s->filterResonance, 20.0f);
                regler (s->ampAttack, 0.003f);
                regler (s->ampDecay, 0.3f);
                regler (s->ampSustain, 75.0f);
                regler (s->ampRelease, 0.08f);
                regler (s->masterLevel, -9.0f);
                break;

            case Type::melodie:
                o1.waveShapeValue = scie;
                o1.voicesValue = 2;
                regler (o1.detune, 0.12f);
                regler (o1.spread, 50.0f);
                o2.waveShapeValue = carre;
                regler (o2.level, -14.0f);
                regler (s->filterFreq, 98.0f);
                regler (s->ampAttack, 0.004f);
                regler (s->ampDecay, 0.4f);
                regler (s->ampSustain, 70.0f);
                regler (s->ampRelease, 0.25f);
                s->reverbOnValue = true;
                regler (s->reverbMix, 0.18f);
                regler (s->masterLevel, -13.0f);
                break;

            case Type::accords:
                o1.waveShapeValue = scie;
                o1.voicesValue = 3;
                regler (o1.detune, 0.2f);
                regler (o1.spread, 80.0f);
                o2.waveShapeValue = triangle;
                regler (o2.level, -6.0f);
                regler (s->filterFreq, 86.0f);
                regler (s->ampAttack, 0.12f);
                regler (s->ampDecay, 0.5f);
                regler (s->ampSustain, 80.0f);
                regler (s->ampRelease, 0.6f);
                s->chorusOnValue = true;
                s->reverbOnValue = true;
                regler (s->reverbMix, 0.25f);
                regler (s->masterLevel, -16.0f);
                break;

            case Type::cloches:
                o1.waveShapeValue = sinus;
                o2.waveShapeValue = sinus;
                regler (o2.tune, 19.0f);
                regler (o2.level, -12.0f);
                s->filterTypeValue = 0;
                regler (s->ampAttack, 0.002f);
                regler (s->ampDecay, 1.2f);
                regler (s->ampSustain, 0.0f);
                regler (s->ampRelease, 0.8f);
                s->reverbOnValue = true;
                regler (s->reverbMix, 0.3f);
                regler (s->masterLevel, -10.0f);
                break;

            case Type::orgue:
                o1.waveShapeValue = sinus;
                o2.waveShapeValue = sinus;
                regler (o2.tune, 12.0f);
                regler (o2.level, -6.0f);
                s->oscParams[2]->waveShapeValue = triangle;
                regler (s->oscParams[2]->tune, 19.0f);
                regler (s->oscParams[2]->level, -14.0f);
                s->filterTypeValue = 0;
                regler (s->ampAttack, 0.01f);
                regler (s->ampDecay, 0.1f);
                regler (s->ampSustain, 100.0f);
                regler (s->ampRelease, 0.08f);
                s->chorusOnValue = true;
                regler (s->masterLevel, -13.0f);
                break;

            case Type::jeuVideo:
                o1.waveShapeValue = carre;
                regler (o1.pulseWidth, 0.25f);
                s->filterTypeValue = 0;
                s->ampAnalogValue = false;
                regler (s->ampAttack, 0.001f);
                regler (s->ampDecay, 0.15f);
                regler (s->ampSustain, 60.0f);
                regler (s->ampRelease, 0.03f);
                regler (s->masterLevel, -16.0f);
                break;

            case Type::batterie:
                break;
        }
    }

    /** Met l'instrument choisi sur la piste (et retire l'ancien s'il y en avait un). */
    inline void installer (te::AudioTrack& piste, Type type)
    {
        for (auto* p : piste.pluginList.getPlugins())
            if (dynamic_cast<te::SamplerPlugin*> (p) != nullptr || dynamic_cast<te::FourOscPlugin*> (p) != nullptr)
                p->deleteFromParent();

        if (type == Type::batterie)
            installerBatterie (piste);
        else
            installerSynthe (piste, type);
    }

    /** Vrai si la piste joue la batterie. */
    inline te::SamplerPlugin* batterieDe (te::AudioTrack& piste)
    {
        return piste.pluginList.findFirstPluginOfType<te::SamplerPlugin>();
    }
}
