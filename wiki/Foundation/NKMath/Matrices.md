# Les matrices

Si le vecteur représente un point ou une direction, la **matrice** représente une
*transformation* : déplacer, tourner, mettre à l'échelle. Multiplier un vecteur par une
matrice, c'est lui appliquer la transformation. C'est l'outil central du rendu et de tout ce
qui bouge dans une scène.

NKMath fournit des matrices carrées `NkMat2`, `NkMat3` et `NkMat4` (templatées, mais on
utilise les alias). `NkMat4` — la matrice 4×4 — est celle des transformations 3D complètes,
et c'est sur elle qu'on passera le plus de temps.

---

## Les opérations de base

Toutes les matrices partagent un socle commun. `Identity()` fabrique la matrice neutre
(celle qui ne transforme rien), l'opérateur `*` compose deux matrices ou applique une
matrice à un vecteur, `Transpose()` retourne la matrice selon sa diagonale, et `Inverse()`
calcule la transformation inverse.

```cpp
NkMat4 m = /* ... */;
NkVec4 p = m * NkVec4{ 1.f, 0.f, 0.f, 1.f };   // transforme un point
NkMat4 back = m.Inverse();                       // la transformation inverse
```

Un détail de robustesse vaut d'être noté : `Inverse()` d'une matrice **singulière** (non
inversible) renvoie l'identité plutôt que de produire des valeurs aberrantes — un secours
sûr qui évite de propager des `NaN` dans toute une scène.

---

## Construire une transformation (NkMat4)

Plutôt que de remplir une matrice à la main, on la fabrique à partir de son intention. Les
*factories* statiques de `NkMat4` couvrent les transformations élémentaires : `Translation`
(ou son alias `Translate`) pour un déplacement, `Scaling`/`Scale` pour une mise à l'échelle,
et les rotations — `RotationX`, `RotationY`, `RotationZ` autour d'un axe, ou `Rotation` à
partir d'angles d'Euler ou d'un axe quelconque.

Remarquez que les rotations prennent un `NkAngle` — un angle *typé* — et non un simple
flottant. C'est volontaire : on ne peut plus passer « 90 » en croyant des degrés là où des
radians sont attendus, l'unité est dans le type (voir
[Quaternions & angles](Quaternions-Angles.md)).

On combine ensuite ces briques par multiplication. Et c'est là que se cache le point le plus
important : **l'ordre se lit de droite à gauche**. Dans `T * R * S`, c'est l'échelle qui
s'applique d'abord, puis la rotation, puis la translation :

```cpp
NkMat4 model = NkMat4::Translation({ 10.f, 0.f, 0.f })
             * NkMat4::RotationY(angle)
             * NkMat4::Scaling({ 2.f, 2.f, 2.f });
// l'objet est d'abord agrandi ×2, puis tourné autour de Y, puis déplacé de 10 en X
```

L'opération inverse existe aussi : `Extract` **décompose** une `NkMat4` en ses composantes
translation / rotation (Euler) / échelle.

Un point pour éviter une fausse attente : `NkMat4` ne propose **pas** de factory
`Perspective` ni `LookAt`. Les matrices de projection et de vue ne sont pas construites ici,
mais au niveau du rendu (les caméras de NKCanvas/NKRenderer) — NKMath s'arrête aux
transformations d'objets.

> **En résumé.** Une matrice est une transformation ; `m * v` l'applique à un vecteur.
> Fabriquez les `NkMat4` avec `Translation`/`Scaling`/`RotationX/Y/Z`/`Rotation`, composez-les
> par `*` en lisant **de droite à gauche**, et souvenez-vous que projection et vue se
> construisent côté rendu, pas dans NKMath.

---

## Aperçu de l'API

`NkMat4` (4×4) est la matrice de transformation 3D ; `NkMat2`/`NkMat3` suivent le même modèle
en dimension inférieure. La liste de tout ce qui est public :

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `static Identity()` `Zero()` | Matrice neutre / nulle. |
| Accès | `operator[](col)` | Accès à une colonne (rangement **par colonnes**). |
| Accès | `operator T*()` | Vue tableau brut (envoi GPU). |
| Accès | `a == b` `a != b` | Comparaison. |
| Composition | `a * b` | Compose deux transformations (droite → gauche). |
| Arithmétique | `a + b` `a - b` `a * s` (+ `+=` `-=` `*=`) | Somme, différence, scalaire (rare). |
| Fabriquer | `Translation(v)` / `Translate(v)` | Déplacement. |
| Fabriquer | `Scaling(v)` / `Scale(v)` | Mise à l'échelle. |
| Fabriquer | `RotationX/Y/Z(angle)` | Rotation autour d'un axe (`NkAngle`). |
| Fabriquer | `Rotation(euler)` / `Rotation(axis, angle)` | Rotation Euler / axe quelconque. |
| Appliquer | `operator*(vec4)` / `operator*(vec3)` | Produit matrice-vecteur. |
| Appliquer | `TransformPoint(p)` | Transforme un **point** (translation + perspective). |
| Appliquer | `TransformVector(v)` | Transforme une **direction** (sans translation). |
| Appliquer | `TransformNormal(n)` | Transforme une **normale** (inverse-transposée). |
| Propriétés | `Transpose()` | Transposée. |
| Propriétés | `Determinant()` | Déterminant (handedness, singularité). |
| Propriétés | `Inverse()` | Transformation inverse. |
| Propriétés | `OrthoNormalized()` / `OrthoNormalizeScaled()` | Ré-orthonormalisation. |
| Décomposition | `Extract(T, R, S)` | Décompose en translation / rotation / échelle. |
| Alias | `NkMat2` `NkMat3` `NkMat4` | Par dimension. |

---

## Référence complète

### Construction, accès, composition

`Identity()` produit la matrice neutre — celle qui ne transforme rien, et le point de départ
de toute composition ; `Zero()` la matrice nulle. On accède à une colonne par `operator[]`
(la matrice est rangée **par colonnes**, comme l'attend OpenGL), et `operator T*()` en donne
une vue tableau brut pour l'**envoyer au GPU** dans un uniform ou un constant buffer.

L'opération centrale est `a * b`, qui **compose** deux transformations. L'ordre est l'inverse
de la lecture : dans `T * R * S`, c'est `S` (l'échelle) qui s'applique d'abord, puis `R`, puis
`T` — on lit de **droite à gauche**. Les autres opérations (`+`, `-`, `* scalaire`) servent
surtout à interpoler des matrices, ce qui est rare.

### `TransformPoint`, `TransformVector`, `TransformNormal` — appliquer correctement

Voici la distinction la plus importante du chapitre, et celle qu'on rate le plus souvent : un
**point**, une **direction** et une **normale** ne se transforment **pas** de la même façon.

- **Un point** (`TransformPoint`) subit la translation et la division perspective (le `w`).
  C'est ce qu'on veut pour placer un **sommet** dans le monde, ou projeter une position à
  l'écran. `operator*` avec un `vec4` (w = 1) fait l'équivalent.
- **Une direction** (`TransformVector`) ignore la translation (`w = 0`) : déplacer une
  direction n'aurait aucun sens. Usages : transformer un **axe**, une **vitesse**, une
  direction de visée d'un repère à un autre.
- **Une normale** (`TransformNormal`) passe par l'**inverse-transposée**. C'est *le* piège de
  l'éclairage : sous une mise à l'échelle **non uniforme**, transformer une normale comme une
  simple direction la fausse — elle cesse d'être perpendiculaire à la surface, et tout
  l'ombrage devient faux. `TransformNormal` fait le calcul correct, ce qui est essentiel dès
  qu'on a des objets étirés.

### Fabriquer une transformation

`Translation`, `Scaling` et les `Rotation*` construisent les briques élémentaires. Les
rotations prennent un `NkAngle` (donc l'unité est explicite — voir
[Quaternions & angles](Quaternions-Angles.md)). On les compose ensuite par `*` pour obtenir la
matrice **modèle** d'un objet :

- **Rendu / scène** : positionner, orienter et dimensionner chaque objet du monde.
- **Animation** : recalculer la matrice d'un os ou d'un objet à chaque frame.
- **Instancing** : générer des centaines de matrices (une forêt, une foule) à partir de
  transformations variées.

> `NkMat4` ne propose **pas** de factory `Perspective` ni `LookAt` : projection et vue se
> construisent côté rendu (caméras NKCanvas/NKRenderer), pas dans NKMath.

### `Determinant`, `Inverse`, `Transpose`, `OrthoNormalized` — propriétés

`Determinant()` mesure le facteur d'échelle du volume. Son **signe** révèle un retournement
(passage d'un repère droitier à gaucher — utile pour corriger l'ordre des sommets après un
miroir), et une valeur **nulle** signale une matrice **singulière**, non inversible.
`Inverse()` calcule la transformation inverse (avec une identité de secours si la matrice est
singulière) — on s'en sert pour passer du monde à l'espace local d'un objet, ou pour bâtir la
matrice de **vue** à partir de la transformation de la caméra. `Transpose()` retourne la
matrice selon sa diagonale ; combinée à l'inverse, elle donne la matrice des normales (cf.
`TransformNormal`). Enfin, `OrthoNormalized()` ré-orthonormalise les axes : sur une matrice
qu'on a fait tourner pendant des milliers de frames, les erreurs d'arrondi s'accumulent et
les axes finissent par n'être plus tout à fait perpendiculaires — cette méthode corrige la
dérive.

### `Extract` — décomposer

`Extract(T, R, S)` fait le chemin inverse des factories : il **décompose** une matrice en sa
translation, sa rotation (via quaternion, donc robuste au gimbal lock) et son échelle. Usages :
afficher les champs translation/rotation/échelle d'un objet dans l'**inspecteur d'un éditeur**,
**mélanger** des animations en interpolant séparément les trois composantes, ou **sérialiser**
une transformation de façon lisible.

### Exemple

```cpp
#include "NKMath/NkMat.h"
using namespace nkentseu::math;

NkMat4 model = NkMat4::Translation({ 10.f, 0.f, 0.f })
             * NkMat4::RotationY(angle)
             * NkMat4::Scaling({ 2.f, 2.f, 2.f });

NkVec3 worldPos = model.TransformPoint(localPos);    // un sommet (avec translation)
NkVec3 worldDir = model.TransformVector(localDir);   // une direction (sans translation)
NkVec3 worldN   = model.TransformNormal(localN);     // une normale (éclairage correct)
```

---

[← Les vecteurs](Vectors.md) · [Index NKMath](README.md) · [Quaternions & angles →](Quaternions-Angles.md)
