# Les smart pointers

> Couche **Foundation** · NKMemory · Posséder un objet et le libérer **automatiquement** :
> le propriétaire unique `NkUniquePtr` (+ variante tableau), le partage compté
> `NkSharedPtr` / `NkWeakPtr`, et le comptage intrusif `NkIntrusivePtr`.

Quand on alloue un objet via NKMemory, une question se pose immédiatement : qui est
responsable de le détruire, et quand ? Le faire à la main — penser à appeler `Delete` au bon
endroit, sur **tous** les chemins de sortie d'une fonction, sans jamais le faire deux fois — est
l'une des sources les plus classiques de fuites et de corruptions de mémoire. Les **smart
pointers** existent pour supprimer ce problème : ce sont de petits objets qui *possèdent* un
pointeur et le libèrent **automatiquement** dès qu'il n'est plus nécessaire. On n'écrit plus de
`Delete` ; la libération suit la durée de vie du smart pointer.

NKMemory en fournit trois familles. Elles ne diffèrent pas par ce qu'elles font (posséder et
libérer) mais par leur **politique de possession**. Choisir le bon revient donc à répondre à une
seule question : *combien de propriétaires cet objet a-t-il ?* — un seul → `NkUniquePtr` ;
plusieurs à parts égales → `NkSharedPtr` (+ `NkWeakPtr` pour observer) ; plusieurs sur un type
qu'on contrôle et qui peut porter lui-même son compteur → `NkIntrusivePtr`. Et un réflexe commun
aux trois, qui est la **règle dure de NKMemory** : dès qu'un objet est confié à un smart pointer,
on ne fait plus **jamais** `new`/`delete` dessus à la main — mélanger libération automatique et
heap CRT ramène exactement les bugs (`0xC0000374`, double-free) qu'on cherchait à éliminer.

- **Namespace** : `nkentseu::memory`
- **Headers** : `#include "NKMemory/NkUniquePtr.h"`, `"NKMemory/NkSharedPtr.h"`,
  `"NKMemory/NkIntrusivePtr.h"`

---

## NkUniquePtr — un seul propriétaire

C'est le cas le plus fréquent, et celui qu'il faut **préférer par défaut** : un objet n'a qu'un
seul propriétaire à un instant donné. `NkUniquePtr<T, Deleter>` modélise exactement cela. Il
détient le pointeur de façon **exclusive** et le détruit tout seul à la fin de sa portée, sans
aucun compteur — son coût est donc celui d'un pointeur brut (plus le deleter, stocké par valeur).

```cpp
{
    auto texture = NkMakeUnique<Texture>("hero.png");
    texture->Bind();
}   // la texture est détruite ici, automatiquement
```

Il n'y a **aucun** `Delete` à écrire, et aucune fuite n'est possible : même si une exception est
levée ou si la fonction fait un `return` au milieu, la destruction a lieu en sortant de la portée.
On accède à l'objet comme à un pointeur normal, avec `operator->` et `operator*`, et
`explicit operator nk_bool()` teste la non-nullité (`if (texture)`).

Puisqu'il n'y a **qu'un** propriétaire, un `NkUniquePtr` ne peut **pas** être copié — copier
reviendrait à avoir deux objets qui croient chacun posséder la même ressource, donc à la détruire
deux fois. Le compilateur l'interdit (constructeur et affectation de copie `= delete`). Ce qu'on
peut faire, c'est **transférer** la possession (un *move*) :

```cpp
auto a = NkMakeUnique<Texture>("hero.png");
auto b = traits::NkMove(a);   // b possède maintenant, a est vide
```

Après le transfert, `a` ne pointe plus sur rien (`if (a)` renvoie faux) : il ne peut toujours y
avoir qu'un seul propriétaire. Sous le capot, le move passe par `Release()`, qui relâche
l'ownership sans détruire.

`NkMakeUnique<T>(args...)` est la façon recommandée de créer l'objet : elle l'alloue via
`NkGetDefaultAllocator().New<T>(...)`, lui transmet vos arguments, et câble un deleter qui
rappellera le **même** allocateur. Si vous avez besoin d'un allocateur précis (un pool, par
exemple), `NkMakeUniqueWithAllocator(alloc, …)` capture cet allocateur dans le deleter.

Pour **emprunter** l'objet sans en prendre la possession — le passer à une fonction qui ne fait que
l'utiliser — récupérez le pointeur brut avec `Get()`, sans rien transférer :

```cpp
void Render(const Texture* tex);   // emprunte, ne possède pas
Render(texture.Get());
```

À l'inverse, `Release()` vous rend le pointeur brut **et** vide le `NkUniquePtr` : la
responsabilité de libérer (via le **même** allocateur) vous revient alors — rare, mais utile à la
frontière avec une API C. `Reset(ptr)` remplace l'objet possédé (détruisant l'ancien), et
`Swap(other)` échange pointeur **et** deleter en `O(1)`.

Enfin, pour les **tableaux**, la spécialisation `NkUniquePtr<T[]>` remplace `operator*`/`->` par
`operator[](index)` (sans **aucun** bounds-check) et se construit avec `NkMakeUniqueArray<T>(count)`
(chaque élément est construit par défaut). Sa libération passe par `DeleteArray`, qui relit
l'en-tête de comptage posé par `NewArray` et détruit les éléments en ordre inverse.

```cpp
auto buffer = NkMakeUniqueArray<int>(256);
buffer[0] = 42;
```

> **En résumé.** `NkUniquePtr` est le choix par défaut : coût nul (pas de compteur), possession
> claire, libération automatique. Non-copyable, **movable** (`NkMove`). `Get()` pour emprunter,
> `Release()` pour reprendre la main, variante `[]` pour les tableaux. N'en sortez que si vous avez
> réellement **plusieurs** propriétaires.

---

## NkSharedPtr et NkWeakPtr — possession partagée

Parfois, un même objet doit être maintenu en vie par plusieurs endroits du code, sans qu'aucun ne
soit « le » propriétaire. Tant qu'au moins un d'entre eux s'y intéresse, l'objet doit vivre ; quand
le dernier disparaît, il doit être détruit. C'est le rôle de `NkSharedPtr<T>`. Il maintient un
**compteur de références** atomique (`NkAtomicInt32`) dans un **bloc de contrôle alloué à part** :
chaque copie l'incrémente, chaque destruction le décrémente, et l'objet n'est libéré que lorsque le
compteur fort retombe à zéro.

```cpp
auto a = NkMakeShared<Texture>("hero.png");   // UseCount() == 1
auto b = a;                                    // UseCount() == 2, a et b partagent
// ...
// quand a ET b ont disparu, la texture est détruite
```

Contrairement au `NkUniquePtr`, copier un `NkSharedPtr` est non seulement permis mais **attendu** :
c'est précisément ainsi qu'on exprime « cet endroit-là s'intéresse aussi à l'objet ». `UseCount()`
donne à tout moment le nombre de propriétaires forts, `Unique()` teste s'il vaut 1, `IsValid()` /
`operator nk_bool` la non-nullité. Notez que `NkMakeShared` fait **deux** allocations (l'objet,
puis le bloc) — ce n'est pas la fusion contiguë objet+bloc d'autres bibliothèques (notée
« optimisation future » dans le header).

Cette commodité a un revers : les **cycles**. Si A garde un `NkSharedPtr` vers B et B un
`NkSharedPtr` vers A, leurs compteurs ne retomberont jamais à zéro même quand plus personne d'autre
ne les utilise — ils se maintiennent mutuellement en vie. C'est une fuite. La solution est
`NkWeakPtr<T>`, une référence **faible** : elle observe l'objet (via un compteur faible distinct)
sans le maintenir en vie. Dans le cycle, l'une des deux directions doit être un `NkWeakPtr` pour
casser la boucle. La conversion `NkSharedPtr → NkWeakPtr` est implicite ; l'inverse passe par une
promotion explicite.

Une référence faible ne donne **pas** d'accès direct à l'objet (pas de `operator*`/`->`) — et pour
cause : l'objet peut avoir été détruit entre-temps. On demande d'abord une promotion en
`NkSharedPtr` avec `Lock()`, **thread-safe** : si l'objet existe encore, on obtient un
`NkSharedPtr` valide qui le maintient en vie le temps qu'on s'en sert ; sinon, un pointeur nul.

```cpp
NkWeakPtr<Texture> weak = a;     // observe, sans prolonger la vie de l'objet

if (auto locked = weak.Lock()) { // promotion sûre et atomique
    locked->Bind();              // ici, l'objet est garanti vivant
}                                // 'locked' relâche sa référence en sortant
```

`Expired()` teste rapidement si l'objet a déjà été détruit, sans tenter la promotion ; `UseCount()`
sur un `NkWeakPtr` observe le compteur fort. **Attention à la portée de la thread-safety** : seul le
*comptage* (copie, destruction, `Lock`, `UseCount`, `Expired`) est atomique ; l'accès à l'objet
pointé via `operator*`/`->` ne l'est pas — protégez-le vous-même si besoin.

> **En résumé.** `NkSharedPtr` quand plusieurs endroits doivent **co-posséder** un objet (compteur
> atomique, bloc de contrôle séparé, 2 allocations) ; `NkWeakPtr` pour **observer** sans posséder,
> et **obligatoirement** pour casser un cycle. On accède à l'objet observé uniquement par `Lock()`.

---

## NkIntrusivePtr — quand l'objet porte son propre compteur

`NkSharedPtr` range son compteur dans un bloc alloué à part (2 allocations). Pour des objets très
nombreux et très partagés — les ressources et entités d'un moteur, typiquement — on peut faire plus
léger en rangeant le compteur **dans l'objet lui-même** : une **seule** allocation, pas de bloc de
contrôle. C'est le comptage *intrusif*. Le contrat : l'objet doit hériter **publiquement** de
`NkIntrusiveRefCounted`, qui lui ajoute le compteur atomique et les opérations `AddRef`/`ReleaseRef`.
`NkIntrusivePtr<T>` s'en sert ensuite comme un `NkSharedPtr`, mais sans bloc séparé.

```cpp
class Entity : public nkentseu::memory::NkIntrusiveRefCounted {
public:
    explicit Entity(const char* name) : mName(name) {}
    void Update();
private:
    const char* mName;
};

auto e  = nkentseu::memory::NkMakeIntrusive<Entity>("Hero");  // UseCount() == 1
auto e2 = e;                                                   // UseCount() == 2
e->Update();
```

Un `static_assert` vérifie à la compilation que `T` hérite bien de `NkIntrusiveRefCounted` :
impossible d'utiliser un `NkIntrusivePtr` sur un type qui ne s'y prête pas. À la différence des deux
autres familles, le comptage intrusif **n'utilise PAS l'allocateur NKMemory** : `NkMakeIntrusive`
fait un `new T(...)`, et la destruction passe par `delete this`.

Un point de vigilance : la destruction est **entièrement** gérée par le compteur. Quand
`ReleaseRef()` fait tomber le compteur à zéro, l'objet se détruit lui-même (`delete this`). Vous ne
devez donc **jamais** faire `delete` sur un tel objet, ni le créer autrement que par
`NkMakeIntrusive` (qui pose le compteur initial à 1 correctement, via un `AddRef()` puis un
constructeur de pointeur en `addRef=false` pour éviter le double-incrément).

> **En résumé.** `NkIntrusivePtr` est un partage compté plus léger (1 allocation, compteur dans
> l'objet), réservé aux types conçus pour ça (héritant de `NkIntrusiveRefCounted`). Idéal pour les
> ressources/entités du moteur massivement partagées. Mais il utilise `new`/`delete this`, pas
> l'allocateur custom.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. `nk_bool`/`nk_int32`/`nk_size` sont
les types primitifs NKCore. Chacun est détaillé (stratégie, cas d'usage) dans la « Référence
complète » qui suit.

### `NkUniquePtr<T, Deleter>` — propriétaire unique

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkUniquePtr()`, `explicit NkUniquePtr(ptr)`, `NkUniquePtr(ptr, deleter)`, move | Nul / adopte / deleter custom / transfert `[O(1)]` (copie `= delete`) |
| Accès | `operator*` `[O(1)]`, `operator->` `[O(1)]`, `Get`, `explicit operator nk_bool`, `IsValid` | Déréférence / membre / pointeur brut / non-nul ? / valide ? |
| Deleter | `GetDeleter()` (mut/const) | Accès au deleter stocké |
| Cycle de vie | `Release` `[O(1)]`, `Reset(ptr=null)` `[O(1)]`, `Swap` `[O(1)]` | Relâche sans détruire / remplace (détruit l'ancien) / échange ptr+deleter |
| Deleter par défaut | `NkDefaultDelete<T>` / `<T[]>` | `operator()` → `Delete` (objet) `[O(1)]` / `DeleteArray` (ordre inverse) `[O(n)]` |
| Fabriques | `NkMakeUnique`, `NkMakeUniqueWithAllocator` | Crée+adopte (allocateur défaut / fourni) |

### `NkUniquePtr<T[], Deleter>` — variante tableau

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Accès | `operator[](index)` `[O(1)]` | Indexé, **sans bounds-check** (pas de `*`/`->`) |
| Identique à `<T>` | `Get`, `IsValid`, `operator nk_bool`, `Release`, `Reset`, `Swap`, `GetDeleter` | Même API ; `Reset` libère via `DeleteArray` |
| Fabriques | `NkMakeUniqueArray`, `NkMakeUniqueArrayWithAllocator` | Alloue `count` éléments (construits par défaut) |

### `NkSharedPtr<T>` — partage compté

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSharedPtr()`, `(nullptr)`, `explicit (ptr, alloc=null)`, copie, move, `explicit (NkWeakPtr)` | Nul / adopte+alloue bloc (**non `noexcept`**) / partage `AddStrongRef` / promotion weak |
| Accès | `Get`, `operator*`, `operator->`, `explicit operator nk_bool`, `IsValid` | Pointeur / déréférence / membre / non-nul ? |
| Interrogation | `UseCount`, `Unique` | Nombre de propriétaires forts / vaut-il 1 ? |
| Cycle de vie | `Reset()`, `Reset(ptr, alloc=null)`, `Swap` `[O(1)]` | Vide / adopte un nouvel objet / échange |
| Fabriques | `NkMakeShared`, `NkMakeSharedWithAllocator` | Crée+partage (2 allocations : objet + bloc) |

### `NkWeakPtr<T>` — observateur faible

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkWeakPtr()`, `NkWeakPtr(NkSharedPtr)` (implicite), copie, move | Vide / observe (`AddWeakRef`) / copie / transfert |
| Promotion | `Lock()` | → `NkSharedPtr` valide si vivant, sinon vide (thread-safe) |
| Interrogation | `Expired`, `IsValid`, `UseCount` | Détruit ? / vivant ? / compteur fort observé |
| Cycle de vie | `Reset()`, `Swap` `[O(1)]`, `operator=` (weak / shared / move) | Vide / échange / réaffecte |

### `NkIntrusivePtr<T>` — comptage intrusif

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `NkIntrusiveRefCounted` : `AddRef`, `ReleaseRef`, `RefCount` | Compteur dans l'objet (init 0) ; `ReleaseRef` → `delete this` à 0 |
| Construction | `NkIntrusivePtr()`, `(nullptr)`, `explicit (ptr, addRef=true)`, copie, move | Nul / adopte (+AddRef) / partage / transfert |
| Accès | `Get`, `operator*`, `operator->`, `explicit operator nk_bool`, `UseCount` | Pointeur / déréférence / membre / non-nul ? / `RefCount()` |
| Cycle de vie | `Reset(ptr=null, addRef=true)`, `Release`, `Swap` `[O(1)]`, `operator=` (ptr / nullptr) | Réaffecte / relâche **sans** `ReleaseRef` / échange |
| Fabrique | `NkMakeIntrusive` | `new T(...)` + AddRef (PAS d'allocateur NKMemory) |
| Comparaisons | `==` / `!=` (ptr, brut, `nullptr`) | Comparent `Get()` |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines du temps réel.
Les éléments triviaux sont décrits brièvement ; les choix de possession le sont **à fond**. Rappel
transverse : ces objets sont la mise en application de la **règle dure NKMemory** — pas de
`new`/`delete` brut, et symétrie `Create`/`Destroy` respectée par les fabriques.

### Choisir : combien de propriétaires ?

Le seul vrai critère est la **politique de possession** :

- **Un seul propriétaire** (le cas par défaut, ~90 %) → `NkUniquePtr`. Pas de compteur, possession
  limpide, coût d'un pointeur brut.
- **Plusieurs propriétaires** à durées de vie indépendantes → `NkSharedPtr`, avec `NkWeakPtr` pour
  les observateurs et pour **briser les cycles**.
- **Plusieurs propriétaires** sur un type que vous contrôlez et qui peut hériter de
  `NkIntrusiveRefCounted` → `NkIntrusivePtr` (1 allocation, mais `new`/`delete this`).

| Critère | `NkUniquePtr` | `NkSharedPtr` (+`NkWeakPtr`) | `NkIntrusivePtr` |
|---------|---------------|------------------------------|------------------|
| Propriétaires | exactement 1 | N (compté) | N (compté) |
| Compteur | aucun | atomique, **bloc séparé** | atomique, **dans l'objet** |
| Allocations | 1 | 2 (objet + bloc) | **1** |
| Copyable | non (move only) | oui | oui |
| Allocateur NKMemory | oui (capturé) | oui (capturé) | **non** (`new`/`delete this`) |
| Contrainte sur `T` | aucune | aucune | hérite de `NkIntrusiveRefCounted` |
| Casse les cycles | — | `NkWeakPtr` | raw / weak externe |

### `NkUniquePtr` à fond

**Possession exclusive, coût nul.** `NkUniquePtr` stocke un `pointer` et un `Deleter` par valeur ;
pas de compteur, pas d'allocation supplémentaire. Le deleter par défaut, `NkDefaultDelete<T>`,
capture un `NkAllocator*` (nullptr = allocateur par défaut) et appelle `allocator->Delete(ptr)`
(destructeur + désallocation) en `O(1)` ; sa spécialisation `<T[]>` appelle `DeleteArray`, qui relit
le count stocké par `NewArray` et détruit en ordre inverse (`O(n)`). `Reset` invoque le deleter sur
l'ancien objet ; `Release` rend le pointeur **sans** détruire (à vous de libérer ensuite via le
même allocateur) ; `Swap` échange pointeur et deleter.

Cas d'usage, par domaine :
- **Rendu / GPU** — possession 1-à-1 d'une ressource GPU (texture, buffer, pipeline) par son
  gestionnaire ; détruite déterministiquement en fin de portée. `NkMakeUniqueArray` pour un tampon
  CPU temporaire de sommets/indices avant upload.
- **ECS / scène** — un système possède son état interne (caches, index) en `NkUniquePtr` ;
  transféré (`NkMove`) quand on déplace le système.
- **Physique** — possession exclusive d'un *broadphase* ou d'un solveur par le monde physique.
- **Animation** — un contrôleur possède sa machine à états ou son squelette de travail.
- **Gameplay / IA** — un arbre de comportement, un blackboard, propriété unique de l'agent.
- **Audio** — un décodeur de flux (MP3/OGG) possédé par la voix qui le lit.
- **UI / 2D** — un widget racine possédant ses enfants (le move exprime le re-parentage).
- **IO** — un handle de fichier ou un parseur, libéré sûrement même sur chemin d'erreur (RAII).

Frontière avec une API C : `Get()` pour **emprunter** (la callee ne possède pas), `Release()` pour
**céder** la possession à du code qui fera la libération lui-même. Et toujours :
`NkMakeUniqueWithAllocator(pool, …)` lie l'objet à `pool` — il **doit** rester géré par ce smart
pointer, qui rappellera ce même `pool`.

### `NkSharedPtr` / `NkWeakPtr` à fond

**Comptage atomique, bloc de contrôle séparé.** Le bloc (`NkSharedControlBlock<T>`) porte deux
compteurs atomiques — `strong` (durée de vie de l'objet) et `weak` (durée de vie du bloc, +1 interne)
— et l'allocateur capturé. `AddStrongRef`/`ReleaseStrongRef` gèrent l'objet (détruit à 0 strong),
`AddWeakRef`/`ReleaseWeakRef` gèrent le bloc (libéré à 0 weak). La promotion sûre repose sur
`TryAddStrongRef`, une boucle **CAS** qui n'incrémente que si l'objet est encore vivant — c'est elle
qui rend `Lock()` thread-safe. Le constructeur depuis pointeur brut **n'est pas `noexcept`** :
l'allocation du bloc peut échouer (auquel cas l'objet adopté est détruit et le shared reste vide).

Cas d'usage, par domaine :
- **Rendu / GPU** — une `NkSharedPtr<Material>` ou `<Texture>` référencée par plusieurs meshes : la
  ressource vit tant qu'un mesh l'utilise, libérée au dernier.
- **ECS** — un component qui pointe une ressource partagée ; `NkWeakPtr` quand un système veut
  *observer* une entité sans empêcher sa destruction (cible d'IA, suivi de caméra).
- **Physique** — contraintes liant deux corps : chaque contrainte garde un `NkWeakPtr` vers les
  corps (les corps possèdent, la contrainte observe), évitant un cycle corps↔contrainte.
- **Animation** — événements d'animation référençant la cible via `NkWeakPtr` (la cible peut mourir
  pendant la lecture ; `Lock()` avant chaque accès).
- **Gameplay / IA** — `mTarget` d'un agent en `NkWeakPtr<Entity>` : on `Lock()` chaque frame, et si
  c'est `Expired()` la cible a disparu, on la lâche proprement.
- **Audio** — un bus partagé par plusieurs voix ; le bus survit tant qu'une voix l'alimente.
- **UI / 2D** — un thème ou un atlas de polices partagé entre fenêtres ; observateurs en `NkWeakPtr`.
- **IO** — un cache d'assets : la map garde des `NkWeakPtr`, les consommateurs des `NkSharedPtr` ;
  une entrée disparaît du cache quand plus personne ne la tient.

`UseCount()` et le compteur sous-jacent sont des **snapshots** (potentiellement périmés juste après
lecture) — à réserver au debug, pas à une logique de décision. Et le rappel de thread-safety :
**seul le comptage** est atomique, **pas** l'accès à l'objet (`*`/`->`) — protégez-le (ex. `NkMutex`).

### `NkIntrusivePtr` à fond

**Compteur dans l'objet, une seule allocation.** `NkIntrusiveRefCounted` porte un
`mutable NkAtomicInt32 mRefCount` (init **0**), d'où `AddRef`/`ReleaseRef` const-corrects
(`AddRef` appelable sur const). `ReleaseRef` fait `delete this` quand le compteur passe à 0 — c'est
pourquoi le destructeur de la base est **virtuel** (destruction polymorphe correcte). Subtilité de
copie : le **copy-ctor remet le compteur à 0** (une copie est un nouvel objet, encore non
référencé) et l'**opérateur d'affectation est un no-op** (le ref-count ne se copie pas).
`NkMakeIntrusive` fait `new T(...)`, puis `AddRef()`, et construit le pointeur en `addRef=false`.

Cas d'usage, par domaine :
- **Rendu / GPU** — ressources moteur massivement partagées (textures, shaders, meshes) où l'on veut
  une seule allocation et un compteur ultra-léger dans la ressource elle-même.
- **ECS** — entités/handles partagés à très haute fréquence : le compteur intrusif évite le bloc de
  contrôle et l'indirection du `NkSharedPtr`.
- **Physique / animation / audio** — objets nodaux d'un graphe (corps, os, nœuds DSP) que vous
  contrôlez et pouvez faire hériter de la base.
- **UI / 2D** — nœuds de scène 2D partagés.

Pièges : **jamais** `delete` direct sur un objet intrusif (passez par `ReleaseRef()`/le smart
pointer) ; `Release()` relâche **sans** appeler `ReleaseRef`, donc l'appelant **doit** l'appeler plus
tard sous peine de fuite ; et il n'existe **pas** de `IsValid()` (les exemples du header le
mentionnent par erreur) — utilisez `explicit operator nk_bool` ou `Get()`. Les comparaisons libres
(`==`/`!=` contre un autre `NkIntrusivePtr`, un `T*` brut ou `nullptr`) comparent les `Get()`.

### Le socle commun

- **Passer par les fabriques.** `NkMakeUnique` / `NkMakeUniqueArray` / `NkMakeShared` /
  `NkMakeIntrusive` (et variantes `*WithAllocator`) câblent correctement allocateur, deleter et
  `AddRef` initial. C'est l'application de la symétrie **Create/Destroy** : on ne construit pas la
  ressource « à la main ».
- **Ne jamais mélanger les allocateurs.** `NkUniquePtr`/`NkSharedPtr` libèrent via l'allocateur
  capturé ; un objet créé par `NkMakeUniqueWithAllocator(pool, …)` doit rester géré par ce smart
  pointer. Mélanger heap CRT et allocateur custom NKMemory → corruption `c0000374`.
- **`Release()` transfère la responsabilité.** Pour `NkUniquePtr`, libérez ensuite via le même
  allocateur ; pour `NkIntrusivePtr`, appelez `raw->ReleaseRef()` — sinon fuite.
- **Casser les cycles.** Côté `NkSharedPtr`, par `NkWeakPtr` ; côté intrusif, par un pointeur
  raw/weak externe.
- **Thread-safety.** Seul le comptage est atomique ; l'accès à l'objet pointé reste à protéger.
  `RefCount`/`UseCount` sont des snapshots, bons pour le debug uniquement.

---

### Exemple

```cpp
#include "NKMemory/NkUniquePtr.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKMemory/NkIntrusivePtr.h"
using namespace nkentseu::memory;

// Unique : possession exclusive, libérée en fin de portée. Move pour transférer.
auto texture = NkMakeUnique<Texture>("hero.png");
texture->Bind();
auto owned = traits::NkMove(texture);     // owned possède, texture est vide

// Tableau : variante operator[] (sans bounds-check).
auto buffer = NkMakeUniqueArray<int>(256);
buffer[0] = 42;

// Shared + Weak : co-possession, et observation pour casser un cycle.
auto sprite = NkMakeShared<Sprite>("coin.png");   // UseCount() == 1
NkWeakPtr<Sprite> watcher = sprite;                // observe sans prolonger
if (auto locked = watcher.Lock()) {                // promotion sûre
    locked->Draw();                                // garanti vivant ici
}

// Intrusif : compteur dans l'objet (T : public NkIntrusiveRefCounted).
auto e  = NkMakeIntrusive<Entity>("Hero");         // UseCount() == 1
auto e2 = e;                                        // UseCount() == 2
e->Update();
// jamais 'delete e' : ReleaseRef() s'en charge quand le compteur tombe à 0
```

---

[← Les allocateurs](Allocators.md) · [Index NKMemory](README.md) · [Récap NKMemory](../NKMemory.md) · [Opérations mémoire →](Memory-Operations.md)
