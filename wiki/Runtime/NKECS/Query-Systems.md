# Requêtes, systèmes et scheduler

> Couche **Runtime** · NKECS · Interroger le monde par **composants** (`NkQueryResult`),
> écrire la logique en **systèmes** (`NkSystem`, `NkSystemDesc`), et les faire tourner en
> parallèle par phases via le **scheduler** (`NkScheduler`, `NkJobPool`).

Un ECS sépare deux choses : **où vivent les données** (les entités et leurs composants, dans le
monde) et **ce qu'on en fait** (la logique, qui lit certains composants et en écrit d'autres). Cette
page traite la seconde moitié. Une fois que les composants sont rangés par archétypes, toute la
logique du jeu se ramène à la même phrase : *« pour chaque entité qui possède tels composants, fais
ceci »*. Le défi n'est pas de l'écrire — c'est de le faire **vite** (sans allocation ni virtuel dans
la boucle chaude), **sûrement** (sans course quand deux systèmes touchent les mêmes données) et
**dans le bon ordre** (la physique avant le rendu, l'entrée avant la physique). NKECS répond aux
trois avec, respectivement, la requête typée, le descripteur de système, et le scheduler à DAG.

Tous les types vivent dans le **même namespace**, et reposent sur quelques types externes (le monde,
le graphe d'archétypes, le masque de composants) documentés ailleurs.

- **Namespace** : `nkentseu::ecs`
- **Headers réels** : `NKECS/Query/NkQuery.h`, `NKECS/System/NkSystem.h`, `NKECS/System/NkScheduler.h`
- **Header parapluie utilisé dans les exemples** : `#include "NKECS/NkECS.h"` *(aspirationnel)*

---

## Interroger le monde : `NkQueryResult`

La requête est la porte d'entrée vers les données. On ne demande **jamais** une entité par son
identifiant pour faire de la logique en masse — on demande *« toutes les entités qui ont à la fois un
`Position` et un `Velocity` »*, et on les parcourt. `world.Query<Position, Velocity>()` renvoie un
`NkQueryResult<Position, Velocity>` : un petit objet qui sait quels archétypes correspondent et sait
les balayer. C'est lui qu'on itère.

La force de cette requête, c'est qu'elle est résolue **à la compilation**. Les types `Ts...` sont
connus, donc il n'y a **aucun virtuel**, **aucune allocation dynamique** pendant l'itération, et
l'accès se fait directement dans les **tableaux contigus** (SoA) du graphe d'archétypes. Le résultat
n'est **pas** copiable (il porte des références vers le graphe et l'index du monde) mais il est
**déplaçable** — c'est pour cela qu'on peut le retourner par valeur depuis `Query`.

```cpp
world.Query<Position, Velocity>().ForEach(
    [dt](NkEntityId e, Position& p, Velocity& v) {
        p.value += v.value * dt;       // mis à jour en place, dans le pool SoA
    });
```

Deux subtilités décident de la signature de votre lambda. D'abord, un composant demandé en **`const`**
(`Query<const Transform>()`) arrive en `const T&` : c'est la façon d'exprimer une **lecture seule** et
de documenter qu'un système ne modifie pas cette donnée. Ensuite, le premier paramètre de votre
fonction est **toujours** le `NkEntityId` de la ligne courante, suivi des composants dans l'ordre des
`Ts`.

> **En résumé.** `world.Query<Ts...>()` rend un `NkQueryResult<Ts...>` (déplaçable, non copiable).
> Tout est résolu à la compilation : zéro virtuel, zéro alloc, accès SoA direct. Un `const T` exprime
> une lecture seule. La lambda reçoit `(NkEntityId, T1&, T2&, …)`.

### Filtrer : `With` et `Without`

Souvent, on veut affiner sans pour autant **recevoir** le composant filtrant. Une requête sur les
soldats vivants et blindés : on parcourt leur `Health`, mais on ne veut pas du `Armor` ni du `Dead`
comme arguments — juste s'en servir comme **conditions**. C'est exactement ce que font les filtres
fluents, chaînables, qui retournent `NkQueryResult&` :

```cpp
world.Query<Health>()
     .With<Armor>()        // ne garder que les entités qui ONT aussi Armor
     .Without<Dead>()      // exclure celles qui ont Dead
     .ForEach([](NkEntityId, Health& h) { /* ... */ });
```

`With<T>()` ajoute `T` aux composants **requis** (l'entité doit le posséder) sans l'injecter dans la
lambda. `Without<T>()` exclut toute entité qui possède `T`. Ce n'est **pas** la même chose qu'ajouter
`T` aux `Ts` de la requête : là, `T` deviendrait un argument de la fonction. `With`/`Without`
agissent uniquement sur le **filtrage des archétypes**.

> **En résumé.** `With<T>()` = condition « possède T » sans le recevoir en argument ; `Without<T>()`
> = condition « ne possède pas T ». Chaînables, ils ne changent **pas** la signature de la lambda
> (contrairement à mettre `T` dans `Query<…>`).

### Deux façons de balayer : `ForEach` et `ForEachBatch`

`ForEach` parcourt **entité par entité** : pratique, lisible, c'est le défaut. Mais quand la boucle
est vraiment chaude et qu'on veut vectoriser (SSE/AVX), on préfère travailler sur les **tableaux
bruts** d'un archétype d'un coup. C'est `ForEachBatch` : il appelle votre fonction **une fois par
archétype non vide**, en lui passant le `count` et les pointeurs de début de chaque pool.

```cpp
world.Query<Position, Velocity>().ForEachBatch(
    [dt](uint32 count, const NkEntityId* ids,
         Position* pos, Velocity* vel) {
        for (uint32 i = 0; i < count; ++i)   // boucle serrée, vectorisable
            pos[i].value += vel[i].value * dt;
    });
```

> **En résumé.** `ForEach` = entité par entité, simple. `ForEachBatch` = archétype par archétype avec
> pointeurs bruts contigus, pour le code vectorisé/SIMD. Même requête, deux granularités.

---

## Écrire la logique : `NkSystem` et `NkSystemDesc`

Une requête isolée ne dit rien de **quand** ni **comment** elle s'exécute par rapport aux autres. Un
**système** encapsule un morceau de logique récurrent et, surtout, **se décrit** lui-même : ce qu'il
lit, ce qu'il écrit, dans quelle phase il tourne, qui doit passer avant lui. Cette description est ce
qui permet au scheduler de paralléliser sans course.

On hérite de `NkSystem` et on implémente deux méthodes pures. `Describe()` renvoie un `NkSystemDesc`
construit de façon fluente — appelé **une seule fois** à l'enregistrement. `Execute(world, dt)` est le
corps, appelé **chaque frame**.

```cpp
class MovementSystem : public NkSystem {
public:
    NkSystemDesc Describe() const override {
        return NkSystemDesc{}
            .Named("Movement")
            .InGroup(NkSystemGroup::Update)
            .Reads<Velocity>()
            .Writes<Position>();           // Writes implique Reads
    }
    void Execute(NkWorld& world, float32 dt) override {
        world.Query<Position, const Velocity>().ForEach(
            [dt](NkEntityId, Position& p, const Velocity& v) {
                p.value += v.value * dt;
            });
    }
};
```

Le `NkSystemDesc` est le cœur du contrat. `Reads<T>()` et `Writes<T>()` remplissent des masques de
composants — **attention** : `Writes<T>()` arme aussi le bit de lecture (écrire suppose lire). C'est
de ces masques que le scheduler déduit les conflits : deux systèmes qui écrivent le même composant,
ou l'un qui lit ce que l'autre écrit, **entrent en conflit** et ne pourront pas tourner en parallèle.
La méthode `ConflictsWith` formalise exactement ce test.

Le reste du descripteur affine l'ordonnancement : `InGroup` choisit la phase, `WithPriority` ordonne
*au sein* d'un groupe (plus petit = plus tôt), `After<OtherSystem>()` impose une dépendance explicite,
`Sequential()` interdit la parallélisation, `Named` nomme le système pour le débogage, et
`Excludes<T>()` marque des composants à exclure des requêtes internes.

> **En résumé.** Un `NkSystem` implémente `Describe()` (une fois, → `NkSystemDesc`) et
> `Execute(world, dt)` (chaque frame). Le `NkSystemDesc` déclare Reads/Writes (un Write implique un
> Read), le groupe, la priorité, les dépendances `After` — c'est lui qui rend la parallélisation
> automatique **et** sûre.

### Les phases : `NkSystemGroup`

Tous les systèmes ne tournent pas au même moment. `NkSystemGroup` nomme les **phases** d'une frame :
`PreUpdate` (préparation, lecture d'entrée), `Update` (gros de la logique), `PostUpdate`
(consolidation), `FixedUpdate` (pas de temps fixe pour la physique), `Render` (préparation du rendu).
La sentinelle `COUNT` vaut le nombre de groupes. Le scheduler exécute `PreUpdate → Update → PostUpdate
→ Render` dans `Run`, **mais** `FixedUpdate` est traité à part (via `RunFixed`) — un point sur lequel
on revient plus bas, car il piège facilement.

> **En résumé.** Les groupes ordonnent les phases d'une frame. `Run` enchaîne PreUpdate, Update,
> PostUpdate, Render — **pas** FixedUpdate, qui passe par `RunFixed`.

### Le raccourci : `NkLambdaSystem`

Créer une classe pour trois lignes de logique est lourd quand on prototype. `NkLambdaSystem` construit
un système directement à partir d'un descripteur et d'une lambda — idéal pour itérer vite, quitte à
promouvoir en vraie classe ensuite.

```cpp
scheduler.AddLambda(
    NkSystemDesc{}.Named("Gravity").Writes<Velocity>(),
    [](NkWorld& w, float32 dt) {
        w.Query<Velocity>().ForEach(
            [dt](NkEntityId, Velocity& v) { v.value.y -= 9.81f * dt; });
    });
```

> **En résumé.** `NkLambdaSystem` (via `AddLambda`) = un système à partir d'un descripteur + une
> lambda, pour prototyper sans déclarer de classe.

---

## Orchestrer : `NkScheduler` et `NkJobPool`

Reste à faire tourner tout ce monde. Le `NkScheduler` enregistre les systèmes, **lit leurs
descripteurs**, en déduit un graphe de dépendances (DAG) — arête A→B si A doit précéder B, que ce soit
par un `After<>` explicite ou par un **conflit d'écriture** détecté dans le même groupe — puis exécute
chaque groupe en **vagues parallèles** grâce à un thread pool, le `NkJobPool`.

On enregistre, on initialise, on lance chaque frame :

```cpp
NkScheduler scheduler;                       // pool de threads auto-dimensionné
scheduler.AddSystem<MovementSystem>();       // possédé par le scheduler
scheduler.AddSystem<CollisionSystem>();
scheduler.Init(world);                       // OnCreate + construit le DAG

// boucle de jeu
scheduler.RunFixed(world, fixedDt);          // physique, pas fixe
scheduler.Run(world, dt);                    // PreUpdate..Render + flush
```

Le scheduler **possède** les systèmes (via `unique_ptr`) : `AddSystem` instancie le type et renvoie
une **référence** valide tant que le scheduler vit. La grande règle de sûreté : comme les systèmes
d'une même vague peuvent s'exécuter **en parallèle**, on ne fait **jamais** de changement structurel
direct (ajout/suppression d'entité) dans `Execute` — on passe par le différé du monde
(`FlushDeferred`), que le scheduler vide au bon moment.

> **En résumé.** Le `NkScheduler` lit les descripteurs → DAG (dépendances `After` + conflits
> d'écriture) → vagues parallèles via `NkJobPool`. Il **possède** les systèmes (références stables).
> Jamais de changement structurel direct en parallèle : utilisez le différé.

### Le piège du FixedUpdate et des cycles

Deux comportements méritent qu'on s'y arrête, parce qu'ils ne crient pas quand on se trompe.
**Premièrement**, `Run` n'exécute **pas** `FixedUpdate` : si vous mettez votre physique dans ce
groupe en espérant qu'elle tourne, elle ne tournera jamais tant que vous n'appelez pas `RunFixed`
séparément depuis votre boucle (typiquement N fois pour rattraper le temps accumulé).
**Deuxièmement**, un **cycle** de dépendances ne lève pas d'erreur : la vague devient vide,
l'exécution du groupe s'arrête **silencieusement**, et les systèmes restants ne tournent pas. Si un
système semble « ne jamais s'exécuter », suspectez un cycle (`After` croisés, ou conflits d'écriture
qui se contredisent).

> **En résumé.** `FixedUpdate` ne tourne **que** via `RunFixed`, jamais via `Run`. Un cycle de
> dépendances n'est pas signalé : il arrête silencieusement le reste du groupe.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Chacun est détaillé (complexité, comportement, usages)
dans la « Référence complète ».

### `NkQueryResult<Ts...>` — `NKECS/Query/NkQuery.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkQueryResult(graph, index, required, withFilter, withoutFilter)` `noexcept` | Construit par `NkWorld::Query<Ts...>()` ; déplaçable, **non copiable**. |
| Filtres | `With<T>()` `[O(kWords)]`, `Without<T>()` `[O(1)]` | Conditions « possède / ne possède pas T », chaînables (`NkQueryResult&`). |
| Itération | `ForEach(fn)` | Entité par entité ; `fn(NkEntityId, T1&, …)`. `[O(archétypes × entités)]` |
| Itération | `ForEachBatch(fn)` | Archétype par archétype (SIMD) ; `fn(count, ids, T1*, …)`. `[O(archétypes)]` |
| Stats | `Count()` `[O(archétypes)]` | Nombre total d'entités correspondantes (`const_cast` interne). |
| Stats | `First()` | Première entité, ou `NkEntityId::Invalid()`. |
| Stats | `Any()` | `true` s'il en existe au moins une (= `Count() > 0`, non court-circuité). |
| Détail interne | `detail::MakeMask<Ts...>()`, `detail::GetPoolPtr<T>(arch, row)` | Helpers (masque depuis types ; pointeur de pool SoA). |

### `NkSystemGroup` / `NkSystemDesc` / `NkSystem` — `NKECS/System/NkSystem.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Phases | `NkSystemGroup` : `PreUpdate`, `Update`, `PostUpdate`, `FixedUpdate`, `Render`, `COUNT` | Phases d'exécution (FixedUpdate exécuté à part). |
| Desc — masques | `Reads<T>()`, `Writes<T>()` (implique Read), `Excludes<T>()` | Composants lus / écrits / exclus des requêtes. |
| Desc — ordre | `InGroup(g)`, `WithPriority(p)`, `After<S>()`, `Sequential()`, `Named(n)` | Groupe, priorité (petit = tôt), dépendance, mono-thread, nom. |
| Desc — champs | `readMask`, `writeMask`, `excludeMask`, `group`, `runInParallel`, `priority`, `name`, `afterTypes` | État brut du descripteur. |
| Desc — conflit | `ConflictsWith(other)` `[O(kWords)]` | `true` si write/read se chevauchent (base de la détection de course). |
| Système | `Describe()` **pur**, `Execute(world, dt)` **pur** | Décrire (1×) / exécuter (chaque frame). |
| Système — hooks | `OnCreate` (appelé par `Init`), `OnDestroy`/`OnEnable`/`OnDisable` (**non appelés**) | Cycle de vie ; seul `OnCreate` est invoqué. |
| Système — état | `IsEnabled()`, `SetEnabled(b)` (n'appelle pas les hooks), `mEnabled` (protégé) | Activation. |
| Macro | `NK_SYSTEM(Type)` | **No-op** (purement cosmétique). |
| Lambda | `NkLambdaSystem(desc, fn)`, `ExecFn = std::function<void(NkWorld&, float32)>` | Système depuis une lambda. |

### `NkJobPool` / `NkScheduler` — `NKECS/System/NkScheduler.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Pool | `NkJobPool(threadCount = 0)` | Thread pool ; 0 = `max(1, hw_concurrency() - 1)`. |
| Pool | `Submit(job)`, `WaitAll()`, `ThreadCount()` | Soumettre (file **LIFO**), attendre tout, compter. |
| Construction | `NkScheduler(jobThreads = 0)` | Crée le scheduler et son pool. |
| Enregistrer | `AddSystem<T>(args…)` → `T&`, `AddLambda(desc, fn)` → `NkLambdaSystem&` | Instancie, possède (`unique_ptr`), renvoie une référence. |
| Enregistrer | `SetEnabled<T>(b)` `[O(systèmes)]` | (Dés)active le premier système de type `T`. |
| Initialiser | `Init(world)` | `OnCreate` sur les systèmes activés + construit le DAG. |
| Exécuter | `Run(world, dt)` | DrainEvents → PreUpdate/Update/PostUpdate/Render → Flush. **Pas FixedUpdate.** |
| Exécuter | `RunFixed(world, fixedDt)` | FixedUpdate + Flush ; appelé séparément. |
| Stats | `SystemCount()`, `GetSystemName(index)` | Nombre / nom (`"<invalid>"` hors borne). |

---

## Référence complète

Chaque élément est repris en détail : comportement et complexité, puis usages **par domaine**. Les
éléments triviaux sont décrits brièvement ; les mécanismes structurants (itération, conflits,
scheduler) le sont à fond.

### `NkQueryResult` — itérer le monde

Le résultat de requête est l'outil le plus utilisé du module : toute la logique de gameplay finit par
une requête. Il est **non copiable** (il référence le graphe d'archétypes et l'index du monde dont la
durée de vie appartient au `NkWorld`) mais **déplaçable**, ce qui autorise le `return` par valeur de
`Query`. En interne il combine trois masques — `required` (les `Ts`), `mWithFilter` (les `With`),
`mWithoutFilter` (les `Without`) — en un `mFinalInclude = required ∪ withFilter`, et délègue le
balayage à `mGraph.ForEachMatching`. Note : la référence `mIndex` est stockée mais **non utilisée**.

**`ForEach` et `ForEachBatch`.** Le premier appelle votre fonction pour **chaque ligne** de chaque
archétype non vide — coût `O(archétypes correspondants × entités)`. Le second l'appelle **une fois par
archétype**, avec `count` et les pointeurs de pools bruts — coût `O(archétypes)` et code vectorisable.
Aucun des deux n'alloue ni ne passe par un virtuel.

- **Rendu** — collecter tous les `Renderable` visibles pour remplir un tampon de commandes ;
  `ForEachBatch` pour pousser des matrices monde en masse vers un buffer GPU.
- **ECS / scène** — la forme canonique de tout système : `Query<A, B>().ForEach(...)` met à jour les
  composants en place dans leurs pools SoA.
- **Physique** — intégrer `Position` à partir de `Velocity` ; détecter les paires en parcourant les
  `Collider`. `ForEachBatch` rend l'intégration vectorisable.
- **Animation** — faire avancer chaque `AnimState` ; échantillonner les squelettes possédant un
  `SkinnedMesh`.
- **Gameplay / IA** — sélectionner les cibles (`With<Enemy>().Without<Dead>()`), évaluer les capteurs
  d'un agent, appliquer des dégâts à tous les `Health`.
- **Audio** — mettre à jour la position des sources spatialisées (`Query<Transform, AudioSource>`).
- **UI / 2D** — disposer/dessiner les widgets portant un composant `Layout` ou `Sprite`.
- **GPU / threading** — `ForEachBatch` est le pont naturel vers le SIMD et l'upload par blocs ;
  combiné au scheduler, plusieurs systèmes balaient des composants disjoints en parallèle.

**Filtres `With` / `Without`.** `With<T>()` ajoute `T` au requis et recalcule `mFinalInclude`
(`O(kWords)`) sans l'injecter dans la lambda ; `Without<T>()` arme un bit d'exclusion (`O(1)`). Le cas
d'école est le tri d'entités par *tags* : `With<Active>().Without<Hidden>()` restreint sans alourdir la
signature. À ne pas confondre avec mettre `T` dans `Query<…>`, ce qui en ferait un argument reçu.

**`Count`, `First`, `Any`.** Trois statistiques `const noexcept`. `Count()` somme `arch->Count()` sur
tous les archétypes correspondants (`O(archétypes)`) — il recourt à un `const_cast<NkArchetypeGraph&>`
en interne car `ForEachMatching` n'est pas `const`. `First()` renvoie l'entité de tête du premier
archétype non vide, ou `NkEntityId::Invalid()` si la requête est vide. `Any()` est implémenté comme
`Count() > 0` : il **n'est pas court-circuité** (il parcourt tout), à garder en tête sur de gros
mondes — pour un simple test d'existence, comprenez qu'il a le coût de `Count`. Usages : afficher un
compteur (Count), saisir un singleton/premier joueur (First), activer une UI conditionnelle (Any).

**`detail::MakeMask` / `detail::GetPoolPtr`.** Exposés dans le header mais **détail
d'implémentation**. `MakeMask<Ts...>()` construit le `NkComponentMask` en activant le bit de chaque
`NkIdOf<remove_const_t<Ts>>()` via un fold (`O(nombre de Ts)`). `GetPoolPtr<T>(arch, row)` renvoie
`arch->ComponentData<remove_const_t<T>>()` — le début du pool SoA du composant ; le paramètre `row`
est **ignoré**. À ne pas appeler directement dans du code applicatif.

### `NkSystemGroup` — les phases

`enum class : uint8` aux valeurs explicites : `PreUpdate=0`, `Update=1`, `PostUpdate=2`,
`FixedUpdate=3`, `Render=4`, `COUNT=5`. L'ordre nominal est PreUpdate → Update → PostUpdate →
FixedUpdate → Render, **mais** le scheduler n'enchaîne que les quatre autres dans `Run` ; `FixedUpdate`
est exécuté à part par `RunFixed`. Le mapping naturel par domaine : entrée et préparation en
`PreUpdate`, logique de jeu/IA/animation en `Update`, résolution des suppressions et consolidation en
`PostUpdate`, physique à pas constant en `FixedUpdate`, extraction des données de rendu et tri des
batches en `Render`. `COUNT` sert de borne pour dimensionner des tableaux indexés par groupe.

### `NkSystemDesc` — le contrat déclaratif

C'est la pièce qui rend la parallélisation **automatique et sûre**. Plutôt que de coder l'ordre à la
main, chaque système **déclare** ses accès et le scheduler en déduit l'ordonnancement.

**Masques.** `Reads<T>()` arme le bit de `T` dans `readMask`. `Writes<T>()` l'arme dans `writeMask`
**et** `readMask` (écrire suppose lire). `Excludes<T>()` l'arme dans `excludeMask` (composants à
écarter des requêtes internes, optimisation). Ces trois masques sont la matière première de la
détection de conflit.

**Réglages d'ordre.** `InGroup(g)` fixe la phase ; `WithPriority(p)` ordonne au sein du groupe (plus
petit = plus tôt) ; `After<OtherSystem>()` ajoute `type_index(typeid(OtherSystem))` à `afterTypes`
pour forcer une précédence explicite ; `Sequential()` met `runInParallel = false` (le système ne
sera jamais parallélisé) ; `Named(n)` fixe le nom de débogage (défaut `"<unnamed>"`). Tous ces
mutateurs sont fluents (`NkSystemDesc&`, `noexcept`) et se chaînent.

**`ConflictsWith(other)`.** Renvoie `true` si, mot par mot sur `NkComponentMask::kWords`, il y a un
chevauchement write-vs-read, write-vs-write ou read-vs-write entre les deux descripteurs (`O(kWords)`,
`[[nodiscard]] noexcept`). C'est **la** base de la détection de course : le scheduler s'en sert pour
décider si deux systèmes du même groupe peuvent coexister dans une vague parallèle.

- **Physique vs rendu** — un `PhysicsSystem` qui `Writes<Transform>` entre en conflit avec un
  `RenderExtractSystem` qui `Reads<Transform>` : le DAG les sérialise (physique d'abord).
- **IA parallèle** — deux systèmes d'IA qui lisent les mêmes capteurs mais écrivent des composants
  **disjoints** ne conflictent pas → ils tournent dans la même vague.
- **Audio / UI** — déclarer `Reads` seul (pas de `Writes`) maximise le parallélisme : autant de
  lecteurs que voulu cohabitent.

### `NkSystem` — la classe de base

Abstraite. On implémente les deux purs : `Describe() const` (renvoie le descripteur, appelé **une
fois** à l'enregistrement) et `Execute(NkWorld&, float32 dt)` (le corps, chaque frame). Ni l'un ni
l'autre n'est `noexcept`. Les hooks de cycle de vie sont `OnCreate`, `OnDestroy`, `OnEnable`,
`OnDisable` — tous `noexcept` et vides par défaut — **mais attention** : seul `OnCreate` est réellement
invoqué (par `NkScheduler::Init`) ; `OnDestroy`/`OnEnable`/`OnDisable` sont **déclarés mais jamais
appelés** par le scheduler dans l'état actuel. `IsEnabled()` lit `mEnabled` (protégé, `true` par
défaut) et `SetEnabled(b)` le fixe **sans** déclencher `OnEnable`/`OnDisable`. En pratique : mettez
toute votre initialisation dépendante du monde dans `Execute` (premier appel) ou acceptez `OnCreate`,
mais ne comptez pas sur les autres hooks tant qu'ils ne sont pas câblés. Usages typiques d'un système :
mouvement, collision, IA, animation, extraction de rendu, mixage audio — chaque sous-système du moteur
peut s'exprimer ainsi.

### Macro `NK_SYSTEM`

`#define NK_SYSTEM(Type)` est un **no-op** : elle ne fait rien, n'a aucun effet, et sert uniquement de
marqueur de lisibilité au-dessus d'une déclaration de système. On peut l'omettre sans conséquence.

### `NkLambdaSystem` — système jetable

`final`, hérite de `NkSystem`. Construit avec un `NkSystemDesc` (déplacé) et une `ExecFn =
std::function<void(NkWorld&, float32)>` (déplacée). `Describe()` renvoie une copie du descripteur ;
`Execute(world, dt)` appelle la lambda **si** elle est non vide. C'est l'outil du **prototypage** et
des petits systèmes ad hoc — un comportement de gravité, un nettoyeur d'entités mortes, un système de
debug — sans la cérémonie d'une classe dédiée. On le crée presque toujours via
`NkScheduler::AddLambda`.

### `NkJobPool` — le thread pool

Pool minimaliste à base de `std::thread`, moteur de la parallélisation des vagues. Le constructeur
`NkJobPool(threadCount = 0)` crée `threadCount` threads, ou `max(1, hardware_concurrency() - 1)` si
0 (on laisse un cœur au thread principal), et lance un `WorkerLoop` par thread ; le destructeur signale
l'arrêt (`mStopping = true`), réveille tout le monde et `join`. `Submit(job)` pousse le job et retourne
**immédiatement** ; `WaitAll()` bloque jusqu'à ce que tous les jobs en vol soient finis (compteur
`mPending` + `mDoneCv`) ; `ThreadCount()` renvoie le nombre de threads.

**Piège à connaître** : la file est **LIFO** (`push_back` au submit, `back()`/`pop_back()` côté
worker), donc l'ordre de prise des jobs n'est **pas** FIFO. Ce n'est pas un problème pour le scheduler,
qui attend de toute façon **toute** la vague via `WaitAll` avant de passer à la suivante — l'ordre
interne d'une vague est indifférent. Au-delà du scheduler, ce pool sert tout calcul *fire-and-forget*
batché (découpage d'un balayage `ForEachBatch` en tâches, génération de chunks, chargement asynchrone).

### `NkScheduler` — l'orchestrateur

Le scheduler relie tout. Sa stratégie, du header : collecter les `NkSystemDesc`, construire un **DAG**
(arête A→B si A précède B : dépendances `After<>` explicites **plus** conflits d'écriture implicites
dans le même groupe), trier topologiquement (Kahn), puis exécuter par **vagues parallèles** via le
job pool. Les changements structurels passent toujours par le différé (`world.FlushDeferred()`).

**Enregistrement.** `AddSystem<T>(args…)` `static_assert` que `T` dérive de `NkSystem`, l'instancie via
`make_unique`, l'enregistre et renvoie une **référence** au système (valide tant que le scheduler vit,
car il le **possède**). `AddLambda(desc, fn)` fait de même pour un `NkLambdaSystem`. `SetEnabled<T>(b)`
cherche le premier système dont `typeId == type_index(typeid(T))` et appelle son `SetEnabled` puis
s'arrête (`O(systèmes)`).

**Initialisation.** `Init(world)` appelle `OnCreate(world)` sur chaque système **activé**, construit le
DAG (`RebuildDAG`), et marque `mInitialized`. À noter : `RegisterSystem` capture le descripteur, le
`typeId`, et — si le scheduler est déjà initialisé — reconstruit le DAG, ce qui rend l'ajout de
systèmes après `Init` sûr.

**Exécution.** `Run(world, dt)` initialise au besoin puis enchaîne : `DrainEvents` → `RunGroup`
PreUpdate → Update → PostUpdate → Render → `FlushDeferred` → `DrainEvents`. Il **n'exécute pas**
`FixedUpdate`. `RunFixed(world, fixedDt)` exécute `RunGroup(FixedUpdate)` puis `FlushDeferred`, et doit
être appelé séparément par la boucle de jeu (typiquement en accumulateur, plusieurs fois par frame
visible). `SystemCount()` et `GetSystemName(index)` (renvoie `desc.name`, ou `"<invalid>"` hors borne)
servent aux outils et au profilage.

**Comment une vague s'exécute** (`RunGroup`, en interne). On collecte les systèmes **activés** du
groupe, on les trie (`TopoSort`, Kahn avec file triée par priorité croissante), puis on avance par
vagues : une vague rassemble les systèmes dont **toutes** les dépendances (présentes dans le
sous-ensemble) sont déjà finies. Si la vague est de taille 1 **ou** que son premier système a
`runInParallel == false`, elle s'exécute **séquentiellement** ; sinon chaque système est soumis au
`mJobPool` puis `WaitAll`. Et le point critique : si à un moment la vague est **vide** alors qu'il
reste des systèmes, c'est un **cycle de dépendances** → l'exécution du groupe s'**arrête
silencieusement** (les systèmes restants ne tournent pas, aucune erreur n'est levée).

**Construction du DAG** (`RebuildDAG`). (1) reset des dépendances ; (2) pour chaque `afterType`,
ajout d'une arête depuis les systèmes de ce type ; (3) pour chaque paire de systèmes **du même groupe**
qui `ConflictsWith`, ordonnancement par `priority` (le plus prioritaire passe d'abord). Complexité
~`O(n² × kWords)`. `TopoSort` est en ~`O(n² log n)` (find + re-tri par priorité à chaque étape).

Pièges et ownership, par domaine :
- **Threading** — `Execute` peut tourner **en parallèle** sur les systèmes d'une vague : aucun
  changement structurel direct (créer/détruire entité, ajouter/retirer composant) — passez par
  `FlushDeferred`. La parallélisation n'a lieu que si `runInParallel` (du premier de la vague) **et**
  vague de taille > 1.
- **Physique** — mettez la physique à pas fixe en `FixedUpdate` **et** appelez `RunFixed` ; sinon elle
  ne s'exécute jamais (`Run` l'ignore).
- **Outils / éditeur** — `SystemCount`/`GetSystemName` alimentent un panneau de profilage ; un cycle
  qui « avale » des systèmes se diagnostique en voyant un système attendu absent de l'exécution.
- **Gameplay** — `AddSystem`/`AddLambda` renvoient des références stables : on peut les garder pour
  régler des paramètres à chaud, ou (dés)activer un système avec `SetEnabled`.
- **Audio / UI** — déclarez ces systèmes en lecture seule autant que possible (`Reads` sans `Writes`)
  pour qu'ils cohabitent dans les vagues parallèles sans sérialisation.

---

### Exemple récapitulatif

```cpp
#include "NKECS/NkECS.h"
using namespace nkentseu::ecs;

// 1) Un vrai système : intègre la position depuis la vitesse.
class MovementSystem : public NkSystem {
public:
    NkSystemDesc Describe() const override {
        return NkSystemDesc{}
            .Named("Movement")
            .InGroup(NkSystemGroup::Update)
            .Reads<Velocity>()
            .Writes<Position>();              // Writes implique Reads
    }
    void Execute(NkWorld& world, float32 dt) override {
        world.Query<Position, const Velocity>().ForEach(
            [dt](NkEntityId, Position& p, const Velocity& v) {
                p.value += v.value * dt;      // accès SoA direct, sans alloc
            });
    }
};

// 2) Mise en place et boucle.
NkScheduler scheduler;                        // pool auto-dimensionné
scheduler.AddSystem<MovementSystem>();
scheduler.AddLambda(                          // un système jetable
    NkSystemDesc{}.Named("Gravity").Writes<Velocity>(),
    [](NkWorld& w, float32 dt) {
        w.Query<Velocity>().ForEach(
            [dt](NkEntityId, Velocity& v) { v.value.y -= 9.81f * dt; });
    });
scheduler.Init(world);                        // OnCreate + DAG

while (running) {
    scheduler.RunFixed(world, fixedDt);       // physique : SÉPARÉ de Run
    scheduler.Run(world, dt);                 // PreUpdate..Render + flush différé
}

// 3) Une requête filtrée, hors système.
uint32 vivants = world.Query<Health>()
                      .With<Armor>()
                      .Without<Dead>()
                      .Count();
```

---

[← Index NKECS](README.md) · [Récap NKECS](../NKECS.md) · [Couche Runtime](../README.md)
