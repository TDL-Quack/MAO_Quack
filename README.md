# MAO Quack

Un petit logiciel de musique (MAO) pour débuter, avec une interface proche d'Ableton Live.

## Télécharger

Onglet **Releases** du dépôt → « Dernière version (Windows) » → `MAO-Quack-Windows.zip`.
Décompresser, puis lancer `MAO Quack.exe`.

## Avancement

- [x] Étape 1 : fenêtre, moteur audio, transport, métronome, pistes, zone d'aide
- [ ] Clips et piano roll
- [ ] Instruments et effets intégrés, sons fournis
- [ ] Mixage, automation, export
- [ ] Warping, racks, VST3

## Technique

C++20, [JUCE](https://juce.com) et [Tracktion Engine](https://github.com/Tracktion/tracktion_engine) (licence GPL).
La fabrication est automatique (GitHub Actions) à chaque modification de `main`.
