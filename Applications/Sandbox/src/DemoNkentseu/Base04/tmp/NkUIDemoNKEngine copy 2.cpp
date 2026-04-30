/**
 * @File    NkUIDemoNKEngine.cpp  — v4 (shaders corrigés par backend)
 * @Brief   Version complète avec NKWindow + NKRHI + NkShaderConvert.
 *
 * CORRECTIONS v2 :
 *   [A] NkUIRHIRenderer::Init() — CreateBuffer utilisait la syntaxe aggregate
 *       {kVBOCap, NkBufferType::NK_VERTEX, ...} qui mappe les champs dans le
 *       MAUVAIS ordre → buffer de taille ~1-4 octets → GL_INVALID_VALUE 1281
 *       sur tout WriteBuffer(). Remplacé par NkBufferDesc::Vertex() /
 *       NkBufferDesc::Index() exactement comme dans NkRHIDemoFull.cpp.
 *
 *   [B] NkUIRHIRenderer::Submit() — WriteBuffer sans vérification de capacité.
 *       Ajout d'un guard : si vsz > kVBOCap on skip (évite overflow).
 *       Les caps ont aussi été augmentées (4 Mo) pour absorber l'ensemble
 *       des DrawLists NkUI (fond + fenêtres + popups + overlay).
 *
 *   [C] NkUIRHIRenderer::Init() — hUBO créé avec NkBufferDesc::Uniform(16)
 *       mais le viewport est un vec2 (8 bytes). Harmonisé à 16 bytes (vec4
 *       avec padding) pour respecter l'alignement std140.
 *
 *   [D] NkUIVertex layout — le champ color est uint32 (RGBA packed).
 *       Le format GPU doit être NK_R32_UINT, pas NK_RGBA8_UNORM,
 *       car le shader décompresse manuellement. Confirmé inchangé : OK.
 *
 *   AJOUTS v3 :
 *   [F] NkShaderConverter intégré : pour le backend Vulkan, les shaders GLSL
 *       sont compilés en SPIRV via NkShaderConverter::GlslToSpirv() avec cache
 *       NkShaderCache (répertoire Build/ShaderCache, format .nksc).
 *       Si le SPIRV est déjà en cache il est chargé directement (0 recompilation).
 *
 *   [G] NkUIDrawList optimisations actives (depuis v3 NkUIDrawList.cpp) :
 *       - Clip rect culling : primitives hors clipRect ignorées
 *       - Alpha threshold  : primitives avec alpha ≤ 1 ignorées
 *       - Occlusion        : rects opaques empilés (max 64) — draw derrière ignoré
 *       Aucune modification requise côté rendu GPU, tout est géré dans DrawList.
 *
 * Usage :
 *   arg: --backend=opengl|vulkan|dx11|dx12|sw|metal
 */

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/ShaderConvert/NkShaderConvert.h"

#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

#include "NKUI/NkUI.h"
#include "NKUI/NkUILayout2.h"
#include "NkUIDemo.h"

#include <memory>   // std::make_unique

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// Shaders NkUI — variantes par backend
// =============================================================================

// ── OpenGL / Software : GLSL avec UBO au binding point 1 ─────────────────
// OpenGL lie le buffer via glBindBufferRange(GL_UNIFORM_BUFFER, binding, ...)
// → il faut un uniform block layout(binding=N) dans le shader.
// L'ancien "uniform vec2 uViewport" était un uniform isolé jamais alimenté
// par le UBO → uViewport=(0,0) → division par zéro → NaN → rien affiché.
static constexpr const char* kNkUI_Vert_GLSL = R"GLSL(
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(std140, binding=1) uniform ViewportBlock {
    vec2 uViewport;
    vec2 _pad;
};
out vec2  vUV;
out vec4  vColor;
void main() {
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    vec2 ndc = (aPos / uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

static constexpr const char* kNkUI_Frag_GLSL = R"GLSL(
#version 460 core
in  vec2 vUV;
in  vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D uTex;
void main() {
    vec4 tc = (vUV.x > 0.0 || vUV.y > 0.0)
            ? texture(uTex, vUV)
            : vec4(1.0);
    fragColor = vColor * tc;
}
)GLSL";

// ── Vulkan GLSL compatible SPIR-V ────────────────────────────────────────
// Différences vs OpenGL GLSL :
//   1. Uniform hors block interdit → UBO avec set/binding explicites
//   2. in/out doivent avoir layout(location=N) explicites
//   3. sampler2D doit avoir set= explicite
static constexpr const char* kNkUI_Vert_GLSL_VK = R"GLSL(
#version 460
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(set=0, binding=1, std140) uniform ViewportBlock {
    vec2 uViewport;
    vec2 _pad;
} vpb;
layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
void main() {
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    vec2 ndc = (aPos / vpb.uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

static constexpr const char* kNkUI_Frag_GLSL_VK = R"GLSL(
#version 460
layout(location=0) in  vec2 vUV;
layout(location=1) in  vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0, binding=0) uniform sampler2D uTex;
void main() {
    vec4 tc = (vUV.x > 0.0 || vUV.y > 0.0)
            ? texture(uTex, vUV)
            : vec4(1.0);
    vec4 color = vColor * tc;
    // color.rgb = pow(color.rgb, vec3(2.2));  // linéaire → sRGB
    color.rgb = pow(color.rgb, vec3(2.2));
    fragColor = color;
}
)GLSL";

// ── HLSL DX11 ────────────────────────────────────────────────────────────
// DX11 : descriptor binding N → VSSetConstantBuffers(N) → register(bN)
//   binding=1 (UBO) → b1  |  binding=0 (texture) → t0  |  sampler → s0
static constexpr const char* kNkUI_Vert_HLSL_DX11 = R"HLSL(
cbuffer Constants : register(b1) { float2 uViewport; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; uint col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 c;
    c.r = float((v.col >>  0u) & 0xFFu) / 255.0;
    c.g = float((v.col >>  8u) & 0xFFu) / 255.0;
    c.b = float((v.col >> 16u) & 0xFFu) / 255.0;
    c.a = float((v.col >> 24u) & 0xFFu) / 255.0;
    o.col = c;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
)HLSL";

static constexpr const char* kNkUI_Frag_HLSL_DX11 = R"HLSL(
Texture2D    uTex     : register(t0);
SamplerState uSampler : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tc = (i.uv.x > 0 || i.uv.y > 0)
              ? uTex.Sample(uSampler, i.uv) : float4(1,1,1,1);
    return i.col * tc;
}
)HLSL";

// ── HLSL DX12 ────────────────────────────────────────────────────────────
// Root signature (CreateDefaultRootSignature) :
//   params[1] = Root CBV à b0  → cbuffer : register(b0)
//   params[2] = SRV table depuis t1 → register(t1) dans le shader
//   params[4] = Sampler table depuis s1 → register(s1) dans le shader
// L'ancien shader utilisait t0/s0 non couverts → E_INVALIDARG pipeline creation.
static constexpr const char* kNkUI_Vert_HLSL_DX12 = R"HLSL(
cbuffer Constants : register(b0) { float2 uViewport; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; uint col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 c;
    c.r = float((v.col >>  0u) & 0xFFu) / 255.0;
    c.g = float((v.col >>  8u) & 0xFFu) / 255.0;
    c.b = float((v.col >> 16u) & 0xFFu) / 255.0;
    c.a = float((v.col >> 24u) & 0xFFu) / 255.0;
    o.col = c;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
)HLSL";

static constexpr const char* kNkUI_Frag_HLSL_DX12 = R"HLSL(
Texture2D    uTex     : register(t1);
SamplerState uSampler : register(s1);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tc = (i.uv.x > 0 || i.uv.y > 0)
              ? uTex.Sample(uSampler, i.uv) : float4(1,1,1,1);
    return i.col * tc;
}
)HLSL";

// =============================================================================
// NkUIRHIRenderer — rendu NkUI via NkIDevice
// =============================================================================
struct NkUIRHIRenderer {
    NkIDevice*       device   = nullptr;
    NkShaderHandle   hShader;
    NkPipelineHandle hPipe;
    NkBufferHandle   hVBO, hIBO;
    NkBufferHandle   hUBO;
    NkTextureHandle  hWhiteTex;
    NkSamplerHandle  hSampler;
    NkDescSetHandle  hLayout, hDescSet;
    NkRenderPassHandle hRP;

    // ── Helpers ShaderConvert ────────────────────────────────────────────
    // Compile GLSL→SPIRV pour Vulkan, avec mise en cache .nksc.
    // Retourne le résultat (result.success == false si échec).
    static NkShaderConvertResult CompileGlslToSpirv(const char* src, NkSLStage stage, const char* dbgName) {
        NkShaderCache& cache = NkShaderCache::Global();
        
        const uint64 key = NkShaderCache::ComputeKey(NkString(src), stage, "spirv");
        
        NkShaderConvertResult cached = cache.Load(key);
        if (cached.success) {
            // VALIDATION : vérifier que c'est bien du SPIR-V valide
            if (cached.SpirvWordCount() > 0) {
                const uint32* words = cached.SpirvWords();
                if (words[0] == 0x07230203) {  // SPIR-V magic number
                    logger.Info("[NkUIShader] Cache hit — {0} (valid SPIR-V)\n", dbgName);
                    return cached;
                } else {
                    logger.Info("[NkUIShader] Cache corrupted — invalid SPIR-V magic 0x{0:08x}\n", words[0]);
                    // Invalider le cache corrompu
                    cache.Invalidate(key);
                    // Fall through to recompile
                }
            }
        }
        
        logger.Info("[NkUIShader] Cache miss — {0}, compiling GLSL->SPIRV...\n", dbgName);
        NkShaderConvertResult res = NkShaderConverter::GlslToSpirv(NkString(src), stage, dbgName);
        
        if (!res.success) {
            logger.Info("[NkUIShader] Compilation failed: {0}\n", res.errors);
            return res;
        }
        
        // Vérifier le SPIR-V généré
        if (res.SpirvWordCount() == 0 || res.SpirvWords()[0] != 0x07230203) {
            logger.Info("[NkUIShader] Generated SPIR-V invalid (magic: 0x{0:08x})\n", 
                        res.SpirvWordCount() > 0 ? res.SpirvWords()[0] : 0);
            res.success = false;
            return res;
        }

        if (res.binary.Size() >= 16) {
            const uint8* bytes = res.binary.Data();
            logger.Infof("First 16 bytes: %02x %02x %02x %02x %02x %02x %02x %02x ...\n",
                        bytes[0], bytes[1], bytes[2], bytes[3],
                        bytes[4], bytes[5], bytes[6], bytes[7]);
        }
        
        cache.Save(key, res);
        logger.Info("[NkUIShader] Compiled OK — {0} ({1} words SPIR-V)\n", 
                    dbgName, (unsigned long long)res.SpirvWordCount());
        logger.Info("[NkUIShader] Compiled — {0}, first word = 0x{1:08x}, size = {2} bytes\n",
            dbgName, 
            (unsigned long long)res.SpirvWords()[0],
            (unsigned long long)res.binary.Size());
        return res;
    }

    // [FIX B] Capacités augmentées : 4 Mo chacun
    // NkUI avec toutes les fenêtres peut générer beaucoup de géométrie.
    // sizeof(NkUIVertex) = 2*4 + 2*4 + 4 = 20 bytes
    // 4 Mo / 20 = ~209 000 vertices max par layer, largement suffisant.
    static constexpr uint64 kVBOCap = 12 * 1024 * 1024;
    static constexpr uint64 kIBOCap = 8 * 1024 * 1024;

    bool Init(NkIDevice* dev, NkRenderPassHandle rp, NkGraphicsApi api) {
        device = dev;
        hRP    = rp;

        // Shader — registres différents par backend (voir commentaires des constantes)
        NkShaderDesc sd;
        sd.debugName = "NkUI";
        if (api == NkGraphicsApi::NK_GFX_API_OPENGL ||
            api == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
            // GLSL avec UBO au binding=1 pour uViewport
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_GLSL);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_GLSL);
        } else if (api == NkGraphicsApi::NK_GFX_API_D3D11) {
            // DX11 : cbuffer b1 (binding=1), texture t0/s0 (binding=0)
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_HLSL_DX11, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_HLSL_DX11, "PSMain");
        } else if (api == NkGraphicsApi::NK_GFX_API_D3D12) {
            // DX12 : Root CBV b0, SRV table t1+, Sampler table s1+
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_HLSL_DX12, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_HLSL_DX12, "PSMain");
        } else 
        if (api == NkGraphicsApi::NK_GFX_API_VULKAN) {
            NkShaderConvertResult vertSpv = CompileGlslToSpirv(
                kNkUI_Vert_GLSL_VK, NkSLStage::NK_VERTEX, "NkUI.vert");
            NkShaderConvertResult fragSpv = CompileGlslToSpirv(
                kNkUI_Frag_GLSL_VK, NkSLStage::NK_FRAGMENT, "NkUI.frag");

            if (!vertSpv.success || !fragSpv.success) {
                logger.Info("[NkUIRenderer] Vulkan SPIRV compilation failed\n");
                return false;
            }
            
            // Créer des copies alignées
            NkVector<uint32> vertWords;
            vertWords.Resize(vertSpv.SpirvWordCount());
            memcpy(vertWords.Data(), vertSpv.binary.Data(), vertSpv.binary.Size());
            
            NkVector<uint32> fragWords;
            fragWords.Resize(fragSpv.SpirvWordCount());
            memcpy(fragWords.Data(), fragSpv.binary.Data(), fragSpv.binary.Size());
            
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        vertWords.Data(), 
                        vertWords.Size() * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        fragWords.Data(),
                        fragWords.Size() * sizeof(uint32));
        }
        // Metal : MSL à ajouter si nécessaire (stub pour l'instant)
        hShader = device->CreateShader(sd);
        if (!hShader.IsValid()) {
            logger.Info("[NkUIRenderer] Shader compile failed\n");
            return false;
        }

        // Vertex layout — NkUIVertex : pos(vec2) + uv(vec2) + color(uint32)
        // Taille totale : 8 + 8 + 4 = 20 bytes
        NkVertexLayout vtxLayout;
        vtxLayout
            .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,  0,  "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,  8,  "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_R32_UINT,    16, "COLOR",    0)
            .AddBinding(0, sizeof(nkui::NkUIVertex));

        // Descriptor layout : binding 0 = sampler2D (fragment),
        //                     binding 1 = UBO viewport (vertex)
        NkDescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                        NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,
                        NkShaderStage::NK_VERTEX);
        hLayout = device->CreateDescriptorSetLayout(layoutDesc);

        // Pipeline : alpha blending, no depth, no cull, scissor test
        NkGraphicsPipelineDesc pd;
        pd.shader         = hShader;
        pd.vertexLayout   = vtxLayout;
        pd.topology       = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer     = NkRasterizerDesc::NoCull();
        pd.rasterizer.scissorTest = true;
        pd.depthStencil   = NkDepthStencilDesc::NoDepth();
        pd.blend          = NkBlendDesc::Alpha();
        pd.renderPass     = hRP;
        pd.debugName      = "NkUIPipeline";
        if (hLayout.IsValid())
            pd.descriptorSetLayouts.PushBack(hLayout);
        hPipe = device->CreateGraphicsPipeline(pd);
        if (!hPipe.IsValid()) {
            logger.Info("[NkUIRenderer] Pipeline creation failed\n");
            return false;
        }

        // [FIX A] Création des buffers GPU avec NkBufferDesc::Vertex() / ::Index()
        // L'ancienne syntaxe aggregate {kVBOCap, NkBufferType::NK_VERTEX, ...}
        // mappait les champs dans le mauvais ordre → buffer de ~1-4 octets.
        hVBO = device->CreateBuffer(NkBufferDesc::VertexDynamic(kVBOCap));
        hIBO = device->CreateBuffer(NkBufferDesc::IndexDynamic(kIBOCap));

        if (!hVBO.IsValid()) { logger.Info("[NkUIRenderer] VBO creation failed\n"); return false; }
        if (!hIBO.IsValid()) { logger.Info("[NkUIRenderer] IBO creation failed\n"); return false; }

        // [FIX C] UBO viewport : vec2 + padding pour alignement std140 = 16 bytes
        hUBO = device->CreateBuffer(NkBufferDesc::Uniform(16));
        if (!hUBO.IsValid()) { logger.Info("[NkUIRenderer] UBO creation failed\n"); return false; }

        // Texture blanche 1×1 (fallback quand UV = {0,0})
        NkTextureDesc tdesc = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RGBA8_UNORM, 1);
        tdesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        tdesc.debugName = "NkUI_WhiteTex";
        hWhiteTex = device->CreateTexture(tdesc);
        if (hWhiteTex.IsValid()) {
            static const uint8 white[4] = {255, 255, 255, 255};
            device->WriteTexture(hWhiteTex, white, 4);
        }

        // Sampler bilinéaire clamp
        NkSamplerDesc samplerDesc{};
        samplerDesc.magFilter  = NkFilter::NK_LINEAR;
        samplerDesc.minFilter  = NkFilter::NK_LINEAR;
        samplerDesc.mipFilter  = NkMipFilter::NK_NONE;
        samplerDesc.addressU   = NkAddressMode::NK_CLAMP_TO_EDGE;
        samplerDesc.addressV   = NkAddressMode::NK_CLAMP_TO_EDGE;
        samplerDesc.addressW   = NkAddressMode::NK_CLAMP_TO_EDGE;
        hSampler = device->CreateSampler(samplerDesc);

        // Descriptor set
        hDescSet = device->AllocateDescriptorSet(hLayout);
        if (hDescSet.IsValid() && hWhiteTex.IsValid() &&
            hSampler.IsValid() && hUBO.IsValid())
        {
            NkDescriptorWrite writes[2] = {};
            writes[0].set           = hDescSet;
            writes[0].binding       = 0;
            writes[0].type          = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[0].texture       = hWhiteTex;
            writes[0].sampler       = hSampler;
            writes[0].textureLayout = NkResourceState::NK_SHADER_READ;
            writes[1].set           = hDescSet;
            writes[1].binding       = 1;
            writes[1].type          = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[1].buffer        = hUBO;
            writes[1].bufferOffset  = 0;
            writes[1].bufferRange   = 16;
            device->UpdateDescriptorSets(writes, 2);
        }

        logger.Info("[NkUIRenderer] Initialized OK — VBO=%llu Ko, IBO=%llu Ko\n",
                    (unsigned long long)(kVBOCap / 1024),
                    (unsigned long long)(kIBOCap / 1024));
        return true;
    }

    // Upload texture NkUI (pour les atlas de polices etc.)
    void UploadTexture(NkDescSetHandle descSet, NkTextureHandle tex, NkSamplerHandle smp) {
        if (!descSet.IsValid() || !tex.IsValid() || !smp.IsValid()) return;
        NkDescriptorWrite w{};
        w.set           = descSet;
        w.binding       = 0;
        w.type          = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
        w.texture       = tex;
        w.sampler       = smp;
        w.textureLayout = NkResourceState::NK_SHADER_READ;
        device->UpdateDescriptorSets(&w, 1);
    }
    
    // Soumet une NkUIContext complète → draw calls GPU
    void Submit(NkICommandBuffer* cmd, const nkui::NkUIContext& ctx, uint32 fbW, uint32 fbH)
    {
        if (!cmd || !hPipe.IsValid() || fbW == 0 || fbH == 0) return;

        // Met à jour le viewport uniform (vec2 + 8 bytes padding)
        const float32 vp[4] = {(float32)fbW, (float32)fbH, 0.f, 0.f};
        device->WriteBuffer(hUBO, vp, 16);

        cmd->BindGraphicsPipeline(hPipe);
        if (hDescSet.IsValid()) cmd->BindDescriptorSet(hDescSet, 0);

        NkViewport viewport{0.f, 0.f, (float32)fbW, (float32)fbH, 0.f, 1.f};
        cmd->SetViewport(viewport);

        // Itère sur les 4 layers dans l'ordre (BG → Windows → Popups → Overlay)
        for (int32 l = 0; l < nkui::NkUIContext::LAYER_COUNT; ++l) {
            const nkui::NkUIDrawList& dl = ctx.layers[l];
            if (!dl.vtx || !dl.idx || dl.cmdCount == 0 || dl.vtxCount == 0 || dl.idxCount == 0) continue;

            const uint64 vsz = (uint64)dl.vtxCount * sizeof(nkui::NkUIVertex);
            const uint64 isz = (uint64)dl.idxCount * sizeof(uint32);

            // [FIX B] Guard : skip si trop grand pour le buffer
            if (vsz > kVBOCap) {
                logger.Info("[NkUIRenderer] Layer {0}: VBO overflow ({1} > {2}), skip\n", l, (unsigned long long)vsz, (unsigned long long)kVBOCap);
                continue;
            }
            if (isz > kIBOCap) {
                logger.Info("[NkUIRenderer] Layer {0}: IBO overflow ({1} > {2}), skip\n", l, (unsigned long long)isz, (unsigned long long)kIBOCap);
                continue;
            }

            device->WriteBuffer(hVBO, dl.vtx, vsz);
            device->WriteBuffer(hIBO, dl.idx,  isz);

            cmd->BindVertexBuffer(0, hVBO);
            cmd->BindIndexBuffer(hIBO, NkIndexFormat::NK_UINT32);

            // Scissor par défaut = plein écran
            NkRect2D currentScissor{0, 0, (int32)fbW, (int32)fbH};
            cmd->SetScissor(currentScissor);

            for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
                const nkui::NkUIDrawCmd& dc = dl.cmds[ci];

                if (dc.type == nkui::NkUIDrawCmdType::NK_CLIP_RECT) {
                    // Met à jour le scissor rect
                    currentScissor = {
                        (int32)::fmaxf(dc.clipRect.x, 0.f),
                        (int32)::fmaxf(dc.clipRect.y, 0.f),
                        (int32)::fmaxf(::fminf(dc.clipRect.w, (float32)fbW), 0.f),
                        (int32)::fmaxf(::fminf(dc.clipRect.h, (float32)fbH), 0.f)
                    };
                    cmd->SetScissor(currentScissor);
                    continue;
                }

                if (dc.idxCount == 0) continue;

                // Texture custom si la commande en a une (atlas de police)
                if (dc.texId != 0) {
                    // TODO: bind la texture dc.texId si le backend le supporte
                    // Pour l'instant on utilise toujours la texture blanche
                }

                cmd->SetScissor(currentScissor);
                cmd->DrawIndexed(dc.idxCount, 1, dc.idxOffset, 0, 0);
            }
        }
    }

    void Destroy() {
        if (!device) return;
        if (hDescSet.IsValid())  device->FreeDescriptorSet(hDescSet);
        if (hLayout.IsValid())   device->DestroyDescriptorSetLayout(hLayout);
        if (hPipe.IsValid())     device->DestroyPipeline(hPipe);
        if (hShader.IsValid())   device->DestroyShader(hShader);
        if (hVBO.IsValid())      device->DestroyBuffer(hVBO);
        if (hIBO.IsValid())      device->DestroyBuffer(hIBO);
        if (hUBO.IsValid())      device->DestroyBuffer(hUBO);
        if (hSampler.IsValid())  device->DestroySampler(hSampler);
        if (hWhiteTex.IsValid()) device->DestroyTexture(hWhiteTex);
        device = nullptr;
    }
};

// =============================================================================
// Helpers
// =============================================================================

// Convertit NkWindow::NkKey → nkui::NkKey (les deux enums ont des valeurs différentes)
// static nkui::NkKey MapWindowKeyToUIKey(NkKey wk) {
//     using WK = NkKey;
//     using UK = nkui::NkKey;
//     switch (wk) {
//         case WK::NK_TAB:        return UK::NK_TAB;
//         case WK::NK_ENTER:      return UK::NK_ENTER;
//         case WK::NK_ESCAPE:     return UK::NK_ESCAPE;
//         case WK::NK_BACK:       return UK::NK_BACKSPACE;
//         case WK::NK_DELETE:     return UK::NK_DELETE;
//         case WK::NK_SPACE:      return UK::NK_SPACE;
//         case WK::NK_LEFT:       return UK::NK_LEFT;
//         case WK::NK_RIGHT:      return UK::NK_RIGHT;
//         case WK::NK_UP:         return UK::NK_UP;
//         case WK::NK_DOWN:       return UK::NK_DOWN;
//         case WK::NK_HOME:       return UK::NK_HOME;
//         case WK::NK_END:        return UK::NK_END;
//         case WK::NK_PAGE_UP:    return UK::NK_PAGE_UP;
//         case WK::NK_PAGE_DOWN:  return UK::NK_PAGE_DOWN;
//         case WK::NK_INSERT:     return UK::NK_INSERT;
//         case WK::NK_A:          return UK::NK_A;
//         case WK::NK_B:          return UK::NK_B;
//         case WK::NK_C:          return UK::NK_C;
//         case WK::NK_D:          return UK::NK_D;
//         case WK::NK_E:          return UK::NK_E;
//         case WK::NK_F:          return UK::NK_F;
//         case WK::NK_G:          return UK::NK_G;
//         case WK::NK_H:          return UK::NK_H;
//         case WK::NK_I:          return UK::NK_I;
//         case WK::NK_J:          return UK::NK_J;
//         case WK::NK_K:          return UK::NK_K;
//         case WK::NK_L:          return UK::NK_L;
//         case WK::NK_M:          return UK::NK_M;
//         case WK::NK_N:          return UK::NK_N;
//         case WK::NK_O:          return UK::NK_O;
//         case WK::NK_P:          return UK::NK_P;
//         case WK::NK_Q:          return UK::NK_Q;
//         case WK::NK_R:          return UK::NK_R;
//         case WK::NK_S:          return UK::NK_S;
//         case WK::NK_T:          return UK::NK_T;
//         case WK::NK_U:          return UK::NK_U;
//         case WK::NK_V:          return UK::NK_V;
//         case WK::NK_W:          return UK::NK_W;
//         case WK::NK_X:          return UK::NK_X;
//         case WK::NK_Y:          return UK::NK_Y;
//         case WK::NK_Z:          return UK::NK_Z;
//         case WK::NK_F1:         return UK::NK_F1;
//         case WK::NK_F2:         return UK::NK_F2;
//         case WK::NK_F3:         return UK::NK_F3;
//         case WK::NK_F4:         return UK::NK_F4;
//         case WK::NK_F5:         return UK::NK_F5;
//         case WK::NK_F6:         return UK::NK_F6;
//         case WK::NK_F7:         return UK::NK_F7;
//         case WK::NK_F8:         return UK::NK_F8;
//         case WK::NK_F9:         return UK::NK_F9;
//         case WK::NK_F10:        return UK::NK_F10;
//         case WK::NK_F11:        return UK::NK_F11;
//         case WK::NK_F12:        return UK::NK_F12;
//         case WK::NK_LCTRL:
//         case WK::NK_RCTRL:      return UK::NK_CTRL;
//         case WK::NK_LSHIFT:
//         case WK::NK_RSHIFT:     return UK::NK_SHIFT;
//         case WK::NK_LALT:
//         case WK::NK_RALT:       return UK::NK_ALT;
//         case WK::NK_LSUPER:
//         case WK::NK_RSUPER:     return UK::NK_SUPER;
//         default:                return UK::NK_NONE;
//     }
// }

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i = 1; i < args.Size(); i++) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")  return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11") return NkGraphicsApi::NK_GFX_API_D3D11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12") return NkGraphicsApi::NK_GFX_API_D3D12;
        if (args[i] == "--backend=metal"   || args[i] == "-bmtl")  return NkGraphicsApi::NK_GFX_API_METAL;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")   return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}

// =============================================================================
// nkmain
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {
    logger.Info("Begin");
    const NkGraphicsApi api = ParseBackend(state.GetArgs());
    logger.Info("[NkUIDemoEngine] Backend: {0}\n", NkGraphicsApiName(api));

    // ── ShaderCache ───────────────────────────────────────────────────────
    // Initialisation du cache global NkShaderCache.
    // Les shaders compilés (SPIRV, GLSL transcodés) sont stockés dans
    // Build/ShaderCache/*.nksc — format binaire FNV-1a 64-bit.
    // Bénéfice : au 2e lancement, aucune recompilation GLSL→SPIRV.
    {
        NkShaderCache& cache = NkShaderCache::Global();
        cache.SetCacheDir("Build/ShaderCache");
        logger.Info("[NkUIDemoEngine] ShaderCache dir: Build/ShaderCache\n");
    }

    // ── Fenêtre ───────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NkUI Demo — {0}", NkGraphicsApiName(api));
    winCfg.width     = 1600;
    winCfg.height    = 900;
    winCfg.centered  = true;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Info("[NkUIDemoEngine] Window creation failed\n");
        return 1;
    }

    // ── Device RHI ────────────────────────────────────────────────────────
    NkDeviceInitInfo deviceInfo;
    deviceInfo.api     = api;
    deviceInfo.surface = window.GetSurfaceDesc();
    deviceInfo.width   = window.GetSize().width;
    deviceInfo.height  = window.GetSize().height;
    deviceInfo.context.vulkan.appName    = "NkUIDemoEngine";
    deviceInfo.context.vulkan.engineName = "Unkeny";

    NkIDevice* device = NkDeviceFactory::Create(deviceInfo);
    if (!device || !device->IsValid()) {
        logger.Info("[NkUIDemoEngine] NkIDevice creation failed ({0})\n",
                    NkGraphicsApiName(api));
        window.Close();
        return 1;
    }
    logger.Info("[NkUIDemoEngine] Device OK. VRAM: {0} Mo\n",
                (unsigned long long)(device->GetCaps().vramBytes >> 20));

    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── NkUI init ─────────────────────────────────────────────────────────
    nkui::NkUIContext       ctx;
    nkui::NkUIFontManager   fonts;
    nkui::NkUIWindowManager wm;
    nkui::NkUIDockManager   dock;
    nkui::NkUILayoutStack   ls;

    ctx.Init((int32)W, (int32)H);
    ctx.SetTheme(nkui::NkUITheme::Dark());

    // fonts.Init() DOIT être appelé avant demo->Init()
    // sinon fonts.Default() retourne nullptr → crash dans RenderText
    fonts.Init();

    wm.Init();
    dock.Init({0.f, ctx.theme.metrics.titleBarHeight,
               (float32)W,
               (float32)H - ctx.theme.metrics.titleBarHeight});

    // [FIX STACK OVERFLOW] NkUIDemo alloué sur le heap — ~1 Mo sur la stack
    // sinon provoquait un stack overflow silencieux avant le 1er logger.Info
    logger.Info("[NkUIDemoEngine] Allocating NkUIDemo on heap...\n");
    auto demo = std::make_unique<nkui::NkUIDemo>();
    logger.Info("[NkUIDemoEngine] NkUIDemo allocated.\n");
    demo->Init(ctx, wm, fonts);
    logger.Info("[NkUIDemoEngine] NkUIDemo initialized.\n");

    // ── NkUI GPU Renderer ─────────────────────────────────────────────────
    NkUIRHIRenderer uiRenderer;
    if (!uiRenderer.Init(device, device->GetSwapchainRenderPass(), api)) {
        logger.Info("[NkUIDemoEngine] NkUIRHIRenderer init failed\n");
        demo->Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── Command buffer ────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd || !cmd->IsValid()) {
        logger.Info("[NkUIDemoEngine] Command buffer creation failed\n");
        demo->Destroy();
        uiRenderer.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── État ──────────────────────────────────────────────────────────────
    bool running = true;
    nkui::NkUIInputState input;
    NkClock clock;
    NkEventSystem& events = NkEvents();

    // Callbacks événements
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (uint32)e->GetWidth();
        H = (uint32)e->GetHeight();
        ctx.viewW = (int32)W;
        ctx.viewH = (int32)H;
    });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        input.SetMousePos((float32)e->GetX(), (float32)e->GetY());
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        // NK_MB_UNKNOWN=0, NK_MB_LEFT=1, NK_MB_RIGHT=2, NK_MB_MIDDLE=3
        // NkUI attend 0=gauche, 1=droit, 2=milieu → soustraire 1
        const int32 btn = (int32)e->GetButton() - 1;
        if (btn >= 0 && btn < 5) input.SetMouseButton(btn, true);
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        const int32 btn = (int32)e->GetButton() - 1;
        if (btn >= 0 && btn < 5) input.SetMouseButton(btn, false);
    });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        input.AddMouseWheel((float32)e->GetOffsetY(), (float32)e->GetOffsetX());
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        // nkui::NkKey uiKey = MapWindowKeyToUIKey(e->GetKey());
        // if (uiKey != nkui::NkKey::NK_NONE) input.SetKey(uiKey, true);
        // if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
        nkui::NkKey uiKey = e->GetKey();
        if (uiKey != nkui::NkKey::NK_UNKNOWN) input.SetKey(uiKey, true);
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        // nkui::NkKey uiKey = MapWindowKeyToUIKey(e->GetKey());
        // if (uiKey != nkui::NkKey::NK_NONE) input.SetKey(uiKey, false);
        nkui::NkKey uiKey = e->GetKey();
        if (uiKey != nkui::NkKey::NK_UNKNOWN) input.SetKey(uiKey, false);
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        const char* utf8 = e->GetUtf8();
        for (int32 i = 0; utf8[i] && i < 32; ++i)
            input.AddInputChar((uint32)(uint8)utf8[i]);
    });

    logger.Info("[NkUIDemoEngine] Loop started. ESC=exit.\n");
    logger.Info("[NkUIDemoEngine] DrawList optimisations actives:\n");
    logger.Info("  - Clip rect culling  (skip hors clipRect)\n");
    logger.Info("  - Alpha threshold    (skip si alpha <= 1)\n");
    logger.Info("  - Occlusion tracking (max 64 rects opaques / DrawList)\n");

    // Compteur pour afficher les stats DrawList toutes les 300 frames
    uint32 frameCount = 0;

    // ==========================================================================
    // Boucle principale
    // ==========================================================================
    while (running) {
        // DOIT être appelé en premier : remet mouseClicked/mouseReleased/keyPressed
        // à zéro pour cette frame. Sans ça, mouseClicked reste true après le 1er clic.
        input.BeginFrame();

        events.PollEvents();
        if (!running) break;

        if (W == 0 || H == 0) continue;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight())
            device->OnResize(W, H);

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        input.dt = dt;

        // ── Frame NkUI ────────────────────────────────────────────────────
        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);

        demo->Render(ctx, wm, dock, ls, dt);

        ctx.EndFrame();
        wm.EndFrame(ctx);

        // Stats DrawList toutes les 300 frames (≈5s à 60fps)
        ++frameCount;
        if (frameCount % 300 == 0) {
            uint32 totalVtx = 0, totalIdx = 0, totalCmd = 0;
            for (int32 l = 0; l < nkui::NkUIContext::LAYER_COUNT; ++l) {
                const nkui::NkUIDrawList& dl = ctx.layers[l];
                totalVtx += dl.vtxCount;
                totalIdx += dl.idxCount;
                totalCmd += dl.cmdCount;
            }
            logger.Info("[NkUIStats] frame={0} vtx={1} idx={2} cmds={3}\n",
                        (unsigned long long)frameCount,
                        (unsigned long long)totalVtx,
                        (unsigned long long)totalIdx,
                        (unsigned long long)totalCmd);
        }

        // Remet les états transitoires à zéro APRÈS que NkUI a consommé l'input
        input.BeginFrame();

        // ── Rendu GPU ─────────────────────────────────────────────────────
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;

        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) { device->EndFrame(frame); continue; }

        const NkRenderPassHandle  rp  = device->GetSwapchainRenderPass();
        const NkFramebufferHandle fbo = device->GetSwapchainFramebuffer();

        cmd->Reset();
        cmd->Begin();

        const NkRect2D area{0, 0, (int32)W, (int32)H};
        if (cmd->BeginRenderPass(rp, fbo, area)) {
            NkViewport vp{0.f, 0.f, (float32)W, (float32)H, 0.f, 1.f};
            cmd->SetViewport(vp);
            cmd->SetScissor(area);

            uiRenderer.Submit(cmd, ctx, W, H);

            cmd->EndRenderPass();
        }

        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────
    device->WaitIdle();

    demo->Destroy();
    uiRenderer.Destroy();
    ctx.Destroy();
    fonts.Destroy();
    wm.Destroy();
    dock.Destroy();

    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[NkUIDemoEngine] Clean exit.\n");
    return 0;
}