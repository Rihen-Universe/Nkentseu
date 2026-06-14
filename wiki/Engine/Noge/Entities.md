# Les entités gameplay

> Couche **Engine** · Noge · Donner un **visage gameplay** à l'ECS : le handle `NkGameObject`,
> la hiérarchie d'acteurs `NkActor` / `NkPawn` / `NkCharacter`, les scripts `NkBehaviour` (+ leurs
> trois systèmes), et la fabrique `NkGameObjectFactory` qui relie tout au monde ECS.

En dessous de Noge vit le moteur ECS bas niveau (NKECS) : un `NkWorld` qui range des **composants**
par archétypes et les parcourt à plein débit. C'est rapide, mais c'est **anonyme** : le `NkWorld`
ne connaît ni « joueur », ni « ennemi », ni « caméra » — seulement des identifiants d'entités
(`NkEntityId`) et des tableaux de données. Écrire du gameplay directement dessus, c'est manipuler
des indices et des composants nus à longueur de journée. La famille **Entities** de Noge pose
**par-dessus** une couche d'API familière, dans l'esprit de Unity et d'Unreal : on parle d'objets
qu'on **crée**, qu'on **nomme**, auxquels on **attache des composants** et des **scripts**, et qui
ont un **cycle de vie** (`BeginPlay`, `Tick`, `EndPlay`). Le tout sans copier de données : un
`NkGameObject` n'est qu'un **handle léger** vers l'entité réelle qui, elle, reste dans le `NkWorld`.

Cette page raconte comment ces pièces s'emboîtent — depuis le handle de base jusqu'aux scripts pilotés
par leurs systèmes, en passant par la fabrique obligatoire.

- **Namespace** : `nkentseu::ecs` pour tout **sauf** `NkGameObjectFactory`, qui est directement dans
  `nkentseu`.
- **Headers** : `NkGameObject.h`, `NkActor.h`, `NkBehaviour.h`, `NkBehaviourSystem.h`,
  `NkGameObjectFactory.h` (sous `Noge/ECS/Entities/` et `Noge/ECS/Factory/`).

---

## Le handle d'entité : `NkGameObject`

C'est la **brique de base**, et la classe **parente de toute la hiérarchie d'acteurs**. Il faut bien
comprendre ce qu'il *est* : non pas un objet qui contient ses données, mais un **handle léger** — deux
champs seulement, un `NkEntityId mId` et un `NkWorld* mWorld` (≈ 16 octets). Toutes les vraies données
(la position, le nom, les composants) vivent dans le `NkWorld` ; le `NkGameObject` ne fait que **les
désigner** et offrir des raccourcis pour y accéder. On le copie librement, on le passe par valeur :
c'est aussi bon marché qu'un pointeur, et plusieurs handles peuvent pointer la même entité.

Ce n'est **pas** un objet qu'on construit avec `new`. Le constructeur explicite
`NkGameObject(NkEntityId, NkWorld*)` est « réservé aux factories » : on crée toujours un GameObject
via `NkGameObjectFactory` ou `NkSceneGraph::SpawnActor`, jamais à la main. La raison est concrète :
la fabrique garantit qu'à la naissance, l'entité possède déjà six **composants invariants** —
`NkTransform`, `NkName`, `NkTag`, `NkParent`, `NkChildren`, `NkBehaviourHost` — sur lesquels tout le
reste de l'API s'appuie sans jamais vérifier leur présence.

Un handle peut être **invalide** : le `NkGameObject()` par défaut (`mWorld == nullptr`), ou un handle
vers une entité détruite. La règle d'or est de tester `IsValid()` (ou `if (go)` via
`operator bool`) avant tout. Attention en particulier à `World()` : il fait un assert puis
**déréférence** `mWorld` — sur un handle invalide, c'est un crash. La comparaison `==` compare **à la
fois** `mId` et `mWorld` : deux handles ne sont égaux que s'ils désignent la même entité dans le même
monde. `Name()` ne crashe jamais, lui : il renvoie `"InvalidGO"` sur handle mort, `"UnnamedGO"` si
l'entité n'a pas de `NkName`.

```cpp
auto go = NkGameObjectFactory::Create(world, "Crate");   // jamais "new NkGameObject"
go.SetPosition(2.f, 0.f, 5.f);                            // raccourci style Unity
if (go) {                                                 // operator bool == IsValid()
    NkLog("nom = %s", go.Name());
}
```

> **En résumé.** `NkGameObject` est un **handle** (≈ 16 octets) vers une entité du `NkWorld`, pas un
> conteneur de données. On le crée **toujours via la fabrique** (jamais `new`), ce qui garantit six
> composants invariants. Testez `IsValid()` / `operator bool` avant d'agir ; `World()` déréférence et
> peut crasher sur handle mort ; `==` compare entité **et** monde.

---

## Composants : trois modes d'accès

Tout l'intérêt d'un GameObject est d'**attacher et lire des composants**. Noge propose **trois**
façons de récupérer un composant, et le choix n'est pas cosmétique : il encode **ce qu'on sait** de la
présence du composant.

`GetComponent<T>()` renvoie un `NkComponentHandle<T>` **nullable** : c'est le mode quand le composant
*peut* être absent. On le teste comme un pointeur — `if (auto h = go.GetComponent<NkRigidBody>())`.
`RequireComponent<T>()` renvoie un `NkRequiredComponent<T>` qui **assert** si le composant manque :
réservé aux **invariants** (ceux que la fabrique garantit), là où l'absence est un bug et doit
exploser. `Optional<T>()` renvoie un `NkOptionalComponent<T>` qui ne crashe **jamais** : si le
composant manque, il renvoie un *dummy* inerte — pratique pour du code « best effort » qui veut écrire
sur un composant sans se soucier de son existence.

Côté écriture, `AddComponent<T>(args…)` ajoute un composant (et ne fait rien, renvoyant `{}`, si le
handle est invalide). `RemoveComponent<T>()` le retire **immédiatement** ; `RemoveComponentDeferred<T>()`
diffère la suppression — c'est la version **sûre pendant une query** (on ne modifie pas la structure
qu'on est en train de parcourir). `HasComponent<T>()` teste la présence.

> **En résumé.** Trois lectures selon votre certitude : `GetComponent` (nullable, on teste),
> `RequireComponent` (assert, pour les invariants), `Optional` (jamais de crash, dummy si absent).
> Écriture via `AddComponent` ; suppression **immédiate** (`RemoveComponent`) ou **différée**
> (`RemoveComponentDeferred`, sûre en pleine query).

---

## Plusieurs composants du même type : `NkComponentBag`

L'ECS classique range **un** composant de chaque type par entité. Mais certains cas en réclament
**plusieurs** (deux sources audio, plusieurs colliders). `AddMultiple<T>(args…)` répond à ce besoin
via un `NkComponentBag<T>` : un petit conteneur **multi-instances**. `GetAllComponents<T>()` renvoie
alors un `NkSpan<T>` sur **toutes** les instances — qu'il y en ait via un bag, une seule (singleton
`{c, 1}`), ou aucune (`{}`).

Le bag est optimisé : il garde jusqu'à **8 éléments inline** (Small Buffer Optimization,
`kSBOCapacity = 8`) sans aucune allocation, et ne bascule sur le tas qu'au-delà. Il n'est **pas
copiable** (copie et affectation supprimées) ; son destructeur détruit proprement les instances.

> **En résumé.** Pour stocker **plusieurs** composants d'un même type sur une entité :
> `AddMultiple<T>()` + `GetAllComponents<T>()` (qui renvoie un `NkSpan` couvrant bag / singleton /
> vide). Le `NkComponentBag` garde 8 instances inline avant de passer au tas, et n'est pas copiable.

---

## La hiérarchie d'acteurs : `NkActor` → `NkPawn` → `NkCharacter`

Le `NkGameObject` est neutre. Pour le **gameplay**, Noge dérive une hiérarchie dans l'esprit d'Unreal,
chaque niveau ajoutant du sens :

`NkActor` introduit le **cycle de vie**. Il hérite des constructeurs de `NkGameObject` et déclare cinq
hooks **virtuels**, tous **vides par défaut**, faits pour être surchargés : `BeginPlay()` (une fois,
avant la première frame), `Tick(dt)` (chaque frame), `LateTick(dt)` (après tous les Tick, avant le
rendu — pour une caméra qui suit sa cible), `FixedTick(fixedDt)` (à pas fixe, pour la physique), et
`EndPlay()` (au déchargement de la scène). Point **crucial** : on n'appelle **jamais** ces méthodes
soi-même — c'est `NkSceneLifecycleSystem` (via le `NkScheduler`) qui les déclenche au bon moment.
`NkActor` fournit aussi `SpawnDefaultComponents()`, qui a un **vrai corps** : il ajoute, s'ils
manquent, `NkTransform`, `NkTag` et `NkName("UnnamedActor")`. (Le commentaire d'en-tête le dit
« appelé par la fabrique », mais le corps visible de `NkGameObjectFactory::Create` ajoute lui-même
les composants invariants sans passer par cette méthode — à appeler soi-même si besoin.)

`NkPawn` est un acteur **contrôlable** — joueur, IA, véhicule. Il ajoute la notion de **possession** :
`Possess(controllerId)` le marque comme contrôlé par une manette/un contrôleur, `Unpossess()` le
libère, `IsPossessed()` et `GetControllerId()` interrogent l'état. C'est le lien entre une entité du
monde et l'entrée qui la pilote.

`NkCharacter` est un **personnage bipède**. Il override `BeginPlay()` avec un corps réel : il appelle
d'abord `NkPawn::BeginPlay()`, puis **auto-ajoute** ce qu'il faut à un personnage s'ils manquent —
`NkCharacterController`, `NkSkeletalMesh`, `NkAnimator`, `NkAudioSource`. C'est l'idiome à reproduire :
**surcharger `BeginPlay()` en appelant d'abord la base**, puis configurer.

```cpp
class Hero : public NkCharacter {
public:
    using NkCharacter::NkCharacter;
    void BeginPlay() noexcept override {
        NkCharacter::BeginPlay();                       // crée controller/mesh/animator/audio
        if (auto cc = GetComponent<NkCharacterController>()) cc->speed = 6.f;
    }
    void Tick(float32 dt) noexcept override { /* logique par frame */ }
};
```

> **En résumé.** Quatre niveaux : `NkGameObject` (handle neutre) → `NkActor` (cinq hooks de cycle de
> vie virtuels et vides, déclenchés par `NkSceneLifecycleSystem`, **pas** par vous) → `NkPawn`
> (possession contrôlable) → `NkCharacter` (`BeginPlay` qui auto-équipe controller/mesh/animator/audio).
> Idiome : surcharger `BeginPlay`, appeler la **base d'abord**, configurer ensuite.

---

## Les scripts : `NkBehaviour`

L'acteur n'est pas la seule façon d'ajouter du comportement. `NkBehaviour` est l'équivalent du
`MonoBehaviour` de Unity : un **script composant** qu'on **attache** à un GameObject (via son
`NkBehaviourHost`), au lieu de dériver l'objet lui-même. C'est un axe **orthogonal** à la hiérarchie
d'acteurs — on peut greffer plusieurs behaviours sur n'importe quel GameObject.

Il offre son propre cycle de vie, plus riche que celui de l'acteur, et **piloté par ses systèmes** :
`OnAwake()` (juste après l'ajout), `OnStart()` (avant la première frame), `OnEnable()` / `OnDisable()`
(quand l'objet devient actif/inactif), `OnUpdate(dt)`, `OnLateUpdate(dt)`, `OnFixedUpdate(fixedDt)`, et
`OnDestroy()`. On attache un behaviour avec `go.AddBehaviour<MonScript>()`, ce qui **lie**
automatiquement le script à son GameObject (le système renseigne `mGameObject`, lisible via
`GetGameObject()`). `SetEnabled(bool)` active/désactive le script (et déclenche `OnEnable`/`OnDisable`),
`IsEnabled()` interroge.

Un point **obligatoire** : `GetTypeName()` est **purement virtuel**. `NkBehaviour` est donc une classe
**abstraite** — tout script *doit* l'implémenter (il sert au débogage). Autre nuance par rapport au
reste de la famille : **aucune** méthode de `NkBehaviour` n'est marquée `noexcept`.

```cpp
class Spinner : public NkBehaviour {
public:
    const char* GetTypeName() const override { return "Spinner"; }  // obligatoire
    void OnUpdate(float dt) override {
        GetGameObject()->Transform().Get()->Rotate(/* … */ dt);
    }
};
// attache + lie automatiquement :
go.AddBehaviour<Spinner>();
```

> **En résumé.** `NkBehaviour` = script attaché (style `MonoBehaviour`), orthogonal aux acteurs.
> Cycle de vie riche (`OnAwake`/`OnStart`/`OnEnable`/`OnDisable`/`OnUpdate`/`OnLateUpdate`/
> `OnFixedUpdate`/`OnDestroy`) piloté par les **systèmes**. `AddBehaviour<T>()` lie le script à son
> GameObject. `GetTypeName()` est **pur virtuel** (classe abstraite) ; rien ici n'est `noexcept`.

---

## Qui appelle les hooks : les trois systèmes de behaviours

Les hooks de `NkBehaviour` ne se déclenchent pas tout seuls : **trois systèmes ECS** (tous dérivés de
`NkSystem`) les pilotent, chacun dans une phase différente de la frame. Ils parcourent tous les
`NkBehaviourHost` actifs (`Query<NkBehaviourHost>().Without<NkInactive>()`) et n'invoquent un hook que
si le behaviour existe et est **activé**.

`NkBehaviourSystem` tourne en phase **PreUpdate**. Il fait deux choses : d'abord il démarre les
nouveaux behaviours — pour chaque host pas encore démarré, il appelle `OnStart()` une fois sur chaque
script puis le marque `mStarted` ; ensuite il appelle `OnUpdate(dt)` sur tous les scripts activés et
déjà démarrés. `NkBehaviourLateSystem` tourne en **PostUpdate** et appelle `OnLateUpdate(dt)`.
`NkBehaviourFixedSystem` tourne en **FixedUpdate** et appelle `OnFixedUpdate(fixedDt)`.

Conséquence à connaître : ces trois systèmes ne déclenchent **que** `OnStart`, `OnUpdate`,
`OnLateUpdate` et `OnFixedUpdate`. Les hooks `OnAwake`, `OnEnable`/`OnDisable` et `OnDestroy` sont
invoqués **ailleurs** (à l'ajout, lors de l'activation, à la destruction) — pas par ce trio.

> **En résumé.** Trois systèmes (`NkSystem`) pilotent les behaviours : `NkBehaviourSystem`
> (**PreUpdate** : `OnStart` puis `OnUpdate`), `NkBehaviourLateSystem` (**PostUpdate** :
> `OnLateUpdate`), `NkBehaviourFixedSystem` (**FixedUpdate** : `OnFixedUpdate`). Ils n'appellent **ni**
> `OnAwake`, **ni** `OnEnable`/`OnDisable`, **ni** `OnDestroy` (déclenchés ailleurs).

---

## La fabrique : `NkGameObjectFactory`

C'est la **porte d'entrée obligatoire**. Le `NkWorld` ne connaît pas `NkGameObject` ; la fabrique est
le **pont** entre les deux — une classe utilitaire **stateless** (constructeur supprimé, jamais
instanciée), située dans le namespace `nkentseu` (et **non** `nkentseu::ecs`).

`Create<T>(world, "nom", args…)` crée l'entité, lui ajoute les **six invariants** (`NkName`, `NkTag`,
`NkTransform`, `NkParent`, `NkChildren`, `NkBehaviourHost`), et renvoie un handle de type `T` (par
défaut `NkGameObject`, mais n'importe quelle classe dérivée — un `Hero`, un `NkCharacter`…). Une
surcharge **sans nom** déduit automatiquement le nom à partir du type. Pour fabriquer en masse,
`CreateBatch<T>(world, "Ennemi", count, out)` remplit un `NkVector<T>` de `count` objets nommés
`"Ennemi_0"`, `"Ennemi_1"`, etc.

```cpp
using nkentseu::NkGameObjectFactory;
auto player = NkGameObjectFactory::Create<Hero>(world, "Player");   // handle typé Hero
NkVector<NkGameObject> mobs;
NkGameObjectFactory::CreateBatch(world, "Orc", 12, mobs);           // Orc_0 … Orc_11
```

> **En résumé.** `NkGameObjectFactory` (namespace `nkentseu`, **stateless**) est l'unique façon
> correcte de créer des GameObjects : `Create<T>(world, nom, …)` pose les six invariants et renvoie un
> handle typé ; `CreateBatch` en produit plusieurs d'un coup. C'est le pont `NkWorld` ↔ `NkGameObject`.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence complète ».

### `NkGameObject` (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkGameObject()` · `NkGameObject(id, world)` | Handle invalide / ctor **réservé aux factories** |
| Identité | `Id()`, `IsValid()`, `World()`, `Name()`, `SetName()` | Identifiant / validité / monde (**déréf.**) / nom (jamais de crash) / renommer |
| Opérateurs | `operator bool`, `operator==`, `operator!=` | Validité ; égalité sur **entité + monde** |
| Invariants | `Transform()`, `Tags()` | Accès requis à `NkTransform` / `NkTag` |
| Transform | `SetPosition` (×2), `SetRotation`, `SetScale`, `GetPosition`, `GetWorldPosition` | Raccourcis style Unity (no-op sans `NkTransform`) |
| Composants (lecture) | `GetComponent<T>` (nullable), `RequireComponent<T>` (assert), `Optional<T>` (dummy), `HasComponent<T>` | Trois modes selon la certitude de présence |
| Composants (écriture) | `AddComponent<T>`, `AddMultiple<T>`, `GetAllComponents<T>`, `RemoveComponent<T>`, `RemoveComponentDeferred<T>` | Ajout simple/multiple, span de tous, suppression immédiate/différée |
| Compat | `Add<T>`, `Get<T>`, `Has<T>`, `Remove<T>`, `GetAll<T>` | Alias descendants des méthodes ci-dessus |
| Hiérarchie | `SetParent`, `GetParent`, `AddChild`, `GetChildren`, `GetChildrenV` | Parent/enfants (**déclarées seulement** ici) |
| Behaviours | `AddBehaviour<T>`, `GetBehaviour<T>` | Attache un script (le **lie**) / le récupère |
| Cycle de vie | `SetActive`, `IsActive`, `DestroyDeferred`, `Destroy` | Activation / destruction (la plupart **déclarées seulement**) |

### `NkComponentBag<T>` (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Capacité | `kSBOCapacity = 8`, `Count()` | 8 instances inline avant le tas / nombre courant |
| Modification | `Add(value = T{})` | Ajoute (bascule au tas au-delà de 8) |
| Accès | `Get(idx)`, `GetAll()` | Élément (`nullptr` si hors borne) / `NkSpan` complet |
| Contraintes | copie/affectation **supprimées** | Non copiable |

### Hiérarchie d'acteurs (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkActor` | `BeginPlay`, `Tick`, `LateTick`, `FixedTick`, `EndPlay` (virtuels, vides) | Cycle de vie, déclenché par `NkSceneLifecycleSystem` |
| `NkActor` | `SpawnDefaultComponents()` | Ajoute `NkTransform`/`NkTag`/`NkName("UnnamedActor")` |
| `NkPawn` | `Possess(id)`, `Unpossess()`, `IsPossessed()`, `GetControllerId()` | Possession d'un acteur contrôlable |
| `NkCharacter` | `BeginPlay()` override | Auto-ajoute controller / mesh / animator / audio |

### `NkBehaviour` (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `OnAwake`, `OnStart`, `OnEnable`, `OnDisable`, `OnUpdate`, `OnLateUpdate`, `OnFixedUpdate`, `OnDestroy` | Hooks virtuels pilotés par les systèmes |
| Lien | `GetGameObject()` | GameObject porteur (rempli auto à l'attache) |
| Activation | `SetEnabled(bool)`, `IsEnabled()` | Active/désactive le script |
| Identité | `GetTypeName()` | **Pur virtuel** — chaque script doit l'implémenter |

### Systèmes de behaviours (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkBehaviourSystem` | `Describe()`, `Execute()` | **PreUpdate** : `OnStart` puis `OnUpdate` |
| `NkBehaviourLateSystem` | `Describe()`, `Execute()` | **PostUpdate** : `OnLateUpdate` |
| `NkBehaviourFixedSystem` | `Describe()`, `Execute()` | **FixedUpdate** : `OnFixedUpdate` |

### `NkGameObjectFactory` (namespace `nkentseu`, **stateless**)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Création | `Create<T>(world, name, args…)`, `Create<T>(world, args…)` | Crée + pose les 6 invariants ; surcharge sans nom |
| Masse | `CreateBatch<T>(world, base, count, out)` | Remplit un `NkVector<T>` (`base_0`, `base_1`…) |
| Prefabs | `InstantiatePrefab`, `InstantiatePrefabBatch` | **Déclarées seulement** (corps hors header) |

---

## Référence complète

Chaque élément est repris en détail, avec ses usages dans les différents domaines du moteur. Le
**statut d'implémentation** est signalé là où le corps de la méthode n'est pas visible dans ces
headers.

### `NkGameObject` — identité et validité

`Id()` renvoie le `NkEntityId` sous-jacent. `IsValid()` vérifie les **trois** conditions à la fois :
un `mWorld` non nul, un `mId` valide, et une entité **encore vivante** (`mWorld->IsAlive(mId)`) — c'est
la seule garantie complète qu'on peut agir. `operator bool` est l'alias court (`if (go)`). Les deux
opérateurs de comparaison comparent `mId` **et** `mWorld` : un GameObject n'est égal à un autre que
s'ils désignent la même entité **du même monde** (utile quand on jongle avec plusieurs scènes/mondes).

`World()` renvoie une **référence** au `NkWorld` — mais il assert (`NKECS_ASSERT(mWorld)`) puis
déréférence : sur un handle invalide, c'est un crash. `Name()`, à l'inverse, est blindé : `"InvalidGO"`
si le handle est mort, `"UnnamedGO"` s'il n'y a pas de `NkName`, sinon le nom réel. `SetName(name)` est
un no-op si le handle est invalide ou si `name` est nul ; sinon il met à jour (ou ajoute) le `NkName`.

- **Éditeur / outils** — afficher l'arbre de scène : `Name()` ne crashe jamais, idéal pour peupler un
  panneau hiérarchie même avec des handles douteux.
- **Gameplay / IA** — stocker des références entre entités (cible, propriétaire) sous forme de handle,
  et vérifier `IsValid()` avant d'agir : un ennemi peut avoir disparu depuis qu'on l'a mémorisé.
- **Réseau / sérialisation** — la comparaison sur `(mId, mWorld)` distingue proprement deux entités
  homonymes dans des mondes différents.

### `NkGameObject` — raccourcis Transform

`Transform()` renvoie un `NkRequiredComponent<NkTransform>` (accès **requis** à l'invariant), `Tags()`
de même pour `NkTag`. Par-dessus, six raccourcis « style Unity » évitent de passer par le composant :
`SetPosition` (deux formes, `NkVec3f` ou `x,y,z`), `SetRotation(NkQuatf)`, `SetScale(NkVec3f)`,
`GetPosition()` (la position **locale**, ou `{}` à défaut), et `GetWorldPosition()` (la position
**monde**, via `t->GetWorldPosition()`). Tous sont des **no-op** si l'entité n'a pas de `NkTransform`
(ce qui n'arrive pas sur un objet créé par la fabrique).

- **Rendu / scène** — positionner rapidement un objet sans manipuler le composant à la main.
- **Animation** — distinguer position locale (`GetPosition`) et monde (`GetWorldPosition`) compte dès
  qu'il y a une hiérarchie parent/enfant.
- **Gameplay** — déplacer une entité, viser, téléporter, en une ligne.

### `NkGameObject` — composants (trois modes)

Le cœur de l'API. `GetComponent<T>()` (→ `NkComponentHandle<T>` **nullable**) est le mode par défaut
quand la présence est incertaine : on le teste comme un pointeur. `RequireComponent<T>()`
(→ `NkRequiredComponent<T>`) **assert** si absent — réservé aux invariants, là où l'absence est un bug.
`Optional<T>()` (→ `NkOptionalComponent<T>`) ne crashe **jamais** : il renvoie un dummy inerte si le
composant manque, pour du code « écris si tu peux » sans branche conditionnelle.

`AddComponent<T>(args…)` ajoute le composant (renvoie `{}` sans rien faire si le handle est invalide).
`RemoveComponent<T>()` retire **immédiatement** ; `RemoveComponentDeferred<T>()` **diffère** — c'est la
version sûre **à l'intérieur d'une query**, où modifier la structure parcourue est dangereux.
`HasComponent<T>()` teste la présence.

- **ECS / systèmes** — un système de physique fait `if (auto rb = go.GetComponent<NkRigidBody>())` ;
  l'absence est normale (tous les objets ne sont pas physiques).
- **Invariants** — lire `RequireComponent<NkTransform>()` quand on *sait* qu'il est là (objet
  fabriqué) : pas de branche, et un assert qui révèle le bug si l'invariant a été violé.
- **Gameplay défensif** — `Optional<NkHealth>()->TakeDamage(10)` ne plante pas même sur une entité
  sans santé.
- **Pendant une query** — supprimer un composant via `RemoveComponentDeferred<T>()` pour ne pas
  corrompre l'itération en cours.

### `NkComponentBag<T>` et `AddMultiple` — plusieurs instances

Quand une entité a besoin de **plusieurs** composants d'un même type, `AddMultiple<T>(args…)` les range
dans un `NkComponentBag<T>` et `GetAllComponents<T>()` les expose tous via un `NkSpan<T>` (qui couvre
indifféremment le cas bag, le cas singleton `{c, 1}` ou le cas vide `{}`). Le bag garde **8 instances
inline** (`kSBOCapacity`) avant de basculer sur le tas, expose `Add`, `Get(idx)` (`nullptr` hors
borne), `GetAll()` et `Count()`, et n'est **pas copiable**.

- **Audio** — plusieurs `NkAudioSource` sur une même entité (un moteur + un klaxon sur un véhicule).
- **Physique / collision** — plusieurs colliders composant une forme complexe.
- **Rendu** — plusieurs sous-meshes ou matériaux portés par la même entité.

### `NkGameObject` — compatibilité descendante

Une seconde série d'alias plus courts double l'API riche : `Add<T>` (→ `AddComponent<T>(…).Get()`),
`Get<T>` / `Get<T>() const` (→ `Resolve`/`ResolveConst`), `Has<T>` (→ `HasComponent`), `Remove<T>`
(→ `RemoveComponent`), `GetAll<T>` (→ `GetAllComponents`). Ils existent pour le code historique ;
préférez les noms longs (`AddComponent`, `HasComponent`…) dans le code neuf, plus explicites.

### `NkGameObject` — behaviours

`AddBehaviour<T>(args…)` récupère ou crée le `NkBehaviourHost` de l'entité, y crée le script, et —
détail important — appelle `SetGameObject(this)` pour **lier** le behaviour à son GameObject (d'où
`GetGameObject()` valide ensuite côté script). Il renvoie `nullptr` si le handle est invalide.
`GetBehaviour<T>()` récupère un script déjà attaché (ou `nullptr`).

- **Gameplay** — greffer un script de comportement (patrouille, ramassage) sur n'importe quel objet
  sans le dériver.
- **Éditeur** — attacher/détacher des scripts à chaud sur une entité sélectionnée.
- **IA** — composer plusieurs behaviours indépendants sur le même agent.

### `NkGameObject` — hiérarchie (déclarées seulement)

`SetParent`, `GetParent`, `AddChild`, `GetChildren(out)` et `GetChildrenV()` exposent l'arbre
parent/enfants (qui s'appuie sur les invariants `NkParent`/`NkChildren`). **Statut :** ces méthodes sont
**déclarées** dans le header mais leur corps est défini **ailleurs** — l'implémentation n'est pas
visible ici, donc on documente l'intention sans pouvoir en garantir l'état. Côté usage attendu :
attacher une arme à une main (animation/scène), grouper des objets sous un pivot (éditeur), propager une
transformation parent → enfants (rendu).

### `NkGameObject` — activation et destruction

`Destroy()` (inline) délègue à `DestroyDeferred()` : la destruction est **différée** (sûre en pleine
frame). `SetActive(bool)` / `IsActive()` activent/désactivent l'objet. **Statut :** `SetActive`,
`IsActive` et `DestroyDeferred` sont **déclarées seulement** ici (corps hors header) ; seul
`Destroy()` a un corps visible (il appelle `DestroyDeferred()`). Usage attendu : désactiver un ennemi
hors champ sans le détruire (gameplay/perf), retirer une entité en fin de vie (gameplay), masquer un
objet d'édition.

### `NkActor` — le cycle de vie

`NkActor` hérite des constructeurs de `NkGameObject` et déclare cinq hooks **virtuels** au **corps vide
par défaut**, faits pour être surchargés : `BeginPlay()` (une fois, avant la première frame),
`Tick(dt)` (chaque frame), `LateTick(dt)` (après tous les `Tick`, avant le rendu), `FixedTick(fixedDt)`
(pas fixe, physique) et `EndPlay()` (au déchargement). **Règle absolue :** ne **jamais** appeler ces
méthodes soi-même — `NkSceneLifecycleSystem` (via le `NkScheduler`) s'en charge. `SpawnDefaultComponents()`
a un **vrai corps** : il ajoute `NkTransform`, `NkTag` et `NkName("UnnamedActor")` s'ils manquent (à
appeler soi-même au besoin ; le corps visible de la fabrique ajoute déjà ses invariants sans l'appeler).

- **Gameplay** — toute la logique d'une entité vit dans `Tick` ; l'initialisation dans `BeginPlay`.
- **Animation / caméra** — `LateTick` pour suivre une cible **après** que tout a bougé.
- **Physique** — `FixedTick` pour une intégration à pas constant indépendant du framerate.
- **Scène** — `EndPlay` pour libérer des ressources au déchargement.

### `NkPawn` — la possession

`NkPawn` ajoute la notion d'acteur **contrôlable**. `Possess(controllerId = 0)` marque l'objet comme
possédé (`mPossessed = true`, `mControllerId = controllerId`), `Unpossess()` réinitialise
(`false` / `0`), `IsPossessed()` et `GetControllerId()` interrogent l'état.

- **Gameplay** — relier une entité du monde à une manette (jeu local à plusieurs : `controllerId` =
  numéro de joueur).
- **IA** — un même pawn peut passer du contrôle joueur au contrôle IA (`Unpossess` puis logique IA).
- **Véhicules** — posséder/quitter un véhicule.

### `NkCharacter` — le personnage équipé

`NkCharacter` override `BeginPlay()` avec un **corps réel** : il appelle `NkPawn::BeginPlay()` (qui,
faute d'override dans `NkPawn`, résout sur `NkActor::BeginPlay()` vide) puis **auto-ajoute** s'ils
manquent `NkCharacterController`, `NkSkeletalMesh`, `NkAnimator` et `NkAudioSource`. C'est l'archétype
prêt à animer/déplacer/sonoriser un bipède. **Idiome :** surcharger `BeginPlay`, appeler
`NkCharacter::BeginPlay()` en premier, puis configurer — le mouvement se pilote via
`GetComponent<NkCharacterController>()`, les invariants via `RequireComponent<T>().Get()`, les
optionnels via `Optional<T>()->…`.

- **Gameplay** — héros, PNJ, ennemis bipèdes prêts à l'emploi.
- **Animation** — l'`NkAnimator` et le `NkSkeletalMesh` ajoutés d'office relient le personnage au
  système d'animation.
- **Audio** — l'`NkAudioSource` permet pas/voix/effets attachés au personnage.

### `NkBehaviour` — le script

`NkBehaviour` est la base de tous les scripts. Ses hooks sont **virtuels**, au corps vide sauf
indication, et **invoqués par les systèmes** (jamais par vous) : `OnAwake()` (après l'ajout),
`OnStart()` (avant la première frame), `OnEnable()`/`OnDisable()` (activation), `OnUpdate(dt)`,
`OnLateUpdate(dt)`, `OnFixedUpdate(fixedDt)`, `OnDestroy()` (avant destruction de l'entité).
`GetGameObject()` renvoie le porteur (rempli automatiquement à l'attache). `SetEnabled(bool)` bascule
`mEnabled` puis déclenche `OnEnable`/`OnDisable` ; `IsEnabled()` interroge. `GetTypeName()` est **pur
virtuel** : tout script concret doit l'implémenter (débogage), ce qui rend `NkBehaviour` **abstraite**.
Subtilité : **aucune** méthode n'est `noexcept` ici (contrairement à `NkGameObject`/`NkActor`). Les
champs protégés `mGameObject`, `mEnabled`, `mStarted` existent ; `mStarted` est piloté par
`NkBehaviourSystem` — **ne le touchez pas** à la main.

- **Gameplay** — comportements modulaires composables (patrouille, déclencheur, ramassage).
- **IA** — un script par sous-comportement, activés/désactivés via `SetEnabled`.
- **UI / éditeur** — scripts attachés à des objets d'interface ou d'édition.
- **Audio** — un behaviour qui déclenche un son sur un événement de jeu.

### Les trois systèmes de behaviours (implémentés inline)

Ces trois systèmes dérivent de `NkSystem` et ont **tous un corps réel** (`Describe()` + `Execute()`).
Chacun interroge `world.Query<NkBehaviourHost>().Without<NkInactive>().ForEach(…)` et ne déclenche un
hook que si le behaviour existe et est activé (`beh && beh->IsEnabled()`, plus `mStarted` pour
update/late/fixed).

`NkBehaviourSystem` (phase `NkSystemGroup::PreUpdate`, `Writes<NkBehaviourHost>().Reads<NkInactive>()`)
fonctionne en deux temps : **OnStart** — pour chaque host pas encore démarré (`!host.hasStarted`), il
parcourt les behaviours et, pour chacun activé et pas encore démarré, appelle `OnStart()` puis fixe
`mStarted = true`, et marque enfin `host.hasStarted = true` ; **OnUpdate** — il appelle `OnUpdate(dt)`
sur chaque behaviour activé **et** démarré. `NkBehaviourLateSystem` (`NkSystemGroup::PostUpdate`)
appelle `OnLateUpdate(dt)`. `NkBehaviourFixedSystem` (`NkSystemGroup::FixedUpdate`) appelle
`OnFixedUpdate(fixedDt)`.

**À retenir :** ce trio ne déclenche **que** `OnStart`, `OnUpdate`, `OnLateUpdate`, `OnFixedUpdate`.
Les hooks `OnAwake`, `OnEnable`/`OnDisable` et `OnDestroy` sont invoqués **ailleurs** (ajout,
activation, destruction). Les groupes de phase (`PreUpdate`/`PostUpdate`/`FixedUpdate`) sont des valeurs
de `NkSystemGroup`, défini dans NKECS — à qualifier `NkSystemGroup::VALEUR`.

- **Ordonnancement** — la séparation en trois phases place naturellement la logique gameplay (Pre), la
  caméra/suivi (Post) et la physique (Fixed) au bon moment de la frame.
- **Perf** — les behaviours inactifs (`NkInactive`) sont écartés dès la query.

### `NkGameObjectFactory` — création (implémentée inline)

Classe **stateless** du namespace `nkentseu` (constructeur **supprimé**). `Create<T>(world, name,
args…)` (corps inline) vérifie par `static_assert` que `T` dérive de `NkGameObject`, crée l'entité
(`world.CreateEntity()`), ajoute les **six invariants** (`NkName(name)`, `NkTag`, `NkTransform`,
`NkParent`, `NkChildren`, `NkBehaviourHost`), et renvoie `T(id, &world, args…)`. La surcharge **sans
nom** déduit le nom via `NkTypeRegistry::Global().TypeName<T>()` puis délègue à la version nommée.
`CreateBatch<T>(world, baseName, count, out)` (inline, **pas** `[[nodiscard]]`) génère `count` objets
nommés `"<baseName>_<i>"` (via `NkSNPrintf` dans un buffer de 256 octets) et les empile dans le
`NkVector<T>& out`.

**Piège d'ambiguïté :** les deux surcharges `Create` peuvent entrer en conflit ; la version « sans
nom » suppose que le premier argument variadique **n'est pas** convertible en `const char*`. Passer un
`const char*` littéral en premier argument part donc bien vers la version **nommée**.

- **Scène** — peupler une scène au chargement (acteurs, props).
- **Gameplay** — spawn d'entités à la volée (projectiles, pickups), `CreateBatch` pour une vague
  d'ennemis.
- **Éditeur** — instancier un nouvel objet dans le monde courant.

### `NkGameObjectFactory` — prefabs (déclarées seulement)

`InstantiatePrefab(world, prefabPath, instanceName = nullptr)` et `InstantiatePrefabBatch(world,
prefabPath, count, out)` instancient des entités à partir d'un **prefab** sur disque. **Statut :** ces
deux méthodes sont **déclarées seulement** dans le header — leur corps est défini ailleurs et dépend de
`NkPrefab` / `NkPrefabRegistry` (forward-déclarés dans `nkentseu`). C'est donc possiblement de la
**spécification** non encore implémentée ; à confirmer côté source. Usage visé : instancier un modèle
réutilisable (ennemi type, élément de décor) une fois ou en masse.

### Le socle commun

- **Tout passe par la fabrique.** Aucun `NkGameObject` ne devrait naître d'un `new` ou d'un ctor
  direct : `NkGameObjectFactory::Create` (ou `NkSceneGraph::SpawnActor`) garantit les six invariants.
- **Handle, pas objet.** Un `NkGameObject` se copie comme un pointeur ; les données restent dans le
  `NkWorld`. Testez toujours `IsValid()` avant d'agir.
- **Deux axes de comportement.** La **hiérarchie d'acteurs** (`NkActor`…) pour dériver l'objet ; les
  **behaviours** (`NkBehaviour`) pour greffer des scripts composables. Les deux coexistent.
- **Cycle de vie piloté.** Hooks d'acteurs déclenchés par `NkSceneLifecycleSystem`, hooks de behaviours
  par les trois systèmes — jamais à la main.
- **Statut spec.** Hiérarchie parent/enfants, activation/destruction et prefabs sont **déclarés** mais
  non implémentés dans ces headers : à vérifier en source avant de s'y fier.

---

### Exemple

```cpp
#include "Noge/ECS/Factory/NkGameObjectFactory.h"
#include "Noge/ECS/Entities/NkActor.h"
#include "Noge/ECS/Entities/NkBehaviour.h"
using namespace nkentseu;            // NkGameObjectFactory
using namespace nkentseu::ecs;       // NkCharacter, NkBehaviour, NkGameObject

// 1) Un personnage : on dérive NkCharacter, on surcharge BeginPlay (base d'abord).
class Hero : public NkCharacter {
public:
    using NkCharacter::NkCharacter;
    void BeginPlay() noexcept override {
        NkCharacter::BeginPlay();                         // ajoute controller/mesh/animator/audio
        if (auto cc = GetComponent<NkCharacterController>()) cc->speed = 6.f;
        Possess(0);                                       // contrôlé par le joueur 0
    }
    void Tick(float32 dt) noexcept override { /* déplacement, visée… */ }
};

// 2) Un script composable, attachable à n'importe quel GameObject.
class Spinner : public NkBehaviour {
public:
    const char* GetTypeName() const override { return "Spinner"; }   // pur virtuel : obligatoire
    void OnUpdate(float dt) override { /* fait tourner GetGameObject() */ }
};

// 3) Création — toujours via la fabrique, jamais "new".
auto hero = NkGameObjectFactory::Create<Hero>(world, "Hero");        // handle typé Hero
hero.AddBehaviour<Spinner>();                                        // greffe + lie le script

NkVector<NkGameObject> orcs;
NkGameObjectFactory::CreateBatch(world, "Orc", 8, orcs);             // Orc_0 … Orc_7

// 4) Accès composants selon la certitude de présence.
hero.SetPosition(0.f, 0.f, 0.f);                                     // raccourci Transform
if (auto rb = hero.GetComponent<NkRigidBody>()) { /* nullable */ }   // peut être absent
hero.RequireComponent<NkTransform>().Get();                          // invariant : assert si absent
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
