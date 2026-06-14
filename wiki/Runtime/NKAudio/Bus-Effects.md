# Bus de mixage, effets DSP et HRTF

> Couche **Runtime** · NKAudio · Router le son par **bus hiérarchiques** (`NkAudioBus`),
> le colorer par une chaîne d'**effets DSP** (`DelayEffect`, `ReverbEffect`, `CompressorEffect`,
> `LimiterEffect`, `BiquadFilter`, `Eq3BandEffect`, `DistortionEffect`, `ChorusEffect`), et le
> spatialiser par un **dataset HRTF** (`NkHrtfDataset`).

Dès qu'un jeu dépasse le « je joue un wav », on se heurte à trois questions distinctes. **Où**
va le son — comment regrouper la musique, les SFX, les voix pour les régler ensemble ? **Comment**
sonne-t-il — comment lui ajouter de l'écho, le compresser, l'égaliser ? Et **d'où** vient-il —
comment faire qu'un pas dans le dos s'entende vraiment derrière l'auditeur ? Cette page couvre ces
trois étages : le **routage** (bus), le **traitement** (effets), et la **spatialisation** (HRTF).
Ce n'est **pas** la lecture d'un son ni la gestion des voix — c'est la tuyauterie qui relie une voix
aux haut-parleurs.

Tout ce qui suit vit sur le **thread audio** : un bus mixe ses voix et ses sous-buses, applique sa
chaîne d'effets, puis remonte le résultat à son parent jusqu'au master, le tout dans le rappel
temps réel du périphérique. La règle qui en découle est dure : pas d'allocation dans la boucle
chaude, pas d'exception, et toute la mémoire passe par un `memory::NkAllocator*` (optionnel,
`nullptr` = défaut) qu'on fournit **à la construction**.

- **Namespace** : `nkentseu::audio`
- **Headers réels** : `#include "NKAudio/NkAudioBus.h"`, `#include "NKAudio/NkAudioEffects.h"`,
  `#include "NKAudio/NkHrtfDataset.h"`
- **Dépendance** : `IAudioEffect` et l'enum `AudioEffectType` sont déclarés ailleurs (via
  `NKAudio/NKAudio.h`), pas dans ces trois headers — ils sont seulement implémentés/référencés ici.

---

## Le routage : `NkAudioBus`

Un `NkAudioBus` est un **point de regroupement** : on n'y joue pas un son, on y fait *transiter*
le son d'autres sources pour les régler d'un seul geste. C'est le modèle d'une table de mixage,
celui de FMOD et de Wwise. On construit une **hiérarchie** — un bus « SFX », un bus « Musique »,
un bus « Voix », tous enfants d'un bus « Master » — et chaque bus applique son volume, son mute,
ses effets, puis pousse son résultat vers son parent. Baisser le bus « Musique » baisse toute la
musique d'un coup ; couper le « Master » coupe tout.

La hiérarchie est **bornée** (allocation statique, pas de surprise temps réel) : `MAX_CHILDREN = 16`
sous-buses par parent, `MAX_EFFECTS = 8` effets par bus, `MAX_NAME_LEN = 32` caractères de nom.
On donne un nom et un parent optionnel à la construction (`NkAudioBus("SFX", master)`), on navigue
par `GetParent()`, `GetChildCount()`, `GetChild(i)`, et on retrouve un bus n'importe où dans la
descendance par `FindDescendant("Footsteps")`. Le bus est **non copiable** (copie et affectation
`= delete`) : un bus est une identité unique dans le graphe, pas une valeur qu'on duplique.

Le **volume** se règle par bus (`SetVolume`/`GetVolume`), mais ce qu'on entend réellement est le
**volume effectif** : `GetEffectiveVolume()` remonte la chaîne des parents et multiplie les volumes
locaux jusqu'au master. C'est cette valeur qui s'applique au mixage. À noter — et c'est un piège —
cette formule ne tient compte **que** des volumes : `SetMute`/`SetSolo` existent comme drapeaux
(`IsMuted`/`IsSoloed`) mais n'entrent **pas** dans le calcul documenté de `GetEffectiveVolume()`.

> **En résumé.** `NkAudioBus` = un nœud de routage hiérarchique borné (16 enfants, 8 effets, nom de
> 32 car.), non copiable. Le volume *effectif* est le produit des volumes locaux jusqu'au master.
> On regroupe les sons par bus pour les régler ensemble, pas pour les jouer.

### Sidechain : le ducking automatique

Le **sidechain** répond à un besoin de mixage classique : faire que la musique **baisse toute seule**
quand une voix parle, puis remonte quand elle se tait. On le déclare en une ligne sur le bus à
atténuer : `musicBus->SetSidechainFromBus(dialogueBus, amount, threshold)`. Le bus « Musique » sera
alors *ducké* dès que le bus « Dialogue » dépasse un certain niveau. `amount` ∈ [0..1] dose
l'atténuation (1 = silence total quand la source est active), et `threshold` est le niveau RMS de la
source au-delà duquel le ducking se déclenche (défauts `0.7f` / `0.05f`). `ClearSidechain()` annule
le lien, `GetSidechainSource()` le relit.

La détection s'appuie sur le **compteur de voix actives** du bus source : un `NkAtomic<int32>` mis à
jour par `IncrementActiveVoices`/`DecrementActiveVoices` (appelés par le moteur), lu par
`GetActiveVoiceCount()` et `HasActiveVoice()`. C'est ce compteur atomique — sûr entre threads — qui
permet au thread audio de savoir « est-ce que ce bus joue quelque chose en ce moment ? ».

> **En résumé.** Le sidechain atténue un bus quand un autre joue (musique qui s'efface sous la voix).
> `amount` = profondeur du ducking, `threshold` = sensibilité RMS. La détection repose sur un
> compteur de voix actives atomique, thread-safe.

### Les effets sur un bus

Un bus porte une **chaîne d'effets** appliquée à son mix : `AddEffect(effect)` ajoute en fin de
chaîne (et renvoie `false` si les `MAX_EFFECTS` sont déjà pris), `RemoveEffect` retire,
`ClearEffects` vide, `GetEffectCount` compte. Point capital : le bus prend un `IAudioEffect*`
**brut, non possédé** — c'est l'appelant qui gère la durée de vie de l'effet (il doit rester vivant
tant qu'il est attaché). L'ensemble — mixage des voix et des sous-buses, application des effets,
écriture dans le buffer de sortie — se fait dans `ProcessChain(outBuf, frames, channels)`, la
méthode « internal » appelée par le moteur depuis le thread audio.

---

## Le traitement : les effets DSP

Tous les effets de `NkAudioEffects.h` partagent la **même interface** (`IAudioEffect`), ce qui les
rend interchangeables dans une chaîne de bus. Chacun implémente quatre points : `Process(buffer,
frameCount, channels)` qui traite l'audio **en place**, `Reset()` qui purge l'état interne (queues
de délai, enveloppes), `OnSampleRateChanged(sampleRate)` qu'il faut appeler quand le moteur change
de fréquence (recalcul des coefficients, réallocation des buffers), et `GetType()` qui renvoie un
`AudioEffectType`. C'est ce contrat uniforme qui permet d'écrire `bus->AddEffect(monEffet)` sans
savoir lequel c'est.

Trois familles se distinguent par leur **gestion mémoire** : ceux qui allouent un buffer interne et
ont donc un destructeur custom **et** acceptent un allocateur — `DelayEffect`, `ReverbEffect`,
`ChorusEffect` ; et ceux qui n'ont **ni** buffer, **ni** destructeur custom, **ni** allocateur —
`CompressorEffect`, `LimiterEffect`, `BiquadFilter`, `Eq3BandEffect`, `DistortionEffect`. Les effets
à paramètres exposent une struct imbriquée `Params` (un agrégat copiable, à valeurs par défaut) plus
un constructeur par défaut qui délègue avec ces valeurs : on peut donc faire `ReverbEffect()` puis
ajuster, ou construire d'emblée avec une `Params` remplie.

> **En résumé.** Tous les effets partagent `Process`/`Reset`/`OnSampleRateChanged`/`GetType` et sont
> interchangeables dans une chaîne. `DelayEffect`, `ReverbEffect`, `ChorusEffect` allouent (destructeur
> + allocateur) ; les autres non. Pensez à propager `OnSampleRateChanged` quand la fréquence change.

> **Piège à retenir.** `LimiterEffect::GetType()` renvoie `AudioEffectType::COMPRESSOR` — il n'existe
> pas de valeur `LIMITER`. Ne distinguez jamais un limiter d'un compresseur par leur `GetType()`.

---

## La spatialisation : `NkHrtfDataset`

Le **HRTF** (*Head-Related Transfer Function*) est ce qui transforme un son « stéréo » en son
**3D crédible au casque** : le cerveau localise une source d'après les minuscules différences de
temps et de filtrage entre les deux oreilles (l'ombre de la tête, le pavillon). `NkHrtfDataset`
stocke ces réponses impulsionnelles (HRIR) pour un quadrillage de directions : on lui demande la
paire d'IR la plus proche d'un azimut/élévation, et on convolue le son avec — il « se met » à cet
endroit dans l'espace.

Le dataset est **non copiable** (un gros buffer, pas une valeur), borné par `MAX_AZIMUTHS = 72`
(résolution ~5°), `MAX_ELEVATIONS = 14` (-40° à 90° par 10°) et `MAX_IR_LENGTH = 512`. Deux façons
de le remplir : charger un fichier `.nkhrtf` mesuré (`LoadFromFile`), ou **générer** un dataset
synthétique (`CreateSynthetic`) à partir d'un modèle de sphère — pas de fichier requis, idéal pour
démarrer ou pour une plateforme sans assets HRTF.

> **En résumé.** `NkHrtfDataset` = la table des réponses impulsionnelles binaurales qui spatialisent
> un son au casque. Chargeable depuis `.nkhrtf` ou générable synthétiquement (modèle de sphère).
> Non copiable, dimensions bornées.

---

## Aperçu de l'API

### `NkAudioBus` — bus de routage hiérarchique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `MAX_EFFECTS=8`, `MAX_CHILDREN=16`, `MAX_NAME_LEN=32` | Bornes statiques (effets / sous-buses / longueur de nom). |
| Construction | `NkAudioBus(name, parent=nullptr)`, `~NkAudioBus()` | Crée un bus, parent optionnel. Non copiable (`= delete`). |
| Identité / hiérarchie | `GetName`, `GetParent`, `GetChildCount`, `GetChild(idx)`, `FindDescendant(name)` | Nom, parent, enfants, recherche récursive par nom. |
| Volume / état | `SetVolume`/`GetVolume`, `GetEffectiveVolume`, `SetMute`/`IsMuted`, `SetSolo`/`IsSoloed` | Volume local, volume effectif (produit jusqu'au master), mute, solo. |
| Effets | `AddEffect`, `RemoveEffect`, `ClearEffects`, `GetEffectCount` | Gérer la chaîne DSP (effets `IAudioEffect*` non possédés). |
| Sidechain | `SetSidechainFromBus(src, amount=0.7, threshold=0.05)`, `ClearSidechain`, `GetSidechainSource` | Ducking : atténuer ce bus quand `src` joue. |
| Internal | `ProcessChain`, `AttachChild`, `IncrementActiveVoices`, `DecrementActiveVoices`, `GetActiveVoiceCount`, `HasActiveVoice` | Appelés par le moteur depuis le thread audio. |

### Effets DSP — interface commune (tous)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Traitement | `Process(buffer, frameCount, channels)` | Traite l'audio **en place**. |
| État | `Reset()` | Purge l'état interne. |
| Fréquence | `OnSampleRateChanged(sampleRate)` | Recalcule coefficients / réalloue buffers. |
| Type | `GetType() → AudioEffectType` | Identifie l'effet. |

### Effets DSP — types concrets

| Effet | Construction notable | `GetType()` | Réglages / particularités |
|-------|----------------------|-------------|---------------------------|
| `DelayEffect` | `(delayTime, feedback, wet, sampleRate, alloc=nullptr)` | `DELAY` | `SetDelayTime`, `SetFeedback`. Buffer alloué (destructeur). |
| `ReverbEffect` | `()` / `(Params, sr=48000, alloc=nullptr)` | `REVERB` | `Params{roomSize,damping,wet,preDelay,diffusion}`, `SetParams`. Buffer alloué. |
| `CompressorEffect` | `()` / `(Params, sr=48000)` | `COMPRESSOR` | `Params{threshold,ratio,attackMs,releaseMs,makeupGain,softKnee}`, `SetParams`, `GetCurrentGainReduction`. |
| `LimiterEffect` | `()` / `(Params, sr=48000)` | `COMPRESSOR` ⚠️ | `Params{thresholdDb,attackMs,releaseMs}`, `SetParams` (noexcept), `GetParams`, `GetCurrentGainReductionDb`. |
| `BiquadFilter` | `(type, cutoffHz, q=0.707, gainDb=0, sr=48000)` | (hors-ligne) | enum `FilterType`, `SetCutoff`/`SetQ`/`SetGain`/`SetType`. |
| `Eq3BandEffect` | `()` / `(Params, sr=48000)` | `EQ_3BAND` | `Params{low/mid/highGainDb, low/mid/highFreq, midQ}`, `SetParams`. 3 biquads. |
| `DistortionEffect` | `(type=SOFT, drive=5, output=0.5)` | `DISTORTION` | enum `DistortionType`, `SetDrive`/`SetOutput`. `Reset`/`OnSampleRateChanged` no-op. |
| `ChorusEffect` | `(rate=0.5, depth=0.003, feedback=0.3, wet=0.5, sr=48000, alloc=nullptr)` | `CHORUS` | Aucun setter public. Buffer alloué. |

### Enums imbriqués

| Enum (scope) | Valeurs |
|--------------|---------|
| `BiquadFilter::FilterType` | `LOW_PASS, HIGH_PASS, BAND_PASS, NOTCH, PEAK_EQ, LOW_SHELF, HIGH_SHELF, ALL_PASS` (0..7) |
| `DistortionEffect::DistortionType` | `SOFT, HARD, FUZZ, OVERDRIVE` (0..3) |

### `NkHrtfDataset` et `NkHrirPair` — spatialisation HRTF

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Paire HRIR | `NkHrirPair{ leftIR, rightIR, length }` | Réponses gauche/droite (pointeurs **non possédés**) + longueur. |
| Constantes | `MAX_AZIMUTHS=72`, `MAX_ELEVATIONS=14`, `MAX_IR_LENGTH=512` | Bornes du quadrillage. |
| Construction | `NkHrtfDataset()`, `~NkHrtfDataset()` | Vide par défaut. Non copiable (`= delete`). |
| Chargement | `LoadFromFile(path, alloc=nullptr)`, `Unload`, `CreateSynthetic(sr, irLen, nAz, nElev, alloc=nullptr)` | Charger `.nkhrtf`, libérer, ou générer un dataset synthétique. |
| Lookup | `GetHRIR(azimuthDeg, elevationDeg) → NkHrirPair` | Plus proche voisin (azimut 0=devant/90=droite, élévation -40..90). |
| Lecture | `IsLoaded`, `GetSampleRate`, `GetIrLength`, `GetAzimuthCount`, `GetElevationCount` | État et dimensions. |

---

## Référence complète

### `NkAudioBus` à fond

**La hiérarchie est l'idée centrale.** Un bus ne sert à rien seul : sa valeur vient du graphe qu'on
bâtit avec lui. Le schéma typique est un arbre — un master en racine, sous lui des bus de catégorie
(« Musique », « SFX », « Voix », « Ambiance »), et sous certains des sous-bus plus fins (« SFX/Armes »,
« SFX/Pas »). Chaque bus n'a qu'à connaître son **parent** (passé au constructeur) ; le moteur attache
l'enfant côté parent via `AttachChild` (appelé par la fabrique de bus du moteur, pas par vous
directement). On retrouve un bus profond sans tenir tout le chemin grâce à `FindDescendant`, qui
descend récursivement par nom.

- **Mixage / production** — regrouper pour régler d'un geste : un slider d'options « Musique » pilote
  `musicBus->SetVolume`, un slider « Effets » pilote `sfxBus->SetVolume`, et le « Master » coiffe le
  tout. C'est exactement le rôle d'une console de mixage.
- **Gameplay** — couper momentanément une catégorie (mute du bus « Ambiance » pendant une cinématique)
  ou isoler une catégorie pour le débogage (`SetSolo` sur « Voix »).
- **Effets de groupe** — poser une reverb sur le seul bus « Environnement » colore d'un coup tous les
  sons du monde sans toucher l'UI ni la musique, parce que la chaîne d'effets s'applique au mix du bus.

**Volume local contre volume effectif.** `GetVolume` renvoie le réglage de **ce** bus ; `GetEffectiveVolume`
remonte les parents et multiplie. Un son sur « SFX/Pas » à 0.8, sous « SFX » à 0.5, sous « Master » à 1.0,
sort à `0.8 × 0.5 × 1.0 = 0.4`. Baisser un parent baisse toute sa branche — c'est précisément l'effet
voulu d'une table de mixage. **Attention** : la formule documentée de `GetEffectiveVolume` n'intègre
**pas** le mute/solo ; ces drapeaux (`IsMuted`/`IsSoloed`) sont gérés par ailleurs dans le mix, pas
dans ce produit de volumes.

### Le sidechain (ducking) à fond

Le ducking est l'automatisme de mixage le plus utile en jeu : la musique et l'ambiance doivent
**laisser la place** au son important du moment. On déclare le lien sur le bus qui doit s'effacer :
`musicBus->SetSidechainFromBus(voiceBus, 0.7f, 0.05f)`. Tant que le `voiceBus` joue (RMS au-dessus de
`threshold`), le `musicBus` est atténué d'un facteur dosé par `amount` ; quand la voix se tait, la
musique revient.

- **Dialogue** — la musique baisse pendant qu'un PNJ parle, sans qu'on touche manuellement au volume
  de la musique : c'est le cas d'école.
- **Alerte / impact** — faire ressortir un son d'alerte ou un gros impact en duckant brièvement tout
  le reste (sidechain depuis un bus « Stinger » vers le « Master » des autres catégories).
- **Réglage** — `amount` proche de 1 efface presque totalement la cible (radio, annonces) ; plus bas,
  il laisse la cible présente mais en retrait (musique sous la voix).

La détection ne « regarde » pas le signal directement : elle interroge le **compteur de voix actives**
de la source, un `NkAtomic<int32>` que le moteur incrémente/décrémente quand une voix démarre/s'arrête
sur ce bus (`IncrementActiveVoices`/`DecrementActiveVoices`). `HasActiveVoice()` (vrai si le compteur
> 0) et `GetActiveVoiceCount()` exposent cet état. Le caractère **atomique** est essentiel : le thread
audio lit ce compteur à chaque rappel sans verrou.

### La chaîne d'effets d'un bus à fond

`AddEffect` empile en **fin** de chaîne — l'ordre compte (un EQ avant ou après une distortion ne sonne
pas pareil) — et renvoie `false` quand les 8 emplacements sont pris : il faut **vérifier** ce retour.
Le bus ne **possède pas** les effets : il garde des `IAudioEffect*` bruts, à vous d'allouer (via
NKMemory) un effet qui survit au bus et de le libérer après détachement. `RemoveEffect` et
`ClearEffects` détachent sans détruire. Tout est consommé par `ProcessChain`, qui mixe voix +
sous-buses puis fait défiler le buffer dans chaque effet — sur le thread audio, donc sans allocation
ni exception en plein traitement.

### Les effets temporels : `DelayEffect`, `ReverbEffect`, `ChorusEffect`

Ces trois-là allouent un **buffer circulaire** interne (d'où leur destructeur custom et leur paramètre
`memory::NkAllocator*` optionnel) parce qu'ils ont besoin de **mémoriser le passé** du signal.

- **`DelayEffect`** — un écho stéréo : le signal est ré-injecté avec un retard et un `feedback`. C'est
  l'écho de caverne, le slap-back d'une voix, le delay rythmique. `feedback` ∈ [0, 0.99] (≥1 = écho qui
  ne meurt jamais — à éviter), `wet` ∈ [0,1] dose la part traitée, `delayTime` est en secondes.
  `SetDelayTime`/`SetFeedback` ajustent à chaud ; changer le delayTime peut réallouer le buffer.
- **`ReverbEffect`** — la *réverbération* : la queue d'énergie d'un espace (salle, cathédrale, hall).
  Implémentée en *Feedback Delay Network* (Schroeder/FDN simplifié), pilotée par sa `Params` :
  `roomSize` (taille perçue), `damping` (extinction des aigus), `wet` (dosage), `preDelay` (silence
  avant la queue, en s), `diffusion` (densité des échos). C'est l'effet d'ambiance par excellence,
  posé sur un bus « Environnement ». `SetParams` recharge tout d'un coup.
- **`ChorusEffect`** — épaissir un son en superposant une copie au **délai modulé par un LFO**, ce qui
  donne l'impression de plusieurs sources légèrement désaccordées (cordes, nappe, voix doublée). Réglé
  uniquement **à la construction** (`rate`, `depth`, `feedback`, `wet`) — aucun setter public. Son délai
  interne plafonne à ~100 ms (4800 samples @48 kHz).

### Les effets de dynamique : `CompressorEffect`, `LimiterEffect`

Ces deux-là **n'allouent rien** : ils ne gardent qu'une enveloppe, d'où l'absence de destructeur custom
et d'allocateur.

- **`CompressorEffect`** — réduit l'**écart** entre les passages forts et faibles (détection RMS). On le
  pose pour rendre une voix régulière, donner du punch à une batterie, ou stabiliser un mix qui « pompe ».
  `Params` : `threshold` (dBFS au-delà duquel ça compresse), `ratio` (force, 4:1 = modéré), `attackMs`
  /`releaseMs` (vitesse d'enclenchement/relâche), `makeupGain` (dB rajoutés après), `softKnee` (transition
  douce autour du seuil). `GetCurrentGainReduction()` renvoie la réduction instantanée en dB — utile pour
  un VU-mètre de compression. `SetParams` recharge.
- **`LimiterEffect`** — un *brick-wall* qui **empêche tout dépassement** au-dessus d'un plafond : c'est
  l'assurance anti-clipping qu'on pose **sur le Master** en dernier maillon. `Params` : `thresholdDb`
  (plafond, typique -0.5 à -1.0 dBFS), `attackMs` (très court), `releaseMs`. `GetCurrentGainReductionDb()`
  alimente un indicateur. **Particularités** : `SetParams` est ici `noexcept` (≠ Compressor), il y a un
  `GetParams`, et surtout son `GetType()` renvoie **`AudioEffectType::COMPRESSOR`** — pas de type dédié.

### Les filtres : `BiquadFilter`, `Eq3BandEffect`

- **`BiquadFilter`** — le couteau suisse du filtrage (forme directe II transposée). Son enum imbriqué
  `FilterType` couvre huit comportements : `LOW_PASS`/`HIGH_PASS` (couper le haut / le bas du spectre),
  `BAND_PASS`/`NOTCH` (garder / supprimer une bande étroite), `PEAK_EQ` (bosse ou creux autour d'une
  fréquence), `LOW_SHELF`/`HIGH_SHELF` (relever/baisser tout un côté du spectre), `ALL_PASS` (déphasage
  sans changer l'amplitude). On le construit avec `(type, cutoffHz, q=0.707, gainDb=0, sr=48000)` — le
  `gainDb` ne compte que pour `PEAK_EQ`/`LOW_SHELF`/`HIGH_SHELF` ; il n'a **pas** de constructeur par
  défaut. `SetCutoff`/`SetQ`/`SetGain`/`SetType` ajustent à chaud. Cas d'usage : un low-pass piloté par
  la distance ou l'occlusion (un son derrière un mur perd ses aigus), un band-pass pour un effet « radio »,
  un notch pour tuer une résonance gênante. Son `GetType()` est la **seule** des effets déclarée hors-ligne
  (la valeur retournée n'est pas visible dans le header) ; il garde un état stéréo (max 2 canaux).
- **`Eq3BandEffect`** — un **égaliseur 3 bandes** prêt à l'emploi, construit en interne sur trois
  `BiquadFilter` (low-shelf, mid-peak, high-shelf). C'est le réglage « grave / médium / aigu » d'une
  chaîne hi-fi. `Params` donne un gain par bande (`lowGainDb`/`midGainDb`/`highGainDb`), les fréquences
  charnières (`lowFreq=100`, `midFreq=1000`, `highFreq=10000`) et la largeur du médium (`midQ`). On
  l'emploie pour façonner globalement le timbre d'un bus (rendre la musique plus chaude, dégager les
  voix). `GetType()` = `EQ_3BAND`.

### La saturation : `DistortionEffect`

`DistortionEffect` colore par **saturation** — il « casse » volontairement le signal. Son enum
`DistortionType` propose quatre couleurs : `SOFT` (overdrive doux, chaleur), `HARD` (écrêtage franc,
agressif), `FUZZ` (saturation extrême, vintage), `OVERDRIVE` (entre soft et hard). On le construit avec
`(type=SOFT, drive=5, output=0.5)` : `drive` pousse l'entrée dans la saturation, `output` rattrape le
niveau ; `SetDrive`/`SetOutput` ajustent. Usages : une guitare, une voix de radio dégradée, un moteur
ou un bruit de machine « sale », un effet de transmission grésillante. Il n'a **ni** buffer **ni** état
temporel : `Reset()` et `OnSampleRateChanged()` sont des **no-op** (corps vides). `GetType()` = `DISTORTION`.

### Le cycle de vie des effets

Tous suivent le même contrat : `Process` traite **en place**, `Reset` purge (à appeler quand on coupe
un son ou téléporte l'auditeur, pour ne pas traîner une queue de delay/reverb), et `OnSampleRateChanged`
**doit** être appelé quand le moteur change de fréquence — c'est là que les coefficients se recalculent
et que les buffers temporels se réallouent. Les effets à `Params` se construisent soit par défaut (le
ctor délègue avec les valeurs par défaut, d'où `ReverbEffect()` immédiat), soit avec une `Params` remplie ;
on remplace ensuite tout le jeu par `SetParams`. Côté possession : un effet est **alloué par vous** (NKMemory),
**attaché** à un bus (`AddEffect`) qui ne le possède pas, et **libéré par vous** après détachement. Un
limiter se pose typiquement en dernier sur le Master.

### `NkHrtfDataset` et `NkHrirPair` à fond

**`NkHrirPair`** est un petit agrégat copiable : `leftIR`/`rightIR` (les réponses impulsionnelles
gauche/droite) et `length` (samples par IR). Capital : ces pointeurs ne sont **pas possédés** — ils
pointent dans le grand buffer du dataset. Ils restent valides tant que le dataset n'est ni `Unload()`
ni détruit ; ne les **conservez pas** au-delà.

**Remplir le dataset.** Deux voies :

- `LoadFromFile(path, alloc=nullptr)` charge un `.nkhrtf` binaire mesuré. Le format est documenté : magic
  « NKHR » (4 o), version=1 (4 o), puis `sampleRate`, `nAzimuths`, `nElevations`, `irLength` (int32), et
  pour chaque couple (élévation, azimut) `2*irLength` float32 (L puis R). Renvoie `true` si succès.
- `CreateSynthetic(sampleRate, irLength, nAzimuths, nElevations, alloc=nullptr)` **génère** un dataset à
  partir d'un modèle de **sphère** (rayon de tête 8.75 cm) : il calcule l'ITD (différence de temps
  inter-aurale) et un *head shadow* passe-bas. Aucun fichier requis — parfait pour démarrer, pour une
  plateforme sans assets, ou comme repli. Typiques : 44100/48000 Hz, irLength 128/256, 36 azimuts (pas
  de 10°), 14 élévations.

`Unload()` libère les buffers ; l'allocateur passé (sinon défaut) sert à toutes les allocations internes.

**Interroger le dataset.** `GetHRIR(azimuthDeg, elevationDeg)` renvoie la paire la plus proche (plus
proche voisin) : `azimuthDeg` ∈ [0..360] (0 = devant, 90 = droite), `elevationDeg` ∈ [-40..90]. Si le
dataset n'est pas chargé, la paire renvoyée a `length=0` — à tester avant de convoluer. (L'interpolation
bilinéaire entre directions voisines est prévue mais **pas encore implémentée**.) En complément,
`IsLoaded`, `GetSampleRate`, `GetIrLength`, `GetAzimuthCount`, `GetElevationCount` exposent l'état et les
dimensions effectives.

- **Audio 3D casque** — la brique d'un son spatialisé crédible : on prend la direction source→auditeur,
  on lit la `NkHrirPair`, on convolue le signal mono avec `leftIR`/`rightIR` pour produire le binaural.
- **VR / AR** — indispensable au casque : un pas derrière l'oreille, une voix venant du côté ; la
  spatialisation HRTF est ce qui rend la présence convaincante.
- **Démarrage sans assets** — `CreateSynthetic` donne une spatialisation correcte sans aucun fichier
  HRTF, le temps d'intégrer ou de mesurer un vrai dataset `.nkhrtf`.

Le **layout interne** (documenté) est un unique buffer continu indexé `mData[elev][az][canal L=0/R=1][sample]`,
de taille `nAzimuths * nElevations * 2 * irLength * sizeof(float32)` ; les bornes d'élévation par défaut
sont -40°..90°. Le dataset est **non copiable** : on le partage par pointeur/référence, on ne le duplique pas.

---

### Exemple

```cpp
#include "NKAudio/NkAudioBus.h"
#include "NKAudio/NkAudioEffects.h"
#include "NKAudio/NkHrtfDataset.h"
using namespace nkentseu::audio;

// 1) Hiérarchie de bus : Master > {Music, SFX, Voice}
NkAudioBus master("Master");
NkAudioBus music("Music", &master);
NkAudioBus sfx  ("SFX",   &master);
NkAudioBus voice("Voice", &master);

music.SetVolume(0.5f);                         // toute la musique d'un coup
float32 v = music.GetEffectiveVolume();        // 0.5 * 1.0 (master)

// 2) Ducking : la musique s'efface quand une voix parle.
music.SetSidechainFromBus(&voice, 0.7f, 0.05f);

// 3) Une chaîne d'effets sur le bus d'environnement + un limiter sur le master.
ReverbEffect::Params rp; rp.roomSize = 0.8f; rp.wet = 0.35f;
ReverbEffect reverb(rp);                       // ctor à Params
sfx.AddEffect(&reverb);                        // effet non possédé : reverb doit survivre au bus

LimiterEffect limiter;                         // Params par défaut (plafond -0.5 dBFS)
master.AddEffect(&limiter);                    // dernier maillon, anti-clipping
// (limiter.GetType() == AudioEffectType::COMPRESSOR — pas de type LIMITER !)

// 4) Spatialisation HRTF : générer un dataset synthétique puis lire une direction.
NkHrtfDataset hrtf;
hrtf.CreateSynthetic(48000, 256, 36, 14);      // pas de fichier requis
NkHrirPair ir = hrtf.GetHRIR(/*azimut*/ 90.f, /*élévation*/ 0.f);  // source à droite
if (ir.length > 0) {
    // convoluer le signal mono avec ir.leftIR / ir.rightIR ...
}
```

---

[← Index NKAudio](README.md) · [Récap NKAudio](../NKAudio.md) · [Couche Runtime](../README.md)
