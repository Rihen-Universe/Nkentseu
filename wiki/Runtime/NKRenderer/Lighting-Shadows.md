# Éclairage, ombres et culling

> Couche **Runtime** · NKRenderer · Les briques qui transforment des maillages en image éclairée :
> la passe **différée** `NkDeferredPass`, les ombres `NkShadowSystem` / `NkVirtualShadowMaps`
> (+ le packer d'atlas), le **culling** `NkCullingSystem`, et l'environnement/IBL, la réflexion
> planaire, le voxel AO.

Une fois qu'on sait dessiner un triangle, la vraie question du temps réel devient : **comment
éclairer toute une scène sans s'écrouler** ? Mille lumières, des ombres douces, un ciel qui se
reflète dans une flaque, et tout ça à 60 images par seconde. Aucune de ces fonctionnalités ne
s'invente au moment du *draw* : elles reposent sur des **sous-systèmes** qu'on initialise une fois,
qu'on alimente chaque frame, et qui produisent des *render targets* ou des textures que le shader
PBR ira lire. Cette page raconte ces sous-systèmes dans l'ordre où la lumière les traverse — du
G-buffer aux ombres, du culling à l'ambiance.

Un fil rouge tient toute la famille ensemble : **le cycle de vie est partout `Init(...)` /
`Shutdown()`** (jamais `Create`/`Destroy` ici), la mémoire passe par NKMemory en interne, et les
configurations sont des `struct` à champs publics avec des défauts raisonnables — on en modifie
deux ou trois et on laisse le reste. Ce n'est **pas** une API où l'on enchaîne des appels libres :
c'est une API d'**objets persistants** qu'on câble (`SetRenderer3D`, `RegisterToRenderGraph`) puis
qu'on nourrit.

- **Namespace** : `nkentseu::renderer`
- **Headers** : `NKRenderer/Passes/Deferred/NkDeferredPass.h`,
  `NKRenderer/Tools/Shadow/NkShadowSystem.h`, `NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h`,
  `NKRenderer/Tools/Shadow/NkShadowAtlasPacker.h`, `NKRenderer/Tools/Culling/NkCullingSystem.h`,
  `NKRenderer/Tools/Environment/NkEnvironmentSystem.h`,
  `NKRenderer/Tools/Reflection/NkPlanarReflectionSystem.h`,
  `NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.h`

---

## La passe différée : `NkDeferredPass`

Avec une seule lumière, on l'éclaire pixel par pixel au moment de dessiner chaque objet : c'est le
rendu *forward*. Mais ajoutez **mille lumières** et ce modèle explose — chaque pixel serait éclairé
autant de fois qu'il y a d'objets dessinés par-dessus, et chaque objet testé contre toutes les
lampes. Le rendu **différé** (*deferred*) coupe ce nœud en deux temps. D'abord la passe
*Geometry* : on dessine la géométrie **une seule fois**, mais au lieu de calculer sa couleur, on
écrit ses **propriétés de surface** dans un jeu de textures, le **G-buffer**. Ensuite la passe
*Lighting* : on parcourt l'écran pixel par pixel et on applique **toutes les lumières** sur ces
propriétés déjà rangées. Le coût de l'éclairage devient indépendant de la complexité géométrique.

`NkGBuffer` est ce jeu de cibles : `albedoMetallic` (la couleur de base + métallicité),
`normalRoughness` (la normale + rugosité), `emissiveAO` (l'émissif + occlusion ambiante),
`velocity` (le mouvement, pour le flou et le TAA), `depth` (la profondeur), et `lightAccum` — le
résultat HDR de l'accumulation. `NkDeferredConfig` décide de la **stratégie d'éclairage** : `tiledLighting`
découpe l'écran en tuiles (`tileSize`) et n'applique sur chaque tuile que les lampes qui la
touchent ; `clusteredLighting` étend l'idée en profondeur (`clusterSlicesZ`). On y active aussi
l'IBL (`ibl`), l'occlusion ambiante en espace écran (`ssao`), les réflexions écran (`ssr`).

```cpp
NkDeferredPass deferred;
deferred.Init(device, &graph, &texLib, width, height, { .tiledLighting = true, .ibl = true });
deferred.RegisterToRenderGraph();
// ... par frame :
deferred.SetCamera(cam, viewProj);
deferred.BeginGeometry(cmd);  /* dessiner les opaques */  deferred.EndGeometry(cmd);
deferred.ClearLights();
deferred.SubmitLights(lights, lightCount);
deferred.BeginLighting(cmd);  /* la passe full-screen */  deferred.EndLighting(cmd);
```

Ce n'est **pas** une passe qui s'auto-imbrique : `Begin*` et `End*` doivent être appairés à la main,
et `SubmitLight(s)` se place **entre** les deux phases, après un `ClearLights()` qui repart à zéro.
L'ordre compte — `SetCamera` d'abord, géométrie ensuite, lumières enfin.

> **En résumé.** `NkDeferredPass` dessine la géométrie une fois dans un **G-buffer**
> (`NkGBuffer`), puis applique toutes les lumières en espace écran — coût d'éclairage découplé de la
> géométrie. `NkDeferredConfig` choisit tiled/clustered + IBL/SSAO/SSR. Ordre manuel :
> `SetCamera` → `BeginGeometry`/`EndGeometry` → `ClearLights`/`SubmitLights` →
> `BeginLighting`/`EndLighting`.

---

## Les ombres : `NkShadowSystem` et `NkVirtualShadowMaps`

Une lumière sans ombre n'a pas de poids : c'est l'ombre qui ancre un objet au sol. La technique de
base est la *shadow map* — on rend la scène **depuis la lumière**, on garde la profondeur, et au
moment d'éclairer on compare : si un pixel est plus loin que ce que la lumière voyait, il est dans
l'ombre. Le problème de la lumière **directionnelle** (le soleil), c'est qu'elle couvre tout le
monde : une seule carte serait floue de près et gâchée de loin. La solution est le **CSM**
(*Cascaded Shadow Maps*) : on découpe le tronc de vue en tranches (`numCascades`) et on donne à
chacune sa propre carte, fine près de la caméra, large au loin.

`NkShadowSystem` est l'implémentation **mono-directionnelle** de cette idée. `NkShadowSystemConfig`
règle la `resolution` de l'atlas, le nombre de cascades, le `pcfMode` (le filtrage des bords), les
*biais* (`shadowBias`, `normalBias`) qui combattent l'auto-ombrage (*shadow acne*), et le partage
des cascades (`lambda`). Le `NkPCFMode` va de `NONE` (bords nets, *aliasés*) à `PCF3x3`/`PCF5x5`
(moyennage doux), `POISSON` (échantillonnage dispersé) et `PCSS` (ombres dont la pénombre s'élargit
avec la distance à l'occludeur — le plus réaliste, le plus coûteux). Le câblage
`SetRenderer3D(...)` est **obligatoire** : sans lui, le système n'a personne à itérer pour trouver
les objets qui projettent une ombre.

`NkVirtualShadowMaps` est le **remplaçant multi-lights**. Là où `NkShadowSystem` ne gère que le
soleil, NkVSM gère le directionnel (CSM), les **spots** et les **points** (une cubemap virtuelle de
6 faces), tous rangés dans un **atlas unique** `D32_FLOAT` ré-alloué chaque frame. Son entrée est
réduite à un seul appel par frame, `RenderAllShadows(cmd)`, qui alloue les slots, met à jour l'UBO
et rend tous les tiles. Deux pièges propres à NkVSM : l'UBO des slots est un **ring multi-frame**
(`framesInFlight`, défaut 3) pour qu'une frame en cours côté GPU ne lise pas ce que le CPU est en
train d'écrire (sinon, scintillement) ; et un **cache** par lumière (`NkLightShadowCache`) évite de
re-rendre une lampe statique — d'où les diagnostics `GetRenderedSlotsCount` / `GetCachedSlotsCount`.

Dans les deux systèmes, attention : la **taille d'atlas** (`resolution` / `atlasSize`) n'est **pas**
modifiable à chaud — elle est fixée à l'`Init`. Tout le reste de la config est re-uploadé à chaque
frame, donc *tweakable* en direct.

> **En résumé.** `NkShadowSystem` = ombres **CSM mono-directionnelles** (le soleil), filtrage
> `NkPCFMode`, biais anti-acné. `NkVirtualShadowMaps` = **multi-lights** (dir CSM + spot + point) dans
> un atlas `D32_FLOAT` unique, un seul `RenderAllShadows` par frame, UBO en **ring** anti-hazard,
> caching des lampes statiques. Les deux exigent `SetRenderer3D` ; la taille d'atlas est figée après
> `Init`.

---

## Le packer d'atlas : `NkShadowAtlasPacker`

NkVSM range des dizaines de tiles d'ombre — de tailles variées (1024 pour la cascade proche, 256
pour une face de point) — dans un seul grand atlas. Comment placer ces rectangles sans qu'ils se
chevauchent, et sans gâcher de place ? C'est un problème de **bin-packing**, et `NkShadowAtlasPacker`
le résout par l'algorithme **skyline** (*bottom-left, best-fit en y*) : il garde la « ligne
d'horizon » des segments occupés et glisse chaque nouveau rectangle au plus bas possible.

Son usage est délibérément simple : pas de constructeur élaboré, on appelle `Reset(w, h)` pour
(re)définir l'atlas, puis `Allocate(tileW, tileH, outRect)` pour chaque tile — il renvoie `true` et
remplit le `NkShadowTileRect` si ça rentre, `false` si c'est plein. `ToUV(rect)` traduit ensuite le
rectangle pixel en coordonnées `[0,1]` (minU, minV, maxU, maxV) que le shader utilisera pour
échantillonner le bon coin de l'atlas. Le point à retenir : **il n'y a pas de défragmentation**
(V0). On ne libère pas un tile au milieu ; on fait `Reset()` **chaque frame** et on repart de zéro.
Chaque `Allocate` est en `O(N)` sur le nombre de segments (≈ nombre de tiles).

> **En résumé.** `NkShadowAtlasPacker` place des rectangles d'ombre dans l'atlas par bin-packing
> skyline : `Reset(w,h)` puis `Allocate(...)` (→ `NkShadowTileRect`), `ToUV` pour le shader. **Pas de
> défrag** : on `Reset()` à chaque frame ; `Allocate` est `O(N)`.

---

## Le culling : `NkCullingSystem`

La frame la plus rapide est celle qu'on ne dessine pas. Avant même de penser éclairage, on **élimine
ce qui ne se verra pas** : ce qui est hors du champ de la caméra, derrière un mur, ou trop loin.
C'est le rôle de `NkCullingSystem`, qui combine quatre tests — *frustum* (dans le tronc de vue),
occlusion (caché par autre chose, via HZB), distance (au-delà de la portée) et *back-face* — et qui
les accélère avec un **octree** spatial interne.

Le modèle est un **registre persistant** : on déclare chaque objet une fois avec `Register(id, aabb,
maxDist, alwaysVisible)`, on suit ses mouvements avec `UpdateAABB(id, ...)`, on le retire avec
`Unregister(id)`. La `NkCullingConfig` active/désactive chaque test et fixe les bornes du monde et
les **seuils de LOD** (`lodDistances`) — car le culling fait d'une pierre deux coups : il dit non
seulement *si* un objet est visible, mais à quel **niveau de détail** le dessiner selon sa distance.

```cpp
NkCullingSystem culling;
culling.Init({ .frustumCulling = true, .distanceCulling = true });
culling.Register(entityId, aabb, 800.f);
// ... par frame :
culling.BeginFrame(cam, viewProj);
int32 lod;
if (culling.TestDrawCall(entityId, &lod) == NkCullResult::NK_VISIBLE)
    submit(entityId, lod);
```

`BeginFrame` calcule la visibilité de toute la frame ; ensuite on interroge au choix
`TestDrawCall(id, &lod)` (un objet, avec son `NkCullResult` et son LOD), `QueryVisible(outIds)` (la
liste des visibles), ou `FilterDrawCalls(dcs, count)` qui **filtre un tableau de drawcalls en place**
et renvoie le nombre survivants. Le `NkCullResult` ne dit pas seulement « visible / pas visible » :
il précise **pourquoi** un objet a été rejeté (`NK_CULLED_FRUSTUM`, `…OCCLUSION`, `…DISTANCE`,
`…BACKFACE`) — précieux pour déboguer une scène où il manque quelque chose.

> **En résumé.** `NkCullingSystem` rejette l'invisible (frustum + occlusion + distance + back-face,
> accéléré par octree) et choisit le **LOD**. Registre persistant `Register`/`UpdateAABB`/`Unregister`,
> puis `BeginFrame` chaque frame, puis `TestDrawCall` / `QueryVisible` / `FilterDrawCalls`.
> `NkCullResult` dit **pourquoi** un objet a sauté.

---

## L'environnement : `NkEnvironmentSystem` (IBL)

Une scène n'est pas éclairée que par des lampes : tout ce qui l'entoure — le ciel, les murs, le sol —
renvoie de la lumière. L'**IBL** (*Image-Based Lighting*) capture cet environnement dans des textures
et s'en sert comme d'une source de lumière ambiante crédible. `NkEnvironmentSystem` produit les trois
ingrédients du PBR : une **cubemap d'irradiance** (la lumière diffuse ambiante, moyennée dans toutes
les directions), une **cubemap préfiltrée** à plusieurs *mips* (les reflets, du net au flou selon la
rugosité), et une **BRDF LUT 2D** (la table qui combine les deux selon l'angle de vue). Les
convolutions sont faites côté **CPU**.

La source est choisie par `NkEnvSource` : `NK_ENV_PROCEDURAL` génère un ciel en dégradé à partir de
trois couleurs (`skyTop`, `horizon`, `ground`) — pratique et sans fichier ; `NK_ENV_HDR_FILE` charge
un `.hdr` équirectangulaire 360° depuis `hdrPath` ; `NK_ENV_NONE` n'auto-charge rien. On peut aussi
piloter à la main avec `LoadProcedural(...)` ou `LoadFromHDR(path)`. Les accesseurs livrent ensuite
les handles GPU au shader (`GetIrradianceCubemap`, `GetPrefilterCubemap`, `GetBRDFLUT`, leurs
samplers), plus `GetSkyEnvCube` : une cubemap dédiée à dessiner le **ciel** lui-même, en HDR brut
sans tonemap.

> **En résumé.** `NkEnvironmentSystem` calcule l'IBL : **irradiance** (ambiant diffus), **prefilter**
> (reflets multi-mips), **BRDF LUT** — depuis un ciel procédural (`skyTop`/`horizon`/`ground`) ou un
> `.hdr` 360°. `GetSkyEnvCube` sert à peindre le ciel.

---

## La réflexion planaire : `NkPlanarReflectionSystem`

Pour un miroir, un sol poli ou une flaque d'eau, l'IBL ne suffit pas : on veut le **reflet exact** de
la scène, pas une ambiance moyennée. La réflexion planaire le fait à l'ancienne et proprement : on
re-rend la scène avec une **caméra miroir** par rapport au plan, dans un *render target* dédié, qu'on
plaque ensuite sur la surface. `NkPlanarReflectionSystem` automatise tout ce ballet.

Le modèle est déclaratif : on `Register(desc)` un plan une fois (sa `normal`, son `point`, la taille
du RT, le `NkMatInstHandle` du matériau qui recevra le reflet et la matrice miroir), et on soumet ses
drawcalls **normalement** chaque frame — le système s'occupe du reste. Le `NkPlanarFaceMode` choisit
quelle(s) face(s) refléter (`FRONT_ONLY`, `BACK_ONLY`, `BOTH`), et `twoSided` alloue un second RT
pour la face arrière. Le travail réel se fait dans `RenderReflections(cmd, r3d)`, à appeler **entre**
`EndScene` et le *Flush* principal : il rejoue la queue de drawcalls miroir dans chaque RT et met à
jour le matériau. C'est un **no-op** si la scène n'est pas ouverte (`r3d->IsInScene()` faux) — un
garde-fou contre un appel hors séquence.

Note de lecture : l'en-tête contient une doc d'usage **aspirationnelle** (une syntaxe
`AddPlanarReflection` / *designated initializers*) ; l'API réellement déclarée est `Register(...)`.

> **En résumé.** `NkPlanarReflectionSystem` re-rend la scène en caméra miroir par plan :
> `Register(NkPlanarReflectionDesc)` une fois, puis `RenderReflections` entre `EndScene` et le Flush.
> `NkPlanarFaceMode` choisit la/les face(s) ; no-op hors scène. (`Register`, pas `AddPlanarReflection`.)

---

## Le voxel AO : `NkVoxelAOSystem`

Le SSAO assombrit les petits recoins, mais il ne voit que ce qui est à l'écran : il rate l'occlusion
**à longue portée**, l'ombre douce qu'un grand mur projette au loin. `NkVoxelAOSystem` comble ce
trou en **voxelisant** les gros occludeurs (des boîtes AABB) dans une texture **3D** `R8_UNORM`, que
le shader PBR échantillonne par *cone-tracing* pour une AO ambiante à grande échelle.

L'usage est en deux temps **explicites**. D'abord on déclare les occludeurs :
`RegisterOccluder(NkVoxelOccluder)` ou le raccourci `RegisterAABB(min, max, opacity)` — chaque appel
marque le volume *dirty* mais **n'uploade rien**. Ensuite, une fois tous déclarés, on appelle
`Build()` : c'est lui qui voxelise côté CPU et envoie la texture 3D au GPU (il renvoie `false` s'il
n'y a aucun occludeur ou si la texture est invalide). `NkVoxelAOConfig` borne le volume couvert
(`minBounds`/`maxBounds`) et sa résolution (`resX`/`resY`/`resZ`). `IsValid()` dit si la texture est
prête, et les accesseurs (`GetVoxelTexture`, `GetVoxelSampler`) la livrent au shader. Limitation de
cette v0 : la voxelisation est **statique** (au boot) et **basée AABB** (pas à la précision du
maillage).

> **En résumé.** `NkVoxelAOSystem` voxelise des AABB en texture 3D `R8_UNORM` pour une AO ambiante
> **longue portée** (cone-tracing). `RegisterOccluder`/`RegisterAABB` marquent *dirty*, puis `Build()`
> **explicite** voxelise et uploade. v0 : statique, basé AABB.

---

## Aperçu de l'API

Tous ces types sont au scope `nkentseu::renderer`. Les enums sont **au niveau du namespace** (pas
imbriqués) : on qualifie `NkPCFMode::PCF5x5`, `NkCullResult::NK_VISIBLE`, etc. Cycle de vie commun :
`Init(...)` / `Shutdown()`.

### Passe différée — `NkDeferredPass`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config | `NkDeferredConfig` | `maxLights`, `tileSize`, `tiledLighting`, `clusteredLighting`, `clusterSlicesZ`, `ibl`, `ssao`, `ssr`, `outputVelocity`, `emissiveScale` |
| G-buffer | `NkGBuffer` | Handles `albedoMetallic`/`normalRoughness`/`emissiveAO`/`velocity`/`depth`/`lightAccum` ; `IsValid()` |
| Cycle de vie | `Init`, `Shutdown`, `RegisterToRenderGraph`, `Resize` | Initialiser / fermer / inscrire dans le RenderGraph / redimensionner |
| Par frame | `SetCamera`, `BeginGeometry`/`EndGeometry`, `SubmitLight`/`SubmitLights`/`ClearLights`, `BeginLighting`/`EndLighting` | Caméra, passe Geometry, lumières, passe Lighting |
| Accès | `GetGBuffer`, `GetLightAccum`, `GetConfig`, `GetWidth`, `GetHeight`, `GetSubmittedLights` | G-buffer, résultat HDR, config, dimensions, nb de lumières soumises |

### Ombres — `NkShadowSystem`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Filtrage | `NkPCFMode` (`NONE`/`PCF3x3`/`PCF5x5`/`POISSON`/`PCSS`) | Qualité du bord d'ombre |
| Config | `NkShadowSystemConfig` | `resolution`, `numCascades`, `pcfMode`, plans, `lambda`, biais, `softness`, `sceneRadius`, `stable`, `visualize` |
| Cycle de vie | `Init`, `Shutdown`, `SetConfig`/`GetConfig`, `SetRenderer3D` | Initialiser / fermer / config live / **câblage obligatoire** |
| Passes | `BeginShadowPass`, `EndShadowPass`, `RenderShadowPasses` | Rendu des cascades depuis la lumière |
| Accès RHI | `GetAtlasTexture`, `GetAtlasSampler`, `GetAtlasRawSampler`, `GetShadowUBO`, `GetShadowRenderPass`, `GetCascadeMats` | Handles + matrices de cascade |

### Virtual Shadow Maps — `NkVirtualShadowMaps`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkVSMShadowQuality`, `NkVSMSlotType` (`DIR_CASCADE`/`SPOT`/`POINT_FACE`) | Qualité ; type de slot |
| Constantes | `kMaxShadowSlots`, `kMaxLightsShadow`, `kMaxCascades`, `kInvalidSlotIdx` | Limites + index invalide |
| Structs | `NkVirtualShadowMapsConfig`, `NkShadowSlot`, `NkLightShadowCache` | Config / slot CPU / cache per-light |
| Cycle de vie | `Init` (+ `framesInFlight`), `Shutdown`, `SetConfig`/`GetConfig`, `SetRenderer3D` | Init avec ring / config live / câblage |
| Par frame | `RenderAllShadows` | **Entry-point unique** : alloue slots, upload UBO, rend les tiles |
| Accès RHI | `GetAtlasTexture`, `GetAtlasSampler`, `GetAtlasRawSampler`, `GetShadowSlotsUBO`, `GetRingSize`, `GetRingBuffer`, `GetShadowRenderPass` | Atlas + ring d'UBO |
| Diagnostics | `GetActiveSlotCount`, `GetAtlasSize`, `GetRenderedSlotsCount`, `GetCachedSlotsCount` | Suivi du caching |

### Packer d'atlas — `NkShadowAtlasPacker`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Rectangle | `NkShadowTileRect` | `x`, `y`, `w`, `h`, `valid` |
| Opérations | `Reset`, `Allocate`, `ToUV` | (Re)définir l'atlas / placer un tile / convertir en UV `[0,1]` |
| Diagnostics | `GetAtlasWidth`, `GetAtlasHeight`, `GetAllocCount` | Dimensions + nb d'allocations |

### Culling — `NkCullingSystem`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Résultat | `NkCullResult` (`NK_VISIBLE`/`…FRUSTUM`/`…OCCLUSION`/`…DISTANCE`/`…BACKFACE`) | Visible, ou raison du rejet |
| Structs | `NkCullable`, `NkCullingConfig` | Objet enregistré / config (tests, LOD, bornes du monde) |
| Cycle de vie | `Init`, `Shutdown` | Initialiser / fermer |
| Registre | `Register`, `Unregister`, `UpdateAABB` | Maintenir le registre persistant |
| Par frame | `BeginFrame`, `TestDrawCall`, `QueryVisible`, `FilterDrawCalls` | Calculer la visibilité puis interroger / filtrer |
| Stats / debug | `GetLastFrameTotal`/`Visible`/`Culled`, `DrawDebugOctree` | Compteurs + visualisation de l'octree |

### Environnement / IBL — `NkEnvironmentSystem`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Source | `NkEnvSource` (`NK_ENV_PROCEDURAL`/`NK_ENV_HDR_FILE`/`NK_ENV_NONE`) | D'où vient l'environnement |
| Config | `NkEnvironmentConfig` | Tailles (irradiance/prefilter/BRDF), cache, `source`, couleurs ciel, `hdrPath` |
| Cycle de vie | `Init`, `Shutdown`, `LoadProcedural`, `LoadFromHDR` | Init / fermer / générer un ciel / charger un `.hdr` |
| Accès RHI | `GetIrradianceCubemap`, `GetPrefilterCubemap`, `GetBRDFLUT`, `GetEnvSampler`, `GetLUTSampler`, `GetSkyEnvCube` | Cartes IBL + samplers + cubemap de ciel |

### Réflexion planaire — `NkPlanarReflectionSystem`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Faces | `NkPlanarFaceMode` (`FRONT_ONLY`/`BACK_ONLY`/`BOTH`) | Quelle(s) face(s) refléter |
| Descripteur | `NkPlanarReflectionDesc` | `normal`, `point`, taille RT, `hdr`, `targetMaterial`, `faceMode`, `twoSided` |
| Handle | `NkPlanarReflectionHandle` | `idx` ; `IsValid()` |
| Cycle de vie | `Init`, `Shutdown` | Initialiser / fermer |
| Plans | `Register`, `Unregister`, `Clear`, `Size` | Gérer la liste de plans |
| Par frame | `RenderReflections` | Re-rendu miroir, entre `EndScene` et le Flush (no-op hors scène) |

### Voxel AO — `NkVoxelAOSystem`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Structs | `NkVoxelOccluder`, `NkVoxelAOConfig` | Occludeur AABB + opacité / bornes + résolution du volume |
| Cycle de vie | `Init`, `Shutdown`, `IsValid` | Initialiser / fermer / texture prête ? |
| Occludeurs | `RegisterOccluder`, `RegisterAABB`, `Clear`, `Build` | Déclarer (dirty) puis **`Build()` explicite** |
| Accès GPU | `GetVoxelTexture`, `GetVoxelSampler`, `GetConfig` | Texture 3D + sampler |

---

## Référence complète

### `NkDeferredPass` à fond

**Le G-buffer.** `NkGBuffer` est le cœur du procédé : au lieu d'une couleur, la passe *Geometry*
écrit dans plusieurs cibles les données dont l'éclairage aura besoin. `albedoMetallic` (RGBA8) porte
la couleur de base et la métallicité ; `normalRoughness` (RGBA16F) la normale encodée et la rugosité
; `emissiveAO` (RGBA16F) l'émission propre et l'AO ; `velocity` (RG16F) le déplacement de chaque
pixel entre deux frames ; `depth` (D32S8) la profondeur ; et `lightAccum` (RGBA16F) reçoit le
résultat HDR. `IsValid()` vérifie que les cibles essentielles sont là (la `velocity` est exclue du
test, car optionnelle). On lit ces cibles via `GetGBuffer()` / `GetLightAccum()` — pour les chaîner
dans une passe de post-traitement, par exemple.

**La stratégie d'éclairage.** `NkDeferredConfig` arbitre entre *tiled* et *clustered*. Le *tiled*
(`tiledLighting`, `tileSize`) découpe l'écran en tuiles et n'évalue par tuile que les lampes qui la
chevauchent — c'est ce qui rend les milliers de lumières (`maxLights`) abordables. Le *clustered*
(`clusteredLighting`, `clusterSlicesZ`) ajoute des tranches en profondeur, mieux pour les scènes avec
beaucoup de variation en Z. `outputVelocity` arme la cible de mouvement (pour le TAA et le *motion
blur*), `emissiveScale` règle l'intensité de l'émissif.

Cas d'usage, par domaine :
- **Rendu** — le scénario nominal : des centaines de point lights dans un niveau d'intérieur, un
  G-buffer plein écran, l'éclairage en une passe.
- **ECS / scène** — les systèmes de gameplay poussent leurs lumières dynamiques via
  `SubmitLights(buffer, count)` à partir d'un tableau de composants `Light` ; `ClearLights` à chaque
  début de frame.
- **UI / outils** — `GetGBuffer()` permet à un éditeur d'afficher les canaux séparément (vue albedo,
  vue normale, vue velocity) pour déboguer un matériau.
- **Post-traitement** — `velocity` alimente le flou de mouvement ; `lightAccum` HDR part vers le
  tonemapping/bloom.

### `NkShadowSystem` à fond

Le filtrage `NkPCFMode` est le curseur qualité/coût de l'ombre. `NONE` ne filtre pas (bords en
escalier) ; `PCF3x3`/`PCF5x5` moyennent un petit voisinage pour adoucir ; `POISSON` disperse les
échantillons selon un disque de Poisson (bruit moins structuré) ; `PCSS` simule une **vraie
pénombre** dont la largeur croît avec la distance entre l'occludeur et le receveur — d'où les deux
samplers exposés, un en mode comparaison (`GetAtlasSampler`) pour le PCF, un *raw* non-comparaison
(`GetAtlasRawSampler`) pour la recherche de bloqueur du PCSS.

Les **biais** sont la partie délicate. `shadowBias` décale la profondeur comparée pour éviter qu'une
surface ne s'auto-ombrage (*acne*) ; `normalBias` décale le long de la normale ; un biais trop fort
décolle l'ombre du pied de l'objet (*peter-panning*). Tout cela est *live-tweakable* : `GetConfig()`
renvoie une référence mutable, et la config est re-uploadée à chaque `RenderShadowPasses` — **sauf la
résolution**, figée à l'`Init` (changer `resolution` ensuite n'a aucun effet, c'est un piège
classique). Le système peut tourner en **mode stub** (atlas 1×1, `cascadeCount=0` → ombre = 1.0,
c.-à-d. tout est éclairé), utile pour isoler un bug. `GetCascadeMats(&n)` rend le pointeur sur les
matrices de cascade et écrit leur nombre dans `n` — c'est ce que le shader utilise pour choisir la
bonne cascade par pixel.

Cas d'usage, par domaine :
- **Rendu** — le soleil d'une scène extérieure, ombres nettes près du joueur, larges au loin.
- **Outils / éditeur** — `visualize` colore les cascades pour régler `lambda` et le `sceneRadius`.
- **Gameplay** — `stable` stabilise l'ombre quand la caméra bouge (sinon les bords « rampent »),
  important pour un jeu à la 3e personne.

### `NkVirtualShadowMaps` à fond

NkVSM généralise tout : un seul atlas `D32_FLOAT` (`atlasSize`, carré) accueille les cascades
directionnelles, les spots et les **six faces** d'un point (`NkVSMSlotType::POINT_FACE`). La config
décrit chaque taille de tile : `cascadeBaseTile` (et la décroissance `baseTile / (1<<i)`), `spotTile`,
`pointFaceTile`, plus le rayon fixe par cascade (`cascadeFixedRadius`, `useFixedCascadeRadius`) pour
une stabilité parfaite. Les constantes bornent le système : jusqu'à `kMaxShadowSlots` (256) slots,
`kMaxLightsShadow` (32) lumières ombrées, `kMaxCascades` (4), et `kInvalidSlotIdx` marque l'absence.

Un `NkShadowSlot` est l'unité de travail CPU : il porte sa `shadowMatrix` (pour l'échantillonnage) et
sa `renderMatrix` (pour le rendu du tile), son `tileRect`/`tileUV` dans l'atlas, la position ou
direction de la lumière (`lightPosOrDir`, dont le `.w` encode la portée/le *split*), son type
(`slotType`), son `subIdx` (numéro de cascade ou de face) et un drapeau `cached`. Le **cache**
(`NkLightShadowCache`) retient la dernière position/direction/portée d'une lampe et si elle a déjà été
rendue : une lampe immobile et statique n'est pas re-rendue, ce que mesurent `GetRenderedSlotsCount`
(rendus cette frame) et `GetCachedSlotsCount` (réutilisés).

Le **ring d'UBO** est le point d'ownership critique : comme `RenderAllShadows` réécrit les slots
chaque frame, écrire dans le buffer que le GPU est en train de lire produirait du **scintillement**.
La solution est `framesInFlight` (défaut 3) buffers en rotation : `GetRingSize()` les compte,
`GetRingBuffer(i)` y accède (borné), `GetShadowSlotsUBO()` rend celui de la frame courante. NkRender3D
bind chacun sur son *descriptor set* respectif. Et comme toujours, `SetRenderer3D(...)` est
obligatoire, et `atlasSize` n'est pas modifiable à chaud (re-init requise).

Cas d'usage, par domaine :
- **Rendu** — une scène avec un soleil **et** des dizaines de torches/spots, toutes ombrées sans
  multiplier les passes manuelles.
- **Gameplay / IA** — un projecteur qui suit un PNJ projette son ombre dynamique ; le cache laisse
  les lampes décoratives statiques tranquilles.
- **Threading** — le ring multi-frame est précisément ce qui permet au CPU de préparer la frame N+1
  pendant que le GPU rend la frame N sans corruption.

### `NkShadowAtlasPacker` à fond

Le packer est une petite pièce mais une pièce **fréquente** (appelée pour chaque tile, chaque frame).
Le `NkShadowTileRect` (`x,y,w,h,valid`) est le résultat d'une `Allocate` réussie. Le contrat est
strict : pas d'`Init`, on commence par `Reset(atlasW, atlasH)` qui vide les segments, puis on
`Allocate` autant de fois que de tiles ; quand l'atlas est plein, `Allocate` renvoie `false` (au
caller de gérer — réduire la taille des tiles, sauter une lampe). `ToUV(rect)` est l'étape qui relie
le monde pixel au monde shader : un `NkVec4f` (minU, minV, maxU, maxV) dans `[0,1]`. Les diagnostics
(`GetAllocCount`, dimensions) servent surtout au debug d'un atlas saturé.

Cas d'usage, par domaine :
- **Rendu** — l'unique consommateur direct : NkVSM le pilote pour ranger ses tiles.
- **Outils** — un visualiseur d'atlas peut rejouer les `Allocate` pour dessiner l'occupation.
- **Généralité** — l'algorithme skyline n'a rien de spécifique aux ombres : tout sous-système qui
  doit empaqueter des rectangles dans une texture (atlas de glyphes, de lightmaps) suit le même
  schéma `Reset`/`Allocate`/`ToUV`.

### `NkCullingSystem` à fond

Le `NkCullResult` est plus riche qu'un booléen : `NK_VISIBLE` ou l'une des quatre raisons de rejet.
`NK_CULLED_FRUSTUM` (hors champ), `NK_CULLED_OCCLUSION` (caché — nécessite `occlusionCulling` et une
passe de profondeur HZB), `NK_CULLED_DISTANCE` (au-delà de `maxDrawDist`), `NK_CULLED_BACKFACE`. Cette
granularité change le débogage : si un objet manque, on sait *quel test* l'a éliminé.

Le `NkCullable` décrit un objet enregistré : son `id`, son `aabb`, ses distances de dessin min/max, et
`alwaysVisible` pour les objets qu'on ne veut **jamais** culler (le ciel, un objet scripté). La
`NkCullingConfig` active chaque test indépendamment et fixe les `lodDistances` — quatre seuils qui
font correspondre la distance caméra à un niveau de détail, renvoyé par le `outLOD` de `TestDrawCall`.
Les bornes du monde (`worldX..worldD`) dimensionnent l'octree interne `NkOctree<NkCullable>` qui
accélère les requêtes spatiales.

L'idiome se découpe nettement : **maintenance** du registre (`Register` à la création d'un objet,
`UpdateAABB` quand il bouge, `Unregister` à sa destruction), puis **chaque frame** `BeginFrame(cam,
viewProj)` qui calcule tout, puis **interrogation**. Trois portes de sortie : `TestDrawCall(id)` pour
un objet précis avec son LOD, `QueryVisible(out)` pour récupérer la liste, ou `FilterDrawCalls(dcs,
count)` qui **compacte un tableau de `NkDrawCall3D` en place** et renvoie le compte survivant — la
forme la plus directe pour brancher le culling juste avant la soumission.

Cas d'usage, par domaine :
- **Rendu** — `FilterDrawCalls` réduit la liste de draws avant de l'envoyer au RHI.
- **ECS** — `Register`/`UpdateAABB` se branchent sur le cycle de vie des entités à composant
  *Renderable* ; `UpdateAABB` au changement de transform.
- **Gameplay / IA** — `QueryVisible` répond aussi à des questions de logique (« quels ennemis sont à
  l'écran ? ») et le LOD pilote la simulation détaillée vs simplifiée.
- **Outils / éditeur** — `DrawDebugOctree(overlay)` dessine la subdivision spatiale, et
  `GetLastFrameTotal/Visible/Culled` alimentent un compteur de débogage.

### `NkEnvironmentSystem` à fond

L'IBL repose sur trois textures précalculées. La **cubemap d'irradiance** (`GetIrradianceCubemap`,
petite, `irradianceSize`) est l'intégrale de la lumière entrante dans toutes les directions : c'est
l'ambiant diffus, ce qui empêche les faces non éclairées par une lampe d'être totalement noires. La
**cubemap préfiltrée** (`GetPrefilterCubemap`, `prefilterSize` × `prefilterMips`) stocke les reflets à
plusieurs niveaux de flou : un matériau lisse lit le mip 0 (net), un matériau rugueux un mip élevé
(flou). La **BRDF LUT** (`GetBRDFLUT`, `brdfLUTSize`) est la table 2D qui, selon l'angle de vue et la
rugosité, dit comment combiner les deux — c'est la part « split-sum » de l'IBL spéculaire.

La source est flexible. `NK_ENV_PROCEDURAL` génère un dégradé à trois bandes (`skyTop`, `horizon`,
`ground`) sans dépendre d'aucun fichier — parfait pour démarrer ou pour un ciel stylisé.
`NK_ENV_HDR_FILE` charge un panorama `.hdr` équirectangulaire (`hdrPath`) et en fait toutes les
convolutions côté CPU. `NK_ENV_NONE` laisse l'app piloter à la main via `LoadProcedural` /
`LoadFromHDR`. `enableCache` / `cacheDir` évitent de recalculer les convolutions à chaque lancement.
`GetSkyEnvCube` est à part : une cubemap HDR brute (RGBA32F, sans tonemap) destinée à **dessiner** le
ciel à l'écran, distincte des cartes servant à éclairer.

Cas d'usage, par domaine :
- **Rendu** — l'IBL est la base de la lumière ambiante PBR, indispensable au rendu métallique
  crédible.
- **Outils / éditeur** — basculer entre ciel procédural et `.hdr` pour comparer un éclairage ; régler
  `skyTop`/`horizon`/`ground` en direct.
- **IO** — `LoadFromHDR` ouvre un fichier `.hdr` ; le cache écrit/relit des fichiers de convolution.
- **2D / fond** — `GetSkyEnvCube` peint un arrière-plan de ciel cohérent avec l'éclairage 3D.

### `NkPlanarReflectionSystem` à fond

Le `NkPlanarReflectionDesc` définit un miroir : le plan (`normal`, `point`), la taille du *render
target* (`rtWidth`, `rtHeight`), s'il est HDR, le `NkMatInstHandle` du matériau qui recevra
automatiquement la texture de reflet **et** la matrice `mirrorViewProj` (si le handle est invalide, le
système rend quand même dans le RT mais ne *binde* rien — utile pour récupérer la texture soi-même).
Le `NkPlanarFaceMode` choisit la face reflétée : `FRONT_ONLY`, `BACK_ONLY`, ou `BOTH` (qui force le
*two-sided* automatiquement). `twoSided` alloue un second RT pour la face arrière. Le
`NkPlanarReflectionHandle` (`idx`, `IsValid()`) identifie un plan enregistré.

Le flux est « enregistre une fois, oublie ensuite ». `Register(desc)` ajoute le plan ;
`Unregister(handle)` / `Clear()` le retirent ; `Size()` les compte. Le travail réel,
`RenderReflections(cmd, r3d)`, se place **entre** `EndScene` et le Flush principal : pour chaque plan,
le système calcule la caméra miroir, choisit le côté selon la position de la vraie caméra, rejoue la
queue de drawcalls dans le RT et met à jour le matériau cible. C'est un **no-op** si la scène n'est
pas ouverte (`r3d->IsInScene()` faux), ce qui évite de rendre hors séquence. Rappel : la syntaxe
`AddPlanarReflection` mentionnée dans l'en-tête est aspirationnelle — l'API réelle est `Register`.

Cas d'usage, par domaine :
- **Rendu** — le cas d'école : un sol poli, un lac, une vitre qui doit refléter la scène exacte.
- **Gameplay** — un miroir d'énigme où le joueur doit voir un objet « derrière » lui.
- **Outils** — `Size()` et les handles permettent à un éditeur de lister et désactiver les plans de
  réflexion pour mesurer leur coût (chaque plan re-rend la scène).

### `NkVoxelAOSystem` à fond

Le voxel AO capture l'occlusion **que le SSAO ne voit pas** : l'écran ne contient pas ce qui est
hors-champ ou derrière, mais un grand mur assombrit pourtant tout un coin de pièce. La parade est de
voxeliser les gros volumes dans une grille 3D. Le `NkVoxelOccluder` est une boîte (`minWorld`,
`maxWorld`) avec une `opacity` (1.0 = opaque, 0.5 = semi-transparent). La `NkVoxelAOConfig` borne le
volume couvert (`minBounds`/`maxBounds`) et sa finesse (`resX`/`resY`/`resZ`) : plus de voxels = plus
de précision, plus de mémoire.

Le cycle est **explicitement en deux temps**, c'est le piège à connaître. `RegisterOccluder(occ)` (ou
le raccourci `RegisterAABB(min, max, opacity)`) **n'uploade rien** : il ajoute l'occludeur et marque
le volume *dirty*. `Clear()` vide et marque dirty aussi. C'est seulement `Build()` qui voxelise côté
CPU et envoie la texture 3D `R8_UNORM` au GPU — à appeler **une fois**, après avoir déclaré tous les
occludeurs (il renvoie `false` s'il n'y en a aucun ou si la texture est invalide). `IsValid()` indique
que la texture est prête ; `GetVoxelTexture` / `GetVoxelSampler` la livrent au shader PBR, qui
l'échantillonne par cone-tracing. Limitation v0 : voxelisation **statique** (au boot) et **basée
AABB** (pas à la précision du maillage).

Cas d'usage, par domaine :
- **Rendu** — l'AO ambiante longue portée qui complète le SSAO de proximité, pour des intérieurs
  crédibles.
- **Outils / niveau** — on déclare les gros blocs d'architecture (murs, plafonds) comme occludeurs au
  chargement du niveau, puis `Build()`.
- **Généralité** — `RegisterAABB`/`Build` reste utile à tout système qui veut une représentation
  voxel grossière d'occludeurs statiques (requêtes de visibilité approchées, par exemple).

### Le socle commun

- **Cycle de vie unifié.** Partout `Init(...)` / `Shutdown()` (jamais `Create`/`Destroy` ici), la
  mémoire venant de NKMemory en interne. `~NkPlanarReflectionSystem` appelle `Shutdown()` tout seul ;
  les autres destructeurs sont déclarés comme filet, mais l'appel explicite de `Shutdown()` reste
  l'usage normal.
- **Câblage `SetRenderer3D`.** Obligatoire pour `NkShadowSystem` et `NkVirtualShadowMaps` : sans
  `NkRender3D*`, ils n'ont aucune liste d'objets à itérer (set par `NkRendererImpl`).
- **Atlas figé.** Pour les deux systèmes d'ombres, la taille d'atlas (`resolution` / `atlasSize`)
  n'est **pas** *live* ; tout le reste de la config est re-uploadé par frame.
- **Enums au namespace.** Aucun enum n'est imbriqué dans une classe : on qualifie `NkPCFMode::`,
  `NkCullResult::`, `NkEnvSource::`, etc., directement sous `nkentseu::renderer`.

---

### Exemple

```cpp
#include "NKRenderer/Passes/Deferred/NkDeferredPass.h"
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Culling/NkCullingSystem.h"
#include "NKRenderer/Tools/Environment/NkEnvironmentSystem.h"
using namespace nkentseu::renderer;

// Init (une fois) — passe différée + ombres multi-lights + culling + IBL.
NkDeferredPass deferred;
deferred.Init(device, &graph, &texLib, W, H, { .tiledLighting = true, .ibl = true });
deferred.RegisterToRenderGraph();

NkVirtualShadowMaps vsm;
vsm.Init(device, &meshSys, &matSys, { .quality = NkVSMShadowQuality::PCF5x5 }, /*framesInFlight*/3);
vsm.SetRenderer3D(&render3D);            // câblage obligatoire

NkCullingSystem culling;
culling.Init({ .frustumCulling = true, .distanceCulling = true });

NkEnvironmentSystem env;
env.Init(device, { .source = NkEnvSource::NK_ENV_PROCEDURAL });

// Par frame.
culling.BeginFrame(cam, viewProj);
uint32 visible = culling.FilterDrawCalls(drawCalls, drawCallCount);   // compacte en place

deferred.SetCamera(cam, viewProj);
deferred.BeginGeometry(cmd);  /* dessiner les 'visible' opaques */  deferred.EndGeometry(cmd);

vsm.RenderAllShadows(cmd);     // toutes les ombres en un appel

deferred.ClearLights();
deferred.SubmitLights(lights, lightCount);
deferred.BeginLighting(cmd);  /* l'éclairage lit le G-buffer + l'atlas d'ombres + l'IBL */
deferred.EndLighting(cmd);
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
