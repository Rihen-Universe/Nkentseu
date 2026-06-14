# NKAudio

> Couche **Runtime** · Le moteur audio du framework : moteur de mixage et de voix,
> sources et listener 3D, bus hiérarchiques, effets DSP (dont HRTF spatial), codecs
> (FLAC / MP3 / OGG Vorbis) et streaming — entièrement AAA, zéro-STL.

Dès qu'un son doit être joué, spatialisé, mélangé ou décodé depuis un fichier, c'est NKAudio
qui s'en charge. C'est un moteur **temps réel** complet : un `AudioEngine` singleton pilote un
pool de 256 voix, route le mix à travers des bus hiérarchiques (Master / SFX / Music / Voice /
UI), applique des effets DSP, gère l'audio 3D (atténuation distance, Doppler, occlusion,
rendu binaural HRTF), et s'appuie sur un backend natif par plateforme (WASAPI, CoreAudio,
ALSA, AAudio, WebAudio…). Autour du moteur gravitent des outils offline (chargement,
génération, mixage, analyse FFT) et une pile de codecs/streaming entièrement from-scratch.

Le format interne unique est **Float32 interleaved** : tout ce qui entre dans le moteur (PCM
chargé, samples synthétisés, flux décodés) est normalisé dans ce format, et la mémoire est
toujours gérée par un `memory::NkAllocator*` — jamais par le heap CRT.

- **Namespace** : `nkentseu::audio`
- **Header parapluie** : `#include "NKAudio/NKAudio.h"` (backends concrets :
  `#include "NKAudio/NkAudioBackends.h"`, qui inclut déjà `NKAudio.h`)
- **Version** : 2.0.0 · **Export** : `NKENTSEU_AUDIO_API` · zéro-STL (`NkVector`,
  `NkFunction`, `memory::NkAllocator`)

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Initialiser le moteur, jouer un son, contrôler une voix | [Le moteur](NKAudio/Engine.md) |
| Positionner une source 3D, déplacer le listener, choisir un backend | [Le moteur](NKAudio/Engine.md) |
| Router et régler le volume des groupes de sons (bus, ducking) | [Bus & effets](NKAudio/Bus-Effects.md) |
| Ajouter de la réverbe, du délai, un compresseur, un EQ… | [Bus & effets](NKAudio/Bus-Effects.md) |
| Spatialiser en binaural (casque) avec un dataset HRTF | [Bus & effets](NKAudio/Bus-Effects.md) |
| Décoder un FLAC / MP3 / OGG en mémoire | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |
| Lire un gros fichier en flux sans tout charger en RAM | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |

Chaque page documente l'**API réelle** : structures, énumérations, classes et leurs méthodes,
avec les pièges d'ownership mémoire (allocateur NKMemory) et de threading (le `Process` des
effets et le `ReadFrames` des players tournent sur le thread audio temps réel).

---

## Aperçu des familles

- **Moteur** (`NKAudio.h`, `NkAudioBackends.h`) — `AudioEngine` (singleton thread-safe,
  PIMPL) : init/shutdown, `Play`/`PlayProcedural`/`PlayMusicCrossfade`, contrôle de voix
  (volume/pitch/pan/loop/seek), audio 3D (`SetSourcePosition`, occlusion, Doppler, HRTF),
  listener 3D, contrôles globaux et rendu offline. `AudioEngineConfig` paramètre l'init ;
  `AudioHandle` désigne une voix ; `VoiceParams` un appel de lecture. Sélection et fabrique
  des pilotes : `IAudioBackend`, `AudioBackendFactory`, `EnsureBackendsRegistered()` + les
  backends concrets (`WasapiAudioBackend`, `CoreAudioBackend`, `AlsaAudioBackend`,
  `AAudioBackend`, `WebAudioBackend`, `NullAudioBackend`…).
- **Bus & effets** (`NkAudioBus.h`, `NkAudioEffects.h`, `NkHrtfDataset.h`) — `NkAudioBus`
  (routing hiérarchique, volume effectif, mute/solo, chaîne d'effets, sidechain/ducking) ;
  l'interface `IAudioEffect` et ses implémentations DSP (`DelayEffect`, `ReverbEffect`,
  `CompressorEffect`, `LimiterEffect`, `BiquadFilter`, `Eq3BandEffect`, `DistortionEffect`,
  `ChorusEffect`) ; le dataset binaural `NkHrtfDataset` + `NkHrirPair`.
- **Codecs & streaming** (`Codecs/…`, `Streaming/…`) — décodeurs from-scratch `NkFLACCodec`,
  `NkMP3Codec`, `NkOGGVorbisCodec` (tous `static Decode` → `AudioSample`) ; l'interface de
  flux `IAudioStream` + la fabrique `OpenAudioStream` + les flux concrets `WavStream`
  (chunked fichier) et `MemoryStream` (sample en RAM) ; le `AudioStreamPlayer` (thread
  décodeur + ring buffer lock-free + crossfade/loop).
- **Outils offline** (dans `NKAudio.h`, hors temps réel) — `AudioLoader` (détection format,
  Load/Save, resample, conversion canaux/format), `AudioGenerator` (tones, bruit, percussions
  synthétiques), `AudioMixer` (mix, crossfade, normalisation, fades), `AudioAnalyzer` (RMS,
  peak, tempo, pitch, FFT, spectrogramme).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKAudio.h` | Parapluie : `AudioEngine`, `AudioHandle`, `AudioSample`, `AudioSource3D`/`AudioListener3D`, `VoiceParams`, `AudioEngineConfig`, `IAudioEffect`, `IAudioBackend`, `AudioBackendFactory`, enums, outils offline. | [Le moteur](NKAudio/Engine.md) |
| `NkAudioBackends.h` | Backends concrets par plateforme + `EnsureBackendsRegistered()`, macro `NK_REGISTER_AUDIO_BACKEND`. | [Le moteur](NKAudio/Engine.md) |
| `NkAudioBus.h` | `NkAudioBus` (routing, volume, mute/solo, effets, sidechain). | [Bus & effets](NKAudio/Bus-Effects.md) |
| `NkAudioEffects.h` | Effets DSP concrets (`DelayEffect`, `ReverbEffect`, `CompressorEffect`, `LimiterEffect`, `BiquadFilter`, `Eq3BandEffect`, `DistortionEffect`, `ChorusEffect`). | [Bus & effets](NKAudio/Bus-Effects.md) |
| `NkHrtfDataset.h` | `NkHrtfDataset`, `NkHrirPair` (spatialisation binaurale). | [Bus & effets](NKAudio/Bus-Effects.md) |
| `Codecs/FLAC/NkFLACCodec.h` | `NkFLACCodec::Decode` (FLAC lossless). | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |
| `Codecs/MP3/NkMP3Codec.h` | `NkMP3Codec::Decode` (MPEG-1/2 Layer 3). | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |
| `Codecs/OGG/NkOGGVorbisCodec.h` | `NkOGGVorbisCodec::Decode`, `NkOGGVorbisEncoderConfig`. | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |
| `Streaming/NkAudioStream.h` | `IAudioStream`, `OpenAudioStream`, `WavStream`, `MemoryStream`. | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |
| `Streaming/NkAudioStreamPlayer.h` | `AudioStreamPlayer` (player de flux). | [Codecs & streaming](NKAudio/Codecs-Streaming.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
