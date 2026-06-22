# NKAudio — sons, musique et synthèse procédurale

> Guide 6 de la série [Guides Nkentseu](README.md). Prérequis conseillé :
> [NKMemory](01-NKMemory.md) (la règle d'or mémoire s'applique ici aussi).
> Langue : français. Style : SFML — une brique à la fois, code réel et compilable.

---

## 1. Introduction

**NKAudio** est le moteur audio de Nkentseu. C'est un moteur **AAA, sans STL**
(il s'appuie sur `NkVector`, `NkFunction`, `NkAllocator` de la fondation), conçu
pour les jeux comme pour les applications exigeantes (DAW, SFX temps réel).

Ce qu'il sait faire, et ce que ce guide va couvrir :

- jouer des **sons courts** (un « pop », un tir, un clic) ;
- jouer une **musique de fond** en boucle, la mettre en pause / reprendre ;
- régler le **volume** (par son et global) ;
- **générer des sons en code** (oscillateurs, accords, enveloppe ADSR) — pratique
  quand on n'a pas encore de fichiers ;
- un aperçu du **streaming** pour les longs fichiers.

L'API publique tient dans un seul header :

```cpp
#include "NKAudio/NKAudio.h"

using namespace nkentseu;
using namespace nkentseu::audio;   // AudioEngine, AudioLoader, AudioGenerator, …
```

Les pièces principales :

| Type | Rôle |
|------|------|
| `AudioEngine` | le moteur, **singleton thread-safe** (`Instance()`), gère le mixage et les voix |
| `AudioSample` | les **données PCM** d'un son en mémoire (Float32 interleaved) |
| `AudioLoader` | charge / décode un fichier en `AudioSample` (`Load`, `LoadFromMemory`, `Free`) |
| `AudioGenerator` | synthétise des sons sans fichier (`GenerateTone`, `GenerateChord`, …) |
| `VoiceParams` | paramètres de lecture (volume, pitch, pan, boucle, bus) |
| `AudioHandle` | identifiant léger d'une voix en cours, pour la contrôler |

> Le moteur normalise tout en **Float32 interleaved stéréo** au chargement. Tu n'as
> jamais à manipuler les octets bruts toi-même.

---

## 2. Initialiser le moteur

`AudioEngine` est un **singleton** : on y accède toujours par
`AudioEngine::Instance()`. On l'initialise une fois au démarrage, on l'éteint une
fois à la fin.

```cpp
#include "NKAudio/NKAudio.h"
using namespace nkentseu;
using namespace nkentseu::audio;

// Au démarrage de l'application :
if (!AudioEngine::Instance().Initialize()) {
    // Pas de carte son, pilote occupé, machine sans audio (CI, serveur)…
    // -> on continue SANS audio, le jeu reste jouable.
    NK_LOG_WARN("Audio indisponible — désactivé");
}

// ... le jeu tourne ...

// À la fermeture :
AudioEngine::Instance().Shutdown();
```

`Initialize()` accepte une configuration optionnelle. Sans argument, il prend les
réglages par défaut (48 kHz, stéréo, 256 voix). Pour personnaliser :

```cpp
AudioEngineConfig cfg;
cfg.sampleRate   = 48000;                       // qualité studio
cfg.channels     = 2;                           // stéréo
cfg.maxVoices    = 256;                          // sons simultanés
cfg.masterVolume = 1.0f;
cfg.backend      = AudioBackendType::AUTO;       // choisit WASAPI/CoreAudio/ALSA/… seul
AudioEngine::Instance().Initialize(cfg);
```

> **Échec gracieux.** `Initialize()` **retourne `false`** si aucun périphérique de
> sortie n'est disponible. Teste toujours la valeur de retour et garde une logique
> de jeu qui fonctionne sans son. Le pattern réel du dépôt (Mú) :
>
> ```cpp
> if (!AudioEngine::Instance().Initialize()) {
>     mInitialized = false;     // on note l'échec
>     return false;             // et toutes les fonctions audio deviennent des no-op
> }
> mInitialized = true;
> ```
>
> Ensuite chaque méthode audio commence par `if (!mInitialized) return;`.

Quelques informations utiles après initialisation :

```cpp
AudioEngine& eng = AudioEngine::Instance();
const char* name = eng.GetBackendName();   // ex. "WASAPI"
int32 sr         = eng.GetSampleRate();
int32 voices     = eng.GetActiveVoices();
float32 latency  = eng.GetLatencyMs();
```

---

## 3. Jouer un son court

Un son court suit trois temps : **charger** un `AudioSample`, le **jouer**, puis
le **libérer** quand on n'en a plus besoin.

```cpp
// 1) Charger depuis le disque -> données PCM en mémoire.
AudioSample shoot = AudioLoader::Load("assets/sfx/shoot.wav");
if (!shoot.IsValid()) {
    NK_LOG_WARN("shoot.wav introuvable ou illisible");
}

// 2) Jouer (paramètres par défaut : volume 1.0, bus "SFX").
AudioEngine::Instance().Play(shoot);

// 3) Plus tard, quand on n'en a plus besoin (fin de niveau, fermeture) :
AudioLoader::Free(shoot);
```

`AudioLoader::Load` reconnaît le format depuis l'extension / les magic bytes :
**WAV, MP3, OGG Vorbis, FLAC**. La sortie est toujours normalisée en Float32.

> ⚠️ **Règle capitale : le `AudioSample` doit rester vivant pendant toute la
> lecture.** `Play()` ne **copie pas** les données — il référence celles du sample
> (sa signature est `Play(const AudioSample& sample, …)` et le commentaire du
> header précise : *« doit rester valide pendant la lecture »*). Si tu libères ou
> détruis le sample alors qu'une voix le joue encore, tu lis de la mémoire morte.
>
> En pratique : **charge tes SFX une fois** au démarrage, range-les dans un tableau
> qui vit aussi longtemps que le jeu, et ne les libère qu'à `Shutdown`.

Pré-charger une banque de sons, puis les jouer à la volée :

```cpp
struct SfxBank {
    AudioSample shoot, explosion, coin;

    void Load() {
        shoot     = AudioLoader::Load("assets/sfx/shoot.wav");
        explosion = AudioLoader::Load("assets/sfx/explosion.ogg");
        coin      = AudioLoader::Load("assets/sfx/coin.wav");
    }
    void Free() {
        AudioLoader::Free(shoot);
        AudioLoader::Free(explosion);
        AudioLoader::Free(coin);
    }
};

SfxBank g_sfx;                 // vit toute la durée du jeu
g_sfx.Load();                  // après Initialize()
// ...
AudioEngine::Instance().Play(g_sfx.coin);   // à chaque pièce ramassée
// ...
g_sfx.Free();                  // avant Shutdown()
```

> Tu peux relancer le **même** sample autant de fois que tu veux : chaque `Play`
> prend une voix libre du pool (256 au maximum). Inutile de recharger le fichier.

---

## 4. VoiceParams — contrôler la lecture

`Play()` accepte un second argument `VoiceParams` qui décrit **comment** jouer le
son. Voici les champs utiles avec leurs valeurs par défaut :

```cpp
VoiceParams p;
p.volume  = 1.0f;     // [0, 1+]  — 1 = volume nominal, >1 amplifie
p.pitch   = 1.0f;     // multiplicateur de hauteur (2.0 = une octave plus aigu)
p.pan     = 0.0f;     // panoramique [-1 = gauche, 0 = centre, +1 = droite]
p.looping = false;    // true = lecture en boucle
p.bus     = "SFX";    // bus de routage : "SFX" / "Music" / "Voice" / "UI"

AudioEngine::Instance().Play(g_sfx.explosion, p);
```

Quelques recettes courantes :

```cpp
// Son plus discret, légèrement à droite.
VoiceParams p; p.volume = 0.4f; p.pan = 0.5f;
AudioEngine::Instance().Play(g_sfx.coin, p);

// Variation de hauteur aléatoire pour éviter la répétition mécanique d'un SFX.
VoiceParams p; p.pitch = 0.9f + 0.2f * frand();   // entre 0.9 et 1.1
AudioEngine::Instance().Play(g_sfx.shoot, p);

// Un son d'interface routé sur le bus UI.
VoiceParams p; p.bus = "UI";
AudioEngine::Instance().Play(g_sfx.click, p);
```

### Les bus

Le moteur crée automatiquement à l'initialisation une hiérarchie de bus :
**Master → SFX, Music, Voice, UI**. Le champ `VoiceParams::bus` décide où passe
le son. Le volume effectif d'une voix est cascadé :

```
volume effectif = voix.volume × bus.volume × … × Master.volume
```

Cela permet, par exemple, de baisser **toute** la musique sans toucher aux effets
(voir §6).

### Contrôler une voix après coup : `AudioHandle`

`Play()` **retourne** un `AudioHandle`, un identifiant léger qui te permet d'agir
sur cette voix précise tant qu'elle joue :

```cpp
AudioHandle h = AudioEngine::Instance().Play(g_sfx.engine, p);

// Plus tard, sur CETTE voix :
AudioEngine::Instance().SetVolume(h, 0.5f);
AudioEngine::Instance().SetPitch(h, 1.2f);
AudioEngine::Instance().SetPan(h, -0.3f);

if (AudioEngine::Instance().IsPlaying(h)) { /* ... */ }

AudioEngine::Instance().Stop(h);              // arrêt immédiat
AudioEngine::Instance().Stop(h, 0.5f);        // arrêt avec fondu de 0.5 s
```

Pour un son court « one-shot » (un tir, une pièce), tu peux ignorer le handle : la
voix se libère toute seule en fin de lecture. Garde le handle seulement si tu veux
piloter le son (musique, moteur de voiture, son qui boucle…).

---

## 5. Musique de fond

La musique, c'est un son long qu'on joue **en boucle** sur le bus `"Music"`. On
garde le `AudioSample` ET le `AudioHandle` pour pouvoir l'arrêter, la mettre en
pause, ou changer son volume.

```cpp
class MusicPlayer {
    AudioSample mSample{};
    AudioHandle mHandle = AUDIO_HANDLE_INVALID;
    bool        mHasMusic = false;

public:
    void Play(const char* path, float32 volume) {
        Stop();                                   // coupe la piste précédente

        mSample = AudioLoader::Load(path);
        if (!mSample.IsValid()) {
            NK_LOG_WARN("musique introuvable");
            return;
        }
        mHasMusic = true;

        VoiceParams p;
        p.looping = true;       // <- la musique tourne en boucle
        p.volume  = volume;
        p.bus     = "Music";    // <- routée sur le bus Music
        mHandle = AudioEngine::Instance().Play(mSample, p);
    }

    void Stop() {
        if (mHasMusic) {
            AudioEngine::Instance().Stop(mHandle);
            AudioLoader::Free(mSample);   // le sample n'est plus joué -> libérable
            mHasMusic = false;
        }
        mHandle = AUDIO_HANDLE_INVALID;
    }

    void Pause()  { if (mHasMusic) AudioEngine::Instance().Pause(mHandle);  }
    void Resume() { if (mHasMusic) AudioEngine::Instance().Resume(mHandle); }
};
```

> On **ne libère le sample qu'après avoir arrêté la voix** (`Stop` puis `Free`).
> L'ordre inverse libérerait des données encore lues par le mixeur — exactement le
> piège du §3.

### Changer de musique en douceur (crossfade)

Pour passer d'une ambiance à une autre sans coupure sèche, le moteur fournit
`PlayMusicCrossfade` : il fond la musique courante du bus `"Music"` et fait entrer
la nouvelle en même temps.

```cpp
AudioSample battle = AudioLoader::Load("assets/music/battle.ogg");
VoiceParams p; p.looping = true; p.volume = 0.8f;   // (bus "Music" implicite ici)
AudioEngine::Instance().PlayMusicCrossfade(battle, /*fadeTime=*/2.0f, p);
// 'battle' doit rester vivant tant que la musique joue.
```

---

## 6. Contrôler le volume

Il y a deux niveaux de réglage.

**Par voix** — agir sur un son précis via son handle :

```cpp
AudioEngine::Instance().SetVolume(musicHandle, 0.3f);   // baisse cette musique
```

**Volume global (master)** — affecte tout ce qui sort du moteur :

```cpp
AudioEngine::Instance().SetMasterVolume(0.7f);          // 70 % du volume général
float32 v = AudioEngine::Instance().GetMasterVolume();
```

Un schéma classique « réglages du jeu » combine un volume **par catégorie**
(stocké côté application) appliqué au moment du `Play`, et un master global :

```cpp
struct AudioSettings {
    float32 master = 1.0f;
    float32 music  = 0.8f;
    float32 sfx    = 1.0f;

    float32 EffectiveSfx()   const { return sfx   * master; }
    float32 EffectiveMusic() const { return music * master; }
};
AudioSettings g_settings;

// À chaque SFX :
VoiceParams p; p.volume = g_settings.EffectiveSfx(); p.bus = "SFX";
AudioEngine::Instance().Play(g_sfx.coin, p);

// Quand l'utilisateur bouge le curseur « musique » :
AudioEngine::Instance().SetVolume(musicHandle, g_settings.EffectiveMusic());
```

> Tu peux aussi régler le volume d'un **bus entier** (tous les SFX, toute la
> musique) via `GetBus("Music")` / `GetMasterBus()`, qui renvoient un `NkAudioBus*`.
> C'est l'équivalent FMOD/Wwise du mixage par catégorie.

---

## 7. Générer des sons en code (sans fichier)

`AudioGenerator` synthétise des `AudioSample` à la volée. C'est idéal pour
prototyper, pour des effets « 8-bit », ou simplement quand on n'a pas encore les
fichiers audio. Chaque fonction **retourne un `AudioSample`** qu'il faudra libérer
avec `AudioLoader::Free`, exactement comme un sample chargé.

### Un ton simple (oscillateur)

```cpp
// Un "bip" sinusoïdal : 880 Hz, 0.09 s, amplitude 0.55.
AudioSample bip = AudioGenerator::GenerateTone(
    880.0f, 0.09f, WaveformType::SINE, 44100, 0.55f);
```

Formes d'onde disponibles (`WaveformType`) : `SINE`, `SQUARE`, `TRIANGLE`,
`SAWTOOTH`, `PULSE`, `NOISE_WHITE`, `NOISE_PINK`.

### Une enveloppe ADSR pour rendre le son naturel

Un ton brut « claque ». Une enveloppe **ADSR** (Attack / Decay / Sustain /
Release) lui donne une montée et une descente douces. On l'applique **in-place**
sur le sample :

```cpp
AdsrEnvelope env;
env.attackTime   = 0.004f;   // montée très rapide
env.decayTime    = 0.04f;    // chute vers le sustain
env.sustainLevel = 0.2f;     // niveau tenu [0,1]
env.releaseTime  = 0.05f;    // extinction
AudioGenerator::ApplyEnvelope(bip, env);
```

### Un accord (synthèse additive)

```cpp
// Carillon de succès : un accord brillant G5 - C6 - E6.
const float32 freqs[3] = { 784.0f, 1047.0f, 1319.0f };
AudioSample good = AudioGenerator::GenerateChord(freqs, 3, /*duration=*/0.28f);
AdsrEnvelope e; e.attackTime = 0.005f; e.decayTime = 0.10f;
               e.sustainLevel = 0.5f;  e.releaseTime = 0.14f;
AudioGenerator::ApplyEnvelope(good, e);
```

### Un chirp (balayage de fréquence)

Utile pour un « boing », une montée de score, un effet SFX :

```cpp
// Descente douce 440 -> 230 Hz : petit "boing" gentil.
AudioSample bad = AudioGenerator::GenerateChirp(440.0f, 230.0f, /*duration=*/0.22f);
AudioGenerator::ApplyEnvelope(bad, env);
```

### Bruit et percussions toutes faites

```cpp
AudioSample whoosh = AudioGenerator::GenerateNoise(0.3f, WaveformType::NOISE_WHITE);
AudioSample kick   = AudioGenerator::GenerateKick();    // FM descendante + click
AudioSample snare  = AudioGenerator::GenerateSnare();
AudioSample hat    = AudioGenerator::GenerateHihat(0.05f, /*closed=*/true);
```

### Pattern réel : une banque de SFX procéduraux

Voici l'idiome du jeu Mú — on génère toute la banque une fois à l'init, on la
range, on la joue, on la libère à la fin :

```cpp
void GenerateSfxBank() {
    auto env = [](float32 a, float32 d, float32 s, float32 r) {
        AdsrEnvelope e; e.attackTime=a; e.decayTime=d;
                        e.sustainLevel=s; e.releaseTime=r; return e;
    };

    // "Tap" : petit pop clair.
    mTap = AudioGenerator::GenerateTone(880.0f, 0.09f, WaveformType::SINE, 44100, 0.55f);
    AudioGenerator::ApplyEnvelope(mTap, env(0.004f, 0.04f, 0.2f, 0.05f));

    // "Win" : fanfare (accord large).
    const float32 win[4] = { 523.0f, 659.0f, 784.0f, 1047.0f };  // C5-E5-G5-C6
    mWin = AudioGenerator::GenerateChord(win, 4, 0.6f);
    AudioGenerator::ApplyEnvelope(mWin, env(0.01f, 0.18f, 0.6f, 0.22f));
}

void PlayTap() {
    if (!mTap.IsValid()) return;
    VoiceParams p; p.volume = g_settings.EffectiveSfx(); p.bus = "SFX";
    AudioEngine::Instance().Play(mTap, p);
}

void FreeBank() {
    if (mTap.IsValid()) AudioLoader::Free(mTap);
    if (mWin.IsValid()) AudioLoader::Free(mWin);
}
```

> Comme un sample chargé, un sample **généré** doit rester vivant pendant sa
> lecture et se libère avec `AudioLoader::Free`. Même règle, même piège.

---

## 8. Aperçu : streaming des longs fichiers

Charger une musique de 4 minutes entièrement en RAM marche, mais coûte cher. Pour
les longs fichiers, NKAudio propose une lecture **« pull » par morceaux** via
l'interface `IAudioStream` (header `NKAudio/Streaming/NkAudioStream.h`).

```cpp
#include "NKAudio/Streaming/NkAudioStream.h"
using namespace nkentseu::audio;

// La factory choisit l'implémentation selon le format (WAV chunked, etc.).
IAudioStream* music = OpenAudioStream("assets/music/ambient.wav");
if (music) {
    float32 buf[1024 * 2];                              // 1024 frames stéréo
    int32 got = music->ReadFrames(buf, 1024);          // lit à la demande
    music->Seek(0);                                     // revenir au début
    bool fini = music->IsEOF();

    delete music;   // ⚠ la factory documente une libération par delete
}
```

`IAudioStream` expose : `ReadFrames`, `Seek`, `GetFrameCount`, `GetSampleRate`,
`GetChannels`, `IsEOF`. En production, un `AudioStreamPlayer` lance un thread
worker qui remplit un ring buffer (loop + crossfade EOF gérés). Le streaming est
un sujet avancé ; pour la plupart des jeux, charger la musique avec
`AudioLoader::Load` + `Play(looping)` (voir §5) suffit largement.

---

## 9. Libérer la mémoire — la règle NKMemory

Tout `AudioSample` (chargé **ou** généré) possède un buffer PCM alloué par un
**allocateur NKMemory**. On le libère avec :

```cpp
AudioLoader::Free(sample);
```

> **Ne fais JAMAIS** `delete sample.data`, `free(sample.data)` ni `delete[]`
> dessus. Mélanger l'allocateur custom de Nkentseu avec le heap CRT provoque une
> **corruption de tas** Windows (`c0000374`). C'est la même règle d'or que dans le
> guide [NKMemory](01-NKMemory.md) : *ce qui est alloué par NKMemory se libère par
> NKMemory*. `AudioLoader::Free` utilise le bon allocateur — celui qui a servi au
> `Load` / au `Generate`.

Bonnes pratiques de cycle de vie :

- **Charge tôt, libère tard.** Précharge tes SFX/banques après `Initialize()`,
  libère-les juste avant `Shutdown()`.
- **N'arrête puis ne libère.** Pour une musique ou un son qui boucle : `Stop(h)`
  **puis** `Free(sample)` — jamais l'inverse.
- **`Shutdown()` ne libère pas tes samples.** Il arrête les voix et ferme le
  backend ; les `AudioSample` que *tu* as créés restent à *ta* charge.
- Vérifie `sample.IsValid()` avant de jouer ou de libérer (un chargement qui
  échoue renvoie un sample invalide, jamais un crash).

---

## 10. Spécificités plateformes

NKAudio choisit automatiquement le backend natif (`AudioBackendType::AUTO`). Tu
n'as rien à coder de spécifique : seule la **déclaration Jenga** change les libs
système liées. Le module `NKAudio.jenga` le fait déjà pour toi :

| Plateforme | Backend | Libs système liées |
|------------|---------|--------------------|
| Windows | WASAPI (+ DirectSound legacy) | `ole32`, `winmm`, `avrt` |
| macOS / iOS | CoreAudio | `AudioToolbox.framework`, `CoreAudio.framework` |
| Linux | ALSA | `asound`, `pthread`, `m` |
| Android | AAudio (8.0+) / OpenSL ES (fallback) | `dl`, `OpenSLES` |
| HarmonyOS | backend OHOS | `dl` |
| Web | WebAudio (Emscripten) | — |
| (tests / serveur) | Null | — |

> **Échec gracieux (rappel important).** Sur une machine sans carte son (CI,
> conteneur, serveur), `Initialize()` renvoie `false`. Le moteur ne crashe pas —
> à toi de désactiver l'audio et de laisser le reste tourner. C'est exactement le
> pattern montré au §2 (`mInitialized = false` → toutes les fonctions audio
> deviennent des no-op).

---

## 11. Récapitulatif

- `AudioEngine::Instance().Initialize()` au démarrage, `.Shutdown()` à la fin ;
  teste le retour de `Initialize` pour un **échec gracieux**.
- **Son court** : `AudioLoader::Load` → `AudioEngine::Instance().Play` → garder le
  sample **vivant** pendant la lecture → `AudioLoader::Free` à la fin.
- `VoiceParams` règle `volume`, `pitch`, `pan`, `looping`, `bus`
  (`SFX`/`Music`/`Voice`/`UI`). `Play` renvoie un `AudioHandle` pour piloter la
  voix (`SetVolume`/`Stop`/`Pause`…).
- **Musique** : `Load` + `Play` avec `looping=true`, `bus="Music"` ; `Pause` /
  `Resume` ; `PlayMusicCrossfade` pour changer de piste en douceur.
- **Volume** : par voix (`SetVolume(handle, v)`) et global
  (`SetMasterVolume(v)`).
- **Génération** : `AudioGenerator::GenerateTone / GenerateChord / GenerateChirp /
  GenerateNoise / GenerateKick…` + `ApplyEnvelope(sample, AdsrEnvelope)` — sans
  aucun fichier.
- **Mémoire** : tout sample se libère avec `AudioLoader::Free`, **jamais**
  `delete`/`free` (corruption de tas). Voir [NKMemory](01-NKMemory.md).

### Dépendances Jenga

Ajoute `"NKAudio"` à ton `nkentseudependson` :

```python
with project("MonJeu"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKWindow", "NKEvent", "NKCanvas", "NKAudio",
         "NKMemory", "NKCore", "NKLogger"],
        extra_includes=["src"],
    )
```

Les libs système par plateforme (Windows `avrt`/`winmm`, Android `OpenSLES`+`dl`,
Linux `asound`…) sont déclarées **dans** `NKAudio.jenga` et propagées
automatiquement — tu n'as rien à ajouter.

---

Étape suivante : [NKUI](07-NKUI.md) pour l'interface (et y brancher des sons de
clic). Retour à l'[index des guides](README.md).
