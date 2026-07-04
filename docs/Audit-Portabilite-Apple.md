# Audit de portabilité Apple (macOS / iOS / tvOS / watchOS)

> Audit systématique multi-agents des modules Nkentseu pour la compilation Apple
> **depuis Windows** (toolchain Zig 0.13 + `ld.lld -flavor darwin`, opt-in Jenga
> `--ios-backend=zig` / `--macos-backend=zig`). Voir aussi
> `Jenga/Docs/wiki/Compilation-Apple-depuis-Windows.md`.
>
> Date : 2026-07. Portée : `Kernel/System`, `Kernel/Foundation`, `Kernel/Runtime`.

## Cause racine (corrigée)

`NKENTSEU_PLATFORM_POSIX` / `_UNIX_LIKE` (`NKPlatform/NkPlatformDetect.h`) **n'incluait
pas** iOS/tvOS/watchOS/visionOS, alors que ce sont des dérivés Darwin/BSD **POSIX
complets** (sockets, pthread, `clock_gettime`, `statvfs`…). Tout module gaté sur
`NKENTSEU_PLATFORM_POSIX` tombait donc silencieusement dans un `#else` vide ou un
type non déclaré sur mobile Apple. **`NKENTSEU_PLATFORM_MACOS` reste, lui, strictement
macOS** (ne doit jamais matcher iOS).

✅ **Corrigé** : ajout de IOS/TVOS/WATCHOS/VISIONOS à la catégorie POSIX (purement
additif, aucun effet sur Windows/Linux/Android/macOS).

## Verdict par module

| Module | Verdict | Note |
|--------|---------|------|
| NKPlatform | ✅ corrigé | cause racine POSIX ci-dessus |
| NKThreading | ✅ corrigé | naming/priority thread étendus à iOS (voir plus bas) |
| NKTime | ✅ prêt | `clock_gettime`/`nanosleep`, `clock_nanosleep` bien gaté Linux |
| NKStream | ✅ prêt | POSIX `read`/`write` sur fd |
| NKSerialization | ✅ prêt | STL pur |
| NKReflection | ✅ prêt | RTTI + STL |
| NKFileSystem | 🟡 dégradé | compile ; **NkFileWatcher = no-op** sur Apple (voir reco) |
| NKNetwork | ✅ débloqué | cassé par la cause racine → compile après le fix POSIX (voir reco TLS/SIGPIPE) |
| NKImage / NKFont | ✅ prêt | codecs purs, aucune dépendance OS |
| NKAudio | ✅ corrigé | build iOS OK après remontée include AudioToolbox (voir plus bas) |
| NKCamera | ✅ corrigé | build iOS OK après fixes `.mm` (uint64/CStr/Sleep/iOS13) |
| NKCanvas | ✅ corrigé (A) | Metal-only iOS, gardes `displaySyncEnabled` etc. |
| NKRHI | 🟡 compile / latent | static lib iOS OK ; gaps Metal/GL/Vulkan = **link/runtime** (voir reco) |
| NKRenderer | ✅ corrigé | build iOS OK après gardes `system()` ; runtime dépend de NKRHI |

> **NKCode iOS** ne tire que NKCanvas/NKGui/NKFont/NKImage/NKWindow/NKEvent + core.
> NKRHI/NKRenderer/NKAudio/NKCamera **ne sont pas** dans son graphe → leurs gaps
> n'ont jamais été compilés sur iOS (faux positif de CI si on ne build que NKCode).

## Correctifs appliqués (vérifiés — rebuild NKCode iOS OK + signé)

1. **NKPlatform** `NkPlatformDetect.h` — POSIX inclut iOS/tvOS/watchOS/visionOS.
2. **NKThreading** `NkThread.cpp::SetName` — branche Darwin étendue à iOS/tvOS/watchOS
   (`pthread_setname_np(const char*)` nomme le thread courant).
3. **NKThreading** `NkThread.cpp::SetPriority` — API sched pthread étendue à iOS/tvOS/watchOS.
4. **NKRHI** `NkDeviceInitInfo.h` — forward-decl `struct UIView;` → `using UIView =
   struct objc_object;` (aligne sur le reste du moteur ; évite une redéclaration
   « kind différent » latente).

## Build réel des modules hors-graphe (iOS arm64) — bugs corrigés

L'audit statique classait NKAudio/NKCamera/NKRenderer « prêts ». Le **build Jenga
réel** (`jenga build --platform iOS --ios-backend=zig --target <M>`) a révélé des
erreurs de compilation que la lecture seule avait ratées. Tous **compilent
désormais** en static lib arm64 :

| Module | Bug révélé au build | Correctif |
|--------|---------------------|-----------|
| **NKRenderer** | `system()` *unavailable on iOS* (4 sites de debug-dump, dont 2 commandes Windows lancées inconditionnellement) — `NkShaderBackend.cpp`, `NkShaderLibrary.cpp` | gardés `#if !IOS/!TVOS/!WATCHOS` (no-op mobile) |
| **NKAudio** | `<AudioToolbox/AudioToolbox.h>` (ObjC) inclus **dans** `namespace nkentseu` → `@class NSError` hors scope global ; et .cpp compilé en C++ pur | include remonté au scope global + détection ObjC++ élargie (build-system) |
| **NKCamera** | `.mm` : `uint64` non visible hors-classe ; `NkString::c_str()` (→ `CStr()`) ; `NkChrono::Sleep(10)` ambigu ; `AVCaptureDeviceType…UltraWide/Triple` = **API iOS 13+** absentes du SDK 12.2 | `using nkentseu::uint64`, `CStr()`, `Sleep(10LL)`, types iOS 13+ guardés `__IPHONE_13_0` + `@available` |
| **NKRHI** | compile en static lib (les gaps Metal/GL/Vulkan restent **link/runtime**, cf. ci-dessus) | aucun (latent tant que non lié dans une app) |

> **Build-system** : la détection Objective-C++ du `IosZigBuilder`
> (`_NeedsObjectiveCppMode`) reconnaît maintenant les en-têtes parapluie de
> frameworks Apple (AudioToolbox, AVFoundation, CoreMedia, CoreVideo, GameController…),
> pas seulement UIKit/Foundation.

## Recommandations traitées (2e passe)

| Reco | Statut | Détail |
|------|--------|--------|
| **NKRHI** exclure OpenGL/Vulkan sur iOS | ✅ fait | `excludefiles` iOS + gardes `!IOS` dans `NkDeviceFactory` (include, `CreateForApi`, `IsApiSupported`, ordre de priorité). Build iOS vert, GL/VK absents. |
| **NKRHI** bugs Metal command-buffer | ✅ fait | `NkMetalCommandBuffer.mm` : macro `COMPUTE_ENC(` collée (function-like) + `UINT` (type Windows) → `uint32`. Compile (428 Ko). |
| **NKRHI** activer `NK_RHI_METAL_ENABLED` | ✅ fait | Backend Metal RHI **réintégré** : `NkMetalDevice.mm` (822 Ko) + `NkMetalCommandBuffer.mm` (428 Ko) compilent, Metal activé iOS+macOS. Voir « Réintégration NKRHI Metal » ci-dessous. |
| **NKNetwork** `SO_NOSIGPIPE` | ✅ fait | posé sur socket créé (`Bind`) + accepté (`Accept`), sous `__APPLE__`. NKNetwork compile iOS (débloqué par le fix POSIX racine). |
| **NKFileSystem** file-watcher Apple | ✅ fait | `NkFileWatcher` : implémentation **kqueue** (`EVFILT_VNODE`) + diff de snapshot du répertoire (macOS+iOS, thread pthread). Compile (360 Ko). |
| **NKNetwork** TLS/HTTPS iOS | ✅ fait | mbedTLS v3.6 LTS **vendoré** (`Externals/Libs/NKMbedTLS`, C pur) + `NKMbedTLS.jenga` (staticlib), cross-compile iOS (7.6 Mo). Wiring **opt-in** `NK_ENABLE_TLS=1` → dépendance + `NKENTSEU_HTTP_USE_MBEDTLS` + include. NkHTTPClient compile son chemin mbedTLS sur iOS. Sans le flag : HTTP simple, zéro dépendance. |

## Réintégration NKRHI Metal (device #1 — fait)

Le backend Metal RHI n'avait **jamais été compilé** et avait accumulé de la dette :

- **Couplage NKCanvas supprimé** : `NkMetalDevice.mm` n'importe plus
  `NkNativeContextAccess.h` (NKCanvas). La `CAMetalLayer` + le `MTLDevice` sont
  reçus via **`init.context.metal.{metalLayer, preferredDevice}`** (handles opaques
  `void*` ajoutés à `NkMetalDesc`). NKRHI reste ignorant de la couche fenêtrage.
- **Signature alignée** : `Initialize(NkIGraphicsContext*)` → `Initialize(const
  NkDeviceInitInfo&)` (interface `NkIDevice` réelle) ; membre fantôme `mCtx` retiré.
- **Zéro STL** (contrainte) : les 10 tables passent de `std::unordered_map` à
  **`NkUnorderedMap`** (API `Find/Erase/Insert/ForEach`, comme NkVulkanDevice) ;
  ~40 sites réécrits ; `std::max`/`push_back` remplacés par équivalents moteur.
- **Portabilité Metal iOS** : macro `COMPUTE_ENC` collée, `UINT`→`uint32`, formats
  desktop gardés `#if TARGET_OS_OSX` (BC*, Depth24/16, ClampToBorder), enums
  `MTLGPUFamilyApple1/6` (indispo SDK 12.2) remplacés par des gardes/valeurs sûres.
- **Handles opaques** : membres de ressources et getters en `void*` (bridge manuel
  `__bridge_retained`/`CFRelease`), typedefs `id<>` réservés à `mDevice`/`mQueue`.

Résultat : NKRHI iOS **vert, Metal activé**, OpenGL/Vulkan exclus. ⚠️ Compile mais
**non exécuté** (validation runtime = Mac/appareil requis) ; à tester aussi macOS natif.

## Dialogs natifs iOS (#3 — fait)

`NkDialogs_iOS.mm` (NKWindow) implémente en UIKit : `OpenMessageBox`
(UIAlertController) ; `OpenFileDialogAsync`/`OpenFolderDialogAsync`
(UIDocumentPickerViewController + delegate **auto-retenu** jusqu'au callback) ;
`SaveFileDialogAsync` (chemin Documents/sandbox). Une **API asynchrone**
(`NkDialogs::Callback = NkFunction<void(const NkDialogResult&)>`, **sans STL**) a été
ajoutée à `NkDialogs.h` — nécessaire car iOS présente ses pickers de façon async
(une API sync bloquerait la runloop → interblocage). Sur desktop, les variantes
async enveloppent l'implémentation sync existante. Les versions **sync** restent des
stubs sur iOS. `<string>`/`<vector>` retirés du header (no-STL).

## Couverture iOS complète + glue rendu Metal (#3/#4 — fait)

**#4 — audit exhaustif** : les 11 derniers modules non encore compilés iOS
(NKSPIRVCross, NKGLSlang, NKShaderc, NKSL, NKCollision, NKPhysics, NKECS, NKGui,
NKUI, NKEditorKit, Noge) **compilent tous sans correction**. → **L'intégralité du
moteur Nkentseu cross-compile pour iOS arm64 depuis Windows.**

**#3 — glue rendu Metal** : la render-path RHI est désormais câblable de bout en bout.
- La fenêtre UIKit expose déjà sa `CAMetalLayer` via `NkWindow::GetSurfaceDesc().metalLayer`.
- Nouveau raccourci `NkDeviceFactory::CreateMetalFromLayer(void* caMetalLayer, ...)`
  (NKRHI) : remplit `NkDeviceInitInfo.context.metal.metalLayer` et crée le device
  Metal — sans coupler NKRHI à NKWindow (handle opaque `void*`).

```cpp
// Wiring type (app / canvas) :
NkSurfaceDesc surf = window->GetSurfaceDesc();
NkIDevice* dev = NkDeviceFactory::CreateMetalFromLayer((void*)surf.metalLayer);
NkICommandBuffer* cb = dev->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
// dev->BeginFrame(...) / cb->... / dev->SubmitAndPresent(cb)  (runtime)
```

Il reste la **boucle de rendu concrète** (pipeline MSL, record des draws, present) —
code purement runtime, à écrire/valider sur appareil.

## Reste (décision / effort dédié)

- **CI** : cible smoke iOS compilant NKRHI/NKRenderer/NKAudio/NKCamera/NKNetwork
  (sinon ces gaps ne sont pas couverts par un build limité à NKCode).
- **Validation runtime Apple** : tout ce qui précède **compile** (iOS arm64) mais
  n'est pas exécuté ici — tester sur appareil/simulateur (et macOS natif pour Metal).
- **Dialogs iOS** : brancher les appelants desktop existants sur les variantes
  `*Async` s'ils doivent fonctionner aussi sur mobile.
