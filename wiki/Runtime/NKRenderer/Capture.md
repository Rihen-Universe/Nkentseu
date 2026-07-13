# NKRenderer — Capture & enregistrement vidéo

> Capturer le rendu final en **image** (PNG) ou en **vidéo** (MP4/H.264 via NKMedia),
> sur tous les backends, **sans bloquer le rendu** — la fenêtre reste vivante pendant
> l'enregistrement. Sert aux cinématiques, tutoriels, validation visuelle headless
> (agents/CI) et à tout export d'images du moteur.

## Vue d'ensemble

```
rendu NkRenderer ──SetFinalColorTargetMirror──▶ NkOffscreenTarget (cible RGBA8)
                                                   │                    │
                                     passe MirrorPresent          NkFrameCapture
                                     (blit plein-écran,           (ring staging +
                                      la fenêtre reste             fences, async)
                                      vivante, ~1 draw)                 │
                                                   ▼                    ▼
                                               swapchain      Poll() → pixels RGBA8
                                               (écran)         → NkVideoRecorder
                                                                 (NKMedia, H.264,
                                                                  thread d'encodage)
```

Trois briques, toutes dans `Tools/Offscreen/` :

| Brique | Rôle |
|---|---|
| `NkOffscreenTarget` | cible de rendu échantillonnable + readback synchrone (`Capture(path)` → PNG) |
| `NkFrameCapture` | capture **asynchrone** : ring de staging buffers + fences, `EnqueueCopy`/`Poll` non bloquants — zéro `WaitIdle` en régime |
| passe `MirrorPresent` | recopie la cible redirigée vers le swapchain (`SetFinalColorTargetMirror(target, true)`) — voir **et** enregistrer |

## Capture d'une image (PNG)

```cpp
renderer::NkOffscreenDesc od;
od.width = w; od.height = h;
od.colorFmt = NkGPUFormat::NK_RGBA8_UNORM;
od.readback = true;
target.Init(device, renderer->GetTextures(), od);
renderer->SetFinalColorTarget(texLib->GetRHIHandle(target.GetColorResult()));
// ... rendre quelques frames ...
device->WaitIdle();
target.Capture("screenshot.png"); // flip Y automatique sur OpenGL
renderer->SetFinalColorTarget(NkTextureHandle{}); // restaure le swapchain
```

Dans **renderdemo** : `NK_CAPTURE=<frame>` (+ `NK_CAPTURE_PATH`) fait tout ça en one-shot.

## Enregistrement vidéo (asynchrone, fenêtre vivante)

```cpp
// 1. Cible + capture async + encodeur
target.Init(device, texLib, od);              // od.readback = false
capture.Init(device, {w, h, /*ring*/3});
recorder.Begin("out.mp4", w, h, /*fps*/10);   // NKMedia, thread d'encodage dédié

// 2. Rediriger la sortie EN GARDANT la fenêtre vivante
renderer->SetFinalColorTargetMirror(texLib->GetRHIHandle(target.GetColorResult()), true);

// 3. Chaque frame (cadence au choix) — AUCUN appel ne bloque :
capture.EnqueueCopy(texLib->GetRHIHandle(target.GetColorResult()), frameIdx);
while (capture.Poll([&](const uint8 *rgba, uint32 w, uint32 h, uint64 tag) {
    recorder.PushVideo(rgba, media::NkVideoInputFormat::RGBA32);
})) {}

// 4. Fin : drainer, restaurer l'affichage, puis finaliser SUR UN AUTRE THREAD.
//    recorder->End() draine toute la file d'encodage (potentiellement des
//    secondes) : sur le thread de rendu ça FIGE l'application. Pattern :
device->WaitIdle();
while (capture.PendingCount() > 0 && capture.Poll(...)) {}
renderer->SetFinalColorTarget(NkTextureHandle{}); // la fenêtre revit tout de suite
media::NkVideoRecorder *toEnd = recorder; recorder = nullptr;
finalizeThread.Start([toEnd](void *) {
    toEnd->End(); // MP4 finalisé en fond, toujours lisible
    memory::NkGetDefaultAllocator().Delete(toEnd);
});
```

`Poll` peut aussi être appelé depuis un **thread consommateur dédié** (un seul thread
à la fois). Si le ring est plein, `EnqueueCopy` **saute la frame** au lieu d'attendre :
le rendu n'est jamais ralenti par la capture.

### Dans renderdemo

| Contrôle | Effet |
|---|---|
| `NK_RECORD=out.mp4` | enregistre dès le lancement |
| **touche INSER** (Insert) | démarre/arrête à chaud — les touches F1-F12 sont réservées aux démos (noms auto `nk_record_NNN.mp4`) |
| `NK_RECORD_FPS=n` | cadence d'échantillonnage (défaut **10**, voir plafond) |
| `NK_RECORD_RECT=x,y,w,h` | n'enregistre que cette **zone** (crop, w/h alignés à 2) |
| `NK_RECORD_W=..` + `NK_RECORD_H=..` | **résolution d'export indépendante** de la fenêtre (ex. 4K natif pendant affichage 720p) — les deux requis, alignés à 2 |
| `NK_RECORD_QP=<10..40>` | **qualité H.264** : plus bas = moins de blocs de compression, fichier plus gros (défaut 24 ; 16-18 = très propre) |
| `NK_RECORD_CODEC=mjpeg` | encode en **MJPEG** (intra pur : zéro macroblocking inter-trame, cadences hautes OK, vidéo seule) au lieu du H.264 |
| `NK_RECORD_MJPEG_Q=<1..100>` | qualité MJPEG (défaut 90) |
| `NK_CAPTURE=<frame>` | PNG one-shot à la frame donnée |

L'arrêt (INSER ou fin de session) restaure l'affichage **immédiatement** et finalise
le MP4 sur un **thread dédié** — aucun freeze. Les stats NKMedia
(`QueueDepth`/`DroppedFrames`/`EncodeFps`) sont loguées à l'arrêt, et la demo
**auto-régule** : si la file d'encodage approche son cap, l'échantillon est sauté
en amont (économise même le readback GPU).

## Multi-backend, qualité, limites

- **Backends** : le mécanisme (offscreen + `CopyTextureToBuffer` + fences) existe sur
  les 6 backends. **Validé mesures à l'appui : OpenGL et DX11** (captures identiques) ;
  Vulkan/DX12 : mêmes chemins, à confirmer visuellement. Le flip Y écran/readback est
  géré par backend.
- **Zone et qualité** : le crop découpe des pixels **existants** — la qualité d'une zone
  = la résolution du rendu source. Pour du 4K/2K natif : `NK_RECORD_W/H` (V3) rend toute
  la scène à la résolution d'export via `NkRenderer::SetRenderSizeOverride(w, h)` — le
  swapchain de la fenêtre n'est pas touché, la passe MirrorPresent fait le pont.
- **Plafond encodeur (mesuré 2026-07-12)** : le H.264 CPU de NKMedia soutient
  **~10 fps en 720p**. La file d'encodage est désormais **bornée** côté NKMedia
  (`maxQueuedFrames`, drop-newest) avec stats `QueueDepth()/DroppedFrames()/EncodeFps()`
  — plus de saturation mémoire possible ; à cadence trop haute des trames sont droppées
  (comptées). Pour du 30-60 fps : `NK_RECORD_CODEC=mjpeg` (encodage intra pur,
  PSNR 41-57 dB mesuré côté NKMedia, aucun scintillement de blocs inter-trame).
- Le resize de la fenêtre pendant un enregistrement **arrête proprement** la vidéo
  (fichier finalisé lisible).

## Fichiers

- `Tools/Offscreen/NkOffscreenTarget.{h,cpp}` — cible + readback + `Capture(png)`
- `Tools/Offscreen/NkFrameCapture.{h,cpp}` — capture asynchrone (ring + fences)
- `Resources/NKRenderer/Shaders/Blit/NkSL/blit.{vert,frag}.nksl` — shader MirrorPresent
- `Applications/Sandbox/src/Demo/main.cpp` — intégration complète de référence (NK_RECORD)
