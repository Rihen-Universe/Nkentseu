# Atomiques et synchronisation

> Couche **Foundation** · NKCore · Les primitives de bas niveau pour partager des données entre
> threads sans data race : la variable atomique `NkAtomic<T>`, le drapeau `NkAtomicFlag`, le verrou
> à attente active `NkSpinLock` / `NkScopedSpinLock`, les barrières mémoire et l'enum
> `NkMemoryOrder`. C'est l'équivalent maison de `<atomic>`.

Dès qu'un programme utilise plusieurs threads, un danger apparaît : si deux threads modifient la
même variable en même temps, le résultat est imprévisible — c'est une *data race*, et c'est l'un
des bugs les plus retors qui soient, car il dépend du timing et ne se reproduit presque jamais.
NKCore fournit les outils de bas niveau pour s'en prémunir : des variables **atomiques**
(modifiables de façon indivisible) et des **verrous** (pour protéger une section de code). On ne
construit pas un moteur multithread sur ces seules briques — au-dessus, NKThreading offre les
mutex bloquants, les pools de threads et les files de tâches — mais tout repose, *in fine*, sur ce
qui suit.

- **Namespace** : `nkentseu`
- **Header** : `#include "NKCore/NkAtomic.h"`

> **Avertissement de loyauté.** Cette page documente l'API **réelle** telle qu'elle est
> implémentée aujourd'hui, pièges compris. Trois faits comptent avant tout : (1) le paramètre
> `NkMemoryOrder` est **toujours ignoré** au runtime — chaque opération émet en réalité une
> barrière `NK_SEQCST` ; (2) sous MSVC, les types dont la taille n'est pas dans {1, 2, 4, 8}
> octets (ou hors {4, 8} pour `Exchange`) **retombent en non-atomique** silencieusement ;
> (3) `CompareExchangeStrong` est aujourd'hui identique à `CompareExchangeWeak`. Lisez ces
> nuances avant de bâtir un algorithme lock-free dessus.

---

## La variable atomique

Une opération est *atomique* si elle se produit « d'un seul coup », sans qu'un autre thread puisse
l'observer à moitié faite. `NkAtomic<T>` enveloppe une valeur `T` (qui doit être trivialement
copiable) et garantit cette indivisibilité pour toutes les opérations qu'on fait dessus. Le type
est **non-copiable** (constructeur et affectation de copie `delete`) : un atomique ne se duplique
pas, il se partage par référence ou par pointeur.

Le cas typique, c'est le compteur partagé. Incrémenter un `int` ordinaire depuis plusieurs threads
est une data race ; avec `NkAtomic`, c'est sûr :

```cpp
NkAtomic<nk_uint32> counter{0};   // ctor explicit — pas d'init implicite via '='
counter.FetchAdd(1);              // incrément atomique, depuis n'importe quel thread
```

`FetchAdd` ajoute une valeur et renvoie l'**ancienne**, le tout atomiquement. La panoplie complète
couvre la lecture (`Load`), l'écriture (`Store`), l'échange (`Exchange`, qui pose une nouvelle
valeur et rend l'ancienne) et la soustraction (`FetchSub`). Le constructeur par défaut initialise
à `T()` (zéro) ; le constructeur à un argument est **explicit**, donc `NkAtomic<int> a = 5;` ne
compile pas — il faut `NkAtomic<int> a{5};`.

Une question revient sans cesse : « cette opération me rend-elle l'ancienne ou la nouvelle valeur ? »
Retenez-la une fois pour toutes, car l'API est **asymétrique** : les `Fetch*`, le post-incrément
`x++` et le post-décrément `x--` rendent l'**ancienne** valeur ; le pré-incrément `++x`, le
pré-décrément `--x`, et les `+= -= &= |= ^=` rendent la **nouvelle**. Les opérateurs arithmétiques
reconstruisent la nouvelle valeur à partir du retour de `Fetch*` (`++x` ≡ `FetchAdd(1) + 1`) :
corrects pour des entiers, à éviter sur des types exotiques.

Une opération mérite une mention spéciale, le *compare-and-swap* (CAS) : « écris cette valeur, mais
**seulement si** la valeur actuelle est encore celle que je crois ». C'est la brique des algorithmes
lock-free :

```cpp
nk_uint32 expected = counter.Load();
while (!counter.CompareExchangeWeak(expected, expected + 1)) {
    // un autre thread est passé entre-temps ; 'expected' a été rafraîchi, on réessaie
}
```

`CompareExchangeWeak` peut échouer même quand la valeur correspondait (pour des raisons
matérielles) : on l'utilise donc **toujours dans une boucle**. Quand il échoue, il met `expected`
**à jour** avec la valeur réellement lue — c'est ce qui permet de réessayer sans relire. Une variante
`CompareExchangeStrong` existe (sémantiquement « sans échec spurieux »), mais elle **délègue à Weak**
dans l'implémentation actuelle : utilisez-la pour exprimer l'intention, sans compter sur la garantie
anti-spurious tant qu'elle n'est pas écrite.

> **En résumé.** `NkAtomic<T>` rend une valeur partagée modifiable sans verrou et sans data race :
> `Load`/`Store`/`Exchange`/`FetchAdd`/`FetchSub`/`CompareExchange*` plus les opérateurs. Non-copiable,
> ctor à un argument **explicit**. Règle d'or du retour : `Fetch*` et `x++`/`x--` → **ancienne**
> valeur ; `++x`/`--x` et `+=`/`-=`/`&=`/`|=`/`^=` → **nouvelle**. CAS toujours en boucle.

---

## Les ordres mémoire

Chaque opération atomique accepte un dernier paramètre optionnel, un `NkMemoryOrder`. Il ne concerne
pas l'atomicité elle-même, mais les **garanties d'ordre** entre cette opération et les
lectures/écritures qui l'entourent — un sujet subtil du modèle mémoire C++11. Les valeurs vont du
plus permissif au plus strict : `NK_RELAXED` (atomicité seule, aucun ordre garanti), `NK_CONSUME`
(acquire pour dépendances de données, rarement utilisé), `NK_ACQUIRE` (barrière de lecture :
on voit les écritures précédentes), `NK_RELEASE` (barrière d'écriture : on rend visibles les
écritures précédentes), `NK_ACQREL` (les deux combinés, pour un read-modify-write), et `NK_SEQCST`
(cohérence séquentielle — le plus fort, mais le plus coûteux).

Voilà la vérité de l'implémentation actuelle : **le paramètre `order` est partout ignoré**
(`(void)order;`), et toutes les opérations émettent en réalité une barrière `NK_SEQCST`. L'argument
sert donc de documentation et de compatibilité d'API, pas d'optimisation runtime. Concrètement,
passer `NK_RELAXED` à un compteur ne vous fera pas gagner de cycles aujourd'hui — le code se comporte
comme s'il était toujours `NK_SEQCST`. La règle de prudence reste donc simple et heureuse : laissez
le défaut, qui est toujours correct.

À côté de `NkAtomic`, `NkAtomicFlag` est le primitif lock-free le plus élémentaire : un drapeau qu'on
positionne avec `TestAndSet` (qui rend l'**ancienne** valeur — `true` = déjà pris par un autre,
`false` = acquis maintenant) et qu'on remet à zéro avec `Clear`. Subtilité à noter : le défaut
d'ordre de `TestAndSet` est `NK_ACQREL`, alors que la plupart des autres méthodes défaut à
`NK_SEQCST` (ce qui, vu le point ci-dessus, ne change rien au runtime — mais documente l'intention).

> **En résumé.** `NkMemoryOrder` ordonne les valeurs `NK_RELAXED` < `NK_CONSUME` < `NK_ACQUIRE` /
> `NK_RELEASE` < `NK_ACQREL` < `NK_SEQCST`. Mais aujourd'hui **l'argument est ignoré** : tout
> s'exécute en `NK_SEQCST`. Ne construisez pas d'optimisation acquire/release fine dessus en l'état.
> `NkAtomicFlag` (`TestAndSet`/`Clear`/`IsSet`) est le drapeau lock-free minimal.

---

## Le verrou : NkSpinLock

Quand on veut protéger non pas une variable mais toute une **section de code** — par exemple le fait
de pousser un élément dans une file partagée —, on emploie un verrou. `NkSpinLock` est un verrou par
*attente active* : un thread qui veut entrer alors que le verrou est pris tourne en boucle jusqu'à
ce qu'il se libère. Pour ne pas brûler bêtement le CPU pendant l'attente, il pratique un **backoff
exponentiel** : à chaque tour raté il double le nombre d'itérations d'attente (plafonné à 1024) en
émettant l'instruction matérielle `PAUSE` / `YIELD`, qui souffle un instant le pipeline du cœur.

```cpp
NkSpinLock lock;

lock.Lock();
queue.PushBack(v);
lock.Unlock();
```

Cette stratégie d'attente active a une conséquence directe : elle ne vaut que pour des sections
**très courtes** et peu disputées. Sur une section longue, ou très contendue, le spin gaspille du
CPU à ne rien faire — un mutex bloquant (du côté de NKThreading) serait alors préférable. `TryLock`
permet de tenter l'acquisition sans bloquer, et renvoie `true` si le verrou a été obtenu. Deux
mises en garde : `NkSpinLock` est **non récursif** — le même thread qui re-`Lock()` un verrou
qu'il tient déjà se bloque lui-même (deadlock) — et il n'interdit pas explicitement la copie au
niveau du compilateur : **ne le copiez pas**.

Écrire `Lock()` puis `Unlock()` à la main expose au même genre d'oubli qu'un `Delete` manqué : si un
`return` ou une exception saute le `Unlock`, le verrou reste pris pour toujours. La solution est la
même que pour les smart pointers — un objet RAII, `NkScopedSpinLock`, qui verrouille à sa construction
et déverrouille à sa destruction :

```cpp
void Push(int v) {
    NkScopedSpinLock guard(lock);   // Lock() ici
    queue.PushBack(v);
}   // Unlock() automatique, quoi qu'il arrive
```

`NkScopedSpinLock` est non-copiable et non-déplaçable, et `NkSpinLockGuard` en est un **alias exact**
(le même type, deux noms). Préférez **toujours** le guard à la paire manuelle.

> **En résumé.** Pour une valeur partagée (compteur, drapeau), `NkAtomic<T>` / `NkAtomicFlag` —
> atomiques, sans verrou. Pour une petite section de code, `NkScopedSpinLock` (RAII, jamais d'oubli).
> `NkSpinLock` est à attente active avec backoff, **non récursif**, réservé aux sections **courtes** ;
> au-delà, un mutex NKThreading. Laissez les ordres mémoire sur le défaut.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (comportement, cas
d'usage) dans la « Référence complète » qui suit.

### `enum class NkMemoryOrder : nk_uint8`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Ordre | `NK_RELAXED` (0) | Atomicité seule, aucune garantie d'ordre. |
| Ordre | `NK_CONSUME` (1) | Acquire pour dépendances de données (rare). |
| Ordre | `NK_ACQUIRE` (2) | Synchronisation lecture : voit les écritures précédentes. |
| Ordre | `NK_RELEASE` (3) | Synchronisation écriture : rend visibles les écritures précédentes. |
| Ordre | `NK_ACQREL` (4) | Acquire + Release (read-modify-write). |
| Ordre | `NK_SEQCST` (5) | Séquentiellement cohérent (plus fort, plus coûteux). |

*Argument `order` ignoré au runtime — tout s'exécute en `NK_SEQCST`.*

### `template <typename T> class NkAtomic`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkAtomic()`, `explicit NkAtomic(T)` | Init à `T()` / init à une valeur. Copie `= delete`. |
| Lecture / écriture | `Load(order)` `[O(1)]`, `Store(value, order)` `[O(1)]` | Charge / stocke atomiquement (barrière full). |
| Read-modify-write | `Exchange(value, order)` | Remplace, rend l'**ancienne** valeur. |
| Read-modify-write | `FetchAdd(value, order)`, `FetchSub(value, order)` | Ajoute / soustrait, rend l'**ancienne** valeur. |
| CAS | `CompareExchangeWeak(expected&, desired, order)` | CAS faible ; `expected` rafraîchi si échec. |
| CAS | `CompareExchangeWeak(expected&, desired, success, failure)` | Variante 2 ordres (`failure` ignoré). |
| CAS | `CompareExchangeStrong(expected&, desired, order)` | CAS fort (délègue à Weak aujourd'hui). |
| CAS | `CompareExchangeStrong(expected&, desired, success, failure)` | Variante 2 ordres (`failure` ignoré). |
| Utilitaire | `IsLockFree()` | `sizeof(T) <= 8` sur compilo supporté, sinon `false`. |
| Conversion | `operator T()`, `operator=(T)` | Lecture = `Load` / écriture = `Store`. |
| Arithmétique | `++` `++(int)` `--` `--(int)` `+=` `-=` | Incréments/décréments atomiques (pré → nouvelle, post → ancienne). |
| Bitwise | `&=` `\|=` `^=` | ET / OU / XOR atomiques (boucle CAS), rendent la **nouvelle** valeur. |

### Typedefs prédéfinis (`using`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Booléen | `NkAtomicBool` | `NkAtomic<nk_bool>`. |
| Entiers signés | `NkAtomicInt8/16/32/64`, `NkAtomicInt` | `NkAtomic<nk_intN>` ; `NkAtomicInt` = `<nk_int32>`. |
| Entiers non signés | `NkAtomicUInt8/16/32/64`, `NkAtomicUint` | `NkAtomic<nk_uintN>` ; `NkAtomicUint` = `<nk_uint32>`. |
| Taille / pointeur | `NkAtomicSize`, `NkAtomicPtr` | `NkAtomic<nk_size>` / `NkAtomic<void*>`. |

*Casse à respecter : `NkAtomicInt` / `NkAtomicUint` (le second en `int` minuscule), distincts de
`NkAtomicInt32` / `NkAtomicUInt32`.*

### `class NkAtomicFlag`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkAtomicFlag(nk_bool flag = false)` | Valeur initiale (non-explicit). |
| Acquisition | `TestAndSet(order = NK_ACQREL)` | Pose `true`, rend l'**ancienne** valeur. |
| Libération | `Clear(order)` | Remet à `false`. |
| Lecture | `IsSet()` | État courant. |

### `class NkSpinLock` / `class NkScopedSpinLock`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Verrou | `NkSpinLock()`, `Lock()`, `TryLock()`, `Unlock()` | Spin à backoff exponentiel ; **non récursif**, ne pas copier. |
| RAII | `explicit NkScopedSpinLock(NkSpinLock&)`, `~NkScopedSpinLock()` | Verrouille au scope, déverrouille en sortie. Non-copiable/déplaçable. |
| Alias | `NkSpinLockGuard` | Synonyme exact de `NkScopedSpinLock`. |

### Barrières mémoire et fonctions globales (free functions)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Barrière | `NkAtomicThreadFence(order)` | Barrière full (CPU). `order` ignoré. |
| Barrière | `NkAtomicAcquireFence()`, `NkAtomicReleaseFence()` | Barrière acquire / release. |
| Barrière | `NkAtomicCompileBarrier()` | Barrière **compile-time seule** (aucune instruction CPU). |
| Utilitaire | `NkAtomicIncrement(atomic, order)`, `NkAtomicDecrement(atomic, order)` | `FetchAdd/Sub(1)` → rendent l'**ancienne** valeur. |
| Utilitaire | `NkAtomicAdd(atomic, value, order)`, `NkAtomicSubtract(atomic, value, order)` | → rendent la **nouvelle** valeur. |

---

## Référence complète

Chaque élément est repris ici en détail. Les éléments triviaux (construction, conversions) sont
décrits brièvement ; les opérations qui comptent vraiment le sont **à fond**, avec leurs usages dans
les différents domaines du temps réel — rendu, ECS, physique, animation, gameplay/IA, audio, UI/2D,
IO, GPU, threading.

### `NkMemoryOrder` — les ordres mémoire

L'enum (`nk_uint8`, six valeurs de 0 à 5) décrit la force de synchronisation qu'on **souhaiterait**
appliquer. Dans le modèle C++ standard, `NK_RELAXED` ne garantit que l'atomicité (un compteur de
statistiques où l'ordre n'importe pas), `NK_ACQUIRE`/`NK_RELEASE` forment la paire classique
producteur/consommateur (un thread publie des données puis lève un drapeau en `RELEASE`, un autre
lit le drapeau en `ACQUIRE` et voit les données), `NK_ACQREL` couvre les read-modify-write, et
`NK_SEQCST` impose un ordre global total. **Mais ici, le paramètre est ignoré partout** : toute
opération émet une barrière `NK_SEQCST` réelle. La conséquence pratique est rassurante en correction
(le défaut est toujours sûr) et décevante en performance (pas de gain à passer en `RELAXED`
aujourd'hui). Documentez vos intentions avec les bonnes valeurs — le jour où l'implémentation
honorera l'argument, votre code sera déjà juste.

### `NkAtomic<T>` — la valeur atomique

Le cœur du module. Membre `volatile T mValue` aligné sur `T`, opérations marquées `FORCE_INLINE` et
`noexcept` (les méthodes ; les free functions, elles, **ne sont pas** `noexcept`). Toutes les
opérations sont `O(1)`. Trois familles à connaître.

**Lecture / écriture — `Load`, `Store`.** Indivisibles, c'est le strict minimum : lire ou écrire une
valeur partagée sans déchirure (un `nk_int64` lu à moitié sur une plateforme 32 bits donnerait une
valeur fantôme). Usages :
- **Rendu / GPU** — un flag « swapchain à recréer » ou un indice de *frame in flight* écrit par le
  thread présentation, lu par le thread rendu.
- **Threading** — publier un pointeur de configuration rechargée à chaud (`Store`) que les workers
  relisent (`Load`).
- **Audio** — le thread UI `Store` un volume cible, le thread audio temps réel le `Load` par échantillon
  sans jamais bloquer (un mutex dans le callback audio = clic garanti).

**Read-modify-write — `Exchange`, `FetchAdd`, `FetchSub`.** Lisent, transforment et réécrivent en un
seul geste indivisible, en rendant l'**ancienne** valeur. `Exchange` pose une valeur et récupère
l'ancienne (idéal pour « récupère le travail en attente et remets le sac à vide » d'un coup).
`FetchAdd`/`FetchSub` sont la base des compteurs. Attention `FetchSub(v)` est implémenté comme
`FetchAdd(-v)` : sur un type **non signé**, `-v` wrappe (correct pour une soustraction modulaire,
mais à comprendre). Usages :
- **ECS / scène** — distribuer des IDs d'entités uniques (`FetchAdd(1)`) entre plusieurs threads de
  création, sans collision.
- **Threading** — compteur de référence d'une ressource partagée (`FetchAdd` à l'acquisition,
  `FetchSub` à la libération ; quand l'ancienne valeur retombe à 1 on détruit) ; compteur de tâches
  restantes d'un *job* parallèle.
- **Physique** — accumuler le nombre de paires de collision détectées par les threads de *broadphase*.
- **Gameplay / IA** — un score partagé, un compteur d'ennemis vivants décrémenté à chaque mort.
- **IO** — octets téléchargés cumulés par plusieurs connexions, pour une barre de progression.

**Compare-and-swap — `CompareExchangeWeak` / `CompareExchangeStrong`.** L'opération reine du
lock-free : « remplace par `desired` *si et seulement si* la valeur vaut encore `expected` ; sinon
dis-moi la vraie valeur ». Le retour `nk_bool` dit le succès ; en cas d'échec, `expected` est
rafraîchi avec la valeur courante, ce qui permet la boucle de retry sans relire. `Weak` peut échouer
spurieusement (raison matérielle) → **toujours dans une boucle**. `Strong` exprime l'absence d'échec
spurieux mais **délègue à Weak** pour l'instant : utilisez-le pour l'intention, pas pour la garantie.
Les surcharges à deux ordres (`success`, `failure`) existent pour la compatibilité d'API : `failure`
est **ignoré**. Usages :
- **Threading** — implémenter une pile ou une file *lock-free* (Treiber stack : on construit le
  nouveau nœud, on tente de l'enficher en tête via CAS, on réessaie si un autre est passé) ; un
  *spinlock* maison ; un *flag* d'initialisation paresseuse « une seule fois ».
- **ECS** — réclamer un slot libre dans un pool partagé (`CompareExchange` de l'état `LIBRE` →
  `OCCUPÉ`), sans verrou global.
- **Rendu** — réserver atomiquement une plage dans un buffer GPU à remplissage concurrent (allocateur
  *bump* lock-free).

**Conversions et opérateurs.** `operator T()` (lecture implicite = `Load` SEQCST) et `operator=(T)`
(écriture = `Store`) rendent l'usage naturel : `int v = atom;` / `atom = 3;`. Les opérateurs
arithmétiques `++ -- += -=` et bitwise `&= |= ^=` opèrent atomiquement ; rappelez-vous la règle de
retour (pré/composé → nouvelle, post → ancienne). Les bitwise passent par une **boucle CAS**
interne (ET/OU/XOR atomiques pour manipuler un champ de bits partagé — masque de couches actives, set
de flags d'état d'un *job system*).

**`IsLockFree`.** Indique si le type est traité sans verrou (approximation : `sizeof(T) <= 8` sur
compilateur supporté, `false` en fallback). Utile pour vérifier qu'un type maison ne tombera pas dans
le chemin lent.

**Le grand piège silencieux.** Sous MSVC, les intrinsics `Interlocked*` n'existent que pour les
tailles {1, 2, 4, 8} (et {4, 8} seulement pour `Exchange`). Au-delà, le code **retombe en
non-atomique** sans erreur : une `NkAtomic<MaStruct16octets>` compilera et tournera, mais ne sera
**pas** thread-safe. Restez sur les entiers, booléens et pointeurs — exactement ce que couvrent les
typedefs.

### Typedefs — les atomiques prêts à l'emploi

On manipule presque toujours `NkAtomic<T>` via ses alias : `NkAtomicBool`, `NkAtomicInt8/16/32/64`,
`NkAtomicUInt8/16/32/64`, plus `NkAtomicInt` / `NkAtomicUint` (= 32 bits), `NkAtomicSize`
(indices/compteurs de taille) et `NkAtomicPtr` (= `NkAtomic<void*>`, pour publier/échanger un
pointeur atomiquement — *double-buffering* de structures, hand-off de ressources entre threads).
Tous restent dans la zone « lock-free garantie » {1, 2, 4, 8} octets. Soignez la casse :
`NkAtomicInt`/`NkAtomicUint` ne sont **pas** `NkAtomicInt32`/`NkAtomicUInt32`.

### `NkAtomicFlag` — le drapeau test-and-set

Le primitif lock-free le plus élémentaire : un `NkAtomic<nk_bool>` habillé. `TestAndSet` pose `true`
et rend l'**ancienne** valeur (`true` = un autre l'avait déjà → vous n'avez pas l'accès ; `false` =
c'est vous qui venez de l'acquérir). `Clear` repose à `false`, `IsSet` lit l'état. Son défaut d'ordre
est `NK_ACQREL` (les autres types défaut à `NK_SEQCST`). Le ctor est **non-explicit** (`NkAtomicFlag
f = true;` compile). Usages :
- **Threading** — la brique d'un verrou minimal (`while (flag.TestAndSet()) { /* spin */ }
  … flag.Clear();`), ou un *guard* « tâche déjà prise par un worker ».
- **Animation / gameplay** — un *latch* « événement déjà déclenché cette frame » qu'on `Clear` au
  début du tick suivant.
- **IO** — signaler « données prêtes » d'un thread producteur vers un consommateur qui *poll*.

### `NkSpinLock` — le verrou à attente active

Protège une **section de code** plutôt qu'une variable. Bâti sur un `NkAtomicFlag`, il **spin** avec
**backoff exponentiel** : à chaque tour raté, le nombre d'itérations d'attente double (plafond 1024),
chaque itération émettant l'instruction matérielle de pause (`_mm_pause` sur x86/x64,
`yield` sur ARM/aarch64, no-op ailleurs) qui détend le pipeline et soulage l'hyper-threading.
`Lock()` bloque jusqu'à acquisition, `TryLock()` tente sans bloquer (`true` = obtenu), `Unlock()`
relâche (par le thread détenteur). **Non récursif** (re-lock par le même thread = deadlock) et
**non explicitement non-copiable** côté compilateur — ne le copiez jamais. Réservé aux sections
**courtes et peu disputées** : un spin sur une section longue brûle un cœur entier pour rien, là où
un mutex bloquant de NKThreading endormirait le thread. Usages :
- **Audio** — protéger l'enregistrement/retrait d'une voix dans la table de mixage, accès très bref.
- **Rendu / threading** — sérialiser le *push* dans une petite file de commandes partagée, l'insertion
  dans un cache de pipelines.
- **ECS** — garder une zone critique minuscule lors de l'allocation d'un archétype rare.
- **UI / 2D** — file d'événements d'entrée poussée par le thread OS, drainée par le thread UI.

### `NkScopedSpinLock` / `NkSpinLockGuard` — le verrou RAII

Le compagnon obligé de `NkSpinLock`. Écrire `Lock()`/`Unlock()` à la main, c'est risquer qu'un
`return` ou une exception saute le `Unlock` et fige le verrou à jamais. `NkScopedSpinLock` prend une
référence (`explicit`, non-copiable, non-déplaçable), verrouille à la construction, **déverrouille à
la destruction** — quoi qu'il arrive. `NkSpinLockGuard` est le **même type** sous un autre nom.
Idiome : `NkScopedSpinLock guard(monLock);` en tête de scope, et on oublie le `Unlock`.

### Barrières mémoire explicites

Pour les cas où l'on ordonne des accès **mémoire ordinaires** autour d'opérations atomiques sans
passer par une variable atomique. `NkAtomicThreadFence` pose une barrière full CPU (`MemoryBarrier`
MSVC, `__atomic_thread_fence` GCC/Clang ; `order` ignoré). `NkAtomicAcquireFence` /
`NkAtomicReleaseFence` visent la moitié lecture / écriture. `NkAtomicCompileBarrier` est purement
**compile-time** : aucune instruction CPU émise, elle empêche seulement le compilateur de réordonner
les accès de part et d'autre (utile pour fixer l'ordre vis-à-vis d'un registre matériel mappé en
mémoire, ou d'un timing précis). **Piège** : sous GCC/Clang, Acquire/Release/Compile produisent
toutes la même barrière compilateur (pas de fence CPU acquire/release dédié). Aucune de ces fonctions
n'est `noexcept`. Domaines : threading bas niveau, IO mappé mémoire (MMIO), GPU (visibilité d'une
écriture avant un *signal* de fence côté CPU).

### Fonctions atomiques globales

Des helpers libres autour de `NkAtomic<T>`. `NkAtomicIncrement` / `NkAtomicDecrement` font
`FetchAdd/Sub(1)` et rendent l'**ancienne** valeur ; `NkAtomicAdd` / `NkAtomicSubtract` (noter
`Subtract`, pas `Sub`) ajoutent/soustraient une valeur et rendent la **nouvelle**. Cette
**asymétrie de retour** est volontaire mais piégeuse : un *refcount* via `NkAtomicDecrement` teste si
le retour vaut `1` (l'ancienne valeur, donc « on était le dernier »), tandis qu'un cumul via
`NkAtomicAdd` lit directement le total courant. Aucune n'est `noexcept`.

> **Récapitulatif des pièges.** (1) `order` ignoré partout → tout est `NK_SEQCST` réel ; (2)
> `CompareExchangeStrong` == `Weak` ; (3) sous MSVC, tailles hors {1,2,4,8} (hors {4,8} pour
> `Exchange`) → **non-atomique silencieux** ; (4) `NkSpinLock` non récursif et non protégé contre la
> copie ; (5) retour **ancienne** pour `Fetch*`/`x++`/`x--`/`TestAndSet`/`NkAtomicIncrement`/
> `NkAtomicDecrement`, **nouvelle** pour `++x`/`--x`/`+=`/`-=`/`&=`/`|=`/`^=`/`NkAtomicAdd`/
> `NkAtomicSubtract` ; (6) défaut d'ordre `NK_ACQREL` pour `NkAtomicFlag::TestAndSet`, `NK_SEQCST`
> ailleurs.

---

### Exemple récapitulatif

```cpp
#include "NKCore/NkAtomic.h"
using namespace nkentseu;

// Compteur partagé entre threads — incrément atomique, lecture sûre.
NkAtomicUInt32 framesRendered{0};
framesRendered.FetchAdd(1);                 // rend l'ANCIENNE valeur
nk_uint32 total = framesRendered.Load();    // lecture indivisible

// Compteur de références via les helpers globaux (refcount classique).
NkAtomicInt32 refs{1};
NkAtomicIncrement(refs);                     // acquire — ancienne valeur
if (NkAtomicDecrement(refs) == 1) {          // on était le dernier détenteur
    // ... détruire la ressource ...
}

// Compare-and-swap : réclamer un slot LIBRE (0) -> OCCUPÉ (1), lock-free.
NkAtomicInt32 slot{0};
nk_int32 expected = 0;
if (slot.CompareExchangeWeak(expected, 1)) {
    // slot acquis sans verrou
}

// Drapeau test-and-set : un latch déclenché une seule fois.
NkAtomicFlag fired;
if (!fired.TestAndSet()) {                   // false = c'est nous qui l'acquérons
    // ... action unique ...
}

// Section critique protégée — RAII, Unlock garanti même en cas de return/throw.
NkSpinLock lock;
void Push(int v, NkVector<int>& queue) {
    NkScopedSpinLock guard(lock);            // Lock() ici
    queue.PushBack(v);
}                                            // Unlock() automatique
```

---

[← Index NKCore](README.md) · [Récap NKCore](../NKCore.md) · [Les traits →](Traits.md)
