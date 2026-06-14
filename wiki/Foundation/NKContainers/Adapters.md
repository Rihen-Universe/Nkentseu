# Les adaptateurs

> Couche **Foundation** · NKContainers · Restreindre un conteneur séquentiel à une **discipline
> d'accès** : la file FIFO `NkQueue` (premier entré, premier sorti) et la pile LIFO `NkStack`
> (dernier entré, premier sorti).

Il y a deux façons de structurer un conteneur. La première est de choisir **comment les éléments
sont rangés en mémoire** — c'est le rôle des [conteneurs séquentiels](Sequential.md) : vecteur
contigu, listes chaînées, deque segmentée. La seconde est de choisir **par quel ordre on entre et
sort** des éléments, sans se soucier du rangement physique. C'est exactement ce que font les
*adaptateurs* : ils prennent un conteneur séquentiel existant et lui imposent une **discipline**.
`NkQueue` impose le FIFO (on défile dans l'ordre où l'on a enfilé), `NkStack` impose le LIFO (on
dépile dans l'ordre inverse). Cette page vous apprend à choisir entre les deux — et à comprendre
pourquoi ce ne sont **pas** des conteneurs à part entière, mais des *vues restreintes*.

Le mot-clé est **restreindre**. Un adaptateur n'ajoute aucune capacité ; il en **retire**. Là où
un `NkDeque` vous laisse pousser et tirer aux deux bouts, lire `[i]`, itérer, `NkQueue` ne vous
laisse plus qu'enfiler à l'arrière et défiler à l'avant — rien d'autre. Cette amputation est
**volontaire** : en interdisant les opérations qui n'ont pas de sens pour une file, le type rend
votre intention explicite et empêche les erreurs. Quand vous lisez `NkQueue<Task>` dans une
signature, vous savez instantanément que les tâches sortiront dans l'ordre d'arrivée.

Les deux types sont **templatés** sur le type d'élément **et** sur le conteneur sous-jacent
(`NkQueue<T, Container = NkDeque<T>>`, `NkStack<T, Container = NkVector<T>>`). Le défaut est choisi
pour que la discipline soit efficace, mais vous pouvez le changer. Toute la mémoire passe donc par
le conteneur interne — donc par NKMemory, jamais par `new`/`delete`.

- **Namespace** : `nkentseu`
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

> ⚠️ Contrairement aux conteneurs séquentiels, les adaptateurs exposent **uniquement** l'API maison
> `PascalCase` — il n'y a **pas** de variantes STL minuscules, pas d'itérateurs, pas de
> `range-based for`. On enfile, on regarde un bout, on défile : c'est tout.

---

## La file FIFO : `NkQueue`

`NkQueue` modélise une **file d'attente** au sens propre : une file de gens devant un guichet. On
arrive **par l'arrière** (`Push`), on regarde qui est en tête (`Front()`), on sert et on retire la
tête (`Pop()`). Le premier arrivé est le premier servi — *first in, first out*. C'est la discipline
naturelle de tout ce qui doit être traité **dans l'ordre d'arrivée** sans qu'aucun ne double les
autres.

Le défaut est `NkDeque<T>`, et ce n'est pas un hasard. Une file enfile à l'arrière (`PushBack`) et
défile à l'avant (`PopFront`) : il faut donc un conteneur qui rende ces **deux** opérations
efficaces. La deque le fait en `O(1)` aux deux bouts. Ce n'est **pas** un `NkVector` — un vecteur
ferait `PopFront` en `O(n)` (il faudrait décaler tout le reste à chaque retrait), ce qui ruinerait
la file. C'est la raison d'être du défaut deque.

```cpp
NkQueue<Task> jobs;
jobs.Push(loadTexture);          // enfile à l'arrière
jobs.Push(loadMesh);
jobs.Push(loadSound);

while (!jobs.Empty()) {
    Task t = jobs.Front();        // le plus ancien
    jobs.Pop();                   // Pop() ne retourne RIEN — on a lu Front() avant
    Run(t);
}                                 // exécutés dans l'ordre d'arrivée : texture, mesh, son
```

Deux accès aux extrémités : `Front()` est le plus **ancien** (le prochain à sortir), `Back()` le
plus **récent** (le dernier entré). Ce n'est **pas** une structure d'inspection : il n'y a ni `[i]`,
ni itérateur, ni `Clear()`. On ne peut regarder que les deux bouts.

> **En résumé.** `NkQueue` = discipline **FIFO**. `Push` enfile à l'arrière, `Front()` lit la tête,
> `Pop()` retire la tête (sans la renvoyer), `Back()` lit la queue. Conteneur par défaut `NkDeque`
> (`O(1)` aux deux bouts — surtout **pas** un vecteur à cause de `PopFront` en `O(n)`). L'outil du
> traitement **dans l'ordre d'arrivée**.

---

## La pile LIFO : `NkStack`

`NkStack` modélise une **pile d'assiettes** : on empile par le haut, on dépile par le haut. Tout se
passe sur **une seule extrémité**, le **sommet** (`Top()`). On empile (`Push`), on regarde le sommet
(`Top()`), on dépile le sommet (`Pop()`). Le dernier posé est le premier repris — *last in, first
out*. C'est la discipline naturelle de tout ce qui suit une logique de **retour en arrière** : on
défait dans l'ordre inverse de ce qu'on a fait.

Le défaut est `NkVector<T>`, et là encore le choix est motivé. Une pile ne touche qu'un bout : elle
empile via `PushBack` et dépile via `PopBack`, deux opérations que le vecteur fait en `O(1)`
**amorti** — sans jamais payer de `PopFront`. Le vecteur est donc parfait, et même préférable à la
deque ici : mémoire contiguë, pas de segmentation. Si vous voulez du `O(1)` **garanti** (pas
seulement amorti), vous pouvez passer `NkStack<T, NkDeque<T>>`.

```cpp
NkStack<NkMat4> matrixStack;     // pile de transformations (scene graph)
matrixStack.Push(world);
matrixStack.Push(world * local); // on descend dans la hiérarchie

const NkMat4& current = matrixStack.Top();
RenderNode(node, current);

matrixStack.Pop();               // on remonte d'un cran — Top() redevient 'world'
```

Un seul accès : `Top()` (le sommet). Il n'y a **pas** de `Front()`/`Back()` exposés sur une pile —
la notion de file n'a pas de sens ici, seul le sommet existe. Pas non plus de `[i]`, d'itérateur ni
de `Clear()`.

> **En résumé.** `NkStack` = discipline **LIFO**. `Push` empile au sommet, `Top()` lit le sommet,
> `Pop()` retire le sommet (sans le renvoyer). Conteneur par défaut `NkVector` (`O(1)` amorti, mémoire
> contiguë ; `NkDeque` pour du `O(1)` garanti). Seul `Top()` est accessible. L'outil du **retour en
> arrière** (annulation, parcours en profondeur, évaluation d'expressions).

---

## File ou pile ? Le critère

Le choix tient en une question : **dans quel ordre les éléments doivent-ils ressortir ?**

- **Dans l'ordre où ils sont entrés** (équité, traitement séquentiel) → `NkQueue` (FIFO).
- **Dans l'ordre inverse** (le plus récent d'abord, retour en arrière) → `NkStack` (LIFO).

Une dernière différence à retenir : `NkStack` possède `operator==`/`operator!=` (on peut comparer
deux piles), **pas** `NkQueue`. Les deux partagent en revanche un piège : **`Pop()` ne renvoie pas
l'élément**. Il faut toujours lire (`Front()`/`Top()`) **avant** de retirer (`Pop()`).

---

## Aperçu de l'API

Tous les éléments publics des deux adaptateurs. Complexités entre crochets (en supposant le
conteneur par défaut). API **exclusivement** `PascalCase` (aucune variante STL minuscule).

### `NkQueue<T, Container = NkDeque<T>>` — file FIFO

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `ValueType`, `SizeType`, `Reference`, `ConstReference`, `ContainerType` | Type d'élément, `usize`, `T&`, `const T&`, conteneur sous-jacent |
| Construction | `NkQueue()`, `NkQueue(const Container&)`, `NkQueue(Container&&)`, `NkQueue(NkInitializerList<T>)`, `NkQueue(std::initializer_list<T>)` | Vide / copie d'un conteneur `[O(n)]` / déplacement `[O(1)]` / liste `[O(m)]` |
| Assignation | `operator=(NkInitializerList<T>)`, `operator=(std::initializer_list<T>)` | Vide puis ré-insère `[O(n+m)]` |
| Accès | `Front()` / `const`, `Back()` / `const` | Plus ancien (tête) / plus récent (queue) `[O(1)]` — précondition `!Empty()` |
| Capacité | `Empty()`, `Size()` | Vide ? / nombre d'éléments `[O(1)]` |
| Modification | `Push(const T&)`, `Push(T&&)`, `Emplace(Args&&…)`, `Pop()`, `Swap(NkQueue&)` | Enfiler `[O(1)]` / construire in-place `[O(1)]` / défiler la tête `[O(1)]` / échanger `[O(1)]` |
| Libre | `NkSwap(a, b)` | Échange `O(1)` (ADL) |

### `NkStack<T, Container = NkVector<T>>` — pile LIFO

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `ValueType`, `SizeType`, `Reference`, `ConstReference`, `ContainerType` | Type d'élément, `usize`, `T&`, `const T&`, conteneur sous-jacent |
| Construction | `NkStack()`, `NkStack(const Container&)`, `NkStack(Container&&)`, `NkStack(NkInitializerList<T>)`, `NkStack(std::initializer_list<T>)` | Vide / copie `[O(n)]` / déplacement `[O(1)]` / liste `[O(m)]` (dernier = sommet) |
| Assignation | `operator=(NkInitializerList<T>)`, `operator=(std::initializer_list<T>)` | Vide puis ré-insère `[O(n+m)]` |
| Accès | `Top()` / `const` | Sommet `[O(1)]` — précondition `!Empty()` |
| Capacité | `Empty()`, `Size()` | Vide ? / hauteur de la pile `[O(1)]` |
| Modification | `Push(const T&)`, `Push(T&&)`, `Emplace(Args&&…)`, `Pop()`, `Swap(NkStack&)` | Empiler `[O(1)*]` / construire in-place `[O(1)*]` / dépiler `[O(1)]` / échanger `[O(1)]` |
| Comparaison | `operator==`, `operator!=` | Égalité du fond vers le sommet `[O(n)]` |
| Libre | `NkSwap(a, b)` | Échange `O(1)` (ADL) |

(*) amorti, avec `NkVector` (`O(1)` garanti avec `NkDeque`).

---

## Référence complète

Chaque élément est repris ici, avec sa complexité et ses usages par domaine. Les opérations
triviales (capacité, échange) sont décrites brièvement ; la discipline FIFO/LIFO et ses
applications le sont **à fond**.

### Le paramètre `Container` — choisir le conteneur sous-jacent

Un adaptateur n'est qu'une **enveloppe** autour d'un membre `Container mContainer;`. Il délègue
chaque opération au conteneur : `Push` appelle `PushBack`, `Front` appelle `Container::Front`, et
ainsi de suite. Le défaut est pensé pour que la discipline soit efficace — mais le bon défaut
**dépend de la discipline** :

- **`NkQueue` → toujours `NkDeque`.** Une file défile à l'avant (`PopFront`). Seule la deque le fait
  en `O(1)` ; un `NkVector` le ferait en `O(n)`. **Ne changez pas** ce défaut sans raison.
- **`NkStack` → `NkVector` (défaut) ou `NkDeque`.** Une pile ne touche qu'un bout (`PushBack`/
  `PopBack`), que le vecteur fait en `O(1)` amorti avec une mémoire contiguë. Passez à
  `NkStack<T, NkDeque<T>>` si vous voulez du `O(1)` **garanti** (pas d'à-coup de réagencement), ou
  `NkList` si la stabilité des nœuds prime (au prix d'un overhead de pointeurs).

Les alias `ValueType`/`SizeType`/`Reference`/`ConstReference`/`ContainerType` exposent ces types
pour le code générique (un template qui prend `typename Q::ValueType`).

### Construction et assignation

Quatre façons de remplir un adaptateur, au-delà du constructeur vide. **Depuis un conteneur** :
`NkQueue<int> q(deque)` copie un conteneur existant (son premier élément devient `Front()`), ou le
**déplace** en `O(1)` si on lui passe un rvalue (`NkQueue<int> q(NkMove(deque))`). **Depuis une
liste d'initialisation** : `NkStack<int> s = {10, 20, 30}` pousse chaque élément dans l'ordre — pour
la pile, `10` finit au **fond** et `30` au **sommet** (`Top() == 30`) ; pour la file, `10` devient
`Front()` et `30` devient `Back()`. Les deux formes de liste (`NkInitializerList` maison et
`std::initializer_list`) sont acceptées. L'assignation par liste **vide d'abord** l'adaptateur (par
une boucle de `Pop`) puis ré-insère — d'où son coût `O(n + m)`.

- **Rendu / ECS** — démarrer une file de travail à partir d'un lot déjà constitué (`NkQueue<Job>
  q(pendingJobs)`), ou une pile de passes de rendu listée à la main.
- **IA / gameplay** — initialiser une file de waypoints ou une pile d'états depuis une configuration.

### `Push`, `Emplace` — entrer un élément

`Push(const T&)` copie un élément dans le conteneur (à l'arrière pour la file, au sommet pour la
pile). Sous `NK_CPP11`, `Push(T&&)` le **déplace** au lieu de le copier — indispensable pour les
types lourds : `q.Push(nkentseu::traits::NkMove(grosTampon))` transfère le contenu sans recopie (le
`grosTampon` devient valide-mais-indéterminé). `Emplace(Args&&…)` va plus loin : il **construit
l'élément in-place** dans le conteneur à partir de ses arguments, économisant à la fois le temporaire
**et** le déplacement. Complexité `O(1)` (amorti pour la pile sur `NkVector`).

- **Rendu** — empiler des matrices de transformation à chaque descente dans le graphe de scène
  (`Push`), enfiler des commandes de dessin à exécuter dans l'ordre.
- **ECS / scène** — file d'entités à créer ou détruire en fin de frame (on enfile pendant la frame,
  on défile au flush pour ne pas muter l'archétype en cours d'itération).
- **Physique** — pile de paires à tester en *broad-phase* (parcours d'un BVH), file d'îlots de
  contacts à résoudre.
- **Animation** — file d'événements de timeline (déclencheurs de visèmes, notifications de keyframe)
  consommés dans l'ordre temporel.
- **Gameplay / IA** — file de waypoints d'un chemin (FIFO : on suit dans l'ordre), pile d'états d'une
  machine à états *push-down* (un menu ouvre un sous-menu, qu'on dépile pour revenir).
- **Audio** — file de buffers à jouer par le mixeur (producteur/consommateur), file de commandes du
  thread principal vers le thread audio.
- **UI / 2D** — pile de zones de découpe (*clip rects* façon ImGui : `Push` une zone, `Pop` pour
  revenir à la précédente), file de notifications à afficher l'une après l'autre.
- **IO** — file de messages réseau reçus, à traiter dans l'ordre d'arrivée ; file de requêtes de
  chargement d'assets.
- **GPU** — file de transferts à uploader, file de fences à attendre.

### `Front`, `Back` (file) et `Top` (pile) — regarder sans retirer

Ces accès renvoient une **référence** (mutable ou const) vers un élément, **sans le retirer**. Pour
la file, `Front()` est le plus ancien (le prochain à sortir) et `Back()` le plus récent ; pour la
pile, `Top()` est le sommet (le prochain à sortir). Tous délèguent au conteneur (`Front`/`Back`/
`Back` respectivement) en `O(1)`.

**Précondition cruciale** : ces méthodes exigent `!Empty()`. En debug, appeler `Front()`/`Top()` sur
un adaptateur vide déclenche `NKENTSEU_ASSERT(!Empty())` ; en **release**, c'est un **comportement
indéfini**. Il n'existe pas de version sûre (`TryFront`/`TryTop`) — c'est à vous de garder le
`if (!q.Empty())` ou la boucle `while (!q.Empty())`.

- **Rendu / UI** — lire la matrice ou le *clip rect* courant (`Top()`) sans dépiler, pour l'appliquer
  au dessin en cours.
- **Gameplay / IA** — consulter le prochain waypoint (`Front()`) pour calculer une direction sans le
  consommer tant qu'on n'est pas arrivé ; lire l'état courant (`Top()`) d'une machine *push-down*.
- **Audio** — inspecter le prochain buffer à jouer avant de décider de le mixer.

### `Pop` — retirer un élément

`Pop()` retire l'élément avant de la file (`PopFront`) ou le sommet de la pile (`PopBack`), en
`O(1)`, avec la même précondition `!Empty()`. **Point capital : `Pop()` ne renvoie rien.** Ce n'est
pas un oubli mais une garantie d'exception ; il n'existe pas de `TryPop(T& out)` (listé comme
extension future). L'idiome obligatoire est donc **lire puis retirer** :

```cpp
Task t = jobs.Front();   // on récupère AVANT
jobs.Pop();              // puis on retire (ne renvoie pas t)
```

C'est le cœur de toute boucle de vidage. Pour une file, la sortie respecte l'ordre d'insertion ;
pour une pile, l'ordre **inverse** :

```cpp
while (!queue.Empty()) { Use(queue.Front()); queue.Pop(); }   // ordre d'arrivée
while (!stack.Empty()) { Use(stack.Top());   stack.Pop(); }   // ordre inverse
```

- **ECS** — vider la file de créations/destructions différées au flush de fin de frame.
- **Animation** — défiler les événements de timeline dont l'instant est passé.
- **Gameplay / IA** — dépiler un état quand on ferme un menu ; défiler un waypoint atteint ;
  *backtracking* d'un labyrinthe (on dépile pour revenir au dernier embranchement, parcours en
  profondeur DFS).
- **Audio / IO** — consommer un buffer joué, un message traité.
- **UI / 2D** — `Pop` la zone de découpe en sortant d'un panneau, restaurant la précédente.

### `Empty`, `Size` — l'état

`Empty()` indique si l'adaptateur ne contient aucun élément (`noexcept`, `O(1)`), `Size()` renvoie
leur nombre (la **hauteur** pour une pile). Ce sont les **garde-fous** des accès : on teste toujours
`Empty()` avant `Front()`/`Top()`/`Pop()`. `Size()` sert à borner une structure — par exemple jeter
le plus vieux quand une file de notifications dépasse un seuil, ou détecter une récursion trop
profonde via la hauteur d'une pile d'états.

### `Swap` et `NkSwap` — échanger deux adaptateurs

`Swap(other)` échange le contenu de deux adaptateurs **du même type** en `O(1)`, en déléguant à
`Container::Swap` (on échange juste les entrailles, on ne recopie rien). La fonction libre
`NkSwap(a, b)` fait la même chose via ADL — l'idiome canonique est :

```cpp
using nkentseu::NkSwap;
NkSwap(fileA, fileB);
```

C'est l'outil du *double buffering* d'une file (on accumule dans l'une pendant qu'on draine l'autre)
ou du transfert atomique d'une pile de commandes vers le consommateur.

### `operator==`, `operator!=` (NkStack uniquement)

`NkStack` peut être comparée : `a == b` vrai si les deux piles contiennent les mêmes éléments dans
le même ordre (la comparaison se fait sur le conteneur interne, du **fond vers le sommet**), en
`O(n)`. `a != b` est son inverse. Utile pour comparer deux états *push-down* (a-t-on la même
navigation de menus ?) ou détecter qu'une pile d'annulation a changé.

**`NkQueue` n'a pas** d'`operator==`/`operator!=`. Deux files ne sont **pas** comparables directement
— il faut les vider dans des conteneurs intermédiaires si on veut les comparer.

### Pièges et limites

À garder en tête (tous documentés dans les headers) :

- **`Pop()` ne renvoie pas l'élément** : lire (`Front()`/`Top()`) **avant** de retirer. Pas de
  `TryPop(T& out)`.
- **Précondition `!Empty()`** sur `Front()`/`Back()`/`Top()`/`Pop()` : assertion en debug,
  **comportement indéfini en release**. Pas de variantes sûres `Try*`.
- **Pas de `Clear()`** ni d'accès au conteneur interne (`GetContainer()`) : on ne peut donc pas
  `Reserve()` le conteneur sous-jacent (extensions futures).
- **Pas d'itération** : aucun itérateur, aucun `range-based for`, aucun `[i]`. Un adaptateur ne se
  parcourt pas — il se vide.
- **Thread-unsafe** par conception : pour un usage producteur/consommateur entre threads, protégez
  l'adaptateur par un mutex externe.
- **`NkQueue` n'a pas d'`operator==`/`!=`** (seul `NkStack` en possède).
- **Move ctor / `Push(T&&)` / `Emplace`** ne sont compilés que sous `NK_CPP11`.

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu;

// File FIFO : un ordonnanceur de tâches, traitées dans l'ordre d'arrivée.
NkQueue<Task> jobs;                      // NkDeque sous le capot
jobs.Push(loadTexture);
jobs.Emplace("compileShader", quality);  // construit in-place, pas de temporaire
while (!jobs.Empty()) {
    Task t = jobs.Front();               // lire AVANT
    jobs.Pop();                          // Pop() ne renvoie rien
    Run(t);
}

// Pile LIFO : pile de transformations d'un graphe de scène.
NkStack<NkMat4> matrices;                // NkVector sous le capot
matrices.Push(world);
matrices.Push(world * local);
RenderNode(node, matrices.Top());        // seul Top() est accessible
matrices.Pop();                          // on remonte d'un cran

// Pile LIFO : backtracking DFS d'un labyrinthe.
NkStack<Cell> path = { start };
while (!path.Empty()) {
    Cell c = path.Top();
    if (c == goal) break;
    Cell next;
    if (HasUnvisitedNeighbor(c, next)) path.Push(next);  // on avance
    else                               path.Pop();        // cul-de-sac : on recule
}

// Conteneur custom : O(1) garanti pour une pile via NkDeque.
NkStack<int, NkDeque<int>> guaranteed;
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Sequential →](Sequential.md)
