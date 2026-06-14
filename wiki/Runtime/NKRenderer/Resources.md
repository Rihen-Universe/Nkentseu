# Les ressources GPU

> Couche **Runtime** · NKRenderer · Tout ce qui **vit en mémoire vidéo** : les ressources
> partagées de base (`NkResources`), la bibliothèque de textures (`NkTextureLibrary`), les
> assets sérialisables (`NkTextureAsset`), le système de maillages (`NkMeshSystem`) et le
> streaming sous budget (`NkStreamingSystem`).

Un moteur de rendu ne dessine jamais à partir de fichiers : il dessine à partir d'**objets GPU**
— des textures, des buffers de sommets, des descriptor sets. Le travail invisible mais central
consiste à **faire passer** les données du disque ou du CPU vers la mémoire vidéo, à les y
**garder le temps utile**, et à les **partager** entre tous les sous-systèmes sans les recréer dix
fois. C'est exactement ce que couvre cette famille « Resources » de NKRenderer. La question n'est
pas « comment je dessine » (c'est le rôle de NkRender3D), mais « **qui possède quoi en VRAM, et
comment j'y accède** ».

Le fil conducteur tient en une idée : il y a une **source unique** des ressources de base. `NkResources`
crée une fois pour toutes les textures par défaut (un pixel blanc, un pixel noir…), les samplers
standards et les descriptor set layouts UE-style. Tout le reste — la bibliothèque de textures, les
maillages, le streaming — **consomme** ces ressources au lieu de les dupliquer. On évite ainsi la
fragmentation et les incohérences (deux « blanc 1×1 » différents qui traînent en mémoire).

Un point de vocabulaire à fixer tout de suite, car c'est le piège récurrent : il existe **deux
niveaux de handles de texture**. `NkTextureHandle` est le handle **RHI brut** (la ressource GPU
nue, exposée par `NkResources`) ; `NkTexHandle` est le **wrapper de la bibliothèque** (exposé par
`NkTextureLibrary` et `NkTextureAssetIO`, il porte mips, sampler, métadonnées). On passe de l'un à
l'autre via `NkTextureLibrary::GetRHIHandle()`. Ce n'est **pas** une simple question de nom : ce
sont deux abstractions distinctes.

- **Namespace** : `nkentseu::renderer`
- **Headers réels** : `NkResources.h`, `NkTextureLibrary.h`, `NkTextureAsset.h`, `NkMeshSystem.h`,
  `NkStreamingSystem.h`

---

## La source unique : `NkResources`

`NkResources` est l'objet qu'on crée **une seule fois** au démarrage, après avoir un `NkIDevice`
valide, et qu'on garde vivant tant que le rendu tourne. Son `Init(device)` fabrique tout ce dont
les autres systèmes ont besoin en commun ; `Shutdown()` libère le tout ; `IsReady()` dit si l'objet
est utilisable. Il est **non-copiable** (copie supprimée explicitement) : c'est une ressource
unique, pas une valeur qu'on recopie.

Ce qu'il fournit se range en quatre familles. D'abord les **textures par défaut 1×1** : un blanc,
un noir, une normale (0.5, 0.5, 1, 1 — la normale « plate » d'un *normal map* neutre), un gris
(0.5, 0.5, 0.5, 1), et un magenta — ce dernier sert de **marqueur de texture manquante**, ce rose
criard qu'on voit dans tous les moteurs quand un asset n'a pas chargé. Toutes ces fonctions sont
`const noexcept` et renvoient un `NkTextureHandle` (handle RHI brut). Ensuite les **samplers**
standards (linéaire/nearest, repeat/clamp/border, anisotropique 16×, un sampler d'ombre PCF, un
sampler cubemap tri-linéaire). Puis les **descriptor set layouts** correspondant aux quatre sets de
l'architecture (Frame, Object, Material, PostProcess). Enfin les **fabriques de buffers**.

```cpp
NkResources res;
if (res.Init(device) != /* succès */) { /* abandon */ }

NkTextureHandle white = res.GetWhiteTex();              // pixel blanc partagé
NkSamplerHandle samp  = res.GetSamplerAnisotropic16();  // sampler standard
NkBufferHandle  cam   = res.CreateUBO(sizeof(CameraData), "CameraUBO");
```

Ce n'est **pas** une bibliothèque de textures : `NkResources` ne charge aucun fichier, ne gère
aucun cache d'assets. Il ne contient que les ressources **fondamentales et partagées**. Le
chargement depuis le disque, c'est `NkTextureLibrary`.

> **En résumé.** `NkResources` = la **source unique** des textures par défaut, samplers,
> descriptor set layouts et fabriques de buffers. Créé une fois au démarrage, non-copiable, lu par
> tous les autres systèmes. Renvoie des handles **RHI bruts** (`NkTextureHandle`/`NkSamplerHandle`…).

---

## Les sets et bindings : la convention UE-style

Avant d'aller plus loin, il faut comprendre la **numérotation** des descriptor sets, car elle
structure tout le pipeline. NkResources expose quatre enums qui fixent cette convention. Le set
**Frame** (`NK_SET_FRAME = 0`) regroupe ce qui change une fois par image : caméra, lumières,
clusters, atlas d'ombres, IBL. Le set **Object** (`NK_SET_OBJECT = 1`) ce qui change par objet :
sa matrice, ses os (skinning), ses données d'instanciation. Le set **Material**
(`NK_SET_MATERIAL = 2`) les paramètres PBR et les textures du matériau (albédo, normale, ORM,
émissif, AO). Le set **PostProcess** (`NK_SET_POSTPROCESS = 3`) la passe d'effets.

Le détail à ne **pas** rater : ces quatre enums (`NkDescSetIndex`, `NkFrameBinding`,
`NkObjectBinding`, `NkMaterialBinding`) sont des `enum` **non-scoped** (du C classique, pas
`enum class`). Les valeurs `NK_SET_*` et `NK_BIND_*` sont donc accessibles **directement** dans le
namespace `renderer`, sans qualification. On écrit `NK_BIND_CAMERA_UBO`, **pas**
`NkFrameBinding::NK_BIND_CAMERA_UBO`.

> **En résumé.** Quatre sets numérotés UE-style : Frame(0)/Object(1)/Material(2)/PostProcess(3),
> avec leurs bindings respectifs. Enums **non-scoped** : on utilise les constantes `NK_SET_*` /
> `NK_BIND_*` directement, sans préfixe de type.

---

## La bibliothèque de textures : `NkTextureLibrary`

`NkTextureLibrary` est la couche au-dessus de `NkResources` : c'est elle qui **charge des fichiers**,
génère les mips, crée les render targets, et tient le compte de ce qui est en VRAM. On l'initialise
en lui passant le `NkIDevice` **et** un pointeur vers `NkResources` — ce dernier est optionnel
(`nullptr` accepté), auquel cas la bibliothèque bascule en **mode dégradé** et crée ses propres
samplers au lieu de déléguer. Elle est elle aussi **non-copiable**.

Tout ce qu'elle renvoie est un `NkTexHandle` — le **wrapper**, pas le handle brut. Pour charger, on
distingue `Load` (image LDR classique), `LoadHDR` (image flottante haute dynamique, pour
l'environnement IBL par exemple) et `LoadCubemap`, qui prend **six chemins** dans l'ordre canonique
des faces : +X, −X, +Y, −Y, +Z, −Z. Toutes acceptent des `NkLoadOptions` qui contrôlent l'espace
colorimétrique (`srgb`), la génération de mips (chaîne Lanczos3), l'anisotropie et le mode de bord.

```cpp
NkTextureLibrary lib;
lib.Init(device, &res);                       // délègue les samplers à NkResources

NkLoadOptions opts;
opts.srgb = true;  opts.genMipmaps = true;
NkTexHandle wood = lib.Load("Textures/wood_albedo.png", opts);

NkTextureHandle rhi = lib.GetRHIHandle(wood); // wrapper → handle RHI brut pour le bind
```

Au-delà du chargement, la bibliothèque sait **créer** des textures à la main (via
`NkTextureCreateDesc` : pixels CPU, dimensions, format, mips, cubemap…), créer des **render targets**
(`CreateRenderTarget`, avec ou sans profondeur, lisibles ou non), et **mettre à jour** une texture
en cours d'exécution (`Update`, avec mip et couche cibles). Pour libérer, `Release` prend le handle
**par référence non-const** : l'appel peut **invalider** votre handle, c'est volontaire — il vous
empêche de réutiliser une référence morte. `ReleaseAll` purge tout.

Enfin, elle expose un **backend de chargement custom** : `SetCustomLoader` branche vos propres
fonctions de décodage d'image (`NkImageLoaderFn` / `NkImageFreeFn`), ce qui permet de remplacer le
décodeur intégré. Côté accès, `GetRHIHandle`/`GetRHISampler` donnent les handles RHI sous-jacents,
`HasMipmaps` interroge l'état, et un jeu de built-ins (`GetWhite1x1`, `GetBlack1x1`,
`GetNormal1x1`, `GetError`, `GetBRDFLUT`) **délègue à `NkResources` si disponible**, sinon retombe
sur un fallback local.

Ce n'est **pas** un gestionnaire d'assets disque avec hot-reload : pour ça, voir `NkTextureAsset`
plus bas. La bibliothèque charge, garde et libère ; elle ne sérialise rien.

> **En résumé.** `NkTextureLibrary` = chargement (LDR/HDR/cubemap), création manuelle, render
> targets, update runtime et stats VRAM, au-dessus de `NkResources`. Renvoie des `NkTexHandle`
> (wrapper). `Release(h&)` invalide votre handle. Mode dégradé si `resources == nullptr`.

---

## Les textures comme assets : `NkTextureAsset`

`NkTextureLibrary` charge un fichier image **brut**. Mais un moteur veut souvent associer à une
texture des **métadonnées d'import** : faut-il la traiter en sRGB ? générer des mips ? quel sampler
lui appliquer ? comment gérer sa transparence ? C'est le rôle de `NkTextureAsset` — un *payload*
`.nkasset` sérialisable (il hérite de `NkISerializable`) qui décrit **comment** une texture doit
être importée, indépendamment de son contenu binaire.

L'asset porte le chemin source disque, le format cible, les options `generateMips` / `sRGB`, un
**preset de sampler** (0=Linear, 1=Nearest, 2=Anisotropic16, 3=ClampLinear), et un **mode alpha**
(0=Opaque, 1=AlphaBlend, 2=AlphaTest avec `alphaCutoff`). La sérialisation se fait via `Serialize` /
`Deserialize`, qui écrivent et relisent exactement les mêmes clés JSON dans une `NkArchive`.

Les opérations de sauvegarde et de chargement passent par `NkTextureAssetIO`, une classe **sans
instance** : ses trois méthodes sont toutes `static` et `noexcept`. `Save` écrit l'asset sur disque
en lui attribuant un AssetId dérivé d'un **chemin logique** (ex. `"/Textures/Wood"`). `Load` relit
un asset depuis le disque et **produit directement** une texture dans la `NkTextureLibrary` qu'on
lui passe. `LoadById` fait de même mais résout l'asset via le `NkAssetRegistry` à partir de son
identifiant. En cas d'échec, on récupère un handle vide `{}`.

```cpp
NkTextureAsset asset;
asset.sourceFilePath = "art/wood.png";
asset.sRGB = true;  asset.generateMips = true;  asset.samplerPreset = 2; // Aniso16
NkTextureAssetIO::Save(asset, "cooked/wood.nkasset", "/Textures/Wood");

// Plus tard, n'importe où :
NkTexHandle h = NkTextureAssetIO::Load("cooked/wood.nkasset", &lib);
```

C'est la brique « Phase H » du pipeline d'assets. Attention à ce qu'elle **n'est pas encore** : il
n'y a **ni cache ref-counted ni hot-reload** natif (prévus en Phase H v1, non livrés). Charger deux
fois le même asset charge deux fois la texture. À noter aussi : le header se protège d'une macro
Win32 hostile via un garde `#undef GetObject`.

> **En résumé.** `NkTextureAsset` = description d'import sérialisable d'une texture 2D (format,
> sRGB, mips, preset de sampler, mode alpha), `.nkasset`. `NkTextureAssetIO` = trois helpers
> **statiques noexcept** (Save / Load / LoadById), échec → `{}`. Pas de cache ni hot-reload (différés).

---

## Le système de maillages : `NkMeshSystem`

Côté géométrie, `NkMeshSystem` joue pour les maillages le rôle que `NkTextureLibrary` joue pour les
textures : il crée, importe, met à jour et dessine des meshes, en renvoyant des `NkMeshHandle`. On
l'initialise avec un `NkIDevice` et on le ferme avec `Shutdown`.

Un mesh se décrit par un `NkMeshDesc` : un **layout de sommets**, un pointeur vers les données de
sommets, un buffer d'indices, une liste de **sous-maillages** (`NkSubMesh`, chacun avec son
matériau, ses bornes, ses flags de visibilité/ombre et ses **niveaux de détail** `NkSubMeshLOD`),
un drapeau `dynamic` et des bornes globales. Le layout (`NkVertexLayout`) liste des
`NkVertexElement` (attribut + format + location) et fournit des **factories** prêtes à l'emploi —
`Default3D`, `Default2D`, `Skinned`, `Debug`, `Particle` — ainsi que des **générateurs de code
shader** (`GenerateGLSL`, `GenerateHLSL`, `GenerateMSL`) qui produisent les déclarations d'entrée
correspondantes, par API.

```cpp
NkMeshSystem meshes;
meshes.Init(device);

NkMeshHandle cube  = meshes.GetCube();                  // primitive en cache
NkMeshHandle ball  = meshes.GetSphere(48, 48);          // plus de subdivisions
NkMeshHandle level = meshes.Import("levels/arena.glb"); // import disque
```

Le système offre une généreuse collection de **primitives built-in** mises en cache (cube, sphère,
icosphère, plan, quad, cylindre, cône, capsule — la plupart paramétrables en subdivisions), ce qui
évite de réécrire la génération de géométrie pour le prototypage ou le debug. Pour les meshes
dynamiques, on dispose de `UpdateVertices` / `UpdateIndices` (remplacement complet) et de
`UpdateVerticesRange` (mise à jour **partielle** du VBO, Phase M.6) — cette dernière **exige** que
le mesh ait été créé avec `dynamic = true`. Comme pour les textures, `Release` prend le handle par
référence non-const (et peut l'invalider), et `ReleaseAll` purge tout.

Le dessin proprement dit est appelé par NkRender3D : `BindMesh` lie les buffers, `DrawSubMesh`
dessine un sous-maillage donné (avec instanciation optionnelle), `DrawAll` dessine tout le mesh. Ces
fonctions prennent un `NkICommandBuffer*`. On peut interroger les sous-maillages
(`GetSubMeshCount`, `GetSubMesh`, `SetSubMeshMaterial`), les bornes (`GetBounds`) et les buffers
sous-jacents (`GetVBO`, `GetIBO`).

> **En résumé.** `NkMeshSystem` = création/import/update/draw de maillages, renvoyant des
> `NkMeshHandle`. `NkVertexLayout` décrit les sommets et **génère le code shader** par API.
> Primitives built-in en cache. `UpdateVerticesRange` réclame `dynamic = true`. Le dessin passe par
> un `NkICommandBuffer*`.

---

## Le streaming sous budget : `NkStreamingSystem`

Une scène réelle ne tient pas en VRAM. `NkStreamingSystem` gère la **résidence** des textures et des
maillages sous un **budget mémoire** : il charge ce qui devient visible, évince ce qui s'éloigne, et
ne dépasse jamais le quota fixé. Il s'initialise au-dessus de la `NkTextureLibrary` et du
`NkMeshSystem` (et d'un `NkIDevice`), avec une `NkStreamingConfig`.

La config fixe les règles du jeu : `budgetBytes` (512 MiB par défaut), le nombre de jobs par frame,
un `mipBias` global, les distances de stream-in/stream-out (avec un multiplicateur), et des bascules
pour activer le streaming de mips, le streaming de maillages, et le mode `async` (le mettre à
`false` rend tout synchrone, utile en debug). Chaque ressource enregistrée est suivie par un
`NkStreamEntry` qui porte son état, sa priorité, son timestamp LRU, sa taille et son mip courant/cible.

Le cycle d'usage est simple. On **enregistre** d'abord ce qui pourra être streamé
(`RegisterTexture`, `RegisterMesh`, avec un identifiant et un chemin), une fois pour toutes. Puis,
chaque frame, le système de visibilité **réclame** ce qui est visible (`Request(id, distance)`) et
**relâche** ce qui ne l'est plus (`Release(id)`). On lui donne la position caméra
(`SetCameraPosition`) et on l'avance d'une frame (`Update(dt)`), où il décide quoi charger et quoi
évincer selon les distances et le budget.

```cpp
NkStreamingSystem stream;
NkStreamingConfig cfg;  cfg.budgetBytes = 768ULL << 20;  // 768 MiB
stream.Init(device, &lib, &meshes, cfg);

stream.RegisterTexture(42, "Textures/wall.png");
// chaque frame :
stream.SetCameraPosition(cam.pos);
stream.Request(42, distFromCamera);
stream.Update(dt);
if (stream.IsResident(42)) { /* utilisable */ }
```

Un `NkStreamCallback` (`SetStateCallback`) notifie les transitions d'état, ce qui permet de réagir
quand une ressource devient résidente (et donc utilisable). Côté observation, on lit l'état d'une
ressource (`GetState`, `IsResident`), l'occupation mémoire (`GetUsedBytes`, `GetBudgetBytes`,
`GetUsageRatio`) et des compteurs (`GetPendingCount`, `GetLoadingCount`, `GetResidentCount`,
`GetEvictedCount`) très utiles pour un overlay de profilage.

> **En résumé.** `NkStreamingSystem` = résidence des textures/maillages **sous budget VRAM**,
> pilotée par la distance caméra. Register une fois, Request/Release par visibilité, SetCameraPosition
> + Update par frame. Callback de transition d'état + stats d'occupation. `async = false` pour un
> mode synchrone de debug.

---

## Aperçu de l'API

Tous les éléments publics, par fichier. Le détail (sémantique, cas d'usage) suit dans la « Référence
complète ».

### `NkResources` — ressources partagées

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(device)`, `Shutdown()`, `IsReady()` | Créer une fois / libérer / prêt ? |
| Textures défaut | `GetWhiteTex`, `GetBlackTex`, `GetNormalTex`, `GetMagentaTex`, `GetGrayTex` | Pixels 1×1 partagés (`NkTextureHandle`) ; magenta = missing-tex |
| Samplers | `GetSamplerLinear{Repeat,Clamp,Border}`, `GetSamplerNearest{Repeat,Clamp}`, `GetSamplerAnisotropic16`, `GetSamplerShadow`, `GetSamplerCubemap` | Samplers standards (`NkSamplerHandle`) ; shadow = PCF compare |
| Layouts | `GetFrameLayout`, `GetObjectLayout`, `GetMaterialLayout`, `GetPostProcessLayout` | Descriptor set layouts (`NkDescSetHandle`) |
| Fabriques buffer | `CreateUBO`, `CreateSSBO`, `CreateVertexDynamic`, `CreateIndexBuffer`, `CreateStagingBuffer` | Buffers (`NkBufferHandle`, `::Null` si échec) |
| Bindings (enums **non-scoped**) | `NkDescSetIndex`, `NkFrameBinding`, `NkObjectBinding`, `NkMaterialBinding` | Constantes `NK_SET_*` / `NK_BIND_*` directes |

### `NkTextureLibrary` — bibliothèque de textures

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(device, resources=nullptr)`, `Shutdown()` | Init (mode dégradé si `nullptr`) / fermer |
| Backend custom | `SetCustomLoader(load, free, user)` | Brancher un décodeur d'image custom |
| Chargement | `Load`, `LoadHDR`, `LoadCubemap` | Image LDR / HDR / cubemap (6 faces +X,−X,+Y,−Y,+Z,−Z) → `NkTexHandle` |
| Création | `Create(desc)`, `CreateRenderTarget` | Texture manuelle / render target |
| Update | `Update(h, data, rowPitch, mip, layer)` | Mise à jour runtime |
| Libération | `Release(h&)`, `ReleaseAll` | Libère (invalide le handle) / tout |
| Accès RHI | `GetRHIHandle`, `GetRHISampler`, `HasMipmaps` | `NkTexHandle` → handles RHI ; a des mips ? |
| Built-ins | `GetWhite1x1`, `GetBlack1x1`, `GetNormal1x1`, `GetError`, `GetBRDFLUT` | Délèguent à `NkResources` sinon fallback |
| Stats | `GetTextureCount`, `GetEstimatedVRAMBytes` | Compte / VRAM estimée |

### `NkTextureAsset` / `NkTextureAssetIO` — assets texture

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Champs | `sourceFilePath`, `targetFormat`, `generateMips`, `sRGB`, `samplerPreset`, `alphaMode`, `alphaCutoff` | Métadonnées d'import (`.nkasset`) |
| Sérialisation | `GetTypeName`, `Serialize(ar)`, `Deserialize(ar)` | Overrides `NkISerializable` (clés JSON exactes) |
| IO (tout **static noexcept**) | `Save`, `Load`, `LoadById` | Écrire / charger en texture / via `NkAssetRegistry` ; échec → `{}` |

### `NkMeshSystem` — maillages

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(device)`, `Shutdown()` | Démarrer / fermer |
| Description | `NkVertexLayout` (factories `Default3D/2D`, `Skinned`, `Debug`, `Particle` ; `GenerateGLSL/HLSL/MSL`), `NkMeshDesc`, `NkSubMesh`, `NkSubMeshLOD` | Décrire sommets/sous-mailles/LOD + générer le code shader |
| Création | `Create(desc)`, `Import(path, importMaterials)` | Mesh manuel / import disque |
| Update | `UpdateVertices`, `UpdateIndices`, `UpdateVerticesRange` (exige `dynamic=true`) | Remplacement complet / partiel du VBO |
| Libération | `Release(h&)`, `ReleaseAll` | Libère (invalide) / tout |
| Sous-mailles | `GetSubMeshCount`, `GetSubMesh`, `SetSubMeshMaterial`, `GetBounds` | Interroger / réassigner matériau / bornes |
| Primitives | `GetCube`, `GetSphere`, `GetIcosphere`, `GetPlane`, `GetQuad`, `GetCylinder`, `GetCone`, `GetCapsule` | Géométrie built-in en cache |
| Dessin | `BindMesh`, `DrawSubMesh`, `DrawAll` (avec `NkICommandBuffer*`) | Lier / dessiner (instanciation possible) |
| Buffers | `GetVBO`, `GetIBO` | Buffers GPU sous-jacents |

### `NkStreamingSystem` — streaming

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Config | `NkStreamingConfig` (budget, jobs, mipBias, distances, bascules, `async`) | Règles du streaming |
| Cycle de vie | `Init(device, texLib, meshSys, cfg)`, `Shutdown()` | Démarrer / fermer |
| Enregistrement | `RegisterTexture`, `RegisterMesh`, `Unregister` | Déclarer une ressource streamable |
| Visibilité | `Request(id, dist)`, `Release(id)` | Réclamer / relâcher par visibilité |
| Par frame | `SetCameraPosition`, `Update(dt)` | Position caméra / avancer d'une frame |
| Callback | `SetStateCallback(cb)` | Notifier les transitions (`NkStreamState`) |
| Accès | `GetState`, `IsResident`, `GetUsedBytes`, `GetBudgetBytes`, `GetUsageRatio` | État / résidence / occupation |
| Stats | `GetPendingCount`, `GetLoadingCount`, `GetResidentCount`, `GetEvictedCount` | Compteurs par état |
| États | `NkStreamState` (**enum class**) | `NK_UNLOADED/PENDING/LOADING/RESIDENT/EVICTING` |

---

## Référence complète

Chaque élément repris en détail. Les triviaux (cycle de vie, accesseurs) sont décrits brièvement ;
les mécanismes de fond (handles, sets, streaming) le sont à fond, avec leurs usages par domaine.

### Les deux niveaux de handles — à graver

C'est le point qui cause le plus d'erreurs, donc il vient en premier. NKRenderer manipule **deux**
représentations d'une texture :

- **`NkTextureHandle`** — le handle **RHI brut**, la ressource GPU nue. C'est ce que renvoie
  `NkResources` (ses textures par défaut) et ce qu'on bind effectivement dans un descriptor set.
- **`NkTexHandle`** — le **wrapper** de plus haut niveau, renvoyé par `NkTextureLibrary` et
  `NkTextureAssetIO`. Il porte les métadonnées (mips, sampler associé) et sert d'identité côté
  bibliothèque.

La conversion se fait dans un seul sens utile : `NkTextureLibrary::GetRHIHandle(NkTexHandle)` →
`NkTextureHandle`. Concrètement : on **charge** et on **garde** des `NkTexHandle` ; au moment de
binder pour le dessin, on demande le `NkTextureHandle` correspondant. Les confondre (passer un
wrapper là où un handle RHI est attendu, ou l'inverse) est une erreur de type silencieuse à
l'origine de bugs de rendu.

### `NkResources` à fond

**Pourquoi une source unique.** Sans elle, chaque sous-système recréerait son propre « blanc 1×1 »,
son propre sampler linéaire, ses propres layouts — d'où duplication en VRAM et incohérences
subtiles. `NkResources` centralise tout cela : on l'`Init` une fois, et `NkTextureLibrary` (entre
autres) y délègue. D'où le mode dégradé : si on passe `resources == nullptr` à la bibliothèque,
elle doit se débrouiller seule et recrée ses samplers.

**Les textures par défaut**, par usage :
- **Blanc / noir** — valeurs neutres pour un slot de matériau non fourni (un albédo blanc = pas de
  teinte, un masque noir = effet désactivé).
- **Normale (0.5, 0.5, 1, 1)** — la normale « plate » : brancher cette texture dans un slot de
  *normal map* revient à n'avoir aucun relief, sans cas particulier dans le shader.
- **Gris (0.5,…)** — valeur moyenne neutre (roughness/metallic par défaut, base de debug).
- **Magenta** — le **marqueur de texture manquante**. Quand un chemin ne charge pas, on substitue
  ce rose : visuellement impossible à rater, il signale immédiatement l'asset cassé.

**Les samplers** couvrent les combinaisons standard filtrage × adressage (linéaire/nearest ×
repeat/clamp/border), plus trois spécialisés : l'**anisotropique 16×** (textures vues en biais,
sols, routes), le **sampler d'ombre** (PCF compare-sampler, pour le filtrage des shadow maps) et le
**sampler cubemap** (tri-linéaire, clamp — pour les environnements/skybox).

**Les layouts** correspondent un-pour-un aux quatre sets (Frame/Object/Material/PostProcess) :
fournir ces layouts une fois garantit que tous les pipelines partagent la même structure de bind.

**Les fabriques de buffers** renvoient un `NkBufferHandle` (ou `::Null` en cas d'échec — à tester) :
`CreateUBO` (données uniformes, ex. matrices caméra), `CreateSSBO` (gros tableaux structurés :
lumières, os, instances), `CreateVertexDynamic` (sommets régénérés par frame), `CreateIndexBuffer`
(indices, qui prend directement les données), `CreateStagingBuffer` (intermédiaire d'upload CPU→GPU).

### Les enums de bindings à fond

Quatre enums **non-scoped** définissent la convention de descripteurs :

- **`NkDescSetIndex`** — les quatre sets : `NK_SET_FRAME=0`, `NK_SET_OBJECT=1`, `NK_SET_MATERIAL=2`,
  `NK_SET_POSTPROCESS=3`. La fréquence de mise à jour décroît avec le numéro inverse : Frame change
  par image, Object par objet, Material plus rarement.
- **`NkFrameBinding`** (set 0) — `NK_BIND_CAMERA_UBO=0`, `NK_BIND_LIGHTS_UBO=1`,
  `NK_BIND_LIGHTS_SSBO=2`, `NK_BIND_CLUSTERS_SSBO=3`, `NK_BIND_SHADOW_ATLAS=4`,
  `NK_BIND_IBL_IRRADIANCE=5`, `NK_BIND_IBL_SPECULAR=6`, `NK_BIND_BRDF_LUT=7`. Tout ce qui est
  « global à la frame » : caméra, lumières (en UBO **et** SSBO selon la passe), clustering,
  ombres et IBL.
- **`NkObjectBinding`** (set 1) — `NK_BIND_OBJECT_UBO=0`, `NK_BIND_BONES_SSBO=1`,
  `NK_BIND_INSTANCE_SSBO=2`. Données par objet : transform, palette d'os (skinning), buffer
  d'instances (instanciation).
- **`NkMaterialBinding`** (set 2) — `NK_BIND_PBR_PARAMS=0`, puis les textures
  `NK_BIND_TEX_ALBEDO=1`, `NK_BIND_TEX_NORMAL=2`, `NK_BIND_TEX_ORM=3`, `NK_BIND_TEX_EMISSIVE=4`,
  `NK_BIND_TEX_AO=5`. Le jeu de textures PBR standard (ORM = Occlusion/Roughness/Metallic packées).

Comme ce sont des `enum` C-style, on écrit directement `NK_BIND_TEX_ALBEDO` dans le namespace
`renderer`, sans `NkMaterialBinding::`. À ne pas confondre avec les enums **scoped** de
`NkMeshSystem` et `NkStreamingSystem` (voir plus bas), qui exigent, eux, la qualification.

### `NkTextureLibrary` à fond

**Chargement.** `Load` (LDR), `LoadHDR` (flottant, pour environnements et données techniques) et
`LoadCubemap` (six faces, ordre +X/−X/+Y/−Y/+Z/−Z — un ordre faux fait pivoter/retourner le ciel)
couvrent les trois familles. Les `NkLoadOptions` pilotent la qualité : `srgb` (espace
colorimétrique — vrai pour les textures de couleur, faux pour les données linéaires comme les
normales), `genMipmaps` (chaîne de mips par filtre **Lanczos3**, indispensable pour le filtrage en
distance), `useAnisotropic`, `useClampEdge`, et un `debugName`.

**Création manuelle.** `Create(NkTextureCreateDesc)` construit une texture à partir de pixels CPU :
on précise dimensions, profondeur, niveaux de mip, format (`NkGPUFormat`), et les drapeaux
`srgb`/`genMips`/`isHDR`/`isCubemap`. C'est la voie pour les textures procédurales (bruit, gradient,
LUT calculée) ou recomposées en mémoire.

**Render targets.** `CreateRenderTarget(w, h, format, depth, readable, name)` alloue une cible de
rendu — la brique des passes off-screen (G-buffer, réflexions, post-process). `depth` ajoute une
profondeur, `readable` autorise l'échantillonnage ultérieur.

**Update runtime.** `Update(h, data, rowPitch, mip, layer)` réécrit le contenu d'une texture
existante (vidéo, texture de données rafraîchie, atlas dynamique), avec contrôle du *row pitch*, du
mip et de la couche ciblés.

**Libération.** `Release` prend le handle **par référence non-const** et peut l'invalider — c'est
une garde : après l'appel, votre variable ne référence plus rien d'utilisable. `ReleaseAll` purge la
bibliothèque entière (typiquement au changement de niveau).

**Backend custom.** `SetCustomLoader(load, free, user)` substitue vos fonctions de décodage
(`NkImageLoaderFn` remplit un `NkImageData` — pixels LDR `uint8` RGBA, ou `hdrPixels` flottants si
`isHDR` ; `NkImageFreeFn` libère). Utile pour brancher un décodeur tiers ou un format maison sans
toucher la bibliothèque.

**Built-ins et stats.** `GetWhite1x1` / `GetBlack1x1` / `GetNormal1x1` / `GetError` /
`GetBRDFLUT` renvoient des `NkTexHandle` ; ils **délèguent à `NkResources`** s'il est branché, sinon
retombent sur un fallback local. `GetBRDFLUT` se distingue : il est non-const et **calculé à la
demande** (puis caché) — c'est la *lookup table* du BRDF pour l'IBL. `GetTextureCount` et
`GetEstimatedVRAMBytes` alimentent un overlay mémoire.

### `NkTextureAsset` et `NkTextureAssetIO` à fond

**L'asset** sépare le **quoi** (le fichier image) du **comment** (les règles d'import). Ses champs
sont autant de décisions d'import : `sourceFilePath` (relatif au CWD), `targetFormat`,
`generateMips`, `sRGB`, un `samplerPreset` entier (0=Linear, 1=Nearest, 2=Anisotropic16,
3=ClampLinear), et la gestion de transparence — `alphaMode` (0=Opaque, 1=AlphaBlend,
2=AlphaTest) avec `alphaCutoff` (seuil utilisé quand `alphaMode==2`). En tant que
`NkISerializable`, il implémente `GetTypeName` (→ `"NkTextureAsset"`), `Serialize` et `Deserialize`,
qui écrivent/relisent les **mêmes clés** dans une `NkArchive` (`"sourceFilePath"`, `"targetFormat"`,
`"generateMips"`, `"sRGB"`, `"samplerPreset"`, `"alphaMode"`, `"alphaCutoff"`).

**Les IO** sont entièrement **statiques et `noexcept`** (`NkTextureAssetIO` ne s'instancie jamais) :
- `Save(asset, outDiskPath, logicalPath, outId)` écrit le `.nkasset` et dérive l'`NkAssetId` du
  **chemin logique** (ex. `"/Textures/Wood"`), récupérable via `outId`.
- `Load(diskPath, texLib)` relit l'asset et **produit directement** une texture dans la
  bibliothèque fournie ; échec → `{}`.
- `LoadById(id, texLib)` résout l'asset par son identifiant via le `NkAssetRegistry`, puis charge.

À retenir sur les **limites** : aucun **cache ref-counted** (deux Load = deux textures) et aucun
**hot-reload** natif (différés en Phase H v1). Et le garde `#undef GetObject` dans le header neutralise
la macro Win32 qui sinon casserait la compilation.

### `NkMeshSystem` à fond

**Décrire un mesh.** `NkVertexLayout` est le cœur de la description : une liste de `NkVertexElement`
(chacun = un `NkVertexAttr`, un `NkVertexFmt`, une `location`) et un `stride`. Les factories
`Default3D` / `Default2D` / `Skinned` / `Debug` / `Particle` couvrent les cas courants sans
écrire le layout à la main. Surtout, le layout sait **générer le code shader** d'entrée
correspondant : `GenerateGLSL(version)`, `GenerateHLSL(dx12)`, `GenerateMSL()` produisent les
déclarations d'attributs par API — un seul layout, trois cibles cohérentes.

Les **attributs** disponibles (`NkVertexAttr`, **enum class** → qualification `NkVertexAttr::`) :
`NK_POSITION`, `NK_NORMAL`, `NK_TANGENT`, `NK_BITANGENT`, `NK_COLOR`, `NK_TEXCOORD0`,
`NK_TEXCOORD1`, `NK_JOINTS`, `NK_WEIGHTS`, `NK_CUSTOM`. Les **formats** (`NkVertexFmt`, scoped) :
`NK_F1`/`NK_F2`/`NK_F3`/`NK_F4` (flottants 1 à 4 composantes), `NK_U8x4_NORM` (couleur compacte),
`NK_U16x4` (indices d'os).

**La hiérarchie d'un mesh.** Un `NkMeshDesc` agrège layout + sommets + indices + une liste de
`NkSubMesh`. Chaque `NkSubMesh` est une **portion** du mesh (plage d'indices, base vertex) avec son
propre `NkMatHandle` (matériau), ses `NkAABB` (bornes), ses flags `visible`/`castShadow`, et une
liste de `NkSubMeshLOD` — chaque LOD étant une plage d'indices associée à une `screenSize` seuil
(le moteur choisit le LOD selon la taille à l'écran). `NkMeshDesc::Simple(...)` est un raccourci
pour les meshes mono-portion.

**Création, import, update.** `Create(desc)` construit le mesh sur GPU ; `Import(path,
importMaterials)` charge depuis un fichier. Les meshes **dynamiques** (créés avec `dynamic=true`)
se mettent à jour : `UpdateVertices` / `UpdateIndices` (remplacement complet) ou
`UpdateVerticesRange` (**partiel**, Phase M.6 — qui **exige** `dynamic=true`). Cas d'usage du
partiel : maillage déformable, terrain modifié localement, géométrie procédurale animée.

**Primitives.** Cube, sphère (stacks/slices), icosphère (subdivisions), plan (divisions),
quad, cylindre, cône, capsule — toutes **mises en cache** (un seul GPU mesh partagé par appels
successifs). Indispensables pour le prototypage, les gizmos de debug, les colliders visuels.

**Dessin.** Appelé par NkRender3D avec un `NkICommandBuffer*` : `BindMesh` lie VBO/IBO,
`DrawSubMesh` dessine une portion (avec `instances` pour l'instanciation), `DrawAll` dessine tout.
Autour : `GetSubMeshCount`/`GetSubMesh`/`SetSubMeshMaterial` (réassigner un matériau à la volée),
`GetBounds`, et l'accès direct aux buffers `GetVBO`/`GetIBO`. Comme ailleurs, `Release(h&)` invalide
le handle, `ReleaseAll` purge tout.

### `NkStreamingSystem` à fond

**Le problème.** La VRAM est finie ; une scène ouverte ne tient pas en entier. Le streaming
**charge à la demande** ce qui devient visible et **évince** ce qui s'éloigne, en respectant un
budget. C'est ce qui permet des mondes plus grands que la mémoire.

**La config** (`NkStreamingConfig`) fixe la politique : `budgetBytes` (512 MiB par défaut, le
plafond VRAM), `maxJobsPerFrame` (limite le travail d'I/O par image pour ne pas saccader),
`mipBias` (0 = pleine résolution, 1 = demi…), `distanceMult` et les seuils `streamInDist` /
`streamOutDist` (l'hystérésis entre les deux évite le va-et-vient de chargement), les bascules
`enableMipStreaming` / `enableMeshStreaming`, et `async` (à `false`, tout devient synchrone — mode
debug).

**Les états** (`NkStreamState`, **enum class** → `NkStreamState::`) décrivent le cycle de vie d'une
ressource : `NK_UNLOADED` (rien en VRAM) → `NK_PENDING` (réclamée) → `NK_LOADING` (en cours) →
`NK_RESIDENT` (prête, utilisable) → `NK_EVICTING` (en cours de libération). Chaque ressource est
suivie par un `NkStreamEntry` (id, chemin, état, priorité, timestamp LRU, taille, mip courant/cible,
flag mesh/texture).

**Le flux par frame.** On `RegisterTexture` / `RegisterMesh` une fois (déclaration, sans charger),
puis chaque image : le système de visibilité appelle `Request(id, distFromCamera)` pour ce qui est
visible et `Release(id)` pour ce qui ne l'est plus, on pose la position avec `SetCameraPosition`,
et `Update(dt)` exécute la logique — priorisation par distance, chargement dans la limite des jobs,
éviction LRU quand le budget est dépassé. `Unregister` retire complètement une ressource du système.

**Observation.** `SetStateCallback(NkStreamCallback)` notifie chaque transition (réagir quand une
ressource passe `NK_RESIDENT`, par exemple révéler un objet sans pop). En lecture : `GetState`,
`IsResident`, et les compteurs mémoire `GetUsedBytes` / `GetBudgetBytes` / `GetUsageRatio` ; côté
diagnostic, `GetPendingCount` / `GetLoadingCount` / `GetResidentCount` / `GetEvictedCount` alimentent
un overlay de profilage du streaming.

### Idiomes et pièges transverses

- **Deux niveaux de handles** : `NkTextureHandle` (RHI brut, `NkResources`) vs `NkTexHandle`
  (wrapper, `NkTextureLibrary`/`NkTextureAssetIO`). Conversion via `GetRHIHandle`. Ne pas les confondre.
- **`NkResources` = source unique** des défauts ; `NkTextureLibrary` délègue, et passe en **mode
  dégradé** si on l'initialise avec `resources == nullptr`.
- **`Release(h&)` par référence non-const** (dans `NkTextureLibrary` et `NkMeshSystem`) : l'appel
  peut **invalider/réinitialiser** votre handle. C'est volontaire.
- **`UpdateVerticesRange` exige `dynamic = true`** : un mesh statique ne se met pas à jour partiellement.
- **Copie** : `NkResources` et `NkTextureLibrary` sont **non-copiables** (copie supprimée
  explicitement). `NkMeshSystem` et `NkStreamingSystem` ne déclarent **pas** de suppression de copie.
- **Enums non-scoped vs scoped** : `NkResources` expose des `enum` C-style (constantes `NK_*`
  directes : `NK_BIND_TEX_ALBEDO`), tandis que `NkMeshSystem` (`NkVertexAttr`, `NkVertexFmt`) et
  `NkStreamingSystem` (`NkStreamState`) utilisent `enum class` (qualification obligatoire :
  `NkVertexAttr::NK_POSITION`).
- **`NkTextureAssetIO` entièrement statique** : retours `{}` en cas d'échec, **pas de cache ni
  hot-reload** (différés Phase H v1).

---

### Exemple

```cpp
#include "NKRenderer/Core/NkResources.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Streaming/NkStreamingSystem.h"
using namespace nkentseu::renderer;

// 1) La source unique, créée une fois.
NkResources res;
res.Init(device);
NkTextureHandle white = res.GetWhiteTex();              // défaut partagé (handle RHI brut)
NkBufferHandle  camUBO = res.CreateUBO(sizeof(CameraData), "CameraUBO");

// 2) La bibliothèque délègue à NkResources.
NkTextureLibrary lib;
lib.Init(device, &res);
NkLoadOptions opts;  opts.srgb = true;  opts.genMipmaps = true;
NkTexHandle wood = lib.Load("Textures/wood.png", opts); // wrapper
NkTextureHandle woodRHI = lib.GetRHIHandle(wood);       // wrapper → RHI brut pour le bind

// 3) Les maillages : une primitive + un mesh dynamique.
NkMeshSystem meshes;
meshes.Init(device);
NkMeshHandle sphere = meshes.GetSphere(48, 48);         // primitive en cache

// 4) Le streaming sous budget, piloté par la distance.
NkStreamingSystem stream;
NkStreamingConfig cfg;  cfg.budgetBytes = 768ULL << 20;
stream.Init(device, &lib, &meshes, cfg);
stream.RegisterTexture(42, "Textures/wall.png");
// ... chaque frame :
stream.SetCameraPosition(cam.pos);
stream.Request(42, distFromCamera);
stream.Update(dt);
if (stream.IsResident(42)) { /* prête à dessiner */ }
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
