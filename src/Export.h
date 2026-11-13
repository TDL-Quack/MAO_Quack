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
        Executeur executeur (tache);

        while (executeur.isThreadRunning())
            if (! juce::MessageManager::getInstance()->runDispatchLoopUntil (10))
                break;
    }

private:
    struct Executeur : public juce::Thread
    {
        Executeur (te::ThreadPoolJobWithProgress& t) : juce::Thread (t.getJobName()), tache (t)
        {
            startThread();
        }

        ~Executeur() override
        {
            tache.signalJobShouldExit();
            waitForThreadToExit (10000);
        }

        void run() override
        {
            while (! threadShouldExit())
                if (tache.runJob() == juce::ThreadPoolJob::jobHasFinished)
                    break;
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
        fichier.deleteFile();

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
        return resultat.existsAsFile() && resultat.getSize() > 44;
    }

   #if QUACK_AVEC_MP3
    inline bool wavVersMp3 (const juce::File& wav, const juce::File& mp3)
    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> lecteur (format.createReaderFor (wav.createInputStream().release(), true));

        if (lecteur == nullptr)
            return false;

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

    /** Lance l'export. `destination` est un fichier (morceau entier) ou un dossier (une piste par fichier).
        Renvoie un message d'erreur, ou une chaîne vide si tout va bien. */
    inline juce::String executer (te::Edit& edit, Format format, bool parPiste, const juce::File& destination,
                                  const juce::String& nomDuMorceau, juce::StringArray& fichiersEcrits)
    {
        const double finEnTemps = finDuMorceauEnTemps (edit);
        if (finEnTemps <= 0.0)
            return fr ("Le morceau est vide : il n'y a rien à exporter.");

        edit.getTransport().stop (false, false);

        // On garde 2 secondes après la dernière note, pour laisser finir les échos et la réverbération
        const auto fin = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (finEnTemps)) + te::TimeDuration::fromSeconds (2.0);
        const te::TimeRange plage (te::TimePosition(), fin);

        const auto pistes = te::getAudioTracks (edit);
        juce::String erreur;

        if (! parPiste)
        {
            if (ecrireFichier (edit, format, pistes, destination, plage))
                fichiersEcrits.add (destination.getFullPathName());
            else
                erreur = fr ("Le fichier n'a pas pu être créé : ") + destination.getFileName();
        }
        else
        {
            destination.createDirectory();
            int numero = 1;

            for (auto* piste : pistes)
            {
                // Les pistes sans aucune note ne donnent pas de fichier
                if (piste->getClips().isEmpty())
                {
                    ++numero;
                    continue;
                }

                const auto nom = juce::File::createLegalFileName (nomDuMorceau + " - " + juce::String (numero).paddedLeft ('0', 2)
                                                                      + " " + piste->getName());
                const auto f = destination.getChildFile (nom + extensionDe (format));

                if (ecrireFichier (edit, format, { piste }, f, plage))
                    fichiersEcrits.add (f.getFullPathName());
                else
                    erreur = fr ("Ce fichier n'a pas pu être créé : ") + f.getFileName();

                ++numero;
            }
        }

        edit.getTransport().ensureContextAllocated();
        return erreur;
    }
}

//==============================================================================
/** La petite fenêtre « Exporter ». */
class DialogueExport : public juce::Component
{
public:
    DialogueExport (te::Edit& e, const juce::String& nom) : edit (e), nomDuMorceau (nom)
    {
        auto preparer = [this] (juce::ToggleButton& b, const juce::String& texte, int groupe, const juce::String& aide)
        {
            addAndMakeVisible (b);
            b.setButtonText (texte);
            b.setRadioGroupId (groupe);
            b.setColour (juce::ToggleButton::textColourId, Couleurs::texte);
            b.setColour (juce::ToggleButton::tickColourId, Couleurs::accent);
            b.setTooltip (aide);
        };

        preparer (wav,  fr ("WAV  -  le son en qualité maximale"), 1, {});
        preparer (mp3,  Export::mp3Disponible() ? fr ("MP3  -  le son en plus léger, facile à partager")
                                                : fr ("MP3  -  pas encore disponible dans cette version"), 1, {});
        preparer (midi, fr ("MIDI  -  seulement les notes, à rouvrir dans Ableton ou un autre logiciel"), 1, {});
        mp3.setEnabled (Export::mp3Disponible());
        wav.setToggleState (true, juce::dontSendNotification);

        preparer (entier,   fr ("Le morceau entier, dans un seul fichier"), 2, {});
        preparer (parPiste, fr ("Une piste par fichier (avec ses effets)"), 2, {});
        entier.setToggleState (true, juce::dontSendNotification);

        addAndMakeVisible (exporter);
        exporter.setButtonText ("Exporter...");
        exporter.setColour (juce::TextButton::buttonColourId, Couleurs::accent);
        exporter.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        exporter.onClick = [this] { choisirDestination(); };

        addAndMakeVisible (message);
        message.setColour (juce::Label::textColourId, Couleurs::texteDoux);
        message.setJustificationType (juce::Justification::topLeft);
        message.setFont (juce::FontOptions (13.0f));

        setSize (460, 330);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Couleurs::panneau);
        g.setColour (Couleurs::accent);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText ("Format", 16, 10, 300, 20, juce::Justification::centredLeft);
        g.drawText ("Contenu", 16, 122, 300, 20, juce::Justification::centredLeft);
    }

    void resized() override
    {
        wav.setBounds (16, 34, getWidth() - 32, 24);
        mp3.setBounds (16, 60, getWidth() - 32, 24);
        midi.setBounds (16, 86, getWidth() - 32, 24);
        entier.setBounds (16, 146, getWidth() - 32, 24);
        parPiste.setBounds (16, 172, getWidth() - 32, 24);
        exporter.setBounds (16, 212, 140, 32);
        message.setBounds (16, 252, getWidth() - 32, getHeight() - 258);
    }

private:
    te::Edit& edit;
    juce::String nomDuMorceau;
    juce::ToggleButton wav, mp3, midi, entier, parPiste;
    juce::TextButton exporter;
    juce::Label message;
    std::shared_ptr<juce::FileChooser> selecteur;

    Export::Format format() const
    {
        return mp3.getToggleState() ? Export::Format::mp3 : midi.getToggleState() ? Export::Format::midi : Export::Format::wav;
    }

    static juce::File dossierExports()
    {
        auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile ("MAO Quack").getChildFile ("Exports");
        d.createDirectory();
        return d;
    }

    void choisirDestination()
    {
        const auto f = format();
        const bool pistes = parPiste.getToggleState();
        const auto ext = Export::extensionDe (f);
        juce::Component::SafePointer<DialogueExport> moi (this);

        if (pistes)
        {
            selecteur = std::make_shared<juce::FileChooser> (fr ("Dans quel dossier ranger les fichiers ?"), dossierExports());
            selecteur->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [moi, f] (const juce::FileChooser& fc)
                                    {
                                        if (moi != nullptr && fc.getResult() != juce::File())
                                            moi->lancer (f, true, fc.getResult());
                                    });
        }
        else
        {
            selecteur = std::make_shared<juce::FileChooser> (fr ("Exporter le morceau"),
                                                             dossierExports().getChildFile (nomDuMorceau + ext), "*" + ext);
            selecteur->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
                                    [moi, f, ext] (const juce::FileChooser& fc)
                                    {
                                        auto fichier = fc.getResult();
                                        if (moi == nullptr || fichier == juce::File())
                                            return;

                                        if (! fichier.hasFileExtension (ext))
                                            fichier = fichier.withFileExtension (ext);

                                        moi->lancer (f, false, fichier);
                                    });
        }
    }

    void lancer (Export::Format f, bool pistes, const juce::File& destination)
    {
        message.setText (fr ("Export en cours..."), juce::dontSendNotification);
        exporter.setEnabled (false);
        juce::Component::SafePointer<DialogueExport> moi (this);

        // On laisse l'écran afficher « en cours » avant de commencer le calcul
        juce::Timer::callAfterDelay (60, [moi, f, pistes, destination]
        {
            if (moi == nullptr)
                return;

            juce::StringArray ecrits;
            auto& morceau = moi->edit;
            const auto nom = moi->nomDuMorceau;

            juce::MouseCursor::showWaitCursor();    // sablier pendant le calcul
            const auto erreur = Export::executer (morceau, f, pistes, destination, nom, ecrits);
            juce::MouseCursor::hideWaitCursor();

            if (moi == nullptr)     // la fenêtre a été fermée pendant l'export
                return;

            moi->exporter.setEnabled (true);

            juce::String texte;
            if (erreur.isNotEmpty())
                texte = erreur;
            else if (ecrits.isEmpty())
                texte = fr ("Aucun fichier créé : les pistes sont vides.");
            else if (ecrits.size() == 1)
                texte = fr ("Terminé ! Fichier créé :\n") + ecrits[0];
            else
                texte = fr ("Terminé ! ") + juce::String (ecrits.size()) + fr (" fichiers créés dans :\n") + destination.getFullPathName();

            moi->message.setText (texte, juce::dontSendNotification);
        });
    }
};
