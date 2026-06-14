# Le scripting visuel et la réplication réseau

> Couche **Engine** · Noge · Deux sous-systèmes de spécification : le **scripting visuel**
> (*Blueprint*, graphes de nœuds à la Unreal) et la **réplication réseau** (entités, snapshots,
> prédiction client). Les deux décrivent une API ; aucun n'est encore opérationnel.

Quand on veut donner du **comportement** à une entité sans recompiler — un déclencheur, une porte
qui s'ouvre, un dialogue, une réaction d'IA —, deux familles d'outils s'imposent dans un moteur. La
première est le **scripting visuel** : on relie des **nœuds** par des fils plutôt que d'écrire du
code, et le designer construit la logique à la souris. La seconde, quand le jeu devient
multijoueur, est la **réplication** : faire qu'une entité créée sur le serveur **existe et bouge de
la même façon** chez tous les clients, malgré la latence. Cette page documente les deux sous-systèmes
tels qu'ils sont **spécifiés** dans Noge.

Un avertissement franc d'abord, parce qu'il conditionne toute la lecture : **ces quatre headers sont
des fichiers de spécification, pas du code en service.** Le module *Blueprint* contient du code inline
qui *ressemble* à une implémentation, mais il est truffé de stubs assumés (« logique fictive »,
« Simplifié »), d'un `GetInput` bugué qui ne propage jamais les données, et il viole la règle
zéro-STL du projet (`std::vector`, `std::function`, `typeid`/RTTI). Le module *réseau*, lui, est une
déclaration de classes **sans aucun `.cpp`** : presque toutes ses méthodes n'ont pas de corps. Pire,
plusieurs incohérences de **casse** (PascalCase contre camelCase) entre les headers les empêchent de
compiler ensemble. Ce n'est donc **pas** une API utilisable telle quelle : c'est une intention
d'architecture, à reprendre. Chaque section signale précisément ce qui marche, ce qui est un stub,
et ce qui ne compile pas.

- **Namespace** : `nkentseu::ecs::blueprint` (scripting visuel) · `nkentseu::ecs` (réseau)
- **Headers** :
  `Noge/ECS/VisualScript/NkBlueprint.h`,
  `Noge/ECS/VisualScript/NkBlueprintHotReload.h`,
  `Noge/ECS/VisualScript/NkValidGraph.h`,
  `Noge/ECS/Replication/NkNetWorld.h`

> **En résumé.** Deux sous-systèmes **de spec** : *Blueprint* (graphes de nœuds, code inline mais
> stubbé et non zéro-STL) et *réplication réseau* (déclarations sans `.cpp`). Aucun n'est prêt à
> l'emploi ; lisez les avertissements « stub »/« ne compile pas » avant d'y toucher.

---

## Le graphe de nœuds : `NkBlueprintGraph` et ses pièces

L'idée du scripting visuel est de représenter un programme comme un **graphe** : des **nœuds**
(les actions, les opérations, les événements) reliés par des **fils** (les *pins*). Deux familles de
fils cohabitent et il faut bien les distinguer. Les fils d'**exécution** (`Exec`) disent *dans quel
ordre* les nœuds s'enchaînent — c'est le flux de contrôle, la « ligne blanche » de Unreal. Les fils
de **données** (`Bool`, `Int`, `Float`, `Vec3`, `EntityId`…) transportent des **valeurs** d'une
sortie vers une entrée. Un nœud reçoit ses entrées, fait son travail dans `Execute`, écrit ses
sorties, et passe la main au nœud suivant par un fil d'exécution.

Concrètement, un `NkBlueprintGraph` détient un tableau de `Nodes` (chacun dérive de la classe de
base `NkBlueprintNode`), un tableau de `Connections` (`NkBlueprintConnection`, qui relie un
`SourcePin` d'un `SourceNode` à un `TargetPin` d'un `TargetNode`), et un `EntryNodeIndex` (par où
commencer). `AddNode` ajoute un nœud, `Link` crée un fil et marque le pin cible comme connecté, et
`Execute` parcourt le graphe à partir de l'entrée en suivant les fils d'exécution.

Ce n'est **pas** un évaluateur de dataflow complet. La propagation des **données** entre nœuds est
là où la spec s'arrête : la routine privée `ResolveInputs` est un **quasi no-op** (son commentaire
le dit : « il faut évaluer les dépendances de données avant l'exécution » — ce n'est pas fait), et
l'accesseur `NkBlueprintNode::GetInput` renvoie **toujours** la valeur par défaut du pin
(`return … ? DefaultValue : DefaultValue;`, un bug visible). Autrement dit : le flux **d'exécution**
se parcourt, mais les **valeurs ne circulent pas vraiment** entre les nœuds. C'est le premier
chantier d'une implémentation réelle.

> **En résumé.** Un graphe = `Nodes` + `Connections` + `EntryNodeIndex`. Deux types de fils : `Exec`
> (ordre d'exécution) et données (valeurs). `Execute` suit les fils Exec ; mais la propagation de
> données (`ResolveInputs`, `GetInput`) est **stubbée/buguée** — les valeurs ne se propagent pas.

---

## Décrire un pin : types, valeurs et union

Avant de relier quoi que ce soit, il faut **typer** les pins. `NkPinPrimitiveType` énumère les
sortes de fils (`Exec`, `Bool`, `Int`, `Float`, `Vec2/3/4`, `String`, `EntityId`, `GameObject`, plus
`None`), et `NkPinDirection` dit si un pin est `Input` ou `Output`. Un `NkPinType` combine une
primitive et, si besoin, un **type custom** (`NkTypeId` — un hash FNV-1a du nom de structure/classe).
Ses petits prédicats (`IsExec`, `IsData`, `IsCustom`) servent à valider les connexions : on ne
branche pas un fil d'exécution sur un pin de données.

La valeur transportée par un fil de données est un `NkValue` : un `NkPinType` plus une **union** qui
loge un `bool`, un `int32`, un `float`, un `NkVec3`, un `entityId` (`uint64`) ou un `void*`. On ne
construit pas un `NkValue` à la main : on passe par ses **fabriques statiques** (`NkValue::Bool(v)`,
`Int`, `Float`, `Vec3`, `Entity(id)`, `Ptr(p, type)`, `String(s)`, `Exec()`), et on relit par les
accesseurs typés (`AsBool`, `AsInt`, `AsFloat`, `AsVec3`, `AsEntityId`, ou `AsPtr<T>()` pour un
pointeur reconverti). À noter : `String` est ici « simplifié » — il stocke juste le `const char*`
dans le `void*` de l'union, sans copie, donc sans gestion de durée de vie.

Ce n'est **pas** un système de variant générique à la `std::variant` : l'union est plate, fixe, et
ne suit pas le type (un `AsFloat()` sur une valeur rangée comme `Int` lit des octets bruts). La
discipline vient du `NkPinType` qui l'accompagne, pas de l'union elle-même.

> **En résumé.** `NkPinType` = primitive (`NkPinPrimitiveType`) + éventuel type custom (`NkTypeId`).
> `NkValue` = type + union plate ; on le crée par fabriques (`NkValue::Float(…)`) et on le relit par
> accesseurs (`AsFloat()`). Union non typée à l'exécution : c'est le `NkPinType` qui garde la
> cohérence.

---

## Écrire un nœud : `NkBlueprintNode` et l'usine

Tout nœud dérive de `NkBlueprintNode`, qui porte un `Name`, un `NodeTypeId`, un drapeau `Enabled`,
ses listes de pins `Inputs`/`Outputs`, et **une seule méthode à implémenter** : la pure virtuelle
`Execute(NkWorld&, NkEntityId self, float dt)`. Un nœud lit ses entrées (`GetInput`, **bugué** comme
vu plus haut), agit sur le monde ECS, et écrit ses sorties (`SetOutput`). `GetCategory()` range le
nœud dans une rubrique de la palette (« Math », « Physics », « Events »…).

Pour qu'un éditeur puisse **créer un nœud par son nom** (au chargement d'un graphe sérialisé, par
exemple), Noge fournit une **usine** : le singleton `NkNodeRegistry`. On y enregistre un type avec
`Register<T>("Nom")` (plafonné à 512 entrées, nom tronqué à 127 caractères), et on instancie avec
`Create("Nom")` (recherche linéaire, `nullptr` si introuvable). La macro
`NK_REGISTER_BLUEPRINT_NODE(Type)` automatise cet enregistrement à l'initialisation statique — un
idiome classique d'auto-déclaration.

Le header livre une **palette de nœuds concrets** prête à servir d'exemples, mais leur logique est
souvent **fictive** : à connaître plutôt qu'à utiliser tel quel. `NkNodeEventBeginPlay` /
`NkNodeEventCustom` sont des points d'entrée (Execute vide). `NkNodeCallFunction` (nommé
`"PrintString"`) a son log **commenté** (no-op). `NkNodeAddFloat`, `NkNodeMakeVector3`,
`NkNodeSwitchInt` couvrent maths/structs/contrôle de flux. `NkNodeRaycast` retourne **toujours
*Miss*** (`bool hit = false` codé en dur — « logique fictive »). `NkNodeSpawnActor` crée une entité.
Une famille de nœuds de **cast** complète l'ensemble (`NkNodeCastIntToFloat`, `…FloatToInt`,
`…EntityToGameObject`, `NkNodeCastToType`, `NkNodeCastActorToClass`, et le template
`NkNodeCastToComponent<T>` qui utilise `world.Get<T>()` **via `typeid`/RTTI**). Seuls les trois
premiers casts sont **auto-enregistrés** ; les autres doivent l'être à la main.

> **En résumé.** Hériter de `NkBlueprintNode`, override `Execute`, peupler `Inputs`/`Outputs`.
> L'usine `NkNodeRegistry` (`Register<T>` / `Create`) + la macro `NK_REGISTER_BLUEPRINT_NODE`
> instancient par nom. La palette fournie est surtout démonstrative (Raycast toujours Miss, PrintString
> no-op).

---

## Brancher le graphe sur une entité : `NkBlueprintComponent`

Pour qu'un graphe **vive** dans le jeu, on l'attache à une entité via un composant. Le
`NkBlueprintComponent` dérive de `NkScriptComponent` (le pont script de l'ECS) et porte un `Graph`.
Il s'insère dans le cycle de vie ECS : `OnStart` déclenche l'événement `"EventBeginPlay"`, `OnUpdate`
déclenche `"EventTick"`. Sous le capot, sa routine privée `TriggerEvent` cherche le nœud portant le
nom de l'événement, fixe l'`EntryNodeIndex` dessus, et exécute le graphe.

Deux réserves de spec, à voir comme des bugs à corriger : `OnUpdate` cherche un nœud `"EventTick"`
qui **n'existe dans aucun nœud fourni** par ce header (il faudra le créer), et `TriggerEvent` exécute
le graphe avec un **`dt = 0.f` codé en dur** — donc tout nœud dépendant du temps recevra zéro.

> **En résumé.** `NkBlueprintComponent` (dérive de `NkScriptComponent`) attache un `Graph` à une
> entité et le pilote par les hooks ECS (`OnStart`→BeginPlay, `OnUpdate`→Tick). Réserves : nœud
> `"EventTick"` absent et `dt` forcé à 0.

---

## Recharger à chaud et valider : `NkBlueprintHotReloadManager`, `NkValidGraph`

Le confort d'un éditeur visuel tient au **hot-reload** : modifier un graphe sur disque et le voir
appliqué **sans relancer** le jeu, en préservant l'état en cours. C'est le rôle du
`NkBlueprintHotReloadManager`. On enregistre un composant avec son fichier (`RegisterComponent`), on
appelle `Poll` chaque frame ; quand le fichier a changé, le manager **capture** les valeurs par
défaut des pins, recharge le graphe depuis le disque, **migre** les valeurs par clé
(`"NodeName_PinIndex"`), remplace l'ancien graphe et notifie via un callback `SetOnReloaded`.

Sauf que, en l'état, **ça ne marche pas** : la détection de changement `GetFileTime` est un **stub
explicite** qui ignore le chemin et renvoie l'heure courante — branché tel quel, le graphe se
rechargerait **à chaque `Poll`**. Et le rechargement dépend de `LoadBlueprintFromFile` (namespace
`serialization`), qui n'est **pas déclaré** dans ces headers.

La **validation** d'un graphe (avant exécution) vit dans `NkValidGraph` : `ValidateGraph` rejette un
graphe vide et vérifie que chaque connexion référence des nœuds/pins dans les bornes ;
`CompactGraph` retire les connexions vers des nœuds invalides. Mais — autre limite de spec — il n'y a
**pas de détection de cycle réelle** (malgré le commentaire), la réindexation après compactage est
« omise », et la sérialisation est faite de **stubs** : `SerializeBlueprint` n'émet qu'un comptage
`{"nodes":N,"connections":M}`, `DeserializeBlueprint` renvoie toujours `false`.

> **En résumé.** Hot-reload **non fonctionnel** : `GetFileTime` est un stub (recharge en boucle) et
> `LoadBlueprintFromFile` n'est pas fourni. Validation partielle (`ValidateGraph`/`CompactGraph`)
> sans détection de cycle ; sérialisation = stubs.

---

## La réplication réseau : `NkNetWorld` et compagnie

Changement complet de sous-système. Quand un jeu passe en multijoueur, une entité ne suffit plus à
exister localement : il faut qu'elle ait une **identité réseau** stable, un **propriétaire**, une
**autorité** (qui décide de son état — serveur, client, ou partagé), et qu'on sache l'**envoyer** sur
le fil puis la **reconstruire** ailleurs. `NkNetWorld` est la façade de tout cela, posée au-dessus du
`NkWorld` ECS : `Init` l'attache à un monde et à un gestionnaire de connexions, `SpawnNetEntity`
(serveur) crée une entité répliquée, `Update(dt)` fait la réplication par frame, `ApplySnapshot`
applique un état reçu et réconcilie, `FindByNetId` retrouve une entité par son identifiant réseau.

Autour gravitent les briques de données, toutes des composants ECS (`NK_COMPONENT`) :
`NkNetEntity` (identité, propriétaire, autorité, drapeaux *dirty*/spawn/destroy, priorité,
pertinence), `NkNetInput` (l'entrée d'un joueur à un *tick* donné : direction, yaw/pitch, boutons),
`NkNetSnapshot` (un instantané d'état : position/vitesse/rotation/échelle + tampon brut), et
`NkNetRelevanceZone` (la sphère autour de laquelle une entité est jugée pertinente à répliquer).
Trois aides complètent l'édifice : `NkNetSystem` (un `NkSystem` ECS qui appelle `Update` dans le
groupe `PostUpdate`), `NkNetInterpolator` (un tampon anti-jitter qui lisse les snapshots distants),
et les **callbacks** (`onEntitySpawned`, `onEntityDestroyed`, `onInputReceived`).

Ce n'est **pas une couche réseau opérationnelle**. **Aucun `.cpp` n'accompagne ce header** : toutes
les méthodes substantielles (`Init`, `SpawnNetEntity`, `Update`, `ApplySnapshot`, la sérialisation
d'entité…) sont **déclarées sans corps**. Seuls quelques accesseurs inline existent (`IsServer`,
`GetTick`). C'est une **maquette d'API**, pas un moteur réseau.

> **En résumé.** `NkNetWorld` = façade de réplication sur le `NkWorld` (spawn serveur, snapshots,
> réconciliation), entourée de composants (`NkNetEntity`/`NkNetInput`/`NkNetSnapshot`) et d'aides
> (`NkNetSystem`, `NkNetInterpolator`). **Sans `.cpp`** : presque tout est non implémenté.

---

## Aperçu de l'API

Toutes les entrées ci-dessous sont **de spécification**. La colonne Rôle signale les stubs et les
non-implémentés. Le détail (et chaque piège) est repris dans la « Référence complète ».

### Scripting visuel — `nkentseu::ecs::blueprint` (`NkBlueprint.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkPinPrimitiveType` (`None`,`Exec`,`Bool`,`Int`,`Float`,`Vec2/3/4`,`String`,`EntityId`,`GameObject`) | Sorte d'un pin (exécution ou type de donnée). |
| Enums | `NkPinDirection` (`Input`,`Output`) | Sens d'un pin. |
| Type | `NkTypeId` | Identifiant de type custom (hash FNV-1a + nom). |
| Type | `NkPinType` · `IsExec`/`IsData`/`IsCustom` | Type d'un pin + prédicats de catégorie. |
| Valeur | `NkValue` (union) · fabriques `Bool/Int/Float/Vec3/Entity/Ptr/String/Exec` · `AsBool/AsInt/AsFloat/AsVec3/AsEntityId/AsPtr<T>` | Valeur transportée par un fil de données. |
| Pin | `NkBlueprintPin` | Pin d'un nœud (nom, type, sens, valeur par défaut). |
| Nœud | `NkBlueprintNode` · `Execute` (pur) · `GetInput`/`SetOutput` · `GetCategory` | Classe de base d'un nœud ; `GetInput` **bugué**. |
| Usine | `NkNodeRegistry` · `Register<T>` · `Create` · macro `NK_REGISTER_BLUEPRINT_NODE` | Création de nœuds par nom (singleton, 512 max). |
| Nœuds concrets | `NkNodeEventBeginPlay/EventCustom`, `NkNodeCallFunction`("PrintString"), `NkNodeAddFloat`, `NkNodeMakeVector3`, `NkNodeSwitchInt`, `NkNodeRaycast`, `NkNodeSpawnActor`, casts (`…IntToFloat`,`…FloatToInt`,`…EntityToGameObject`,`NkNodeCastToType`,`NkNodeCastToComponent<T>`,`NkNodeCastActorToClass`) | Palette **démonstrative** (Raycast toujours Miss, PrintString no-op). |
| Graphe | `NkBlueprintConnection` · `NkBlueprintGraph` · `AddNode`/`Link`/`Execute` | Graphe de nœuds + connexions ; `ResolveInputs` no-op. |
| Composant | `NkBlueprintComponent` (`: NkScriptComponent`) · `OnStart`/`OnUpdate`/`GetTypeName` | Attache un graphe à une entité (réserves : `dt=0`, `"EventTick"` absent). |

### Hot-reload & validation — `nkentseu::ecs::blueprint`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Hot-reload | `NkBlueprintHotReloadManager` · `RegisterComponent`/`UnregisterComponent`/`Poll`/`SetOnReloaded` | Recharge un graphe modifié ; `GetFileTime` **stub** (recharge en boucle). |
| Hot-reload | `NkBlueprintStateSnapshot` | État capturé (valeurs de pins, en cours ?). |
| Validation | `ValidateGraph` · `CompactGraph` | Vérifie/nettoie un graphe (pas de détection de cycle). |
| Sérialisation | `SerializeBlueprint` · `DeserializeBlueprint` | **Stubs** (comptage seul / renvoie `false`). |

### Réplication réseau — `nkentseu::ecs` (`NkNetWorld.h`, sans `.cpp`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkNetAuthority` (`Server`,`Client`,`Shared`,`NoAuthority`) | Qui fait autorité sur l'état. |
| Macro | `NK_NET_COMPONENT(T)` | Marque conceptuelle d'un composant répliqué (no-op). |
| Composants | `NkNetEntity`, `NkNetInput`, `NkNetRelevanceZone` | Identité réseau, entrée joueur, zone de pertinence. |
| Donnée | `NkNetSnapshot` · `IsValid` | Instantané d'état (pos/vel/rot/scale + tampon). |
| Monde | `NkNetWorld` · `Init`/`SpawnNetEntity`/`DestroyNetEntity`/`TransferAuthority`/`SubmitInput`/`ApplyPendingInputs`/`Update`/`ApplySnapshot`/`FindByNetId` | Façade de réplication — **non implémentée**. |
| Monde | `IsServer`/`GetTick` (inline) · config (`replicationRate`…) · callbacks (`onEntitySpawned`…) | Accesseurs et réglages exposés. |
| Système | `NkNetSystem` (`: NkSystem`) · `Describe`/`Execute` | Branche `NkNetWorld::Update` dans `PostUpdate`. |
| Anti-jitter | `NkNetInterpolator` · `Push`/`Sample`/`SetDelay`/`GetDelay` | Lisse les snapshots distants (`Push`/`Sample` sans corps). |

---

## Référence complète

Rappel de cadre : **rien de tout cela n'est en service**. La suite explique l'intention de chaque
élément, son cas d'usage par domaine, **et** son état réel (implémenté inline / stub / déclaré sans
corps), pour qu'une future implémentation sache exactement quoi reprendre.

### Les types de pins : `NkPinPrimitiveType`, `NkPinDirection`, `NkPinType`, `NkTypeId`

`NkPinPrimitiveType` est l'ABC du graphe : un pin est soit un fil d'**exécution** (`Exec`), soit un
fil de **donnée** d'un type précis (`Bool`, `Int`, `Float`, `Vec2/3/4`, `String`, `EntityId`,
`GameObject`), soit `None` (réservé aux types custom). `NkPinDirection` (`Input`/`Output`) dit de
quel côté du nœud se trouve le pin. `NkPinType` empaquette une primitive et un `NkTypeId` optionnel,
et fournit les prédicats qui rendent un éditeur sûr :

- **Éditeur / UI** — au moment de tirer un fil entre deux pins, `IsExec()`/`IsData()` valident la
  compatibilité (on ne relie pas une ligne d'exécution à une donnée), et `IsCustom()` détecte un pin
  de type structure défini par l'utilisateur.
- **Gameplay / IA** — les primitives `EntityId` et `GameObject` permettent de faire transiter une
  référence d'entité d'un nœud à l'autre (cible d'un tir, acteur à activer).
- **Animation / scène** — `Vec3` couvre positions, échelles, axes ; un graphe peut composer une
  transformation à partir de ses composantes.

`NkTypeId` identifie un type custom par un **hash FNV-1a** de son nom (`Hash`) plus le pointeur de
nom (`Name`). Son constructeur depuis `const char*` n'est **pas** `explicit` (conversion implicite
pratique), et son `operator==` compare **et** le hash **et** le pointeur de nom — un point de
fragilité (deux `const char*` identiques mais à des adresses différentes seront jugés inégaux).

### La valeur transportée : `NkValue`

`NkValue` est ce qui circule sur un fil de données : un `NkPinType` accolé à une **union** plate
(`bool`/`int32`/`float`/`NkVec3`/`uint64`/`void*`). On ne touche jamais l'union directement — on
**fabrique** (`NkValue::Float(3.f)`, `NkValue::Entity(id)` qui range `id.Pack()`,
`NkValue::Ptr(p, type)` pour un objet custom) et on **relit** par accesseur (`AsFloat()`,
`AsEntityId()` qui fait `NkEntityId::Unpack`, `AsPtr<T>()` qui `static_cast` le `void*`). Usages :

- **Math / gameplay** — un nœud `AddFloat` lit deux `NkValue::Float`, additionne, écrit un
  `NkValue::Float`.
- **ECS / scène** — `NkValue::Entity` transporte la référence d'une entité spawnée vers le nœud
  suivant ; `AsPtr<T>()` reconstruit un pointeur de composant.
- **Limite** — `NkValue::String` est « simplifié » : il stocke le `const char*` brut dans le `void*`,
  **sans copie ni durée de vie gérée**. Ne pas s'y fier pour du texte dynamique. L'union n'étant pas
  typée à l'exécution, lire avec le mauvais accesseur (`AsInt` sur un `Float`) lit des octets bruts.

### Les pins et les nœuds : `NkBlueprintPin`, `NkBlueprintNode`

Un `NkBlueprintPin` décrit un point de branchement : `Name`, `NkPinType`, `NkPinDirection`,
`DefaultValue` (la valeur quand rien n'est branché) et `IsConnected`. Un `NkBlueprintNode` regroupe
ses pins (`Inputs`/`Outputs`), porte un `Name`/`NodeTypeId`/`Enabled`, et impose **une** méthode :
`Execute(NkWorld&, NkEntityId self, float dt)`, où le nœud agit sur l'ECS. `GetCategory()` classe le
nœud dans la palette.

Deux défauts de spec, centraux pour quiconque reprend le code :

- **`GetInput(index)` est bugué** : `return … ? DefaultValue : DefaultValue;`. Il renvoie **toujours**
  la valeur par défaut du pin, jamais une valeur **propagée** depuis un nœud amont, et ne vérifie pas
  les bornes. Tant que ce n'est pas corrigé, un nœud ne peut pas lire ce que lui envoie son
  prédécesseur.
- `SetOutput(index, value)` fonctionne (écrit `Outputs[index].DefaultValue` si l'index est valide),
  mais sans une vraie propagation côté entrée, l'écriture reste « locale » au nœud.

### Créer des nœuds par nom : `NkNodeRegistry` et la macro

Un éditeur charge des graphes depuis le disque : il lui faut **instancier un nœud à partir de son
nom**. `NkNodeRegistry` est le singleton (Meyers, `Global()`) qui rend cela possible :
`Register<T>("Nom")` mémorise une fabrique (`std::make_unique<T>()`, plafond **512** entrées, nom
tronqué à 127 caractères), `Create("Nom")` retrouve la fabrique par **recherche linéaire** et renvoie
un `unique_ptr` (ou `nullptr`). La macro `NK_REGISTER_BLUEPRINT_NODE(Type)` crée un objet statique
anonyme dont le constructeur appelle `Register<Type>(#Type)` au chargement — **auto-enregistrement**
sans code d'init centralisé.

- **Éditeur / outils** — peupler la palette de nœuds, désérialiser un graphe (`Create` pour chaque
  nœud nommé), supporter des nœuds **plug-in** ajoutés par auto-enregistrement.
- **Note style** — `FactoryFn` est un `std::function` et `Create` rend un `std::unique_ptr` : ce
  sous-module **n'est pas zéro-STL** (voir « Pièges »).

### La palette de nœuds concrets

Le header fournit des nœuds prêts à instancier, surtout pour **illustrer** la mécanique. À traiter
comme des exemples, leur logique étant souvent fictive :

- **Événements** — `NkNodeEventBeginPlay` et `NkNodeEventCustom` (pin `Damage` en `Float`) sont des
  points d'entrée à `Execute` vide ; ce sont des ancres pour `TriggerEvent`.
- **Flux de contrôle** — `NkNodeCallFunction` (nommé `"PrintString"`, log **commenté** → no-op) et
  `NkNodeSwitchInt` (sorties Exec `0`/`1`/`Default`) structurent l'enchaînement.
- **Math / structs** — `NkNodeAddFloat` (`A+B`) et `NkNodeMakeVector3` (X/Y/Z → `Vec3`).
- **Physique** — `NkNodeRaycast` **retourne toujours *Miss*** (`bool hit = false` codé en dur,
  « logique fictive ») : à brancher sur le vrai module de collision lors de l'implémentation.
- **Spawning / ECS** — `NkNodeSpawnActor` appelle `world.CreateEntity()`.
- **Casts (utilitaires)** — `NkNodeCastIntToFloat`, `NkNodeCastFloatToInt` (tronque),
  `NkNodeCastEntityToGameObject` (vérifie `IsValid()` + `world.IsAlive()`), `NkNodeCastToType` et
  `NkNodeCastActorToClass` (constructeurs `explicit` prenant un `NkTypeId` cible, vérification par
  Hash ; le second passe par `world.Get<NkTag>()`), et le template `NkNodeCastToComponent<T>` (utilise
  `world.Get<T>()` **via `typeid`/RTTI**). **Seuls** `…IntToFloat`, `…FloatToInt` et
  `…EntityToGameObject` sont auto-enregistrés en fin de header ; les autres sont à enregistrer
  manuellement.

### Le graphe et son exécution : `NkBlueprintConnection`, `NkBlueprintGraph`

`NkBlueprintConnection` relie quatre indices : `SourceNode`/`SourcePin` → `TargetNode`/`TargetPin`.
`NkBlueprintGraph` détient `Nodes`, `Connections`, `EntryNodeIndex`. `AddNode` déplace un nœud dans le
graphe ; `Link` ajoute une connexion et marque le pin cible `IsConnected = true` ; `Execute` parcourt
le graphe **itérativement** depuis `EntryNodeIndex` à l'aide d'une pile, appelle `ResolveInputs` puis
`node->Execute`, et suit les sorties d'exécution.

- **Gameplay / scène** — un graphe « porte » : `EventBeginPlay` → vérifier une clé → ouvrir → jouer
  un son. Le flux d'exécution se parcourt correctement.
- **Limite majeure** — `ResolveInputs` est un **quasi no-op** (la propagation de données n'est pas
  implémentée), et `Execute` **suppose** qu'un nœud exécuté active **toutes** ses sorties Exec
  (simplification, pas de branchement conditionnel réel). `FindTargetNode` renvoie `0xFFFFFFFF` quand
  aucune cible n'est trouvée. Donc : le graphe **chaîne** les nœuds mais ne fait pas **circuler les
  valeurs**.

### Attacher au jeu : `NkBlueprintComponent`

`NkBlueprintComponent` dérive de `NkScriptComponent` et porte le `Graph`. Il s'intègre au cycle ECS :
`OnStart` déclenche `"EventBeginPlay"`, `OnUpdate` déclenche `"EventTick"`, `GetTypeName()` renvoie
`"NkBlueprintComponent"`. La routine privée `TriggerEvent(name)` cherche le nœud du bon `Name`, place
l'`EntryNodeIndex` dessus et exécute.

- **Gameplay / IA** — donner un comportement scriptable à un PNJ, un piège, un objet interactif, sans
  recompiler.
- **Réserves** — aucun nœud `"EventTick"` n'est défini dans le header (`OnUpdate` ne trouvera rien à
  exécuter tant qu'on n'en crée pas un), et `TriggerEvent` exécute avec **`dt = 0.f`** : tout nœud
  sensible au temps recevra zéro. À corriger pour propager le vrai `dt`.

### Recharger à chaud : `NkBlueprintHotReloadManager`, `NkBlueprintStateSnapshot`

Le hot-reload permet d'**itérer sans relancer** : on modifie le graphe sur disque, le moteur le
recharge en préservant l'état. `RegisterComponent(comp, path)` associe un composant à un fichier (si
les deux sont valides), `Poll(world, self)` vérifie chaque frame si le fichier a changé et déclenche
le rechargement, `SetOnReloaded(fn)` notifie le résultat. En interne, `CaptureState` mémorise les
valeurs par défaut des pins d'entrée dans un `NkBlueprintStateSnapshot` (clé `"NodeName_PinIndex"`),
puis `ReloadGraph` recharge, **migre** les valeurs par clé, échange l'ancien graphe et appelle le
callback.

- **Éditeur / outils** — boucle d'itération designer : ajuster un graphe et voir l'effet
  immédiatement, en gardant les réglages en cours.
- **Pourquoi ça ne marche pas** — `GetFileTime(path)` est un **stub assumé** : il **ignore le chemin**
  et renvoie l'heure courante, donc `Poll` croit que le fichier change **en permanence** (rechargement
  en boucle). De plus, `ReloadGraph` appelle `serialization::LoadBlueprintFromFile`, **non déclaré**
  dans ces headers, et l'exemple courant invoque un `Instance()` que la classe **n'expose pas** (pas
  de singleton). En clair : maquette, pas fonctionnalité.

### Valider et sérialiser : `NkValidGraph`

`ValidateGraph(graph)` refuse un graphe vide et vérifie que chaque connexion pointe vers des
nœuds/pins **dans les bornes** ; `CompactGraph(graph)` retire (via `remove_if`) les connexions vers
des nœuds invalides ou nuls.

- **Robustesse** — passer la validation avant `Execute` évite les accès hors bornes sur un graphe
  corrompu (chargé d'un fichier douteux, édité à la main).
- **Limites** — **pas de détection de cycle** réelle malgré le commentaire ; la **réindexation** des
  connexions après compactage est « omise » (les indices peuvent donc devenir faux). Et la
  sérialisation est faite de **stubs** : `SerializeBlueprint` n'émet que `{"nodes":N,"connections":M}`
  (un comptage), `DeserializeBlueprint` ne fait **rien** et renvoie toujours `false`. Le hot-reload n'a
  donc **aucun backend de sérialisation fonctionnel** ici.
- **Détail de structure** — `NkValidGraph.h` n'a **pas de `#pragma once`** et définit des fonctions
  **non-inline** : c'est un `.cpp` déguisé, à compiler **une seule fois**, pas à inclure dans
  plusieurs unités (sinon erreurs de symbole multiple).

### L'autorité et les composants réseau : `NkNetAuthority`, `NkNetEntity`, `NkNetInput`, `NkNetSnapshot`, `NkNetRelevanceZone`

En réseau, la première question est **qui décide**. `NkNetAuthority` (`Server`, `Client`, `Shared`,
`NoAuthority`) qualifie l'autorité d'une entité. Elle vit dans `NkNetEntity` (composant) avec son
identité (`netId`), son `ownerId`, ses drapeaux (`dirty`, `pendingSpawn`, `pendingDestroy`,
`isLocal`), son `spawnTick` et ses curseurs `priority`/`relevance` (pour arbitrer la bande passante).

- **Gameplay** — `NkNetInput` capture l'**intention** d'un joueur à un `tick` (direction, `yaw`/`pitch`,
  `jump`/`attack`/`interact`, `buttons`) : le client l'envoie, le serveur la rejoue. Ses méthodes
  `Serialize(NkBitWriter&)`/`Deserialize(NkBitReader&)` sont **déclarées sans corps**.
- **Synchronisation** — `NkNetSnapshot` est l'**instantané** d'état d'une entité à un `tick`
  (`position`/`velocity`/`rotation`/`scale` + un tampon brut `data[512]`), avec un `timestamp` ;
  `IsValid()` (inline) teste `netId.IsValid()`.
- **Pertinence / scène** — `NkNetRelevanceZone` (composant) porte un `center`, un `radius` et un
  `alwaysRadius` : on ne réplique une entité qu'aux clients pour qui elle est **pertinente** (proche),
  ce qui économise la bande passante sur les grandes scènes.

La macro `NK_NET_COMPONENT(T)` **n'impose rien** : définie deux fois, elle se réduit à un commentaire
no-op. C'est un **marqueur conceptuel** (« ce composant est répliqué »), pas un contrat compilé.

### La façade : `NkNetWorld`

`NkNetWorld` orchestre la réplication au-dessus du `NkWorld` ECS. Côté **serveur** : `SpawnNetEntity`
crée une entité répliquée, `ApplyPendingInputs` rejoue les entrées reçues, `Update(dt)` diffuse l'état
chaque frame, `TransferAuthority` change de propriétaire. Côté **client** : `SubmitInput` envoie
l'intention locale, `ApplySnapshot` applique un état serveur et **réconcilie** (prédiction). Communs :
`FindByNetId`, `DestroyNetEntity`, et les accesseurs inline `IsServer`/`GetTick`. La config est en
membres directs (`replicationRate = 20`, `inputSendRate = 60`, `interpolateRemote`, `interpDelay`,
`enablePrediction`, `enableRollback`), et trois **callbacks** (`onEntitySpawned`, `onEntityDestroyed`,
`onInputReceived`) — typés avec `NkFunction` (le type maison, **contraste** avec le `std::function` du
côté Blueprint).

- **Gameplay multijoueur** — un FPS : le serveur fait autorité sur les positions, les clients
  prédisent leur propre mouvement (`enablePrediction`) et réconcilient sur les snapshots ; le
  *rollback* (`enableRollback`) est un drapeau prévu pour les jeux à netcode déterministe.
- **Scène / pertinence** — combiné à `NkNetRelevanceZone`, `Update` ne réplique qu'aux clients
  concernés.
- **État réel** — **toutes** ces méthodes substantielles sont **déclarées sans corps** (aucun `.cpp`),
  de même que les routines privées `SerializeEntity`/`DeserializeEntity`/`SerializeDelta` et l'historique
  `SnapHistory::GetAt`. La classe est **non copiable** (copie/affectation `= delete`). C'est une **pure
  spec d'API**.

### Les aides : `NkNetSystem`, `NkNetInterpolator`

`NkNetSystem` (dérive de `NkSystem`, `final`) est le point d'**intégration ECS** : son `Describe()`
(inline, idiome **builder fluide** —
`Reads<NkNetEntity>().Reads<NkTransform>().InGroup(NkSystemGroup::PostUpdate).WithPriority(50.f).Sequential().Named("NkNetSystem")`)
le range en `PostUpdate`, et son `Execute` appelle `mNetWorld->Update(dt)` si le monde réseau existe.
C'est, en pratique, la façon dont la réplication s'insère dans la boucle de jeu.

`NkNetInterpolator` lutte contre le **jitter** des entités distantes. On lui `Push` les snapshots
reçus et on `Sample(renderTime)` un état lissé en arrière dans le temps (d'un `delay` réglable par
`SetDelay`/`GetDelay`) : au lieu de téléporter une entité distante à chaque paquet, on **interpole**
entre deux snapshots récents pour un mouvement fluide malgré la latence variable.

- **Animation / rendu réseau** — afficher les autres joueurs de façon fluide même quand les paquets
  arrivent irrégulièrement.
- **État réel** — le cœur (`Push`, `Sample`) est **déclaré sans corps** ; seuls `SetDelay`/`GetDelay`
  sont inline. À implémenter.

### Constantes et types externes attendus

`NkNetWorld.h` **référence sans les définir** : `kNkReplicationHistorySize` (taille des ring buffers),
les types `NkNetId`/`NkPeerId`/`NkTimestampMs`/`NkVec3f`/`NkQuatf`, `NkConnectionManager`,
`NkBitWriter`/`NkBitReader`, `NkFunction`, `NkVector`, `NkSystemDesc`/`NkSystem`, `NkTransform`, et la
macro `NK_COMPONENT`. Ils viennent des modules `Protocol`, NKECS, NKMath, NKContainers et NKThreading
(`threading::NkMutex` protège les entrées en attente). Une implémentation devra donc fournir un `.cpp`
**et** garantir la disponibilité de ces dépendances.

---

## Pièges majeurs (avant toute reprise)

Au-delà des stubs déjà signalés, plusieurs incohérences **empêchent purement la compilation** ou
trahissent le comportement attendu. À traiter en premier :

- **Casse divergente PascalCase ↔ camelCase entre headers (bloquant).** `NkBlueprint.h` déclare ses
  membres en **PascalCase** (`Nodes`, `Connections`, `EntryNodeIndex`, `Inputs`/`Outputs`, `Name`,
  `DefaultValue`, `IsConnected`, `SourceNode`/`TargetPin`…, `Graph`). Mais `NkBlueprintHotReload.h`,
  `NkValidGraph.h` et les exemples y accèdent en **camelCase** (`graph.nodes`, `node.inputs`,
  `pin.defaultValue`, `conn.sourceNode`, `comp->graph`, `entryNodeIndex`). Ces deux fichiers **ne
  compilent pas** contre `NkBlueprint.h` en l'état. (C'est la trace du bug connu « includes/chemins
  ECS obsolètes ».)
- **Propagation de données absente.** `GetInput` renvoie toujours `DefaultValue` et `ResolveInputs`
  est un no-op : les valeurs ne circulent pas entre nœuds. À implémenter pour un vrai dataflow.
- **Hot-reload non branché.** `GetFileTime` est un stub (recharge en boucle), `LoadBlueprintFromFile`
  (namespace `serialization`) n'est pas déclaré ici, et `Instance()` employé dans l'exemple **n'existe
  pas** (pas de singleton sur `NkBlueprintHotReloadManager`).
- **Sérialisation = coquilles.** `SerializeBlueprint` n'émet qu'un comptage, `DeserializeBlueprint`
  renvoie toujours `false`. Pas de persistance réelle des graphes.
- **`NkValidGraph.h` sans `#pragma once`**, fonctions non-inline : à traiter comme un `.cpp` (compiler
  une fois), pas comme un header à inclure partout.
- **Namespace de fermeture trompeur dans `NkNetWorld.h`** : le fichier ouvre `nkentseu::ecs` mais ferme
  sur un commentaire `// namespace net`. Le contenu est bien dans **`nkentseu::ecs`**, pas
  `nkentseu::net`.
- **Mélange STL / zéro-STL.** Le sous-système Blueprint **viole** la règle zéro-STL du projet
  (`std::vector`/`string`/`function`/`unique_ptr`, `typeid`/RTTI dans `NkNodeCastToComponent`), alors
  que le sous-système réseau respecte les conteneurs maison (`NkVector`, `NkFunction`). À harmoniser
  vers le style maison.
- **Réseau entièrement non implémenté.** `NkNetWorld` et `NkNetInterpolator` n'ont **aucun `.cpp`** :
  presque toutes leurs méthodes sont déclarées sans corps.

---

### Exemple

```cpp
// SPÉCIFICATION — ne compile/fonctionne PAS en l'état (voir « Pièges majeurs »).
#include "Noge/ECS/VisualScript/NkBlueprint.h"
using namespace nkentseu::ecs::blueprint;

// 1) Enregistrer un type de nœud (auto au chargement avec la macro).
NK_REGISTER_BLUEPRINT_NODE(NkNodeAddFloat);

// 2) Construire un petit graphe : BeginPlay -> AddFloat.
NkBlueprintGraph graph;
graph.AddNode(NkNodeRegistry::Global().Create("EventBeginPlay")); // index 0
graph.AddNode(NkNodeRegistry::Global().Create("AddFloat"));       // index 1
graph.Link(/*src*/0, /*pin*/0, /*tgt*/1, /*pin*/0);              // fil d'exécution
graph.EntryNodeIndex = 0;

// 3) L'attacher à une entité via le composant ECS.
NkBlueprintComponent bp;
bp.Graph = std::move(graph);
// bp.OnStart(world, self);  // déclenche "EventBeginPlay"
// ATTENTION : la propagation des données (GetInput) est buguée -> AddFloat
//             ne lira pas de vraies entrées tant que ce n'est pas corrigé.
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
