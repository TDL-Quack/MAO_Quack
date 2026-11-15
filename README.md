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
- [x] Quack Sampler (clavier ou tranches), sons personnels dans Quack Drums, warp des clips audio
- [x] Démo au premier lancement, icône, bibliothèque dans Documents/MAO Quack
- [x] Pistes et clips audio, boucle, repères
- [x] Plugins VST3 (recherche, catégories, fenêtre du plugin), automations
- [x] Version Linux (AppImage)
- [x] Version 1.0 : Quack Drums en grille de 16 pads avec kits, point d'entrée des clips, guide (docs/Guide.md), installeur Windows

## Technique

C++20, [JUCE](https://juce.com) et [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) (licence GPL).
La fabrication est automatique (GitHub Actions) à chaque modification de `main`.

## À faire (1.1)

- [ ] Copier-coller des chaînes d'effets d'une piste à l'autre

## Licence

MAO Quack est un logiciel libre, distribué sous licence **GNU AGPL v3** (voir `LICENSE.md`), comme JUCE sur lequel il repose. Il utilise aussi Tracktion Engine (GPL v3) et, pour l'export MP3, l'encodeur LAME (LGPL).

