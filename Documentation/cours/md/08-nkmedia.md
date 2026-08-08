# NKMedia : lire et écrire de la vidéo

Ce chapitre est le plus court des cinq, et c'est délibéré.

`NKMedia` est de très loin le plus gros module du moteur. Il contient des
décodeurs H.264, HEVC, VP8, VP9, AV1, MPEG-2, Theora, AAC, Opus (avec ses deux
moitiés CELT et SILK), AMR — tous écrits à la main, sans `ffmpeg`. Le
seul décodeur AV1 pèse 286 Ko de source. Un chapitre pour débutants qui
essaierait d'en faire le tour serait illisible et faux.

Nous nous limiterons donc à **quatre classes de façade**, plus une
cinquième en bonus. Tout le reste est de la plomberie interne, et vous n'avez
aucune raison d'y toucher.

> **✅ Le périmètre de ce chapitre**
>
> - `NkMediaProbe` — qu'y a-t-il dans ce fichier ?
> - `NkVideoWriter` — écrire une vidéo, en MJPEG ;
> - `NkVideoReader` — la lire image par image ;
> - `NkVideoRecorder` — enregistrer ce que l'application affiche ;
> - `NkImageSequenceWriter` — écrire une séquence d'images.
>
> Cinq classes, aucune ligne de codec. Si vous maîtrisez ces cinq-là, vous savez
> faire tout ce dont une application a besoin.

## Le module, et une inversion inhabituelle

**`Kernel/Runtime/NKMedia/NKMedia.jenga:17-22`**

```cpp
    nkentseudependson(
        ["NKCore", "NKPlatform", "NKMemory", "NKContainers", "NKMath", "NKLogger",
         "NKStream", "NKFileSystem", "NKImage", "NKThreading", "NKTime"],
        selfexport="NKMedia",
        extra_includes=["src"],
    )
```

`NKImage` est dans la liste, et pour une raison élégante : **le
codec MJPEG n'est rien d'autre que le codec JPEG de NKImage appliqué image par
image**. Les séquences d'images passent, elles aussi, par les codecs PNG, BMP,
TGA et QOI de NKImage. Le chapitre 5 n'était donc pas un préalable arbitraire.

Il n'y a pas d'en-tête parapluie : on inclut le sous-en-tête précis dont on a
besoin. Tout vit dans le *namespace* `nkentseu::media`.

**`Kernel/Runtime/NKMedia/ — arborescence (abrégée)`**

```
NKMedia/
  NKMedia.jenga · ROADMAP.md (1529 l. — la source de verite sur l'etat reel)
  src/NKMedia/
    NkMediaProbe.{h,cpp}        <- detection conteneur + pistes
    NkMediaDemux.{h,cpp}        <- extraction des paquets audio
    Video/
      NkVideoTypes.h            <- NkVideoInputFormat
      NkVideoReader.{h,cpp}     <- LECTURE : conteneur -> RGBA8
      NkVideoWriter.{h,cpp}     <- ECRITURE simple
      NkVideoRecorder.{h,cpp}   <- CAPTURE A/V threadee
      NkVideoConverter.{h,cpp}  <- sequence/AVI -> video
      NkImageSequenceWriter.{h,cpp}
      Containers/ NkAviWriter · NkMovWriter · NkMp4H264Writer · NkWebmWriter
    Audio/Containers/NkWavWriter.{h,cpp}
    Codecs/ Video/{H264,HEVC,VP8,VP9,AV1,Mpeg1,Mpeg2,Theora} · Aac · Opus · AMR
```

### Ici, les commentaires *sous-estiment* le module

Nous avons pris l'habitude, depuis le chapitre 5, de nous méfier des
commentaires qui promettent trop. NKMedia présente le problème inverse, et il
faut le savoir pour ne pas renoncer à des fonctionnalités qui existent.

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoReader.h:8-11`**

```cpp
//   - Conteneur MOV/MP4 (ISOBMFF) : piste vidéo -> MJPEG (mjpa/jpeg). H264 = à venir.
// H264/AVC (MP4 courant) exige un décodeur complet (gros chantier séparé) : la
// lecture d'une piste avc1 renvoie une erreur claire tant que le décodeur n'existe pas.
```

C'est faux. Le fichier `Video/NkVideoReader.cpp` affecte réellement
`info.codec` à `"h264"` (lignes 1592, 1828, 2002), `"hevc"`
(955, 1563), `"vp8"` (1572, 1650), `"vp9"` (1578, 1635),
`"av1"` (1586, 1644), `"mpeg2"` (1869), `"theora"` (1944),
en plus de `"mjpeg"`, `"rawrgb"` et `"image"`. Les
décodeurs sont branchés.

De même, `Video/NkVideoWriter.h:5-6` annonce « AVI pour l'instant ;
MP4/WebM à venir » alors que les écrivains MOV/MP4 et WebM sont livrés.

> **✅ La source de vérité de ce module**
>
> Pour NKMedia, la vérité n'est ni dans les en-têtes ni dans les
> `.jenga` : elle est dans `Kernel/Runtime/NKMedia/ROADMAP.md`, un
> document de 1 529 lignes tenu à jour. Prenez le réflexe de l'ouvrir avant de
> conclure qu'une fonctionnalité manque.

Voici l'essentiel de cette feuille de route, au moment de la rédaction :

| **Brique** | **Statut** |
|---|---|
| Probe / démux conteneurs | fini — MP4, WebM, WAV, OGG, MP3, FLAC |
| Extraction de paquets | fini — MP4 (`stbl` et fMP4), WebM |
| Opus CELT / SILK | fini (SILK exact au bit face à libopus) |
| Dispatcher Opus | *partiel* — mode hybride non fait |
| Décodeur AAC-LC | fini, exact au bit |
| Muxers (écriture) | fini — AVI, MOV/MP4, WAV, WebM |
| Vidéo (décodage) | fini — H.264 Main+High, VP8, VP9, HEVC |
| **Vidéo (encodage)** | **en cours** — RAW/MJPEG/MPEG-1 + H.264 baseline |
| AV1 / MPEG-2 / Theora | **restent à faire** |

> **⚠️ Ce sur quoi ce cours ne s'engage pas**
>
> - **L'encodage H.264** est marqué « en cours » et limité au profil
>   *baseline*. Il fonctionne — le `NkVideoRecorder` l'utilise
>   par défaut — mais ce n'est pas un chemin sur lequel bâtir un exercice de
>   débutant. **Restez en MJPEG** ;
> - **AV1, MPEG-2 et Theora** : le code de décodage existe, la feuille
>   de route ne les déclare pas terminés ;
> - **Opus en mode hybride** et en stéréo peut être « refusé
>   proprement » : un fichier `.opus` courant n'est pas garanti ;
> - **HEVC** refuse proprement les tuiles, le PCM, les
>   échantillonnages 4:2:2 et 4:4:4 ;
> - **VP8** refuse la segmentation par macrobloc et les partitions
>   multiples.
>
> « Refusé proprement » est une bonne nouvelle : cela signifie une erreur claire
> plutôt qu'une image corrompue.

## `NkMediaProbe` : qu'y a-t-il dans ce fichier ?

C'est la classe par laquelle commencer, parce qu'elle ne décode rien : elle lit
les en-têtes et vous dit ce que contient le fichier.

**`Kernel/Runtime/NKMedia/src/NKMedia/NkMediaProbe.h:21-70 (abrégé)`**

```cpp
enum class NkMediaContainer { NK_UNKNOWN, NK_MP4, NK_WEBM, NK_WAV, NK_OGG, NK_MP3, NK_FLAC };
enum class NkMediaTrackType { NK_UNKNOWN, NK_AUDIO, NK_VIDEO };

struct NkMediaTrack {
        NkMediaTrackType type = NkMediaTrackType::NK_UNKNOWN;
        NkString codec;          // "aac","opus","vorbis","vp8","vp9","h264","pcm","mp3",...
        int32 sampleRate = 0;    int32 channels = 0;      // audio
        int32 width = 0;         int32 height = 0;        // video
        int32 bitsPerSample = 0; bool pcmBigEndian = false;
        NkVector<nk_uint8> codecPrivate;                  // OpusHead pour Opus, etc.
};
struct NkMediaInfo {
        NkMediaContainer container = NkMediaContainer::NK_UNKNOWN;
        NkVector<NkMediaTrack> tracks;
        const char* ContainerName() const;
        const NkMediaTrack* FirstAudio() const;
        const NkMediaTrack* FirstVideo() const;
};
struct NkMediaProbe {
        static bool Probe(const uint8* data, usize size, NkMediaInfo& out);
        static bool ProbeFile(const char* path, NkMediaInfo& out);
        static NkMediaContainer DetectContainer(const uint8* data, usize size);
        static bool SelfTest();
};
```

L'usage réel, tiré de l'application de test :

**`Applications/NKMediaTest/src/main.cpp:331-353 (abrégé)`**

```cpp
    if (argc >= 2) {
        media::NkMediaInfo info;
        if (!media::NkMediaProbe::ProbeFile(argv[1], info)) {
            printf("[ERREUR] probe echoue : %s\n", argv[1]);
            return 1;
        }
        printf("Conteneur : %s\n", info.ContainerName());
        printf("Pistes    : %d\n", (int)info.tracks.Size());
        for (uint64 i = 0; i < info.tracks.Size(); ++i) {
            const media::NkMediaTrack &t = info.tracks[i];
            printf("  [%d] %-5s codec=%-6s", (int)i, TrackTypeName(t.type), t.codec.CStr());
            if (t.type == media::NkMediaTrackType::NK_AUDIO)
                printf(" %d Hz, %d canal(aux)", t.sampleRate, t.channels);
            if (t.type == media::NkMediaTrackType::NK_VIDEO)
                printf(" %dx%d", t.width, t.height);
            printf("\n");
        }
```

`FirstAudio()` et `FirstVideo()` évitent de parcourir soi-même les
pistes quand on ne s'intéresse qu'à la principale — ils rendent `nullptr`
s'il n'y en a pas.

> **✅ Ce qu'il faut retenir**
>
> Avant d'ouvrir un fichier avec `NkVideoReader`, sondez-le. Vous saurez
> alors si le codec est l'un de ceux que vous savez traiter, et vous pourrez
> afficher un message utile au lieu d'un échec muet. `ProbeFile` est
> instantané : il ne décode aucune image.

## Écrire une vidéo

### L'API

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoWriter.h:26-81`**

```cpp
enum class NkVideoCodec { RAW_BGR, MJPEG, MPEG1 };
enum class NkVideoContainer { AVI, MOV, ELEMENTARY };
// (NkVideoInputFormat { RGB24, RGBA32, BGR24 } vit dans Video/NkVideoTypes.h:16-20)

struct NkVideoConfig {
        int32 width = 0;  int32 height = 0;
        int32 fpsNum = 30;  int32 fpsDen = 1;
        NkVideoCodec codec = NkVideoCodec::MJPEG;
        NkVideoContainer container = NkVideoContainer::AVI;
        int32 quality = 90;                 // MJPEG 1..100
        int32 audioSampleRate = 0;          // 0 = video muette
        int32 audioChannels = 0;
};
class NkVideoWriter {
        bool Open(const char* path, const NkVideoConfig& cfg);
        bool WriteFrame(const uint8* pixels, NkVideoInputFormat fmt);
        bool AddAudioSamples(const int16* interleaved, usize frameCount);
        bool Close();
        bool IsOpen() const;   int32 FrameCount() const;
};
```

Notez la cadence exprimée en fraction : `fpsNum / fpsDen`. Pour du
30 images/s, `30/1` ; pour du 29,97 (NTSC), `30000/1001`. Les
formats vidéo n'aiment pas les flottants.

### Le squelette de référence

**`Applications/NKVideoTest/src/main.cpp:83-112`**

```cpp
    bool MakeVideo(const char *path, media::NkVideoCodec codec, media::NkVideoContainer container, int32 w, int32 h,
                   int32 fps, int32 nframes) {
        media::NkVideoConfig cfg;
        cfg.width = w;
        cfg.height = h;
        cfg.fpsNum = fps;
        cfg.fpsDen = 1;
        cfg.codec = codec;
        cfg.container = container;
        cfg.quality = 88;

        media::NkVideoWriter vw;
        if (!vw.Open(path, cfg)) {
            ::printf("  [ERREUR] ouverture %s\n", path);
            return false;
        }
        uint8 *rgb = (uint8 *)memory::NkAlloc((usize)w * h * 3);
        for (int32 fr = 0; fr < nframes; ++fr) {
            RenderFrame(rgb, w, h, fr, nframes);
            if (!vw.WriteFrame(rgb, media::NkVideoInputFormat::RGB24)) {
                ::printf("  [ERREUR] trame %d\n", fr);
                memory::NkFree(rgb);
                vw.Close();
                return false;
            }
        }
        memory::NkFree(rgb);
        vw.Close();
        return true;
    }
```

Quatre choses à retenir de ce bloc.

1. **Le tampon est alloué une fois**, hors de la boucle, et réutilisé.
   Allouer une image par trame est le meilleur moyen de rendre l'encodage
   plus lent que le décodage ;
2. **la mémoire vient de `memory::NkAlloc`** et repart par
   `memory::NkFree` — la règle du chapitre 5 ne change pas ;
3. **le chemin d'erreur appelle quand même `Close()`** ;
4. **le format d'entrée est explicite** :
   `NkVideoInputFormat::RGB24`. Le writer accepte aussi
   `RGBA32` et `BGR24` et fait la conversion.

> **⚠️ `Close()` n'est pas optionnel**
>
> Un fichier AVI se termine par un index ; un MP4 par un atome `moov`.
> Aucun des deux n'est écrit tant que `Close()` n'a pas été appelé. Sans
> lui, vous obtenez un fichier de la bonne taille, contenant toutes vos images, et
> qu'**aucun lecteur au monde n'ouvrira**. Le message typique est
> « `moov atom not found` ».
>
> Le dépôt prend explicitement cette précaution dans son code de capture :
>
> **`Applications/NKViewportDemo/src/NKViewportDemo/main.cpp:358-362`**
>
> ```cpp
>     // Robustesse : si la fenêtre est fermée AVANT la fin de l'enregistrement, on finalise quand même
>     // (écrit le moov) → le MP4 reste lisible (sinon "moov atom not found").
>     if (rec.IsRecording()) {
>         rec.End();
>     }
> ```
>
> La même règle vaut pour `NkVideoRecorder::End()` et
> `NkWebmWriter::Finalize()`. **Prévoyez une finalisation sur tous les
> chemins de sortie, y compris la fermeture brutale de la fenêtre.**

### Deux limites de l'écriture

- **L'audio n'est pas supportée partout** :
  `Video/NkVideoWriter.h:50-52` précise que la piste audio est
  « supportée par AVI … et MOV/MP4 … Non supportée par
  ELEMENTARY/MPEG1 (**Open échoue si demandée**) ». Au moins l'échec
  est franc ;
- **le sens des pixels** : les encodeurs veulent du *haut en
  bas* (`Video/NkVideoTypes.h:16-20`). Or OpenGL rend de bas en
  haut. Nous verrons le paramètre qui corrige cela.

### La séquence d'images, à la manière de Blender

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkImageSequenceWriter.h:21-42`**

```cpp
enum class NkImageSeqFormat { PNG, JPEG, BMP, TGA, QOI };
struct NkImageSequenceWriter {
        bool Open(const char* dir, const char* basename, int32 width, int32 height,
                  NkImageSeqFormat fmt, int32 padding = 4, int32 quality = 90);
        bool WriteFrame(const uint8* pixels, NkVideoInputFormat fmt);
        bool Close();  int32 FrameCount() const;
};
```

**`Applications/NKVideoTest/src/main.cpp:185-201`**

```cpp
        media::NkImageSequenceWriter seq;
        bool ok = seq.Open(outDir, "nkframe", W, H, media::NkImageSeqFormat::PNG, 4, 90);
        if (ok) {
            uint8 *rgb = (uint8 *)memory::NkAlloc((usize)W * H * 3);
            for (int32 fr = 0; fr < 5 && ok; ++fr) { // 5 images d'exemple
                RenderFrame(rgb, W, H, fr, N);
                ok = seq.WriteFrame(rgb, media::NkVideoInputFormat::RGB24);
            }
            memory::NkFree(rgb);
            seq.Close();
        }
```

Cela produit `nkframe_0000.png`, `nkframe_0001.png`… Cette
classe délègue entièrement aux codecs de NKImage : c'est le chemin le plus fiable
du module, et le plus facile à déboguer — vous pouvez ouvrir chaque image.

Et pour transformer ensuite la séquence en vidéo, il existe un raccourci en une
ligne :

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoConverter.h:24-30`**

```cpp
static int32 ImageSequenceToVideo(const char* prefix, int32 digits, const char* suffix,
                                  int32 first, int32 count, const char* outPath,
                                  int32 fpsNum, int32 fpsDen, int32 quality = 90);
static int32 MjpegAviToVideo(const char* aviPath, const char* outPath, int32 quality = 90);
```

Ces deux fonctions renvoient le *nombre de trames* traitées, pas un
booléen : zéro signifie l'échec.

## Lire une vidéo

### L'API

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoReader.h:26-70`**

```cpp
    struct NkVideoFrame {
            NkVector<nk_uint8> rgba;         // width*height*4, row-major, haut->bas
            int32 width = 0;   int32 height = 0;
            int64 timestampMs = 0;
            int32 index = -1;
    };
    struct NkVideoReaderInfo {
            int32 width = 0;   int32 height = 0;
            int32 frameCount = -1;           // -1 = inconnu
            double fps = 0.0;
            NkString codec;                  // "mjpeg"|"rawrgb"|"h264"|"hevc"|"vp8"|"vp9"|...
            NkString container;              // "avi"|"mov"|"mp4"|"sequence"
    };
    class NkVideoReader {
            NkVideoReader(const NkVideoReader&) = delete;          // NON COPIABLE
            bool Open(const char* path);
            bool IsOpen() const;
            const NkVideoReaderInfo& Info() const;
            bool ReadFrame(NkVideoFrame& out);                      // false = fini
            bool SeekFrame(int32 index);
            int32 CurrentIndex() const;
            void Close();
            static bool SelfTest();
    };
```

Le point capital est dans le commentaire de `NkVideoFrame::rgba` :
**RGBA8, ligne par ligne, de haut en bas**. C'est exactement le format
qu'attendent `NkTexture::Update` et `UploadImageRGBA`. Il n'y a
aucune conversion à écrire.

### La boucle de lecture

**`Applications/NkVideoReadTest/src/main.cpp:3041-3070 (abrégé)`**

```cpp
    const char *path = argv[1];
    NkVideoReader rd;
    if (!rd.Open(path)) {
        printf("  [KO] impossible d'ouvrir/lire : %s\n", path);
        return 1;
    }
    const NkVideoReaderInfo &in = rd.Info();
    printf("  conteneur=%s codec=%s  %dx%d  %.2f fps  frames=%d\n", in.container.CStr(), in.codec.CStr(), in.width,
           in.height, in.fps, in.frameCount);

    int32 count = 0;
    NkVideoFrame fr;
    while (rd.ReadFrame(fr)) {
        /* ... traiter fr.rgba ... */
        ++count;
    }
```

Remarquez que `fr` est déclarée *hors* de la boucle et réutilisée :
son `NkVector` conserve sa capacité d'une trame à l'autre. La déclarer
dans la boucle provoquerait une allocation par image.

`Open` accepte aussi un **dossier d'images** : le champ
`container` vaut alors `"sequence"` et le codec `"image"`.
C'est le moyen le plus simple de tester votre code de lecture sans avoir de
vidéo sous la main.

### Le seek n'est pas ce que vous croyez

**`Applications/NkVideoReadTest/src/main.cpp:2762-2765`**

```
 SeekFrame se contente de repositionner sur l'IDR précédente (voir implémentation) : il
 faut ensuite redécoder EN AVANT jusqu'à la cible, exactement comme le fait la boucle de
 rattrapage du lecteur (Applications/NkVideoPlayer).
```

Dans un format à trames inter-dépendantes comme H.264, l'image numéro 1 000 ne
peut pas être décodée seule : elle décrit des différences par rapport aux
précédentes. `SeekFrame(1000)` vous replace donc sur la dernière image
autonome avant la cible, et c'est à vous d'appeler `ReadFrame` jusqu'à ce
que `CurrentIndex() >= 1000`.

Sur une séquence d'images ou un AVI MJPEG — où chaque image est indépendante —
le seek est exact.

### Décoder coûte cher

Une phrase à graver : **ne décodez jamais plus d'une image par image
affichée**. Le lecteur du dépôt borne explicitement sa boucle de rattrapage :

**`Applications/NkVideoPlayer/src/main.cpp:562-620 (extrait)`**

```cpp
        if (!paused && !ended) {
            if (audioOn && !streamPlayer.IsFinished())
                mediaClock = streamPlayer.GetPositionSeconds();
            else
                mediaClock += (float64)(dt * speed);
            const int32 targetIdx = (int32)(mediaClock * (float64)fps);
            /* resynchronisation dure si on a plus de 1,5 s de retard */
            const int32 kHardResyncLagFrames = (int32)(fps * 1.5f);
            if (!firstFrame && (targetIdx - reader.CurrentIndex()) > kHardResyncLagFrames) {
                if (reader.SeekFrame(targetIdx) && reader.ReadFrame(fr))
                    havePendingFrame = true;
            }
            NkChrono burstTimer;
            while ((firstFrame || reader.CurrentIndex() < targetIdx) &&
                   burstTimer.Elapsed().ToMilliseconds() < 150.0) {
                if (reader.ReadFrame(fr)) { havePendingFrame = true; firstFrame = false; }
                else if (loop) { /* SeekFrame(0) + ReadFrame */ }
                else { paused = true; break; }
            }
            if (havePendingFrame)
                pushFrame(); // upload + dessin UNE SEULE FOIS, avec la derniere image de la rafale
        }
```

Trois garde-fous dans ce seul bloc : la rafale de rattrapage est limitée à
150 ms de temps mur ; au-delà d'une seconde et demie de retard on saute
carrément ; et surtout, **l'upload GPU n'a lieu qu'une fois**, avec la
dernière image de la rafale. Décoder dix images pour n'en afficher qu'une est
acceptable ; en uploader dix ne l'est pas.

> **✅ L'horloge maîtresse**
>
> Notez la première ligne : quand l'audio joue, c'est *lui* qui donne
> l'heure (`mediaClock = streamPlayer.GetPositionSeconds()`). Ce n'est
> qu'en son absence qu'on accumule le *delta time*. La raison est
> physiologique : l'oreille détecte un hoquet audio de 20 ms, l'œil ne voit pas
> une image manquante. On synchronise donc toujours l'image sur le son, jamais
> l'inverse.

## Afficher la vidéo

### Dans une fenêtre NkCanvas

Le principe : créer **une** texture à la taille de la vidéo, puis la mettre
à jour à chaque nouvelle image.

**`Applications/NkVideoPlayer/src/main.cpp:355-362`**

```cpp
    // ── 4) Texture RGBA de la taille vidéo + sprite d'affichage ───────────────
    NkTexture frameTex;
    if (!frameTex.Create(*target.GetRenderer(), (uint32)vidW, (uint32)vidH, NkColor2D{0, 0, 0, 255})) {
        logger.Error("[NkVideoPlayer] echec creation texture {0}x{1}", vidW, vidH);
        window.Close();
        return -5;
    }
    NkSprite sprite(frameTex);
```

puis, à chaque image décodée :

**`Applications/NkVideoPlayer/src/main.cpp:424-431`**

```cpp
    auto pushFrame = [&]() {
        rawW = fr.width;
        rawH = fr.height;
        rawFrame = fr.rgba;
        applyLook(fr.rgba.Data(), (uint64)fr.width * fr.height);
        frameTex.Update(fr.rgba.Data(), (uint32)fr.width, (uint32)fr.height, 0, 0);
        haveFrame = true;
    };
```

et le dessin, qui n'a rien de particulier :

**`Applications/NkVideoPlayer/src/main.cpp:664-668`**

```cpp
        target.Clear(NkColor2D{0, 0, 0, 255});
        if (haveFrame)
            target.Draw(static_cast<const NkDrawable &>(sprite));
        target.Display();
```

> **⚠️ Ne recréez jamais la texture par image**
>
> `Create` alloue une texture GPU ; `Update` y recopie des pixels.
> Appeler `Create` à chaque trame, c'est allouer et libérer une texture
> soixante fois par seconde : la mémoire GPU se fragmente et le pilote finit par
> protester. Créez une fois, mettez à jour ensuite.

### Dans une interface

Le motif est le même, avec l'API du backend d'interface. L'IDE du dépôt en donne
la version complète, y compris le cas du changement de dimensions :

**`Applications/NKCode/src/NKCode/Shell/NkVideoViewer.h:70-82`**

```cpp
        inline void NkVideoUpload(editorkit::NkEditorShell *shell, NkVideoClip *c) {
            if (!shell || c->frame.rgba.Empty() || c->frame.width <= 0 || c->frame.height <= 0)
                return;
            const uint8 *px = c->frame.rgba.Data();
            if (c->texId == 0 || c->texW != c->frame.width || c->texH != c->frame.height) {
                c->texId = shell->UploadRGBA(px, c->frame.width, c->frame.height);
                c->texW = c->frame.width;
                c->texH = c->frame.height;
            } else {
                shell->UpdateRGBA(c->texId, px, c->frame.width, c->frame.height);
            }
            c->haveFrame = c->texId != 0;
        }
```

Premier appel ou changement de taille : `UploadRGBA`, qui crée. Sinon :
`UpdateRGBA`, qui recopie. C'est exactement la même logique que pour
NKImage au chapitre 5, avec une texture qui vit plus longtemps qu'une frame.

Et le cadencement, côté interface :

**`Applications/NKCode/src/NKCode/Shell/NkVideoViewer.h:153-170`**

```cpp
            float32 dt = ctx.input.dt;
            if (dt <= 0.f || dt > 0.25f)
                dt = 1.f / 60.f; // garde-fou (onglet revenu au premier plan / hoquet)
            const float32 frameDur = 1.f / (c->fps * (c->speed > 0.01f ? c->speed : 0.01f));
            if (c->playing && !c->ended) {
                c->acc += dt;
                if (c->acc > frameDur * 4.f)
                    c->acc = 0.f; // evite le rattrapage explosif
                while (c->acc >= frameDur) {
                    c->acc -= frameDur;
                    if (c->reader->ReadFrame(c->frame)) {
                        c->index = c->frame.index;
                        NkVideoUpload(shell, c);
                    }
                    /* ... loop / fin ... */
                }
            }
```

Les deux garde-fous méritent d'être notés, parce qu'ils correspondent à deux
situations réelles : un *delta time* aberrant quand l'onglet revient au
premier plan après plusieurs secondes, et un accumulateur qui a débordé et
voudrait décoder quarante images d'un coup.

## Enregistrer ce que l'application affiche

### L'API

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoRecorder.h:39-77`**

```cpp
enum class NkRecorderCodec { H264, MJPEG };

struct NkVideoRecorder {
        bool Begin(const char* path, int32 width, int32 height, int32 fpsNum = 60, int32 fpsDen = 1,
                   int32 qp = 20, int32 maxQueuedFrames = 32,
                   NkRecorderCodec codec = NkRecorderCodec::H264, int32 mjpegQuality = 90);
        int32 AddAudio(int32 sampleRate, int32 channels, const char* lang3 = nullptr);
        int32 AddSubtitleTrack(const char* lang3 = nullptr);
        bool PushVideo(const uint8* pixels, NkVideoInputFormat fmt, bool flipVertical = false);
        void PushAudio(int32 trackIdx, const int16* interleaved, uint32 frames);
        void AddSubtitle(int32 trackIdx, const char* utf8, uint32 startMs, uint32 durMs);
        bool End();
        bool IsRecording() const;  int32 FrameCount() const;
        int32 QueueDepth();  uint64 DroppedFrames();  double EncodeFps();
};
```

La différence avec `NkVideoWriter` est architecturale :

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoRecorder.h:5-8`**

```
 **Encodage sur un THREAD de fond** : la boucle de rendu pousse les trames capturées
 dans une file (rapide) et un worker encode en arrière-plan → l'application reste
 FLUIDE pendant la capture (l'encodeur H.264 est lourd).
```

`PushVideo` rend la main immédiatement ; l'encodage a lieu ailleurs.
`End()` joint ce fil : **ne détruisez jamais un recorder sans avoir
appelé `End()`**.

### La capture, de bout en bout

Le point de jonction avec le reste du moteur est délicieusement simple : la
capture d'écran est une `NkImage`.

**`Applications/NKViewportDemo/src/NKViewportDemo/main.cpp:329-362 (abrégé)`**

```cpp
        if (record && target->CaptureToImage(capImg) && capImg.IsValid()) {
            if (!rec.IsRecording()) {
                if (rec.Begin("viewport_capture.mp4", capImg.Width(), capImg.Height(), 30, 1, 24)) {
                    recAudio = rec.AddAudio(kRate, 1, "fre");
                    const int32 st = rec.AddSubtitleTrack("fre");
                    rec.AddSubtitle(st, "Capture live NKCanvas (DX11) - Nkentseu", 0, 4000);
                }
            }
            if (rec.IsRecording()) {
                rec.PushVideo(capImg.Pixels(), media::NkVideoInputFormat::RGBA32);
                rec.PushAudio(recAudio, recTone.Data(), (uint32)kAudioPerFrame);
                if (recFrames >= kRecTotal) { rec.End(); running = false; }
            }
        }
```

`NkRenderWindow::CaptureToImage(NkImage&)` remplit une `NkImage`,
et `capImg.Pixels()` part directement dans `PushVideo`. NKImage,
NkCanvas et NKMedia se rejoignent en une ligne.

### Trois pièges du recorder

> **⚠️ L'ordre des appels**
>
> `Video/NkVideoRecorder.h:51-54` : `AddAudio` et
> `AddSubtitleTrack` sont à appeler « juste après `Begin` (avant les
> trames) ». Ajouter une piste en cours d'enregistrement n'a pas de sens dans un
> conteneur MP4, dont la table des pistes est écrite en tête.

> **⚠️ En MJPEG, l'audio est ignorée — silencieusement**
>
> **`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoRecorder.h:36-38`**
>
> ```
>  conteneur MOV/MP4, **VIDEO SEULE (audio/sous-titres ignores dans ce mode)**
> ```
>
> C'est le piège le plus vicieux du module : `AddAudio` vous rend un index
> parfaitement valide, `PushAudio` accepte vos échantillons sans broncher,
> et le fichier produit est muet. Rien ne vous prévient.
>
> Vous êtes donc devant un choix : MJPEG (léger, sûr, muet) ou H.264 (avec audio,
> mais l'encodeur est marqué « en cours »). Pour apprendre, prenez MJPEG et
> enregistrez l'audio à part.

> **⚠️ Le recorder abandonne des trames**
>
> **`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoRecorder.h:45-46`**
>
> ```
>  `maxQueuedFrames` BORNE la file video : si l'encodeur (H.264 lourd) prend du retard,
>  les nouvelles trames sont ABANDONNEES (drop-newest) au lieu de gonfler la memoire
>  (temps reel : perdre une trame vaut mieux que geler). Abandons comptes (DroppedFrames()).
> ```
>
> C'est le bon comportement — mais il faut le savoir, sinon on s'étonne qu'une
> capture de dix secondes à 60 images/s en contienne 400. Surveillez
> `DroppedFrames()` et `QueueDepth()`, et régulez si nécessaire :
>
> **`Applications/Sandbox/src/Demo/main.cpp:1025-1050 (extrait)`**
>
> ```cpp
>                     const bool encoderBusy = recorder && recorder->QueueDepth() >= 24;
>                     if (!encoderBusy)
>                         (void)recordCapture.EnqueueCopy(...);
> ```

### Le sens des pixels

**`Kernel/Runtime/NKMedia/src/NKMedia/Video/NkVideoRecorder.h:56-57`**

```
 `flipVertical` pour framebuffer bottom-up (OpenGL)
```

OpenGL considère l'origine en bas à gauche ; les formats vidéo la placent en
haut à gauche. Si votre capture provient d'un *framebuffer* OpenGL lu
directement, passez `flipVertical = true`. Le symptôme d'un oubli est
sans ambiguïté : la vidéo est à l'envers.

## Le démux audio, en deux mots

Nous n'en aurons pas besoin dans ce cours, mais vous croiserez la classe :

**`Kernel/Runtime/NKMedia/src/NKMedia/NkMediaDemux.h:22-48`**

```cpp
struct NkMediaPacket {
        usize offset = 0;   // position dans le buffer SOURCE (ne copie pas !)
        usize size = 0;
        int64 timestampMs = 0;
        int64 granule = -1;             // OGG
        int64 discardPaddingNs = 0;     // WebM
};
struct NkMediaDemux {
        static bool ExtractAudioPackets(const uint8* data, usize size, const NkMediaInfo& info,
                                        NkVector<NkMediaPacket>& out);
        static bool ExtractAudioPacketsFile(const char* path, NkVector<nk_uint8>& outBytes,
                                            NkMediaInfo& outInfo, NkVector<NkMediaPacket>& out);
        static bool SelfTest();
};
```

> **⚠️ Les paquets pointent dans le tampon source**
>
> `NkMediaPacket` ne contient pas de données : il contient un
> *décalage*. Le commentaire est explicite — « Un paquet encodé (pointe dans
> le buffer d'origine ; ne copie pas) » (`NkMediaDemux.h:22`), et pour la
> variante fichier : « `outBytes` reçoit le buffer (**à garder
> vivant** car les paquets pointent dedans) » (`NkMediaDemux.h:47`).
>
> Si votre `NkVector<nk_uint8> bytes` sort de portée avant que vous ayez
> fini de lire les paquets, vous lisez de la mémoire libérée. Gardez les deux
> ensemble, dans la même portée.

## Vérifier que tout marche sur votre machine

NKMedia est le module le mieux doté du moteur en auto-tests — il en compte 52.
L'application `NkVideoReadTest`, lancée *sans argument*, les
enchaîne :

**`Applications/NkVideoReadTest/src/main.cpp:252-276`**

```cpp
    if (argc < 2) {
        printf("  [self-test] ecrire AVI MJPEG -> relire -> verifier...\n");
        bool ok = NkVideoReader::SelfTest();
        printf("  [ %s ] NkVideoReader::SelfTest (AVI MJPEG round-trip)\n", ok ? "OK " : "KO");
        bool okH264 = NkH264Decoder::SelfTest();
        bool okCavlc = NkH264Cavlc::SelfTest();
        bool okVp9 = NkVp9Decoder::SelfTest();
        bool okAv1 = NkAv1Decoder::SelfTest();
        bool okHevc = NkHevcDecoder::SelfTest();
        bool okHevcCabac = NkHevcCabacState::SelfTest();
        bool okWav = NkWavWriter::SelfTest();
        bool okWebm = NkWebmWriter::SelfTest();
        bool all = ok && okH264 && okCavlc && okVp9 && okAv1 && okHevc && okHevcCabac && okWav && okWebm;
        printf("=== %s ===\n", all ? "LECTURE VIDEO OPERATIONNELLE" : "ECHEC");
        return all ? 0 : 1;
    }
```

Le premier de la liste est le plus parlant : il écrit un AVI MJPEG, le relit, et
vérifie que ce qui sort correspond à ce qui est entré. C'est ce
*round-trip* qui valide indirectement `NkVideoWriter`, qui n'a pas
d'auto-test propre — pas plus que `NkVideoRecorder`,
`NkVideoConverter`, `NkImageSequenceWriter`, ni les écrivains de
conteneurs.

Deux détails pratiques :

- ces auto-tests **écrivent dans le répertoire courant** :
  `nkvideoreader_selftest.avi`,
  `nkwavwriter_selftest.wav`,
  `nkwebmwriter_selftest.webm`. Lancez-les depuis un dossier de
  travail, pas depuis la racine du dépôt ;
- le décodeur HEVC lit une variable d'environnement,
  `NK_HEVC_THREADS` : absente ou à 0, il choisit tout seul ; à 1,
  il travaille séquentiellement (`NkVideoReadTest/src/main.cpp:249-250`).
  Utile pour départager un bug de décodage d'un bug de parallélisme.

## Une leçon de performance qui dépasse la vidéo

La feuille de route documente un bug de performance instructif :

**`Kernel/Runtime/NKMedia/ROADMAP.md:1447-1458 (abrégé)`**

```
 le vrai coupable etait `NkVideoReader::ReadFrame` [...] : la gestion du DPB (liste de
 references) COPIAIT EN PROFONDEUR (au lieu de deplacer) jusqu'a 16 `NkH264Frame`
 (plans Y/Cb/Cr + grilles de mouvement, ~380 Ko/image) DEUX FOIS par frame decodee
 [...] cout mesure ~80 ms/frame et CROISSANT [...] >90% du temps total. Fix : traits::NkMove
```

Quatre-vingts millisecondes par image, dont plus de 90 % en copies inutiles.
La leçon est transférable bien au-delà de la vidéo : **dans une boucle
chaude, une copie profonde qui aurait dû être un déplacement peut représenter
l'essentiel du temps d'exécution**. Le champ `NkVideoFrame::rgba` est un
conteneur : le copier par valeur à chaque image coûte cher, et le lecteur du
dépôt ne le fait que parce qu'il a explicitement besoin d'une copie non
retouchée (`Applications/NkVideoPlayer/src/main.cpp:427`).

Le même document ajoute un avertissement de méthode, qui vaut pour toute mesure :

**`Kernel/Runtime/NKMedia/ROADMAP.md:1449-1452`**

```
 (mesure fiable : chrono mur autour d'un run borne, PAS le champ `t=` de `NkVideoReadTest`
 qui affiche le PTS video, pas le temps de decodage reel — piege rencontre une fois)
```

Le *PTS* est l'instant auquel une image doit être affichée dans la vidéo. Il
n'a rien à voir avec le temps qu'il a fallu pour la décoder. Confondre les deux,
c'est mesurer la durée du film au lieu de la vitesse du lecteur.

## Exercices

> **✏️ 1 — Sonder avant d'ouvrir**
>
> Écrivez un petit outil en ligne de commande qui prend un chemin, appelle
> `NkMediaProbe::ProbeFile`, et affiche le conteneur et toutes les pistes.
> Passez-lui successivement un WAV, un MP3, un MP4 et un fichier texte renommé en
> `.mp4`.
>
> Ajoutez ensuite une règle : si la première piste vidéo n'a pas un codec parmi
> `"mjpeg"`, `"rawrgb"` ou `"image"`, affichez un
> avertissement disant que la lecture n'est pas garantie par ce cours. Vous venez
> d'écrire le garde-fou qui manque à la plupart des applications.

> **✏️ 2 — Le *round-trip* complet**
>
> Générez une vidéo de 100 images en MJPEG/AVI, avec un motif dont vous pouvez
> prédire le contenu — par exemple un rectangle qui se déplace d'un pixel par
> image. Relisez-la ensuite avec `NkVideoReader` et vérifiez que le nombre
> d'images correspond, et que le rectangle est bien là où vous l'attendez à
> l'image 50.
>
> Recommencez avec le conteneur MOV. Puis essayez `RAW_BGR` et comparez la
> taille des fichiers : vous aurez une idée concrète de ce que compresser veut
> dire.

> **✏️ 3 — Prouver que `Close()` est obligatoire**
>
> Reprenez l'exercice précédent et commentez l'appel à `vw.Close()`.
> Regardez la taille du fichier produit — elle est presque identique — puis
> essayez de l'ouvrir avec votre lecteur vidéo habituel, et avec
> `NkVideoReader`.
>
> Cet exercice prend deux minutes et vous fera gagner une soirée le jour où votre
> application se fermera brutalement en cours d'enregistrement.

> **✏️ 4 — Un lecteur cadencé, dans votre interface**
>
> Ajoutez à votre application NKGui un panneau qui lit une séquence d'images
> depuis un dossier (c'est le chemin le plus simple : `Open` accepte un
> dossier) et l'affiche avec le widget `Image` du chapitre 5.
>
> Cadencez sur `Info().fps` avec un accumulateur, en reprenant les deux
> garde-fous de `NkVideoViewer` : *delta time* aberrant et
> accumulateur débordé. Ajoutez un bouton pause et un `SliderFloat` de
> vitesse. Vérifiez que vous n'appelez `UploadRGBA` qu'une seule fois — au
> premier affichage — et `UpdateRGBA` ensuite.
