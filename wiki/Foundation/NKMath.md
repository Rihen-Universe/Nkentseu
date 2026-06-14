# NKMath

> Couche **Foundation** · La bibliothèque mathématique du moteur : vecteurs, matrices,
> quaternions, angles, couleurs, géométrie 2D, fonctions et aléatoire — avec accélération SIMD.

Dès qu'une chose a une **position**, une **direction**, une **orientation**, une **couleur**
ou une **dimension**, elle s'exprime avec NKMath. C'est le langage commun de tout le moteur :
le rendu calcule des transformations de matrices, la physique projette des vecteurs, les
caméras composent des rotations, l'UI teste des rectangles. Comprendre NKMath, c'est tenir le
fil de presque tout le reste.

Les types sont **templatés** sur le type des composantes (`NkVec3T<float32>`,
`NkVec3T<int32>`…) mais, en pratique, on manipule presque toujours leurs **alias** prêts à
l'emploi — `NkVec3` (float), `NkVec2i` (entier), `NkColor`, `NkMat4`… Cette doc utilise les
alias partout.

- **Namespace** : `nkentseu::math`
- **Header parapluie** : `#include "NKMath/NKMath.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Manipuler des positions, directions, vitesses | [Les vecteurs](NKMath/Vectors.md) |
| Déplacer / tourner / dimensionner des objets (transformations) | [Les matrices](NKMath/Matrices.md) |
| Orienter et interpoler des rotations 3D | [Quaternions & angles](NKMath/Quaternions-Angles.md) |
| Trigonométrie, interpolation, bornage, comparaison de flottants | [Les fonctions](NKMath/Functions.md) |
| Mélanger, éclaircir, convertir des couleurs | [La couleur](NKMath/Color.md) |
| Tests de survol, de collision, de bornes (rectangles, segments, intervalles) | [La géométrie](NKMath/Geometry.md) |
| Tirer des nombres, vecteurs, couleurs aléatoires | [L'aléatoire](NKMath/Random.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses formules et
ses cas d'usage concrets (éclairage, collision, animation, IA, 2D…).

---

## Aperçu des familles

- **Vecteurs** (`NkVec.h`) — `NkVec2/3/4` et leurs variantes typées. Arithmétique, produit
  scalaire (`Dot`) et vectoriel (`Cross`), longueur (`Len`/`LenSq`), normalisation,
  projection, réflexion, interpolation.
- **Matrices** (`NkMat.h`) — `NkMat2/3/4`. Composition de transformations, application à un
  point / une direction / une normale, déterminant, inverse, décomposition.
- **Quaternions & angles** (`NkQuat.h`, `NkAngle.h`, `NkEulerAngle.h`) — `NkQuat` pour les
  rotations sans gimbal lock (`Slerp`…), `NkAngle`/`NkAngleD` pour un angle qui porte son
  unité, `NkEulerAngle` (pitch/yaw/roll).
- **Fonctions** (`NkFunctions.h`, `NkSIMD.h`) — la boîte à outils scalaire : trigonométrie,
  racines, arrondis, `NkClamp`/`NkLerp`/`NkSmoothstep`, `NkNearlyEqual`, conversions d'angle,
  constantes — avec variantes SIMD pour le traitement par lots.
- **Couleur** (`NkColor.h`) — `NkColor` (RGBA 8 bits, c'est le `NkColor2D` du rendu),
  `NkColorF` (flottant, pour calculer), `NkHSV` (ajustements artistiques).
- **Géométrie** (`NkRectangle.h`, `NkSegment.h`, `NkRange.h`) — `NkRectangle` (survol,
  collision SAT, AABB), `NkSegment`, `NkRange` (intervalles bornés).
- **Aléatoire** (`NkRandom.h`) — `NkRandom` tire scalaires, vecteurs, couleurs, matrices.

---

## Index des 16 headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKMath.h` | Parapluie (inclut tout). | — |
| `NkVec.h` | `NkVec2/3/4T` + alias. | [Vecteurs](NKMath/Vectors.md) |
| `NkMat.h` | `NkMat2/3/4T`. | [Matrices](NKMath/Matrices.md) |
| `NkQuat.h` | `NkQuatT` / `NkQuat`. | [Quaternions & angles](NKMath/Quaternions-Angles.md) |
| `NkAngle.h` | `NkAngleT` (`NkAngle`/`NkAngleD`). | [Quaternions & angles](NKMath/Quaternions-Angles.md) |
| `NkEulerAngle.h` | `NkEulerAngle`. | [Quaternions & angles](NKMath/Quaternions-Angles.md) |
| `NkFunctions.h` | Fonctions math (trig, sqrt, lerp…). | [Fonctions](NKMath/Functions.md) |
| `NkSIMD.h` | Variantes SIMD. | [Fonctions](NKMath/Functions.md) |
| `NkColor.h` | `NkColor`, `NkColorF`, `NkHSV`. | [Couleur](NKMath/Color.md) |
| `NkRectangle.h` | `NkRectT` + alias rect. | [Géométrie](NKMath/Geometry.md) |
| `NkSegment.h` | `NkSegment`. | [Géométrie](NKMath/Geometry.md) |
| `NkRange.h` | `NkRangeT` + alias. | [Géométrie](NKMath/Geometry.md) |
| `NkRandom.h` | `NkRandom`. | [Aléatoire](NKMath/Random.md) |
| `NkMathFormat.h` | `NkFormatter` (formatage texte des types math). | — |
| `NkLegacySystem.h` | Compat ancienne API. | — |
| `NkMathApi.h` | Macros d'export. | — |

---

[← Couche Foundation](README.md) · [Index du wiki](../README.md)
