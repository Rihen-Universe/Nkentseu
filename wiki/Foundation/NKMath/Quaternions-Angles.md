# Quaternions et angles

Représenter une rotation 3D semble simple — trois angles, un par axe — jusqu'à ce qu'on se
heurte au *gimbal lock* : dans certaines configurations, deux des trois axes s'alignent et
on perd un degré de liberté, avec des rotations qui « sautent ». Les **quaternions**
contournent ce problème, et c'est pourquoi ils sont l'outil de référence pour composer et
interpoler des rotations. NKMath fournit aussi des types d'angles explicites, pour ne plus
jamais confondre radians et degrés.

---

## NkQuat — la rotation 3D

Un quaternion unitaire encode une rotation. On en fait deux choses, exprimées par le même
opérateur `*`. Multiplier deux quaternions **compose** leurs rotations ; multiplier un
quaternion par un vecteur **applique** la rotation à ce vecteur :

```cpp
NkQuat r   = a * b;                // compose : applique d'abord b, puis a
NkVec3 dir = a * NkVec3{ 0,0,1 };  // tourne le vecteur (0,0,1)
```

Le quaternion a une propriété élégante qui simplifie la vie : pour un quaternion **unitaire**,
l'inverse est égal au conjugué. `Conjugate()` calcule donc l'inverse d'une rotation à moindre
coût qu'une inversion générale. `Normalize()`/`Normalized()` ramènent un quaternion à
l'unité (utile après des accumulations qui font dériver sa norme), et `IsNormalized()`
vérifie cette unitarité.

Mais l'intérêt majeur du quaternion, c'est l'**interpolation**. Faire passer une caméra ou
une articulation d'une orientation à une autre, en douceur, c'est interpoler entre deux
quaternions. NKMath en propose trois variantes, du compromis au plus soigné. `Lerp`
interpole linéairement (rapide, mais approximatif). `NLerp` ajoute une normalisation (correct
et rapide, au prix d'une vitesse angulaire qui n'est pas tout à fait constante). `Slerp` —
l'interpolation *sphérique*, l'algorithme de Shoemake — donne le résultat le plus juste, avec
une vitesse angulaire constante :

```cpp
NkQuat mid = a.Slerp(b, 0.5f);   // orientation à mi-chemin, parfaitement lisse
```

En pratique : `Slerp` pour les rotations qui doivent être visuellement parfaites (caméras,
animation), `NLerp` quand la performance prime et qu'une légère irrégularité passe inaperçue.
La construction d'un quaternion (depuis un axe et un angle, ou des angles d'Euler) et ses
conversions vers/depuis une matrice se trouvent dans `NkQuat.h`.

---

## NkAngle — un angle qui connaît son unité

Combien de bugs naissent d'un angle passé en degrés là où des radians étaient attendus ?
`NkAngle` (et sa version double précision `NkAngleD`) règle la question en rendant l'unité
**partie du type**. Un `NkAngle` transporte un angle sans ambiguïté ; c'est lui que prennent
les factories de rotation des [matrices](Matrices.md), ce qui rend l'unité explicite à
chaque appel — impossible de se tromper.

---

## NkEulerAngle — trois angles intuitifs

`NkEulerAngle` regroupe les trois angles familiers : **pitch** (tangage, autour de X), **yaw**
(lacet, autour de Y) et **roll** (roulis, autour de Z). C'est ce que prend
`NkMat4::Rotation`, qui les compose dans l'ordre `RotationZ(roll) * RotationY(yaw) *
RotationX(pitch)`.

```cpp
NkEulerAngle e{ /* pitch */, /* yaw */, /* roll */ };
NkMat4 rot = NkMat4::Rotation(e);
```

Les angles d'Euler sont intuitifs pour un humain, mais ce sont eux qui ramènent le gimbal
lock — d'où le conseil : pour de l'animation ou de la composition continue de rotations,
repassez aux quaternions.

> **En résumé.** `NkQuat` pour composer et interpoler des rotations sans gimbal lock
> (`Slerp` pour le rendu lisse, `NLerp` pour la vitesse, `Conjugate` = inverse pour un
> quaternion unitaire). `NkAngle` met l'unité dans le type. `NkEulerAngle` est intuitif mais
> sujet au gimbal lock — réservez-le aux cas simples.

---

## Aperçu de l'API

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| **NkQuat** | `NkQuat(axis, angle)` | Rotation depuis un axe et un angle. |
| Construire | `NkQuat(euler)` · `NkQuat(matrix)` | Depuis des angles d'Euler / une matrice. |
| Construire | `static RotateX/Y/Z(angle)` | Rotation autour d'un axe principal. |
| Construire | `static LookAt(...)` | Orienter vers une direction. |
| Appliquer | `a * b` | Compose deux rotations. |
| Appliquer | `q * v` (vec3) | Applique la rotation à un vecteur. |
| Arithmétique | `+` `-` `* s` `/ s` (+ `+=`…) · `operator[]` `operator T*` | Interne aux interpolations / accès aux composantes. |
| Inverser | `Conjugate()` · `Inverse()` | Inverse (= conjugué pour un unitaire). |
| Normaliser | `Normalize()` · `Normalized()` | Ré-unitarise. |
| Mesurer | `Dot(b)` · `Len()` · `LenSq()` | Proximité de deux rotations / normes. |
| Extraire | `Angle()` · `Axis()` | Angle / axe de la rotation représentée. |
| Convertir | `ToMat4()` / `operator NkMat4` · `operator NkEulerAngle` | Vers matrice / angles d'Euler. |
| Interpoler | `Lerp`/`Mix` · `NLerp` · `Slerp` | Interpolations de rotation. |
| **NkAngle** | `NkAngle` / `NkAngleD` | Angle typé float32 / float64. |
| Construire | `NkAngle(degrees)` · `FromRad/FromRadian(r)` | Depuis degrés / radians. |
| Convertir | `operator Precision()` | Valeur brute (pour `NkSin`…). |
| Arithmétique | `+` `-` `*` `/` · `-a` · `+=`… | Auto-wrappée. |
| **NkEulerAngle** | `pitch` `yaw` `roll` (NkAngle) | Tangage / lacet / roulis. |
| | `{ pitch, yaw, roll }` · `operator==` | Construction / comparaison. |

---

## Référence complète

### NkQuat — le quaternion

**Construire une rotation.** La forme la plus parlante est l'axe et l'angle :
`NkQuat(axis, angle)` se lit « tourne de `angle` autour de `axis` ». On peut aussi partir
d'angles d'Euler (`NkQuat(euler)`) ou extraire la rotation d'une matrice (`NkQuat(matrix)`),
et les raccourcis `RotateX/Y/Z(angle)` couvrent les axes principaux. Enfin, **`LookAt(...)`**
fabrique la rotation qui **oriente vers une direction** : c'est elle qui fait qu'une caméra
regarde sa cible ou qu'un ennemi se tourne vers le joueur.

**Composer et appliquer.** **`a * b`** compose deux rotations — le résultat applique d'abord
`b`, puis `a` (comme les matrices, on lit de droite à gauche). **`q * v`** (avec un `NkVec3`)
**applique** la rotation à un vecteur. C'est l'opération qu'on retrouve dans tous les domaines
où quelque chose s'oriente :

- **Rendu / caméra** : orienter un objet ou la caméra dans la scène.
- **Animation & skinning** : la rotation de chaque os d'un squelette.
- **Physique** : l'orientation d'un corps rigide en rotation.
- **Gameplay** : faire pivoter un personnage ou une tourelle vers une cible.

Les opérateurs arithmétiques restants (`+`, `-`, `* s`, `/ s`) et l'accès aux composantes
(`operator[]`, `operator T*` sur x, y, z, w) servent surtout en interne aux interpolations.

**Inverser et normaliser.** **`Conjugate()`** est l'astuce qui rend les quaternions
efficaces : pour un quaternion unitaire, le conjugué **est** l'inverse — il annule la
rotation, à un coût bien moindre qu'une inversion générale (`Inverse()` le renvoie
directement). Au fil des compositions la norme dérive un peu ; **`Normalize()` /
`Normalized()`** la ramènent à 1. **`Dot(b)`** mesure la proximité de deux rotations — c'est
ce que `Slerp` consulte pour prendre le chemin le plus court — et `Len()`/`LenSq()` donnent
les normes.

**Extraire et convertir.** On peut interroger un quaternion : **`Angle()`** donne l'angle de
rotation représenté, **`Axis()`** son axe. Et on le convertit selon le besoin —
**`ToMat4()`** (ou `operator NkMat4`) produit la matrice de rotation à envoyer au rendu,
**`operator NkEulerAngle`** redonne des angles d'Euler lisibles dans un éditeur.

**Interpoler.** Passer une orientation d'une valeur à une autre **en douceur**, c'est
interpoler entre deux quaternions — un besoin omniprésent :

- **Animation** : transition lisse entre deux poses (blend d'animations).
- **Caméra** : faire pivoter le regard vers une nouvelle cible sans à-coup.
- **IA / tourelles** : orienter progressivement un canon vers l'ennemi.

**`Slerp(to, t)`** est l'interpolation de référence (sphérique, vitesse angulaire constante)
pour les cas où le résultat doit être parfait. **`NLerp`** (lerp puis normalisation) est plus
rapide quand une légère irrégularité passe inaperçue ; **`Lerp`/`Mix`** est l'interpolation
linéaire brute.

### NkAngle — un angle qui connaît son unité

Un `NkAngle` est stocké en interne en degrés et **auto-normalisé** (« wrappé ») dans un
intervalle standard — fini les angles qui dérivent à 7300° au fil des accumulations. On le
construit depuis des **degrés** (`NkAngle(degrees)`) ou des **radians**
(`FromRad`/`FromRadian`), et `operator Precision()` récupère la valeur brute quand une
fonction comme `NkSin` l'attend. Ses opérateurs arithmétiques (`+`, `-`, `*`, `/`, la
négation `-a`, et les `+=`…, avec un autre angle ou un scalaire) sont **tous wrappés
automatiquement**. Aliases : `NkAngle` (float32), `NkAngleD` (float64).

Tout cela ne sert qu'une chose, mais une chose précieuse : rendre l'**unité explicite**. Les
factories de rotation des matrices et des quaternions prennent un `NkAngle`, ce qui élimine à
la racine le bug le plus banal de la 3D — « j'ai passé des degrés là où on attendait des
radians ».

### NkEulerAngle — trois angles intuitifs

`NkEulerAngle` regroupe trois `NkAngle` qui correspondent à la façon dont un humain pense une
orientation : **`pitch`** (le tangage, rotation autour de X — regarder en haut/en bas),
**`yaw`** (le lacet, autour de Y — regarder à gauche/à droite) et **`roll`** (le roulis,
autour de Z — incliner la tête). On le construit par `{ pitch, yaw, roll }` et on le compare
par `==`. Il se convertit en rotation via `NkQuat(euler)` ou `NkMat4::Rotation(euler)`.

Son **usage** naturel, ce sont les orientations manipulées par un humain — les champs d'un
éditeur, les contrôles d'une caméra. Mais dès qu'on compose des rotations en continu, il
ramène le gimbal lock : on repasse alors au quaternion.

### Exemple

```cpp
#include "NKMath/NkQuat.h"
using namespace nkentseu::math;

NkQuat r = NkQuat({ 0.f, 1.f, 0.f }, NkAngle(90.f));  // 90° autour de Y
NkVec3 forward = r * NkVec3{ 0.f, 0.f, 1.f };          // applique la rotation
NkQuat look = NkQuat::LookAt(/* direction vers la cible */);

NkQuat a = /* ... */, b = /* ... */;
NkQuat mid  = a.Slerp(b, 0.5f);   // orientation à mi-chemin, lisse
NkMat4 rot  = mid.ToMat4();        // pour le rendu
```

---

[← Les matrices](Matrices.md) · [Index NKMath](README.md) · [Les fonctions →](Functions.md)
