# Les allocateurs

> Couche **Foundation** · NKMemory · La **source unique** de toute mémoire dynamique : l'interface
> `NkAllocator` (avec ses helpers `New`/`Delete`), l'allocateur par défaut du moteur, et la famille
> d'allocateurs spécialisés (pools, arène, multi-niveaux, conteneurs, pont STL).

Tout objet créé dynamiquement dans Nkentseu naît d'un **allocateur**. Plutôt que d'appeler
directement le tas du système (`malloc`/`new`), le moteur passe par une abstraction — `NkAllocator` —
qui sait, en plus d'allouer, *suivre* ce qu'elle distribue : détection de fuites, profiling, budgets
par sous-système, statistiques d'occupation. C'est cette indirection qui rend possible tout ce qu'on
verra dans les autres chapitres de NKMemory (smart pointers, GC, scopes), et c'est elle qui interdit
la corruption du tas Windows.

L'idée maîtresse : **tous** les allocateurs, quelle que soit leur stratégie interne (tas système,
pool de blocs, arène, buddy…), exposent la **même** interface. On peut donc écrire du code qui prend
un `NkAllocator&` sans rien savoir de la stratégie sous-jacente, et brancher l'une ou l'autre selon
le besoin — exactement comme NKContainers est *conscient de l'allocateur* sans connaître son
implémentation.

- **Namespace** : `nkentseu::memory`
- **Headers** : `#include "NKMemory/NkAllocator.h"` (cœur), `NkPoolAllocator.h`,
  `NkPoolAllocatorTyped.h`, `NkMultiLevelAllocator.h`, `NkContainerAllocator.h`, `NkStlAdapter.h`

---

## Allouer et libérer un objet

Dans la grande majorité des cas, on ne manipule pas la mémoire brute : on veut *un objet*. Deux
helpers suffisent, `New` et `Delete`, fournis par n'importe quel `NkAllocator` :

```cpp
auto& alloc = nkentseu::memory::NkGetDefaultAllocator();

Widget* w = alloc.New<Widget>(x, y, "ok");   // alloue + aligne + construit
// ...
alloc.Delete(w);                              // détruit + libère
```

`New<T>(args…)` fait trois choses d'un coup : il réserve la mémoire (`Allocate(sizeof(T),
alignof(T))`), l'aligne correctement **automatiquement**, puis construit l'objet par *placement new*
en lui transmettant vos arguments (perfect-forwarding). S'il ne reste plus de mémoire, il renvoie
`nullptr` plutôt que de lever. `Delete` fait le chemin inverse : il appelle le destructeur `~T()`
puis rend la mémoire. Si l'objet est `nullptr`, `Delete` ne fait rien — pas besoin de tester avant.

`NkGetDefaultAllocator()` renvoie l'allocateur global du moteur (un `NkMallocAllocator`, c'est-à-dire
`malloc`/`free` alignés). C'est lui qu'on utilise par défaut, partout, sauf besoin particulier.

Pour un tableau d'objets, la paire équivalente est `NewArray`/`DeleteArray` :

```cpp
Widget* tab = alloc.NewArray<Widget>(16);   // construit 16 widgets
alloc.DeleteArray(tab);                       // les détruit dans l'ordre inverse
```

La distinction n'est **pas** cosmétique : `NewArray` range discrètement un en-tête (`count` +
`offset`) juste avant le premier élément, ce qui permet à `DeleteArray` de retrouver la base réelle
du bloc et de détruire chaque élément en ordre inverse. Détruire un tableau avec `Delete` (au lieu de
`DeleteArray`) corromprait le pointeur de base, car le bloc ne commence pas là où pointe `tab`.

> **La règle d'or du moteur.** Un objet créé par `New` se libère par `Delete`, un tableau par
> `DeleteArray`, et **toujours via le même allocateur** que celui de l'allocation. Mélanger ces voies
> — par exemple un `delete`/`std::free` standard sur un objet issu de `New`, ou libérer avec `NkFree`
> un buffer rendu par un codec qui a pourtant alloué via `NkAlloc` (même chemin, OK) — provoque une
> corruption du tas (`STATUS_HEAP_CORRUPTION`, code `0xC0000374`) qui ne se manifeste souvent qu'à la
> fermeture, loin de la vraie cause. C'est pour éviter ce piège que toute classe avec une méthode de
> **création** doit exposer la **destruction** symétrique (`Create`↔`Destroy`).

---

## Sous le capot : l'interface brute

Les helpers `New`/`Delete` sont bâtis sur quelques opérations bas niveau que les allocateurs
implémentent réellement. On les utilise rarement en direct, mais il est utile de savoir qu'elles
existent. `Allocate(size, alignment)` réserve un bloc brut aligné (l'alignement **doit** être une
puissance de 2) ; `Deallocate(ptr)` le rend (`nullptr` accepté, c'est un no-op) ; sa variante
*sized* `Deallocate(ptr, size)` permet aux allocateurs qui le peuvent d'aller plus vite ;
`Reallocate(ptr, oldSize, newSize)` redimensionne (par défaut : alloue, copie, libère ; en cas
d'échec rend `nullptr` **et** laisse l'ancien pointeur valide) ; `Calloc(size)` alloue en mettant à
zéro. Une dernière, `Reset()`, libère **tout** d'un seul coup — sans rien désallouer individuellement.

```cpp
void* block = alloc.Allocate(1024, 16);   // 1 Ko aligné sur 16
// ...
alloc.Deallocate(block);
```

`Reset()` est anodine pour l'allocateur par défaut (no-op), mais c'est l'opération **vedette** des
allocateurs à durée de vie groupée (linéaire, arène, pools), comme on va le voir. Pour la version
C-style sans objet C++, le module expose aussi des fonctions globales (`NkAlloc`, `NkAllocZero`,
`NkRealloc`, `NkFree`) qui visent l'allocateur par défaut ou celui qu'on leur passe.

---

## Choisir un allocateur selon l'usage

L'allocateur par défaut (`NkMallocAllocator`, qui s'appuie sur `malloc`/`free` alignés) convient à
presque tout. Mais certaines situations gagnent à utiliser une stratégie spécialisée.

### Le pool de taille fixe, pour les objets homogènes

Quand on crée et détruit en masse des objets de **même taille** — des particules, des nœuds d'arbre,
des événements — le tas système devient un mauvais choix : il fragmente et chaque allocation coûte
cher. Un *pool* résout ça en pré-réservant un grand bloc découpé en cases de taille fixe ; allouer
revient alors à prendre la première case libre (temps constant, zéro fragmentation).

`NkFixedPoolAllocator` est un **template** : la taille de bloc et le nombre de cases sont fixés à la
compilation. Son constructeur ne prend qu'un nom (pour le débogage) :

```cpp
template<nk_size BlockSize, nk_size NumBlocks = 256>
class NkFixedPoolAllocator : public NkAllocator;

NkFixedPoolAllocator<sizeof(Particle), 4096> pool;

Particle* p = pool.New<Particle>(/* … */);   // la taille demandée doit tenir dans BlockSize
// ...
pool.Delete(p);
```

L'intérêt décisif vient en fin de cycle : `pool.Reset()` rend **toutes** les cases libres d'un seul
coup, en temps proportionnel au nombre de blocs — bien plus rapide que libérer chaque objet un par
un. C'est le modèle « allocateur de frame » ou « pool par niveau » : on alloue tout au long de la
frame, puis on jette tout à la fin.

Un point à retenir : `Reset()` ne *détruit* pas les objets, il se contente de récupérer la mémoire.
Si vos objets ont un destructeur non trivial, appelez `Delete` dessus avant ; réservez le `Reset()`
brut aux types simples (POD). Comme un pool a une capacité fixe, vérifiez toujours le retour de
`New`/`Allocate` (`nullptr` = pool plein) ; et notez que `NkFixedPoolAllocator::Allocate` **ignore
l'alignement** et renvoie `nullptr` immédiatement si `size > BlockSize`. Pour l'inspecter,
`GetNumFreeBlocks()` donne le nombre de cases disponibles, `GetUsage()` le taux d'occupation [0–1],
et `Owns(ptr)` indique si un pointeur provient bien de ce pool.

### Les autres stratégies

Quand les tailles varient mais que la durée de vie reste groupée, `NkVariablePoolAllocator` joue un
rôle similaire sans contrainte de taille unique. Pour manipuler des objets typés en gardant
construction/destruction automatiques, `NkPoolAllocatorTyped` (la classe `NkTypedPool<T, …>`) enrobe
un pool fixe. Quand un sous-système mêle petits objets et gros tampons,
`NkMultiLevelAllocator`/`NkAllocTier` aiguille chaque demande vers le « niveau » (Tiny/Small/Medium/
Large) adapté à sa taille, évitant d'éparpiller les petites allocations parmi les grosses. Enfin,
`NkContainerAllocator` est l'allocateur taillé pour les conteneurs et les petites allocations
fréquentes, et `NkStlAdapter` pontent délibérément vers la STL pour réutiliser `std::vector` &
consorts au-dessus d'un allocateur Nkentseu. Tous (sauf l'adaptateur STL et `NkTypedPool`, qui sont
des *wrappers*) partagent l'interface `NkAllocator`, donc tout ce qu'on a vu (`New`, `Delete`,
`Reset`) s'y applique tel quel.

> **En résumé.** Par défaut, `NkGetDefaultAllocator()` et `New`/`Delete` suffisent. Passez à un pool
> fixe pour des objets homogènes et nombreux (et profitez de `Reset()` pour libérer en bloc), à un
> multi-niveaux pour une charge hétérogène, à un container/STL-adapter pour les conteneurs. Quelle
> que soit la stratégie, l'interface ne change pas — et la règle « même voie pour allouer et
> libérer » non plus.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (stratégie,
complexité, cas d'usage) dans la « Référence complète » qui suit. Complexités / `noexcept` entre
crochets quand c'est utile.

### Cœur : interface et helpers (`NkAllocator.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `NkAllocatorBase` | Interface minimale : nom + destruction virtuelle (pas d'allocation). |
| Base | `GetName()` `[noexcept]` | Nom de l'allocateur (débogage, non-possédé). |
| Interface | `NkAllocator` (= `NkIAllocator`) | Interface principale, dérive de `NkAllocatorBase`. |
| Brut (pur) | `Allocate(size, align)` `[=0]`, `Deallocate(ptr)` `[=0]` | Réserve aligné / rend (nullptr = no-op). |
| Brut (défaut) | `Deallocate(ptr, size)`, `Reallocate(ptr, old, new, align)`, `Calloc(size, align)`, `Reset()` `[noexcept]`, `Name()` `[noexcept]` | Sized delete / redim / alloc+zéro / tout libérer / alias de `GetName`. |
| Helpers typés | `New<T>(args…)`, `Delete<T>(ptr)` `[noexcept]` | Alloue+construit / détruit+libère un objet. |
| Helpers typés | `NewArray<T>(count, args…)`, `DeleteArray<T>(ptr)` `[noexcept]` | Tableau (en-tête `count`/`offset`) / destruction inverse. |
| Helpers typés | `As<T>(ptr)` `[noexcept]` | `reinterpret_cast<T*>` (aucun check). |
| Constantes | `NK_MEMORY_DEFAULT_ALIGNMENT` | `alignof(std::max_align_t)`. |
| Enum | `NkMemoryFlag` (NONE/READ/WRITE/EXECUTE/RESERVE/COMMIT/ANONYMOUS) + `\|` `&` `\|=` | Flags pour les mappings virtuels. |

### Allocateurs concrets (`NkAllocator.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Système | `NkNewAllocator` | `operator new`/`delete` alignés. |
| Système | `NkMallocAllocator` | `malloc`/`free` alignés — **allocateur par défaut**. |
| Système | `NkVirtualAllocator` (+ `AllocateVirtual`, `FreeVirtual`, membre `flags`) | Mappings bas niveau (mmap/VirtualAlloc), grandes régions, permissions. |
| Frame | `NkLinearAllocator` (+ `Capacity`/`Used`/`Available`) | Bump pointer `O(1)`, libération en bloc via `Reset`. |
| Frame | `NkArenaAllocator` (+ `Marker`, `CreateMarker`, `FreeToMarker`) | Linéaire avec markers pour rollback partiel. |
| LIFO | `NkStackAllocator` (+ `Capacity`/`Used`) | Pile : seul le dernier bloc se libère. |
| Pool | `NkPoolAllocator` (+ `BlockSize`/`BlockCount`) | Blocs fixes runtime, free-list `O(1)`. |
| Généraliste | `NkFreeListAllocator` (+ `Capacity`) | Tailles variables, coalescing des blocs libres. |
| Généraliste | `NkBuddyAllocator` | Buddy system, tailles arrondies à la puissance de 2. |

### Fonctions globales (`NkAllocator.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Singletons | `NkGetDefaultAllocator`, `NkGetMallocAllocator`, `NkGetNewAllocator`, `NkGetVirtualAllocator` `[noexcept]` | Accès aux allocateurs partagés du module. |
| Config | `NkSetDefaultAllocator(ptr)` `[noexcept]` | Remplace le défaut (au démarrage, non thread-safe). |
| C-style | `NkAlloc`, `NkAllocZero`, `NkRealloc` | Allouer / allouer+zéro / redimensionner. |
| C-style | `NkFree(ptr)`, `NkFree(ptr, size)` | Libérer (variante *sized*). |

### Pools haute performance (`NkPoolAllocator.h`, `NkPoolAllocatorTyped.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Pool fixe | `NkFixedPoolAllocator<BlockSize, NumBlocks=256>` | Pool template `O(1)`, thread-safe (`NkSpinLock`). |
| Pool fixe | `GetNumFreeBlocks`, `GetNumBlocks`/`GetBlockSize` (`static constexpr`), `Owns`, `GetUsage` | Inspection / appartenance / taux d'occupation. |
| Pool variable | `NkVariablePoolAllocator` (+ `GetLiveBytes`/`GetLiveAllocations`/`Owns`) | Tailles variables, header de tracking par allocation. |
| Typé | `NkTypedPool<T, BlockSize, NumBlocks=256>` (+ `New`/`Delete`/`Reset`/`Owns`/`GetUsage`…) | Wrapper typé d'un `NkFixedPoolAllocator`. |
| Fabrique | `MakeTypedPool<T, BlockSize, NumBlocks>(name)` | Crée un `NkTypedPool` (nom exact, **sans** préfixe `Nk`). |

### Multi-niveaux & conteneurs (`NkMultiLevelAllocator.h`, `NkContainerAllocator.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Multi-niveaux | `NkMultiLevelAllocator` (+ `GetStats`, `DumpStats`, constantes `TINY/SMALL_SIZE`…) | Dispatch par taille vers Tiny/Small/Medium/Large. |
| Multi-niveaux | `NkAllocTier` (Tiny/Small/Medium/Large), `NkMultiLevelAllocatorStats`(+`GetFragmentation`), `NkGetMultiLevelAllocator()` | Énumération des tiers / stats / singleton. |
| Conteneurs | `NkContainerAllocator` (+ `GetStats`, constantes `NK_CONTAINER_*`) | 13 size classes, cache TLS sans lock, pages pré-allouées. |
| Conteneurs | `NkContainerAllocatorStats` (= `NkStats`) | Compteurs pages / blocs / grandes allocations. |

### Pont STL (`NkStlAdapter.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Adaptateur | `NkStlAdapter<T, AllocatorType>` (+ `allocate`/`deallocate`/`construct`/`destroy`/`max_size`/`rebind`) | Allocateur conforme STL au-dessus d'un `NkAllocator`. |
| Traits | `std::allocator_traits<NkStlAdapter<…>>` (spécialisation) | Politiques de propagation (move/swap = true, copy = false). |
| Alias | `NkContainerVector<T>`, `NkMallocVector<T>`, `NkPoolVector<T, BlockSize, NumBlocks>` | `std::vector` câblé sur container / malloc / pool fixe. |

---

## Référence complète

Chaque élément est repris ici en détail : stratégie, complexité, et usages dans les différents
domaines du temps réel — rendu, ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU. Les
éléments triviaux sont décrits brièvement ; les pièces maîtresses, **à fond**.

### `NkAllocatorBase` — la racine minimale

C'est le plus petit dénominateur commun : un nom (`GetName()`, un `const nk_char*` non-possédé) et
une destruction virtuelle. Elle ne déclare **aucune** méthode d'allocation, et elle est à la fois
non-copiable et non-déplaçable (un allocateur est une *ressource* qu'on partage par référence, jamais
qu'on duplique). On en hérite rarement directement : on dérive de `NkAllocator`. Son seul intérêt
public est le nom, utilisé partout dans les logs et le profiling pour attribuer une allocation à un
sous-système (« renderer », « audio voices », « ecs chunks »…).

### `NkAllocator` — l'interface principale (à fond)

Tout passe par elle (`NkIAllocator` en est l'alias). Elle définit trois types membres (`Pointer` =
`void*`, `SizeType` = `nk_size`, `AlignType` = `nk_size`) et deux familles de méthodes.

**Les primitives.** `Allocate(size, alignment)` et `Deallocate(ptr)` sont **pures** : chaque
allocateur concret les implémente à sa façon. L'alignement doit être une puissance de 2 ; `size==0`
est un comportement indéfini selon l'implémentation ; `Deallocate(nullptr)` est toujours un no-op,
mais un *double-free* réel reste un UB. Trois méthodes ont une implémentation par défaut qu'on peut
surcharger : `Reallocate` (par défaut alloue/copie/libère — et en cas d'échec **rend `nullptr` sans
invalider l'ancien pointeur**), `Calloc` (Allocate + zéro), et la variante *sized* `Deallocate(ptr,
size)` (par défaut délègue à la version simple). `Reset()` est un no-op par défaut, surchargé par les
allocateurs à durée de vie groupée.

**Les helpers typés** (templates inline). Ce sont eux qu'on utilise au quotidien :

- `New<T>(args…)` — `Allocate(sizeof(T), alignof(T))` puis *placement new* avec perfect-forwarding ;
  renvoie `nullptr` si l'allocation échoue. **À libérer par `Delete<T>`.**
- `Delete<T>(ptr)` — `nullptr` safe ; appelle `ptr->~T()` puis `Deallocate(ptr)`.
- `NewArray<T>(count, args…)` — `count==0` → `nullptr` ; pose un en-tête interne (`count` + `offset`
  aligné sur `alignof(T)`) avant le premier élément, construit chacun. **À libérer par
  `DeleteArray<T>`.**
- `DeleteArray<T>(ptr)` — reconstitue la base via l'offset, détruit les éléments en ordre inverse,
  puis `Deallocate(base)`.
- `As<T>(ptr)` — simple `reinterpret_cast<T*>` (propage `nullptr`, aucun check à l'exécution).

Note : `New/Delete/NewArray/DeleteArray` appellent toujours `Deallocate(ptr)` (jamais la version
*sized*). C'est le point de contact omniprésent du moteur, donc la **symétrie** est sacrée :
- **Rendu / GPU** — un `NkPipeline`, un `NkTexture` créés par `New` se rendent par `Delete` du même
  allocateur ; un buffer de pixels rendu par un codec NKImage (alloué via `NkAlloc`) se libère par
  `NkFree`, jamais `delete[]`.
- **ECS** — chunks d'archétypes, *singleton components* alloués via l'allocateur du monde.
- **Audio** — instances de voix, nœuds de graphe DSP créés/détruits par paires symétriques.
- **UI/2D** — widgets, *draw lists* ; `NewArray` pour un lot de glyphes pré-construits.

### `NkMemoryFlag` & constantes

`NK_MEMORY_DEFAULT_ALIGNMENT` vaut `alignof(std::max_align_t)` : l'alignement neutre par défaut.
`NkMemoryFlag` (READ/WRITE/EXECUTE/RESERVE/COMMIT/ANONYMOUS, combinables par `|`, `&`, `|=`
`constexpr`) décrit les permissions et le mode d'un mapping virtuel ; il n'intervient qu'avec
`NkVirtualAllocator`.

### Allocateurs système : `NkNewAllocator`, `NkMallocAllocator`, `NkVirtualAllocator`

Tous trois adaptent une primitive de l'OS à l'interface `NkAllocator`. **`NkNewAllocator`** route
vers `::operator new`/`delete` avec alignement (`std::align_val_t` en C++17). **`NkMallocAllocator`**
route vers `_aligned_malloc`/`_aligned_free` (Windows) ou `posix_memalign`/`free` (POSIX) ; c'est
**l'allocateur par défaut** du module, et il surcharge en plus la version *sized* de `Deallocate`.
Tous deux conviennent au cas général (un objet par-ci par-là, durée de vie quelconque).

**`NkVirtualAllocator`** est différent : il fait des mappings bas niveau (mmap/VirtualAlloc),
arrondis à la page, pour de **grandes** régions avec permissions r/w/x — la matière première d'un
allocateur de plus haut niveau ou d'un système qui réserve d'énormes plages. Il expose
`AllocateVirtual(size, tag)` / `FreeVirtual(ptr)` et un membre public **modifiable** `flags` (de type
`NkMemoryFlag`, non `mutable` au sens C++) qui pilote
les prochaines allocations virtuelles. Usages : back-end d'arènes massives, streaming de monde ouvert
(réserver l'espace d'adressage, *commit* à la demande), heaps GPU staging.

### `NkLinearAllocator` — le bump pointer (à fond)

C'est l'archétype de l'allocateur **frame-based**. Il tient une grande région et un offset qui
*monte* : chaque `Allocate` avance l'offset (`O(1)`, zéro recherche), et il n'y a **pas** de
libération individuelle — seul `Reset()` ramène l'offset à zéro et rend tout d'un coup (le dernier
bloc peut toutefois être désalloué, optimisation *top-of-stack*). `Capacity()` / `Used()` /
`Available()` renseignent l'occupation. C'est l'outil rêvé pour la mémoire à durée de vie d'une
frame :
- **Rendu** — *command buffers*, structures transitoires reconstruites chaque frame puis jetées d'un
  `Reset()`.
- **Physique** — paires de contacts du *broadphase*, scratch d'un solveur d'îlots.
- **IA/gameplay** — résultats de requêtes spatiales d'une frame (voisins, *raycasts* groupés).

### `NkArenaAllocator` — linéaire avec markers

Même principe que le linéaire, mais avec des **markers** (type opaque `Marker`) pour des rollbacks
partiels : `CreateMarker()` capture l'offset courant, `FreeToMarker(m)` libère tout ce qui a été
alloué **après** le marker. On obtient des « sous-portées » imbriquées sans coût : alloc/dealloc en
`O(1)`, et `Reset()` vide tout. Usages : passes de rendu imbriquées, parsing récursif (scope par
nœud), simulation où l'on teste une hypothèse puis revient en arrière (IA, *what-if* physique).

### `NkStackAllocator` — la pile LIFO

Variante stricte : on ne peut désallouer que **le dernier** bloc alloué (discipline LIFO, via un
en-tête `previousOffset`). `Capacity()`/`Used()` pour l'inspection. C'est le modèle des appels
imbriqués où la durée de vie suit exactement la pile d'appels : sous-systèmes empilés au démarrage,
allocations temporaires d'un algorithme récursif, contextes de scope.

### `NkPoolAllocator` — pool de blocs fixes (runtime)

À distinguer du template `NkFixedPoolAllocator` : ici la taille de bloc et le nombre de blocs sont
fixés **à l'exécution** (constructeur `NkPoolAllocator(blockSize, blockCount)`). La free-list est
*in-place* (`O(1)` pour allouer comme pour libérer), `Reset()` la reconstruit, et `BlockSize()` /
`BlockCount()` l'inspectent. Idéal pour des objets homogènes dont la taille n'est connue qu'au
runtime (taille d'un *component* déterminée par réflexion, taille d'un nœud chargée d'un fichier) :
- **ECS** — un pool par type de component.
- **Particules / audio** — voix, particules : on recycle des cases identiques sans fragmenter.

### `NkFreeListAllocator` & `NkBuddyAllocator`

`NkFreeListAllocator` est le **généraliste** : tailles variables, avec *coalescing* bidirectionnel
des blocs libres adjacents pour combattre la fragmentation (en-têtes `BlockHeader` size/next/prev/
isFree). `Capacity()` renseigne la région totale. C'est le bon défaut quand un sous-système gère un
budget mémoire fermé avec des allocations hétérogènes mais de durées de vie variées (heaps de scène,
caches d'assets).

`NkBuddyAllocator` arrondit chaque demande à la puissance de 2 supérieure et fusionne les *buddies*
libres ; il s'appuie en interne sur un `NkFreeListAllocator`. Son intérêt : allocation/désallocation
prévisibles et anti-fragmentation forte, au prix d'un peu de perte interne (arrondi). Usage typique :
sous-allocateurs GPU/pools de heaps où l'on veut des tailles alignées sur des puissances de 2.

### Fonctions globales — singletons et API C-style

`NkGetDefaultAllocator()` (un `NkMallocAllocator`, *Meyer's singleton* thread-safe après init) est le
point d'entrée par défaut ; `NkGetMallocAllocator` / `NkGetNewAllocator` / `NkGetVirtualAllocator`
donnent accès aux instances partagées. `NkSetDefaultAllocator(ptr)` remplace le défaut (`nullptr` =
retour à Malloc) — **non thread-safe**, à n'appeler qu'au tout début, avant le moindre usage. La
couche C-style — `NkAlloc(size, alloc, align)`, `NkAllocZero(count, size, …)`, `NkRealloc(ptr, old,
new, …)`, `NkFree(ptr, alloc)` et sa variante *sized* `NkFree(ptr, size, alloc)` — sert quand on n'a
pas d'objet C++ : buffers d'IO, tampons retournés par un codec, interop C. Là encore, **un bloc
`NkAlloc` se libère par `NkFree`**, jamais `std::free`.

### `NkFixedPoolAllocator<BlockSize, NumBlocks>` — pool template (à fond)

Le pool **le plus rapide** : taille de bloc et compte fixés à la compilation, buffer contigu
pré-alloué (via `::operator new`), free-list *in-place* → `Allocate` et `Deallocate` en `O(1)`,
thread-safe via `NkSpinLock`. Deux `static_assert` gardent l'invariant (`BlockSize >= sizeof(nk_size)`
pour loger le chaînage, `NumBlocks > 0`). `Allocate` renvoie `nullptr` **sans prendre le lock** si
`size > BlockSize`, et **ignore l'alignement** demandé. `Reset()` reconstruit la free-list en
`O(NumBlocks)` mais **ne détruit pas** les objets. Inspection : `GetNumFreeBlocks()`,
`GetNumBlocks()`/`GetBlockSize()` (`static constexpr`), `Owns(ptr)` (test d'intervalle, sans lock),
`GetUsage()` (taux [0–1]). Le destructeur libère le buffer (`::operator delete`) sans détruire les
objets restants — d'où la discipline « `Delete` chaque objet à destructeur non trivial avant le
`Reset`/la fin de vie ».
- **Rendu** — *draw commands*, instances homogènes.
- **Gameplay** — projectiles, événements, nœuds d'un quadtree.
- **Audio** — voix d'un mixeur (taille fixe, recyclage permanent).

### `NkVariablePoolAllocator` — pool de tailles variables

Quand les tailles diffèrent mais qu'on veut un suivi par allocation, ce pool s'appuie sur un backend
`malloc` plus un en-tête de tracking (magic, taille demandée, offset, chaînage prev/next).
`Allocate`/`Deallocate` sont `O(1)` ; `size==0` → `nullptr` ; un alignement non-puissance-de-2 tombe
sur l'alignement par défaut ; en debug, `Deallocate` vérifie le *magic* et **ignore silencieusement**
un pointeur corrompu. `Reset()` (`O(n)`) libère toutes les allocations vivantes. `GetLiveBytes()` /
`GetLiveAllocations()` donnent l'occupation réelle (hors overhead), `Owns(ptr)` parcourt la liste
(`O(n)`). Le define `NKENTSEU_DISABLE_POOL_DEBUG` retire les checks magic/Owns en release.

### `NkTypedPool<T, BlockSize, NumBlocks>` — le pool typé (`NkPoolAllocatorTyped.h`)

Header-only, il **n'hérite pas** de `NkAllocator` : il enrobe un `NkFixedPoolAllocator` pour offrir
des `New`/`Delete` **typés** avec construction/destruction automatiques. `New(args…)` alloue puis
*placement new* (et, si les exceptions sont activées, libère et rend `nullptr` si le constructeur
lève) ; `Delete(ptr)` est `nullptr` safe et appelle `~T()` puis libère ; `Reset()` délègue **sans
détruire** les objets. Trois `static_assert` (`sizeof(T) <= BlockSize`, `BlockSize >= sizeof(nk_size)`,
`NumBlocks > 0`). Inspection identique au pool fixe (`GetNumFreeBlocks`, `GetUsage`, `Owns`,
`GetNumBlocks`/`GetBlockSize`). La fabrique libre **`MakeTypedPool<T, BlockSize, NumBlocks>(name)`**
(nom exact, **sans** préfixe `Nk`) en construit un. Parfait pour un pool d'objets d'un seul type :
particules typées, nœuds d'arbre, *events* d'un même struct.

### `NkMultiLevelAllocator` & `NkAllocTier` (à fond)

Le « routeur » par taille. Chaque demande est dirigée vers un **tier** selon `size` :
`NkAllocTier::Tiny` (≤ 64 o → `NkFixedPoolAllocator<64, 4096>`), `Small` (≤ 1 Ko →
`NkFixedPoolAllocator<1024, 1024>`), `Medium` (≤ 1 Mo → `NkVariablePoolAllocator`), et `Large`
(> 1 Mo → `malloc`/`free` direct). Un en-tête de dispatch (`magic` + tier + taille + offset) précède
chaque bloc pour router le `Deallocate` au bon tier ; le tout est thread-safe (`NkSpinLock`). Les
seuils sont publics (`TINY_SIZE`/`TINY_COUNT`, `SMALL_SIZE`/`SMALL_COUNT`, `MEDIUM_THRESHOLD`).
`Reallocate` n'est jamais *in-place* (alloue/copie/libère). `GetStats()` renvoie
`NkMultiLevelAllocatorStats` (occupation par tier + compteurs Large + `GetFragmentation()`),
`DumpStats()` les écrit dans le log. Le singleton `NkGetMultiLevelAllocator()` en fournit une
instance partagée.

**Deux pièges importants.** `Reset()` réinitialise Tiny/Small/Medium et les compteurs Large, mais
**ne libère pas** les `malloc` LARGE — pas plus que le destructeur ; les grosses allocations
réclament une libération manuelle. Et comme le dispatch tient compte de l'en-tête + padding, une
demande de 64 o peut basculer en tier *Small*. Usage : un allocateur « universel » de sous-système
qui mélange des nuées de petits objets et quelques gros tampons (heap d'un module, scène) sans
éparpiller les petits parmi les grands.

### `NkContainerAllocator` (à fond)

L'allocateur taillé pour les **conteneurs** et les petites allocations fréquentes : 13 *size classes*
(16, 32, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048), un **cache TLS sans lock** par
thread (fast path), des pages pré-allouées, un compteur de génération qui invalide le cache TLS au
`Reset`, un registry d'instances anti-dangling et des locks fins par size class. Une demande ≤ 2048 o
(et alignement ≤ 16) prend la *fast path* par size class ; au-delà, on passe par le backing
allocator. `Reallocate` est un no-op si l'on rétrécit, sinon alloue/copie/libère. `Reset()` incrémente
la génération (invalide le TLS) et rend pages + grandes allocations. `GetStats()` renvoie
`NkContainerAllocatorStats` (= `NkStats`). Il **n'a pas** de `Owns()` public. Constantes exposées :
`NK_CONTAINER_CLASS_COUNT`, `NK_CONTAINER_LARGE_CLASS`, `NK_CONTAINER_SMALL_ALIGNMENT`,
`NK_CONTAINER_TLS_CACHE_LIMIT`/`RETAIN`. C'est l'allocateur de prédilection des `NkVector`/`NkList`/…
et de tout ce qui *churn* beaucoup de petites allocations (UI, événements, strings courtes).

### `NkStlAdapter<T, AllocatorType>` — le pont STL

Header-only, il **ponte délibérément vers la STL** : il fournit l'interface `std::allocator`
(`allocate`/`deallocate`/`construct`/`destroy`/`max_size`/`rebind` + les `value_type`/`pointer`/…)
au-dessus de n'importe quel `AllocatorType` Nkentseu, qu'il détient **par pointeur non-possédé**
(l'allocateur doit donc survivre au conteneur). `allocate(n)` route vers `Alloc->Allocate(n*sizeof(T),
alignof(T))` (lève `std::bad_alloc` si les exceptions sont activées, sinon rend `nullptr` ; tombe sur
`NkGetDefaultAllocator()` si le pointeur est nul). La spécialisation de `std::allocator_traits` fixe
les politiques de propagation : **move** et **swap** propagent (`true_type`), **copy** non
(`false_type`), `is_always_equal = false`. Trois alias prêts à l'emploi câblent `std::vector` :
`NkContainerVector<T>` (sur `NkContainerAllocator`), `NkMallocVector<T>` (sur `NkMallocAllocator`),
`NkPoolVector<T, BlockSize, NumBlocks>` (sur `NkFixedPoolAllocator`). C'est la passerelle pour
réutiliser des conteneurs STL existants (ou du code tiers) tout en gardant le contrôle mémoire du
moteur — typiquement en outillage, IO, ou intégration de bibliothèques externes.

---

### Exemple récapitulatif

```cpp
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkPoolAllocatorTyped.h"
#include "NKMemory/NkStlAdapter.h"
using namespace nkentseu::memory;

// 1) Cas par défaut : New/Delete sur l'allocateur global.
auto& alloc = NkGetDefaultAllocator();          // NkMallocAllocator
Widget* w = alloc.New<Widget>(10, 20, "ok");
alloc.Delete(w);                                 // symétrique, même allocateur

// 2) Tableau : NewArray <-> DeleteArray (en-tête interne, ne PAS mélanger avec Delete).
Particle* batch = alloc.NewArray<Particle>(256);
alloc.DeleteArray(batch);

// 3) Arène + marker : on alloue du scratch, puis on rembobine.
NkArenaAllocator arena(1u << 20);                // 1 Mo
auto m = arena.CreateMarker();
void* scratch = arena.Allocate(4096, 16);
arena.FreeToMarker(m);                            // libère tout après le marker

// 4) Pool typé : objets homogènes, New/Delete sans toucher au tas système.
auto pool = MakeTypedPool<Particle, sizeof(Particle), 4096>("particles");
Particle* p = pool.New(/* … */);
pool.Delete(p);
// pool.Reset();  // rend toutes les cases, mais NE détruit PAS les objets vivants

// 5) C-style pour un buffer brut (IO/codec) : NkAlloc <-> NkFree (jamais std::free).
void* buf = NkAlloc(1024);
NkFree(buf);

// 6) STL au-dessus d'un allocateur Nkentseu (l'allocateur doit survivre au conteneur).
NkContainerAllocator a;
NkContainerVector<int> v(&a);
v.push_back(42);
```

---

[← Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md) · [Les smart pointers →](SmartPointers.md)
