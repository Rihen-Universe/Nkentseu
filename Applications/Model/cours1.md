# Cours Complet — Création d'applications 2D/3D avec le framework Nkentseu (NKRHI)

---

## Table des matières

1. Architecture générale du framework
2. Initialisation : fenêtre, système, surface
3. Création du device RHI
4. Le command buffer et la boucle de rendu
5. Buffers GPU (vertex, index, uniform, storage)
6. Shaders : écriture, compilation, cache
7. Textures, samplers et formats
8. Render passes et framebuffers
9. Pipelines graphiques
10. Rendu 3D : éclairage, ombres (shadow maps)
11. Rendu 2D : overlay et dessin en écran
12. Dessin offscreen (render-to-texture)
13. MSAA et post-processing
14. Compute shaders
15. Techniques avancées : cubemaps, GBuffer, rendu différé
16. Gestion des ressources et bonnes pratiques

---

## 1. Architecture générale

Le framework est organisé en couches distinctes :

```
NkSystem / NkWindow          ← Plateforme, fenêtre, événements
      ↓
NkSurfaceDesc               ← Pont entre fenêtre et GPU
      ↓
NkDeviceFactory             ← Sélectionne l'API (OpenGL, Vulkan, DX11, DX12, Metal, Software)
      ↓
NkIDevice                   ← Le device GPU central
      ↓
NkICommandBuffer            ← Enregistrement des commandes GPU
      ↓
NkISwapchain                ← Surface de présentation
```

**Principe fondamental** : toutes les ressources GPU sont identifiées par des handles opaques (`NkBufferHandle`, `NkTextureHandle`, etc.). On ne manipule jamais de pointeurs vers des objets GPU natifs (pas de `VkBuffer*`, pas de `ID3D12Resource*`). Cela rend le code indépendant de l'API.

---

## 2. Initialisation : fenêtre, système, surface

### 2.1 Point d'entrée

```cpp
#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"

using namespace nkentseu;

int nkmain(const NkEntryState& state) {
    // Initialiser le système global
    NkAppData appData;
    appData.appName = "MonApp";
    appData.enableRendererDebug = true;
    if (!NkInitialise(appData)) return -1;

    // Créer la fenêtre
    NkWindowConfig cfg;
    cfg.title    = "Mon Application";
    cfg.width    = 1280;
    cfg.height   = 720;
    cfg.centered = true;
    cfg.resizable = true;

    NkWindow window;
    if (!window.Create(cfg)) return -2;

    // ... (voir sections suivantes)

    window.Close();
    NkClose();
    return 0;
}
```

### 2.2 Récupérer la surface native

La `NkSurfaceDesc` est le pont entre la fenêtre et le backend graphique. Elle contient les handles natifs appropriés à la plateforme (HWND sur Windows, Display/Window sur X11, etc.).

```cpp
NkSurfaceDesc surface = window.GetSurfaceDesc();
// surface.width, surface.height, surface.dpi sont remplis
// surface.hwnd (Windows), surface.display (X11), etc.
```

---

## 3. Création du device RHI

### 3.1 Remplir NkDeviceInitInfo

```cpp
NkDeviceInitInfo initInfo;
initInfo.api     = NkGraphicsApi::NK_API_OPENGL;  // ou VULKAN, D3D12, etc.
initInfo.surface = window.GetSurfaceDesc();
initInfo.width   = 1280;
initInfo.height  = 720;

// Configuration OpenGL 4.6 core profile
initInfo.context = NkContextDesc::MakeOpenGL(4, 6, /*debug=*/true);
// Ou Vulkan :
// initInfo.context = NkContextDesc::MakeVulkan(/*validation=*/true);
// Ou DirectX 12 :
// initInfo.context = NkContextDesc::MakeDirectX12(/*debug=*/true);
```

### 3.2 Créer le device via la factory

```cpp
NkIDevice* device = NkDeviceFactory::Create(initInfo);
if (!device || !device->IsValid()) {
    // Fallback automatique si l'API principale n'est pas disponible
    device = NkDeviceFactory::CreateWithFallback(initInfo, {
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE
    });
}
```

### 3.3 Gestion du resize

```cpp
NkEvents().AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* ev) {
    device->OnResize(ev->GetWidth(), ev->GetHeight());
});
```

---

## 4. Le command buffer et la boucle de rendu

### 4.1 Structure d'une frame

Chaque frame suit ce cycle :

```
BeginFrame() → Acquire swapchain image → Begin CB → BeginRenderPass
→ Draw calls → EndRenderPass → End CB → Submit → Present → EndFrame()
```

### 4.2 Implémentation

```cpp
NkCommandPool cmdPool(device, NkCommandBufferType::NK_GRAPHICS);

// Boucle principale
bool running = true;
while (running) {
    // Pomper les événements
    while (NkEvent* ev = NkEvents().PollEvent()) {
        if (ev->As<NkWindowCloseEvent>()) running = false;
    }

    // Début de frame
    NkFrameContext frame;
    if (!device->BeginFrame(frame)) continue;  // fenêtre minimisée, etc.

    // Acquérir un command buffer
    NkICommandBuffer* cmd = cmdPool.Acquire();
    cmd->Begin();

    // Configurer le viewport
    NkViewport vp{};
    vp.x = 0; vp.y = 0;
    vp.width  = (float)device->GetSwapchainWidth();
    vp.height = (float)device->GetSwapchainHeight();
    vp.minDepth = 0.f; vp.maxDepth = 1.f;
    cmd->SetViewport(vp);

    NkRect2D scissor{{0, 0}, {(int)vp.width, (int)vp.height}};
    cmd->SetScissor(scissor);

    // Commencer le render pass du swapchain
    NkRect2D area{{0,0},{(int32)vp.width,(int32)vp.height}};
    cmd->BeginRenderPass(
        device->GetSwapchainRenderPass(),
        device->GetSwapchainFramebuffer(),
        area
    );

    // === Ici : draw calls ===

    cmd->EndRenderPass();
    cmd->End();

    // Soumettre et présenter
    device->SubmitAndPresent(cmd);
    cmdPool.Release(cmd);

    device->EndFrame(frame);
}

cmdPool.Reset();
device->WaitIdle();
NkDeviceFactory::Destroy(device);
```

---

## 5. Buffers GPU

### 5.1 Vertex buffer

```cpp
// Définir la structure d'un vertex
struct Vertex {
    float x, y, z;     // position
    float nx, ny, nz;  // normale
    float u, v;         // UV
};

// Données
Vertex vertices[] = {
    {-0.5f, -0.5f, 0.f,  0,0,1,  0,0},
    { 0.5f, -0.5f, 0.f,  0,0,1,  1,0},
    { 0.5f,  0.5f, 0.f,  0,0,1,  1,1},
    {-0.5f,  0.5f, 0.f,  0,0,1,  0,1},
};

// Créer le buffer immutable (données statiques)
NkBufferDesc vbDesc = NkBufferDesc::Vertex(sizeof(vertices), vertices);
vbDesc.debugName = "QuadVB";
NkBufferHandle vbHandle = device->CreateBuffer(vbDesc);
```

### 5.2 Index buffer

```cpp
uint16_t indices[] = {0,1,2, 2,3,0};

NkBufferDesc ibDesc = NkBufferDesc::Index(sizeof(indices), indices);
NkBufferHandle ibHandle = device->CreateBuffer(ibDesc);
```

### 5.3 Uniform buffer (constantes de shader)

Les uniform buffers sont mis à jour chaque frame avec les matrices, couleurs, etc.

```cpp
struct PerFrameUniforms {
    float viewProj[16];  // matrice view-projection (4x4)
    float time;
    float _pad[3];
};

// Buffer dynamique (CPU write chaque frame)
NkBufferDesc ubDesc = NkBufferDesc::Uniform(sizeof(PerFrameUniforms));
NkBufferHandle ubHandle = device->CreateBuffer(ubDesc);

// Mise à jour chaque frame
PerFrameUniforms uniforms;
// ... remplir uniforms.viewProj ...
device->WriteBuffer(ubHandle, &uniforms, sizeof(uniforms));
```

### 5.4 Storage buffer (lecture/écriture GPU)

```cpp
// Pour le compute shader ou les SSBO
NkBufferDesc sbDesc = NkBufferDesc::Storage(sizeof(float) * 1024);
NkBufferHandle sbHandle = device->CreateBuffer(sbDesc);
```

### 5.5 Binding dans le command buffer

```cpp
cmd->BindVertexBuffer(0, vbHandle, 0);
cmd->BindIndexBuffer(ibHandle, NkIndexFormat::NK_UINT16, 0);
cmd->DrawIndexed(6); // 6 indices → 2 triangles
```

---

## 6. Shaders

### 6.1 Écriture en GLSL

**Vertex shader** (`mesh.vert.glsl`) :

```glsl
#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

layout(set = 0, binding = 0) uniform PerFrame {
    mat4 viewProj;
    float time;
};

layout(push_constant) uniform PushConstants {
    mat4 model;
};

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;

void main() {
    vec4 worldPos = model * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal   = normalize(mat3(transpose(inverse(model))) * aNormal);
    vUV       = aUV;
    gl_Position = viewProj * worldPos;
}
```

**Fragment shader** (`mesh.frag.glsl`) :

```glsl
#version 460 core

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 albedo = texture(uAlbedo, vUV).rgb;
    // Éclairage Lambert simple
    vec3 lightDir = normalize(vec3(1, 2, 1));
    float NdotL = max(dot(vNormal, lightDir), 0.0);
    fragColor = vec4(albedo * (0.2 + 0.8 * NdotL), 1.0);
}
```

### 6.2 Compilation et cache

```cpp
// Activer le cache disque
NkShaderCache::Global().SetCacheDir("Build/ShaderCache");

// Charger et compiler
auto loadShaderStage = [&](const char* path, NkSLStage stage) -> NkShaderStageDesc {
    NkString strPath(path);
    uint64 key = NkShaderCache::ComputeKey(strPath, stage);
  
    NkShaderConvertResult spirv = NkShaderCache::Global().Load(key);
    if (!spirv.success) {
        spirv = NkShaderConverter::LoadAsSpirv(strPath);
        if (spirv.success) NkShaderCache::Global().Save(key, spirv);
    }
  
    NkShaderStageDesc desc{};
    desc.stage = (NkShaderStage)(1 << (int)stage);
    desc.spirvBinary = spirv.binary;
    return desc;
};

NkShaderDesc shaderDesc;
shaderDesc.AddStage(loadShaderStage("shaders/mesh.vert.glsl", NkSLStage::NK_VERTEX));
shaderDesc.AddStage(loadShaderStage("shaders/mesh.frag.glsl", NkSLStage::NK_FRAGMENT));

NkShaderHandle shader = device->CreateShader(shaderDesc);
```

### 6.3 Push constants

Les push constants sont de petites données (128 octets max) passées ultra-rapidement :

```glsl
// Dans le shader (déclaré ci-dessus)
layout(push_constant) uniform PushConstants {
    mat4 model;
};
```

```cpp
// Dans le command buffer
float modelMatrix[16] = { /* matrice identité */ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
cmd->PushConstants(NkShaderStage::NK_VERTEX, 0, sizeof(modelMatrix), modelMatrix);
```

---

## 7. Textures, samplers et formats

### 7.1 Créer et uploader une texture 2D

```cpp
// Charger les pixels (ex: depuis un loader d'image)
uint32_t* pixels = /* ... charger image 512x512 RGBA ... */;

NkTextureDesc texDesc = NkTextureDesc::Tex2D(512, 512, NkGPUFormat::NK_RGBA8_SRGB, /*mips=*/0);
texDesc.debugName = "AlbedoTexture";
NkTextureHandle texHandle = device->CreateTexture(texDesc);

// Upload
uint32_t rowPitch = 512 * 4; // largeur × bytes_par_pixel
device->WriteTexture(texHandle, pixels, rowPitch);

// Générer les mipmaps automatiquement
device->GenerateMipmaps(texHandle, NkFilter::NK_LINEAR);
```

### 7.2 Choisir le bon format


| Cas d'usage                    | Format recommandé     |
| ------------------------------ | ---------------------- |
| Couleur standard (sRGB)        | `NK_RGBA8_SRGB`        |
| Données linéaires (normales) | `NK_RGBA8_UNORM`       |
| HDR / floating point           | `NK_RGBA16_FLOAT`      |
| Depth buffer                   | `NK_D32_FLOAT`         |
| Depth + Stencil                | `NK_D24_UNORM_S8_UINT` |
| Texture compressée (BC7)      | `NK_BC7_SRGB`          |
| Mobile (ASTC)                  | `NK_ASTC_4X4_SRGB`     |

### 7.3 Créer un sampler

```cpp
// Sampler trilinéaire standard
NkSamplerDesc sampDesc = NkSamplerDesc::Linear();
sampDesc.mipFilter    = NkMipFilter::NK_LINEAR;
sampDesc.maxAnisotropy = 16.f;  // Anisotropique 16x
NkSamplerHandle sampHandle = device->CreateSampler(sampDesc);

// Sampler pour shadow map
NkSamplerDesc shadowSamp = NkSamplerDesc::Shadow();
NkSamplerHandle shadowSampHandle = device->CreateSampler(shadowSamp);
```

---

## 8. Render passes et framebuffers

Un **render pass** décrit les attachements (color, depth) et leurs opérations (clear, load, store). Un **framebuffer** lie ces descriptions à des textures concrètes.

### 8.1 Render pass standard (forward)

```cpp
// Render pass avec un color attachment + depth
NkRenderPassDesc rpDesc = NkRenderPassDesc::Forward(
    NkGPUFormat::NK_RGBA8_SRGB,
    NkGPUFormat::NK_D32_FLOAT
);
NkRenderPassHandle renderPass = device->CreateRenderPass(rpDesc);
```

### 8.2 Framebuffer custom (offscreen)

```cpp
uint32_t W = 1024, H = 1024;

// Créer les attachements comme textures
NkTextureHandle colorTex = device->CreateTexture(
    NkTextureDesc::RenderTarget(W, H, NkGPUFormat::NK_RGBA8_SRGB)
);
NkTextureHandle depthTex = device->CreateTexture(
    NkTextureDesc::DepthStencil(W, H, NkGPUFormat::NK_D32_FLOAT)
);

// Créer le framebuffer
NkFramebufferDesc fbDesc;
fbDesc.renderPass = renderPass;
fbDesc.colorAttachments.PushBack(colorTex);
fbDesc.depthAttachment = depthTex;
fbDesc.width  = W;
fbDesc.height = H;
NkFramebufferHandle offscreenFB = device->CreateFramebuffer(fbDesc);
```

---

## 9. Pipelines graphiques

Le **graphics pipeline** est l'objet qui combine shader + état de rastérisation + blending + layout des vertices.

### 9.1 Layout des vertices

```cpp
NkVertexLayout layout;
layout.AddBinding(0, sizeof(Vertex))  // binding 0, stride = taille d'un vertex
      .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  offsetof(Vertex, x), "POSITION")
      .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT,  offsetof(Vertex, nx), "NORMAL")
      .AddAttribute(2, 0, NkGPUFormat::NK_RG32_FLOAT,   offsetof(Vertex, u), "TEXCOORD");
```

### 9.2 Descriptor set layouts

```cpp
// Set 0 : données par frame (matrices view/proj)
NkDescriptorSetLayoutDesc set0Desc;
set0Desc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_VERTEX);
NkDescSetHandle set0Layout = device->CreateDescriptorSetLayout(set0Desc);

// Set 1 : texture albedo + sampler
NkDescriptorSetLayoutDesc set1Desc;
set1Desc.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
NkDescSetHandle set1Layout = device->CreateDescriptorSetLayout(set1Desc);
```

### 9.3 Créer le pipeline

```cpp
NkGraphicsPipelineDesc pipeDesc;
pipeDesc.shader      = shader;
pipeDesc.vertexLayout= layout;
pipeDesc.renderPass  = renderPass;
pipeDesc.topology    = NkPrimitiveTopology::NK_TRIANGLE_LIST;
pipeDesc.rasterizer  = NkRasterizerDesc::Default();  // culling back, solid
pipeDesc.depthStencil= NkDepthStencilDesc::Default(); // depth test + write
pipeDesc.blend       = NkBlendDesc::Opaque();
pipeDesc.debugName   = "MeshPipeline";

// Déclarer les push constants (obligatoire sur Vulkan)
pipeDesc.AddPushConstant(NkShaderStage::NK_VERTEX, 0, 64); // mat4 model

// Layouts des descriptor sets
pipeDesc.descriptorSetLayouts.PushBack(set0Layout);
pipeDesc.descriptorSetLayouts.PushBack(set1Layout);

NkPipelineHandle pipeline = device->CreateGraphicsPipeline(pipeDesc);
```

### 9.4 Allouer et remplir les descriptor sets

```cpp
// Allouer les instances
NkDescSetHandle set0 = device->AllocateDescriptorSet(set0Layout);
NkDescSetHandle set1 = device->AllocateDescriptorSet(set1Layout);

// Lier le UBO au set 0, binding 0
device->BindUniformBuffer(set0, 0, ubHandle, sizeof(PerFrameUniforms));

// Lier la texture+sampler au set 1, binding 0
device->BindTextureSampler(set1, 0, texHandle, sampHandle);
```

### 9.5 Utilisation dans le command buffer

```cpp
cmd->BindGraphicsPipeline(pipeline);
cmd->BindDescriptorSet(set0, 0);
cmd->BindDescriptorSet(set1, 1);
cmd->BindVertexBuffer(0, vbHandle);
cmd->BindIndexBuffer(ibHandle, NkIndexFormat::NK_UINT16);

float model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
cmd->PushConstants(NkShaderStage::NK_VERTEX, 0, 64, model);
cmd->DrawIndexed(6);
```

---

## 10. Rendu 3D : éclairage et ombres

### 10.1 Éclairage Phong (dans le shader fragment)

```glsl
// Fragment shader éclairage Phong
layout(set = 0, binding = 0) uniform PerFrame {
    mat4  viewProj;
    vec3  cameraPos;
    vec3  lightPos;
    vec3  lightColor;
};

void main() {
    vec3 albedo   = texture(uAlbedo, vUV).rgb;
    vec3 N        = normalize(vNormal);
    vec3 L        = normalize(lightPos - vWorldPos);
    vec3 V        = normalize(cameraPos - vWorldPos);
    vec3 H        = normalize(L + V);  // half-vector (Blinn-Phong)

    float ambient  = 0.05;
    float diffuse  = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 64.0);

    vec3 color = lightColor * albedo * (ambient + diffuse) + vec3(1.0) * specular * 0.5;
    fragColor = vec4(color, 1.0);
}
```

### 10.2 Shadow maps

Les shadow maps nécessitent deux passes :

1. **Shadow pass** : rendu depuis la lumière → produit la depth map
2. **Color pass** : rendu depuis la caméra → compare depth avec la shadow map

**Shadow pass — vertex shader** (`shadow.vert.glsl`) :

```glsl
#version 460 core
layout(location = 0) in vec3 aPosition;

layout(push_constant) uniform PC {
    mat4 lightViewProj;
    mat4 model;
};

void main() {
    gl_Position = lightViewProj * model * vec4(aPosition, 1.0);
}
```

**Shadow pass — fragment shader** (`shadow.frag.glsl`) :

```glsl
#version 460 core
// Pas de sortie couleur nécessaire — seul le depth buffer compte
void main() {}
```

**Configuration du pipeline shadow** :

```cpp
NkRenderPassDesc shadowRPDesc = NkRenderPassDesc::ShadowMap(NkGPUFormat::NK_D32_FLOAT);
NkRenderPassHandle shadowRP = device->CreateRenderPass(shadowRPDesc);

NkTextureHandle shadowMap = device->CreateTexture(
    NkTextureDesc::DepthStencil(2048, 2048, NkGPUFormat::NK_D32_FLOAT)
);

NkFramebufferDesc shadowFBDesc;
shadowFBDesc.renderPass = shadowRP;
shadowFBDesc.depthAttachment = shadowMap;
shadowFBDesc.width = shadowFBDesc.height = 2048;
NkFramebufferHandle shadowFB = device->CreateFramebuffer(shadowFBDesc);

NkGraphicsPipelineDesc shadowPipeDesc;
shadowPipeDesc.shader      = shadowShader;
shadowPipeDesc.renderPass  = shadowRP;
shadowPipeDesc.rasterizer  = NkRasterizerDesc::ShadowMap(); // depth bias activé
shadowPipeDesc.depthStencil= NkDepthStencilDesc::Default();
shadowPipeDesc.blend       = NkBlendDesc::Opaque();
NkPipelineHandle shadowPipeline = device->CreateGraphicsPipeline(shadowPipeDesc);
```

**Fragment shader du color pass avec shadow** :

```glsl
layout(set = 2, binding = 0) uniform sampler2DShadow uShadowMap;
layout(set = 0, binding = 0) uniform PerFrame {
    mat4 viewProj;
    mat4 lightViewProj;
    vec3 cameraPos;
    vec3 lightPos;
};

float ShadowFactor(vec3 worldPos) {
    vec4 shadowCoord = lightViewProj * vec4(worldPos, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xy  = shadowCoord.xy * 0.5 + 0.5;
    // PCF : comparaison via sampler2DShadow
    return texture(uShadowMap, shadowCoord.xyz);
}

void main() {
    float shadow = ShadowFactor(vWorldPos);
    vec3 color   = albedo * (ambient + shadow * diffuse);
    fragColor    = vec4(color, 1.0);
}
```

**Boucle de rendu avec deux passes** :

```cpp
cmd->Begin();

// === Passe 1 : Shadow Map ===
cmd->BeginDebugGroup("ShadowPass");
cmd->BeginRenderPass(shadowRP, shadowFB, {{0,0},{2048,2048}});
cmd->SetViewport({0,0,2048,2048,0,1});
cmd->BindGraphicsPipeline(shadowPipeline);
// Dessiner toutes les géométries qui projettent des ombres
for (auto& mesh : scene.meshes) {
    cmd->PushConstants(NkShaderStage::NK_VERTEX, 0, 128, &mesh.shadowPC);
    cmd->BindVertexBuffer(0, mesh.vb);
    cmd->BindIndexBuffer(mesh.ib, NkIndexFormat::NK_UINT32);
    cmd->DrawIndexed(mesh.indexCount);
}
cmd->EndRenderPass();
cmd->EndDebugGroup();

// Transition de la shadow map : depth_write → shader_read
cmd->TextureBarrier(shadowMap,
    NkResourceState::NK_DEPTH_WRITE,
    NkResourceState::NK_SHADER_READ,
    NkPipelineStage::NK_LATE_FRAGMENT,
    NkPipelineStage::NK_FRAGMENT_SHADER
);

// === Passe 2 : Rendu couleur ===
cmd->BeginDebugGroup("ColorPass");
cmd->BeginRenderPass(renderPass, device->GetSwapchainFramebuffer(), fullArea);
cmd->SetViewport({0,0,(float)W,(float)H,0,1});
cmd->BindGraphicsPipeline(pipeline);
cmd->BindDescriptorSet(set0, 0);  // matrices + lumière
cmd->BindDescriptorSet(set1, 1);  // albedo
cmd->BindDescriptorSet(shadowSet, 2); // shadow map
// ... draw calls ...
cmd->EndRenderPass();
cmd->EndDebugGroup();

cmd->End();
device->SubmitAndPresent(cmd);
```

---

## 11. Rendu 2D : overlay et dessin en écran

L'overlay 2D (HUD, UI, texte) se dessine après le rendu 3D, **dans le même render pass**, avec un pipeline sans depth test et avec blending alpha.

### 11.1 Shader 2D simple

```glsl
// sprite.vert.glsl
#version 460 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform PC {
    mat4 orthoProj;  // matrice orthographique pour l'écran
};

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = orthoProj * vec4(aPos, 0.0, 1.0);
}
```

```glsl
// sprite.frag.glsl
#version 460 core
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(set = 0, binding = 0) uniform sampler2D uSprite;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = texture(uSprite, vUV) * vColor;
}
```

### 11.2 Pipeline overlay 2D

```cpp
NkGraphicsPipelineDesc spritePipeDesc;
spritePipeDesc.shader      = spriteShader;
spritePipeDesc.vertexLayout= spriteLayout;
spritePipeDesc.renderPass  = renderPass; // même render pass que le 3D
spritePipeDesc.rasterizer  = NkRasterizerDesc::NoCull();
spritePipeDesc.depthStencil= NkDepthStencilDesc::NoDepth(); // pas de depth test !
spritePipeDesc.blend       = NkBlendDesc::Alpha();           // blending alpha
NkPipelineHandle spritePipeline = device->CreateGraphicsPipeline(spritePipeDesc);
```

### 11.3 Matrice orthographique et vertex buffer dynamique

```cpp
struct SpriteVertex { float x,y,u,v; float r,g,b,a; };

// Buffer dynamique (mis à jour chaque frame)
NkBufferHandle spriteVB = device->CreateBuffer(
    NkBufferDesc::VertexDynamic(sizeof(SpriteVertex) * 4096)
);

// Projection orthographique (pixel-perfect)
float ortho[16];
MakeOrtho(ortho, 0, (float)W, (float)H, 0, -1, 1); // origine haut-gauche

// Dans la boucle de rendu, après le 3D :
cmd->BindGraphicsPipeline(spritePipeline);
cmd->PushConstants(NkShaderStage::NK_VERTEX, 0, 64, ortho);
// Uploader et dessiner les sprites batchés
device->WriteBuffer(spriteVB, spriteData.data(), spriteData.size() * sizeof(SpriteVertex));
cmd->BindVertexBuffer(0, spriteVB);
cmd->Draw(spriteData.size());
```

---

## 12. Dessin offscreen (render-to-texture)

Le rendu offscreen consiste à dessiner dans une texture plutôt que directement à l'écran. Cela sert pour les post-effects, les reflets, les miniatures, etc.

### 12.1 Setup complet

```cpp
// 1. Créer les textures de sortie
NkTextureHandle offscreenColor = device->CreateTexture(
    NkTextureDesc::RenderTarget(1024, 1024, NkGPUFormat::NK_RGBA16_FLOAT)
);
NkTextureHandle offscreenDepth = device->CreateTexture(
    NkTextureDesc::DepthStencil(1024, 1024)
);

// 2. Créer le render pass offscreen
NkRenderPassDesc offRPDesc;
offRPDesc.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA16_FLOAT))
         .SetDepth(NkAttachmentDesc::Depth());
NkRenderPassHandle offRP = device->CreateRenderPass(offRPDesc);

// 3. Créer le framebuffer offscreen
NkFramebufferDesc offFBDesc;
offFBDesc.renderPass = offRP;
offFBDesc.colorAttachments.PushBack(offscreenColor);
offFBDesc.depthAttachment = offscreenDepth;
offFBDesc.width = offFBDesc.height = 1024;
NkFramebufferHandle offFB = device->CreateFramebuffer(offFBDesc);
```

### 12.2 Utilisation en deux passes

```cpp
cmd->Begin();

// Passe offscreen : rendre la scène dans la texture
cmd->BeginRenderPass(offRP, offFB, {{0,0},{1024,1024}});
cmd->SetViewport({0,0,1024,1024,0,1});
// ... dessiner la scène depuis une autre caméra (ex: reflet, portal) ...
cmd->EndRenderPass();

// Transition : render_target → shader_read
cmd->TextureBarrier(offscreenColor,
    NkResourceState::NK_RENDER_TARGET,
    NkResourceState::NK_SHADER_READ,
    NkPipelineStage::NK_COLOR_ATTACHMENT,
    NkPipelineStage::NK_FRAGMENT_SHADER
);

// Passe principale : utiliser la texture comme entrée
cmd->BeginRenderPass(device->GetSwapchainRenderPass(),
                      device->GetSwapchainFramebuffer(), mainArea);
// Lier offscreenColor comme texture dans un descriptor set
device->BindTextureSampler(postFxSet, 0, offscreenColor, sampHandle);
cmd->BindGraphicsPipeline(postFxPipeline);
cmd->BindDescriptorSet(postFxSet, 0);
cmd->Draw(3); // triangle plein écran
cmd->EndRenderPass();

cmd->End();
```

---

## 13. MSAA (anticrénelage multi-échantillons)

### 13.1 Configurer le pipeline MSAA

```cpp
// Texture MSAA 4x
NkTextureHandle msaaColor = device->CreateTexture(
    NkTextureDesc::RenderTarget(W, H, NkGPUFormat::NK_RGBA8_SRGB, NkSampleCount::NK_S4)
);

// Render pass avec resolve (MSAA → non-MSAA)
NkRenderPassDesc msaaRPDesc;
NkAttachmentDesc colorAtt;
colorAtt.format  = NkGPUFormat::NK_RGBA8_SRGB;
colorAtt.samples = NkSampleCount::NK_S4;
msaaRPDesc.AddColor(colorAtt)
          .SetDepth({NkGPUFormat::NK_D32_FLOAT, NkSampleCount::NK_S4})
          .SetResolve(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_SRGB)); // resolve vers 1x

// Le pipeline doit aussi déclarer MSAA 4x
pipeDesc.samples = NkSampleCount::NK_S4;
```

---

## 14. Compute shaders

Les compute shaders tournent entièrement sur GPU, hors du pipeline graphique.

### 14.1 Shader GLSL compute

```glsl
// blur.comp.glsl — flou gaussien horizontal
#version 460 core
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D uInput;
layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D uOutput;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size  = imageSize(uOutput);
    if (coord.x >= size.x || coord.y >= size.y) return;

    vec2 texel = 1.0 / vec2(size);
    float weights[5] = float[](0.227, 0.194, 0.121, 0.054, 0.016);
    vec4  result = vec4(0.0);

    for (int i = -4; i <= 4; i++) {
        vec2 uv = (vec2(coord) + vec2(i, 0)) * texel;
        result += texture(uInput, uv) * weights[abs(i)];
    }
    imageStore(uOutput, coord, result);
}
```

### 14.2 Créer et dispatcher un compute pipeline

```cpp
NkShaderDesc computeShaderDesc;
computeShaderDesc.AddGLSL(NkShaderStage::NK_COMPUTE, blurGlslSource);
NkShaderHandle computeShader = device->CreateShader(computeShaderDesc);

NkComputePipelineDesc computePipeDesc;
computePipeDesc.shader = computeShader;
NkPipelineHandle computePipeline = device->CreateComputePipeline(computePipeDesc);

// Dispatch dans le command buffer
cmd->BindComputePipeline(computePipeline);
cmd->BindDescriptorSet(computeSet, 0);
// Groupes de (16×16) → pour une image 1024×1024 : 64×64 groupes
cmd->Dispatch(1024 / 16, 1024 / 16, 1);

// Barrière UAV : attendre la fin de l'écriture compute avant lecture fragment
cmd->TextureBarrier(outputTex,
    NkResourceState::NK_UNORDERED_ACCESS,
    NkResourceState::NK_SHADER_READ,
    NkPipelineStage::NK_COMPUTE_SHADER,
    NkPipelineStage::NK_FRAGMENT_SHADER
);
```

---

## 15. Techniques avancées

### 15.1 Cubemaps (skybox, IBL)

```cpp
// Créer la texture cubemap (6 faces)
NkTextureHandle cubemap = device->CreateTexture(
    NkTextureDesc::Cubemap(512, NkGPUFormat::NK_RGBA16_FLOAT, /*mips=*/0)
);

// Uploader chaque face
for (uint32_t face = 0; face < 6; face++) {
    device->WriteTextureRegion(cubemap,
        facePixels[face],
        0, 0, 0,        // x,y,z
        512, 512, 1,    // width,height,depth
        0, face,        // mip, layer
        512 * 8         // rowPitch pour RGBA16F
    );
}

// Dans le shader
// layout(set = 0, binding = 0) uniform samplerCube uSkybox;
// vec3 color = texture(uSkybox, normalize(vDir)).rgb;
```

### 15.2 GBuffer pour le rendu différé

```cpp
// Créer le render pass GBuffer (4 color + depth)
NkRenderPassDesc gbufferRP = NkRenderPassDesc::GBuffer();
// → Albedo (RGBA8_SRGB) + Normal (RGBA16F) + ORM (RGBA8) + Emission (RGBA16F) + Depth

NkFramebufferDesc gbufferFBDesc;
gbufferFBDesc.renderPass = gbufferRPHandle;
gbufferFBDesc.colorAttachments.PushBack(albedoTex);
gbufferFBDesc.colorAttachments.PushBack(normalTex);
gbufferFBDesc.colorAttachments.PushBack(ormTex);
gbufferFBDesc.colorAttachments.PushBack(emissionTex);
gbufferFBDesc.depthAttachment = depthTex;
gbufferFBDesc.width = W; gbufferFBDesc.height = H;
```

**Fragment shader GBuffer** :

```glsl
layout(location = 0) out vec4 gAlbedo;   // RGB = albedo
layout(location = 1) out vec4 gNormal;   // RGB = normal world-space (encodée)
layout(location = 2) out vec4 gORM;      // R=occlusion, G=roughness, B=metallic
layout(location = 3) out vec4 gEmission; // RGB = émission

void main() {
    gAlbedo   = vec4(texture(uAlbedo, vUV).rgb, 1.0);
    gNormal   = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    gORM      = texture(uORM, vUV);
    gEmission = texture(uEmission, vUV);
}
```

**Lighting pass** (quad plein écran lit les 4 GBuffer textures) :

```glsl
// lighting.frag.glsl
layout(set = 0, binding = 0) uniform sampler2D gAlbedo;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gORM;
layout(set = 0, binding = 3) uniform sampler2D gDepth;
layout(set = 0, binding = 4) uniform sampler2DShadow gShadow;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec3 albedo   = texture(gAlbedo, uv).rgb;
    vec3 N        = normalize(texture(gNormal, uv).rgb * 2.0 - 1.0);
    float roughness = texture(gORM, uv).g;
    float metallic  = texture(gORM, uv).b;

    // Reconstituer la position depuis le depth
    float depth = texture(gDepth, uv).r;
    vec3 worldPos = ReconstructPosition(uv, depth, invViewProj);

    // PBR lighting
    vec3 color = PBR_BRDF(albedo, N, worldPos, roughness, metallic, lights);
    fragColor = vec4(color, 1.0);
}
```

---

## 16. Gestion des ressources et bonnes pratiques

### 16.1 Cycle de vie correct

```cpp
// À la fin de l'application, TOUJOURS dans cet ordre :
device->WaitIdle();  // attendre que le GPU ait fini

// Détruire dans l'ordre inverse de création
device->DestroyPipeline(pipeline);
device->FreeDescriptorSet(set0);
device->FreeDescriptorSet(set1);
device->DestroyDescriptorSetLayout(set0Layout);
device->DestroyDescriptorSetLayout(set1Layout);
device->DestroyShader(shader);
device->DestroyBuffer(vbHandle);
device->DestroyBuffer(ibHandle);
device->DestroyBuffer(ubHandle);
device->DestroySampler(sampHandle);
device->DestroyTexture(texHandle);
device->DestroyFramebuffer(offFB);
device->DestroyRenderPass(offRP);

cmdPool.Reset();
NkDeviceFactory::Destroy(device);
```

### 16.2 Nommage pour le débogage (RenderDoc, PIX, Xcode)

```cpp
device->SetDebugName(vbHandle,  "Terrain_VertexBuffer");
device->SetDebugName(texHandle, "Terrain_AlbedoTexture");
device->SetDebugName(pipeline,  "Terrain_Pipeline");

cmd->BeginDebugGroup("ShadowPass", 1,0.5f,0);
// ... draw calls ...
cmd->EndDebugGroup();
```

### 16.3 Statistiques de frame

```cpp
const NkIDevice::FrameStats& stats = device->GetLastFrameStats();
// stats.drawCalls, stats.triangles, stats.gpuTimeMs, ...
device->ResetFrameStats();
```

### 16.4 Résumé du pipeline de rendu complet

```
frame N :
├── BeginFrame()
├── [optional] Compute pass (skinning, particles, blur)
│     ├── Dispatch()
│     └── UAV Barrier
├── Shadow pass (depth only, light POV)
│     ├── BeginRenderPass(shadowRP, shadowFB)
│     ├── Draw scene geometry
│     └── EndRenderPass()  →  TextureBarrier → shader_read
├── GBuffer pass (deferred) ou Color pass (forward)
│     ├── BeginRenderPass(gbufferRP / mainRP, fb)
│     ├── BindPipeline + BindDescriptors + PushConstants
│     ├── Draw 3D meshes
│     └── EndRenderPass()
├── [optional] Lighting pass (deferred)
│     ├── Lire GBuffer, calculer PBR + ombres
│     └── EndRenderPass()
├── [optional] Post-processing (bloom, tonemap, FXAA)
│     ├── Compute ou fullscreen quad
│     └── TextureBarrier
├── 2D overlay pass (HUD, UI, texte)
│     ├── BindPipeline(sprite, no-depth, alpha-blend)
│     ├── Draw sprites batchés
│     └── EndRenderPass()
├── SubmitAndPresent(cmd)
└── EndFrame()
```

---

Ce cours couvre l'intégralité du système graphique exposé par l'API Nkentseu. Chaque concept — des primitives de base aux techniques avancées comme le rendu différé et les compute shaders — s'appuie directement sur les interfaces et descripteurs définis dans les headers fournis. L'abstraction RHI vous permet d'écrire ce code une seule fois et de le faire fonctionner sur OpenGL, Vulkan, DirectX 12, Metal ou le rasterizer logiciel sans modifier votre logique applicative.
