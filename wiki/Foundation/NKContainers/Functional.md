# Les conteneurs fonctionnels

> Couche **Foundation** · NKContainers · Traiter le **code comme une donnée** : le conteneur
> polymorphe d'appelables `NkFunction`, les foncteurs utilitaires de `NkFunctional`
> (`NkHash`, comparateurs, prédicats), et l'application partielle `NkBind`.

Il arrive un moment où ce qu'on veut ranger dans une structure n'est pas une valeur mais une
**action** : « quand le bouton est cliqué, appelle *ceci* », « pour chaque entité, exécute *ce*
système », « lance *cette* méthode sur *ce* thread ». On a besoin de manipuler des morceaux de
code comme on manipule des entiers — les **stocker** dans une variable, les **passer** en
argument, les **garder** dans un tableau pour les rappeler plus tard. C'est tout l'objet de cette
famille : transformer une lambda, un foncteur, un pointeur de fonction ou une méthode membre en un
objet ordinaire que le reste du moteur peut transporter sans rien connaître de sa nature réelle.

Le cœur du sujet tient en une idée : la **gomme de type** (*type erasure*). Une lambda et un
pointeur de fonction n'ont pas le même type C++ ; pourtant, dès lors qu'ils s'appellent de la
même façon (`int(int)` par exemple), on aimerait les ranger dans la **même** boîte. `NkFunction`
est cette boîte. Autour gravitent les **foncteurs** de `NkFunctional` (des objets dont le seul
rôle est d'être appelés : « comparer », « hacher »), et `NkBind`, qui **pré-remplit** certains
arguments d'un appelable pour en fabriquer un plus simple. C'est l'équivalent maison, zero-STL et
sans exceptions, de `<functional>`.

- **Namespace** : `nkentseu` (et le détail interne dans `nkentseu::detail`)
- **Headers** : `#include "NKContainers/Functional/NkFunction.h"`,
  `NkFunctional.h`, `NkBind.h`

---

## La boîte à appelables : `NkFunction`

`NkFunction` range **n'importe quoi d'appelable** derrière une signature unique. On l'écrit
toujours sous la forme `NkFunction<R(Args...)>` — le type de retour, puis les paramètres entre
parenthèses, exactement comme on déclarerait une fonction. La déclaration primaire `NkFunction<T>`
sans signature n'est **pas** définie : écrire `NkFunction<int>` échoue volontairement à la
compilation, c'est la syntaxe `NkFunction<int(int)>` qui est attendue.

Une fois la signature fixée, on y verse ce qu'on veut tant que ça s'appelle de la bonne façon :
une lambda, un foncteur, un pointeur de fonction libre, ou même un couple **objet + méthode
membre**. À l'intérieur, `NkFunction` ne garde qu'**un seul pointeur** vers une base virtuelle
(`CallableBase`) plus l'allocateur ; le vrai appelable est cloné sur le **tas** via cet allocateur.

```cpp
NkFunction<int(int)> f = [](int x) { return x * 2; };   // depuis une lambda
int y = f(21);                                          // 42

struct Player { void Hit(int dmg); };
Player p;
NkFunction<void(int)> onHit(&p, &Player::Hit);          // depuis objet + méthode
onHit(10);                                              // appelle p.Hit(10)
```

Ce n'est **pas** un `std::function` à *small-buffer optimization* : il n'y a **aucun** stockage
interne sur place. Chaque construction et chaque **copie** allouent sur le tas (le copy-ctor
**clone** l'appelable) ; seul le **déplacement** est gratuit (il vole le pointeur, et la source
devient vide). Les méthodes `UsesSbo()`, `GetSboBufferSize()`, `GetAllocationCount()`,
`GetTotalMemory()` et `ResetGlobalStats()` existent pour la compatibilité d'API mais renvoient des
valeurs fixes (`false`, `0`…) : il n'y a pas de SBO réelle à interroger.

Ce n'est **pas** non plus un `std::function` qui *jette* : appeler une `NkFunction` **vide** ne
lève pas de `bad_function_call`. Elle retourne silencieusement `R{}` (ou rien si `R` est `void`).
La validité se teste donc explicitement, avant l'appel, avec `if (f)`, `f.IsValid()` ou
`f != nullptr`.

```cpp
NkFunction<int(int)> g;          // vide
if (g) g(5);                     // on teste AVANT : ici on n'appelle pas
int z = g(5);                    // sinon : pas de crash, z == 0 (= int{})
```

> **En résumé.** `NkFunction<R(Args...)>` est une boîte à *type erasure* pour tout appelable
> (lambda, foncteur, pointeur de fonction, objet+méthode). **Toujours** une allocation tas (pas de
> SBO) ; copie = clone (alloue), déplacement = vol (gratuit). Appeler une boîte vide est
> **silencieux** (`R{}`), pas une exception — testez avec `if (f)`.

---

## Lier une méthode et pré-remplir : `NkBind`

Une lambda, c'est commode, mais souvent on a déjà la fonction sous la main et on veut juste en
**figer une partie**. C'est l'**application partielle** : prendre `add(a, b, c)` et en fabriquer
`add12(c)` où `a` et `b` valent déjà 1 et 2. `NkBind` fait exactement ça. Il renvoie un petit
foncteur, `NkBoundCallable`, qui garde le callable et les arguments figés dans un `NkTuple`, et
qui **ne fait aucune allocation** par lui-même — on peut ensuite l'assigner à une `NkFunction`
(qui, elle, le clonera).

```cpp
int add(int a, int b, int c) { return a + b + c; }
auto add12 = NkBind(add, 1, 2);   // a=1, b=2 figés
int r = add12(3);                  // 6

struct Math { int Mul(int x) const; };
Math m;
auto times5 = NkBind(&m, &Math::Mul, 5);   // surcharge méthode membre
int q = times5(/* x = */7);                // attention : ici 5 est figé → m.Mul(5)
```

Un point à garder en tête : tous les arguments figés sont **décayés** (copiés *par valeur* dans le
tuple). `NkBind` ne conserve **pas** de référence vers vos variables locales par défaut — ce qui
est figé est une copie au moment de l'appel à `NkBind`.

À côté de l'application partielle, `NkBind` fournit deux familles d'adaptateurs très utiles au
quotidien :

- `NkBind0(obj, &Type::Method)` enveloppe simplement une méthode membre en un appelable **sans
  rien figer** — utile quand on veut juste « une fonction » à partir d'une méthode.
- `NkBindThreadFunc` et `NkBindThreadFuncNoArg` produisent directement une
  `NkFunction<void(void*)>` (le `ThreadFunc` attendu par `NkThread::Start()`), à partir d'une
  méthode `void(void*)`, d'une méthode **sans paramètre**, ou d'une fonction libre `void(void*)`.

```cpp
struct Worker { void Run(void* data); };
Worker w;
NkFunction<void(void*)> entry = NkBindThreadFunc(&w, &Worker::Run);
thread.Start(entry, &context);   // passe directement à NkThread::Start
```

Enfin, `NkPartial(f)` fabrique un `NkPartialCallable` orienté **curryfication**. En l'état, son
`operator()` n'expose que l'**appel direct** `f(args...)` (par SFINAE sur le retour) ; il ne
re-binde pas automatiquement sur une signature incomplète.

> **En résumé.** `NkBind(f, a, b)` fige des arguments (**décayés**, copiés par valeur) et renvoie
> un `NkBoundCallable` **sans allocation**, assignable à une `NkFunction`. `NkBind0` wrappe une
> méthode sans rien figer. Pour un thread-entry, préférez `NkBindThreadFunc` /
> `NkBindThreadFuncNoArg` plutôt que de bricoler le ctor de `NkFunction`.

---

## Les foncteurs prêts à l'emploi : `NkFunctional`

Certains appelables sont si universels qu'on les fournit tout faits : **comparer** deux valeurs,
**hacher** une clé, combiner des booléens. Ce sont les **foncteurs** de `NkFunctional` — des
`struct` minuscules dont l'`operator()` est `constexpr noexcept`, sans état. Ils sont la matière
première des conteneurs associatifs : un `NkHashMap<Key, V>` se sert de `NkHash<Key>` pour ranger
ses clés, et les conteneurs triés s'appuient sur `NkLess<T>`.

`NkHash<T>` est le seul à mériter une mise en garde. Son **gabarit primaire ne compile pas** :
il contient un `static_assert(sizeof(T) == 0, …)` qui se déclenche à l'instanciation. Autrement
dit, `NkHash` **doit être spécialisé** pour chaque type qu'on veut hacher. Le module en fournit
déjà pour les entiers (`int32_t`, `uint32_t`, `int64_t`, `uint64_t`), les flottants (`float32`,
`float64`, avec un traitement soigné de NaN et des ±0/±inf) et `NkString` (FNV-1a). Pour **votre**
type de clé, vous écrivez votre propre spécialisation.

```cpp
template<>
struct nkentseu::NkHash<EntityId> {
    constexpr usize operator()(const EntityId& id) const noexcept {
        return NkHash<uint64_t>{}(id.value);   // on réutilise une spé existante
    }
};
```

Les comparateurs (`NkEqual`, `NkLess`, `NkGreater`, `NkLessEqual`, `NkGreaterEqual`) et les
prédicats logiques (`NkLogicalAnd`, `NkLogicalOr`, `NkLogicalNot`) n'appellent, eux, que
l'opérateur correspondant du type (`lhs < rhs`, `lhs && rhs`, `!value`…). On les passe à un tri,
à une recherche, ou à un conteneur comme comparateur par défaut.

> **En résumé.** `NkFunctional` = foncteurs sans état, `constexpr noexcept`. `NkHash<T>` **doit
> être spécialisé** (le primaire ne compile pas) ; spécialisations fournies pour entiers, flottants
> et `NkString`. Les comparateurs/prédicats délèguent simplement à l'opérateur du type.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Le détail (formule, complexité, cas d'usage) suit dans
la « Référence complète ». Complexités entre crochets.

### `NkFunction<R(Args...)>` — boîte à appelables (type erasure)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `using NkNullptrT`, `using ResultType = R` | Type de `nullptr` (sans `<cstddef>`) ; type de retour. |
| Construction | `NkFunction(allocator)`, `NkFunction(nullptr, …)` | Boîte **vide** (avec ou sans syntaxe STL-like). |
| Construction | `NkFunction(F&& f, allocator)` | Depuis lambda / foncteur / pointeur de fonction `[O(1)+alloc]`. |
| Construction | `NkFunction(obj, &T::meth)` (non-const **et** const) | Depuis objet + **méthode membre**. |
| Construction | `NkFunction(obj, allocator)` | Binding **multiple** (à remplir via `BindMethod`). |
| Rule of Five | copie (clone, **alloue**), déplacement (vol, gratuit), destructeur | Copie profonde / transfert `O(1)` / `Clear()`. |
| Affectation | `= nullptr`, `= const&`, `= &` (lvalue), `= &&`, `= F&&` | Reset / copie / déplacement / depuis appelable quelconque. |
| Affectation | `= NkPair<T*, méthode>` (const et non-const) | Depuis une paire objet+méthode. |
| Binding multiple | `BindMethod(obj, &T::meth, index)` (const et non-const) | Ajoute/remplace une méthode à `index` `[O(index)]`. |
| Invocation | `operator()(args...)` | Appel standard ; vide → `R{}` `[O(1)+virtuel]`. |
| Invocation | `operator()(index, args...)` | Appel **indexé** (binding multiple), borne vérifiée. |
| Validité | `operator bool`, `IsValid`, `== / != nullptr` (free friends) | Non vide ? |
| Utilitaires | `Swap(other)`, `Clear()` | Échange `O(1)` / vider. |
| Stats (stubs SBO) | `UsesSbo`→`false`, `GetSboBufferSize`→`0`, `GetAllocationCount`→`0`, `GetTotalMemory`→`0`, `ResetGlobalStats` | Compat d'API, valeurs fixes. |
| Libre | `NkSwap(a, b)` | Swap non-membre (ADL) `[O(1)]`. |

### `NkFunctional` — foncteurs utilitaires

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Hachage | `NkHash<T>` (primaire) | **Doit être spécialisé** (`static_assert` sinon). |
| Hachage | `NkHash<int32/uint32/int64/uint64>` | `key ^ (key >> 16/32)`, `constexpr`. |
| Hachage | `NkHash<float32/float64>` | Gère NaN / ±0 / ±inf, sinon bit-cast → hash entier. |
| Hachage | `NkHash<NkString>` | FNV-1a sur `Data()`/`Length()` (**non `constexpr`**). |
| Comparateurs | `NkEqual`, `NkLess`, `NkGreater`, `NkLessEqual`, `NkGreaterEqual` | `==`, `<`, `>`, `<=`, `>=` ; `constexpr noexcept`. |
| Prédicats | `NkLogicalAnd`, `NkLogicalOr`, `NkLogicalNot` | `&&`, `\|\|`, `!`. |

### `NkBind` — application partielle, currying, threads

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Index sequence | `NkIndexSequence<Indices...>`, `NkMakeIndexSequence<N>` | Remplacement de `std::index_sequence` (récursion profondeur `N`). |
| Foncteur | `NkBoundCallable<F, BoundTuple>` | Résultat de `NkBind` ; **sans allocation** ; `operator()` const/non-const. |
| Bind | `NkBind(f, boundArgs...)` | Application partielle générique (args **décayés**). |
| Bind | `NkBind(obj, &T::meth, boundArgs...)` (const et non-const) | Application partielle sur **méthode membre**. |
| Wrap | `NkBind0(obj, &T::meth)` (const et non-const) | Méthode → appelable, **sans rien figer**. |
| Thread | `NkBindThreadFunc(obj, &T::meth)`, `NkBindThreadFunc(func)` | → `NkFunction<void(void*)>` depuis méthode `void(void*)` / fonction libre. |
| Thread | `NkBindThreadFuncNoArg(obj, &T::meth)` (const et non-const) | → `NkFunction<void(void*)>` depuis méthode **sans paramètre**. |
| Curry | `NkPartialCallable<F>`, `NkPartial(f)` | Curryfication (appel direct par SFINAE). |

---

## Référence complète

Chaque élément est repris ici en détail : complexité, mécanisme, et usages dans les différents
domaines du moteur. Les éléments triviaux sont décrits brièvement ; les pièces maîtresses
(`NkFunction`, `NkBind`, `NkHash`) le sont **à fond**.

### `NkFunction` à fond

**Le mécanisme de type erasure.** `NkFunction` ne garde qu'un `CallableBase* m_callable` (une base
virtuelle) plus un `memory::NkAllocator* m_allocator`. L'appelable concret est rangé dans l'une de
quatre implémentations internes : `CallableImpl<T>` (lambda, foncteur, pointeur de fonction),
`MethodCallableImpl<T>` (méthode non-const), `MethodCallableConstImpl<T>` (méthode const) et
`MultiMethodCallableImpl<T>` (binding multiple indexé, tableau dynamique de `MethodEntry`). Toute
cette indirection passe par une **allocation tas** : il n'y a **pas** de small-buffer optimization,
contrairement à beaucoup de `std::function`.

**Le coût des opérations.** La construction depuis un appelable alloue (`CallableImpl`). La
**copie** appelle `Clone` → **réallocation** ; le **déplacement** vole le pointeur et laisse la
source vide, en `O(1)` sans allocation — d'où l'intérêt systématique de `NkMove` quand on n'a plus
besoin de l'original. L'appel `operator()` est `O(1)` plus une indirection virtuelle. L'allocateur
fourni est résolu et **validé** par `detail::NkResolveFunctionAllocator` : s'il est nul ou mal
aligné (`alignof(void*)`), on retombe sur `memory::NkGetDefaultAllocator()`.

**Les surcharges d'affectation, et pourquoi.** Le `template operator=(F&&)` accepte n'importe quel
appelable — mais il est **exclu** pour `NkFunction` lui-même (via `NkEnableIf_t<!NkIsSame_v<…>>`)
afin de laisser la place aux copy/move. La surcharge supplémentaire `operator=(NkFunction&)` (lvalue
non-const) existe précisément pour que `fn1 = fn2` ne soit pas happé par le template `F&&` mais
parte bien vers la copie. On peut aussi affecter une `NkPair<T*, méthode>` directement.

**Le binding multiple.** Avec le ctor `NkFunction(obj)` puis `BindMethod(obj, &T::meth, index)`, on
range **plusieurs** méthodes indexées dans une même boîte, qu'on rappelle ensuite par
`f(index, args...)`. La résolution est à **borne vérifiée** : un index hors limites ou une méthode
nulle retourne `R{}`/rien, sans crash. Le coût d'un `BindMethod` est lié à `ResizeMethods` (réalloc
+ recopie `O(index)`).

Cas d'usage, par domaine :
- **Gameplay / IA** — table de **callbacks** d'événements (mort d'une entité, fin d'un timer,
  collision déclencheur), états d'une machine à états stockés comme `NkFunction` indexées.
- **UI / 2D** — le `onClick` d'un bouton, le `onValueChanged` d'un slider : on range la réaction
  dans le widget sans connaître son type réel.
- **Audio** — callback de fin de lecture d'un son, hook de génération de buffer.
- **ECS** — un système = `NkFunction<void(float dt)>` ; un planificateur les garde dans un tableau
  et les appelle dans l'ordre.
- **IO / threading** — un `NkFunction<void(void*)>` est le point d'entrée d'un thread (voir
  `NkBindThreadFunc` ci-dessous).
- **Rendu** — étapes d'un *render graph* enregistrées comme appelables paramétrés, rejouées chaque
  frame.

Attention à deux choses : le coût d'allocation à **chaque** copie (préférez le déplacement dans les
boucles chaudes), et le fait qu'une boîte **vide** s'appelle silencieusement — toujours `if (f)`
avant un appel dont l'effet de bord compte.

### `NkBind` à fond

**`NkBind` générique.** `NkBind(f, a, b, …)` renvoie un `NkBoundCallable<NkDecay_t<F>,
NkTuple<NkDecay_t<BoundArgs>...>>` : il stocke le callable (par valeur, après *decay*) et les args
figés dans un `NkTuple`, **sans allocation dynamique**. À l'appel, `operator()(freeArgs...)`
recompose `func_(boundArgs..., freeArgs...)` en dépliant `NkMakeIndexSequence<NumBound>`, et déduit
le retour par `decltype` (transparence totale). Comme tout est *decayé*, **aucune référence** vers
vos locales n'est conservée : on capture des copies.

**Les surcharges méthode membre.** `NkBind(obj, &T::method, …)` existe en version **non-const** et
**const** : elles capturent `obj` + `method` dans une lambda interne, enveloppée dans un
`NkBoundCallable`. C'est ce qui permet `NkBind(&m, &Math::Mul, 5)`.

**`NkBind0` — le wrap pur.** Quand on ne veut figer **aucun** argument mais juste transformer une
méthode en appelable, `NkBind0(obj, &T::meth)` (const ou non-const) produit une lambda qui capture
`obj` + `meth` et accepte tous les arguments de la méthode.

**Les adaptateurs de thread.** `NkThread::Start()` n'attend qu'un seul argument de type
`NkFunction<void(void*)>` (= `ThreadFunc`). Trois fabriques le produisent directement :
`NkBindThreadFunc(obj, &T::meth)` pour une méthode `void(void*)`, `NkBindThreadFuncNoArg(obj,
&T::meth)` pour une méthode **sans paramètre** (la lambda ignore alors le `void*`), et la surcharge
**fonction libre** `NkBindThreadFunc(void(*)(void*))` qui wrappe explicitement en
`[func](void* d){ func(d); }`. Cette dernière est volontaire : assigner un `void(*)(void*)` au ctor
de `NkFunction` risquerait de matcher le ctor `NkFunction(T* obj)` (binding multiple) au lieu de
l'appelable attendu.

**`NkMakeIndexSequence`.** Remplacement maison de `std::index_sequence`, généré par récursion
template de profondeur `N` (limite compilateur de l'ordre de 256–1024). `NkIndexSequence<…>::size()`
donne le nombre d'indices. C'est l'outil bas niveau qui permet à `NkBoundCallable` de déplier son
tuple d'arguments.

**`NkPartial` — curryfication.** `NkPartial(f)` renvoie un `NkPartialCallable<NkDecay_t<F>>` dont
l'`operator()` (const et non-const) tente l'**appel direct** `func_(args...)` (SFINAE sur le
retour). Tel qu'écrit, il n'expose que cette forme directe — il ne re-binde pas automatiquement sur
une signature insuffisante.

Cas d'usage, par domaine :
- **Threading** — lancer une méthode membre comme corps de thread sans écrire de trampoline à la
  main (`NkBindThreadFunc`).
- **Gameplay / IA** — pré-câbler un callback avec son **contexte** : `NkBind(&Enemy::OnHit, this)`
  donne un `void()` prêt à être rangé dans un dispatcher.
- **UI** — relier un bouton à `controller.DoAction(id)` en figeant `id` au moment du câblage.
- **Animation / audio** — différer un appel paramétré (jouer *ce* clip, déclencher *ce* son) en
  capturant ses paramètres à la construction.
- **ECS** — fabriquer des fonctions de système spécialisées en figeant des paramètres de config.

### `NkFunctional` à fond

**`NkHash<T>` — le contrat.** Le primaire **ne compile pas** (`static_assert(sizeof(T) == 0)`) :
c'est volontaire, il force à fournir une spécialisation pour chaque type haché. Les spécialisations
livrées :
- entiers — `int32_t`/`uint32_t` font `key ^ (key >> 16)`, `int64_t`/`uint64_t` font
  `key ^ (key >> 32)` ; toutes `constexpr`.
- flottants — `float32`/`float64` traitent d'abord les cas spéciaux (NaN → 0, ±0/±inf → 1 ou 2),
  puis *bit-cast* vers l'entier de même taille et délèguent au `NkHash` entier correspondant. Cela
  garantit que `+0.f` et `-0.f` hachent pareil et que NaN ne casse pas la table.
- `NkString` — FNV-1a sur `Data()`/`Length()`, avec des constantes adaptées à `sizeof(usize)`
  (offset/primes 64-bit si 8 octets, sinon 32-bit). **Non `constexpr`** (contrairement aux autres).

`NkHash` est le hacheur par défaut des tables (`NkHashMap`, `NkHashSet`) : voir
[CacheFriendly](CacheFriendly.md). Pour toute clé personnalisée (un `EntityId`, une paire de
coordonnées de tuile, un handle de ressource), on écrit sa spécialisation — typiquement en
combinant les hachages des champs via les spécialisations entières existantes.

**Comparateurs et prédicats.** `NkEqual`, `NkLess`, `NkGreater`, `NkLessEqual`, `NkGreaterEqual`
ne font qu'appliquer l'opérateur correspondant (`lhs < rhs`…) ; `NkLogicalAnd`/`NkLogicalOr`/
`NkLogicalNot` combinent des booléens. Tous sont `constexpr noexcept` et sans état. On les passe
comme **comparateur** à un tri (`NkLess` pour un ordre croissant, `NkGreater` pour un tas-max), à
une recherche, ou comme paramètre de tri d'un conteneur ordonné. En pratique on les croise dans
tous les domaines dès qu'il faut ordonner : trier des entités par profondeur pour le rendu
transparent (**rendu**), ordonner une file de priorité d'IA (**gameplay**), classer des contacts
par pénétration (**physique**), ranger des événements par horodatage (**audio**/timeline).

### Le socle commun

- **Namespace direct.** Ces trois headers déclarent leurs symboles directement dans `nkentseu`
  (et le détail dans `nkentseu::detail`), **pas** dans `nkentseu::containers`.
- **Allocateur conscient.** `NkFunction` prend un `memory::NkAllocator*` (défaut
  `&memory::NkGetDefaultAllocator()`), validé à la construction. La mémoire vient de NKMemory,
  jamais de `new`. Voir [NKMemory](../NKMemory.md).
- **Pas d'exceptions.** Appeler une `NkFunction` vide renvoie `R{}` ; un appel indexé hors borne
  renvoie `R{}`. Aucun `throw`, cohérent avec la politique zero-STL.
- **Sans allocation côté Bind.** `NkBoundCallable` ne fait aucune allocation par lui-même ; c'est la
  `NkFunction` qui le reçoit qui clonera (et donc allouera).

---

### Exemple

```cpp
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Functional/NkBind.h"
#include "NKContainers/Functional/NkFunctional.h"
using namespace nkentseu;

// NkFunction : une table de callbacks d'événements, rappelée plus tard.
NkFunction<void(int)> onDamage = [](int dmg) { /* … */ };
if (onDamage) onDamage(25);                 // on teste AVANT d'appeler

// NkBind : pré-câbler une méthode membre avec son contexte.
struct Enemy { void Hit(int dmg); };
Enemy e;
NkFunction<void(int)> hit = NkBind(&e, &Enemy::Hit);   // -> e.Hit(dmg)
hit(10);

// NkBindThreadFunc : une méthode membre comme corps de thread.
struct Worker { void Run(void* data); };
Worker w;
NkFunction<void(void*)> entry = NkBindThreadFunc(&w, &Worker::Run);
// thread.Start(entry, &context);

// NkFunctional : spécialiser NkHash pour une clé maison (le primaire ne compile pas).
struct TileKey { uint32_t x, y; };
template<>
struct nkentseu::NkHash<TileKey> {
    constexpr usize operator()(const TileKey& k) const noexcept {
        return NkHash<uint32_t>{}(k.x) ^ (NkHash<uint32_t>{}(k.y) << 1);
    }
};
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [CacheFriendly →](CacheFriendly.md)
