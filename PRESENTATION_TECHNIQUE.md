# Nkentseu, Jenga, Noge & Nogee — présentation technique

> Vue d'ensemble pour un public technique (développeurs, ingénieurs). Pour une
> version grand public, voir [EXPLICATION_SIMPLE.md](EXPLICATION_SIMPLE.md).

---

## En une phrase

**Jenga** (build system) construit **Nkentseu** (moteur/framework C++ zero-STL,
cross-plateforme) ; **Noge** en est la couche moteur de jeu (Engine) ; **Nogee**
est l'éditeur bâti sur Noge.

---

## Jenga — système de build

Système de build **cross-plateforme écrit en Python**, édité par **Rihen**
(v2.0.4). Particularité : il compile les projets natifs **directement via les
toolchains natives** (clang/MSVC/gcc, NDK, emsdk, OHOS-SDK…), **sans générer de
CMake ni de Makefile** intermédiaire.

- **Langages cibles** : C, C++, Objective-C, Assembly, Rust, Zig.
- **Descripteurs** : fichiers `.jenga` (Python + DSL : `workspace`, `project`,
  `toolchain`, `filter`, packaging, réseau/pare-feu centralisé via `FirewallSpec`…).
- **Plateformes** : Windows, Linux, Android, Web/WASM (production) ; macOS, iOS,
  HarmonyOS (prêtes) ; Xbox (partiel) ; Switch/PS (licence).
- **Atouts** : cache incrémental multi-niveaux, build parallèle, daemon,
  packaging multi-format (MSI/Inno/DEB/PKG/APK/HAP + installeur self-extracting),
  génération de docs.

> Produit **autonome** : Jenga peut construire n'importe quel projet natif, pas
> seulement Nkentseu. Ici, c'est l'outil qui compile toute la stack ci-dessous.

---

## Nkentseu — moteur / framework

Framework C++ **modulaire, haute performance, cross-plateforme et zero-STL**
(conteneurs/algorithmes maison, pas de `std::`, allocateurs custom — `NkAlloc`/
`NkFree`). Pensé pour jeux, simulation, VR/AR/MR, CAO, scientifique. Développé en
**solo par Rihen**, **construit avec Jenga**.

### Architecture en couches (chaque couche n'utilise que celles en dessous)

```
Applications (Pong, démos, PV3DE, …)
        ▲
Engine  — Noge  (framework Application : boucle, LayerStack, EventBus, ECS gameplay)
        ▲
Runtime — NKWindow · NKEvent · NKCanvas (2D) · NKRHI · NKRenderer (3D) · NKUI ·
          NKImage · NKAudio · NKCamera · NKFont · NKECS · NKCollision
        ▲
System  — NKLogger · NKThreading · NKTime · NKStream · NKFileSystem ·
          NKNetwork · NKReflection · NKSerialization
        ▲
Foundation — NKCore · NKMath · NKMemory · NKContainers · NKPlatform
        ▲
OS / Matériel — Windows · Linux · macOS · Android · iOS · Web · HarmonyOS · Xbox
```

### Deux chemins de rendu **parallèles** (point d'architecture clé)

| Chemin | Stack | Pour quoi |
|--------|-------|-----------|
| **2D** | **NKCanvas** (façade SFML-like) + **NKUI** (immediate-mode, « notre ImGui ») | Jeux 2D et démos (ex. **Pong**) |
| **3D** | **NKRenderer** (PBR/IBL/ombres, ~MVP UE5-like) → **NKRHI** (abstraction GPU) | Moteur **Noge** et éditeur **Nogee** |

- **NKRHI** : couche d'abstraction GPU, **6 backends** (OpenGL, Vulkan, DX11, DX12,
  Metal, Software) — handles opaques, compute, cross-compile de shaders.
- **NKCanvas** : moteur 2D façade (`NkRenderer2D`, `NkRenderWindow`, `NkTexture`,
  `NkFont`, formes, clip/scissor) au-dessus de ses propres backends 2D.
- **NKUI** : interface immediate-mode ; rend via NKCanvas pour la 2D **et** via
  NKRenderer pour l'éditeur.

---

## Noge — moteur de jeu (couche Engine)

**Noge = *Nkentseu Onirique Game Engine*** 🌙 — la couche **Engine** de Nkentseu :
le **framework applicatif** sur lequel on développe un jeu.

- Boucle principale (fixed + variable timestep), `Application` lifecycle
  (`OnInit/OnStart/OnUpdate/OnRender/…`), **LayerStack** (Layers/Overlays),
  **EventBus** typé (publish/subscribe), **ECS gameplay** (GameObject, components,
  systèmes, scènes, prefabs).
- **Rend via NKRenderer** (→ NKRHI), **pas** via NKCanvas.
- État : socle (Application/LayerStack/EventBus) en place ; nombreux sous-systèmes
  (anim, design, modeling…) encore au stade spécification.

---

## Nogee — éditeur du moteur Noge

**Nogee = Noge + `e` (Editor)** : l'**éditeur** du moteur Noge.

- C'est une **application bâtie sur Noge** (elle *utilise* Noge).
- Panels dockables (Viewport, SceneTree, Inspector, AssetBrowser, Console…),
  gizmos, sélection, undo/redo, gestion de projet.
- **Rend via NKRenderer** et **utilise NKUI à travers NKRenderer** pour son UI.
- Anciennement nommé **Unkeny** dans des documents plus anciens.

---

## Schéma des dépendances

```
Jenga ──construit──▶ Nkentseu
                        │
                        ├─ Foundation / System (NKCore, NKMath, NKMemory, NKThreading, …)
                        │
                        ├─ NKRHI ─▶ NKRenderer (3D) ─▶ Noge ─▶ Nogee
                        │                                 └─ NKUI (via NKRenderer)
                        │
                        └─ NKCanvas (2D) + NKUI ─▶ jeux 2D (Pong, démos)
```

---

## Récapitulatif

| Élément | Nature | Rôle | Rendu |
|---------|--------|------|-------|
| **Jenga** | Build system (Python) | Compile/package toute la stack, multi-OS | — |
| **Nkentseu** | Moteur/framework C++ zero-STL | Toute la stack technique en couches | — |
| **Noge** | Couche Engine de Nkentseu | Framework de jeu 2D/3D (boucle, ECS, events) | **NKRenderer → NKRHI** |
| **Nogee** | Application (sur Noge) | Éditeur visuel du moteur Noge | **NKRenderer** + **NKUI** |

> **Annexes** : **PV3DE** (Patient Virtuel 3D Émotif — application médicale cible,
> au stade doc) est l'autre application phare visée par l'écosystème.
