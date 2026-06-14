# Le ramasse-miettes

> Couche **Foundation** · NKMemory · Un ramasse-miettes *mark-and-sweep* **optionnel** pour les
> graphes d'objets enchevêtrés : le collecteur `NkGarbageCollector`, la base collectable
> `NkGcObject`, le visiteur de marquage `NkGcTracer` et la racine externe `NkGcRoot`.

Les smart pointers couvrent la quasi-totalité des besoins de gestion de durée de vie. Mais ils ont
une limite, qu'on a déjà croisée : les **cycles**. Quand des objets se référencent mutuellement en
formant des boucles arbitraires — un graphe de scène, un arbre syntaxique, des structures de
scripting — le comptage de références seul ne suffit plus, et casser chaque cycle à la main avec des
références faibles devient vite ingérable.

Pour ces cas, et **seulement** ces cas, NKMemory propose un ramasse-miettes *mark-and-sweep*
optionnel. Le principe est classique : partant de quelques objets « racines » connus pour être
vivants, on *marque* tout ce qui est atteignable en suivant les références, puis on *balaie* en
libérant tout ce qui n'a pas été marqué — c'est-à-dire ce que plus personne n'atteint, cycles
compris. Ce n'est **pas** un GC concurrent ni générationnel : `Collect()` est un cycle unique,
**bloquant**, qui suspend les autres threads le temps du marquage et du balayage.

- **Namespace** : `nkentseu::memory`
- **Header** : `#include "NKMemory/NkGc.h"`

> C'est un outil spécialisé. Si vous hésitez entre le GC et un smart pointer, prenez le smart
> pointer : il est plus simple et déterministe. Le GC ne se justifie que face à des graphes d'objets
> vraiment enchevêtrés.

---

## Rendre un objet collectable

Un objet géré par le GC hérite **publiquement** de `NkGcObject`. Cet héritage lui demande une seule
chose en retour : savoir **déclarer ses références sortantes**, en surchargeant `Trace`. C'est par
là que le collecteur découvre ce que votre objet maintient vivant. La classe de base porte des
métadonnées privées (chaînage, drapeau de marquage, allocateur d'origine) que vous n'avez jamais à
toucher — seul `Trace` vous concerne.

```cpp
class MyNode : public NkGcObject {
public:
    explicit MyNode(int v) : value(v) {}

    void Trace(NkGcTracer& tracer) override {
        if (child) tracer.Mark(child);   // « je référence 'child' »
    }

    int      value = 0;
    MyNode*  child = nullptr;
};
```

Chaque appel à `tracer.Mark(x)` dit au collecteur « cet objet-là est atteignable depuis moi ». La
règle est impérative : déclarez **toutes** vos références. Un objet réellement référencé mais oublié
dans `Trace` sera considéré comme inatteignable et collecté à tort — vous vous retrouveriez avec un
pointeur pendouillant. C'est le seul vrai piège du GC. Le réflexe sûr est de toujours garder chaque
marquage (`if (ptr) tracer.Mark(ptr);`) et d'itérer vos conteneurs pour marquer chaque élément
non-null.

Une contrainte importante : `Trace` est appelée **pendant la phase de marquage, le lock du GC déjà
détenu**. On n'y alloue donc rien, on n'y modifie pas l'objet, on n'appelle ni `New`, ni `Delete`,
ni `Collect` (deadlock garanti). `Trace` ne fait qu'une chose : énumérer des `Mark`.

> **En résumé.** Héritez publiquement de `NkGcObject` et surchargez `Trace` pour **déclarer
> exhaustivement** vos références sortantes via `tracer.Mark(...)`. `Trace` s'exécute sous le lock
> du GC : pas d'allocation, pas de mutation, pas de `New`/`Delete`/`Collect` à l'intérieur. Une
> référence oubliée = un objet collecté à tort.

---

## Allouer, enraciner, collecter

On ne crée pas un objet collectable avec `new` ni avec l'allocateur ordinaire, mais via le
collecteur lui-même, qui le prend en charge :

```cpp
NkGarbageCollector gc;

auto* root  = gc.New<MyNode>(42);
auto* child = gc.New<MyNode>(17);
root->child = child;

static MyNode* gRoot = root;
static NkGcRoot rootHandle(reinterpret_cast<NkGcObject**>(&gRoot));
gc.AddRoot(&rootHandle);       // 'root' est un point de départ vivant
// ...
gc.Collect();                  // marque depuis les racines, balaie le reste
```

`gc.New<T>()` vérifie à la compilation, via un `static_assert`, que `T` hérite bien de `NkGcObject`
— impossible donc de confier au GC un type qui ne s'y prête pas. En interne, il alloue via
l'allocateur configuré, construit l'objet par *placement new* avec *perfect forwarding*, l'inscrit
dans le collecteur, et renvoie `nullptr` si l'allocation échoue.

`AddRoot` (et son inverse `RemoveRoot`) déclarent les points de départ du marquage : typiquement vos
objets globaux ou de premier plan, ceux qui sont vivants par définition. Une racine n'est pas un
pointeur mais un `NkGcRoot`, qui enveloppe l'**adresse** d'un pointeur (`NkGcObject**`, une double
indirection). C'est subtil et utile : comme la racine connaît l'adresse du *slot*, elle suit
automatiquement la réassignation du pointeur qu'il contient — vous changez `gRoot`, le GC voit la
nouvelle cible sans rien redéclarer. `Bind()` ne sert que si l'adresse du slot lui-même change (par
exemple un tableau de racines réalloué).

Tout ce qui est atteignable depuis une racine survit ; le reste tombe au prochain `Collect()`.
`Delete<T>` reste disponible si vous voulez détruire explicitement un objet sans attendre le
balayage, mais l'esprit du GC est justement de vous décharger de cette décision.

> **En résumé.** Créez vos objets avec `gc.New<T>(...)` (jamais `new`), désignez les points de
> départ avec un `NkGcRoot` sur l'**adresse** d'un pointeur + `AddRoot`, puis `gc.Collect()` libère
> l'inatteignable. La double indirection de `NkGcRoot` suit la réassignation toute seule.
> `Delete<T>` détruit à la demande ; pour tout le reste, les smart pointers restent le bon choix.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Complexités et `noexcept` entre
crochets quand c'est utile. Chacun est détaillé dans la « Référence complète » qui suit.

### `NkGcObject` — base collectable

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `using NkGcDestroyFn = void (*)(NkGcObject*, NkAllocator*) noexcept` | Signature de la destruction personnalisée (objets `New<T>`). |
| Cycle de vie | `NkGcObject()` `[noexcept]`, `virtual ~NkGcObject() = default` | Initialise les métadonnées GC / destruction polymorphe sûre. |
| Marquage | `virtual void Trace(NkGcTracer&)` | **À surcharger** : déclare les références sortantes (no-op par défaut). |
| Debug | `IsMarked()` `[nodiscard, noexcept]` | Marqué lors de la dernière collecte ? (reset après chaque `Collect`). |

### `NkGcTracer` — visiteur de marquage

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `explicit NkGcTracer(NkGarbageCollector&)` `[noexcept]` | Interne — fourni par le GC, ne pas instancier. |
| Marquage | `Mark(NkGcObject*)` `[noexcept]` | Marque l'objet atteint ; `nullptr` ignoré ; **idempotent** ; déclenche `Trace` récursif. |

### `NkGcRoot` — racine externe

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `explicit NkGcRoot(NkGcObject** slot = nullptr)` `[noexcept]` | Enveloppe l'adresse d'un pointeur protégé (bindable plus tard). |
| Binding | `Bind(NkGcObject** slot)` `[noexcept]` | Met à jour l'adresse du slot (si le slot lui-même se déplace). |
| Accès | `Slot()` `[nodiscard, noexcept]` | Renvoie l'adresse du pointeur protégé (ou `nullptr`). |

### `NkGarbageCollector` — mark & sweep

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `explicit NkGarbageCollector(NkAllocator* = nullptr)` `[noexcept]`, `~NkGarbageCollector()` | Construit (fallback `NkGetDefaultAllocator`) / libère tous les objets restants. **Non-copiable, non-movable**. |
| Allocateur | `SetAllocator(NkAllocator*)` `[noexcept]`, `GetAllocator()` `[nodiscard, noexcept]` | Définit / lit l'allocateur (Set échoue en silence si des objets sont déjà inscrits). |
| Création type-safe | `New<T>(args...)`, `Delete<T>(T*)` `[Delete: noexcept ; New ne l'est pas]` | Alloue+construit+inscrit un objet managé / désinscrit+détruit. |
| Tableaux | `NewArray<T>(count)`, `DeleteArray<T>(T*, count)` `[noexcept]` | Tableau `new T[]` inscrit élément par élément / `delete[]` après désinscription. |
| Enregistrement | `RegisterObject(NkGcObject*)` `[noexcept]`, `UnregisterObject(NkGcObject*)` `[noexcept]` | Inscrit / désinscrit un objet (idempotent ; Unregister **n'appelle pas** le destructeur). |
| Racines | `AddRoot(NkGcRoot*)` `[noexcept]`, `RemoveRoot(NkGcRoot*)` `[noexcept]` | Ajoute / retire une racine externe. |
| Mémoire brute | `Allocate(size, alignment = alignof(void*))` `[noexcept]`, `Free(void*)` `[noexcept]` | Mémoire **non tracée** par le GC (libération manuelle). |
| Interrogation | `ContainsObject(const NkGcObject*)` `[nodiscard, noexcept, O(1) moy.]`, `ObjectCount()` `[nodiscard, noexcept]` | Objet managé ? / nombre d'objets gérés. |
| Collecte | `Collect()` `[noexcept]` | Cycle mark-and-sweep complet, **bloquant**. |

### Fabriques au niveau système (déclarées dans `NkMemory.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Système | `NkMemorySystem::CreateGc(NkAllocator* = nullptr)` | Crée un collecteur géré par le système mémoire. |
| Système | `NkMemorySystem::DestroyGc(NkGarbageCollector*)` `→ nk_bool` | Détruit ce collecteur (symétrie `CreateGc` ↔ `DestroyGc`). |

---

## Référence complète

Chaque élément est repris ici en détail : sa stratégie, sa complexité, et ses usages dans les
différents domaines du moteur. Les éléments triviaux sont décrits brièvement ; les opérations qui
structurent l'algorithme le sont **à fond**. Rappel transversal : **`new`/`delete` raw sont
interdits** dans Nkentseu (mélanger heap CRT et allocateur NKMemory provoque la corruption de tas
Windows `c0000374`) ; toute la mémoire passe par NKMemory, et chaque création a sa destruction
symétrique.

### Le modèle interne, en un mot

Le collecteur tient ses objets dans **deux structures simultanées** : une **liste doublement
chaînée** (qu'il parcourt lors du balayage) et une **table de hachage à sondage linéaire avec
tombstones** (lookup/insertion/suppression en `O(1)` moyen). Les racines externes forment une
**liste simplement chaînée** de `NkGcRoot`. La table se réorganise (rehash) quand le facteur de
charge dépasse 70 % ou que les tombstones dépassent 20 %. Toute la classe est protégée par un
`NkSpinLock` interne : **chaque méthode publique prend le lock**, ce qui rend le GC thread-safe pour
ses propres opérations — mais pas les données membres de *vos* objets, qui restent à votre charge.

### `NkGcObject` — la base collectable

Hériter publiquement de `NkGcObject` est le ticket d'entrée. Le constructeur (noexcept) initialise
les métadonnées (chaînages à `nullptr`, drapeau de marquage à `false`) ; le destructeur est
**virtuel**, condition d'une destruction polymorphe correcte quand le GC libère un objet par un
pointeur de base. Le type imbriqué `NkGcDestroyFn` est la signature d'une fonction de destruction
personnalisée que `New<T>` installe automatiquement — vous n'avez jamais à la fournir vous-même.

- **Rendu** — un nœud de graphe de scène (transform + maillage + enfants) qui peut former des
  cycles parent↔enfant.
- **ECS / gameplay** — des entités de script qui se référencent mutuellement (un ennemi qui cible un
  joueur qui cible l'ennemi).
- **Animation** — un graphe de blend où des états pointent les uns vers les autres.
- **UI / 2D** — un arbre de widgets avec des liens retour (un panneau qui connaît son enfant focus,
  l'enfant qui connaît son parent).

`IsMarked()` est un accessoire de **debug** : il indique si l'objet a survécu au dernier marquage,
mais sa valeur est remise à zéro à chaque `Collect()`. Ne bâtissez aucune logique métier dessus.

### `Trace` — déclarer les références sortantes

C'est **le** point qui fait marcher tout le reste. Le GC ne connaît pas la structure de vos objets ;
c'est `Trace` qui la lui révèle, en appelant `tracer.Mark(...)` pour chaque pointeur sortant vers un
autre `NkGcObject`. Par défaut, `Trace` est un no-op : un objet sans surcharge ne référence
personne. La complexité de `Trace` est celle de votre énumération (généralement `O(k)` pour `k`
références).

- **Rendu** — marquer le maillage, le matériau, la texture et les nœuds enfants référencés.
- **Physique** — marquer les corps liés par une contrainte (joint, ressort).
- **IA / gameplay** — marquer la cible, le propriétaire, les nœuds voisins d'un graphe de
  navigation.
- **Audio** — marquer le bus parent et les sources enfants d'un groupe de mixage.

La discipline est stricte : déclarer **toutes** les références (une omission = collecte prématurée =
*dangling*) ; et ne **rien** faire d'autre que des `Mark` (le lock est déjà pris, allouer ou
appeler `Collect`/`New`/`Delete` ici provoque un deadlock ou une récursion infinie).

### `NkGcTracer::Mark` — le marquage

`Mark(object)` est la primitive du parcours. Elle ignore `nullptr`, et elle est **idempotente** :
re-marquer un objet déjà marqué ne fait rien — c'est précisément ce qui permet au mark-and-sweep de
traiter les **cycles** sans boucler indéfiniment. Marquer un objet pour la première fois déclenche
récursivement son `Trace`, propageant l'atteignabilité de proche en proche. `Mark` acquiert le lock
du GC (thread-safe) et délègue à la mécanique interne de marquage. Vous n'instanciez jamais un
`NkGcTracer` vous-même : le GC vous en fournit un pendant `Collect()`.

### `NkGcRoot` — ancrer ce que le marquage ne voit pas

Le marquage part des racines. Tout ce qui n'est atteignable depuis aucune racine est, par
définition, candidat à la collecte. `NkGcRoot` est l'outil qui déclare ces points de départ pour les
références **hors du graphe managé** : variables globales, état détenu par du code non-managé (C,
scripts), caches ou registres qui maintiennent un objet vivant sans le tracer.

Sa particularité est la **double indirection** : un `NkGcRoot` ne mémorise pas un pointeur, mais
l'**adresse** d'un pointeur (`NkGcObject**`). Conséquence pratique : réassigner le pointeur pointé
est suivi automatiquement, sans toucher à la racine. `Bind(slot)` ne sert qu'au cas où l'adresse du
slot elle-même se déplace (un tableau de racines réalloué). `Slot()` relit cette adresse. Une racine
n'a d'effet qu'une fois passée à `AddRoot` ; `RemoveRoot` la retire.

- **Gameplay** — le pointeur global vers le monde courant, la scène active.
- **UI** — la racine de l'arbre de widgets, le focus courant.
- **Scripting** — la table globale d'un interpréteur embarqué dont les objets sont managés.

### Création et destruction type-safe : `New`, `Delete`, `NewArray`, `DeleteArray`

`New<T>(args...)` est le chemin normal pour fabriquer un objet managé. Il alloue
`sizeof(T)`/`alignof(T)` via l'allocateur configuré, construit par *placement new* avec *perfect
forwarding* des arguments, installe les métadonnées (allocateur d'origine + destructeur
personnalisé), puis inscrit l'objet. Un `static_assert` impose `T : NkGcObject`. Il renvoie
`nullptr` si l'allocation échoue. Il **n'est pas** `noexcept` (le constructeur de `T` peut lever) et
ne doit **jamais** être appelé depuis `Trace` (deadlock). `Delete<T>(obj)` fait l'inverse : il
désinscrit d'abord, puis détruit par le chemin approprié (destructeur custom + désallocation via
l'allocateur d'origine pour les objets créés par `New`).

`NewArray<T>(count)` est à part : il alloue via `new T[count]` (donc **pas** par l'allocateur
custom, et le drapeau de possession n'est pas posé), puis inscrit chaque élément individuellement.
Sa destruction passe **obligatoirement** par `DeleteArray<T>(objects, count)` (qui désinscrit chaque
élément puis `delete[]`). Ne croisez jamais les chemins : un tableau de `NewArray` ne se libère pas
avec `Delete`, et un objet de `New` ne se libère pas avec `delete[]`.

- **Rendu** — allouer en bloc un tableau de nœuds de scène managés (`NewArray`), ou un nœud isolé
  (`New`).
- **ECS** — instancier des composants-objets script via `New`, détruire à la demande via `Delete`.
- **Gameplay** — créer des entités managées sans se soucier de leur libération (le `Collect` s'en
  charge).

> Rappel de la **symétrie stricte** : `New<T>` ↔ `Delete<T>`, `NewArray<T>` ↔ `DeleteArray<T>`
> (même `count`), `Allocate` ↔ `Free`, et au niveau système `CreateGc` ↔ `DestroyGc`. Croiser ces
> paires (par ex. un `delete` raw sur un objet `New<T>`) mélange le heap CRT et l'allocateur NKMemory
> → corruption de tas `c0000374`.

### Enregistrement manuel : `RegisterObject`, `UnregisterObject`

Si vous gérez l'allocation vous-même (objet sur la pile, sur un autre pool, membre d'un agrégat),
vous pouvez quand même le confier au marquage avec `RegisterObject` — l'opération est **idempotente**
(double inscription sans effet). `UnregisterObject` le retire de l'index ; c'est un no-op s'il n'y
est pas, et surtout il **n'appelle pas** le destructeur : il dit seulement au GC d'oublier l'objet.
Les deux sont `O(1)` moyen (table de hachage) et thread-safe. À réserver aux cas où vous maîtrisez
la durée de vie autrement que par `New`/`Delete`.

### Racines : `AddRoot`, `RemoveRoot`

`AddRoot(root)` enchaîne un `NkGcRoot` dans la liste des racines consultée au début de chaque
`Collect()` (phase « mark roots »). `RemoveRoot(root)` l'en retire (no-op si absent). Les deux sont
`O(1)` et thread-safe. Pensez-y comme à l'activation/désactivation d'un point d'ancrage : tant qu'une
racine pointe vers un objet, lui et tout son sous-graphe survivent aux collectes.

### Mémoire brute : `Allocate`, `Free`

`Allocate(size, alignment)` et `Free(ptr)` exposent l'allocateur sous-jacent pour de la mémoire
**brute, non tracée** par le GC — buffers, métadonnées, objets qui ne dérivent pas de `NkGcObject`.
C'est un raccourci de commodité (même allocateur que les objets managés), mais le GC ne suit pas ces
blocs : à vous de les libérer avec `Free`. Ne mélangez jamais ces deux-là avec `New`/`Delete`
(`Free` sur un objet `New<T>`, ou `Delete<T>` sur un pointeur d'`Allocate`, est une faute).

### Interrogation et collecte : `ContainsObject`, `ObjectCount`, `Collect`

`ContainsObject(obj)` teste l'appartenance en `O(1)` moyen via la table de hachage ; `ObjectCount()`
renvoie le nombre d'objets actuellement managés — pratique pour un compteur de debug ou une
assertion de fin de niveau.

`Collect()` est le cœur : il déroule le cycle complet — (1) marquer depuis les racines, (2) propager
récursivement via `Trace`, (3) balayer en détruisant et désinscrivant tout ce qui n'a pas été
marqué. Il est **bloquant** : il garde le lock du GC pendant toute sa durée, suspendant de fait les
threads qui voudraient toucher au GC. Ne l'appelez **jamais** depuis `Trace` (récursion infinie /
deadlock), et ne mutez pas le graphe depuis un autre thread pendant son exécution.

- **Gameplay** — un `Collect` ponctuel entre deux niveaux, quand une pause est acceptable.
- **Scripting** — un `Collect` après l'exécution d'un lot de scripts qui ont créé des objets
  temporaires enchevêtrés.
- **Outils / éditeur** — recycler les nœuds devenus inatteignables après une opération d'édition
  massive.

Le destructeur `~NkGarbageCollector` joue le rôle de filet : il libère **tous** les objets managés
restants. La classe est volontairement **non-copiable et non-movable** (les quatre opérations
spéciales sont `= delete`) : un collecteur possède son graphe de façon unique, le dupliquer ou le
déplacer n'aurait pas de sens.

### Configuration de l'allocateur : `SetAllocator`, `GetAllocator`

`SetAllocator(alloc)` choisit l'allocateur des **futures** allocations (`nullptr` → défaut). Détail
de sûreté : il **échoue silencieusement** si des objets sont déjà inscrits — on ne change pas
d'allocateur sous les pieds d'objets déjà alloués ailleurs. Conséquence : configurez l'allocateur
**avant** toute allocation. `GetAllocator()` relit l'allocateur courant.

### Fabriques système : `CreateGc` / `DestroyGc`

Le header `NkGc.h` n'expose aucune *free function* ni fabrique `NkMake*` : on instancie directement
(`NkGarbageCollector gc;`). En revanche, `NkMemorySystem` (déclaré dans `NkMemory.h`) propose
`CreateGc(allocator)` et `DestroyGc(gc) → nk_bool`, qui placent le collecteur sous la gestion du
système mémoire avec la **symétrie Create/Destroy** imposée à tout le moteur. Utilisez cette voie
quand le GC doit vivre aussi longtemps que le système mémoire ; un `NkGarbageCollector` local suffit
pour un usage circonscrit.

---

### Exemple récapitulatif

```cpp
#include "NKMemory/NkGc.h"
using namespace nkentseu::memory;

class SceneNode : public NkGcObject {
public:
    explicit SceneNode(int id) : id(id) {}

    void Trace(NkGcTracer& tracer) override {
        if (parent) tracer.Mark(parent);     // déclarer TOUTES les références…
        if (child)  tracer.Mark(child);      // …sinon collecte prématurée (dangling)
    }

    int        id     = 0;
    SceneNode* parent = nullptr;
    SceneNode* child  = nullptr;
};

NkGarbageCollector gc;                         // allocateur par défaut

// Création managée — jamais 'new'.
auto* a = gc.New<SceneNode>(1);
auto* b = gc.New<SceneNode>(2);
a->child  = b;  b->parent = a;                 // cycle a <-> b, sans fuite

// Racine globale : double indirection -> suit la réassignation de 'gWorldRoot'.
static SceneNode* gWorldRoot = a;
static NkGcRoot rootHandle(reinterpret_cast<NkGcObject**>(&gWorldRoot));
gc.AddRoot(&rootHandle);

gc.Collect();                                  // a et b survivent (atteints via la racine)

gWorldRoot = nullptr;                          // la racine (déjà inscrite) pointe désormais nullptr
gc.Collect();                                  // a et b sont balayés (cycle inatteignable)
// ~NkGarbageCollector libèrera tout reliquat.
```

---

[← Suivi & profilage](Tracking-Profiling.md) · [Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md) · [Tags & budgets →](Tags-Budgets.md)
