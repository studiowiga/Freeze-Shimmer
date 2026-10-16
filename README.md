# Freeze Shimmer — VST3 (Windows 11)

Plugin d'effet **VST3** « Frozen Cosmic Reverb » par **Studio Wiga**, qui réunit dans une seule interface :

- un **shimmer / cosmic reverb** inspiré de la *Pico Shimmer* (3 scènes, retard, modulation) ;
- un **freeze / sound retainer** inspiré de la *Pico Deep Freeze* (sustain infini, couches, glissando).

L'interface reprend votre visuel `proto_freeze_shimmer.png` (fond sombre, jacks et mentions **OUTPUT** à gauche / **INPUT** à droite conservés) ; les 7 boutons sont redessinés en vectoriel (code), et une ligne d'état discrète est
placée sous l'encadré « FREEZE SHIMMER ».

> **Précision importante.** Les algorithmes des pédales Electro-Harmonix sont propriétaires, et le schéma fourni
> décrit le câblage matériel (3 potentiomètres, interrupteurs SPDT, footswitch, LED, entrée/sortie), pas l'algorithme audio.
> Les moteurs de ce plugin sont donc **écrits from scratch** pour reproduire le *comportement* décrit dans les manuels
> (fonctions des boutons, modes, interactions), pas une copie bit-à-bit du son. Projet indépendant, non affilié à
> Electro-Harmonix (noms de pédales cités à titre de référence).

---

## Installation (utilisateur)

1. Récupérez `FreezeShimmer-win64.zip` dans **Actions → dernier build → Artifacts** (ou dans **Releases** si vous avez poussé un tag `vX.Y.Z`).
2. Dézippez, puis clic droit sur `Install-FreezeShimmer.ps1` → *Exécuter avec PowerShell* (droits admin demandés).
   Le script copie `Freeze Shimmer.vst3` dans `C:\Program Files\Common Files\VST3`.
3. Relancez le scan des plugins de votre DAW.

Un exécutable autonome (`Freeze Shimmer.exe`) est aussi fourni pour essayer le plugin sans DAW.

## Compiler soi-même

Prérequis : Windows 11, Visual Studio 2022 (charge de travail *Développement Desktop C++*), CMake ≥ 3.22, Git.

```powershell
git clone <votre-repo> FreezeShimmer
cd FreezeShimmer
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
# -> build\FreezeShimmer_artefacts\Release\VST3\Freeze Shimmer.vst3
```

JUCE 8.0.12 est téléchargé automatiquement par CMake. Option `-DFS_COPY_AFTER_BUILD=ON` pour copier le VST3 après compilation
(terminal administrateur requis).

**Via GitHub** : poussez le dépôt ; le workflow `.github/workflows/build.yml` compile le VST3 sur `windows-2022`, lance les tests DSP
sous Linux et publie le zip. Un tag `v1.0.0` crée une *Release* avec le zip.

---

## Commandes

| Bouton | Page A (étiquette blanche) | Page B (étiquette bleue) |
|---|---|---|
| haut gauche | **DRY** — niveau du signal direct | idem |
| haut droite | **EFFECT** — niveau du son figé | idem |
| gauche 1 | **BLEND** — dry/wet du shimmer | **RVB MIX** — mix réverb (0 = retard seul) |
| centre | **TIME** — decay, **infini** au maximum | **DELAY** — retard 0–950 ms (0 = off) |
| droite 1 | **SPEED** — attaque/relâchement du freeze | **LAYER** — nombre de couches (1–4, Latch) |
| gauche 2 | **VOICE** — plate → shimmer → strings | **DEPTH** — profondeur de modulation |
| droite 2 | **TONE** — luminosité du wet | **RATE** — vitesse de modulation |
| droite 2 (menu) | **GLISS** — fondu entre deux freezes (clic droit sur le bouton → *GLISS*) | |

- **Bouton MODE** (haut) : clic = scène suivante (**INTERGALACTIC** vert · **OFF-WORLD** orange · **ETHERDUST** rouge) ;
  `Maj`+clic = mode freeze suivant (**LATCH · MOMENT · AUTO**) ; maintien ~1 s = bascule page A/B ;
  **molette** = preset suivant / précédent ; clic droit = menu complet (presets, taille, scène, mode, page, bypass, canal MIDI, table MIDI).
- **LED unique** (au-dessus de MODE) : éteinte en bypass · **bleu fixe** pédale active · **bleu clignotant** freeze actif.
- **Footswitch** (bas) : **appui maintenu** = freeze · **double-clic** = arrête le freeze (voir ci-dessous). Bypass : clic droit → menu, ou CC 105.
- **Double-clic** sur un bouton = valeur par défaut. Glisser verticalement, ou molette.

### Scènes (shimmer)

- **INTERGALACTIC** : shimmer classique (octave +12 dans la boucle de réverb), VOICE morphe *plate → shimmer → strings*.
- **OFF-WORLD** : shimmer plus doux/sombre avec modulation renforcée ; VOICE en moitié haute = textures de synthé vintage (octaves désaccordées).
- **ETHERDUST** : shimmer « glitch » à temps de retard aléatoires (de scintillements granulaires à des échos distincts) ; RVB MIX bas = plus de stutter.

### Freeze (footswitch)

Mode **LATCH** (par défaut) :
1. **Maintenez** le footswitch : le plugin fige en continu ce qu'il entend (accord, note, nappe…).
2. **Relâchez** : le son figé est maintenu à l'infini, vous jouez par-dessus (le signal direct passe normalement, niveau DRY).
3. **Double-clic** sur le footswitch : le freeze s'arrête (fondu réglé par SPEED).
Un nouvel appui ajoute une couche (jusqu'à LAYER = 4) ou glisse vers le nouveau son (GLISS).

Autres modes (`Maj`+clic sur MODE) :
- **MOMENT** : le son est figé seulement tant que l'on maintient le footswitch.
- **AUTO** : capture automatique à chaque nouvelle attaque détectée ; GLISS règle le morphing d'une note à la suivante. Un clic sur le footswitch arme / désarme l'auto-capture.

SPEED = attaque / relâchement du freeze · LAYER = nombre de couches · GLISS = durée du fondu entre deux freezes.

Chaîne audio : `entrée → Shimmer (BLEND) → Freeze (capture post-shimmer) → DRY × signal + EFFECT × son figé → sortie`.
En bypass, les queues de réverb s'éteignent naturellement.

### Ligne d'état

Sous l'encadré : dernier paramètre touché et sa valeur lisible (%, s, ms, Hz…), état *scène · mode · page · FROZEN*,
et à droite la valeur **MIDI** correspondante (`CC24 = 70`). Elle passe en vert quand la valeur vient d'un message MIDI entrant.

---

## MIDI

Tous les paramètres sont aussi **automatisables** depuis le DAW (utile car certains hôtes n'envoient pas le MIDI aux effets —
voir *MIDI learn* / mapping de paramètres de votre DAW). Canal réglable (menu clic droit, **OMNI** par défaut).

| CC | Fonction | CC | Fonction |
|---|---|---|---|
| 20 | DRY | 28 | VOICE |
| 21 | EFFECT | 29 | DEPTH |
| 22 | BLEND | 30 | TONE |
| 23 | RVB MIX | 31 | RATE |
| 24 | TIME | 32 | GLISS |
| 25 | DELAY | 102 | Scène (0-42 / 43-85 / 86-127) |
| 26 | SPEED | 103 | Mode freeze LATCH / MOMENT / AUTO |
| 27 | LAYER | 104 · **64** | Footswitch (≥ 64 enfoncé) — 64 = pédale de sustain |
| 105 | Effet actif ≥ 64 / bypass | 106 | Relâche le freeze |

*Program Change* 0 à 11 = preset d'usine.

---

## Presets d'usine (12)

Preset par défaut à l'ouverture d'une nouvelle instance : **CLEAN PLATE**.

Molette sur le bouton MODE, ou clic droit → *Presets d'usine*, ou *Program Change* MIDI / liste de programmes de l'hôte.

| # | Nom | Caractère |
|---|---|---|
| 1 | GLACIAL PAD | shimmer long, nappe cristalline |
| 2 | AURORA STRINGS | octaves + cordes éthérées |
| 3 | CLEAN PLATE | plate courte et propre, sans shimmer |
| 4 | INFINITE COSMOS | tank infini, shimmer qui n'en finit pas |
| 5 | OFF-WORLD SYNTH | textures de synthé vintage, modulation lente |
| 6 | TAPE ECHO SHIMMER | écho modulé + shimmer |
| 7 | ETHERDUST GLITCH | stutters aléatoires scintillants |
| 8 | FROZEN DRONE | freeze 4 couches, drone qui glisse |
| 9 | AUTO FREEZE SWELL | freeze automatique, enchaînement doux |
| 10 | DARK CATHEDRAL | cathédrale sombre et très longue |
| 11 | SPARKLE DELAY | delay brillant, shimmer aigu |
| 12 | MOMENT RISER | freeze tant que l'on appuie, montée rapide |

---

## Fenêtre

- Ouverture à **50 %** (493 px de haut ; la largeur suit le ratio du visuel, 50 px de fond sombre de chaque côté de la pédale). Clic droit sur le bouton MODE ou le footswitch → *Taille de la fenêtre* : 50 %, 75 %, 100 % (986 px).
- Redimensionnable à la souris par le coin, de 50 % (taille minimale = taille par défaut) à 150 % ; la taille est **mémorisée dans le projet**.

---

## Structure du dépôt

```
CMakeLists.txt
Assets/background.jpg            visuel Studio Wiga (recadré, fond sombre, couleurs rehaussées, INPUT/OUTPUT)
Source/
  PluginProcessor.*              audio, MIDI, paramètres
  PluginEditor.*                 interface : boutons vectoriels, MODE, footswitch, ligne d'état
  Params.h  Presets.h            paramètres, table MIDI, 12 presets d'usine
  dsp/                           moteurs DSP sans dépendance JUCE
    ShimmerEngine.h  Reverb.h  PitchShifter.h   (shimmer, FDN 8 lignes, octaves)
    FreezeEngine.h   FFT.h                      (freeze spectral à conservation de hauteur)
tests/dsp_test.cpp               tests automatiques du DSP (génère des WAV d'écoute)
scripts/Install-FreezeShimmer.ps1
.github/workflows/build.yml      CI : tests + build VST3 Windows + release
```

Tests DSP en local (Linux/macOS/WSL) : `g++ -O2 -std=c++17 -ISource/dsp tests/dsp_test.cpp -o dsp_test && ./dsp_test .`

## Notes techniques

- **Freeze** : vocodeur de phase figé (FFT 4096, recouvrement 75 %). À la capture on mesure magnitude **et** avance de phase de chaque bin,
  donc la hauteur est conservée (testé : accord La/Do♯/Mi figé sans dérive). Sustain stable, resynthèse en boucle, fondus pilotés par SPEED/GLISS.
- **Shimmer** : réverb FDN 8 lignes (matrice de Householder) avec diffusion d'entrée, amortissement, modulation ; l'octave supérieure est
  réinjectée dans la boucle (pitch shifter granulaire). Gain de boucle automatiquement réduit quand TIME augmente (pas d'auto-oscillation) ; TIME max = tank figé.
- Latence : aucune déclarée (le freeze met 1-3 trames de 21 ms à atteindre son plein niveau).
- Interface redimensionnable à ratio fixe (≈ 0,66).

## Licence

JUCE est utilisé sous licence **AGPLv3/GPLv3** : ce projet est distribué sous **GPL-3.0** (voir `LICENSE`).
Le visuel « Freeze Shimmer / Studio Wiga » reste la propriété de son auteur.
