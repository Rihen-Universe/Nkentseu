# NKMath — documentation détaillée

Le module **NKMath**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKMath.md](../NKMath.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec sa formule et
ses cas d'usage concrets (éclairage, collision, animation, IA, 2D…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Vectors.md](Vectors.md) | Positions, directions, vitesses : arithmétique, `Dot`/`Cross`, normalisation, réflexion, interpolation. | `NkVec.h` |
| [Matrices.md](Matrices.md) | Transformations d'objets : composition, application à un point/direction/normale, inverse, décomposition. | `NkMat.h` |
| [Quaternions-Angles.md](Quaternions-Angles.md) | Rotations 3D sans gimbal lock (`Slerp`…), angles typés. | `NkQuat.h`, `NkAngle.h`, `NkEulerAngle.h` |
| [Functions.md](Functions.md) | Boîte à outils scalaire : trig, `NkClamp`/`NkLerp`/`NkSmoothstep`, `NkNearlyEqual`… + SIMD. | `NkFunctions.h`, `NkSIMD.h` |
| [Color.md](Color.md) | Trois représentations de couleur (8 bits / flottant / HSV) et leurs conversions. | `NkColor.h` |
| [Geometry.md](Geometry.md) | Survol, collision (SAT), boîtes englobantes : rectangles, segments, intervalles. | `NkRectangle.h`, `NkSegment.h`, `NkRange.h` |
| [Random.md](Random.md) | Tirages aléatoires : scalaires, vecteurs, couleurs, matrices. | `NkRandom.h` |

[← Récap NKMath](../NKMath.md) · [← Couche Foundation](../README.md)
