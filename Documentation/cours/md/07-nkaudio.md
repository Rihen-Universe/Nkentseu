# NKAudio : faire du bruit

Un bouton qui ne fait aucun bruit quand on clique dessus paraît cassé. C'est
irrationnel et c'est ainsi. Ce chapitre vous donne les moyens de corriger cela
en une dizaine de lignes, puis explique tout ce qu'il y a derrière — parce que
le module va beaucoup plus loin qu'un simple « jouer un fichier ».

`NKAudio` est, des cinq modules d'intégration, celui dont la façade est
la plus aboutie. Un seul en-tête suffit :

**`L'unique inclusion nécessaire`**

```cpp
#include "NKAudio/NkAudio.h"
```

et tout vit dans le *namespace* `nkentseu::audio`.

## Le module, et sa dépendance surprenante

**`Kernel/Runtime/NKAudio/NKAudio.jenga:30-35`**

```cpp
    nkentseudependson(
        # NKMedia : decodeur Opus from-scratch (codec .opus via NkOpusCodec).
        ["NKPlatform", "NKCore", "NKMemory", "NKContainers", "NKLogger", "NKThreading", "NKFileSystem", "NKStream", "NKMedia"],
        selfexport="NKAudio",
        extra_includes=["src"],
    )
```

`NKAudio` dépend de `NKMedia`, qui dépend lui-même de
`NKImage`. La raison est mince — le décodeur Opus vit dans NKMedia — mais
elle est réelle : dans l'ordre de construction, NKImage vient d'abord, puis
NKMedia, puis NKAudio. Nous enseignons l'audio avant la vidéo parce que la
façade audio est plus simple, mais retenez la direction de la flèche : c'est
l'audio qui appelle la vidéo, jamais l'inverse.

**`Kernel/Runtime/NKAudio/ — arborescence (abrégée)`**

```
NKAudio/
  src/NKAudio/
    NkAudio.h            (1427 l.)  <- EN-TETE PUBLIC UNIQUE : tout est la
    NkAudioEngineCore.cpp           <- AudioEngine (PIMPL)
    NkAudioLoader.cpp               <- AudioLoader (WAV natif + dispatch codecs)
    NkAudioMixer.cpp · NkAudioGenerator.cpp · NkAudioAnalyzer.cpp
    NkAudioEffects.{h,cpp}          <- effets DSP concrets
    NkAudioBackends.{h,cpp}         <- WASAPI / CoreAudio / ALSA / AAudio / WebAudio / Null
    NkAudioBus.{h,cpp}              <- bus hierarchiques Master -> SFX/Music/Voice/UI
    NkAudioCapture.{h,cpp}          <- MICRO (entree)
    Codecs/  MP3/ · OGG/ · FLAC/ · Opus/
    Streaming/
      NkAudioStream.{h,cpp}         <- IAudioStream (pull) + WavStream/MemoryStream
      NkAudioStreamPlayer.{h,cpp}   <- thread decodeur + ring buffer
      NkContainerAudioStream.{h,cpp}<- piste audio d'un conteneur video
```

## L'invariant fondateur : un seul format interne

Avant toute chose, comprenez ce que le module fait de vos fichiers :

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:258-260`**

```
 @note Format interne normalisé : Float32 interleaved stéréo.
       La conversion est faite au chargement.
```

Peu importe que votre fichier soit un WAV 8 bits mono à 22 050 Hz ou un FLAC
24 bits à 96 000 Hz : après `AudioLoader::Load`, vous avez des flottants
32 bits entrelacés. Cela simplifie énormément la suite — le mixeur n'a qu'un seul
cas à traiter — et cela signifie qu'un fichier chargé occupe souvent plus de
mémoire que sur le disque.

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:262-284`**

```cpp
    struct AudioSample {
            float32 *data = nullptr;   ///< Samples Float32 (interleaved)
            usize frameCount = 0;      ///< Nombre de frames (samples/channels)
            int32 sampleRate = 48000;
            int32 channels = 2;
            AudioFormat format = AudioFormat::UNKNOWN;
            float32 GetDuration() const;
            usize   GetSampleCount() const;
            bool    IsValid() const;
            memory::NkAllocator *mAllocator = nullptr; ///< Allocateur responsable
    };
```

Notez le vocabulaire, qu'il faut fixer une fois pour toutes : une
*`frame`* est un instant, un *`sample`* est une valeur.
En stéréo, une *frame* contient deux *samples*. `frameCount`
compte les instants, pas les nombres — d'où
`GetSampleCount() = frameCount * channels`.

Notez aussi `mAllocator` : chaque `AudioSample` se souvient de qui
l'a alloué. Nous verrons pourquoi c'est vital.

## Le patron canonique en cinq temps

L'application `NkAudioPlayer` ouvre son fichier principal sur la recette,
et il n'y a rien à y ajouter :

**`Applications/NkAudioPlayer/src/main.cpp:2-9`**

```cpp
// Le patron CORRECT pour lire un fichier audio vers les haut-parleurs, ET l'afficher :
//   1. AudioEngine::Instance().Initialize()  -> ouvre le device + thread de mixage
//   2. AudioLoader::Load(path)               -> décode WAV/MP3/OGG/FLAC/Opus en AudioSample
//   3. engine.Play(sample, params)           -> lance une voix (bus "Music")
//   4. boucle : GetPlaybackPosition -> tête de lecture sur la FORME D'ONDE dessinée
//   5. engine.Shutdown()
```

### Temps 1 — initialiser le moteur

**`Applications/NkAudioPlayer/src/main.cpp:113-121`**

```cpp
    // ── 1) Moteur audio ───────────────────────────────────────────────────────
    audio::AudioEngine &engine = audio::AudioEngine::Instance();
    audio::AudioEngineConfig cfg;
    if (!engine.Initialize(cfg)) {
        logger.Error("[NkAudioPlayer] AudioEngine::Initialize a echoue (device indisponible ?)");
        return -2;
    }
    logger.Info("[NkAudioPlayer] device reel : {0} Hz, {1} canaux", engine.GetSampleRate(), engine.GetChannels());
```

`AudioEngine` est un **singleton** : `Instance()` rend
toujours la même. `Initialize` ouvre le périphérique de sortie et lance le
fil de mixage.

La ligne de journalisation n'est pas décorative. Le périphérique impose son taux
d'échantillonnage et son nombre de canaux ; votre `cfg` n'exprime qu'un
souhait. **Lisez `GetSampleRate()` et `GetChannels()` après
`Initialize()`, jamais avant.**

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:396-411`**

```cpp
    struct AudioEngineConfig {
            AudioBackendType backend = AudioBackendType::AUTO;
            int32 sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;   // 48000
            int32 channels   = AUDIO_DEFAULT_CHANNELS;      // 2
            int32 bufferSize = AUDIO_DEFAULT_BUFFER_SIZE;
            int32 maxVoices  = AUDIO_MAX_VOICES;
            bool enableHrtf = false;    bool enableDoppler = true;
            float32 masterVolume = 1.0f;
            memory::NkAllocator *allocator = nullptr;       ///< nullptr = allocateur global
            bool enableMasterLimiter = true;
            float32 masterLimiterThresholdDb = -0.5f;
            ResamplingQuality resamplingQuality = ResamplingQuality::LINEAR;
    };
```

Les valeurs par défaut conviennent. **Ne touchez pas au champ
`backend`** — nous verrons dans un instant pourquoi.

> **✅ Un seul `Initialize`, un seul `Shutdown`**
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1071-1073`**
>
> ```
>  @note Initialize() et Shutdown() sont appelés une seule fois
>        depuis le thread principal. Toutes les autres méthodes
>        sont thread-safe via des atomics et queues lock-free.
> ```
>
> Les contrats sont formels : `Initialize` exige
> `!IsInitialized()`, `Shutdown` exige `IsInitialized()`
> (`NkAudio.h:1100-1111`). Toutes les *autres* méthodes, elles, sont
> appelables de n'importe où.

### Temps 2 — décoder le fichier

**`Applications/NkAudioPlayer/src/main.cpp:115-121`**

```cpp
    // ── 2) Charge/decode le fichier ───────────────────────────────────────────
    audio::AudioSample sample = audio::AudioLoader::Load(path.CStr());
    if (!sample.IsValid()) {
        logger.Error("[NkAudioPlayer] impossible de charger/decoder : {0}", path.CStr());
        engine.Shutdown();
        return -3;
    }
```

`AudioLoader` est entièrement statique :

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:583-648`**

```cpp
static AudioFormat DetectFormat(const uint8* data, usize size);
static AudioFormat DetectFormatFromPath(const char* path);
static AudioSample Load(const char* path, memory::NkAllocator* allocator = nullptr);
static AudioSample LoadFromMemory(const uint8* data, usize size,
                                  AudioFormat format = AudioFormat::UNKNOWN,
                                  memory::NkAllocator* allocator = nullptr);
static bool SaveWAV(const char* path, const AudioSample& sample);
static void Free(AudioSample& sample);
static void ConvertSampleFormat(AudioSample& sample, SampleFormat target);
static void Resample(AudioSample& sample, int32 targetSampleRate, bool highQuality = true);
static void ConvertChannels(AudioSample& sample, int32 targetChannels);
```

`Load` détecte le format par le contenu, pas par l'extension : renommer un
MP3 en `.wav` ne le trompe pas.

### Temps 3 — lancer une voix

**`Applications/NkAudioPlayer/src/main.cpp:158-162`**

```cpp
    // ── 4) Joue + prepare la forme d'onde ─────────────────────────────────────
    audio::VoiceParams vp;
    vp.bus = "Music";
    vp.volume = 1.0f;
    audio::AudioHandle handle = engine.Play(sample, vp);
```

Une *voix* est une instance de lecture. Jouer trois fois le même
`AudioSample` crée trois voix indépendantes, chacune avec son volume, sa
position de lecture et son *handle*.

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:346-357`**

```cpp
    struct VoiceParams {
            float32 volume = 1.0f;      float32 pitch = 1.0f;    float32 pan = 0.0f;
            bool looping = false;       float32 loopStart = 0.0f; float32 loopEnd = -1.0f;
            float32 fadeInTime = 0.0f;  float32 startOffset = 0.0f;
            int32 priority = 128;
            AudioSource3D source3d;
            const char *bus = "SFX";    ///< Bus de routage (SFX/Music/Voice/UI)
    };
```

Le `AudioHandle` est un simple entier enveloppé
(`NkAudio.h:227-249`), avec un `IsValid()` et une conversion
implicite en `bool`. Il sert à piloter la voix après coup :

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1145-1166`**

```cpp
void Stop(AudioHandle handle, float32 fadeOutTime = 0.0f);
void Pause(AudioHandle handle);  void Resume(AudioHandle handle);
bool IsPlaying(AudioHandle) const; bool IsPaused(AudioHandle) const; bool IsLooping(AudioHandle) const;
void SetVolume/SetPitch/SetPan/SetLooping(AudioHandle, ...);
float32 GetVolume/GetPitch/GetPan(AudioHandle) const;
float32 GetPlaybackPosition(AudioHandle) const;
void    SetPlaybackPosition(AudioHandle, float32 seconds);
```

> **⚠️ `Play` peut échouer sans rien dire**
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1125`**
>
> ```
>  @return Handle de contrôle, invalide si pool plein
> ```
>
> Le nombre de voix simultanées est plafonné par `maxVoices`. Passé ce
> seuil, `Play` renvoie un *handle* invalide et rien ne se joue. Si
> vous comptez réutiliser le *handle*, testez `h.IsValid()`. Si vous
> n'en avez pas besoin — un son de clic, par exemple — ignorer le retour est
> parfaitement légitime.

### Temps 4 — piloter pendant la lecture

**`Applications/NkAudioPlayer/src/main.cpp:185-191`**

```cpp
                else if (k->GetKey() == NkKey::NK_SPACE) {
                    paused = !paused;
                    if (paused)
                        engine.Pause(handle);
                    else
                        engine.Resume(handle);
                }
```

et la détection de fin naturelle :

**`Applications/NkAudioPlayer/src/main.cpp:254-255`**

```cpp
        if (!engine.IsPlaying(handle) && !paused)
            running = false; // fin naturelle
```

Notez la condition composée : une voix en pause n'est pas « en train de jouer ».
Sans le `&& !paused`, appuyer sur la barre d'espace fermerait
l'application.

### Temps 5 — l'ordre de fermeture, qui n'est pas négociable

**`Applications/NkAudioPlayer/src/main.cpp:258-259`**

```cpp
    engine.Shutdown();
    audio::AudioLoader::Free(sample);
```

> **⚠️ `Shutdown()` d'abord, `Free()` ensuite. Toujours.**
>
> La documentation de `Play` pose le contrat :
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1123`**
>
> ```
>  @param sample  Sample source (doit rester valide pendant la lecture)
> ```
>
> `Play` ne copie pas vos échantillons : la voix lit *directement* dans
> `sample.data`, depuis le fil audio. Libérer le `AudioSample` avant
> d'avoir arrêté le moteur, c'est laisser un fil temps réel lire de la mémoire
> rendue au système.
>
> Le symptôme est particulièrement déplaisant : cela ne plante pas toujours. Selon
> ce qui a été réalloué à cet endroit, vous obtiendrez un grésillement, un bruit
> blanc, un silence — ou un plantage, mais dix secondes plus tard, ailleurs.
>
> **Retenez la séquence : arrêter le moteur, puis libérer les données.**
> Toujours dans cet ordre, dans les cinq chemins de sortie de votre programme.

Et la libération elle-même a sa règle :

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:284, 620`**

```
memory::NkAllocator *mAllocator = nullptr; ///< Allocateur responsable (jamais nullptr si IsValid())
 @note Doit être appelé avec le même allocateur utilisé pour Load()
```

C'est la même règle que pour NKImage : jamais `delete`, jamais
`free`. Un `IAudioStream` qu'on n'a confié à personne se détruit
avec `memory::NkGetDefaultAllocator().Delete(stream)` — le dépôt le note
explicitement, avec la mention du plantage `c0000374` en cas de mélange
(`Applications/NkAudioDemo/src/main.cpp:44`).

## Jouer un son au clic d'un bouton

Voici l'objectif annoncé. Tout est en place ; il ne reste qu'à assembler.

### Au démarrage — une seule fois

**`Exemple écrit pour le cours (API : NkAudio.h:1091, 1104, 595)`**

```cpp
#include "NKAudio/NkAudio.h"
using namespace nkentseu;

audio::AudioEngine &engine = audio::AudioEngine::Instance();
if (!engine.Initialize(audio::AudioEngineConfig{})) {
    logger.Error("[UI] audio indisponible : les sons d'interface seront muets");
    // ... mais l'application continue : le son n'est pas vital.
}
audio::AudioSample clic = audio::AudioLoader::Load("Resources/Audio/clic.wav");
if (!clic.IsValid())
    logger.Warn("[UI] son de clic introuvable");
```

Remarquez la posture : l'échec de l'audio ne doit pas empêcher l'application de
démarrer. Une carte son occupée, un périphérique débranché, une machine sans
sortie : ce sont des situations normales.

### Dans la frame — quand le widget renvoie `true`

**`Exemple écrit pour le cours (API : NkGuiWidgets.h:44, NkAudio.h:1127)`**

```cpp
if (nkgui::Button(ctx, "Valider")) {
    audio::VoiceParams vp;
    vp.bus = "UI";                       // bus dedie : volume UI reglable a part
    engine.Play(clic, vp);               // handle ignore : son court, « fire and forget »
    // ... l'action du bouton ...
}
```

Trois lignes. C'est tout — et c'est possible parce que `Play` est
*non bloquant* : il inscrit la voix dans le mixeur et rend la main
immédiatement. Votre frame ne ralentit pas.

### À la fermeture

**`Exemple écrit pour le cours (API : NkAudio.h:1114, 622)`**

```cpp
engine.Shutdown();
audio::AudioLoader::Free(clic);
```

> **✅ Pourquoi le bus « UI »**
>
> Ranger les sons d'interface sur leur propre bus n'est pas de la coquetterie :
> cela permet à l'utilisateur de les couper sans toucher à la musique, par un
> simple `engine.GetBus("UI")->SetMute(true)`. Si vous les mettez sur le bus
> « SFX » par défaut, vous ne pourrez plus les distinguer des bruitages du jeu.
> Le coût de ce choix est nul ; le coût de sa correction plus tard ne l'est pas.

## Les bus : régler des groupes de sons ensemble

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1246-1251`**

```
 Le bus Master est cree automatiquement a Initialize() avec ses
 4 sous-buses standard : SFX (default), Music, Voice, UI.

 Volume effectif d'une voix = voix.volume * bus.volume *
                              bus.parent.volume * ... * Master.volume.
```

Les quatre bus existent dès `Initialize()` : vous n'avez rien à créer.
Écrire `vp.bus = "Music"` suffit.

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1255-1276`**

```cpp
NkAudioBus* GetMasterBus();
NkAudioBus* GetBus(const char* name);
NkAudioBus* GetOrCreateBus(const char* name, NkAudioBus* parent = nullptr);
```

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudioBus.h:53-137 (abrégé)`**

```cpp
static constexpr int32 MAX_EFFECTS = 8;
static constexpr int32 MAX_CHILDREN = 16;
static constexpr int32 MAX_NAME_LEN = 32;

NkAudioBus* GetParent() const noexcept;
NkAudioBus* GetChild(int32 idx) const noexcept;
NkAudioBus* FindDescendant(const char* name) noexcept;
void SetVolume(float32) noexcept;  float32 GetVolume() const noexcept;
float32 GetEffectiveVolume() const noexcept;              // produit recursif
void SetMute(bool) / IsMuted() / SetSolo(bool) / IsSoloed()
bool AddEffect(IAudioEffect*) / RemoveEffect / ClearEffects
void SetSidechainFromBus(NkAudioBus* sourceBus, float32 amount = 0.7f,
                         float32 threshold = 0.05f) noexcept;
```

Un cas d'usage typique — baisser la musique sans toucher aux bruitages :

**`Exemple écrit pour le cours (API : NkAudio.h:1266, NkAudioBus.h:84)`**

```cpp
if (audio::NkAudioBus *bus = engine.GetBus("Music"))
    bus->SetVolume(0.35f);
```

`GetBus` renvoie `nullptr` si le moteur n'est pas initialisé : le
`if` n'est pas facultatif.

Le *sidechain* mérite un mot, parce qu'il résout un problème courant :
`SetSidechainFromBus(voix, 0.7f)` sur le bus Music fait automatiquement
baisser la musique quand un dialogue joue. C'est ce que font tous les jeux, et
cela tient ici en un appel.

Enfin, le fondu croisé entre deux musiques :

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1297`**

```cpp
AudioHandle PlayMusicCrossfade(const AudioSample& newMusic, float32 fadeTime = 2.0f,
                               const VoiceParams& params = VoiceParams{});
```

> **✅ Le limiteur master, et pourquoi le son ne sature jamais**
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:409-411`**
>
> ```cpp
>             bool enableMasterLimiter = true;
>             float32 masterLimiterThresholdDb = -0.5f;
> ```
>
> Un limiteur est actif par défaut sur la sortie. C'est pourquoi mettre un volume
> supérieur à 1,0 ne produit pas de distorsion : le signal est compressé au lieu
> d'être écrêté. C'est un filet de sécurité utile — mais cela veut aussi dire que
> si vous poussez tous vos volumes, vous n'obtiendrez pas « plus fort », vous
> obtiendrez « plus compressé ».

## Les fichiers longs : le streaming

Charger une heure de podcast en `Float32` stéréo à 48 kHz représente
environ 1,4 Go en mémoire. Pour tout ce qui dépasse quelques dizaines de
secondes, il faut décoder au fil de l'eau.

**`Kernel/Runtime/NKAudio/src/NKAudio/Streaming/NkAudioStream.h:42-68`**

```cpp
    class IAudioStream {
            virtual ~IAudioStream() = default;
            virtual int32   ReadFrames(float32* outBuf, int32 maxFrames) noexcept = 0;
            virtual bool    Seek(nk_int64 frameIdx) noexcept = 0;
            virtual nk_int64 GetFrameCount() const noexcept = 0;
            virtual int32   GetSampleRate() const noexcept = 0;
            virtual int32   GetChannels() const noexcept = 0;
            virtual bool    IsEOF() const noexcept = 0;
    };
    IAudioStream* OpenAudioStream(const char* path) noexcept;
```

C'est une interface *pull* : on demande des *frames*, on en reçoit. Il
faut donc quelqu'un pour tirer régulièrement, et à l'avance — c'est le rôle du
lecteur de flux, qui lance un fil décodeur et remplit un tampon circulaire :

**`Kernel/Runtime/NKAudio/src/NKAudio/Streaming/NkAudioStreamPlayer.h:41-135 (abrégé)`**

```cpp
    class AudioStreamPlayer {
            bool Init(int32 sampleRate, int32 channels, int32 ringBufferFrames = 88200) noexcept;
            void Shutdown() noexcept;
            bool Play(IAudioStream* stream, bool loop = false) noexcept;
            void Stop() noexcept;  void Pause() noexcept;  void Resume() noexcept;
            int32 ReadFrames(float32* outBuf, int32 maxFrames) noexcept;
            bool IsPlaying() const noexcept;
            void SetVolume(float32) / GetVolume()
            void SetSpeed(float32) / GetSpeed()
            void SeekContent(float32 seconds) noexcept;
            void FlushRing() noexcept;
            bool IsActive() const noexcept;  bool IsFinished() const noexcept;
    };
```

L'ouverture, telle qu'elle est écrite dans la démo :

**`Applications/NkAudioDemo/src/main.cpp:27-50 (abrégé)`**

```cpp
static int RunStreamingTest(const char *path) {
    IAudioStream *stream = OpenAudioStream(path);
    if (!stream) {
        logger.Error("[NkAudioDemo] OpenAudioStream echec");
        return 1;
    }
    int32 sampleRate = stream->GetSampleRate();
    int32 channels = stream->GetChannels();

    AudioStreamPlayer player;
    if (!player.Init(sampleRate, channels, 88200)) {
        memory::NkGetDefaultAllocator().Delete(stream); // voir note c0000374
        return 2;
    }
    if (!player.Play(stream, /*loop=*/false)) {
        player.Shutdown();
        return 3;
    }
```

**Après `Play`, le lecteur possède le flux** : c'est lui qui le
détruira. Avant `Play` — donc dans les chemins d'erreur — c'est à vous de
le faire, avec l'allocateur du moteur.

Reste à brancher ce lecteur sur le moteur. C'est le rôle du *callback*
procédural.

## Le *callback* procédural, et le fil audio

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1140-1141`**

```cpp
using ProceduralCallback = NkFunction<void(float32* buffer, int32 frames, int32 channels)>;
AudioHandle PlayProcedural(ProceduralCallback callback, const VoiceParams& params = VoiceParams{});
```

Au lieu de lire un `AudioSample` figé, la voix appelle votre fonction
chaque fois qu'elle a besoin d'échantillons. C'est ainsi qu'on branche un flux :

**`Applications/NkVideoPlayer/src/main.cpp:396-402`**

```cpp
                audio::VoiceParams vp;
                vp.bus = "Music";
                audioHandle = engine.PlayProcedural(
                    [&streamPlayer](float32 *buf, int32 frames, int32 /*channels*/) {
                        streamPlayer.ReadFrames(buf, frames);
                    },
                    vp);
```

C'est aussi ainsi qu'on écrit un synthétiseur : rien ne vous oblige à lire un
fichier, vous pouvez calculer les échantillons.

> **⚠️ Ce *callback* tourne sur le fil audio**
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1134-1139`**
>
> ```
>  Le callback est appelé depuis le thread audio pour remplir
>  les buffers à la demande. Idéal pour synthétiseur logiciel.
>  @note TEMPS RÉEL : callback ne doit pas allouer/locker
> ```
>
> « Temps réel » veut dire ici : votre fonction dispose de quelques millisecondes,
> et si elle les dépasse, l'utilisateur entend un craquement.
>
> Sont donc interdits dans ce *callback* : allouer de la mémoire, prendre un
> verrou, ouvrir un fichier, journaliser, redimensionner un conteneur. Tout ce qui
> peut faire attendre un fil peut faire craquer le son.
>
> Ce que vous pouvez faire : lire des tableaux préalloués, calculer, écrire dans
> le tampon fourni, manipuler des atomiques.

### L'ordre de destruction avec un lecteur de flux

Ce commentaire du lecteur vidéo mérite d'être lu deux fois :

**`Applications/NkVideoPlayer/src/main.cpp:672-676`**

```
 streamPlayer.Shutdown() joint le thread décodeur et libère le IAudioStream
 qu'il possède (ContainerAudioStream/MemoryStream/WavStream) ; l'ordre importe :
 arrêter l'engine (qui appelle le callback procédural depuis son thread audio)
 AVANT de détruire streamPlayer, pour ne jamais mixer un stream en cours de
 destruction.
```

D'où la séquence complète, dans le bon ordre :

**`Exemple écrit pour le cours (invariant : NkVideoPlayer/src/main.cpp:672-676)`**

```cpp
engine.Shutdown();     // 1) arrete le fil audio -> plus aucun appel au callback
player.Shutdown();     // 2) joint le fil decodeur et libere le stream
```

> **✅ La règle générale des trois fermetures**
>
> `engine.Shutdown()` vient **toujours en premier**, parce que c'est
> lui qui fait tourner le fil qui touche à tout le reste. Ensuite seulement on
> détruit ce qu'il lisait : les lecteurs de flux, puis les `AudioSample`.

## Le micro

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudioCapture.h:27-75 (abrégé)`**

```cpp
    struct NkCaptureDeviceInfo { /* ... */ bool isDefault = false; };
    struct NkCaptureConfig {
            int32 sampleRate = 48000;  int32 channels = 1;  int32 ringSeconds = 4;
    };
    using NkAudioInCallback = NkFunction<void(const float32* interleaved, int32 frames, int32 channels)>;

    class NkAudioCapture {
            static NkVector<NkCaptureDeviceInfo> EnumerateDevices();
            bool Open(const NkCaptureConfig& config);  void Close();
            bool Start();  void Stop();  bool IsCapturing() const;
            int32 Read(float32* out, int32 maxFrames);
            int32 Available() const;
            void SetCallback(NkAudioInCallback cb);
            int32 SampleRate() const;  int32 Channels() const;
            static bool SelfTest();
    };
```

Deux modes coexistent : le *pull* (`Read` depuis votre boucle) et le
*push* (`SetCallback`). Le premier est plus simple et suffit
largement :

**`Exemple écrit pour le cours (API : NkAudioCapture.h:53-63)`**

```cpp
#include "NKAudio/NkAudioCapture.h"

audio::NkAudioCapture cap;
audio::NkCaptureConfig ccfg;      // 48000 Hz, mono, ring de 4 s
if (!cap.Open(ccfg) || !cap.Start())
    return 1;

NkVector<float32> bloc; bloc.Resize(1024);
while (enregistre) {
    const int32 n = cap.Read(bloc.Data(), 1024);   // 0 si rien de dispo
    /* ... accumuler les n frames ... */
}
cap.Stop();
cap.Close();
```

`Read` qui renvoie 0 n'est pas une erreur : c'est « rien de neuf ». Le
tampon circulaire fait quatre secondes par défaut ; si vous ne lisez pas assez
souvent, les plus anciennes données sont perdues.

`NkAudioCapture` est l'une des deux seules classes du module à posséder un
auto-test (`NkAudioCapture.cpp:930`) ; l'autre est `NkDenoiser`
(`NkDenoiser.cpp:304`). Il n'y a d'auto-test ni pour
`AudioEngine`, ni pour `AudioLoader`, ni pour les codecs.

L'application `NkMicRecord` donne au passage le test de codecs le plus
court du dépôt :

**`Applications/NkMicRecord/src/main.cpp:76-89 (abrégé)`**

```cpp
    // Mode décodage : NkMicRecord --decode <in.(wav|mp3|ogg|flac|opus)> <out.wav>
    // Charge via AudioLoader (auto-détection du format, dont Ogg-Opus) et
    // réécrit en WAV 16-bit — test end-to-end des codecs NKAudio.
    if (argc >= 4 && NkString(argv[1]) == NkString("--decode")) {
        audio::AudioSample smp = audio::AudioLoader::Load(argv[2]);
        if (!smp.IsValid()) {
            printf("[DECODE] ERREUR : chargement impossible : %s\n", argv[2]);
            return 1;
        }
        const bool ok = audio::AudioLoader::SaveWAV(argv[3], smp);
```

## Tester sans carte son

Voici la fonctionnalité la plus utile pour apprendre — et la moins connue.

**`Kernel/Runtime/NKAudio/src/NKAudio/NkAudio.h:1321`**

```cpp
void RenderToBuffer(float32* outputBuffer, int32 frameCount, int32 channels = 2);
```

Couplée au backend `NULL_OUTPUT`, elle permet de faire tourner tout le
moteur — voix, bus, effets, limiteur — *sans aucun périphérique*, et de
récupérer le mixage dans un tableau que vous pouvez inspecter :

**`Exemple écrit pour le cours (API : NkAudio.h:122-136, 1321)`**

```cpp
audio::AudioEngineConfig cfg;
cfg.backend = audio::AudioBackendType::NULL_OUTPUT;   // aucune sortie physique
engine.Initialize(cfg);
engine.Play(smp);

NkVector<float32> mix; mix.Resize(48000 * 2);         // 1 s de stereo
engine.RenderToBuffer(mix.Data(), 48000, 2);          // appel SYNCHRONE
// mix contient maintenant exactement ce qui serait sorti des haut-parleurs.
```

> **✅ Ce qu'il faut retenir**
>
> `NULL_OUTPUT` + `RenderToBuffer` transforme l'audio en calcul pur,
> donc en quelque chose de **vérifiable**. Un test qui vérifie que le mixage
> n'est pas silencieux, qu'il ne dépasse pas 1,0, ou qu'un bus muet produit bien
> du zéro, tourne sans matériel et sans attendre. C'est ainsi qu'il faut aborder
> les exercices de ce chapitre.

## État réel du module

### Ce qui fonctionne

| | |
|---|---|
| **Décodage** | WAV natif, MP3, OGG Vorbis, FLAC, Opus — tous écrits dans le dépôt |
| **Sortie** | WASAPI, CoreAudio, ALSA, AAudio, WebAudio, Null |
| **Capture** | `NkAudioCapture` complet, avec auto-test |
| **Streaming** | `IAudioStream`, `AudioStreamPlayer`, piste audio de conteneur vidéo |
| **Mixage** | bus hiérarchiques, sidechain, fondu croisé musique, limiteur master |
| **3D** | atténuation, occlusion, absorption de l'air, HRTF synthétique |
| **Hors ligne** | `RenderToBuffer` |

Le décodage mérite une précision, parce que le dépôt se calomnie lui-même : le
commentaire de tête de `NkAudioLoader.cpp:3` parle encore de « stubs
MP3/OGG/FLAC ». **C'est périmé.** Les quatre décodeurs sont branchés sur de
vrais codecs (`NkAudioLoader.cpp:411-429`). Encore un cas où le
commentaire ment et le code dit vrai.

### Ce qui ne fonctionne pas

> **⚠️ Ne jamais demander `DIRECTSOUND`**
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudioBackends.cpp:445-451`**
>
> ```cpp
>         bool DirectSoundAudioBackend::Initialize(int32 sr, int32 ch, int32 buf) {
>             mSampleRate = sr;
>             mChannels = ch;
>             mBufferSize = buf;
>             mImpl = memory::NkGetDefaultAllocator().New<DsImpl>();
>             // DirectSoundCreate8, CreateSoundBuffer, etc.
>             return true;
>         }
> ```
>
> Lisez bien la dernière ligne : `return true`. Le backend annonce le
> succès sans avoir ouvert quoi que ce soit. Vous obtiendrez un moteur
> « initialisé », des voix « en lecture », des positions qui avancent — et un
> silence absolu, sans le moindre message d'erreur.
>
> **Laissez `cfg.backend` sur `AUTO`**, qui choisit WASAPI sous
> Windows, CoreAudio sous macOS, ALSA sous Linux. Trois autres valeurs de l'énumération — `PULSE_AUDIO`, `OPEN_SL_ES`, `CUSTOM` — n'ont
> pas de classe de backend correspondante dans
> `NkAudioBackends.h` : elles existent dans l'énumération et nulle part
> ailleurs.

> **⚠️ Le rééchantillonnage « haute qualité » est du linéaire**
>
> **`Kernel/Runtime/NKAudio/src/NKAudio/NkAudioLoader.cpp:588-592`**
>
> ```cpp
>         void AudioLoader::ResampleSinc(AudioSample &sample, int32 targetSampleRate) {
>             // Pour l'instant, fallback linéaire
>             // TODO: Implémenter Kaiser-windowed Sinc pour qualité studio
>             ResampleLinear(sample, targetSampleRate);
>         }
> ```
>
> Donc `AudioLoader::Resample(sample, taux, /*highQuality=*/true)` fait
> exactement la même chose que `false`. C'est le jumeau exact du piège
> `NK_LANCZOS3` du chapitre 5 : ça ne plante pas, ça ment.
>
> Par symétrie, les valeurs `SINC_4` et `SINC_8` de
> `ResamplingQuality` dans `AudioEngineConfig` sont à vérifier avant
> d'être présentées comme différenciantes.
>
> La bonne nouvelle, c'est que vous n'avez normalement pas à rééchantillonner
> vous-même :
>
> **`Applications/NkAudioPlayer/src/main.cpp:15-16`**
>
> ```
>  NOTE (agent NKCode) : le mixage NKAudio convertit le TAUX du fichier vers celui du
>  device (fix mixBuffer/format-device). Un lecteur n'a RIEN à faire côté rééchantillonnage.
> ```

### Le reste de la surface, en survol

Le module contient bien davantage que ce chapitre ne couvre. Pour que vous
sachiez au moins que cela existe :

- `IAudioEffect` (`NkAudio.h:438`) et les effets concrets de
  `NkAudioEffects.h` — réverbération, écho, chorus, compresseur,
  filtres, *pitch shift*… Ils s'attachent à une voix
  (`AddEffect`), à un bus, ou à la sortie
  (`AddMasterEffect`). **Leur propriété reste à l'appelant**
  (`NkAudio.h:1230`) : l'effet doit survivre à ce à quoi il est
  attaché ;
- l'audio 3D : `SetSourcePosition`, `SetListenerOrientation`,
  `SetSourceOcclusion`. Notez que **c'est l'application qui
  calcule l'occlusion** — « L'application fait le raycast pour calculer
  cette valeur » (`NkAudio.h:1177-1178`). Pour le HRTF, si vous
  n'avez pas de fichier de données, `GenerateSyntheticHrtf()`
  (`NkAudio.h:1211`) n'en demande aucun ;
- `AudioGenerator` (`NkAudio.h:695`) — synthèse de formes
  d'onde, enveloppes ADSR ;
- `AudioAnalyzer` (`NkAudio.h:947`) — FFT, avec un
  `FftResult` qui porte les magnitudes et une conversion
  `BinToFrequency`. De quoi dessiner un spectre en temps réel.

## Relier le son à l'image

NKAudio ne dépend d'aucun module graphique : le raccordement est entièrement à
votre charge, et c'est très bien ainsi. Trois motifs reviennent :

1. **Le son au clic** — le widget renvoie `true`, vous appelez
   `Play` avec `vp.bus = "UI"`. C'est ce que nous avons fait ;
2. **La forme d'onde** — `AudioSample::data` et
   `frameCount` vous donnent tout pour pré-calculer une enveloppe
   min/max par colonne de pixels, et `GetPlaybackPosition(handle)`
   vous donne la tête de lecture. C'est exactement ce que fait
   `ComputePeaks` dans
   `Applications/NkAudioPlayer/src/main.cpp:56-87`, avant d'envoyer
   un quad par colonne au renderer ;
3. **Le spectre** — `AudioAnalyzer` rend un `FftResult`,
   une barre par *bin*.

Le calcul de la fraction lue, pour dessiner un curseur de lecture :

**`Applications/NkAudioPlayer/src/main.cpp:212-215`**

```cpp
        // Position de lecture -> fraction [0..1].
        const float32 pos = engine.GetPlaybackPosition(handle);
        const float32 frac = (durSec > 0.0) ? math::NkClamp((float32)(pos / durSec), 0.0f, 1.0f) : 0.0f;
        const int32 headCol = (int32)(frac * (float32)cols);
```

Le `NkClamp` n'est pas superflu : la position peut légèrement dépasser la
durée en fin de lecture, à cause de la granularité du tampon.

## Exercices

> **✏️ 1 — Le son au clic, complet**
>
> Ajoutez à votre application NKGui trois boutons et trois sons différents, tous
> sur le bus « UI ». Puis ajoutez un `Checkbox` « sons d'interface » qui
> appelle `engine.GetBus("UI")->SetMute(...)`.
>
> Vérifiez ensuite trois choses : que couper le bus UI n'affecte pas un son joué
> sur « SFX » ; que l'application démarre normalement si vous renommez les fichiers
> audio pour les rendre introuvables ; et que la fermeture n'émet aucun bruit
> parasite — c'est-à-dire que vous appelez bien `Shutdown()` avant
> `Free()`.

> **✏️ 2 — Mesurer sans écouter**
>
> Écrivez un programme console qui initialise le moteur en `NULL_OUTPUT`,
> joue un son, et rend une seconde de mixage dans un tableau avec
> `RenderToBuffer`. Calculez et affichez le maximum absolu et la moyenne
> quadratique du tableau.
>
> Recommencez en réglant le bus sur `SetVolume(0.5f)` : le maximum doit être
> divisé par deux. Puis avec `SetMute(true)` : le tableau doit être
> rigoureusement nul. Vous venez d'écrire un test audio automatisable, sans
> matériel.

> **✏️ 3 — Prouver l'ordre de fermeture**
>
> Écrivez volontairement la mauvaise séquence :
> `AudioLoader::Free(sample);` *puis* `engine.Shutdown();` sur
> un son de plusieurs secondes en cours de lecture. Lancez plusieurs fois et notez
> ce que vous entendez et ce qui se passe.
>
> Puis remettez le bon ordre. Cet exercice n'a l'air de rien : il vous vaccine
> contre une classe entière de bugs qu'on ne diagnostique pas au débogueur, parce
> que le plantage arrive ailleurs et plus tard.

> **✏️ 4 — Un synthétiseur en dix lignes**
>
> Utilisez `PlayProcedural` avec un *callback* qui remplit le tampon
> d'une sinusoïde, en gardant la phase dans une variable capturée par référence.
> Ajoutez un `SliderFloat` NKGui qui règle la fréquence.
>
> Attention : le *slider* s'écrit depuis le fil principal et le
> *callback* lit depuis le fil audio. Réfléchissez à ce que vous partagez —
> et relisez l'interdiction d'allouer et de verrouiller. Une variable atomique de
> type `float` est la bonne réponse ; un `NkVector` redimensionné
> dans le *callback* est la mauvaise.
