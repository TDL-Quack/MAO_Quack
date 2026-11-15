# Guide de MAO Quack

MAO Quack est un petit logiciel pour faire de la musique sur ordinateur. Il ressemble volontairement à Ableton Live, en plus simple : ce que tu apprends ici te servira le jour où tu passeras à un logiciel plus gros.

Ce guide a deux parties : **faire son premier morceau**, pas à pas, puis **comprendre Quack Synth**, le synthétiseur fourni.

Un conseil avant de commencer : en bas à gauche de la fenêtre, la **zone d'aide** explique ce qui se trouve sous ta souris. Dans le doute, survole.

[TOC]

---

## Partie 1 : mon premier morceau

### 1. Faire connaissance avec la fenêtre

| Zone | Où | À quoi elle sert |
|---|---|---|
| Barre de menus | tout en haut | Fichier, Édition, Affichage, Options, Aide |
| Barre de transport | sous les menus | tempo, métronome, lecture, stop, position, boucle, automation |
| Navigateur | à gauche | la réserve d'instruments et d'effets |
| Arrangement | au centre | ton morceau, de gauche à droite ; une ligne par piste |
| En-têtes de pistes | à droite de l'arrangement | nom, muet (M), solo (S), volume de chaque piste |
| Zone d'aide | en bas à gauche | l'explication de ce que tu survoles |
| Zone du bas | en bas | les notes du clip choisi, ou l'instrument et les effets de la piste choisie |

Trois mots à connaître :

- Une **piste** est une ligne du morceau. Une **piste MIDI** contient des notes, jouées par un instrument. Une **piste audio** contient des sons enregistrés (boucles, voix, bruitages).
- Un **clip** est un bloc posé sur une piste : un bloc de notes sur une piste MIDI, un son sur une piste audio.
- Le **tempo** est la vitesse du morceau, en battements par minute (BPM).

### 2. Régler le tempo

Dans la barre de transport, glisse sur la case **BPM**. 120 est un bon départ ; autour de 90 pour quelque chose de posé, 140 et plus pour quelque chose de nerveux.

Le bouton rond à côté allume le **métronome**, qui fait « tic » pendant la lecture pour t'aider à rester en rythme.

### 3. Poser une batterie

Un nouveau morceau s'ouvre avec deux pistes MIDI vides et deux pistes audio.

1. Dans le navigateur, ouvre **Instruments**.
2. Glisse **Quack Drums** sur la piste « MIDI 1 » (ou choisis la piste puis double-clique sur Quack Drums). La piste prend le nom de l'instrument.
3. Dans l'arrangement, **double-clique** sur cette piste, au tout début : un clip d'une mesure apparaît.
4. Clique sur le clip : la zone du bas affiche le **piano roll**. Pour une batterie, chaque ligne est un son (grosse caisse, caisse claire, charley…).
5. **Double-clique** dans la grille pour poser une note. Double-clique sur une note pour l'effacer.

Un rythme simple pour commencer (une mesure = 4 temps, chaque temps divisé en 4 cases) :

- **Grosse caisse** : sur les temps 1 et 3.
- **Caisse claire** : sur les temps 2 et 4.
- **Charley fermé** : une note toutes les deux cases.

Appuie sur **Espace** pour écouter, et encore sur Espace pour arrêter.

> **Astuce :** le bouton **Crayon** (touche **B**) change le geste : un simple clic pose une note, un clic sur une note l'efface. Éteins-le pour sélectionner et déplacer.

### 4. Faire tourner le rythme en boucle

Pour travailler, il est pratique que le rythme tourne sans s'arrêter.

- Choisis ton clip et fais **Ctrl+L** : la boucle se cale sur lui et s'allume.
- La boucle est la **barre orange tout en haut de la règle**. Ses deux extrémités sont des curseurs : tire-les séparément pour l'allonger ou la raccourcir. Attrape le milieu pour la déplacer.
- Le bouton **Boucle** de la barre de transport l'allume et l'éteint.

### 5. Ajouter une basse

1. Glisse **Instruments → Quack Synth → Basse** sur la piste « MIDI 2 ».
2. Double-clique sur la piste pour créer un clip, puis clique dessus.
3. Le piano roll affiche maintenant un clavier. Plus haut = plus aigu, plus bas = plus grave. Le nom de la note sous la souris s'affiche.
4. Pose quelques notes graves. Pour allonger une note, tire son bord droit.

Une basse efficace joue souvent la même note que la grosse caisse, sur les mêmes temps.

### 6. Ajouter des accords et une mélodie

Il te faut de nouvelles pistes : sous la dernière piste, clique sur **+ Ajouter une piste**, puis **Piste MIDI**. Glisse ensuite un instrument dessus : **Accords** pour des notes tenues, **Mélodie** pour l'air principal.

Un **accord**, ce sont plusieurs notes jouées en même temps. Pour débuter, pose trois notes l'une au-dessus de l'autre en sautant à chaque fois une touche blanche (par exemple Do, Mi, Sol), et fais-les durer toute la mesure.

Pour la mélodie, reste d'abord sur les touches blanches : tout sonnera juste avec les accords ci-dessus.

### 7. Les gestes qui font gagner du temps

**Dans le piano roll**

| Geste | Effet |
|---|---|
| Glisser dans le vide | entourer plusieurs notes |
| Maj + clic | ajouter une note à la sélection |
| Ctrl+A | tout sélectionner |
| Ctrl+C, Ctrl+X, Ctrl+V | copier, couper, coller (au trait bleu, qu'on place d'un clic) |
| Ctrl+D | dupliquer la sélection juste après |
| Ctrl + glisser | dupliquer les notes en les déplaçant |
| Flèches | déplacer les notes (Maj + haut/bas : une octave) |
| Suppr | effacer |
| Menu de gauche | changer la longueur du clip (1 à 16 mesures) |

**Dans l'arrangement**

| Geste | Effet |
|---|---|
| Glisser un clip | le déplacer, y compris vers une autre piste du même type |
| Ctrl + glisser | le dupliquer |
| Tirer le bord droit | l'allonger ou le raccourcir |
| Tirer le bord gauche | choisir où il commence |
| Ctrl+D | le dupliquer juste après |
| Ctrl+C puis Ctrl+V | le coller au trait de lecture |
| Poser un clip sur un autre | il l'écrase |
| Molette | monter et descendre |
| Maj + molette | avancer et reculer |
| Ctrl + molette | zoomer |

**Partout** : **Ctrl+Z** annule, **Ctrl+Y** rétablit. N'aie pas peur d'essayer.

### 8. Construire le morceau

Un morceau, c'est une histoire : des parties arrivent, d'autres s'en vont.

- Duplique tes clips (Ctrl+D) pour remplir plusieurs mesures.
- Fais entrer les instruments un par un : la batterie seule, puis la basse, puis les accords, puis la mélodie.
- Retire presque tout pendant quelques mesures, puis fais tout revenir : c'est le principe du « break ».
- Pose des **repères** pour t'y retrouver : double-clique dans la règle, sous la barre de boucle, et donne un nom (« Intro », « Refrain »…). On les déplace en les glissant ; clic droit pour les renommer ou les supprimer.

Les boutons **M** (muet) et **S** (solo) de chaque piste servent à écouter une partie sans les autres, ou toute seule. Pense à les éteindre ensuite : un solo oublié s'entend aussi dans l'export.

### 9. Ajouter des sons enregistrés

Sur une **piste audio**, double-clique pour choisir un fichier son, ou glisse un fichier depuis l'explorateur de Windows (WAV, MP3, AIFF, FLAC, OGG).

Clique sur le clip audio : la zone du bas affiche son dessin, son volume, et le bouton **Warp**. Le Warp cale une boucle sur le tempo du morceau. MAO Quack devine le tempo d'origine d'après la durée ; si la boucle ne tombe pas juste, corrige le BPM d'origine, ou essaie les boutons **/2** et **x2**.

### 10. Les effets

Clique sur l'en-tête d'une piste : la zone du bas montre sa **chaîne**, c'est-à-dire son instrument suivi de ses effets. Le son la traverse de gauche à droite, donc **l'ordre compte**.

Pour ajouter un effet, glisse-le depuis **Effets** dans le navigateur, à l'endroit voulu (un trait blanc montre où il se posera), ou clique sur **+ Effet**.

| Effet | Ce qu'il fait | Quand l'utiliser |
|---|---|---|
| EQ | monte ou baisse graves, médiums, aigus | éclaircir un son, enlever du grave inutile |
| Volume | un bouton de niveau et de gauche/droite | équilibrer à un endroit précis de la chaîne |
| Compressor | rapproche les sons forts et les sons faibles | rendre un son plus régulier ; effet « pompe » avec le sidechain |
| Delay | écho | donner du mouvement à une mélodie |
| Reverb | impression de grande salle | donner de l'espace |
| Distortion | salit et durcit | une basse ou un lead plus agressif |

Chaque effet a un bouton **On** pour comparer avec et sans, un bouton **–** pour le réduire en fine bande, et un **X** pour le retirer. Pour le déplacer, glisse sa barre de titre.

**L'effet « pompe » (sidechain)** : mets un Compressor sur la piste d'accords, puis dans sa liste Sidechain choisis la piste de batterie. À chaque coup, les accords baissent un instant. La barre **GR** montre de combien.

### 11. Faire bouger les réglages : l'automation

L'automation fait changer un réglage tout seul pendant le morceau : un volume qui monte, un filtre qui s'ouvre.

1. Clique sur **Automation** dans la barre de transport (touche **A**).
2. Sur chaque piste, un bouton remplace le volume : il choisit **quel réglage** tu dessines (Volume, Pan, ou n'importe quel potard de l'instrument et des effets).
3. Sur la piste, **clique** pour poser un point, **glisse** pour le déplacer, **double-clique** ou clic droit pour l'effacer.
4. Appuie de nouveau sur **A** pour revenir aux clips.

### 12. Enregistrer et exporter

- **Ctrl+S** enregistre ton morceau en fichier `.quack`, dans `Documents\MAO Quack`. Ce fichier ne s'ouvre qu'avec MAO Quack. Enregistre souvent.
- **Fichier → Exporter** (Ctrl+E) fabrique un fichier à faire écouter : coche **WAV** (qualité maximale), **MP3** (plus léger) ou **MIDI** (les notes seulement, à rouvrir dans un autre logiciel). Tu peux en cocher plusieurs, et choisir « une piste par fichier ».

---

## Partie 2 : comprendre Quack Synth

Quack Synth fabrique des sons à partir de rien. Tous les préréglages (Basse, Mélodie, Accords, Cloches, Orgue, Jeu vidéo) sont le même synthétiseur, réglé différemment. Pour apprendre, pars du préréglage **Init** : un son brut, sans filtre ni effet.

Le son suit toujours le même trajet :

**Oscillateurs → Filtre → Effets**

Les oscillateurs fabriquent le son, le filtre le sculpte, les effets le décorent. Trois onglets correspondent à ces trois étapes : **OSC**, **FILTER / MOD**, **FX**.

### La colonne de gauche, toujours visible

- **L'oscilloscope** dessine le son en direct. C'est ton meilleur professeur : change un réglage, joue une note et regarde le dessin se transformer.
- **Level** : le volume général du synthé.
- **Vel** : de combien la force d'une note change son volume.
- **Glide** : en mode Mono ou Legato, le temps pour glisser d'une note à la suivante.
- **Mono / Legato / Poly** : Poly joue plusieurs notes à la fois (accords). Mono n'en joue qu'une (basses, solos). Legato est un Mono où les notes liées ne redémarrent pas l'enveloppe.

Pour entendre le synthé sans lancer le morceau, clique sur une touche du clavier du piano roll.

### Onglet OSC : les quatre oscillateurs

Un oscillateur produit une onde. Sa **forme** donne la couleur du son. Clique sur le petit écran pour la choisir :

| Forme | Son | Pour quoi |
|---|---|---|
| Sine | doux, rond, pur | sous-basses, cloches |
| Triangle | doux avec un peu de brillance | flûtes, sons calmes |
| Saw (scie) | riche, brillant | basses, leads, nappes |
| Square (carré) | creux, façon jeu vidéo | sons rétro, basses |
| Noise | du souffle | vent, percussions |

Chaque oscillateur a les mêmes réglages :

- **Le bouton numéroté** l'allume ou le coupe. Coupe les autres pour écouter ce que chacun apporte.
- **Tune** : la hauteur, en demi-tons. +12 = une octave plus haut, –12 = une octave plus bas.
- **Fine** : un accordage très fin. Deux oscillateurs légèrement décalés donnent un son plus large.
- **Level** : son volume.
- **Width** : pour l'onde carrée seulement, la largeur du carré.
- **Voices** : le nombre de copies jouées ensemble (unisson).
- **Detune** : désaccorde légèrement ces copies. Ne s'entend qu'avec 2 voix ou plus.
- **Spread** : répartit les voix entre la gauche et la droite.
- **Pan** : place l'oscillateur à gauche ou à droite.

**Trois essais à faire depuis Init**

1. *Un son épais* : sur l'oscillateur 1, monte Voices à 4 et Detune vers un quart.
2. *Une basse solide* : allume l'oscillateur 2 en Square, avec Tune à –12.
3. *Un son large* : allume l'oscillateur 2 en Saw, avec Fine très légèrement décalé.

### Onglet FILTER / MOD : sculpter et faire bouger

#### Le filtre

Le filtre enlève une partie du son. Sa courbe montre ce qui passe : graves à gauche, aigus à droite.

- **Type** : *Lowpass* garde les graves et coupe les aigus (le plus utilisé). *Highpass* fait l'inverse. *Bandpass* ne garde qu'une bande au milieu. *Notch* creuse un trou. *Off* : pas de filtre.
- **12 dB / 24 dB** : la pente. 24 dB coupe plus franchement.
- **Freq** : la fréquence de coupure, là où le filtre commence à couper.
- **Res** : la résonance, qui fait « siffler » le son juste à la coupure.
- **Key** : fait suivre le filtre avec la hauteur de la note.

Tu peux aussi tirer directement le point sur la courbe : gauche/droite pour Freq, haut/bas pour Res.

#### Les enveloppes (ENV 1, 2, 3)

Une enveloppe décrit la vie d'une note, en quatre réglages :

- **Attack** : le temps pour arriver au maximum. Court : ça claque. Long : ça gonfle doucement.
- **Decay** : le temps pour redescendre.
- **Sustain** : le niveau tant que la note est tenue. À zéro, le son s'éteint tout seul, comme un piano.
- **Release** : le temps pour s'éteindre quand la note s'arrête.

**ENV 1** règle toujours le **volume** de la note. **ENV 2** et **ENV 3** sont libres : elles ne font rien tant que tu ne les as pas assignées à un potard.

#### Les LFO (LFO 1, 2)

Un LFO est un oscillateur très lent. On ne l'entend pas : il fait bouger un réglage tout seul, en boucle. **Wave** choisit la forme du mouvement, **Rate** sa vitesse, **Depth** son amplitude, **Sync** le cale sur le tempo.

#### Assigner une modulation (comme dans Vital ou Serum)

C'est le geste le plus important du synthé :

1. Ouvre l'onglet **FILTER / MOD** et choisis une source : ENV 2, ENV 3, LFO 1 ou LFO 2.
2. Attrape sa poignée **« Glisser sur un potard »**.
3. Dépose-la sur n'importe quel potard. Pour atteindre un potard d'un autre onglet, passe sur cet onglet pendant le glisser : il s'ouvre.

Sur le potard modulé :

- une **pastille de couleur** apparaît : glisse-la vers le haut ou le bas pour doser (vers le bas : sens inverse) ;
- un **arc** montre jusqu'où le réglage bouge, et un **point blanc** suit la valeur en direct ;
- pour retirer la modulation : **clic droit** sur le potard → « Délier », ou clic droit sur la pastille.

**Quatre recettes classiques**

| Recette | Source | Posée sur | Résultat |
|---|---|---|---|
| Basse qui claque | ENV 2 (Decay court, Sustain bas) | Freq du filtre | le filtre s'ouvre puis se referme à chaque note |
| Wah-wah | LFO 1 (Sine) | Freq du filtre | le son s'ouvre et se ferme en boucle |
| Vibrato | LFO 1 (Sine, rapide, faible dose) | Fine d'un oscillateur | la note tremble légèrement |
| Trémolo | LFO 2 | Level | le volume ondule |

Quand une modulation agit sur le filtre, une **courbe blanche** s'anime sur l'écran du filtre.

### Onglet FX : les effets du synthé

Appliqués tout à la fin, après le filtre, dans cet ordre :

- **Distortion** : salit le son.
- **Chorus** : l'épaissit, comme si plusieurs instruments jouaient ensemble.
- **Delay** : un écho calé sur le tempo.
- **Reverb** : l'impression d'une grande salle.

Chacun s'allume d'un clic sur son nom.

### Enregistrer son propre son

Quand un son te plaît, clique sur **Save** dans la barre de titre du synthé et donne-lui un nom. Tu le retrouves dans la liste des préréglages et dans le navigateur, sous Quack Synth. Clic droit dessus dans le navigateur pour le supprimer.

### Fabriquer une basse de A à Z

1. Glisse **Quack Synth** (le dossier lui-même) sur une piste MIDI : tu obtiens le son **Init**.
2. Onglet **OSC** : oscillateur 1 en Saw. Allume l'oscillateur 2, en Square, **Tune à –12**.
3. Colonne de gauche : passe en **Mono**.
4. Onglet **FILTER / MOD** : filtre **Lowpass**, **Freq** vers le premier tiers, un peu de **Res**.
5. Choisis **ENV 2** : Attack à zéro, Decay court, Sustain bas.
6. Glisse la poignée d'ENV 2 sur **Freq**, puis dose avec la pastille.
7. Joue des notes graves. Clique sur **Save**.

Tu viens de refaire, à peu de chose près, le préréglage Basse.

---

## Les autres instruments, en bref

**Quack Drums** : une grille de 16 pads, un son par pad (C1 en bas à gauche). Clic pour écouter ; clic droit pour renommer, charger un autre son ou vider le pad ; on peut aussi glisser un fichier son sur un pad. **Save** enregistre le kit entier.

**Quack Sampler** : joue un son enregistré comme un instrument. En mode **Clavier**, le son est joué plus aigu ou plus grave selon la touche. En mode **Tranches**, il est découpé en parts égales, une par touche à partir de C1 : idéal pour rejouer une boucle de batterie dans l'ordre que tu veux.

**Plugins VST3** : menu **Options → Plugins VST** pour chercher ceux de ton ordinateur. Ils apparaissent ensuite dans le navigateur ; clic droit pour les ranger dans Instruments ou Effets.

## Où sont mes fichiers ?

Tout est dans `Documents\MAO Quack` :

- tes morceaux (`.quack`) ;
- le dossier `Exports` ;
- le dossier `Bibliothèque` : préréglages, kits, favoris, réglages et journal d'erreurs.

En cas de problème, **Aide → Ouvrir le journal d'erreurs** montre ce que le logiciel a noté.
