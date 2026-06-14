# Tags et budgets mémoire

> Couche **Foundation** · NKMemory · Attribuer chaque octet à un **sous-système** : l'énumération
> `NkMemoryTag`, la façade de plafonds `NkMemoryBudget`, le relevé `NkMemoryTagStats`, et les
> helpers d'allocation taggée `TaggedAlloc` / `TaggedFree` / `TaggedCalloc`.

Le profileur du chapitre précédent répond à une question : « **combien** de mémoire mon programme
consomme-t-il ? ». Sur un vrai moteur, une question plus utile arrive très vite : « combien en
consomme **chaque sous-système** ? ». Les textures, les maillages, l'audio, le code de jeu, le
réseau… chacun a son enveloppe, et quand la mémoire se tend on veut savoir **lequel déborde** —
idéalement *avant* que ça pose problème. C'est exactement ce que résolvent les **tags** et les
**budgets** : on étiquette chaque allocation avec un tag identifiant son sous-système, on fixe un
plafond par tag, et on suit sa consommation en temps réel.

Ce n'est **pas** un allocateur de plus, ni un remplaçant de NKMemory : c'est une couche de
**comptabilité** posée par-dessus. `NkMemoryBudget` ne réserve aucun octet — il **compte**. Les
allocations réelles continuent de passer par les allocateurs habituels ; le tagging se contente de
noter « tant d'octets viennent d'être pris/rendus sous tel tag ». Et tout est pensé pour le **temps
réel** : compteurs atomiques légers (pas de verrou global), donc on peut taguer depuis n'importe
quel thread sans tuer la boucle de jeu.

- **Namespace** : `nkentseu::memory`
- **Header** : `#include "NKMemory/NkTag.h"`
- **Release** : avec `NKENTSEU_DISABLE_MEMORY_TAGGING`, budgets et stats ne sont plus trackés, les
  `Tagged*` se réduisent à `NkAlloc`/`NkFree` et `GetStats` renvoie des zéros — le coût tombe à
  **zéro overhead**. Ne faites donc jamais reposer une logique fonctionnelle critique sur les
  budgets en release.

---

## Étiqueter : `NkMemoryTag`

Un tag est une valeur de l'énumération `NkMemoryTag`, dont le type sous-jacent est `nk_uint8` — donc
**256 tags distincts au maximum**. L'enum est organisée par **plages de domaine**, chacune avec ses
tags nommés *et* des slots `*_RESERVED_*` libres : ainsi une couche peut déclarer ses propres
catégories dans sa plage sans empiéter sur les autres ni casser la compatibilité.

Les grands domaines : **Core Engine** (`NK_MEMORY_ENGINE`, `NK_MEMORY_CONTAINER`,
`NK_MEMORY_ALLOCATOR`), **Game Systems** (`NK_MEMORY_GAME`, `NK_MEMORY_ENTITY`,
`NK_MEMORY_SCRIPT`), **Rendering** (`NK_MEMORY_RENDER`, `NK_MEMORY_TEXTURE`, `NK_MEMORY_MESH`,
`NK_MEMORY_SHADER`), **Physics** (`NK_MEMORY_PHYSICS`, `NK_MEMORY_COLLISION`, `NK_MEMORY_RIGID`),
**Audio** (`NK_MEMORY_AUDIO`, `NK_MEMORY_SOUND`), **Streaming & Network** (`NK_MEMORY_STREAMING`,
`NK_MEMORY_NETWORK`). Un cas à part : `NK_MEMORY_DEBUG` (valeur 255) sert aux allocations de
debugging et **n'est pas comptée dans les budgets** — c'est voulu, pour ne pas polluer les
compteurs avec de l'instrumentation jetable.

> **En résumé.** `NkMemoryTag` (`enum class : nk_uint8`, max 256) range les allocations par
> sous-système, en plages de domaine extensibles via les slots `*_RESERVED_*`. Taguez en priorité
> les **gros consommateurs** (textures, meshes, audio, scènes). `NK_MEMORY_DEBUG` échappe aux
> budgets.

---

## Plafonner et surveiller : `NkMemoryBudget`

`NkMemoryBudget` est la **façade entièrement statique** qui gère les plafonds et les statistiques
par tag. On ne l'instancie pas (pas de constructeur, pas de `NkMake*`) : on appelle ses méthodes
statiques. On commence par **fixer un budget**, puis on **interroge**.

```cpp
using namespace nkentseu::memory;
constexpr nk_uint64 TEXTURE_BUDGET = 512ull * 1024 * 1024;   // 512 Mo

NkMemoryBudget::SetBudget(NkMemoryTag::NK_MEMORY_TEXTURE, TEXTURE_BUDGET);
NkMemoryBudget::SetBudgetWarningThreshold(0.9f);   // alerter dès 90 %
NkMemoryBudget::SetBudgetAlertsEnabled(true);
```

Le seuil d'avertissement est l'astuce qui rend le système **préventif** plutôt que **réactif** :
plutôt que d'attendre le dépassement, on déclenche une alerte quand un tag atteint 90 % de son
budget, ce qui laisse le temps d'évincer des ressources avant la saturation. Attention toutefois à
une subtilité : `SetBudget(tag, 0)` ne veut **pas** dire « budget nul » mais **illimité** —
`IsOverBudget` renvoie alors toujours `false` et `GetBudgetRemaining` renvoie `LLONG_MAX`.

Deux questions reviennent à l'usage. « Suis-je en train de dépasser ? » se lit avec `IsOverBudget` ;
« où en est tel sous-système ? » se lit avec `GetStats`, qui renvoie un `NkMemoryTagStats` **par
valeur** (un instantané ; les vraies valeurs peuvent bouger juste après l'appel).

```cpp
if (NkMemoryBudget::IsOverBudget(NkMemoryTag::NK_MEMORY_TEXTURE)) {
    // évincer des textures avant d'en charger de nouvelles
}
NkMemoryTagStats s = NkMemoryBudget::GetStats(NkMemoryTag::NK_MEMORY_RENDER);
// s.totalAllocated, s.peakAllocated, s.allocationCount, ...
```

En coulisses, ce sont `OnAllocate(tag, bytes)` et `OnDeallocate(tag, bytes)` qui tiennent les
compteurs à jour — appelés **automatiquement** par les `Tagged*` et les allocateurs taggés, vous
n'avez normalement pas à les invoquer vous-même. Ce ne sont **pas** des allocateurs : ils ne
réservent rien, ils notifient. `DumpStats()` crache un tableau aligné vers le log, et `ResetStats()`
remet les compteurs à zéro (pour isoler un benchmark) **sans toucher aux budgets configurés**.

> **En résumé.** `NkMemoryBudget` (statique) : `SetBudget` (0 = illimité), `SetBudgetWarningThreshold`
> sous 1.0 pour être prévenu **avant** le dépassement, `IsOverBudget`/`GetStats` pour surveiller,
> `DumpStats`/`ResetStats` pour profiler. Les compteurs sont alimentés par `OnAllocate`/`OnDeallocate`
> — appelés pour vous par les `Tagged*`.

---

## Allouer taggé : `TaggedAlloc` / `TaggedFree` / `TaggedCalloc`

Pour que les compteurs se remplissent tout seuls, on alloue via les **helpers taggés** plutôt qu'en
appelant `NkAlloc`/`NkFree` directement. `TaggedAlloc` alloue *et* appelle `OnAllocate` après succès ;
`TaggedFree` appelle `OnDeallocate` *avant* de libérer ; `TaggedCalloc` fait comme `TaggedAlloc` mais
**zero-initialise**. La règle dure de NKMemory s'applique pleinement ici : **`TaggedAlloc` ↔
`TaggedFree` doivent être symétriques** — même `tag`, même `allocator`. Croiser ce couple avec
`NkAlloc`, `std::free` ou `delete[]` = heap corruption Windows `c0000374`.

```cpp
void* tex = TaggedAlloc(1024 * 1024, NkMemoryTag::NK_MEMORY_TEXTURE);
// ... usage ...
TaggedFree(tex, NkMemoryTag::NK_MEMORY_TEXTURE);   // MÊME tag, MÊME allocator
```

> **En résumé.** `TaggedAlloc`/`TaggedCalloc` allouent en notifiant le budget ; `TaggedFree` libère
> en le décrémentant. Symétrie obligatoire (même tag, même allocator) — jamais mélanger avec
> `NkAlloc`/`std::free`/`delete[]`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics de `NkTag.h`. Chacun est détaillé dans la « Référence
complète » qui suit. `[noexcept]` et complexités entre crochets.

### `NkMemoryTag` — l'énumération (`enum class : nk_uint8`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Core Engine | `NK_MEMORY_ENGINE` `NK_MEMORY_CONTAINER` `NK_MEMORY_ALLOCATOR` (+ `CORE_RESERVED_3..9`) | Moteur interne / conteneurs génériques / overhead allocateurs |
| Game Systems | `NK_MEMORY_GAME` `NK_MEMORY_ENTITY` `NK_MEMORY_SCRIPT` (+ `GAME_RESERVED_13..19`) | Gameplay / entités-composants ECS / VM de scripting |
| Rendering | `NK_MEMORY_RENDER` `NK_MEMORY_TEXTURE` `NK_MEMORY_MESH` `NK_MEMORY_SHADER` (+ `RENDER_RESERVED_24..29`) | Commandes/pipelines / textures CPU / meshes / shaders compilés |
| Physics | `NK_MEMORY_PHYSICS` `NK_MEMORY_COLLISION` `NK_MEMORY_RIGID` (+ `PHYSICS_RESERVED_33..39`) | Solvers/broadphase / shapes / corps rigides |
| Audio | `NK_MEMORY_AUDIO` `NK_MEMORY_SOUND` (+ `AUDIO_RESERVED_42..49`) | Mixers/buses/effets / buffers PCM, streaming |
| Streaming & Network | `NK_MEMORY_STREAMING` `NK_MEMORY_NETWORK` (+ `IO_RESERVED_52..59`) | Cache de streaming / buffers réseau |
| Debug | `NK_MEMORY_DEBUG` (= 255) | Allocations de debug — **non comptées dans les budgets** |

### `NkMemoryTagStats` — le relevé par tag (struct, valeurs en octets)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Champs | `totalAllocated` `peakAllocated` `allocationCount` `averageSize` `fragmentation` | Alloué courant / pic / nb d'allocations / taille moyenne (float) / ratio frag `[0..1]` |
| Construction | `constexpr NkMemoryTagStats()` `[noexcept]` | Tout à zéro |
| Calcul | `ComputeAverage()` `[noexcept, O(1)]` | Recalcule `averageSize` en place, **renvoie `*this`** (chaining) |
| Calcul | `EstimateFragmentation(freeBytes)` `[noexcept, O(1)]` | Estime `fragmentation` en place, **renvoie `*this`** (chaining) |

### `NkMemoryBudget` — la façade statique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Budgets | `SetBudget(tag, bytes)` `[noexcept, O(1)]` | Fixe le plafond (`0` = **illimité**) |
| Budgets | `GetBudget(tag)` `[[nodiscard], noexcept, O(1)]` | Relit le plafond (`0` = illimité) |
| Tracking | `OnAllocate(tag, bytes)` `[noexcept, O(1)]` | Notifie une allocation (auto) |
| Tracking | `OnDeallocate(tag, bytes)` `[noexcept, O(1)]` | Notifie une déallocation (auto) |
| Tracking | `GetBudgetRemaining(tag)` `[[nodiscard], noexcept, O(1)]` | Octets restants (**négatif** si dépassé, `LLONG_MAX` si illimité) |
| Tracking | `IsOverBudget(tag)` `[[nodiscard], noexcept, O(1)]` | Usage > budget ? (`false` si illimité) |
| Stats | `GetStats(tag)` `[[nodiscard], noexcept, O(1)]` | Instantané `NkMemoryTagStats` par valeur |
| Stats | `DumpStats()` `[noexcept, O(tags)]` | Tableau formaté vers le log |
| Stats | `ResetStats()` `[noexcept]` | Remet les compteurs à zéro (**garde** les budgets) |
| Alertes | `SetBudgetAlertsEnabled(enabled)` `[noexcept]` | Active/désactive les warnings de dépassement |
| Alertes | `SetBudgetWarningThreshold(threshold)` `[noexcept]` | Seuil préventif `[0..1]` (ex. `0.9f` = 90 %) |

### Helpers d'allocation taggée (free functions, non `noexcept`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Allocation | `TaggedAlloc(bytes, tag, allocator = nullptr, alignment = NK_MEMORY_DEFAULT_ALIGNMENT)` | Alloue + `OnAllocate` ; `nullptr` si échec |
| Allocation | `TaggedCalloc(count, elementSize, tag, allocator = nullptr, alignment = …)` | Alloue + **zéro-initialise** + `OnAllocate` |
| Libération | `TaggedFree(ptr, tag, allocator = nullptr)` | `OnDeallocate` + libère (`nullptr` = no-op) |

---

## Référence complète

### `NkMemoryTag` — les catégories à fond

Le type sous-jacent `nk_uint8` borne l'enum à **256 valeurs** : c'est largement suffisant et ça
garde un tag sur un seul octet, donc traçable sans coût. L'organisation par plages de domaine, avec
des slots `*_RESERVED_*` libres, est ce qui rend le système **extensible sans breaking change** : une
nouvelle couche pose ses tags dans sa plage réservée. Concrètement, par domaine :

- **Rendu** — `NK_MEMORY_TEXTURE` pour les pixels côté CPU avant upload GPU, `NK_MEMORY_MESH` pour
  vertices/indices/skins, `NK_MEMORY_SHADER` pour les pipelines compilés, `NK_MEMORY_RENDER` pour les
  command buffers et descriptors. Ce sont vos plus gros postes : tagez-les en premier.
- **ECS / scène** — `NK_MEMORY_ENTITY` pour les archétypes et composants, `NK_MEMORY_GAME` pour la
  logique de gameplay au-dessus.
- **Physique** — `NK_MEMORY_PHYSICS` (solvers, broadphase, contraintes), `NK_MEMORY_COLLISION`
  (shapes convexes, primitives), `NK_MEMORY_RIGID` (états/transforms/velocities).
- **Animation / gameplay / IA** — généralement `NK_MEMORY_GAME` ou `NK_MEMORY_SCRIPT` (la VM Lua et
  consorts) ; les arbres de comportement et blackboards d'IA y trouvent leur place.
- **Audio** — `NK_MEMORY_AUDIO` pour mixers/buses/effets, `NK_MEMORY_SOUND` pour les buffers PCM et
  le streaming.
- **UI / 2D** — souvent sous `NK_MEMORY_CONTAINER` (atlas, listes de draw) faute de tag dédié, ou un
  slot réservé d'une couche UI.
- **IO / réseau** — `NK_MEMORY_STREAMING` pour le cache d'assets à la demande, `NK_MEMORY_NETWORK`
  pour les buffers de paquets et la sérialisation.

`NK_MEMORY_DEBUG` (255) est volontairement **hors budget** : l'instrumentation de debug ne doit pas
fausser le bilan d'un sous-système.

### `NkMemoryTagStats` — lire les métriques

C'est une **struct à champs publics** (agrégat avec constructeur). `totalAllocated` est l'occupation
courante, `peakAllocated` la marque haute historique (précieuse pour dimensionner : c'est le pic, pas
la moyenne, qui fait planter), `allocationCount` le nombre **cumulé** d'allocations (jamais
décrémenté), `averageSize` la taille moyenne (float, pour la précision), et `fragmentation` un ratio
`[0..1]` (0 = parfait, 1 = très fragmenté). Le constructeur `constexpr NkMemoryTagStats()` met tout à
zéro.

Deux méthodes **modifient en place et renvoient `*this`** (donc se chaînent), toutes deux en `O(1)` :

- `ComputeAverage()` calcule `averageSize = totalAllocated / allocationCount`, mais **seulement si**
  `allocationCount > 0`. Elle ne recalcule pas `total`/`peak` — elle dérive une métrique des
  compteurs déjà remplis.
- `EstimateFragmentation(freeBytes)` pose `fragmentation = freeBytes / (totalAllocated + freeBytes)`,
  seulement si `total > 0`. C'est une **estimation simplifiée** (ratio libre/utilisé) ; une vraie
  fragmentation exigerait de tracker les blocs libres.

Par domaine, ces deux dérivées servent surtout au **profiling** : comparer `peakAllocated` frame à
frame pour repérer un pic de streaming de textures, suivre `averageSize` pour détecter qu'un pool de
particules s'est mis à faire des allocations atypiques, surveiller `fragmentation` sur un allocateur
audio à voix nombreuses.

```cpp
NkMemoryTagStats s = NkMemoryBudget::GetStats(NkMemoryTag::NK_MEMORY_RENDER);
s.ComputeAverage().EstimateFragmentation(freeBytes);   // chaining
```

### `NkMemoryBudget` — la façade à fond

Classe **statique pure** : aucun état exposé, aucune instance, aucun `NkMake*`, aucun smart-pointer.
On l'appelle de partout, depuis n'importe quel thread (atomiques légers, pas de verrou global).

**Configurer.** `SetBudget(tag, bytes)` fixe le plafond ; `bytes = 0` signifie **illimité** (et non
« nul »). Le changement est atomique et **ne réagit pas rétroactivement** sur les allocations déjà en
place. `GetBudget(tag)` relit ce plafond. Pratique typique : un `InitBudgets()` au démarrage qui
attribue à chaque gros sous-système son enveloppe (512 Mo de textures, tant de meshes, tant d'audio).

**Tracker.** `OnAllocate`/`OnDeallocate` incrémentent/décrémentent atomiquement — ils sont appelés
**automatiquement** par `TaggedAlloc`/`TaggedFree` et par les allocateurs taggés ; vous ne les
appelez à la main que si vous écrivez votre propre allocateur taggé (en surchargeant
`NkAllocator::Allocate` pour appeler `OnAllocate(mTag, size)` après le backing réel, et en trackant
la taille par pointeur pour `OnDeallocate`). `GetBudgetRemaining(tag)` renvoie les octets restants —
**négatif** si on a déjà dépassé, `LLONG_MAX` si le budget est illimité. `IsOverBudget(tag)` est le
booléen direct (`false` si illimité).

Côté domaines, le tracking se câble naturellement aux moments-clés :

- **Rendu** — avant de charger une nouvelle texture, `IsOverBudget(NK_MEMORY_TEXTURE)` déclenche une
  éviction LRU ; le seuil d'alerte à 90 % laisse même anticiper.
- **Streaming / IO** — `GetBudgetRemaining(NK_MEMORY_STREAMING)` pilote la profondeur de pré-chargement
  d'assets à la demande.
- **Audio** — plafonner `NK_MEMORY_SOUND` borne le nombre de buffers PCM résidents.
- **ECS / gameplay** — surveiller `NK_MEMORY_ENTITY` repère un spawn qui s'emballe.

**Profiler.** `GetStats(tag)` renvoie un **instantané par valeur** (les vraies valeurs peuvent bouger
juste après — c'est une photo, pas une vue live). `DumpStats()` écrit un tableau aligné
(Tag/Used/Budget/% Used/Peak + ligne TOTAL) vers `NK_FOUNDATION_LOG_INFO`, idéal à appeler
périodiquement (toutes les 60 frames). `ResetStats()` remet **tous** les compteurs à zéro pour isoler
un benchmark — mais **ne touche pas aux budgets configurés** ; à manier avec précaution en production
puisqu'il efface l'historique partagé.

**Alertes.** `SetBudgetAlertsEnabled(enabled)` active/désactive le log de warnings sur dépassement
(activé en debug, désactivé en release par défaut). `SetBudgetWarningThreshold(threshold)` fixe le
seuil **préventif** en `[0..1]` : `0.9f` déclenche une alerte « bientôt plein » à 90 % du budget,
émise depuis `OnAllocate`. C'est ce qui transforme le système de **réactif** (« trop tard, ça a
débordé ») en **préventif** (« il reste 10 %, agis »).

### Helpers taggés — allouer en comptant

`TaggedAlloc`, `TaggedFree` et `TaggedCalloc` sont des **free functions** (non `noexcept`, non
`[[nodiscard]]`) qui combinent allocation et tagging. `TaggedAlloc(bytes, tag, allocator, alignment)`
alloue puis appelle `OnAllocate` après succès (`nullptr` en cas d'échec) ; `TaggedCalloc(count,
elementSize, tag, …)` fait pareil mais **zéro-initialise** `count * elementSize` octets ;
`TaggedFree(ptr, tag, allocator)` appelle `OnDeallocate` **avant** de libérer (`nullptr` accepté =
no-op). `allocator = nullptr` ⇒ allocateur par défaut, et `alignment` vaut `NK_MEMORY_DEFAULT_ALIGNMENT`
(défini dans `NkAllocator.h`).

La **règle dure NKMemory** s'applique sans exception. On n'utilise **jamais** `new`/`delete` bruts, et
on ne **croise jamais** les allocateurs : un bloc pris par `TaggedAlloc` se rend par `TaggedFree`, avec
le **même tag et le même allocator**. Le libérer par `std::free`, `delete[]` ou même `NkFree` direct
casse la comptabilité et, pire, corrompt le tas (Windows `c0000374`). Cette symétrie est la version
« tags » de la règle générale **Create ⇄ Destroy** : toute prise de ressource a sa restitution
appariée, par la même API.

En release avec `NKENTSEU_DISABLE_MEMORY_TAGGING`, les trois helpers retombent sur `NkAlloc`/`NkFree`
directs (zéro overhead), budgets et stats ne sont plus suivis, et `GetStats` renvoie des zéros : ne
bâtissez donc aucune logique de jeu critique sur la valeur des budgets.

---

### Exemple récapitulatif

```cpp
#include "NKMemory/NkTag.h"
using namespace nkentseu::memory;

// 1. Au démarrage : on fixe les enveloppes des gros sous-systèmes.
void InitBudgets() {
    NkMemoryBudget::SetBudget(NkMemoryTag::NK_MEMORY_TEXTURE, 512ull * 1024 * 1024); // 512 Mo
    NkMemoryBudget::SetBudget(NkMemoryTag::NK_MEMORY_MESH,    128ull * 1024 * 1024);
    NkMemoryBudget::SetBudgetWarningThreshold(0.9f);   // prévenir à 90 %
    NkMemoryBudget::SetBudgetAlertsEnabled(true);
}

// 2. Charger une texture, en restant dans le budget (cycle symétrique).
void* LoadTexture(nk_size bytes) {
    if (NkMemoryBudget::IsOverBudget(NkMemoryTag::NK_MEMORY_TEXTURE))
        EvictLeastRecentlyUsedTextures();              // libérer avant de prendre
    return TaggedAlloc(bytes, NkMemoryTag::NK_MEMORY_TEXTURE);
}
void FreeTexture(void* tex) {
    TaggedFree(tex, NkMemoryTag::NK_MEMORY_TEXTURE);   // MÊME tag, MÊME allocator
}

// 3. Profiler une frame : instantané + dérivées chaînées.
void ProfileFrame(nk_uint64 freeBytes) {
    NkMemoryTagStats s = NkMemoryBudget::GetStats(NkMemoryTag::NK_MEMORY_RENDER);
    s.ComputeAverage().EstimateFragmentation(freeBytes);
    NkMemoryBudget::DumpStats();                        // toutes les 60 frames, par ex.
}

// 4. Benchmark isolé : reset des compteurs sans perdre les budgets.
void Benchmark() {
    NkMemoryBudget::ResetStats();                       // garde les budgets configurés
    for (int i = 0; i < 1000; ++i) {
        void* p = TaggedCalloc(64, 1024, NkMemoryTag::NK_MEMORY_GAME);  // zéro-initialisé
        TaggedFree(p, NkMemoryTag::NK_MEMORY_GAME);
    }
    NkMemoryTagStats s = NkMemoryBudget::GetStats(NkMemoryTag::NK_MEMORY_GAME);
    // s.peakAllocated, s.allocationCount, ...
}
```

---

[← Le ramasse-miettes](GarbageCollector.md) · [Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md) · [Index par pointeur →](Pointer-Hash.md)
