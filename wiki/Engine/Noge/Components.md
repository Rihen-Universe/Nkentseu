# Les composants ECS de Noge

> Couche **Engine** · Noge · Les *données* attachées aux entités : Transform, Tag, rendu,
> physique, animation, audio, caméra, UI, hiérarchie de scène — plus les **handles**
> ergonomiques qui y donnent accès et le **registre** de sérialisation.

Un moteur ECS sépare radicalement trois choses : les **entités** (de simples identifiants), les
**systèmes** (le comportement, qui itère sur les entités) et les **composants** (les *données*, et
rien que les données). Cette page documente la grande famille des **composants** de Noge : ce que
chacun stocke, ce qu'il signifie, et **dans quel domaine** on s'en sert. La règle d'or à garder en
tête : un composant Noge est presque toujours un `struct` **POD** à membres publics directs et
valeurs par défaut — pas d'encapsulation, pas de logique cachée. Quand un composant *expose* une
méthode (`Play()`, `AddForce()`, `RecalcProjection()`), c'est soit un raccourci d'écriture sur ses
propres champs, soit un calcul pur ; **le travail réel** (propager les transforms, intégrer la
physique, dessiner) appartient à des **systèmes** — dont beaucoup ne sont **pas fournis** dans ces
headers et relèvent donc de la *spécification*.

Ce n'est **pas** un framework objet où chaque entité serait une classe : on ne dérive pas de
`NkGameObject`, on **compose** une entité en lui attachant des composants. Et ce n'est **pas** non
plus un ensemble parfaitement consolidé : plusieurs types coexistent en **deux générations** (une
version « riche » POD plein et une version « légère » à base de `NkString`), et quelques types sont
carrément **redéfinis** dans deux fichiers — autant de pièges signalés explicitement plus bas.

- **Namespace** : `nkentseu::ecs` (sauf `NkComponentHandle.h` → `nkentseu`, et
  `NkComponentRegistry.h` → `nkentseu`, NKSerialization)
- **Headers réels** : `Core/NkCoreComponents.h` · `Core/NkTag.h` · `Core/NkTransform.h` ·
  `Animation/NkAnimation.h` · `Audio/NkAudio.h` · `Audio/NkAudioComponents.h` ·
  `Physics/NkPhysics.h` · `Physics/NkPhysicsComponents.h` · `Rendering/NkCamera.h` ·
  `Rendering/NkRenderComponents.h` · `Rendering/NkRenderer.h` ·
  `SceneComponent/NkSceneComponent.h` · `UI/NkUIComponent.h` · `NkComponentHandle.h` ·
  `NkComponentRegistry.h`

> **Remarque transversale.** Chaque définition de composant est suivie de la macro `NK_COMPONENT(T)`
> (de `NKECS/Core/NkTypeRegistry.h`) qui enregistre le type comme composant ECS. Les
> **sous-structures** (`NkBone`, `NkKeyframe`, `NkSocket`, `NkSpriteFrame`, les structs `Event`
> UI…) n'ont **pas** cette macro : ce sont des briques internes, pas des composants attachables.

---

## Le cœur : `NkTransform`, identité et hiérarchie

Le **Transform** est le composant que toute entité spatiale possède. Il stocke un placement
**local** — position, rotation (quaternion), échelle — et conserve, en lecture seule, le résultat
**monde** calculé par le système de transform. La distinction local/monde est fondamentale : on
**écrit** `localPosition`/`localRotation`/`localScale`, et un système lit la hiérarchie parent→enfant
pour recomposer `worldMatrix`/`worldPosition`. Les champs monde sont `mutable` et marqués
*read-only* : les toucher à la main n'a pas de sens, c'est le système qui les remplit.

```cpp
NkTransform t;
t.SetLocalPosition(0.f, 1.f, 0.f);
t.SetLocalRotationEuler(0.f, 90.f, 0.f);   // pitch, yaw, roll en degrés
t.SetLocalScale(2.f);                       // échelle uniforme
// worldMatrix / worldPosition seront remplis par le système, pas ici.
```

Toute écriture marque `worldDirty = true` ; le système recalcule alors. `ComputeLocalMatrix()` rend
la matrice TRS locale à la demande, et `GetWorldForward/Right/Up()` extraient les axes du repère
monde de la matrice.

L'**identité** d'une entité, elle, vit dans `NkName` (un nom lisible), `NkTag` (un bitfield de rôles
gameplay), `NkLayer` (couche + masque de collision/rendu), et une poignée de **tags vides**
(`NkInactive`, `NkStatic`, `NkPersist`, `NkHideInEditor`) dont la **simple présence** porte un état.

Ce n'est **pas** une scène-graphe à pointeurs : le lien parent/enfant passe par les composants
`NkParent` (l'entité du parent) et `NkChildren` (le tableau borné des enfants), pas par des nœuds
chaînés.

> **En résumé.** `NkTransform` = placement **local** que vous écrivez + résultat **monde**
> *read-only* recalculé par un système. L'identité = `NkName` / `NkTag` / `NkLayer` + tags vides
> dont la présence vaut booléen. La hiérarchie = `NkParent` + `NkChildren`.

> **⚠️ Conflit réel.** `NkParent` et `NkChildren` sont **redéfinis** dans `NkSceneComponent.h`
> (constante `kMaxChildren` au lieu de `kMax`). Ne jamais inclure `NkTransform.h` et
> `NkSceneComponent.h` ensemble : violation ODR. De même, le `NkTransform` canonique **n'a pas** de
> membres `localMatrix` / `position` / `dirty` — alors que le système `NkSceneTransformSystem` les
> utilise. Incohérence WIP détaillée en référence.

---

## Accéder aux composants : les trois handles

Itérer dans un système, c'est une chose ; **attraper** le composant d'une entité précise au gré du
gameplay en est une autre. Noge fournit pour cela trois petits wrappers type-Unity, dans le
namespace `nkentseu` (pas `ecs`). Tous résolvent le composant via `NkWorld::Get<T>()` — ils ne le
**possèdent** pas, ils savent juste le retrouver.

Le choix entre les trois est une question de **contrat de présence** :

- `NkComponentHandle<T>` — l'usage courant. `Get()` peut retourner `nullptr` (à tester) ; mais
  `operator->` et `operator*` **assertent** si le composant est absent.
- `NkOptionalComponent<T>` — *never-crash*. Si le composant manque, `operator->` pointe vers un
  dummy interne — donc on ne crashe jamais, mais **les écritures sur le dummy sont perdues
  silencieusement**. Pour lectures et actions idempotentes uniquement.
- `NkRequiredComponent<T>` — pour les composants **garantis présents** (les invariants d'un
  GameObject : `NkTransform`, `NkName`, `NkTag`). Aucun test null à écrire ; asserte si l'invariant
  est violé.

```cpp
NkComponentHandle<NkTransform> tr(world, id);
if (tr) tr->Translate({0, 1, 0});         // sûr : on a testé bool d'abord

NkOptionalComponent<NkAudioSource> snd(world, id);
snd->Play();                               // ne crashe pas même si absent (no-op silencieux)
```

> **En résumé.** `NkComponentHandle` (présence incertaine, `Get()` peut être null,
> `operator->` asserte), `NkOptionalComponent` (jamais de crash, dummy silencieux → lecture seule),
> `NkRequiredComponent` (présence garantie, zéro test). Dans une **boucle chaude**, préférez
> `world.Get<T>()` direct : ces handles re-résolvent à chaque appel (pas de cache réel).

---

## Aperçu de l'API

Tous les éléments publics, par famille. Le détail (champs, formules, cas d'usage, statut) suit dans
la « Référence complète ».

### Handles et registre

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handle | `NkComponentHandle<T>` | Accès résolu ; `Get()` nullable, `operator->`/`*` assertent. |
| Handle | `NkOptionalComponent<T>` | *Never-crash* ; dummy silencieux si absent. |
| Handle | `NkRequiredComponent<T>` | Présence garantie ; asserte sinon. |
| Registre | `NkComponentRegistry` | Registre de (dé)sérialisation des composants par `NkTypeId`. |
| Macro | `NK_REGISTER_COMPONENT(Type, Name)` | Enregistre un composant au démarrage (dans un `.cpp`). |
| Macro | `NK_REGISTER_COMPONENT_CUSTOM(Type, Name, Ser, Deser)` | Idem avec fonctions custom. |

### Core (identité, transform, hiérarchie)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Transform | `NkTransform` | Placement local (écriture) + monde (read-only). |
| Hiérarchie | `NkParent`, `NkChildren` | Entité parente / enfants bornés. |
| Identité | `NkName` | Nom lisible (128 car.). |
| Identité | `NkTag` + `enum NkTagBit` | Bitfield de rôles gameplay. |
| Identité | `NkLayer` | Couche + masque de collision/rendu. |
| Tags vides | `NkInactive`, `NkStatic`, `NkPersist`, `NkHideInEditor` | Présence = état. |
| Méta | `NkEntityMeta` | UID persistant, lien prefab, frame de création. |
| Convenance | `NkCoreComponents.h` | Header agrégateur (Transform + Tag). |

### Animation et géométrie

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Maillage | `NkStaticMesh`, `NkSkeletalMesh` | Mesh statique (LODs) / skinné (blend shapes). |
| Squelette | `NkSkeleton` (+ `NkBone`, `NkBoundingBox`) | Os, bind pose, matrices de skinning. |
| Anim data | `NkAnimationClip` (+ `NkAnimationCurve`, `NkKeyframe`, `NkAnimationProperty`) | Clip = courbes Hermite. |
| State machine | `NkAnimator` (+ `NkAnimatorState`, `NkAnimatorTransition`, `Param`) | Machine d'états + paramètres. |
| Édition | `NkMeshEditor` (+ `NkMeshEditorVertex/Edge/Face`) | Outil de modélisation (**spec**). |
| IK | `NkIKChain` | Chaîne FABRIK (**spec**). |
| Enums | `NkMeshTopology`, `NkWrapMode`, `NkMeshEditMode` | Topologie / bouclage / mode d'édition. |

### Physique (deux générations)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Corps (riche) | `NkRigidbody2D`, `NkRigidbody3D` | Corps dynamique 2D/3D + API force/impulsion. |
| Collision (riche) | `NkCollider2D`, `NkCollider3D`, `NkPhysicsMaterial` | Formes de collision, matériau. |
| Joints (riche) | `NkJoint2D`, `NkJoint3D`, `NkCharacterController` | Liaisons, contrôleur de perso. |
| Léger | `NkRigidbodyComponent`, `NkColliderComponent`, `NkVelocityComponent` | Versions `NkString`/minimales. |
| Enums | `NkBodyType`, `NkCollider2D/3DShape`, `NkJoint2D/3DType`, `NkCollisionDetection`… | Voir référence. |

### Caméra

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Virtuelle | `NkCamera` | Caméra de rendu (perspective/ortho) + projection. |
| Physique | `NkCameraPhysical` | Webcam / appareil réel + calibration AR. |
| Suivi | `NkCameraTarget` | Follow / orbit (**spec**). |
| Léger | `NkCameraComponent` | Version compacte. |
| Enums | `NkProjectionMode`, `NkClearMode`, `NkPhysicalCameraType`, `NkCameraProjection` | Voir référence. |

### Rendu (deux générations)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeur | `NkColor4`, `NkRect2D` | Couleur RGBA / rectangle (**redéfinis**, voir pièges). |
| 2D (riche) | `NkRenderer2D`, `NkSprite` (+ `NkSpriteFrame`) | Quad/sprite + animation. |
| 3D (riche) | `NkRenderer3D`, `NkLight`, `NkSkybox` | Mesh rendu, lumière, skybox. |
| Effets (riche) | `NkParticleSystem`, `NkTrailRenderer`, `NkLineRenderer` | Particules, traînée, ligne. |
| Léger | `NkMeshComponent`, `NkMaterialComponent` (+ `NkMaterialSlot`), `NkSkinnedMeshComponent`, `NkLightComponent`, `NkSpriteComponent`, `NkParticleSystemComponent`, `NkBlendshapeComponent` | Versions `NkString`. |
| Enums | `NkBlendMode` (×2 **incompatibles**), `NkLightType` (×2), `NkFlipMode`, `NkShadowCastMode`, `NkParticleEmitterShape`… | Voir pièges. |

### Audio (deux générations)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Riche | `NkAudioSource`, `NkAudioListener`, `NkAudioReverbZone` | Source 3D + API Play/Stop, écouteur, zone de réverb. |
| Léger | `NkAudioSourceComponent`, `NkAudioListenerComponent` | Versions `NkString`. |
| Enum | `NkAudioRolloff` | Atténuation par distance. |

### Scène (hiérarchie style Unreal)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Composant | `NkSceneComponent` (+ `NkSocket`) | Transform locale + sockets nommés. |
| Hiérarchie | `NkParent`, `NkChildren`, `NkSceneNode` | **Redéfinis** ici ; nœud éditeur. |
| Fonctions | `AttachSceneComponent`, `DetachSceneComponent` | Attacher / détacher dans le monde. |
| Système | `NkSceneTransformSystem` | Propagation des world transforms (**spec/WIP**). |

### UI (2D screen-space, 3D world-space, HUD)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Racine 2D | `NkCanvas`, `NkRectTransform` (+ `NkAnchor`, `NkAnchors`, `NkEdges`) | Canvas UI + ancrage. |
| Widgets | `NkUIImage`, `NkUIText`, `NkUIButton`, `NkUISlider`, `NkUIProgressBar`, `NkUIScrollView`, `NkUIPanel`, `NkUIInputField`, `NkUIToggle`, `NkUIDropdown`, `NkUILayout`, `NkUITooltip` | Composants d'interface 2D. |
| UI 3D | `NkBillboard`, `NkWorldCanvas`, `NkNameTag`, `NkHealthBar3D`, `NkUIMarker3D` | UI ancrée dans le monde. |
| HUD | `NkHUDCrosshair`, `NkHUDMinimap` | Réticule, minimap. |
| Events | `NkOnButtonClicked`, `NkOnSliderChanged`, `NkOnToggleChanged`, `NkOnInputChanged`/`Submitted`, `NkOnDropdownChanged`, `NkOnScrollChanged` | Structs d'événements UI (non composants). |
| Enums | `NkCanvasMode`, `NkTextAlign`, `NkImageType`, `NkInputContentType`, `NkUILayoutType`, `NkBillboardMode`… | Voir référence. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage à travers les domaines (gameplay/IA,
rendu, animation, physique, audio, UI/éditeur, IO, scène) et son **statut** quand il relève de la
spec.

### Les handles — `NkComponentHandle`, `NkOptionalComponent`, `NkRequiredComponent`

Tous trois sont des templates header-only, inline, du namespace `nkentseu`, longs de 24 octets (un
`NkWorld*` + un `NkEntityId`). Ils dépendent de `NkWorld::Get<T>()`/`Has<T>()` et de
`NkEntityId::Invalid()`/`IsValid()`. **API stable et implémentée.**

- `NkComponentHandle<T>` — construit par défaut (invalide) ou avec `(world, id)`. `Get()` est
  `[[nodiscard]]`, retourne `nullptr` si le monde est null, l'id invalide, ou le composant absent.
  `operator->` et `operator*` appellent `Get()` et **assertent** (`NKECS_ASSERT`) sur null — donc
  *à n'utiliser qu'après* avoir vérifié `IsValid()` ou la conversion `bool`. Accesseurs : `EntityId()`,
  `World()`. **Piège** : la doc de design en tête de fichier annonce un *cache* de pointeur invalidé
  après `FlushDeferred` ; l'implémentation **ne cache pas** et re-résout à chaque appel.
  - **Gameplay/IA** : attraper le `NkTransform` ou le `NkRigidbody3D` d'une cible désignée.
  - **Audio/UI** : déclencher `Play()` sur une `NkAudioSource` connue, lire l'état d'un `NkUIButton`.

- `NkOptionalComponent<T>` — *never-crash*. `IsPresent()` interroge `mWorld->Has<T>(mId)`.
  `operator->` (const et non-const) retourne `&mDummy` (un `mutable T mDummy = {}` interne) quand le
  composant manque — donc jamais de null à déréférencer, mais **toute écriture sur le dummy est
  perdue**. `Get()` (nullable, contrairement à `operator->`) sert quand on veut savoir si l'écriture
  aurait pris effet.
  - **Rendu/UI** : lire en boucle un paramètre optionnel (un `NkSkybox`, un `NkUITooltip`) sans
    brancher de `if` partout.
  - **À éviter** pour toute modification persistante d'un composant qui pourrait manquer.

- `NkRequiredComponent<T>` — pour les composants dont la présence est un **invariant** (créés par
  `NkGameObjectFactory::Create()`). `operator->`, `operator*` et `Get()` retournent toujours une
  référence valide et **assertent** (avec un message renvoyant à la factory) si l'invariant casse.
  - **Cœur du gameplay** : `NkTransform`, `NkName`, `NkTag` qu'on sait toujours là → code sans bruit
    de vérification.

### `NkComponentRegistry` — (dé)sérialisation des composants

> Malgré son chemin dans l'arborescence Noge/ECS, ce fichier appartient conceptuellement au **module
> NKSerialization** (header-guard, includes, export). Namespace `nkentseu`. Singleton Meyer
> thread-safe **en lecture**, STL-free (`NkVector` + pointeurs de fonctions), recherche linéaire
> O(n) optimisée pour < 1000 composants. **Implémenté complètement (inline).**

Le registre indexe chaque composant sérialisable par son `NkTypeId` et lui associe deux pointeurs de
fonctions : `SerializeFn` (`nk_bool(*)(const void*, NkArchive&)`) et `DeserializeFn`
(`nk_bool(*)(void*, const NkArchive&)`). La structure publique `Entry { typeId, name, serialize,
deserialize }` est ce que les recherches retournent.

- **Enregistrement** — `Register<T>(name, serFn=nullptr, deserFn=nullptr)` : met à jour les fonctions
  non-null si le type existe déjà, sinon ajoute une entrée (avec fallbacks `DefaultSerialize<T>` /
  `DefaultDeserialize<T>` si null). `Unregister<T>()` est un no-op si absent. **Ni l'un ni l'autre
  ne sont thread-safe** — à faire au démarrage uniquement.
- **Recherche** — `Find(NkTypeId)` (O(n)), `FindByName(name)` (`strcmp`, sensible à la casse, O(n×m)),
  `Find<T>()` (= `Find(NkTypeOf<T>())`). `Count()` est O(1).
- **(Dé)sérialisation** — par id (`Serialize(id, obj, out)` / `Deserialize(id, obj, arc)`) ou par
  type (`Serialize(const T&, out)` / `Deserialize(T&, arc)`).
- **Parcours** — `ForEach(fn)` itère `const Entry&` (ne pas modifier le registre pendant).
- **Fallbacks** — `DefaultSerialize/Deserialize<T>` : si `T` dérive de `NkISerializable`, délègue à
  `obj->Serialize/Deserialize` ; sinon dump binaire brut en hexadécimal sous les clés `"__raw__"` et
  `"__rawSz__"` (= `sizeof(T)`), avec vérification de la taille à la relecture.
- **Macros** (à mettre **uniquement dans un `.cpp`**, jamais en header → sinon ODR) :
  `NK_REGISTER_COMPONENT(Type, Name)` crée un static anonyme dont le ctor appelle `Register<Type>`
  avant `main` ; `NK_REGISTER_COMPONENT_CUSTOM(Type, Name, SerFn, DeserFn)` fait de même avec des
  fonctions custom.

Cas d'usage : **IO/scène** — sauvegarde/chargement de scènes `.nkscene`, de prefabs ; **éditeur** —
inspecteur générique qui sérialise n'importe quel composant connu par son `NkTypeId`.

### `NkCoreComponents.h` — agrégateur

Header de convenance pur : il inclut `NkTransform.h` + `NkTag.h` et n'expose **aucun symbole propre**.
À inclure quand on veut « le cœur » d'un coup (Transform/Parent/Children + Name/Tag/Layer + tags
vides).

### `NkTransform` à fond

Les champs **locaux** (que vous écrivez) : `localPosition` (`NkVec3f`), `localRotation` (`NkQuatf`,
identité par défaut), `localScale` (`NkVec3f`, `{1,1,1}`). Les champs **monde** *read-only* et
`mutable` (remplis par le système) : `worldMatrix`, `worldPosition`, `worldDirty` (à `true` par
défaut). Toutes les mutations (`SetLocalPosition`, `Translate`, `SetLocalRotation`,
`SetLocalRotationEuler` en degrés via `NkEulerAngle`, `Rotate`, `SetLocalScale` vecteur ou uniforme)
marquent `worldDirty`. Les lectures monde — `GetWorldPosition()`, `GetWorldForward/Right/Up()`
(extraits des colonnes de `worldMatrix`) — ne sont fiables qu'**après** passage du système.
`ComputeLocalMatrix()` rend la matrice TRS locale ; `MarkDirty()` est `const` (champ mutable).

- **Scène/gameplay** : placement des entités, déplacement, rotation de tourelles, échelle d'objets.
- **Caméra/rendu** : `GetWorldForward()` pour orienter un regard, une lumière directionnelle.
- **Animation** : cible des courbes (position/rotation animées) avant skinning.

> **⚠️ Statut WIP.** Ce `NkTransform` n'a **pas** de `localMatrix`, ni `position`, ni `dirty` — mais
> `NkSceneTransformSystem` (dans `NkSceneComponent.h`) écrit ces trois membres. Le système ne compile
> donc pas en l'état contre ce Transform : à traiter comme code de spec non aligné.

### `NkName`, `NkTag` (+ `NkTagBit`), `NkLayer`, tags vides, `NkEntityMeta`

- `NkName` — `char value[128]` initialisé à `"Entity"`. `Set/Get`, `operator==` (`strcmp`). **Piège** :
  le ctor `explicit NkName(const char*)` fait `strncpy(value, name, 127)` et **ne null-termine pas**
  si la source atteint 127 caractères. Usage : éditeur, logs, recherche par nom.
- `NkTag` — `uint64 bits` interprété via l'enum `NkTagBit` (scope `NkTagBit::VALEUR`, bitfield) :
  `None=0`, `Player`, `Enemy`, `Ally`, `Neutral`, `Ground`, `Wall`, `Ceiling`, `Trigger`, `Sensor`,
  `Projectile`, `Pickup`, `Interactable`, `Destructible`, `Environment`, `Terrain`, `Water`, `UI`,
  `Camera`, `Light`, `Particle`, `Audio` (bits 0→20), puis `EditorOnly` (bit 48), `Prefab` (49),
  `Internal` (63) ; les bits **21→47 sont libres** pour l'utilisateur, **48→63 réservés système**.
  API : `Add`, `Remove`, `Has` (un bit), `HasAll`, `HasAny`, `HasNone`, `Clear`. **Piège** :
  `operator&(NkTagBit, NkTagBit)` renvoie un **`bool`** (test d'intersection ≠ 0), pas un `NkTagBit`.
  - **Gameplay/IA** : filtrer ennemis vs alliés, marquer un trigger, un projectile, un objet
    ramassable ; un système de tir ne touche que les `Destructible`.
- `NkLayer` — `layer` (0-31) + `mask` (`0xFFFFFFFF`). `CanCollideWith(other)` est **bidirectionnel**
  (chaque mask doit inclure la couche de l'autre) ; `LayerBit()` = `1u << layer`. Constantes :
  `kDefault=0`, `kIgnoreRaycast=2`, `kWater=4`, `kUI=5`, `kPostProcessing=8`.
  - **Physique/rendu** : matrices de collision par couche, exclusion du raycast, masques de rendu.
- Tags vides (présence = état) : `NkInactive` (entité désactivée), `NkStatic` (immobile → optim),
  `NkPersist` (survit au changement de scène), `NkHideInEditor` (masquée du panneau de scène).
- `NkEntityMeta` — `uid` persistant, `char prefabPath[256]`, `isPrefabInstance`, `isPrefabRoot`,
  `createdFrame`. Usage : éditeur/IO (lien vers le prefab d'origine, identité stable inter-sessions).

### Animation et géométrie (`NkAnimation.h`)

Enums : `NkMeshTopology { Triangles=0, Quads=1, Lines=2, Points=3 }`,
`NkWrapMode { Once=0, Loop=1, PingPong=2, ClampForever=3 }`,
`NkMeshEditMode { Object=0, Vertex=1, Edge=2, Face=3 }`.

- **`NkBoundingBox`** (non composant) — `min`/`max` + `Center()` / `Extent()`. Brique de culling.
- **`NkStaticMesh`** — handle GPU `meshId`, `meshPath[256]`, comptes de sous-mesh/sommets/indices,
  `topology`, `bounds`, flags `isReadable`/`generateMipMaps`, et un système de **LODs** (jusqu'à 8 :
  `lodMeshIds`, `lodDistances` par défaut `{0,20,50,100,200,500,1000,2000}`, `lodCount`). Pas de
  méthode. **Rendu** : props de décor, géométrie sans squelette.
- **`NkBone`** (non composant) — `name[64]`, `parent` (-1 = racine), `bindPose`/`inverseBindPose`,
  transform local. **`NkSkeleton`** (jusqu'à 256 os) — `bones`, `boneCount`, `skeletonPath`,
  `skeletonId`, `skinMatrices[256]`, et `FindBone(name)` (strcmp → index ou -1). **Animation** :
  squelette partagé entre meshes skinnés.
- **`NkSkeletalMesh`** — comme le static mesh + `skeletonId`, jusqu'à 8 matériaux, **blend shapes**
  (jusqu'à 64 : noms + poids), flags `castShadow`/`receiveShadow`/`visible`/`updateWhenOffscreen`.
  **Animation/rendu** : personnages, créatures.
- **`NkKeyframe`** (non composant) — `time`, `value`, `inTangent`, `outTangent`.
  **`NkAnimationCurve`** (non composant, jusqu'à 64 keys) — `Evaluate(t)` fait une **interpolation
  Hermite cubique** entre les keyframes encadrant `t` (clamp aux bords). **Implémenté.**
  **`NkAnimationProperty`** (non composant) — cible une propriété (`path`, `propertyName`,
  `boneIndex`, `curveIndex`).
- **`NkAnimationClip`** — `name`, `clipPath`, `length` (sec), `frameRate` (30), `wrapMode` (Loop),
  `isLooping`, `hasRootMotion`, `clipId`, jusqu'à 128 propriétés + 512 courbes. **Animation** : une
  animation jouable (marche, saut).
- **`NkAnimatorTransition`** / **`NkAnimatorState`** (non composants) — la transition porte l'état
  cible, une condition (`conditionParam`, `conditionValue`, `conditionIsGreater`), `exitTime`
  normalisé [0..1], `transitionDuration` (0.2), `hasExitTime` ; l'état porte `name`, `clipId`,
  `speed`, `loop`, jusqu'à 16 transitions.
- **`NkAnimator`** — la **machine d'états** complète (jusqu'à 64 états/clips, 32 paramètres, 8
  couches). Sous-struct `Param { name[64], value, isTrigger }`. Playback : `time`, `normalizedTime`,
  `playbackSpeed`, `playing`, `applyRootMotion`, état courant/suivant et progression de transition.
  API : `Play(stateName)` (cherche par nom, reset le temps), `SetFloat`, `SetBool` (délègue à
  SetFloat 1/0), `SetTrigger` (seulement si le param est un trigger), `GetFloat`, `IsInState`.
  **Animation/gameplay** : piloter les animations d'un perso par des paramètres (vitesse, « saute »).
- **`NkMeshEditorVertex/Edge/Face`** (non composants) — sommet (position/normal/uv/couleur/selected),
  arête (`v0`/`v1`/selected/sharp/seam), face (jusqu'à 8 sommets, `materialIndex`, `faceNormal`).
  **`NkMeshEditor`** — outil de **modélisation** : `editMode`, flags (`isDirty`, `showWireframe`,
  `snapToGrid` + `gridSize`, `proportionalEdit` + rayon, miroirs X/Y/Z), comptes, `editorDataHandle`
  (buffer CPU externe), `gpuMeshId`, pile d'undo. **Statut spec** : commentaires explicites — tailles
  statiques « pour la simplicité », les vraies données vivent dans un buffer géré par
  `NkMeshEditorSystem` (non fourni). **Éditeur/modeling**.
- **`NkIKChain`** — Inverse Kinematics **FABRIK** : `effectorBone`, `rootBone`, `target` (monde),
  `chainLength` (3), `iterations` (10), `tolerance`, `enabled`, cible-entité optionnelle,
  `poleVector`. Pas de méthode → résolution = système externe **non fourni** → **spec**.
  **Animation** : poser un pied sur le sol, une main sur une poignée.

### Physique riche (`NkPhysics.h`)

Enums communs : `NkBodyType { Dynamic=0, Kinematic=1, Static=2 }`,
`NkCollisionDetection { Discrete=0, Continuous=1, ContinuousDynamic=2 }`,
`NkInterpolation { None=0, Interpolate=1, Extrapolate=2 }`,
`NkCollider2DShape { Box=0, Circle=1, Capsule=2, Polygon=3, Edge=4, Chain=5 }`,
`NkCapsuleDirection2D { Vertical=0, Horizontal=1 }`,
`NkCollider3DShape { Box=0, Sphere=1, Capsule=2, Cylinder=3, ConvexMesh=4, TriangleMesh=5, Terrain=6 }`,
`NkCapsuleDirection3D { X=0, Y=1, Z=2 }`,
`NkJoint2DType { Fixed=0, Hinge=1, Slider=2, Spring=3, Distance=4 }`,
`NkJoint3DType { Fixed=0, Hinge=1, Slider=2, BallSocket=3, Spring=4, Universal=5 }`.

- **`NkRigidbody2D`** / **`NkRigidbody3D`** — config (`bodyType`, `mass`, `gravityScale`, `drag`,
  `angularDrag`, gels d'axes, matériau friction/restitution) + état runtime (`velocity`,
  `angularVelocity`, `isSleeping`, accumulateurs `force`/`torque`, `interpolation`). Le 3D ajoute
  `centerOfMass`, `inertiaTensor`, `collisionDetection`. API runtime : `AddForce` (accumule),
  `AddImpulse` (`velocity += imp/mass`), `AddTorque`, `SetVelocity`, `Stop()` ; le 3D ajoute
  `AddForceAtPoint(f, worldPoint)` dont le torque effectif est « calculé par le système physique »
  (**spec partielle**). **Physique/gameplay** : tout ce qui bouge sous la simulation (caisses,
  véhicules, projectiles).
- **`NkCollider2D`** / **`NkCollider3D`** — `shape`, offset/center, `isTrigger`, `layer`/`layerMask`,
  friction/restitution, `enabled`, paramètres par forme (box/circle/sphere/capsule/polygon/chain/
  mesh/terrain) et IDs internes (`physicsBodyId`, `physicsShapeId`). **Physique** : forme de
  collision ; `isTrigger` = zone de détection sans réponse.
- **`NkPhysicsMaterial`** — `staticFriction`, `dynamicFriction`, `restitution`, `name[64]`.
- **`NkJoint2D`** / **`NkJoint3D`** — `type`, `connectedBody`, ancrages, `enableCollision`,
  `breakable` + seuils ; paramètres par type (hinge limites/moteur, spring/distance fréquence/amorti,
  axe 3D). **Physique** : portes, pendules, suspensions, chaînes.
- **`NkCharacterController`** — `radius`, `height`, `slopeLimit`, `stepOffset`, `skinWidth`, `center`,
  + état runtime (`velocity`, `isGrounded`, `isOnSlope`, `groundAngle`, `groundNormal`). `Move(motion)`
  ne fait que poser la vélocité — le mouvement réel est un système externe → **spec**.
  **Gameplay** : déplacement d'un personnage joueur sans simulation rigide complète.

### Physique légère (`NkPhysicsComponents.h`)

Versions minimales (`using namespace nkentseu::math`). Enums propres :
`NkRigidbodyType { Dynamic=0, Kinematic, Static }`,
`NkColliderShape { Box=0, Sphere, Capsule, Cylinder, Mesh }`.

- **`NkRigidbodyComponent`** — `type`, `mass`, `drag`, `angularDrag`, `useGravity`, gels de rotation,
  + champs calculés par `NkPhysicsSystem` (`velocity`, `angularVelocity`, `force`).
- **`NkColliderComponent`** — `shape`, `offset`, `size` (box=extents, sphere=radius en x), friction/
  restitution, `isTrigger`, `layer`.
- **`NkVelocityComponent`** — `linear` + `angular`, pour des systèmes simples sans rigidbody complet.

> **⚠️ Deux générations.** `NkRigidbody3D` ≠ `NkRigidbodyComponent`, `NkCollider3D` ≠
> `NkColliderComponent` — types **distincts**, pas des alias. Choisir une convention par projet.

### Caméra (`NkCamera.h`)

Enums : `NkProjectionMode { Perspective=0, Orthographic=1, Frustum=2 }`,
`NkClearMode { SkyboxOrColor=0, DepthOnly=1, None=2, Color=3 }`,
`NkPhysicalCameraType { Webcam=0, Smartphone=1, DSLR=2, IPCamera=3, Capture=4, Depth=5, Stereo=6, Thermal=7, Virtual=8 }`.

- **`NkCamera`** — la caméra **virtuelle** de rendu : projection (`projectionMode`, `fieldOfViewDeg`,
  `orthographicSize`, `nearClip`/`farClip`, `aspectRatio`), priorité/viewport, mode de clear et
  couleur de fond, masque de culling, flags (`isMainCamera`, `hdr`, `msaa`+samples, `renderToTexture`),
  post-process (gamma, exposure, bloom, AO, DoF) et paramètres 2D (`zoom`, `offset2D`). Matrices
  *read-only* (`viewMatrix`/`projMatrix`/`viewProjMatrix`, remplies par `NkCameraSystem`). Sous-struct
  `Ray { origin, direction }`. Méthodes **implémentées** : `RecalcProjection(aspect)` (remplit la
  projection perspective ou ortho), `ScreenToWorldRay(ndcX, ndcY)` (perspective, **simplifié**),
  `SetOrthographic(size)`, `SetPerspective(fov, near, far)`.
  - **Rendu** : caméra principale, caméras secondaires (rétroviseur, minimap via `renderToTexture`).
  - **Gameplay/IA** : `ScreenToWorldRay` pour le picking, le tir à la souris.
- **`NkCameraPhysical`** — webcam/appareil **réel** : identité (`deviceId`, `rtspUrl`, `deviceName`),
  acquisition (résolution, frame rate, FourCC), exposition (`iso`, `shutterSpeed`, `aperture`,
  `focalLength`, taille capteur, auto-exposure/focus), runtime (`isOpen`, `isStreaming`, `frameCount`,
  `textureId`) et **calibration AR** (focale px, point principal, distorsion[8]). Méthodes
  implémentées : `ComputeFOVDegrees()`, `ComputeAspectRatio()`, `SetResolution(w, h)`.
  - **IO/AR** : flux caméra (pont vers le module Runtime NKCamera), réalité augmentée calibrée.
- **`NkCameraTarget`** — suivi : `target`, `offset` (`{0,2,-5}`), `smoothSpeed`, `lookAheadFactor`,
  verrous d'axes, `orbitMode` + rayon/angles. Pas de méthode → logique = système **non fourni** →
  **spec**. **Gameplay** : caméra TPS qui suit le joueur, caméra orbitale.

### Rendu : valeurs `NkColor4` / `NkRect2D`

`NkColor4` — `r`/`g`/`b`/`a` (1 par défaut), ctor `constexpr(r,g,b,a=1)`, statics couleurs
(`White/Black/Red/Green/Blue/Clear/Yellow/Cyan/Magenta`) + `FromU8(r,g,b,a=255)`. `NkRect2D` —
`x`/`y`/`w`/`h` + ctor constexpr. Usage universel : couleurs de teinte, UV rects, viewports.

> **⚠️ Redéfinitions.** `NkColor4` est défini **3 fois** (`NkRenderComponents.h`, `NkRenderer.h`,
> référencé sans redéfinition dans `NkUIComponent.h`) et `NkRect2D` **2 fois**. **Seule** la version
> de `NkRenderer.h` ajoute `FromHex(uint32 hex)` (ARGB packé). Ne pas inclure deux headers Rendering
> qui les redéfinissent ensemble.

### Rendu léger (`NkRenderComponents.h`)

Version « légère » à base de `NkString`. Enums :
`NkLightType { Directional=0, Point, Spot, Area, Ambient }`,
`NkBlendMode { Opaque=0, AlphaBlend, Additive, Multiply, PremultAlpha }` (5 valeurs),
`NkRenderer2DType { None=0, Quad, Sprite, Text, Shape, Tilemap, Custom }`,
`NkCameraProjection { Perspective=0, Orthographic }`.

- **`NkMeshComponent`** — `meshHandle`, `meshPath` (`NkString`), `subMeshIndex`, flags `visible`/
  `castShadow`/`receiveShadow`. **Rendu** : mesh statique en version compacte.
- **`NkMaterialSlot`** (non composant) — `materialHandle` + `materialPath`. **`NkMaterialComponent`**
  — jusqu'à 8 slots + `SetMaterial(slot, path)` (étend `slotCount`). **Rendu** : multi-matériaux.
- **`NkSkinnedMeshComponent`** — handles mesh/squelette + chemins, jusqu'à 128 matrices d'os, flags.
- **`NkCameraComponent`** — version compacte : `projection`, fov/clips/orthoSize/aspect, `priority`,
  `renderTargetHandle`, matrices *read-only*. **Distinct** de `NkCamera` (riche).
- **`NkLightComponent`** — `type`, `color`, `intensity`, `range`, angles inner/outer, `castShadow`.
- **`NkSpriteComponent`** — `textureHandle`, `texturePath`, `uvRect`, `color`, `pixelsPerUnit`,
  `blendMode`, flips. **2D**.
- **`NkParticleSystemComponent`** — `particleHandle`, `presetPath`, `playing`/`loop`, `duration`,
  `speed`. **Effets** (config légère pointant un preset).
- **`NkBlendshapeComponent`** — jusqu'à 64 `weights`, `count`, `dirty`. **Animation faciale**.

### Rendu riche (`NkRenderer.h`)

Enums : `NkBlendMode { Opaque=0, AlphaBlend, Additive, Multiply, Premult, Screen }` (**6 valeurs**),
`NkFlipMode { None=0, Horizontal, Vertical, Both }`,
`NkShadowCastMode { Off=0, On, TwoSided, ShadowsOnly }`,
`NkLODMode { Auto=0, Fixed, None }`,
`NkLightType { Directional=0, Point, Spot, Area, Ambient }`,
`NkParticleSimulationSpace { World=0, Local }`,
`NkParticleEmitterShape { Sphere=0, Hemisphere, Cone, Box, Circle, Edge, Mesh }`.

> **⚠️ `NkBlendMode` incompatible entre les deux fichiers.** Ici 6 valeurs (index 4 = `Premult`,
> + `Screen`) contre 5 dans `NkRenderComponents.h` (index 4 = `PremultAlpha`, pas de `Screen`).
> Ne jamais mélanger.

- **`NkRenderer2D`** — `textureId` (0 = quad coloré), `color`, `uvRect`, `blendMode`, `flip`,
  `sortingOrder`/`sortingLayer`, `pivot`, `size`, flags (`visible`/`castShadow`/`receiveShadow`/
  `pixelPerfect`), `shaderId`, `shaderParams[8]`. **2D** : un quad/sprite simple sans animation.
- **`NkSpriteFrame`** (non composant) — `uvRect`, `pivot`, `duration`. **`NkSprite`** — sprite
  **animé** (jusqu'à 256 frames) : texture, couleur, blend/flip, tri, pivot/size, animation
  (`frames`, `frameCount`, `currentFrame`, `frameTimer`, `playing`, `loop`, `playbackSpeed`,
  `currentUV`). Méthodes : `SetSingleFrame`, `Play`/`Pause`/`Stop`, `Update(dt)` (**implémenté** :
  avance les frames, gère loop/fin). **2D/gameplay** : animations de sprites (perso 2D, effets).
- **`NkRenderer3D`** — `meshId`, jusqu'à 8 matériaux, `castShadow` (`NkShadowCastMode`),
  `receiveShadow`, `visible`, `cullingMask`, LOD (`lodMode`/`lodBias`/`lodLevel`), instancing,
  `colorOverride`, `shaderParams[8]`. **Rendu** : objet 3D rendu, avec LOD et instancing.
- **`NkLight`** — `type`, `color`, `intensity`, `range`, `spotAngle`/`spotBlend`, ombres
  (`castShadow`, `shadowResolution`, biais), `areaSize`, **volumétrique** (intensité/scatter),
  **flare** (texture/taille). **Rendu/éclairage** : soleil directionnel, lampes, spots, lumières
  volumétriques.
- **`NkParticleSystem`** — config CPU **complète** : émission (taux, burst, durée, loop, prewarm),
  lifetime/vitesse/taille (min-max + over-lifetime), couleur start/end, rotation, gravité +
  `simSpace`, **shape d'émission** (`NkParticleEmitterShape` + radius/angle/box), rendu (texture,
  `blendMode` Additive par défaut, `maxParticles`, billboard étiré), runtime `activeParticles`.
  Méthodes : `Play`/`Stop`/`Pause`/`Resume`. **Effets** : feu, fumée, étincelles, magie.
- **`NkTrailRenderer`** — traînée (jusqu'à 128 points) : `time`, largeurs/couleurs start-end,
  texture, blend, `minVertexDistance`. **Effets** : sillage d'épée, traînée de projectile.
- **`NkLineRenderer`** — polyligne (jusqu'à 256 points) : `width`, `color`, `loop`, `worldSpace`.
  **Rendu/debug** : tracés, lasers, gizmos de chemin.
- **`NkSkybox`** — `cubemapId`, `exposure`, `rotation`. **Rendu** : ciel/environnement.

### Audio riche (`NkAudio.h`)

Enum `NkAudioRolloff { Logarithmic=0, Linear=1, Custom=2 }`.

- **`NkAudioSource`** — source : `clipId`, `clipPath`, `volume` [0..1], `pitch` [0.1..3], `pan`
  [-1..1], `loop`, `playOnAwake`, `is3D`, distances min/max, `spatialBlend` (0=2D, 1=3D),
  `dopplerLevel`, `spread`, `rolloff` ; runtime (`isPlaying`, `playbackTime`, handle) ; mixage
  (`mixerGroup` = "Master"), effets (réverb wet, low-pass cutoff, `mute`). API : `Play`, `Stop`
  (reset le temps), `Pause`, `Resume` — qui ne touchent **que les flags runtime** (pas de moteur réel
  ici). **Audio/gameplay** : sons positionnés (pas, tirs, ambiances).
- **`NkAudioListener`** — `volume`, `enabled` (un seul par scène attendu). **Audio** : l'oreille,
  généralement sur la caméra/le joueur.
- **`NkAudioReverbZone`** — `minDistance`/`maxDistance` + paramètres EAX complets (`room`, `roomHF`,
  `decayTime`, `decayHFRatio`, réflexions/delays, références HF/LF, `enabled`). **Audio/scène** :
  réverbération par zone (caverne, cathédrale).

### Audio léger (`NkAudioComponents.h`)

- **`NkAudioSourceComponent`** — `clipPath` (`NkString`), `clipHandle`, `volume`, `pitch`, distances,
  `playOnStart`, `loop`, `spatialize`, `playing`. **Distinct** de `NkAudioSource`.
- **`NkAudioListenerComponent`** — `volume`.

### Scène (`NkSceneComponent.h`) — hiérarchie style Unreal

Forward declarations `NkGameObject` / `NkSceneComponent`. **Redéfinit** `NkParent` (`entity`) et
`NkChildren` (constante `kMaxChildren=64`, mêmes `Add`/`Remove` swap-remove/`Has`) — d'où le conflit
ODR avec `NkTransform.h`.

- **`NkSceneNode`** — métadonnées éditeur : `name[64]`, `active`, `visible`, `layer` + ctor
  `explicit(const char*)`. **Éditeur** : nœud nommé dans l'arbre de scène.
- **`NkSocket`** (non composant) — point d'attache nommé : `name[64]`, transform local + ctor 4 args
  et `GetLocalMatrix()` (TRS). **Animation/gameplay** : accrocher une arme à une main, un effet à un
  os.
- **`NkSceneComponent`** — transform locale (`localPosition`/`localRotation`/`localScale`), flags
  `dirty`/`hierarchyDirty`, jusqu'à 16 sockets, `componentName` (= "SceneComponent"), `enabled`.
  Ctors par défaut et `explicit(const char*)`. API locale (qui appellent `MarkDirty()`) :
  `SetLocalPosition/Rotation/Scale`, `SetLocalTransform(pos,rot,scale)` ; dirty (`MarkDirty`,
  `MarkClean`, `IsDirty`, `IsHierarchyDirty`) ; `GetLocalMatrix()` (TRS) ; sockets (`AddSocket`
  complet ou par nom, `FindSocket`, `HasSocket`) ; helpers 2D (`SetLocalPosition2D`,
  `SetLocalRotation2D` quat axe Z). **Scène/éditeur** : alternative au Transform pour une hiérarchie
  à la Unreal avec sockets.
- **Fonctions libres** (inline) : `AttachSceneComponent(world, childId, parentId)` (pose `NkParent`,
  ajoute à `NkChildren`, marque dirty, vérifie `IsAlive`) et `DetachSceneComponent(world, childId)`
  (retire l'enfant, `Remove<NkParent>`, marque dirty). **Scène** : (dé)parenter à l'exécution.
- **`NkSceneTransformSystem`** (`final : public NkSystem`) — propagation batchée des world transforms
  (DFS itératif, parents avant enfants). `Describe()` : reads NkSceneComponent/NkParent/NkChildren/
  NkTransform, writes NkTransform, groupe `PreUpdate`, priorité -50. `Execute()` traite les racines
  puis propage.

> **⚠️ Statut spec / non compilable en l'état.** Le système écrit `transform.worldMatrix` mais aussi
> `transform.localMatrix`, `transform.dirty`, `transform.position` — membres **absents** du
> `NkTransform` canonique → ne compile pas tel quel. De plus, dans `PropagateToChildren`, la
> `NkVector<StackEntry> stack` n'est jamais dimensionnée et la condition `stackSize < stack.size()`
> (taille 0) **empêche tout push** : la propagation aux enfants ne s'exécute jamais (bug logique).
> `NkSystem`/`NkSystemDesc`/`NkSystemGroup` viennent de NKECS. Traiter comme WIP. (Le bloc final du
> fichier est commenté — exemples.)

### UI (`NkUIComponent.h`)

> **⚠️ Dépendance d'inclusion.** Ce header utilise `NkColor4` **sans le redéfinir** : il faut avoir
> inclus auparavant un header qui le définit (`NkRenderer.h` / `NkRenderComponents.h`), sinon la
> compilation isolée échoue.

**Types communs.** `NkAnchor` (non composant) : `minX`/`minY`/`maxX`/`maxY` (0.5). `namespace
NkAnchors` : fonctions inline `TopLeft … BottomRight`, `StretchFull/H/V`. `NkEdges` (non composant) :
`left`/`right`/`top`/`bottom` + ctors `(all)`, `(h,v)`, `(l,r,t,b)`. Enums :
`NkTextAlign { Left=0, Center, Right, Top=0, Middle=1, Bottom=2 }` (**valeurs dupliquées
volontairement** : Left==Top==0… — réutilisé pour H et V, piège si on switch dessus),
`NkOverflow { Visible=0, Hidden, Scroll, Truncate }`,
`NkUIState { Normal=0, Hovered, Pressed, Disabled, Selected }`,
`NkCanvasMode { ScreenSpace=0, WorldSpace, CameraSpace }`,
`NkImageType { Simple=0, Sliced, Tiled, Filled }`,
`NkFillMethod { Horizontal=0, Vertical, Radial90, Radial180, Radial360 }`,
`NkInputContentType { Standard=0, Integer, Decimal, Alphanumeric, Password, Email, Custom }`,
`NkButtonTransition { None=0, ColorTint, SpriteSwap, Animation }`,
`NkUILayoutType { None=0, Horizontal, Vertical, Grid }`,
`NkUIAlignment { Start=0, Center, End, SpaceBetween, SpaceAround }`,
`NkBillboardMode { FullFacing=0, VerticalOnly, Horizontal }`.

- **`NkCanvas`** — racine UI 2D : `mode`, dimensions de référence (1920×1080), `pixelsPerUnit`,
  `planeDistance`, `cameraEntity`, `scaleWithScreen`, `scaleFactor`, `sortingOrder`, `visible`/
  `interactable`, `targetTextureId`. **Note** : ce nom collisionne conceptuellement avec le module
  Runtime NKCanvas — ici c'est un composant ECS UI, distinct.
- **`NkRectTransform`** — `anchor`, `pivot`, `anchoredPos`, `sizeDelta` (`{100,30}`), `localScale`,
  `localRotation` (deg CCW), `visible` ; rect calculé (par `NkUILayoutSystem`) `rectX/Y/W/H` ;
  `Contains(px, py)` et `Center()`. **UI** : positionnement ancré de chaque widget.
- **`NkUIImage`** — `textureId`, `color`, `imageType`, `preserveAspect`, `raycastTarget`, bordure
  (`NkEdges`), méthode de remplissage (`fillMethod`/`fillAmount`/`fillClockwise`/`fillOrigin`),
  `blendMode`, `materialId`. **UI** : images, jauges remplies (radiales).
- **`NkUIText`** — `char text[1024]`, `fontId`, `fontSize`, `color`, alignements H/V, `overflow`,
  styles (bold/italic/underline/strikethrough), espacements, contour, ombre. `SetText(t)` (strncpy,
  gère null). **`SetTextf(fmt, ...)` est DÉCLARÉE sans corps → à implémenter dans un `.cpp` (spec /
  linker error si appelée).** **UI** : tout texte d'interface.
- **`NkUIButtonColors`** (non composant) — couleurs par état + `fadeDuration`. **`NkUIButton`** —
  `interactable`, `state`, `transition`, `colors`, `stateTimer`, mode toggle (`isToggle`/`isOn`),
  sprites par état, sons (clic/hover), `buttonId`, `buttonTag`. `Click()` (implémenté : passe
  Pressed, toggle `isOn`). **UI/gameplay** : boutons de menu, toggles.
- **`NkUISlider`** — `value`/`minValue`/`maxValue`, `wholeNumbers`, `vertical`, `interactable`,
  `state`, IDs background/fill/handle, couleurs, `handleSize`. `Normalized()` / `SetNormalized(t)`
  (clamp [0..1], `round` si entiers). **UI** : volume, réglages.
- **`NkUIProgressBar`** — `value`, `vertical`/`reverse`, couleurs, `showLabel` + `labelFormat`,
  animation (`animate`/`animSpeed`/`displayedValue`). **UI** : barres de chargement, XP.
- **`NkUIScrollView`** — `horizontal`/`vertical`, sensibilité, `normalizedPosition`, dimensions de
  contenu, inertie (`decelerationRate`, `velocity`), scrollbars. **UI** : listes défilantes.
- **`NkUIPanel`** — `backgroundColor`, texture, `padding`, `cornerRadius`, bordure, `clipContent`.
  **UI** : conteneurs, fenêtres, fonds.
- **`NkUIInputField`** — `char text[512]`, `placeholder`, `passwordMask`, `contentType`, `maxLength`,
  `multiline`, `isFocused`, caret (`caretPos`/`caretVisible`/`caretBlinkRate`/`caretTimer`), couleurs,
  `fontId`/`fontSize`. `SetText(t)` (strncpy + `caretPos`=strlen), `IsEmpty()`. **UI** : saisie de
  texte, mots de passe.
- **`NkUIToggle`** — `isOn`, `interactable`, `state`, couleurs, `checkmarkId`, `toggleGroupId`,
  `label`. **UI** : cases à cocher, radios (via groupe).
- **`NkUIDropdown`** — jusqu'à 64 options, `selectedIndex`, `isOpen`, `maxHeight`, `itemHeight`,
  couleurs d'items. `AddOption(opt)`, `GetSelected()` (= "" si hors borne). **UI** : menus déroulants.
- **`NkUILayout`** — `type`, `alignment`, `padding`, `spacing`, contrôle largeur/hauteur, `forceExpand`,
  grille (`columns`, `cellSize`, `cellSpacing`), `reverseArrangement`. **UI** : disposition auto
  horizontale/verticale/grille.
- **`NkUITooltip`** — `text[512]`, `delay`/`timer`, couleurs, `maxWidth`, `padding`. **UI** : info-bulles.

**UI 3D (world-space).** `NkBillboard` (orienté caméra : `mode`, `lockScale`, `screenSize`, texture,
couleur, taille), `NkWorldCanvas` (canvas dans le monde : dimensions, `pixelsPerUnit`, `faceCamera`,
render texture), `NkNameTag` (étiquette de nom au-dessus d'une entité : couleurs, `yOffset`, `scale`,
`visibleDistance`, `alwaysOnTop`), `NkHealthBar3D` (barre de vie : `value`/`targetValue`/`animSpeed`,
couleurs full/empty, dimensions, `hideWhenFull`, `faceCamera`), `NkUIMarker3D` (marqueur d'objectif :
icône, couleur, distances de visibilité, tailles écran min/max, `clampToScreen`, label + distance
affichée). **Gameplay/UI 3D** : noms de joueurs, barres de vie flottantes, marqueurs de quête.

**HUD.** `NkHUDCrosshair` (réticule : texture, couleur, `size`/`gap`/`thickness`, `dynamicSpread`),
`NkHUDMinimap` (minimap : texture, `size`/`range`, `screenPos`, `rotate`, icône joueur, `circular`).
**Gameplay** : FPS, jeux d'action.

**Events UI** (structs simples, **non composants** — dispatchés par l'UISystem) : `NkOnButtonClicked`
(`button`, `buttonId`, `tag`), `NkOnSliderChanged` (`slider`, `value`, `normalized`),
`NkOnToggleChanged` (`toggle`, `isOn`), `NkOnInputChanged` / `NkOnInputSubmitted` (`field`, `text`),
`NkOnDropdownChanged` (`dropdown`, `index`, `option`), `NkOnScrollChanged` (`scroll`, `position`).

> **Statut UI.** Les structures de données et les helpers inline (`Contains`, `Normalized`, `Click`,
> `AddOption`…) sont **implémentés**. En revanche toute la **logique de comportement** (layout,
> raycast, interaction, dispatch d'events) repose sur des systèmes cités en commentaire
> (`NkUILayoutSystem`, `UISystem`) **non fournis** dans ces headers → comportement = **spec**.
> Seule méthode déclarée sans corps : `NkUIText::SetTextf`.

---

### Exemple

```cpp
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/Rendering/NkRenderer.h"
#include "Noge/ECS/Components/NkComponentHandle.h"
using namespace nkentseu::ecs;

// Composer une entité : un Transform, un nom, un mesh rendu, une lumière.
NkTransform tr;
tr.SetLocalPosition(0.f, 0.f, 0.f);
tr.SetLocalRotationEuler(0.f, 45.f, 0.f);

NkName name("Hero");

NkRenderer3D mesh;
mesh.meshId = heroMeshId;
mesh.castShadow = NkShadowCastMode::On;

NkLight sun;
sun.type = NkLightType::Directional;
sun.color = nkentseu::ecs::NkColor4::White();
sun.intensity = 3.f;

// Attraper un composant en cours de jeu, sans crasher s'il manque.
nkentseu::NkComponentHandle<NkTransform> h(world, entityId);
if (h) h->Translate({0.f, 1.f, 0.f});   // sûr : on a testé bool d'abord
```

---

[← Index Noge](README.md) · [Récap Noge](../Noge.md) · [Couche Engine](../README.md)
