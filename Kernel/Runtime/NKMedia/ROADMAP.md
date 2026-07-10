# NKMedia — conteneurs & codecs média (audio/vidéo) — ROADMAP

> Module Runtime **from-scratch, zero-STL** (`namespace nkentseu::media`) pour **lire et écrire** des
> formats conteneurs (MP4/ISOBMFF, WebM/Matroska, …) et, à terme, décoder/encoder leurs **codecs**
> (audio : AAC, Opus, Vorbis ; vidéo : VP8/VP9, H.264…). Motivé par un besoin réel : le corpus vocal
> (getUserMedia navigateur) arrive en **WebM/MP4**, que NKAudio (WAV/MP3/OGG/FLAC) ne sait pas lire ;
> et plus largement pour la **lecture/écriture vidéo in-engine** (cutscenes, enregistrement, caméra).
>
> ⚠️ **Honnêteté d'échelle** : un décodeur vidéo complet (H.264/VP9) from-scratch est un travail énorme.
> On construit **par couches**, chaque brique testable. Tant que les décodeurs natifs n'existent pas, un
> **repli par transcodage externe (ffmpeg)** dépanne (préparation de dataset, offline).

| Brique | Statut | Contenu |
|---|---|---|
| 1. Probe / démux conteneurs | 🔶 EN COURS | détecte le conteneur (MP4/WebM/WAV/OGG/MP3/FLAC), liste **pistes + codecs + params** (parseurs ISOBMFF + EBML) |
| 2. Extraction de paquets | ⬜ | sortir les **paquets encodés** par piste (échantillons audio, NAL units vidéo) + timestamps |
| 3. Décodeur audio AAC-LC | ⬜ | MP4 → PCM (AAC Low Complexity from-scratch) |
| 4. Décodeur audio Opus / Vorbis | ⬜ | WebM → PCM (Opus, puis Vorbis) |
| 5. Muxers (écriture) | ⬜ | écrire WAV puis WebM/MP4 (conteneur) |
| 6. Vidéo (décode) | ⬜ | VP8/VP9 puis H.264 (très long) → frames RGBA (→ NKImage/NKRHI) |
| 7. Vidéo (encode) + AV sync | ⬜ | enregistrement, mux A/V, horloge de présentation |

## Livré
- **Brique 1 (2026-07-10)** — `NkMediaProbe` (`NkMediaProbe.{h,cpp}`) : détection de conteneur + parseurs
  **ISOBMFF** (boîtes MP4 : `ftyp/moov/trak/mdia/minf/stbl/stsd/hdlr`, fourcc du sample entry, params audio)
  et **EBML** (Matroska/WebM : `Segment/Tracks/TrackEntry`, `CodecID`, `Audio` SamplingFrequency/Channels,
  `Video` PixelWidth/Height). Renvoie `NkMediaInfo { container, tracks[] }`. Testé HEADLESS (détection des
  magies + varints EBML) **et sur le corpus réel** (Bassa `.mp3` → **MP4/AAC**, ghomala' `.mp3` → **WebM/Opus**).

## En cours / À venir
- Brique 2 : extraction des paquets encodés (offsets `stco/stsz/stsc` MP4 ; blocs SimpleBlock WebM).
- Puis décodeurs audio (AAC, Opus) → PCM float32 (branché comme codec supplémentaire de NKAudio).
- Vidéo bien plus tard (frames → NKImage/NKRHI). Repli ffmpeg documenté pour la prépa dataset entre-temps.

## Dépendances
Foundation (NKCore/NKMemory/NKContainers/NKMath) + NKStream/NKFileSystem (I/O). Consommateurs visés :
NKAudio (codecs audio), NKSpeech (corpus voix), NKImage/NKRHI (frames vidéo), NKCamera (capture).
