# Les conteneurs spécialisés

> Couche **Foundation** · NKContainers · Des structures **de domaine**, taillées pour un problème
> précis : le **graphe** `NkGraph` (relations entre choses) et les arbres de **partitionnement
> spatial** `NkOctree` (3D) / `NkQuadTree` (2D) pour répondre vite à « qu'y a-t-il autour ? ».

Les conteneurs des autres pages répondent à la question « comment **ranger** mes éléments ». Ceux-ci
répondent à une question différente, plus haut niveau : « comment **raisonner** sur mes éléments ».
Un graphe ne range pas des sommets, il range des **relations** — qui est connecté à qui. Un octree
ne range pas des objets, il range leur **position** de telle façon qu'on puisse, en un éclair,
trouver tous ceux qui tombent dans une zone donnée. Ce ne sont **pas** des collections
généralistes : on les sort quand le problème *est* un problème de connectivité ou de voisinage
spatial. Cette page vous montre quand, et surtout avec quels pièges réels du moteur.

Les trois types sont **templatés** et **conscients de l'allocateur** : la mémoire vient toujours de
NKMemory (l'allocateur par défaut `memory::NkGetDefaultAllocator()` est récupéré quand on passe
`nullptr`), jamais de `new`/`delete`. Contrairement aux conteneurs séquentiels, ils **n'exposent
PAS** la double casse STL : aucune méthode `insert`/`size`/`begin` minuscule, **seules les méthodes
`PascalCase` existent**. Aucun n'a d'itérateurs ni de sérialisation, et tous sont **thread-unsafe**.

- **Namespace** : `nkentseu`
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## Le graphe : `NkGraph`

Dès qu'on doit modéliser des **relations** — un réseau de routes entre villes, les dépendances entre
tâches d'un build, les passages entre salles d'un niveau, l'arbre de dialogue d'un PNJ — on a besoin
d'un **graphe**. `NkGraph<VertexType>` range des *sommets* (les choses) et des *arêtes* (les liens
entre elles), via une **liste d'adjacence** : à chaque sommet est associé le vecteur de ses voisins.
Le graphe peut être **dirigé** (le lien va dans un seul sens, comme une dépendance) ou **non dirigé**
(le lien est mutuel, comme une route à double sens) — on le décide au constructeur.

Le `VertexType` doit être **hashable** (`NkHash<T>`) et **comparable** (`operator==`) : ce sont les
deux conditions pour qu'il serve de clé dans la table d'adjacence. Un `int`, une `NkString`, un
identifiant d'entité conviennent d'office ; pour un type maison, on spécialise `nkentseu::NkHash<T>`
et on définit `operator==` (le header montre un exemple `struct Ville`).

```cpp
NkGraph<int> roads(false);     // non dirigé : route = lien mutuel
roads.AddEdge(0, 1);           // crée les sommets 0 et 1, puis le lien 0—1
roads.AddEdge(1, 2);
roads.AddEdge(0, 2);
roads.BFS(0, [](const int& city) {   // parcours en largeur depuis 0
    LogVisit(city);
});
```

Notez que `AddEdge` **crée automatiquement** les sommets s'ils n'existent pas : appeler `AddVertex`
au préalable est optionnel. Deux pièges **réels** à connaître avant de s'engager. D'abord, le
**poids n'est pas conservé** : `AddEdge(a, b, weight)` accepte un `float` mais ne le stocke nulle
part (la liste de voisins ne contient que des sommets) — il n'y a donc **pas** de Dijkstra natif
possible. Ensuite, ce n'est **pas** une structure d'accès indexé : pas d'itérateurs, et `HasEdge`
parcourt **linéairement** les voisins (`O(degré)`), à éviter en boucle chaude.

> **En résumé.** `NkGraph` = liste d'adjacence sommet→voisins, dirigé ou non (choix au ctor).
> `VertexType` doit être hashable + comparable. `AddEdge` crée les sommets tout seul. Parcours
> `DFS`/`BFS` en `O(V+E)` avec un visitor. **Le poids passé à `AddEdge` n'est pas stocké** (pas de
> chemin pondéré natif) ; `HasEdge` est `O(degré)`.

---

## Le partitionnement spatial : `NkOctree` et `NkQuadTree`

Le second besoin est géométrique : « **quels objets sont dans cette zone ?** », « **qui est près de
ce point ?** ». La réponse naïve — tester *tous* les objets à chaque requête, en `O(n)` — s'effondre
dès qu'il y en a des milliers. Un **arbre de partitionnement spatial** découpe récursivement
l'espace : tant qu'un nœud contient peu d'objets il les garde, et dès qu'il en a trop il se
**subdivise** en sous-régions égales. Une requête ne descend alors que dans les branches qui
recoupent la zone cherchée, et ignore tout le reste — d'où un coût moyen logarithmique au lieu de
linéaire.

`NkOctree<T>` partitionne l'espace **3D** : chaque nœud se découpe en **8 octants** (un cube en huit
sous-cubes). `NkQuadTree<T>` est son équivalent **2D** : chaque nœud se découpe en **4 quadrants**.
À part la dimension, ils sont **jumeaux** — même seuil de subdivision (`MAX_CAPACITY = 4` objets par
nœud avant découpe), même profondeur maximale (`MAX_DEPTH = 8`), même API, mêmes pièges. On choisit
l'octree pour un monde 3D (culling de caméra, requêtes de proximité dans une scène), le quadtree
pour un plan 2D (carte, minimap, broad-phase de collision 2D).

```cpp
NkQuadTree<EntityId> tree(0.f, 0.f, 1280.f, 720.f);   // racine = l'écran
for (auto& e : entities)
    tree.Insert(e.id, e.pos.x, e.pos.y);
// Qui est dans ce rectangle de sélection ?
tree.Query(rx, ry, rw, rh, [](const EntityId& id) {
    Select(id);
});
```

Trois pièges **réels** communs aux deux. Un : l'insertion **hors des bounds racine** est
**ignorée silencieusement** (no-op) — un objet placé hors du volume initial disparaît sans erreur.
Deux : il n'y a **ni `Remove` ni `UpdatePosition`**. Pour des objets **mobiles**, le pattern est
`Clear()` puis réinsertion complète à chaque frame (`Clear` réutilise l'allocateur, c'est conçu pour
ça). Trois — le plus subtil : **`QueryRadius` ne filtre PAS par la distance**. Il utilise seulement
l'AABB englobant le cercle/la sphère, et appelle le visitor pour tous les objets de cette boîte ;
le test de distance euclidienne précis est **laissé à l'appelant**. Vous recevrez donc des **faux
positifs aux coins** — à filtrer vous-même si la précision compte.

> **En résumé.** `NkOctree` (3D, 8 octants) et `NkQuadTree` (2D, 4 quadrants) sont jumeaux :
> subdivision à 4 objets / profondeur 8, `Insert`/`Query` en `O(log n + k)`, visitor par requête.
> **Insert hors bounds = ignoré ; pas de Remove (Clear+réinsertion pour le mobile) ; `QueryRadius`
> ne teste que l'AABB englobant** — filtrez la distance vous-même.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Aucune variante STL minuscule : seul le `PascalCase`
existe. Complexités entre crochets. Le détail (formules, domaines d'usage) suit dans la « Référence
complète ».

### `NkGraph<VertexType, Allocator>` — graphe par liste d'adjacence

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `struct Edge { From; To; Weight }`, `SizeType`, `VertexList`, `EdgeList`, `AdjacencyList` | Arête (poids `float`) ; alias `usize` ; vecteur de sommets ; vecteur d'arêtes (réservé) ; table sommet→voisins |
| Construction | `NkGraph(directed = false, allocator = nullptr)` `[O(1)]` | Dirigé ou non ; allocateur (défaut = NKMemory) |
| Sommets | `AddVertex` `[O(1)*]`, `HasVertex` `[O(1)*]`, `RemoveVertex` `[O(V+E)]` | Ajouter (idempotent) / tester / retirer (+ arêtes incidentes) |
| Arêtes | `AddEdge(from, to, weight=1)` `[O(1)*]`, `HasEdge` `[O(degré)]`, `RemoveEdge` `[O(deg+deg)]` | Lier (crée les sommets, **poids non stocké**) / tester / délier |
| Requêtes | `GetNeighbors` `[O(1)*]`, `GetDegree` `[O(1)]`, `VertexCount`, `EdgeCount`, `IsDirected`, `Empty`, `GetAllocator` | Voisins (ptr const ou `nullptr`) / degré / compteurs / dirigé ? / vide ? / allocateur |
| Parcours | `DFS(start, visitor)` `[O(V+E)]`, `BFS(start, visitor)` `[O(V+E)]` | Profondeur (récursif) / largeur (file). Visitor `void(const VertexType&)`. **Non-const** |
| Libre | `NkSwap(a, b)` `[O(1)]` | Échange (idiome ADL `using nkentseu::NkSwap;`) |

### `NkOctree<T, Allocator>` — partitionnement 3D (8 octants)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `struct Bounds` (X,Y,Z,Width,Height,Depth), `struct Entry { Data; X; Y; Z }`, `SizeType` | AABB 3D semi-ouvert `[min,max)` ; objet + position ; `usize` |
| Bounds | `Contains(px,py,pz)` `[O(1)]`, `Intersects(other)` `[O(1)]`, `GetCenter(&x,&y,&z)` `[O(1)]` | Point dedans ? / chevauchement ? / centre **par paramètres de sortie** |
| Construction | `NkOctree(x,y,z,w,h,d, allocator=nullptr)` `[O(1)]`, `~NkOctree` `[O(n)]` | Racine sur un volume ; destruction récursive |
| Capacité | `Empty`, `Size` | Vide ? / nombre d'objets indexés |
| Modification | `Insert(data,x,y,z)` `[O(log₈n)]`, `Clear` `[O(n)]` | Insérer (**hors bounds = ignoré**) / vider + recréer racine |
| Requêtes | `Query(x,y,z,w,h,d, visitor)` `[O(log₈n+k)]`, `QueryRadius(cx,cy,cz,r, visitor)` `[O(log₈n+k)]` | Boîte / sphère (**AABB seul, pas de test distance**). Visitor `void(const T&)`, **const** |
| Accès | `GetRootBounds`, `GetAllocator` | Bounds racine / allocateur |
| Libre | `NkSwap(a, b)` `[O(1)]` | Échange |

### `NkQuadTree<T, Allocator>` — partitionnement 2D (4 quadrants)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `struct Bounds` (X,Y,Width,Height), `struct Entry { Data; X; Y }`, `SizeType` | AABB 2D semi-ouvert ; objet + position ; `usize` |
| Bounds | `Contains(px,py)` `[O(1)]`, `Intersects(other)` `[O(1)]`, `GetCenter(&x,&y)` `[O(1)]` | Point dedans ? / chevauchement ? / centre **par paramètres de sortie** |
| Construction | `NkQuadTree(x,y,w,h, allocator=nullptr)` `[O(1)]`, `~NkQuadTree` `[O(n)]` | Racine sur une zone ; destruction récursive |
| Capacité | `Empty`, `Size` | Vide ? / nombre d'objets indexés |
| Modification | `Insert(data,x,y)` `[O(log₄n)]`, `Clear` `[O(n)]` | Insérer (**hors zone = ignoré**) / vider + recréer racine |
| Requêtes | `Query(x,y,w,h, visitor)` `[O(log₄n+k)]`, `QueryRadius(cx,cy,r, visitor)` `[O(log₄n+k)]` | Rectangle / cercle (**AABB seul, pas de test distance**). Visitor `void(const T&)`, **const** |
| Accès | `GetRootBounds`, `GetAllocator` | Bounds racine / allocateur |
| Libre | `NkSwap(a, b)` `[O(1)]` | Échange |

(*) `O(1)` **amorti** (table de hachage de l'adjacence).

---

## Référence complète

Chaque élément repris en détail : complexité, ce qu'il fait vraiment, et ses usages par domaine
(rendu, ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU). Le trivial est bref ,
l'important — surtout les pièges — est traité à fond.

### `NkGraph` — construction et sommets

`NkGraph(directed, allocator)` est le seul constructeur (`O(1)`, pas de copie/move déclaré) : le
booléen fixe une fois pour toutes si les arêtes sont à sens unique. `AddVertex(v)` est **idempotent**
(il vérifie d'abord `Contains`) et alloue une liste de voisins vide en `O(1)` amorti ;
`HasVertex(v)` teste sa présence en `O(1)` amorti ; `RemoveVertex(v)` est l'opération la plus chère
(`O(V+E)`) car elle doit balayer **toutes** les listes pour effacer les arêtes **entrantes**, puis
supprimer la liste sortante, et corriger les compteurs.

- **Gameplay / IA** — sommets = salles, points de patrouille, nœuds de navigation ; `AddVertex`
  pour peupler le graphe d'un niveau au chargement.
- **ECS / scène** — sommets = entités, pour modéliser un graphe de parenté ou de dépendances entre
  systèmes ; `RemoveVertex` quand une entité meurt (coûteux, à faire hors boucle chaude).
- **IO / build** — sommets = fichiers ou tâches d'un pipeline d'assets.

### `NkGraph::AddEdge`, `HasEdge`, `RemoveEdge` — les arêtes

`AddEdge(from, to, weight = 1.0f)` **crée automatiquement** les deux sommets s'ils manquent, puis
ajoute `to` aux voisins de `from` ; pour un graphe **non dirigé** il ajoute aussi l'arête inverse
`to→from` (sauf pour une boucle `from == to`). `mEdgeCount` n'est incrémenté **qu'une seule fois**.
Coût `O(1)` amorti. **Le piège central du type** : le paramètre `weight` n'est **pas stocké** — la
liste de voisins ne contient que des sommets, et `Edge`/`EdgeList` ne sont jamais peuplés. Il n'y a
donc pas de plus court chemin pondéré natif.

`HasEdge(from, to)` parcourt **linéairement** les voisins de `from` (`O(degré(from))`) et ne vérifie
**pas** implicitement `to→from`. `RemoveEdge(from, to)` retire `from→to` (et `to→from` si non
dirigé) et décrémente `mEdgeCount` une fois ; coût `O(degré(from) + degré(to))`.

- **Gameplay / IA** — arêtes = passages entre salles, transitions d'une machine à états de dialogue.
- **Physique / contraintes** — arêtes = liens entre corps rigides (ragdoll, tissu) modélisés comme
  un graphe de contraintes.
- **UI / 2D** — arêtes = dépendances de layout (un widget contraint par un autre).
- Pour des chemins **pondérés** (distance, coût de déplacement), le poids n'étant pas conservé, il
  faut maintenir la pondération **à côté** (une `NkHashMap<paire, float>` séparée, par exemple).

### `NkGraph` — requêtes

`GetNeighbors(v)` renvoie un **pointeur const** vers la liste de voisins, ou `nullptr` si le sommet
est absent (`O(1)` amorti) — toujours tester le retour. `GetDegree(v)` donne la taille de cette
liste (0 si absent ; c'est le **degré sortant** pour un graphe dirigé), en `O(1)`. Les accesseurs
`VertexCount()`, `EdgeCount()`, `IsDirected()`, `Empty()` (vrai si `mVertexCount == 0`) et
`GetAllocator()` sont tous `O(1)` et `NKENTSEU_NOEXCEPT`.

- **Gameplay / IA** — `GetNeighbors` est l'opération de base d'un pathfinding maison (on étend les
  voisins du nœud courant) ; `GetDegree` sert à repérer les carrefours ou les culs-de-sac.
- **Rendu** — parcourir les voisins d'un nœud de scène pour propager une transformation.

### `NkGraph::DFS` et `BFS` — les parcours

Les deux sont des **templates** prenant un `start` et un `visitor` de signature
`void(const VertexType&)`, et coûtent `O(V + E)`. Tous deux sont **non-const** (méthodes membres non
const). `DFS` (profondeur d'abord) est **récursif** et marque les sommets visités via un
`NkUnorderedSet` ; il appelle le visitor **à la première visite**. `BFS` (largeur d'abord) est
**itératif** : il utilise un `NkVector` comme file (avec un index `head`) plus un `NkUnorderedSet` ;
nuance d'ordre importante — il **marque** `start` au moment de l'**enfiler** mais appelle le visitor
au moment de le **défiler**.

- **Gameplay / IA** — `BFS` trouve le chemin en **nombre minimal d'arêtes** (utile sur une grille
  non pondérée) ; `DFS` détecte des cycles, explore un labyrinthe, fait un flood-fill logique.
- **IO / build** — `DFS` pour un tri topologique de dépendances ; `BFS` pour propager par niveaux.
- **Rendu / scène** — `BFS` pour parcourir un graphe de scène par profondeur de hiérarchie.
- **Règle** : le visitor **ne doit pas muter le graphe** pendant le parcours (il invaliderait les
  structures internes de marquage).

### `NkOctree` / `NkQuadTree::Bounds` — les boîtes englobantes

`Bounds` est l'**AABB** qui délimite chaque nœud : `(X, Y, Z, Width, Height, Depth)` en 3D,
`(X, Y, Width, Height)` en 2D, avec une convention **semi-ouverte `[min, max)`** sur chaque axe (ce
qui lève les ambiguïtés de frontière entre nœuds voisins). `Contains(px, py[, pz])` teste si un point
y tombe (`O(1)`), `Intersects(other)` teste le chevauchement de deux boîtes (`O(1)`), et
`GetCenter(&x, &y[, &z])` écrit le centre **par paramètres de sortie `float&`** — il **ne retourne
rien** (la doc qui parle de « tableau » est trompeuse : suivre la signature réelle).

- **Rendu** — la boîte d'un nœud sert au **frustum culling** : si le frustum ne coupe pas le nœud,
  toute sa branche est sautée d'un coup.
- **Physique** — `Intersects` est la **broad-phase** : on ne teste finement que les paires dont les
  AABB se recouvrent.
- **UI / 2D** — `Contains` pour le hit-testing d'un clic dans une zone.

### `NkOctree` / `NkQuadTree::Entry` — l'objet stocké

`Entry` empaquette la donnée et sa position : `{ T Data; float X, Y[, Z]; }`, construit par
`Entry(data, x, y[, z])` (la donnée `T` est **copiée**). C'est ce que l'arbre range réellement dans
ses nœuds feuilles. En pratique on n'instancie pas `Entry` à la main : `Insert` le fait.

### `NkOctree` / `NkQuadTree` — construction et capacité

Le constructeur fixe la **racine** sur un volume (`x,y,z,w,h,d` en 3D) ou une zone (`x,y,w,h` en
2D), `allocator = nullptr` retombant sur l'allocateur par défaut de NKMemory ; coût `O(1)`. Le
destructeur appelle `DestroyNode(mRoot)` récursivement (`O(n)` nœuds). Les nœuds sont **alloués
individuellement** via `mAllocator->Allocate`/`Deallocate` + placement new — d'où la recommandation
d'un **pool allocator** pour limiter la fragmentation. `Empty()` (vrai si `mSize == 0`) et `Size()`
(nombre d'objets indexés) sont `O(1)`.

### `NkOctree::Insert` / `NkQuadTree::Insert` — placer un objet

`Insert(data, x, y[, z])` descend récursivement jusqu'à la feuille couvrant la position ; quand une
feuille dépasse `MAX_CAPACITY = 4` (et qu'on n'a pas atteint `MAX_DEPTH = 8`), elle se **subdivise**.
Coût moyen `O(log₈ n)` (octree) / `O(log₄ n)` (quadtree). `mSize` n'est incrémenté **que si le point
tombe dans les bounds** : un `Insert` **hors du volume racine est un no-op silencieux** (aucune
erreur, l'objet est simplement perdu) — dimensionner la racine pour englober tout le monde de jeu.

- **ECS / scène** — indexer chaque entité par sa position au début de la frame.
- **Physique** — peupler la grille de broad-phase avant la détection de collision.
- **Audio** — indexer les sources sonores pour ne mixer que celles proches de l'auditeur.

### `NkOctree::Clear` / `NkQuadTree::Clear` — réinitialiser

`Clear()` détruit tous les nœuds, **recrée la racine avec les mêmes bounds**, et remet `mSize = 0`
(`O(n)`). C'est la **pierre angulaire du flux dynamique** : faute de `Remove` ou de
`UpdatePosition`, la façon canonique de gérer des objets **mobiles** est de `Clear()` l'arbre puis de
**tout réinsérer** à chaque frame. Comme `Clear` réutilise l'allocateur, ce cycle est conçu pour
être bon marché image après image.

### `NkOctree::Query` / `NkQuadTree::Query` — la requête par zone

`Query(zone..., visitor)` est un **template** `const` qui appelle le `visitor` `void(const T&)` pour
chaque objet dont la position tombe dans la **boîte/rectangle** donné, en `O(log n + k)` (`k` =
nombre de résultats). L'**ordre de visite n'est pas garanti**. C'est la requête de référence — celle
qui justifie l'existence de la structure.

- **Rendu** — récupérer en `O(log n + k)` les objets visibles dans le frustum (approximé par une
  boîte) au lieu de balayer toute la scène.
- **Physique / collision** — broad-phase : pour un corps, ne récupérer que les candidats de sa
  cellule et des cellules voisines.
- **UI / 2D** — sélection rectangulaire (rubber-band) sur une carte ou un éditeur.
- **Audio** — sources audibles dans un rayon (approximé par sa boîte englobante).
- **Règle** : `const` mais le visitor **ne doit pas muter l'arbre** pendant la requête.

### `NkOctree::QueryRadius` / `NkQuadTree::QueryRadius` — la requête par rayon (attention)

`QueryRadius(centre..., radius, visitor)` **promet** une requête sphère (3D) / cercle (2D) mais, dans
l'implémentation **réelle**, ne fait qu'une `Query` sur l'**AABB englobant** le rayon : le lambda
interne appelle `visitor(data)` **directement**, sans appliquer le test de **distance euclidienne**
(le filtrage sphérique est laissé en commentaire, à la charge de l'appelant). Conséquence concrète :
vous recevez des **faux positifs aux coins** de la boîte. Même coût `O(log n + k)`.

- **Gameplay / IA** — « ennemis à portée d'attaque » : `QueryRadius` donne les candidats, **puis**
  filtrez avec `LenSq(pos − centre) < radius²` (cf. [Vectors](../NKMath/Vectors.md), réflexe du
  carré pour éviter une racine).
- **Audio** — sources dans un rayon d'audibilité : même schéma, filtre distance côté appelant.
- **Physique** — explosion à rayon d'effet : récupérer les candidats, appliquer la décroissance
  selon la vraie distance.

### `GetRootBounds`, `GetAllocator` — accès

`GetRootBounds()` renvoie la `Bounds` de la racine (utile pour redimensionner ou vérifier l'emprise
du monde), `GetAllocator()` l'allocateur ; tous deux `O(1)` et `NKENTSEU_NOEXCEPT`.

### L'ordre des octants et des quadrants

Le découpage suit un **ordre fixe documenté**. Pour le **quadtree** : `0=NW, 1=NE, 2=SW, 3=SE`
(N = Y+, S = Y−, E = X+, W = X−, convention **Y-up**). Pour l'**octree** :
`0=NWF, 1=NFE, 2=SWF, 3=SFE, 4=NWB, 5=NEB, 6=SWB, 7=SEB` (mêmes N/S/E/W, plus F = avant / B = arrière
sur Z). On n'a normalement pas à manipuler ces indices (l'arbre route les objets tout seul), mais ils
fixent l'ordre déterministe de la subdivision.

### `NkSwap` — l'échange (les trois types)

Chaque type fournit une **free function** `NkSwap(lhs, rhs)` `NKENTSEU_NOEXCEPT` en `O(1)`, qui
échange directement les membres internes (`NkGraph` : ses 5 membres ; `NkOctree`/`NkQuadTree` :
`mRoot`, `mSize`, `mAllocator`) via `traits::NkSwap`. C'est le **seul** mécanisme d'échange (aucun de
ces types ne déclare de copie/move). Idiome **ADL** :

```cpp
using nkentseu::NkSwap;
NkSwap(treeA, treeB);   // O(1) : on ne déplace que les pointeurs racine
```

### Le socle commun

- **Allocateur conscient.** Les trois prennent un `Allocator` (défaut `memory::NkAllocator`,
  résolu via `memory::NkGetDefaultAllocator()` si `nullptr`). Voir [NKMemory](../NKMemory.md).
- **Pas de double casse.** Contrairement aux séquentiels, **aucune** méthode STL minuscule : que du
  `PascalCase`. Pas d'itérateurs, donc pas de `range-based for`.
- **Visitor par valeur.** Les parcours/requêtes prennent le visitor **par valeur** dans l'API
  publique (`void(const VertexType&)` / `void(const T&)`) ; il **ne doit jamais muter** la structure
  pendant le parcours.
- **Bounds semi-ouverts.** Convention `[min, max)` sur tous les axes (octree/quadtree), pour des
  frontières non ambiguës.
- **Pas de sérialisation, thread-unsafe.** Aucun des trois ne sérialise ni ne se protège des accès
  concurrents — synchroniser à l'extérieur si partagé entre threads.

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu;

// Graphe : carte de salles non dirigée, parcourue en largeur depuis l'entrée.
NkGraph<int> rooms(false);
rooms.AddEdge(0, 1);          // crée 0 et 1, puis le lien mutuel
rooms.AddEdge(1, 2);
rooms.AddEdge(0, 2);
rooms.BFS(0, [](const int& room) {   // visite par niveaux
    Reveal(room);
});

// QuadTree : broad-phase 2D, reconstruit chaque frame (objets mobiles).
NkQuadTree<EntityId> tree(0.f, 0.f, worldW, worldH);
tree.Clear();                                   // pattern dynamique
for (auto& e : entities)
    tree.Insert(e.id, e.pos.x, e.pos.y);        // hors zone = ignoré

// Ennemis à portée : QueryRadius donne les candidats AABB, on filtre la distance.
tree.QueryRadius(px, py, range, [&](const EntityId& id) {
    auto& e = world.Get(id);
    if ((e.pos - player).LenSq() < range * range)   // filtre réel (faux positifs sinon)
        Hit(id);
});
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [← Couche Foundation](../README.md)
