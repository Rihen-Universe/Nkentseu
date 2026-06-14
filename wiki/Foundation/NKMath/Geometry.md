# La géométrie

Après les vecteurs viennent les formes qu'on construit avec : le rectangle, le segment,
l'intervalle. Ce sont les primitives des tests de **survol** (la souris est-elle sur ce
bouton ?), de **collision** (ces deux objets se touchent-ils ?) et de **bornage** (cette
valeur est-elle dans la plage autorisée ?).

---

## NkRectangle — le rectangle aligné

Un rectangle est défini par une position (son coin supérieur gauche) et une taille. NKMath
range ces deux informations dans une **union** astucieuse : on peut y accéder soit par
`x/y/width/height`, soit par `position`/`size` (un `NkVec2`), selon ce qui est le plus
naturel sur le moment.

Le test le plus fréquent, c'est `Contains` : ce point est-il à l'intérieur ? C'est ce qui
détecte le survol d'un bouton ou le clic dans une zone :

```cpp
NkRect2f panel{ 30.f, 30.f, 320.f, 210.f };   // x, y, w, h
if (panel.Contains(mouse.x, mouse.y)) { /* la souris est sur le panneau */ }
```

Mais le rectangle sert aussi à la **collision**. `SeparatingAxis` implémente le test SAT
(*theorème de l'axe séparateur*), la méthode de référence pour savoir si deux formes
convexes s'intersectent. `Clamp` projette un point dans les limites du rectangle (pour garder
un curseur ou un objet à l'intérieur d'une zone), `Enlarge` agrandit le rectangle pour
englober un point ou un autre rectangle (calcul d'une **boîte englobante**, une AABB), et
`Corner(i)` renvoie l'un des quatre coins.

Les alias couvrent les usages : `NkRect2i` (`NkIntRect`) pour les viewports et le clipping
en pixels entiers, `NkRect2f` (`NkFloatRect`) pour la géométrie écran flottante, `NkRect2d`
pour la double précision.

---

## NkSegment — le segment

Un segment relie deux points. `Length()` donne sa longueur (utile, par exemple, pour la
diagonale d'une boîte). Sa méthode la plus intéressante est `Project(axis)` : elle projette
le segment sur un axe et renvoie l'**intervalle** couvert — c'est précisément la brique du
test SAT évoqué plus haut (on projette les deux formes sur un axe et on regarde si leurs
intervalles se chevauchent). La fonction libre `SegmentsMayIntersect(a, b)` teste si deux
segments se croisent.

---

## NkRange — l'intervalle

Un `NkRange` représente une plage `[min, max]`. Il maintient toujours l'invariant
`min ≤ max` (même si on lui passe les bornes à l'envers), ce qui évite une classe entière de
bugs. C'est l'outil propre pour exprimer « entre telle et telle valeur » plutôt que de
trimballer deux variables séparées.

```cpp
NkRangeFloat health{ 0.f, 100.f };

bool ok      = health.Contains(hp);   // hp est-il dans la plage ?
float32 safe = health.Clamp(hp);      // ramène hp dans [0, 100]
```

`Clamp` borne une valeur, `Contains` teste l'appartenance (d'une valeur ou d'un autre
intervalle), `Overlaps` teste si deux plages se chevauchent, et `Length()` donne l'étendue
(`max − min`). On le retrouve aussi du côté de [`NkRandom`](Random.md), pour borner un tirage.

---

## Aperçu de l'API

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| **NkRectangle** | `position` (`x`,`y`) · `size` (`width`,`height`) | Données (union : nommé ou vectoriel). |
| Test | `Contains(px, py)` / `Contains(vec)` | Le point est-il dans le rectangle ? |
| Géométrie | `Center()` / `GetCenter()` · `GetCenterX/Y()` | Point central. |
| Géométrie | `Clamp(point)` · `Corner(i)` | Projeter un point dans la zone / i-ème coin. |
| Collision | `SeparatingAxis(axis)` | Test SAT (axe séparateur). |
| AABB | `Enlarge(point)` / `Enlarge(rect)` | Agrandir pour englober. |
| | `operator==` · alias `NkRect2i/f/d`, `NkIntRect`/`NkFloatRect` | Comparaison / alias. |
| **NkSegment** | `NkSegment(a, b)` · `Length()` | Segment / longueur. |
| | `Project(axis)` → `NkRangeFloat` · `==` `!=` | Projection sur un axe / comparaison. |
| | `SegmentsMayIntersect(a, b)` *(libre)* | Deux segments se croisent-ils ? |
| **NkRange** | `NkRange(min, max)` | Intervalle (réordonné, `min ≤ max`). |
| | `GetMin/GetMax` · `SetMin/SetMax` · `Length()` | Bornes / étendue. |
| Test | `Contains(value/range)` · `Overlaps(other)` · `Clamp(value)` | Appartenance / chevauchement / bornage. |
| Alias | `NkRange`/`NkRangeFloat`/`NkRangeDouble`/`NkRangeInt`/`NkRangeUInt` | Par type. |

---

## Référence complète

### NkRectangle — le rectangle aligné

Le rectangle est la forme la plus utilisée du jeu 2D, et il sert dans plusieurs domaines.
**`Contains`** teste si un point est à l'intérieur — c'est le **picking** : la souris
survole-t-elle ce bouton, le clic tombe-t-il dans cette zone, ce tir touche-t-il cette
hitbox ? **`SeparatingAxis`** implémente le test SAT (théorème de l'axe séparateur), la
référence pour savoir si deux formes convexes **entrent en collision**. **`Enlarge`** agrandit
le rectangle pour englober un point ou un autre rectangle — c'est le calcul d'une **boîte
englobante** (AABB), brique de base du *broad-phase* en physique (écarter rapidement les paires
d'objets trop éloignées pour se toucher) et du *frustum culling* en rendu. **`Clamp`** ramène
un point dans les limites (garder un curseur, un objet, une caméra dans une zone), et
`Center`/`Corner` donnent les points remarquables. Les alias `NkRect2i` (pixels entiers — UI,
viewport, clipping) et `NkRect2f` (flottant — géométrie écran) couvrent les usages.

### NkSegment — le segment

Un segment relie deux points. Sa `Length()` sert dès qu'on a besoin d'une distance le long
d'une ligne. Mais sa méthode la plus puissante est **`Project(axis)`** : elle projette le
segment sur un axe et renvoie l'**intervalle** couvert — c'est exactement la brique du test SAT
vu plus haut (on projette deux formes sur un axe et on regarde si leurs intervalles se
chevauchent). On le retrouve donc en **collision** (segment contre segment via
`SegmentsMayIntersect`, raycast contre arête), en **IA** (ligne de vue : le segment joueur-ennemi
croise-t-il un mur ?) et en **rendu** (découpage de lignes).

### NkRange — l'intervalle

`NkRange` représente une plage `[min, max]`, et maintient toujours l'invariant `min ≤ max`
(même si on lui passe les bornes à l'envers) — ce qui supprime une classe entière de bugs.
C'est l'outil propre pour exprimer « entre telle et telle valeur », et il sert dans plusieurs
domaines : **`Clamp`** borne une valeur (PV dans `[0, max]`, volume, paramètre) ; **`Contains`**
teste l'appartenance ; **`Overlaps`** détecte si deux plages se chevauchent — c'est la
collision **1D**, et le dernier maillon du test SAT (les intervalles projetés se
chevauchent-ils ?). On le croise aussi du côté de [`NkRandom`](Random.md), pour borner un
tirage, et en audio pour décrire une plage de fréquences.

> **En résumé.** `NkRectangle` pour le survol/clic (`Contains`), la collision (`SeparatingAxis`)
> et les boîtes englobantes (`Enlarge`) ; `NkSegment` pour les arêtes et la projection sur un
> axe ; `NkRange` pour exprimer et borner une plage (`Clamp`, `Contains`, `Overlaps`), avec
> l'invariant `min ≤ max` garanti.

---

[← La couleur](Color.md) · [Index NKMath](README.md) · [L'aléatoire →](Random.md)
