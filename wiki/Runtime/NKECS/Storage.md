# Le stockage par archétypes

> Couche **Runtime** · NKECS · La machinerie qui range les composants en mémoire : le registre de
> types `NkTypeRegistry`, le pool contigu `NkComponentPool`, l'archétype `NkArchetype`, l'index
> d'entités `NkEntityIndex` et le graphe de transitions `NkArchetypeGraph`.

Un ECS pose une question simple en apparence : **où range-t-on les composants d'un million
d'entités** pour que les systèmes les parcourent à pleine vitesse ? La réponse naïve — un tableau de
composants par entité — détruit le cache : on saute partout en mémoire et chaque entité traîne des
champs qu'aucun système ne lit ce frame-là. NKECS répond par les **archétypes** : on regroupe
ensemble *toutes les entités qui ont exactement la même liste de composants*, et on range chaque type
de composant dans son **propre tableau contigu** (un *SoA*, structure-of-arrays). Un système qui veut
les entités « Position + Vitesse » ne visite que les archétypes qui contiennent ces deux types, et y
balaie deux tableaux serrés en mémoire. C'est le même principe que la contiguïté du `NkVector` (voir
[Sequential](../../Foundation/NKContainers/Sequential.md)), poussé à l'échelle d'un moteur entier.

Cette page décrit la **couche de stockage** — la plomberie sous le `NkWorld`. La plupart des types
ici sont **internes** : en gameplay on touche au monde, pas au pool. Mais comprendre cette mécanique
explique *pourquoi* l'ECS est rapide, et la quelques pièges (limites dures, allocation hétérogène,
migration en deux temps) qu'elle impose.

- **Namespace** : `nkentseu::ecs`
- **Header parapluie** : `#include "NKECS/NKECS.h"`
- **Headers** : `NKECS/Core/NkTypeRegistry.h`, `NKECS/Storage/NkComponentPool.h`,
  `NKECS/Storage/NkArchetype.h`, `NKECS/Storage/NkArchetypeGraph.h` (tous tirent
  `NKECS/NkECSDefines.h`).

> **Avertissement d'implémentation.** Malgré la règle projet « zéro-STL / NKMemory uniquement », ces
> headers utilisent réellement `new`/`delete[]` (graphe, archétype, index) et
> `_aligned_malloc`/`std::aligned_alloc`/`std::free`/`std::memcpy` (pools). C'est documenté tel quel.
> Ne mélangez pas ces objets avec `nkFree` ; voir « Pièges transverses » en fin de page.

---

## Identifier un type sans le connaître : `NkTypeRegistry`

Le premier problème d'un ECS est paradoxal : il doit manipuler des composants **sans savoir leur
type** à la compilation. Un système d'itération générique reçoit « le tableau du composant n°7 » et
doit savoir le construire, le détruire, le déplacer — sans jamais écrire `Position` ou `Velocity`
dans son code. La solution est le **registre de types** : à chaque type `T` rencontré, on attribue un
`NkComponentId` entier unique, et on stocke à côté une fiche `ComponentMeta` qui contient sa taille,
son alignement, et **cinq pointeurs de fonction** (construire, détruire, déplacer, copier, échanger)
générés à la compilation pour `T` mais exposés sous une signature `void*` indépendante de `T`.

Concrètement, `NkIdOf<T>()` est tout ce dont on a besoin : il enregistre `T` si c'est la première
fois (lazy, thread-safe) et renvoie son identifiant. L'enregistrement est **automatique** — pas
besoin de déclarer ses composants ailleurs.

```cpp
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };

NkComponentId pid = NkIdOf<Position>();   // attribue (ou relit) l'ID de Position
const ComponentMeta* m = NkMetaOf<Velocity>();   // fiche : size, align, fn ptrs…
```

Ce n'est **pas** un `typeid` runtime classique : l'ID est un petit entier dense (0…255), pas un
`std::type_index`, ce qui permet de l'utiliser comme indice de tableau et comme bit dans un masque.
La macro optionnelle `NK_COMPONENT(T)`, placée après la struct, n'ajoute qu'une chose : un **nom
lisible** (`SetName`) pour le débogage. L'enregistrement, lui, marche sans elle.

> **En résumé.** `NkTypeRegistry` attribue un `NkComponentId` dense par type et stocke sa
> `ComponentMeta` (taille, alignement, 5 fonctions de cycle de vie typées `void*`). `NkIdOf<T>()`
> suffit (lazy, thread-safe) ; `NK_COMPONENT(T)` n'ajoute qu'un nom. Limite dure : **256 types**.

---

## Une signature en un masque : `NkComponentMask`

Pour décrire « cette entité a Position + Vitesse + Rendu », on ne stocke pas une liste : on allume
des **bits** dans un masque de 256 bits (`NkComponentMask`, quatre mots de 64 bits). La signature
d'une entité, la requête d'un système (« je veux tout ce qui a *au moins* ces composants »), un
filtre d'exclusion — tout devient une opération bit-à-bit, donc quasi gratuite. `ContainsAll` teste
le « contient au moins » d'une requête ; `HasAny` teste un recoupement (utile pour l'exclusion) ;
`Hash` sert à indexer un archétype par sa signature.

```cpp
NkComponentMask sig;
sig.Set(NkIdOf<Position>());
sig.Set(NkIdOf<Velocity>());
// sig identifie désormais l'archétype « Position + Vitesse »
```

> **En résumé.** `NkComponentMask` = bitset fixe de 256 bits. Une signature, une requête, un filtre
> sont des masques ; `ContainsAll`/`HasAny` répondent aux requêtes en `O(4 mots)`.

---

## Un type, un tableau contigu : `NkComponentPool`

Le pool est l'unité de stockage atomique : **un seul type de composant**, rangé dans un bloc
contigu aligné, exactement comme un `NkVector` — mais piloté par la `ComponentMeta` (les pointeurs de
fonction) plutôt que par le type statique. Il croît géométriquement (×1.5), construit/détruit ses
éléments via les fonctions de la meta, et offre l'opération clé d'un ECS : le **swap-remove** en
`O(1)`. Retirer l'élément `i` ne décale rien : on échange `i` avec le dernier, on détruit le dernier,
on décrémente la taille. L'ordre change, mais comme l'ECS retrouve les entités par leur archétype et
leur ligne (et non par leur position absolue), cela ne pose pas de problème.

Ce n'est **pas** un conteneur grand public : on ne crée pas de `NkComponentPool` à la main, c'est
l'archétype qui en gère un par type. Les **tags** (composants vides, `std::is_empty_v`) sont un cas
spécial : ils n'allouent **jamais** de mémoire — `At()` renvoie `nullptr`, mais `Size()` augmente
quand même, ce qui suffit à « marquer » une entité sans coût.

> **En résumé.** `NkComponentPool` = un type, mémoire contiguë alignée, croissance ×1.5, swap-remove
> `O(1)`. Move-only. Les tags ne stockent rien (`At()=nullptr`, `Size()` croît). Usage interne.

---

## Le groupe homogène : `NkArchetype`

L'archétype rassemble **toutes les entités de signature identique** et leur stockage : une
`NkComponentPool` par type présent, plus un tableau parallèle des `NkEntityId` (pour savoir quelle
entité occupe chaque ligne). Une entité y occupe une **ligne** (`row`) : sa Position est à
`positionPool[row]`, sa Vitesse à `velocityPool[row]`, son ID à `entityIds[row]`. Ajouter une entité,
c'est pousser une valeur par défaut dans chaque pool ; la retirer, c'est un swap-remove dans chacun.

Le moment délicat est la **transition** : quand on ajoute un composant à une entité, sa signature
change, donc elle doit **changer d'archétype**. NKECS fait ça en deux temps : `MigrateFrom` déplace
les composants communs vers le nouvel archétype, puis — **impératif** — `RemoveEntity` les retire de
l'ancien. Oublier la seconde étape laisse un doublon.

```cpp
// requête : itérer toutes les entités « Position + Vitesse »
NkArchetype* a = graph.Get(id);
Position* pos = a->ComponentData<Position>();   // tableau brut SoA
Velocity* vel = a->ComponentData<Velocity>();
for (uint32 i = 0; i < a->Count(); ++i)
    pos[i] = { pos[i].x + vel[i].x, /* … */ };  // boucle chaude, contiguë
```

`ComponentData<T>()` donne le **pointeur brut** du tableau d'un composant — c'est lui qui rend les
systèmes rapides (parcours linéaire, vectorisable). Attention : `NkArchetype` est **gros** (256 pools
inline + trois tableaux de 256) ; il est **move-only**, on ne le copie jamais.

> **En résumé.** `NkArchetype` = entités de même signature, SoA (un pool par type) + tableau des
> `NkEntityId` parallèle. `ComponentData<T>()` pour la boucle chaude. Transition = `MigrateFrom`
> **puis** `RemoveEntity` sur la source. Gros, move-only, interne.

---

## Retrouver une entité, détecter les fantômes : `NkEntityIndex`

Un `NkEntityId` stocké dans du gameplay peut survivre à l'entité elle-même. Comment éviter de
déréférencer une entité morte ? `NkEntityId` embarque une **génération** en plus de son index : à
chaque `Free`, la génération de ce slot est incrémentée, donc tous les anciens ID portant l'ancienne
génération deviennent **détectables comme morts** par `IsAlive`. C'est la parade classique au
*dangling* (génération 0 = invalide).

`NkEntityIndex` est la table globale entité → (archétype, ligne) : `Allocate` crée une entité
(réutilisant un slot libéré via une free-list, ou en étendant), `Free` la tue et bump la génération,
`GetRecord` / `SetRecord` lisent et mettent à jour sa position. Réflexe à acquérir : **toujours
`IsAlive` avant de déréférencer** un ID conservé.

> **En résumé.** `NkEntityIndex` mappe entité → (archétype, ligne) en `O(1)` et gère les
> **générations** anti-dangling. `IsAlive` détecte un ID périmé. Limites : ~1M entités, free-list de
> 256 slots (au-delà, l'index libéré est perdu).

---

## Créer, trouver, transiter : `NkArchetypeGraph`

Le graphe est le **chef d'orchestre** des archétypes : il les crée à la demande, les retrouve par
signature, et — c'est tout son intérêt — **met en cache les transitions**. Demander « l'archétype
obtenu en ajoutant le composant *c* à l'archétype *A* » est l'opération la plus fréquente d'un ECS ;
plutôt que de recalculer le masque à chaque fois, le graphe garde une arête `(A, c, add) → B` dans
une table de hachage maison. La première fois coûte un recalcul ; ensuite c'est `O(1)`.

C'est aussi lui qui **pilote les requêtes** : `ForEachMatching(required, excluded, fn)` balaie tous
les archétypes, garde ceux qui contiennent `required` et n'ont rien de `excluded`, et appelle `fn`
sur chacun. C'est le cœur de l'itération d'un système.

```cpp
graph.ForEachMatching(required, excluded, [](NkArchetype* a) {
    auto* pos = a->ComponentData<Position>();
    for (uint32 i = 0; i < a->Count(); ++i) { /* traiter pos[i] */ }
});
```

> **En résumé.** `NkArchetypeGraph` crée/cache les archétypes (un par signature) et **mémorise les
> transitions** add/remove (cache d'arêtes `O(1)`). `ForEachMatching` est le moteur des requêtes.
> L'`NkArchetypeId` est stable et **égal à l'indice**. Limite : 4096 archétypes. Non copiable.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par header. Complexités et `noexcept` indiqués quand ils
éclairent l'usage.

### `NkECSDefines.h` — types & constantes fondamentaux

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Identifiants | `NkComponentId` (uint32), `kInvalidComponentId` | ID dense de type ; sentinelle `0xFFFFFFFF`. |
| Identifiants | `NkArchetypeId` (uint32), `kInvalidArchetypeId` | ID d'archétype (= indice) ; sentinelle. |
| Limites | `kMaxComponentTypes` (256), `kMaxArchetypes` (4096), `kMaxEntities` (~1M), `kChunkSize`, `kMaxSystemsPerGroup` | Dimensionnements **durs**. |
| Macros | `NKECS_NODISCARD`, `NKECS_INLINE`, `NKECS_FORCEINLINE`, `NKECS_UNREACHABLE()`, `NKECS_ASSERT(cond)` | Outillage ; **`ASSERT` log seulement, sans abort**. |
| Hachage | `detail::FNV1a`, `detail::FNV1aBytes`, `kFNVBasis`, `kFNVPrime` | FNV-1a `constexpr` (noms, masques). |
| Entité | `struct NkEntityId` | Index + génération, anti-dangling. |
| Vue | `template<T> struct NkSpan` | Vue non-possédante `{data, size}`. |

`NkEntityId` : champs `index`/`gen` ; `IsValid()`, `Pack()`/`Unpack()` `[O(1)]`, `Invalid()`,
comparaisons `== != <` (sur `Pack()`), tous `constexpr noexcept`.
`NkSpan<T>` : `data`/`size`, ctors (dont déduction depuis `T[N]`), `begin`/`end`/`operator[]`
(non vérifié)/`empty`, tous `constexpr noexcept`.

### `NkTypeRegistry.h` — registre & métadonnées

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Métadonnées | `struct ComponentMeta` | `id`/`name`/`typeHash`/`size`/`align`/`isZeroSize` + 5 fn ptrs cycle de vie. |
| Générateurs | `detail::DefaultConstruct/Destruct/MoveConstruct/CopyConstruct/SwapAt<T>` | Fonctions de cycle de vie générées par `T` (no-op/memcpy si trivial). |
| Registre | `NkTypeRegistry::Global()` | Singleton thread-safe. |
| Registre | `Register<T>()`, `IdOf<T>()` | Enregistre (lazy) et renvoie l'ID `[O(1)` amorti`]`. |
| Registre | `IdOfConst<T>() const`, `Get(id) const`, `Count() const`, `SetName(id, name)` | Lecture seule / fiche / compte / renommage. |
| Masque | `struct NkComponentMask` | Bitset 256 bits : `Set`/`Clear`/`Has` `[O(1)]`, `ContainsAll`/`HasAny`/`IsEmpty`/`Count` `[O(4)]`, `Hash`, `ForEach`, `==`/`!=`. |
| Helpers | `NkIdOf<T>()`, `NkMetaOf<T>()` | Raccourcis sur le singleton. |
| Macro | `NK_COMPONENT(Type)` | Auto-enregistre + donne un **nom lisible** (optionnel). |

### `NkComponentPool.h` — pool SoA d'un type

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkComponentPool()`, `NkComponentPool(meta)`, move ctor/=, dtor | Vide / lié à une meta / **move-only** / libère. |
| Accès | `Size`, `Capacity`, `Empty`, `Meta`, `At(i)` `[O(1)]`, `AtTyped<T>(i)`, `Data<T>()` | Compte / capacité / vide ? / fiche / élément générique / typé / tableau brut. |
| Modification | `PushDefault` `[O(1)*]`, `PushCopy`, `PushMove`, `SwapRemove(i)` `[O(1)]`, `MoveFrom`, `Clear`, `Shrink`, `Reserve` | Ajouts ; **swap-remove** (change l'ordre, renvoie `last` ou `kSwapRemoveNoSwap`) ; migration ; vidage ; ajustement capacité. |

### `NkArchetype.h` — archétype & index d'entités

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Position | `struct NkEntityRecord` | `{archetypeId, row}` : où vit une entité. |
| Archétype | `NkArchetype(id, mask)`, move ctor/= | Crée un pool par bit du masque ; **move-only**. |
| Accès | `Id`, `Mask`, `Count`, `Empty`, `Has(cid)`, `GetPool(cid)` `[O(1)]`, `GetEntity(row)`, `Entities()` | Identité / signature / taille / pool d'un type / entité d'une ligne / vue. |
| Mutation | `AddEntity(id)` `[O(1)*]`, `RemoveEntity(row)` `[O(pools)]`, `MigrateFrom(src, row, id)`, `ForEachRow(fn)` | Ajoute/retire une ligne ; migre (**sans** supprimer la source) ; itère les lignes. |
| Typé | `GetComponent<T>(row)`, `ComponentData<T>()`, `SetComponent<T>(row, val)` | Accès à un composant d'une ligne / **tableau brut SoA** / écriture. |
| Index | `class NkEntityIndex` : `Allocate`, `Free`, `IsAlive`, `GetRecord`, `SetRecord`, `AliveCount` | Table globale entité → position + **générations**. |

### `NkArchetypeGraph.h` — graphe & transitions

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkArchetypeGraph()`, dtor | Crée l'archétype vide (id 0) ; non copiable. |
| Accès | `Get(id)` `[O(1)]`, `EmptyId`, `Count` | Archétype par ID (= indice) / archétype vide / compte. |
| Transitions | `GetOrCreate(mask)`, `AddComponent(src, cid)`, `RemoveComponent(src, cid)` | Trouve/crée par signature ; transitions **cachées** `[O(1)` moyen`]`. |
| Requêtes | `ForEachMatching(required, excluded, fn)` | Moteur d'itération `[O(archétypes × 4)]`. |

---

## Référence complète

Cette section reprend chaque élément en détail : son comportement, sa complexité, et ses usages dans
les différents domaines du moteur. Le trivial est expédié ; ce qui structure les performances de
l'ECS est traité **à fond**.

### `NkEntityId` — l'identité forte

Un `NkEntityId` est une paire `{index, gen}` packée dans 64 bits. L'`index` localise le slot dans
`NkEntityIndex` ; la `gen` (génération) le rend **infalsifiable dans le temps** : quand le slot est
réutilisé pour une nouvelle entité, sa génération change, donc tout ancien ID portant l'ancienne
génération échoue à `IsAlive`. `Pack()`/`Unpack()` sérialisent l'ID en un `uint64` (réseau, fichier,
clé de table) ; la comparaison `<` (sur `Pack()`) permet de l'utiliser comme clé triée. `Invalid()`
fournit la sentinelle. Tout est `constexpr noexcept` — aucun coût runtime au-delà de quelques
décalages de bits.

- **Gameplay / IA** — référencer une cible, un propriétaire, un parent sans pointeur fragile :
  l'agent garde l'`NkEntityId` du joueur, et `IsAlive` lui dit si la cible existe encore.
- **IO / réseau** — `Pack()` donne une clé compacte sérialisable ; le pair distant la `Unpack` pour
  retrouver l'entité.
- **Outils / éditeur** — une sélection, un *undo* qui réfère des entités survivent à leur
  suppression et la détectent proprement.

### `NkSpan` — la vue non-possédante

Une simple paire `{T* data, usize size}` qui ne possède rien : elle décrit une tranche d'un tableau
existant (par exemple `NkArchetype::Entities()`). `begin`/`end` la rendent compatible
`range-based for`, `operator[]` est **non vérifié** (rapide, hors-bornes = comportement indéfini), le
ctor template déduit la taille d'un tableau C. C'est l'équivalent local du *span*, utilisé partout où
l'on veut passer « ce bloc d'éléments » sans copier ni transférer la propriété.

### `ComponentMeta` et les générateurs de cycle de vie

`ComponentMeta` est la **carte d'identité runtime d'un type** : son `id`, son `name`, un `typeHash`
(FNV-1a du nom décoré), sa `size`, son `align`, le drapeau `isZeroSize` (tag), et **cinq pointeurs de
fonction** — construire par défaut, détruire, déplacer, copier, échanger. Ces fonctions sont générées
à la compilation (`detail::DefaultConstruct<T>` etc.) mais exposées sous une signature `void*`, ce
qui permet au pool et à l'archétype de gérer **n'importe quel type sans le connaître**. Le `if
constexpr` les optimise : un type trivialement copiable se déplace par `memcpy`, un type vide ne fait
rien, un type trivialement destructible ne paie pas de destructeur.

- **ECS** — c'est tout le secret du stockage type-effacé : un système générique reçoit une
  `ComponentMeta*` et sait fabriquer/détruire ses éléments.
- **Sérialisation / outils** — `name`, `size`, `align`, `typeHash` permettent à un inspecteur ou à un
  sérialiseur de raisonner sur un composant sans en avoir le type C++ sous la main.

### `NkTypeRegistry` — l'attribution des identifiants

Le registre est un **singleton** (`Global()`, Meyers, thread-safe). Son rôle : donner à chaque type
un `NkComponentId` dense et unique, et conserver sa `ComponentMeta`. `Register<T>()` (et son alias
`IdOf<T>()`) est l'entrée principale : il enregistre `T` si absent — strip const/ref, alloue l'ID via
un compteur atomique, remplit la meta, mémorise tout — puis renvoie l'ID. Le chemin rapide est sans
verrou (lecture atomique *acquire*) ; seul le premier enregistrement prend le verrou interne
(*double-checked locking*). C'est donc `O(1)` amorti et sûr en multithread.

`IdOfConst<T>()` est la variante **lecture seule** : elle ne crée rien et renvoie
`kInvalidComponentId` si `T` n'a jamais été vu — utile pour interroger sans provoquer
d'enregistrement. `Get(id)` renvoie la fiche (ou `nullptr` hors plage), `Count()` le nombre de types,
`SetName(id, name)` écrase le nom (débogage).

- **ECS** — l'ossature : tout passe par les IDs denses (indices de tableau, bits de masque).
- **Threading** — l'enregistrement est lui-même thread-safe (atomics + verrou interne), donc deux
  systèmes parallèles peuvent toucher un type neuf sans course.
- **Outils / éditeur** — `Count()` et `Get()` listent les types de composants connus pour peupler un
  menu « Ajouter un composant ».

Le piège est la **limite dure de 256 types** : au-delà, `NKECS_ASSERT` se déclenche — mais comme
l'assert *log seulement, sans abort*, un dépassement peut continuer silencieusement. À surveiller dans
un gros projet.

### `NkComponentMask` — la signature en bits

Le masque encode une signature (ensemble de composants) sur 256 bits = 4 mots de 64. `Set`/`Clear`/
`Has` manipulent un bit en `O(1)` (bornés, donc sûrs hors plage). Les opérations d'ensemble sont en
`O(4 mots)` : `ContainsAll(other)` (ce masque est-il un sur-ensemble — le test « contient au moins »
d'une requête), `HasAny(other)` (intersection non vide — le test d'exclusion), `IsEmpty`. `Count()`
fait un popcount matériel, `ForEach(fn)` itère uniquement les bits actifs (via `ctz` + effacement du
bit bas), `Hash()` (FNV sur les mots) sert de clé pour indexer un archétype.

- **ECS / requêtes** — une requête système est un couple `(requis, exclus)` de masques ;
  `ContainsAll` et `HasAny` filtrent les archétypes en quelques instructions.
- **Gameplay** — tester « cette entité a-t-elle le composant *Stunned* » se résout en un `Has`.

### `NkComponentPool` — un type, contigu

Le pool stocke **un seul type**, contigu, aligné, piloté par la `ComponentMeta`. Il croît de ×1.5
(`GrowCapacity` part de 8) ; `Reserve` pré-alloue, `Shrink` rend l'excédent, `Clear` détruit tout en
gardant le buffer. La construction passe par `PushDefault` (valeur par défaut), `PushCopy`,
`PushMove`. L'opération signature est **`SwapRemove(i)`** : `O(1)`, elle échange l'élément `i` avec le
dernier (via `swapAt`), détruit le dernier, décrémente — et **renvoie l'indice déplacé** (`last`) ou
`kSwapRemoveNoSwap` si l'élément était déjà en dernière position. `MoveFrom` déplace un élément depuis
un autre pool de même type (pour la migration d'archétype), **sans** retirer de la source.

`At(i)` donne l'adresse générique d'un élément, `AtTyped<T>(i)` la version castée, `Data<T>()` le
**pointeur brut du tableau** — c'est ce dernier qui rend les systèmes vectorisables.

- **ECS** — chaque pool est une colonne du SoA d'un archétype ; `Data<T>()` alimente la boucle
  chaude.
- **Rendu / physique / animation** — un système de transformation lit `Data<Transform>()` et balaie
  le tableau d'un trait, ami du cache (même bénéfice que `NkVector::Data()`).
- **Mémoire** — les **tags** (composants vides) sont gratuits : aucune allocation, `At()=nullptr`,
  mais `Size()` croît, ce qui suffit à marquer une entité (« Sélectionné », « Mort »).

Pièges : le pool est **move-only** (copie supprimée) ; il utilise l'allocateur **aligné CRT**
(`_aligned_malloc`/`std::aligned_alloc`), pas NKMemory — ne le libérez jamais via `nkFree`.

### `NkEntityRecord` et `NkArchetype` — le groupe homogène

`NkEntityRecord` est juste `{archetypeId, row}` : la position d'une entité vivante. `NkArchetype`
l'exploite pour ranger toutes les entités d'une signature : à la construction, il crée **une
`NkComponentPool` par bit** du masque (récupérant chaque `ComponentMeta` via le registre), et tient
un tableau parallèle des `NkEntityId`. Une entité = une **ligne** (`row`) commune à tous les pools.

`AddEntity(id)` pousse une ligne par défaut dans chaque pool (`O(1)` amorti) et renvoie son `row`.
`RemoveEntity(row)` fait un swap-remove dans chaque pool et renvoie l'entité qui a été **déplacée**
vers `row` (ou `Invalid` si c'était la dernière) — l'appelant doit mettre à jour le record de cette
entité déplacée. `MigrateFrom(src, srcRow, id)` alloue une ligne ici et, pour chaque pool, déplace le
composant depuis `src` s'il est commun, sinon le construit par défaut — **mais ne retire rien de
`src`**. D'où l'idiome obligatoire de transition en deux temps (`MigrateFrom` *puis*
`src.RemoveEntity`).

Les accès typés sont le cœur du gameplay des systèmes : `GetComponent<T>(row)` (un composant d'une
entité, `nullptr` si absent), `SetComponent<T>(row, val)` (écriture), et surtout
`ComponentData<T>()` — le **tableau brut** d'un trait, pour balayer l'archétype à plein débit.

- **ECS / systèmes** — un système de mouvement récupère `ComponentData<Position>()` et
  `ComponentData<Velocity>()` et les parcourt en parallèle, sans saut mémoire.
- **Rendu** — un système de culling lit `ComponentData<Bounds>()` ; un *batcher* lit
  `ComponentData<MeshRef>()`.
- **Physique / collision** — les corps d'un même type (sphères, AABB) sont contigus, idéal pour la
  broad-phase.
- **Animation / audio / UI** — même schéma : tout sous-système itère ses composants d'un seul trait.

Pièges majeurs : `NkArchetype` est **volumineux** (256 pools inline + 3 tableaux de 256) → move-only,
jamais copié ; la transition est en **deux temps** ; et son destructeur **ne libère pas
`mEntityIds`** (le tableau d'IDs alloué par `new[]`) — fuite documentée telle quelle.

### `NkEntityIndex` — la table globale et les générations

L'index est la table entité → position, avec gestion des générations. Chaque slot est une `Entry`
`{record, gen, alive}`. `Allocate()` rend un `NkEntityId` : il réutilise un slot libéré (via une
**free-list LIFO** de 256, en conservant la génération) ou étend la table (génération initiale 1) ;
il renvoie `Invalid` si la limite (~1M) est atteinte. `Free(id)` marque mort, **incrémente la
génération** (invalidant tous les anciens ID), réinitialise le record et rend le slot à la free-list.
`IsAlive(id)` vérifie validité + plage + `alive` + correspondance de génération, en `O(1)`.
`GetRecord`/`SetRecord` lisent et mettent à jour la position ; `AliveCount()` donne le compte vivant.

- **ECS** — le point d'entrée de toute résolution d'`NkEntityId` vers son archétype et sa ligne.
- **Gameplay / IA** — `IsAlive` avant chaque déréférencement d'une cible mémorisée évite le
  *dangling* sans coût.
- **Outils / éditeur** — une sélection ou une pile *undo* peuvent garder des IDs et tester proprement
  leur survie.

Pièges : la **génération 0 est invalide** ; la **free-list est bornée à 256** — au-delà de 256 slots
libres en attente, l'index libéré est **perdu** (correct mais sous-optimal) ; et `Allocate` échoue à
~1M entités.

### `NkArchetypeGraph` — le graphe et le cache de transitions

Le graphe possède **tous** les archétypes (`new NkArchetype` à la création, `delete` à la
destruction), avec un invariant simple : l'`NkArchetypeId` **est l'indice** dans le tableau interne,
donc `Get(id)` est un accès direct `O(1)` et les IDs sont stables. À la construction il crée
l'archétype vide (id 0, `EmptyId()`).

`GetOrCreate(mask)` trouve l'archétype d'une signature ou le crée (cache Mask→Id, sondage linéaire,
FNV). Les deux opérations vedettes sont les transitions : `AddComponent(src, cid)` et
`RemoveComponent(src, cid)` renvoient l'archétype obtenu en ajoutant/retirant un composant — la
première fois en recalculant le masque, **ensuite via un cache d'arêtes** `(src, cid, op) → dst` en
`O(1)` moyen. C'est ce cache qui rend les transitions massives (ajouter un composant à des milliers
d'entités) bon marché.

`ForEachMatching(required, excluded, fn)` est le **moteur des requêtes** : il balaie les archétypes
non vides, garde ceux dont le masque `ContainsAll(required)` et qui n'ont aucun bit de `excluded`, et
appelle `fn(NkArchetype*)`. Coût `O(archétypes × 4 mots)` — typiquement quelques dizaines d'archétypes
testés, puis itération SoA serrée sur les retenus.

- **ECS / systèmes** — chaque système exécute une requête via `ForEachMatching` puis traite les
  archétypes retenus avec `ComponentData<T>()`.
- **Gameplay** — ajouter/retirer un composant (« étourdir », « équiper ») passe par
  `AddComponent`/`RemoveComponent`, accéléré par le cache d'arêtes.
- **Outils / éditeur** — `Count()` et `Get()` permettent d'inspecter la topologie des archétypes
  (combien, lesquels).

Pièges : **4096 archétypes maximum** (assert) ; les tables de cache (puissances de 2) sont indexées
par `& (taille-1)` et ne devraient jamais être pleines (assert sinon) ; le graphe est **non
copiable**.

### L'idiome de transition complet

Ajouter un composant à une entité existante combine les cinq types de cette page. C'est la séquence
canonique, à respecter dans l'ordre :

```cpp
NkEntityRecord* rec = entityIndex.GetRecord(entity);
NkArchetypeId srcId  = rec->archetypeId;
uint32        srcRow = rec->row;

NkArchetypeId dstId  = graph.AddComponent(srcId, NkIdOf<Velocity>());   // cache d'arêtes
uint32        newRow = graph.Get(dstId)->MigrateFrom(*graph.Get(srcId), srcRow, entity);
graph.Get(srcId)->RemoveEntity(srcRow);          // IMPÉRATIF après MigrateFrom
entityIndex.SetRecord(entity, dstId, newRow);    // l'entité vit désormais ailleurs
```

Oublier `RemoveEntity` laisse un doublon dans l'ancien archétype ; oublier `SetRecord` laisse l'index
pointer vers l'ancienne position.

---

## Pièges transverses

- **Allocation hétérogène.** `new`/`delete[]` (graphe, archétype, index) et
  `_aligned_malloc`/`std::aligned_alloc`/`std::free` (pools) — **pas NKMemory**, contrairement à la
  règle projet. Ne libérez jamais ces objets avec `nkFree` ; ne mélangez pas les deux mondes.
- **Limites dures.** 256 types, 4096 archétypes, ~1M entités, free-list de 256 — au-delà, assert ou
  perte silencieuse.
- **`NKECS_ASSERT` ne fait que logger** (pas d'abort) : un dépassement de capacité peut continuer en
  silence. Surveillez les logs.
- **Tags** (`isZeroSize`) : aucune mémoire, `At()=nullptr`, mais `Size()` augmente quand même.
- **Move-only partout** : archétype, pool, graphe ne se copient pas.
- **Deux pièges de cycle de vie** : `NkArchetype::~NkArchetype` ne libère pas `mEntityIds` ;
  `MigrateFrom` n'efface pas la source (faire `RemoveEntity` ensuite).

---

### Exemple récapitulatif

```cpp
#include "NKECS/NKECS.h"
using namespace nkentseu::ecs;

struct Position { float x, y, z; };
struct Velocity { float x, y, z; };

NkArchetypeGraph graph;
NkEntityIndex    index;

// 1. Créer une entité « Position + Vitesse »
NkComponentMask sig;
sig.Set(NkIdOf<Position>());
sig.Set(NkIdOf<Velocity>());
NkArchetypeId   aid = graph.GetOrCreate(sig);
NkEntityId      e   = index.Allocate();
uint32          row = graph.Get(aid)->AddEntity(e);
index.SetRecord(e, aid, row);
graph.Get(aid)->SetComponent<Position>(row, { 0, 0, 0 });
graph.Get(aid)->SetComponent<Velocity>(row, { 1, 0, 0 });

// 2. Système de mouvement : itérer tous les archétypes « Position + Vitesse »
NkComponentMask required = sig, excluded;     // rien à exclure
graph.ForEachMatching(required, excluded, [](NkArchetype* a) {
    Position* p = a->ComponentData<Position>();   // tableaux bruts SoA
    Velocity* v = a->ComponentData<Velocity>();
    for (uint32 i = 0; i < a->Count(); ++i) {
        p[i].x += v[i].x; p[i].y += v[i].y; p[i].z += v[i].z;
    }
});

// 3. Détruire proprement
if (index.IsAlive(e)) {
    NkEntityRecord* r = index.GetRecord(e);
    graph.Get(r->archetypeId)->RemoveEntity(r->row);
    index.Free(e);   // bump la génération : tout ancien NkEntityId devient « mort »
}
```

---

[← Index NKECS](README.md) · [Récap NKECS](../NKECS.md) · [Couche Runtime](../README.md)
