# MAO Quack

Un petit logiciel de musique (MAO) pour débuter, avec une interface proche d'Ableton Live.

## Télécharger

Onglet **Releases** du dépôt → « Dernière version (Windows) » → `MAO-Quack-Windows.zip`.
Décompresser, puis lancer `MAO Quack.exe`.

## Avancement

- [x] Étape 1 : fenêtre, moteur audio, transport, métronome, pistes, zone d'aide
- [x] Étape 2 : clips, piano roll, batterie et synthés intégrés
- [x] Sélection multiple et copier-coller des notes
- [x] Enregistrer / ouvrir un morceau (.quack), ajouter et supprimer des pistes, 7 instruments
- [x] Synthétiseur complet avec préréglages, effets (EQ, Volume, Compressor sidechain, Delay, Reverb, Distortion)
- [x] Navigateur en arborescence avec favoris, export WAV / MP3 / MIDI (plusieurs formats à la fois)
- [x] Synthé à onglets avec oscilloscope et modulations par glisser-déposer, barre de menus, périphériques réductibles et déplaçables
- [ ] Sampler, chargement de sons dans le Drum Rack
- [ ] VST3, automations
- [ ] Mixage, automation, export
- [ ] Warping, racks, VST3

## Technique

C++20, [JUCE](https://juce.com) et [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) (licence GPL).
La fabrication est automatique (GitHub Actions) à chaque modification de `main`.
