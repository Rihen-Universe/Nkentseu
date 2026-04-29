# NkSL — Shader Language Cross-Compiler v4.0

Système de langage de shader cross-platform pour NKEngine.  
**Un source → sept cibles** : GLSL (OpenGL), GLSL (Vulkan), SPIR-V, HLSL DX11 (SM5), HLSL DX12 (SM6+), MSL, C++ (software rasterizer).

---

## Nouveautés v4.0

### Nouveaux backends

| Cible | Enum | Compilateur | Cas d'usage |
|---|---|---|---|
| GLSL Vulkan 4.50+ | `NK_GLSL_VULKAN` | glslang | `layout(set=N, binding=M)`, subpassInput, push_constant |
| HLSL SM5 (DX11) | `NK_HLSL_DX11` | fxc.exe | Renommage de l'ancien `NK_HLSL` |
| HLSL SM6+ (DX12) | `NK_HLSL_DX12` | dxc.exe | register spaceN, wave ops, bindless SM6.6 |

### Bugs corrigés

| Bug | Fichier | Correction |
|---|---|---|
| Cache O(n) — `NkVector` + recherche linéaire | `NkSLCompiler.cpp` | `NkUnorderedMap<uint64, NkSLCacheEntry>` → O(1) |
| Singleton non thread-safe (`if (!gCompiler)`) | `NkSLIntegration.cpp` | `std::call_once` + `std::once_flag` |
| `glslang::InitializeProcess()` non protégé | `NkSLCompiler.cpp` | `std::call_once` |
| `NkSLShaderLibrary::Get()` sans mutex | `NkSLCompiler.cpp` | `NkSLMutexLock` sur toutes les lectures |
| `Preprocess()` sans garde d'inclusion | `NkSLCompiler.cpp` | `NkVector<NkString>* includedFiles` + `#pragma once` |
| `HashSource()` incomplet | `NkSLCompiler.cpp` | Inclut `glslVulkanVersion`, `hlslShaderModelDX12`, `dx12DefaultSpace` |
| SPIR-V généré depuis GLSL OpenGL | `NkSLCompiler.cpp` | SPIR-V généré depuis `NK_GLSL_VULKAN` (set/binding corrects) |

---

## Architecture

```
NkSL source (.nksl)
        │
        ▼
  NkSLLexer          — tokenisation
        │
        ▼
  NkSLParser         — AST récursif descendant
        │
        ▼
  NkSLCodeGen_*      — génération de code par cible
   ├── NkSLCodeGenGLSL          → OpenGL 4.30+  (layout(binding=))
   ├── NkSLCodeGenGLSLVulkan    → Vulkan GLSL 4.50+ (layout(set=N, binding=M))  ← NOUVEAU
   ├── SPIR-V                   → via glslang (depuis GLSL Vulkan)
   ├── NkSLCodeGenHLSL_DX11     → HLSL SM5, fxc  (register(bN))
   ├── NkSLCodeGenHLSL_DX12     → HLSL SM6, dxc  (register(bN, spaceM))          ← NOUVEAU
   ├── NkSLCodeGen_MSL          → Metal 2+ (natif)
   ├── NkSLCodeGenMSLSpirvCross → Metal 2+ (via SPIRV-Cross)
   └── NkSLCodeGenCPP           → Software rasterizer
        │
        ▼
  NkSLCompiler       — pipeline complet + cache O(1) disque
        │
        ▼
  NkSLIntegration    — helpers NkIDevice::CreateShader()
```

---

## Syntaxe NkSL

La syntaxe est **GLSL-like** avec des annotations `@` pour les bindings.

```glsl
@binding(set=0, binding=0)
uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

@binding(set=0, binding=1)
uniform sampler2DShadow uShadowMap;

@location(0) in  vec3 aPos;
@location(1) in  vec3 aNormal;
@location(2) in  vec3 aColor;

@location(0) out vec3 vWorldPos;
@location(1) out vec3 vNormal;
@location(2) out vec3 vColor;

@stage(vertex)
@entry
void vertMain() {
    vec4 wp   = ubo.model * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal   = normalize(mat3(transpose(inverse(ubo.model))) * aNormal);
    vColor    = aColor;
    gl_Position = ubo.proj * ubo.view * wp;
}

@location(0) out vec4 fragColor;

@stage(fragment)
@entry
void fragMain() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDirW.xyz);
    float diff = max(dot(N, L), 0.0);
    fragColor  = vec4(diff * vColor, 1.0);
}
```

---

## Correspondances par cible

### Bindings uniforms

| NkSL source | GLSL OpenGL | GLSL Vulkan | HLSL DX11 | HLSL DX12 |
|---|---|---|---|---|
| `@binding(set=0, binding=1)` | `layout(binding=1)` | `layout(set=0, binding=1)` | `register(b1)` | `register(b1, space0)` |
| `@push_constant` | *(non supporté)* | `layout(push_constant)` | *(cbuffer b0)* | `ConstantBuffer<T> cb : register(b0, space0)` |
| `uniform sampler2D tex;` | `layout(binding=N) uniform sampler2D tex;` | `layout(set=S, binding=N) uniform sampler2D tex;` | `Texture2D tex_tex : register(tN); SamplerState tex_smp : register(sN);` | `Texture2D tex_tex : register(tN, space0); SamplerState tex_smp : register(sN, space0);` |

### Builtins vertex

| NkSL/GLSL | GLSL Vulkan | HLSL DX11/12 | MSL |
|---|---|---|---|
| `gl_VertexID` | `gl_VertexIndex` | `input._VertexID (SV_VertexID)` | `vertex_id [[vertex_id]]` |
| `gl_InstanceID` | `gl_InstanceIndex` | `input._InstanceID (SV_InstanceID)` | `instance_id [[instance_id]]` |
| `gl_Position` | `gl_Position` | `output._Position (SV_Position)` | `out.position [[position]]` |

### Wave intrinsics (DX12 SM6.0+ / SPIR-V)

| NkSL / GLSL subgroup | HLSL DX12 (SM6) |
|---|---|
| `subgroupSize()` | `WaveGetLaneCount()` |
| `subgroupElect()` | `WaveIsFirstLane()` |
| `subgroupAdd(x)` | `WaveActiveSum(x)` |
| `subgroupAll(x)` | `WaveActiveAllTrue(x)` |
| `subgroupAny(x)` | `WaveActiveAnyTrue(x)` |
| `subgroupBroadcast(x, i)` | `WaveReadLaneAt(x, i)` |
| `subgroupBallot(b)` | `WaveActiveBallot(b)` |

---

## Utilisation

### Cas simple — le bon target selon l'API

```cpp
// Initialiser le compilateur une seule fois au démarrage
NkSL::InitCompiler("./shader_cache");

// CreateShaderFromSource choisit automatiquement le bon target via ApiToTarget()
// OpenGL    → NK_GLSL
// Vulkan    → NK_SPIRV
// DX11      → NK_HLSL_DX11
// DX12      → NK_HLSL_DX12
// Metal     → NK_MSL
NkShaderHandle hShader = NkSL::CreateShaderFromSource(
    device,
    kPhongShader,
    { NkSLStage::VERTEX, NkSLStage::FRAGMENT },
    "PhongShadow"
);
```

### Choisir un target explicitement

```cpp
// GLSL pour Vulkan (texte lisible, pas de SPIR-V binaire)
auto res = compiler.Compile(source, NkSLStage::NK_VERTEX,
                             NkSLTarget::NK_GLSL_VULKAN);
printf("GLSL Vulkan:\n%s\n", res.source.CStr());

// HLSL SM5 pour DirectX 11
auto dx11 = compiler.Compile(source, NkSLStage::NK_VERTEX,
                              NkSLTarget::NK_HLSL_DX11);

// HLSL SM6 pour DirectX 12 — wave ops + spaces
NkSLCompileOptions dx12Opts;
dx12Opts.hlslShaderModelDX12  = 60; // SM6.0
dx12Opts.dx12DefaultSpace      = 0;
dx12Opts.dx12InlineRootSignature = false;
auto dx12 = compiler.Compile(source, NkSLStage::NK_VERTEX,
                              NkSLTarget::NK_HLSL_DX12, dx12Opts);
```

### DX12 avec bindless heap (SM6.6)

```cpp
NkSLCompileOptions opts;
opts.hlslShaderModelDX12 = 66;   // SM6.6
opts.dx12BindlessHeap    = true; // émet NKSL_TEX2D / NKSL_SAMPLER macros
opts.dx12DefaultSpace    = 0;

auto res = compiler.Compile(source, NkSLStage::NK_FRAGMENT,
                             NkSLTarget::NK_HLSL_DX12, opts);
// Le code généré contient :
// #define NKSL_TEX2D(idx)      ((Texture2D)ResourceDescriptorHeap[idx])
// #define NKSL_SAMPLER(idx)    ((SamplerState)SamplerDescriptorHeap[idx])
```

### GLSL Vulkan avec subpassInput

```cpp
// shader.nksl
// @binding(set=0, binding=0, input_attachment_index=0)
// uniform subpassInput uGBuffer;
// ...
// vec4 color = subpassLoad(uGBuffer);

auto res = compiler.Compile(source, NkSLStage::NK_FRAGMENT,
                             NkSLTarget::NK_GLSL_VULKAN);
// Émet : layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput uGBuffer;
```

### DX12 avec root signature inline

```cpp
NkSLCompileOptions opts;
opts.hlslShaderModelDX12    = 60;
opts.dx12InlineRootSignature = true; // émet [RootSignature(...)] avant l'entry point
auto res = compiler.Compile(source, NkSLStage::NK_VERTEX,
                             NkSLTarget::NK_HLSL_DX12, opts);
```

### Compiler pour toutes les cibles en une passe

```cpp
auto multi = compiler.CompileAllTargets(source, NkSLStage::NK_VERTEX, {
    NkSLTarget::NK_GLSL,
    NkSLTarget::NK_GLSL_VULKAN,
    NkSLTarget::NK_SPIRV,
    NkSLTarget::NK_HLSL_DX11,
    NkSLTarget::NK_HLSL_DX12,
    NkSLTarget::NK_MSL,
}, opts, "phong");

if (multi.allSucceeded()) {
    // multi.results[0] = GLSL OpenGL
    // multi.results[1] = GLSL Vulkan
    // multi.results[2] = SPIR-V
    // multi.results[3] = HLSL DX11
    // multi.results[4] = HLSL DX12
    // multi.results[5] = MSL
    // multi.reflection = reflection partagée (AST parsé une seule fois)
}
```

### Vérifier les capacités disponibles

```cpp
auto caps = NkSLFeatureCaps::ForTarget(NkSLTarget::NK_HLSL_DX12, 66);
if (caps.waveIntrinsics)  printf("Wave ops disponibles\n");
if (caps.bindlessHeapSM66) printf("ResourceDescriptorHeap disponible (SM6.6)\n");
if (caps.meshShaders)     printf("Mesh shaders disponibles (SM6.5+)\n");

auto vkCaps = NkSLFeatureCaps::ForTarget(NkSLTarget::NK_GLSL_VULKAN);
if (vkCaps.pushConstants) printf("Push constants Vulkan disponibles\n");
if (vkCaps.subpassInput)  printf("subpassInput disponible\n");
```

---

## Configuration CMake

```cmake
add_subdirectory(Modules/Runtime/NKRHI/src/NKRHI/SL)
target_link_libraries(MyApp PRIVATE NkSL)
```

### Options

| Option | Défaut | Description |
|---|---|---|
| `NKSL_USE_GLSLANG` | ON | SPIR-V via glslang embarqué (standalone) |
| `NKSL_USE_SPIRV_CROSS` | ON | MSL via SPIRV-Cross embarqué |
| `NKSL_USE_SHADERC` | OFF | SPIR-V via shaderc (Vulkan SDK optionnel) |

---

## Limitations connues

1. **Tessellation MSL** : parsé, génération limitée (Metal 3 requis pour mesh shading).
2. **Atomic counters MSL** : non supportés.
3. **Subroutines GLSL** : non supportées.
4. **double en MSL** : automatiquement downgradé en `float`.
5. **Analyse sémantique incomplète** : les déclarations de variables non déclarées peuvent passer sans erreur.
6. **#define avec paramètres** : le préprocesseur ne supporte pas les macros paramétriques.

---

## Roadmap

- [ ] Optimiseur IR (élimination du code mort, constant folding)
- [ ] Support WGSL (WebGPU)
- [ ] Analyse sémantique complète (type checking exhaustif)
- [ ] #define avec paramètres (préprocesseur complet)
- [ ] Specialization constants Vulkan (`layout(constant_id=N)`)
- [ ] Geometry shaders DX12 SM6
- [ ] Tests unitaires (corpus de shaders de référence)
