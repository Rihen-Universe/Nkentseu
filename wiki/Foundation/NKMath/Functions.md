# Les fonctions mathématiques

Sous les vecteurs et les matrices, il y a les **fonctions scalaires** : le sinus, la racine
carrée, l'interpolation, le bornage… NKMath en rassemble une bibliothèque complète, utilisée
absolument partout dans le moteur — une balle qui décrit un cercle appelle `NkCos`/`NkSin`,
une couleur qu'on borne appelle `NkSaturate`, une transition douce appelle `NkSmoothstep`.

Pourquoi une bibliothèque maison plutôt que `<cmath>` ? Pour l'uniformité (mêmes noms, même
comportement sur toutes les plateformes), pour les versions vectorisées (SIMD), et pour
quelques fonctions absentes de la lib standard mais précieuses en temps réel — `NkRsqrt`,
`NkSaturate`, `NkSmoothstep`, `NkNearlyEqual`…

```cpp
#include "NKMath/NkFunctions.h"
using namespace nkentseu::math;

float32 x = R * NkCos(t);
float32 y = R * NkSin(t);                // une balle sur un cercle
float32 v = NkClamp(speed, 0.f, 600.f);  // borner une vitesse
float32 e = NkSmoothstep(0.f, 1.f, p);   // une transition douce (easing)
```

Au-delà du catalogue ci-dessous, retenez surtout les fonctions « temps réel » qu'on oublie
souvent : `NkNearlyEqual` pour comparer des flottants (jamais `==`), `NkSaturate` pour
ramener dans `[0,1]` (couleurs, facteurs), `NkSmoothstep` pour adoucir une interpolation, et
`NkRsqrt` (l'inverse de la racine) pour normaliser vite.

---

## Aperçu de l'API

Toutes les fonctions, par famille (détails et cas d'usage dans la « Référence complète ») :

| Catégorie | Fonctions |
|-----------|-----------|
| Trigonométrie | `NkSin` `NkCos` `NkTan` · `NkAsin` `NkAcos` `NkAtan` `NkAtan2` · `NkSinh` `NkCosh` `NkTanh` |
| Puissances & racines | `NkSqrt` `NkRsqrt` `NkCbrt` `NkPow` `NkPowInt` `NkSquare` · `NkExp` `NkLog` `NkLog2` `NkLog10` |
| Arrondis & décomposition | `NkFloor` `NkCeil` `NkRound` `NkTrunc` · `NkFmod` `NkModf` `NkFrexp` `NkLdexp` `NkILogb` `NkCopysign` |
| Bornage | `NkAbs` `NkFabs` `NkMin` `NkMax` `NkClamp` `NkSaturate` |
| Interpolation | `NkLerp` `NkMix` `NkSmoothstep` `NkSmootherstep` |
| Comparaison flottants | `NkNearlyEqual` `NkIsNearlyZero` `NkIsFinite` |
| Entiers & bits | `NkIsPowerOf2` `NkNextPowerOf2` `NkClz` `NkCtz` `NkPopcount` `NkGcd` `NkLcm` `NkDivMod64` `NkDivI64` |
| Conversion d'angle | `NkToRadians` `NkToDegrees` `NkRadiansFromDegrees` `NkDegreesFromRadians` |
| Constantes | `NkPi` `NkPis2` (π/2) `NkPi2` (2π) `NkEpsilon` |
| SIMD (`NkSIMD.h`) | versions vectorisées des fonctions chaudes |

---

## Référence complète

### Trigonométrie

`NkSin`, `NkCos`, `NkTan` (qui attendent des **radians**) sont la base de tout ce qui oscille
ou tourne : un objet qui décrit un cercle (`x = R·cos t`, `y = R·sin t`), un mouvement de
flottaison, une onde. Leurs inverses `NkAsin`/`NkAcos`/`NkAtan` retrouvent un angle à partir
d'un rapport. Une mérite d'être mise en avant : **`NkAtan2(y, x)`** donne l'angle d'un vecteur
`(x, y)` — c'est l'outil qui répond à « dans quelle direction pointe ce vecteur ? » :

- **Gameplay / IA** : le cap d'un projectile, l'angle vers lequel orienter un sprite pour
  qu'il regarde sa cible (un « look-at » 2D).
- **UI** : l'angle d'une jauge circulaire, d'un cadran.
- **Physique** : l'orientation d'une vélocité.

Les hyperboliques `NkSinh`/`NkCosh`/`NkTanh` servent surtout à des courbes et certaines
fonctions d'easing.

### Puissances et racines

`NkSqrt` calcule la racine carrée exacte (longueur d'un vecteur, distance). Sa cousine
**`NkRsqrt`** calcule l'**inverse** de la racine (`1/√x`) — c'est précisément ce dont on a
besoin pour **normaliser** un vecteur (le diviser par sa longueur), et c'est plus rapide que
`1.0 / NkSqrt(x)` ; sur les chemins chauds (des milliers de normalisations par frame), la
différence compte. `NkPow`/`NkPowInt`/`NkSquare`/`NkCbrt` couvrent les puissances, et
`NkExp`/`NkLog`/`NkLog2`/`NkLog10` les décroissances exponentielles (un fondu qui ralentit) et
les échelles logarithmiques (le son en décibels).

### Bornage et interpolation — le cœur du temps réel

C'est la famille qu'on utilise le plus, et celle qui mérite le plus d'attention.

**`NkClamp(v, lo, hi)`** borne une valeur dans un intervalle — une barre de vie qui ne
descend pas sous 0 ni au-dessus du max, une vitesse plafonnée, un index qui reste valide.
**`NkSaturate(v)`** est le cas particulier `[0, 1]`, omniprésent en **rendu** : les
composantes d'une couleur, un facteur d'éclairage, un alpha doivent rester dans `[0, 1]`.

**`NkLerp(a, b, t)`** interpole linéairement (`a + (b−a)·t`). On le retrouve partout :
- **Animation** : déplacer un objet de A vers B, un fondu de couleur, une caméra qui suit.
- **Audio** : un fondu de volume.
- **Procédural** : mélanger deux valeurs selon un poids.

**`NkSmoothstep(a, b, t)`** (et `NkSmootherstep`, plus doux encore) est une interpolation
**adoucie** : au lieu d'une transition linéaire, le départ et l'arrivée se font en douceur
(dérivée nulle aux bords). C'est l'**easing** de base — une UI qui glisse élégamment, une
porte qui s'ouvre, un dégradé de brouillard qui s'estompe naturellement plutôt que d'un coup.

### Comparaison de flottants

Une règle d'or : **ne comparez jamais deux flottants avec `==`**. Le moindre arrondi fait
diverger des valeurs « égales », et le test échoue de façon imprévisible. **`NkNearlyEqual(a, b)`**
teste l'égalité *à une tolérance près*, et c'est ce qu'il faut utiliser. `NkIsNearlyZero(x)`
en est le cas particulier autour de zéro, et `NkIsFinite(x)` détecte une valeur corrompue
(infini ou NaN) — précieux pour repérer une division par zéro qui s'est propagée.

### Entiers, bits, angles, constantes

`NkIsPowerOf2`/`NkNextPowerOf2` servent partout où les puissances de deux comptent — tailles
d'allocation, alignements, dimensions de textures. `NkClz`/`NkCtz`/`NkPopcount` manipulent les
bits, `NkGcd`/`NkLcm` font de l'arithmétique entière. Côté angles, `NkToRadians`/`NkToDegrees`
convertissent les unités (ou, mieux, utilisez le type [`NkAngle`](Quaternions-Angles.md) qui
porte son unité). Enfin, les constantes `NkPi`, `NkPis2` (π/2), `NkPi2` (2π, le tour complet),
`NkEpsilon` évitent les valeurs magiques.

### Variantes SIMD (`NkSIMD.h`)

Pour les traitements de **masse** — des milliers de particules, du signal audio — il existe
des versions vectorisées des fonctions chaudes (`NkCos`/`NkSin`/`NkSqrt`… par lots). Réservez-
les aux hot-paths volumineux ; pour le cas scalaire courant, les fonctions ci-dessus prennent
déjà la meilleure implémentation matérielle disponible.

> **En résumé.** Au quotidien : `NkClamp`/`NkLerp` pour borner et interpoler, `NkSmoothstep`
> pour adoucir, `NkSaturate` pour les couleurs, `NkAtan2` pour l'angle d'un vecteur, `NkRsqrt`
> pour normaliser vite, et surtout **`NkNearlyEqual` plutôt que `==`** pour les flottants.

---

[← Quaternions & angles](Quaternions-Angles.md) · [Index NKMath](README.md) · [La couleur →](Color.md)
