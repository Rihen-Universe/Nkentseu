# Les conteneurs amis du cache

> Couche **Foundation** · NKContainers · Trois structures **contiguës** taillées pour la vitesse
> mémoire : le tableau de taille fixe `NkArray`, le pool d'objets `NkPool`, et le tampon circulaire
> `NkRingBuffer`.

Sur du matériel moderne, le coût d'un programme n'est plus le nombre d'opérations mais le nombre de
**défauts de cache** : un accès qui rate le cache peut coûter cent fois un accès qui le touche. La
règle d'or du temps réel en découle : **garder les données dans un seul bloc contigu, et éviter les
allocations dispersées**. Les conteneurs de cette page sont conçus exactement pour ça. Ils ne
remplacent pas les [conteneurs séquentiels](Sequential.md) (qui répondent à la question « dans quel
ordre ? ») ; ils répondent à une question différente — « comment **placer** mes objets en mémoire
pour que le CPU les avale au débit maximal ? »

Aucun des trois ne réagence ni ne réalloue en cours de route : leur capacité est **fixée à la
construction**. C'est précisément ce qui les rend prévisibles. `NkArray` vit sur la **pile**,
`NkPool` et `NkRingBuffer` allouent **un seul bloc** sur le tas via un `Allocator` (par défaut
`memory::NkGetDefaultAllocator()`) — jamais par `new`/`delete`, toute la mémoire passe par NKMemory.
Les deux derniers gèrent la durée de vie des objets à la main (placement new + appel explicite du
destructeur), conformément à la règle zéro-STL du moteur.

- **Namespace** : `nkentseu` (les types sont au niveau `nkentseu::`, **pas** `nkentseu::containers`)
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## Le tableau de taille fixe : `NkArray`

`NkArray<T, N>` est le plus simple des trois : un tableau C `T[N]` enveloppé dans une coquille zéro
coût. La taille `N` est un **paramètre de template**, connue à la compilation, et le tableau vit sur
la **pile** — il n'y a aucune allocation, aucun pointeur de gestion, aucune métadonnée. Son
`sizeof` vaut exactement `sizeof(T) * N`, et son agencement mémoire est identique à celui de
`std::array`. C'est le conteneur que l'on prend quand on sait **dès l'écriture du code** combien
d'éléments on aura : les composantes d'une matrice 4×4, les quatre coins d'un quad, les huit sommets
d'une boîte englobante, un petit historique de longueur fixe.

Le membre `mData` est **public**, précisément pour autoriser l'initialisation par agrégat à la C :

```cpp
NkArray<int, 3> a = { 1, 2, 3 };     // aggregate init, possible car mData est public
int second = a[1];                    // accès O(1), assert debug si hors bornes
renderer.Upload(a.Data(), a.Size());  // Data() = pointeur brut contigu, prêt pour le GPU
```

Ce n'est **pas** un `NkVector` : `NkArray` ne grandit jamais, n'a ni `PushBack`, ni `Insert`, ni
`Reserve` — sa taille est gravée dans le type. Et comme il vit sur la pile, un `N` énorme provoque un
*stack overflow* : pour de grandes tailles, restez sur un conteneur tas. À l'inverse, pour de petits
`N`, il est imbattable — aucune indirection, tout en registre/cache.

> **En résumé.** `NkArray<T, N>` = tableau C de taille fixe **sur la pile**, `sizeof` = `sizeof(T)*N`,
> zéro allocation, accès `O(1)`. Le choix quand `N` est connu à la compilation. `mData` public pour
> l'init `{...}`. Pas de croissance ; attention au stack overflow si `N` est grand.

---

## Le pool d'objets : `NkPool`

Quand on **crée et détruit sans cesse** des objets de même type — particules, balles, nœuds d'un
arbre, composants — appeler l'allocateur général à chaque fois est lent et **fragmente** le tas.
`NkPool<T>` règle le problème : il réserve **un seul bloc contigu** de `capacity` slots à la
construction, puis distribue et reprend les slots en **temps constant** grâce à une *free list
intrusive*. Quand un slot est libre, il stocke (dans sa propre mémoire) un pointeur vers le slot
libre suivant ; allouer = retirer la tête de cette liste, désallouer = la remettre en tête. Aucune
recherche, aucune allocation système après le démarrage.

```cpp
NkPool<Particle> pool(10000);          // un seul bloc de 10000 slots, free list chaînée
Particle* p = pool.Construct(pos, vel); // Allocate() + placement new ; nullptr si plein
// ...
pool.Destroy(p);                        // ~Particle() puis remet le slot dans la free list
```

Point crucial : le pool **ne gère pas la durée de vie tout seul**. `Allocate()` rend un slot brut
*sans construire* le `T` (à vous le placement new), et `Deallocate()` rend le slot *sans détruire*
le `T`. Les helpers `Construct`/`Destroy` font le couplage pour vous, mais même `~NkPool`, `Reset()`
et `Clear()` **n'appellent aucun destructeur** : ce sont des opérations purement mémoire. Oublier de
`Destroy` un objet non trivial (un `NkString`, par exemple) avant de réinitialiser le pool **fuit**
son buffer. C'est le prix de la vitesse : le contrôle total de la durée de vie vous revient.

Ce n'est **pas** un conteneur que l'on parcourt : un pool ne sait pas quels slots sont actifs (il ne
trace pas les objets vivants). Si vous devez itérer sur les objets, gardez vous-même la liste de
leurs pointeurs.

> **En résumé.** `NkPool<T>` = bloc fixe de `capacity` slots, allocation/désallocation `O(1)` via
> free list intrusive, zéro fragmentation. Vous gérez la durée de vie : `Construct`/`Destroy` (ou
> `Allocate`+placement new / destructeur+`Deallocate`). **Aucun destructeur automatique**, même au
> `Reset`/`Clear`/destructeur. Vérifiez le `nullptr` quand le pool est plein.

---

## Le tampon circulaire : `NkRingBuffer`

`NkRingBuffer<T>` est une file FIFO à **capacité fixe** qui se referme sur elle-même. On écrit à une
tête (`Push`), on lit à une queue (`Pop`), et les index repartent à zéro en bout de bloc (wrapping
modulo). Sa particularité — et son piège — est l'**écrasement automatique** : pousser dans un buffer
plein **détruit silencieusement le plus ancien** élément pour faire de la place. C'est exactement ce
qu'on veut pour un **historique borné** : les 60 derniers temps de frame, les N dernières lignes de
log, une fenêtre glissante d'échantillons audio, un journal d'événements récents.

```cpp
NkRingBuffer<float> frameTimes(60);   // capacité 60, écrasement quand plein
frameTimes.Push(dt);                   // si plein : jette le plus vieux, sans prévenir
float oldest = frameTimes.Front();     // le plus ancien encore présent
float newest = frameTimes.Back();      // le plus récent
for (usize i = 0; i < frameTimes.Size(); ++i)
    accumulate(frameTimes[i]);         // operator[] indexe dans l'ordre FIFO logique
```

Contrairement au pool, le ring buffer **gère bien la durée de vie** : `Pop`, `PopDiscard`, `Clear`
et le destructeur appellent les destructeurs des éléments actifs. Et contrairement aux conteneurs
séquentiels, il n'a **pas d'itérateurs** (donc pas de `range-based for`) : on le parcourt
manuellement via `operator[]`, qui indexe dans l'**ordre logique** (0 = le plus ancien), jamais
l'index physique du bloc.

Ce n'est **pas** une file extensible : si la perte de données est inacceptable, testez `!IsFull()`
**avant** chaque `Push`. Et `Front`/`Back`/`Pop` sur un buffer vide partent en assert/UB — vérifiez
`!Empty()` d'abord.

> **En résumé.** `NkRingBuffer<T>` = FIFO à capacité fixe, `Push`/`Pop` en `O(1)`, **écrasement
> silencieux du plus ancien** quand plein. Idéal pour historiques bornés et fenêtres glissantes.
> Détruit ses éléments (au contraire de `NkPool`). Pas d'itérateurs : parcourir via `operator[]`
> (ordre FIFO). Gardez `PopDiscard` quand la valeur de retour est inutile.

---

## Aperçu de l'API

Seul `NkArray` expose en plus les variantes STL minuscules (`size`, `empty`, `begin`, `end`,
`data`, `swap`…) pour le `range-based for` ; `NkPool` et `NkRingBuffer` n'offrent que l'API
`PascalCase`. Complexités entre crochets.

### `NkArray<T, N>` — tableau de taille fixe (pile)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `mData` (membre **public**) | Tableau C sous-jacent ; autorise l'init par agrégat `{...}` |
| Accès | `At(i)` `[O(1)]`, `operator[](i)` `[O(1)]`, `Front`, `Back`, `Data` | Borné (assert) / non vérifié / extrémités / pointeur brut contigu |
| Itération | `begin`/`end`, `cbegin`/`cend`, `rbegin`/`rend` | Itérateurs = pointeurs ; `range-based for` ; sens inverse |
| Capacité | `Empty` `[O(1)]`, `Size` `[O(1)]`, `MaxSize` `[O(1)]` | Vide ? / nombre `N` / capacité `N` (compile-time) |
| Contenu | `Fill(value)` `[O(N)]`, `Swap(other)` `[O(N)]` | Remplir tout / échanger élément par élément |
| Spécialisation | `NkArray<T, 0>` | Tableau vide : `Size`=0, `Empty`=true, `Fill`/`Swap` no-op ; `operator[]` interdit |
| Libre | `NkSwap(a, b)`, `operator==` `[O(N)]`, `operator!=` | Échange ADL ; égalité élément par élément (short-circuit) |

### `NkPool<T, Allocator>` — pool d'objets à free list (tas)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkPool(capacity)`, `NkPool(capacity, allocator)` | Bloc fixe + free list chaînée `[O(capacity)]` ; **non copiable** |
| Bas niveau | `Allocate()` `[O(1)]`, `Deallocate(ptr)` `[O(1)]` | Pop/push de la free list ; **ne construit/détruit pas** `T` |
| Durée de vie | `Construct(args…)` `[O(1)]`, `Destroy(ptr)` `[O(1)]` | `Allocate`+placement new / destructeur+`Deallocate` |
| Métadonnées | `Capacity`, `Allocated`, `Available`, `IsFull`, `Empty` | Total / actifs / libres / plein ? / aucun actif ? (tous `O(1)`) |
| Avancé | `Owns(ptr)` `[O(1)]`, `Reset()` `[O(capacity)]`, `Clear()` | Appartient au bloc ? / reconstruit la free list (**sans détruire**) |

### `NkRingBuffer<T, Allocator>` — tampon circulaire FIFO (tas)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRingBuffer(capacity)`, `(capacity, alloc)`, `(initList)`, copie, déplacement | Capacité fixe ; init par liste (peut écraser) ; copie profonde `[O(N)]` ; move `[O(1)]` |
| Affectation | `operator=(initList)`, `operator=(copie)`, `operator=(move)` | Réaffecte par liste / copie profonde / transfert `O(1)` |
| Accès | `Front` `[O(1)]`, `Back` `[O(1)]`, `operator[](i)` `[O(1)]` | Plus ancien / plus récent / position **logique FIFO** |
| Métadonnées | `Empty`, `IsFull`, `Size`, `Capacity` | Vide ? / plein ? / actifs / capacité (tous `O(1)`) |
| Modification | `Push(value)` `[O(1)]`, `Push(T&&)`, `Emplace(args…)`, `Pop()` `[O(1)]`, `PopDiscard()` `[O(1)]`, `Clear()` `[O(N)]`, `Swap` `[O(1)]` | Enfiler (**écrase** si plein) / défiler avec ou sans copie / vider / échanger |
| Libre | `NkSwap(a, b)` | Échange ADL `O(1)` |

---

## Référence complète

### Choisir : lequel pour quel besoin ?

Le critère n'est pas l'ordre (comme pour les séquentiels) mais le **schéma mémoire** :

- **Vous connaissez `N` à la compilation et il est petit** → `NkArray`. Zéro allocation, sur la pile.
- **Vous créez/détruisez en rafale des objets de même type** → `NkPool`. `O(1)` sans fragmentation.
- **Vous voulez une file FIFO bornée qui jette le plus vieux** → `NkRingBuffer`. Historique glissant.

| Aspect | `NkArray` | `NkPool` | `NkRingBuffer` |
|--------|-----------|----------|----------------|
| Emplacement mémoire | **pile** | tas (1 bloc) | tas (1 bloc) |
| Taille fixée à | la **compilation** (`N`) | la construction | la construction |
| Allocation par op | **aucune** | `O(1)` (free list) | `O(1)` (slot fixe) |
| Durée de vie auto | n/a (valeurs) | **non** (jamais) | **oui** (Pop/Clear/dtor) |
| Itérateurs | oui (= pointeurs) | non | non (`operator[]`) |
| Plein → comportement | n/a (taille gravée) | `Allocate` rend `nullptr` | **écrase le plus ancien** |

### `NkArray` à fond

**Sur la pile, sans frais.** `NkArray` n'est rien de plus qu'un `T[N]` décoré de méthodes. Son
`sizeof` est exactement `sizeof(T)*N`, son agencement est celui de `std::array`, et le membre
`mData` est public pour que `NkArray<int,3> a = {1,2,3};` compile comme un agrégat. La taille vit
dans le **type** : `Size()`, `MaxSize()` et `Empty()` sont `constexpr` et résolus à la compilation
(`static_assert(a.Size() == 3)` est légal).

**Accès sûr ou rapide.** `At(i)` et `operator[](i)` rendent tous deux une référence en `O(1)` ; la
différence est l'`NKENTSEU_ASSERT(i < N)` en debug (release : `operator[]` ne vérifie plus rien).
`Front()`/`Back()` désignent le premier/dernier élément (précondition `N > 0`), et `Data()` livre le
**pointeur brut contigu** — l'interface vers les API C et le GPU. Les itérateurs *sont* des
pointeurs, d'où la compatibilité immédiate avec le `range-based for` et les variantes inverses
`rbegin`/`rend`.

`Fill(value)` écrit la même valeur dans les `N` cases (`O(N)`), `Swap(other)` échange élément par
élément via `traits::NkSwap`. La **spécialisation `NkArray<T, 0>`** existe pour le code générique :
elle n'a pas de `mData`, rapporte `Size()`=0 et `Empty()`=true, ses `Fill`/`Swap` sont des no-op, et
son `operator[]` part en assert (ne l'appelez jamais). Les fonctions libres `operator==`/`operator!=`
comparent élément par élément (`O(N)`, court-circuit ; `N=0` → égaux) ; il n'y a **pas** de
comparaison lexicographique (`<`, `>`), ni de `find`/`contains`.

Cas d'usage, par domaine :
- **Rendu / GPU** — les 4 sommets d'un quad, les coins d'un AABB, un petit *push constant* : envoyés
  tels quels via `Data()`/`Size()`, sans la moindre allocation dans la boucle de dessin.
- **Math / physique** — stocker les colonnes d'une matrice, les coefficients d'un polynôme, un jeu
  fixe de plans de *frustum* (6) à tester contre une sphère.
- **Animation** — les poids d'un *blend* de quelques clips, les indices d'os influençant un sommet
  (souvent 4), pré-dimensionnés une fois pour toutes.
- **Gameplay / IA** — les 8 voisins d'une case de grille, les directions cardinales, un petit tampon
  d'entrées récentes pour détecter un combo.
- **Audio** — les gains d'un petit bus de mixage à N canaux, les coefficients d'un filtre IIR
  d'ordre fixe.
- **UI / 2D** — les 4 marges d'un widget, les coins arrondis d'un rectangle, une palette fixe.

### `NkPool` à fond

**La free list intrusive.** Le pool alloue à la construction un tableau de `capacity` `Node`, où
`union Node { T Object; Node* Next; }` — donc `sizeof(Node) = max(sizeof(T), sizeof(Node*))`. Tant
qu'un slot est libre, sa mémoire sert à stocker le pointeur `Next` vers le slot libre suivant : la
free list ne coûte **rien** de plus que le bloc lui-même. La construction chaîne tous les slots en
`O(capacity)` ; ensuite, `Allocate()` retire la tête (`O(1)`, rend `nullptr` si plein) et
`Deallocate()` la remet en tête (`O(1)`). C'est aussi rapide qu'un allocateur peut l'être, et sans
fragmentation puisque tous les objets vivent dans le même bloc contigu (excellent pour le cache lors
d'un parcours, si vous tenez vous-même la liste des actifs).

**Vous possédez la durée de vie.** `Allocate`/`Deallocate` ne touchent **jamais** au constructeur ni
au destructeur de `T` — ce sont des opérations purement mémoire. Pour le confort, `Construct(args…)`
combine `Allocate` + placement new (via `traits::NkForward`) et `Destroy(ptr)` combine destructeur +
`Deallocate`. Mais attention : **rien** ne détruit automatiquement les objets actifs, ni `~NkPool`,
ni `Reset()`, ni `Clear()` (ces deux derniers se contentent de reconstruire la free list en
`O(capacity)` et de remettre `Allocated` à 0). Sur un type non trivial, réinitialiser sans `Destroy`
préalable **fuit** (un `NkString` perdrait son buffer). De même, détruisez tous vos objets *avant* de
détruire le pool. `Owns(ptr)` teste l'appartenance au bloc (`O(1)`, sans garantir que le slot soit
alloué). Le pool est **non copiable** et **thread-unsafe** (synchronisez à l'extérieur).

Cas d'usage, par domaine :
- **Rendu** — recycler des objets transitoires par frame (commandes de dessin, requêtes de
  *culling*) sans repasser par l'allocateur général ; tout reste contigu donc *cache-friendly*.
- **ECS** — stocker les instances d'un composant d'un même type, créées/détruites au gré du *spawn*
  et de la mort des entités, en `O(1)` et sans fragmenter le tas.
- **Physique** — un pool de *contacts* ou de *manifolds* régénérés à chaque pas de simulation ;
  `Reset()` en début de frame (types triviaux) pour tout libérer d'un coup.
- **Animation** — pools d'instances de *clips* en cours, de *poses* temporaires, alloués/relâchés au
  fil des transitions.
- **Gameplay / IA** — particules, projectiles, *waypoints*, nœuds d'un arbre de recherche (A\*) : le
  cas d'école, des milliers de créations/destructions par seconde.
- **Audio** — un pool de voix : `Construct` quand un son démarre, `Destroy` quand il s'éteint, sans
  jamais allouer dans le thread temps réel.

### `NkRingBuffer` à fond

**Le wrapping et l'écrasement.** Le ring buffer alloue un bloc de `capacity` `T` (champ `mCapacity`)
et maintient trois index : `mTail` (lecture), `mHead` (écriture) et `mSize` (actifs, entre 0 et
`capacity`), les deux premiers repliés modulo la capacité. `Push(value)` construit en place (placement new) en tête puis avance `mHead` ;
si le buffer est **plein**, il détruit d'abord l'élément en `mHead`, le remplace, et avance `mHead`
**et** `mTail` — c'est l'**écrasement silencieux** du plus ancien. Aucun signal n'est émis : si la
perte est inacceptable, testez `!IsFull()` avant. `Pop()` copie l'élément de queue, le détruit,
avance `mTail`, décrémente `mSize` et **retourne la copie** ; `PopDiscard()` fait pareil **sans la
copie de retour** — préférez-le quand la valeur ne vous sert pas (économie d'une copie). Sous C++11,
`Push(T&&)` (move via `traits::NkMove`) et `Emplace(args…)` (construction in-place via
`traits::NkForward`) évitent les copies pour les types lourds (`NkString`, `NkVector`).

**Vie des éléments et accès.** Contrairement au pool, le ring buffer **détruit** bien ses éléments
actifs : `Pop`, `PopDiscard`, `Clear()` (qui boucle des `Pop`) et le destructeur (qui appelle
`Clear()` puis libère le bloc) passent tous le destructeur. `Front()`/`Back()` donnent le plus ancien
/ le plus récent (`O(1)`, assert `!Empty()`), et `operator[](i)` indexe dans l'**ordre logique
FIFO** : `0` est toujours le plus ancien, `(mTail + i) % capacity` en interne — jamais l'index
physique. Modifier un élément via une référence **ne change pas** son rang FIFO. Il n'y a **pas**
d'itérateurs (donc pas de `range-based for`), pas de `Peek`, pas d'opérations en bloc : on itère à la
main de `0` à `Size()`. La copie est profonde (`O(N)`, préserve têtes/queue), le move `O(1)`, et
`Swap`/`NkSwap` échangent tous les membres en `O(1)`. Le conteneur est **thread-unsafe**.

Cas d'usage, par domaine :
- **Rendu / profilage** — fenêtre glissante des derniers temps de frame pour afficher un FPS lissé ;
  `Push(dt)` à chaque frame, le plus vieux tombe tout seul.
- **Audio** — file d'échantillons producteur/consommateur, ligne à retard (echo/reverb) où l'on lit
  un *tap* en arrière via `operator[]`, tampon d'entrée micro borné.
- **Gameplay / réseau** — historique d'états pour la réconciliation client, *input buffer* des
  dernières commandes (détection de combos, *rollback*), file d'événements récents.
- **IA** — mémoire courte d'un agent (les N derniers stimuli perçus), trace de positions pour un
  comportement de poursuite.
- **UI / log** — console qui ne garde que les N dernières lignes, *toasts* récents, historique de
  saisie navigable.
- **IO** — tampon circulaire de lecture/écriture sur un flux, journal d'événements à taille bornée.

### Le socle commun et les différences

- **Tous cache-friendly.** Bloc contigu unique : `NkArray` sur la pile, `NkPool`/`NkRingBuffer` un
  seul bloc tas via `Allocator` (défaut `memory::NkGetDefaultAllocator()`). Voir
  [NKMemory](../NKMemory.md).
- **Capacité figée.** Aucun des trois ne réalloue ni ne réagence : prévisibilité totale, pas de pic
  de latence caché. La capacité se choisit une fois.
- **Durée de vie — la différence clé.** `NkPool` ne détruit **jamais** automatiquement (même au
  `Reset`/`Clear`/destructeur) : à vous les `Destroy`. `NkRingBuffer` détruit **toujours** ses
  éléments actifs (`Pop`/`PopDiscard`/`Clear`/destructeur). `NkArray` stocke des valeurs (rien à
  gérer).
- **Thread-safety.** `NkPool` et `NkRingBuffer` sont thread-unsafe par défaut — synchronisez à
  l'extérieur si plusieurs threads y touchent.
- **Zéro-STL.** `NkPool`/`NkRingBuffer` utilisent placement new + appel explicite du destructeur,
  conformément à la règle NKMemory (jamais de `new`/`delete` raw).

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu;

// NkArray : les 4 coins d'un quad, sur la pile, prêts pour le GPU.
NkArray<NkVec2, 4> quad = { {0,0}, {1,0}, {1,1}, {0,1} };
renderer.Upload(quad.Data(), quad.Size());   // pointeur brut contigu, zéro allocation

// NkPool : un pool de particules, création/destruction O(1) sans fragmentation.
NkPool<Particle> pool(10000);
Particle* p = pool.Construct(spawnPos, spawnVel);   // nullptr si plein
if (p) { /* ... simulation ... */ pool.Destroy(p); } // détruire avant de relâcher le slot

// NkRingBuffer : fenêtre glissante des 60 derniers temps de frame.
NkRingBuffer<float> frameTimes(60);
frameTimes.Push(dt);                          // écrase le plus ancien quand plein
float avg = 0.f;
for (usize i = 0; i < frameTimes.Size(); ++i) // pas d'itérateurs : operator[] FIFO
    avg += frameTimes[i];
avg /= frameTimes.Size();
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Conteneurs séquentiels →](Sequential.md)
