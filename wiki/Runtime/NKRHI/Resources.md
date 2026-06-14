# Décrire les ressources et le pipeline

> Couche **Runtime** · NKRHI · Tous les **descripteurs** passés au device pour créer une
> ressource GPU : tampons, textures, samplers, layouts de sommets, états de rastérisation /
> profondeur / blend, shaders, render passes, pipelines, descriptors, barrières, copies et
> synchronisation.

Pour créer **quoi que ce soit** sur le GPU — un tampon de sommets, une texture, un pipeline
complet —, le RHI ne prend jamais une liste d'arguments à rallonge. Il prend **un seul objet de
description**, un `*Desc`, que vous remplissez puis lui tendez. Le device lit ce descripteur,
fabrique la ressource réelle, et vous rend en échange un **handle opaque** (`NkRhiHandle<>`) — un
simple jeton qui désigne la ressource sans rien révéler de l'API sous-jacente (Vulkan, DX12,
OpenGL, Metal ou le rasterizer logiciel). Tout ce fichier, c'est donc **le vocabulaire pour dire
au GPU ce que vous voulez**, pas le moteur qui le fabrique.

L'idiome est constant et il faut le comprendre une fois pour toutes : chaque `*Desc` est un
**agrégat POD-like** avec des **valeurs par défaut raisonnables** — on ne renseigne que ce qui
diffère du défaut. Ce n'est **pas** un objet vivant : un `*Desc` n'alloue rien, ne possède rien, ne
détruit rien ; il *décrit*. Une fois la ressource créée, le `*Desc` peut être jeté. Pour aller
vite, presque chaque famille fournit des **fabriques statiques** (`Vertex(...)`, `Tex2D(...)`,
`Forward(...)`) qui pré-remplissent les cas courants, et certaines un **builder fluide**
(`AddStage(...).AddColor(...)`) qui renvoie `*this&` pour chaîner.

- **Namespace** : `nkentseu`
- **Header** : `#include "NKRHI/Core/NkDescs.h"`
- **Dépend de** : `NKRHI/Core/NkTypes.h` (enums, handles), `NKContainers` (`NkVector`,
  `NkFunction`)

> **En résumé.** Un `*Desc` est une fiche de spécification immuable et sans propriété ; le device la
> lit et renvoie un `NkRhiHandle<>` opaque. Renseignez ce qui diffère du défaut, ou partez d'une
> fabrique statique. Les enums (`NkGPUFormat`, `NkResourceState`…) et tous les `Nk*Handle` vivent
> dans `NkTypes.h`, pas ici.

---

## Décrire la mémoire : tampons et textures

Les deux ressources de stockage du GPU sont le **tampon** (un bloc linéaire d'octets) et la
**texture** (un tableau d'éléments à 1, 2 ou 3 dimensions, avec format de pixel et mipmaps).

`NkBufferDesc` décrit un tampon par sa taille en octets, son **type** (vertex, index, uniform,
storage…), son **usage** (où il vit : `NK_DEFAULT` côté GPU, `NK_UPLOAD` mappable côté CPU,
`NK_READBACK` pour relire) et ses **bind flags** (à quels étages il s'attache). Plutôt que de
jongler avec ces quatre champs, on part presque toujours d'une fabrique : `Vertex(sz, data)` pour
un tampon de sommets **immuable** rempli une fois, `VertexDynamic(sz)` pour un tampon réécrit
chaque frame, `Uniform(sz)` pour des constantes, `Storage(sz)` pour de la lecture/écriture en
compute, `Staging(sz)` pour un intermédiaire de transfert.

`NkTextureDesc` décrit une texture par son **type** (`NK_TEX2D`, `NK_CUBE`, `NK_TEX3D`…), son
**format** (`NK_RGBA8_SRGB`, `NK_D32_FLOAT`…), ses dimensions, son nombre de **mips** (0 = générer
toute la chaîne), son **MSAA** et ses bind flags. Là encore les fabriques couvrent l'essentiel :
`Tex2D(...)` pour une image échantillonnée, `RenderTarget(...)` pour une cible de rendu,
`DepthStencil(...)` pour un tampon de profondeur, `Cubemap(...)` pour une *skybox* ou une sonde
IBL, `Tex3D(...)` pour un volume (brouillard, *voxel AO*).

Attention : ce ne sont **pas** des images CPU comme `NkImage` du module NKImage. Un `NkTextureDesc`
ne contient au plus que le pointeur `initialData` du mip 0 — il *décrit* l'objet GPU ; le pixel
vit ensuite côté carte.

> **En résumé.** `NkBufferDesc` = bloc d'octets (type + usage + bind) ; `NkTextureDesc` = image GPU
> typée (format + dims + mips + MSAA). Partez d'une fabrique (`Vertex`, `Uniform`, `Tex2D`,
> `RenderTarget`, `DepthStencil`, `Cubemap`, `Tex3D`) et n'ajustez que le reste.

---

## Décrire comment lire une texture : le sampler

Une texture ne se lit pas « brute » : c'est un **sampler** qui décide comment l'échantillonner —
filtrage (lisse ou pixelisé), comportement hors bordure (répéter, clamper), anisotropie, et le cas
spécial du **filtrage de comparaison** pour les ombres. `NkSamplerDesc` rassemble ces réglages.

On le manipule presque toujours par preset : `Linear()` (filtrage lisse, le défaut),
`Nearest()` (pixel-art, données non interpolables), `Anisotropic(n)` (textures vues de biais, sol,
route), `Clamp()` (étirer le bord au lieu de répéter), et le très spécifique `Shadow()` qui active
la comparaison de profondeur (`compareEnable=true`) pour le *percentage-closer filtering* des
ombres.

Le sampler est une ressource à part entière : on en crée **peu** (une poignée pour tout le moteur)
et on les réutilise partout, contrairement aux textures qu'on multiplie.

> **En résumé.** `NkSamplerDesc` = la *politique de lecture* d'une texture, indépendante de la
> texture. Choisissez un preset (`Linear`/`Nearest`/`Anisotropic`/`Clamp`/`Shadow`) ; créez-en peu,
> réutilisez-les.

---

## Décrire la forme d'un sommet : le vertex layout

Le GPU lit un tampon de sommets comme un flux d'octets : il faut lui dire **comment l'interpréter**.
C'est le rôle du *vertex layout*, fait d'**attributs** (chaque champ : position, normale, UV — son
emplacement, son format, son décalage dans le stride) et de **bindings** (chaque *stream* de
tampon : son numéro, son stride, et s'il avance par sommet ou par instance).

`NkVertexLayout` se construit avec un **builder fluide** : on enchaîne `AddBinding(...)` et
`AddAttribute(...)`. Le drapeau `perInstance` d'un binding est la clé de l'**instancing** : un
stream par-instance porte les données qui changent d'une instance à l'autre (matrice monde,
couleur) pendant que le stream par-vertex reste partagé.

> **En résumé.** Le vertex layout traduit les octets bruts d'un tampon en champs typés.
> `AddAttribute` décrit un champ, `AddBinding` décrit un stream ; `perInstance=true` ouvre
> l'instancing.

---

## Le cas du rasterizer logiciel

Le backend logiciel n'a pas de carte graphique : il interpole et shade **sur le CPU**. NKRHI lui
donne donc un sommet enrichi, `NkVertexSoftware` (position clip, normale, UV, couleur, plus 16
attributs libres), et trois **alias de fonctions** qui sont les signatures des shaders écrits en
C++ : `NkVertexShaderSoftware`, `NkPixelShaderSoftware`, `NkComputeShaderSoftware`. Vous écrivez le
shader comme une lambda C++, le rasterizer l'appelle par sommet / par pixel.

Ce mécanisme n'existe que pour le backend SW — sur les vrais GPU, le shader vient de son code source
(GLSL/HLSL/MSL) ou de son bytecode (SPIR-V/DXIL). C'est utile pour le débogage, les tests sans GPU,
ou un *fallback* garanti.

> **En résumé.** `NkVertexSoftware` + les trois alias `Nk*ShaderSoftware` permettent d'écrire des
> shaders **en C++** pour le rasterizer logiciel. Inutile (et inutilisé) sur les backends matériels.

---

## Décrire le pipeline : états fixes, shaders, render pass

Au-dessus des ressources viennent les **états du pipeline graphique**. Chacun est un petit `*Desc`
avec ses presets :

- **Rastérisation** (`NkRasterizerDesc`) — remplissage plein/wireframe, *culling*, sens des faces,
  *depth bias*. Presets : `Default()`, `NoCull()`, `Wireframe()`, `ShadowMap()` (avec le bias
  qui élimine l'acné d'ombre).
- **Profondeur/stencil** (`NkDepthStencilDesc`, `NkStencilOpState`) — test et écriture de
  profondeur, opérations de stencil. Presets : `Default()`, `NoDepth()`, `ReadOnly()`,
  `DepthEqual()`, `ReverseZ()`.
- **Blend** (`NkBlendDesc`, `NkBlendAttachment`) — comment la couleur émise se mélange à l'existante.
  Presets : `Opaque()`, `Alpha()`, `Additive()`.

Le **shader** se décrit avec `NkShaderDesc` (un sac d'étages `NkShaderStageDesc`) et son builder
fluide : `AddGLSL`, `AddHLSL`, `AddMSL`, `AddSPIRV`, etc. Multi-source : un même descripteur peut
porter le GLSL, le HLSL, le SPIR-V… et le backend choisit ce qu'il sait consommer.

La **render pass** (`NkRenderPassDesc`, `NkAttachmentDesc`) décrit les *attachments* (couleurs +
profondeur), comment ils sont chargés/stockés, et le **framebuffer** (`NkFramebufferDesc`) lie cette
passe à des textures concrètes. Presets de passe : `Forward()`, `ShadowMap()`, `GBuffer()`.

Tout cela se réunit dans `NkGraphicsPipelineDesc` (shader + layout + états + render pass) ou
`NkComputePipelineDesc` pour le compute.

> **En résumé.** Le pipeline = shader + vertex layout + trois états fixes (rasterizer, depth/stencil,
> blend) + render pass. Chaque morceau a un `*Desc` à presets ; tout est rassemblé par
> `NkGraphicsPipelineDesc` / `NkComputePipelineDesc`.

---

## Décrire les liaisons, transitions et la synchro

Enfin, les descripteurs « plomberie » : comment le shader **voit** les ressources, comment elles
changent d'**état**, comment on **copie** et **synchronise**.

- **Descriptors** (`NkDescriptorType`, `NkDescriptorBinding`, `NkDescriptorSetLayoutDesc`,
  `NkDescriptorWrite`) — le contrat entre le shader et les ressources : quel *binding* est un
  uniform buffer, une texture, un sampler… et l'écriture concrète qui attache une ressource à un
  *binding*.
- **Barrières** (`NkBufferBarrier`, `NkTextureBarrier`) — déclarent une **transition d'état**
  (de cible de rendu à ressource lisible, par exemple) que les API explicites (Vulkan/DX12) exigent.
- **Copies** (`NkBufferCopyRegion`, `NkBufferTextureCopyRegion`, `NkTextureCopyRegion`) — régions
  d'upload/download/blit.
- **Indirect** (`NkDrawIndirectArgs`, `NkDrawIndexedIndirectArgs`, `NkDispatchIndirectArgs`) — le
  layout mémoire exact des arguments de draw/dispatch quand c'est le **GPU** qui les remplit.
- **Présentation & global** (`NkSwapchainDesc`, `NkBindlessHeapDesc`, `NkSubmitInfo`) — la
  swapchain, le tas *bindless*, et la soumission synchronisée par sémaphores/fence.

> **En résumé.** Cette dernière famille décrit la *circulation* : qui voit quoi (descriptors), qui
> change d'état (barrières), qui se copie où (copies), qui pilote le GPU depuis le GPU (indirect), et
> comment on présente et synchronise (swapchain / submit).

---

## Aperçu de l'API

Tous les types sont dans le namespace `nkentseu`, header `NKRHI/Core/NkDescs.h`. Chaque struct est
un agrégat à valeurs par défaut ; les fabriques `static` et les builders fluides sont listés.

### Tampons, textures, samplers

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Buffer | `NkBufferDesc` | Décrit un tampon (sizeBytes, type, usage, bindFlags, initialData, debugName). |
| Buffer (fabriques) | `Vertex` · `VertexDynamic` · `Index` · `IndexDynamic` · `Uniform` · `Storage` · `Staging` | Presets vertex (immuable/dynamique), index, uniform, storage (R/W, `cpuRead`), staging. |
| Texture | `NkTextureDesc` | Décrit une texture (type, format, w/h/depth, arrayLayers, mipLevels, samples, bind, usage, initialData, rowPitch). |
| Texture (fabriques) | `Tex2D` · `RenderTarget` · `DepthStencil` · `Cubemap` · `Tex3D` | Image 2D / cible de rendu / profondeur / cubemap (6 faces) / volume 3D. |
| Sampler | `NkSamplerDesc` | Filtrage, adressage U/V/W, LOD bias/min/max, anisotropie, comparaison, bordure. |
| Sampler (presets) | `Linear` · `Nearest` · `Anisotropic` · `Shadow` · `Clamp` | Lisse / pixel / anisotrope / comparaison d'ombre / clamp bord. |

### Vertex layout et rasterizer logiciel

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Vertex | `NkVertexAttribute` | Un champ (location, binding, format, offset, semanticName/Idx). |
| Vertex | `NkVertexBinding` | Un stream (binding, stride, perInstance). |
| Vertex | `NkVertexLayout` | Conteneur d'attributs + bindings. |
| Vertex (builder) | `AddAttribute` · `AddBinding` | Ajout fluide d'un champ / d'un stream. |
| Software | `NkVertexSoftware` | Sommet enrichi CPU (position, normal, uv, color, clipZ/W, 16 attrs). |
| Software (méthodes) | `SetAttrVec3` · `SetAttrVec4` · `GetAttrVec3` · `GetAttrVec4` | Lecture/écriture bornée des 16 attributs libres. |
| Software (alias) | `NkVertexShaderSoftware` · `NkPixelShaderSoftware` · `NkComputeShaderSoftware` | Signatures de shaders C++ (vertex/pixel/compute). |

### États du pipeline

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Rasterizer | `NkRasterizerDesc` | fillMode, cullMode, frontFace, depthClip, scissor, MSAA, depthBias×3. |
| Rasterizer (presets) | `Default` · `NoCull` · `Wireframe` · `ShadowMap` | Standard / pas de cull / fil de fer / biais d'ombre + cull front. |
| Depth/Stencil | `NkStencilOpState` | Ops stencil (fail/depthFail/pass), compareOp, masques, reference. |
| Depth/Stencil | `NkDepthStencilDesc` | Test/écriture profondeur, compareOp, stencil front/back. |
| Depth/Stencil (presets) | `Default` · `NoDepth` · `ReadOnly` · `DepthEqual` · `ReverseZ` | Standard / off / lecture seule / égalité / Z inversé. |
| Blend | `NkBlendAttachment` | Mélange par attachment (facteurs/ops couleur+alpha, writeMask). |
| Blend (presets attachment) | `Opaque` · `Alpha` · `PreMultAlpha` · `Additive` | Opaque / alpha / alpha pré-multiplié / additif. |
| Blend | `NkBlendDesc` | Attachments + alphaToCoverage + blendConstants (ctor pousse 1 Opaque). |
| Blend (presets) | `Opaque` · `Alpha` · `Additive` | Remplace `attachments[0]`. |

### Shaders et render pass

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Shader | `NkShaderStageDesc` | Un étage : stage + sources (GLSL/HLSL/MSL/SW) / SPIR-V / DXIL / Metal IR / fn CPU + entryPoint. |
| Shader | `NkShaderDesc` | Conteneur d'étages + debugName. |
| Shader (builder) | `AddStage` · `AddGLSL` · `AddHLSL` · `AddMSL` · `AddSPIRV` · `AddSW` · `AddSWFn` · `AddSWVertex` · `AddSWFragment` · `AddSWCompute` | Ajout fluide d'un étage par source/bytecode/fonction. |
| Render pass | `NkAttachmentDesc` | Un attachment (format, samples, load/store, stencil load/store, clearValue). |
| Render pass (fabriques) | `Color` · `Depth` · `ColorLoad` | Color clear / depth (clear 1.0) / color en LOAD. |
| Render pass | `NkRenderPassDesc` | colorAttachments + depth + resolve MSAA + finalForPresent. |
| Render pass (builder) | `AddColor` · `SetDepth` · `SetResolve` | Ajout fluide d'attachments. |
| Render pass (presets) | `Forward` · `ShadowMap` · `GBuffer` | 1 color+depth / depth seul / G-Buffer 4 colors+depth. |
| Framebuffer | `NkFramebufferDesc` | Lie une render pass à des textures concrètes (handles + w/h/layers). |

### Pipelines et passes compute

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Pipeline | `NkGraphicsPipelineDesc` | shader + vertexLayout + topology + 3 états + renderPass + pushConstants + descriptorSetLayouts. |
| Pipeline (builder) | `AddPushConstant` | Déclare un push constant range. |
| Compute | `NkComputePipelineDesc` | shader + pushConstants + descriptorSetLayouts. |
| Compute (builder) | `AddPushConstant` | Idem côté compute (obligatoire Vulkan). |
| Compute pass | `NkComputePassDesc` | Scoping debug d'une région compute (debugName, timestamp, hint). |

### Descriptors, barrières, copies, indirect, sync

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Descriptor | `NkDescriptorType` (enum) | Type de liaison (uniform/storage buffer, textures, samplers, input attachment). |
| Descriptor | `NkDescriptorBinding` | Un binding (numéro, type, count, stages). |
| Descriptor | `NkDescriptorSetLayoutDesc` | Layout d'un set + bindless. |
| Descriptor (méthodes) | `Add` · `AddBindless` · `TotalBindingCount` | Ajout / ajout bindless / compte. |
| Descriptor | `NkDescriptorWrite` | Attache une ressource concrète à un binding. |
| Barrière | `NkBufferBarrier` · `NkTextureBarrier` | Transition d'état (before/after + stages [+ mips/layers]). |
| Copie | `NkBufferCopyRegion` · `NkBufferTextureCopyRegion` · `NkTextureCopyRegion` | Régions buffer↔buffer / buffer↔texture / texture↔texture. |
| Indirect | `NkDrawIndirectArgs` · `NkDrawIndexedIndirectArgs` · `NkDispatchIndirectArgs` | Layout mémoire des args de draw/dispatch GPU-driven. |
| Présentation | `NkSwapchainDesc` | Chaîne d'images (taille, formats, imageCount, vsync, hdr). |
| Bindless | `NkBindlessHeapDesc` | Capacités du tas bindless (max par type). |
| Sync | `NkSubmitInfo` | Soumission de command buffers + wait/signal sémaphores + fence. |

---

## Référence complète

Chaque élément est repris ici à fond, avec ses usages dans les différents domaines du moteur. La
règle invariable : un `*Desc` *décrit*, il n'alloue ni ne possède rien ; les `const char*`
(`debugName`, sources) et les pointeurs de données sont **non-owning** (durée de vie à la charge de
l'appelant).

### `NkBufferDesc` — le tampon

Un tampon est un bloc linéaire d'octets sur le GPU. Quatre champs définissent sa nature : la
**taille** (`sizeBytes`), le **type** (`type=NK_VERTEX` par défaut), l'**usage** (où il réside :
`NK_DEFAULT` rapide côté GPU, `NK_UPLOAD` mappable et réécrit par le CPU, `NK_READBACK` relisible) et
les **bind flags** (à quels étages il s'attache). On part presque toujours d'une fabrique :

- **Rendu** — `Vertex(sz, data)` / `Index(sz, data)` pour la géométrie statique chargée une fois
  (usage immuable, bind `NK_VERTEX_BUFFER` / index). `VertexDynamic`/`IndexDynamic` pour un maillage
  régénéré chaque frame (particules, terrain morphing, UI).
- **Rendu / shading** — `Uniform(sz)` (UPLOAD) porte les constantes par frame/objet (matrices,
  paramètres de matériau) ; on le réécrit chaque frame côté CPU.
- **GPU / compute** — `Storage(sz, cpuRead)` est le tampon lecture-écriture des shaders compute
  (bind `NK_STORAGE_BUFFER | NK_UNORDERED_ACCESS`) : simulation de particules, *culling* GPU,
  réductions ; `cpuRead=true` le bascule en `NK_READBACK` pour rapatrier un résultat (histogramme,
  comptage de visibilité).
- **IO / transfert** — `Staging(sz)` (UPLOAD, bind `NK_NONE`) est l'intermédiaire neutre d'un
  transfert : on y écrit côté CPU puis on copie vers un tampon/texture `NK_DEFAULT`.
- **ECS / gameplay** — un tampon storage peut héberger un tableau d'instances (transformes,
  couleurs) consommé par l'instancing, alimenté depuis vos composants.

`initialData=nullptr` signifie « non initialisé » ; `debugName` étiquette la ressource dans
RenderDoc/PIX (non-owning).

### `NkTextureDesc` — la texture

La texture est un tableau d'éléments formaté, à 1/2/3 dimensions, avec mipmaps et MSAA optionnels.
Le **type** fixe la topologie (`NK_TEX2D`, `NK_CUBE`, `NK_TEX3D`…), le **format** le pixel
(`NK_RGBA8_SRGB`, `NK_RGBA16_FLOAT`, `NK_D32_FLOAT`…), `mipLevels=0` demande la chaîne complète,
`arrayLayers=6` est imposé pour les cubemaps. Les fabriques couvrent chaque besoin :

- **Rendu** — `Tex2D(w, h, fmt, mips)` pour toute texture échantillonnée (albedo, normal map,
  ORM…), typiquement en sRGB pour la couleur. `RenderTarget(w, h)` (bind
  `NK_RENDER_TARGET | NK_SHADER_RESOURCE`) pour rendre dans une texture puis la relire — base du
  post-processing, des reflets planaires, du bloom.
- **Rendu / profondeur** — `DepthStencil(w, h)` (bind `NK_DEPTH_STENCIL | NK_SHADER_RESOURCE`)
  produit un tampon de profondeur qu'on peut aussi échantillonner : *shadow maps*, SSAO,
  reconstruction de position.
- **Éclairage / IBL** — `Cubemap(sz, fmt, mips)` (type `NK_CUBE`, 6 faces, carré) sert aux skybox,
  aux sondes de réflexion, à l'irradiance pré-filtrée.
- **Volumes / GPU** — `Tex3D(w, h, d, fmt)` (bind `NK_SHADER_RESOURCE | NK_UNORDERED_ACCESS`) pour
  un brouillard volumétrique, une grille d'AO voxelisée, une LUT 3D de color grading.
- **UI / 2D / outils** — `Tex2D` en `NK_RGBA8_UNORM` pour atlas de glyphes, sprites, icônes
  d'éditeur.

`initialData` ne porte que le mip 0, layer 0 ; `rowPitch=0` laisse le moteur calculer le pas de
ligne. Ce n'est pas une image CPU : c'est une description d'objet GPU.

### `NkSamplerDesc` — la politique de lecture

Un sampler dit **comment** échantillonner une texture, indépendamment de quelle texture. Les champs
clés : `magFilter`/`minFilter`/`mipFilter` (lisse `NK_LINEAR` ou net `NK_NEAREST`),
`addressU/V/W` (répéter, clamper…), `maxAnisotropy` (1 = off, jusqu'à 16), et le couple
`compareEnable`/`compareOp` du filtrage de comparaison. Les presets :

- **Rendu** — `Linear()` (le défaut) pour la quasi-totalité des textures couleur ; `Anisotropic(n)`
  pour les surfaces vues en rasance (sol, route, murs) où le bilinéaire devient flou.
- **2D / UI / pixel-art** — `Nearest()` pour ne pas lisser : sprites pixel-art, atlas de glyphes
  bitmap, données encodées dans une texture (ID, indices) qu'il ne faut surtout pas interpoler.
- **Éclairage / ombres** — `Shadow()` active la comparaison de profondeur (`compareEnable=true`,
  `compareOp=NK_LESS_EQUAL`, adressage `NK_CLAMP_TO_EDGE`, bordure `NK_OPAQUE_WHITE`) : c'est le
  sampler du PCF qui adoucit le bord des ombres.
- **Post-process / cibles** — `Clamp()` étire le pixel de bord plutôt que de répéter, ce qui évite
  les artefacts en bordure d'un fullscreen pass ou d'une render target relue.

On crée **peu** de samplers (une poignée pour tout le moteur) et on les partage.

### `NkVertexAttribute`, `NkVertexBinding`, `NkVertexLayout` — la forme d'un sommet

Le GPU reçoit des octets ; le layout les rend lisibles. Un **attribut** décrit un champ : sa
`location` (le slot vu par le shader), son `binding` (de quel stream il provient), son `format`
(`NK_RGB32_FLOAT` pour une position, etc.), son `offset` dans le stride, et — pour DX11 — son
`semanticName` (`POSITION`, `NORMAL`…). Un **binding** décrit un stream : son numéro, son `stride`
(taille d'un sommet en octets) et `perInstance`.

`NkVertexLayout` rassemble les deux, via un builder fluide. Cas d'usage :

- **Rendu** — déclarer le format de sommet d'un maillage : position + normale + UV + tangente, le
  layout doit coller exactement à la structure C++ envoyée dans le tampon.
- **Rendu / instancing** — un second binding `perInstance=true` porte la matrice monde et la couleur
  par instance ; on dessine une forêt, une foule, un système de particules en un seul draw.
- **2D / UI** — un layout minimal position + UV + couleur pour le batch de quads.

`AddAttribute(loc, bind, fmt, off, sem, semIdx)` et `AddBinding(bind, stride, instanced)`
retournent `*this&` pour chaîner.

### `NkVertexSoftware` et les alias `Nk*ShaderSoftware` — le rasterizer CPU

Le backend logiciel shade sur le CPU. `NkVertexSoftware` est son sommet de travail : `position`
(clip `NkVec4`), `normal`, `uv`, `color`, les coordonnées `clipZ`/`clipW`, et un tableau de **16
attributs libres** (`attrs[16]`, `attrCount`) pour faire transiter n'importe quelle donnée
interpolée du vertex shader vers le pixel shader. Les méthodes sont **bornées** :
`SetAttrVec3(base, …)` n'écrit que si `base+2 < 16` (et avance `attrCount`), `GetAttrVec3(base)`
renvoie `{}` hors borne — donc pas de débordement silencieux.

Les trois alias sont les **signatures** des shaders écrits en C++ :

- `NkVertexShaderSoftware` — `(vertexData, vertexIndex, uniformData) -> NkVertexSoftware`.
- `NkPixelShaderSoftware` — `(interpolated, uniformData, texSampler) -> NkVec4f`.
- `NkComputeShaderSoftware` — `(groupX, groupY, groupZ, uniformData) -> void`.

Usages : **débogage** (rejouer un shader sans GPU, instrumenter pixel par pixel), **tests**
(CI sans carte graphique), **fallback** garanti sur une machine sans driver, **outils** (rendu
hors-ligne d'aperçus). Sur les backends matériels, on n'écrit jamais ces fonctions — le shader vient
de son source ou de son bytecode.

### `NkRasterizerDesc` — l'état de rastérisation

Contrôle la conversion des triangles en fragments : `fillMode` (plein ou fil de fer), `cullMode`
(quelle face on jette), `frontFace` (sens horaire/anti-horaire), `depthClip`, `scissorTest`, MSAA,
et le **depth bias** (`depthBiasConst`/`Slope`/`Clamp`). Presets :

- **Rendu** — `Default()` (cull back, CCW) pour la géométrie opaque normale.
- **Rendu / transparence et 2D** — `NoCull()` pour les feuillages, les *billboards*, les quads d'UI
  qu'on voit des deux côtés.
- **Outils / debug** — `Wireframe()` pour visualiser la topologie d'un maillage dans l'éditeur.
- **Éclairage / ombres** — `ShadowMap()` applique un *depth bias* (`const=1.25`, `slope=1.75`) et
  cull les faces avant pour éliminer l'**acné d'auto-ombrage** lors du rendu de la shadow map.

### `NkStencilOpState`, `NkDepthStencilDesc` — profondeur et stencil

`NkDepthStencilDesc` règle le **test de profondeur** (`depthTestEnable`, `depthCompareOp=NK_LESS`),
l'**écriture** de profondeur (`depthWriteEnable`), et le **stencil** (`stencilEnable` + une
`NkStencilOpState` par face : ops de fail/depth-fail/pass, compareOp, masques, référence). Presets :

- **Rendu** — `Default()` (test + write `NK_LESS`) pour la passe opaque qui construit le Z-buffer.
- **Rendu / transparence** — `ReadOnly()` teste mais n'écrit pas la profondeur : on dessine les
  objets transparents triés sans corrompre le Z-buffer.
- **Rendu / overlay & post** — `NoDepth()` désactive tout : UI, fullscreen passes, gizmos toujours
  visibles.
- **Rendu / Z-prepass** — `DepthEqual()` (`NK_EQUAL`, write off) ne shade que les fragments dont la
  profondeur correspond exactement à un prepass préalable (zéro overdraw).
- **Précision** — `ReverseZ()` (`NK_GREATER`) répartit mieux la précision du depth buffer pour les
  grandes scènes (combiné à une projection Z inversée).

Le **stencil** sert aux masques (portails, miroirs, contours d'objets sélectionnés dans l'éditeur,
décalcomanies).

### `NkBlendAttachment`, `NkBlendDesc` — le mélange des couleurs

Le blend décide comment la couleur émise par le pixel shader se combine à celle déjà présente.
`NkBlendAttachment` règle ça **par attachment** : `blendEnable`, les facteurs et opérations couleur
(`srcColor`/`dstColor`/`colorOp`) et alpha, le `colorWriteMask`. Presets d'attachment :

- **Rendu opaque** — `Opaque()` : pas de mélange, la nouvelle couleur écrase.
- **Transparence** — `Alpha()` (`SRC_ALPHA`, `ONE_MINUS_SRC_ALPHA`) pour le verre, le feuillage,
  les fondus ; `PreMultAlpha()` (srcColor=ONE) quand l'alpha est déjà pré-multiplié (compositing UI,
  certains atlas).
- **Effets** — `Additive()` (src=dst=ONE) pour le feu, les étincelles, les *glow*, les lasers, où
  les contributions s'ajoutent et n'occultent jamais.

`NkBlendDesc` agrège un `NkVector<NkBlendAttachment>` (un par cible, utile pour un MRT/G-Buffer où
chaque cible a son propre mélange), plus `alphaToCoverage` et `blendConstants[4]`. **Piège** : son
constructeur pousse déjà un `NkBlendAttachment::Opaque()` — `attachments` n'est donc *jamais* vide
après construction ; ne re-`PushBack` pas un Opaque par mégarde. Ses presets `Opaque/Alpha/Additive`
**remplacent** `attachments[0]`.

### `NkShaderStageDesc`, `NkShaderDesc` — les shaders

`NkShaderStageDesc` décrit **un étage** (vertex, fragment, compute…) et peut porter, en parallèle,
plusieurs représentations : `glslSource`/`hlslSource`/`mslSource`/`swSource` (sources texte),
`spirvBinary` (bytecode Vulkan), `dxilData`/`metalIRData` (bytecodes DX/Metal), un `cpuFn` (fonction
CPU pour le backend logiciel), plus l'`entryPoint` (`"main"`, ou `"VSMain"`/`"PSMain"` pour HLSL).
Le device pioche la représentation qu'il sait consommer.

`NkShaderDesc` est le sac d'étages, avec un builder fluide :

- **Rendu** — `AddGLSL(stage, src)` + `AddSPIRV(stage, data, sz)` pour fournir à la fois la source et
  le bytecode pré-compilé ; le multi-source permet un même descripteur pour Vulkan, DX, GL et Metal.
- **GPU / compute** — un seul étage compute (`AddSPIRV` ou `AddGLSL`) pour la simulation, le culling,
  le post.
- **Software / debug** — `AddSWVertex(fn)` / `AddSWFragment(fn)` / `AddSWCompute(fn)` pour le
  rasterizer CPU.

`AddSPIRV` fait un `Resize` + `memcpy` du binaire dans `spirvBinary` (copie). `AddSWFn` stocke un
pointeur **opaque sans copie ni ownership**.

**Deux pièges importants** à connaître sur `AddSWVertex/Fragment/Compute` :
- Ils font un `new` **brut** (`cpuFn = new NkXxxShaderSoftware(fn)`) censé être « libéré dans
  `CreateShader` » : l'ownership est transféré au device — ne réutilisez pas le `NkShaderStageDesc`
  hors de ce flux.
- Ils poussent l'étage **deux fois** (`stages.PushBack(s)` explicite **puis** `return AddStage(s)`
  qui re-`PushBack`) : le même `cpuFn` apparaît en double dans `stages`, risque latent de
  double-free / double-exécution. À surveiller.

(Les méthodes SkSL commentées dans le header ne sont **pas** déclarées : ce n'est pas de l'API.)

### `NkAttachmentDesc`, `NkRenderPassDesc`, `NkFramebufferDesc` — les passes de rendu

Une **render pass** déclare les cibles d'un rendu et comment elles sont traitées. `NkAttachmentDesc`
décrit une cible : `format`, `samples`, `loadOp` (`NK_CLEAR` efface au début, `NK_LOAD` conserve),
`storeOp`, le stencil load/store, et la `clearValue`. Fabriques : `Color(fmt, load)`,
`Depth(fmt)` (clearValue profondeur 1.0), `ColorLoad(fmt)` (= Color en `NK_LOAD`, pour accumuler
au lieu d'effacer).

`NkRenderPassDesc` agrège les `colorAttachments`, un `depthAttachment` optionnel (`hasDepth`), un
`resolveAttachment` MSAA optionnel (`hasResolve`), et le drapeau **`finalForPresent`**. Builder :
`AddColor`, `SetDepth`, `SetResolve`. Presets :

- **Rendu** — `Forward(color, depth, msaa)` : 1 color + depth, le MSAA s'applique aux deux. La passe
  principale d'un rendu forward.
- **Éclairage / ombres** — `ShadowMap(depth)` : depth seul, aucune couleur. La passe qui remplit une
  shadow map.
- **Rendu différé** — `GBuffer()` : 4 cibles (albedo `RGBA8_SRGB`, normal `RGBA16_FLOAT`, ORM
  `RGBA8_UNORM`, emission `RGBA16_FLOAT`) + depth. La géométrie du *deferred shading*.

**`finalForPresent`** est crucial : à `true` (Vulkan), le `finalLayout` de `color[0]` devient
`PRESENT_SRC_KHR` au lieu de `COLOR_ATTACHMENT_OPTIMAL`. À mettre sur le **dernier** pass écrivant
dans la swapchain, sinon `vkQueuePresentKHR` rejette l'image (VUID-...-01430).

`NkFramebufferDesc` est l'étape concrète : il **lie** une `renderPass` (handle) à des textures
réelles (`colorAttachments`, `depthAttachment`, `resolveAttachments` en handles), avec `width`,
`height`, `layers`. La render pass dit la *forme* ; le framebuffer fournit les *pixels*.

### `NkGraphicsPipelineDesc`, `NkComputePipelineDesc`, `NkComputePassDesc` — les pipelines

`NkGraphicsPipelineDesc` est le **rassemblement final** du rendu : un `shader` (handle), le
`vertexLayout`, la `topology` (`NK_TRIANGLE_LIST` par défaut), les trois états (`rasterizer`,
`depthStencil`, `blend`), le `renderPass`/`subpass`, les `patchControlPoints` (tessellation), et —
**important** — les `pushConstants` et `descriptorSetLayouts` : ces deux-là doivent être déclarés
**ici** pour que le *pipeline layout* soit valide sur Vulkan/DX12. `AddPushConstant(stages, offset,
size)` ajoute un range.

`NkComputePipelineDesc` est l'équivalent minimal pour le compute : `shader` + `pushConstants` +
`descriptorSetLayouts`. Les push constants sont **obligatoires sur Vulkan**, ignorés (implicites) sur
GL/DX. Usages : simulation de particules, *culling* GPU, génération de mipmaps, post-process compute.

`NkComputePassDesc` ne crée pas de pipeline : il **délimite une région** compute pour le débogage
(scoping RenderDoc/PIX/Xcode), avec `debugName`, `enableTimestamp` (mesure du temps GPU) et
`hintDispatchCount` (indice de pré-allocation).

### `NkDescriptorType`, `NkDescriptorBinding`, `NkDescriptorSetLayoutDesc`, `NkDescriptorWrite` — les liaisons

`NkDescriptorType` énumère **comment** une ressource est vue par le shader (indices 0..8) :
`NK_UNIFORM_BUFFER`, `NK_UNIFORM_BUFFER_DYNAMIC`, `NK_STORAGE_BUFFER`, `NK_STORAGE_BUFFER_DYNAMIC`,
`NK_SAMPLED_TEXTURE` (texture + sampler séparés), `NK_STORAGE_TEXTURE` (image load/store),
`NK_COMBINED_IMAGE_SAMPLER` (combinés, style GL), `NK_SAMPLER`, `NK_INPUT_ATTACHMENT` (subpass input
Vulkan).

`NkDescriptorBinding` décrit **un** point de liaison : son `binding` (numéro), son `type`, son
`count` (taille d'array) et les `stages` qui le voient. `NkDescriptorSetLayoutDesc` rassemble ces
bindings en un **layout de set**, éventuellement `isBindless`. Méthodes :

- `Add(binding, type, stages, count)` — ajoute un binding classique.
- `AddBindless(binding, type, stages, maxCount)` — passe le set en bindless et ajoute un binding de
  grande capacité (textures bindless, *mega-array* de matériaux).
- `TotalBindingCount()` — le nombre de bindings, O(1).

`NkDescriptorWrite` est l'**écriture concrète** qui attache une ressource à un binding d'un set
existant : `set`, `binding`, `arrayElem`, `type`, puis la ressource (`buffer` + `bufferOffset`/
`bufferRange`, ou `texture` + `sampler`) et `textureLayout`. Usages : brancher l'UBO de frame, lier
les textures d'un matériau, mettre à jour un slot d'un array bindless quand un asset est chargé
(streaming).

### `NkBufferBarrier`, `NkTextureBarrier` — les transitions d'état

Sur les API explicites (Vulkan/DX12), une ressource doit **changer d'état** avant d'être utilisée
autrement : une texture passe de cible de rendu à ressource échantillonnable, un tampon de
destination de copie à lecture de vertex. `NkBufferBarrier` déclare `stateBefore`/`stateAfter` et
les `srcStage`/`dstStage` (étages du pipeline concernés). `NkTextureBarrier` ajoute la précision du
**sous-ensemble** : `baseMip`/`mipCount`, `baseLayer`/`layerCount` (par défaut `UINT32_MAX` = tout).
Usages : après avoir rendu une shadow map, barrière vers `NK_SHADER_READ` pour l'échantillonner ;
après un compute qui écrit un storage buffer, barrière avant de le lire en vertex ; transitionner
chaque mip lors d'une génération de mipmaps.

### `NkBufferCopyRegion`, `NkBufferTextureCopyRegion`, `NkTextureCopyRegion` — les copies

Trois régions pour les trois sens de copie. `NkBufferCopyRegion` (buffer↔buffer) :
`srcOffset`/`dstOffset`/`size`. `NkBufferTextureCopyRegion` (buffer↔texture, l'upload d'une image) :
`bufferOffset`, `bufferRowPitch` (0 = *tight packed*), `mipLevel`/`arrayLayer`, position
`x`/`y`/`z` et `width`/`height`/`depth`. `NkTextureCopyRegion` (texture↔texture, un blit) :
mip/layer/position source et destination + extent. **Piège** : dans les deux régions de texture,
`width` et `height` **n'ont pas de valeur par défaut** (seul `depth=1`) — toujours les renseigner.
Usages : uploader un asset depuis un staging buffer, rapatrier un readback, copier un mip vers le
suivant, blitter une partie d'atlas.

### `NkDrawIndirectArgs`, `NkDrawIndexedIndirectArgs`, `NkDispatchIndirectArgs` — l'indirect

Ces trois structs définissent le **layout mémoire exact** des arguments de draw/dispatch quand
c'est le **GPU** qui les remplit (rendu *GPU-driven*). `NkDrawIndirectArgs` : `vertexCount`,
`instanceCount`, `firstVertex`, `firstInstance`. `NkDrawIndexedIndirectArgs` ajoute `indexCount`,
`firstIndex` et `vertexOffset` (signé). `NkDispatchIndirectArgs` : `groupsX`/`groupsY`/`groupsZ`.
Usages : un compute de *culling* écrit ces structs dans un tampon, puis un draw indirect dessine
exactement ce qui a survécu — sans aller-retour CPU. Idéal pour des dizaines de milliers d'objets,
des particules, un dispatch dont le nombre de groupes dépend d'un comptage GPU.

### `NkSwapchainDesc`, `NkBindlessHeapDesc`, `NkSubmitInfo` — présentation, bindless, synchro

`NkSwapchainDesc` décrit la **chaîne d'images** présentée à l'écran : `width`/`height` (0 = taille
fenêtre), `colorFormat` (`NK_BGRA8_UNORM`), `depthFormat`, `samples`, `imageCount` (2 = double
buffer, 3 = triple), `vsync`, `hdr`. C'est ce qu'on (re)crée au démarrage et à chaque redimensionnement
de fenêtre.

`NkBindlessHeapDesc` dimensionne le **tas bindless** : combien de textures échantillonnées
(`maxSampledTextures=65536`), de storage textures, d'uniform/storage buffers, de samplers. Le
bindless permet à un shader d'indexer dynamiquement un immense tableau de ressources (matériaux,
textures de scène) sans rebind par draw.

`NkSubmitInfo` décrit une **soumission** synchronisée : les `commandBuffers` à exécuter, les
`waitSemaphores` (+ `waitStages`, un par sémaphore) qui retardent le départ, les `signalSemaphores`
émis à la fin, et la `fence` signalée quand le GPU a fini (que le CPU peut attendre). C'est le cœur
de la synchro GPU↔GPU (chaîner deux passes sur deux queues) et GPU↔CPU (savoir quand une frame est
terminée). **Piège** : ce sont des **pointeurs bruts non-owning** vers des tableaux dont l'appelant
doit garder la durée de vie pendant toute la soumission.

### Pièges transverses

- Les membres `NkVector<...>` allouent via NKContainers (donc NKMemory) ; **copier un `*Desc` copie
  ses vecteurs**. Les `const char* debugName` et les sources sont **non-owning** : leur durée de vie
  est à la charge de l'appelant.
- `NkBlendDesc()` pousse **toujours** un attachment Opaque par défaut — ne re-`PushBack` pas un
  Opaque par mégarde (double attachment).
- `NkShaderDesc::AddSWVertex/Fragment/Compute` : **double `PushBack`** + **`new` brut** — comportement
  à surveiller (voir section Shaders).
- `NkBufferTextureCopyRegion`/`NkTextureCopyRegion` : `width` et `height` **sans défaut**, à toujours
  renseigner.
- `NkSampleCount`, tous les enums d'état/format/stage et tous les `Nk*Handle` sont définis dans
  `NkTypes.h`, **pas ici**.
- Aucune méthode de ce header n'est `noexcept`/`nodiscard` ; les statics sont O(1) sauf celles qui
  construisent des `NkVector` (elles allouent via `PushBack`).

---

### Exemple

```cpp
#include "NKRHI/Core/NkDescs.h"
using namespace nkentseu;

// Un tampon de sommets immuable + un uniform réécrit chaque frame.
NkBufferDesc vb = NkBufferDesc::Vertex(sizeof(verts), verts);
NkBufferDesc ub = NkBufferDesc::Uniform(sizeof(FrameConstants));

// Une texture albedo sRGB avec mips, et son sampler anisotrope.
NkTextureDesc tex = NkTextureDesc::Tex2D(1024, 1024, NK_RGBA8_SRGB, /*mips*/0);
NkSamplerDesc smp = NkSamplerDesc::Anisotropic(16.f);

// La forme du sommet : position + UV (un seul stream per-vertex).
NkVertexLayout layout;
layout.AddBinding(0, sizeof(Vertex))
      .AddAttribute(0, 0, NK_RGB32_FLOAT, offsetof(Vertex, pos), "POSITION")
      .AddAttribute(1, 0, NK_RG32_FLOAT,  offsetof(Vertex, uv),  "TEXCOORD");

// Une passe forward (1 color + depth) avec présentation finale.
NkRenderPassDesc pass = NkRenderPassDesc::Forward();
pass.finalForPresent = true;   // dernier pass vers la swapchain (Vulkan)

// Le pipeline graphique : shader + layout + états + render pass.
NkGraphicsPipelineDesc pipe;
pipe.vertexLayout = layout;
pipe.blend        = NkBlendDesc::Alpha();          // transparence
pipe.depthStencil = NkDepthStencilDesc::ReadOnly(); // teste mais n'écrit pas
pipe.rasterizer   = NkRasterizerDesc::NoCull();
pipe.debugName    = "ForwardTransparent";
// pipe.shader / pipe.renderPass = handles renvoyés par le device.
```

---

[← Index NKRHI](README.md) · [Récap NKRHI](../NKRHI.md) · [Couche Runtime](../README.md)
