#pragma once

#include "Commun.h"
#include "Instruments.h"

#if QUACK_AVEC_MP3
 #include <lame/lame.h>
#endif

//==============================================================================
/** Indique au moteur comment exécuter ses longs calculs (le rendu audio) :
    dans un fil séparé, pendant que la fenêtre continue de répondre. */
class ComportementQuack : public te::UIBehaviour
{
public:
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& tache) override
    {
        Journal::ecrire ("Tache longue : debut (" + tache.getJobName() + ")");
        Executeur executeur (tache);

        // On laisse la fenêtre vivre pendant le calcul : le moteur a besoin d'elle pour finir proprement
        while (executeur.isThreadRunning())
            juce::MessageManager::getInstance()->runDispatchLoopUntil (15);

        Journal::ecrire ("Tache longue : fin");
    }

    void showWarningMessage (const juce::String& message) override
    {
        dernierMessage() = message;
        Journal::ecrire ("Avertissement du moteur : " + message);
    }

    void showWarningAlert (const juce::String& titre, const juce::String& message) override
    {
        dernierMessage() = titre + " - " + message;
        Journal::ecrire ("Alerte du moteur : " + titre + " - " + message);
    }

    /** Le dernier avertissement émis par le moteur audio (vide s'il n'y en a pas eu). */
    static juce::String& dernierMessage()
    {
        static juce::String m;
        return m;
    }

private:
    struct Executeur : public juce::Thread
    {
        Executeur (te::ThreadPoolJobWithProgress& t) : juce::Thread ("Rendu MAO Quack"), tache (t)
        {
            startThread();
        }

        ~Executeur() override
        {
            // Normalement le fil est déjà terminé ; sinon on l'attend sans bloquer la fenêtre
            while (isThreadRunning())
                juce::MessageManager::getInstance()->runDispatchLoopUntil (15);
        }

        void run() override
        {
            try
            {
                while (tache.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
                {}
            }
            catch (const std::exception& e)
            {
                Journal::ecrire (juce::String ("Exception pendant le rendu : ") + e.what());
            }
            catch (...)
            {
                Journal::ecrire ("Exception inconnue pendant le rendu");
            }
        }

        te::ThreadPoolJobWithProgress& tache;
    };
};

/** Export du morceau : WAV, MP3 ou MIDI, en un seul fichier ou une piste par fichier. */
namespace Export
{
    enum class Format { wav, mp3, midi };

    inline bool mp3Disponible()
    {
       #if QUACK_AVEC_MP3
        return true;
       #else
        return false;
       #endif
    }

    inline juce::String extensionDe (Format f)
    {
        switch (f)
        {
            case Format::wav:   return ".wav";
            case Format::mp3:   return ".mp3";
            case Format::midi:  return ".mid";
        }
        return {};
    }

    /** La fin du dernier clip, en temps (battements). */
    inline double finDuMorceauEnTemps (te::Edit& edit)
    {
        double fin = 0.0;

        for (auto* piste : te::getAudioTracks (edit))
            for (auto* clip : piste->getClips())
                fin = juce::jmax (fin, edit.tempoSequence.toBeats (clip->getPosition().time).getEnd().inBeats());

        return fin;
    }

    //==========================================================================
    inline bool rendreWav (te::Edit& edit, const juce::Array<te::Track*>& pistes, const juce::File& fichier, te::TimeRange plage)
    {
        Journal::ecrire ("Rendu WAV vers " + fichier.getFullPathName());
        fichier.getParentDirectory().createDirectory();
        fichier.deleteFile();
        ComportementQuack::dernierMessage() = {};

        te::Renderer::Parameters p (edit);
        p.destFile = fichier;
        p.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
        p.bitDepth = 16;
        p.sampleRateForAudio = 44100.0;
        p.blockSizeForAudio = 512;
        p.time = plage;
        p.tracksToDo = te::toBitSet (pistes);
        p.usePlugins = true;
        p.useMasterPlugins = true;
        p.canRenderInMono = false;
        p.checkNodesForAudio = false;

        const auto resultat = te::Renderer::renderToFile ("Export", p);

        // Ce qui compte, c'est que le fichier soit bien là et qu'il contienne du son
        const bool ok = fichier.existsAsFile() && fichier.getSize() > 1024;

        Journal::ecrire ("Rendu WAV termine : retour moteur = " + juce::String (resultat.existsAsFile() ? "fichier" : "vide")
                            + ", fichier present = " + juce::String (fichier.existsAsFile() ? "oui" : "non")
                            + ", taille = " + juce::String (fichier.getSize())
                            + (ComportementQuack::dernierMessage().isNotEmpty() ? ", message = " + ComportementQuack::dernierMessage()
                                                                                : juce::String()));
        return ok;
    }

   #if QUACK_AVEC_MP3
    inline bool wavVersMp3 (const juce::File& wav, const juce::File& mp3)
    {
        juce::WavAudioFormat format;
        Journal::ecrire ("Conversion MP3 vers " + mp3.getFullPathName());
        std::unique_ptr<juce::AudioFormatReader> lecteur (format.createReaderFor (wav.createInputStream().release(), true));

        if (lecteur == nullptr)
        {
            Journal::ecrire ("Conversion MP3 : impossible de relire le WAV temporaire");
            return false;
        }

        auto* lame = lame_init();
        if (lame == nullptr)
            return false;

        lame_set_in_samplerate (lame, (int) lecteur->sampleRate);
        lame_set_num_channels (lame, 2);
        lame_set_brate (lame, 192);
        lame_set_quality (lame, 2);

        if (lame_init_params (lame) < 0)
        {
            lame_close (lame);
            return false;
        }

        mp3.deleteFile();
        juce::FileOutputStream sortie (mp3);
        bool ok = sortie.openedOk();

        const int bloc = 4096;
        juce::AudioBuffer<float> tampon (2, bloc);
        std::vector<unsigned char> code ((size_t) (bloc * 5 / 4 + 7200));

        for (juce::int64 position = 0; ok && position < lecteur->lengthInSamples; position += bloc)
        {
            const int n = (int) juce::jmin ((juce::int64) bloc, lecteur->lengthInSamples - position);
            tampon.clear();
            lecteur->read (&tampon, 0, n, position, true, true);

            const int ecrits = lame_encode_buffer_ieee_float (lame, tampon.getReadPointer (0), tampon.getReadPointer (1),
                                                              n, code.data(), (int) code.size());
            ok = ecrits >= 0 && (ecrits == 0 || sortie.write (code.data(), (size_t) ecrits));
        }

        if (ok)
        {
            const int ecrits = lame_encode_flush (lame, code.data(), (int) code.size());
            ok = ecrits >= 0 && (ecrits == 0 || sortie.write (code.data(), (size_t) ecrits));
        }

        lame_close (lame);
        sortie.flush();
        Journal::ecrire (juce::String ("Conversion MP3 terminee : ") + (ok ? "ok" : "echec"));
        return ok;
    }
   #endif

    //==========================================================================
    inline bool ecrireMidi (te::Edit& edit, const juce::Array<te::AudioTrack*>& pistes, const juce::File& fichier)
    {
        const int ticks = 960;
        const double bpm = edit.tempoSequence.getTempo (0)->getBpm();

        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (ticks);

        juce::MidiMessageSequence tempo;
        tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        tempo.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), 0.0);
        midi.addTrack (tempo);

        for (auto* piste : pistes)
        {
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, piste->getName()), 0.0);

            const int canal = Instruments::batterieDe (*piste) != nullptr ? 10 : 1;

            for (auto* clip : piste->getClips())
            {
                auto* mc = dynamic_cast<te::MidiClip*> (clip);
                if (mc == nullptr)
                    continue;

                const auto tempsClip = edit.tempoSequence.toBeats (mc->getPosition().time);
                const double debutClip = tempsClip.getStart().inBeats();
                const double longueur = tempsClip.getLength().inBeats();

                for (auto* n : mc->getSequence().getNotes())
                {
                    const double debut = n->getStartBeat().inBeats();
                    const double fin = juce::jmin (longueur, n->getEndBeat().inBeats());

                    if (debut < 0.0 || debut >= longueur || fin <= debut)
                        continue;

                    seq.addEvent (juce::MidiMessage::noteOn (canal, n->getNoteNumber(), (juce::uint8) juce::jlimit (1, 127, n->getVelocity())),
                                  (debutClip + debut) * ticks);
                    seq.addEvent (juce::MidiMessage::noteOff (canal, n->getNoteNumber()), (debutClip + fin) * ticks);
                }
            }

            seq.updateMatchedPairs();
            midi.addTrack (seq);
        }

        fichier.deleteFile();
        juce::FileOutputStream sortie (fichier);
        return sortie.openedOk() && midi.writeTo (sortie, 1);
    }

    //==========================================================================
    /** Écrit un fichier pour les pistes données. Renvoie vrai si tout s'est bien passé. */
    inline bool ecrireFichier (te::Edit& edit, Format format, const juce::Array<te::AudioTrack*>& pistes,
                               const juce::File& fichier, te::TimeRange plage)
    {
        if (format == Format::midi)
            return ecrireMidi (edit, pistes, fichier);

        juce::Array<te::Track*> aRendre;
        for (auto* p : pistes)
            aRendre.add (p);

        if (format == Format::wav)
            return rendreWav (edit, aRendre, fichier, plage);

       #if QUACK_AVEC_MP3
        auto temporaire = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("MAOQuack").getChildFile ("export_temporaire.wav");
        temporaire.getParentDirectory().createDirectory();

        const bool ok = rendreWav (edit, aRendre, temporaire, plage) && wavVersMp3 (temporaire, fichier);
        temporaire.deleteFile();
        return ok;
       #else
        return false;
       #endif
    }

    /** Lance l'export de tous les formats demandés dans le dossier choisi.
        Renvoie un message d'erreur, ou une chaîne vide si tout va bien. */
    /** Vrai pendant qu'un export est en cours (pour ne jamais en lancer deux à la fois). */
    inline bool& enCours()
    {
        static bool b = false;
        return b;
    }

    inline juce::String executer (te::Edit& edit, const juce::Array<Format>& formats, bool parPiste, const juce::File& dossier,
                                  const juce::String& nomDuMorceau, juce::StringArray& fichiersEcrits)
    {
        if (enCours())
        {
            Journal::ecrire ("Export : deja en cours, demande ignoree");
            return fr ("Un export est déjà en cours.");
        }

        const juce::ScopedValueSetter<bool> garde (enCours(), true);
        Journal::ecrire ("Export : debut, dossier = " + dossier.getFullPathName() + ", par piste = " + juce::String (parPiste ? "oui" : "non"));

        const double finEnTemps = finDuMorceauEnTemps (edit);
        if (finEnTemps <= 0.0)
            return fr ("Le morceau est vide : il n'y a rien à exporter.");

        if (! dossier.createDirectory())
            return fr ("Impossible de créer ou d'ouvrir ce dossier : ") + dossier.getFullPathName();

        edit.getTransport().stop (false, false);

        // On garde 2 secondes après la dernière note, pour laisser finir les échos et la réverbération
        const auto fin = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (finEnTemps)) + te::TimeDuration::fromSeconds (2.0);
        const te::TimeRange plage (te::TimePosition(), fin);

        const auto pistes = te::getAudioTracks (edit);
        const auto base = juce::File::createLegalFileName (nomDuMorceau);
        juce::StringArray erreurs;

        auto ecrireUn = [&] (Format format, const juce::Array<te::AudioTrack*>& lesPistes, const juce::File& f)
        {
            if (ecrireFichier (edit, format, lesPistes, f, plage))
            {
                fichiersEcrits.add (f.getFullPathName());
            }
            else
            {
                auto e = fr ("Échec : ") + f.getFileName();
                if (ComportementQuack::dernierMessage().isNotEmpty())
                    e += " (" + ComportementQuack::dernierMessage() + ")";
                erreurs.add (e);
            }
        };

        for (auto format : formats)
        {
            if (! parPiste)
            {
                ecrireUn (format, pistes, dossier.getChildFile (base + extensionDe (format)));
                continue;
            }

            int numero = 1;
            for (auto* piste : pistes)
            {
                // Les pistes sans aucun clip ne donnent pas de fichier
                if (! piste->getClips().isEmpty())
                {
                    const auto nom = juce::File::createLegalFileName (base + " - " + juce::String (numero).paddedLeft ('0', 2)
                                                                          + " " + piste->getName());
                    ecrireUn (format, { piste }, dossier.getChildFile (nom + extensionDe (format)));
                }

                ++numero;
            }
        }

        Journal::ecrire ("Export : reallocation de la lecture");
        edit.getTransport().ensureContextAllocated();
        Journal::ecrire ("Export : fin, " + juce::String (fichiersEcrits.size()) + " fichier(s), " + juce::String (erreurs.size()) + " erreur(s)");

        return erreurs.joinIntoString ("\n");
    }
}

//==============================================================================
/** La petite fenêtre « Exporter ». */
class DialogueExport : public juce::Component
{
public:
    DialogueExport (te::Edit& e, const juce::String& nom) : edit (e), nomDuMorceau (nom)
    {
        auto preparer = [this] (juce::ToggleButton& b, const juce::String& texte, int groupe)
        {
            addAndMakeVisible (b);
            b.setButtonText (texte);
            b.setRadioGroupId (groupe);
            b.setColour (juce::ToggleButton::textColourId, Couleurs::texte);
            b.setColour (juce::ToggleButton::tickColourId, Couleurs::accent);
            b.setColour (juce::ToggleButton::tickDisabledColourId, Couleurs::texteDoux);
        };

        // Les formats se cochent librement : on peut en exporter plusieurs d'un coup
        preparer (wav,  fr ("WAV  -  le son en qualité maximale"), 0);
        preparer (mp3,  Export::mp3Disponible() ? fr ("MP3  -  le son en plus léger, facile à partager")
                                                : fr ("MP3  -  pas disponible dans cette version"), 0);
        preparer (midi, fr ("MIDI  -  seulement les notes, à rouvrir dans Ableton ou un autre logiciel"), 0);
        mp3.setEnabled (Export::mp3Disponible());
        wav.setToggleState (true, juce::dontSendNotification);

        preparer (entier,   fr ("Le morceau entier (un fichier par format)"), 2);
        preparer (parPiste, fr ("Une piste par fichier (avec ses effets)"), 2);
        entier.setToggleState (true, juce::dontSendNotification);

        addAndMakeVisible (nomFichier);
        nomFichier.setText (nom, juce::dontSendNotification);
        nomFichier.setFont (juce::FontOptions (14.0f));
        nomFichier.setSelectAllWhenFocused (true);

        addAndMakeVisible (exporter);
        exporter.setButtonText ("Exporter...");
        exporter.setColour (juce::TextButton::buttonColourId, Couleurs::accent);
        exporter.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        exporter.onClick = [this] { choisirDossier(); };

        addAndMakeVisible (message);
        message.setColour (juce::Label::textColourId, Couleurs::texte);
        message.setJustificationType (juce::Justification::topLeft);
        message.setFont (juce::FontOptions (13.0f));
        message.setMinimumHorizontalScale (1.0f);

        setSize (500, 460);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);
        g.setColour (Couleurs::accent);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (fr ("Formats (tu peux en cocher plusieurs)"), 16, 10, 400, 20, juce::Justification::centredLeft);
        g.drawText ("Contenu", 16, 122, 300, 20, juce::Justification::centredLeft);
        g.drawText ("Nom du fichier", 16, 206, 300, 20, juce::Justification::centredLeft);
    }

    void resized() override
    {
        wav.setBounds (16, 34, getWidth() - 32, 24);
        mp3.setBounds (16, 60, getWidth() - 32, 24);
        midi.setBounds (16, 86, getWidth() - 32, 24);
        entier.setBounds (16, 146, getWidth() - 32, 24);
        parPiste.setBounds (16, 172, getWidth() - 32, 24);
        nomFichier.setBounds (16, 230, getWidth() - 32, 26);
        exporter.setBounds (16, 272, 140, 32);
        message.setBounds (16, 314, getWidth() - 32, getHeight() - 322);
    }

private:
    te::Edit& edit;
    juce::String nomDuMorceau;
    juce::ToggleButton wav, mp3, midi, entier, parPiste;
    juce::TextButton exporter;
    juce::TextEditor nomFichier;
    juce::Label message;
    std::shared_ptr<juce::FileChooser> selecteur;

    static juce::File dossierExports()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("MAO Quack").getChildFile ("Exports");
        d.createDirectory();
        return d;
    }

    void choisirDossier()
    {
        juce::Array<Export::Format> formats;
        if (wav.getToggleState())    formats.add (Export::Format::wav);
        if (mp3.getToggleState())    formats.add (Export::Format::mp3);
        if (midi.getToggleState())   formats.add (Export::Format::midi);

        if (formats.isEmpty())
        {
            message.setText (fr ("Coche au moins un format."), juce::dontSendNotification);
            return;
        }

        const bool pistes = parPiste.getToggleState();
        juce::Component::SafePointer<DialogueExport> moi (this);

        selecteur = std::make_shared<juce::FileChooser> (fr ("Dans quel dossier ranger les fichiers ?"), dossierExports());
        selecteur->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                [moi, formats, pistes] (const juce::FileChooser& fc)
                                {
                                    if (moi != nullptr && fc.getResult() != juce::File())
                                        moi->lancer (formats, pistes, fc.getResult());
                                });
    }

    void lancer (juce::Array<Export::Format> formats, bool pistes, const juce::File& dossier)
    {
        if (Export::enCours())
            return;

        auto nom = juce::File::createLegalFileName (nomFichier.getText().trim());
        if (nom.isEmpty())
            nom = nomDuMorceau;

        message.setText (fr ("Export en cours... ne touche à rien pendant le calcul."), juce::dontSendNotification);
        exporter.setEnabled (false);
        juce::Component::SafePointer<DialogueExport> moi (this);

        // Un message unique : il laisse l'écran afficher « en cours » avant le calcul, et ne peut pas se répéter
        juce::MessageManager::callAsync ([moi, formats, pistes, dossier, nom]
        {
            if (moi == nullptr)
                return;

            juce::StringArray ecrits;
            auto& morceau = moi->edit;

            const auto erreur = Export::executer (morceau, formats, pistes, dossier, nom, ecrits);

            if (moi == nullptr)     // la fenêtre a été fermée pendant l'export
                return;

            moi->exporter.setEnabled (true);

            juce::String texte;
            if (ecrits.size() == 1)
                texte = fr ("Terminé ! Fichier créé :\n") + ecrits[0];
            else if (ecrits.size() > 1)
                texte = fr ("Terminé ! ") + juce::String (ecrits.size()) + fr (" fichiers créés dans :\n") + dossier.getFullPathName();
            else if (erreur.isEmpty())
                texte = fr ("Aucun fichier créé : les pistes sont vides.");

            if (erreur.isNotEmpty())
                texte += (texte.isEmpty() ? "" : "\n\n") + erreur;

            moi->message.setText (texte, juce::dontSendNotification);
        });
    }
};
