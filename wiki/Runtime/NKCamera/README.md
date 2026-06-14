# NKCamera — documentation détaillée

Le module **NKCamera**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKCamera.md](../NKCamera.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses cas d'usage
concrets (énumération, streaming, photo/vidéo, mapping IMU, multi-flux).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Camera.md](Camera.md) | Énumérer et ouvrir les caméras, récupérer les frames (queue thread-safe), prendre photo/vidéo, contrôles physiques, gérer plusieurs flux (`NkMultiCamera`), piloter une `NkCamera2D` depuis l'IMU, et le contrat backend `NKICameraBackend`. | `NkCameraSystem.h`, `NkCamera2D.h`, `NkCameraTypes.h`, `NKICameraBackend.h` |

[← Récap NKCamera](../NKCamera.md) · [← Couche Runtime](../README.md)
