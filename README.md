# Nkentseu Engine Framework

**Nkentseu** est un framework C++ **modulaire, haute performance, cross-platform et
zero-STL** (conteneurs et algorithmes maison), conçu pour les jeux vidéo, la
simulation, la VR/AR/MR, les outils CAO et les applications scientifiques.

- **Langage** : C++17/20, sans dépendance à la STL (allocateurs et conteneurs maison).
- **Plateformes** : Windows · Linux (XLib / XCB / Wayland) · macOS · Android · iOS ·
  Web (Emscripten) · HarmonyOS.
- **Build** : système maison **Jenga** (descripteurs `*.jenga` en Python).
- **Développeur** : Rihen — solo. Langue de travail : français.

> ⚠️ **Statut : en développement actif.** Ce README reflète l'état **réel** du dépôt
> (et non un objectif). Les marqueurs de statut : ✅ livré / 🔶 partiel ou en cours /
> ⏳ planifié, non démarré.

---

## Comprendre le projet en 2 minutes

- **[EXPLICATION_SIMPLE.md](EXPLICATION_SIMPLE.md)** — version grand public, sans jargon.
- **[PRESENTATION_TECHNIQUE.md](PRESENTATION_TECHNIQUE.md)** — architecture en couches,
  modules, les deux chemins de rendu (2D NKCanvas / 3D NKRenderer→NKRHI).
- **[ARCHITECTURE.md](ARCHITECTURE.md)** — document d'architecture de référence.
- **[ECOSYSTEM.md](ECOSYSTEM.md)** — la famille de produits (moteur, éditeurs, apps).
- **[Guides/](Guides/)** — tutoriels pas-à-pas style SFML pour **utiliser** le moteur
  (NKWindow, NKEvent, NKMemory, NKImage, NKCanvas, NKAudio, NKUI, NKNetwork + projet 2D complet).

En une phrase : **Jenga** (build) construit **Nkentseu** (moteur C++ zero-STL) ; **Noge**
en est la couche framework de jeu ; **Nogee** est l'éditeur (planifié) bâti dessus.

---

## Architecture en couches

Chaque couche n'utilise que celles en dessous.

```
Applications (Mou, Pong, Songoo, Nkoung, Sandbox, demos, …)
        ▲
Engine — Noge (framework Application : boucle, LayerStack, EventBus, ECS gameplay)
        ▲
Runtime — NKWindow · NKEvent · NKCanvas · NKRHI · NKImage · NKAudio · NKCamera ·
          NKFont · NKECS · NKUI · NKCollision · NKRenderer
        ▲
System — NKLogger · NKThreading · NKTime · NKStream · NKFileSystem · NKNetwork ·
         NKReflection · NKSerialization
        ▲
Foundation — NKCore · NKMath · NKMemory · NKContainers · NKPlatform
        ▲
OS / Matériel (Windows · Linux · macOS · Android · iOS · Web · HarmonyOS)
```

### Arborescence réelle du dépôt

```
Nkentseu/
├── Kernel/
│   ├── Foundation/   NKCore · NKMath · NKMemory · NKContainers · NKPlatform
│   ├── System/       NKLogger · NKThreading · NKTime · NKStream · NKFileSystem ·
│   │                 NKNetwork · NKReflection · NKSerialization
│   ├── Runtime/      NKWindow · NKEvent · NKCanvas · NKRHI · NKImage · NKAudio ·
│   │                 NKCamera · NKFont · NKECS · NKUI · NKCollision · NKRenderer
│   ├── AI/           (scaffold — sous-système ML/DL/RL à venir)
│   └── Bare/         (scaffold — couche OS bare-metal, différé)
├── Engine/Noge/      Framework Application (moteur de jeu sur NKRenderer)
├── Applications/     Mou · Pong · Songoo · Nkoung · Sandbox · NKCode · Nogee · PV3DE ·
│                     NkCameraDemos · NkImageDemo · NkAudioDemo · …
├── Externals/        Dépendances vendored (ex. NKGlad)
├── Spark/            NkSpark — langage embarqué maison (scaffold)
├── Guides/           Tutoriels d'usage (style SFML)
├── Nkentseu.jenga    Workspace de build racine
└── gitcommit.sh · gitpush.sh · gitpr.sh   Scripts git du dépôt
```

> Chaque module possède sa propre `ROADMAP.md` (tableau de synthèse + sections
> Livré / En cours / À venir / Bugs / Dépendances).

---

## État des modules

### Foundation

| Module | Rôle | Statut |
|--------|------|:--:|
| **NKCore** | Types primitifs, macros, asserts, traits zero-STL | ✅ |
| **NKMath** | Vec/Mat/Quat/Color, fonctions math (SIMD à finaliser) | ✅ |
| **NKMemory** | Allocateurs (Arena/Linear/Stack/Buddy/FreeList…), smart pointers | ✅ |
| **NKContainers** | ~40 conteneurs maison (Vector, Map, Set, String…) | ✅ |
| **NKPlatform** | Détection OS / architecture / CPU | ✅ |

### System

| Module | Rôle | Statut |
|--------|------|:--:|
| **NKLogger** | Logging (sinks multiples, async) | ✅ |
| **NKThreading** | Threads, mutex, atomics, ThreadPool | ✅ |
| **NKTime** | Chrono, clock, date | ✅ |
| **NKStream** | Flux binaire / texte, interface `NKIResource` | ✅ |
| **NKFileSystem** | Path, fichiers, watcher | ✅ |
| **NKSerialization** | JSON / XML / YAML / binaire / NkNative | ✅ |
| **NKReflection** | Réflexion minimaliste | 🔶 |
| **NKNetwork** | Sockets / transport (couche Replication ECS non implémentée) | 🔶 |

### Runtime

| Module | Rôle | Statut |
|--------|------|:--:|
| **NKWindow** | Fenêtres natives (Win32 · XLib · XCB · Wayland · Cocoa · UIKit · Android · Emscripten · HarmonyOS) + DPI / hot-plug écrans | ✅ |
| **NKEvent** | Événements typés, clavier/souris/tactile/manette/focus | ✅ |
| **NKImage** | 12 codecs from-scratch (PNG, JPEG, BMP, TGA, QOI, HDR, EXR, SVG…) | ✅ |
| **NKAudio** | Moteur audio AAA STL-free (256 voix, HRTF, buses, MP3, OGG) | ✅ |
| **NKCamera** | Capture caméra multi-OS, conversion de formats | ✅ |
| **NKFont** | Parsing TTF / OTF, atlas (SDF à venir) | ✅ |
| **NKRHI** | RHI bas niveau 6 backends — validé bout-en-bout sur **5** (Vulkan · OpenGL · DX11 · DX12 · Software) ; compute & cross-compile de shaders | ✅ |
| **NKCanvas** | Couche 2D SFML-like (sprites, formes, texte, transformations, render textures) — rend sur 5 backends (GL · DX11 · DX12 · SW · Vulkan) ; Metal en stub | 🔶 |
| **NKRenderer** | Rendu 3D ~80 % d'un MVP UE5-like (PBR, IBL HDR, CSM/Virtual Shadow Maps, Planar Reflection, Bloom, ACES, Voxel AO) ; Vulkan + OpenGL validés, DX11/DX12 en cours | 🔶 |
| **NKECS** | ECS bas niveau à archétypes | 🔶 |
| **NKUI** | UI immediate-mode (docking, thèmes, widgets) | 🔶 |
| **NKCollision** | Collisions / physique | ⏳ |

### Engine

| Module | Rôle | Statut |
|--------|------|:--:|
| **Noge** | Framework Application (boucle, LayerStack, EventBus, ECS gameplay) | 🔶 |

### Applications & démos

| App | Description | Statut |
|-----|-------------|:--:|
| **Mou** | Plateforme de 6 jeux éducatifs maternelle (3–5 ans), sur NKCanvas | ✅ jouable (Windows + Android) |
| **Pong** | Jeu de référence multi-backend sur NKCanvas | ✅ |
| **Songoo** | Jeu 2D (migration NKCanvas + build Android) | 🔶 |
| **Nkoung** | Plateforme de mini-jeux 2D | 🔶 |
| **Sandbox** | Bac à sable de démos (NKRHI, NKRenderer, NKUI…) | 🔶 |
| **NkCameraDemos / NkImageDemo / NkAudioDemo** | Démos de modules | ✅ |
| **NKCode** | IDE/éditeur de code sur Nkentseu+Jenga | ⏳ scaffold |
| **Nogee** | Éditeur de jeu | ⏳ coquille |
| **PV3DE** | Patient Virtuel 3D Émotif (médical) | ⏳ doc |

---

## Compiler avec Jenga

Le build utilise **Jenga** (descripteurs `*.jenga` en Python). Workspace racine :
[Nkentseu.jenga](Nkentseu.jenga).

```bash
jenga build                                   # hôte par défaut, config Debug
jenga build --target <Projet>                 # un projet précis (ex. Mou, Pong, NKRHI)
jenga build --config Release|Debug            # configuration
jenga build --platform windows|linux|macos|android|ios|web|harmonyos
jenga build --platform linux --linux-backend xlib|xcb|wayland   # backend fenêtrage Linux
```

Exemples :

```bash
jenga build --target Mou --config Debug                          # le jeu Mú (hôte)
jenga build --target Mou --config Release --platform android     # APK Android
jenga build --target NKRHI --config Debug --platform web         # module RHI en WebAssembly
```

> **Toolchain Windows** : `clang-mingw` (msys64/ucrt64).
> **Android** : nécessite `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, `JAVA_HOME` (JDK 17)
> et un `debug.keystore` pour produire un APK installable.

---

## Conventions de code

- **Mémoire** : **jamais** de `new`/`delete`/`malloc`/`free` bruts → toujours **NKMemory**
  (`nkentseu::memory::NkGetDefaultAllocator().New<T>()/.Delete()`, `NkAlloc`/`NkFree`).
  Mélanger allocateur custom et heap CRT provoque une corruption de tas. Toute classe
  avec `Create` doit avoir `Destroy`.
- **Nommage** : types/classes `NkPascalCase` · interfaces préfixe `NKI` · méthodes
  `PascalCase` · membres privés `mCamelCase` · macros `NKENTSEU_UPPER_SNAKE` · types
  primitifs `nk_uint32`, `nk_float32`…
- **Zero-STL** : conteneurs et algorithmes maison (NKContainers, NKCore).
- **Sérialisation** : JSON (`.nkproj`, `.nkscene`, `.nkcase`), binaire `.nkb`.

---

## Contribuer (git)

Le dépôt fournit des scripts qui garantissent des commits/PR propres :

```bash
./gitcommit.sh "<message>" [chemins...]    # commit (stage ciblé si chemins fournis)
./gitpush.sh                               # push de la branche courante
./gitpr.sh "<titre>" "<corps>" [base]      # crée une PR via gh (base = main par défaut)
```

---

## Licence

Nkentseu est distribué sous licence **MIT**. Voir [LICENSE](LICENSE).

**Auteur** : Rihen — nkentseu@gmail.com
