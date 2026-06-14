# Les vecteurs

Le vecteur est l'objet de base de toute la géométrie du moteur : une position, une
direction, une vitesse, une couleur même, tout finit par s'exprimer en vecteurs. NKMath en
fournit en deux, trois et quatre dimensions, sous forme de types **templatés**
(`NkVec2T<T>`, `NkVec3T<T>`, `NkVec4T<T>`) — mais en pratique, on manipule presque toujours
leurs **alias** prêts à l'emploi.

```cpp
NkVec3  a{ 1.f, 0.f, 0.f };   // float par défaut
NkVec2i pixel{ 100, 200 };     // version entière
```

`NkVec2`/`NkVec3`/`NkVec4` sont en flottants ; quand le type compte, on prend `NkVec2f`
(float), `NkVec2i` (entier), `NkVec2u` (entier non signé) ou `NkVec2d` (double). Les
composantes s'atteignent par `.x`, `.y`, `.z`, `.w`.

---

## Calculer avec les vecteurs

Les opérateurs arithmétiques font ce qu'on attend, composante par composante : `a + b`,
`a - b`, et la multiplication par un scalaire `a * 2.f` pour mettre à l'échelle. Une
subtilité à connaître : `a * b` entre deux vecteurs est le produit **composante par
composante** (Hadamard), pas un produit scalaire — celui-ci a son propre nom.

Justement, les deux produits fondamentaux de la géométrie ont chacun leur méthode. Le
**produit scalaire**, `Dot`, mesure à quel point deux vecteurs pointent dans la même
direction (et vaut zéro s'ils sont perpendiculaires). Le **produit vectoriel**, `Cross`
(réservé à `NkVec3`), donne un vecteur perpendiculaire aux deux — la base, par exemple, du
calcul d'une normale :

```cpp
NkVec3 a{ 1.f, 0.f, 0.f }, b{ 0.f, 1.f, 0.f };
float32 d = a.Dot(b);     // 0 : ils sont perpendiculaires
NkVec3  n = a.Cross(b);   // (0, 0, 1) : perpendiculaire au plan de a et b
```

Pour la longueur, deux variantes : `Len()` donne la norme, mais `LenSq()` donne son carré —
et il faut prendre le réflexe de préférer le carré dès qu'on **compare** des distances, car
il évite une racine carrée coûteuse (comparer les carrés revient au même que comparer les
longueurs). `Distance(other)` mesure l'écart entre deux points.

---

## Normaliser et interpoler

Normaliser un vecteur, c'est le ramener à une longueur de 1 tout en gardant sa direction —
une opération omniprésente dès qu'on raisonne en directions. Attention à la nuance entre les
deux formes : `Normalize()` modifie le vecteur **en place**, tandis que `Normalized()`
laisse l'original intact et renvoie une **copie** normalisée.

```cpp
NkVec3 dir = (cible - position).Normalized();   // direction de 'position' vers 'cible'
```

`Lerp(to, t)` interpole linéairement entre deux vecteurs (utile pour des transitions de
position), et le module fournit une variante « lerp puis normalisation » spécifiquement
pensée pour interpoler des **directions** sans qu'elles raccourcissent en chemin. Deux
dernières méthodes complètent l'outillage géométrique : `Project(onto)` projette le vecteur
sur un autre, et `Reflect(normal)` calcule sa réflexion par rapport à une surface — exactement
ce qu'il faut pour faire rebondir une balle sur un mur.

> **En résumé.** Travaillez avec les alias (`NkVec3`, `NkVec2f`, `NkVec2i`…). `Dot` pour
> l'alignement, `Cross` pour la perpendiculaire (3D), `LengthSquared` plutôt que `Length`
> pour comparer des distances, `Normalized()` (copie) ou `Normalize()` (en place) selon que
> vous voulez préserver l'original. `a * b` est un produit composante par composante, pas un
> produit scalaire.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (formule,
cas d'usage) dans la « Référence complète » qui suit.

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `.x` `.y` `.z` `.w` | Composantes (accès nommé). |
| Accès | `operator[](i)` | Accès indexé (0 = x…). |
| Accès | `operator T*()` | Vue tableau brut (interop GPU). |
| Accès | `operator NkVecNT<U>()` | Conversion de type de composante. |
| Construction | `NkVecNT{ x, y, … }` | Construit par composantes. |
| Construction | `static Zero()` `One()` | Vecteur nul / vecteur unité. |
| Construction | `static UnitX()` `UnitY()` `UnitZ()`(3D) | Axes du repère. |
| Arithmétique | `a + b` `a - b` | Addition / soustraction. |
| Arithmétique | `a * b` `a / b` | Produit / division composante par composante. |
| Arithmétique | `a * s` `a / s` (+ `s*a`, `a±s`) | Opérations avec un scalaire. |
| Arithmétique | `+=` `-=` `*=` `/=` | Versions mutables. |
| Arithmétique | `-a` · `++` `--` · `==` `!=` | Négation, incrément, comparaison. |
| Produits | `Dot(b)` | Produit scalaire (alignement). |
| Produits | `Cross(b)` (3D) | Produit vectoriel (perpendiculaire). |
| Normes | `Len()` `LenSq()` | Norme et son carré. |
| Normes | `Distance(b)` | Distance entre deux points. |
| Normes | `Collinear(b)` | Test de colinéarité. |
| Direction | `Normalize()` `Normalized()` | Normalisation (en place / copie). |
| Direction | `IsUnit()` | La norme vaut-elle déjà 1 ? |
| Direction | `Normal()` (2D) | Perpendiculaire normalisé. |
| Direction | `Rotate90/180/270()` (2D) | Rotations par multiples de 90°. |
| Direction | `Project(onto)` `Reject(onto)` | Projection / composante orthogonale. |
| Direction | `Reflect(normal)` | Réflexion (rebond). |
| Interpolation | `Lerp` `NLerp` `SLerp` | Interpolations linéaire / normalisée / sphérique. |
| Alias | `NkVec2/3/4` · `…f` `…i` `…u` `…d` | Versions par type de composante. |

---

## Référence complète

Chaque élément est repris ici en détail. Les éléments triviaux (construction, opérateurs)
sont décrits brièvement ; les opérations géométriques le sont **à fond**, avec leurs usages
dans les différents domaines du temps réel — rendu, physique, animation, gameplay, 2D.

### Construction, accès, arithmétique

Un vecteur se construit par ses composantes (`NkVec3{ x, y, z }`), qu'on relit par
`.x`/`.y`/`.z`/`.w` ou par `operator[]`. `operator T*()` en donne une vue tableau brut —
pratique pour pousser un vecteur dans un uniform GPU qui attend un `float*` —, et
`operator NkVecNT<U>()` convertit le type des composantes (passer de `NkVec2i`, des pixels
entiers, à `NkVec2f`, des coordonnées flottantes). Les statics fournissent les vecteurs de
référence : `Zero()` sert souvent d'accumulateur de départ (une somme de forces), `One()` de
facteur neutre, et `UnitX/Y/Z()` désignent les axes du repère.

Côté arithmétique, l'**addition** compose des déplacements (`position + vitesse * dt`), la
**soustraction** donne le vecteur d'un point vers un autre (`cible - origine`, qui porte à la
fois la direction *et* la distance), la **multiplication par un scalaire** met à l'échelle.
Un piège à connaître : `a * b` entre deux vecteurs est un produit **composante par
composante** (utile pour moduler une couleur par une autre), à ne **pas** confondre avec le
produit scalaire ci-dessous.

### `Dot` — le produit scalaire

Mathématiquement, `a·b = aₓbₓ + a_yb_y + a_zb_z = |a||b| cos θ`. Le résultat est un nombre qui
mesure l'**alignement** de deux vecteurs : maximal quand ils pointent dans le même sens, nul
quand ils sont perpendiculaires, négatif quand ils s'opposent. C'est sans doute l'opération la
plus utilisée de toute la 3D, et on la croise dans presque tous les domaines :

- **Éclairage** : la lumière diffuse d'une surface suit la loi de Lambert, `max(0, N·L)`
  (N = normale, L = direction vers la lumière). Le reflet spéculaire utilise `R·V` (Phong) ou
  `N·H` (Blinn-Phong).
- **Rendu** : `N·V < 0` révèle qu'une face tourne le dos à la caméra (back-face culling) ;
  proche de 0, la surface est vue de profil (effet de rim/silhouette).
- **Physique & collision** : projeter une vitesse sur la normale du contact (`v·n`) sépare ce
  qui pénètre la surface de ce qui glisse ; le **signe** de `v·n` dit si deux objets se
  rapprochent ou s'éloignent.
- **Gameplay & IA** : un garde voit-il le joueur ? On compare `regard · directionVersJoueur`
  (normalisés) à `cos(demi-champ-de-vision)` — au-dessus du seuil, la cible est dans le cône.
- **2D** : tout cela vaut aussi en 2D (angle entre deux directions, projection sur un axe — la
  base du test de collision SAT).

### `Cross` — le produit vectoriel (3D)

`a × b` renvoie un vecteur **perpendiculaire** au plan formé par `a` et `b`, de norme
`|a||b| sin θ`. Ses usages tournent autour de la perpendicularité et de l'aire :

- **Rendu** : la **normale** d'un triangle se calcule en croisant deux de ses arêtes — c'est
  le point de départ de tout l'éclairage.
- **Géométrie** : la norme du produit vaut l'**aire** du parallélogramme (donc le double de
  celle du triangle) ; son signe donne l'**orientation** (horaire / anti-horaire), utile pour
  trier des sommets ou savoir de quel côté d'une arête tombe un point.
- **Physique** : le **moment** d'une force (couple) est un produit vectoriel `r × F`.
- **Caméra** : reconstruire un repère orthonormé (`right = forward × up`) pour bâtir une
  matrice de vue.

### `Len`, `LenSq`, `Distance`, `Collinear` — longueurs et distances

`Len()` donne la norme (longueur) du vecteur, `LenSq()` son carré. Selon ce que le vecteur
représente, la longueur prend un sens : celle d'un vecteur *vitesse* est la vitesse scalaire,
celle d'une *force* son intensité, celle d'un vecteur *d'un point à un autre* leur distance.
Le **réflexe de performance** à acquérir : pour comparer des distances (portée, collision de
sphères), comparez les **carrés** (`LenSq() < r*r`) plutôt que les longueurs — on économise
une racine carrée par test, et il y en a énormément. `Distance(b)` mesure directement l'écart
entre deux points (proximité, IA, déclenchement d'événements), et `Collinear(b)` teste si deux
vecteurs sont alignés (points colinéaires, arêtes parallèles, simplification de tracés).

### `Normalize`, `Normalized`, `IsUnit` — la direction pure

Normaliser, c'est ramener un vecteur à une longueur de 1 sans changer sa direction. C'est
indispensable dès qu'on raisonne en **directions**, lesquelles n'ont pas de longueur propre :

- **Éclairage** : normales et directions de lumière doivent être unitaires pour que les
  produits scalaires (`N·L`) aient un sens.
- **Mouvement** : une direction normalisée × une vitesse donne un pas à vitesse constante
  (sans normaliser, un objet irait plus vite en diagonale).
- **Rotation** : l'axe d'une rotation doit être unitaire.

`Normalize()` modifie le vecteur sur place, `Normalized()` en renvoie une copie sans toucher
l'original, et `IsUnit()` vérifie si la norme vaut déjà 1 (pour éviter une normalisation inutile).

### `Reflect` — la réflexion

`Reflect(n)` calcule `r = v − 2(v·n)n` : le vecteur `v` réfléchi par rapport à une surface de
normale `n`. C'est, mot pour mot, le **rebond** — une balle qui ricoche sur un mur (gameplay
2D comme 3D), un rayon lumineux réfléchi en miroir (spéculaire parfait, ray tracing).

### `Project`, `Reject` — décomposer un vecteur

`Project(onto)` renvoie la part de `a` alignée **le long** de `b`, `Reject(onto)` la part
**perpendiculaire**. Ensemble, ils découpent un vecteur en deux morceaux orthogonaux — ce qui
est au cœur de la réponse à une collision : on projette la vitesse sur la normale du contact
pour séparer la composante qui « entre » dans le mur (à annuler ou faire rebondir) de celle
qui « glisse » le long (à conserver) — `Reject` donne précisément ce **glissement**. En rendu,
la projection sert aussi à décomposer un vecteur sur les axes d'un repère.

### `Normal`, `Rotate90/180/270` — utilitaires 2D

En 2D, `Normal()` donne le perpendiculaire normalisé d'un vecteur — la normale d'un segment,
pour un éclairage 2D ou un test de collision. `Rotate90/180/270()` font pivoter un vecteur par
un multiple droit de 90° **sans trigonométrie** (donc très vite) — pratique pour les grilles,
les pièces de puzzle, les directions cardinales.

### `Lerp`, `NLerp`, `SLerp` — interpoler

`Lerp(to, t)` interpole **linéairement** : à `t = 0` on est sur `a`, à `t = 1` sur `b`, entre
les deux sur une ligne droite. C'est l'outil des transitions de position, des fondus, d'une
caméra qui rattrape sa cible. Pour interpoler des **directions** plutôt que des positions, on
veut garder la longueur 1 tout du long : `NLerp` fait un lerp suivi d'une normalisation
(rapide), `SLerp` une interpolation sphérique (vitesse angulaire constante, idéale pour faire
pivoter une direction de visée en douceur).

### Alias

Les types templatés s'emploient via leurs alias : `NkVec2`/`NkVec3`/`NkVec4` (en `float32` par
défaut), et les variantes typées `…f` (float32), `…i` (int32), `…u` (uint32), `…d` (double) —
par exemple `NkVec2i` pour des coordonnées de pixels entières, `NkVec3f` pour des positions
dans le monde.

### Exemple

```cpp
#include "NKMath/NkVec.h"
using namespace nkentseu::math;

NkVec3 a{ 3.f, 4.f, 0.f };
NkVec3 b = NkVec3::UnitX();          // (1, 0, 0)

float32 len  = a.Len();              // 5  (longueur)
float32 d    = a.Dot(b);             // 3  (alignement → éclairage, culling)
NkVec3  n    = a.Cross(b);           // (0, 0, -4)  (normale)
NkVec3  unit = a.Normalized();       // (0.6, 0.8, 0)  (direction)
NkVec3  bounce = vel.Reflect(wallN); // rebond sur un mur
```

---

[Index NKMath →](../NKMath.md) · [Les matrices →](Matrices.md)
