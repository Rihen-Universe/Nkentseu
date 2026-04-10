# Cours complet : Créer une application 2D/3D avec le RHI Nkentseu

Ce cours couvre l'intégralité du système, de la fenêtre à l'écran, en passant par les shaders, les textures, les ombres, le dessin offscreen et les overlays.

---

## Module 1 — Architecture générale du système

Avant de coder, il faut comprendre comment les couches s'emboîtent.

```
NkSystem / NkEventSystem       ← gestion des événements OS
       ↓
NkWindow + NkSurfaceDesc       ← fenêtre native + descripteur de surface
       ↓
NkDeviceFactory → NkIDevice    ← device GPU (OpenGL, Vulkan, DX12, Metal…)
       ↓
NkISwapchain                   ← surface de présentation (frame → écran)
       ↓
NkICommandBuffer                ← enregistrement des commandes GPU
       ↓
Pipeline / RenderPass / Shader  ← description du rendu
```

Le principe fondamental : **NkWindow ne sait pas qui rend**. Elle expose un `NkSurfaceDesc` — un struct de handles natifs (HWND, wl\_surface, NSView…) — et le backend graphique fait le reste.

---

## Module 2 — Initialisation et fenêtre

### 2.1 Démarrer le système

```cpp
#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"
#include "NKRHI/Core/NkDeviceFactory.h"

using namespace nkentseu;

int nkmain(const NkEntryState& state) {
    // Initialiser le système (events, gamepads, OLE sur Win32…)
    NkInitialise({ .appName = "MonApp" });

    // Créer la fenêtre
    NkWindowConfig cfg;
    cfg.title    = "Mon Application";
    cfg.width    = 1280;
    cfg.height   = 720;
    cfg.centered = true;
    cfg.resizable = true;

    NkWindow window;
    if (!window.Create(cfg)) return -1;

    // … suite …

    NkClose();
    return 0;
}
```

### 2.2 Récupérer le descripteur de surface

```cpp
NkSurfaceDesc surface = window.GetSurfaceDesc();
// surface.hwnd, surface.width, surface.height, surface.dpi…
// Ce struct est le SEUL lien entre la fenêtre et le GPU.
```

---

## Module 3 — Créer le device GPU

### 3.1 Remplir le NkDeviceInitInfo

```cpp
NkDeviceInitInfo init;
init.api     = NkGraphicsApi::NK_API_OPENGL;   // ou VULKAN, D3D12, METAL…
init.context = NkContextDesc::MakeOpenGL(4, 6, /*debug=*/true);
init.surface = window.GetSurfaceDesc();
init.width   = cfg.width;
init.height  = cfg.height;
```

### 3.2 Créer le device via la factory

```cpp
NkIDevice* device = NkDeviceFactory::Create(init);
if (!device) {
    // Fallback automatique : essayer OpenGL puis Software
    device = NkDeviceFactory::CreateWithFallback(init, {
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE
    });
}
```

### 3.3 Informations sur le device

```cpp
const NkDeviceCaps& caps = device->GetCaps();
// caps.maxTextureDim2D, caps.msaa4x, caps.computeShaders…

NkContextInfo info = device->GetContextInfo();
// info.renderer, info.vramMB, info.api…
```

---

## Module 4 — Boucle de rendu principale

### 4.1 Structure de la boucle

```cpp
NkFrameContext frame;
bool running = true;

while (running) {
    // 1. Pomper les événements OS
    while (NkEvent* ev = NkEvents().PollEvent()) {
        if (ev->As<NkWindowCloseEvent>()) running = false;
        if (auto* resize = ev->As<NkGraphicsContextResizeEvent>()) {
            device->OnResize(resize->GetWidth(), resize->GetHeight());
        }
    }

    // 2. Début de frame GPU
    if (!device->BeginFrame(frame)) continue;

    // 3. Acquérir le command buffer
    NkICommandBuffer* cmd = device->CreateCommandBuffer();
    cmd->Begin();

    // 4. Enregistrer les commandes (voir modules suivants)
    // …

    // 5. Soumettre et présenter
    cmd->End();
    device->SubmitAndPresent(cmd);
    device->EndFrame(frame);
    device->DestroyCommandBuffer(cmd);
}

device->WaitIdle();
NkDeviceFactory::Destroy(device);
window.Close();
```

---

## Module 5 — Shaders

### 5.1 Écrire un shader GLSL

Convention de nommage des fichiers :

```
shader.vert.glsl   ← vertex
shader.frag.glsl   ← fragment
shader.comp.glsl   ← compute
```

**Vertex shader minimal :**

```glsl
// triangle.vert.glsl
#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;
    outNormal   = inNormal;
}
```

**Fragment shader minimal :**

```glsl
// triangle.frag.glsl
#version 460 core

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1) uniform UBO {
    vec4 lightDir;
    vec4 lightColor;
} ubo;

void main() {
    vec3 n    = normalize(inNormal);
    float ndl = max(dot(n, -ubo.lightDir.xyz), 0.0);
    vec4  tex = texture(uTexture, inTexCoord);
    outColor  = tex * vec4(ubo.lightColor.rgb * ndl, 1.0);
}
```

### 5.2 Compiler le shader (GLSL → SPIR-V → RHI)

```cpp
#include "NKRHI/ShaderConvert/NkShaderConvert.h"

// Option A — depuis des fichiers sur disque
auto vertSpv = NkShaderConverter::LoadAsSpirv("triangle.vert.glsl");
auto fragSpv = NkShaderConverter::LoadAsSpirv("triangle.frag.glsl");

if (!vertSpv.success) { /* afficher vertSpv.errors */ }

// Option B — depuis une string en mémoire
NkString glslSrc = "...";
auto spv = NkShaderConverter::GlslToSpirv(glslSrc, NkSLStage::NK_VERTEX, "myVert");

// Cache disque pour éviter la recompilation au prochain lancement
NkShaderCache::Global().SetCacheDir("Build/ShaderCache");
uint64 key = NkShaderCache::ComputeKey(glslSrc, NkSLStage::NK_VERTEX);
auto cached = NkShaderCache::Global().Load(key);
if (!cached.success) {
    cached = NkShaderConverter::GlslToSpirv(glslSrc, NkSLStage::NK_VERTEX);
    NkShaderCache::Global().Save(key, cached);
}
```

### 5.3 Créer le NkShaderHandle

```cpp
NkShaderDesc shaderDesc;
shaderDesc
    .AddSPIRV(NkShaderStage::NK_VERTEX,
              vertSpv.SpirvWords(), vertSpv.SpirvWordCount() * 4)
    .AddSPIRV(NkShaderStage::NK_FRAGMENT,
              fragSpv.SpirvWords(), fragSpv.SpirvWordCount() * 4);

// Pour OpenGL (GLSL direct) :
shaderDesc
    .AddGLSL(NkShaderStage::NK_VERTEX,   vertGlslSource)
    .AddGLSL(NkShaderStage::NK_FRAGMENT, fragGlslSource);

// Pour DX11/12 (HLSL) :
shaderDesc
    .AddHLSL(NkShaderStage::NK_VERTEX,   hlslSource, "VSMain")
    .AddHLSL(NkShaderStage::NK_FRAGMENT, hlslSource, "PSMain");

NkShaderHandle shader = device->CreateShader(shaderDesc);
```

---

## Module 6 — Buffers (vertex, index, uniforms)

### 6.1 Vertex buffer statique

```cpp
struct Vertex {
    float position[3];
    float texCoord[2];
    float normal[3];
};

Vertex verts[] = {
    {{ -0.5f, -0.5f, 0.f }, { 0.f, 0.f }, { 0.f, 0.f, 1.f }},
    {{  0.5f, -0.5f, 0.f }, { 1.f, 0.f }, { 0.f, 0.f, 1.f }},
    {{  0.f,   0.5f, 0.f }, { 0.5f,1.f }, { 0.f, 0.f, 1.f }},
};

NkBufferHandle vbo = device->CreateBuffer(
    NkBufferDesc::Vertex(sizeof(verts), verts)
);
```

### 6.2 Index buffer

```cpp
uint16_t indices[] = { 0, 1, 2 };
NkBufferHandle ibo = device->CreateBuffer(
    NkBufferDesc::Index(sizeof(indices), indices)
);
```

### 6.3 Uniform buffer (données changeant par frame)

```cpp
struct FrameUBO {
    float lightDir[4];
    float lightColor[4];
};

// Usage NK_UPLOAD = CPU écrit, GPU lit
NkBufferHandle ubo = device->CreateBuffer(
    NkBufferDesc::Uniform(sizeof(FrameUBO))
);

// Mise à jour chaque frame
FrameUBO data = { {0.f, -1.f, 0.f, 0.f}, {1.f, 1.f, 1.f, 1.f} };
device->WriteBuffer(ubo, &data, sizeof(data));
```

### 6.4 Vertex layout

```cpp
NkVertexLayout layout;
layout
    .AddBinding(0, sizeof(Vertex))
    .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  offsetof(Vertex, position), "POSITION")
    .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   offsetof(Vertex, texCoord), "TEXCOORD")
    .AddAttribute(2, 0, NkGPUFormat::NK_RGB32_FLOAT,  offsetof(Vertex, normal),   "NORMAL");
```

---

## Module 7 — Textures et samplers

### 7.1 Charger une texture 2D

```cpp
// Supposons que vous ayez des pixels RGBA8 (ex. chargés depuis stb_image)
uint32_t texWidth = 512, texHeight = 512;
uint8_t* pixels = /* votre chargeur d'image */;

NkTextureHandle tex = device->CreateTexture(
    NkTextureDesc::Tex2D(texWidth, texHeight, NkGPUFormat::NK_RGBA8_SRGB, /*mips=*/0)
);

// 0 mips = toute la chaîne mipmaps générée automatiquement
device->WriteTexture(tex, pixels, texWidth * 4);
device->GenerateMipmaps(tex);
```

### 7.2 Créer un sampler

```cpp
// Sampler standard linéaire avec répétition
NkSamplerHandle sampler = device->CreateSampler(NkSamplerDesc::Linear());

// Sampler anisotropique x16
NkSamplerHandle samplerAniso = device->CreateSampler(NkSamplerDesc::Anisotropic(16.f));

// Sampler pour shadow map (compare + clamp)
NkSamplerHandle samplerShadow = device->CreateSampler(NkSamplerDesc::Shadow());
```

### 7.3 Texture Render Target (pour le rendu offscreen)

```cpp
NkTextureHandle colorRT = device->CreateTexture(
    NkTextureDesc::RenderTarget(1280, 720, NkGPUFormat::NK_RGBA8_SRGB)
);
NkTextureHandle depthRT = device->CreateTexture(
    NkTextureDesc::DepthStencil(1280, 720, NkGPUFormat::NK_D32_FLOAT)
);
```

### 7.4 Cubemap (pour l'environnement / skybox)

```cpp
NkTextureHandle cubemap = device->CreateTexture(
    NkTextureDesc::Cubemap(512, NkGPUFormat::NK_RGBA16_FLOAT, /*mips=*/0)
);
// Uploader chacune des 6 faces
for (uint32 face = 0; face < 6; face++) {
    device->WriteTextureRegion(cubemap, facePixels[face],
        0, 0, 0, 512, 512, 1, /*mip=*/0, /*layer=*/face);
}
```

---

## Module 8 — Render Pass et Framebuffer

Le **RenderPass** décrit la structure des attachements (format, load/store ops). Le **Framebuffer** lie le RenderPass à des textures concrètes.

### 8.1 Render pass forward standard

```cpp
NkRenderPassHandle mainPass = device->CreateRenderPass(
    NkRenderPassDesc::Forward(
        NkGPUFormat::NK_RGBA8_SRGB,   // couleur
        NkGPUFormat::NK_D32_FLOAT,    // profondeur
        NkSampleCount::NK_S4          // MSAA x4
    )
);
```

### 8.2 Render pass offscreen

```cpp
NkRenderPassDesc offscreenDesc;
offscreenDesc
    .AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA16_FLOAT))
    .SetDepth(NkAttachmentDesc::Depth(NkGPUFormat::NK_D32_FLOAT));

NkRenderPassHandle offscreenPass = device->CreateRenderPass(offscreenDesc);
```

### 8.3 Framebuffer

```cpp
NkFramebufferDesc fbDesc;
fbDesc.renderPass = offscreenPass;
fbDesc.colorAttachments.PushBack(colorRT);
fbDesc.depthAttachment  = depthRT;
fbDesc.width  = 1280;
fbDesc.height = 720;

NkFramebufferHandle offscreenFB = device->CreateFramebuffer(fbDesc);
```

### 8.4 Render pass shadow map (depth only)

```cpp
NkRenderPassHandle shadowPass = device->CreateRenderPass(
    NkRenderPassDesc::ShadowMap(NkGPUFormat::NK_D32_FLOAT)
);

NkTextureHandle shadowMap = device->CreateTexture(
    NkTextureDesc::DepthStencil(2048, 2048, NkGPUFormat::NK_D32_FLOAT)
);

NkFramebufferDesc shadowFBDesc;
shadowFBDesc.renderPass       = shadowPass;
shadowFBDesc.depthAttachment  = shadowMap;
shadowFBDesc.width = shadowFBDesc.height = 2048;

NkFramebufferHandle shadowFB = device->CreateFramebuffer(shadowFBDesc);
```

---

## Module 9 — Descriptor Sets (binding des ressources dans les shaders)

### 9.1 Créer le layout

```cpp
NkDescriptorSetLayoutDesc layoutDesc;
layoutDesc
    .Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)  // texture
    .Add(1, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_FRAGMENT);          // ubo

NkDescSetHandle layout = device->CreateDescriptorSetLayout(layoutDesc);
```

### 9.2 Allouer et remplir un descriptor set

```cpp
NkDescSetHandle descSet = device->AllocateDescriptorSet(layout);

// Binder texture + sampler
device->BindTextureSampler(descSet, 0, tex, sampler);

// Binder un uniform buffer
device->BindUniformBuffer(descSet, 1, ubo, sizeof(FrameUBO));
```

---

## Module 10 — Pipeline graphique

Le pipeline compile et verrouille tous les états GPU.

### 10.1 Pipeline 3D standard

```cpp
NkGraphicsPipelineDesc pipeDesc;
pipeDesc.shader      = shader;
pipeDesc.vertexLayout = layout;
pipeDesc.topology    = NkPrimitiveTopology::NK_TRIANGLE_LIST;
pipeDesc.rasterizer  = NkRasterizerDesc::Default();    // solid, cull back
pipeDesc.depthStencil = NkDepthStencilDesc::Default(); // depth test+write, less
pipeDesc.blend       = NkBlendDesc::Opaque();
pipeDesc.samples     = NkSampleCount::NK_S4;
pipeDesc.renderPass  = mainPass;
pipeDesc.descriptorSetLayouts.PushBack(layout);
pipeDesc.AddPushConstant(NkShaderStage::NK_VERTEX, 0, sizeof(float) * 16); // MVP mat4

NkPipelineHandle pipeline = device->CreateGraphicsPipeline(pipeDesc);
```

### 10.2 Pipeline 2D (overlay, UI)

```cpp
NkGraphicsPipelineDesc pipe2D;
pipe2D.shader      = shader2D;
pipe2D.vertexLayout = layout2D;
pipe2D.topology    = NkPrimitiveTopology::NK_TRIANGLE_LIST;
pipe2D.rasterizer  = NkRasterizerDesc::NoCull();       // pas de culling pour la 2D
pipe2D.depthStencil = NkDepthStencilDesc::NoDepth();   // pas de test de profondeur
pipe2D.blend       = NkBlendDesc::Alpha();             // transparence alpha
pipe2D.renderPass  = mainPass;

NkPipelineHandle pipeline2D = device->CreateGraphicsPipeline(pipe2D);
```

### 10.3 Pipeline pour shadow map

```cpp
NkGraphicsPipelineDesc shadowPipeDesc;
shadowPipeDesc.shader      = shadowShader;  // vertex only
shadowPipeDesc.vertexLayout = layout;
shadowPipeDesc.rasterizer  = NkRasterizerDesc::ShadowMap(); // depth bias + cull front
shadowPipeDesc.depthStencil = NkDepthStencilDesc::Default();
shadowPipeDesc.blend       = NkBlendDesc::Opaque();
shadowPipeDesc.renderPass  = shadowPass;

NkPipelineHandle shadowPipeline = device->CreateGraphicsPipeline(shadowPipeDesc);
```

### 10.4 Pipeline wireframe (debug)

```cpp
NkGraphicsPipelineDesc wirePipeDesc = pipeDesc;
wirePipeDesc.rasterizer = NkRasterizerDesc::Wireframe();
NkPipelineHandle wirePipeline = device->CreateGraphicsPipeline(wirePipeDesc);
```

---

## Module 11 — Rendu 3D complet (avec éclairage et ombres)

### 11.1 Pass 1 — Shadow map (rendu offscreen depth only)

```glsl
// shadow.vert.glsl
#version 460 core
layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PC {
    mat4 lightMVP;
} pc;

void main() {
    gl_Position = pc.lightMVP * vec4(inPosition, 1.0);
}
```

```cpp
// Dans la boucle de rendu :
cmd->BeginDebugGroup("ShadowPass");

cmd->TextureBarrier(shadowMap,
    NkResourceState::NK_SHADER_READ, NkResourceState::NK_DEPTH_WRITE,
    NkPipelineStage::NK_FRAGMENT_SHADER, NkPipelineStage::NK_EARLY_FRAGMENT);

NkRect2D shadowArea = {0, 0, 2048, 2048};
cmd->BeginRenderPass(shadowPass, shadowFB, shadowArea);

NkViewport shadowVP = {0, 0, 2048, 2048};
cmd->SetViewport(shadowVP);
cmd->SetScissor(shadowArea);

cmd->BindGraphicsPipeline(shadowPipeline);
cmd->BindVertexBuffer(0, vbo);
cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT16);

// Push la matrice lumière
cmd->PushConstants(NkShaderStage::NK_VERTEX, 0, sizeof(lightMVP), &lightMVP);
cmd->DrawIndexed(indexCount);

cmd->EndRenderPass();

// Transition shadow map → texture lisible
cmd->TextureBarrier(shadowMap,
    NkResourceState::NK_DEPTH_WRITE, NkResourceState::NK_SHADER_READ,
    NkPipelineStage::NK_LATE_FRAGMENT, NkPipelineStage::NK_FRAGMENT_SHADER);

cmd->EndDebugGroup();
```

### 11.2 Shader 3D avec éclairage Phong et ombres

```glsl
// scene.vert.glsl
#version 460 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormalWS;
layout(location = 2) out vec3 outPosWS;
layout(location = 3) out vec4 outPosLightSpace;

layout(push_constant) uniform PC {
    mat4 mvp;
    mat4 model;
    mat4 lightMVP;
} pc;

void main() {
    vec4 worldPos    = pc.model * vec4(inPosition, 1.0);
    gl_Position      = pc.mvp  * vec4(inPosition, 1.0);
    outUV            = inTexCoord;
    outNormalWS      = mat3(transpose(inverse(pc.model))) * inNormal;
    outPosWS         = worldPos.xyz;
    outPosLightSpace = pc.lightMVP * worldPos;
}
```

```glsl
// scene.frag.glsl
#version 460 core
layout(location = 0) in vec2  inUV;
layout(location = 1) in vec3  inNormalWS;
layout(location = 2) in vec3  inPosWS;
layout(location = 3) in vec4  inPosLightSpace;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D   uAlbedo;
layout(set = 0, binding = 1) uniform sampler2DShadow uShadowMap;  // compare sampler

layout(set = 0, binding = 2) uniform SceneUBO {
    vec4 lightDir;
    vec4 lightColor;
    vec4 ambientColor;
    vec4 cameraPos;
} uScene;

float ShadowFactor(vec4 posLightSpace) {
    vec3 proj  = posLightSpace.xyz / posLightSpace.w;
    proj       = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    return texture(uShadowMap, proj);  // PCF hardware
}

void main() {
    vec3 N       = normalize(inNormalWS);
    vec3 L       = normalize(-uScene.lightDir.xyz);
    vec3 V       = normalize(uScene.cameraPos.xyz - inPosWS);
    vec3 H       = normalize(L + V);

    float diff   = max(dot(N, L), 0.0);
    float spec   = pow(max(dot(N, H), 0.0), 64.0);
    float shadow = ShadowFactor(inPosLightSpace);

    vec4 albedo  = texture(uAlbedo, inUV);
    vec3 color   = uScene.ambientColor.rgb * albedo.rgb
                 + shadow * (diff * uScene.lightColor.rgb * albedo.rgb
                           + spec * uScene.lightColor.rgb * 0.5);

    outColor = vec4(color, albedo.a);
}
```

### 11.3 Pass 2 — Rendu principal

```cpp
cmd->BeginDebugGroup("MainPass");

NkRect2D mainArea = {0, 0, 1280, 720};
cmd->BeginRenderPass(device->GetSwapchainRenderPass(),
                     device->GetSwapchainFramebuffer(), mainArea);

NkViewport vp = {0, 0, 1280, 720, 0.f, 1.f, true};
cmd->SetViewport(vp);
cmd->SetScissor(mainArea);

cmd->BindGraphicsPipeline(pipeline);
cmd->BindDescriptorSet(descSet, 0);
cmd->BindVertexBuffer(0, vbo);
cmd->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT16);

struct PushData { float mvp[16]; float model[16]; float lightMVP[16]; } push = /* … */;
cmd->PushConstants(NkShaderStage::NK_VERTEX, 0, sizeof(push), &push);

cmd->DrawIndexed(indexCount);

cmd->EndRenderPass();
cmd->EndDebugGroup();
```

---

## Module 12 — Rendu offscreen et post-processing

L'offscreen consiste à rendre dans une texture, puis à utiliser cette texture dans un pass suivant.

### 12.1 Schéma multi-pass

```
[Pass Géométrie] → colorRT + depthRT (offscreen)
       ↓
[Pass Post-process] → appliquer bloom, tonemap, SSAO…
       ↓
[Pass Final / Blit] → swapchain (écran)
```

### 12.2 Shader de post-process (tonemap + gamma)

```glsl
// postprocess.frag.glsl
#version 460 core
layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uHDRBuffer;

vec3 ToneMapACES(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 hdr    = texture(uHDRBuffer, inUV).rgb;
    vec3 mapped = ToneMapACES(hdr);
    outColor    = vec4(pow(mapped, vec3(1.0/2.2)), 1.0); // gamma correction
}
```

### 12.3 Fullscreen triangle (pas de VBO nécessaire)

```glsl
// fullscreen.vert.glsl — génère un triangle en clip space sans VBO
#version 460 core
layout(location = 0) out vec2 outUV;
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    outUV   = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
```

```cpp
// Rendu du fullscreen triangle (3 vertices, pas de buffer)
cmd->BindGraphicsPipeline(postPipeline);
cmd->BindDescriptorSet(postDescSet, 0);  // lie colorRT comme texture
cmd->Draw(3);  // fullscreen triangle
```

---

## Module 13 — Compute Shaders

### 13.1 Exemple : génération de bruit GPU

```glsl
// noise.comp.glsl
#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) uniform writeonly image2D uOutput;

float hash(uvec2 p) {
    uvec2 q = 1103515245u * ((p >> 1u) ^ (p.yx));
    uint  n = 1103515245u * ((q.x) ^ (q.y >> 3u));
    return float(n) * (1.0 / float(0xffffffffu));
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    float noise = hash(uvec2(coord));
    imageStore(uOutput, coord, vec4(noise, noise, noise, 1.0));
}
```

```cpp
// Créer le pipeline compute
NkComputePipelineDesc compDesc;
compDesc.shader = computeShader;
NkPipelineHandle computePipeline = device->CreateComputePipeline(compDesc);

// Texture UAV (write depuis le compute)
NkTextureHandle noiseTexture = device->CreateTexture(
    NkTextureDesc::Tex3D(512, 512, 1, NkGPUFormat::NK_RGBA8_UNORM)
);

// Dans le command buffer (hors render pass)
cmd->TextureBarrier(noiseTexture,
    NkResourceState::NK_UNDEFINED, NkResourceState::NK_UNORDERED_ACCESS);

cmd->BindComputePipeline(computePipeline);
cmd->BindDescriptorSet(compDescSet, 0);
cmd->Dispatch(512/8, 512/8, 1);  // groups = texture_size / local_size

cmd->UAVBarrier(noiseTexture.id == 0 ? NkBufferHandle{} : NkBufferHandle{});
cmd->TextureBarrier(noiseTexture,
    NkResourceState::NK_UNORDERED_ACCESS, NkResourceState::NK_SHADER_READ);
```

---

## Module 14 — Dessin 2D et overlays

### 14.1 Pipeline overlay (sur le rendu 3D)

L'overlay se dessine **dans le même render pass** que la scène 3D, après les objets opaques, avec le blend alpha activé et sans test de profondeur.

```cpp
// Fin de la scène 3D, toujours dans BeginRenderPass…EndRenderPass

cmd->BindGraphicsPipeline(pipeline2D);   // blend alpha, no depth
cmd->BindDescriptorSet(uiDescSet, 0);   // texture UI / font atlas

// Vertices 2D générés dynamiquement (positions en NDC ou pixels)
device->WriteBuffer(dynamicVBO, uiVertices, uiVertexCount * sizeof(Vertex2D));
cmd->BindVertexBuffer(0, dynamicVBO);
cmd->Draw(uiVertexCount);
```

### 14.2 Shader 2D

```glsl
// ui.vert.glsl
#version 460 core
layout(location = 0) in vec2 inPos;    // pixels
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

layout(push_constant) uniform PC {
    vec2 screenSize;
} pc;

void main() {
    vec2 ndc   = (inPos / pc.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    outUV      = inUV;
    outColor   = inColor;
}
```

```glsl
// ui.frag.glsl
#version 460 core
layout(location = 0) in  vec2 inUV;
layout(location = 1) in  vec4 inColor;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uAtlas;

void main() {
    outColor = inColor * texture(uAtlas, inUV);
}
```

---

## Module 15 — Gestion du redimensionnement

```cpp
NkEvents().AddEventCallback<NkGraphicsContextResizeEvent>(
    [&](NkGraphicsContextResizeEvent* ev) {
        uint32 w = ev->GetWidth(), h = ev->GetHeight();

        // Attendre que le GPU soit idle avant de recréer
        device->WaitIdle();
        device->OnResize(w, h);

        // Recréer les render targets dépendants de la taille
        device->DestroyTexture(colorRT);
        device->DestroyTexture(depthRT);
        device->DestroyFramebuffer(offscreenFB);

        colorRT     = device->CreateTexture(NkTextureDesc::RenderTarget(w, h));
        depthRT     = device->CreateTexture(NkTextureDesc::DepthStencil(w, h));
        offscreenFB = device->CreateFramebuffer(/* … */);

        // Mettre à jour le viewport et la matrice de projection
        projection = MakePerspective(60.f, float(w)/h, 0.1f, 1000.f);
    }
);
```

---

## Module 16 — Gestion des événements graphiques

Le système d'événements couvre aussi le cycle de vie GPU.

```cpp
// Contexte GPU prêt (charger les ressources ici)
NkEvents().AddEventCallback<NkGraphicsContextReadyEvent>(
    [&](NkGraphicsContextReadyEvent* ev) {
        // Charger shaders, textures, pipelines…
        LoadGPUResources(device, ev->GetApi());
    }
);

// Contexte GPU perdu (device removed, mise en veille)
NkEvents().AddEventCallback<NkGraphicsContextLostEvent>(
    [&](NkGraphicsContextLostEvent* ev) {
        device->WaitIdle();
        UnloadGPUResources(device);
        // Attendre NkGraphicsContextReadyEvent pour recréer
    }
);

// Pression mémoire GPU (mobile)
NkEvents().AddEventCallback<NkGraphicsGpuMemoryEvent>(
    [&](NkGraphicsGpuMemoryEvent* ev) {
        if (ev->IsCritical()) {
            // Libérer les caches de textures, LODs basses priorité…
            textureCache.EvictAll();
        }
    }
);
```

---

## Module 17 — Synchronisation GPU avancée (Vulkan/DX12)

```cpp
// Créer des sémaphores GPU-GPU
NkSemaphoreHandle imageAvailable  = device->CreateGpuSemaphore();
NkSemaphoreHandle renderFinished  = device->CreateGpuSemaphore();
NkFenceHandle     frameFence      = device->CreateFence(/*signaled=*/true);

// Acquérir l'image du swapchain
swapchain->AcquireNextImage(imageAvailable, frameFence);

// Soumettre en attendant imageAvailable, en signalant renderFinished
device->SubmitGraphics(cmd,
    imageAvailable, NkPipelineStage::NK_COLOR_ATTACHMENT,
    renderFinished, frameFence);

// Présenter en attendant renderFinished
swapchain->Present(&renderFinished, 1);

// Attendre la fence avant de réutiliser les ressources de ce frame
device->WaitFence(frameFence);
device->ResetFence(frameFence);
```

---

## Module 18 — Pool de command buffers et multi-threading

```cpp
// Créer un pool par thread de rendu
NkCommandPool cmdPool(device, NkCommandBufferType::NK_GRAPHICS);

// Chaque frame
auto* cmd = cmdPool.Acquire();
cmd->Begin();
// … record …
cmd->End();

device->Submit(&cmd, 1, frameFence);
device->WaitFence(frameFence);
cmdPool.Release(cmd);
// ou à la fin : cmdPool.ReleaseAll()
```

---

## Module 19 — Déboguer le rendu

```cpp
// Nommer les ressources (visible dans RenderDoc / PIX / Xcode)
device->SetDebugName(vbo,      "Mesh_VertexBuffer");
device->SetDebugName(tex,      "Albedo_Diffuse");
device->SetDebugName(pipeline, "Pipeline_PBR");

// Groupes de debug dans le command buffer
cmd->BeginDebugGroup("ShadowPass", 1.f, 0.5f, 0.f);
// … commandes du shadow pass …
cmd->EndDebugGroup();

cmd->BeginDebugGroup("MainPass", 0.f, 1.f, 0.5f);
// … commandes du main pass …
cmd->EndDebugGroup();

cmd->InsertDebugLabel("DrawMesh_Character");

// Stats de la dernière frame
const auto& stats = device->GetLastFrameStats();
// stats.drawCalls, stats.triangles, stats.gpuTimeMs…
```

---

## Module 20 — Exemple complet intégré

Voici le squelette complet d'une application 3D avec shadow map et overlay 2D.

```cpp
int nkmain(const NkEntryState&) {
    NkInitialise({ .appName = "Demo3D" });

    // === Fenêtre ===
    NkWindowConfig cfg; cfg.title="Demo"; cfg.width=1280; cfg.height=720;
    NkWindow window; window.Create(cfg);

    // === Device ===
    NkDeviceInitInfo init;
    init.context = NkContextDesc::MakeOpenGL(4, 6, true);
    init.surface = window.GetSurfaceDesc();
    init.width = 1280; init.height = 720;
    NkIDevice* device = NkDeviceFactory::Create(init);

    // === Resources ===
    // Shaders, buffers, textures, passes, pipelines…
    // (voir modules 5 à 10)

    // === Events ===
    bool running = true;
    NkEvents().AddEventCallback<NkWindowCloseEvent>(
        [&](auto*) { running = false; });
    NkEvents().AddEventCallback<NkGraphicsContextResizeEvent>(
        [&](auto* ev) { device->OnResize(ev->GetWidth(), ev->GetHeight()); });

    // === Boucle ===
    NkFrameContext frame;
    while (running) {
        NkEvents().PollEvents();
        if (!device->BeginFrame(frame)) continue;

        auto* cmd = device->CreateCommandBuffer();
        cmd->Begin();

        // Pass 1 : shadow map
        cmd->BeginRenderPass(shadowPass, shadowFB, {0,0,2048,2048});
        /* … render scene from light … */
        cmd->EndRenderPass();

        // Pass 2 : géométrie 3D → colorRT (offscreen)
        cmd->BeginRenderPass(offscreenPass, offscreenFB, {0,0,1280,720});
        /* … render scene with lighting + shadow … */
        cmd->EndRenderPass();

        // Pass 3 : post-process → swapchain
        cmd->BeginRenderPass(device->GetSwapchainRenderPass(),
                             device->GetSwapchainFramebuffer(), {0,0,1280,720});
        /* … fullscreen tonemap … */

        // Overlay 2D (dans le même pass)
        /* … UI, HUD … */

        cmd->EndRenderPass();
        cmd->End();

        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
        device->DestroyCommandBuffer(cmd);
    }

    // === Cleanup ===
    device->WaitIdle();
    /* détruire toutes les ressources */
    NkDeviceFactory::Destroy(device);
    window.Close();
    NkClose();
    return 0;
}
```

---

## Récapitulatif des concepts clés


| Concept        | Classe / Struct       | Rôle                         |
| -------------- | --------------------- | ----------------------------- |
| Fenêtre       | `NkWindow`            | Crée la surface native       |
| Surface        | `NkSurfaceDesc`       | Pont entre fenêtre et GPU    |
| Device         | `NkIDevice`           | Toutes les opérations GPU    |
| Swapchain      | `NkISwapchain`        | Présentation à l'écran     |
| Command Buffer | `NkICommandBuffer`    | Enregistrement des commandes  |
| Buffer         | `NkBufferHandle`      | VBO, IBO, UBO, SSBO           |
| Texture        | `NkTextureHandle`     | Textures 2D/3D/Cube/RT        |
| Sampler        | `NkSamplerHandle`     | Filtrage des textures         |
| Shader         | `NkShaderHandle`      | Programme GPU compilé        |
| Pipeline       | `NkPipelineHandle`    | État GPU complet verrouillé |
| Render Pass    | `NkRenderPassHandle`  | Description des attachements  |
| Framebuffer    | `NkFramebufferHandle` | Lie le pass aux textures      |
| Descriptor Set | `NkDescSetHandle`     | Binding des ressources        |
| Événements   | `NkEventSystem`       | Clavier, souris, resize, GPU  |

Ce système vous permet de cibler OpenGL, Vulkan, DirectX 11/12, Metal et Software avec le même code applicatif — seul le `NkContextDesc` change.
