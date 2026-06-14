# La scène

> Couche **Engine** · Noge · Donner une **vie** à un monde ECS : la hiérarchie de transformes
> (`NkSceneGraph`), l'orchestrateur de chargement et de transitions (`NkSceneManager`), les systèmes
> de lifecycle qui pilotent la boucle (`NkSceneLifecycleSystem`), le script de scène à dériver
> (`NkSceneScript`), et le système de modèles d'objets (`NkPrefab`).

Un `NkWorld` ECS sait stocker des entités et des composants, mais il ne sait **rien** d'un *niveau* :
ni qu'une caméra est *enfant* d'un personnage, ni qu'on *charge* le menu puis l'arène, ni que telle
logique doit tourner « au début », « chaque frame » puis « à la fin ». La couche Scène de Noge ajoute
exactement ce chaînon manquant : **une scène est un monde qu'on a appris à faire vivre**. Elle pose
une hiérarchie parent/enfant par-dessus les entités, un cycle de vie déterministe
(`BeginPlay → Tick → EndPlay`), un gestionnaire qui charge et décharge des scènes entières, et des
*prefabs* — des modèles réutilisables qu'on instancie à volonté.

Le fil rouge de toute cette page tient en une règle : **l'ownership est manuel** (`new`/`delete`,
pas de smart pointers — choix explicite pour éviter l'overhead), et **les méthodes de lifecycle ne
s'appellent jamais à la main**. Vous décrivez *quoi* faire (un script, une factory de scène) ; les
systèmes et le manager décident *quand*. Cette page vous apprend qui possède quoi, et qui appelle
quoi.

- **Namespaces** : `nkentseu::ecs` pour `NkSceneGraph`, `NkSceneManager`, les systèmes lifecycle et
  `NkSceneScript` ; **`nkentseu`** (et **pas** `::ecs`) pour `NkPrefab` et tout son écosystème.
- **Headers réels** : `Noge/ECS/Scene/NkSceneGraph.h`, `Noge/ECS/Scene/NkSceneManager.h`,
  `Noge/ECS/Scene/NkSceneLifecycleSystem.h`, `Noge/ECS/Scene/NkSceneScript.h`,
  `Noge/ECS/Prefab/NkPrefab.h`.

---

## Le graphe de scène : `NkSceneGraph`

C'est le cœur de la couche. Un `NkSceneGraph` est un **wrapper non-propriétaire** d'un `NkWorld`
(il en garde une simple référence) qui ajoute deux choses au monde brut : une **hiérarchie de
transformes** (chaque entité a un parent, des enfants, un transform local et un transform monde) et
un **cycle de vie** scriptable. On le construit en lui passant le monde et un nom
(`NkSceneGraph(world, "MainLevel")`), et son destructeur garantit le nettoyage en appelant `EndPlay()`.

Ce n'est **pas** un conteneur d'entités : il ne possède pas le monde, il le décore. Détruire le
graphe ne détruit pas le monde ; le graphe doit donc vivre *moins longtemps* que son `NkWorld`. Il
n'est **ni copiable** (copies `= delete`) **ni thread-safe** : un seul thread, le principal.

Spawner une entité passe par lui pour qu'elle naisse avec les bons composants de hiérarchie.
`SpawnNode("name")` crée une entité dotée d'un `NkSceneNode`, d'un `NkParent` racine, d'un
`NkChildren` vide, d'un `NkLocalTransform` identité et d'un `NkWorldTransform` identité marqué *dirty*.
`SpawnActor<T>("name", x, y, z)` fait davantage : il délègue à `NkGameObjectFactory::Create<T>` (le
monde ne connaît pas `NkGameObject`, d'où la factory) puis positionne l'acteur, et renvoie
l'instance `T` par valeur.

```cpp
NkSceneGraph scene(world, "Arena");
NkEntityId floor   = scene.SpawnNode("Floor");
NkEntityId crate   = scene.SpawnNode("Crate");
scene.SetParent(crate, floor);              // la caisse suit le sol
```

> **En résumé.** `NkSceneGraph` décore un `NkWorld` d'une **hiérarchie** et d'un **cycle de vie**.
> Non-propriétaire, non copiable, non thread-safe, durée de vie ≤ celle du monde. On spawne par lui
> (`SpawnNode` / `SpawnActor<T>`) pour hériter des composants de hiérarchie. Son destructeur appelle
> `EndPlay()`.

---

## La hiérarchie : `NkParent`, `NkChildren`, `NkSceneNode`, les transformes

La hiérarchie n'est pas une structure à part : ce sont **des composants ECS** posés sur chaque
entité. `NkParent` (singleton) porte un seul `NkEntityId` — `Invalid()` signifie *racine de scène*.
`NkChildren` porte la liste des enfants, mais **inline et sans heap** : un tableau C de `kMax = 64`
entrées, doublé d'un compteur. Ses méthodes `Add` / `Remove` / `Has` ont un vrai corps inline et
sont utilisables directement — mais attention à leur sémantique : `Add` ne vérifie **pas** les
doublons et `Remove` fait un *swap-with-last* qui **ne préserve pas l'ordre** des enfants.

`NkSceneNode` porte les métadonnées : un `name` de 64 octets (tronqué à 63), deux drapeaux **séparés
intentionnellement** — `active` (gameplay) et `visible` (rendu) — et un `layer` (0-255). On peut
vouloir une entité invisible mais toujours active (un déclencheur), ou visible mais gelée.

Les deux transformes forment un couple. `NkLocalTransform` est le **POD source** : `position[3]`,
`rotation[4]` (quaternion x,y,z,w à garder normalisé), `scale[3]` — c'est ce que vous éditez.
`NkWorldTransform` est la **matrice monde calculée** (16 floats, **row-major**, translation en
`m[12..14]`) ; elle est **remplie par `NkTransformSystem`** à partir des transformes locaux et de la
hiérarchie. Ne l'écrivez **jamais** à la main (elle est écrasée), et ne la lisez pas avant le premier
Update (elle n'a pas encore été calculée).

> **En résumé.** La hiérarchie = des **composants** : `NkParent` (un parent, `Invalid()` = racine),
> `NkChildren` (64 enfants inline, sans contrôle de doublon, ordre non préservé après `Remove`),
> `NkSceneNode` (nom + `active`/`visible` séparés + `layer`). Côté transform : on édite
> `NkLocalTransform` (POD), on **lit** `NkWorldTransform` (calculé par `NkTransformSystem`, jamais
> écrit à la main).

---

## Le cycle de vie : qui appelle quoi

Une scène vivante suit une séquence stricte : `BeginPlay` une fois au démarrage, puis chaque frame
`Tick(dt)` (logique variable) et `LateTick(dt)` (après tout le monde — caméras, suivi), à cadence
fixe `FixedTick(fixedDt)` (physique, pas constant), et `EndPlay` une fois à la fin. `Pause` / `Resume`
gèlent et reprennent (un `Tick` est ignoré si la scène est en pause ou pas démarrée).

Ces méthodes sont **publiques sur `NkSceneGraph`, mais réservées** à `NkSceneLifecycleSystem` et
`NkSceneManager` — le header le répète sur chacune. **Vous ne les appelez jamais.** Votre logique va
dans un *script de scène*, dont les callbacks `On…` sont déclenchés *par* ces méthodes. C'est la
différence fondamentale avec un simple objet : vous décrivez le comportement, l'ordonnanceur décide
du moment.

> **En résumé.** Séquence : `BeginPlay` (une fois) → `Tick`/`LateTick`/`FixedTick` (par frame) →
> `EndPlay` (une fois), plus `Pause`/`Resume`. Ces méthodes sont **interdites à l'appel manuel** :
> seuls les systèmes lifecycle et le manager les invoquent. Votre code vit dans un `NkSceneScript`.

---

## Le script de scène : `NkSceneScript`

`NkSceneScript` est l'**interface à dériver** pour mettre de la logique dans une scène — **0 ou 1 par
scène**. On l'attache au graphe par `scene.SetScript<MonScript>(args…)`, qui fait le `new`, injecte le
contexte (le monde et la scène) et renvoie le pointeur typé. Tous les callbacks ont une implémentation
**par défaut vide** : on n'override que ceux qui nous intéressent.

```cpp
class ArenaScript final : public NkSceneScript {
    void OnBeginPlay() noexcept override {
        mPlayer = SpawnPrefab(*playerPrefab, {0.f, 1.f, 0.f}, "Player");
    }
    void OnTick(float dt) noexcept override { /* logique par frame */ }
    NkGameObject mPlayer;
};
scene.SetScript<ArenaScript>();   // pas de delete manuel du retour
```

Les callbacks `OnBeginPlay` / `OnTick` / `OnLateTick` / `OnFixedTick` / `OnEndPlay` / `OnPause` /
`OnResume` sont appelés **automatiquement** par les systèmes lifecycle — jamais à la main. À l'intérieur,
le contexte est disponible via `GetWorld()` / `GetScene()` (nuls si non injecté), et des helpers
facilitent le travail : `FindGameObjectsWithTag` / `FindGameObjectByName` (recherches `O(n)`,
case-sensitive), `GetComponentInWorld<T>()` (le composant global/singleton du monde, inline),
`SpawnPrefab(prefab, position, name)` et `DestroyGameObject(go)` (destruction **immédiate**).

Un détail à connaître : les **callbacks** sont `noexcept`, mais les **helpers entités**
(`FindGameObjectsWithTag`, `FindGameObjectByName`, `SpawnPrefab`, `DestroyGameObject`) ne le sont
**pas**. Et `SpawnPrefab` prend une position de type `NkMath::NkVec3f` (qualifié `NkMath::`).

> **En résumé.** `NkSceneScript` = votre logique, **0 ou 1 par scène**, attaché via `SetScript<T>`.
> On override les callbacks `On…` voulus (le reste est vide par défaut), on accède au contexte par
> `GetWorld()`/`GetScene()` et aux helpers (`FindGameObject…`, `SpawnPrefab`, `DestroyGameObject`).
> Callbacks `noexcept`, helpers **non** `noexcept`. Jamais appeler les `On…` soi-même.

---

## Les systèmes de lifecycle : `NkSceneLifecycleSystem`

Le pont entre la scène et l'ordonnanceur, c'est **trois systèmes ECS** (`final : public NkSystem`),
un par cadence : `NkSceneFixedTickSystem` (groupe `FixedUpdate`, priorité 100, appelle `FixedTick`),
`NkSceneTickSystem` (groupe `Update`, priorité 100, appelle `Tick`) et `NkSceneLateTickSystem`
(groupe `PostUpdate`, priorité **200** pour s'exécuter **en dernier**, appelle `LateTick`). Chacun
tient un `NkSceneGraph*` non-possédé ; si ce pointeur est nul, leur `Execute()` est un **no-op
silencieux**.

On ne les instancie pas à la main : le helper libre `RegisterSceneLifecycle(scheduler, scene)` crée
les trois via `new` et les enregistre — **le scheduler en prend ownership** (ne les supprimez pas
vous-même). Au-dessus, `SceneLifecycleManager` est un **registre non-propriétaire** de scènes : il ne
delete rien. `RegisterScene` est idempotent (un doublon renvoie `true` sans rien refaire),
`IsRegistered` / `GetRegisteredCount` / `FindSceneByName` interrogent le registre, et `CleanupAll`
appelle `EndPlay()` sur les scènes démarrées puis vide le registre **sans les delete**.

Le piège central est ici : **`UnregisterScene` doit être appelé AVANT `delete scene`**. Le scheduler
garde les systèmes (donc des pointeurs vers la scène) ; les retirer du `SceneLifecycleManager` ne les
retire pas du scheduler — détruire la scène sans précaution laisse des *dangling pointers*.

> **En résumé.** Trois systèmes (`FixedTick`/`Tick`/`LateTick`, groupes `FixedUpdate`/`Update`/
> `PostUpdate`, priorités 100/100/**200**) câblent la scène à la boucle, enregistrés en bloc par
> `RegisterSceneLifecycle` (ownership = **scheduler**). `SceneLifecycleManager` est un registre qui ne
> possède rien. Toujours `UnregisterScene` **avant** `delete scene`.

---

## L'orchestrateur : `NkSceneManager` et les transitions

Charger un niveau entier, passer du menu à l'arène, recharger après un game over : c'est le rôle de
`NkSceneManager`. Il référence un `NkWorld` (non possédé) mais **prend l'ownership des `NkSceneGraph*`**
qu'il crée. Le modèle est celui d'une **fabrique enregistrée** : on associe un nom à une
`NkSceneFactory` (`NkSceneGraph* factory(NkWorld&)`) via `Register("Arena", factory)`, puis on demande
`LoadScene("Arena")`. La factory fait le `new`, peuple la scène et la renvoie (ou `nullptr` en erreur) ;
le manager prend la suite.

`LoadScene` est **synchrone et bloquant**. Son flux : décharger la scène courante (`EndPlay` +
`FlushDeferred` + `delete`), éventuellement afficher une *loading scene*, appeler la factory, stocker
le résultat, lancer `BeginPlay()`, puis décharger la loading scene. En cas d'erreur (factory nulle,
nom inconnu) il logue, renvoie `false` et **préserve l'état courant**. `ReloadCurrent` recharge la
scène active (transition `Instant`), `GetCurrent` / `GetCurrentName` / `HasScene` interrogent l'état,
et `Pause` / `Resume` délèguent à la scène courante (no-op silencieux si aucune).

Les **transitions** sont décrites par `NkSceneTransition` (un `NkTransitionType`, une durée, une
éventuelle scène de chargement), construites par les statics inline `Instant()`, `Fade(dur)` et
`WithLoading(loadingScene, fadeDur)`. **Attention au statut** : l'enum `NkTransitionType` propose
`Instant` / `Fade` / `FadeWhite` / `Custom`, mais le header marque `Custom` comme **non fonctionnel**
(« Phase 5 du TODO »), et `Update(dt)` — censé animer les fondus — est un **placeholder** (Phase 5).
Autrement dit : aujourd'hui, seul `Instant` change vraiment de scène ; les fondus ne sont pas animés.

```cpp
NkSceneManager mgr(world);
mgr.Register("Menu",  [](NkWorld& w) -> NkSceneGraph* { return new NkSceneGraph(w, "Menu"); });
mgr.Register("Arena", makeArena);
mgr.LoadScene("Menu");                       // Instant par défaut
mgr.LoadScene("Arena", NkSceneTransition::Fade(0.5f));
```

> **En résumé.** `NkSceneManager` = fabriques enregistrées (`Register` nom→factory) + chargement
> **synchrone** (`LoadScene`/`ReloadCurrent`), il **possède** les scènes (delete à l'unload).
> Transitions via `NkSceneTransition::Instant/Fade/WithLoading`. **Statut spec** : `NkTransitionType::
> Custom` et l'animation des fondus (`Update`) ne sont pas implémentés (Phase 5).

---

## Les modèles : `NkPrefab`

Un *prefab* est un **modèle d'objet** réutilisable — un ennemi, une caisse, une particule — qu'on
décrit une fois et qu'on instancie autant qu'on veut. Contrairement à tout le reste de cette page,
`NkPrefab` vit dans le namespace **`nkentseu`** (pas `::ecs`) ; il référence donc les types ECS en les
qualifiant (`ecs::NkWorld`, `ecs::NkEntityId`, `ecs::NkGameObject`). Il est **zéro-STL**
(`NkString`, `NkVector`, `NkUnorderedMap`, `NkFunction`).

On le construit avec un **builder fluide** : chaque `With…` renvoie le prefab par référence et
s'enchaîne. `WithComponent<T>(args…)` construit un `T` temporaire, le sérialise et le stocke ;
`WithTag(bit)` accumule des bits de tag ; `WithLayer(id)` fixe le layer ; `WithChild(child)` ajoute un
enfant (inline ou référence vers un autre prefab) ; `WithBlueprint(path)` rattache un blueprint.

```cpp
NkPrefab enemy("Enemy");
enemy.WithComponent<Health>(100)
     .WithComponent<Velocity>()
     .WithLayer(2)
     .WithTag(kTagEnemy);

ecs::NkGameObject go = enemy.InstantiateGameObject(world, "Enemy_01");
```

L'instanciation existe en trois formes : `Instantiate(world, name)` (renvoie un `NkEntityId`),
`InstantiateGameObject(world, name)` (inline, renvoie un `NkGameObject`) et `InstantiateBatch(world,
count, out)` pour en créer plusieurs d'un coup. La sérialisation JSON passe par `Serialize` /
`Deserialize`. Au global, `NkPrefabRegistry::Global()` (Meyers singleton) catalogue les prefabs par
leur `path` (`Register`, `Get`, `ForEach`, `Count`).

> **En résumé.** `NkPrefab` (namespace **`nkentseu`**, zéro-STL) = un modèle décrit par builder fluide
> (`WithComponent`/`WithTag`/`WithLayer`/`WithChild`/`WithBlueprint`), instancié par `Instantiate` /
> `InstantiateGameObject` / `InstantiateBatch`, catalogué dans `NkPrefabRegistry::Global()`.

---

## Aperçu de l'API

Tout le public, en un coup d'œil. Chacun est détaillé dans la « Référence complète » qui suit.

### `NkSceneGraph` — graphe de scène (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSceneGraph(world, name="Scene")`, `~NkSceneGraph` | Wrap d'un `NkWorld` ; dtor appelle `EndPlay()`. Copies `= delete`. |
| Identité / état | `Name()`, `IsPaused()`, `World()` (×2) | Nom (jamais nul), en pause ?, accès au monde. |
| Script | `SetScript<T>(args…)`, `ClearScript()`, `GetScript<T>()` (×2), `HasScript()` | Attache/efface/lit le script (0 ou 1). `new`/`delete` **manuel**. |
| Lifecycle (réservé) | `BeginPlay`, `Tick`, `LateTick`, `FixedTick`, `EndPlay`, `Pause`, `Resume` | **Jamais à appeler soi-même** (systèmes/manager). |
| Spawn | `SpawnNode(name)`, `SpawnActor<T>(name, x,y,z, args…)` | Crée une entité (composants hiérarchie) / un acteur via factory. |
| Hiérarchie | `SetParent(child, parent)`, `DetachFromParent(child)` | Reparente / détache (`parent = Invalid` = racine). |
| Recherche | `FindByName(name)`, `FindByLayer(layer, out)` | Premier match `O(n)` / **ajoute** au vecteur. |
| Activation | `SetActive(id, b)`, `SetVisible(id, b)` | Drapeaux `active` / `visible` (**non récursif**). |
| Destruction | `DestroyRecursive(id)` | Détruit l'entité + descendants (**immédiat**, post-order). |
| Alias | `NkScene` | `using NkScene = NkSceneGraph;` — **`@deprecated`**. |

### Composants de hiérarchie (namespace `nkentseu::ecs`, `NK_COMPONENT`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Parenté | `NkParent { entity }` | Parent unique ; `Invalid()` = racine. |
| Enfants | `NkChildren` (`kMax=64`, `Add`/`Remove`/`Has`) | Tableau inline ; `Add` sans contrôle doublon, `Remove` swap (ordre non préservé). |
| Métadonnées | `NkSceneNode { name[64], active, visible, layer }` | Nom + drapeaux gameplay/rendu séparés + couche. |
| Transform local | `NkLocalTransform { position[3], rotation[4], scale[3] }` | POD source (quaternion à normaliser). |
| Transform monde | `NkWorldTransform { matrix[16], dirty }` | Matrice **row-major** calculée par `NkTransformSystem` (lire, ne pas écrire). |

### `NkSceneScript` — script de scène (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Callbacks (vides) | `OnBeginPlay`, `OnTick`, `OnLateTick`, `OnFixedTick`, `OnEndPlay`, `OnPause`, `OnResume` | Override optionnel ; appelés par les systèmes. `noexcept`. |
| Contexte | `SetContext(world, scene)`, `GetWorld()`, `GetScene()` | Injection (auto) / accès (nuls si non injecté). |
| Helpers entités | `FindGameObjectsWithTag`, `FindGameObjectByName`, `GetComponentInWorld<T>`, `SpawnPrefab`, `DestroyGameObject` | Recherches `O(n)`, composant global, spawn de prefab, destruction immédiate. **Non `noexcept`** (y compris le template `GetComponentInWorld<T>`). |

### `NkSceneManager` — orchestrateur (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkSceneManager(world)`, `~NkSceneManager` | Réf monde, **possède** les scènes. Copies `= delete`. |
| Enregistrement | `Register(name, factory)`, `SetLoadingScene(name)` | Nom→factory (remplace si existe) / scène de chargement. |
| Chargement | `LoadScene(name, transition=Instant)`, `ReloadCurrent()` | Charge / recharge. **Synchrone, bloquant.** |
| Accès courant | `GetCurrent()` (×2), `GetCurrentName()`, `HasScene()` | Scène / nom / présence. |
| Contrôle | `Pause()`, `Resume()`, `Update(dt)` | Délègue à la scène ; `Update` = **placeholder** (Phase 5). |

### `NkSceneTransition` / `NkTransitionType` (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `NkTransitionType::Instant/Fade/FadeWhite/Custom` | Style de transition (`Custom` **non implémenté**). |
| Données | `NkSceneTransition { type, duration, loadingScene }` | Description d'une transition. |
| Fabriques | `Instant()`, `Fade(dur=0.5)`, `WithLoading(loading, fadeDur=0.3)` | Construction inline (pas de `FadeWhite(…)`). |

### Systèmes de lifecycle (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Systèmes | `NkSceneFixedTickSystem`, `NkSceneTickSystem`, `NkSceneLateTickSystem` | `FixedUpdate`/100, `Update`/100, `PostUpdate`/**200**. No-op si scène nulle. |
| Helper libre | `RegisterSceneLifecycle(scheduler, scene)` | Crée+enregistre les 3 (ownership = scheduler). |
| Registre | `SceneLifecycleManager` (`RegisterScene`, `UnregisterScene`, `IsRegistered`, `GetRegisteredCount`, `FindSceneByName`, `CleanupAll`, `DumpRegisteredScenes`) | Registre **non-propriétaire** ; `Unregister` avant `delete`. |

### `NkPrefab` et son écosystème (namespace `nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données prefab | `NkPrefabComponentData`, `NkPrefabChild` | Composant sérialisé / enfant (inline ou référence). |
| Prefab | `NkPrefab(name)`, champs `name/path/version/guid/components/children/…` | Le modèle lui-même. |
| Builder | `WithComponent<T>`, `WithTag`, `WithLayer`, `WithChild`, `WithBlueprint` | Construction fluide (renvoient `NkPrefab&`). |
| Instanciation | `Instantiate`, `InstantiateGameObject`, `InstantiateBatch` | Entité / GameObject / lot. |
| Sérialisation | `Serialize`, `Deserialize` | JSON. |
| Helpers | `HasComponent`, `GetComponentData` | Présence / données d'un composant. |
| Instance | `NkPrefabInstance` (`SetOverride<T>`, `RevertOverride`, `RevertAll`) | Composant de suivi (`SetOverride` **non implémenté**). |
| Registre | `NkPrefabRegistry::Global()` (`Register`, `LoadFromFile`, `Get`, `ForEach`, `Count`) | Catalogue global par `path` (`LoadFromFile` à implémenter). |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines (gameplay/IA,
rendu, animation, physique, audio, UI/éditeur, IO, scène). Les statuts **spec / non implémentés** et
les **incohérences de doc** sont signalés là où ils existent.

### `NkSceneGraph` à fond

**Construction et ownership.** Le graphe ne possède **pas** le monde : il en garde une référence. Sa
durée de vie doit donc être incluse dans celle du `NkWorld`. Le destructeur appelle `EndPlay()` pour
garantir que le script de scène est proprement terminé même si on oublie de le faire. Les copies sont
`= delete` (et aucun move n'est déclaré) : un graphe ne se duplique pas.

**Le script de scène (`SetScript` / `ClearScript` / `GetScript` / `HasScript`).** La gestion mémoire
est **manuelle** : `SetScript<T>(args…)` fait `new T(...)`, injecte le contexte et renvoie le pointeur
typé — que vous ne devez **jamais** delete vous-même (le graphe s'en charge). Un piège majeur :
`SetScript` appelle bien `OnEndPlay()` sur l'ancien script s'il existe, **mais ne le delete pas** avant
de réassigner le pointeur → changer de script à chaud sans passer par `ClearScript()` d'abord
provoque une **fuite mémoire**. La règle sûre : `ClearScript()` (qui, lui, appelle `OnEndPlay()` puis
`delete`) avant tout remplacement. `GetScript<T>()` est un `static_cast` **sans aucun contrôle
runtime** : demander le mauvais type est un comportement indéfini ; `HasScript()` teste seulement la
non-nullité, pas le type.

**Spawn.** `SpawnNode(name)` est l'entrée bas niveau : une entité dotée des cinq composants de
hiérarchie (`NkSceneNode`, `NkParent` racine, `NkChildren` vide, `NkLocalTransform` identité,
`NkWorldTransform` dirty). `SpawnActor<T>(name, x, y, z, args…)` est l'entrée gameplay : il passe par
`NkGameObjectFactory::Create<T>` (parce que le monde ne connaît pas `NkGameObject`) puis positionne
l'acteur, et renvoie l'instance par valeur. Cas d'usage par domaine :
- **Gameplay / IA** — `SpawnActor<Enemy>("Goblin", x, y, z)` pour peupler un niveau ; chaque acteur
  est un `NkGameObject` prêt à recevoir composants et logique.
- **Rendu** — `SpawnNode` pour des entités purement visuelles (props, décors) qu'on positionne et
  rend via leur `NkWorldTransform`.
- **Scène / éditeur** — créer des nœuds vides comme regroupeurs (un « dossier » dans l'arbre), parents
  de plusieurs objets, déplacés ensemble.

**Hiérarchie (`SetParent` / `DetachFromParent`).** `SetParent(child, parent)` met à jour le `NkParent`
de l'enfant, le retire du `NkChildren` de l'ancien parent, l'ajoute à celui du nouveau, et marque le
transform *dirty*. `parent = Invalid()` détache (l'enfant redevient racine) — c'est exactement ce que
fait `DetachFromParent`. **Ne pas appeler pendant l'itération d'une query hiérarchique** : on modifie
la structure qu'on parcourt. Usages : attacher une arme à la main d'un personnage (rendu/animation),
rattacher une caméra à un véhicule, grouper des objets sous un nœud d'éditeur.

**Recherche (`FindByName` / `FindByLayer`).** `FindByName(name)` est un parcours `O(n)`,
**case-sensitive**, qui renvoie le **premier** match (l'ordre n'est pas garanti) ou `Invalid()`.
Pratique pour le débogage, le scripting, le câblage par nom — moins pour une boucle chaude.
`FindByLayer(layer, out)` collecte toutes les entités d'une couche, mais **ajoute** au vecteur sans le
vider : pensez à `out.Clear()` avant si nécessaire. Utile pour traiter une couche entière (tous les
ennemis, tout l'UI, tout le décor) en rendu sélectif ou en logique de gameplay.

**Activation / visibilité (`SetActive` / `SetVisible`).** Deux drapeaux **distincts** : `active`
(gameplay — l'entité doit-elle être mise à jour ?) et `visible` (rendu — doit-elle être dessinée ?).
Tous deux sont **non récursifs** : désactiver un parent **n'affecte pas** ses enfants ; à vous de
propager si vous le voulez. Cas typiques : geler un déclencheur tout en le laissant visible en éditeur,
cacher un objet sans interrompre sa logique (un son d'ambiance attaché à un objet invisible).

**Destruction (`DestroyRecursive`).** Détruit l'entité **et tous ses descendants** en *post-order*
(les feuilles d'abord), en prenant une copie locale des enfants, en détachant du parent, puis en
appelant `mWorld.Destroy()`. C'est une **destruction immédiate** : ne l'appelez **pas** pendant une
itération de query — préférez alors la destruction différée du monde. Usage : retirer un sous-arbre
entier d'un coup (un véhicule et tous ses accessoires, un groupe d'UI).

**Alias `NkScene`.** `using NkScene = NkSceneGraph;` existe pour la transition, mais est marqué
**`@deprecated`** : dans du code neuf, écrivez `NkSceneGraph`.

### Les composants de hiérarchie à fond

- **`NkParent`** — un seul `NkEntityId`, `Invalid()` signifiant racine. C'est un *singleton* de
  composant (une entité a un parent au plus). Sa modification **doit** marquer
  `NkWorldTransform::dirty = true` pour que la matrice monde soit recalculée — mais ce header ne
  l'automatise pas, c'est `SetParent` qui s'en charge.
- **`NkChildren`** — tableau **inline** de `kMax = 64` entrées + un compteur, **zéro heap**. Ses trois
  méthodes ont un corps inline réel : `Add` ajoute en fin en `O(1)` (renvoie `false` si plein, **sans
  vérifier les doublons**), `Remove` fait une recherche linéaire puis un *swap-with-last* (donc **ne
  préserve pas l'ordre** des enfants), `Has` est une recherche linéaire `O(n)`. À 64 enfants, on est
  plein : pour des hiérarchies très larges, regroupez par nœuds intermédiaires.
- **`NkSceneNode`** — `name[64]` (null-terminated, tronqué à 63 via `NkStrNCpy`), deux drapeaux
  `active` (gameplay) et `visible` (rendu) **séparés intentionnellement**, et `layer` (0-255). Le
  constructeur `explicit NkSceneNode(const char* n)` copie le nom. La séparation active/visible est
  délibérée : un déclencheur peut être actif et invisible, un décor visible et figé.
- **`NkLocalTransform`** — le POD source (~40 octets, SIMD-friendly) : `position[3]`, `rotation[4]`
  (quaternion x,y,z,w **à garder normalisé**), `scale[3]`. C'est ce que **vous** éditez (gameplay,
  animation, physique écrivent ici).
- **`NkWorldTransform`** — la **matrice monde** (16 floats, **row-major**, translation en `m[12..14]`)
  + un drapeau `dirty`. Elle est **calculée par `NkTransformSystem`** (externe) à partir des locaux et
  de la hiérarchie. **Ne l'écrivez jamais** (elle est écrasée au prochain Update) et **ne la lisez pas
  avant le premier Update** (elle n'est pas encore calculée). C'est elle que le rendu consomme pour
  placer chaque objet dans le monde.

### `NkSceneScript` à fond

L'interface se dérive et s'attache via `SetScript<T>`. Le header précise que **toutes les méthodes
non-template sont implémentées dans `NkSceneScript.cpp`** ; seul `GetComponentInWorld<T>` est inline.

**Callbacks lifecycle.** Les sept (`OnBeginPlay`, `OnTick`, `OnLateTick`, `OnFixedTick`, `OnEndPlay`,
`OnPause`, `OnResume`) sont `virtual … noexcept` avec une **implémentation par défaut vide** : on
n'override que l'utile. Ils sont déclenchés **automatiquement** par les systèmes lifecycle — **jamais
manuellement**. Répartition des rôles :
- `OnBeginPlay` — initialisation (spawner les acteurs de départ, brancher les abonnements, charger les
  ressources de la scène).
- `OnTick(dt)` — logique de gameplay/IA par frame, `dt` variable.
- `OnLateTick(dt)` — ce qui doit voir le résultat du Tick : caméras qui suivent, contraintes,
  *post*-logique.
- `OnFixedTick(fixedDt)` — physique et tout ce qui exige un pas constant.
- `OnEndPlay` — nettoyage (libérer les ressources, se désabonner).
- `OnPause` / `OnResume` — geler/relancer audio, animations, timers.

**Contexte (`SetContext`, `GetWorld`, `GetScene`).** `SetContext(world, scene)` est appelé par
`NkSceneGraph::SetScript<T>` via friendship — **jamais à la main**. Ensuite `GetWorld()` et
`GetScene()` donnent accès au monde et au graphe (nuls si le contexte n'a pas été injecté).

**Helpers entités.** Attention : contrairement aux callbacks, ces déclarations ne sont **pas**
`noexcept` — y compris le template `GetComponentInWorld<T>()`.
- `FindGameObjectsWithTag(tag)` — parcourt les entités portant un `NkTag`, case-sensitive, renvoie un
  `NkVector<NkGameObject>` (vide si pas de contexte ou aucun match). Idéal pour agir sur une catégorie
  entière (tous les ennemis, tous les ramassables).
- `FindGameObjectByName(name)` — `O(n)` sur `NkName` (`strcmp`), premier match ; renvoie un handle
  **invalide** (`IsValid() == false`) si absent. Pour le câblage par nom et le scripting.
- `GetComponentInWorld<T>()` — **inline** : renvoie `mWorld->GetGlobalComponent<T>()` (le
  composant **global/singleton** du monde) ou `nullptr`. Pour accéder à un état partagé : score
  global, réglages de la partie, contexte d'entrée.
- `SpawnPrefab(prefab, position, name)` — instancie un `NkPrefab` (via `prefab.Instantiate`), applique
  la `position` (type **`NkMath::NkVec3f`**) au root et renvoie le `NkGameObject` (invalide en erreur).
- `DestroyGameObject(go)` — `go.Destroy()` **immédiat** : à ne **pas** faire pendant une query →
  utilisez plutôt `GetWorld()->DestroyDeferred(id)`.

> Divergence de namespace à connaître : `NkPrefab` est **forward-déclaré dans `ecs::`** en tête de
> `NkSceneScript.h`, alors que sa définition réelle est dans `nkentseu::` (voir plus bas). C'est une
> incohérence de la forward-declaration, pas un second type.

### Les systèmes de lifecycle à fond

Les trois systèmes (`NkSceneFixedTickSystem`, `NkSceneTickSystem`, `NkSceneLateTickSystem`) sont
`final : public NkSystem`, chacun tenant un `NkSceneGraph*` **non-possédé**. Si ce pointeur est nul,
`Execute()` est un **no-op silencieux** (pas de crash). Leur `Describe()` les place dans le pipeline :
- `NkSceneFixedTickSystem` — groupe `NkSystemGroup::FixedUpdate`, nom `"NkSceneFixedTick"`, priorité
  **100** ; `Execute` appelle `mScene->FixedTick(dt)` (`dt` constant, `world` inutilisé). C'est le
  canal de la physique.
- `NkSceneTickSystem` — groupe `NkSystemGroup::Update`, nom `"NkSceneTick"`, priorité **100** ;
  `Execute` appelle `mScene->Tick(dt)` (`dt` variable). Le canal du gameplay.
- `NkSceneLateTickSystem` — groupe `NkSystemGroup::PostUpdate`, nom `"NkSceneLateTick"`, priorité
  **200** (donc **en dernier**) ; `Execute` appelle `mScene->LateTick(dt)`. Le canal des caméras et de
  la *post*-logique.

**`RegisterSceneLifecycle(scheduler, scene)`** crée les trois via `new` et les enregistre d'un coup ;
**le scheduler en prend ownership** — ne les delete pas. C'est l'unique appel à faire pour brancher une
scène sur la boucle.

**`SceneLifecycleManager`** est un **registre non-propriétaire** (pointeurs nus). Son destructeur,
inline dans le header, ne fait quasiment rien (le scheduler détruit les systèmes) puis vide le
registre — il **ne delete pas les scènes**. Ses méthodes :
- `RegisterScene(scheduler, scene)` — valide la non-nullité, est **idempotent** (un doublon renvoie
  `true` sans rien refaire), appelle `RegisterSceneLifecycle` puis ajoute au registre.
- `UnregisterScene(scene)` — retire du registre local, idempotent (`false` si absente). **Le scheduler
  garde les systèmes** : il faut donc appeler ceci **avant** `delete scene`, sinon les systèmes
  pointent dans le vide.
- `IsRegistered` / `GetRegisteredCount` — interrogation `O(n)` / compteur.
- `FindSceneByName(name)` — `O(n·m)`, `nullptr` si absente ou nom null/vide.
- `CleanupAll()` — pour chaque scène : `EndPlay()` (si démarrée) + retrait du registre, puis `Clear()`.
  **Ne delete pas** les scènes.
- `DumpRegisteredScenes()` — debug/profiling. Note : le doc évoque un « logger optionnel » mais la
  signature **ne prend aucun paramètre**.

Usage UI/éditeur typique : `SceneLifecycleManager` permet à un éditeur de garder plusieurs scènes
enregistrées (preview, scène éditée) et d'auditer leur état (`GetRegisteredCount`,
`DumpRegisteredScenes`) sans toucher à leur durée de vie.

### `NkSceneManager` à fond

**Construction et ownership.** Référence un `NkWorld` (non possédé) mais **possède** les
`NkSceneGraph*`. Le destructeur décharge la scène courante (`EndPlay` garanti). Copies `= delete`.

**Enregistrement.** `Register(name, factory)` associe un nom à une `NkSceneFactory`
(`NkSceneGraph* factory(NkWorld&)`) — si le nom existe, la factory est **remplacée**. La factory fait
le `new`, peuple la scène, et renvoie le pointeur (ou `nullptr` en cas d'erreur). `SetLoadingScene(name)`
désigne la scène de chargement par défaut (qui doit elle-même être enregistrée), déclenchée quand la
transition n'est pas `Instant` et que `loadingScene` est non vide.

**Chargement (`LoadScene` / `ReloadCurrent`).** `LoadScene(name, transition)` est **synchrone et
bloquant**. Son flux complet : `UnloadCurrent` (`EndPlay` + `FlushDeferred` + `delete`) → éventuelle
loading scene → factory `(*factory)(mWorld)` → stockage → `BeginPlay()` → unload de la loading scene.
En cas d'erreur (factory nulle, scène inconnue) il logue, renvoie `false` et **préserve l'état
courant** (pas de scène à moitié chargée). `ReloadCurrent()` recharge la scène active en transition
`Instant` (`false` si aucune). Cas d'usage : passage menu→jeu, niveau→niveau, *reload* après mort.

**Accès et contrôle.** `GetCurrent()` (×2), `GetCurrentName()` (vide si rien) et `HasScene()`
interrogent l'état. `Pause()` / `Resume()` délèguent à la scène courante et sont des **no-op
silencieux** s'il n'y en a pas — pratique pour brancher une touche pause sans vérifier l'état.

**`Update(dt)` — statut spec.** Censé animer les transitions, c'est aujourd'hui un **placeholder**
(« Phase 5 du TODO ») : les fondus ne sont pas animés. Il **ne gère pas** `Tick`/`LateTick`/`FixedTick`
(c'est le rôle des systèmes lifecycle), uniquement les transitions.

### `NkSceneTransition` et `NkTransitionType` à fond

`NkSceneTransition` décrit une transition : `type` (`NkTransitionType`), `duration` (0.5 par défaut),
`loadingScene`. On la construit par les statics inline :
- `Instant()` → transition immédiate, aucune animation. **La seule réellement fonctionnelle
  aujourd'hui.**
- `Fade(dur = 0.5f)` → fondu (de durée donnée).
- `WithLoading(loadingScene, fadeDur = 0.3f)` → fondu **avec** une scène de chargement intermédiaire.

L'enum `NkTransitionType` (scope `NkTransitionType::`) compte **exactement** quatre valeurs :
`Instant = 0`, `Fade`, `FadeWhite`, `Custom`. **Statut spec** : `Custom` est explicitement marqué
« non fonctionnel » dans le header (nécessite la Phase 5 du TODO).

> **Incohérence de doc à connaître.** Certains exemples appellent `NkSceneTransition::FadeWhite(0.8f)`,
> mais **aucune méthode statique `FadeWhite(...)` n'est déclarée** : seuls `Instant`, `Fade` et
> `WithLoading` existent. L'exemple est trompeur — n'écrivez pas `FadeWhite(...)`.

### `NkPrefab` et son écosystème à fond

> Rappel de namespace : tout ce bloc vit dans **`nkentseu`** (pas `::ecs`), et qualifie les types ECS
> (`ecs::NkWorld`, `ecs::NkEntityId`, `ecs::NkGameObject`). Zéro-STL.

**`NkPrefabComponentData`** — un composant sérialisé : `typeName`, `jsonValue`, `isOverridden`. C'est
l'unité de stockage d'un composant dans un prefab.

**`NkPrefabChild`** — un enfant du prefab : `name`, `prefabPath` (vide = nœud inline, sinon référence
vers un autre prefab), transform local (`localPosition`, `localRotation = NkQuatf::Identity()`,
`localScale`) et `isActive`. Permet de bâtir une **hiérarchie** complète dans un seul prefab (un
véhicule + ses roues, un personnage + ses accessoires).

**`NkPrefab`** — le modèle. Champs publics : `name`, `path`, `version = "1.0"`, `guid`, la map
`components`, le vecteur `children`, `blueprintPath`, `tagBits`, `layer`. Le constructeur
`explicit NkPrefab(const char* name)` initialise le nom et calcule `guid = ecs::detail::FNV1a(name)`.
Le **builder fluide** (tout inline, renvoie `NkPrefab&`) :
- `WithComponent<T>(args…)` — construit un `T` temporaire, le sérialise (via `SerializeComponent` +
  `ecs::NkIdOf<T>()`, buffer 4096) et le stocke sous son nom de type. C'est ainsi qu'on décrit l'état
  initial d'un composant (vie, vitesse, apparence).
- `WithTag(bit)` — `tagBits |= bit` (catégoriser : ennemi, ramassable, déclencheur).
- `WithLayer(id)` — fixe la couche.
- `WithChild(child)` — `children.PushBack` (construire la hiérarchie).
- `WithBlueprint(path)` — rattache un blueprint (logique visuelle/script).

**Instanciation.**
- `Instantiate(world, name)` — crée l'arbre d'entités et renvoie l'`ecs::NkEntityId` du root
  (implémentation en `.cpp`).
- `InstantiateGameObject(world, name)` — **inline** : `Instantiate(...)` puis renvoie
  `ecs::NkGameObject(id, &world)`. La forme préférée côté gameplay.
- `InstantiateBatch(world, count, out)` — crée `count` instances d'un coup (`.cpp`). Idéal pour des
  particules, une foule, un champ d'astéroïdes.

Cas d'usage par domaine : **gameplay/IA** (modèle d'ennemi instancié au spawn), **rendu** (un prop
décoré d'un composant de mesh), **animation** (un personnage + son rig en enfants), **physique** (un
objet + ses colliders), **audio** (une source sonore avec son composant audio), **éditeur** (drag &
drop d'un prefab dans la scène).

**Sérialisation.** `Serialize(buffer, size)` / `Deserialize(json)` (`.cpp`, renvoient un `bool`) pour
écrire/relire le prefab en JSON — le format d'asset d'un prefab sur disque.

**Helpers inline.** `HasComponent(typeName)` (teste `components.Contains`) et
`GetComponentData(typeName)` (renvoie le `NkPrefabComponentData*` ou `nullptr`) — pour inspecter un
prefab (éditeur, outillage).

**`NkPrefabInstance`** — un **composant ECS** (`NK_COMPONENT`) qui suit une instance de prefab dans le
monde : `rootEntity`, `prefabPath`, `instanceName`, `isOverridden`, la map `overrides` (clé
`"NomType.NomChamp"` → JSON) et `childEntities`. Ses méthodes :
- `SetOverride<T>(fieldName, value)` — **statut spec / non implémenté** : le corps inline contient un
  `// TODO: sérialiser value → JSON`, **ignore `value`**, met `isOverridden = true` et renvoie `true`
  **sans rien sérialiser**. À ne pas considérer comme fonctionnel.
- `RevertOverride(key)` — `overrides.Remove(key)` (inline).
- `RevertAll()` — `overrides.Clear(); isOverridden = false;` (inline).

Le système d'override vise l'**éditeur** (modifier une instance sans toucher au prefab source), mais
la sérialisation de la valeur reste à écrire.

**`NkPrefabRegistry`** — catalogue **global** (Meyers singleton via `Global()`), tout inline sauf
`LoadFromFile`. `Register(prefab)` indexe par `prefab.path` (`false` si le path est vide) ;
`Get(path)` (surchargé `const char*` et `const NkString&`) renvoie le `const NkPrefab*` ou `nullptr` ;
`ForEach(fn)` itère en appelant `fn(path, prefab)` ; `Count()` renvoie le nombre de prefabs.
`LoadFromFile(path)` est **déclaré sans corps visible** (`.cpp`) → vraisemblablement **à
implémenter**. Le registre est le point d'entrée pour charger tous les prefabs d'un projet au boot
puis les instancier par chemin.

> Référencé dans les exemples mais **non déclaré ici** :
> `NkGameObjectFactory::InstantiatePrefab(world, path, name)`.

### Les idiomes transverses et les pièges

- **Ownership manuel, pas de smart pointers** (choix explicite anti-overhead). Le `NkSceneManager`
  delete les scènes créées par `new` dans les factories. Le `NkScheduler` possède les systèmes
  lifecycle. Le `SceneLifecycleManager` ne possède **rien** (registre de pointeurs nus).
- **Ne jamais appeler les méthodes de lifecycle** : `BeginPlay`/`Tick`/`LateTick`/`FixedTick`/`EndPlay`
  sur le graphe, les `On…` sur le script, `Execute()` sur les systèmes — réservés aux systèmes et au
  manager.
- **`SetScript<T>` ne delete pas l'ancien script** : il appelle seulement `OnEndPlay()` puis écrase le
  pointeur → **fuite** si on change de script sans `ClearScript()` d'abord.
- **`GetScript<T>` = `static_cast` non vérifié** : type erroné = UB.
- **`FindByLayer` AJOUTE** au vecteur fourni (`Clear()` manuel si besoin).
- **`SetActive` / `SetVisible` non récursifs** ; **`SetParent` / `DestroyRecursive` immédiats** :
  prudence en itération de query (préférer la destruction différée du monde).
- **`UnregisterScene` AVANT `delete scene`**, sinon *dangling pointers* dans les systèmes du scheduler.
- **Durées de vie** : graphe ≤ monde ; manager/script/systèmes ≤ monde ; `SceneLifecycleManager` ≤
  scheduler.
- **Non thread-safe** partout (thread principal uniquement).
- **Statuts spec / non implémentés** : `NkTransitionType::Custom` et `NkSceneManager::Update` (fondus
  non animés, Phase 5) ; `NkPrefabInstance::SetOverride<T>` (ne sérialise rien) ;
  `NkPrefabRegistry::LoadFromFile` (déclaration sans corps).
- **Incohérences de doc** : `NkSceneTransition::FadeWhite(0.8f)` appelle une static **inexistante** ;
  `DumpRegisteredScenes()` documenté « avec logger » mais **sans paramètre** ; `NkPrefab`
  forward-déclaré dans `ecs::` (dans `NkSceneScript.h`) alors qu'il est défini dans `nkentseu::`.

---

### Exemple

```cpp
#include "Noge/ECS/Scene/NkSceneGraph.h"
#include "Noge/ECS/Scene/NkSceneManager.h"
#include "Noge/ECS/Scene/NkSceneLifecycleSystem.h"
#include "Noge/ECS/Scene/NkSceneScript.h"
#include "Noge/ECS/Prefab/NkPrefab.h"
using namespace nkentseu;
using namespace nkentseu::ecs;

// 1) Un script de scène : la logique du niveau.
class ArenaScript final : public NkSceneScript {
    void OnBeginPlay() noexcept override {
        NkPrefab enemy("Enemy");
        enemy.WithComponent<Health>(100).WithLayer(2).WithTag(kTagEnemy);
        for (int i = 0; i < 8; ++i)
            SpawnPrefab(enemy, { float(i) * 2.f, 0.f, 0.f }, "Enemy");
    }
    void OnTick(float dt) noexcept override { /* IA, gameplay */ }
};

// 2) Une factory de scène : crée le graphe, lui attache le script.
NkSceneGraph* makeArena(NkWorld& world) {
    auto* scene = new NkSceneGraph(world, "Arena");   // ownership → manager
    scene->SetScript<ArenaScript>();                  // ne pas delete le retour
    return scene;
}

// 3) Câblage : manager + systèmes de lifecycle.
NkSceneManager mgr(world);
mgr.Register("Arena", makeArena);
mgr.LoadScene("Arena");                               // synchrone : BeginPlay() lancé

RegisterSceneLifecycle(scheduler, mgr.GetCurrent()); // Tick/LateTick/FixedTick branchés
// boucle : scheduler.Run(...) appelle Tick/LateTick/FixedTick de la scène.
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
