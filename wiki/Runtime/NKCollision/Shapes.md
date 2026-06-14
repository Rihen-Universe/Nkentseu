# Les formes de collision

> Couche **Runtime** · NKCollision · Les **primitives géométriques** de la détection de
> collision : boîtes alignées (`AABB2D`/`AABB3D`), boîtes orientées (`OBB2D`/`OBB3D`), sphères et
> cercles, capsules, enveloppes convexes, triangles et maillages, terrains, et le wrapper
> polymorphe `CollisionShape` qui les unifie sans héritage.

Détecter une collision, c'est d'abord **décrire des formes**. Avant de savoir si deux objets se
touchent, il faut un vocabulaire géométrique : un personnage est une **capsule**, une caisse une
**boîte orientée**, une planète une **sphère**, un terrain un **champ de hauteurs**, un rocher une
**enveloppe convexe**. NKCollision fournit exactement ce vocabulaire — une bibliothèque de
primitives, en 2D et en 3D, conçues autour de deux algorithmes maîtres : **GJK** (qui n'a besoin
que d'une fonction « point extrême dans une direction », la *fonction de support*) et **SAT** (qui
projette une forme sur un axe et compare des intervalles). Cette page vous apprend à choisir et à
manipuler ces formes.

Toutes les formes partagent une même philosophie. Chacune offre une **fonction `support(dir)`** —
le coin, le sommet ou le point le plus loin dans une direction donnée — car c'est tout ce dont GJK
a besoin pour tester l'intersection de n'importe quelle paire de convexes. Les formes-boîtes
ajoutent une **`project(axis)`** qui renvoie un `Interval` (la « zone d'ombre » de la forme sur un
axe), brique de base du test SAT. Et presque toutes savent se réduire à une **`toAABB()`** : une
boîte englobante grossière mais ultra-rapide, l'outil de la *broadphase* (le premier filtre, où
l'on écarte en masse les paires qui ne peuvent pas se toucher).

- **Namespace** : `col`
- **Header** : `#include "NKCollision/NkShapes.h"`

> **Avertissement d'état.** NKCollision est un **module greenfield, non livré**. Trois réserves
> importantes : (1) il utilise la **STL directement** (`std::vector`, `std::array`,
> `std::shared_ptr`, `<immintrin.h>`), ce qui n'est pas aligné sur la politique zéro-STL du moteur ;
> (2) plusieurs fonctions clés sont **seulement déclarées** (corps dans un `.cpp` non fourni) :
> `Triangle::closestPoint`, et `CollisionShape::worldAABB/support3D/support2D` ; (3) surtout,
> `NkShapes.h` **ne compile pas seul** : il dépend de types math (`Vec2`, `Vec3`, `Mat3`, `Quat`,
> `Transform`, `Ray`, `Interval`) et de constantes (`kEpsilon`, `kPi`, `kInfinity`) qui devraient
> venir de `NkMath.h` — mais **ce header est intégralement commenté**, donc ces types **ne sont
> fournis nulle part**. Tout ce qui suit décrit l'API telle qu'écrite, en supposant ce socle math
> activé.

---

## Les boîtes alignées aux axes : `AABB2D` et `AABB3D`

C'est la forme la plus simple et la plus utilisée — non pas pour décrire finement un objet, mais
pour l'**englober grossièrement**. Une *axis-aligned bounding box* est définie par deux coins,
`min` et `max`, et ses côtés restent toujours parallèles aux axes du monde. Cette contrainte la rend
presque gratuite à tester : deux AABB se chevauchent si et seulement si leurs intervalles se
chevauchent sur **chaque** axe. C'est pour cela qu'elle est la reine de la *broadphase*.

Le détail le plus important est le **constructeur par défaut** : il initialise `min` à `+kInfinity`
et `max` à `-kInfinity`. La boîte est donc « inversée » (vide) — délibérément. Cet état est le point
de départ idéal pour la construire **incrémentalement** : on appelle `expand(point)` pour chaque
sommet, et la boîte grandit pour tous les contenir, sans cas particulier pour le premier.

```cpp
AABB3D box;                       // vide : min = +inf, max = -inf
for (const Vec3& v : meshVertices)
    box.expand(v);                // grandit pour englober chaque sommet
// box contient maintenant tout le maillage
```

Une fois construite, la boîte se mesure (`center`, `extents`, `size`, `volume`/`area`,
`surfaceArea`/`perimeter`), se teste (`contains`, `overlaps`, `overlapArea`) et se gonfle
(`fatten(margin)` — utile pour ajouter une marge de tolérance, ou un *speculative margin* qui
anticipe le mouvement de la frame). L'`AABB3D` ajoute deux outils que la 2D n'a pas : `raycast`,
un test rayon-boîte par la méthode des *slabs* (qui écrit le `tHit` par référence et renvoie `true`
si le rayon touche), et `project(axis)`, la demi-extension projetée sur un axe pour SAT. Enfin,
`merge(a, b)` (statique) renvoie l'union de deux boîtes — l'opération de base d'un arbre englobant
(BVH).

Ce n'est **pas** une forme pour la *narrowphase* précise : une AABB ne tourne pas, donc un objet
oblique tient mal dedans (beaucoup de vide). Pour la précision, on passe à l'OBB ou aux convexes.

> **En résumé.** `AABB2D`/`AABB3D` = boîte définie par `min`/`max`, alignée aux axes, test de
> chevauchement par intervalle (très rapide). Constructeur par défaut **vide inversée** → construire
> par `expand`. C'est la forme de **broadphase** et de BVH (`merge`), pas de précision. La 3D ajoute
> `raycast` (slabs) et `project` (SAT).

---

## Les boîtes orientées : `OBB2D` et `OBB3D`

Là où l'AABB est prisonnière des axes du monde, l'*oriented bounding box* peut **tourner**. Elle se
définit par un `center`, un jeu d'**axes locaux** unitaires (deux en 2D, trois en 3D) et des
`halfExtents` (les demi-tailles le long de chaque axe). C'est l'enveloppe naturelle d'une caisse,
d'une porte, d'une plate-forme inclinée — tout objet rectangulaire qui ne reste pas aligné au monde.

La 2D se construit par un angle (`OBB2D(center, halfExtents, angle)` calcule les axes via
`cosf`/`sinf`), la 3D par une **matrice de rotation** (`OBB3D(c, he, Mat3)`, dont les axes sont les
colonnes) ou par un **quaternion** (`OBB3D(c, he, Quat)`, converti en `Mat3`). Les deux offrent la
fonction de support GJK (`support(dir)`, qui projette la direction dans le repère orienté),
`toAABB()` (l'AABB englobante, somme des projections absolues des axes × demi-extensions) et
`project(axis)` qui renvoie l'`Interval {centre−rayon, centre+rayon}` pour SAT.

L'OBB est le terrain de jeu naturel du **SAT** : tester deux OBB revient à projeter les deux sur un
petit nombre d'axes candidats (les axes des deux boîtes, et leurs produits croisés en 3D) et à
vérifier que les intervalles se recouvrent partout. `project` est précisément la brique qui fournit
ces intervalles.

> **En résumé.** `OBB2D`/`OBB3D` = boîte qui **tourne** (centre + axes locaux unitaires +
> demi-extensions). Construction par angle (2D) ou `Mat3`/`Quat` (3D). Donne `support` (GJK),
> `project`→`Interval` (SAT) et `toAABB` (broadphase). Plus précise que l'AABB, plus chère à tester.

---

## Les sphères et les cercles : `Sphere` et `Circle2D`

La sphère est la forme **la moins chère du monde** en collision : un `center` et un `radius`
suffisent, et tester deux sphères, c'est comparer une distance à une somme de rayons — sans même de
racine carrée, en comparant les **carrés**. C'est la forme de prédilection des projectiles, des
zones d'effet, des tests de portée, et le nœud terminal idéal d'un arbre englobant.

`Sphere` (3D) est la plus complète : `support`, `toAABB`, `contains(point)` (comparaison
`lengthSq ≤ radius²`, donc sans `sqrt`), `overlaps(autre)` (distance² ≤ (r+r')²), plus le `volume`
(`4/3·π·r³`) et la `surfaceArea` (`4·π·r²`). Son équivalent 2D, `Circle2D`, est volontairement plus
maigre : `support`, `toAABB` et `overlaps` seulement — **pas** de `contains`, ni d'`area`/`volume`.
Si vous avez besoin de tester l'appartenance d'un point à un cercle, il faudra le faire à la main
(la dissymétrie est à connaître).

```cpp
Sphere bullet{ pos, 0.05f };
if (bullet.overlaps(Sphere{ enemyPos, enemyRadius }))   // distance² ≤ (r+r')², pas de sqrt
    hit(enemy);
```

> **En résumé.** `Sphere`/`Circle2D` = `center` + `radius`, le test le moins cher (carrés, pas de
> `sqrt`). `Sphere` est riche (`contains`, `volume`, `surfaceArea`) ; `Circle2D` n'a que `support`,
> `toAABB`, `overlaps`. Idéales pour portée, projectiles, feuilles de BVH.

---

## Les capsules : `Capsule3D` et `Capsule2D`

Une capsule est une **sphère balayée le long d'un segment** : deux extrémités `a` et `b`, et un
`radius`. Géométriquement, c'est un cylindre coiffé de deux demi-sphères. C'est **la** forme du
personnage en jeu vidéo : elle monte les marches en douceur (pas de coins qui accrochent), glisse
le long des murs, et reste peu coûteuse à tester (tout se ramène à une distance point-segment ou
segment-segment).

`Capsule3D` offre la fonction de support (qui choisit l'extrémité selon `dir·(b−a)` puis ajoute
`dir.normalized()·radius`), `toAABB` (l'AABB du segment gonflée de `radius`), et un jeu de mesures :
`length`, `center`, `dir`, et surtout `closestPoint(p)` — le point le plus proche **sur le
segment** `[a, b]`, paramètre clampé `[0, 1]`. Cette dernière est le cœur de tout test impliquant
une capsule. À noter : `Capsule3D::closestPoint` possède une **garde de dégénérescence** (si la
longueur² du segment est inférieure à `kEpsilon²`, elle renvoie `a`, traitant la capsule comme un
point).

`Capsule2D` est l'analogue plan, avec les mêmes `support`, `toAABB` et `closestPoint`. Mais
**attention** : sa version de `closestPoint` n'a **pas** la garde anti-dégénérescence de la 3D —
elle divise par `ab.lengthSq()` sans test `kEpsilon`. Si `a == b` (capsule réduite à un point),
le résultat est **NaN**. À utiliser en s'assurant que le segment a une longueur non nulle.

> **En résumé.** `Capsule3D`/`Capsule2D` = segment `[a,b]` + `radius`, la forme des personnages.
> `closestPoint(p)` (point le plus proche sur le segment, `t` clampé) est la primitive maîtresse.
> Piège : la 3D protège le cas `a==b`, la **2D ne le fait pas** → NaN si segment dégénéré.

---

## Les convexes arbitraires : `ConvexHull3D` et `ConvexPolygon2D`

Quand aucune primitive simple ne suffit (un rocher, un débris, une pièce mécanique), on décrit
l'objet par son **enveloppe convexe** : une liste de sommets, et, pour la 3D, les faces et arêtes
qui les relient. GJK sait tester n'importe quel convexe pourvu qu'il fournisse un `support` — et
c'est exactement ce que ces deux structures donnent.

`ConvexHull3D` contient un `std::vector<Vec3> vertices`, des `Face` (chacune une liste d'indices CCW
vue de l'extérieur, plus une `normal`), des `Edge` (paires d'indices) et un `aabb` en cache. Ses
méthodes : `buildAABB()` (recalcule la boîte depuis tous les sommets, `O(n)`), `support(dir)` (un
**balayage linéaire `O(n)`** de tous les sommets — le code note lui-même qu'on pourrait l'améliorer
par *hill-climbing*), et `transformed(t)` qui renvoie une copie déplacée par un `Transform`.
**Subtilité à connaître** : `transformed` applique `transformPoint` aux sommets mais **recopie les
normales des faces telles quelles**, sans les re-transformer — elles seront fausses après une
rotation (à corriger si vous comptez dessus).

`ConvexPolygon2D` est l'analogue plan : un `std::vector<Vec2> vertices` en ordre CCW et un `aabb`. Il
offre `buildAABB()`, `support(dir)` (linéaire `O(n)`), et `normals()` — les normales d'arêtes pour
SAT, obtenues par `edge.perp().normalized()` sur chaque arête (`(i+1)%n`), avec une allocation
(`reserve(n)`).

> **En résumé.** `ConvexHull3D`/`ConvexPolygon2D` = convexes décrits par leurs sommets, testables
> par GJK via `support` (balayage `O(n)`). Cache `aabb` rebâti par `buildAABB`. La 2D donne ses
> `normals()` (axes SAT). Piège : `ConvexHull3D::transformed` ne re-transforme **pas** les normales
> de faces.

---

## Les triangles et maillages : `Triangle` et `TriangleMesh`

Pour la **géométrie statique** d'un niveau — sols, murs, escaliers, mobilier non convexe — on ne
décrit pas une forme convexe unique mais une **soupe de triangles**. `Triangle` est l'unité : trois
sommets `v[3]`, avec `normal()` (le produit vectoriel de deux arêtes, normalisé), `centroid()` et
`aabb()`. Sa méthode `closestPoint(p)` (point le plus proche d'un point sur le triangle) est
**déclarée mais non définie** dans le header — son corps est attendu dans un `.cpp` non fourni.

`TriangleMesh` agrège ces triangles façon GPU : un `std::vector<Vec3> vertices`, un
`std::vector<uint32_t> indices` (par triplets) et un `aabb`. On y lit `triangleCount()`
(`indices.size()/3`) et `getTriangle(i)` qui reconstruit le i-ème triangle — **sans contrôle de
borne** (accès direct, à vous de respecter `triangleCount()`). `buildAABB()` recalcule la boîte
globale. Le header note explicitement que le **BVH** sur les triangles est « construit séparément » :
le maillage ne porte donc **pas** sa structure d'accélération, il n'est que la donnée brute.

> **En résumé.** `Triangle` (3 sommets, `normal`/`centroid`/`aabb`, `closestPoint` **non défini**)
> et `TriangleMesh` (sommets + indices, `getTriangle` sans borne-check) décrivent la géométrie
> **statique** d'un niveau. Pas de BVH intégré (à bâtir à part).

---

## Le terrain : `Heightfield`

Un terrain n'est pas une soupe de triangles arbitraire mais une **grille régulière de hauteurs** —
et cette régularité permet des requêtes bien plus rapides. `Heightfield` stocke un
`std::vector<float> heights` (en *row-major* `[z][x]`), les dimensions `cols`/`rows`, une `origin`,
les tailles de cellule `cellSizeX`/`cellSizeZ`, et les bornes `minHeight`/`maxHeight`.

Deux requêtes de hauteur : `getHeight(x, z)` lit la grille par indices, avec **clamp** aux bornes
`[0, cols-1]`/`[0, rows-1]` (jamais hors-tableau) ; `heightAt(wx, wz)` interpole **bilinéairement**
la hauteur en coordonnées **monde** (calcul de la cellule, des fractions, des quatre coins) — c'est
celle qu'on appelle pour poser un personnage sur le sol. Un **piège** : `heightAt` tronque la
cellule via `(int)lx`, qui arrondit vers zéro, donc le comportement pour des coordonnées **avant**
l'origine (cellule négative) n'est pas garanti correct. Enfin `toAABB()` donne la boîte du terrain
(de l'origine au coin opposé, hauteur = `maxHeight − minHeight`).

> **En résumé.** `Heightfield` = grille `[z][x]` de hauteurs + métriques de cellule. `getHeight`
> (indexé, clampé) et `heightAt` (monde, **bilinéaire**) pour poser les objets au sol ; `toAABB`
> pour la broadphase. Piège : `heightAt` douteux pour des coordonnées avant l'origine.

---

## Le wrapper polymorphe : `CollisionShape`

On a maintenant une douzaine de formes — mais un système de collision veut les manipuler **de façon
uniforme**, sans savoir laquelle il tient. `CollisionShape` résout ça **sans héritage ni vtable** :
c'est un **tag-union** (une étiquette `ShapeType` + une union des formes). Les formes triviales
(cercle, AABB, OBB, capsule, sphère) sont stockées **inline** dans l'union ; les formes lourdes
(polygone convexe, enveloppe, maillage, terrain) vivent **sur le tas** via `std::shared_ptr`, hors
de l'union, pour ne pas la faire grossir.

On ne construit jamais une `CollisionShape` à la main : on passe par les **fabriques statiques**,
qui posent l'étiquette et le membre actif au bon endroit. `makeSphere`, `makeCircle2D`,
`makeAABB3D`, `makeAABB2D`, `makeOBB3D`, `makeCapsule3D` (placement-new dans l'union) ;
`makeConvexHull`, `makeTriangleMesh`, `makeHeightfield` (qui **prennent possession** du `shared_ptr`
par déplacement).

```cpp
auto player = CollisionShape::makeCapsule3D(feet, head, 0.4f);
auto ground = CollisionShape::makeHeightfield(terrainData);   // prend possession

if (player.is2D()) { /* … */ }                                // discrimine 2D vs 3D
```

Côté requêtes uniformes, `is2D()` est **défini inline** (vrai si le type est une forme 2D, `O(1)`).
Les trois opérations centrales — `worldAABB(t)` (AABB monde pour broadphase), `support3D(dir, t)` et
`support2D(dir)` (les supports GJK dispatchés par `type`, en repère monde) — sont **déclarées mais
non définies** dans le header : leur corps est dans un `.cpp` non fourni.

> **En résumé.** `CollisionShape` = wrapper **tag-union** (sans vtable) qui unifie toutes les formes.
> On le crée par **fabriques** (`make…`), on l'interroge par `is2D` (défini) et `worldAABB` /
> `support3D` / `support2D` (déclarés, corps en `.cpp`). Pièges importants : pas de fabrique pour
> `OBB2D`/`Capsule2D`/`ConvexPolygon2D`/`Compound` ; lire le mauvais membre de l'union est un UB.

---

## Aperçu de l'API

Tous les éléments publics du header `NkShapes.h` (namespace `col`). Complexités entre crochets quand
elles éclairent l'usage. Les méthodes **déclarées non définies** dans le header sont marquées (†).

### Tag et énumération

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `ShapeType` (uint8_t) | Étiquette : 2D `Circle2D`,`AABB2D`,`OBB2D`,`Capsule2D`,`Polygon2D` ; 3D `Sphere`,`AABB3D`,`OBB3D`,`Capsule3D`,`ConvexHull`,`TriangleMesh`,`Heightfield`,`Compound` ; sentinelle `Count`. `Polygon2D`/`Compound` n'ont **aucun** membre ni fabrique. |

### `AABB2D` — boîte alignée 2D

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `min`, `max` (`Vec2`) | Les deux coins. |
| Construction | `AABB2D()` | Vide inversée (`min=+inf`, `max=-inf`). |
| Construction | `AABB2D(min, max)`, `AABB2D(center, halfW, halfH)` | Par coins / par centre + demi-tailles. |
| Accès | `center`,`extents`,`size`,`area`,`perimeter`,`valid` `[O(1)]` | Mesures de la boîte. |
| Mutateur | `expand(p)`,`expand(o)`,`fatten(margin)` `[O(1)]` | Étendre à un point / à une boîte / gonfler. |
| Requête | `contains(p)`,`contains(o)`,`overlaps(o)`,`overlapArea(o)` `[O(1)]` | Inclusion / chevauchement / aire d'intersection. |
| Requête | `support(dir)` `[O(1)]` | Coin extrême (GJK). |
| Static | `merge(a, b)` | Union de deux boîtes. |

### `AABB3D` — boîte alignée 3D

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `min`, `max` (`Vec3`) | Les deux coins. |
| Construction | `AABB3D()`, `AABB3D(min, max)`, `static fromCenterHalfExtents(c, he)` | Vide inversée / par coins / par centre+demi-extensions. |
| Accès | `center`,`extents`,`size`,`volume`,`surfaceArea`,`valid` `[O(1)]` | Mesures. |
| Mutateur | `expand(p)`,`expand(o)`,`fatten(margin)` `[O(1)]` | Étendre / gonfler. |
| Requête | `contains(p)`,`overlaps(o)` `[O(1)]` | Inclusion / chevauchement. |
| Requête | `raycast(ray, tHit&)` `[O(1)]` | Test rayon-boîte (slabs), écrit `tHit`. |
| Requête | `support(dir)`,`project(axis)` `[O(1)]` | Coin extrême (GJK) / demi-extension projetée (SAT). |
| Static | `merge(a, b)` | Union. |

### `OBB2D` / `OBB3D` — boîtes orientées

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `center`, `axes[2]`/`axes[3]`, `halfExtents` | Centre + axes locaux unitaires + demi-tailles. |
| Construction (2D) | `OBB2D()`, `OBB2D(c, he, angle)` | Identité / par angle (`cosf`/`sinf`). |
| Construction (3D) | `OBB3D()`, `OBB3D(c, he, Mat3)`, `OBB3D(c, he, Quat)` | Identité / par matrice / par quaternion. |
| Méthode | `support(dir)`,`toAABB()`,`project(axis)` `[O(1)]` | GJK / AABB englobante / `Interval` SAT. |

### `Sphere` / `Circle2D`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `center`, `radius` | Centre + rayon. |
| Construction | `Sphere()`/`Sphere(c, r)` · `Circle2D()`/`Circle2D(c, r)` | Unité / explicite. |
| Méthode (commune) | `support(dir)`,`toAABB()`,`overlaps(autre)` `[O(1)]` | GJK / AABB / chevauchement (carrés). |
| Sphere seul | `contains(p)`,`volume()`,`surfaceArea()` `[O(1)]` | Inclusion (sans `sqrt`), volume, aire. |

### `Capsule3D` / `Capsule2D`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `a`, `b`, `radius` | Segment + rayon. |
| Construction | `Capsule3D()`/`(a,b,r)` · `Capsule2D()`/`(a,b,r)` | Défaut / explicite. |
| Méthode (commune) | `support(dir)`,`toAABB()`,`closestPoint(p)` `[O(1)]` | GJK / AABB / point le plus proche sur le segment (`t` clampé). |
| Capsule3D seul | `length()`,`center()`,`dir()` `[O(1)]` | Mesures du segment. |

### Convexes, triangles, maillages, terrain

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `ConvexHull3D` | `vertices`,`faces`(`Face{indices,normal}`),`edges`(`Edge{a,b}`),`aabb` | Données du polyèdre convexe. |
| `ConvexHull3D` | `buildAABB()` `[O(n)]`,`support(dir)` `[O(n)]`,`transformed(t)` `[O(n)]` | Rebuild AABB / GJK linéaire / copie transformée (normales non re-transformées). |
| `ConvexPolygon2D` | `vertices`(CCW),`aabb` ; `buildAABB()`,`support(dir)` `[O(n)]`,`normals()` `[O(n)]` | Polygone convexe + axes SAT. |
| `Triangle` | `v[3]` ; `normal()`,`centroid()`,`aabb()` `[O(1)]` ; `closestPoint(p)` (†) | Triangle unique. |
| `TriangleMesh` | `vertices`,`indices`,`aabb` ; `triangleCount()` `[O(1)]`,`getTriangle(i)` `[O(1)]` (sans borne-check),`buildAABB()` `[O(n)]` | Soupe de triangles statique. |
| `Heightfield` | `heights`,`cols`,`rows`,`origin`,`cellSizeX/Z`,`min/maxHeight` | Grille de terrain. |
| `Heightfield` | `getHeight(x,z)` `[O(1)]`(clampé),`heightAt(wx,wz)` `[O(1)]`(bilinéaire),`toAABB()` `[O(1)]` | Hauteur indexée / monde / boîte. |

### `CollisionShape` — wrapper tag-union

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `type`, union des formes triviales, `shared_ptr` des formes lourdes | Étiquette + stockage hybride (inline / tas). |
| Construction | `CollisionShape()` | Sphère unité par défaut. |
| Fabriques | `makeSphere`,`makeCircle2D`,`makeAABB3D`,`makeAABB2D`,`makeOBB3D`,`makeCapsule3D` | Formes triviales (placement-new). |
| Fabriques | `makeConvexHull`,`makeTriangleMesh`,`makeHeightfield` | Formes lourdes (prennent possession du `shared_ptr`). |
| Méthode | `is2D()` `[O(1)]` | Discrimine 2D vs 3D (défini inline). |
| Méthode | `worldAABB(t)` (†),`support3D(dir, t)` (†),`support2D(dir)` (†) | AABB monde / supports GJK dispatchés par `type`. |

---

## Référence complète

### Les fonctions transverses : `support`, `project`, `toAABB`

Avant de détailler chaque forme, il faut comprendre les trois fonctions qu'elles partagent, car ce
sont elles qui font l'architecture.

**`support(dir)` — la clé de GJK.** Elle renvoie le **point de la forme le plus loin dans la
direction `dir`**. C'est, étonnamment, *tout* ce que l'algorithme **GJK** réclame pour tester
l'intersection de deux convexes : il ne « voit » jamais la forme, seulement son support. Cela
explique pourquoi des formes aussi différentes qu'une sphère, une OBB et une enveloppe arbitraire
s'intègrent au même moteur — chacune sait juste donner son point extrême.
- **Physique / collision** — l'unique brique partagée par toutes les paires de convexes. Sur sphère
  et capsule, le support est `O(1)` (formule directe) ; sur enveloppe, c'est un balayage `O(n)`.
- **GPU / threading** — comme le support est sans état, les tests GJK d'un *batch* de paires se
  parallélisent trivialement (chaque paire est indépendante).

**`project(axis)` → `Interval` — la clé de SAT.** Réservée aux boîtes (`AABB3D`, `OBB2D`, `OBB3D`),
elle renvoie l'**intervalle** que la forme occupe une fois projetée sur un axe (sa « zone d'ombre »).
Le **théorème de l'axe séparateur** dit que deux convexes sont disjoints si et seulement s'il existe
**un** axe sur lequel leurs intervalles ne se touchent pas. `project` produit ces intervalles.
- **Physique** — test OBB-OBB / polygone-polygone, où l'on essaie quelques axes candidats et où le
  premier qui sépare prouve l'absence de collision.
- **Rendu / outils** — projeter une boîte sur un axe sert aussi au *frustum culling* et aux requêtes
  d'éditeur (sélection par boîte).

**`toAABB()` — la clé de la broadphase.** Toute forme sait se réduire à une AABB grossière. C'est
l'outil du **premier filtre** : on insère les AABB dans une grille ou un arbre, et seules les paires
dont les AABB se chevauchent passent au test précis. Le couple `toAABB` (par forme) + `merge` (sur
AABB) est exactement ce qu'il faut pour bâtir et entretenir un BVH.

### `AABB2D` / `AABB3D` à fond

L'AABB n'est presque jamais la forme « vraie » d'un objet — c'est sa **doublure rapide**. Sa raison
d'être est le coût quasi nul de `overlaps` (comparaison d'intervalles axe par axe) et de `raycast`
(les *slabs* : on intersecte le rayon avec les deux plans de chaque axe et on garde l'intervalle
commun).

Le **constructeur par défaut inversé** (`min=+inf`, `max=-inf`) mérite qu'on s'y arrête : il rend la
construction incrémentale propre. On part d'une boîte vide, on `expand` chaque élément, et il n'y a
pas de cas spécial « premier point ». `fatten(margin)` gonfle la boîte — pour une marge de
tolérance, ou pour un volume *spéculatif* qui englobe la position de la frame courante **et** de la
suivante (collision continue approchée).

Cas d'usage par domaine :
- **Physique** — broadphase : chaque corps publie son `worldAABB`, on ne teste finement que les
  paires dont les boîtes se chevauchent. `merge` construit les nœuds internes du BVH.
- **Rendu** — *frustum culling* (l'AABB d'un objet est dans le tronc de vue ?), tri front-to-back,
  calcul des bornes d'un maillage par `expand`.
- **GPU** — `raycast` AABB pré-filtre les rayons avant un test triangle coûteux (picking, ombres).
- **ECS / scène** — une AABB par entité, indexée dans une grille spatiale pour les requêtes de
  voisinage.
- **Outils / éditeur** — la « bounding box » dessinée autour d'un objet sélectionné, la sélection
  rectangulaire (boîte contre boîte via `overlaps`/`overlapArea`).
- **IA** — zones de déclenchement, volumes de patrouille testés grossièrement avant tout calcul fin.

### `OBB2D` / `OBB3D` à fond

L'OBB est le compromis entre l'AABB (rapide, imprécise) et le convexe arbitraire (précis, cher).
Elle colle de près à tout objet rectangulaire qui **tourne** : une caisse posée de travers, une
plate-forme inclinée, un véhicule. Son test naturel est le **SAT** via `project`.

Le détail de construction compte : la 2D part d'un angle (`cosf`/`sinf` montent les deux axes), la
3D d'une `Mat3` (axes = colonnes) ou d'un `Quat` (converti). `toAABB()` la « ré-aligne » en boîte
englobante (somme des projections absolues), utile pour réinjecter une OBB dans une broadphase
purement AABB.

- **Physique** — collision boîte-boîte précise (caisses empilées, véhicules), où le SAT trouve l'axe
  et la profondeur de pénétration minimale.
- **Gameplay** — *hitbox* orientée d'une arme ou d'un personnage qui pivote.
- **Outils / éditeur** — *gizmo* de sélection orienté qui épouse l'orientation locale de l'objet.
- **Rendu** — volumes englobants orientés plus serrés que l'AABB pour des objets allongés et tournés.

### `Sphere` / `Circle2D` à fond

La sphère paie le moins cher de toutes : `overlaps` et `contains` comparent des **carrés** de
distance, jamais de `sqrt`. C'est pour ça qu'on la retrouve partout où il faut beaucoup de tests
approximatifs et rapides.

La dissymétrie 2D/3D est à mémoriser : `Sphere` offre `contains`, `volume` et `surfaceArea` ;
`Circle2D` n'a que `support`, `toAABB` et `overlaps`. Pas d'`area`, pas de `contains` côté cercle.

- **Gameplay / IA** — portée d'une attaque, rayon d'aggro, zone d'effet (explosion) : `overlaps`
  contre une sphère est le test « est-ce dans le rayon ? ».
- **Physique** — la sphère est la *narrowphase* la plus simple (bille, projectile) et la feuille
  idéale d'une *bounding sphere hierarchy*.
- **Audio** — atténuation par distance : `overlaps`/distance² entre l'auditeur et une source pour
  savoir si elle est audible, sans racine.
- **Rendu** — *bounding sphere* d'un objet pour un *culling* invariant par rotation ; portée d'une
  lumière ponctuelle.
- **2D** — `Circle2D` pour les pastilles de collision (balles, particules, *power-ups*).

### Capsules à fond

La capsule est la forme du **corps mobile**. Son intérêt physique : pas d'arête vive, donc elle
glisse sur les murs et franchit les marches sans accrocher, tout en restant peu coûteuse. Tout
revient à `closestPoint` — le point le plus proche sur le **segment** `[a, b]` —, dont on déduit
ensuite la distance à un point, à un autre segment, à un triangle.

Le piège de fidélité est crucial : `Capsule3D::closestPoint` **protège** le segment dégénéré (si
`len² < kEpsilon²` elle renvoie `a`), mais `Capsule2D::closestPoint` **ne le fait pas** et divise
par `ab.lengthSq()` sans garde → **NaN** si `a == b`. En 2D, garantissez un segment de longueur non
nulle, ou ajoutez le test vous-même.

- **Gameplay / physique** — le *character controller* : capsule du joueur testée contre le monde,
  réponse calculée à partir de `closestPoint`.
- **Animation** — capsules sur les os (bras, jambes) pour la collision du corps et le *ragdoll*.
- **IA** — volume du personnage pour l'évitement d'obstacles et le *navmesh*.
- **Outils / éditeur** — édition visuelle d'une capsule de *collider* (deux poignées + rayon).

### Convexes arbitraires à fond

Quand l'objet n'est ni boîte ni sphère, on le décrit par son enveloppe convexe. La force de cette
représentation est de rester **GJK-compatible** : `support(dir)` (balayage `O(n)`) suffit. Le coût
linéaire est acceptable pour des enveloppes de quelques dizaines de sommets ; le code note qu'un
*hill-climbing* (sauter de voisin en voisin le long des arêtes) le ramènerait quasi à `O(1)`.

Le piège de `ConvexHull3D::transformed` est important : il transforme les **sommets** mais recopie
les **normales de faces** telles quelles. Après une rotation, ces normales sont fausses — si votre
test s'en sert (SAT par faces, *contact manifold*), recalculez-les. La 2D, elle, fournit `normals()`
à la demande (donc toujours correctes pour la pose courante des sommets).

- **Physique** — débris, rochers, pièces mécaniques : collision précise via GJK + EPA (profondeur),
  ou SAT en 2D à partir de `normals()`.
- **Outils** — enveloppe convexe générée automatiquement comme *collider* d'un maillage de rendu.
- **Gameplay 2D** — plateformes et obstacles polygonaux non rectangulaires.

### Triangles, maillages, terrain à fond

Pour le **décor statique**, on ne convexifie pas : on garde la géométrie telle quelle, en triangles.
`Triangle` est l'unité (`normal`, `centroid`, `aabb` immédiats ; `closestPoint` **non défini** dans
le header). `TriangleMesh` est la collection brute (sommets + indices), avec `getTriangle(i)` **sans
contrôle de borne** — respectez `triangleCount()`. Le maillage **ne porte pas de BVH** (« construit
séparément ») : pour interroger un gros niveau efficacement, il faut une structure d'accélération
externe par-dessus.

`Heightfield` est le cas particulier du **terrain** : une grille régulière, donc des requêtes en
`O(1)` au lieu de parcourir des triangles. `getHeight(x, z)` lit la grille (clampée), `heightAt(wx,
wz)` interpole bilinéairement en monde — c'est elle qui « pose » personnages et objets sur le sol.
Attention : `heightAt` tronque la cellule par `(int)` (arrondi vers zéro), donc son résultat est
douteux pour des coordonnées **avant** l'origine.

- **Physique** — collision du monde statique : rayons et capsules testés contre les triangles
  (filtrés par un BVH externe) ; le terrain via `heightAt` pour la marche au sol.
- **Gameplay / IA** — hauteur du sol sous un agent, génération de *navmesh* à partir du maillage.
- **Rendu** — le même maillage sert souvent de source géométrique ; `aabb` pour le *culling*.
- **Outils / éditeur** — sculpture de terrain (édition de `heights`), import de *collision mesh*.

### `CollisionShape` à fond

`CollisionShape` est la pièce d'**unification** : elle permet à un système (broadphase, narrowphase,
requêtes de rayon) de manipuler n'importe quelle forme sans connaître son type concret, et **sans
coût de vtable**. Le choix d'un tag-union plutôt que de l'héritage est délibéré : les formes
triviales tiennent inline (pas d'allocation, *cache-friendly*), seules les lourdes vont sur le tas
via `shared_ptr` (et se partagent — un même maillage de niveau référencé par mille colliders).

L'usage discipliné passe **exclusivement par les fabriques** : elles posent l'étiquette `type` et
construisent le bon membre. À l'inverse, lire un membre d'union qui ne correspond pas à `type` est un
**comportement indéfini** — il n'y a aucune validation runtime. `is2D()` (le seul défini inline)
permet d'aiguiller 2D vs 3D ; `worldAABB`, `support3D` et `support2D` (déclarés, corps en `.cpp`)
sont les points d'entrée que la broadphase et GJK appelleront en repère monde.

Limites à connaître : il n'existe **pas** de fabrique pour `OBB2D`, `Capsule2D`, `ConvexPolygon2D`
ni `Compound`, bien que `polygon2D` soit un membre — ces formes existent isolément mais ne sont pas
encore intégrables au wrapper. Enfin, l'union mêle des types non triviaux (les formes ont des
constructeurs) et des `shared_ptr` hors union sans destructeur/copie utilisateur déclaré : la
sémantique de copie et de destruction repose donc sur la trivialité des types stockés et sur le
comptage de références des pointeurs — à confirmer dans le `.cpp`, et à n'utiliser qu'à travers les
fabriques.

- **Physique** — le type de *collider* attaché à un corps, transporté de façon uniforme dans tout le
  pipeline (broadphase → GJK → résolution).
- **ECS** — un composant `Collider` portant une `CollisionShape` par entité.
- **Réseau / IO** — sérialisation d'un collider : on lit `type`, puis le membre correspondant (les
  formes lourdes restent à sauvegarder via leur `shared_ptr`).
- **Outils / éditeur** — édition unifiée du collider d'un objet quel que soit son type.

### Les types math dont tout dépend (prévus, non livrés)

Toutes les méthodes ci-dessus reposent sur des types et constantes (`Vec2`, `Vec3`, `Vec4`, `Mat3`,
`Mat4`, `Quat`, `Transform`, `Ray`, `Ray2D`, `Interval`, `kEpsilon`, `kPi`, `kInfinity`) qui
**devraient** venir de `NkMath.h`. **Mais ce header est intégralement commenté** : il inclut
`<cmath>`/`<cassert>`/`<algorithm>`/`<immintrin.h>`, porte un commentaire `// trouver comment
utiliser NKMath`, et laisse tout le reste en commentaire de ligne. **Aucune de ces entités n'est
donc réellement déclarée**, et `NkShapes.h` ne peut pas compiler seul tant que ce socle n'est pas
activé (ou remplacé par les types du module NKMath du moteur). Le commentaire liste précisément
l'API math attendue — vecteurs avec `dot`/`cross`/`lengthSq`/`normalized`/`perp`, `Transform` avec
`transformPoint`, `Ray` avec `invDir()`/`at(t)`, `Interval` avec `overlaps`/`overlap` — ce qui
confirme qu'il s'agit bien du header destiné à fournir le socle géométrique de NKCollision. Tant
qu'il reste commenté, considérez NKCollision comme une **spécification de formes**, pas un module
compilable en l'état.

---

### Exemple récapitulatif

```cpp
#include "NKCollision/NkShapes.h"
using namespace col;

// Broadphase : englober un maillage, puis tester deux boîtes.
AABB3D box;                                   // vide inversée
for (const Vec3& v : meshVertices) box.expand(v);
AABB3D other = AABB3D::fromCenterHalfExtents({ 5,0,0 }, { 1,1,1 });
bool maybeTouch = box.overlaps(other);        // filtre rapide, sans test précis

// Narrowphase simple : deux sphères, sans racine carrée.
Sphere bullet{ pos, 0.05f };
if (bullet.overlaps(Sphere{ enemyPos, enemyRadius })) hit(enemy);

// Le character controller : une capsule, point le plus proche sur le segment.
Capsule3D body{ feet, head, 0.4f };
Vec3 onAxis = body.closestPoint(contactPoint);   // (protégé contre a==b en 3D)

// Wrapper unifié : on crée par fabrique, on aiguille par is2D().
auto playerShape = CollisionShape::makeCapsule3D(feet, head, 0.4f);
auto groundShape = CollisionShape::makeHeightfield(terrainData);  // prend possession
if (!playerShape.is2D()) { /* pipeline 3D : worldAABB(t), support3D(dir, t)… */ }

// Terrain : poser un objet sur le sol par interpolation bilinéaire.
float y = terrainData->heightAt(worldX, worldZ);
```

---

[← Index NKCollision](README.md) · [Récap NKCollision](../NKCollision.md) · [Couche Runtime](../README.md)
