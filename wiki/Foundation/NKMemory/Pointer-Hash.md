# Index par pointeur

> Couche **Foundation** · NKMemory · Deux tables de hachage dont la **clé est un pointeur** :
> l'ensemble `NkPointerHashSet` (présence) et la table `NkPointerHashMap` (valeur associée), pensées
> pour indexer des allocations et des objets par leur **adresse**, en `O(1)`.

Voici le dernier outil de NKMemory, et le plus pointu. C'est une table de hachage un peu
particulière : sa **clé est un pointeur**. Pas le contenu pointé — l'**adresse** elle-même. La
comparaison se fait par égalité d'adresse (`==`), et le hash n'examine jamais ce vers quoi le
pointeur pointe : il triture les bits de l'adresse (un *finalizer* MurmurHash3) pour répartir
uniformément même des adresses alignées qui se ressemblent.

À quoi cela sert-il ? Le traceur de fuites, par exemple, doit retrouver instantanément les
métadonnées associées à une adresse donnée (« cette adresse qu'on me demande de libérer, quand et où
a-t-elle été allouée ? »). Indexer par adresse, en temps constant, est exactement le besoin. Plus
généralement, dès qu'on veut un cache où l'**identité** d'un objet — son adresse — sert de clé
(associer des données annexes à des ressources, à des objets existants), ces tables sont l'outil
idoine. C'est d'ailleurs ainsi que `NkMemoryTracker` détecte les fuites en `O(1)`.

Ce n'est **pas** un dictionnaire généraliste : il n'y a ni hachage de contenu, ni clés `string`, ni
template sur le type de clé — la clé est figée à `const void*`. Et ce n'est **pas** un conteneur
propriétaire : les tables ne possèdent **ni** les objets pointés par les clés, **ni** les valeurs ;
leur cycle de vie reste à votre charge.

Sous le capot, les deux conteneurs partagent la même mécanique, et c'est elle qui fonde les
performances : **adressage ouvert** avec **sondage linéaire** dans un tableau **contigu**
(cache-friendly, pas d'indirection par nœud), capacité toujours **puissance de 2** (l'indexation se
fait par masquage `index & (cap-1)` au lieu d'un modulo), et **tombstones** (marqueurs de
suppression) pour des effacements en `O(1)`. La table se **réagence** (rehash) toute seule quand le
taux de remplissage dépasse 70 %, et fait un nettoyage quand les tombstones passent au-dessus de
40 % de la capacité. La capacité minimale est de 16 entrées.

Elles offrent un accès en **`O(1)` en moyenne** (le pire cas, `O(n)`, est très rare et lié à un
agrégat de collisions). Il en existe deux variantes, selon qu'on veut juste mémoriser une présence
ou associer une valeur.

- **Namespace** : `nkentseu::memory`
- **Header** : `#include "NKMemory/NkHash.h"`

---

## Le cycle de vie : `Initialize` puis `Shutdown`

Une habitude à prendre tout de suite : le **constructeur n'alloue rien**. Il se contente de
mémoriser l'allocateur. Tant qu'on n'a pas appelé `Initialize()`, la table est vide de buffer et
toute opération est inopérante. C'est un choix délibéré — il sépare la *construction* (gratuite,
sans effet de bord) de l'*allocation* (qui peut échouer, et qu'on veut maîtriser).

```cpp
NkPointerHashSet seen;            // ne touche pas le tas
seen.Initialize(128);            // ALLOUE ici (capacité arrondie à la pow2 ≥ 128)
// ... usage ...
seen.Shutdown();                 // libère le buffer interne (le destructeur le fait aussi)
```

`Initialize(initialCapacity = 64, allocator = nullptr)` arrondit la capacité demandée à la puissance
de 2 supérieure (minimum 16), alloue, et renvoie `false` si l'allocation échoue. Elle est
**idempotente** : la rappeler sur une table déjà initialisée est sans effet. `Shutdown()` libère
tout et remet l'état à zéro ; il est sûr même si rien n'a été alloué, et le **destructeur l'appelle
automatiquement**. Cet appairage `Initialize` ↔ `Shutdown` est la déclinaison locale de la règle
dure du projet : **toute classe avec un point de création explicite a son point de destruction**.

> **En résumé.** Le constructeur n'alloue pas : `Initialize()` doit précéder toute opération,
> `Shutdown()` (ou le destructeur) libère. `Initialize` est idempotente et renvoie `false` si
> l'allocation rate. Respectez la symétrie `Initialize` ↔ `Shutdown`.

---

## NkPointerHashSet — un ensemble de pointeurs

Quand on veut seulement savoir « ai-je déjà vu ce pointeur ? », l'ensemble suffit :

```cpp
NkPointerHashSet seen;
seen.Initialize(128);

seen.Insert(obj);                 // true si nouveau, false si déjà présent
if (seen.Contains(obj)) { /* déjà rencontré */ }
seen.Erase(obj);                  // true si l'élément était là (marquage tombstone)
seen.Shutdown();
```

`Insert(key)` ajoute une clé en **ignorant les doublons** : il renvoie `true` si la clé est nouvelle,
`false` si elle était déjà là (ou en cas d'erreur). La clé ne doit être **ni `nullptr` ni la
sentinelle tombstone**. `Contains(key)` teste la présence sans rien modifier, et `Erase(key)`
supprime par marquage tombstone (renvoyant `true` si la clé était présente). À cela s'ajoutent
`Clear()` (vide tout mais **conserve** la capacité), `Reserve(n)` (pré-réserve), et les accesseurs
d'état `Size()`, `Capacity()`, `LoadFactor()`, `IsInitialized()`. L'interface est volontairement
minimale.

> **En résumé.** `NkPointerHashSet` mémorise une **présence** d'adresses. `Insert` (true si nouveau),
> `Contains`, `Erase` en `O(1)` moyen ; `nullptr` et la tombstone sont des clés interdites.

---

## NkPointerHashMap — une valeur par pointeur

Quand on veut associer une **valeur** (un autre pointeur) à chaque clé, c'est la table. Clé et valeur
sont stockées **adjacentes** (`NkEntry`) pour la localité de cache.

```cpp
NkPointerHashMap cache;
cache.Initialize(256);

cache.Insert(resourceKey, resourceData);   // insère OU met à jour

if (void* data = cache.Find(resourceKey)) {
    // trouvé : 'data' est la valeur associée
}
```

`Insert(key, value)` **insère ou met à jour** : il renvoie `true` si la clé est nouvelle, **`false`
si la clé existait déjà** (la valeur est alors écrasée) — `false` ici n'est donc pas une erreur, mais
une mise à jour. `Find(key)` renvoie la valeur, ou `nullptr` si la clé est absente. Ce qui soulève
une subtilité : que faire si `nullptr` est une valeur *légitime* à stocker ? Avec `Find`, on ne
pourrait pas distinguer « absent » de « présent, mais la valeur vaut nullptr ». C'est précisément à
ça que sert `TryGet` :

```cpp
void* out = nullptr;
if (cache.TryGet(resourceKey, &out)) {
    // la clé existe ; 'out' a été renseigné (et peut valoir nullptr légitimement)
}
```

`TryGet(key, outValue)` sépare le *succès de la recherche* (son retour booléen) de la *valeur
trouvée* : `outValue` est **toujours écrit** (`nullptr` si la clé est absente, la valeur sinon).
Utilisez-le dès que `nullptr` peut être une vraie valeur ; sinon, `Find` est plus direct. Pour le
reste, `Contains(key)` teste la simple présence (plus rapide que `Find` quand la valeur ne vous
intéresse pas), et `Erase(key, outValue = nullptr)` supprime en écrivant au passage l'ancienne valeur
dans `outValue` si on le fournit — pratique pour libérer la ressource qu'on vient de retirer :

```cpp
void* oldData = nullptr;
if (cache.Erase(resourceKey, &oldData)) {
    FreeResource(oldData);        // on récupère l'ancienne valeur avant de la perdre
}
```

`Clear`, `Reserve`, `Size`, `Capacity`, `LoadFactor`, `IsInitialized` complètent l'ensemble, avec la
même sémantique que pour le Set.

> **En résumé.** `NkPointerHashMap` associe une valeur (`void*`) à chaque clé-adresse. `Insert`
> insère **ou** met à jour (false = mise à jour). `Find` pour le cas courant, `TryGet` quand
> `nullptr` est une valeur valide, `Erase(key, &old)` pour récupérer l'ancienne valeur en supprimant.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Complexités entre crochets quand elles
éclairent.

### Stratégie commune

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Conception | Open addressing + linear probing, tableau contigu | Pas d'indirection, ami du cache. |
| Conception | Capacité = puissance de 2, indexation par masking | `index & (cap-1)` au lieu de modulo. |
| Conception | Tombstones (suppression `O(1)`) | Mémoire récupérée seulement au rehash. |
| Seuils | Rehash si load factor > 70 % ; cleanup si tombstones > 40 % | Réagencement auto, `O(1)` amorti. |
| Contraintes | Non thread-safe ; clés stables ; `nullptr`/tombstone interdits | Synchro externe ; ne pas invalider une clé en place. |

### `detail` — utilitaires internes (déclarés publiquement)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Hash | `HashPointer(ptr)` `[noexcept]` | Hash d'un pointeur (MurmurHash3 finalizer), dans `[0, SIZE_MAX]`. |
| Capacité | `NextPow2(value)` `[noexcept]` | Plus petite puissance de 2 ≥ `value`. |
| Sentinelle | `TombstoneKey()` `[noexcept]` | Marqueur de suppression (adresse `0x1`), jamais une clé valide. |
| Sentinelle | `InvalidIndex()` `[noexcept]` | Index invalide = `SIZE_MAX`. |

### `NkPointerHashSet` — ensemble de clés `const void*`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `explicit NkPointerHashSet(allocator = nullptr)` | Stocke l'allocateur, **n'alloue pas**. |
| Cycle de vie | `Initialize(cap = 64, alloc = nullptr)`, `Shutdown()` | Alloue (idempotent) / libère. Non-copiable. |
| Capacité | `Reserve(cap)`, `Clear()` `[O(capacity)]` | Pré-réserve (marge 50 %) / vide en gardant la capacité. |
| Éléments | `Insert(key)` `[O(1)~]`, `Contains(key)` `[O(1)~]`, `Erase(key)` `[O(1)~]` | Ajout (true si nouveau) / présence / suppression (tombstone). |
| État | `IsInitialized()`, `Size()`, `Capacity()`, `LoadFactor()` | Init ? / compte / capacité (pow2) / taux de remplissage. |

### `NkPointerHashMap` — table `const void*` → `void*`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `NkEntry { const void* Key; void* Value; }` (2 ctors `constexpr`) | Paire clé/valeur stockée contiguë. |
| Construction | `explicit NkPointerHashMap(allocator = nullptr)` | Stocke l'allocateur, **n'alloue pas**. |
| Cycle de vie | `Initialize(cap = 64, alloc = nullptr)`, `Shutdown()` | Alloue (idempotent) / libère. Non-copiable. |
| Capacité | `Reserve(cap)`, `Clear()` `[O(capacity)]` | Pré-réserve (marge 50 %) / vide en gardant la capacité. |
| Éléments | `Insert(key, value)` `[O(1)~]` | Insère **ou** met à jour (true si nouveau, false si update). |
| Éléments | `Find(key)` `[O(1)~]` | Valeur ou `nullptr` (ambigu si `nullptr` est une valeur). |
| Éléments | `TryGet(key, outValue)` `[O(1)~]` | Bool de succès ; `outValue` toujours écrit (lève l'ambiguïté). |
| Éléments | `Contains(key)` `[O(1)~]`, `Erase(key, outValue = nullptr)` `[O(1)~]` | Présence / suppression (récupère l'ancienne valeur). |
| État | `IsInitialized()`, `Size()`, `Capacity()`, `LoadFactor()` | Init ? / compte / capacité (pow2) / taux de remplissage. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines du moteur. Les
éléments triviaux sont décrits brièvement ; les opérations importantes le sont **à fond**.

### La conception interne, en détail

Comprendre la mécanique évite les surprises. Les deux conteneurs sont des **tables à adressage
ouvert** : il n'y a pas de listes de collision, tout vit dans **un seul tableau contigu** (`const
void** mKeys` pour le Set, `NkEntry* mEntries` pour la Map). Quand un emplacement est occupé, le
**sondage linéaire** essaie le suivant, et ainsi de suite — ce qui reste très local en mémoire (le
CPU prélit les cases voisines). L'indexation exploite que la **capacité est toujours une puissance de
2** : au lieu d'un coûteux `hash % cap`, on fait `hash & (cap-1)`, un simple masquage de bits.

La **suppression** est en `O(1)` grâce aux **tombstones** : effacer ne déplace rien, on pose juste un
marqueur (`detail::TombstoneKey()`, adresse `0x1`) à la place de la clé. La mémoire n'est récupérée
qu'au prochain rehash. Deux seuils pilotent les réagencements : un **rehash de croissance** quand le
load factor dépasse 70 % (la table double et redistribue tout, `O(n)` mais `O(1)` amorti), et un
**rehash de nettoyage** quand les tombstones dépassent 40 % de la capacité (pour ne pas laisser les
sentinelles ralentir les sondages). La capacité minimale est 16. Tout passe par un `NkAllocator*`, et
les conteneurs ne sont **pas thread-safe**.

### `Initialize`, `Shutdown` — le cycle de vie

`Initialize(initialCapacity = 64, allocator = nullptr)` est le **point d'allocation** : capacité
arrondie à la puissance de 2 (min 16), allocateur du paramètre prioritaire sur celui du constructeur,
retour `false` si l'allocation échoue. **Idempotente** : un second appel sur une table déjà prête ne
fait rien. `Shutdown()` libère le buffer, remet l'état à zéro (après, `IsInitialized()` vaut `false`),
est sûr même non-initialisé, et **est appelé par le destructeur**. Les deux conteneurs sont
**non-copiables** (constructeur de copie et `operator=` supprimés) : ils possèdent leur buffer en
exclusivité. Cas d'usage :

- **GPU / rendu** — initialiser un cache pointeur→handle au démarrage d'un sous-système, le `Shutdown`
  à l'arrêt ; pas d'allocation surprise dans la frame.
- **IO / ressources** — un gestionnaire de ressources crée son index au chargement d'un niveau,
  `Shutdown` au déchargement.
- **Audio** — un mixeur indexe ses voix actives par adresse de source, table créée à l'init du moteur.

### `Reserve`, `Clear` — gérer la capacité sans réagencer

`Reserve(requestedCapacity)` pré-réserve de la place pour éviter les rehash en pleine charge. Il vise
une capacité avec **50 % de marge** (`target = size + size/2 + 1`), et ne fait rien si la capacité est
déjà suffisante et les tombstones rares. Si vous connaissez le nombre d'entrées à l'avance, un
`Reserve` initial supprime les réagencements coûteux pendant le remplissage. `Clear()` vide toutes les
entrées mais **conserve** la capacité allouée (il remet `Size()` et le compteur de tombstones à zéro,
en `O(capacity)`) — bien plus rapide qu'un `Shutdown` suivi d'un `Initialize`. Cas d'usage :

- **Rendu** — un index « objets visibles cette frame » qu'on `Clear` à chaque début de frame plutôt
  que de le détruire/recréer.
- **Physique** — une *broad-phase* qui rebâtit son ensemble de paires candidates par frame : `Reserve`
  une fois, `Clear` à chaque pas.
- **ECS / scène** — pré-réserver l'index entité→données quand on connaît le nombre d'entités d'un
  niveau.
- **UI / 2D** — un cache widget→état qu'on vide entre deux écrans sans relâcher la mémoire.

### `Insert` — ajouter une clé

Sur le **Set**, `Insert(key)` ajoute en ignorant les doublons : `true` si la clé est nouvelle, `false`
si elle existait déjà (ou en cas d'erreur). Sur la **Map**, `Insert(key, value)` **insère ou met à
jour** : `true` si la clé est nouvelle, `false` si elle existait (la valeur est alors **écrasée**) —
ici, `false` signale une mise à jour, pas un échec. Dans les deux cas, la clé ne doit être **ni
`nullptr` ni la sentinelle tombstone**, et un rehash automatique a lieu si le load factor dépasse
70 %. Complexité `O(1)` moyen, `O(n)` pire cas. Cas d'usage :

- **Mémoire / IO** — le traceur indexe chaque allocation par son adresse au moment du `NkAlloc`.
- **GPU / rendu** — associer une ressource CPU (mesh, texture) à son handle GPU : `Insert` au premier
  upload, simple mise à jour ensuite si le handle change.
- **Gameplay / IA** — marquer les entités déjà traitées par un parcours (un BFS sur le graphe de
  scène) pour ne pas y revenir (Set).
- **Animation** — lier un objet animé à son état d'interpolation courant (Map).

### `Find` — récupérer une valeur (Map)

`Find(key)` renvoie la valeur associée, ou `nullptr` si la clé est absente — en lecture seule, `O(1)`
moyen. **Attention à l'ambiguïté** : `nullptr` peut signifier « clé absente » *ou* « clé présente avec
valeur `nullptr` ». Si `nullptr` ne peut jamais être une valeur stockée légitime, `Find` est le moyen
le plus direct ; sinon, préférez `TryGet`. Cas d'usage :

- **Rendu** — retrouver le handle GPU d'un mesh donné avant de l'envoyer au pipeline.
- **IO / ressources** — résoudre un pointeur de ressource vers ses métadonnées de chargement.
- **Audio** — retrouver le canal de mixage attaché à une source sonore.

### `TryGet` — lever l'ambiguïté (Map)

`TryGet(key, outValue)` sépare le **succès de la recherche** (le retour booléen) de la **valeur
trouvée** : `outValue` est **toujours écrit** (`nullptr` si absent, la valeur sinon). C'est l'outil dès
que `nullptr` est une valeur stockée valide. Cas d'usage :

- **Gameplay / IA** — une table objet→cible où « pas de cible » (`nullptr`) est un état légitime,
  distinct de « objet inconnu ».
- **UI / 2D** — un cache widget→focus où l'absence de focus (`nullptr`) doit se différencier d'un
  widget non encore enregistré.
- **Animation** — associer un os à son override de pose, l'override pouvant être `nullptr` (pas
  d'override) sans signifier que l'os est absent.

### `Contains` — tester la présence

`Contains(key)` teste seulement la présence d'une clé, sans récupérer de valeur (donc plus rapide que
`Find` quand la valeur ne vous intéresse pas), `O(1)` moyen, sans modifier la table. Cas d'usage :

- **Gameplay / IA** — « cet ennemi est-il déjà dans la liste d'agression ? » (Set).
- **Physique** — « cette paire de corps a-t-elle déjà été testée ce pas ? » pour dédupliquer.
- **Mémoire** — « cette adresse est-elle suivie ? » avant de la libérer.

### `Erase` — supprimer une clé

`Erase(key)` (Set) ou `Erase(key, outValue = nullptr)` (Map) supprime par **tombstone** : `O(1)`
moyen, renvoie `true` si la clé était présente. Sur la Map, si `outValue` est fourni, l'**ancienne
valeur** y est écrite avant suppression — idéal pour libérer proprement ce qu'on retire. Un rehash de
nettoyage se déclenche si les tombstones dépassent 40 % de la capacité. Cas d'usage :

- **Mémoire / IO** — retirer une allocation de l'index au `NkFree`, en récupérant ses métadonnées
  pour les journaliser.
- **GPU / rendu** — quand une ressource est déchargée, `Erase(meshPtr, &handle)` puis détruire le
  handle GPU récupéré.
- **Audio** — retirer une voix quand son `voicePtr` est libéré, en récupérant son état pour le
  finaliser.

### Accesseurs d'état

`IsInitialized()` indique si la table est allouée (`mKeys != nullptr` / `mEntries != nullptr`),
`Size()` le nombre d'éléments, `Capacity()` la capacité courante (toujours une puissance de 2), et
`LoadFactor()` le taux de remplissage (`Size / Capacity`, ou `0` si non allouée). Tous sont `inline`,
`O(1)`, et triviaux — on s'en sert pour la télémétrie, décider d'un `Reserve`, ou asserter qu'une
table est prête.

### Les sentinelles `detail`

`detail::HashPointer`, `NextPow2`, `TombstoneKey` et `InvalidIndex` sont les briques internes,
exposées surtout à titre documentaire. `HashPointer` applique le finalizer MurmurHash3 à une adresse,
`NextPow2` calcule la capacité, `TombstoneKey()` (adresse `0x1`) est le marqueur de suppression — et,
de fait, la **clé interdite** dans `Insert` (avec `nullptr`) — et `InvalidIndex()` (`SIZE_MAX`) signale
un emplacement non trouvé. On y touche rarement directement.

### Règles dures à ne pas oublier

- **`new`/`delete` interdits.** Les buffers internes passent par `NkAllocator` (NKMemory). Ne mélangez
  **jamais** l'allocation interne avec `new`/`delete` ou `std::free` côté appelant → heap corruption
  Windows **c0000374**. Et rappelez-vous : ces tables **ne possèdent pas** les objets pointés par les
  clés/valeurs — c'est à vous de les libérer.
- **Symétrie `Initialize` ↔ `Shutdown`**, et appairage `Insert` ↔ `Erase` : la règle « tout point de
  création a son point de destruction » s'applique ici comme partout dans NKMemory.
- **Stabilité des clés.** Ne déplacez ni n'invalidez un objet tant que son adresse sert de clé : la
  table compare des adresses, pas des contenus.
- **Non thread-safe.** En accès concurrent, protégez par un verrou externe (par exemple `NkSpinLock`
  de [NKCore](../NKCore.md)).
- **Pas d'itération.** Aucune API ne permet de parcourir les entrées : si vous devez énumérer,
  maintenez une liste parallèle (un [NkVector](../NKContainers/Sequential.md), par exemple).

---

### Exemple

```cpp
#include "NKMemory/NkHash.h"
using namespace nkentseu::memory;

// Set : marquer les objets déjà visités lors d'un parcours de scène.
NkPointerHashSet visited;
visited.Initialize(256);
if (visited.Insert(node)) {           // true => première visite
    Process(node);
}
visited.Shutdown();

// Map : un cache ressource -> handle GPU, avec libération propre au retrait.
NkPointerHashMap gpuCache;
gpuCache.Reserve(1024);               // après Initialize, pré-réserve pour éviter les rehash
gpuCache.Insert(meshKey, gpuHandle);  // insère, ou met à jour (renvoie false)

void* handle = nullptr;
if (gpuCache.TryGet(meshKey, &handle) && handle) {
    Bind(handle);                     // distingue "absent" de "valeur nullptr"
}

void* old = nullptr;
if (gpuCache.Erase(meshKey, &old)) {
    DestroyGpuHandle(old);            // récupère l'ancienne valeur avant de l'oublier
}
gpuCache.Shutdown();
```

---

[← Tags & budgets](Tags-Budgets.md) · [Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md)
