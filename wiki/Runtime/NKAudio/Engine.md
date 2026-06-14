# Le moteur audio

> Couche **Runtime** · NKAudio · Le cœur du son temps réel : le singleton `AudioEngine`, les
> **voix** qu'on joue, le son **3D** (panoramique, distance, Doppler, HRTF), les **bus**
> hiérarchiques, et le **pilote** bas niveau (`IAudioBackend`) qui parle à la carte son.

Faire du son dans un jeu ou une simulation, ce n'est pas « lire un fichier .wav ». C'est mélanger
**des dizaines de sons à la fois** — la musique, les pas, les tirs, une voix, un clic d'interface —,
les positionner dans l'espace autour de l'auditeur, les faire monter et descendre en volume, le
tout **soixante fois par seconde sans le moindre accroc**. Le moindre retard, la moindre allocation
mémoire au mauvais moment, et c'est un *clic*, un *crachat* audible. NKAudio résout ce problème avec
un **moteur central unique** qui tourne sur son propre thread temps réel, mélange jusqu'à 256 voix,
et expose au reste du code une API simple : « joue ce son, donne-moi un *handle*, et laisse-moi
agir dessus ».

Ce n'est **pas** une bibliothèque de lecture de fichiers (ça, c'est `AudioLoader`, sur une autre
page), ni un studio d'effets hors-ligne (ça, c'est `AudioMixer`/`AudioGenerator`). C'est le
**runtime** : la machine qui tourne pendant que le jeu tourne. Tout passe par un seul point
d'entrée, `AudioEngine::Instance()`.

- **Namespace** : `nkentseu::audio`
- **Headers** : `#include "NKAudio/NKAudio.h"`, `#include "NKAudio/NkAudioBackends.h"`

---

## Jouer un son : `AudioEngine` et `AudioHandle`

`AudioEngine` est un **singleton** : il n'en existe qu'un, on l'obtient par
`AudioEngine::Instance()`, et son constructeur est privé (impossible d'en créer un soi-même, ni de
le copier). On l'**initialise une fois** au démarrage avec un `AudioEngineConfig`, on le **détruit
une fois** à l'arrêt avec `Shutdown()` — ces deux opérations sur le **thread principal
uniquement** ; tout le reste est *thread-safe*.

Jouer un son tient en une ligne. On donne un `AudioSample` (les données PCM, chargées par
`AudioLoader` ou synthétisées par `AudioGenerator`) à `Play`, et on récupère un `AudioHandle` :

```cpp
auto& engine = AudioEngine::Instance();
engine.Initialize();                          // config par défaut : 48 kHz stéréo, 256 voix

AudioSample shot = AudioLoader::Load("gunshot.wav");
AudioHandle h = engine.Play(shot);            // joue immédiatement, renvoie un handle
if (!h) { /* pool de voix plein : le son n'a pas démarré */ }
```

Le `AudioHandle` est une **valeur légère de 4 octets** — pas un pointeur, pas un objet à libérer.
C'est un *ticket* qui désigne la voix en cours. On le teste avec `IsValid()` ou directement
`if (h)` : un handle **invalide** signifie que le pool des 256 voix était plein et que le son n'a
pas pu démarrer. Une fois le handle en main, tout le contrôle de la voix passe par lui : `Stop`,
`Pause`, `Resume`, `SetVolume`, `SetPitch`, `SetPan`, position de lecture…

Ce n'est **pas** un objet propriétaire : le handle ne « possède » pas le son, et l'`AudioSample`
sous-jacent reste la propriété de l'appelant — il doit **rester valide pendant toute la lecture**.
Si vous libérez le sample (`AudioLoader::Free`) pendant qu'une voix le joue, vous lisez de la
mémoire morte.

Pour les sons *calculés à la volée* plutôt que stockés — un synthétiseur, un bruit de moteur
paramétrique, un test — il existe `PlayProcedural`, qui prend un *callback* appelé sur le thread
audio pour remplir le buffer frame par frame.

> **En résumé.** Tout passe par `AudioEngine::Instance()`. `Initialize`/`Shutdown` au thread
> principal, le reste *thread-safe*. `Play(sample)` renvoie un `AudioHandle` léger (4 octets) ;
> testez `if (h)` (invalide = pool plein). Le handle pilote la voix ; le sample doit survivre à la
> lecture. `PlayProcedural` pour un son généré en direct.

---

## Le son dans l'espace : audio 3D et HRTF

Un son n'a pas seulement un volume — il a une **position**. Une explosion à votre gauche doit
sortir à gauche ; une voiture qui s'éloigne doit faiblir ; une sirène qui passe doit changer de
hauteur (l'effet Doppler). NKAudio gère tout cela par voix, via les paramètres `AudioSource3D`
portés par chaque voix, et un `AudioListener3D` unique qui représente les oreilles de l'auditeur
(typiquement la caméra).

Le mode 3D ne s'active **que si on le demande** : par défaut une voix est plate (stéréo simple). On
passe `source3d.positional = true` (ou `SetSourcePositional(h, true)`) pour qu'elle soit spatialisée.
Dès lors, le moteur calcule le panoramique et l'atténuation à partir de la position de la source,
de celle de l'auditeur et du modèle d'atténuation choisi :

```cpp
engine.SetSourcePositional(h, true);
engine.SetSourcePosition(h, enemyX, enemyY, enemyZ);
engine.SetListenerPosition(camX, camY, camZ);
engine.SetListenerOrientation(fwdX, fwdY, fwdZ,  upX, upY, upZ);
```

L'`AttenuationModel` décide **comment le volume décroît avec la distance** : `NONE` (jamais), `LINEAR`
(décroissance droite entre `minDistance` et `maxDistance`), `INVERSE` (le défaut, réaliste : le son
est divisé quand on double la distance), `EXPONENTIAL` (chute plus brutale). On peut aussi modéliser
un **cône directionnel** (un haut-parleur, une bouche qui parle dans une direction), une **occlusion**
fournie par l'application (un mur entre la source et l'auditeur, valeur [0..1]), et l'**absorption de
l'air** sur les longues distances.

Pour aller plus loin que le simple panoramique gauche/droite, NKAudio offre le **HRTF**
(*Head-Related Transfer Function*) : la simulation de la façon dont **votre tête et vos oreilles**
filtrent le son selon sa provenance, ce qui donne au casque une vraie sensation de hauteur et
d'arrière. Le HRTF exige trois choses ensemble : `positional = true`, `useHrtf = true`, **et** un
*dataset* HRTF chargé. On charge un fichier `.nkhrtf` avec `LoadHrtfDataset`, ou on en
**génère un synthétique** sans fichier avec `GenerateSyntheticHrtf` — utile pour démarrer sans
ressource externe.

> **En résumé.** Le 3D est **opt-in** par voix (`positional = true`). On place la source
> (`SetSourcePosition`) et l'auditeur (`SetListenerPosition`/`SetListenerOrientation`), on choisit
> un `AttenuationModel` (`INVERSE` par défaut). Cône, occlusion, absorption de l'air en option. Le
> HRTF (immersion au casque) demande `positional` + `useHrtf` + un dataset chargé ou synthétique.

---

## Organiser le mélange : les bus

Quand des dizaines de sons jouent, on ne veut pas régler chacun individuellement : on veut dire
« baisse **toute la musique** » ou « coupe **toute l'interface** ». C'est le rôle des **bus**, une
hiérarchie de groupes de volume. NKAudio en crée automatiquement à l'initialisation : un bus racine
`"Master"`, et quatre sous-bus `"SFX"` (le défaut), `"Music"`, `"Voice"` et `"UI"`. Chaque voix est
routée vers un bus via `VoiceParams::bus` ; le **volume effectif** d'une voix est le **produit de
toute la chaîne** de bus jusqu'au Master.

```cpp
VoiceParams p;
p.bus = "Music";          // cette voix sera contrôlée par le bus Music
engine.Play(track, p);

NkAudioBus* music = engine.GetBus("Music");   // pour agir sur tout le groupe
```

On récupère les bus par `GetMasterBus()`, `GetBus(name)` (recherche récursive), ou on en crée de
nouveaux avec `GetOrCreateBus(name, parent)` (parent nul = sous le Master ; renvoie l'existant si le
nom existe déjà). À noter : dans ces headers, `NkAudioBus` n'est que **déclaré en avant** — son API
membre détaillée vit ailleurs ; ici on manipule des pointeurs.

Sur la même idée hiérarchique, NKAudio gère le **fondu enchaîné de musique** : `PlayMusicCrossfade`
fait disparaître en fondu les voix du bus `"Music"` tout en faisant apparaître la nouvelle piste sur
ce même bus — la transition de niveau de jeu en une ligne.

> **En résumé.** Les bus regroupent les voix pour un contrôle de volume collectif. Hiérarchie
> auto : `Master` › `SFX`/`Music`/`Voice`/`UI` ; le volume effectif = produit de la chaîne. On
> route via `VoiceParams::bus`, on agit via `GetBus`/`GetOrCreateBus`. `PlayMusicCrossfade` enchaîne
> deux musiques sur le bus `Music`.

---

## Le pilote : `IAudioBackend` et les effets

Sous le moteur, quelqu'un doit **vraiment** envoyer les échantillons à la carte son. C'est le rôle
de `IAudioBackend`, l'interface du pilote bas niveau, implémentée différemment selon le système :
WASAPI/DirectSound (Windows), CoreAudio (macOS/iOS), ALSA/PulseAudio (Linux), AAudio/OpenSL ES
(Android), WebAudio (navigateur), et `NullAudioBackend` (sortie muette, partout). On choisit
rarement le backend à la main : `AudioEngineConfig::backend = AudioBackendType::AUTO` laisse le
moteur prendre le meilleur de la plateforme. Pour les cas avancés, `AudioBackendFactory` permet de
créer un backend par nom, par type, ou d'en **enregistrer un sur mesure** (macro
`NK_REGISTER_AUDIO_BACKEND`).

L'autre point d'extension est le **traitement du signal** : `IAudioEffect`, une interface *Strategy*
qu'on branche sur une voix (`AddEffect`) ou sur tout le mélange (`AddMasterEffect`). Réverbération,
écho, compression, égaliseur, filtres — l'`AudioEffectType` énumère ce que le moteur sait traiter.
Point **crucial et non négociable** : la méthode `Process` d'un effet, comme tout *callback* audio,
s'exécute **sur le thread audio temps réel** ; il y est **interdit d'allouer de la mémoire ou de
poser un verrou**. La moindre allocation peut faire rater l'échéance du buffer → un *glitch* audible.

> **En résumé.** `IAudioBackend` est le pilote OS (WASAPI, CoreAudio, ALSA…), choisi en `AUTO` par
> défaut, extensible via `AudioBackendFactory` + `NK_REGISTER_AUDIO_BACKEND`. `IAudioEffect` ajoute
> du DSP par voix ou sur le master. **Règle d'or du thread audio** : dans `Process`/les callbacks,
> jamais d'allocation ni de verrou.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la
« Référence complète » qui suit.

### Constantes et énumérations

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Constantes | `AUDIO_DEFAULT_SAMPLE_RATE` (48000), `_CHANNELS` (2), `_BUFFER_SIZE` (256) | Réglages par défaut du moteur. |
| Constantes | `AUDIO_MAX_VOICES` (256), `_MAX_EFFECTS_PER_VOICE` (8), `_MAX_MASTER_EFFECTS` (16) | Plafonds du pool et des effets. |
| Constantes | `AUDIO_DEFAULT_FFT_SIZE` (2048), `AUDIO_SPEED_OF_SOUND` (343 m/s) | Analyse / calcul Doppler. |
| Constantes | `AUDIO_INVALID_ID` (0), `AUDIO_HANDLE_INVALID` | Identifiant / handle invalides. |
| Enum | `AudioFormat` | `UNKNOWN, WAV, MP3, OGG, FLAC, OPUS, AIFF, RAW`. |
| Enum | `SampleFormat` | `UNKNOWN, INT_8, INT_16, INT_24, INT_32, FLOAT_32, FLOAT_64`. |
| Enum | `AudioBackendType` | `AUTO, DIRECTSOUND, WASAPI, CORE_AUDIO, ALSA, PULSE_AUDIO, OPEN_SL_ES, AAUDIO, WEB_AUDIO, CUSTOM, NULL_OUTPUT`. |
| Enum | `WaveformType` | `SINE, SQUARE, TRIANGLE, SAWTOOTH, NOISE_WHITE, NOISE_PINK, PULSE`. |
| Enum | `AudioEffectType` | `NONE, REVERB, ECHO, DELAY, CHORUS, FLANGER, PHASER, DISTORTION, COMPRESSOR, LIMITER, GATE, EQ_3BAND, EQ_PARAMETRIC, LOW_PASS, HIGH_PASS, BAND_PASS, NOTCH, PITCH_SHIFT, TIME_STRETCH, AUTOPAN, TREMOLO, VIBRATO`. |
| Enum | `VoiceState` | `FREE, PLAYING, PAUSED, STOPPING, FINISHED`. |
| Enum | `AttenuationModel` | `NONE, INVERSE, LINEAR, EXPONENTIAL`. |
| Enum | `ResamplingQuality` | `LINEAR, SINC_4, SINC_8`. |

### Structures

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handle | `AudioHandle` | Ticket 4 octets vers une voix ; `IsValid`, `operator bool`, `==`/`!=`. |
| Données PCM | `AudioSample` | Échantillon Float32 interleaved ; `GetDuration`, `GetSampleCount`, `IsValid`. |
| Spatial | `AudioSource3D` | Paramètres 3D d'une voix (position, cône, atténuation, occlusion, HRTF…). |
| Spatial | `AudioListener3D` | Position/vitesse/orientation de l'auditeur. |
| Lecture | `VoiceParams` | volume, pitch, pan, boucle, priorité, `source3d`, `bus`. |
| Synthèse | `AdsrEnvelope` | attack/decay/sustain/release. |
| Analyse | `FftResult` | magnitudes/phases ; `BinToFrequency`. |
| Config | `AudioEngineConfig` | Paramètres d'`Initialize` (backend, taux, voix, HRTF, limiteur…). |

### Interfaces et le moteur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interface | `IAudioEffect` | Effet DSP branchable (Strategy) ; `Process`, `Reset`, `Set/GetWetMix`, `Set/GetEnabled`. |
| Interface | `IAudioBackend` | Pilote bas niveau ; `Initialize/Start/Stop/SetCallback`, latence, état. |
| Factory | `AudioBackendFactory` | `Register`, `Create`, `CreateDefault`, `CreateByType`. |
| Cycle de vie | `AudioEngine::Instance`, `Initialize`, `Shutdown`, `IsInitialized` | Accès singleton + bornes de vie. |
| Lecture | `Play`, `PlayProcedural` | Jouer un sample / un callback ; renvoie un `AudioHandle`. |
| Contrôle voix | `Stop`, `Pause`, `Resume`, `IsPlaying/IsPaused/IsLooping` | Transport par handle. |
| Contrôle voix | `Set/GetVolume`, `Set/GetPitch`, `Set/GetPan`, `SetLooping`, `Set/GetPlaybackPosition` | Paramètres par handle. |
| Audio 3D | `SetSourcePosition/Velocity/Direction`, `SetSourcePositional`, `SetSourceMin/MaxDistance`, `SetSourceOcclusion/AirAbsorption/HRTF` | Spatialisation par voix. |
| HRTF | `LoadHrtfDataset`, `GenerateSyntheticHrtf`, `UnloadHrtfDataset`, `IsHrtfLoaded` | Gestion du dataset binaural. |
| Listener | `SetListenerPosition/Velocity/Orientation` | Oreilles de l'auditeur. |
| Effets | `AddEffect`/`RemoveEffect`/`ClearEffects`, `AddMasterEffect`/`RemoveMasterEffect`/`ClearMasterEffects` | DSP par voix / sur le master. |
| Bus | `GetMasterBus`, `GetBus`, `GetOrCreateBus`, `PlayMusicCrossfade` | Hiérarchie de volume + fondu musique. |
| Global | `Set/GetMasterVolume`, `StopAll`/`PauseAll`/`ResumeAll`, `RenderToBuffer` | Contrôles d'ensemble + rendu hors-ligne. |
| Infos | `GetBackendType/Name`, `GetSampleRate/Channels/BufferSize`, `GetLatencyMs`, `GetActiveVoices/MaxVoices` | État du moteur. |
| Macro | `NK_REGISTER_AUDIO_BACKEND(Class, Name)` | Auto-enregistre un backend dans la factory. |
| Init | `EnsureBackendsRegistered()` | Enregistre les backends natifs (appelée par `Initialize`). |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage dans les différents domaines du temps
réel — jeux, simulation, outils, applications scientifiques.

### `AudioHandle` — le ticket d'une voix

Une valeur **de 4 octets** (un seul champ `uint32 id`), pensée pour être copiée librement et stockée
sans coût. Elle ne désigne pas un objet à libérer : c'est une **référence opaque** vers une voix
dans le pool du moteur. On la teste avant de s'en servir : `IsValid()` (ou `operator bool`, donc
`if (h)`) renvoie faux quand `id == AUDIO_INVALID_ID`, c'est-à-dire quand `Play` n'a **pas** pu
allouer de voix (pool des 256 plein) — un cas réel quand beaucoup de sons partent en même temps,
qu'il faut gérer (ignorer le son le moins prioritaire, par exemple). `operator==`/`!=` permettent de
comparer deux handles (savoir si deux variables désignent la même voix). La constante
`AUDIO_HANDLE_INVALID` sert de valeur sentinelle initiale.

### `AudioSample` — les données sonores

Le conteneur de PCM brut. Son **format interne est unique et fixe** : `float32` **interleaved** (les
canaux entrelacés : L, R, L, R…), choix qui simplifie tout le DSP en aval. Les champs : `data` (le
buffer), `frameCount` (nombre de *frames* — une frame = un échantillon par canal), `sampleRate`,
`channels`, le `format` d'origine, et l'allocateur `mAllocator` qui a servi à l'allouer. Trois
aides : `GetDuration()` (durée en secondes = `frameCount / sampleRate`, 0 si le taux est invalide),
`GetSampleCount()` (`frameCount * channels`, le nombre total de floats), `IsValid()` (données non
nulles et dimensions cohérentes).

Cas d'usage : c'est la **monnaie d'échange** de tout NKAudio. `AudioLoader` en produit en lisant un
fichier, `AudioGenerator` en synthétisant, `AudioMixer` en mélangeant ; `AudioEngine::Play` et
toutes les fonctions d'analyse en consomment. **Propriété et durée de vie** : un sample retourné par
le loader/générateur/mixeur est alloué via un `memory::NkAllocator` ; il faut le libérer avec
`AudioLoader::Free(sample)` (le **même** allocateur), jamais avec `delete`. Et tant qu'une voix le
joue, il doit rester en mémoire.

### `AudioSource3D` — la voix dans l'espace

L'ensemble des paramètres spatiaux portés par une voix. `position`, `velocity`, `direction` (en
coordonnées monde) ; `minDistance`/`maxDistance`/`rolloffFactor` règlent la courbe d'atténuation ;
`coneInnerAngle`/`coneOuterAngle`/`coneOuterGain` (en degrés) modélisent un son **directionnel** (un
mégaphone, une voix qui parle dans une direction : plein volume dans le cône intérieur, atténué au
dehors). `attenuationModel` choisit la loi de décroissance. Trois interrupteurs importants :
`positional` **active** le mode 3D (faux par défaut → son plat) ; `useDoppler` autorise le décalage
de hauteur quand source et auditeur se rapprochent/s'éloignent ; `useHrtf` demande le rendu binaural
(exige `positional` **et** un dataset). Enfin `occlusion` ([0..1], fournie par l'application qui sait
s'il y a un mur) et `airAbsorption` (atténuation des hautes fréquences sur la distance) affinent le
réalisme.

- **Jeux** — un ennemi qu'on entend approcher dans le dos (HRTF), une cascade dont le volume monte
  quand on s'en rapproche (`INVERSE`), un haut-parleur d'arène orienté vers le public (cône).
- **Simulation / VR** — placement spatial fidèle des sources, Doppler d'un véhicule qui passe,
  occlusion par les cloisons d'un bâtiment.
- **Scientifique** — reproduction contrôlée d'un champ sonore (psychoacoustique, tests d'audition).

### `AudioListener3D` — les oreilles

L'unique auditeur, généralement collé à la caméra. `position` et `velocity` (pour le Doppler côté
auditeur), `forward` et `up` (l'orientation : sans elle, le moteur ne saurait pas où est « la
gauche »). On le met à jour chaque frame avec `SetListenerPosition/Velocity/Orientation`.

### `VoiceParams` — comment jouer

Le paquet d'options passé à `Play`. `volume` ([0,1+], au-delà de 1 amplifie), `pitch` (multiplie la
hauteur **et** la vitesse de lecture), `pan` ([-1,1], gauche↔droite, pour le son **non** 3D).
`looping` + `loopStart`/`loopEnd` (boucler une portion, `-1` = jusqu'à la fin) servent aux musiques
et aux ambiances continues. `fadeInTime` adoucit le démarrage, `startOffset` saute au milieu du son.
`priority` ([0-255]) départage quand le pool est saturé (un son grave écrase un son anodin).
`source3d` embarque toute la spatialisation ci-dessus, et `bus` (`"SFX"` par défaut, ou `"Music"`/
`"Voice"`/`"UI"`) **route** la voix vers son groupe de volume.

### `AdsrEnvelope` — la forme dans le temps

Quatre paramètres qui décrivent l'évolution du volume d'un son dans le temps : `attackTime` (montée
initiale), `decayTime` (chute vers le palier), `sustainLevel` ([0,1], le palier tenu), `releaseTime`
(extinction). C'est le vocabulaire des **synthétiseurs** : une attaque brève + release court donne un
son percussif, une attaque lente un son « qui gonfle ». Appliqué hors-ligne par
`AudioGenerator::ApplyEnvelope`.

### `FftResult` — un spectre

Le résultat d'une transformée de Fourier : `magnitudes` et `phases` (des `NkVector<float32>`),
`fftSize` et `sampleRate`. La méthode `BinToFrequency(bin)` traduit un indice de *bin* en fréquence
réelle (`bin * sampleRate / fftSize`) — pour savoir « quelle note » correspond à un pic. Utile aux
**visualiseurs** (barres de spectre), à la **détection** de fréquence, à l'**analyse** scientifique
d'un signal.

### `AudioEngineConfig` — l'initialisation

Tout ce qui paramètre `Initialize`. `backend` (`AUTO` choisit le meilleur de la plateforme),
`sampleRate`/`channels`/`bufferSize` (le buffer plus petit = moins de latence mais plus de risque de
*glitch*), `maxVoices`. `enableHrtf`/`enableDoppler` activent globalement ces fonctions,
`masterVolume` le niveau de départ, `allocator` (nul = allocateur global de NKMemory). Deux réglages
de sécurité/qualité : `enableMasterLimiter` + `masterLimiterThresholdDb` (un limiteur sur le master
qui empêche la saturation, à -0,5 dB par défaut) et `resamplingQuality` (`LINEAR` rapide, `SINC_4`/
`SINC_8` plus propres pour les changements de hauteur/taux).

### `IAudioEffect` — le DSP branchable

L'interface d'un effet (patron *Strategy*). Quatre méthodes **pures** : `Process(buffer, frameCount,
channels)` (le traitement lui-même, **sur le thread audio**), `Reset()` (vider les états internes,
ex. les lignes de retard d'une réverbe), `OnSampleRateChanged(rate)` (recalculer les coefficients si
le taux change), `GetType()` (renvoie son `AudioEffectType`). Quatre virtuelles non pures avec
implémentation par défaut : `IsEnabled`/`SetEnabled` (bypass) et `GetWetMix`/`SetWetMix` (dosage
sec/traité), s'appuyant sur les membres protégés `mEnabled` et `mWetMix`. **La contrainte centrale**
: `Process` tourne en temps réel → **aucune allocation, aucun verrou**, sous peine de *glitch*. On
branche un effet sur une voix (`AddEffect`) ou sur tout le mélange (`AddMasterEffect`), et **on en
reste propriétaire** : le moteur ne le détruit pas.

### `IAudioBackend` — le pilote OS

L'abstraction du périphérique de sortie. Type imbriqué `AudioCallback` =
`NkFunction<void(float32* buffer, int32 frames, int32 channels)>` : le moteur fournit ce callback, le
backend l'appelle quand la carte son réclame des échantillons. Méthodes (toutes pures) :
`Initialize(sampleRate, channels, bufferSize)`, `Shutdown`, `SetCallback`, `Start`/`Stop`/`Pause`/
`Resume`, les accesseurs `GetSampleRate/Channels/BufferSize`, `GetLatencyMs` (la latence réelle,
cruciale pour synchroniser son et image), `IsRunning`, `GetName`. Tout y est *thread-safe* **sauf**
`SetCallback`. On ne l'utilise quasiment jamais directement — le moteur s'en charge — mais c'est le
point d'extension pour un périphérique exotique.

### `AudioBackendFactory` — choisir le pilote

La fabrique de backends. Type imbriqué `CreatorFunc` = `NkFunction<IAudioBackend*()>`.
`Register(name, creator)` inscrit un backend ; `Create(name)` en fabrique un par nom (nul si
inconnu) ; `CreateDefault()` choisit selon la plateforme ; `CreateByType(type)` selon
l'`AudioBackendType`. La macro globale `NK_REGISTER_AUDIO_BACKEND(Class, Name)` automatise
l'inscription (struct anonyme + instance statique qui appelle `Register` au chargement). Et
`EnsureBackendsRegistered()` enregistre les backends natifs : elle est **appelée explicitement par
`AudioEngine::Initialize()`** (indispensable en bibliothèque statique, où le linker peut éliminer les
initialiseurs statiques), et elle est idempotente.

### `AudioEngine` — le cœur, en détail

**Cycle de vie.** `Instance()` donne le singleton. `Initialize(config)` démarre le moteur (à n'appeler
que si `!IsInitialized()`), crée les bus, lance le thread audio. `Shutdown()` arrête tout
proprement (fondu de sortie de toutes les voix). À faire **sur le thread principal**.

**Lecture.** `Play(sample, params)` lance une voix et renvoie son handle (invalide = pool plein).
`PlayProcedural(callback, params)` joue un son **généré à la volée** : le `ProceduralCallback`
(`NkFunction<void(float32*, int32, int32)>`) remplit le buffer — donc soumis à la règle temps réel
(pas d'allocation/verrou).

**Contrôle des voix.** `Stop(h, fadeOutTime)` arrête (avec fondu optionnel), `Pause`/`Resume`
suspendent/relancent. On interroge l'état avec `IsPlaying`/`IsPaused`/`IsLooping`. On règle en direct
`SetVolume`/`SetPitch`/`SetPan`/`SetLooping` (et leurs lecteurs `GetVolume`/`GetPitch`/`GetPan`), et
on navigue dans le son avec `GetPlaybackPosition`/`SetPlaybackPosition` (en secondes — sauter à un
repère, rembobiner).

**Audio 3D.** Toute la spatialisation par voix : `SetSourcePosition/Velocity/Direction`,
`SetSourcePositional` (l'interrupteur 3D), `SetSourceMinDistance`/`SetSourceMaxDistance` (la zone
d'atténuation), `SetSourceOcclusion` ([0..1], le mur), `SetSourceAirAbsorption`, et `SetSourceHRTF`
(qui exige un dataset chargé).

**HRTF.** `LoadHrtfDataset(path)` charge un `.nkhrtf`, `GenerateSyntheticHrtf(irLength, nAzimuths,
nElevations)` en fabrique un sans fichier (démarrer sans ressource), `UnloadHrtfDataset` libère, et
`IsHrtfLoaded` indique si un dataset est prêt.

**Listener.** `SetListenerPosition`/`SetListenerVelocity`/`SetListenerOrientation` placent et
orientent l'auditeur — à mettre à jour chaque frame depuis la caméra.

**Effets.** Sur une voix : `AddEffect`/`RemoveEffect`/`ClearEffects`. Sur le mélange global :
`AddMasterEffect`/`RemoveMasterEffect`/`ClearMasterEffects`. Dans les deux cas, l'effet
**appartient à l'appelant**.

**Bus.** `GetMasterBus()` (le bus racine, nul si non initialisé), `GetBus(name)` (recherche récursive
dans la hiérarchie), `GetOrCreateBus(name, parent)` (crée sous `parent` ou sous Master, renvoie
l'existant si le nom existe). `PlayMusicCrossfade(newMusic, fadeTime, params)` enchaîne deux musiques
sur le bus `"Music"`. Rappel : `NkAudioBus` n'est ici qu'une **déclaration en avant** — on ne
manipule que des pointeurs.

**Contrôles globaux.** `SetMasterVolume`/`GetMasterVolume`, et les actions d'ensemble `StopAll`/
`PauseAll`/`ResumeAll` (pratiques pour un menu pause, une perte de focus de la fenêtre). `RenderToBuffer(out, frameCount, channels)` fait un **rendu hors-ligne synchrone** : il remplit un
buffer fourni (≥ `frameCount * channels * sizeof(float32)`) **sans démarrer le thread audio** — pour
capturer en WAV, tester de façon déterministe, ou alimenter un backend personnalisé.

**Informations.** `GetBackendType`/`GetBackendName` (quel pilote tourne), `GetSampleRate`/
`GetChannels`/`GetBufferSize`, `GetLatencyMs` (latence pour la synchro audio-vidéo), et
`GetActiveVoices`/`GetMaxVoices` (suivi de charge, débogage du pool).

### Le socle commun

- **Format interne unique.** Tout est `float32` interleaved : conversion au chargement, et tous les
  buffers DSP suivent cette convention.
- **Un seul moteur.** Tout passe par `AudioEngine::Instance()`. `Initialize`/`Shutdown` au thread
  principal ; le reste est *thread-safe*.
- **Mémoire NKMemory.** Les `AudioSample` viennent d'un `memory::NkAllocator` ; libérez-les avec
  `AudioLoader::Free` (même allocateur), jamais `delete`. Voir [NKMemory](../../Foundation/NKMemory.md).
- **Règle du thread audio.** `IAudioEffect::Process`, `AudioCallback`, `ProceduralCallback` tournent
  en temps réel : **interdiction d'allouer ou de verrouiller**.

---

### Exemple

```cpp
#include "NKAudio/NKAudio.h"
using namespace nkentseu::audio;

auto& engine = AudioEngine::Instance();

// Démarrage : 48 kHz stéréo, sélection auto du backend.
AudioEngineConfig cfg;
cfg.enableHrtf = true;
engine.Initialize(cfg);

// Un son simple, routé vers le bus SFX (défaut).
AudioSample shot = AudioLoader::Load("gunshot.wav");
AudioHandle h = engine.Play(shot);
if (h) engine.SetVolume(h, 0.8f);

// Un ennemi spatialisé qu'on entend approcher au casque (HRTF).
AudioHandle e = engine.Play(growl);
engine.SetSourcePositional(e, true);
engine.SetSourceHRTF(e, true);                 // dataset chargé via cfg.enableHrtf + Generate/Load
engine.SetSourcePosition(e, 10.f, 0.f, -5.f);
engine.SetListenerPosition(0.f, 0.f, 0.f);
engine.SetListenerOrientation(0.f, 0.f, -1.f,  0.f, 1.f, 0.f);

// Changement de musique en fondu enchaîné, sur le bus Music.
engine.PlayMusicCrossfade(nextTrack, 3.0f);

// ... à l'arrêt :
engine.Shutdown();
AudioLoader::Free(shot);                        // même allocateur que Load
```

---

[← Index NKAudio](README.md) · [Récap NKAudio](../NKAudio.md) · [Couche Runtime](../README.md)
