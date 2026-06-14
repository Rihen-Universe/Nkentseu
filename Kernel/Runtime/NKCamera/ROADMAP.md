# NKCamera — Roadmap

État actuel (mai 2026) : module **livré** sur P1+P2. Capture caméra physique cross-platform avec backend par OS sélectionné via macros. Backends production sur Windows (Media Foundation), Linux (V4L2), Android (Camera2 NDK + MediaCodec), macOS (AVFoundation), iOS (AVFoundation + CMMotionManager), **Web (getUserMedia + Asyncify)**. Conversion formats unifiée RGBA8/BGRA8/RGB8/YUYV/NV12/**YUV420 I420**/**MJPEG** (via NkJPEGCodec). `SaveFrameToFile` réactivé via NKImage (PNG/JPG/BMP/TGA/QOI). Mode `IMAGE_SEQUENCE_ONLY` cross-platform implémenté dans `NkCameraSystem` (sauve chaque frame en PNG/JPG numérotée). Interface renommée `NKICameraBackend`. Manquent encore : contrôles manuels avancés (ISO, exposition manuelle, balance custom), HDR multi-frame, filtres temps réel, détection visages/QR.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Interface NKICameraBackend (contrat) | Livré | — | — |
| NkCameraTypes (device, config, frame, photo result, video config) | Livré | — | — |
| Pixel formats (RGBA8, BGRA8, RGB8, YUV420, NV12, YUYV, MJPEG) | Livré | — | — |
| Énumération devices + hot-plug callback | Livré | — | — |
| NkCameraSystem singleton + frame queue thread-safe | Livré | — | — |
| NkMultiCamera (plusieurs caméras simultanées) | Livré | — | — |
| Mapping caméra physique -> NkCamera2D virtuelle (IMU) | Livré | — | — |
| Smoothing yaw/pitch + sensitivity config | Livré | — | — |
| Backend Win32 (Media Foundation) | Livré | — | — |
| Backend Linux (V4L2) | Livré | — | — |
| Backend Android (Camera2 NDK + MediaCodec H.264) | Livré | — | — |
| Backend macOS (AVFoundation + AVAssetWriter) | Livré | — | — |
| Backend iOS (AVFoundation + CMMotionManager IMU) | Livré | — | — |
| Backend Emscripten (getUserMedia + canvas + ASYNCIFY) | Livré | — | — |
| Backend Noop (headless, tests) | Livré | — | — |
| Capture photo (CapturePhoto + CapturePhotoToFile) | Livré | — | — |
| Enregistrement vidéo (StartVideoRecord) | Livré | — | — |
| Mode IMAGE_SEQUENCE_ONLY (fallback PNG/JPG cross-platform) | Livré | — | — |
| Contrôles : AutoFocus, AutoExposure, AutoWhiteBalance | Livré | — | — |
| Contrôles : Zoom, Flash, Torch, FocusPoint | Livré (mobile) | — | — |
| Conversion frame -> RGBA8 (ConvertToRGBA8) | Livré | — | — |
| Conversion YUV420 I420 / NV12 / YUYV / MJPEG -> RGBA8 | Livré | — | — |
| Orientation IMU (mobile/XR) | Livré (iOS/Android/Linux IIO) | — | — |
| Save frame to file (PNG/JPG/BMP/TGA/QOI via NKImage) | Livré | — | — |
| Exposition manuelle / ISO / shutter speed | TODO | L | P2 |
| Balance des blancs custom (température, teinte) | TODO | M | P3 |
| HDR multi-frame (bracket exposure) | TODO | L | P3 |
| Filtres temps réel (denoise, sharpen, color grade) | TODO | L | P3 |
| Détection visages / QR / AR markers | TODO | L | P3 |
| Tests unitaires complets par backend | Partiel | M | P2 |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Tableau plateforme × backend

| Plateforme | Backend | Statut | API native | Vidéo recording | Photo | IMU | LOC |
|------------|---------|:------:|------------|:---:|:---:|:---:|----:|
| Windows desktop | NkWin32CameraBackend | Livré | Media Foundation (mf/mfplat/mfreadwrite) | Livré | Livré | non | 443 |
| Linux | NkLinuxCameraBackend | Livré | V4L2 (videodev2) | Livré (ffmpeg pipe ou RAW fallback) | Livré | Partiel (sysfs IIO) | 761 |
| Android | NkAndroidCameraBackend | Livré | Camera2 NDK + MediaCodec + MediaMuxer (JNI) | Livré | Livré | Livré (Android Sensor) | 836 |
| macOS | NkCocoaCameraBackend | Livré | AVFoundation + AVAssetWriter | Livré (H.264) | Livré | non | 99 |
| iOS | NkUIKitCameraBackend | Livré | AVFoundation + CMMotionManager | Livré | Livré | Livré | 80 |
| Emscripten / Web | NkEmscriptenCameraBackend | Livré | getUserMedia + canvas.getImageData + ASYNCIFY (setInterval pump) | IMAGE_SEQUENCE_ONLY uniquement | Livré (depuis frame courante) | non | ~310 |
| Autre / serveur / UWP / Xbox | NkNoopCameraBackend | Stub (toujours échec) | aucune | non | non | non | 39 |

Total backends : 7 ; tous compilent et fonctionnent sur leur cible. UWP et Xbox utilisent actuellement le Noop.

---

## Tableau formats de capture

| Format pixel | Lecture native | Conversion vers RGBA8 | Notes |
|--------------|:--:|:--:|-------|
| RGBA8        | Livré | identité | Output cible canonique |
| BGRA8        | Livré (macOS/iOS) | Livré | swap canaux |
| RGB8         | Livré | Livré | ajout alpha 255 |
| YUV420 (I420 planar) | Livré (Linux V4L2, Android) | Livré | Y plane + U plane + V plane, BT.601 |
| NV12         | Livré (Android, Windows MF) | Livré | semi-planar Y + UV entrelacé, BT.601 |
| YUYV / YUY2  | Livré (Linux V4L2, Win32 MF) | Livré | packed Y0 U Y1 V, BT.601 |
| MJPEG        | Livré (V4L2, MF) | Livré | décodage via NkJPEGCodec (NKImage) — sortie RGB24/GRAY8 puis padding alpha |
| NV21         | Possible côté Android | À côté de I420 | swap U/V avant appel à ConvertToRGBA8 |

---

## Livré

### Phase 1 — Types et contrat
- `NKICameraBackend` interface complète : cycle de vie, énumération, streaming, photo, vidéo, contrôles, infos session, orientation IMU — [NKICameraBackend.h](src/NKCamera/NKICameraBackend.h)
- `NkCameraDevice` avec modes (width × height × fps × format)
- `NkCameraConfig` avec presets (QVGA, VGA, HD, FHD, 4K) + auto-focus / exposure / white balance
- `NkCameraFrame` avec timestamp, stride, accès pixel RGBA helper
- `NkVideoRecordConfig` avec modes AUTO / VIDEO_ONLY / IMAGE_SEQUENCE_ONLY
- `NkCameraState` machine d'état (CLOSED / OPENING / STREAMING / RECORDING / PAUSED / ERROR)
- Callbacks : `NkFrameCallback`, `NkCameraHotPlugCallback`

### Phase 2 — Système central
- `NkCameraSystem` singleton avec sélection backend à la compilation — [NkCameraSystem.h](src/NKCamera/NkCameraSystem.h)
- `NkCameraSystem::EnumerateDevices` + hot-plug callback
- Streaming avec callback + queue thread-safe (`EnableFrameQueue`, `DrainFrameQueue`)
- `GetLastFrame` mutex-protégé
- `NkMultiCamera` pour ouvrir plusieurs caméras simultanément (un backend par stream)

### Phase 3 — Mapping caméra virtuelle (IMU -> NkCamera2D)
- `SetVirtualCameraTarget` + `SetVirtualCameraMapping`
- `VirtualCameraMapConfig` : yawSensitivity, pitchSensitivity, translationScale, invertX/Y, smoothing factor
- Référence d'orientation capturée à l'activation -> mouvements relatifs
- Lissage exponentiel `mSmoothedYaw / mSmoothedPitch` via lerp
- `UpdateVirtualCamera(dt)` à appeler chaque frame

### Phase 4 — Backends natifs production-ready
- **Windows / Media Foundation** : énumération `MFEnumDeviceSources`, sélection par index via `ActivateObject`, IMFSourceReader, threading dédié, formats négociés
- **Linux / V4L2** : scan `/dev/video*`, `VIDIOC_QUERYCAP`, mmap buffers, IMU via `/sys/bus/iio/devices/iio:device*`, vidéo via pipe ffmpeg (fallback RAW)
- **Android / Camera2 NDK** : `ACameraManager`, `ACameraCaptureSession`, `AImageReader` (RGBA_8888 / YUV_420_888), encodage H.264 via MediaCodec + MediaMuxer JNI, capteurs Android pour IMU
- **macOS / AVFoundation** : `AVCaptureSession` + `AVCaptureVideoDataOutput`, enregistrement H.264 via `AVAssetWriter`, delegate Objective-C dans `.mm`
- **iOS / AVFoundation + CMMotionManager** : pipeline AVCapture similaire macOS + IMU yaw/pitch/roll via Core Motion, photo capture + focus point + torch + zoom

### Phase 5 — Capture photo + enregistrement vidéo
- `CapturePhoto` : frame courante capturée vers `NkPhotoCaptureResult`
- `CapturePhotoToFile` (mobile / desktop natif)
- `StartVideoRecord` / `StopVideoRecord` avec config codec (h264 / aac / mp4 par défaut)
- Mode AUTO choisit la meilleure voie native disponible
- `GetRecordingDurationSeconds`

### Phase 6 — Contrôles caméra physique
- AutoFocus, AutoExposure, AutoWhiteBalance (toggle)
- Zoom (level multiplicatif)
- Flash + Torch (mobile)
- FocusPoint normalisé (tap-to-focus mobile)
- `GetOrientation` retourne yaw/pitch/roll + accel x/y/z

---

## En cours / TODO immédiat

### Conversion YUV haute performance
- Chemin SIMD (SSE2 / NEON / WASM SIMD) pour YUYV / NV12 / YUV420 — actuellement scalaire 100% portable
- Support BT.709 (HD) explicite — actuellement BT.601 par défaut (correct SD mais légèrement off pour HD)
- Path de conversion intégrée NV21 (variante Android où U et V sont inversés par rapport à I420)

### Démos
- 3 démos livrées sous `Applications/NkCameraDemos/` : Viewer (1 caméra plein écran), Multi (2x2 split-screen via NkMultiCamera), FormatTest (génération + conversion YUYV/NV12/YUV420/MJPEG cross-format)
- Validation runtime restante : tester chaque démo sur Win32 / Linux V4L2 / Android Camera2 / Emscripten getUserMedia

### Documentation
- Migration nommage interface : `INkCameraBackend` -> `NKICameraBackend` (convention `NKI` pour interfaces moteur). Le doc historique `docs/NKENTSEU_PRODUCTION_ANALYSIS_2026.md` n'a pas été mis à jour (snapshot temporel).

---

## À venir / À ajouter (futur proche)

### Contrôles manuels avancés
- Exposition manuelle (shutter speed)
- ISO / gain manuel
- Balance des blancs custom (température Kelvin + teinte)
- Plage focus manuel (loin / proche)
- HDR multi-frame (bracket exposure puis fusion)

### Pipeline post-traitement
- Filtres temps réel (denoise NLM, sharpen unsharp mask, vignette)
- Color grading (LUT 3D 17×17×17)
- Stabilisation logicielle (optical flow basique)

### Audio sync
- Capture micro synchronisée pour l'enregistrement vidéo (actuellement `captureAudio` dans config mais pas implémenté partout)
- Lien avec NKAudio pour capture micro unifiée

### Détection / analyse
- Détection de visages (Haar cascades ou modèle léger)
- Lecture QR code / codes-barres
- Détection AR markers (ArUco)
- Background segmentation pour effet "green screen"

### Backends additionnels
- Backend libuvc générique cross-platform (USB Video Class direct)
- Backend OpenCV pour prototype rapide (optionnel)
- Backend NDI / Spout pour capture flux réseau

### Quirks restants
- Le mapping IMU -> caméra 2D n'utilise pas la translation (translationScale=0 par défaut) — la rotation seule fonctionne
- `SaveFrameToFile` détecte le format depuis l'extension via `NkImage::Save` ; les formats reconnus sont PNG, JPG, BMP, TGA, PPM, HDR, QOI, GIF, WebP, SVG. Une extension inconnue échoue silencieusement (retour `false`).

---

## Bugs / quirks connus
- UWP et Xbox tombent sur Noop (pas de backend dédié)
- Emscripten : nécessite HTTPS (ou localhost) pour `getUserMedia` ; le pump est via `setInterval` JS, donc le FPS effectif dépend du throttling navigateur (cap ~60 Hz, baisse en arrière-plan)
- macOS backend déclare `NK_PIXEL_BGRA8` en sortie (cohérent avec CoreVideo) — `ConvertToRGBA8` swap les canaux côté CPU avant upload GPU
- Android requiert `android.permission.CAMERA` + `SetJNIEnv` avant `Init()`
- Linux IMU dépend de la présence d'un device IIO accéléromètre (laptops convertibles surtout) — pas garanti
- Le backend Web ne supporte pas l'enregistrement vidéo natif : utiliser `NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY` (chaque frame sauvée individuellement)

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types, atomics, plateforme), NKContainers (NkVector, NkString), NKTime (NkChrono), NKLogger, NKMath, NKWindow (NkTypes / NkPixelFormat), NKPlatform (détection OS), **NKImage** (codecs JPEG/PNG pour conversion MJPEG et SaveFrameToFile)
- **Couches OS** : Media Foundation, V4L2, Camera2 NDK, AVFoundation, CMMotionManager, getUserMedia + Asyncify
- **Modules au-dessus qui en dépendent** : runtime XR / AR (mapping IMU caméra), `Applications/NkCameraDemos` (3 démos référence), application MR Nkentseu
