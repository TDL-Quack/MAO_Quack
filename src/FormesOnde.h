#pragma once

#include "Commun.h"

/** Le dessin (forme d'onde) des fichiers son, calculé une fois puis gardé en mémoire. */
namespace FormesOnde
{
    struct Forme
    {
        std::vector<float> cretes;      // le niveau maximal de chaque petite tranche de son
        double duree = 0.0;             // en secondes
        static constexpr double parSeconde = 200.0;

        /** Le niveau maximal entre deux instants (en secondes). */
        float crete (double debut, double fin) const
        {
            if (cretes.empty())
                return 0.0f;

            const int i1 = juce::jlimit (0, (int) cretes.size() - 1, (int) (debut * parSeconde));
            const int i2 = juce::jlimit (i1, (int) cretes.size() - 1, (int) (fin * parSeconde));
            float m = 0.0f;

            for (int i = i1; i <= i2; ++i)
                m = juce::jmax (m, cretes[(size_t) i]);

            return m;
        }
    };

    inline juce::AudioFormatManager& formats()
    {
        static juce::AudioFormatManager m;
        static bool pret = false;

        if (! pret)
        {
            m.registerBasicFormats();
            pret = true;
        }

        return m;
    }

    inline bool estFichierSon (const juce::File& f)
    {
        return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
    }

    inline const Forme& obtenir (const juce::File& fichier)
    {
        static std::map<juce::String, Forme> cache;
        const auto cle = fichier.getFullPathName();

        if (auto it = cache.find (cle); it != cache.end())
            return it->second;

        Forme forme;

        if (std::unique_ptr<juce::AudioFormatReader> lecteur { formats().createReaderFor (fichier) })
        {
            forme.duree = (double) lecteur->lengthInSamples / lecteur->sampleRate;

            const int tranche = juce::jmax (1, (int) (lecteur->sampleRate / Forme::parSeconde));
            const int parBloc = tranche * 256;
            juce::AudioBuffer<float> tampon ((int) juce::jmax (1u, lecteur->numChannels), parBloc);
            forme.cretes.reserve ((size_t) (lecteur->lengthInSamples / tranche + 1));

            for (juce::int64 position = 0; position < lecteur->lengthInSamples; position += parBloc)
            {
                const int n = (int) juce::jmin ((juce::int64) parBloc, lecteur->lengthInSamples - position);
                lecteur->read (&tampon, 0, n, position, true, true);

                for (int debut = 0; debut < n; debut += tranche)
                {
                    float m = 0.0f;
                    for (int canal = 0; canal < tampon.getNumChannels(); ++canal)
                        m = juce::jmax (m, tampon.getMagnitude (canal, debut, juce::jmin (tranche, n - debut)));

                    forme.cretes.push_back (m);
                }
            }
        }

        return cache.emplace (cle, std::move (forme)).first->second;
    }
}

//==============================================================================
/** Les repères posés dans la règle (« Couplet », « Refrain »...), enregistrés avec le morceau. */
namespace Reperes
{
    inline juce::ValueTree arbre (te::Edit& edit)
    {
        return edit.state.getOrCreateChildWithName ("QUACKREPERES", nullptr);
    }

    inline int nombre (te::Edit& edit)                          { return arbre (edit).getNumChildren(); }
    inline double temps (te::Edit& edit, int i)                 { return (double) arbre (edit).getChild (i).getProperty ("temps", 0.0); }
    inline juce::String nom (te::Edit& edit, int i)             { return arbre (edit).getChild (i).getProperty ("nom").toString(); }

    inline void ajouter (te::Edit& edit, double tempsEnBattements, const juce::String& nomRepere)
    {
        juce::ValueTree r ("REPERE");
        r.setProperty ("temps", tempsEnBattements, nullptr);
        r.setProperty ("nom", nomRepere, nullptr);
        arbre (edit).appendChild (r, &edit.getUndoManager());
    }

    inline void deplacer (te::Edit& edit, int i, double tempsEnBattements)
    {
        arbre (edit).getChild (i).setProperty ("temps", tempsEnBattements, &edit.getUndoManager());
    }

    inline void renommer (te::Edit& edit, int i, const juce::String& n)
    {
        arbre (edit).getChild (i).setProperty ("nom", n, &edit.getUndoManager());
    }

    inline void supprimer (te::Edit& edit, int i)
    {
        arbre (edit).removeChild (i, &edit.getUndoManager());
    }
}
