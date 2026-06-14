# La scène : contexte, nœuds, caméras et drawables

> Couche **Runtime** · NKRenderer · Décrire **ce qu'on rend, où et comment** : le contrat de frame
> `NkSceneContext`, le scene graph (`NkITransformable` / `NkSceneNode`), les caméras
> (`NkCamera` / `NkCamera3D` / `NkCamera2D` + contrôleur orbital), et l'interface de soumission
> `NkIDrawable`.

Un moteur de rendu ne « dessine » pas dans le vide : il lui faut savoir **où est la caméra**, **où
sont les objets**, **comment ils sont éclairés** et **dans quel ordre** les soumettre. Cette page
couvre la couche *Core* de NKRenderer — celle qui structure une scène 3D **avant** que le rendu
proprement dit ne commence. La logique est toujours la même : l'application **décrit** une frame
(un `NkSceneContext`), organise ses objets dans une **hiérarchie** de transforms (`NkSceneNode`),
les regarde à travers une **caméra**, et chaque objet **se soumet** lui-même au renderer
(`NkIDrawable`). Trois responsabilités bien séparées — placement, point de vue, soumission — qui se
combinent sans jamais se mélanger.

Le fil conducteur tient en une idée : **transformer et dessiner sont deux choses distinctes.**
Un nœud peut avoir une position dans le monde sans jamais rien dessiner (un point de pivot, un
groupe), et un objet peut être dessinable sans porter de transform propre. NKRenderer matérialise
cette séparation par deux interfaces *duales et indépendantes*, `NkITransformable` et `NkIDrawable`,
qu'on implémente l'une, l'autre, les deux, ou aucune.

- **Namespace** : `nkentseu::renderer`
- **Headers** : `NkSceneContext.h`, `NkITransformable.h`, `NkSceneNode.h`, `NkCamera.h`,
  `NkCameraController.h`, `NkIDrawable.h`

---

## Le contrat de frame : `NkSceneContext`

Avant de soumettre quoi que ce soit, le renderer a besoin d'un **état global de la frame** : où
regarde la caméra, quelles lumières sont actives, quel environnement (skybox, IBL), y a-t-il du
brouillard, quel est le temps écoulé. Plutôt que d'éparpiller ces réglages dans des dizaines
d'appels, NKRenderer les rassemble dans une **structure unique** que l'application remplit et passe
à `NkRender3D::BeginScene(ctx)`. C'est le *contrat d'entrée* d'une frame.

`NkSceneContext` est un **agrégat pur** : pas de méthodes, pas de constructeur déclaré, juste des
champs publics avec des valeurs par défaut sensées. On le construit par initialisation agrégée, on
règle ce qui change, on laisse le reste à ses défauts.

```cpp
NkSceneContext ctx;
ctx.camera = myCamera;                 // une NkCamera3D complète, COPIÉE chaque frame
ctx.lights = sceneLights;              // copie CPU des lumières au BeginScene
ctx.envMap = skyboxCubemap;
ctx.ibl    = iblHandle;
ctx.time      = clock.total;
ctx.deltaTime = clock.delta;
ctx.frameIdx  = frame++;
renderer.GetRender3D()->BeginScene(ctx);
```

Deux pièges à retenir. D'abord, `camera` est un `NkCamera3D` **par valeur**, pas un pointeur : le
contexte en prend une *copie* à chaque frame — pensez à y mettre l'état à jour de votre caméra, pas
une caméra périmée. Ensuite, `lights` est un `NkVector<NkLightDesc>` également **copié** au moment du
`BeginScene` : c'est un instantané CPU, modifier le vecteur source après coup n'affecte pas la frame
en cours.

Ce n'est **pas** un objet vivant qu'on garde et qu'on muterait au fil du rendu : c'est un *snapshot*
descriptif, jeté et reconstruit chaque frame. Et ce n'est **pas** l'endroit où l'on définit le
*viewMode* du pipeline par feature — `viewMode` ici est un simple drapeau de rendu global
(`NkViewMode::NK_SOLID` par défaut).

> **En résumé.** `NkSceneContext` = le contrat d'entrée d'une frame 3D, un agrégat de champs sans
> méthode, passé à `BeginScene`. `camera` (par valeur) et `lights` (copie CPU) sont *capturés* au
> moment de l'appel. Remplissez ce qui change, laissez les défauts faire le reste.

---

## Placer les objets : `NkTransform`, `NkITransformable`, `NkSceneNode`

Tout objet dans une scène a une **position, une orientation, une échelle** — et souvent un
**parent** (une roue tourne avec sa voiture, une main suit son bras). NKRenderer stocke cela sous
forme **TRS** (Translation-Rotation-Scale) compacte : `NkTransform` ne pèse que 32 octets (un
`NkVec3f`, un `NkQuatf`, un `NkVec3f`) là où une matrice 4×4 en coûte 64. La matrice n'est calculée
qu'au besoin, par `ToMatrix()`, dans l'ordre **T · R · S**.

```cpp
NkTransform t;                         // identité par défaut
t.translation = { 0.f, 1.f, 0.f };
t.rotation    = yawQuat;
t.scale       = { 2.f, 2.f, 2.f };
NkMat4f local = t.ToMatrix();          // T * R * S, column-major
```

`NkITransformable` est l'**interface pure** de tout ce qui a une place dans la scène : poser/lire la
transform locale, la position/rotation/échelle locales, remonter au parent, lire la **matrice
monde** (qui compose avec les ancêtres), et marquer l'arbre *dirty*. Elle ne dit rien de la manière
dont c'est stocké — elle décrit seulement le *contrat de placement*.

`NkSceneNode` en est l'**implémentation par défaut**, et la **base du scene graph** : c'est de lui
qu'héritent `NkStaticMesh`, `NkSkeletalMesh`, `NkLight`, `NkCamera`, `NkSprite`, `NkSceneGroup`.
Son apport central est la **hiérarchie paresseuse** : la matrice monde n'est *pas* recalculée à
chaque frame, mais seulement quand on la demande après un changement. Modifier la position met le
nœud *dirty* ; le prochain `GetWorldMatrix()` reconstruit la matrice et la met en cache ; les
appels suivants la relisent sans rien recalculer. Déplacer un parent propage le *dirty* à tous ses
enfants, qui se recalculeront à leur tour à la demande.

```cpp
NkSceneNode car;        car.SetName("car");
NkSceneNode wheel;      wheel.SetName("wheel");
car.AddChild(&wheel);   wheel.SetParent(&car);   // wheel suit désormais car

car.SetLocalPosition({ 10.f, 0.f, 0.f });        // marque car + enfants dirty
NkVec3f wheelWorld = wheel.GetWorldPosition();   // recalcul lazy, inclut car
```

Ce n'est **pas** un système qui *possède* ses objets : parent et enfants sont des **pointeurs bruts
non-owning**. `NkSceneNode` ne gère aucune durée de vie — c'est à vous de garder vivants les nœuds
référencés, et de retirer un enfant (`RemoveChild`) avant de le détruire. De même, `SetName` prend
un `const char*` non-owning : la chaîne doit rester valide tant que le nœud l'utilise. Enfin, un
`NkSceneNode` est **non-copiable** (copie supprimée) : on le manipule par pointeur, pas par valeur.

> **En résumé.** `NkTransform` = TRS compact (32 o), matrice T·R·S à la demande. `NkITransformable`
> = le contrat de placement (transform locale, parent, matrice monde). `NkSceneNode` = sa mise en
> œuvre et la racine du scene graph, avec **matrice monde paresseuse** et propagation du *dirty*.
> Parent, enfants et nom sont **non-owning** : à vous la durée de vie. Non-copiable.

---

## Le point de vue : `NkCamera`, `NkCamera3D`, `NkCamera2D`

Une caméra fournit deux matrices : la **vue** (où l'on est et où l'on regarde) et la **projection**
(comment le monde se projette à l'écran), dont le produit donne la **view-proj**. NKRenderer
construit ces matrices **paresseusement**, exactement comme le scene graph : un drapeau `mDirty`
est levé par chaque setter, et le premier `GetView()`/`GetProj()`/`GetViewProj()` reconstruit le
nécessaire avant de répondre. Comme ces getters sont `const` mais doivent pouvoir recalculer, les
matrices cachées sont `mutable`.

`NkCamera` est la **base abstraite** : elle tient les trois matrices, expose les getters lazy,
`Invalidate()` pour forcer un rebuild, et délègue le calcul réel à `RebuildImpl()` (implémenté par
chaque dérivé) et la production de l'UBO à `BuildUBO()`. On ne l'instancie pas directement — on
prend une de ses deux spécialisations.

`NkCamera3D` est la caméra **3D** : LookAt (position / cible / up), perspective ou orthographique,
champ de vision, aspect, near/far — et surtout le **frustum culling**. À chaque appel,
`IsAABBVisible(aabb)` et `IsSphereVisible(center, radius)` recalculent les **6 plans normalisés** du
frustum depuis la view-proj et testent si le volume englobant tombe dedans. C'est la première ligne
de défense de la performance : ne pas soumettre ce qui est hors champ.

```cpp
NkCamera3D cam;
cam.SetPosition({ 0.f, 2.f, 8.f });
cam.SetTarget({ 0.f, 1.f, 0.f });
cam.SetFOV(60.f);
cam.SetAspect(width, height);          // surcharge largeur/hauteur
cam.SetNearFar(0.1f, 500.f);

if (cam.IsSphereVisible(obj.center, obj.radius))
    obj.Submit(&renderer, ctx);        // sinon : culled, rien à faire
```

`NkCamera2D` est la caméra **2D** orthographique : centre, zoom, rotation, dimensions du viewport.
Au-delà des matrices, elle offre les conversions **écran ↔ monde** (`ScreenToWorld`,
`WorldToScreen`) — indispensables pour savoir quelle case de la carte le curseur survole, ou placer
un objet sous la souris — et `GetOrtho()`, un alias de la projection. Son `BuildUBO()` remplit la
partie ortho et **vide** les champs 3D.

L'idiome à respecter : **toujours passer par les setters** (qui marquent *dirty*), ou par
`Invalidate()` après une modification directe via `SetData()`. Et grâce à la spécialisation forte,
le compilateur **interdit** d'appeler `GetForward()` (3D) sur une `NkCamera2D` : les deux familles
ne partagent que ce qui a un sens commun.

> **En résumé.** `NkCamera` (base) fournit vue/proj/view-proj **paresseuses** + UBO. `NkCamera3D` =
> LookAt + perspective/ortho + **frustum culling** (`IsAABBVisible`/`IsSphereVisible`, 6 plans
> recalculés à l'appel). `NkCamera2D` = ortho centre/zoom/rotation + conversions **écran↔monde**.
> Modifiez via les setters (ils invalident) ; la 3D et la 2D sont fortement séparées.

---

## Piloter la caméra : `NkOrbitCameraController3D`

Une caméra sait *où regarder*, mais pas *comment réagir à la souris*. Le **contrôleur** comble ce
vide — sans jamais dépendre de NKEvent ni de NKWindow. C'est un **objet d'état pur** : l'application
traduit ses propres inputs (souris, gamepad, tactile) en appels `Rotate / Pan / Zoom / Move`, et le
contrôleur entretient un état (yaw, pitch, distance, cible) qu'il **applique** ensuite à une caméra.

`NkOrbitCameraController3D` modélise une **orbite en coordonnées sphériques** autour d'une cible :
on tourne (`Rotate`), on déplace le point visé (`Pan`, `MoveTarget`, `MoveCameraRelative`), on
approche/éloigne (`Zoom`). Le **pitch est clampé à ±1.553 rad (~89°)** pour éviter le *gimbal lock*
au zénith. C'est le contrôleur des viewers d'objet, des éditeurs, des inspecteurs 3D — partout où
l'on tourne *autour* de quelque chose.

```cpp
NkOrbitCameraController3D orbit;
orbit.SetCenter({ 0.f, 0.5f, 0.f }, 9.f, 0.f, -0.2f);  // cible, distance, yaw, pitch

// dans le code d'input (vous décidez la source) :
orbit.Rotate(mouseDX, mouseDY);        // dx/dy * vitesse, puis clamp du pitch
orbit.Zoom(wheelDelta);                // step>0 = zoom-in

// dans la frame :
orbit.Update(dt);                      // auto-orbit si activé
orbit.Apply(myCamera);                 // pousse position + cible dans la caméra
```

À noter : `Zoom(step)` est **multiplicatif** (`distance *= pow(zoomStep, step)`), borné par
`min/maxDistance` — un zoom qui « ralentit » naturellement près de l'objet. `Apply` ne touche
**que** la position et la cible de la caméra : il ne change ni le FOV, ni l'aspect, ni le near/far,
et **ne possède pas** la caméra. `SetCenter` mémorise aussi l'état de départ, que `Recenter()`
restaure d'un coup.

Ce contrôleur n'est **pas** une caméra *fly* (pas de déplacement libre en première personne) ni un
contrôleur 2D : **seule l'orbite 3D** est fournie ici. Toutes ses méthodes publiques sont *inline*
et **sans** `noexcept`.

> **En résumé.** `NkOrbitCameraController3D` = état d'orbite sphérique (yaw/pitch/distance/cible),
> piloté par `Rotate/Pan/Zoom/Move*` que **vous** alimentez depuis vos inputs, puis `Update(dt)` et
> `Apply(cam)`. Pitch clampé (~89°), zoom multiplicatif borné, `Apply` ne touche que position+cible.
> Pas de fly-cam, pas de 2D ici.

---

## Soumettre au rendu : `NkIDrawable`

Reste la question : comment un objet **se dessine** ? NKRenderer renverse la logique habituelle —
ce n'est pas le renderer qui sait dessiner chaque type d'objet, c'est **l'objet qui se soumet
lui-même**. L'interface `NkIDrawable` est ce contrat de soumission, **dual et indépendant** de
`NkITransformable` : transformer et dessiner sont séparés.

Un drawable annonce sa **catégorie** (`GetCategory`), dit s'il est **visible** (`IsVisible` — sinon
le scene graph saute sa soumission), fournit son **AABB monde** pour le frustum culling
(`GetWorldAABB`, ou `NkAABB::Infinite()` s'il ne sait pas se borner), indique s'il **projette une
ombre** (`CastsShadow`, par défaut `false`), et surtout construit ses `NkDrawCall*` dans
`Submit(renderer, ctx)`. `Submit` est appelé **après** `IsVisible()` et le frustum culling, et
route vers le bon sous-système (`renderer->GetRender3D()->Submit(...)`, etc.).

La catégorie pilote le **tri et le passage de rendu** auquel l'objet appartient. L'enum
`NkDrawableCategory` (sur `uint8`) compte exactement cinq valeurs :

```cpp
class GlassPane : public NkIDrawable {
public:
    NkDrawableCategory GetCategory() const noexcept override
        { return NkDrawableCategory::NK_DRAWABLE_TRANSPARENT; }   // verre → blend
    bool   IsVisible()    const noexcept override { return mVisible; }
    NkAABB GetWorldAABB() const noexcept override { return mAABB; }
    bool   CastsShadow()  const noexcept override { return true; }
    void   Submit(NkRenderer* r, const NkSceneContext& ctx) override {
        r->GetRender3D()->Submit(/* mes draw calls */);
    }
};
```

C'est l'idiome central de la couche : `NkITransformable` *place*, `NkIDrawable` *dessine*, et un
objet peut être l'un, l'autre, les deux (le cas courant : un mesh dans le graphe), ou ni l'un ni
l'autre. `Submit` est la seule méthode **non** `noexcept` de l'interface (elle peut allouer/échouer)
et `CastsShadow` est la seule **non pure** (implémentation par défaut fournie).

> **En résumé.** `NkIDrawable` = le contrat de soumission : catégorie, visibilité, AABB monde,
> ombre, et `Submit` qui construit et route les draw calls. Appelé **après** visibilité + culling.
> Dual de `NkITransformable` : transformer et dessiner sont indépendants. Cinq catégories sur
> `uint8`.

---

## Aperçu de l'API

Tous les symboles vivent dans `nkentseu::renderer`. Les types `NkMat4f`, `NkVec3f`, `NkVec2f`,
`NkQuatf`, `NkAABB`, `NkTexHandle`, `NkIBLHandle`, `NkLightDesc`, `NkViewMode`, `NkCameraUBO`,
`NkCamera3DData`, `NkCamera2DData` sont **définis ailleurs** (`NkRendererTypes.h`, NKMath,
NKContainers) — ici on ne fait que les utiliser.

### `NkSceneContext` — contrat de frame (agrégat)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Vue | `camera` (`NkCamera3D`, par valeur) | Point de vue de la frame (copié). |
| Lumière | `lights` (`NkVector<NkLightDesc>`) | Lumières actives (copie CPU au `BeginScene`). |
| Environnement | `envMap`, `ibl`, `iblIntensity`, `ambientIntensity`, `ambientColor` | Skybox, IBL (irradiance+GGX+BRDF LUT), intensités, ambiant. |
| Réflexion | `mirrorViewProj` (déf. `Identity`) | View-proj du reflet planaire (Identity = aucun reflet). |
| Temps | `time`, `deltaTime`, `frameIdx` | Horloge et index de frame. |
| Brouillard | `fogEnabled`, `fogColor`, `fogDensity`, `fogStart`, `fogEnd` | Activation et paramètres du fog. |
| Affichage | `viewMode` (déf. `NK_SOLID`) | Mode de rendu global. |

### `NkTransform` — TRS local

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `translation`, `rotation`, `scale` | TRS local (32 octets). |
| Méthodes | `ToMatrix()`, `static Identity()` | Matrice T·R·S column-major ; transform neutre. |

### `NkITransformable` — interface de placement (pure)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Transform locale | `Get/SetLocalTransform`, `SetLocalPosition/Rotation/Scale`, `GetLocalPosition/Rotation/Scale` | Lire/écrire la transform locale. |
| Monde | `GetWorldMatrix`, `GetWorldPosition` | Matrice/position monde (composées, cache lazy). |
| Hiérarchie | `GetParent`, `SetParent`, `MarkTransformDirty` | Parent et propagation du *dirty*. |

### `NkSceneNode : NkITransformable` — implémentation + scene graph

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | ctor/dtor, copie **supprimée** | Non-copiable ; manipulé par pointeur. |
| Overrides | toute l'API `NkITransformable` | Placement avec matrice monde **paresseuse**. |
| Graphe | `GetChildren`, `AddChild`, `RemoveChild` | Enfants (pointeurs **non-owning**). |
| Identité | `GetName`, `SetName` | Nom `const char*` **non-owning**. |

### `NkCamera` / `NkCamera3D` / `NkCamera2D` — caméras

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base (`NkCamera`) | `GetView/GetProj/GetViewProj`, `BuildUBO`, `Invalidate` | Matrices lazy, UBO, invalidation. |
| 3D — transform | `SetPosition/Target/Up`, `GetPosition/Target/Up`, `GetForward/Right` | LookAt et axes. |
| 3D — projection | `SetFOV/Aspect(×2)/NearFar/Ortho`, `GetFOV/Near/Far/Aspect/IsOrtho` | Perspective ou ortho. |
| 3D — culling | `IsAABBVisible`, `IsSphereVisible` | Frustum culling (6 plans à l'appel). |
| 3D — data | `Get/SetData`, `BuildUBO` | Accès au `NkCamera3DData`. |
| 2D — transform | `SetCenter/Zoom/Rotation/Viewport`, `GetCenter/Zoom/Rotation/Width/Height` | Ortho centre/zoom/rotation. |
| 2D — spécifiques | `GetOrtho`, `ScreenToWorld`, `WorldToScreen`, `Get/SetData`, `BuildUBO` | Alias proj + conversions écran↔monde. |

### `NkOrbitCameraController3D` — contrôleur orbital 3D

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Configuration | `SetCenter`, `Recenter` | État courant + état de reset. |
| Actions | `Rotate`, `Pan`, `Zoom`, `MoveTarget`, `MoveCameraRelative`, `Update`, `Apply` | Pilotage, mise à jour, application à une caméra. |
| État | `GetPosition/Target/Distance/Yaw/Pitch`, `IsAutoOrbit` | Lecture de l'état. |
| Sensibilité | `SetRotateSpeed/PanSpeed/ZoomStep/AutoOrbit/AutoOrbitSpeed/MinDistance/MaxDistance` | Réglages. |

### `NkDrawableCategory` / `NkIDrawable` — soumission au rendu

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum (`uint8`) | `NK_DRAWABLE_OPAQUE`(0), `_TRANSPARENT`(1), `_VFX`(2), `_2D`(3), `_DEBUG`(4) | Catégorie de tri/passe. |
| Interface | `GetCategory`, `IsVisible`, `GetWorldAABB`, `CastsShadow`, `Submit` | Décrire et soumettre l'objet. |

---

## Référence complète

### `NkSceneContext` à fond

C'est le **point d'entrée** de toute frame 3D : on le remplit, on le passe à `BeginScene`, le
renderer en fait son état de travail. Comme c'est un agrégat sans méthode, son intérêt est entièrement
dans la **sémantique de capture** de ses champs et dans les défauts choisis.

- **`camera` (par valeur).** Une `NkCamera3D` *copiée* dans le contexte. Conséquence pratique : la
  caméra que vous y mettez doit être à jour ; muter votre caméra source après la copie n'affecte pas
  la frame. La copie reste légère (la caméra ne porte que ses data + 3 matrices cachées).
- **`lights` (copie CPU).** Le `NkVector<NkLightDesc>` est *capturé* au `BeginScene` — c'est un
  instantané. **Rendu** : c'est ici qu'on décide combien de lumières éclairent la frame (directionnelle
  du soleil, points, spots). **Gameplay** : on n'ajoute au vecteur que les lumières *pertinentes*
  (proches, allumées) pour borner le coût.
- **Environnement** : `envMap` (cubemap skybox), `ibl` (irradiance + GGX préfiltré + BRDF LUT) avec
  `iblIntensity` (1 par défaut), et le repli ambiant `ambientIntensity` (0.15) × `ambientColor`
  (blanc). C'est ce qui donne l'éclairage *indirect* d'un PBR — un intérieur sombre baisse
  l'intensité IBL, un extérieur ensoleillé la monte.
- **`mirrorViewProj`** : la view-proj d'un **reflet planaire** (eau, miroir, sol poli). Laissée à
  `Identity()`, elle signale *pas de reflet* — on ne la remplit que pour une passe de réflexion.
- **Temps** : `time` (horloge cumulée, pour animer des shaders), `deltaTime` (pas de la frame),
  `frameIdx` (compteur, utile au TAA, au jittering, aux structures double-buffer).
- **Brouillard** : `fogEnabled` arme le fog ; `fogColor` (gris-bleu par défaut), `fogDensity`
  (exponentiel), `fogStart`/`fogEnd` (linéaire). **Ambiance** : un fog dense et coloré pose une
  atmosphère ; le linéaire sert surtout à masquer le *far plane*.
- **`viewMode`** : drapeau de rendu global (`NK_SOLID` par défaut) — défini ailleurs, simplement
  transporté ici.

Le contrat est *snapshot*, pas *live* : on ne garde pas un `NkSceneContext` entre frames pour le
muter, on en reconstruit un. C'est ce qui rend le rendu déterministe vis-à-vis de l'état décrit.

### `NkTransform` à fond

`NkTransform` est le **format de stockage** d'un placement : trois champs (`translation`, `rotation`
quaternion identité par défaut, `scale` unité), 32 octets contre 64 pour une mat4. Le quaternion
évite le *gimbal lock* et s'interpole proprement (SLerp). `ToMatrix()` *matérialise* le placement
dans l'ordre **T · R · S** (column-major) — l'ordre canonique : on met à l'échelle, on oriente, on
translate. `Identity()` renvoie la transform neutre. On garde les objets en TRS (compact, animable)
et on ne paie la matrice qu'au moment d'en avoir besoin (envoi GPU, composition).

### `NkITransformable` et `NkSceneNode` à fond

**L'interface** `NkITransformable` décrit *tout ce qui a une place* sans présumer du stockage :
transform locale (get/set), raccourcis position/rotation/échelle locales, **matrice monde** (qui
compose avec le parent, avec cache lazy), position monde, et la gestion de hiérarchie
(`GetParent`/`SetParent`/`MarkTransformDirty`). On l'implémente directement si l'on veut un placement
sur mesure ; sinon on hérite de `NkSceneNode`.

**L'implémentation** `NkSceneNode` apporte trois choses :

- **La matrice monde paresseuse.** `mWorldDirty` est levé par tout changement (setters locaux,
  `SetParent`, `MarkTransformDirty`) ; `GetWorldMatrix()` ne recalcule que si nécessaire et met en
  cache (`mWorldCached`, `mutable`). Sur une grande scène, c'est crucial : seuls les nœuds *modifiés*
  (et leur sous-arbre) recalculent. **Animation** : bouger un os marque sa branche dirty, le reste du
  squelette ne recalcule rien. **Gameplay** : un objet statique ne recalcule jamais sa matrice après
  la première frame.
- **Le scene graph.** `AddChild`/`RemoveChild`/`GetChildren` + `SetParent` tissent la hiérarchie.
  Déplacer un parent propage le *dirty* aux enfants. **Modélisation** : assembler une voiture
  (carrosserie → roues → enjoliveurs). **VFX** : attacher un émetteur de particules à une arme.
  **Caméra** : suspendre une caméra à un nœud qui suit le joueur.
- **L'identité.** `GetName`/`SetName` pour le debug, l'éditeur, la recherche de nœud.

Le **piège mémoire** est cardinal : parent, enfants et nom sont **non-owning**. `NkSceneNode` ne
détruit rien — pas d'ownership, pas de cycle de vie géré. À l'appelant de garder les nœuds vivants,
de retirer un enfant avant de le détruire, et de maintenir la chaîne `mName` valide. Le nœud est
aussi **non-copiable** (copie supprimée) : on le passe par pointeur, jamais par valeur.

### `NkCamera`, `NkCamera3D`, `NkCamera2D` à fond

**La base** `NkCamera` factorise le mécanisme *lazy* : trois matrices cachées (vue, proj, view-proj),
des getters `const` qui déclenchent un `Rebuild()` si `mDirty`, `Invalidate()` pour forcer, et deux
points d'extension — `RebuildImpl()` (le calcul, par dérivé) et `BuildUBO()` (le remplissage du
`NkCameraUBO` pour le GPU). On ne l'utilise jamais seule.

**La 3D** `NkCamera3D` couvre tout le viseur perspectif/ortho :

- **LookAt** : `SetPosition`/`SetTarget`/`SetUp` définissent le repère ; `GetForward`/`GetRight` le
  *calculent* (les autres getters sont de simples lectures de `mData`). **Caméra de jeu** : suivre une
  cible (target = joueur), reconstruire un repère pour orienter le tir.
- **Projection** : `SetFOV` (champ vertical en degrés), `SetAspect` (ratio direct *ou* largeur/hauteur
  via la surcharge — la seconde évite de calculer le ratio à la main), `SetNearFar`, `SetOrtho`
  (bascule perspective↔ortho avec une taille). Les getters miroir (`GetFOV`, `GetNear`, `GetFar`,
  `GetAspect`, `IsOrtho`) lisent `mData`.
- **Frustum culling** : `IsAABBVisible` et `IsSphereVisible` recalculent les **6 plans normalisés** du
  frustum depuis la view-proj **à chaque appel**, puis testent l'inclusion. C'est l'optimisation reine
  du rendu de scène : ne soumettre que le visible. **Rendu** : tester chaque mesh avant `Submit`.
  **Streaming** : décharger ce qui sort du champ. (Si vous testez en masse, le recalcul par appel
  invite à grouper les tests dans la même frame.)
- **UBO/data** : `BuildUBO(time, dt)` produit le bloc GPU ; `GetData`/`SetData` exposent le
  `NkCamera3DData` (`position, target, up, fovY, nearPlane, farPlane, aspect, ortho`) — `SetData`
  remplace tout et marque *dirty*.

**La 2D** `NkCamera2D` est ortho pure :

- **Réglage** : `SetCenter` (point regardé), `SetZoom`, `SetRotation` (degrés), `SetViewport`
  (dimensions en pixels). Getters miroir pour chacun.
- **Conversions écran↔monde** : `ScreenToWorld` et `WorldToScreen` font le pont entre pixels et
  coordonnées du monde 2D. **Outils/UI** : savoir quelle entité est sous le curseur, poser un objet
  sous la souris, dessiner une sélection. **Caméra 2D** : convertir le clic en case de carte.
- **`GetOrtho`** est un alias de `GetProj()`. `BuildUBO` remplit la partie ortho et **vide** les
  champs 3D. `GetData`/`SetData` exposent `NkCamera2DData` (`center, zoom, rotation, width, height`).

L'**idiome commun** : modifiez toujours via les setters (qui invalident) ou `Invalidate()` après un
`SetData()` ; les getters de matrices, bien que `const`, reconstruisent par `mutable`. La
**séparation forte** 3D/2D est volontaire : appeler `GetForward()` sur une `NkCamera2D` ne compile
pas — chaque famille n'expose que ce qui a du sens pour elle.

### `NkOrbitCameraController3D` à fond

Le contrôleur est un **état découplé des inputs** : il ne connaît ni clavier ni souris, c'est
l'application qui appelle ses actions. Cette indépendance est un choix d'architecture — le même
contrôleur sert au clavier, au gamepad, au tactile ou à un script de démo.

- **Configuration** : `SetCenter(target, distance, yaw, pitch)` fixe l'état courant *et* l'état de
  reset ; `Recenter()` restaure ce dernier. Pratique pour un bouton « recentrer la vue ».
- **Rotation** : `Rotate(dx, dy)` ajoute `dx/dy × vitesse` au yaw/pitch puis **clampe le pitch** à
  ±~89° — pas de retournement au pôle. C'est le drag souris d'un viewer.
- **Translation de la cible** : `Pan(dx, dy)` glisse la cible dans le **plan caméra** (droite/haut),
  à l'échelle `panSpeed × distance` (panoramique proportionnel à l'éloignement) ; `MoveTarget(delta)`
  translate la cible directement en **world-space** (sans facteur) ; `MoveCameraRelative(dx, dy, dz)`
  déplace la cible dans le **repère caméra** (`dx` strafe droite, `dz` avance planaire en XZ, `dy`
  élévation directe) — la base d'un déplacement type WASD autour de l'objet.
- **Zoom** : `Zoom(step)` est **multiplicatif** (`distance *= pow(zoomStep, step)`, `step > 0` =
  approcher), borné par `min/maxDistance` — un zoom au feeling naturel, qui ne traverse jamais
  l'objet ni ne s'envole.
- **Boucle** : `Update(dt)` fait tourner le yaw si l'**auto-orbit** est armé (présentation produit,
  tour de manège d'un menu) ; `Apply(cam)` pousse `GetPosition()` + cible dans la caméra — et **rien
  d'autre** (pas de FOV/near/far), sans posséder la caméra.
- **Lecture** : `GetPosition` calcule la position sphérique depuis yaw/pitch/distance/cible ;
  `GetTarget/Distance/Yaw/Pitch` et `IsAutoOrbit` lisent l'état. Les **setters de sensibilité**
  (`SetRotateSpeed`, `SetPanSpeed`, `SetZoomStep`, `SetAutoOrbit`, `SetAutoOrbitSpeed`,
  `SetMinDistance`, `SetMaxDistance`) ajustent le ressenti.

Les défauts livrent un viewer prêt à l'emploi (cible `{0, 0.5, 0}`, distance 9, pitch -0.2,
`rotateSpeed` 0.005, `panSpeed` 0.0015, `zoomStep` 0.88, bornes 0.5–200). Toutes les méthodes
publiques sont *inline* et **sans `noexcept`**. **Il n'y a ici qu'un contrôleur d'orbite 3D** — pas
de fly-cam première personne, pas de contrôleur 2D.

### `NkDrawableCategory` et `NkIDrawable` à fond

`NkIDrawable` inverse la responsabilité : l'objet **se soumet** au lieu d'être dessiné par un
dispatch central. C'est ce qui permet d'ajouter un nouveau type d'objet sans toucher au renderer.

- **`GetCategory`** range l'objet dans une des **cinq** catégories de tri/passe (enum sur `uint8`) :
  `NK_DRAWABLE_OPAQUE` (mesh 3D opaque, passe principale), `NK_DRAWABLE_TRANSPARENT` (blend trié
  back-to-front : verre, fumée), `NK_DRAWABLE_VFX` (particules, traînées, faisceaux),
  `NK_DRAWABLE_2D` (sprites, formes, UI superposée), `NK_DRAWABLE_DEBUG` (gizmos, axes, AABB de
  debug). C'est la catégorie qui détermine *dans quel ordre* et *avec quel état* l'objet est rendu.
- **`IsVisible`** : un court-circuit — si `false`, le scene graph **saute** `Submit`. Idéal pour
  masquer un objet sans le retirer du graphe (LOD, toggle de calque, culling logique).
- **`GetWorldAABB`** : la boîte englobante monde, base du **frustum culling**. Un objet qui ne sait
  pas se borner (ciel, post-effet plein écran) retourne `NkAABB::Infinite()` pour rester toujours
  visible.
- **`CastsShadow`** : seule méthode **non pure** (défaut `false`) — surchargez-la à `true` pour les
  objets qui contribuent aux *shadow maps* (les casters), en laissant le menu fretin hors de la passe
  d'ombre.
- **`Submit(renderer, ctx)`** : le cœur. Seule méthode **non `noexcept`** (elle construit des
  `NkDrawCall*`, peut allouer). Appelée **après** `IsVisible()` *et* le frustum culling, elle route
  vers le bon sous-système (`renderer->GetRender3D()->Submit(...)`, etc.). C'est ici que l'objet
  traduit son état + le `NkSceneContext` en commandes de dessin.

L'**idiome de dualité** est la clé de cette couche : `NkITransformable` *place*, `NkIDrawable`
*dessine*, et les deux sont **indépendants**. Un mesh de scène est *les deux*. Un nœud de pivot ou
un groupe est *seulement* transformable. Un post-effet plein écran est *seulement* drawable. Cette
orthogonalité garde le scene graph simple et le rendu extensible.

---

### Exemple

```cpp
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Core/NkSceneNode.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkCameraController.h"
using namespace nkentseu::renderer;

// 1. Scene graph : une voiture avec une roue attachée (pointeurs non-owning).
NkSceneNode car;   car.SetName("car");
NkSceneNode wheel; wheel.SetName("wheel");
car.AddChild(&wheel);
car.SetLocalPosition({ 10.f, 0.f, 0.f });          // marque car + wheel dirty
NkVec3f wheelWorld = wheel.GetWorldPosition();      // recalcul lazy, inclut car

// 2. Caméra orbitale pilotée par un contrôleur d'état.
NkCamera3D cam;
cam.SetFOV(60.f);
cam.SetAspect(width, height);
cam.SetNearFar(0.1f, 500.f);

NkOrbitCameraController3D orbit;
orbit.SetCenter({ 0.f, 0.5f, 0.f }, 9.f, 0.f, -0.2f);
orbit.Rotate(mouseDX, mouseDY);                     // input → état (pitch clampé)
orbit.Zoom(wheelDelta);                             // step>0 = zoom-in (borné)
orbit.Update(dt);                                   // auto-orbit éventuel
orbit.Apply(cam);                                   // position + cible seulement

// 3. Contrat de frame, soumission des objets visibles.
NkSceneContext ctx;
ctx.camera    = cam;                                // COPIÉE dans le contexte
ctx.lights    = sceneLights;                        // snapshot CPU
ctx.time      = clock.total;
ctx.deltaTime = dt;

renderer.GetRender3D()->BeginScene(ctx);
for (NkIDrawable* obj : drawables) {
    if (!obj->IsVisible()) continue;
    if (!cam.IsAABBVisible(obj->GetWorldAABB())) continue;   // frustum culling
    obj->Submit(&renderer, ctx);                              // l'objet se soumet
}
renderer.GetRender3D()->EndScene();
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
