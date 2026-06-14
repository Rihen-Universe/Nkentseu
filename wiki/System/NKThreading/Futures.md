# Les futurs et les promesses

> Couche **System** · NKThreading · Récupérer **plus tard** le résultat d'un calcul lancé sur un
> autre thread : le handle de lecture `NkFuture<T>`, son pendant pour les opérations sans retour
> `NkFuture<void>`, et le handle d'écriture `NkPromise<T>` qui les satisfait.

Dès qu'on lance un travail sur **un autre thread** — charger une texture, décompresser un son,
résoudre un chemin d'IA, calculer une passe de physique en arrière-plan — on se heurte à une
question de synchronisation : le thread qui a *demandé* le résultat n'est pas celui qui le
*produit*, et les deux ne marchent pas au même rythme. Comment le consommateur récupère-t-il la
valeur **quand elle est prête**, sans tourner en boucle à interroger « c'est fini ? c'est fini ? »
(*busy-wait*, qui gaspille un cœur entier) et sans risquer de lire une valeur encore en train
d'être écrite (*data race*) ? La réponse classique est le couple **future / promesse** : un canal à
un seul créneau, écrit une fois par le producteur, lu une fois (ou plusieurs) par le consommateur,
avec toute la synchronisation cachée à l'intérieur.

Le partage des rôles est strict et c'est ce qui rend le modèle sûr. La **promesse**
(`NkPromise<T>`) est le bout **écriture** : c'est elle qui crée le canal, qui le distribue, et qui
finit par y déposer la valeur. Le **futur** (`NkFuture<T>`) est le bout **lecture** : il ne peut
rien écrire, seulement attendre puis lire. Les deux partagent en coulisses un même *état* protégé
par un mutex et une variable de condition — mais l'utilisateur ne voit que ces deux poignées aux
responsabilités opposées.

- **Namespace** : `nkentseu::threading` (alias *legacy* dans `nkentseu`, voir plus bas)
- **Header canonique** : `#include "NKThreading/NkFuture.h"` (déclare **tout**)
- **Header de compatibilité** : `#include "NKThreading/NkPromise.h"` (ne fait qu'inclure le premier
  et publier deux alias *legacy* — il n'apporte aucun type nouveau)

---

## Le côté écriture : `NkPromise<T>`

C'est par la promesse qu'on commence, parce que c'est elle qui **crée le canal**. Construire un
`NkPromise<T>` alloue immédiatement l'état partagé (un petit objet ref-compté contenant le mutex, la
variable de condition, le créneau pour la valeur et un drapeau « prêt »). On en tire ensuite un
futur avec `GetFuture()`, qu'on confie au consommateur, puis — quand le calcul est terminé — on
dépose le résultat avec `SetValue(...)`. Ce dépôt réveille **tous** les threads en attente sur le
futur.

```cpp
NkPromise<NkTexture> promise;
NkFuture<NkTexture>  future = promise.GetFuture();   // on distribue la lecture

// ... sur un thread de chargement ...
NkTexture tex = LoadTextureFromDisk("hero.png");
promise.SetValue(NkMove(tex));                        // dépôt + réveil des waiters
```

La promesse est **move-only** : on ne peut pas la copier. Ce n'est pas un caprice — la
responsabilité de « satisfaire ce canal exactement une fois » ne se duplique pas. Si on pouvait
copier la promesse, deux copies pourraient prétendre toutes deux remplir le même créneau. On peut en
revanche la **déplacer** (`NkMove(promise)`) pour transférer cette responsabilité dans une closure
ou un thread ; la source devient alors vide et ne peut plus rien satisfaire.

Trois manières de satisfaire le canal : `SetValue(const T&)` (par copie), `SetValue(T&&)` (par
déplacement, pour les types coûteux ou *move-only*), et `SetException(NkExceptionHandle)` (pour
signaler un échec). **La satisfaction est unique** : le premier appel gagne, et tout appel ultérieur
est silencieusement **ignoré** (`if (mReady) return;`). Pas d'erreur levée, juste un no-op — un
garde-fou contre la double-écriture, pas un mécanisme de signalement.

> **En résumé.** `NkPromise<T>` est le bout **écriture**, créé d'office avec son état partagé.
> `GetFuture()` distribue le bout lecture (autant de fois qu'on veut, même état). `SetValue`
> (copie ou move) ou `SetException` satisfont le canal **une seule fois** — les appels suivants sont
> ignorés. **Move-only** : on déplace la responsabilité, on ne la copie jamais.

---

## Le côté lecture : `NkFuture<T>`

Le futur est la poignée qu'on donne au consommateur. Il ne peut **pas** écrire ; il sait seulement
*attendre* puis *lire*. Trois façons d'interroger, selon ce qu'on veut faire pendant que le résultat
mûrit.

`Get()` est l'attente **bloquante totale** : il appelle `Wait()` puis renvoie la valeur par
**référence constante** (aucune copie). C'est le cas simple « je n'ai rien d'autre à faire, je veux
le résultat maintenant ». `Wait()` bloque sans renvoyer de valeur — utile quand on veut juste se
synchroniser. `IsReady()` est le test **non-bloquant** : il répond immédiatement « prêt ou pas »,
pour décider quoi faire sans s'arrêter. Et `WaitFor(ms)` est l'attente **bornée** : il bloque au
plus `ms` millisecondes et renvoie `true` si le résultat est arrivé à temps, `false` sur *timeout* —
exactement ce qu'il faut pour ne pas dépasser un budget de frame.

```cpp
// Tout au long de la frame, sans jamais bloquer la boucle :
if (future.IsReady()) {
    const NkTexture& tex = future.Get();   // immédiat, on sait qu'il est prêt
    material.SetAlbedo(tex);
}

// Ou : attendre, mais au plus 2 ms, pour rester dans le budget de frame
if (future.WaitFor(2)) {
    use(future.Get());
}
```

Ce n'est **pas** du *busy-wait* : sous le capot, `Wait()`/`WaitFor()` s'endorment sur une variable
de condition avec un prédicat anti-réveils-intempestifs, donc le thread en attente ne consomme aucun
CPU tant que la valeur n'arrive pas. Et ce n'est **pas** un canal à usage unique côté lecture : le
futur est **copiable** et partage l'état, si bien que **plusieurs consommateurs** peuvent détenir
chacun leur copie et lire le même résultat une fois prêt.

Un piège majeur à connaître : un `NkFuture` **construit par défaut** (sans passer par `GetFuture()`)
est *vide* — son état interne est `nullptr`. Sur un tel futur, `IsReady()` renvoie `false`,
`Wait()` retourne tout de suite, `WaitFor()` renvoie `false`… mais `Get()` déréférence un état nul
→ **comportement indéfini / crash**. Règle de fer : **n'obtenez jamais un futur autrement que par
`promise.GetFuture()`**.

> **En résumé.** `NkFuture<T>` est le bout **lecture**. `Get()` = attendre puis lire par référence
> ; `Wait()` = se synchroniser ; `IsReady()` = tester sans bloquer ; `WaitFor(ms)` = attendre avec
> *timeout*. L'attente est efficace (variable de condition, jamais de *busy-wait*). Le futur est
> **copiable** (plusieurs lecteurs). **Ne jamais appeler `Get()` sur un futur vide** (UB) —
> toujours l'obtenir via `GetFuture()`.

---

## Les opérations sans valeur : `NkFuture<void>`

Toutes les tâches asynchrones ne **renvoient** pas une valeur ; certaines on veut juste savoir
qu'elles sont **finies** — un fichier flushé sur disque, une passe de simulation terminée, un job de
nettoyage achevé. Pour cela, `NkFuture<void>` est une **spécialisation** dédiée : c'est un pur
signal de complétion.

Elle se manie comme un `NkFuture<T>` *moins la lecture* : `IsReady()`, `Wait()` et `WaitFor(ms)`
sont identiques, mais il n'y a **pas de `Get()`** (rien à renvoyer) et **pas de type membre
`ValueType`**. Côté production, on utilise un `NkPromise<void>` (instancié depuis le template
général), et on le satisfait avec `SetException` ou, pour signaler simplement la fin, l'une des
surcharges `SetValue` du template général.

> **En résumé.** `NkFuture<void>` = un futur **sans valeur**, simple signal « c'est terminé ».
> Mêmes `IsReady`/`Wait`/`WaitFor`, **pas de `Get()`** ni de `ValueType`. Attention : il n'existe
> **pas** de `SetVoid()` — on satisfait son `NkPromise<void>` via l'API du template général.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. Le détail (complexité, comportement, usages) est dans la
« Référence complète » qui suit. Tout est dans `nkentseu::threading` ; tout est *inline*/template,
les classes portent `NKENTSEU_THREADING_CLASS_EXPORT`.

### `NkPromise<T>` — le bout écriture (move-only)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkPromise()` `[noexcept, 1 alloc]` | Crée l'état partagé (drapeau non-prêt, valeur par défaut). |
| Construction | `NkPromise(NkPromise&&)`, `operator=(NkPromise&&)` `[noexcept]` | Déplacement : transfère la responsabilité (source vidée). |
| Construction | copie `= delete` | Copie **interdite** (responsabilité non duplicable). |
| Construction | `~NkPromise()` | Libère la référence sur l'état (futurs déjà distribués intacts). |
| Distribution | `GetFuture()` `[noexcept, O(1)]` | Renvoie un `NkFuture<T>` sur le **même** état (appelable plusieurs fois). |
| Satisfaction | `SetValue(const T&)` `[noexcept, O(1)+copie]` | Dépose la valeur (copie) + réveille tous les waiters. |
| Satisfaction | `SetValue(T&&)` `[noexcept]` | Dépose la valeur (déplacement) — types coûteux / *move-only*. |
| Satisfaction | `SetException(NkExceptionHandle)` `[noexcept]` | Stocke le handle opaque + marque prêt + réveille. |

### `NkFuture<T = void>` — le bout lecture (copiable)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type membre | `ValueType = T` | Type de la valeur contenue. |
| Construction | `NkFuture()` `[noexcept]` | Futur **vide** (état nul — `Get()` interdit). |
| Construction | copie, `operator=` (copie) | Copie *shallow* : partage l'état (ref-counting, plusieurs lecteurs). |
| Construction | move, `operator=` (move) `[noexcept]` | Déplacement (source vidée). |
| Construction | `~NkFuture()` | Libère **une** référence (les autres copies survivent). |
| Lecture | `Get()` `[nodiscard, noexcept, O(1) hors attente]` | `Wait()` puis renvoie la valeur **par `const&`** (UB si vide). |
| Attente | `Wait()` `[noexcept]` | Bloque jusqu'à prêt (immédiat si vide). |
| Attente | `WaitFor(uint32 ms)` `[nodiscard, noexcept]` | Bloque au plus `ms` ; `true`=prêt, `false`=timeout. |
| Test | `IsReady()` `[nodiscard, noexcept, O(1)]` | Prêt ? — **non-bloquant**, thread-safe. |

### `NkFuture<void>` — spécialisation sans valeur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkFuture()` `[noexcept]` | Futur vide (copie/move/destructeur implicites). |
| Test | `IsReady()` `[nodiscard, noexcept]` | Prêt ? — non-bloquant. |
| Attente | `Wait()` `[noexcept]` | Bloque jusqu'à complétion. |
| Attente | `WaitFor(uint32 ms)` `[nodiscard, noexcept]` | Attente bornée (`true`/timeout). |
| — | **pas de `Get()`, pas de `ValueType`** | Pur signal de complétion. |

### Types et alias

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `NkExceptionHandle = void*` | Handle d'exception **opaque, type-erased** (`nullptr` = pas d'exception). |
| Alias *legacy* | `nkentseu::NkPromise<T>`, `nkentseu::NkFuture<T>` | Redirections dépréciées (zéro overhead) vers `nkentseu::threading::*`. |

---

## Référence complète

Chaque élément repris en détail : comportement, complexité, et usages dans les différents domaines
du temps réel. Les éléments triviaux sont brefs ; les opérations qui portent des pièges sont
traitées à fond.

### `NkExceptionHandle` — le handle d'erreur opaque

`using NkExceptionHandle = void*;`. C'est un pointeur **type-erased** servant à transporter une
erreur d'un thread à l'autre **sans** dépendre de `<exception>` ni de `std::exception_ptr` (zéro-STL
oblige). La valeur `nullptr` signifie « pas d'exception ». Point crucial : ces headers ne fournissent
**aucune** fonction pour capturer, encoder ou réinterpréter ce handle — son contenu et sa durée de
vie sont **entièrement à la charge du code utilisateur**. C'est un canal d'erreur *brut*, pas un
système d'exceptions clé en main.

### `NkPromise<T>` — créer et satisfaire le canal

**Construction.** `NkPromise()` alloue l'état partagé via `memory::MakeSharedPtr<State>()` (conforme
NKMemory, jamais `new`) : `O(1)` plus **une allocation**. L'état naît non-prêt (`mReady=false`),
sans exception (`mException=nullptr`), avec une valeur par défaut. La promesse est **move-only** :
copie et affectation-copie sont `= delete`, déplacement et affectation-déplacement sont `noexcept` ;
après un move, la source est vide et ne peut plus satisfaire. Le destructeur ne libère qu'**une**
référence sur l'état — les futurs déjà distribués restent parfaitement valides après la mort de la
promesse (le `State` survit tant qu'un futur le référence).

**`GetFuture()`** renvoie un `NkFuture<T>` qui partage le même `State` (copie du `NkSharedPtr`,
ref-count incrémenté), en `O(1)`. Appelable **plusieurs fois** : tous les futurs ainsi obtenus
pointent le même état, d'où le support naturel de plusieurs consommateurs.

**`SetValue` / `SetException`** verrouillent le mutex, vérifient le drapeau (`if (mReady) return;`
→ satisfaction unique), écrivent la valeur (ou le handle), lèvent `mReady` et appellent `NotifyAll()`
pour réveiller **tous** les waiters. `O(1)` hors coût de copie/déplacement de la valeur. Tous gardent
aussi contre un état nul (`if (!mState) return;`).

Usages, par domaine :
- **IO / réseau** — un job de chargement (texture, mesh, son, niveau) déposé sur un thread pool
  publie son résultat via `SetValue`, ou un `SetException` si le fichier manque ou la requête échoue.
- **Outils / éditeur** — une compilation de shader, un import d'asset, un *bake* de lightmap lancés
  en tâche de fond : la promesse signale la fin (et le statut) au thread UI sans le bloquer.
- **GPU** — encapsuler la fin d'un transfert asynchrone ou la disponibilité d'un *readback* : la
  promesse est satisfaite par le thread qui scrute la fence GPU.
- **Threading** — brique de base d'un *task system* : chaque tâche soumise rend un futur, satisfait
  par la promesse que le pool détient (et déplace dans la closure du worker).

### `NkFuture<T>` — attendre et lire

**Type membre.** `using ValueType = T;` expose le type contenu pour le code générique.

**Construction.** Le constructeur par défaut crée un futur **vide** (`mState == nullptr`). La copie
et l'affectation-copie sont *shallow* : elles partagent l'état (ref-counting) — c'est ce qui autorise
**plusieurs lecteurs** du même résultat. Le move vide la source ; le destructeur libère une seule
référence (les autres copies survivent).

**`Get()`** `[[nodiscard]] noexcept`. Appelle `Wait()` puis renvoie `mState->mValue` **par référence
constante** — aucune copie. `O(1)` hors temps d'attente. **Comportement à connaître** : `Wait()`
retourne immédiatement si l'état est nul, si bien que `Get()` sur un futur **vide** déréférence un
pointeur nul → **UB/crash**. Par ailleurs `Get()` ne consulte **pas** `mException` : un échec signalé
par `SetException` n'est **pas** re-propagé ici (le pattern « throw on Get » n'existe pas dans cette
implémentation).

**`Wait()`** `noexcept`. Attente bloquante sans *timeout*. État nul → retour immédiat. Sinon
verrouille et s'endort sur la variable de condition avec le prédicat `mReady` (anti-réveils
intempestifs). Bloque indéfiniment si jamais satisfait — d'où l'intérêt de `WaitFor` quand on a un
budget de temps.

**`WaitFor(uint32 ms)`** `[[nodiscard]] noexcept`. Attente **bornée**. État nul → `false`. Sinon
calcule une deadline absolue (`NkConditionVariable::GetMonotonicTimeMs() + ms`) et attend jusque-là :
`true` si prêt, `false` sur *timeout*. Le délai est relatif au moment de l'appel ; la précision
dépend du *scheduler* OS. Cas limite `WaitFor(0)` : se comporte en quasi-*poll* (deadline ≈ maintenant)
— renvoie selon que `mReady` est déjà vrai.

**`IsReady()`** `[[nodiscard]] noexcept`. Test **non-bloquant**, thread-safe : état nul → `false`,
sinon verrouille et renvoie `mReady`. `O(1)`. Vrai dès que `SetValue`/`SetException` a été appelé.

Usages, par domaine :
- **Rendu** — un *streaming* de textures/meshes lancé pendant la frame : on teste `IsReady()` chaque
  frame, et dès que c'est prêt on récupère l'asset sans avoir bloqué la boucle une seule fois.
- **ECS / scène** — un chargement de chunk de monde ou d'instanciation d'archétype en arrière-plan ;
  le système qui en a besoin attend avec `WaitFor(budget)` pour ne pas trouer la frame.
- **Physique / animation** — une passe de simulation ou un *bake* de pose lancé en parallèle :
  `Wait()` au point de barrière où le résultat devient indispensable.
- **Gameplay / IA** — une requête de *pathfinding* coûteuse confiée à un worker ; l'agent vérifie
  `IsReady()` tant que le chemin n'est pas là, et continue son comportement en attendant.
- **Audio** — un décodage de flux ou un préchargement de banque de sons ; le mixeur consomme le
  buffer dès `IsReady()`.
- **UI / 2D** — afficher un *spinner* tant que `!IsReady()`, basculer sur le contenu dès qu'il
  arrive — sans figer l'interface.
- **IO / réseau** — la réponse d'une requête : `WaitFor` impose un *timeout* applicatif propre.

### `NkFuture<void>` — le signal de complétion

Spécialisation explicite pour les opérations **sans valeur de retour**. Son état interne ne contient
**pas** de champ valeur ; elle n'expose donc **ni `Get()` ni `ValueType`**. Seul le constructeur par
défaut est explicitement déclaré (copie/move/destructeur sont ceux, implicites, du compilateur).
`IsReady()`, `Wait()` et `WaitFor(ms)` ont exactement le même comportement et la même implémentation
*inline* que la version générale (lock + prédicat `mReady`, deadline monotone pour `WaitFor`).

À noter : il n'existe **pas** de spécialisation `NkPromise<void>` ; un `NkPromise<void>` est instancié
depuis le **template général** `NkPromise<T>`. Conséquence directe : **`SetVoid()` n'existe pas** (il
n'apparaît que dans des commentaires de doc), et un appel `promise.SetValue()` *sans argument* ne
correspond à **aucune** surcharge déclarée. On satisfait donc un `NkPromise<void>` via l'API du
template général (`SetValue`/`SetException`).

Usages, par domaine :
- **Threading / outils** — « ce job est-il fini ? » sans avoir besoin de sa sortie : *flush* disque,
  nettoyage différé, fin d'une passe de *bake*.
- **GPU** — signaler la complétion d'un *upload* ou d'une commande sans rien renvoyer.
- **Gameplay** — synchroniser plusieurs systèmes sur la fin d'une étape (« la frame physique est
  terminée ») via une simple barrière de complétion.

### Le socle commun (interne, mais structurant)

- **État partagé protégé.** Promesse et futurs partagent un `State` ref-compté (`memory::NkSharedPtr`)
  contenant `NkMutex` + `NkConditionVariable` + valeur + handle d'exception + drapeau `mReady`. Tout
  accès est sérialisé par `NkScopedLock` (RAII). Voir [Mutex & verrous](Mutexes.md) et
  [Variables de condition](ConditionVariables.md).
- **Attente sans busy-wait.** `Wait`/`WaitFor` s'endorment sur la variable de condition avec un
  prédicat anti-réveils-intempestifs ; `WaitFor` s'appuie sur `GetMonotonicTimeMs()` pour une
  deadline absolue robuste.
- **Mémoire zéro-STL.** Le `State` vient de `MakeSharedPtr` (NKMemory, jamais `new`) et survit tant
  qu'un `NkPromise` **ou** un `NkFuture` le référence — un futur distribué reste valide après la mort
  de la promesse. Voir [NKMemory](../../Foundation/NKMemory.md).

### Alias *legacy* (`NkPromise.h`)

Le header `NKThreading/NkPromise.h` est un **pur fichier de compatibilité** : il n'inclut que
`NkFuture.h` et publie, dans le namespace `nkentseu`, deux alias dépréciés à coût nul —
`template<typename T> using NkPromise = ::nkentseu::threading::NkPromise<T>;` et l'équivalent pour
`NkFuture` (qui redirige aussi la spécialisation `void`). Aucun type, macro, enum ou fonction libre
nouveau n'y est déclaré ; les `#define` de migration qu'on peut y croiser (`NKENTSEU_DISABLE_LEGACY_ALIASES`
et consorts) sont **uniquement dans des blocs de commentaires** — non définis dans le code réel. Pour
du code neuf, préférez `nkentseu::threading`.

### Pièges à retenir

- **Futur vide → UB.** `Get()` sur un `NkFuture` construit par défaut déréférence `nullptr`. Toujours
  obtenir le futur via `promise.GetFuture()`.
- **Exceptions non re-propagées.** `SetException` stocke un handle, mais `Get()` ne le consulte pas et
  **aucun accesseur public** ne l'expose. La gestion d'erreur réelle est à écrire côté utilisateur ;
  le pattern « throw on Get » n'est pas fourni.
- **Satisfaction unique et silencieuse.** Après le premier `SetValue`/`SetException`, les suivants
  sont des no-op (pas d'erreur). Ne comptez pas dessus pour détecter une double-écriture.
- **Promesse move-only.** Pour la transférer dans un thread/closure, déplacez-la (`NkMove`). La copie
  est une erreur de compilation — volontairement.
- **`SetVoid()` n'existe pas.** Pour `NkPromise<void>`, utilisez l'API du template général.

---

### Exemple

```cpp
#include "NKThreading/NkFuture.h"
using namespace nkentseu::threading;

// --- Producteur / consommateur : charger une texture en tâche de fond ---
NkPromise<NkTexture> promise;
NkFuture<NkTexture>  future = promise.GetFuture();   // on distribue la lecture

// Sur un worker (la promesse est DÉPLACÉE dans la closure : move-only) :
threadPool.Submit([p = NkMove(promise)]() mutable {
    NkTexture tex = LoadTextureFromDisk("hero.png");
    p.SetValue(NkMove(tex));                          // dépôt + réveil des waiters
});

// Dans la boucle de frame : on ne bloque jamais.
if (future.IsReady()) {
    const NkTexture& tex = future.Get();             // immédiat, par référence
    material.SetAlbedo(tex);
}

// Variante : attendre, mais au plus 2 ms pour tenir le budget de frame.
if (future.WaitFor(2)) {
    use(future.Get());
}

// --- Signal de complétion sans valeur ---
NkPromise<void> done;
NkFuture<void>  doneFut = done.GetFuture();
threadPool.Submit([p = NkMove(done)]() mutable {
    FlushToDisk();
    p.SetValue();   // (template général ; PAS de SetVoid())
});
doneFut.Wait();     // barrière : on attend la fin, rien à lire
```

---

[← Index NKThreading](README.md) · [Récap NKThreading](../NKThreading.md) · [Couche System](../README.md)
