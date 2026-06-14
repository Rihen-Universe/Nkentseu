# Les systèmes ECS de Noge

> Couche **Engine** · Noge · Les quatre briques qui font « tourner » une scène : le système de
> **rendu** `NkRenderSystem`, la **propagation hiérarchique** des transforms `NkTransformSystem`,
> la **sérialisation de scène** `NkSceneSerializer` (format `.nkscene`) et la **réflexion des
> composants** `NkReflectComponents` pour l'inspecteur de l'éditeur.

Un monde ECS, à lui seul, ne fait rien : ce ne sont que des entités et des composants rangés en
mémoire. Ce qui leur donne vie, ce sont les **systèmes** — du code qui parcourt les composants et
agit. Noge livre deux systèmes prêts à brancher (rendu et transforms) plus deux outils de
*plomberie* qui ne sont pas des systèmes ECS au sens strict mais des services adressés au même
monde : transformer une scène en JSON et exposer les champs des composants à un panneau
d'inspection. Cette page explique **quand** chacun intervient dans la frame et **pourquoi** il est
écrit comme il l'est.

Les deux systèmes héritent de `ecs::NkSystem` et se décrivent au scheduler via un `NkSystemDesc`
(quels composants ils lisent, lesquels ils écrivent, dans quel groupe ils tournent). Les deux
services de sérialisation/réflexion s'appuient sur des **registres singletons indexés par nom de
type**, alimentés au démarrage par des macros d'auto-enregistrement. Rien ne passe par `new` :
toute la mémoire vient de NKMemory (`NkVector`, `NkString`).

- **Namespaces** : `nkentseu` (NkRenderSystem, NkTransformSystem) et `nkentseu::ecs`
  (NkSceneSerializer, NkReflectComponents) ; les deux systèmes font `using namespace math;`
- **Headers** : `NkRenderSystem.h`, `NkTransformSystem.h`, `NkSceneSerializer.h`,
  `NkReflectComponents.h` (sous `Engine/Noge/src/Noge/ECS/Systems/`)

---

## Le pont vers le renderer : `NkRenderSystem`

`NkRenderSystem` est le **trait d'union entre l'ECS et NKRenderer**. À chaque frame, il parcourt le
monde, en extrait ce qui doit être dessiné, et le pousse dans le pipeline 3D. Il ne *décide* rien
sur le rendu lui-même (les ombres, le PBR, le bloom restent l'affaire de NKRenderer) : son rôle est
de **traduire** des composants ECS en appels de soumission. C'est un `final` qui dérive de
`ecs::NkSystem`, toutes ses méthodes sont `noexcept`, et il s'ordonnance dans le groupe
`NkSystemGroup::Render`.

Son cycle de frame est documenté en quatre temps. D'abord **UpdateActiveCamera** : il cherche la
caméra de priorité maximale et en tire les matrices `view`/`proj`/`viewProj`. Ensuite
**CollectLights** : il *query* tous les `NkLightComponent`, les convertit et les range dans le
`NkSceneContext3D`. Puis **SubmitMeshes** : il *query* les entités qui ont à la fois Mesh, Material
et Transform, résout les *handles* GPU via le `NkResourceManager`, applique un **frustum culling**
(`viewProj × AABB`) et appelle `NkRender3D::Submit`. Enfin il clôt la scène par
`NkRender3D::EndScene(cmd)`.

Avant de lancer le scheduler, on l'amorce avec ses dépendances :

```cpp
NkRenderSystem render;
render.Init(&renderer, &commandBuffer);   // à appeler AVANT NkScheduler::Init
render.SetAmbientIntensity(0.2f);
```

Ce n'est **pas** un renderer : il ne possède ni le device, ni les passes, ni les shaders. Et ce
n'est **pas** un système autonome — si on oublie `Init`, `mRenderer`/`mCmd` restent nuls. Quelques
réglages sont protégés par une garde anti-null (`SetWireframe`, `GetStats` retournent sans rien
casser si le renderer manque), mais `SetCommandBuffer` ne l'est pas : ne lui passez pas n'importe
quoi.

Un point d'ordonnancement à connaître : son `Describe()` annonce `Writes<NkCameraComponent>` (la
caméra active est *écrite*) et lecture seule pour Transform / Mesh / Material / Light. C'est ce
contrat de lecture/écriture qui permet au scheduler de paralléliser sans risque de collision.

> **En résumé.** `NkRenderSystem` traduit l'ECS vers NKRenderer chaque frame : caméra active →
> lumières → maillages soumis (culling) → fin de scène. Appelez `Init(renderer, cmd)` avant le
> scheduler. Il *écrit* la caméra, *lit* le reste. Ce n'est pas le renderer, juste le pont.

---

## La hiérarchie des transforms : `NkTransformSystem`

Dès qu'une scène a une **hiérarchie** — une main attachée à un bras, une roue à un châssis, une
arme à un personnage —, la position monde d'un objet dépend de celle de son parent. `NkTransformSystem`
calcule ces **matrices monde** en propageant les transforms locales du parent vers l'enfant. C'est
un `final` dérivé de `ecs::NkSystem`, ordonnancé en `PreUpdate` avec une **priorité explicite de
1000** — c'est-à-dire **le tout premier** système de la frame, pour que tout le monde travaille
ensuite sur des matrices à jour.

Le détail qui compte, c'est **comment** il parcourt l'arbre. Plutôt qu'une récursion (qui ferait
exploser la pile sur une hiérarchie profonde), il fait un **DFS itératif** avec sa propre pile
`mStack`. Il collecte les racines (un `NkTransform` sans `NkParent` valide), puis, pour chaque nœud
dépilé, calcule `worldMatrix = parent.worldMatrix * ComputeLocalMatrix()`, en extrait
`worldPosition` (la 4ᵉ colonne), remet `worldDirty` à faux, et empile les enfants (`NkChildren`) en
leur propageant le drapeau *dirty* d'un ancêtre.

Ce n'est **pas** une simple boucle plate : l'ordre parent-avant-enfant est garanti par le DFS, et
ce n'est **pas** une récursion (zéro risque de *stack overflow*). C'est aussi conçu pour ne
**rien allouer** par frame : `mStack` est réutilisé d'une frame à l'autre (`Clear()` seulement),
et `NkMat4f` est aligné 16 octets pour le SIMD.

```cpp
NkTransformSystem transforms;   // se déclare lui-même en PreUpdate priorité 1000
// ... ajouté au scheduler, il tourne avant tout le reste de PreUpdate.
```

Deux pièges. D'abord, à cause de cette priorité `1000.f`, **ne placez pas un autre système à
priorité ≥ 1000 dans PreUpdate** si ce système dépend de matrices monde déjà calculées — il
tournerait avant. Ensuite, par cohérence il faut le noter : son `Describe()` n'est **pas** marqué
`noexcept` (contrairement aux autres méthodes du système).

> **En résumé.** `NkTransformSystem` propage les matrices monde dans la hiérarchie par un **DFS
> itératif** (pas de récursion, pas de stack overflow), en PreUpdate priorité **1000** (premier
> absolu), avec **zéro allocation** par frame. Il *lit* `NkParent`/`NkChildren`, *écrit*
> `NkTransform`. Rien ne doit s'intercaler avant lui s'il dépend du monde calculé.

---

## Sauver et charger une scène : `NkSceneSerializer`

Une scène doit pouvoir être **écrite sur disque** puis **relue** à l'identique : c'est ce que fait
`NkSceneSerializer`, qui transforme une `NkSceneGraph` en **JSON** (format `.nkscene`, version 1) et
inversement. Le fichier a une forme simple : un objet racine `version` / `name` / `entities[]`, et
chaque entité est `{ id, components: { "NkXxx": {...} } }`.

La vraie question est : *comment sérialiser un composant dont le sérialiseur ne connaît pas le
type ?* La réponse de Noge tient en deux voies. Si le composant implémente `NkISerializable`, il
fournit son propre `Serialize`/`Deserialize`. Sinon, on retombe sur la **réflexion** (`NkReflect`).
Et pour savoir *quels* types existent, le sérialiseur consulte un **registre** : tout composant
non enregistré est purement et simplement **ignoré** à la sérialisation.

Ce registre se peuple via une macro d'auto-enregistrement, à poser dans le `.cpp` du composant :

```cpp
// dans NkTransform.cpp, par exemple
NK_SERIALIZE_COMPONENT(NkTransform,
    [](const void* c, NkArchive& out) -> bool { /* ... */ return true; },
    [](void* c, const NkArchive& in)  -> bool { /* ... */ return true; },
    [](NkWorld& w, NkEntityId id)     { w.Add<NkTransform>(id); });
```

Les trois callbacks sont typiquement des **lambdas sans capture** (convertibles en pointeurs de
fonction). La macro déclare une struct statique dont le constructeur enregistre le tout au
démarrage du programme.

Ce n'est **pas** un format binaire : c'est du JSON lisible, pour des fichiers `.nkscene` versionnés
et diffables. Et le `Load` n'est **pas** destructif : il est **additif** — les entités déjà
présentes dans la scène sont préservées. Pour un chargement propre, videz d'abord (le commentaire
du header suggère `scene.World().FlushDeferred()` puis de recréer une scène vide), sinon vous
risquez des **doublons d'entités**.

À côté du disque, deux variantes travaillent sur une **archive mémoire** :
`SaveToArchive` / `LoadFromArchive`, pensées pour le **réseau** ou un système **undo/redo**.

> **En résumé.** `NkSceneSerializer` (dé)sérialise une `NkSceneGraph` en JSON `.nkscene` (v1).
> Chaque composant est sérialisé via `NkISerializable` ou, à défaut, par réflexion ; les types se
> déclarent avec `NK_SERIALIZE_COMPONENT` (lambdas sans capture). `Load` est **additif** (videz
> avant pour éviter les doublons). Variantes `…ToArchive`/`…FromArchive` pour réseau & undo/redo.

---

## Décrire les composants à l'éditeur : `NkReflectComponents`

Pour qu'un **InspectorPanel** affiche et édite les champs d'un composant (un curseur pour
l'échelle, un champ texte pour un nom, un sélecteur d'asset…), il lui faut des **métadonnées** :
quel champ, à quel offset, de quel type, avec quelle plage. C'est exactement ce que fournit
`NkReflectComponents` — une réflexion **minimaliste et orientée éditeur**, entièrement *inline*
dans le header (le registre et son alimentation ne demandent aucun `.cpp`).

Le mécanisme se déclare par un bloc de macros, à poser dans un `.cpp` :

```cpp
NK_COMPONENT_BEGIN(NkTransform, "Transform")
  NK_FIELD_VEC3(position, "Position")
  NK_FIELD_QUAT(rotation, "Rotation")
  NK_FIELD_RANGE(scale, "Scale", Vec3f, 0.001f, 100.f, 0.01f)
NK_COMPONENT_END(NkTransform)
```

`NK_COMPONENT_BEGIN` ouvre une struct d'auto-enregistrement qui fabrique un `NkComponentMeta`
(nom de type, nom affiché, taille, et un `addFn` qui fait `world.Add<Type>(id)`). Chaque `NK_FIELD…`
pousse un `NkFieldMeta` calculé par `offsetof`/`sizeof`. `NK_COMPONENT_END` enregistre le tout dans
le singleton `NkComponentMetaRegistry`. L'éditeur peut alors itérer sur `All()` pour bâtir un menu
« Add Component », ou `Find("NkTransform")` pour dresser un inspecteur champ par champ.

Ce n'est **pas** la réflexion de sérialisation : `NkComponentMetaRegistry` et le registre de
`NkSceneSerializer` sont deux mécanismes **distincts** (l'un pour l'éditeur, l'autre pour le
disque), même si leur `AddFn` partage la signature `void(*)(NkWorld&, NkEntityId)`. Et le typage de
champ est **fermé** : un champ est l'une des valeurs de l'enum `NkFieldType` (booléen, entiers,
flottants, vecteurs, quaternion, matrice, chaînes, enum, chemin d'asset, couleur) — pas un type
arbitraire.

Un piège de documentation à signaler : l'exemple en **tête de fichier** montre une forme
`NK_FIELD_VEC3(scale, "Scale", 0.001f, 100.f)` à quatre arguments **qui n'existe pas**. Seul
`NK_FIELD_VEC3(field, display)` (deux arguments) est défini ; la version avec plage est
`NK_FIELD_RANGE(scale, "Scale", Vec3f, 0.001f, 100.f, 0.01f)`. Suivez la forme `NK_FIELD_RANGE`.

> **En résumé.** `NkReflectComponents` décrit les champs des composants pour l'inspecteur de
> l'éditeur, *inline* dans le header. On déclare avec `NK_COMPONENT_BEGIN`/`NK_FIELD…`/
> `NK_COMPONENT_END` ; le type de champ est l'enum **fermé** `NkFieldType`. Mécanisme distinct de la
> sérialisation. Attention : la forme `NK_FIELD_VEC3` à 4 arguments de l'en-tête n'existe pas.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par brique. Le détail (sémantique, cas d'usage, statut) suit
dans la « Référence complète ».

### `NkRenderSystem` (namespace `nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkRenderSystem()`, `~NkRenderSystem()` | Constructeur / destructeur par défaut, `noexcept`. |
| Amorçage | `Init(renderer, cmd)` | Stocke renderer + command buffer ; à appeler **avant** le scheduler. |
| Contrat ECS | `Describe()` | Construit le `NkSystemDesc` (lit Transform/Mesh/Material/Light, écrit Camera, groupe Render). |
| Exécution | `Execute(world, dt)` | Frame complète (caméra → lumières → maillages → fin de scène). |
| Réglages | `SetCommandBuffer(cmd)`, `SetAmbientIntensity(v)`, `SetEnvMap(handle)`, `SetWireframe(v)` | Command buffer / intensité ambiante / env map / fil de fer (garde anti-null). |
| Lecture | `GetStats()` | Statistiques du renderer (ou stats vides si renderer null). |

### `NkTransformSystem` (namespace `nkentseu`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkTransformSystem()`, `~NkTransformSystem()` | Constructeur / destructeur par défaut, `noexcept`. |
| Contrat ECS | `Describe()` | `NkSystemDesc` : lit `NkParent`/`NkChildren`, écrit `NkTransform`, PreUpdate priorité 1000. |
| Exécution | `Execute(world, dt)` | Propagation des matrices monde par DFS itératif. |

### `NkSceneSerializer` (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interface composant | `struct NkComponentSerializer` | POD de pointeurs de fonction (`serialize`/`deserialize`/`addDefault` + `typeName`). |
| Typedefs | `SerializeFn`, `DeserializeFn`, `AddFn` | Signatures des callbacks de (dé)sérialisation et de création par défaut. |
| Enregistrement | `RegisterComponentSerializer(cs)` · template `RegisterComponentSerializer<T>(name, sfn, dfn, addFn)` | Ajout direct / helper *inline* (utilisé par la macro). |
| Disque | `Save(scene, path)`, `Load(scene, path)` | Écriture/lecture JSON `.nkscene` (`Load` **additif**). |
| Mémoire | `SaveToArchive(scene, out)`, `LoadFromArchive(scene, in)` | Vers/depuis une archive (réseau, undo/redo). |
| Macro | `NK_SERIALIZE_COMPONENT(Type, Ser, Deser, Add)` | Auto-enregistrement d'un composant (dans son `.cpp`). |
| Interne | `struct Registry` (kMax = 128, `Get`/`Find`) | Singleton Meyers, recherche linéaire `NkStrEqual`. |

### `NkReflectComponents` (namespace `nkentseu::ecs`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type de champ | `enum class NkFieldType : nk_uint8` | Jeu **fermé** de types éditables (cf. référence). |
| Métadonnée champ | `struct NkFieldMeta` | Nom, tooltip, type, offset, size, plage, longueur, enum, flags. |
| Métadonnée composant | `struct NkComponentMeta` | typeName, displayName, taille, `fields`, `addFn`. |
| Registre | `class NkComponentMetaRegistry` (`Get`/`Register`/`Find`/`All`) | Singleton Meyers ; `Register` **remplace** un homonyme. |
| Macros | `NK_COMPONENT_BEGIN/END`, `NK_FIELD`, `NK_FIELD_RANGE`, `NK_FIELD_VEC3`, `NK_FIELD_QUAT`, `NK_FIELD_STR_FIXED`, `NK_FIELD_BOOL`, `NK_FIELD_ASSET` | Déclaration des composants et de leurs champs. |

---

## Référence complète

Chaque élément est repris en détail, avec ses usages dans les différents domaines (gameplay/IA,
rendu, animation, physique, audio, UI/éditeur, IO, scène) et son **statut** d'implémentation.

### `NkRenderSystem` à fond

**Statut : implémenté.** L'essentiel du corps réside dans le `.cpp` — `Execute`,
`UpdateActiveCamera`, `CollectLights`, `SubmitMeshes`, `IsVisible` y sont définis ; le reste
(`Init`, `Describe`, les setters, `GetStats`) est *inline* dans le header.

**Amorçage et contrat.** `Init(renderer, cmd)` mémorise le `renderer::NkRenderer*` et le
`NkICommandBuffer*` ; il faut l'appeler **avant** `NkScheduler::Init`, faute de quoi le système
tournerait avec des pointeurs nuls. `Describe()` produit le descripteur via un builder fluide :
`.Reads<NkTransform>().Reads<NkMeshComponent>().Reads<NkMaterialComponent>().Reads<NkLightComponent>().Writes<NkCameraComponent>().InGroup(NkSystemGroup::Render).Sequential().Named("NkRenderSystem")`.
C'est ce contrat qui informe le scheduler : tout est en lecture sauf la caméra active, qui est
écrite.

**La frame, étape par étape.** `Execute` enchaîne :

- **UpdateActiveCamera** — sélectionne la caméra de priorité maximale et calcule view / proj /
  viewProj ; mémorise l'entité active (`mActiveCameraId`) et `mViewProjMatrix`.
- **CollectLights** — *query* les `NkLightComponent`, les pousse dans `mSceneCtx.lights` (un
  `NkSceneContext3D`) en convertissant leur type via `ConvertLightType`.
- **SubmitMeshes** — *query* les entités Mesh + Material + Transform, résout les handles GPU via le
  `NkResourceManager`, applique le **frustum culling** (`viewProj × AABB`, voir `IsVisible`) et
  soumet chaque maillage visible à `NkRender3D::Submit`.
- **EndScene** — `NkRender3D::EndScene(cmd)` clôt la passe.

**Conversion des types de lumière.** `ConvertLightType` est un `switch` complet, *inline*, qui
traduit l'enum ECS vers l'enum du renderer — deux scopes **distincts** à respecter dans le code :
les valeurs ECS sont **sans** préfixe `NK_`, celles du renderer **avec**.

- `ecs::NkLightType::Directional` → `renderer::NkLightType::NK_DIRECTIONAL`
- `ecs::NkLightType::Point` → `renderer::NkLightType::NK_POINT`
- `ecs::NkLightType::Spot` → `renderer::NkLightType::NK_SPOT`
- `ecs::NkLightType::Area` → `renderer::NkLightType::NK_AREA`
- `ecs::NkLightType::Ambient` → `renderer::NkLightType::NK_AMBIENT`
- `default` → `renderer::NkLightType::NK_DIRECTIONAL`

**Réglages de rendu** (tous *inline*) :

- `SetCommandBuffer(cmd)` — change le `NkICommandBuffer*` ciblé. **Pas de garde null** : ne lui
  passez pas un pointeur invalide.
- `SetAmbientIntensity(v)` — fixe `mAmbientIntensity` (défaut `0.2f`), l'éclairage ambiant
  uniforme de la scène.
- `SetEnvMap(handle)` — fixe `mEnvMapHandle` (défaut `0`), la *cubemap* d'environnement (IBL /
  réflexions).
- `SetWireframe(v)` — bascule le rendu fil de fer via `mRenderer->Renderer3D().SetWireframe(v)`,
  protégé par une garde si `mRenderer` est nul. Utile pour le débogage géométrique en éditeur.
- `GetStats()` — renvoie les `renderer::NkRendererStats` (draw calls, triangles…), ou `mEmptyStats`
  si le renderer est nul. Brique typique d'un overlay de stats ou d'un panneau de profilage.

**Membres** notables : `mRenderer`/`mCmd` (les deux dépendances injectées), `mEnvMapHandle`,
`mAmbientIntensity`, le `mSceneCtx` (contexte de scène 3D), `mOpaqueCalls`
(`NkVector<NkDrawCall3D>`), `mActiveCameraId`, `mViewProjMatrix` (`NkMat4f::Identity()` au départ) et
`mEmptyStats`.

Cas d'usage par domaine :

- **Rendu / scène** — c'est sa raison d'être : transformer le monde ECS en soumissions GPU chaque
  frame, avec culling au passage. C'est la couche qui fait *apparaître* une scène ECS à l'écran.
- **Caméra / gameplay** — il choisit la caméra active par priorité : passer d'une vue cinématique à
  une vue joueur revient à ajuster la priorité d'une `NkCameraComponent` (le système écrit la caméra
  retenue).
- **Éclairage** — `CollectLights` + `ConvertLightType` montent la liste de lumières du frame ;
  `SetAmbientIntensity` et `SetEnvMap` règlent l'ambiance globale (IBL).
- **UI / éditeur** — `SetWireframe` pour un mode debug, `GetStats` pour un overlay de performance.

Pièges : appeler `Init` **avant** le scheduler ; `mRenderer`/`mCmd` peuvent être nuls (gardes dans
`SetWireframe`/`GetStats`, **pas** dans `SetCommandBuffer`) ; le `Describe()` déclare
`Writes<NkCameraComponent>`, ce qui pèse sur l'ordonnancement parallèle.

### `NkTransformSystem` à fond

**Statut : implémenté** (`Execute` et `ProcessEntity` dans le `.cpp`).

**Contrat.** `Describe()` (inline) construit :
`.Reads<NkParent>().Reads<NkChildren>().Writes<NkTransform>().InGroup(NkSystemGroup::PreUpdate).WithPriority(1000.f).Named("NkTransformSystem")`.
La priorité `1000.f` en fait le **premier** système de PreUpdate.

**L'algorithme.** `Execute` collecte les **racines** (un `NkTransform` sans `NkParent` valide), puis
descend l'arbre par un **DFS itératif** alimenté par `mStack`. Chaque nœud (`ProcessEntity`) calcule
`worldMatrix = parentWorld * ComputeLocalMatrix()`, en extrait `worldPosition` (la colonne 3 de la
matrice), remet `worldDirty` à faux, et empile ses enfants (`NkChildren`) en propageant le drapeau
*dirty* : si un ancêtre est *dirty*, tout son sous-arbre l'est. La pile interne
(`struct StackEntry { NkEntityId entity; NkMat4f parentWorld = Identity(); bool parentDirty = false; }`)
est rangée dans `mStack`, un `NkVector<StackEntry>` **réutilisé** d'une frame à l'autre — on
n'appelle que `Clear()`, jamais de réallocation. Le tout est SoA-friendly et SIMD (`NkMat4f` aligné
16 octets).

Cas d'usage par domaine :

- **Animation** — un squelette est une hiérarchie d'os : la pose monde de chaque os dépend de son
  parent ; c'est exactement cette propagation. Attacher une arme à la main d'un personnage suit le
  même mécanisme parent/enfant.
- **Scène / éditeur** — déplacer un groupe (sélectionner un parent et le bouger) entraîne tous les
  enfants ; le *dirty flag* évite de recalculer les sous-arbres immobiles.
- **Rendu** — les `worldMatrix` produites ici sont ce que `NkRenderSystem` lit ensuite (Transform en
  lecture) pour positionner les maillages ; d'où la priorité maximale.
- **Gameplay / physique** — toute logique qui interroge la position monde d'une entité enfant (tir,
  raycast depuis un sous-objet) compte sur des matrices déjà propagées.

Pièges : la priorité `1000.f` impose qu'aucun autre système PreUpdate dépendant des matrices monde
ne soit à priorité ≥ 1000 (il tournerait avant) ; et `Describe()` n'est **pas** `noexcept` ici,
contrairement aux autres méthodes.

### `NkSceneSerializer` à fond

**Statut : déclaré, corps dans le `.cpp`.** `Save` / `Load` / `SaveToArchive` / `LoadFromArchive` /
`SerializeEntity` / `DeserializeEntity` sont déclarés non-inline ; seul le `struct Registry` est
entièrement *inline* dans le header.

**`struct NkComponentSerializer` — l'interface d'un composant.** C'est un POD de pointeurs de
fonction décrivant *comment* (dé)sérialiser un type :

- typedefs : `SerializeFn = bool(*)(const void* comp, NkArchive& out)`,
  `DeserializeFn = bool(*)(void* comp, const NkArchive& in)`,
  `AddFn = void(*)(NkWorld& world, NkEntityId id)`.
- membres : `typeName` (nullptr par défaut), `serialize`, `deserialize`, `addDefault` (qui crée le
  composant par défaut, pour la désérialisation).
- sémantique : un composant **non enregistré** est **ignoré** à la sérialisation.

**`class NkSceneSerializer`.** Le constructeur est `= default` (pas `noexcept`). L'enregistrement
passe par `RegisterComponentSerializer(const NkComponentSerializer&)` (déclaration, corps `.cpp`) ou
par le helper *inline* templaté `RegisterComponentSerializer<T>(name, sfn, dfn, addFn)` — ce dernier
remplit un `NkComponentSerializer` et délègue à la surcharge non-template ; c'est lui qu'appelle la
macro `NK_SERIALIZE_COMPONENT`.

Les quatre points d'entrée d'I/O (corps `.cpp`) :

- `Save(scene, path)` — écrit la scène en JSON `.nkscene`.
- `Load(scene, path)` — reconstruit la scène ; **les entités existantes sont préservées** (chargement
  **additif**). Pour repartir propre, le header conseille `scene.World().FlushDeferred()` puis de
  recréer une scène vide.
- `SaveToArchive(scene, out)` / `LoadFromArchive(scene, in)` — mêmes opérations vers/depuis une
  archive mémoire (réseau, undo/redo).

Les deux privés `SerializeEntity` / `DeserializeEntity` traitent une entité isolée (son objet
`{ id, components }`).

**`struct Registry`** (singleton inline) : `kMax = 128` entrées dans un tableau `entries[kMax]`, un
compteur `count`, un Meyers singleton `Get()` (`static Registry r`) et un `Find(name)` à recherche
**linéaire** O(count) via `NkStrEqual` (nullptr si absent).

**Macro `NK_SERIALIZE_COMPONENT(Type, Ser, Deser, Add)`.** À placer dans le `.cpp` du composant.
Elle définit une struct statique d'auto-enregistrement `_NkSerializeAutoReg_##Type` dont le ctor
`noexcept` appelle `RegisterComponentSerializer<Type>(#Type, Ser, Deser, Add)`, puis instancie un
global `_nk_serialize_autoreg_##Type`. Les arguments sont typiquement des **lambdas sans capture**
(convertibles en pointeurs de fonction).

Cas d'usage par domaine :

- **Scène / IO** — c'est son cœur : sérialiser un niveau dans un `.nkscene` versionné, lisible et
  diffable (le `version` permet de migrer les formats).
- **UI / éditeur** — « Save Scene » / « Open Scene » de l'éditeur appellent `Save`/`Load` ;
  l'éditeur doit penser à vider la scène avant un `Load` (sinon doublons).
- **Réseau** — `SaveToArchive`/`LoadFromArchive` sérialisent une scène ou un sous-état vers un
  buffer mémoire pour réplication.
- **Gameplay** — undo/redo : capturer l'état en archive, puis le restaurer.

Pièges : registre **borné à 128** (`kMax`) — la gestion d'un débordement éventuel est dans le `.cpp`
de `RegisterComponentSerializer`, pas visible au header ; `Find` est O(n) ; `Load` est **additif**
(risque de doublons si la scène n'est pas vidée).

### `NkReflectComponents` à fond

**Statut : entièrement inline / implémenté dans le header** (registre + enregistrement, aucun `.cpp`
requis pour la mécanique). Seules les **déclarations** `NK_COMPONENT_BEGIN/END` des composants
standard restent en **SPEC** (commentées en fin de header), à placer dans un
`NkCoreComponents.reflect.cpp`.

**`enum class NkFieldType : nk_uint8`** — le jeu **fermé** de types qu'un champ peut prendre. Scope
`nkentseu::ecs::NkFieldType::VALEUR`. Valeurs : `Unknown = 0`, `Bool`, `Int8`, `Int16`, `Int32`,
`Int64`, `UInt8`, `UInt16`, `UInt32`, `UInt64`, `Float32`, `Float64`, `Vec2f`, `Vec3f`, `Vec4f`,
`Quatf`, `Mat4f`, `String` (NkString), `StringFixed` (char[N]), `Enum` (entier + liste de noms),
`AssetPath` (NkString + browser d'asset), `Color` (NkVec4f interprété RGBA dans [0, 1]). C'est ce
type qui dit à l'inspecteur quel **widget** afficher (case à cocher, curseur, champ texte, sélecteur
de couleur, browser d'asset…).

**`struct NkFieldMeta`** — la description d'un champ : `name` (affiché), `tooltip` (optionnel),
`type`, `offset` (offsetof) et `size` (sizeof) ; `rangeMin`/`rangeMax`/`step` pour la plage d'un
éditeur numérique ; `strMaxLen` pour `StringFixed` ; `enumNames`/`enumCount` pour `Enum` ; et les
flags `readOnly` / `hidden`.

**`struct NkComponentMeta`** — la description d'un composant : `typeName`, `displayName`,
`sizeBytes`, le `NkVector<NkFieldMeta> fields`, et un `AddFn addFn = void(*)(NkWorld&, NkEntityId)`
qui est le callback du bouton « Add Component ».

**`class NkComponentMetaRegistry`** — singleton tout *inline* : `Get()` (Meyers),
`Register(meta)` qui **remplace** un meta de même `typeName` (comparaison `NkStrEqual`) s'il existe
sinon `PushBack` (recherche linéaire O(n)), `Find(typeName)` (linéaire, nullptr si absent) et
`All()` qui renvoie tous les metas (pour itérer le menu « Add Component »). Membre privé :
`NkVector<NkComponentMeta> mMetas` — **pas de borne fixe** (NkVector dynamique), contrairement au
registre à 128 de `NkSceneSerializer`.

**Les macros de déclaration** (à enchaîner dans un `.cpp`) :

- `NK_COMPONENT_BEGIN(Type, DisplayName)` — ouvre une struct d'auto-enregistrement
  `_NkMetaAutoReg_##Type` ; son ctor `noexcept` crée un `NkComponentMeta _meta` avec
  `typeName = #Type`, `displayName = DisplayName`, `sizeBytes = sizeof(Type)` et
  `addFn = [](NkWorld& w, NkEntityId id){ w.Add<Type>(id); }`. **Ouvre un bloc** à fermer.
- `NK_FIELD(field, display, ftype)` — pousse un `NkFieldMeta` : `name = display`,
  `type = NkFieldType::ftype`, `offset = offsetof(Type, field)`,
  `size = sizeof(((Type*)0)->field)`.
- `NK_FIELD_RANGE(field, display, ftype, mn, mx, st)` — comme `NK_FIELD` plus
  `rangeMin`/`rangeMax`/`step` (cast `(float)`).
- `NK_FIELD_VEC3(field, display)` → `NK_FIELD(field, display, Vec3f)`.
- `NK_FIELD_QUAT(field, display)` → `NK_FIELD(field, display, Quatf)`.
- `NK_FIELD_STR_FIXED(field, display, maxLen)` — type `StringFixed`, fixe `strMaxLen = maxLen`.
- `NK_FIELD_BOOL(field, display)` → `NK_FIELD(field, display, Bool)`.
- `NK_FIELD_ASSET(field, display)` → `NK_FIELD(field, display, AssetPath)`.
- `NK_COMPONENT_END(Type)` — appelle `NkComponentMetaRegistry::Get().Register(_meta)`, ferme la
  struct et déclare l'instance globale `_nk_meta_autoreg_##Type`.

Cas d'usage par domaine :

- **UI / éditeur** — sa raison d'être : monter dynamiquement un InspectorPanel (champs typés,
  plages, tooltips, lecture seule, masquage) et un menu « Add Component » via `All()` + `addFn`.
- **Scène** — combiné au sérialiseur, il donne une chaîne complète « éditer puis sauver » d'un
  composant standard-layout.
- **Animation / rendu** — exposer proprement les champs d'un composant matériel, lumière ou
  transform (curseurs de plage avec `NK_FIELD_RANGE`, couleurs via `Color`, assets via
  `NK_FIELD_ASSET`).

Pièges : l'exemple de **tête de fichier** montre `NK_FIELD_VEC3(scale, "Scale", 0.001f, 100.f)` à
quatre arguments — **cette forme n'existe pas** ; seul `NK_FIELD_VEC3(field, display)` (2 arguments)
est défini, et la version avec plage est `NK_FIELD_RANGE(scale, "Scale", Vec3f, 0.001f, 100.f,
0.01f)` (incohérence doc en-tête vs macros réelles). `offsetof(Type, field)` exige un type
standard-layout ; `sizeof(((Type*)0)->field)` est l'idiome de taille de membre. Et le registre n'a
**pas** de borne fixe (NkVector), contrairement aux 128 entrées de `NkSceneSerializer::Registry`.

### Notes transverses

- Les deux registres (`NkSceneSerializer::Registry` et `NkComponentMetaRegistry`) sont des
  **singletons Meyers** indexés par nom de type (`NkStrEqual`), à recherche **linéaire**.
- `NkComponentSerializer::AddFn` et `NkComponentMeta::AddFn` partagent la **même signature**
  `void(*)(NkWorld&, NkEntityId)` mais sont **deux mécanismes distincts** : sérialisation disque
  d'un côté, réflexion éditeur de l'autre.
- Les types `nk_uint8/uint32/uint64/usize`, `float32`, `NkVector`, `NkString`, `NkStrEqual`,
  `NkMat4f`, `NkArchive`, `NkWorld`, `NkEntityId`, `NkSceneGraph`,
  `NkSystem`/`NkSystemDesc`/`NkSystemGroup` proviennent des modules NKCore / NKContainers / NKMath /
  NKSerialization / NKECS inclus en amont.

---

### Exemple

```cpp
#include "Noge/ECS/Systems/NkRenderSystem.h"
#include "Noge/ECS/Systems/NkTransformSystem.h"
#include "Noge/ECS/Systems/NkSceneSerializer.h"
#include "Noge/ECS/Systems/NkReflectComponents.h"
using namespace nkentseu;
using namespace nkentseu::ecs;

// 1) Brancher les deux systèmes sur le scheduler.
NkTransformSystem transforms;          // PreUpdate priorité 1000 (premier absolu)
NkRenderSystem    render;
render.Init(&renderer, &commandBuffer); // AVANT le scheduler
render.SetAmbientIntensity(0.2f);
render.SetEnvMap(envCubemapHandle);
// ... scheduler.Add(transforms); scheduler.Add(render); scheduler.Init();

// 2) Déclarer un composant à l'éditeur (dans un .cpp).
NK_COMPONENT_BEGIN(NkTransform, "Transform")
  NK_FIELD_VEC3(position, "Position")
  NK_FIELD_QUAT(rotation, "Rotation")
  NK_FIELD_RANGE(scale, "Scale", Vec3f, 0.001f, 100.f, 0.01f)
NK_COMPONENT_END(NkTransform)

// 3) Le rendre sérialisable (dans un .cpp).
NK_SERIALIZE_COMPONENT(NkTransform,
    [](const void* c, NkArchive& out) -> bool { /* écrire les champs */ return true; },
    [](void* c, const NkArchive& in)  -> bool { /* relire les champs */ return true; },
    [](NkWorld& w, NkEntityId id)     { w.Add<NkTransform>(id); });

// 4) Sauver / charger une scène.
NkSceneSerializer serializer;
serializer.Save(scene, "levels/intro.nkscene");
// scene.World().FlushDeferred(); + scène vide AVANT un Load propre (Load est additif).
serializer.Load(scene, "levels/intro.nkscene");
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
