# Matériaux et shaders

> Couche **Runtime** · NKRenderer · Décrire **l'apparence** d'une surface (couleur, métal, rugosité,
> contour toon…) et le **code GPU** qui la calcule : la classe haute `NkMaterial`, l'instance bas
> niveau `NkMaterialInstance`, l'usine `NkMaterialSystem`, les assets `.nkasset`, et toute la chaîne
> de shaders (`NkShaderLibrary`, backends de compilation, transpileur NkSL).

Un maillage tout seul ne sait pas de quoi il a l'air. Il connaît sa **forme** — des sommets, des
normales, des coordonnées de texture — mais pas s'il est en or poli, en peau, en verre ou dessiné au
crayon. C'est le rôle du **matériau** : il dit *comment la lumière interagit avec la surface*, et il
porte le **code GPU** (les shaders) qui transforme cette description en pixels. NKRenderer sépare
nettement ces deux mondes : les **matériaux** (ce que vous réglez : couleurs, métal, contours) et
les **shaders** (le code qui les exécute), reliés par un système de templates et de pipelines.

Le piège habituel, c'est de croire qu'il faut écrire du GLSL pour changer une couleur. Ce n'est
**pas** le cas : NKRenderer fournit une vingtaine de **types de matériaux** prêts à l'emploi (PBR
métallique, toon, peau, verre, eau, sketch…), et vous n'écrivez du shader que pour un effet vraiment
custom. Tout le reste se règle par des **setters fluents** à la manière d'Unreal — `mat->SetAlbedo(...)
->SetRoughness(...)`. Cette page vous apprend d'abord à *utiliser* les matériaux, puis à descendre
vers les couches qui les font tourner.

- **Namespace** : `nkentseu::renderer` (les handles RHI — `NkIDevice`, `NkICommandBuffer`,
  `NkShaderHandle` — sont au namespace parent `::nkentseu::`)
- **Headers** : `NKRenderer/Materials/NkMaterial.h`, `NkMaterialSystem.h`, `NkMaterialCollection.h`,
  `NkMaterialAsset.h`, `NkMaterialLibrary.h` · `NKRenderer/Shader/NkShaderBackend.h`,
  `NkShaderLibrary.h`, `NkShaderIncludeResolver.h`

---

## L'objet manipulable : `NkMaterial`

C'est la porte d'entrée, celle qu'on prend quand on ne veut pas penser au GPU. `NkMaterial` est un
**objet unique manipulable** qui encapsule à la fois un *template* (le type de matériau et son
pipeline) et une *instance* (vos réglages personnels) — l'équivalent direct d'un
`UMaterialInstanceDynamic` d'Unreal. On le fabrique, on le règle, on l'attache à un objet à dessiner.

On ne le construit **jamais** avec `new` : il s'alloue par NKMemory via la fabrique statique
`Create`, et se libère par `Destroy` (qui remet votre pointeur à `null`). Choisir le type, c'est
choisir tout le modèle d'éclairage d'un coup — `NK_PBR_METALLIC` pour une surface réaliste,
`NK_TOON` pour du cel-shading, `NK_UNLIT` pour ignorer la lumière.

```cpp
NkMaterial* gold = NkMaterial::Create(matSys, NK_PBR_METALLIC);
gold->SetAlbedo({ 1.f, 0.78f, 0.34f })   // couleur or
    ->SetMetallic(1.f)                   // pleinement métallique
    ->SetRoughness(0.15f);               // poli
// ... drawCall.material = gold->GetInstHandle();
NkMaterial::Destroy(gold);               // gold devient nullptr
```

Tous les setters renvoient `NkMaterial*`, ce qui autorise le **chaînage fluent** ci-dessus. À côté
des raccourcis sémantiques (`SetMetallic`, `SetRoughness`, `SetOutline`…), il existe une famille de
**paramètres nommés** (`SetFloat("monParam", 0.5f)`) pour piloter n'importe quel uniform custom, et
des **wrappers UE5** (`SetScalarParameterValue`, `SetVectorParameterValue`…) qui sont des alias
*zero-cost* des précédents — utiles pour qui vient d'Unreal.

Ce n'est **pas** un objet jetable qu'on recrée chaque frame : on le crée une fois, on le règle, on
le réutilise. Et ce n'est **pas** non plus l'instance bas niveau — `NkMaterial` est un *wrapper*
au-dessus de `NkMaterialInstance`, pensé pour le confort.

> **En résumé.** `NkMaterial` = l'objet matériau « tout-en-un », façon `UMaterialInstanceDynamic`.
> `Create(sys, type)` / `Destroy(mat)` (jamais `new`/`delete`), setters fluents enchaînables,
> raccourcis PBR/Toon + paramètres nommés + wrappers UE5. Le point d'entrée pour 90 % des cas.

---

## Hériter d'un parent : `CreateChild` et les overrides

Très vite, on veut **plusieurs variantes d'un même matériau** : le même métal en rouge, en bleu, en
usé. Recréer dix matériaux complets serait gâché — `CreateChild` crée un **enfant** qui *hérite* de
tous les paramètres du parent, et ne stocke que ce que vous changez. C'est le mécanisme M.4 de
**live link** : modifier le parent se propage automatiquement aux enfants… **sauf** sur les champs
qu'un enfant a explicitement *overridés*.

```cpp
NkMaterial* base  = NkMaterial::Create(matSys, NK_PBR_METALLIC);
base->SetRoughness(0.3f);
NkMaterial* red   = NkMaterial::CreateChild(base);
red->SetAlbedo({ 1.f, 0.f, 0.f });    // override de la couleur seulement
base->SetRoughness(0.8f);             // 'red' suit (roughness non overridée)
red->ResetParameter("albedo");        // re-link : 'red' redevient comme le parent
```

La règle est précise : un setter sur le parent ne touche un enfant que si le champ correspondant
**n'a pas** été overridé chez lui. Pour annuler un override et **resynchroniser** avec le parent, on
appelle `ResetParameter(name)` (paramètre nommé), ou `ResetPBROverride(bit)` / `ResetToonOverride(bit)`
pour les blocs PBR/Toon (où chaque champ correspond à un bit, `NK_PBR_O_ALBEDO`, `NK_TOON_O_OUTLINE`…).

Le **piège** à retenir : le parent doit **survivre** tant qu'il a des enfants vivants — détruire un
parent avant ses enfants laisse ces derniers pointer dans le vide. Ce n'est **pas** une copie
indépendante : un enfant garde un lien vivant vers son parent.

> **En résumé.** `CreateChild(parent)` = variante héritée (live link M.4) : l'enfant suit le parent
> sauf sur les champs overridés ; `ResetParameter`/`ResetPBROverride`/`ResetToonOverride` re-lient au
> parent. **Le parent doit rester vivant tant qu'il a des enfants.**

---

## L'usine et les instances : `NkMaterialSystem`

Sous le confort de `NkMaterial` se trouve la vraie machinerie : `NkMaterialSystem` détient les
**templates** (types de matériaux compilés en pipelines GPU) et les **instances** (jeux de
paramètres). C'est lui qu'on initialise une fois au démarrage avec le device et les bibliothèques de
textures et de shaders, et qu'on éteint en fin de vie.

L'instance bas niveau, `NkMaterialInstance`, ne se crée **que** via `NkMaterialSystem::CreateInstance`
(son constructeur n'est pas public) — c'est l'objet que `NkMaterial` enveloppe. On descend rarement à
ce niveau, sauf pour du code moteur ou quand on veut piloter finement le descriptor set GPU.

```cpp
NkMaterialSystem matSys;
matSys.Init(device, texLib, shaderLib, NK_GFX_API_VULKAN);
// après l'Init de NkRender3D, avant tout bind :
matSys.SetSharedContext(globalLayout, objectLayout, vertexLayout, renderPass);

NkMaterialInstance* inst = matSys.CreateInstance(matSys.DefaultPBR());
inst->SetAlbedo({ 0.2f, 0.6f, 1.f })->SetRoughness(0.4f);
```

Un détail d'ordre **crucial** : `SetSharedContext` doit être appelé *après* l'init de `NkRender3D` et
*avant* tout `BindInstance` — il transmet les layouts globaux, le layout de sommets et la render pass
sans lesquels les pipelines ne peuvent pas se compiler. Sur Vulkan, un redimensionnement de fenêtre
impose en plus un `UpdateRenderPass(rp)`. Ce n'est **pas** optionnel : oublier `SetSharedContext`
fait échouer le premier bind.

> **En résumé.** `NkMaterialSystem` = l'usine `Init`/`Shutdown` qui gère templates + instances +
> pipelines. `CreateInstance`/`DestroyInstance` pour le bas niveau ; **`SetSharedContext` obligatoire
> avant tout bind**, `UpdateRenderPass` au resize Vulkan. Onze templates built-in (`DefaultPBR()`,
> `DefaultToon()`, `DefaultLayeredV1()`…) prêts à instancier.

---

## Paramètres partagés et assets disque

Deux besoins reviennent toujours : partager une valeur entre **plein de matériaux** (l'heure du jour,
une teinte d'équipe), et **sauver un matériau sur disque** pour l'éditer ou le recharger à chaud.

`NkMaterialCollection` répond au premier : c'est un **pool de paramètres nommés global**, un seul UBO
partagé (set 0, binding 25), limité à 64 paramètres. On y écrit `SetFloat("dayTime", 0.7f)` une fois,
et *tous* les shaders y accèdent. Au second besoin répondent `NkMaterialAsset` (le payload `.nkasset`
sérialisable d'un matériau) et `NkMaterialLibrary` (le cache qui scanne un dossier, charge par chemin
logique, et fait du **hot-reload**).

```cpp
NkMaterialCollection mpc;
mpc.Init(device);
mpc.SetVec3("teamColor", { 1.f, 0.2f, 0.2f });   // visible par tous les shaders

NkMaterialLibrary lib;
lib.Init(device, &matSys, &texLib);
lib.ScanDirectory("Resources/Materials");        // indexe les .nkasset
NkMatInstHandle gold = lib.Load("/Materials/Metals/Gold");
lib.EnableHotReload(true);                        // édité sur disque → rechargé
```

Ce n'est **pas** au matériau de gérer ces valeurs partagées : la collection existe précisément pour
éviter de dupliquer une même donnée dans 200 instances. Et la bibliothèque ne *charge* pas tout au
scan — elle indexe seulement ; les payloads (et leurs textures) ne se résolvent qu'au `Load`, puis au
`Bind`.

> **En résumé.** `NkMaterialCollection` = paramètres globaux partagés (1 UBO, max 64, binding 25).
> `NkMaterialAsset` = un matériau sur disque (`.nkasset`, sérialisable). `NkMaterialLibrary` = scan +
> cache ref-counted + hot-reload des assets. Le scan **indexe** ; le `Load` charge ; le `Bind` résout
> les textures.

---

## Le code GPU : shaders et NkSL

Tout en bas, ce sont des **shaders** qui calculent réellement chaque pixel. `NkShaderLibrary` est le
cache central : on lui demande de charger un couple vertex+fragment (`LoadVF`) ou de compiler depuis
une source (`CompileVF`), et il rend un `NkShaderHandle` RHI directement utilisable dans un pipeline.
Il gère aussi le **hot-reload** (recompiler quand un fichier change).

La compilation passe par un **backend** par API : `NkShaderBackendGL` (GLSL), `…VK` (GLSL → SPIR-V),
`…DX11`/`…DX12` (HLSL), `…MSL` (Metal). Au-dessus de tout cela, le **NkSL** est le langage de shader
maison : un seul source `.nksl` que `NkShaderBackendNkSL` *transpile* vers l'API cible — écrire une
fois, tourner partout.

```cpp
NkShaderLibrary shaders;
shaders.Init(device, NK_GFX_API_VULKAN, /*useNkSL=*/true);

::nkentseu::NkShaderHandle h =
    shaders.LoadVF("Shaders/water.vert", "Shaders/water.frag", "water");
// h se passe tel quel à NkGraphicsPipelineDesc::shader
shaders.PollHotReload();   // chaque frame : recompile ce qui a changé
```

Un **piège de namespace** à connaître : dans toute cette couche shader, les handles sont des
`::nkentseu::NkShaderHandle` (RHI) — toujours **qualifier** pour ne pas les confondre avec un type
renderer-side. Et `NkShaderIncludeResolver` inline récursivement les `#include "...glsli"` *avant*
compilation : les fichiers `.glsli` sont des fragments réutilisables, sans `#version` ni `main()`.

> **En résumé.** `NkShaderLibrary` = cache `Init`/`Shutdown` qui charge/compile des shaders et rend
> des `::nkentseu::NkShaderHandle` RHI prêts pour le pipeline, avec hot-reload. Les backends compilent
> par API ; **NkSL** transpile un source unique vers toutes les API ; `NkShaderIncludeResolver` gère
> les `#include`. Toujours qualifier `::nkentseu::NkShaderHandle`.

---

## Aperçu de l'API

Tous les types ci-dessous sont au namespace `nkentseu::renderer` (handles RHI exceptés). Chaque
élément est détaillé dans la « Référence complète ».

### Matériaux — types et paramètres (`NkMaterialSystem.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `enum class NkMaterialType` | Famille du matériau : PBR (`NK_PBR_METALLIC`…), NPR (`NK_TOON`…), debug (`NK_UNLIT`…), layered, custom, 2D. |
| Test | `NkMaterialIsType2D(t)` | Vrai si le type est 2D (`>= 120 && < 200`) → choisit le layout 2D vs 3D. |
| État pipeline | `enum class NkCullMode` (`NK_BACK`/`FRONT`/`NONE`), `NkFillMode` (`NK_SOLID`/`WIREFRAME`) | Faces cachées et remplissage. |
| Masques layered | `enum NkLayerMaskSource` | Source du masque de mélange (`NK_LAYER_MASK_VCOLOR_R`…`_LAYER_ALPHA`). |
| Bits override | `enum NkPBROverrideBit`, `NkToonOverrideBit` | Champs PBR/Toon overridables (bitfield `1u<<N`). |
| Params GPU | `NkPBRParams`, `NkToonParams` | Blocs std140 PBR / Toon (alignés `16`, taille fixe). |
| Params layered | `NkLayeredParams`, `NkPBRLayer`, `NkLayeredV1Params` | Mélange 2 couches / couche unitaire / jusqu'à 8 couches. |
| Template | `NkMaterialTemplateDesc` | Description d'un template (type, état, sources custom par API). |

### Matériaux — objets (`NkMaterial.h`, `NkMaterialSystem.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Objet haut | `NkMaterial` | Wrapper manipulable (template + instance), façon UE5. |
| Instance bas | `NkMaterialInstance` | Instance d'un template (créée par le système). |
| Usine | `NkMaterialSystem` | Gère templates, instances, pipelines ; built-ins. |
| Cycle (haut) | `NkMaterial::Create`, `CreateChild`, `Destroy` | Fabrique NKMemory ; enfant hérité ; destruction (met à `null`). |
| Cycle (bas) | `CreateInstance`, `CreateChildInstance`, `DestroyInstance` | Instances via le système. |
| Setters | `SetAlbedo`, `SetMetallic`, `SetRoughness`, `SetEmissive`, `SetNormalMap`, `SetORMMap`… | Raccourcis PBR fluents. |
| Setters NPR | `SetToonThreshold`, `SetToonSmooth`, `SetOutline`, `SetRim`, `SetMatcapMap`… | Raccourcis Toon/NPR. |
| Nommés | `SetFloat`/`SetVec2..4`/`SetColor`/`SetInt`/`SetBool`/`SetTexture` | Paramètres arbitraires par nom. |
| UE5 | `SetScalarParameterValue`, `SetVectorParameterValue`, `SetTextureParameterValue`, `SetStaticSwitchParameterValue` | Alias zero-cost (compat Unreal). |
| Layered | `SetLayerBase`/`SetLayerTop`/`SetLayerMaskSource` ; `SetLayeredV1`/`SetLayerV1`/`SetLayerV1Mask`/`SetLayerV1Count` | Mélange de couches v0 / v1. |
| Overrides | `ResetParameter`, `ResetPBROverride`, `ResetToonOverride` | Re-link au parent (live link M.4). |
| Shadow/divers | `SetReceiveShadow`, `SetShadowBiasMul`, `SetCastShadowAlphaTest`, `SetTriplanarTileSize` | Ombres per-matériau (VSM v1), texturage triplanaire. |
| État | `IsValid`, `GetQueue`, `GetName`, `GetType`, `GetInstHandle` | Introspection ; handle pour `NkDrawCall3D::material`. |
| Built-ins | `DefaultPBR`, `DefaultToon`, `DefaultUnlit`, `DefaultWireframe`, `DefaultLayeredV1`, `DefaultSkin`, `DefaultHair`, `DefaultAnime`, `DefaultArchviz`, `DefaultReflFloor`, `DefaultLayered` | Templates prêts à instancier. |
| Contexte | `SetSharedContext`, `UpdateRenderPass`, `BindInstance`, `GetPipeline`, `FlushCompilations`, `GetInstanceLayout` | Plomberie pipelines (set=2). |

### Paramètres partagés et assets

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Collection | `NkMaterialCollection` | Pool global de params nommés (1 UBO, max `kMaxParams=64`, `kBinding=25`). |
| Collection — accès | `SetFloat`/`SetVec2..4`/`SetColor`/`SetInt`, `Get`, `GetSlot`, `Upload`, `GetUBO` | Écriture/lecture par nom ; upload UBO. |
| Texture ref | `NkMaterialTextureRef` | Réf texture d'un asset (`slot`, `assetId`, `fallbackPath`). |
| Asset | `NkMaterialAsset` | Payload `.nkasset` sérialisable (`NkISerializable`). |
| Bibliothèque | `NkMaterialLibrary` | Scan/cache/load/hot-reload des `.nkasset`. |
| Biblio — API | `ScanDirectory`, `Load`, `LoadById`, `Reload`, `Save`, `EnableHotReload`, `PollHotReload`, `GetAsset`, `CountLoaded` | Cycle complet des assets disque. |

### Shaders (`NkShaderBackend.h`, `NkShaderLibrary.h`, `NkShaderIncludeResolver.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Étape | `enum class NkShaderStage` | `NK_VERTEX`…`NK_TASK` (8 étapes). |
| Compilation | `NkShaderCompileResult`, `NkShaderCompileOptions` | Sortie (succès/erreurs/bytecode) / options (entry, defines, SM…). |
| Backend | `NkShaderBackend` (interface) | `Compile` / `SupportsHotReload` / `GetBackendName`. |
| Backends | `…GL`, `…VK`, `…DX11`, `…DX12`, `…MSL`, `…NkSL` | Un par API + transpileur NkSL. |
| Fabrique | `NkCreateShaderBackend(api, useNkSL)` | Crée un backend (ownership appelant). |
| Includes | `NkShaderIncludeResolver::Resolve` | Inline les `#include "...glsli"`. |
| Programme | `NkShaderProgram` | Un shader compilé (handles, chemins, bytecode, mtimes). |
| Bibliothèque | `NkShaderLibrary` | Cache + hot-reload ; rend des `::nkentseu::NkShaderHandle` RHI. |
| Biblio — chargement | `LoadVF`, `LoadVGF`, `LoadCompute`, `CompileVF`, `LoadOrCompileVF` | Depuis fichier / source / user-override. |
| Biblio — gestion | `Find`, `Get`, `GetRHIHandle`, `PollHotReload`, `HasPendingReloads`, `Release`, `ReleaseAll` | Accès, hot-reload, libération. |

---

## Référence complète

### `NkMaterialType` — choisir le modèle d'éclairage

C'est l'enum qui détermine **tout** : le modèle d'éclairage, le pipeline, et même si le matériau est
2D ou 3D. Les valeurs sont regroupées par familles avec des **plages numériques** stables (utilisées
par `NkMaterialIsType2D` et par le système pour aiguiller le rendu) :

- **Réaliste (PBR), à partir de 0** — `NK_PBR_METALLIC` (le défaut, workflow métal/rugosité),
  `NK_PBR_SPECULAR`, plus des spécialisations : `NK_ARCHIVIZ`, `NK_SKIN`, `NK_HAIR`, `NK_GLASS`,
  `NK_CLOTH`, `NK_CAR_PAINT`, `NK_FOLIAGE`, `NK_WATER`, `NK_TERRAIN`, `NK_EMISSIVE`, `NK_VOLUME`,
  `NK_REFL_FLOOR` (sol miroir).
- **NPR (non photoréaliste), à partir de 20** — `NK_TOON`, `NK_TOON_INK`, `NK_ANIME`,
  `NK_WATERCOLOR`, `NK_SKETCH`, `NK_PIXEL_ART`, `NK_FLAT`, `NK_UPBGE_EEVEE`. Le cel-shading, l'aquarelle,
  le crayonné, le pixel art.
- **Debug, à partir de 60** — `NK_UNLIT` (ignore la lumière), `NK_DEBUG_NORMALS`, `NK_DEBUG_UV`,
  `NK_WIREFRAME_MAT`, `NK_DEBUG_DEPTH`, `NK_DEBUG_AO`. Pour visualiser une donnée brute pendant le
  développement.
- **Layering, à partir de 80** — `NK_LAYERED`, `NK_LAYERED_V1` : mélanger plusieurs matériaux selon un
  masque.
- **Custom (100)** — `NK_CUSTOM` : vous fournissez vos propres sources de shaders dans le template.
- **2D, à partir de 120** — `NK_SPRITE_2D`, `NK_GLOW_2D` : sprites et effets de glow pour le rendu 2D.

Cas d'usage par domaine : **rendu** réaliste (PBR), **stylisé / gameplay** (toon, anime, sketch pour
un look cartoon), **éditeur / outils** (les types debug pour inspecter normales, UV, profondeur, AO),
**UI / 2D** (`NK_SPRITE_2D`, `NK_GLOW_2D`). La fonction libre `NkMaterialIsType2D(t)` (vrai pour
`120 ≤ t < 200`) sert au moteur à choisir le bon vertex/pipeline layout — un sprite n'a pas le même
format de sommet qu'un maillage éclairé.

### `NkCullMode`, `NkFillMode` — l'état du pipeline

Deux petits enums qui pilotent le rasterizer. `NkCullMode` (`NK_BACK` par défaut, `NK_FRONT`,
`NK_NONE`) décide quelles faces sont éliminées : on cache l'arrière des objets opaques (`NK_BACK`),
ou on désactive le culling (`NK_NONE`) pour du verre, du feuillage, des plans doubles-faces.
`NkFillMode` choisit entre rendu plein (`NK_SOLID`) et **fil de fer** (`NK_WIREFRAME`) — ce dernier
est précieux côté **outils/éditeur** pour visualiser la topologie d'un maillage. (Les enums
`NkRenderQueue` et `NkBlendMode`, eux, vivent dans `Core/NkRendererTypes.h`, pas dans ces headers.)

### `NkLayerMaskSource`, `NkPBROverrideBit`, `NkToonOverrideBit` — les enums de contrôle

`NkLayerMaskSource` désigne **d'où vient le masque** qui mélange deux couches d'un matériau layered :
une composante de couleur de sommet (`NK_LAYER_MASK_VCOLOR_R`/`G`/`B`/`A`), une coordonnée de texture
(`_UV_X`/`_UV_Y`), une constante (`_CONSTANT`), ou l'alpha de la couche (`_LAYER_ALPHA`). On choisit,
par exemple, de peindre la rouille via la couleur de sommet (`_VCOLOR_R`) — un usage typique du
*vertex painting* en **outils de level design**.

`NkPBROverrideBit` et `NkToonOverrideBit` sont des **bitfields** (`1u << N`) qui repèrent quel champ
d'un matériau enfant a été overridé : `NK_PBR_O_ALBEDO`, `_METALLIC`, `_ROUGHNESS`, `_NORMAL_STR`,
`_CLEARCOAT`, `_SUBSURFACE`, `_ANISOTROPY`, `_SHEEN`, `_REFL_FLOOR`… côté PBR ; `NK_TOON_O_ALBEDO`,
`_SHADOW_TH`, `_OUTLINE`, `_RIM`, `_MATCAP`… côté Toon. Ce sont eux qu'on passe à `ResetPBROverride` /
`ResetToonOverride` pour re-lier un champ précis à son parent.

### Les structs de paramètres GPU — `NkPBRParams`, `NkToonParams`, layered

Ce sont les blocs **std140** que le shader lit directement (alignés `16`, tailles fixes — leur layout
doit matcher *exactement* les UBO des shaders). On les manipule rarement à la main (les setters s'en
chargent), mais les comprendre aide au debug GPU.

- **`NkPBRParams`** (96 B) — le cœur du PBR : `albedo` (RGBA), `emissive`, et les scalaires
  `metallic`, `roughness`, `ao`, `emissiveStrength`, `normalStrength`, plus les effets avancés
  `clearcoat`/`clearcoatRough` (vernis), `subsurface`/`subsurfaceColor` (peau, cire, feuillage —
  diffusion sous la surface), `anisotropy` (cheveux, métal brossé), `sheen` (velours, tissu). Le
  champ `reflFloorFaceMode` (réutilisant `_pad[0]` : 0=FrontOnly, 1=BackOnly, 2=Both) gère le sol
  miroir.
- **`NkToonParams`** — les réglages du cel-shading : `albedoColor`, `shadowColor`, les seuils
  `shadowThreshold`/`shadowSmooth` (où passe l'ombre et sa douceur), `outlineWidth`/`outlineColor`
  (le contour), `rimIntensity`/`rimColor` (la lumière de bord), `specHardness`, `matcapStrength`.
- **`NkPBRLayer`** (32 B) — une couche PBR minimale (`albedo` avec A = usage du masque, `metallic`,
  `roughness`), brique de base du layering v1.
- **`NkLayeredParams`** (208 B) — le layering **v0** : une couche `base`, une couche `top`, et un
  `maskSource` (0=R, 1=G, 2=B, 3=A) qui dit comment les mélanger.
- **`NkLayeredV1Params`** (336 B) — le layering **v1**, jusqu'à 8 couches (`layers[8]`), avec sources
  de masque et constantes par couche (`maskSources0/1`, `maskConstants0/1`) et un `numLayers`. C'est
  ce qui permet un **terrain** multi-textures ou un objet décoré de plusieurs matériaux fondus.

### `NkMaterialTemplateDesc` — décrire un template

La struct qu'on remplit pour enregistrer un *nouveau* template via `NkMaterialSystem::RegisterTemplate`.
Elle porte le `type`, l'état pipeline complet (`queue`, `blendMode`, `cullMode`, `fillMode`,
`depthWrite`, `depthTest`, `doubleSided`), un `name`, et — uniquement si `type == NK_CUSTOM` — les
**sources de shaders par API** : `vertSrcGL`/`fragSrcGL`, `…VK`, `…DX11`, `…DX12`, `…MSL`, ou un
`nkslSource` unique transpilé pour toutes. C'est la voie pour un effet entièrement maison (un shader
de portail, de bouclier énergétique, de distorsion) qui ne rentre dans aucune famille standard.

### `NkMaterial` à fond — l'API haute, Unreal-style

`NkMaterial` est conçu pour qu'on n'ait *jamais* à penser au GPU pour un matériau standard. Quelques
piliers :

- **Cycle de vie.** `Create(sys, type)` ou `Create(sys, templateName)` allouent par NKMemory ;
  `CreateChild(parent)` fabrique une variante héritée ; `Destroy(mat)` libère et met `mat` à `null`.
  Ne jamais `delete` directement, ne jamais détruire un parent avant ses enfants.
- **Setters fluents.** Tous renvoient `NkMaterial*`, d'où le chaînage. Trois familles : les
  **raccourcis sémantiques** (`SetAlbedo`, `SetMetallic`, `SetRoughness`, `SetEmissive`,
  `SetNormalMap`, `SetORMMap` — AO+Roughness+Metallic en une texture —, `SetSubsurface`,
  `SetClearcoat` côté PBR ; `SetToonThreshold`, `SetOutline`, `SetRim`, `SetMatcapMap` côté toon) ;
  les **paramètres nommés** (`SetFloat`, `SetVec2..4`, `SetColor`, `SetInt`, `SetBool`, `SetTexture`)
  pour piloter n'importe quel uniform custom ; et les **wrappers UE5** zero-cost
  (`SetScalarParameterValue` → `SetFloat`, `SetVectorParameterValue` → `SetVec4`, etc.) pour la
  familiarité.
- **Layering** (`SetLayerBase`/`SetLayerTop`/`SetLayerMaskSource`) pour le mélange de couches v0.
- **Ombres per-matériau (NkVSM v1)** : `SetReceiveShadow`, `SetShadowBiasMul` (ajuster l'acné
  d'auto-ombrage), `SetCastShadowAlphaTest` (réservé v1), avec leurs getters const. Utile quand un
  objet précis doit ignorer les ombres ou corriger un artefact local.
- **Triplanar** (`SetTriplanarTileSize`, en mètres ; 0 = désactivé) projette une texture selon les
  trois axes du monde sans UV — idéal pour des **terrains** ou des rochers générés où dessiner des UV
  serait pénible.
- **État** : `IsValid`, `GetQueue`, `GetName`, `GetType`, et surtout `GetInstHandle()` qui fournit le
  `NkMatInstHandle` à coller dans `NkDrawCall3D::material`. La méthode `Bind(cmd, texLib)` est interne
  (le moteur l'appelle pour vous).

Cas d'usage transverses : **rendu** (régler n'importe quelle surface), **gameplay/IA** (changer la
couleur d'un ennemi en alerte via `SetEmissive`, faire clignoter un objet ramassable),
**animation** (animer un paramètre nommé au fil du temps — dissolution, fondu), **outils/éditeur** (un
inspecteur qui appelle `SetFloat`/`SetColor` selon ce que l'utilisateur règle), **UI/2D** (matériaux
`NK_SPRITE_2D`/`NK_GLOW_2D`).

### `NkMaterialInstance` à fond — le bas niveau

C'est l'objet réellement lié au GPU (un descriptor set, un UBO de params). On ne le crée que via
`NkMaterialSystem::CreateInstance` / `CreateChildInstance` ; `NkMaterial` l'enveloppe pour le confort.
Ses setters reprennent les mêmes familles que `NkMaterial` (raccourcis PBR/Toon, paramètres nommés)
mais renvoient `NkMaterialInstance*`. En plus, il expose des **getters** sur l'état brut
(`GetPBR()`, `GetToon()`, `GetLayered()`, `GetLayeredV1()`, `GetDescSet()`, `IsDirty()`/`MarkClean()`
pour la gestion du *dirty flag*, `GetParent()`/`GetChildCount()` pour la hiérarchie) et l'API layered
v1 complète (`SetLayerV1`, `SetLayerV1Mask`, `SetLayerV1Count`). On descend ici pour du **code
moteur** : un système ECS qui touche directement les instances, ou un éditeur qui inspecte le
descriptor set.

### `NkMaterialSystem` à fond — l'usine

`Init(device, texLib, shaderLib, api)` câble le système ; `Shutdown()` libère tout. Côté
**templates** : `RegisterTemplate`, `FindTemplate(name)`, `GetTemplateName`, `GetTemplateType`. Côté
**instances** : `CreateInstance`, `CreateChildInstance` (descSet/UBO indépendants mais params copiés
du parent, avec live link), `DestroyInstance` (met à `null`), `GetInstance(handle)`.

Le bloc **contexte/pipelines** est la partie sensible : `SetSharedContext(globalLayout, objectLayout,
vertexLayout, rp)` transmet ce que les pipelines ont besoin de connaître — **à appeler après l'Init de
`NkRender3D` et avant tout `BindInstance`**. `UpdateRenderPass(rp)` est requis au resize Vulkan.
`BindInstance(cmd, inst)` met à jour le descset si dirty puis le lie ; `GetPipeline(tmpl, rp)` compile
le pipeline à la demande (lazy) ; `FlushCompilations()` force la fin des compilations en attente ;
`GetInstanceLayout()` rend le layout du descriptor set per-instance (set=2).

Les **built-ins** (`DefaultPBR()`, `DefaultToon()`, `DefaultUnlit()`, `DefaultWireframe()`,
`DefaultLayeredV1()`, `DefaultSkin()`, `DefaultHair()`, `DefaultAnime()`, `DefaultArchviz()`,
`DefaultReflFloor()`, `DefaultLayered()`) renvoient des `NkMatHandle` prêts à `CreateInstance` — la
voie rapide pour obtenir une surface correcte sans rien enregistrer. Enfin, le bloc **bibliothèque**
(`GetLibrary()`/`SetLibrary()`, non-owning) relie le système au `NkMaterialLibrary` créé par
`NkRendererImpl`.

### `NkMaterialCollection` à fond — les paramètres partagés (M.2)

Un **pool global** de paramètres nommés, matérialisé par un seul UBO (set 0, binding `kBinding=25`),
plafonné à `kMaxParams=64` paramètres (1 KiB). `Init(device)` / `Shutdown()` l'encadrent. On y écrit
par nom (`SetFloat`, `SetVec2..4`, `SetColor` — alias de `SetVec4` —, `SetInt` ; un float occupe
`.x`, un vec3 `.xyz`, un vec4 ses 4 composantes), on relit avec `Get(name)` (zéro si inconnu) et
`GetSlot(name)`. `Upload()` écrit l'UBO si dirty (appelé automatiquement par `NkRender3D` en début de
frame), `GetUBO()` rend le handle de buffer.

Usages : **gameplay/monde** (heure du jour, météo, intensité globale d'un effet partagés par tous les
matériaux), **outils** (un panneau global qui pilote un paramètre artistique à l'échelle de la scène).
Le **piège** : limite stricte de 64 params, et le hash est calculé sur le **contenu** (FNV-1a via
`NkHash<NkString>`), pas sur des octets bruts.

### `NkMaterialTextureRef` et `NkMaterialAsset` — la sérialisation

`NkMaterialTextureRef` décrit **une** texture d'un matériau-asset : un `slot` fonctionnel (la chaîne
`"albedo"`, `"normal"`, `"orm"`, `"emissive"`, `"matcap"`, `"shadow_ramp"`…), un `assetId` (réf forte
vers un Texture2D `.nkasset`), et un `fallbackPath` (chemin direct si l'assetId est nul ou
introuvable). Il sait se `Serialize`/`Deserialize` dans un `NkArchive`.

`NkMaterialAsset` est le **payload disque complet** d'un matériau (`: public NkISerializable`,
`GetTypeName()` → `"NkMaterialAsset"`, type asset `NkAssetType::Material`). Il porte tout : `type`,
`name`, l'état pipeline (`queue`, `blendMode`, `cullMode`, `fillMode`, `depthWrite`/`depthTest`/
`doubleSided`), les blocs `pbr` et `toon`, un vecteur de `textures`, et la référence à un shader custom
(`customShaderAssetId`, `customShaderDir`). Le `type` détermine quel bloc est utilisé (Unlit/Debug
retombent sur `pbr.albedo`). Les textures sont en **résolution différée** : leurs `NkAssetId` ne se
résolvent qu'au bind, pas au chargement. C'est la pièce maîtresse côté **IO/outils** : tout ce qu'un
éditeur de matériaux sauve et recharge.

### `NkMaterialLibrary` à fond — cache et hot-reload (Phase G)

La bibliothèque orchestre les assets `.nkasset` sur disque. `Init(device, materialSystem,
textureLibrary)` / `Shutdown()` ; `IsValid()` (vrai si device non-null).

- **Scan** — `ScanDirectory(rootDir)` parcourt récursivement le dossier, enregistre les `.nkasset` de
  type Material dans `NkAssetRegistry::Global()` **sans charger** les payloads, et retourne le nombre
  d'assets indexés. Démarrage rapide : on indexe d'abord, on charge à la demande.
- **Load** — `Load(logicalPath)` charge par chemin logique (`/Materials/Metals/Gold`) avec un cache
  **ref-counted** (même chemin → même handle) ; `LoadById(id)` est plus rapide. `Reload(id)` force un
  rechargement disque et **patche l'instance existante sur place** (le handle est préservé — les objets
  qui s'y réfèrent voient la mise à jour).
- **Hot-reload** — `EnableHotReload(bool)` branche un `NkFileWatcher` (no-op si invalide),
  `PollHotReload(dt)` est le fallback par polling de *mtime* (throttle 1 Hz). Édition d'un matériau
  dans un éditeur externe → mise à jour live dans le jeu.
- **Save** — `Save(asset, outPath, outId)` écrit un `NkMaterialAsset` et génère un `NkAssetId` si
  besoin. La voie d'export d'un éditeur.
- **Introspection** — `CountLoaded()`, `GetAsset(id)`.

Cas d'usage : **pipeline d'assets/outils** (un éditeur de matériaux), **itération artiste** (le
hot-reload supprime le cycle compiler-relancer), **IO** (charger une bibliothèque de matériaux au
chargement d'un niveau).

### Shaders — `NkShaderStage`, `NkShaderCompileResult`, `NkShaderCompileOptions`

`NkShaderStage` énumère les huit étapes programmables : `NK_VERTEX`, `NK_FRAGMENT`, `NK_GEOMETRY`,
`NK_COMPUTE`, `NK_TESS_CTRL`, `NK_TESS_EVAL`, `NK_MESH`, `NK_TASK` — du pipeline classique jusqu'au
*mesh shading* moderne et au **compute** (post-processing, simulation GPU, culling). Une compilation
prend des `NkShaderCompileOptions` (`entryPoint`, `defines` au format `"D1=1;D2;…"`, `debug`,
`optimize`, `generateSPIRV`, `smMajor`/`smMinor` pour le *shader model*) et rend un
`NkShaderCompileResult` (`success`, `errors`, `bytecode` SPIR-V/DXBC/DXIL ou source, `preprocessed`).

### `NkShaderBackend` et ses backends — compiler par API

L'interface `NkShaderBackend` expose trois méthodes : `Compile(source, stage, opts)`,
`SupportsHotReload()`, `GetBackendName()`. Cinq backends `final` couvrent les API : `NkShaderBackendGL`
(GLSL 4.60, hot-reload **oui**), `NkShaderBackendVK` (GLSL → SPIR-V, **non**), `NkShaderBackendDX11`
(HLSL SM5, **oui**), `NkShaderBackendDX12` (HLSL SM6 via DXC, **non**), `NkShaderBackendMSL` (Metal
MSL 2.x, **non**). Le sixième, `NkShaderBackendNkSL`, est le **transpileur** : son constructeur prend
l'API cible (`explicit NkShaderBackendNkSL(targetApi)`), et sa méthode `Transpile(nkslSource, target)`
convertit un source NkSL vers le langage du backend visé — c'est lui qui réalise la promesse « écrire
un shader, le faire tourner partout ». La fabrique libre `NkCreateShaderBackend(api, useNkSL)` alloue
le bon backend (ownership à l'appelant). Concrètement, c'est de la **plomberie GPU** : on s'en sert
rarement directement, `NkShaderLibrary` l'utilise pour vous.

### `NkShaderIncludeResolver` — la modularité des shaders (M.5)

Classe utilitaire 100 % statique. `Resolve(source, currentFilePath)` inline **récursivement** les
directives `#include "...glsli"` / `<...glsli>` avant compilation : une inclusion introuvable devient
un commentaire d'erreur + warning, et l'anti-cycle saute silencieusement les doubles inclusions. La
recherche suit trois règles : chemin absolu `Resources/` tel quel ; `Include/...` relatif à
`Resources/NKRenderer/Shaders/` ; nom nu cherché dans `Resources/NKRenderer/Shaders/Include/`. Les
`.glsli` sont des **fragments** (fonctions de bruit, BRDF, helpers) sans `#version` ni `main()` — la
factorisation du code shader, exactement comme un `#include` C.

### `NkShaderProgram` et `NkShaderLibrary` à fond

`NkShaderProgram` représente un shader compilé : un `handle` (ID renderer-side, clé du cache et du
hot-reload), un `rhiHandle` (le handle RHI pour les pipelines), un `name`, les chemins par étape
(`vertPath`/`fragPath`/`geomPath`/`compPath`), le `bytecode` par étape, les *mtimes* (pour détecter
les changements), et un drapeau `valid`.

`NkShaderLibrary` est le cache central. Point **capital** : tous les handles qu'il rend ou reçoit sont
des `::nkentseu::NkShaderHandle` **RHI**, directement passables à `NkGraphicsPipelineDesc::shader` —
toujours les qualifier pour éviter la confusion avec un handle renderer-side. `Init(device, api,
useNkSL)` / `Shutdown()` l'encadrent.

- **Depuis fichier** — `LoadVF(vert, frag, name)`, `LoadVGF(vert, geom, frag, name)` (avec géométrie),
  `LoadCompute(comp, name)` (pour un shader de calcul).
- **Depuis source** — `CompileVF(vertSrc, fragSrc, name, backendOverride)` : passer un
  `backendOverride` non-null compile via ce backend précis (le chemin NkSL par-shader, opt-in).
- **User-override / fallback** — `LoadOrCompileVF(materialName, fallbackVS, fallbackFS)` cherche
  d'abord un fichier utilisateur dans `Resources/NKRenderer/Shaders/<materialName>/<Backend>/…`, et
  retombe sinon sur les sources embarquées. C'est ce qui permet à un utilisateur de **remplacer** le
  shader d'un matériau sans toucher au moteur.
- **Hot-reload** — `PollHotReload()` (à appeler chaque frame), `HasPendingReloads()`.
- **Accès** — `Find(name)`, `Get(handle)` (le `NkShaderProgram*`), `GetRHIHandle(handle)` (le handle
  RHI ; null si le programme est invalide).
- **Libération** — `Release(handle)` (met le handle à `null`), `ReleaseAll()`.

Cas d'usage : **rendu** (chaque matériau pointe vers un programme), **GPU/compute** (`LoadCompute`
pour de la simulation ou du post-process), **outils/itération** (hot-reload + user-override pour
expérimenter un shader sans recompiler le moteur).

### Idiomes et pièges transverses

- **Ownership.** `NkMaterial` via `Create`/`Destroy` ; `NkMaterialInstance` via le système
  (`CreateInstance`/`DestroyInstance`) ; `NkMaterialSystem`/`NkMaterialCollection`/`NkMaterialLibrary`/
  `NkShaderLibrary` via `Init`/`Shutdown` ; `NkCreateShaderBackend` rend un backend dont **vous**
  êtes responsable. Toutes les destructions par fabrique mettent l'argument `*&` à `null`. Jamais de
  `new`/`delete` direct (heap corruption NKMemory).
- **Live link M.4.** Un setter parent ne propage qu'aux enfants dont le champ/bit n'est **pas**
  overridé ; `ResetXxxOverride` / `ResetParameter` re-lient au parent. **Le parent doit survivre à ses
  enfants.**
- **Chaînage fluent.** `NkMaterial*` et `NkMaterialInstance*` sont retournés par leurs setters.
- **GPU / threading.** `BindInstance` / `Bind` / `GetPipeline` / `Upload` touchent le device ;
  `SetSharedContext` est requis **avant** tout bind ; `UpdateRenderPass` au resize Vulkan.
- **std140.** Les structs `NkPBRParams` / `NkToonParams` / `NkLayered*` doivent matcher *exactement*
  les UBO des shaders (tailles documentées) ; `NkMaterialCollection` = 1 vec4 par slot, max 64.
- **Ambiguïté de handle.** Côté shader, toujours `::nkentseu::NkShaderHandle` (RHI), jamais un wrapper
  renderer-side non qualifié.

---

### Exemple

```cpp
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
using namespace nkentseu::renderer;

// 1. Le système, une fois au démarrage.
NkMaterialSystem matSys;
matSys.Init(device, &texLib, &shaderLib, NK_GFX_API_VULKAN);
matSys.SetSharedContext(globalLayout, objectLayout, vertexLayout, renderPass);

// 2. Un matériau PBR « or », réglé en fluent.
NkMaterial* gold = NkMaterial::Create(&matSys, NK_PBR_METALLIC);
gold->SetAlbedo({ 1.f, 0.78f, 0.34f })
    ->SetMetallic(1.f)
    ->SetRoughness(0.15f);

// 3. Une variante héritée : même or, plus mat — seul roughness est overridé.
NkMaterial* brushedGold = NkMaterial::CreateChild(gold);
brushedGold->SetRoughness(0.55f);

// 4. Un matériau toon pour un ennemi stylisé.
NkMaterial* enemy = NkMaterial::Create(&matSys, NK_TOON);
enemy->SetAlbedo({ 0.8f, 0.1f, 0.1f })
     ->SetOutline(2.f, { 0.f, 0.f, 0.f })
     ->SetRim(0.6f, { 1.f, 0.3f, 0.3f });

// 5. À l'envoi : drawCall.material = enemy->GetInstHandle();

// 6. Un shader chargé via la bibliothèque (handle RHI, prêt pour le pipeline).
::nkentseu::NkShaderHandle h =
    shaderLib.LoadVF("Shaders/portal.vert", "Shaders/portal.frag", "portal");

// Nettoyage (les enfants AVANT le parent).
NkMaterial::Destroy(brushedGold);
NkMaterial::Destroy(gold);
NkMaterial::Destroy(enemy);
```

---

[← Index NKRenderer](README.md) · [Récap NKRenderer](../NKRenderer.md) · [Couche Runtime](../README.md)
