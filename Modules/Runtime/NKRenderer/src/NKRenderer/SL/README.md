# NkSL — Shader Language Cross-Compiler

Système de langage de shader cross-platform pour NKEngine.
**Un source → six cibles** : GLSL, SPIR-V, HLSL, MSL, C++ (software rasterizer).

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
   ├── GLSL          → OpenGL 4.30+
   ├── SPIR-V        → Vulkan (via shaderc ou glslang)
   ├── HLSL          → DirectX 11/12 (SM 5+)
   ├── MSL           → Metal 2+
   └── C++           → Software rasterizer
        │
        ▼
  NkSLCompiler       — pipeline complet + cache disque
        │
        ▼
  NkSLIntegration    — helpers NkIDevice::CreateShader()
```

---

## Syntaxe NkSL

La syntaxe est **GLSL-like** avec des annotations `@` pour les bindings.

### Exemple complet — shader Phong

```glsl
// Uniform block
@binding(set=0, binding=0)
uniform MainUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 lightVP;
    vec4 lightDirW;
    vec4 eyePosW;
} ubo;

// Sampler shadow map
@binding(set=0, binding=1)
uniform sampler2DShadow uShadowMap;

// Vertex inputs
@location(0) in  vec3 aPos;
@location(1) in  vec3 aNormal;
@location(2) in  vec3 aColor;

// Vertex outputs
@location(0) out vec3 vWorldPos;
@location(1) out vec3 vNormal;
@location(2) out vec3 vColor;

// Vertex entry point
@stage(vertex)
@entry
void vertMain() {
    vec4 wp  = ubo.model * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    vNormal   = normalize(mat3(transpose(inverse(ubo.model))) * aNormal);
    vColor    = aColor;
    gl_Position = ubo.proj * ubo.view * wp;
}

// Fragment output
@location(0) out vec4 fragColor;

// Fragment entry point
@stage(fragment)
@entry
void fragMain() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDirW.xyz);
    float diff = max(dot(N, L), 0.0);
    fragColor  = vec4(diff * vColor, 1.0);
}
```

### Annotations disponibles

| Annotation | Description | Exemple |
|---|---|---|
| `@binding(set=N, binding=M)` | Binding Vulkan / GLSL | `@binding(set=0, binding=1)` |
| `@location(N)` | Slot in/out vertex | `@location(0) in vec3 aPos;` |
| `@stage(name)` | Déclarer un stage | `@stage(vertex)` |
| `@entry` | Marquer l'entry point | `@entry void main()` |
| `@push_constant` | Push constant Vulkan | `@push_constant uniform PC { ... }` |
| `@builtin(name)` | Variable builtin | `@builtin(position) vec4 pos;` |

### Types supportés

Tous les types GLSL sont supportés et sont mappés automatiquement :

| NkSL / GLSL | HLSL | MSL | C++ |
|---|---|---|---|
| `float` | `float` | `float` | `float` |
| `vec2/3/4` | `float2/3/4` | `float2/3/4` | `NkVec2/3/4f` |
| `mat4` | `column_major float4x4` | `float4x4` | `NkMat4f` |
| `sampler2D` | `Texture2D + SamplerState` | `texture2d<float> + sampler` | `NkSWTexture2D*` |
| `sampler2DShadow` | `Texture2D + SamplerComparisonState` | `depth2d<float>` | `NkSWTexture2D*` |
| `image2D` | `RWTexture2D<float4>` | `texture2d<float, access::read_write>` | — |

### Intrinsèques supportées

Toutes les fonctions mathématiques GLSL standard :
`dot`, `cross`, `normalize`, `length`, `mix`, `clamp`, `smoothstep`,
`pow`, `sqrt`, `abs`, `sin`, `cos`, `tan`, `atan`, `floor`, `ceil`,
`fract`, `mod`, `min`, `max`, `reflect`, `refract`, `texture`,
`textureSize`, `imageLoad`, `imageStore`, `transpose`, `inverse`, ...

---

## Utilisation

### Cas simple — créer un shader depuis source

```cpp
// Initialiser le compilateur (une seule fois au démarrage)
NkSL::InitCompiler("./shader_cache");

// Créer un shader — le bon bytecode est généré selon l'API du device
NkShaderHandle hShader = NkSL::CreateShaderFromSource(
    device,
    kPhongShader,
    { NkSLStage::VERTEX, NkSLStage::FRAGMENT },
    "PhongShadow"
);

// Utilisation normale via NkIDevice
NkGraphicsPipelineDesc pipeDesc;
pipeDesc.shader = hShader;
// ...
```

### Créer depuis un fichier

```cpp
NkShaderHandle h = NkSL::CreateShaderFromFile(
    device, "shaders/phong.nksl",
    { NkSLStage::VERTEX, NkSLStage::FRAGMENT }
);
```

### Bibliothèque de shaders avec hot-reload

```cpp
NkSLShaderLibrary lib(&NkSL::GetCompiler(), "./shaders");

lib.Register("phong",   "phong.nksl",   { NkSLStage::VERTEX, NkSLStage::FRAGMENT });
lib.Register("tonemap", "tonemap.nksl", { NkSLStage::COMPUTE });

NkSLTarget target = NkSL::ApiToTarget(device->GetApi());
lib.CompileAll(target);

// Dans la boucle principale :
uint32 reloaded = lib.HotReload(target);
if (reloaded > 0) {
    // Re-créer les pipelines impactés
}

// Obtenir un NkShaderDesc
NkShaderDesc desc;
lib.FillShaderDesc("phong", target, desc);
NkShaderHandle h = device->CreateShader(desc);
```

### Valider un shader sans compiler

```cpp
auto errors = NkSL::Validate(source, "myshader.nksl");
for (auto& e : errors)
    printf("line %u: %s\n", e.line, e.message.CStr());
```

### Inspecter le code généré

```cpp
// Voir le GLSL généré pour debug
NkString glsl = NkSL::GetGeneratedSource(
    NkGraphicsApi::NK_API_OPENGL,
    kPhongShader,
    NkSLStage::VERTEX
);
printf("%s\n", glsl.CStr());

// Voir le HLSL généré
NkString hlsl = NkSL::GetGeneratedSource(
    NkGraphicsApi::NK_API_DIRECTX11,
    kPhongShader,
    NkSLStage::VERTEX
);
```

### Compilation offline (build pipeline)

```cpp
NkSLCompiler compiler("./shader_cache");

for (auto target : { NkSLTarget::GLSL, NkSLTarget::SPIRV, NkSLTarget::HLSL, NkSLTarget::MSL }) {
    for (auto stage : { NkSLStage::VERTEX, NkSLStage::FRAGMENT }) {
        auto res = compiler.Compile(source, stage, target, {}, "shader");
        if (!res.success) { /* log errors */ }
        else {
            // Sauvegarder le bytecode dans un fichier .spv, .hlsl, .msl...
            SaveToFile("shader." + NkString(NkSLTargetName(target)), res.bytecode);
        }
    }
}

// Persister le cache pour les runs suivants
compiler.GetCache().Flush();
```

---

## Installation et build

### Prérequis

- CMake 3.16+
- Compilateur C++17
- **Pour SPIR-V** : Vulkan SDK (shaderc ou glslang)

### Configuration CMake

```cmake
# Dans votre CMakeLists.txt parent :
add_subdirectory(NkSL)
target_link_libraries(MyApp PRIVATE NkSL)
```

### Options CMake

| Option | Défaut | Description |
|---|---|---|
| `NKSL_USE_SHADERC` | ON | Compilation SPIR-V via shaderc (Vulkan SDK) |
| `NKSL_USE_GLSLANG` | OFF | Compilation SPIR-V via glslang direct |

### Sans SPIR-V compiler

Si ni shaderc ni glslang n'est disponible, le compilateur NkSL retourne
le GLSL intermédiaire au lieu du SPIR-V. En Vulkan, cela nécessite
l'extension `VK_NV_glsl_shader` (disponible sur NVIDIA uniquement, non recommandé
en production). **Pour un usage production, installer le Vulkan SDK.**

```bash
# Ubuntu/Debian
sudo apt install libvulkan-dev glslang-tools

# Windows : télécharger le Vulkan SDK depuis https://vulkan.lunarg.com
# Puis définir VULKAN_SDK dans les variables d'environnement
```

---

## Correspondance des types par API

### Textures et samplers

| NkSL | GLSL | HLSL | MSL |
|---|---|---|---|
| `sampler2D` | `uniform sampler2D tex;` | `Texture2D tex; SamplerState tex_smp;` | `texture2d<float> tex [[texture(N)]]; sampler tex_smp [[sampler(N)]];` |
| `sampler2DShadow` | `uniform sampler2DShadow tex;` | `Texture2D tex; SamplerComparisonState tex_smp;` | `depth2d<float> tex; sampler tex_smp;` |
| `texture(tex, uv)` | `texture(tex, uv)` | `tex_tex.Sample(tex_smp, uv)` | `tex_tex.sample(tex_smp, uv)` |

### Builtins

| NkSL / GLSL | HLSL | MSL |
|---|---|---|---|
| `gl_Position` | `output.Position (SV_Position)` | `out.position [[position]]` |
| `gl_FragCoord` | `input.Position (SV_Position)` | `in.position [[position]]` |
| `gl_VertexID` | `input.VertexID (SV_VertexID)` | `vertex_id [[vertex_id]]` |
| `gl_LocalInvocationID` | `GroupThreadID (SV_GroupThreadID)` | `thread_position_in_threadgroup` |
| `gl_GlobalInvocationID` | `DispatchThreadID (SV_DispatchThreadID)` | `thread_position_in_grid` |

---

## Limitations connues

1. **Pas de SPIR-V sans Vulkan SDK** : installer shaderc (Vulkan SDK 1.3+).
2. **Tessellation** : les shaders tess control/eval sont parsés mais la génération MSL est limitée (Metal 3 requis pour le mesh shading).
3. **Atomic counters** : supportés en GLSL/HLSL, pas encore en MSL.
4. **Subroutines GLSL** : non supportées (pas d'équivalent HLSL/MSL propre).
5. **double en MSL** : Metal ne supporte pas `double` sur GPU — automatiquement downgradé en `float`.
6. **Matrices** : attention à la convention row-major (HLSL) vs column-major (GLSL). NkSL utilise column-major (GLSL) et génère `column_major float4x4` en HLSL pour la compatibilité avec le code C++ NKEngine.

---

## Roadmap

- [ ] Optimiseur IR (élimination du code mort, constant folding)
- [ ] Support SPIR-V → GLSL via spirv-cross (pour le debug)
- [ ] Support WGSL (WebGPU)
- [ ] Analyse sémantique complète (type checking, undeclared variables)
- [ ] Reflection automatique (extraction des bindings depuis l'AST → NkDescriptorSetLayoutDesc)
- [ ] #include avec garde d'inclusion (#pragma once)
- [ ] Macros et préprocesseur complet (#define avec paramètres)
- [ ] Specialization constants Vulkan
