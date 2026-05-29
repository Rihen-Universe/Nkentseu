# NKAudio — Roadmap

État actuel (mai 2026) : moteur audio AAA STL-free livré avec pool de 256 voix, mixage temps réel, spatialisation 3D + HRTF (mesuré et synthétique), buses hiérarchiques style FMOD/Wwise, sidechain/ducking, streaming via ring buffer, effets DSP (delay, reverb FDN, compressor, limiter, biquad, EQ 3-bandes, distortion, chorus), codecs WAV/MP3/OGG Vorbis/FLAC (MP3 = port complet minimp3 Layer 3), backends WASAPI/DirectSound/CoreAudio/ALSA/AAudio/OpenSL ES/WebAudio/Null. Opus, capture micro et streaming incrémental restent absents.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| AudioEngine singleton (PIMPL, thread-safe) | Livré | — | — |
| Pool de voix lock-free (AUDIO_MAX_VOICES=256) | Livré | — | — |
| AudioHandle / VoiceParams / VoiceState | Livré | — | — |
| Crossfade musique (`PlayMusicCrossfade`) | Livré | — | — |
| RenderToBuffer (rendu offline) | Livré | — | — |
| AudioLoader (WAV, MP3, OGG, FLAC, SaveWAV) | Livré | — | — |
| Codec WAV (PCM int/float, mono/stéréo) | Livré | — | — |
| Codec OGG Vorbis (port stb_vorbis) | Livré | — | — |
| Codec FLAC (decode from-scratch) | Livré | — | — |
| Codec MP3 Layer 3 (decode) | Livré | — | — |
| Codec MP3 Layer 1 / Layer 2 | TODO | M | P3 |
| Codec MP3 streaming incrémental + seek | TODO | M | P2 |
| Codec Opus | TODO | L | P2 |
| Codec AIFF | TODO | M | P3 |
| Resampling LINEAR | Livré | — | — |
| Resampling SINC_4 / SINC_8 (Kaiser) | Livré | — | — |
| Channel conversion mono/stéréo/multi | Livré | — | — |
| AudioGenerator (tones, noise, chirp, drums) | Livré | — | — |
| ADSR + LFO | Livré | — | — |
| AudioMixer offline (mix, crossfade, normalize, fade, concat, reverse, insert) | Livré | — | — |
| AudioAnalyzer (RMS, peak, FFT, spectrogramme, BPM, pitch YIN) | Livré | — | — |
| Effects : Delay | Livré | — | — |
| Effects : Reverb FDN (Schroeder) | Livré | — | — |
| Effects : Compressor + Limiter brick-wall | Livré | — | — |
| Effects : BiquadFilter (LP, HP, BP, Notch, PeakEQ, Shelf, AllPass) | Livré | — | — |
| Effects : EQ 3 bandes | Livré | — | — |
| Effects : Distortion (Soft, Hard, Fuzz, Overdrive) | Livré | — | — |
| Effects : Chorus | Livré | — | — |
| Effects : Flanger, Phaser, Echo, Gate, Pitch shift, Time stretch, Tremolo, Vibrato, Autopan | TODO | M | P2 |
| Audio Buses hiérarchiques (Master, SFX, Music, Voice, UI) | Livré | — | — |
| Sidechain / ducking par bus | Livré | — | — |
| Mute / solo par bus | Livré | — | — |
| Audio 3D (positional, Doppler, attenuation, cone) | Livré | — | — |
| Occlusion + air absorption (lowpass) | Livré | — | — |
| HRTF dataset .nkhrtf (chargement) | Livré | — | — |
| HRTF synthétique (modèle sphérique ITD/ILD) | Livré | — | — |
| HRTF interpolation bilinéaire entre voisins | TODO | M | P3 |
| Streaming WavStream (lecture chunked) | Livré | — | — |
| Streaming MemoryStream (wrap AudioSample) | Livré | — | — |
| Streaming réel FLAC/MP3/OGG (pas full-decode) | Partiel | L | P2 |
| AudioStreamPlayer (ring buffer SPSC + worker thread + loop + crossfade) | Livré | — | — |
| Backend Null (test sans hw) | Livré | — | — |
| Backend WASAPI (Windows Vista+) | Livré | — | — |
| Backend DirectSound (XP/Vista legacy) | Livré | — | — |
| Backend CoreAudio (macOS/iOS) | Livré | — | — |
| Backend ALSA (Linux) | Livré | — | — |
| Backend PulseAudio (Linux haut niveau) | TODO | M | P3 |
| Backend AAudio (Android 8.0+) | Livré | — | — |
| Backend OpenSL ES (Android 4.0+ fallback) | Livré | — | — |
| Backend WebAudio (Emscripten via ScriptProcessor) | Livré | — | — |
| Backend WebAudio AudioWorklet (latence basse) | TODO | M | P3 |
| AudioBackendFactory + auto-register | Livré | — | — |
| Capture micro / input device | TODO | L | P1 |
| Voice chat / VoIP (Opus + jitter buffer) | TODO | L | P3 |
| Convolution reverb (impulse responses) | TODO | L | P3 |
| Tests unitaires | Partiel | M | P2 |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Phase 1 — Fondations et types
- API publique unique [NKAudio.h](src/NKAudio/NKAudio.h) (≈1450 lignes)
- Types : AudioSample, AudioHandle, AudioSource3D, AudioListener3D, VoiceParams, AdsrEnvelope, AudioEngineConfig
- Enums : AudioFormat, SampleFormat, AudioBackendType, WaveformType, AudioEffectType, VoiceState, AttenuationModel, ResamplingQuality
- Zero STL en API publique (NkVector, NkFunction, NkAllocator)

### Phase 2 — Loader / Generator / Mixer / Analyzer offline
- `AudioLoader` : WAV natif, MP3, OGG, FLAC, SaveWAV — [NkAudioLoader.cpp](src/NKAudio/NkAudioLoader.cpp)
- Resampling Sinc (Kaiser-windowed) + linéaire, conversion canaux 1/2/4/6/8
- `AudioGenerator` : oscillateurs (sin, square, triangle, sawtooth, pulse), noises (white, pink Voss-McCartney), chirp, accords, ADSR, LFO, kick/snare/hihat
- `AudioMixer` : mix, crossfade, concatenate, normalize, fade in/out, gain, reverse, insert
- `AudioAnalyzer` : RMS, peak, FFT Cooley-Tukey, spectrogramme STFT, détection BPM autocorrélation, détection pitch YIN, band energies

### Phase 3 — Moteur temps réel
- `AudioEngine` singleton avec PIMPL — [NkAudioEngineCore.cpp](src/NKAudio/NkAudioEngineCore.cpp)
- Pool de 256 voix lock-free
- `Play`, `PlayProcedural` (callback synthèse temps réel)
- Contrôle voix : Stop/Pause/Resume avec fade-out, Set/GetVolume/Pitch/Pan/Looping/PlaybackPosition
- StopAll / PauseAll / ResumeAll
- `RenderToBuffer` pour rendu offline (capture WAV, tests)

### Phase 4 — Audio 3D + HRTF
- Spatialisation 3D : position, vélocité, direction, cone, Doppler, atténuation (none, inverse, linear, exponential)
- Occlusion + air absorption (lowpass dynamique selon distance)
- Listener 3D avec orientation forward/up
- HRTF : dataset binaire .nkhrtf (magic NKHR + sample rate + grid azimut/elevation + HRIR float32)
- HRTF synthétique sphérique (ITD + ILD + head shadow) — aucune dépendance externe
- API : `LoadHrtfDataset`, `GenerateSyntheticHrtf`, `UnloadHrtfDataset`, `SetSourceHRTF`

### Phase 5 — Buses hiérarchiques + sidechain
- `NkAudioBus` (Master -> SFX/Music/Voice/UI par défaut) — [NkAudioBus.h](src/NKAudio/NkAudioBus.h)
- Volume effectif cascadé (voix * bus * parent * master)
- Mute / solo par bus
- Chaîne d'effets DSP par bus (MAX_EFFECTS=8)
- Sidechain ducking (musique baissée quand voice joue)
- `PlayMusicCrossfade` : transition musicale automatique
- Limiter brick-wall auto sur Master (anti-clipping)

### Phase 6 — Effets DSP (`NkAudioEffects.h/cpp`)
- Delay stéréo avec feedback
- Reverb FDN Schroeder (8 combs + 4 allpass, pre-delay, damping, diffusion)
- Compressor (RMS detection, soft/hard knee)
- Limiter brick-wall (attack 0.1ms, threshold paramétrable)
- BiquadFilter polyvalent (low/high/band-pass, notch, peak EQ, low/high shelf, all-pass)
- EQ 3 bandes (low shelf + mid peak + high shelf)
- Distortion (Soft/Hard/Fuzz/Overdrive avec tanh + clamp)
- Chorus (délai modulé LFO)

### Phase 7 — Codecs audio
- WAV : PCM int8/16/24/32, float32/64, mono/stéréo/multi
- OGG Vorbis : port stb_vorbis (multi-canal jusqu'à 8, VBR, floor type 1, residue 0/1/2)
- FLAC : décodeur from-scratch complet (CONSTANT/VERBATIM/FIXED/LPC subframes, Rice partitioning, decorrelation stereo joint, CRC8/CRC16)
- MP3 Layer 3 : décodeur complet (port minimp3 CC0 en style Nkentseu) — header parser MPEG-1/2, skip ID3v1+v2, bit reservoir cross-frame, tables Huffman complètes (kHuffTabs ~1842 entrées + count1 tab32/33), power-law dequantization, scalefactors decode, joint stereo (mid/side + intensity), IMDCT 12/36 + window switching, polyphase synthesis QMF (DCT-II 32-pts + filterbank 512-tap), sortie float32 normalisée [-1, +1] — [NkMP3Codec.cpp](src/NKAudio/Codecs/MP3/NkMP3Codec.cpp) (1254 lignes, sans SIMD, sans dépendance externe)

### Phase 8 — Streaming
- `IAudioStream` (pull-based : ReadFrames, Seek, IsEOF) — [NkAudioStream.h](src/NKAudio/Streaming/NkAudioStream.h)
- `WavStream` : streaming chunked depuis fichier (faible RAM)
- `MemoryStream` : wrap d'un AudioSample (FLAC/MP3/OGG en RAM)
- `AudioStreamPlayer` : ring buffer SPSC + thread worker + loop + crossfade EOF

### Phase 9 — Backends natifs
- Null (thread tick simulé pour tests) — toutes plateformes
- WASAPI exclusif + DirectSound legacy (Windows)
- CoreAudio (macOS / iOS)
- ALSA (Linux)
- AAudio (Android 8+) + OpenSL ES (fallback Android 4.0+)
- WebAudio via Emscripten ScriptProcessorNode
- Enregistrement explicite via `EnsureBackendsRegistered()` (résiste au stripping des static initializers en lib statique)

---

## En cours / TODO immédiat

### Capture micro / input
- Pas d'API d'entrée audio actuellement (uniquement output)
- Nécessaire pour : voice chat, enregistrement, analyse temps réel, échantillonneur

### Streaming réel FLAC/MP3/OGG
- Actuellement : décodage complet en RAM puis MemoryStream
- À faire : décodeurs incrémentaux (frame-by-frame) pour faible empreinte sur longs fichiers

---

## À venir / À ajouter (futur proche)

### Effets DSP manquants
- Flanger (chorus court + feedback)
- Phaser (all-pass cascade)
- Echo multi-tap explicite (distinct du Delay)
- Noise gate
- Pitch shift overlap-add
- Time stretch phase vocoder
- Tremolo, Vibrato, Autopan
- Convolution reverb (FFT par blocs avec impulse response WAV)

### Codecs audio supplémentaires
- Opus (codec moderne streaming, VoIP) — déjà dans `AudioFormat::OPUS` mais non implémenté
- AIFF (Apple Audio Interchange Format)
- Format Nkentseu compact pour SFX (header + PCM compressé ADPCM ?)

### Extensions MP3
- **Layer 1 et Layer 2** : actuellement skippés silencieusement (`HdrGetLayer == 1` ne traite que Layer 3). Rare en pratique (Layer 3 = 99% des MP3) — P3.
- **Streaming MP3 incrémental** : `Decode` charge le buffer complet en RAM. Refactor en `IAudioStream` chunk-by-chunk pour gros fichiers (>100 MB) — P2.
- **Seek API** : `SeekToFrame(idx)` / `SeekToTime(ms)`. Implique un scan préalable (table d'index frames) ou une table VBR (XING/VBRI header) — P2.
- **Chemin SIMD** : branches SSE/NEON de minimp3 retirées au portage. Réintroduire via NKMath SIMD si profile montre que polyphase synthesis est un hotspot — P3.
- **Path float32 direct** : Mp3dScalePcm clippe en int16 avant normalisation. Path float32 pur éviterait la perte de précision — P3.

### Backends supplémentaires
- PulseAudio (Linux haut niveau, alternative ALSA)
- WebAudio AudioWorklet (latence ~5ms vs 80ms ScriptProcessor, nécessite COOP/COEP)

### HRTF avancé
- Interpolation bilinéaire entre HRIR voisins (actuellement nearest-neighbor)
- Importeur datasets MIT KEMAR / CIPIC / SADIE -> .nkhrtf
- Personnalisation HRTF par taille de tête / forme d'oreille

### Voice chat / réseau
- Opus encode/decode pour transport bas débit
- Jitter buffer adaptatif
- Echo cancellation (AEC)
- Noise suppression (RNNoise ou équivalent)

### Tests
- 5 fichiers de test existants : [NkAudioTests.cpp](tests/NkAudioTests.cpp), TestStreaming, TestMP3, TestFLAC, TestAudioFormats
- Manquent : tests effets DSP, tests sidechain, tests HRTF (vérification ITD/ILD), benchmarks latence backend

---

## Bugs / quirks connus
- MP3 : ne décode que Layer 3. Un fichier Layer 1/2 produit `AudioSample` vide silencieusement.
- MP3 : trois bugs critiques de portage ont été fixés dans le code (commentaires `BUG CRITIQUE precedemment` dans [NkMP3Codec.cpp](src/NKAudio/Codecs/MP3/NkMP3Codec.cpp) — `kMaxScfi`, `HdrGetMySampleRate`, `L3LdexpQ2`). Tout MP3 décodé avant ces fixes était bruité.
- HRTF synthétique : moins précis qu'un dataset mesuré (MIT KEMAR), suffit pour MVP
- WebAudio : latence élevée (~80ms) faute d'AudioWorklet
- OpenSL ES Android : format int16 uniquement, conversion float32 -> int16 dans le callback

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types, atomics, plateforme), NKMemory (NkAllocator), NKContainers (NkVector, NkFunction), NKFileSystem (NkFile pour streaming + AAssetManager Android), NKPlatform (macros API + détection OS)
- **Modules au-dessus qui en dépendent** : NKEngine (gameplay sons), NKUI (sons d'interface), NKScript (API audio scriptable), runtime jeu / sandbox audio
