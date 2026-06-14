# Suivi des fuites et profilage

> Couche **Foundation** · NKMemory · Savoir si **tout ce que j'ai alloué a été libéré**
> (le traceur `NkMemoryTracker`) et **combien de mémoire je consomme là, maintenant** (le
> profileur `NkMemoryProfiler`).

Dès qu'on alloue beaucoup de mémoire, deux questions finissent toujours par se poser :
*est-ce que tout ce que j'ai alloué a bien été libéré ?* et *combien de mémoire mon programme
consomme-t-il, à cet instant ?* Ce sont deux questions **différentes** — l'une qualitative
(qui n'a pas été libéré, et **où** l'allocation a-t-elle été faite ?), l'autre quantitative
(combien d'octets vivants, quel pic ?) — et NKMemory leur répond avec **deux outils distincts**
qu'il ne faut pas confondre. Le **traceur** (`NkMemoryTracker`) retient *chaque allocation
vivante* avec ses métadonnées (fichier, ligne, fonction) pour cracher la liste des fuites au
shutdown. Le **profileur** (`NkMemoryProfiler`) ne retient *aucun pointeur* : il agrège des
compteurs (totaux, live, pic) et déclenche des **hooks** à chaque allocation. L'un trouve les
fuites, l'autre mesure la consommation.

Les deux ne fonctionnent que parce que **toutes les allocations passent par un allocateur** —
c'est tout l'intérêt de l'indirection NKMemory vue au premier chapitre. Un allocateur custom
appelle `NK_TRACK_ALLOC` / `NkMemoryProfiler::NotifyAlloc` après chaque succès, et
`NK_UNTRACK_ALLOC` / `NkMemoryProfiler::NotifyFree` à chaque libération ; ni le traceur ni le
profileur ne « voient » la mémoire d'eux-mêmes. Rappel de la **règle dure** du module : aucune
allocation par `new`/`delete` directs, tout passe par NKMemory (sinon heap corruption Windows
`c0000374` quand on mélange allocateur custom et heap CRT).

- **Namespace** : `nkentseu::memory`
- **Headers** : `#include "NKMemory/NkTracker.h"` · `#include "NKMemory/NkProfiler.h"`

---

## Détecter les fuites avec NkMemoryTracker

Le principe est simple : le traceur garde une trace de **chaque allocation vivante**, avec ses
métadonnées — surtout le fichier, la ligne et la fonction d'où elle provient. Tant qu'une
allocation n'a pas été désenregistrée, elle reste dans la table. Il suffit donc, à la fermeture
du programme, de demander ce qui reste : **ce qui reste, ce sont les fuites.**

```cpp
using namespace nkentseu::memory;

// à la toute fin de l'application :
GetGlobalMemoryTracker().DumpLeaks();
```

`GetGlobalMemoryTracker()` est la **fonction libre** qui donne accès à l'instance globale
unique du traceur (un singleton de Meyer, thread-safe à l'initialisation). Si la liste est vide,
aucune fuite. Sinon, chaque entrée vous dit *où* l'allocation non libérée a été faite — c'est
cette information (le `fichier:ligne` + la fonction) qui transforme « il y a une fuite quelque
part » en « il y a une fuite ligne 214 de tel fichier ». C'est tout ce qui sépare une chasse au
bug d'une heure d'une correction de deux minutes.

Sous le capot, le traceur n'est **pas** une simple liste qu'on parcourt linéairement : c'est une
**table de hachage à chaînage séparé** (4096 buckets), indexée par le pointeur utilisateur. Du
coup `Register`, `Unregister` et `Find` sont en **`O(1)` moyen** — on peut tracer un moteur
entier sans que le tracking devienne le goulot. Seuls `DumpLeaks` et `Clear` sont en `O(n)`
(ils balaient toutes les entrées vivantes). L'accès concurrent est protégé par un `NkSpinLock`,
donc on peut allouer depuis plusieurs threads sans corrompre la table.

`GetStats()` complète le tableau avec un **instantané chiffré** : octets vivants, pic atteint,
allocations vivantes, total cumulé.

```cpp
auto s = GetGlobalMemoryTracker().GetStats();   // liveBytes / peakBytes / liveAllocations / totalAllocations
```

> **En résumé.** `NkMemoryTracker` répond « ai-je une fuite, et **où** ? ». Appelez
> `DumpLeaks()` au shutdown ; lisez `GetStats()` pour un instantané chiffré. Lookup par pointeur
> en `O(1)` moyen (hash-table 4096 buckets, thread-safe via spinlock). Accès via la fonction
> libre `GetGlobalMemoryTracker()`.

### Cerner une fuite par scope

`DumpLeaks()` global est parfait au shutdown, mais comment savoir *quel sous-système* fuit ?
L'astuce est de **comparer les statistiques à l'entrée et à la sortie** d'une zone de code, en
s'appuyant sur le champ `liveBytes` du snapshot `GetStats()`. Le header de NKMemory montre ce
patron — un petit objet RAII — qu'il vous revient d'écrire (NKMemory ne fournit *pas* cette
classe toute faite) :

```cpp
class ScopedMemoryTracker {
public:
    explicit ScopedMemoryTracker(const char* name)
        : mName(name), mStart(GetGlobalMemoryTracker().GetStats()) {}
    ~ScopedMemoryTracker() {
        auto end = GetGlobalMemoryTracker().GetStats();
        nk_size leaked = end.liveBytes - mStart.liveBytes;
        if (leaked) GetGlobalMemoryTracker().DumpLeaks();   // + un log du scope 'mName'
    }
private:
    const char*            mName;
    NkMemoryTracker::Stats mStart;
};

{
    ScopedMemoryTracker check("ChargementNiveau");
    LoadLevel();
}   // à la destruction : signale les octets non libérés pendant ce scope
```

Le `NkMemoryTracker::Stats` est un `struct` à quatre champs (`liveBytes`, `peakBytes`,
`liveAllocations`, `totalAllocations`) ; on calcule le delta entrée/sortie sur `liveBytes`. À
noter : `GetStats()` retourne une **copie** prise sous lock — la valeur peut changer juste après
l'appel (un autre thread alloue), c'est un instantané, pas une vue live.

> **En résumé.** Pas de tracking par sous-système intégré : on l'obtient en comparant deux
> `GetStats()` (delta sur `liveBytes`) autour d'un scope, via un petit RAII maison. Le snapshot
> est une copie, donc une photo instantanée.

### Réinitialiser le tracking : `Clear`

`Clear()` réinitialise complètement le traceur : il libère toutes les entrées de la table et
remet les compteurs à zéro. Point crucial — `Clear()` **ne touche pas aux allocations réelles**,
seulement au *tracking*. Son usage typique : juste avant un shutdown ordonné, pour purger les
entrées dont on sait qu'elles seront libérées par un mécanisme que le traceur ne voit pas (et
ainsi éviter les **faux positifs** au `DumpLeaks()` final). C'est une opération `O(n)`.

> **En résumé.** `Clear()` ≠ free : il vide la table de tracking et reset les compteurs, sans
> libérer la mémoire réelle. À utiliser pour écarter les faux positifs avant un shutdown.

---

## Les métadonnées d'une allocation : `NkAllocationInfo`

Chaque entrée tracée est un `NkAllocationInfo`, copié tel quel dans la table. C'est la
**fiche d'identité** d'une allocation : son pointeur, sa taille, son tag, et surtout sa
**provenance** (fichier, ligne, fonction). Tous les pointeurs de chaîne y sont **non-possédés** :
ils référencent des littéraux `__FILE__` / `__func__` qui vivent pour toute la durée du
programme — le traceur ne les copie ni ne les libère.

On le construit rarement à la main : la fabrique `NkAllocationInfo::Make(ptr, size, tag, file,
line, func)` remplit tout en `O(1)` (`userPtr = basePtr = ptr`, `count = 1`, `name = nullptr`,
timestamp/threadId à 0). C'est exactement ce que fait la macro `NK_TRACK_ALLOC` pour vous. Le
constructeur par défaut, lui, **zéro-initialise tous les champs** (`constexpr`, `noexcept`).

> **En résumé.** `NkAllocationInfo` = fiche d'une allocation (ptr, taille, tag, file/line/func,
> timestamp, thread). Pointeurs de chaîne **non-possédés**. On la fabrique via `Make(...)` en
> `O(1)` ; `NK_TRACK_ALLOC` s'en charge.

---

## Mesurer la consommation avec NkMemoryProfiler

Là où le traceur s'intéresse aux *fuites* (qualitatif : qui n'a pas été libéré, et où), le
profileur s'intéresse à la *consommation* (quantitatif : combien). Il ne retient **aucun
pointeur** et ne sait **pas** dumper de fuites : il agrège quelques compteurs globaux, qu'on lit
avec `GetGlobalStats()`. C'est une **API entièrement statique** — il n'y a *rien* à instancier,
on appelle directement `NkMemoryProfiler::...`.

```cpp
struct GlobalStats {
    nk_uint64 totalAllocations;   // cumulatif
    nk_uint64 totalFrees;         // cumulatif
    nk_uint64 liveAllocations;    // actives maintenant
    nk_uint64 liveBytes;          // octets actuellement alloués
    nk_uint64 peakBytes;          // pic historique
    float32   avgAllocationSize;  // taille moyenne (octets)
};

void FrameEnd() {
    auto g = NkMemoryProfiler::GetGlobalStats();
    Log("live=%llu o, peak=%llu o", g.liveBytes, g.peakBytes);
}
```

Le profileur expose des **hooks** qu'on installe une fois pour toutes : `SetAllocCallback`,
`SetFreeCallback`, `SetReallocCallback`. Chacun reçoit un pointeur de fonction `noexcept`
(passer `nullptr` désactive le hook — les compteurs internes, eux, restent toujours à jour).
C'est le point d'entrée pour instrumenter finement : mesurer le temps entre deux allocations,
journaliser un pic, alimenter un graphe en temps réel. Les callbacks sont déclenchés par les
notifications explicites `NotifyAlloc`, `NotifyFree`, `NotifyRealloc` que les allocateurs
appellent eux-mêmes.

```cpp
NkMemoryProfiler::SetAllocCallback(
    [](void* ptr, nk_size size, const nk_char* tag) noexcept {
        // surtout : NE PAS allouer ici (récursion infinie alloc → callback → alloc)
        g_ring.PushSample(ptr, size, tag);   // ring-buffer pré-alloué
    });
```

**Le piège mortel des callbacks** : ils sont marqués `noexcept` *et* ne doivent **jamais
allouer de mémoire**. Un callback qui alloue déclenche une nouvelle notification, qui rappelle
le callback, qui réalloue… récursion infinie. En pratique : ring-buffers pré-alloués, simples
compteurs, ou logging asynchrone. Ils peuvent par ailleurs être appelés **depuis n'importe quel
thread**.

Un réflexe à garder : échantillonnez `GetGlobalStats()` **périodiquement** (toutes les N
frames), pas à chaque allocation. Sinon le coût de la mesure finit par dominer celui qu'on
voulait mesurer. `DumpProfilerStats()` produit un dump compact (totaux + moyennes) vers le log.

> **En résumé.** `NkMemoryProfiler` répond « combien je consomme ? ». API **statique** (rien à
> instancier). Lisez `GetGlobalStats()` périodiquement ; installez `SetAllocCallback` /
> `SetFreeCallback` / `SetReallocCallback` pour instrumenter ; les callbacks doivent être
> `noexcept` et **ne jamais allouer**.

---

## Les deux ensemble

Traceur et profileur se complètent. En développement, on laisse le traceur actif et on vérifie
`DumpLeaks()` au shutdown — une liste vide devient une garantie de propreté. En production, le
suivi détaillé peut être désactivé pour son coût (`NKENTSEU_DISABLE_MEMORY_TRACKING` réduit
`Register`/`Unregister`/`Find` à des no-op inline), tout en gardant le profileur léger — de
simples compteurs — si l'on veut surveiller la consommation (et lui-même se désactive via
`NKENTSEU_DISABLE_MEMORY_PROFILING`). Et les deux deviennent bien plus parlants combinés aux
**tags** : on ne sait plus seulement *qu'on* fuit, mais *quel* sous-système fuit.

> **En résumé.** Traceur = « ai-je une fuite, où ? » (qualitatif, retient les pointeurs).
> Profileur = « combien je consomme ? » (quantitatif, agrégé, statique). En dev les deux ; en
> prod, profileur léger seul. Flags de build pour tout désactiver.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence complète »
qui suit. Complexités et `noexcept` entre crochets quand c'est utile.

### `struct NkAllocationInfo` — fiche d'une allocation (`NkTracker.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Pointeurs | `userPtr`, `basePtr` | Pointeur rendu à l'utilisateur (aligné) / pointeur brut de l'allocateur. |
| Tailles | `size`, `count`, `alignment` | Octets / nombre d'éléments (tableau) / alignement requis. |
| Tag | `tag` (`nk_uint8`) | `NkMemoryTag` casté en `uint8`. |
| Source | `file`, `line`, `function`, `name` | Fichier:ligne, fonction, nom optionnel — chaînes **non-possédées**. |
| Contexte | `timestamp`, `threadId` | Instant relatif / ID du thread allocateur. |
| Construction | `NkAllocationInfo()` `[constexpr, noexcept]` | Zéro-initialise **tous** les champs. |
| Fabrique | `static Make(ptr, sz, tag, file, line, func)` `[O(1), noexcept]` | Remplit la fiche (`userPtr=basePtr=ptr`, `count=1`). |

### `class NkMemoryTracker` — détection de fuites (`NkTracker.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkMemoryTracker()` `[noexcept]`, `~NkMemoryTracker()` | Init la hash-table / libère les entrées restantes (sous lock). **Non-copiable**. |
| Tracking | `Register(info)` `[O(1) moy., noexcept]` | Enregistre (ou met à jour) une allocation. |
| Tracking | `Unregister(ptr)` `[O(1) moy., noexcept]` | Désenregistre par `userPtr` ; no-op si absent. |
| Tracking | `Find(ptr)` `[O(1) moy., noexcept, [[nodiscard]]]` | Métadonnées par pointeur, `nullptr` sinon. |
| Reporting | `DumpLeaks()` `[O(n), noexcept]` | Dump de **toutes** les allocations non libérées. |
| Stats | `struct Stats { liveBytes; peakBytes; liveAllocations; totalAllocations; }` | Snapshot des compteurs. |
| Stats | `GetStats()` `[O(1), noexcept, [[nodiscard]]]` | Copie thread-safe des compteurs. |
| Cycle de vie | `Clear()` `[O(n), noexcept]` | Vide le tracking + reset compteurs (≠ free réel). |

### Fonction libre & macros (`NkTracker.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Accès global | `GetGlobalMemoryTracker()` `[noexcept]` | Instance globale unique (singleton de Meyer). |
| Macro | `NK_TRACK_ALLOC(ptr, size, tag)` | Si `ptr` non nul : `Make(...)` + `Register` (no-op si tracking désactivé). |
| Macro | `NK_UNTRACK_ALLOC(ptr)` | Si `ptr` non nul : `Unregister(ptr)`. |
| Flag build | `NKENTSEU_DISABLE_MEMORY_TRACKING` | Register/Unregister/Find → no-op ; DumpLeaks/GetStats vides. |
| Flag build | `NKENTSEU_MEMORY_TRACKING_VERBOSE` | Log + dump auto + assertions double-register / unregister inconnu. |

### Types de callbacks (`NkProfiler.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Hook | `AllocCallback = void(*)(void*, nk_size, const nk_char*) noexcept` | Notifié à chaque allocation (ptr, size, tag). |
| Hook | `FreeCallback = void(*)(void*, nk_size) noexcept` | Notifié à chaque libération (`size`=0 si inconnue). |
| Hook | `ReallocCallback = void(*)(void*, void*, nk_size, nk_size) noexcept` | Notifié à chaque réallocation (old/new ptr, old/new size). |

### `class NkMemoryProfiler` — profilage runtime (`NkProfiler.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Stats | `struct GlobalStats { totalAllocations; totalFrees; liveAllocations; liveBytes; peakBytes; avgAllocationSize; }` | Compteurs agrégés. |
| Installation | `static SetAllocCallback(cb)` `[noexcept]` | Installe le hook alloc (`nullptr` désactive). |
| Installation | `static SetFreeCallback(cb)` `[noexcept]` | Installe le hook free. |
| Installation | `static SetReallocCallback(cb)` `[noexcept]` | Installe le hook realloc. |
| Notification | `static NotifyAlloc(ptr, size, tag)` `[noexcept]` | Maj stats + callback, **après** alloc réussie. |
| Notification | `static NotifyFree(ptr, size)` `[noexcept]` | Maj stats + callback (`ptr` nul accepté). |
| Notification | `static NotifyRealloc(old, new, oldSz, newSz)` `[noexcept]` | Resize in-place ou réallocation complète. |
| Stats | `static GetGlobalStats()` `[O(1), noexcept, [[nodiscard]]]` | Snapshot thread-safe des compteurs. |
| Reporting | `static DumpProfilerStats()` `[O(1), noexcept]` | Dump compact (totaux + moyennes) vers le log. |
| Flag build | `NKENTSEU_DISABLE_MEMORY_PROFILING` | Notify* → no-op ; GetGlobalStats → zéros ; DumpProfilerStats → no-op. |

---

## Référence complète

### `NkAllocationInfo` — la fiche d'identité

C'est la donnée que le traceur stocke pour chaque allocation vivante, recopiée *par valeur* dans
la table. Les champs `userPtr` et `basePtr` distinguent le pointeur **rendu à l'utilisateur**
(potentiellement décalé pour respecter un alignement) du pointeur **brut** que l'allocateur
devra rendre au système — utile quand une allocation alignée masque sa vraie base. `size` /
`count` / `alignment` décrivent la forme de l'allocation, `tag` la range dans un sous-système
(via `NkMemoryTag`, chapitre des tags).

Le cœur du diagnostic, ce sont `file` / `line` / `function` (et le `name` optionnel, ex.
`"TextureCache"`). **Tous non-possédés** : ils pointent vers des littéraux `__FILE__`/`__func__`
captés à l'allocation, qui survivent au programme entier — donc aucune copie de chaîne, aucun
free. `timestamp` et `threadId` situent l'allocation dans le temps et entre threads.

On ne construit presque jamais un `NkAllocationInfo` à la main : la fabrique `Make(...)` le
remplit en `O(1)`, et c'est `NK_TRACK_ALLOC` qui l'appelle pour vous avec `__FILE__`, `__LINE__`,
`__func__`. Le constructeur par défaut (`constexpr noexcept`) zéro-initialise tout — pratique
pour une fiche neutre.

### `NkMemoryTracker` — le traceur, à fond

**Stratégie interne.** Le traceur est une **table de hachage à chaînage séparé** : 4096 buckets,
chaque bucket pointant une liste chaînée de nœuds `{ NkAllocationInfo, Next }`. La clé est le
pointeur utilisateur, dont on mixe les bits pour le disperser. Conséquence directe : les trois
opérations chaudes — `Register`, `Unregister`, `Find` — sont en **`O(1)` moyen**, ce qui rend
viable le traçage d'un moteur entier (des centaines de milliers d'allocations) sans que le
tracking devienne le goulot d'étranglement. Un `NkSpinLock` (NKCore) sérialise les accès : on
peut allouer/libérer depuis plusieurs threads sans corrompre la table. Le coût mémoire est
`4096 × sizeof(pointeur)` pour les buckets, plus ~100 octets par allocation vivante.

**Cycle de vie.** Le traceur est **non-copiable** (`= delete` sur copie et affectation) : son
usage prévu est une **instance globale unique**, obtenue par `GetGlobalMemoryTracker()`. Le
destructeur libère les entrées restantes sous lock (il n'est *pas* `noexcept`).

**Les opérations.** `Register(info)` insère la fiche en tête du bucket hashé ; si le pointeur est
déjà connu, il **met à jour** l'entrée au lieu de créer un doublon. `Unregister(ptr)` retire la
fiche par `userPtr` ; si le pointeur est introuvable, c'est un **no-op silencieux** (pas
d'erreur — robuste face à une double-libération). `Find(ptr)` (`[[nodiscard]]`) renvoie la fiche
ou `nullptr`. `DumpLeaks()` balaie toutes les entrées et émet une ligne par fuite (ptr, taille,
tag, fichier:ligne, fonction) plus un total, vers le log. `GetStats()` (`[[nodiscard]]`) renvoie
une copie des compteurs. `Clear()` vide la table et reset les compteurs **sans** libérer la
mémoire réelle.

L'usage par domaine du traceur tourne toujours autour de la même idée — *où ai-je oublié de
libérer ?* — déclinée selon le sous-système qui alloue :

- **Rendu / GPU** — un `NkTexture`, un `NkBuffer` ou un pipeline créé via `Create` mais jamais
  `Destroy` : le `DumpLeaks()` de fin pointe le `fichier:ligne` du `Create` orphelin (rappel : à
  chaque `Create` doit répondre un `Destroy`, et à chaque `New`/`NK_MEM_NEW` un `Delete`).
- **ECS** — des composants ou archétypes alloués au chargement d'une scène et non rendus au
  déchargement : un `ScopedMemoryTracker` autour du `LoadScene`/`UnloadScene` isole la fuite au
  sous-système.
- **Physique / collision** — des broadphases, paires de contact ou proxies recyclés frame après
  frame ; un `liveAllocations` qui **monte régulièrement** dans `GetStats()` trahit un cycle
  alloc-sans-free.
- **Animation** — des pistes, squelettes ou clips chargés à la demande : le `name` de la fiche
  (`"AnimClip:run"`) rend le rapport lisible.
- **Gameplay / IA** — des graphes de navigation, blackboards, arbres de comportement instanciés
  par entité : tracer permet de vérifier que la destruction d'un agent rend bien tout.
- **Audio** — des voix, buffers décodés (MP3/OGG) ou bus : une fuite ici grossit lentement et
  passe inaperçue sans traceur.
- **UI / 2D** — atlas de glyphes, listes de commandes, textures de fonts d'un panneau fermé.
- **IO / chargement** — buffers de fichiers lus puis « oubliés » après parsing.

**Pièges.** `GetStats()` est une **photo** prise sous lock : sa valeur peut être périmée dès
l'instruction suivante. `Clear()` ne libère **pas** la mémoire — il l'oublie ; ne l'utilisez pas
en pensant désallouer. Et `DumpLeaks()`/`Clear()` sont `O(n)` : à réserver au shutdown ou aux
bornes de scope, pas à la boucle chaude.

### `GetGlobalMemoryTracker` et les macros `NK_TRACK_ALLOC` / `NK_UNTRACK_ALLOC`

`GetGlobalMemoryTracker()` est la **fonction libre** (pas une méthode statique de la classe) qui
expose le singleton du traceur — c'est par elle qu'on appelle `DumpLeaks()` / `GetStats()`. Les
deux macros sont le pont entre vos allocateurs et ce singleton :

- `NK_TRACK_ALLOC(ptr, size, tag)` — si `ptr` est non nul, fabrique une fiche via
  `NkAllocationInfo::Make(ptr, size, tag, __FILE__, __LINE__, __func__)` et l'enregistre. C'est
  ce qui capture *automatiquement* la provenance — vous n'avez rien à passer.
- `NK_UNTRACK_ALLOC(ptr)` — si `ptr` est non nul, désenregistre.

On les place dans un allocateur custom : `NK_TRACK_ALLOC` dans `Allocate()` **après** le succès,
`NK_UNTRACK_ALLOC` dans `Deallocate()` **autour de** la libération réelle. Quand
`NKENTSEU_DISABLE_MEMORY_TRACKING` est défini, les deux macros deviennent `((void)0)` — zéro
coût en production. Le flag `NKENTSEU_MEMORY_TRACKING_VERBOSE`, lui, ajoute un log par
Register/Unregister, un dump automatique et des assertions (double-register, unregister d'un
pointeur inconnu) — précieux quand on traque une corruption.

### Les callbacks `AllocCallback` / `FreeCallback` / `ReallocCallback`

Ce sont trois **typedefs de pointeurs de fonction**, tous `noexcept`. Le profileur les invoque
au fil des notifications : `AllocCallback(ptr, size, tag)` à l'allocation, `FreeCallback(ptr,
size)` à la libération (`size` peut valoir 0 si inconnue, `ptr` nul est accepté),
`ReallocCallback(oldPtr, newPtr, oldSize, newSize)` à la réallocation. Deux contraintes
absolues, déjà soulignées : ils peuvent être appelés **depuis n'importe quel thread**, et ils ne
doivent **jamais allouer** (sous peine de récursion infinie). Le contrat `noexcept` est dans la
signature même — une exception qui s'en échappe est fatale.

### `NkMemoryProfiler` — le profileur, à fond

**Stratégie interne.** Le profileur est **purement statique** : trois pointeurs de callback
statiques, un `GlobalStats` agrégé, un `NkSpinLock` statique. Aucun lookup par pointeur, aucune
table — d'où un overhead minuscule (~40 octets pour stats + lock, ~10-20 cycles par
notification). Il **ne sait pas** lister des fuites : ce n'est pas son rôle, c'est celui du
traceur. On ne l'instancie **jamais** ; tout passe par `NkMemoryProfiler::...`.

**Installation.** `SetAllocCallback` / `SetFreeCallback` / `SetReallocCallback` posent (ou
retirent, avec `nullptr`) les hooks. Même sans hook installé, **les compteurs internes restent à
jour** — on peut donc profiler la consommation sans jamais écrire un seul callback.

**Notifications.** Ce sont les allocateurs eux-mêmes qui appellent `NotifyAlloc(ptr, size, tag)`
**après** une allocation réussie, `NotifyFree(ptr, size)` à la libération (souvent avec `size=0`
car la taille est rarement connue côté free), et `NotifyRealloc(oldPtr, newPtr, oldSize,
newSize)` au resize — qu'il soit in-place (`oldPtr==newPtr`) ou une vraie réallocation ; un
`oldPtr` nul signifie une première allocation, et on n'appelle pas la notif si `newPtr` est nul
(échec). Chaque notif met à jour les stats puis déclenche le callback correspondant s'il existe.

**Lecture & rapport.** `GetGlobalStats()` (`[[nodiscard]]`, `O(1)`) renvoie une copie du
`GlobalStats` (totaux cumulés, live, pic, taille moyenne) ; `DumpProfilerStats()` en imprime un
résumé compact dans le log.

Les usages par domaine du profileur tournent autour de *combien*, et *quand le pic survient* :

- **Rendu / GPU** — surveiller `liveBytes`/`peakBytes` autour de la création des ressources d'un
  niveau pour caler les budgets VRAM/RAM ; détecter le pic à l'upload des textures.
- **ECS** — mesurer le coût mémoire d'un spawn massif d'entités (vagues d'ennemis), tracer la
  courbe via un hook qui pousse `size` dans un graphe temps réel.
- **Physique** — observer la croissance des structures de broadphase quand la scène se densifie.
- **Animation / audio** — suivre la mémoire des clips et voix actifs pour décider quand évincer
  les plus anciens (politique de cache).
- **Gameplay / IA** — détecter un sous-système dont `totalAllocations` explose frame après frame
  (allocations par frame = mauvais signe).
- **UI / 2D** — mesurer l'empreinte des atlas et listes de commandes.
- **IO** — voir le pic transitoire au chargement d'un gros fichier, pour dimensionner un buffer
  de streaming.

**Pièges.** Échantillonnez `GetGlobalStats()` **périodiquement**, pas à chaque allocation
(sinon la mesure coûte plus cher que ce qu'elle mesure). Un callback qui alloue = récursion
infinie : ring-buffer pré-alloué obligatoire. En production, `NKENTSEU_DISABLE_MEMORY_PROFILING`
réduit les `Notify*` à des no-op inline et fait retourner des zéros à `GetGlobalStats()`.

### Le contexte NKMemory qui les entoure

Traceur et profileur ne servent qu'à *observer* des allocations qui suivent déjà les règles dures
du module. Rappel utile pour interpréter un rapport :

- **`new`/`delete` directs interdits** : tout passe par NKMemory (`NK_MEM_NEW`/`NK_MEM_DELETE`,
  `NkAllocator`, pools). Mélanger allocateur custom et heap CRT (`std::free`, `delete[]` sur un
  pointeur NKMemory) corrompt le heap (`c0000374`) — et le traceur, qui voit l'enregistrement
  mais pas la libération CRT, signalera alors une fausse fuite.
- **Symétrie `Create`/`Destroy`** : toute classe avec une méthode de création doit exposer la
  destruction. Une fuite tracée pointant un `Create` est presque toujours un `Destroy` oublié.
- **Pools** : détruire explicitement (`obj->~T()`) **avant** `Deallocate()` (et avant `Reset()`,
  qui n'appelle pas les destructeurs) — sinon le tracking reste cohérent mais l'objet fuit ses
  propres ressources.

---

### Exemple récapitulatif

```cpp
#include "NKMemory/NkTracker.h"
#include "NKMemory/NkProfiler.h"
using namespace nkentseu::memory;

// 1) Dans un allocateur custom : brancher tracking + profiling.
void* MyAllocator::Allocate(nk_size size, NkMemoryTag tag) {
    void* p = RawAlloc(size);
    if (p) {
        NK_TRACK_ALLOC(p, size, tag);                  // fiche file:line:func auto
        NkMemoryProfiler::NotifyAlloc(p, size, "MyAllocator");
    }
    return p;
}
void MyAllocator::Deallocate(void* p) {
    NK_UNTRACK_ALLOC(p);
    NkMemoryProfiler::NotifyFree(p, 0);                // taille inconnue côté free → 0
    RawFree(p);
}

// 2) Instrumenter un hook (NE PAS allouer dedans).
NkMemoryProfiler::SetAllocCallback(
    [](void* /*ptr*/, nk_size size, const nk_char* /*tag*/) noexcept {
        g_allocSizeRing.Push(size);                    // ring-buffer pré-alloué
    });

// 3) Échantillonner la consommation, périodiquement.
void OnFrameEnd() {
    static int n = 0;
    if ((n++ % 60) == 0) {                             // 1 frame sur 60
        auto g = NkMemoryProfiler::GetGlobalStats();
        Log("live=%llu o  peak=%llu o  moy=%.0f o", g.liveBytes, g.peakBytes, g.avgAllocationSize);
    }
}

// 4) Au shutdown : la liste des fuites, avec leur provenance.
void OnShutdown() {
    auto s = GetGlobalMemoryTracker().GetStats();
    if (s.liveAllocations != 0)
        GetGlobalMemoryTracker().DumpLeaks();          // ptr, size, tag, fichier:ligne, fonction
}
```

---

[← Opérations mémoire](Memory-Operations.md) · [Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md) · [Le ramasse-miettes →](GarbageCollector.md)
