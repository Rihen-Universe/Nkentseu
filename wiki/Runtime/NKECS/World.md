# Le monde ECS

> Couche **Runtime** · NKECS · Le cœur du système entités-composants : le `NkWorld` qui crée,
> détruit et fait vivre les entités, l'identifiant fort `NkEntityId`, les builders fluides
> `NkEntityBuilder` / `NkBatchBuilder`, et les types fondamentaux (`NkSpan`, constantes de
> dimensionnement, assertions).

Un moteur de jeu finit toujours par poser la même question : **où ranger l'état du monde ?**
Une scène, c'est des milliers d'objets — un vaisseau a une position, une vitesse, un modèle 3D,
des points de vie ; une particule n'a qu'une position et une couleur ; une lumière n'a ni l'un ni
l'autre. L'approche orientée objet classique (une classe `GameObject` avec tous les champs
possibles) gaspille la mémoire et casse le cache. L'**ECS** (Entity-Component-System) renverse le
problème : une **entité** n'est qu'un identifiant ; ses **composants** sont des données pures
rangées en blocs contigus par type ; et les **systèmes** parcourent ces blocs à plein débit. Le
`NkWorld` est la façade qui orchestre tout cela.

Ce n'est **pas** un graphe de scène (pas de hiérarchie parent/enfant ici), ni un conteneur d'objets
polymorphes. C'est un **registre de données** organisé pour la performance : ajouter un composant à
une entité peut la faire **migrer** d'un *archétype* à un autre (un archétype = l'ensemble des
entités qui ont exactement la même combinaison de composants), ce qui garde chaque bloc
parfaitement homogène et donc parcourable sans saut de cache.

`NkWorld` est volontairement **autonome et générique** : il ne connaît **pas** les notions de plus
haut niveau (`NkGameObject`, `NkPrefab`, `NkBehaviourHost`) qui vivent dans la couche Nkentseu
au-dessus. Côté allocation, l'API publique est **zéro-STL** : tout passe par `NkVector`,
`NkFunction` et `NkSpan` maison (NKContainers / NKCore).

- **Namespace** : `nkentseu::ecs` (cœur), avec des **alias de commodité** réexportés dans
  `nkentseu` (sauf si `NK_ECS_NO_ALIASES` est défini avant l'inclusion)
- **Header parapluie** : `#include "NKECS/NKECS.h"`
- **Headers couverts ici** : `World/NkWorld.h`, `NkECSDefines.h`, `NKECS.h`
- **Thread-safety** : `NkWorld` n'est **pas** thread-safe — sérialisez les accès concurrents avec un
  mutex externe.

> **Portée de cette page.** Seuls ces trois headers sont documentés. Les symboles cités mais
> déclarés ailleurs (`NkComponentMask`, `ecs::NkIdOf<T>`, `NkEntityIndex`, `NkArchetypeGraph`,
> `NkQueryResult`, `NkGameplayEventBus`, `NkTypeRegistry`, les systèmes…) sont marqués
> **[externe]** et renvoyés à leurs propres pages.

---

## L'identifiant d'entité : `NkEntityId`

Avant de créer quoi que ce soit, il faut comprendre **comment on désigne** une entité. Le réflexe
naïf serait de garder un pointeur ou un simple indice entier. Les deux ont le même défaut mortel :
quand l'entité est détruite et que sa place est **recyclée** pour une autre, l'ancien indice pointe
soudain sur le **mauvais** objet — c'est le bug du *dangling reference*, source de plantages
silencieux. `NkEntityId` résout cela en combinant deux champs : un `index` (la position) **et** une
`gen` (la génération, un compteur). À chaque destruction d'une case, sa génération est incrémentée ;
un ancien ID, qui garde l'ancienne génération, ne **correspondra jamais** à la nouvelle entité
logée à la même place.

C'est un type **trivial de 8 octets** (`static_assert(sizeof(NkEntityId) == 8)` dans le header
parapluie), entièrement `constexpr noexcept`. On le construit comme un agrégat — `NkEntityId id{42, 1}`
désigne l'index 42, génération 1 — et on teste sa validité avec `IsValid()` (faux pour
`Invalid()` ou un ID jamais initialisé). Pour le réseau ou la persistance, `Pack()` l'aplatit en un
`uint64` (génération en poids fort, index en poids faible) et `Unpack()` le reconstruit ; ne tentez
**jamais** de refaire ces bits à la main.

```cpp
NkEntityId id = world.CreateEntity();
uint64 wire = id.Pack();              // sérialisable (réseau, sauvegarde)
NkEntityId back = NkEntityId::Unpack(wire);   // identique
if (id == back && id.IsValid()) { /* … */ }
```

Les comparaisons sont fournies : `==` / `!=` (index **et** gen égaux) et `<` (ordre via `Pack()`,
donc utilisable comme clé triée dans un conteneur ordonné).

> **En résumé.** `NkEntityId` = `index` + `gen` sur 8 octets, `constexpr`. La génération empêche
> qu'un ancien ID désigne par erreur une entité recyclée. `Pack()`/`Unpack()` pour le réseau et la
> sauvegarde (gen en poids fort), `IsValid()` pour tester, `<` pour ordonner.

---

## Créer des entités : à l'unité ou en masse

La voie la plus directe est `CreateEntity()` : elle alloue une entité **vide** (sans aucun
composant) et renvoie son `NkEntityId` en `O(1)` amorti. Mais en pratique on veut presque toujours
l'équiper de composants dans la foulée — d'où les **builders fluides**.

`Create()` renvoie un `NkEntityBuilder`. Subtilité importante : l'entité **existe déjà** dès l'appel
à `Create()` (le builder a appelé `CreateEntity()` dans son constructeur) ; les `With<T>(...)`
chaînés appliquent les composants **immédiatement**, et `Build()` ne fait que **rendre l'ID déjà
créé**. Ce n'est donc **pas** un patron « configurer puis valider » différé — l'entité est vivante
avant `Build()`.

```cpp
auto ship = world.Create()
    .With<Transform>({ .pos = {0,0,0} })
    .With<Velocity>()                  // valeur par défaut T{}
    .With<Health>({ 100 })
    .Build();                          // renvoie l'ID, rien de plus
```

Quand il faut peupler une scène — un champ d'astéroïdes, un système de particules, une foule — créer
les entités une par une est gaspilleur. `CreateBatch(count, out)` renvoie un `NkBatchBuilder` qui,
lui, **diffère** vraiment : chaque `With<T>(...)` enregistre le composant et sa valeur, mais rien
n'est créé tant qu'on n'a pas appelé `Build()`. Au `Build()`, les `count` entités sont allouées et
chaque composant leur est appliqué — en `O(count × nbComposants)`. Le tableau `out` doit pointer sur
au moins `count` éléments et rester **vivant** jusqu'au `Build()` ; la valeur passée à `With` est
**copiée** dans le lambda interne (capture par copie).

```cpp
NkEntityId ids[1000];
world.CreateBatch(1000, ids)
    .With<Position>()
    .With<Velocity>({ .x = 1.f })
    .Build();                          // 1000 entités créées ici, pas avant
```

> **En résumé.** `CreateEntity()` = une entité vide, `O(1)`. `Create()...Build()` = builder fluide
> dont l'entité est **déjà vivante** dès `Create()` (`With` applique en direct, `Build` retourne
> juste l'ID). `CreateBatch(count, out)...Build()` = N entités identiques, **réellement différées**
> jusqu'au `Build()`. Le tableau `out` appartient à l'appelant.

---

## Ajouter, lire, modifier les composants

Une fois l'entité créée, `Add<T>(id, value)` est l'opération centrale : elle **ajoute** le composant
s'il manque, ou **met à jour** sa valeur s'il est déjà là, et renvoie une **référence** au composant
stocké. C'est elle qui peut déclencher une **migration d'archétype** : si le composant est nouveau,
l'entité change de combinaison, donc de bloc mémoire — sa ligne de données est recopiée dans le
nouvel archétype et son ancienne place est récupérée par un *swap-remove*. Si le composant existait
déjà, c'est une simple écriture en place, `O(1)`.

Il faut bien distinguer trois façons de **toucher** un composant, car elles ne se comportent pas
pareil quand le composant est **absent** :

- `Add<T>` — crée **ou** met à jour. Toujours sûr, peut migrer l'archétype.
- `Set<T>` — met à jour **seulement si présent** ; **silencieusement ignoré** sinon. Pratique quand
  on sait que le composant existe, dangereux quand on n'en est pas sûr (le header recommande `Add<T>`
  pour garantir l'ajout).
- `GetRef<T>` — renvoie une référence directe, mais **asserte** (et comportement indéfini en release)
  si le composant n'existe pas.

Pour **lire**, `Get<T>` renvoie un **pointeur** : `nullptr` si l'entité n'a pas ce composant (ou pas
de record / archétype invalide) — c'est la lecture **sûre**, qu'on teste avant de déréférencer.
`Has<T>` répond juste par oui/non en `O(1)`. Pour **retirer**, `Remove<T>` supprime immédiatement
(et peut, comme `Add`, réorganiser les archétypes).

```cpp
if (Health* hp = world.Get<Health>(id)) {   // lecture sûre
    hp->value -= 10;
}
world.Set<Velocity>(id, {0,0,0});            // no-op si Velocity absent !
Transform& t = world.GetRef<Transform>(id);  // asserte si absent
world.Remove<Health>(id);                     // suppression immédiate
```

> **En résumé.** `Add<T>` ajoute **ou** met à jour (peut migrer d'archétype) ; `Set<T>` ne met à
> jour que si présent (sinon **ignoré**) ; `GetRef<T>` **asserte** si absent ; `Get<T>` renvoie un
> **pointeur** (`nullptr` = absent), la lecture sûre. `Has<T>` teste la présence en `O(1)`,
> `Remove<T>` supprime tout de suite.

---

## Le piège de l'itération : les opérations différées

Voici la règle qui sauve le plus de plantages en ECS : **ne modifiez pas la structure du monde
pendant que vous l'itérez.** Quand un système parcourt une `Query`, détruire une entité ou retirer
un composant **immédiatement** (`Destroy`, `Remove`) peut **invalider l'itérateur** en cours — un
bug classique et difficile à reproduire. La solution n'est pas d'interdire ces actions, mais de les
**différer** : `DestroyDeferred`, `RemoveDeferred`, `AddDeferred` enregistrent l'intention sans la
réaliser, puis `FlushDeferred()` les applique toutes en bloc, **en fin de frame**, quand plus rien
n'itère.

```cpp
for (auto [hp] : world.Query<Health>()) {
    if (hp.value <= 0)
        world.DestroyDeferred(/* l'entité courante */);  // sûr pendant l'itération
}
world.FlushDeferred();   // applique destructions/ajouts/retraits en attente
```

`Destroy(id)` immédiat existe (libération directe des ressources de l'entité) mais ne doit servir
**qu'en dehors** de toute itération de query.

> **En résumé.** Pendant un `Query`, n'appelez **jamais** `Destroy`/`Remove` immédiats : préférez
> `DestroyDeferred`/`RemoveDeferred`/`AddDeferred`, puis videz la file avec `FlushDeferred()` en fin
> de frame.

---

## Les événements gameplay : immédiat ou en file

Le monde porte aussi un **bus d'événements** gameplay (délégué à un `NkGameplayEventBus`
**[externe]**), pour découpler les sous-systèmes : un système signale « le joueur a cliqué », un
autre y réagit, sans se connaître. On s'abonne à un type d'événement `T` avec `Subscribe<T>(fn)`,
qui renvoie un `SubscriptionId` à **conserver** pour pouvoir se désabonner plus tard avec
`Unsubscribe<T>(id)`.

L'émission a **deux temporalités**, et le choix compte : `Emit<T>(event)` distribue **immédiatement**
(le handler s'exécute tout de suite, dans la pile d'appel courante), tandis que `QueueEvent<T>(event)`
**met en file** — l'événement ne sera distribué qu'au prochain `DrainEvents()`. La version en file
est précisément l'analogue, côté événements, des opérations différées : elle évite qu'un handler ne
modifie le monde au mauvais moment.

```cpp
auto sub = world.Subscribe<NkOnButtonClicked>([](const auto& e){ /* … */ });
world.Emit(NkOnButtonClicked{ .id = 7 });        // synchrone, ici et maintenant
world.QueueEvent(NkOnSliderChanged{ .v = 0.5f }); // différé
// … en fin de frame :
world.DrainEvents();                              // distribue les événements en file
world.Unsubscribe<NkOnButtonClicked>(sub);
```

> **En résumé.** `Subscribe<T>` rend un `SubscriptionId` (à garder pour `Unsubscribe<T>`). `Emit<T>`
> = distribution **synchrone** immédiate ; `QueueEvent<T>` = **différée**, vidée par `DrainEvents()`.

---

## Aperçu de l'API

Tous les éléments publics des trois headers, en un coup d'œil. Le détail (complexité, usages par
domaine) suit dans la « Référence complète ». Les types de retour marqués **[externe]** sont
déclarés dans d'autres headers du module.

### `NkEntityId` — identifiant fort d'entité (`NkECSDefines.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `index`, `gen` | Position + génération (8 octets au total). |
| Constantes | `kInvalidIndex` (`0xFFFFFFFF`), `kInvalidGen` (`0`) | Sentinelles d'invalidité. |
| Test | `IsValid()` `[O(1)]` | `index` et `gen` tous deux valides. |
| Sérialisation | `Pack()` `[O(1)]`, `static Unpack(uint64)` | Aplatir / reconstruire (gen en poids fort). |
| Fabrique | `static Invalid()` | ID invalide. |
| Opérateurs | `==`, `!=`, `<` | Égalité (index+gen) ; ordre via `Pack()`. |

### Identifiants et constantes (`NkECSDefines.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Types | `NkComponentId` (=`uint32`), `NkArchetypeId` (=`uint32`) | ID de type de composant / d'archétype. |
| Sentinelles | `kInvalidComponentId`, `kInvalidArchetypeId` (`0xFFFFFFFF`) | Valeurs invalides. |
| Dimensionnement | `kMaxComponentTypes` (256), `kMaxArchetypes` (4096), `kMaxEntities` (~1 048 576), `kChunkSize` (16 Ko), `kMaxSystemsPerGroup` (128) | Limites dures du système. |
| Alias numériques | `uint8/16/32/64`, `int8/16/32/64`, `usize`, `float32/64` | Réexports de `nkentseu::`. |

### Macros et `detail` (`NkECSDefines.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Macros | `NKECS_NODISCARD`, `NKECS_INLINE`, `NKECS_FORCEINLINE` | `[[nodiscard]]` / `inline` / `__forceinline`. |
| Macros | `NKECS_UNREACHABLE()`, `NKECS_ASSERT(cond)` | Code inatteignable ; assertion. |
| `detail` | `AssertFail(cond, file, line)` | Logge l'échec d'assertion (NkLogger). |
| `detail` | `FNV1a(s)`, `FNV1aBytes(data, len)`, `kFNVBasis`, `kFNVPrime` | Hachage FNV-1a (C-string récursif / bloc mémoire). |

### `NkSpan<T>` — vue contiguë légère (`NkECSDefines.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `data`, `size` (membres **publics**) | Pointeur + nombre d'éléments. |
| Construction | `NkSpan()`, `NkSpan(d, s)`, `NkSpan(arr[N])` | Vide / pointeur+taille / déduit d'un tableau C. |
| Accès | `operator[](i)`, `begin()`, `end()`, `empty()` | Indexé non vérifié ; for-range ; vide ? |

### `NkEntityBuilder` — création fluide d'une entité (`NkWorld.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `explicit NkEntityBuilder(NkWorld&)` | Crée l'entité **immédiatement**. |
| Composants | `With<T>(const T& = T{})`, `With<T>(T&&)` | Applique en direct, chaînable. |
| Finalisation | `Build()` `[O(1)]` | Renvoie l'ID **déjà créé**. |

### `NkBatchBuilder` — création en masse (`NkWorld.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBatchBuilder(NkWorld&, NkEntityId* out, uint32 count)` | Cible et nombre (différé). |
| Composants | `With<T>(const T& = T{})` | Empile le composant, chaînable. |
| Finalisation | `Build()` `[O(count × composants)]` | Crée les N entités **maintenant**. |

### `NkWorld` — façade principale (`NkWorld.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkWorld()`, `~NkWorld()`, move-ctor/assign | Non copiable, **movable**. |
| Création | `CreateEntity()` `[O(1)]`, `Create()`, `CreateBatch(count, out)` | Entité vide / builder fluide / builder en masse. |
| Destruction | `Destroy(id)`, `DestroyDeferred(id)` | Immédiate (hors itération) / différée. |
| État | `IsAlive(id)` `[O(1)]`, `EntityCount()` `[O(1)]` | Vivante ? / nombre d'entités vivantes. |
| Composants | `Add<T>(id, val)`, `Remove<T>(id)` | Ajout/màj (peut migrer) / suppression immédiate. |
| Composants différés | `AddDeferred<T>(id, val)`, `RemoveDeferred<T>(id)` | Empilés pour `FlushDeferred()`. |
| Accès | `Get<T>(id)`, `GetRef<T>(id)`, `Has<T>(id)`, `Set<T>(id, val)` | Pointeur / réf (asserte) / présence / màj si présent. |
| Requêtes | `Query<Ts...>()` | Itération cache-friendly → `NkQueryResult<Ts...>` **[externe]**. |
| Événements | `Subscribe<T>(fn)`, `Unsubscribe<T>(id)`, `Emit<T>(e)`, `QueueEvent<T>(e)` | Abonnement / émission immédiate / mise en file. |
| Drain / flush | `FlushDeferred()`, `DrainEvents()` | Applique ops différées / distribue événements en file. |
| Sous-systèmes | `Graph()`, `EntityIndex()`, `EventBus()`, `static Registry()` | Accès avancé (types **[externe]**). |

### Header parapluie (`NKECS.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config | `NK_ECS_NO_ALIASES` | Supprime les alias `nkentseu::` si défini avant inclusion. |
| Alias | `NkEntityId`, `NkComponentId`, `NkWorld`, `NkComponentMask` **[ext]**, `NkSystem`/`NkScheduler`/… **[ext]** | Réexports de `ecs::` dans `nkentseu`. |
| Fonction | `NkIdOf<T>()` | ID stable d'un type de composant (délègue à `ecs::NkIdOf` **[ext]**). |

---

## Référence complète

Chaque élément est repris ici en détail. Les éléments triviaux sont décrits brièvement ; les
opérations structurantes (création, ajout/migration, requêtes, événements) le sont **à fond**, avec
leurs usages dans les différents domaines du temps réel.

### `NkEntityId` à fond

L'identifiant fort est la pierre angulaire de toute la sécurité du monde. Sa structure `index` + `gen`
sur 8 octets (`static_assert` dans `NKECS.h`) le rend trivialement copiable et `constexpr` :

- **Gameplay / IA** — on stocke un `NkEntityId` comme cible d'un projectile, comme « ennemi
  verrouillé » d'une tourelle, comme propriétaire d'un objet posé. Quand la cible meurt et qu'une
  autre entité recycle sa place, l'ancien ID **cesse de correspondre** (`gen` différente) :
  `IsAlive(id)` renvoie faux, pas de tir sur le mauvais ennemi.
- **IO / réseau** — `Pack()` aplatit l'ID en un `uint64` transmissible ; `Unpack()` le restaure
  côté pair. La génération en poids fort garantit qu'un message en retard référençant une entité
  morte sera détecté.
- **Outils / éditeur** — `<` (ordre via `Pack()`) permet d'utiliser l'ID comme clé d'un conteneur
  ordonné (sélections triées, table d'undo). `==`/`!=` comparent index **et** génération.
- **Persistance** — sauvegarder/recharger un niveau passe par `Pack()`/`Unpack()` : ne **jamais**
  manipuler les bits à la main, `gen` est en poids fort et `index` en poids faible.

`IsValid()` (faux pour `Invalid()` ou un ID jamais initialisé), `Invalid()` (la sentinelle
`{kInvalidIndex, kInvalidGen}`), et l'idiome d'agrégat `NkEntityId{42, 1}` complètent l'outillage.
Toutes ces opérations sont `O(1)` `constexpr noexcept`.

### Constantes et types fondamentaux

`NkComponentId` et `NkArchetypeId` sont de simples `uint32` typés, avec leurs sentinelles
`kInvalidComponentId` / `kInvalidArchetypeId`. Les **constantes de dimensionnement** fixent les
limites dures du moteur et méritent qu'on les connaisse avant de concevoir un jeu ambitieux :

- `kMaxComponentTypes` = **256** types de composants distincts (c'est pourquoi `NkComponentMask`
  fait 32 octets = 256 bits — voir le `static_assert` de `NKECS.h`).
- `kMaxArchetypes` = **4096** combinaisons de composants coexistantes.
- `kMaxEntities` = **1 << 20**, soit ~1,05 million d'entités vivantes simultanées.
- `kChunkSize` = **16 Ko**, la taille d'un bloc mémoire dense (compromis cache/granularité).
- `kMaxSystemsPerGroup` = **128** systèmes par groupe d'ordonnancement.

Ces valeurs guident le découpage des composants : un projet qui frôle 256 types doit factoriser
(par exemple regrouper plusieurs flags dans un seul composant *bitfield*).

### Macros et `detail` — assertions et hachage

`NKECS_NODISCARD` / `NKECS_INLINE` / `NKECS_FORCEINLINE` sont les habillages portables de
`[[nodiscard]]`, `inline` et `__forceinline`. `NKECS_UNREACHABLE()` marque un chemin de code
impossible (`__assume(false)` MSVC, `__builtin_unreachable()` GCC/Clang) — l'optimiseur en profite,
mais c'est un **comportement indéfini** si le chemin est en fait atteint, donc à réserver aux
branches logiquement mortes. `NKECS_ASSERT(cond)` vérifie une condition et, en cas d'échec, appelle
`detail::AssertFail` qui **logge** via NkLogger (l'`std::abort()` est commenté/désactivé) : l'erreur
est signalée, pas fatale.

Le bloc `detail` fournit aussi le **hachage FNV-1a**, brique d'identification de types/chaînes :
`FNV1a(s)` hache une C-string `\0`-terminée de façon **récursive** (un caractère par appel, donc à
réserver aux chaînes courtes connues à la compilation), tandis que `FNV1aBytes(data, len)` hache un
bloc mémoire arbitraire en boucle, `O(len)`. Les deux sont `constexpr` et partent de `kFNVBasis`
avec `kFNVPrime`.

### `NkSpan` à fond

`NkSpan<T>` est le **substitut maison de `std::span`** : une vue **non-possédante** sur une zone
contiguë, deux champs `data` + `size` exposés **publiquement** (attention : `size` est un **membre**,
pas une méthode `size()`). Elle ne possède rien — elle pointe sur une mémoire qui doit lui survivre.
Ses usages traversent toutes les couches :

- **Rendu / GPU** — passer un sous-ensemble d'un tableau de sommets ou d'instances à une fonction
  d'upload sans recopier ni sans figer le type de conteneur source.
- **ECS** — exposer la tranche de composants d'un archétype à un système, qui la parcourt en
  `for (auto& c : span)` (grâce à `begin()`/`end()`).
- **IO / réseau** — décrire un buffer reçu (pointeur + longueur) sans transférer la propriété.
- **Outils** — passer une vue sur une sélection sans copie.

Trois constructeurs `constexpr` : vide, `(pointeur, taille)`, et **déduction depuis un tableau C**
`T arr[N]`. `operator[]` n'effectue **aucune vérification de bornes** (rapide), `empty()` teste
`size == 0`.

### Création d'entités : `CreateEntity`, `Create`, `CreateBatch`

Trois portes d'entrée, trois intentions :

- `CreateEntity()` — l'entité **nue**, `O(1)` amorti (délègue à `mEntityIndex.Allocate()`
  **[externe]**). On l'utilise quand on va remplir les composants soi-même, ou pour une entité
  purement logique.
- `Create()` — le **builder fluide** d'une entité. L'entité est créée dès l'appel ; `With<T>`
  applique chaque composant **immédiatement** (surcharges copie `const T&` et move `T&&`) ; `Build()`
  renvoie l'ID en `O(1)`. Idéal en **gameplay** pour assembler lisiblement un acteur (un projectile :
  `Transform` + `Velocity` + `Damage`).
- `CreateBatch(count, out)` — le **builder en masse**, le seul vraiment différé : `With<T>` empile la
  recette, `Build()` crée les `count` entités et applique tout en `O(count × nbComposants)`. C'est
  l'outil des **systèmes de particules**, des **champs d'astéroïdes**, du **peuplement de foule** ou
  du **streaming de tuiles** où l'on instancie des milliers d'entités identiques d'un coup.

Côté **ownership**, le tableau `out` du batch appartient à l'appelant : il doit contenir au moins
`count` éléments et rester valide jusqu'au `Build()` ; la valeur passée à `With` est **copiée** dans
le lambda interne.

### `Add` et la migration d'archétype

`Add<T>(id, value)` est le **cœur battant** du monde. Son comportement dépend de l'état de l'entité :

- **Cas rapide** — le composant fait déjà partie de l'archétype courant : simple écriture en place,
  `*ptr = value`, **O(1)**.
- **Cas migration** — le composant est nouveau : l'entité doit **changer d'archétype**. Le monde
  récupère ou crée l'archétype cible (`GetOrCreate` / `AddComponent` sur le graphe **[externe]**),
  **recopie la ligne** de données dans le nouvel archétype (`MigrateFrom`), puis **swap-remove** la
  ligne d'origine (l'entité déplacée pour combler le trou voit son record corrigé), et met à jour le
  record. Coût = **O(taille de la ligne migrée)**.

Cette mécanique de migration est exactement ce qui garde chaque archétype **homogène et contigu**,
donc parcourable à plein débit par les systèmes (data-oriented design). En contrepartie, **changer
fréquemment la composition** d'une entité (ajouter/retirer un composant à chaque frame) coûte des
migrations répétées — on préfère alors un composant *flag* qu'on active/désactive en données plutôt
que d'ajouter/retirer le composant lui-même. `Add` existe en surcharge copie et move, et renvoie une
référence au composant stocké (pratique pour le configurer juste après).

`Remove<T>` est l'opération inverse, **immédiate**, qui peut elle aussi réorganiser les archétypes —
d'où l'avertissement : à éviter pendant l'itération d'une `Query`.

### Lire et modifier : `Get`, `GetRef`, `Has`, `Set`

Quatre accès, du plus prudent au plus tranchant :

- `Get<T>(id)` — la lecture **sûre** : renvoie un **pointeur**, `nullptr` si l'entité n'a pas le
  composant (ou pas de record, ou archétype invalide), `O(1)`. Version `const` fournie. C'est la
  forme à privilégier en **gameplay/IA** (« si cette entité a un composant `Stunned`, alors… »).
- `Has<T>(id)` — répond juste **oui/non** en `O(1)` (via `arch->Has(...)`). Pour brancher une logique
  sans avoir besoin de la donnée.
- `GetRef<T>(id)` — renvoie une **référence** directe, mais **asserte** (`NKECS_ASSERT`) si le
  composant est absent — et **comportement indéfini en release**. À réserver aux endroits où la
  présence est **garantie** par construction (par exemple dans un système filtré par `Query`).
- `Set<T>(id, value)` — met à jour la valeur **uniquement si le composant existe**, et est
  **silencieusement ignoré** sinon (le header recommande `Add<T>` quand on veut garantir l'ajout).
  Piège fréquent : croire que `Set` ajoute — non, il faut `Add`.

### `Query` — parcourir le monde

`Query<Ts...>()` est la façade d'itération : elle construit le **masque** des composants requis
(`detail::MakeMask<...>()` **[externe]**) et renvoie un `NkQueryResult<Ts...>` **[externe]** conçu
pour un parcours **cache-friendly archétype par archétype**. L'idiome est le *structured binding* en
range-for :

```cpp
for (auto [t, v] : world.Query<Transform, Velocity>())
    t.pos += v.value * dt;
```

C'est la boucle chaude de presque tous les **systèmes** : intégration physique
(`Transform` + `Velocity`), application de dégâts (`Health` + `DamageOverTime`), rendu (collecte des
entités `Transform` + `Mesh`), IA (entités `AIState` + `Perception`). Règle de sûreté absolue : ne
**modifie pas la structure** (Destroy/Remove/Add immédiats) pendant l'itération — utilise les
versions différées (voir plus bas). Les masques `with`/`without` du résultat sont passés vides par
défaut.

### Opérations différées : `AddDeferred`, `RemoveDeferred`, `DestroyDeferred`, `FlushDeferred`

Le quatuor différé existe pour une seule raison : **survivre à l'itération**. Pendant qu'un système
parcourt une `Query`, toute modification structurelle immédiate risque d'invalider l'itérateur. Les
trois `*Deferred` enregistrent l'intention dans une file interne (`DeferredOp`, un
`enum { Add, Remove, Destroy }` + l'entité + un `NkFunction`), et `FlushDeferred()` les **exécute
toutes** d'un coup — typiquement en **fin de frame**, quand plus aucune query ne tourne. C'est le
patron *command buffer* du monde.

- **Gameplay** — marquer pour destruction les ennemis morts repérés pendant la boucle de combat,
  puis tout nettoyer après.
- **Physique** — ajouter un composant `Collided` aux entités touchées pendant la passe de détection,
  appliqué une fois la passe finie.
- **Outils** — appliquer une vague d'éditions structurelles atomiquement.

`Destroy(id)` **immédiat** reste disponible pour les contextes hors itération (libération directe).

### Événements gameplay : `Subscribe`, `Emit`, `QueueEvent`, `DrainEvents`

Le bus (délégué à `NkGameplayEventBus` **[externe]**) découple les sous-systèmes par
publication/abonnement typés :

- `Subscribe<T>(fn)` — abonne un handler à l'événement `T`, renvoie un `SubscriptionId` à
  **conserver**. `Unsubscribe<T>(id)` retire l'abonnement.
- `Emit<T>(event)` — distribution **synchrone immédiate** (surcharges lvalue et rvalue/move) : le
  handler s'exécute dans la pile courante. À utiliser quand la réaction doit être instantanée et que
  le handler ne touche pas à la structure du monde.
- `QueueEvent<T>(event)` — **met en file** ; rien n'est distribué avant `DrainEvents()`. C'est
  l'équivalent « événement » des opérations différées, sûr vis-à-vis de l'itération.
- `DrainEvents()` — vide la file (délègue à `mEventBus.Drain()` **[externe]**), typiquement en fin de
  frame.

Usages typiques : **UI** (`NkOnButtonClicked`, `NkOnSliderChanged` **[externe]** → réagir à
l'interaction), **gameplay** (« joueur mort », « niveau terminé »), **audio** (« jouer ce son »
émis par un système, consommé par le mixeur).

### Sous-systèmes et registre : `Graph`, `EntityIndex`, `EventBus`, `Registry`

Pour l'**usage avancé / outillage**, le monde expose ses sous-systèmes (tous de types **[externe]**) :
`Graph()` (le `NkArchetypeGraph`), `EntityIndex()` (le `NkEntityIndex`), `EventBus()` (le
`NkGameplayEventBus`). Ils servent à inspecter ou piloter finement le monde — par exemple un
**éditeur** qui veut lister les archétypes existants. `Registry()` est **statique** : elle renvoie le
`NkTypeRegistry::Global()` **[externe]**, le registre global des types de composants (c'est lui qui
sous-tend `NkIdOf<T>`). À manipuler avec précaution : ce sont les rouages internes.

### Cycle de vie et sémantique de valeur

`NkWorld` est **non copiable** (ctor/affectation copie supprimés) mais **movable** (move-ctor et
move-assign `noexcept` par défaut). Concrètement, on **transfère** un monde (par exemple le sortir
d'une fonction de chargement) mais on ne le duplique jamais accidentellement — un monde pèse lourd
(graphe d'archétypes, index d'entités, bus). `IsAlive(id)` et `EntityCount()` (tous deux `O(1)`)
donnent l'état courant. Rappel **threading** : aucune méthode n'est thread-safe ; les accès
concurrents doivent être sérialisés par un mutex externe.

### Le header parapluie et les alias

`#include "NKECS/NKECS.h"` tire tout le module (types, stockage, systèmes, scheduler, monde,
requêtes, événements, réflexion). Il porte les `static_assert` de cohérence (ID 64 bits, masque 32
octets) et, par défaut, **réexporte les noms du cœur `ecs::` dans `nkentseu::`** (`NkEntityId`,
`NkWorld`, etc.) pour un usage plus court. Définir `NK_ECS_NO_ALIASES` **avant** l'inclusion
supprime ce bloc d'alias (utile pour éviter une collision de noms dans un projet hôte). La fonction
libre `nkentseu::NkIdOf<T>()` délègue à `ecs::NkIdOf<T>()` **[externe]** : elle renvoie l'**ID stable**
d'un type de composant, indispensable dès qu'on manipule des masques ou des pools à la main.

> **Piège documenté.** Les exemples en commentaire de `NKECS.h` montrent
> `world.AddComponent<T>(...)`, `world.ForEach<...>(...)`, `scheduler.AddSystem(...)` : c'est une API
> **aspirationnelle/fictive**. Les vraies méthodes sont `NkWorld::Add<T>` (ajout) et
> `NkWorld::Query<...>` (itération) — `AddComponent` et `ForEach` **n'existent pas** sur `NkWorld`.

---

### Exemple récapitulatif

```cpp
#include "NKECS/NKECS.h"
using namespace nkentseu;   // alias activés (pas de NK_ECS_NO_ALIASES)

NkWorld world;

// 1. Création fluide d'une entité (vivante dès Create()).
auto player = world.Create()
    .With<Transform>({ .pos = {0,0,0} })
    .With<Velocity>()
    .With<Health>({ 100 })
    .Build();

// 2. Création en masse, réellement différée jusqu'au Build().
NkEntityId asteroids[500];
world.CreateBatch(500, asteroids)
    .With<Transform>()
    .With<Velocity>({ .x = 1.f })
    .Build();

// 3. Boucle de jeu : un système d'intégration via Query.
for (auto [t, v] : world.Query<Transform, Velocity>())
    t.pos += v.value * dt;

// 4. Lecture sûre + destruction différée pendant l'itération.
for (auto [hp] : world.Query<Health>())
    if (hp.value <= 0)
        world.DestroyDeferred(/* entité courante */);

// 5. Événements : immédiat vs file.
auto sub = world.Subscribe<NkOnButtonClicked>([](const auto& e){ /* … */ });
world.Emit(NkOnButtonClicked{});       // synchrone
world.QueueEvent(NkOnSliderChanged{}); // différé

// 6. Fin de frame : appliquer les opérations différées.
world.FlushDeferred();   // destructions/ajouts/retraits en attente
world.DrainEvents();     // événements mis en file

// 7. Sérialisation d'un ID (réseau / sauvegarde).
uint64 wire = player.Pack();
NkEntityId back = NkEntityId::Unpack(wire);
```

---

[← Index NKECS](README.md) · [Récap NKECS](../NKECS.md) · [Couche Runtime](../README.md)
