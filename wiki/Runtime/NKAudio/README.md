# NKAudio — documentation détaillée

Le module **NKAudio**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKAudio.md](../NKAudio.md).

Moteur audio AAA zéro-STL de la couche Runtime : namespace `nkentseu::audio`, header parapluie
`#include "NKAudio/NKAudio.h"`. Tout passe en **Float32 interleaved** et toute la mémoire est
gérée par un `memory::NkAllocator*` (jamais le heap CRT). Chaque page documente l'API publique
réelle (structures, énumérations, classes, méthodes) avec les pièges d'ownership et de
threading temps réel.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Engine.md](Engine.md) | Initialiser le moteur (`AudioEngine`), jouer et contrôler des voix, audio 3D (sources/listener, occlusion, Doppler, HRTF), sélection et fabrique de backends, rendu offline. | `NKAudio.h`, `NkAudioBackends.h` |
| [Bus-Effects.md](Bus-Effects.md) | Bus de mixage hiérarchiques (`NkAudioBus` : volume, mute/solo, routing, sidechain/ducking), effets DSP (`IAudioEffect` + délai/réverbe/compresseur/limiter/biquad/EQ/distortion/chorus), dataset HRTF binaural. | `NkAudioBus.h`, `NkAudioEffects.h`, `NkHrtfDataset.h` |
| [Codecs-Streaming.md](Codecs-Streaming.md) | Décodeurs from-scratch FLAC / MP3 / OGG Vorbis (`static Decode` → `AudioSample`), interface de flux `IAudioStream` + `OpenAudioStream`/`WavStream`/`MemoryStream`, player de flux `AudioStreamPlayer`. | `Codecs/FLAC/NkFLACCodec.h`, `Codecs/MP3/NkMP3Codec.h`, `Codecs/OGG/NkOGGVorbisCodec.h`, `Streaming/NkAudioStream.h`, `Streaming/NkAudioStreamPlayer.h` |

[← Récap NKAudio](../NKAudio.md) · [← Couche Runtime](../README.md)
