# NKCamera

> Couche **Runtime** · La capture caméra multi-OS du moteur : énumération et ouverture
> de périphériques, flux de frames, photo et vidéo, caméra 2D pilotée par l'IMU — au-dessus
> d'une interface backend par plateforme.

Dès qu'une application a besoin de **voir le monde réel** — webcam d'un jeu, caméra avant/arrière
d'un mobile, capture multi-flux d'un outil — elle passe par NKCamera. Le module énumère les
périphériques, ouvre une session de streaming, livre des frames thread-safe à votre boucle
principale, et sait piloter une petite caméra 2D virtuelle à partir des données de l'IMU
(yaw/pitch/roll) pour les usages mobile/XR. Tout le travail spécifique à l'OS (Win32, Cocoa,
UIKit, Android, Linux, Emscripten) est caché derrière une interface unique.

On manipule presque toujours la **façade singleton** `NkCameraSystem` (une caméra physique à la
fois) via le raccourci `NkCamera()`, et `NkMultiCamera` quand on veut plusieurs flux en parallèle.
La configuration passe par des structs simples (`NkCameraConfig`, `NkCameraFrame`…) ; le format de
pixel `NkPixelFormat` n'est PAS défini ici — il vient de la couche NKWindow.

- **Namespace** : `nkentseu`
- **Header d'entrée** : `#include "NKCamera/NkCameraSystem.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Lister les caméras disponibles et leurs modes | [La caméra](NKCamera/Camera.md) |
| Ouvrir un flux, récupérer les frames dans la boucle principale | [La caméra](NKCamera/Camera.md) |
| Prendre une photo, lancer un enregistrement vidéo | [La caméra](NKCamera/Camera.md) |
| Gérer plusieurs caméras simultanément | [La caméra](NKCamera/Camera.md) |
| Piloter une caméra 2D virtuelle depuis l'IMU (mobile/XR) | [La caméra](NKCamera/Camera.md) |
| Convertir une frame en RGBA8, lire un pixel | [La caméra](NKCamera/Camera.md) |
| Écrire un backend de plateforme (`NKICameraBackend`) | [La caméra](NKCamera/Camera.md) |

La page suit la même structure que le reste du wiki : un **tutoriel** narratif, un **aperçu**
tabulaire de toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses
cas d'usage concrets (énumération, streaming, photo/vidéo, mapping IMU, multi-flux).

---

## Aperçu des familles

- **Système de caméra** (`NkCameraSystem.h`) — la façade `NkCameraSystem` (singleton, une caméra à
  la fois) accessible via `NkCamera()` : énumération, streaming, queue de frames thread-safe, photo,
  vidéo, contrôles physiques (focus/exposition/zoom/flash), mapping IMU vers une `NkCamera2D`.
  `NkMultiCamera` gère plusieurs flux (`Stream`) indépendants.
- **Caméra 2D** (`NkCamera2D.h`) — `NkCamera2D`, une caméra minimale (position + rotation) pilotée
  par le système via `UpdateVirtualCamera()`.
- **Types & formats** (`NkCameraTypes.h`) — les structs de données : `NkCameraDevice` (+ `Mode`),
  `NkCameraConfig`, `NkCameraFrame`, `NkPhotoCaptureResult`, `NkVideoRecordConfig`,
  `NkCameraOrientation`, les enums `NkCameraFacing`/`NkCameraResolution`/`NkCameraState`, les
  callbacks `NkFrameCallback`/`NkCameraHotPlugCallback`, et les helpers
  `NkCameraPixelFormatToString`/`NkResolutionToSize`.
- **Interface backend** (`NKICameraBackend.h`) — `NKICameraBackend`, le contrat polymorphe que
  chaque plateforme implémente (Win32/Cocoa/UIKit/Android/Linux/Emscripten/Noop, hors périmètre).

> **Note de dépendance** : `NkPixelFormat` provient de `NKWindow/Core/NkTypes.h` (couche NKWindow).
> NKCamera n'ajoute que `NkCameraPixelFormatToString` autour de lui.

---

## Index des 4 headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkCameraSystem.h` | Header d'entrée. `NkCameraSystem` (singleton + `NkCamera()`), `NkMultiCamera` + `Stream`, alias `NkCameraBackend`. | [La caméra](NKCamera/Camera.md) |
| `NkCamera2D.h` | `NkCamera2D` (caméra 2D minimale, position + rotation). | [La caméra](NKCamera/Camera.md) |
| `NkCameraTypes.h` | Structs/enums/callbacks et helpers (`NkCameraDevice`, `NkCameraConfig`, `NkCameraFrame`…). | [La caméra](NKCamera/Camera.md) |
| `NKICameraBackend.h` | Interface backend `NKICameraBackend`. | [La caméra](NKCamera/Camera.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
