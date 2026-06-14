# La capture caméra

> Couche **Runtime** · NKCamera · Ouvrir un **périphérique de capture** réel (webcam, caméra
> avant/arrière mobile), recevoir ses **frames** image par image, prendre des photos, et — sur
> mobile/XR — piloter une caméra virtuelle 2D à partir de l'**IMU** du téléphone.

Tôt ou tard, une application a besoin de **voir le monde réel** : un avatar qui suit le visage de
l'utilisateur, un scanner de QR code, un fond vidéo en réalité augmentée, un aperçu webcam dans un
outil de visioconférence. NKCamera est la couche qui répond à ce besoin de façon **uniforme sur
toutes les plateformes** — Win32, Linux (V4L2), macOS/iOS (AVFoundation), Android (Camera2),
navigateur (`getUserMedia`) — en cachant derrière une seule façade les API natives, qui n'ont
absolument rien en commun. La question n'est jamais « comment parler à DirectShow ou à V4L2 » (le
backend s'en charge), mais « comment **récupérer mes frames sans en perdre**, et qu'en faire ».

Le point d'entrée pratique est le **singleton** `NkCameraSystem`, qu'on atteint par le raccourci
global `NkCamera()`. Il gère **une seule caméra physique à la fois** : on l'ouvre avec une
`NkCameraConfig`, on lit les frames via `GetLastFrame` ou une `queue` thread-safe, et on referme.
Pour plusieurs caméras simultanées (stéréo, surveillance multi-flux), il existe `NkMultiCamera`, où
chaque `Stream` encapsule son propre backend indépendant.

Une chose à comprendre d'emblée : NKCamera **ne dessine rien** et **n'écrit aucun fichier**. Ce
n'est ni un moteur de rendu ni un encodeur — c'est une **source de pixels en mémoire**. Une frame
arrive sous forme de `NkCameraFrame` (un `NkVector<uint8>` de pixels) ; à vous de l'envoyer au GPU,
de la convertir, ou de l'analyser. Les API d'écriture fichier (`CapturePhotoToFile`,
`SaveFrameToFile`) sont conservées **uniquement pour compatibilité** et renvoient `false`.

- **Namespace** : `nkentseu` (tous les symboles)
- **Header d'entrée** : `#include "NKCamera/NkCameraSystem.h"` (tire `NKICameraBackend.h` →
  `NkCameraTypes.h` + le backend de la plateforme). `NkCamera2D.h` est indépendant.
- **Dépendance externe** : `NkPixelFormat` vient de NKWindow (`NKWindow/Core/NkTypes.h`), pas de
  NKCamera. NKCamera n'ajoute que `NkCameraPixelFormatToString`.

---

## Décrire ce qu'on veut : `NkCameraConfig` et les présets

Avant d'ouvrir quoi que ce soit, il faut **dire au backend ce qu'on attend** : quelle caméra,
quelle résolution, quel format de sortie. C'est le rôle de `NkCameraConfig`. Plutôt que de jongler
avec des dimensions en dur, on choisit un **préset** (`NkCameraResolution`) — `NK_CAM_RES_HD`
(1280×720) par défaut, jusqu'à `NK_CAM_RES_4K` — et le système matérialise les pixels pour vous.

Le piège tient en un appel : un préset n'est qu'une **étiquette**. Tant qu'on n'a pas appelé
`Resolve()`, les champs `width`/`height` valent 0, et le backend retombe alors sur un fallback
640×480. `Resolve()` traduit le préset en dimensions concrètes (et force 640×480 / 30 fps si tout
est nul). C'est donc le geste à ne pas oublier dès qu'on fixe un préset à la main.

```cpp
NkCameraConfig cfg;
cfg.deviceIndex = 0;                 // la première caméra énumérée
cfg.preset      = NK_CAM_RES_HD;     // 1280x720
cfg.fps         = 30;
cfg.outputFormat = NK_PIXEL_RGBA8;   // on veut du RGBA prêt à téléverser
cfg.Resolve();                       // matérialise width=1280, height=720
```

Ce n'est **pas** une promesse stricte : le backend choisit le mode le plus proche que le matériel
sait fournir (parcourez `NkCameraDevice::modes` pour connaître les modes réellement supportés). Les
booléens `autoFocus`/`autoExposure`/`autoWhiteBalance` et `flipHorizontal` sont des **souhaits** que
le backend honore s'il le peut.

> **En résumé.** `NkCameraConfig` décrit la session souhaitée ; choisissez un `NkCameraResolution`
> et appelez **`Resolve()`** avant l'ouverture pour matérialiser `width`/`height` (sinon fallback
> 640×480). `deviceIndex` désigne quelle caméra ; `outputFormat` le format des frames reçues.

---

## Recevoir les frames sans en perdre : `NkCameraFrame` et la queue

Une fois le flux ouvert avec `StartStreaming(cfg)`, les frames arrivent. La plus simple façon de les
lire est `GetLastFrame(out)` : il **copie la dernière frame disponible** dans votre `NkCameraFrame`,
de façon **thread-safe**. C'est parfait pour un aperçu vidéo, où ne montrer que l'image la plus
récente suffit — si deux frames arrivent entre deux affichages, on jette la plus vieille sans
remords.

Mais ce n'est **pas** ce qu'on veut quand chaque frame compte (analyse, enregistrement, traitement).
Là, on active une **file thread-safe** avec `EnableFrameQueue(maxQueueSize)` (4 par défaut), puis on
**défile** une à une avec `DrainFrameQueue(out)` dans la boucle principale. La queue absorbe les
décalages entre le thread de capture (qui pousse) et le thread de jeu (qui consomme), de la même
manière qu'un `NkDeque` sert de tampon entre un producteur et un consommateur.

```cpp
NkCamera().EnableFrameQueue(4);
NkCamera().StartStreaming(cfg);

// boucle principale
NkCameraFrame frame;
while (NkCamera().DrainFrameQueue(frame)) {
    if (frame.format != NK_PIXEL_RGBA8)
        NkCameraSystem::ConvertToRGBA8(frame);   // garantir du RGBA
    texture.Upload(frame.data.Data(), frame.width, frame.height);
}
```

La frame elle-même (`NkCameraFrame`) possède ses pixels (`NkVector<uint8> data`), connaît son
`width`/`height`/`stride`, son `format`, un `timestampUs` en microsecondes et un `frameIndex`. Un
accesseur `GetPixelRGBA(x, y)` lit un pixel packé — **mais uniquement** si `format == NK_PIXEL_RGBA8`
(sinon il renvoie 0). D'où le réflexe : si vous comptez lire des pixels, convertissez d'abord.

> **En résumé.** `GetLastFrame` = dernière image, thread-safe, idéal pour l'aperçu (on peut sauter
> des frames). `EnableFrameQueue` + `DrainFrameQueue` = ne rien rater, idéal pour l'analyse/
> l'enregistrement. `GetPixelRGBA` n'opère qu'en `NK_PIXEL_RGBA8` → `ConvertToRGBA8` d'abord.

---

## Piloter une caméra virtuelle avec l'IMU : `NkCamera2D`

Sur mobile et en XR, la caméra du téléphone est aussi un **capteur d'orientation** : son IMU
(gyroscope + accéléromètre) sait comment l'appareil est tenu. NKCamera exploite cela pour faire
**bouger une caméra virtuelle 2D** quand on incline le téléphone — un effet de parallaxe, un fond
qui réagit, une visée à la « regarde autour de toi ».

`NkCamera2D` est volontairement minimal : il ne fait **pas** de rendu, ne connaît aucun backend
graphique. Il stocke juste une position et une rotation, qu'on relie au système par
`SetVirtualCameraTarget(&cam2D)`. À chaque frame, `UpdateVirtualCamera(dt)` lit l'orientation IMU du
backend et l'applique à la caméra liée, avec un **lissage** configurable (`VirtualCameraMapConfig`)
pour éviter les saccades.

```cpp
NkCamera2D virtualCam;
NkCamera().SetVirtualCameraTarget(&virtualCam);   // ownership reste à VOUS
NkCamera().SetVirtualCameraMapping(true);

// chaque frame
NkCamera().UpdateVirtualCamera(dt);
float x = virtualCam.GetX(), rot = virtualCam.GetRotation();
```

Attention à la **durée de vie** : le système ne possède pas le pointeur, c'est à vous de garantir
que `virtualCam` survit aussi longtemps qu'il est lié. Et comme tout le reste ici, cela suppose que
le backend supporte l'IMU (`GetOrientation` renvoie `false` sur desktop) — sinon rien ne bouge.

> **En résumé.** `NkCamera2D` = caméra logique (position + rotation, sans rendu) qu'on lie via
> `SetVirtualCameraTarget` puis qu'on rafraîchit chaque frame avec `UpdateVirtualCamera(dt)`. Le
> lissage vient de `VirtualCameraMapConfig` ; l'IMU n'existe que sur les backends mobile/XR.

---

## Aperçu de l'API

Tous les symboles sont dans `nkentseu`. `NkPixelFormat` provient de NKWindow. Complexités notées
quand elles comptent.

### `NkCameraTypes.h` — types de données

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Format | `NkCameraPixelFormatToString(f)` | `NkPixelFormat` → C-string ("RGBA8"…"MJPEG"/"UNKNOWN") `[O(1)]` |
| Orientation | `enum NkCameraFacing` | `ANY` / `FRONT` / `BACK` / `EXTERNAL` |
| Résolution | `enum NkCameraResolution` | Présets `CUSTOM`/`QVGA`/`VGA`/`HD`/`FHD`/`4K` |
| Résolution | `NkResolutionToSize(r, w, h)` | Écrit les dimensions d'un préset `[O(1)]` |
| Périphérique | `struct NkCameraDevice` | `index`, `id`, `name`, `facing`, `modes` ; `IsValid()`, `ToString()` |
| Périphérique | `NkCameraDevice::Mode` | `width`, `height`, `fps`, `format` (un mode supporté) |
| Config | `struct NkCameraConfig` | Paramètres d'ouverture ; `Resolve()` matérialise le préset `[O(1)]` |
| Frame | `struct NkCameraFrame` | `width/height/format/stride/timestampUs/frameIndex/data` |
| Frame | `NkCameraFrame::IsValid()` | Frame non vide ? `[O(1)]` |
| Frame | `NkCameraFrame::GetPixelRGBA(x, y)` | Pixel packé (RGBA8 seul, sinon 0) `[O(1)]` |
| Frame | `NkCameraFrame::DefaultStride(w, fmt)` | Stride par défaut selon le format `[O(1)]` |
| Photo | `struct NkPhotoCaptureResult` | `success`/`errorMsg`/`frame`/`savedPath` ; `operator bool` |
| Vidéo | `struct NkVideoRecordConfig` | `outputPath`, codecs, `bitrateBps`, `Mode` (AUTO/VIDEO_ONLY/IMAGE_SEQUENCE_ONLY) |
| État | `enum NkCameraState` | `CLOSED`/`OPENING`/`STREAMING`/`RECORDING`/`PAUSED`/`ERROR` |
| Callbacks | `NkFrameCallback` | `void(const NkCameraFrame&)` — à chaque frame |
| Callbacks | `NkCameraHotPlugCallback` | `void(const NkVector<NkCameraDevice>&)` — au (dé)branchement |
| IMU | `struct NkCameraOrientation` | `yaw/pitch/roll` (degrés) + `accelX/Y/Z` (m/s²) |

### `NKICameraBackend.h` — contrat backend

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init()`, `Shutdown()` | Démarrer / arrêter le backend (pur) |
| Énumération | `EnumerateDevices()`, `SetHotPlugCallback(cb)` | Lister les périphériques / écouter le hot-plug |
| Streaming | `StartStreaming(cfg)`, `StopStreaming()`, `GetState()` | Ouvrir/fermer/état du flux |
| Frames | `SetFrameCallback(cb)`, `GetLastFrame(out)` | Recevoir les frames |
| Photo | `CapturePhoto(out)`, `CapturePhotoToFile(path)` | Capturer une photo |
| Vidéo | `StartVideoRecord(cfg)`, `StopVideoRecord()`, `IsRecording()`, `GetRecordingDurationSeconds()` | Enregistrement |
| Infos | `GetWidth/Height/FPS/Format()`, `GetLastError()` | Paramètres de la session courante |
| Contrôles (défaut `false`) | `SetAutoFocus/Exposure/WhiteBalance`, `SetZoom`, `SetFlash`, `SetTorch`, `SetFocusPoint`, `GetOrientation` | Réglages optionnels selon matériel |

### `NkCamera2D.h` — caméra virtuelle

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Position | `SetPosition(x, y)`, `GetX()`, `GetY()` | Position 2D (inline) |
| Rotation | `SetRotation(deg)`, `GetRotation()` | Rotation en degrés (inline) |

### `NkCameraSystem.h` — façade et multi-caméra

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Singleton | `NkCameraSystem::Instance()`, `NkCamera()` | Accès unique (ctor privé, copie supprimée) |
| Cycle de vie | `Init()`, `Shutdown()`, `IsReady()` | Géré par `NkSystem::Initialise/Close` |
| Énumération | `EnumerateDevices()`, `SetHotPlugCallback(cb)` | Lister / écouter le hot-plug |
| Streaming | `StartStreaming(cfg={})`, `StopStreaming()`, `GetState()`, `IsStreaming()` | Ouvrir/fermer une caméra |
| Frames | `SetFrameCallback(cb)`, `GetLastFrame(out)`, `EnableFrameQueue(n=4)`, `DrainFrameQueue(out)` | Recevoir/défiler les frames (thread-safe) |
| Photo | `CapturePhoto(out)`, `CapturePhotoToFile(path="")` | Capture (fichier **désactivé**) |
| Vidéo | `StartVideoRecord(cfg={})`, `StopVideoRecord()`, `IsRecording()`, `GetRecordingDurationSeconds()` | Enregistrement |
| Contrôles | `SetAutoFocus/Exposure/WhiteBalance`, `SetZoom`, `SetFlash`, `SetTorch`, `SetFocusPoint` | Réglages physiques (`false` si non supporté) |
| Infos | `GetWidth/Height/FPS/Format()`, `GetLastError()`, `GetCurrentDeviceIndex()`, `GetBackend()` | Session courante |
| IMU / virtuelle | `SetVirtualCameraTarget(cam2D)`, `SetVirtualCameraMapping(b)`, `IsVirtualCameraMappingEnabled()`, `SetVirtualCameraMapConfig(cfg)`, `GetVirtualCameraMapConfig()`, `UpdateVirtualCamera(dt)`, `GetCurrentOrientation(out)` | Mapping caméra virtuelle ← IMU |
| Mapping (config) | `VirtualCameraMapConfig` | `yaw/pitchSensitivity`, `translationScale`, `invertX/Y`, `smoothing`, `smoothFactor` |
| Statiques | `ConvertToRGBA8(frame)`, `SaveFrameToFile(...)`, `GenerateAutoPath(prefix, ext)` | Conversion / sauvegarde (**désactivée**) / chemin auto |
| Multi-caméra | `NkMultiCamera::Open/Close/CloseAll/Get/Count` | Plusieurs caméras simultanées |
| Multi (flux) | `NkMultiCamera::Stream` | Un flux = une caméra (backend indépendant) |
| Alias | `NkCameraBackend` | Backend concret résolu par plateforme |

---

## Référence complète

Chaque élément repris en détail : les structures de données simples sont brèves, le système de
capture et le mapping IMU sont traités à fond, avec leurs usages dans les domaines du moteur.

### Les types de données — `NkCameraTypes.h`

**`NkCameraPixelFormatToString(f)`** convertit un `NkPixelFormat` en chaîne lisible (`"RGBA8"`,
`"BGRA8"`, `"RGB8"`, `"YUV420"`, `"NV12"`, `"YUYV"`, `"MJPEG"`, ou `"UNKNOWN"`). `O(1)`. Utile pour
le **logging** (savoir ce que le backend a réellement négocié) et les **panneaux d'outils**
(afficher le format de flux). Rappel : les valeurs `NkPixelFormat` appartiennent à NKWindow ;
NKCamera n'ajoute que ce convertisseur.

**`NkCameraFacing`** (`ANY`/`FRONT`/`BACK`/`EXTERNAL`) décrit l'orientation physique. Sur mobile, on
demande `FRONT` pour un selfie / suivi de visage, `BACK` pour scanner ou filmer la scène ; sur
desktop, `EXTERNAL` distingue une webcam USB de la caméra intégrée. C'est un **souhait** qu'on pose
dans `NkCameraConfig::facing` et qu'on retrouve dans `NkCameraDevice::facing`.

**`NkCameraResolution`** + **`NkResolutionToSize(r, w, h)`** forment le système de présets : du
`QVGA` (320×240) au `4K` (3840×2160). La fonction écrit les dimensions dans `w`/`h` (`CUSTOM` et
défaut → 640×480), `O(1)`. On les rencontre surtout indirectement, via `NkCameraConfig::Resolve()`.

**`NkCameraDevice`** décrit un périphérique énuméré : un `index` (celui qu'on passera en
`deviceIndex`), un `id` stable propre à l'OS (path V4L2, GUID Win32, uniqueID Apple), un `name`
lisible, son `facing`, et la liste de ses `modes` supportés. `IsValid()` (vrai si `id` non vide) et
`ToString()` (format `Camera[index] "name" facing=N modes=N`) aident au **debug** et au choix dans
une **UI de sélection de caméra**. Le sous-type **`Mode`** (`width`, `height`, `fps`, `format`)
énumère une combinaison réellement disponible — c'est en parcourant `modes` qu'on sait ce que le
matériel sait faire avant de demander une config.

**`NkCameraConfig`** rassemble tout ce qui décrit une ouverture. Détaillé plus haut ; le geste clé
est **`Resolve()`**, qui matérialise le préset et garantit des valeurs non nulles (`O(1)`). Les
champs `autoFocus`/`autoExposure`/`autoWhiteBalance`/`flipHorizontal` sont des souhaits initiaux.

**`NkCameraFrame`** est l'unité de travail :
- **Rendu / 2D** — `data` (un `NkVector<uint8>`) se téléverse tel quel dans une texture GPU via
  `data.Data()` ; `stride` indique l'alignement des lignes (ne pas supposer `width*4`).
- **Analyse / vision** — `GetPixelRGBA(x, y)` lit un pixel packé `(R<<24)|(G<<16)|(B<<8)|A` en
  `O(1)`, **seulement** en `NK_PIXEL_RGBA8` (sinon 0) ; convertir avant via `ConvertToRGBA8`.
- **Synchronisation** — `timestampUs` (microsecondes) et `frameIndex` permettent de cadencer, de
  mesurer le débit réel, ou d'aligner la caméra avec l'audio/l'IMU.
- `IsValid()` (dimensions > 0 et `data` non vide) garde la boucle de traitement.
- `DefaultStride(w, fmt)` calcule un stride par défaut (RGBA8/BGRA8 → `w*4`, RGB8 → `w*3`, défaut →
  `w*4`), utile pour allouer un tampon de conversion.

**`NkPhotoCaptureResult`** porte le résultat d'une photo : `success`, `errorMsg`, la `frame`
capturée, et `savedPath`. Son `operator bool` permet l'idiome `if (result) { … }`. C'est le retour
naturel de `CapturePhoto`.

**`NkVideoRecordConfig`** configure un enregistrement : `outputPath`, `bitrateBps` (4 Mbit/s par
défaut), `videoCodec`/`audioCodec`/`container` (h264/aac/mp4), `captureAudio`, et un **`Mode`**
(`AUTO` = vidéo native puis fallback ; `VIDEO_ONLY` = échec si la vidéo native manque ;
`IMAGE_SEQUENCE_ONLY` = force l'image par image). À noter : la **sortie fichier vidéo n'est pas
exposée publiquement** ici — le mode séquence d'images correspond à des membres internes du système.

**`NkCameraState`** (`CLOSED`→`OPENING`→`STREAMING`/`RECORDING`/`PAUSED`/`ERROR`) sert à piloter une
machine à états dans l'UI (bouton « démarrer » grisé tant qu'on n'est pas `STREAMING`, bandeau
rouge en `RECORDING`, message d'erreur en `ERROR` à coupler avec `GetLastError()`).

Les **callbacks** offrent un modèle *push* en complément du *pull* (`GetLastFrame`/`Drain`).
`NkFrameCallback` est invoqué à chaque frame — pratique pour un thread d'analyse dédié, mais
attention à ne pas y faire de travail lourd (on bloque le thread de capture). `NkCameraHotPlugCallback`
reçoit la **nouvelle liste de périphériques** au branchement/débranchement : exactement ce qu'il
faut pour rafraîchir un menu déroulant de sélection de caméra sans que l'utilisateur ait à relancer.

**`NkCameraOrientation`** (`yaw`/`pitch`/`roll` en degrés + `accelX/Y/Z` en m/s²) transporte les
données IMU vers le mapping de caméra virtuelle (voir plus bas). Pertinent en **XR/AR** (aligner une
scène 3D sur l'orientation réelle), en **gameplay mobile** (incliner pour viser), et en **outils**
(stabilisation, détection de secousse via l'accéléromètre).

### `NKICameraBackend` — l'interface backend

C'est le **contrat** (préfixe `NKI`, polymorphe, `virtual ~NKICameraBackend() = default`) que chaque
plateforme implémente (Win32/Linux/Cocoa/UIKit/Android/Emscripten/Noop — hors périmètre ici). On ne
l'utilise quasiment jamais directement : la façade `NkCameraSystem` le possède et délègue tout.
On peut tout de même y accéder via `GetBackend()` pour un besoin bas niveau.

Les **méthodes pures** couvrent le cycle complet — `Init`/`Shutdown`, `EnumerateDevices` +
`SetHotPlugCallback`, `StartStreaming`/`StopStreaming` + `GetState`, `SetFrameCallback` +
`GetLastFrame`, capture photo (`CapturePhoto`/`CapturePhotoToFile`), enregistrement
(`StartVideoRecord`/`StopVideoRecord`/`IsRecording`/`GetRecordingDurationSeconds`), et les accesseurs
de session (`GetWidth/Height/FPS/Format`, `GetLastError`). Tout ce que la façade expose se ramène à
ces appels.

Les **méthodes virtuelles à implémentation par défaut** retournent **`false`** (= non supporté) :
`SetAutoFocus`, `SetAutoExposure`, `SetAutoWhiteBalance`, `SetZoom`, `SetFlash`, `SetTorch`,
`SetFocusPoint`, et `GetOrientation` (IMU, mobile/XR). Ce choix de design est important : un backend
desktop n'a ni flash ni torche ni IMU, et il **ne plante pas** — il dit simplement « non ». D'où la
règle d'usage : **toujours tester le retour** de ces contrôles plutôt que de supposer qu'ils ont eu
lieu.

### `NkCamera2D` — la caméra virtuelle 2D

Caméra logique **minimale et inline** : elle ne porte que `mX`/`mY`/`mRotationDeg`, manipulés par
`SetPosition`/`SetRotation` et relus par `GetX`/`GetY`/`GetRotation`. Aucun ctor/dtor déclaré, aucun
rendu, aucun backend graphique. Son unique raison d'être est de **recevoir l'orientation IMU** via
`NkCameraSystem::UpdateVirtualCamera`.

- **Gameplay mobile** — incliner le téléphone fait défiler/pivoter la vue ; on lit ensuite `GetX()`
  et `GetRotation()` pour positionner le rendu.
- **AR / parallaxe** — un fond 2D qui réagit aux mouvements de l'appareil donne une illusion de
  profondeur sans 3D.
- **Outils / éditeur** — un viewport dont le cadrage suit un capteur, ou un mode démo « regarde
  autour de toi ».

L'ownership reste **externe** : c'est vous qui possédez le `NkCamera2D`, le système n'en garde qu'un
pointeur (durée de vie à garantir).

### `NkCameraSystem` — la façade singleton

**Singleton non-copiable.** `Instance()` (Meyers singleton) ou le raccourci `NkCamera()` — le ctor
est privé, la copie supprimée : on ne crée jamais d'instance soi-même. C'est cohérent avec le fait
qu'**une seule caméra physique** est gérée à la fois (multi via `NkMultiCamera`).

**Cycle de vie.** `Init()`/`Shutdown()` sont appelés par `NkSystem::Initialise`/`Close` ; `IsReady()`
(inline) dit si le sous-système est prêt. En pratique, l'application n'appelle pas ces deux-là
directement.

**Énumération et hot-plug.** `EnumerateDevices()` retourne la liste complète des
`NkCameraDevice` ; on choisit ensuite via `NkCameraConfig::deviceIndex` (= `devices[i].index`).
`SetHotPlugCallback(cb)` notifie les branchements/débranchements (rafraîchir une UI de sélection).

**Streaming.** Le cœur : `StartStreaming(cfg)` ouvre la caméra `cfg.deviceIndex`,
`StopStreaming()` la ferme, `GetState()`/`IsStreaming()` renseignent l'état. Pour **changer de
caméra**, il n'y a pas de bascule directe : `StopStreaming()` puis `StartStreaming(newCfg)` avec un
autre `deviceIndex`.
- **Frames** — deux stratégies déjà vues : `GetLastFrame(out)` (dernière image, thread-safe, on peut
  sauter des frames → aperçu) ou la file `EnableFrameQueue(n)` + `DrainFrameQueue(out)` (ne rien
  rater → analyse/enregistrement). `SetFrameCallback(cb)` ajoute un modèle *push*.

**Capture photo.** `CapturePhoto(out)` remplit un `NkPhotoCaptureResult` (frame en mémoire) — c'est
**la** voie recommandée. `CapturePhotoToFile(path)` existe pour compatibilité mais **l'écriture
fichier est désactivée** : ne pas s'y fier pour persister.

**Enregistrement vidéo.** `StartVideoRecord(cfg)`/`StopVideoRecord()`, avec `IsRecording()` et
`GetRecordingDurationSeconds()` pour l'UI (chronomètre, voyant rouge).

**Contrôles physiques.** `SetAutoFocus`/`SetAutoExposure`/`SetAutoWhiteBalance`, `SetZoom(level)`
(1.0 = aucun zoom), `SetFlash`/`SetTorch`, `SetFocusPoint(normX, normY)` (coordonnées normalisées,
ex. tap-to-focus). Tous renvoient `false` si le backend ne supporte pas — **tester le retour**.

**Informations session.** `GetWidth/Height/FPS/Format()` donnent les paramètres **réellement**
négociés (pas forcément ceux demandés), `GetLastError()` le dernier message, `GetCurrentDeviceIndex()`
(inline) la caméra ouverte, et `GetBackend()` (inline, const et non-const) l'accès bas niveau au
`NKICameraBackend`.

**Mapping caméra virtuelle ← IMU.** `SetVirtualCameraTarget(cam2D)` lie un `NkCamera2D*` **que vous
possédez** ; `SetVirtualCameraMapping(b)` active/désactive le mapping ;
`IsVirtualCameraMappingEnabled()` (inline) en donne l'état. À chaque frame,
`UpdateVirtualCamera(dt)` lit l'IMU et l'applique avec lissage. `GetCurrentOrientation(out)` expose
l'orientation brute si le backend la fournit. Le réglage passe par le sous-type
**`VirtualCameraMapConfig`** :
- `yawSensitivity`/`pitchSensitivity` — gain appliqué aux rotations.
- `translationScale` — 0 = rotation seule, > 0 = l'inclinaison déplace aussi la caméra.
- `invertX`/`invertY` — inverser un axe (préférence joueur).
- `smoothing` + `smoothFactor` (lerp ; 0.05 très lisse, 1.0 instantané) — amorti anti-saccade,
  matérialisé en interne par `mSmoothedYaw`/`mSmoothedPitch`.
  Réglé via `SetVirtualCameraMapConfig(cfg)` (inline), relu via `GetVirtualCameraMapConfig()` (inline).

**Utilitaires statiques.** `ConvertToRGBA8(frame)` convertit **en place** n'importe quel format de
frame en RGBA8 — l'étape indispensable avant `GetPixelRGBA` ou un téléversement GPU attendant du
RGBA. `SaveFrameToFile(...)` renvoie `false` (I/O **désactivée**, compatibilité). `GenerateAutoPath(prefix, ext)`
fabrique un chemin automatique (nommage daté), utile même quand on gère soi-même l'écriture ailleurs.

**Raccourci global.** `NkCamera()` (inline) équivaut à `NkCameraSystem::Instance()` — l'idiome
courant : `NkCamera().StartStreaming(cfg);`.

### `NkMultiCamera` — plusieurs caméras à la fois

Quand `NkCameraSystem` (une seule caméra) ne suffit pas — **stéréo**, **surveillance multi-flux**,
**comparaison avant/arrière simultanée** — `NkMultiCamera` gère plusieurs caméras en parallèle,
chacune avec son **backend indépendant**.

- `Open(deviceIndex, cfg)` ouvre une caméra **et démarre son streaming**, renvoyant la référence au
  `Stream` créé.
- `Close(deviceIndex)` ferme une caméra, `CloseAll()` toutes.
- `Get(deviceIndex)` récupère un flux (`nullptr` si absent), `Count()` (inline) le nombre ouvert.

Les `Stream` sont possédés par `unique_ptr` dans `mStreams` : **ne pas conserver une référence après
`Close`/`CloseAll`** (elle devient pendante).

**`NkMultiCamera::Stream`** = une caméra physique ouverte, avec son propre `NkCameraBackend`. Il
réplique l'essentiel de la façade pour son flux : `Start(cfg)`/`Stop()`, `GetLastFrame(out)`
(thread-safe), `EnableQueue(sz)` + `DrainFrame(out)`, `GetState()`, `DeviceIndex()` (inline),
`GetLastError()`, et `CapturePhotoToFile(path)`. Le constructeur prend le `deviceIndex` ; chaque
flux pousse/défile ses frames indépendamment des autres — exactement le découplage qu'il faut pour
traiter deux objectifs en parallèle.

### Le socle commun

- **`NkPixelFormat` vient de NKWindow.** NKCamera ne définit pas les formats de pixels, seulement le
  convertisseur `NkCameraPixelFormatToString`. Les valeurs MJPEG/NV12/YUYV/YUV420 existent côté
  NKWindow.
- **Pas de rendu, pas d'I/O fichier.** NKCamera produit des pixels en mémoire ; les API d'écriture
  (`CapturePhotoToFile`, `SaveFrameToFile`) sont des stubs de compatibilité (`false`). Pour
  persister, traitez la `NkCameraFrame` vous-même.
- **Thread-safety.** `GetLastFrame`/`DrainFrameQueue` (et leurs équivalents `Stream`) sont protégés
  par mutex internes. La queue est recommandée depuis la boucle principale pour ne pas rater de
  frames (taille par défaut 4).
- **Ownership clair.** Les `Stream` de `NkMultiCamera` sont en `unique_ptr` ; le `NkCamera2D` lié au
  système reste possédé par l'appelant.

---

### Exemple récapitulatif

```cpp
#include "NKCamera/NkCameraSystem.h"
using namespace nkentseu;

// 1. Choisir une caméra et configurer la session.
NkVector<NkCameraDevice> devices = NkCamera().EnumerateDevices();
NkCameraConfig cfg;
cfg.deviceIndex  = devices.IsEmpty() ? 0 : devices[0].index;
cfg.preset       = NK_CAM_RES_HD;          // 1280x720
cfg.outputFormat = NK_PIXEL_RGBA8;
cfg.Resolve();                             // matérialise width/height

// 2. Ouvrir le flux avec une queue, pour ne rater aucune frame.
NkCamera().EnableFrameQueue(4);
NkCamera().StartStreaming(cfg);

// 3. Boucle principale : défiler les frames, garantir le RGBA, téléverser.
NkCameraFrame frame;
while (NkCamera().DrainFrameQueue(frame)) {
    if (frame.format != NK_PIXEL_RGBA8)
        NkCameraSystem::ConvertToRGBA8(frame);
    texture.Upload(frame.data.Data(), frame.width, frame.height);
}

// 4. Prendre une photo en mémoire (l'écriture fichier est désactivée).
NkPhotoCaptureResult shot;
if (NkCamera().CapturePhoto(shot)) {
    process(shot.frame);                   // operator bool : if (shot) marche aussi
}

// 5. (Mobile/XR) Piloter une caméra virtuelle 2D par l'IMU.
NkCamera2D virtualCam;
NkCamera().SetVirtualCameraTarget(&virtualCam);   // ownership externe
NkCamera().SetVirtualCameraMapping(true);
NkCamera().UpdateVirtualCamera(dt);               // chaque frame

NkCamera().StopStreaming();

// Multi-caméra : deux objectifs en parallèle, backends indépendants.
NkMultiCamera multi;
auto& s0 = multi.Open(0, cfg);
auto& s1 = multi.Open(1, cfg);
NkCameraFrame f;
if (s0.GetLastFrame(f)) { /* ... */ }
multi.CloseAll();
```

---

[← Index NKCamera](README.md) · [Récap NKCamera](../NKCamera.md) · [Couche Runtime](../README.md)
