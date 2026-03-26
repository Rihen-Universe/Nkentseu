# Architecture du Framework Nkentseu

> Framework C++ multi-plateforme de gestion de fenêtres, d'événements et de rendu pixel.
> Namespace racine : `nkentseu`

---

## Table des matières

1. [Vue d'ensemble](#vue-densemble)
2. [Exigences fonctionnelles](#exigences-fonctionnelles)
3. [Exigences non-fonctionnelles](#exigences-non-fonctionnelles)
4. [Hiérarchie des modules](#hiérarchie-des-modules)
5. [Diagramme de dépendances](#diagramme-de-dépendances)
6. [Diagramme de classes — NKWindow](#diagramme-de-classes--nkwindow)
7. [Diagramme de classes — NKContainers](#diagramme-de-classes--nkcontainers)
8. [Diagramme de classes — NKMemory](#diagramme-de-classes--nkmemory)
9. [Diagramme de séquence — Cycle de vie fenêtre](#diagramme-de-séquence--cycle-de-vie-fenêtre)
10. [Diagramme de séquence — Traitement d'un événement](#diagramme-de-séquence--traitement-dun-événement)
11. [Diagramme d'états — NkWindow](#diagramme-détats--nkwindow)
12. [Diagramme d'états — NkEventSystem](#diagramme-détats--nkeventsystem)
13. [Diagramme de cas d'utilisation](#diagramme-de-cas-dutilisation)
14. [Plateformes supportées](#plateformes-supportées)
15. [Conventions de code](#conventions-de-code)

---

## Vue d'ensemble

Nkentseu est organisé en trois couches :

```
┌─────────────────────────────────────────────────────────┐
│                    Applications                         │
│                  (Sandbox, Jeux…)                       │
├──────────────────────────┬──────────────────────────────┤
│       Runtime            │       Runtime                │
│      NKWindow            │     NKRenderer               │
│      NKCamera            │                              │
├──────────────────────────┴──────────────────────────────┤
│       System                                            │
│  NKTime  NKStream  NKLogger  NKThreading               │
│  NKFileSystem  NKReflection  NKSerialization           │
├─────────────────────────────────────────────────────────┤
│       Foundation                                        │
│  NKPlatform  NKCore  NKMath  NKMemory  NKContainers   │
└─────────────────────────────────────────────────────────┘
```

---

## Exigences fonctionnelles

| ID   | Exigence                                                                 | Module         |
|------|--------------------------------------------------------------------------|----------------|
| F-01 | Créer et gérer des fenêtres natives multi-plateforme                     | NKWindow       |
| F-02 | Distribuer des événements clavier, souris, tactile, manette              | NKWindow       |
| F-03 | Chaque événement porte un identifiant de fenêtre (`NkWindowId`)         | NKWindow       |
| F-04 | Rendu pixel (Software Renderer, OpenGL)                                 | NKRenderer     |
| F-05 | Gestion mémoire custom (allocateurs, smart pointers)                    | NKMemory       |
| F-06 | Structures de données custom (NkVector, NkHashMap, NkString, …)         | NKContainers   |
| F-07 | Chronomètre haute-précision et gestion du temps                         | NKTime         |
| F-08 | Flux de données fichier/binaire                                         | NKStream       |
| F-09 | Journalisation asynchrone multi-sink                                    | NKLogger       |
| F-10 | Détection plateforme/arch/compilateur à la compilation                  | NKPlatform     |
| F-11 | Reflexion d'objets C++ à runtime                                        | NKReflection   |
| F-12 | Sérialisation JSON/binaire                                              | NKSerialization|
| F-13 | Système de fichiers abstrait                                            | NKFileSystem   |
| F-14 | Threading/synchronisation cross-plateforme                              | NKThreading    |
| F-15 | Caméra 3D (projection, vue, frustum)                                    | NKCamera       |

---

## Exigences non-fonctionnelles

| ID    | Exigence                                                                          |
|-------|-----------------------------------------------------------------------------------|
| NF-01 | **Zéro STL dans les interfaces publiques** — utiliser les types Nk               |
| NF-02 | **Zéro PIMPL** dans NkWindow/NkEventSystem — membres platform via `#ifdef`       |
| NF-03 | **Zéro allocation dynamique** dans le chemin critique (hot path)                 |
| NF-04 | C++17 minimum (C++20 pour NKPlatform/NKCore)                                     |
| NF-05 | Compilation sur Windows (clang-mingw), Linux (clang), macOS, Android, Web, Xbox  |
| NF-06 | Temps de compilation optimisé via PCH (Precompiled Headers)                      |
| NF-07 | Thread-safety sur `NkEventSystem::PollEvent()` (mutex autour de la queue)        |
| NF-08 | Pas de dépendances externes hormis les SDK OS (Win32, X11, Wayland, NDK, emsdk)  |
| NF-09 | Tests unitaires intégrés via le framework **Unitest**                             |
| NF-10 | Benchmarks comparatifs Nk vs STL dans chaque module conteneur                    |

---

## Hiérarchie des modules

```
Nkentseu/
├── Modules/
│   ├── Foundation/
│   │   ├── NKPlatform/   — Macros OS/arch/compilateur       (C++20)
│   │   ├── NKCore/       — Types, assertions, bits           (C++20)
│   │   ├── NKMath/       — Vec2, Vec3, Rect, types géom.    (C++17)
│   │   ├── NKMemory/     — Allocateurs, smart pointers       (C++17)
│   │   └── NKContainers/ — NkVector, NkHashMap, NkString…   (C++17)
│   ├── System/
│   │   ├── NKTime/       — Chrono haute-précision            (C++17)
│   │   ├── NKStream/     — Flux fichier/binaire              (C++17)
│   │   ├── NKLogger/     — Logs asynchrones                  (C++17)
│   │   ├── NKThreading/  — Threads, mutex, condition_var     (C++17)
│   │   ├── NKFileSystem/ — FS abstrait                       (C++17)
│   │   ├── NKReflection/ — Réflexion runtime                 (C++17)
│   │   └── NKSerialization/ — JSON/binaire                   (C++17)
│   └── Runtime/
│       ├── NKWindow/     — Fenêtrage + événements            (C++17)
│       ├── NKRenderer/   — Rendu Software/OpenGL             (C++17)
│       └── NKCamera/     — Caméra 3D                         (C++17)
└── Applications/
    └── Sandbox/          — Démo tous OS                      (C++17)
```

---

## Diagramme de dépendances

```
NKPlatform
    │
    ▼
NKCore ──────────────────────┐
    │                        │
    ├──────────┐             │
    ▼          ▼             ▼
NKMath     NKMemory      NKLogger
    │          │
    └────┬─────┘
         ▼
    NKContainers
         │
    ┌────┼────┬──────────┐
    ▼    ▼    ▼          ▼
NKTime NKStream NKThreading …(System)
    │
    ▼
NKWindow ──────────────────────────────────────────────
    │  ├── Platform/Win32/     (Windows desktop)
    │  ├── Platform/XLib/      (Linux X11/XLib)
    │  ├── Platform/XCB/       (Linux X11/XCB)
    │  ├── Platform/Wayland/   (Linux Wayland)
    │  ├── Platform/Emscripten/ (Web / Emscripten)
    │  ├── Platform/Android/   (Android NDK)
    │  ├── Platform/Cocoa/     (macOS)
    │  ├── Platform/UIKit/     (iOS)
    │  ├── Platform/UWP/       (Windows Runtime)
    │  ├── Platform/Xbox/      (Xbox Series/One)
    │  └── Platform/Noop/      (Headless / CI)
    ▼
NKRenderer
    │
    ▼
NKCamera
    │
    ▼
Sandbox
```

---

## Diagramme de classes — NKWindow

```
┌──────────────────────────────────────────────────┐
│                   NkWindow                       │
├──────────────────────────────────────────────────┤
│ - mId           : NkWindowId                     │
│ - mIsOpen       : bool                           │
│ - mConfig       : NkWindowConfig                 │
│ - mLastError    : NkError                        │
│ [Win32]                                          │
│ - mHwnd         : HWND                           │
│ - mHInstance    : HINSTANCE                      │
│ [XLib]                                           │
│ - mDisplay      : Display*                       │
│ - mXid          : ::Window                       │
│ [Android]                                        │
│ - mNativeWindow : ANativeWindow*                 │
│ [WASM]                                           │
│ - mCanvasId     : NkString                       │
├──────────────────────────────────────────────────┤
│ + Create(cfg) : bool                             │
│ + Close()                                        │
│ + IsOpen() : bool                                │
│ + GetId() : NkWindowId                           │
│ + SetTitle(title)                                │
│ + GetTitle() : NkString                          │
│ + GetSize() : NkVec2u                            │
│ + SetSize(w, h)                                  │
│ + SetFullscreen(on)                              │
│ + SetEventCallback(cb)                           │
└──────────────────────────────────────────────────┘
               │ possédée par
               ▼
┌──────────────────────────────────────────────────┐
│                  NkEventSystem                   │
├──────────────────────────────────────────────────┤
│ - mWindows      : NkHashMap<NkWindowId, NkWindow*>│
│ - mWindowCbs    : NkHashMap<NkWindowId, NkEventCallback>│
│ - mGlobalCb     : NkEventCallback                │
│ - mEventQueue   : NkDeque<NkUniquePtr<NkEvent>>  │
│ - mReady        : bool                           │
│ - mNextId       : NkAtomic<NkWindowId>           │
│ [Win32]                                          │
│ - mHwndToId     : NkHashMap<HWND, NkWindowId>    │
│ [XLib]                                           │
│ - mXidToId      : NkHashMap<::Window, NkWindowId>│
├──────────────────────────────────────────────────┤
│ + Init() : bool                                  │
│ + RegisterWindow(win) : NkWindowId               │
│ + UnregisterWindow(id)                           │
│ + PollEvent() : NkEvent*                         │
│ + PollEvents()                                   │
│ + SetWindowCallback(id, cb)                      │
│ + SetGlobalCallback(cb)                          │
│ - DispatchToCallbacks(evt)                       │
│ - PumpOS()  [platform-specific]                  │
│ - ProcessMessage(...)                            │
└──────────────────────────────────────────────────┘
               │ gère
               ▼
┌──────────────────────────────────────┐
│               NkEvent                │
├──────────────────────────────────────┤
│ # mWindowID  : NkWindowId            │
│ # mTimestamp : NkTimestampMs         │
│ # mHandled   : bool                  │
├──────────────────────────────────────┤
│ + GetType() : NkEventType::Value     │
│ + GetCategory() : NkEventCategory    │
│ + Clone() : NkEvent*                 │
│ + ToString() : NkString              │
│ + MarkHandled()                      │
│ + Is<T>() : bool                     │
│ + As<T>() : T*                       │
└───────────────┬──────────────────────┘
                │ héritage
    ┌───────────┼───────────────────────┐
    ▼           ▼                       ▼
NkKeyPressEvent  NkMouseMoveEvent  NkWindowCloseEvent
NkKeyReleaseEvent NkMouseButtonPressEvent NkWindowResizeEvent
NkKeyRepeatEvent  NkMouseWheelVerticalEvent …
```

---

## Diagramme de classes — NKContainers

```
namespace nkentseu::core

NkVector<T>         NkHashMap<K,V>      NkUnorderedMap<K,V>
NkDeque<T>          NkMap<K,V>          NkSet<T>
NkQueue<T>          NkStack<T>          NkArray<T,N>
NkString            NkStringView        NkFunction<Sig>
NkPriorityQueue<T>

NkString :
  - SSO_SIZE buffer intégré (stack)
  - Fallback heap si taille > SSO_SIZE
  + CStr() : const char*
  + Format(fmt, ...) : NkString   [static]
  + Append(str)
  + operator+(lhs, rhs)
```

---

## Diagramme de classes — NKMemory

```
namespace nkentseu::memory

NkIAllocator (interface)
    ├── NkDefaultAllocator    — malloc/free
    ├── NkArenaAllocator      — linear bump allocator
    ├── NkPoolAllocator       — fixed-size block pool
    └── NkStackAllocator      — LIFO stack

NkUniquePtr<T>    → remplace std::unique_ptr
NkSharedPtr<T>    → remplace std::shared_ptr
NkWeakPtr<T>      → remplace std::weak_ptr
NkScopePtr<T>     → RAII scope guard
```

---

## Diagramme de séquence — Cycle de vie fenêtre

```
Application          NkWindow          NkEventSystem
     │                  │                    │
     │   Create(cfg)    │                    │
     ├──────────────────►│                    │
     │                  │ RegisterWindow(this)│
     │                  ├────────────────────►│
     │                  │    id = nextId++    │
     │                  │◄────────────────────┤
     │                  │ CreateWindowNative()│
     │                  │  (Win32/XLib/…)    │
     │                  ├──────────────(OS)──►│
     │                  │                    │
     │   window.IsOpen()│                    │
     ├──────────────────►│                    │
     │   ◄── true ───── │                    │
     │                  │                    │
     │   while(IsOpen)  │                    │
     │   eventSys.PollEvent()                │
     │──────────────────────────────────────►│
     │                  │     PumpOS()        │
     │                  │◄────────────────────┤
     │                  │   OS messages       │
     │   ◄── NkEvent* ──────────────────────-│
     │                  │                    │
     │   window.Close() │                    │
     ├──────────────────►│                    │
     │                  │ UnregisterWindow(id)│
     │                  ├────────────────────►│
     │                  │ DestroyNative()     │
     │                  ├──────────────(OS)──►│
```

---

## Diagramme de séquence — Traitement d'un événement

```
OS (Win32/XLib)   WindowProc     NkEventSystem        Application
      │               │               │                    │
      │  WM_KEYDOWN   │               │                    │
      ├──────────────►│               │                    │
      │               │ winId=lookup  │                    │
      │               ├──────────────►│                    │
      │               │               │ NkKeyPressEvent evt│
      │               │               │ evt.mWindowID=winId│
      │               │               │                    │
      │               │               │ DispatchCallbacks  │
      │               │               ├───────────────────►│
      │               │               │   per-window cb    │
      │               │               ├───────────────────►│
      │               │               │   global cb        │
      │               │               │                    │
      │               │               │ mEventQueue.push() │
      │               │               │                    │
      │        PollEvent() call        │                    │
      │───────────────────────────────►│                    │
      │               │               │ pop + return       │
      │               │               ├───────────────────►│
```

---

## Diagramme d'états — NkWindow

```
         ┌──────────────┐
         │   Inexistante │
         └──────┬───────┘
                │ Create(cfg)
                ▼
         ┌──────────────┐
         │    Ouverte    │◄────────────────────────┐
         └──────┬───────┘                          │
       ┌────────┼────────────────┐                 │
       │        │                │                 │
       ▼        ▼                ▼                 │
  ┌─────────┐ ┌──────────┐ ┌──────────┐           │
  │Minimisée│ │Maximisée │ │Plein écran│           │
  └────┬────┘ └────┬─────┘ └────┬─────┘           │
       └───────────┼────────────┘                  │
                   │ Restore()                     │
                   └───────────────────────────────┘
                         │ Close() / WM_DESTROY
                         ▼
                   ┌──────────────┐
                   │   Fermée     │
                   └──────────────┘
```

---

## Diagramme d'états — NkEventSystem

```
     ┌───────────────┐
     │  Non-initialisé│
     └───────┬────────┘
             │ Init()
             ▼
     ┌───────────────┐
     │    Prêt        │◄─────────────────────────┐
     └───────┬────────┘                          │
             │                                   │
   ┌─────────┼──────────────┐                    │
   │         │              │                    │
   ▼         ▼              ▼                    │
┌──────┐ ┌───────┐  ┌───────────────┐           │
│ Idle │ │Pumping│  │ Dispatching   │           │
└──┬───┘ └───┬───┘  └───────┬───────┘           │
   │ PollEvent│             │ done              │
   ├──────────┘             └───────────────────┘
   │ Shutdown()
   ▼
┌──────────────┐
│  Arrêté      │
└──────────────┘
```

---

## Diagramme de cas d'utilisation

```
                         Système Nkentseu
      ┌──────────────────────────────────────────────────────┐
      │                                                      │
      │   ┌─────────────────────────────────────┐           │
      │   │          NKWindow                   │           │
      │   │  ┌────────────────────────────┐     │           │
      │   │  │  Créer fenêtre             │     │           │
      │   │  │  Fermer fenêtre            │     │           │
      │   │  │  Redimensionner            │     │           │
      │   │  │  Passer en plein écran     │     │           │
      │   │  │  Écouter les événements    │     │           │
      │   │  │  Gérer plusieurs fenêtres  │     │           │
      │   │  └────────────────────────────┘     │           │
      │   └─────────────────────────────────────┘           │
Application│                                                │
 (Jeu / │   ┌─────────────────────────────────────┐           │
  Éditeur)  │          NKRenderer                 │           │
      │   │  Blit Software Renderer            │           │
      │   │  Rendu OpenGL                      │           │
      │   └─────────────────────────────────────┘           │
      │                                                      │
      │   ┌─────────────────────────────────────┐           │
 Build │   │          Jenga Build System         │           │
 System│   │  build --platform linux/windows/… │           │
      │   │  build --config Debug/Release      │           │
      │   └─────────────────────────────────────┘           │
      └──────────────────────────────────────────────────────┘
```

---

## Plateformes supportées

| Plateforme    | Backend       | Toolchain           | Arch        | Statut    |
|---------------|---------------|---------------------|-------------|-----------|
| Windows       | Win32         | clang-mingw         | x86_64      | ✓ Actif   |
| Linux XLib    | X11/XLib      | clang/gcc           | x86_64      | ✓ Actif   |
| Linux XCB     | X11/XCB       | clang/gcc           | x86_64      | ✓ Actif   |
| Linux Wayland | Wayland+xdg   | clang/gcc           | x86_64      | ✓ Actif   |
| Linux Headless| Noop          | clang/gcc           | x86_64      | ✓ CI/WSL  |
| macOS         | Cocoa         | apple-clang         | arm64/x86_64| ✓ Actif   |
| Android       | NativeActivity| android-ndk         | arm64       | ✓ Actif   |
| iOS           | UIKit         | apple-clang         | arm64       | 🔄 Partiel|
| Web           | Emscripten    | emscripten          | wasm32      | ✓ Actif   |
| HarmonyOS     | ArkUI Native  | —                   | arm64       | 🔄 Partiel|
| Xbox Series   | UWP GameCore  | xbox-clang          | x86_64      | 🔄 Partiel|
| Xbox One      | UWP GameCore  | xbox-clang          | x86_64      | 🔄 Partiel|

---

## Conventions de code

### Nommage

| Élément        | Convention     | Exemple                    |
|----------------|----------------|----------------------------|
| Classes/Structs| `NkPascalCase` | `NkWindowConfig`           |
| Méthodes       | `PascalCase`   | `GetSize()`, `PushBack()`  |
| Membres privés | `mCamelCase`   | `mWindowID`, `mIsOpen`     |
| Constantes     | `NK_UPPER`     | `NK_INVALID_WINDOW_ID`     |
| Macros         | `NK_MACRO`     | `NK_EVENT_TYPE_FLAGS`      |
| Namespaces     | `lowercase`    | `nkentseu`, `core`, `memory`|

### Règles importantes

1. **Pas de STL dans les interfaces** — utiliser `NkVector` au lieu de `std::vector`, etc.
2. **Pas de PIMPL** — membres platform via `#ifdef` dans les headers
3. **Pas de `new`/`delete` raw** — utiliser `NkUniquePtr`, `NkSharedPtr`
4. **PCH obligatoire** — chaque projet déclare `pchheader`/`pchsource` dans son `.jenga`
5. **`ToString()` de debug** — utiliser `NkString::Format("%d", x)` pour les conversions numériques

### Ordre des includes

```cpp
// 1. PCH (implicite via compilateur)
// 2. En-tête du module courant
#include "NKWindow/Core/NkWindow.h"
// 3. En-têtes internes (même module)
#include "NKEvent/NkEventSystem.h"
// 4. En-têtes des dépendances (NKContainers, NKMath, …)
#include "NKContainers/String/NkString.h"
// 5. En-têtes platform (avec garde #ifdef)
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#  include <windows.h>
#endif
```
