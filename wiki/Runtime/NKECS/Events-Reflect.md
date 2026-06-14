# Événements, réflexion et sérialisation

> Couche **Runtime** · NKECS · La famille « Events-Reflect » : le **bus d'événements gameplay**
> (`NkGameplayEventBus`), le **système de réflexion** des types (`NkReflect`), et les **helpers de
> sérialisation JSON** (`NkJsonSerialization`).

Un moteur ECS ne se réduit pas à des composants et des systèmes. Trois besoins transversaux
reviennent sans cesse : **faire communiquer** des systèmes sans qu'ils se connaissent (un événement
« le joueur est mort » déclenche le son, l'UI, le score), **décrire les types** pour qu'un éditeur,
un inspecteur ou un sérialiseur sache ce qu'ils contiennent, et **écrire/relire** ces données sur
disque. NKECS répond à ces trois besoins avec trois headers distincts, dans trois namespaces
distincts, chacun avec sa propre philosophie — et c'est précisément là qu'il faut être prudent.

Une mise en garde avant tout : **un seul de ces trois headers est réellement autonome.**
`NkGameplayEventBus.h` est zéro-STL et compile seul. `NkReflect.h` utilise une STL *partielle*
(`std::strcmp`, `std::declval`, `offsetof`). `NkJsonSerialization.h`, lui, est **STL-full** et
**ne compile pas en standalone dans NKECS** : il inclut `<nlohmann/json.hpp>`, `<fstream>`, et
surtout deux headers `NkPrefab.h` / `NkBlueprint.h` qui **n'existent pas dans ce module** (ils
vivent chez Noge). On documente sa surface telle qu'elle est, mais on ne peut pas la compiler ici.

- **Namespaces** : `nkentseu::ecs` · `nkentseu::ecs::reflect` · `nkentseu::ecs::serialization`
- **Headers réels** : `NKECS/Events/NkGameplayEventBus.h` · `NKECS/Reflect/NkReflect.h` ·
  `NKECS/Serialization/NkJsonSerialization.h`
- Tous incluent `NKECS/NkECSDefines.h` (fournit `NkComponentId`, `kInvalidComponentId`,
  `kMaxComponentTypes`, `NkIdOf<T>`, `NKECS_ASSERT`, les types primitifs). **Aucun header parapluie.**

---

## Le bus d'événements gameplay : `NkGameplayEventBus`

Quand le joueur meurt, plusieurs systèmes doivent réagir — l'audio joue un jingle, l'UI affiche un
écran, le score s'enregistre — sans que le système de combat ait à connaître chacun d'eux. C'est
exactement le rôle d'un **bus d'événements** : un point de rendez-vous où l'on **émet** des
événements typés et où d'autres s'**abonnent**. L'émetteur ignore qui écoute, l'écouteur ignore qui
émet ; ils ne partagent qu'un *type d'événement*.

Premier piège à dissiper : **`NkGameplayEventBus` n'est PAS le `NkEventBus` du Core moteur.** Ce
sont deux choses différentes. Un alias de compatibilité `using NkEventBus = NkGameplayEventBus;`
existe encore, mais il est **déprécié** — il ne sert qu'à la migration. Dans du code neuf, on nomme
le type complet.

Deuxième point : ce bus **n'est pas un singleton**. On l'instancie soi-même, typiquement dans le
World ou le GameManager, et on le passe là où c'est nécessaire. Sous le capot, il gère
dynamiquement **un canal (`NkEventChannel<T>`) par type d'événement** rencontré, créé au premier
abonnement à ce type.

```cpp
NkGameplayEventBus bus;                 // pas de singleton : on possède l'instance

auto id = bus.Subscribe<PlayerDied>([](const PlayerDied& e) {
    audio.Play("death_jingle");
});
// ... plus tard, ailleurs, sans rien savoir de l'abonné :
bus.Emit(PlayerDied{ /* ... */ });      // synchrone : le handler s'exécute ici
```

`Subscribe` renvoie un `SubscriptionId` (un `uint64` qui encode `(TypeId<T> << 32) | HandleId`) —
on le garde pour pouvoir se désabonner. C'est le moment d'introduire la distinction la plus
importante du module : **émettre tout de suite ou différer.**

> **En résumé.** `NkGameplayEventBus` (≠ `NkEventBus` Core) est un bus typé, non-singleton, qui crée
> un canal par type d'événement. On `Subscribe<T>` (→ `SubscriptionId`), on `Emit<T>` (synchrone) ou
> on `Queue<T>` (différé). L'alias `NkEventBus` est déprécié.

### Émettre maintenant ou plus tard : `Emit` vs `Queue`/`Drain`

`Emit(event)` est **synchrone** : il parcourt les handlers du canal et les appelle *immédiatement*,
dans le thread appelant. Simple, mais dangereux **pendant l'itération ECS** : un handler qui crée ou
détruit des entités peut invalider la boucle en cours. C'est là qu'intervient le couple
`Queue`/`Drain`.

`Queue(event)` ne fait que **mettre l'événement en file** (par copie ou par déplacement) ; rien ne
s'exécute. Plus tard, en un point sûr (typiquement la fin de frame), `Drain()` vide *toutes* les
files et appelle alors les handlers. C'est le **pattern recommandé** :

```cpp
// Pendant la mise à jour d'un système (itération ECS en cours) :
bus.Queue(EntityHit{ target, damage });   // ne s'exécute pas tout de suite

// ... fin de frame, hors de toute itération :
world.FlushDeferred();
bus.Drain();                               // maintenant, tous les handlers tournent
```

`Drain` est aussi conçu pour être **réentrant-safe** : il échange d'abord la file localement avant
de dispatcher, de sorte qu'un handler qui re-`Queue` un événement ne perturbe pas la passe en cours
(le nouvel événement partira au `Drain` suivant). `HasPending()` permet de savoir s'il reste quelque
chose à drainer.

> **En résumé.** `Emit` = synchrone, ici et maintenant — à éviter pendant une itération ECS.
> `Queue` + `Drain` = différé : on accumule en sécurité, on dispatche en fin de frame. `Drain` est
> réentrant-safe (swap interne). `HasPending()` indique s'il reste des événements en file.

### Se désabonner, et les limites à connaître

`Unsubscribe<T>(id)` retire un abonnement — mais attention, **la suppression est différée** :
l'abonnement est seulement *marqué inactif* (`active = false`), et la purge réelle n'a lieu qu'au
prochain `Drain()` de ce canal. Conséquence concrète : un handler venant d'être désabonné **peut
encore être appelé une fois** si une boucle d'émission est déjà en cours. La bonne hygiène est de
se désabonner dans le destructeur du propriétaire (RAII) pour éviter les *dangling callbacks*.

Deux plafonds matériels sont à garder en tête, car ils sont **durs** :

- **256 types d'événements distincts** au maximum (`kMaxChannels = 256`). Le bus stocke ses canaux
  dans un tableau fixe ; au-delà, l'assertion `NKECS_ASSERT(mCount < kMaxChannels)` saute.
- **Pas de heap par canal**, mais chaque entrée réserve un `storage[]` *inline* surdimensionné
  (×4 de `sizeof(NkEventChannel<int>)`). Un événement `T` **trop gros** fait échouer un
  `static_assert` à la compilation — préférez des événements légers (id + quelques scalaires).

> **En résumé.** `Unsubscribe` est **différé** (marque inactif, purge au prochain `Drain`) → un
> handler peut être appelé une fois de trop. Plafond dur de **256 types**, et stockage canal inline
> → gardez les événements **petits**. Désabonnez-vous en RAII.

---

## La réflexion des types : `NkReflect`

Un éditeur a besoin de savoir, pour un type donné, *quels champs il possède, de quel type, à quel
offset, avec quelles métadonnées* — pour afficher un inspecteur, sérialiser automatiquement,
exposer à un système de réseau ou à un blueprint. C'est le rôle de la **réflexion** : décrire un
type *à l'exécution* via des structures de données. NKECS fournit une réflexion **minimaliste mais
inspirée d'Unreal/Unity** : on décore les champs avec des macros, et un registre global les indexe.

Le cœur tient en deux structures : `NkFieldInfo` décrit **un champ**, `NkTypeInfo` décrit **un type
entier** (sa taille, son alignement, son tableau de champs, ses fonctions de cycle de vie). Un type
de champ est exprimé par l'enum `NkFieldType` (de `Bool`, `Int32`, `Float32`… jusqu'aux types
moteur `Vec3`, `Quat`, `Mat4`, `EntityId`, et aux conteneurs `Array`/`Object`). Et chaque champ
porte un **bitfield de métadonnées** 64 bits, `NkMetaFlag`, qui dit *comment* l'éditeur, le
sérialiseur, le réseau ou le blueprint doivent le traiter.

```cpp
struct Transform { NkVec3 position; NkVec3 scale; NkQuat rotation; };

NK_REFLECT_BEGIN(Transform)
    NK_FIELD_EX(position, NkFieldType::Vec3)
    NK_FIELD_EX(scale,    NkFieldType::Vec3)
    NK_FIELD_EX(rotation, NkFieldType::Quat)
NK_REFLECT_END()
```

Ce bloc fait deux choses : il construit un `NkTypeInfo` décrivant `Transform`, et il **s'enregistre
tout seul au démarrage** dans le registre global via un objet statique. Pas d'appel explicite — il
suffit que la TU contenant ces macros soit liée au binaire.

> **En résumé.** `NkReflect` décrit les types à l'exécution : `NkFieldInfo` (un champ : nom, type,
> offset, taille, métadonnées) et `NkTypeInfo` (un type : taille, alignement, champs, fonctions de
> vie). On déclare via `NK_REFLECT_BEGIN/NK_FIELD.../NK_REFLECT_END`, qui **s'auto-enregistre** au
> démarrage. STL partielle (`std::strcmp`, `offsetof`).

### Les métadonnées : `NkMetaFlag`

`NkMetaFlag` est un **bitfield 64 bits** (chaque flag vaut `1ULL << n`) qui range les intentions par
familles : la **visibilité** (`Visible`, `HideInEditor`, `ReadOnly`, `Advanced`), la
**sérialisation** (`Serialize`, `NoSerialize`, `SerializeDefault`), l'**édition** (`EditAnywhere`,
`EditDefaultsOnly`, `EditFixedSize`, `NoEdit`), le **blueprint** (`BlueprintReadWrite`,
`BlueprintReadOnly`, `BlueprintCallable`, `BlueprintPure`), le **réseau** (`Replicated`,
`RepNotify`, `RepSkipOwner`), l'**UI** (`Range`, `Password`, `Multiline`, `ColorPicker`), des
**utilitaires** (`Instanced`, `Transient`, `Duplicate`, `NeverDuplicate`), et huit flags
**réservés utilisateur** (`User0`…`User7`). Un champ combine ces flags par OU ; les helpers de
`NkFieldInfo` (`HasFlag`, `IsEditable`, `IsSerializable`) les interrogent. Par défaut, un champ
déclaré vaut `Visible | EditAnywhere | Serialize` — visible, éditable, sérialisé.

> **En résumé.** `NkMetaFlag` = un OU de bits 64 bits décrivant l'intention par familles
> (visibilité, sérialisation, édition, blueprint, réseau, UI, utilitaires, + 8 user). Défaut d'un
> champ : `Visible | EditAnywhere | Serialize`.

### Le registre : `NkReflectRegistry`

Une fois les types décrits, il faut les retrouver. `NkReflectRegistry` est le **singleton** (via
`Global()`) qui les indexe. On `Get(id)` par `NkComponentId` en **`O(1)`** (accès direct par
indice), ou `GetByName(name)` en **`O(n)`** (recherche linéaire), et `ForEach(fn)` itère tous les
types connus — exactement ce dont un panneau « ajouter un composant » d'éditeur a besoin. Une
subtilité à noter : `Register` borne sur `kMaxComponentTypes` (de NkECSDefines) tandis que le
tableau interne fait `kMaxTypes = 512` — les deux constantes peuvent différer.

> **En résumé.** `NkReflectRegistry::Global()` est le **singleton** des types. `Get(id)` en `O(1)`,
> `GetByName` en `O(n)`, `ForEach` pour parcourir. Plafond **512 types**.

### Les pièges des macros

Trois pièges importants, car ils touchent à ce que les macros **ne font pas** :

- **`NK_PROPERTY` n'extrait pas vraiment Category/Tooltip/Min/Max.** Le plumbing de macros
  (`NK_META_FIND_ARG`) est un **stub qui renvoie toujours la valeur par défaut** — l'extraction
  d'arguments nommés n'est pas réellement implémentée. Ne comptez pas dessus.
- **`serialize`/`deserialize` et les ctors/dtors de `NkTypeInfo` restent `nullptr`** : les macros ne
  les renseignent jamais. Si vous en avez besoin, câblez-les à la main.
- **`offsetof` exige un type standard-layout.** `NK_FIELD`/`NK_FIELD_EX`/`NK_ARRAY` calculent
  l'offset via `offsetof` et la taille via `std::declval` → un type non standard-layout donnera des
  offsets non fiables.

> **En résumé.** Les macros couvrent la *structure* (champs, offsets, flags), pas le *comportement* :
> `NK_PROPERTY` n'extrait pas ses arguments nommés (stub), les fonctions sérialise/ctor restent
> `nullptr`, et `offsetof` requiert un type standard-layout.

---

## La sérialisation JSON : `NkJsonSerialization`

> **Avertissement (lu dans le header).** Ce fichier est **STL-full** et **ne compile pas en
> standalone dans NKECS**. Il dépend de `nlohmann::json`, de `<fstream>`, et de types
> `NkPrefab` / `NkBlueprintGraph` / `NkValue` qui **proviennent de headers absents de NKECS**
> (`NkPrefab.h`, `NkBlueprint.h` sont chez Noge). On documente sa surface ; elle n'est pas autonome
> et son usage de `std::ofstream`/`std::ifstream` est **incohérent avec la politique zéro-STL** du
> moteur (le reste utiliserait NKFileSystem/NkFile).

Son rôle est d'écrire et relire des **prefabs** et des **graphes de blueprint** en JSON. Il fournit
pour cela des paires `to_json` / `from_json` (le protocole d'adaptation de nlohmann) pour trois
familles de types — `NkValue`, `NkPrefab`, `NkBlueprintGraph` — puis quatre fonctions fichier qui
s'appuient dessus. Toutes les fonctions fichier sont `inline`, `noexcept`, et **avalent les
exceptions** (`try/catch(...)` → `return false`), de sorte qu'un appel ne fait jamais déborder
d'exception mais renvoie simplement un booléen de succès.

```cpp
// (Compilerait uniquement dans un contexte où NkPrefab/NkBlueprintGraph sont disponibles.)
NkPrefab prefab = /* ... */;
bool ok = serialization::SaveToFile("enemy.nkprefab", prefab);   // dump JSON indenté
// ...
NkPrefab reloaded;
ok = serialization::LoadFromFile("enemy.nkprefab", reloaded);    // false si échec/exception
```

> **En résumé.** `NkJsonSerialization` sérialise Prefab/Blueprint en JSON via `to_json`/`from_json`
> + 4 fonctions fichier (`SaveToFile`/`LoadFromFile` + variantes Blueprint), toutes `inline`,
> `noexcept`, renvoyant `bool` (false sur exception). **STL-full, non autonome dans NKECS.**

---

## Aperçu de l'API

Tous les éléments publics des trois headers, par famille. Complexités entre crochets quand utile.

### `nkentseu::ecs` — `NkGameplayEventBus.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Macro | `NK_EVENT(Type)` | **No-op** purement documentaire (Doxygen), sans effet. |
| Interface | `IEventQueue` | Base type-erased : `Drain()`, `Clear()`, `Empty()` (tous `noexcept`). |
| Canal | `NkEventChannel<T>` | Canal thread-safe pour un type `T`, hérite de `IEventQueue`. |
| Canal — types | `Handler` (= `NkFunction<void(const T&)>`), `HandleId` (`uint32`), `kInvalidHandle` | Type du callback ; id d'abonnement ; sentinelle `0xFFFFFFFF`. |
| Canal — abonnement | `Subscribe(handler)` `[O(1)*]`, `Unsubscribe(id)` `[O(n)]` | Abonner (→ `HandleId`) ; désabonner (différé : marque inactif). |
| Canal — émission | `Emit(event)` `[O(n)]`, `Queue(const T&)` / `Queue(T&&)` `[O(1)*]` | Émettre synchrone ; mettre en file (copie / déplacement). |
| Canal — file | `Drain()` `[O(ev×h)]`, `Clear()`, `Empty()`, `HandlerCount()` | Vider+exécuter ; vider sans exécuter ; file vide ? ; nb handlers (actifs+inactifs). |
| Bus | `NkGameplayEventBus` | Façade non-template, **non-singleton**, un canal par type. |
| Bus — type | `SubscriptionId` (`uint64`) | Encode `(TypeId<T> << 32) \| HandleId`. |
| Bus — abonnement | `Subscribe<T>(fn)` (→ `SubscriptionId`), `Unsubscribe<T>(id)` | Abonner (crée le canal au 1er appel) ; désabonner (silencieux si absent). |
| Bus — émission | `Emit<T>(const T&)` / `Emit<T>(T&&)`, `Queue<T>(const T&)` / `Queue<T>(T&&)` | Émettre (silencieux si pas de canal) ; mettre en file (crée le canal). |
| Bus — file | `Drain()`, `Clear()`, `HasPending()` | Drainer tous les canaux (ordre de création) ; tout vider ; reste-t-il des événements ? |
| Alias | `NkEventBus` = `NkGameplayEventBus` | **Déprécié** (compat migration). |

### `nkentseu::ecs::reflect` — `NkReflect.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkFieldType` (`uint8`) | Type d'un champ : `Bool`/`Int*`/`UInt*`/`Float*`/`String`, `Vec2..4`/`Quat`/`Mat4`, `EntityId`/`ComponentId`/`ArchetypeId`, `Array`/`Object`/`Enum`/`Flags`. |
| Enum (bitfield) | `NkMetaFlag` (`uint64`) | Métadonnées par familles : visibilité, sérialisation, édition, blueprint, réseau, UI, utilitaires, + `User0..7`. |
| Struct | `NkFieldInfo` | Décrit un champ (nom, type, offset, size, count, flags, category, tooltip, min/max, nested…). |
| `NkFieldInfo` — helpers | `HasFlag`, `IsArray`, `IsEditable`, `IsSerializable` | Tester un flag ; tableau ? ; éditable ? ; sérialisable ? (tous `[[nodiscard]] const noexcept`). |
| Struct | `NkTypeInfo` | Décrit un type (componentId, name, size, align, fields, fieldCount) + fonctions de vie. |
| `NkTypeInfo` — fns vie | `CtorFn`/`DtorFn`/`CopyFn`/`MoveFn` (+ membres `defaultCtor`/`dtor`/`copyCtor`/`moveCtor`) | Pointeurs de fonction ctor/dtor/copie/déplacement (défaut `nullptr`). |
| `NkTypeInfo` — JSON | `SerializeFn`/`DeserializeFn` (+ `serialize`/`deserialize`) | Pointeurs sérialise/désérialise (défaut `nullptr`). |
| `NkTypeInfo` — helpers | `FindField(name)` `[O(fieldCount)]`, `HasField(name)` | Chercher un champ par nom (`std::strcmp`). |
| Registre | `NkReflectRegistry` | **Singleton** global des types réfléchis. |
| Registre — accès | `Global()`, `Register(info)` `[O(1)]`, `Get(id)` `[O(1)]`, `GetByName(name)` `[O(n)]`, `ForEach(fn)`, `Count()` | Singleton ; enregistrer ; lire par id / par nom ; itérer ; compter. Plafond **512**. |
| Macros | `NK_REFLECT_BEGIN(Type)` / `NK_REFLECT_END()` | Ouvre/ferme un bloc de réflexion auto-enregistré. |
| Macros — champs | `NK_FIELD`, `NK_FIELD_EX(.., type)`, `NK_FIELD_META(.., flags, cat, tip, min, max)`, `NK_ARRAY(.., count)` | Déclarer un champ (auto / typé / méta-personnalisé / tableau fixe). |
| Macros — façade | `NK_PROPERTY(field, ...)` | Style Unreal/Unity → `NK_FIELD_META` (⚠ extraction d'args = **stub**). |
| Macros — plumbing | `NK_META_*` (`EXTRACT_*`, `PACK_FLAGS`, `FLAG_IF`, `HAS_ARG`, `FIND_ARG`, tokens `ARG_*`…) | Internes ; `NK_META_FIND_ARG` renvoie **toujours le défaut** (non implémenté). |

### `nkentseu::ecs::serialization` — `NkJsonSerialization.h` (STL-full, non autonome)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `json` = `nlohmann::json` | Type JSON sous-jacent. |
| `NkValue` | `to_json`/`from_json(.., NkValue&)` | (Dé)sérialise une valeur selon `NkValueType` (Int/Float/Bool/Vec3/String/EntityId). |
| `NkPrefab` | `to_json`/`from_json(.., NkPrefab&)` | Émet/lit name, version, tagBits, layer, `components`, `children`, `blueprintPath`. |
| `NkBlueprintGraph` | `to_json`/`from_json(.., NkBlueprintGraph&)` | Émet/lit `nodes` (+ pins) et `connections`. |
| Fichier — prefab | `SaveToFile(path, prefab)`, `LoadFromFile(path, prefab)` | Écrire (`dump(4)`) / lire un prefab. `bool`, `noexcept`. |
| Fichier — blueprint | `SaveBlueprintToFile(path, graph)`, `LoadBlueprintFromFile(path, graph)` | Écrire / lire un graphe. `bool`, `noexcept`. |

---

## Référence complète

Chaque élément en détail : comportement, complexité, et usages par domaine. Les éléments triviaux
sont brefs ; les mécanismes centraux (Queue/Drain, métadonnées, auto-enregistrement) le sont à fond.

### `NK_EVENT` et `IEventQueue` — les fondations du bus

`NK_EVENT(Type)` est un **no-op** : `#define NK_EVENT(Type)` n'émet rien. Il sert uniquement de
marqueur documentaire (Doxygen) pour signaler « ce type est un événement ». Ne lui prêtez aucun
effet de compilation.

`IEventQueue` est l'interface **type-erased** qui permet au bus de ranger des canaux de types
différents dans un même tableau, sans template. Trois méthodes pures, toutes `noexcept` : `Drain()`
(exécute puis vide), `Clear()` (vide sans exécuter), `Empty()` (file vide ?). C'est le contrat
qu'implémente chaque `NkEventChannel<T>`. On ne la manipule en pratique qu'à travers le bus.

### `NkEventChannel<T>` — le canal d'un type

Le canal est l'unité de travail réelle : il gère les abonnés et la file d'un **unique** type `T`
(qui doit être copiable/movable), de façon **thread-safe** (un `NkMutex` interne protège toutes les
opérations).

- **`Subscribe(handler)`** — ajoute une entrée `{id, fn, active=true}` en fin du vecteur d'abonnés
  et renvoie `mNextId++` (les ids commencent à 1 ; 0xFFFFFFFF est la sentinelle `kInvalidHandle`).
  `O(1)` amorti. *Usage* : un système audio s'abonne à `SoundRequested`, un système de score à
  `EnemyKilled`, un HUD à `HealthChanged`.
- **`Unsubscribe(id)`** — recherche linéaire `O(n)` par id, puis **marque `active=false`** : la
  suppression est **différée**, l'entrée n'est physiquement retirée qu'au prochain `Drain`.
  Silencieux si l'id est introuvable. *Conséquence* : un handler peut être appelé une dernière fois
  s'il se désabonne au beau milieu d'une boucle d'émission — d'où l'intérêt du RAII.
- **`Emit(event)`** — `O(n)` sur les handlers : parcourt et appelle tous les `active && fn`,
  **synchrone**, sous lock. *Usage* : la notification immédiate hors itération (changement d'état UI,
  événement déclenché par une entrée utilisateur traitée seule).
- **`Queue(const T&)` / `Queue(T&&)`** — pousse l'événement dans la file (`O(1)` amorti), par copie
  ou par déplacement. Rien ne s'exécute. *Usage* : la voie sûre pendant l'itération ECS — combats,
  collisions, dégâts s'accumulent puis se dispatchent en fin de frame.
- **`Drain()`** — le cœur. (1) il **swap** la file vers une copie locale `toDispatch`
  (anti-réentrance : un handler peut re-`Queue` sans corrompre la passe) ; (2) double boucle
  événement × handler appelant les handlers actifs ; (3) **purge** les abonnés inactifs par
  compactage write-pointer puis `Resize(write)`. `O(events × handlers + handlers)`. *Usage* : appelé
  une fois par frame, après `FlushDeferred()` du world.
- **`Clear()`** — vide la file **sans** exécuter (`O(file)`). *Usage* : reset de scène, changement de
  niveau, où l'on jette les événements en attente.
- **`Empty()`** — file vide ? (lock sur mutex `mutable`). **`HandlerCount()`** — nombre d'abonnés, en
  comptant **actifs + inactifs non encore purgés** ; utile pour du debug/outillage éditeur, à lire
  avec la nuance de la purge différée.

Par domaine, le canal sert partout où un *type* d'événement précis circule : **audio** (requêtes de
lecture), **gameplay/IA** (perception, états), **UI/2D** (clics, focus), **réseau/IO** (paquets
reçus à dispatcher dans le thread principal), **outils/éditeur** (sélection, modification d'asset).

### `NkGameplayEventBus` — la façade

Le bus masque tout ce qui précède : on ne touche jamais les canaux directement. Au premier
`Subscribe<T>` ou `Queue<T>` d'un type, il **crée le canal inline** (placement-new dans un
`storage[]` réservé — pas d'allocation heap par canal) ; les `Emit<T>` ne créent **pas** de canal
(silencieux si personne n'écoute).

- **`Subscribe<T>(fn)`** — `GetOrCreate<T>()` puis abonne, encode et renvoie un `SubscriptionId`
  (`(TypeId<T> << 32) | HandleId`). Le `SubscriptionId` global garantit l'unicité entre types.
- **`Unsubscribe<T>(id)`** — `Find<T>()` ; si le canal existe, `Unsubscribe(id & 0xFFFFFFFF)`. Le
  type-safety vient du paramètre template `T` (la partie haute de `id` n'est pas re-vérifiée).
- **`Emit<T>` / `Queue<T>`** (chacun en versions `const T&` et `T&&`) — `Emit` ne crée rien et reste
  silencieux sans canal ; `Queue` crée le canal au besoin.
- **`Drain()`** — parcourt les canaux `[0..mCount)` dans l'**ordre de création** et draine chacun.
  *Domaine* : la boucle de jeu appelle `bus.Drain()` une fois par frame pour libérer tous les
  événements accumulés (combat → audio → UI → score, dans un point sûr).
- **`Clear()`** — vide toutes les files. **`HasPending()`** — true si au moins un canal n'est pas
  vide (utile pour décider de drainer, ou attendre que tout soit consommé avant un changement
  d'état).

Limites structurelles à intégrer dans la conception : **256 types** maximum (`kMaxChannels`),
recherche linéaire `O(n ≤ 256)` du canal par `TypeId`, et stockage canal **inline** surprovisionné
(×4) → événements **petits** obligatoires. Le bus n'étant pas un singleton, sa durée de vie est
celle de son propriétaire (World/GameManager) : tous les abonnés doivent se désabonner avant sa
destruction, idéalement par RAII.

### `NkFieldType` — vocabulaire des types de champ

L'enum (`uint8`, valeurs explicites) couvre trois groupes : les **scalaires/primitifs** (`Bool`,
`Int8..64`, `UInt8..64`, `Float32/64`, `String`), les **types moteur** (`Vec2`=20, `Vec3`, `Vec4`,
`Quat`, `Mat4`, et les identifiants ECS `EntityId`=30, `ComponentId`, `ArchetypeId`), et les
**composés** (`Array`=40, `Object`, `Enum`, `Flags`). C'est ce que lit un inspecteur d'éditeur pour
choisir le bon widget (slider pour un `Float32`, color picker pour un `Vec4`, champ texte pour une
`String`, sous-arbre pour un `Object`), et ce que lit un sérialiseur pour formater correctement
chaque champ.

### `NkMetaFlag` — l'intention attachée à un champ

Ce bitfield 64 bits est le **langage de configuration** des champs, organisé par familles
fonctionnelles (chaque flag = `1ULL << n`) :

- **Édition / inspecteur (outils, UI)** : `Visible`/`HideInEditor`/`ReadOnly`/`Advanced` règlent
  l'affichage ; `EditAnywhere`/`EditDefaultsOnly`/`EditFixedSize`/`NoEdit` la modifiabilité ;
  `Range`/`Password`/`Multiline`/`ColorPicker` choisissent le widget (un slider borné, un champ
  masqué, une zone multiligne, un sélecteur de couleur).
- **Sérialisation (IO)** : `Serialize`/`NoSerialize`/`SerializeDefault` décident si et comment un
  champ est écrit sur disque — `Transient` exclut un champ runtime du sauvegardé.
- **Blueprint (gameplay/visual scripting)** : `BlueprintReadWrite`/`ReadOnly`/`Callable`/`Pure`
  exposent un champ ou une fonction au graphe de script.
- **Réseau** : `Replicated`/`RepNotify`/`RepSkipOwner` pilotent la réplication d'un champ entre
  client et serveur (répliqué, notifié au changement, ignoré chez le propriétaire).
- **Cycle de vie (éditeur/outils)** : `Instanced`, `Duplicate`/`NeverDuplicate` règlent la copie
  d'objets.
- **`User0..7`** : huit bits réservés pour des conventions propres à un projet.

Les helpers de `NkFieldInfo` traduisent ces bits en décisions : `IsEditable()` = `!ReadOnly &&
!NoEdit`, `IsSerializable()` = `Serialize && !NoSerialize`, `IsArray()` = `count > 1 || type ==
Array`, `HasFlag(f)` teste un bit. Le défaut d'un champ déclaré (`Visible | EditAnywhere |
Serialize`) correspond au cas le plus courant : un champ visible, modifiable et sauvegardé.

### `NkFieldInfo` — la description d'un champ

Structure de pure donnée, tous champs avec valeurs par défaut. L'essentiel : `name`, `type`
(`NkFieldType`), `offset` et `size` (l'emplacement physique du champ dans la struct, calculés par
`offsetof`/`sizeof`), `count` (nombre d'éléments pour un tableau), `metaFlags`. Le reste est de la
métadonnée d'outillage : `category`/`tooltip`/`displayName` (présentation éditeur),
`minValue`/`maxValue` (bornes d'un slider `Range`), `editCondition` (afficher conditionnellement),
`replicates` (nom du callback `RepNotify`), `nested`/`nestedCount` (sous-champs d'un `Object`),
`defaultValue` (valeur de réinitialisation). Cette richesse est ce qui permet à un **inspecteur**
d'être généré entièrement à partir de la réflexion, sans code spécifique par type.

### `NkTypeInfo` — la description d'un type

`NkTypeInfo` agrège tout ce qu'on sait d'un type : son `componentId` (clé d'indexation dans le
registre), `name`, `size`, `align`, et le tableau `fields`/`fieldCount`. S'y ajoutent **deux jeux de
pointeurs de fonction** : le **cycle de vie** (`defaultCtor`, `dtor`, `copyCtor`, `moveCtor`, de
types `CtorFn`/`DtorFn`/`CopyFn`/`MoveFn`) qui permet de construire/détruire/copier un objet *sans
connaître son type statique* — exactement ce dont un système ECS générique ou un éditeur a besoin
pour instancier un composant à partir de son seul id ; et la **sérialisation** (`serialize` /
`deserialize`, de types `SerializeFn`/`DeserializeFn`, signatures `bool(const void*, char*, uint32)`
et `bool(void*, const char*)`) pour écrire/relire un type en JSON via un buffer brut. Attention :
ces pointeurs **ne sont jamais renseignés par les macros** — ils restent `nullptr` tant qu'on ne les
câble pas à la main.

Les helpers `FindField(name)` (recherche linéaire `std::strcmp`, `O(fieldCount)`, `nullptr` si
absent) et `HasField(name)` permettent de retrouver un champ par son nom — utile à un sérialiseur
qui relit un JSON champ par champ, ou à un script qui adresse une propriété par son nom.

### `NkReflectRegistry` — le registre global

Le **singleton** (`Global()`, Meyers singleton via static local) qui centralise tous les
`NkTypeInfo`. `Register(info)` indexe par `componentId` (ignore si `>= kMaxComponentTypes`) et met à
jour `mCount` en `O(1)`. `Get(id)` fait un accès direct par indice en `O(1)` (`nullptr` si `id >=
mCount`), `GetByName(name)` une recherche linéaire `O(n)` (`std::strcmp`), `ForEach(fn)` itère les
types ayant un `name` non-nul. `Count()` renvoie le nombre de types. Usages : un panneau « ajouter
un composant » d'**éditeur** liste tous les types via `ForEach` ; un **sérialiseur** retrouve le
`NkTypeInfo` d'un composant par son id pour itérer ses champs ; un système de **réseau** consulte les
flags `Replicated` d'un type. À noter le décalage de constantes : `Register` borne sur
`kMaxComponentTypes` (NkECSDefines), mais le stockage interne fait `kMaxTypes = 512` —
potentiellement différents.

### Les macros de réflexion — déclaration et auto-enregistrement

`NK_REFLECT_BEGIN(Type)` ouvre un **namespace anonyme**, déclare un struct d'auto-enregistrement
`_NkReflectReg_##Type`, pose l'alias `_T = Type`, et ouvre un tableau `static constexpr
NkFieldInfo _fields[]`. Entre ce begin et le end, on liste les champs :

- **`NK_FIELD(field)`** — champ « brut » : `type = Unknown`, `offset = offsetof(_T, field)`, `size =
  sizeof(declval<_T>().field)`, `count = 1`, flags par défaut `Visible|EditAnywhere|Serialize`.
- **`NK_FIELD_EX(field, type)`** — idem, mais le `NkFieldType` est donné explicitement (le bon choix
  quand le type compte pour l'inspecteur ou le sérialiseur, ex. `Vec3`, `Quat`).
- **`NK_FIELD_META(field, flags, cat, tip, min, max)`** — six paramètres pour personnaliser
  entièrement les métadonnées (flags, catégorie, tooltip, bornes).
- **`NK_ARRAY(field, count)`** — tableau fixe : `size = sizeof(declval<_T>().field[0])`, flags
  `Visible|EditFixedSize|Serialize`.

`NK_REFLECT_END()` ferme le tableau, construit un `NkTypeInfo` (`componentId = NkIdOf<_T>()`, `name
= #Type`, plus `size`/`align`/`fields`/`fieldCount`), appelle
`NkReflectRegistry::Global().Register(info)`, et **instancie l'objet statique** `_sNkReflectReg_##Type`
— c'est cette instanciation au démarrage qui rend l'enregistrement **automatique**. Corollaire
pratique : la TU contenant ces macros doit être **liée** au binaire, sinon le type n'apparaîtra pas
dans le registre.

`NK_PROPERTY(field, ...)` est une **façade** style Unreal/Unity censée accepter des arguments nommés
et se développer en `NK_FIELD_META`. **Piège majeur** : le plumbing de macros qui extrait ces
arguments (`NK_META_FIND_ARG`, `NK_META_EXTRACT_*`, tokens `NK_META_ARG_*`…) est **simplifié** et
`NK_META_FIND_ARG` **renvoie toujours la valeur par défaut** — l'extraction de Category/Tooltip/Min/
Max n'est **pas réellement implémentée**. N'utilisez pas `NK_PROPERTY` en comptant sur ces valeurs ;
préférez `NK_FIELD_META` explicite.

### Les helpers JSON `to_json`/`from_json` (serialization)

Trois familles d'adaptateurs nlohmann (`inline ... noexcept`), à n'employer que dans un contexte où
les types Prefab/Blueprint existent :

- **`NkValue`** — `to_json` sérialise `type` + `value` selon `NkValueType` (Int/Float/Bool/Vec3
  `{x,y,z}`/String/EntityId ; les autres cas tombent en `default: break`). `from_json` reconstruit
  par switch ; les chaînes via `std::strncpy(v.data.str, ..., sizeof-1)`.
- **`NkPrefab`** — `to_json` émet `name`, `version`, `tagBits`, `layer`, le tableau `components`
  (`type`/`data` JSON brut/`overridden`), le tableau `children` (`name`/`prefabPath`/`position`[3]/
  `scale`[3]/`active`), et `blueprintPath` s'il est non vide. `from_json` reconstruit avec des
  valeurs par défaut (`name = "UnnamedPrefab"`, `version = "1.0"`…) puis parse components et children.
- **`NkBlueprintGraph`** — `to_json` émet `nodes` (id index/type/enabled/inputs `[index,name,default]`
  /outputs `[index,name]`, en sautant les nodes nuls) et `connections`
  (srcNode/srcPin/tgtNode/tgtPin). `from_json` recrée chaque node via
  `blueprint::NkNodeRegistry::Global().Create(type)`, restaure les defaults des pins, recharge les
  connections (`srcNode`/`tgtNode` défaut `0xFFFFFFFF`) et re-marque `isConnected = true` sur les
  pins d'entrée connectés.

### Les fonctions fichier (serialization)

Quatre fonctions `inline noexcept`, toutes bâties sur `try/catch(...) → bool`. **`SaveToFile(path,
prefab)`** ouvre un `std::ofstream` et écrit `json(prefab).dump(4)` (JSON indenté 4 espaces) ;
**`LoadFromFile(path, prefab)`** ouvre un `std::ifstream` et applique `from_json`.
**`SaveBlueprintToFile`** / **`LoadBlueprintFromFile`** font la même chose pour un
`NkBlueprintGraph`. Toutes renvoient `false` en cas d'échec d'ouverture ou d'exception (avalée).
*Limite de cohérence* : ces I/O passent par `std::ofstream`/`std::ifstream`, **pas** par
NKFileSystem/NkFile — contrairement au reste du moteur. Sur les plateformes où l'I/O fichier doit
passer par l'abstraction (Android, par ex.), ce header ne convient pas tel quel.

### Le socle commun et les avertissements de fiabilité

- **Trois philosophies différentes.** `NkGameplayEventBus.h` est **zéro-STL** (réellement
  autonome). `NkReflect.h` utilise une **STL partielle** (`std::strcmp`, `std::declval`,
  `<cstdint>`, `<cstring>`, `<functional>`, `offsetof`). `NkJsonSerialization.h` est **STL-full**
  (`<nlohmann/json.hpp>`, `<fstream>`, `std::ofstream`, `std::string`, `std::make_unique`,
  `std::strncpy`).
- **NkJsonSerialization ne compile pas dans NKECS.** Il inclut `../Prefab/NkPrefab.h` et
  `../VisualScript/NkBlueprint.h`, **absents de ce module** (ils sont chez Noge), et toute son API
  dépend de types (`NkPrefab`, `NkBlueprintGraph`, `NkValue`) déclarés ailleurs. C'est documenté
  ici par souci d'exhaustivité, mais c'est **non autonome**.
- **Aucun header parapluie** ne tire ces trois fichiers ; chacun inclut `NKECS/NkECSDefines.h` pour
  `NkComponentId`, `kInvalidComponentId`, `kMaxComponentTypes`, `NkIdOf<T>`, `NKECS_ASSERT` et les
  types primitifs.

---

### Exemple récapitulatif

```cpp
// 1) BUS — pattern Queue/Drain pendant l'itération ECS.
#include "NKECS/Events/NkGameplayEventBus.h"
using namespace nkentseu::ecs;

NkGameplayEventBus bus;                          // pas un singleton

auto subId = bus.Subscribe<EnemyKilled>([&](const EnemyKilled& e) {
    score += e.points;                           // un seul abonné parmi d'autres
});

// pendant la mise à jour des systèmes (itération en cours) :
bus.Queue(EnemyKilled{ enemyId, 100 });          // différé, sûr

// fin de frame, point sûr :
world.FlushDeferred();
if (bus.HasPending()) bus.Drain();               // tous les handlers tournent ici
bus.Unsubscribe<EnemyKilled>(subId);             // différé : purge au prochain Drain

// 2) RÉFLEXION — décrire un type, auto-enregistré au démarrage.
#include "NKECS/Reflect/NkReflect.h"
using namespace nkentseu::ecs::reflect;

struct Health { float current; float max; };

NK_REFLECT_BEGIN(Health)
    NK_FIELD_META(current, NkMeta_Visible | NkMeta_EditAnywhere | NkMeta_Serialize,
                  "Stats", "Points de vie actuels", 0.f, 100.f)
    NK_FIELD_EX(max, NkFieldType::Float32)
NK_REFLECT_END()

// plus tard : un inspecteur d'éditeur parcourt tous les types connus.
NkReflectRegistry::Global().ForEach([](const NkTypeInfo& t) {
    for (uint32 i = 0; i < t.fieldCount; ++i)
        if (t.fields[i].IsEditable()) /* afficher le widget du champ */;
});
```

---

[← Index NKECS](README.md) · [Récap NKECS](../NKECS.md) · [Couche Runtime](../README.md)
