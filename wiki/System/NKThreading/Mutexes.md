# Les verrous

> Couche **System** · NKThreading · Protéger une donnée partagée entre plusieurs threads :
> le mutex standard `NkMutex`, le mutex réentrant `NkRecursiveMutex`, le verrou
> lecteur/rédacteur `NkReaderWriterLock`, le spin-lock `NkSpinLock`, et leurs gardes RAII.

Dès qu'**au moins deux threads touchent la même donnée et qu'au moins un l'écrit**, on entre dans
la zone dangereuse de la *data race* : deux écritures qui se chevauchent, une lecture qui voit un
état à moitié modifié, et le programme se met à mentir de façon non reproductible. Un *verrou*
résout le problème en imposant qu'**un seul thread à la fois** traverse la section critique. La
question n'est jamais « lequel marche » (ils marchent tous), mais « lequel colle à la **durée** de
ma section critique et à la **forme** de mes accès ». Tout le compromis tient en quelques phrases :
**un mutex endort le thread qui attend (idéal pour les sections longues, coûteux à réveiller) ; un
spin-lock le fait tourner à vide (idéal pour quelques cycles, gaspilleur au-delà) ; un verrou
lecteur/rédacteur laisse passer les lecteurs en parallèle mais sérialise les écrivains.** Cette page
vous apprend à choisir, puis à ne jamais oublier de déverrouiller grâce aux gardes RAII.

Tous les verrous de base sont **non-copiables et non-déplaçables** : un verrou est une ressource
ancrée à un endroit, pas une valeur qu'on transporte. La règle d'or, elle, est universelle : on
n'appelle (presque) jamais `Lock()` / `Unlock()` à la main — on pose un **garde RAII** sur la pile,
qui verrouille à la construction et déverrouille à la destruction, *quoi qu'il arrive* (retour
anticipé, exception, branche oubliée).

- **Namespaces** : `nkentseu::threading` (mutex, RW lock, gardes) · `nkentseu::memory` (spin-locks)
- **Headers** : `#include "NKThreading/NkMutex.h"` · `"NKThreading/NkSpinLock.h"` ·
  `"NKThreading/NkScopedLock.h"` · `"NKThreading/NkSharedMutex.h"` ·
  `"NKThreading/Synchronization/NkReaderWriterLock.h"`

---

## Le mutex standard : `NkMutex`

C'est le verrou **par défaut**, celui qu'on prend quand on ne sait pas encore. Il garantit
l'**exclusion mutuelle** : tant qu'un thread le détient, tout autre thread qui appelle `Lock()` est
**endormi** par le système jusqu'à libération. C'est exactement ce qu'on veut pour une section
critique qui dure plus que quelques cycles — le thread en attente ne consomme pas de CPU. À
l'intérieur, c'est un `SRWLOCK` sur Windows et un `pthread_mutex_t` sur POSIX ; aucune allocation
dynamique après construction.

`Lock()` bloque, `Unlock()` libère, `TryLock()` tente sans bloquer (et renvoie immédiatement
`true`/`false`), `TryLockFor(ms)` tente pendant une durée bornée. Toutes ces méthodes sont
`noexcept`, et tous les `TryLock*` sont `[[nodiscard]]` : leur résultat **doit** être lu, sinon vous
croyez détenir un verrou que vous n'avez pas.

```cpp
NkMutex mtx;
{
    NkLockGuard guard(mtx);        // Lock() à la construction
    sharedScore += points;          // section critique
}                                   // Unlock() automatique en sortant du bloc
```

Ce n'est **pas** un verrou réentrant : si le **même** thread appelle `Lock()` deux fois de suite
sans déverrouiller entre les deux, il se bloque lui-même — un *deadlock* garanti. Si votre code peut
re-rentrer dans la même section (une fonction récursive qui reprend le verrou), il vous faut
`NkRecursiveMutex` (voir plus bas). À noter aussi : `TryLockFor` est implémenté par une **boucle
active** (re-tentative + cession du thread + horloge monotone), donc sa précision dépend de
l'ordonnanceur ; `TryLockFor(0)` revient à `TryLock()`.

> **En résumé.** `NkMutex` = exclusion mutuelle, le thread en attente est **endormi** (bon pour les
> sections longues). `Lock`/`Unlock`/`TryLock`/`TryLockFor`, tout `noexcept`, `TryLock*`
> `[[nodiscard]]`. **Non-réentrant** : re-Lock sur le même thread = deadlock. Posez un garde RAII
> plutôt que d'appeler `Unlock` à la main.

---

## Le mutex réentrant : `NkRecursiveMutex`

Parfois, le **même** thread doit reprendre un verrou qu'il détient déjà : une méthode publique
verrouille puis appelle une autre méthode publique qui re-verrouille, ou un algorithme récursif
descend dans une structure protégée. Avec `NkMutex`, c'est le deadlock immédiat. `NkRecursiveMutex`
résout ce cas précis grâce à un **compteur de récursivité** : chaque `Lock()` du thread détenteur
incrémente le compteur, chaque `Unlock()` le décrémente, et la libération réelle n'a lieu que
lorsqu'il **retombe à zéro**.

Cette réentrance n'est pas gratuite — l'implémentation (un `CRITICAL_SECTION` sur Windows, un
`pthread_mutex_t` configuré `RECURSIVE` sur POSIX) coûte environ deux à trois fois plus cher qu'un
`NkMutex` simple. C'est pourquoi ce n'est **pas** le choix par défaut : on ne le prend que quand la
réentrance est réelle. Il expose `Lock()`, `TryLock()` (`true` s'il acquiert, `false` si un **autre**
thread le détient) et `Unlock()` — mais **ni** `TryLockFor`, **ni** d'accès au handle natif,
contrairement à `NkMutex`.

```cpp
NkRecursiveMutex rmtx;
void Visit(Node* n) {
    NkScopedLock<NkRecursiveMutex> g(rmtx);   // ok même si déjà détenu par ce thread
    process(n);
    for (Node* c : n->children) Visit(c);      // re-Lock réentrant : compteur++
}
```

> **En résumé.** `NkRecursiveMutex` = même thread peut re-verrouiller (compteur de récursivité,
> libéré à zéro). Coût ~2-3× un `NkMutex`, donc **pas** par défaut. Pas de `TryLockFor` ni de handle
> natif. Alias legacy `nkentseu::NkRecursiveMutex` (déprécié, zéro overhead).

---

## Le verrou lecteur/rédacteur : `NkReaderWriterLock`

Beaucoup de données sont **lues souvent et écrites rarement** : une table de ressources chargées,
une grille spatiale, une config. Un `NkMutex` y sérialise *tout*, y compris des lectures qui ne se
gênent pas entre elles. Le `NkReaderWriterLock` distingue deux régimes : **plusieurs lecteurs en
parallèle** (`LockRead`), mais **un seul écrivain exclusif** (`LockWrite`) qui bloque tout le monde
le temps de modifier. On gagne énormément quand les lectures dominent.

Il est **writer-preferring** : pour éviter qu'un flux continu de lecteurs n'**affame** un écrivain
qui attend, tout nouveau `LockRead()` se met en file dès qu'un écrivain est en attente. C'est la
bonne politique — mais elle a un corollaire à connaître : un thread qui détient déjà une lecture et
re-appelle `LockRead()` alors qu'un écrivain attend peut **se bloquer lui-même** (la lecture n'est
pas réentrante). À l'intérieur, le RW lock s'appuie sur un `NkMutex` et deux variables de condition.

```cpp
NkReaderWriterLock rw;
// lecteurs concurrents
{ NkReadLock r(rw);  auto* tex = registry.Find(id); use(tex); }
// écrivain exclusif
{ NkWriteLock w(rw); registry.Insert(id, LoadTexture(path)); }
```

Ce qui n'existe **pas** ici (méfiez-vous des exemples « std-like » qui traînent dans les commentaires
de header) : pas de `TryLockFor`, pas de `HasReaders`/`GetReaderCount`/`IsLocked`, pas de tag
d'adoption, pas de constructeur par défaut de garde. L'API réelle se limite à `LockRead`/
`TryLockRead`/`UnlockRead` et `LockWrite`/`TryLockWrite`/`UnlockWrite`.

> **En résumé.** `NkReaderWriterLock` = lectures **parallèles**, écritures **exclusives** ; idéal
> quand on lit beaucoup et écrit peu. **Writer-preferring** (pas de famine des écrivains, mais la
> lecture n'est pas réentrante). Gardes dédiés `NkReadLock` / `NkWriteLock`. Alias `NkSharedMutex` /
> `NkSharedLock` / `NkUniqueLock`.

---

## Le spin-lock : `NkSpinLock`

Quand la section critique est **minuscule** — incrémenter un compteur, pousser un pointeur dans une
file, échanger deux champs, bref **moins d'une centaine de cycles** — endormir puis réveiller un
thread coûte bien plus cher que la section elle-même. Le `NkSpinLock` choisit l'inverse : le thread
en attente **tourne à vide** (il *spin*) jusqu'à ce que le verrou se libère, sans jamais passer par
le système. Pour des sections ultra-courtes et peu contestées, c'est le plus rapide qui soit (le
chemin rapide tient en quelques cycles).

C'est un atomique booléen aligné sur une **ligne de cache** (`alignas(NKENTSEU_CACHE_LINE_SIZE)`)
pour éviter le *false sharing*. `Lock()` tente un `Exchange` atomique ; en cas de succès il rend la
main aussitôt, sinon il bascule sur un chemin lent à *backoff* adaptatif (instruction PAUSE, puis
cession du thread). `TryLock()` fait un seul essai non-bloquant, `Unlock()` relâche en une écriture.
`IsLocked()` existe — mais **uniquement pour le debug/les métriques** : il lit en ordre `RELAXED`,
donc sa valeur peut être obsolète à l'instant où vous la lisez, et ne doit **jamais** servir de
logique de synchronisation.

```cpp
using nkentseu::memory::NkSpinLock;
using nkentseu::memory::NkScopedSpinLock;

NkSpinLock spin;
{
    NkScopedSpinLock g(spin);   // ~3-5 cycles si non contesté
    ++allocCounter;              // section ULTRA courte
}
```

Ce n'est **pas** un mutex à tout faire : si la section dure (un appel système, une allocation, une
boucle), le spin gaspille du CPU et fait chuter les performances de toute la machine — prenez
`NkMutex`. Autre prudence : ne détruisez jamais un `NkSpinLock` pendant qu'un thread l'attend encore.

> **En résumé.** `NkSpinLock` = attente **active**, réservé aux sections **< 100 cycles**, atomique
> aligné cache (anti false-sharing). Chemin rapide quelques cycles, sinon backoff adaptatif.
> `IsLocked()` = debug/métriques seulement (RELAXED, potentiellement obsolète). Section longue →
> `NkMutex`.

---

## Les gardes RAII : ne jamais oublier `Unlock`

Appeler `Unlock()` à la main est une bombe à retardement : il suffit d'un `return` anticipé, d'une
exception ou d'une branche oubliée pour laisser le verrou fermé à jamais. La solution est un objet
**sur la pile** qui verrouille à sa construction et déverrouille à sa destruction — automatiquement,
en sortant du bloc, quelle que soit la façon dont on en sort. NKThreading en fournit plusieurs,
chacun adapté à un cas, et il faut éviter de les confondre.

`NkScopedLock<TMutex>` est le garde **générique** : il fonctionne avec n'importe quel type exposant
`Lock()`/`Unlock()` en `noexcept` — donc avec `NkMutex`, `NkRecursiveMutex` *et* `NkSpinLock`. Son
alias `NkLockGuard` est le cas le plus courant (`NkScopedLock<NkMutex>`). Il est **ni copiable ni
déplaçable** : sa portée est figée au scope où on le déclare, point. `NkScopedLockMutex`, lui, est
**spécifique à `NkMutex`** et **déplaçable** (move-only) : on peut transférer la propriété du verrou,
le déverrouiller à la main (`Unlock()`, qui invalide aussi le garde) ou récupérer son pointeur
(`GetMutex()`). Pour le spin-lock, `NkScopedSpinLock` joue exactement le même rôle (move-only,
`Unlock()` manuel, `GetLock()`). Enfin, `NkReadLock` et `NkWriteLock` sont les gardes dédiés du
RW lock — non déplaçables, portée figée au scope.

```cpp
NkLockGuard g(mtx);                          // générique, scope figé (cas courant)
NkScopedLock<NkSpinLock> sg(spin);           // le même template marche pour un spin-lock
NkScopedLockMutex mg(mtx);                    // spécifique NkMutex, déplaçable + Unlock() manuel
```

Le **piège** à graver : `NkScopedLockMutex` (spécifique, déplaçable, dtor gardé par un pointeur)
n'est **pas** la même chose que `NkScopedLock<NkMutex>` (générique, ni move ni Unlock manuel, dtor
qui déverrouille inconditionnellement). Même si l'un porte « ScopedLock » dans son nom, ce n'est
pas une instanciation de l'autre.

> **En résumé.** Posez toujours un garde RAII. `NkScopedLock<T>` / `NkLockGuard` = générique, portée
> figée (marche pour mutex, recursive, spin). `NkScopedLockMutex` & `NkScopedSpinLock` = **move-only**
> avec `Unlock()` manuel. `NkReadLock` / `NkWriteLock` = dédiés au RW lock. Ne confondez pas
> `NkScopedLockMutex` et `NkScopedLock<NkMutex>`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. `noexcept` partout sauf mention contraire ; `[[nodiscard]]`
sur tous les `TryLock*` et accesseurs notés. Chacun est détaillé dans la « Référence complète ».

### `nkentseu::threading::NkMutex` — mutex standard (`NkMutex.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkMutex()`, `~NkMutex()` | Init / libération du mutex natif. **Non-copiable, non-déplaçable** (4 special members `= delete`). |
| Verrouillage | `Lock()`, `Unlock()` | Bloquant / libération par le thread détenteur. |
| Tentative | `TryLock()` `[O(1), [[nodiscard]]]` | Essai non-bloquant. |
| Tentative | `TryLockFor(milliseconds)` `[[[nodiscard]]]` | Essai borné dans le temps (boucle active ; `0` ≈ `TryLock`). |
| Natif | `GetNativeHandle()`, `Get()` | Handle natif (`SRWLOCK&` Win / `pthread_mutex_t&` POSIX) — usage avancé. |

### `nkentseu::threading::NkRecursiveMutex` — mutex réentrant (`NkMutex.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRecursiveMutex()`, `~NkRecursiveMutex()` | Init / libération. **Non-copiable, non-déplaçable**. |
| Verrouillage | `Lock()`, `Unlock()` | Incrémente / décrémente le compteur de récursivité (libéré à 0). |
| Tentative | `TryLock()` `[[[nodiscard]]]` | `true` si acquis, `false` si détenu par un **autre** thread. |
| Alias legacy | `nkentseu::NkRecursiveMutex` | Redirection dépréciée (zéro overhead). |

### `nkentseu::threading::NkReaderWriterLock` — verrou lecteur/rédacteur (`Synchronization/NkReaderWriterLock.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkReaderWriterLock()` | Compteurs à zéro, pas d'allocation. **Non-copiable, non-déplaçable**. |
| Lecture | `LockRead()`, `UnlockRead()` | Verrou **partagé** ; bloque si écrivain actif ou en attente. |
| Lecture | `TryLockRead()` `[[[nodiscard]]]` | Essai non-bloquant de lecture. |
| Écriture | `LockWrite()`, `UnlockWrite()` | Verrou **exclusif** ; attend la fin de tous les lecteurs. |
| Écriture | `TryLockWrite()` `[[[nodiscard]]]` | Essai non-bloquant d'écriture. |

### `nkentseu::memory::NkSpinLock` — spin-lock (`NkSpinLock.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSpinLock()`, `~NkSpinLock()` | Init `false` (inline) / `= default`. **Non-copiable, non-déplaçable**. |
| Verrouillage | `Lock()`, `Unlock()` | Attente active (fast-path `Exchange` + slow-path backoff) / relâche (~1 cycle). |
| Tentative | `TryLock()` `[O(1), [[nodiscard]]]` | Un seul essai non-bloquant. |
| État | `IsLocked()` `[[[nodiscard]]]` | **Debug/métriques uniquement** (RELAXED, peut être obsolète). |

### Gardes RAII

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Générique | `NkScopedLock<TMutex>` (`NkScopedLock.h`) | Garde template (tout type `Lock()/Unlock()` noexcept). **Ni copiable ni déplaçable**. |
| Alias | `NkLockGuard` = `NkScopedLock<NkMutex>` | Le cas le plus courant. |
| Spécifique mutex | `NkScopedLockMutex` (`NkMutex.h`) | Garde **move-only** de `NkMutex` ; `Unlock()`, `GetMutex()` `[[[nodiscard]]]`. |
| Spécifique spin | `NkScopedSpinLock` (`NkSpinLock.h`) | Garde **move-only** de `NkSpinLock` ; `Unlock()`, `GetLock()` `[[[nodiscard]]]`. |
| Lecture RW | `NkReadLock` (`NkReaderWriterLock.h`) | Garde lecture (non déplaçable). |
| Écriture RW | `NkWriteLock` (`NkReaderWriterLock.h`) | Garde écriture (non déplaçable). |

### Alias « shared_mutex-like » (`NkSharedMutex.h`) et legacy

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `nkentseu::threading` | `NkSharedMutex` = `NkReaderWriterLock` | Redirection (même API que le RW lock). |
| `nkentseu::threading` | `NkSharedLock` = `NkReadLock`, `NkUniqueLock` = `NkWriteLock` | Gardes lecture / écriture. |
| `nkentseu` (legacy) | `NkSharedMutex`, `NkSharedLock`, `NkUniqueLock`, `NkLockGuard`, `NkScopedLock<T>`, `NkRecursiveMutex` | Redirections **dépréciées** (rétrocompat). |

---

## Référence complète

### Choisir : le tableau de décision

Le vrai critère est la **durée** de votre section critique et la **forme** de vos accès :

- **Section qui dure plus que quelques cycles, accès exclusif** → `NkMutex`. C'est 90 % des cas.
- **Le même thread doit re-verrouiller (récursion, méthodes qui s'appellent)** → `NkRecursiveMutex`.
- **On lit beaucoup, on écrit rarement** → `NkReaderWriterLock` (`NkSharedMutex`).
- **Section minuscule (< 100 cycles), peu contestée** → `NkSpinLock`.

| Critère | `NkMutex` | `NkRecursiveMutex` | `NkReaderWriterLock` | `NkSpinLock` |
|---------|-----------|--------------------|----------------------|--------------|
| Thread en attente | endormi (OS) | endormi (OS) | endormi (OS) | **tourne à vide** |
| Réentrant (même thread) | non (deadlock) | **oui** (compteur) | non (lecture non réentrante) | non |
| Lectures parallèles | non | non | **oui** | non |
| Section idéale | longue | longue | longue (lecture ≫ écriture) | **ultra-courte** |
| Coût relatif | référence | ~2-3× | plus lourd (2 condvars) | quasi nul si non contesté |
| `TryLockFor` | **oui** | non | non | non |

### `NkMutex` à fond

**Le verrou de référence.** `Lock()` endort le thread tant que le verrou est pris ; c'est ce qui le
rend bon marché pour les sections **longues** — le CPU n'est pas gaspillé pendant l'attente. À
l'intérieur, un `SRWLOCK` (Windows, dont l'initialisation ne peut pas échouer) ou un
`pthread_mutex_t` (POSIX, où un échec d'init est mémorisé). Aucune allocation dynamique après
construction. C'est aussi le seul verrou de cette page à proposer `TryLockFor(ms)`, pratique pour
abandonner une tentative au bout d'un délai plutôt que d'attendre indéfiniment ; son implémentation
est une **boucle active** (re-tentative + cession + horloge monotone), donc sa précision suit
l'ordonnanceur, et `TryLockFor(0)` équivaut à `TryLock()`. `GetNativeHandle()` / `Get()` exposent le
handle natif pour les usages avancés (le passer à une variable de condition, par exemple — `NkMutex`
déclare `NkConditionVariable` comme *friend*).

Cas d'usage, par domaine :
- **Rendu** — protéger une file de commandes ou un pool de ressources GPU partagé entre le thread de
  jeu et le thread de rendu ; section courte mais pas assez pour un spin si elle peut bloquer.
- **ECS / scène** — sérialiser l'ajout/retrait d'entités quand plusieurs systèmes mutent le monde
  hors de la passe parallèle.
- **Physique** — garder une structure de contacts ou un *broadphase* mis à jour par un seul thread à
  la fois.
- **Audio** — protéger l'échange d'état entre le thread audio temps réel et le thread de contrôle
  (mais attention au temps réel : préférer souvent un échange sans verrou).
- **IO / réseau** — sérialiser l'accès à un socket, à un buffer de réception, à un cache de fichiers.
- **Outils / éditeur** — protéger un document partagé entre l'UI et un thread de tâche de fond.

**Piège** : `NkMutex` est **non-réentrant**. Un `Lock()` sur un thread qui le détient déjà = deadlock
silencieux. Si la récursion est possible, passez à `NkRecursiveMutex`. Et ne verrouillez jamais
depuis un *signal handler* (UB sur POSIX).

### `NkRecursiveMutex` à fond

La réentrance est sa seule raison d'être : un **compteur** monte à chaque `Lock()` du thread
détenteur et descend à chaque `Unlock()`, le verrou n'étant réellement libéré qu'au retour à zéro.
Ça résout proprement le cas « une méthode publique verrouille et appelle une autre méthode publique
qui re-verrouille », et les algorithmes récursifs sur une structure protégée.

- **ECS / scène / outils** — un graphe de scène où parcourir un nœud peut déclencher une opération
  qui re-verrouille le même sous-arbre.
- **Gameplay / IA** — une machine à états ou un arbre de comportement dont une transition rappelle
  une fonction qui reprend le même verrou.
- **Threading** — un wrapper de bibliothèque où les API publiques s'appellent entre elles, chacune
  verrouillant par prudence.

Le revers : un coût d'environ **2 à 3 fois** celui d'un `NkMutex`, et l'absence de `TryLockFor` comme
d'accès au handle natif. À ne sortir que si la réentrance est réelle — sinon `NkMutex`. L'alias
`nkentseu::NkRecursiveMutex` (legacy) pointe vers le même type, sans surcoût, mais est déprécié.

### `NkReaderWriterLock` à fond

Le gain est l'**asymétrie** : `LockRead()` autorise **plusieurs lecteurs simultanés** (aucun ne
modifie, ils ne se gênent pas), tandis que `LockWrite()` impose l'**exclusivité totale** le temps
d'écrire. Quand les lectures dominent largement les écritures, le débit explose par rapport à un
`NkMutex` qui sérialiserait tout. La politique est **writer-preferring** : dès qu'un écrivain attend,
les nouveaux lecteurs se mettent en file — un écrivain ne peut donc pas être affamé par un flot
continu de lectures. À la libération, le dernier lecteur réveille un écrivain en attente ; un
écrivain qui termine réveille les lecteurs en attente puis un écrivain.

- **Rendu** — un registre de textures/matériaux/meshes lu par chaque *draw* mais modifié seulement au
  chargement : des dizaines de lectures parallèles, de rares écritures exclusives.
- **ECS / scène** — une grille spatiale ou un *octree* interrogé en masse par les systèmes de requête
  et reconstruit ponctuellement.
- **Gameplay / IA** — une *blackboard* partagée, une table de navigation, lues par beaucoup d'agents,
  écrites rarement.
- **IO / réseau** — un cache de fichiers ou de connexions à forte dominante de lecture.
- **Outils / éditeur** — un asset partagé consulté par plusieurs panneaux, modifié à l'enregistrement.

**Pièges** : la lecture **n'est pas réentrante** — combinée au caractère writer-preferring, un thread
qui détient une lecture et re-`LockRead()` pendant qu'un écrivain attend peut s'auto-bloquer.
Méfiez-vous aussi des API « std-like » qu'on voit parfois dans les commentaires de header
(`TryLockFor`, `HasReaders`, `GetReaderCount`, tag d'adoption…) : elles **n'existent pas** réellement.
L'API se limite strictement aux six méthodes `LockRead`/`TryLockRead`/`UnlockRead` et `LockWrite`/
`TryLockWrite`/`UnlockWrite`. En interne, le RW lock s'appuie sur un `NkMutex` et deux
`NkConditionVariable`.

### `NkSpinLock` à fond

Le pari du spin-lock : pour une section **vraiment minuscule**, faire tourner le thread en attente
quelques cycles coûte moins cher que de l'endormir puis de le réveiller (un aller-retour vers le
noyau, des centaines à des milliers de cycles). Le verrou est un `NkAtomicBool` **aligné sur une
ligne de cache** (`alignas(NKENTSEU_CACHE_LINE_SIZE)`) pour qu'il ne partage pas sa ligne avec une
autre donnée chaude (false sharing). `Lock()` tente un `Exchange(true, ACQUIRE)` : si le verrou était
libre, on a fini en ~3-5 cycles ; sinon on bascule sur un chemin lent à **backoff adaptatif** —
quelques dizaines d'itérations de PAUSE, puis cession du thread (`SwitchToThread`/`sched_yield`).
`Unlock()` est un simple `Store(false, RELEASE)` (~1 cycle). `TryLock()` fait un seul `Exchange`
non-bloquant.

- **Threading** — protéger l'enqueue/dequeue d'une file de tâches d'un *thread pool*, incrémenter un
  compteur de travaux restants : quelques instructions, contestation rare.
- **Mémoire** — sérialiser un push/pop sur une *free-list* d'allocateur (d'où sa place dans
  `nkentseu::memory`).
- **ECS** — réserver atomiquement un slot dans un buffer de composants pendant une passe parallèle.
- **Rendu** — pousser une commande dans une file partagée, réserver un offset dans un buffer
  transient.
- **Audio** — sur le thread temps réel, un verrou très court qui ne doit **jamais** endormir le thread
  (un `NkMutex` y serait risqué) — à condition que la section reste vraiment minuscule.

`IsLocked()` lit l'état en ordre `RELAXED` : c'est un instantané **possiblement obsolète**, bon pour
un log ou une métrique, **jamais** pour décider d'une action de synchronisation. **Pièges** : ne
jamais l'employer pour une section longue (gaspillage CPU à l'échelle de la machine entière) ; ne
jamais détruire un `NkSpinLock` pendant qu'un thread l'attend.

### Les gardes RAII à fond

Un garde RAII transforme « verrouiller/déverrouiller » en « entrer/sortir d'un bloc ». Il y en a
quatre familles, qu'il ne faut pas mélanger :

- `NkScopedLock<TMutex>` — le **template générique**, par *duck typing* : tout type avec `Lock()` et
  `Unlock()` en `noexcept` convient (`NkMutex`, `NkRecursiveMutex`, `NkSpinLock`). Il tient une
  **référence** au verrou, déverrouille **inconditionnellement** à la destruction, et est **ni
  copiable ni déplaçable** — sa portée est rivée au scope. Son alias `NkLockGuard` = `NkScopedLock<NkMutex>`
  est l'écriture la plus courante.
- `NkScopedLockMutex` — garde **spécifique à `NkMutex`**, **move-only** : il tient un *pointeur*
  (nullable), ce qui permet de **transférer** la propriété du verrou par déplacement. On peut le
  déverrouiller à la main avec `Unlock()` (qui met le pointeur à `nullptr` et neutralise le garde),
  ou récupérer le mutex géré via `GetMutex()`. Son destructeur ne déverrouille que si le pointeur est
  non nul.
- `NkScopedSpinLock` — l'équivalent move-only pour `NkSpinLock` : `Unlock()` manuel, `GetLock()`,
  même sémantique de transfert.
- `NkReadLock` / `NkWriteLock` — gardes **dédiés** au RW lock, tenant une référence, non déplaçables,
  portée figée. `NkReadLock` prend/relâche en lecture, `NkWriteLock` en écriture.

Cas d'usage transverses (tous domaines) : on pose un garde dès qu'on entre dans une section
protégée, et la **portée** du garde devient la durée du verrou — d'où l'importance de bloc
`{ … }` serrés autour de la section critique pour ne pas tenir le verrou plus longtemps que
nécessaire. Le **move** (`NkScopedLockMutex`, `NkScopedSpinLock`) sert quand un *factory* doit
verrouiller puis **renvoyer** le verrou tenu à l'appelant. Les gardes non déplaçables
(`NkScopedLock<T>`, `NkReadLock`, `NkWriteLock`) couvrent l'immense majorité des cas.

**Piège majeur** : ne confondez pas `NkScopedLockMutex` (spécifique `NkMutex`, déplaçable, pointeur
nullable, dtor gardé, `Unlock()`/`GetMutex()`) et `NkScopedLock<NkMutex>` (générique, référence, ni
move ni `Unlock` manuel, dtor inconditionnel). Le premier n'est **pas** une instanciation du second,
malgré le nom.

### Les alias et le socle commun

- **`NkSharedMutex` / `NkSharedLock` / `NkUniqueLock`** (dans `nkentseu::threading`, via
  `NkSharedMutex.h`) sont de **pures redirections** `using` vers `NkReaderWriterLock` / `NkReadLock` /
  `NkWriteLock` — zéro overhead. Conséquence : ils ont **exactement** l'API du RW lock (pas l'API
  std-like avec move/timeout/adopt, qui n'est pas réelle).
- **Alias legacy** dans le namespace parent `nkentseu` (`NkRecursiveMutex`, `NkSharedMutex`,
  `NkSharedLock`, `NkUniqueLock`, `NkLockGuard`, `NkScopedLock<T>`) : des redirections **dépréciées**
  conservées pour la rétrocompatibilité (ex. `NkEventBus`).
- **Tous non-copiables, non-déplaçables.** Les quatre verrous de base (`NkMutex`, `NkRecursiveMutex`,
  `NkReaderWriterLock`, `NkSpinLock`) interdisent copie *et* déplacement : un verrou est ancré.
- **Tout `noexcept`.** Aucune méthode de verrouillage ne lève ; les `TryLock*` et les accesseurs
  (`IsLocked`, `GetMutex`, `GetLock`) sont `[[nodiscard]]` — leur retour **doit** être lu.
- **Macro d'export** `NKENTSEU_THREADING_CLASS_EXPORT` sur toutes les classes (sauf le template
  `NkScopedLock`). Types primitifs : `nk_bool`, `nk_uint32`, `nk_uint64`.

---

### Exemple récapitulatif

```cpp
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKThreading/NkSpinLock.h"
#include "NKThreading/NkSharedMutex.h"     // NkReaderWriterLock + alias

using namespace nkentseu::threading;
using nkentseu::memory::NkSpinLock;
using nkentseu::memory::NkScopedSpinLock;

// 1) Mutex standard + garde générique : section longue, accès exclusif.
NkMutex commandsMtx;
{
    NkLockGuard g(commandsMtx);              // Lock() à la construction
    renderQueue.PushBack(cmd);               // section critique
}                                            // Unlock() automatique

// 2) Verrou lecteur/rédacteur : beaucoup de lectures, rares écritures.
NkSharedMutex registryRW;                    // alias de NkReaderWriterLock
{ NkSharedLock r(registryRW); use(registry.Find(id)); }      // lecteurs parallèles
{ NkUniqueLock w(registryRW); registry.Insert(id, tex); }    // écrivain exclusif

// 3) Spin-lock : section ULTRA courte sur un thread pool.
NkSpinLock taskSpin;
{
    NkScopedSpinLock g(taskSpin);            // ~3-5 cycles si non contesté
    ++tasksRemaining;
}

// 4) Tentative bornée dans le temps (seul NkMutex la propose).
if (commandsMtx.TryLockFor(5)) {             // [[nodiscard]] : on LIT le résultat
    NkScopedLockMutex held(commandsMtx);     // déjà verrouillé ; on adopte la portée
    held.Unlock();                           // libération manuelle si besoin
}
```

---

[← Index NKThreading](README.md) · [Récap NKThreading](../NKThreading.md) · [Couche System](../README.md)
