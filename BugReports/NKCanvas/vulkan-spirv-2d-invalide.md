# NKCanvas Vulkan — Crash dans vkCreateGraphicsPipelines (SPIR-V 2D invalide)

- **Catégorie** : NKCanvas (backend Vulkan / shaders)
- **Sévérité** : élevée (backend Vulkan inutilisable — crash dès l'init du renderer 2D)
- **Date** : 2026-06-05
- **Statut** : **résolu**

## Symptôme

Backend Vulkan : crash **`SIGSEGV` à l'intérieur du driver NVIDIA**
(`nvoglv64.dll`) pendant `vkCreateGraphicsPipelines`, juste après
`[NkVk2D] Using precompiled SPIR-V shaders` / `MakePipeline[Alpha]`. En `gdb` :

```
Thread 1 received signal SIGSEGV
#17 vulkan-1!vkResetEvent ()                          vulkan-1.dll
#18 NkVulkanRenderer2D::CreatePipelines()::$_0::operator()  NkVulkanRenderer2D.cpp:758  (vkCreateGraphicsPipelines)
#19 NkVulkanRenderer2D::CreatePipelines               NkVulkanRenderer2D.cpp:798
#20 NkVulkanRenderer2D::Initialize                    NkVulkanRenderer2D.cpp:459
```

Le `VkGraphicsPipelineCreateInfo` paraissait **structurellement valide** (modules
créés, `pipelineLayout`/`renderPass` non nuls, dynamic states VIEWPORT+SCISSOR).

## Contexte

- `NkVulkanRenderer2D` charge un SPIR-V **pré-compilé embarqué** depuis
  `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkRenderer2DVkSpv.inl`
  (tableaux `kVk2DVertSpv[]` / `kVk2DFragSpv[]`).
- Validation layers Vulkan **non installées/activées** → pas de message
  explicatif, juste le crash driver.

## Cause racine

Le SPIR-V embarqué avait été **assemblé À LA MAIN** (commentaire d'origine du
`.inl` : *« Généré par gen_spirv.py (assemblage manuel des opcodes SPIR-V) »*).
Cet assemblage manuel était **invalide / incomplet** :

- vertex : **238 mots** embarqués vs **341** pour le vrai shader compilé ;
- fragment : **135 mots** vs **165**.

`vkCreateShaderModule` **ne valide pas** entièrement le SPIR-V → il accepte le
module, puis le **compilateur du driver segfault** en consommant un bytecode
malformé dans `vkCreateGraphicsPipelines`. (C'est un piège classique : un module
« créé OK » ne garantit pas un SPIR-V valide.)

## Diagnostic (méthode reproductible)

Le Vulkan SDK fournit `glslangValidator` et `spirv-val`
(`C:\VulkanSDK\<ver>\Bin\`). Compiler le GLSL canonique et valider :

```bash
glslangValidator -V nk2d.vert -o nk2d.vert.spv && spirv-val nk2d.vert.spv   # -> valide, 341 mots
glslangValidator -V nk2d.frag -o nk2d.frag.spv && spirv-val nk2d.frag.spv   # -> valide, 165 mots
```
La différence de taille (341 vs 238) confirme que l'embarqué était tronqué.

## Solution

**Régénérer le SPIR-V depuis le GLSL canonique** (déjà présent dans le `.inl`
comme `kVk2DVertGLSL`/`kVk2DFragGLSL`) avec `glslangValidator -V`, le **valider**
avec `spirv-val`, puis ré-empaqueter les tableaux `uint32` dans le `.inl` (script
`gen_inl.py`, little-endian, word counts mis à jour). **Ne plus jamais assembler
le SPIR-V à la main.**

Résultat après fix :
```
MakePipeline[Alpha] OK · [Add] OK · [Mul] OK · [None] OK
[NkVk2D] CreatePipelines DONE (4/4 pipelines OK)
[NkVk2D] Initialized
```

Fichier régénéré : `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkRenderer2DVkSpv.inl`.

## Vérification

- `spirv-val` passe sur les deux modules.
- Au lancement Vulkan : 4/4 pipelines créés, `Init OK`, l'app atteint les scènes
  (plus de `SIGSEGV`).

## Notes / pièges

- **Module créé ≠ SPIR-V valide** : toujours valider avec `spirv-val`. Un module
  invalide ne plante qu'à la **création du pipeline**, dans le driver.
- Pour avoir des messages au lieu d'un crash muet : activer les **validation
  layers** (`VK_LAYER_KHRONOS_validation`) — mais énumérer d'abord les couches
  dispo (`vkEnumerateInstanceLayerProperties`) et skip+warn si absentes (sinon
  `vkCreateInstance` plante quand le SDK n'est pas là).
- Bug distinct repéré sur le même backend : `ctx->GetInfo()` Vulkan retourne des
  valeurs incohérentes (`windowWidth/Height`, projection) → à corriger séparément
  (n'empêche pas le pipeline, mais fausse la projection). Voir
  [vulkan-getinfo-garbage.md](vulkan-getinfo-garbage.md) *(à investiguer)*.

## Liens

- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkRenderer2DVkSpv.inl`
- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkVulkanRenderer2D.cpp` (CreatePipelines, MakeModule, MakePipeline)
- Outils : `C:\VulkanSDK\1.4.350.0\Bin\{glslangValidator,spirv-val}`
