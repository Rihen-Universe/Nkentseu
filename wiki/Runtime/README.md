# Couche Runtime

La couche **Runtime** regroupe les **sous-systèmes** du moteur — tout ce qui tourne pendant
l'exécution d'une application : fenêtres et entrées, rendu 2D/3D, audio, polices, images, ECS,
interface utilisateur, collisions. Elle est bâtie sur [Foundation](../Foundation/README.md) et
[System](../System/README.md), et sert de socle à l'Engine (Noge) et aux applications.

## Modules

| Module | Rôle | Doc |
|--------|------|-----|
| **NKWindow** | Fenêtres natives multi-OS, surfaces, contexte, points d'entrée | [NKWindow.md](NKWindow.md) |
| **NKEvent** | Événements typés : clavier, souris, tactile, manette/HID, fenêtre, drag-drop | [NKEvent.md](NKEvent.md) |
| **NKImage** | Image CPU + 12 codecs from-scratch (PNG/JPEG/QOI/HDR/EXR…) | [NKImage.md](NKImage.md) |
| **NKCamera** | Capture caméra multi-OS | [NKCamera.md](NKCamera.md) |
| **NKFont** | Polices TTF/OTF from-scratch, triangulation de glyphes | [NKFont.md](NKFont.md) |
| **NKCollision** | Détection de collision : broad/narrow phase, CCD, formes, monde | [NKCollision.md](NKCollision.md) |
| **NKECS** | ECS bas niveau à archétypes : monde, stockage, requêtes, systèmes | [NKECS.md](NKECS.md) |
| **NKCanvas** | Rendu 2D SFML-like (transform, vertex array, shapes, sprites, textures, targets) | [NKCanvas.md](NKCanvas.md) |
| **NKRHI** | Interface matérielle de rendu (6 backends), compute, ML, command buffers | [NKRHI.md](NKRHI.md) |
| **NKSL** | Langage de shaders maison : lexer, parser, codegen, cross-compile, VM | [NKSL.md](NKSL.md) |
| **NKAudio** | Moteur audio AAA STL-free : bus, effets, HRTF, codecs, streaming | [NKAudio.md](NKAudio.md) |
| **NKUI** | UI immediate-mode : widgets, layout, dock, thème, rendu | [NKUI.md](NKUI.md) |
| **NKRenderer** | Rendu 3D UE5-like : PBR, IBL, ombres (CSM/VSM), bloom, voxel, sculpt, VFX | [NKRenderer.md](NKRenderer.md) |

## Dépendances

```
Foundation + System
   ▲
Runtime
   ├── NKWindow ─ NKEvent          (fenêtrage + entrées)
   ├── NKImage · NKFont · NKCamera (ressources/médias)
   ├── NKCanvas ─ NKRHI ─ NKSL     (rendu 2D, RHI, shaders)
   ├── NKAudio · NKUI · NKECS · NKCollision
   └── NKRenderer                  (rendu 3D, au sommet, utilise NKRHI/NKSL)
```

> **Note** : le NkSL de **NKRenderer** (transpileur texte ad-hoc, `@uniform`/`@target`) est
> DISTINCT du module **NKSL** (vrai compilateur `@binding`/`@stage`/`@entry`). Le parapluie
> `NKNetwork.h` (couche System) reste cassé ; sans rapport ici.

[← Index du wiki](../README.md) · [← Couche System](../System/README.md)
