/**
 * @File    NkUIDemoNKEngine.cpp
 * @Brief   Version complète avec NKWindow + NKRHI (comme NkRHIDemoFull.cpp).
 *          Démo NkUI intégrée au moteur NKEngine.
 *
 * Usage :
 *   Remplace nkmain dans votre exécutable de démo.
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

// NkUI
#include "NkUI/NkUI.h"
#include "NkUI/NkUILayout2.h"

// La démo (header-only)
#include "NkUIDemo.h"

#include "NKMemory/NkUniquePtr.h"

// SPIR-V pour NkUI (si Vulkan — généré depuis les shaders ci-dessous)
// #include "NkUIDemoVkSpv.inl"

using namespace nkentseu;
using namespace nkentseu::math;
using namespace nkentseu::nkui;



// =============================================================================
// Shaders NkUI — variantes par backend
// =============================================================================

// ── OpenGL / Software : GLSL avec UBO au binding point 1 ─────────────────
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
    vec2 uv = vUV;
    vec4 tc = texture(uTex, uv);
    fragColor = vColor * tc;
}
)GLSL";

// ── Vulkan GLSL compatible SPIR-V ────────────────────────────────────────
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
    vec2 uv = vUV;
    uv.y = 1.0 - uv.y;
    vec4 tc = texture(uTex, uv);
    vec4 color = vColor * tc;
    color.rgb = pow(color.rgb, vec3(2.2));
    fragColor = color;
}
)GLSL";

// ── HLSL DX11 ────────────────────────────────────────────────────────────
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
    float4 tc = uTex.Sample(uSampler, i.uv);
    return i.col * tc;
}
)HLSL";

// ── HLSL DX12 ────────────────────────────────────────────────────────────
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
    float4 tc = uTex.Sample(uSampler, i.uv);
    return i.col * tc;
}
)HLSL";

// ─────────────────────────────────────────────────────────────────────────────
// NkUIRHIRenderer — rendu NkUI via NkIDevice
// ─────────────────────────────────────────────────────────────────────────────
struct NkUIRHIRenderer {
    NkIDevice*      device        = nullptr;
    NkShaderHandle  hShader;
    NkPipelineHandle hPipe;
    NkBufferHandle  hVBO, hIBO;
    NkBufferHandle  hUBO;             // viewport uniform
    NkTextureHandle hWhiteTex;        // texture 1x1 blanche (fallback)
    NkSamplerHandle hSampler;
    NkDescSetHandle hLayout, hDescSet;
    NkRenderPassHandle hRP;

    // Taille des buffers GPU (dynamique)
    static constexpr uint64 kVBOCap = 4 * 1024 * 1024;  // 1 Mo
    static constexpr uint64 kIBOCap = 4 * 1024 * 1024;

    // Compilation GLSL→SPIRV pour Vulkan
    static NkShaderConvertResult CompileGlslToSpirv(const char* src, NkSLStage stage, const char* dbgName) {
        NkShaderCache& cache = NkShaderCache::Global();
        const uint64 key = NkShaderCache::ComputeKey(NkString(src), stage, "spirv");
        
        NkShaderConvertResult cached = cache.Load(key);
        if (cached.success) {
            if (cached.SpirvWordCount() > 0) {
                const uint32* words = cached.SpirvWords();
                if (words[0] == 0x07230203) {
                    logger.Infof("[NkUIShader] Cache hit — %s\n", dbgName);
                    return cached;
                } else {
                    cache.Invalidate(key);
                }
            }
        }
        
        logger.Infof("[NkUIShader] Cache miss — %s, compiling...\n", dbgName);
        NkShaderConvertResult res = NkShaderConverter::GlslToSpirv(NkString(src), stage, dbgName);
        
        if (!res.success) {
            logger.Info("[NkUIShader] Compilation failed: {0}\n", res.errors);
            return res;
        }
        
        cache.Save(key, res);
        logger.Infof("[NkUIShader] Compiled OK — %s (%llu words)\n", 
                    dbgName, (unsigned long long)res.SpirvWordCount());
        return res;
    }

    bool Init(NkIDevice* dev, NkRenderPassHandle rp, NkGraphicsApi api) {
        device = dev;
        hRP    = rp;

        // Shader — registres différents par backend
        NkShaderDesc sd;
        sd.debugName = "NkUI";
        if (api == NkGraphicsApi::NK_API_OPENGL ||
            api == NkGraphicsApi::NK_API_SOFTWARE) {
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_GLSL);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_GLSL);
        } else if (api == NkGraphicsApi::NK_API_DIRECTX11) {
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_HLSL_DX11, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_HLSL_DX11, "PSMain");
        } else if (api == NkGraphicsApi::NK_API_DIRECTX12) {
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_HLSL_DX12, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_HLSL_DX12, "PSMain");
        } else if (api == NkGraphicsApi::NK_API_VULKAN) {
            NkShaderConvertResult vertSpv = CompileGlslToSpirv(
                kNkUI_Vert_GLSL_VK, NkSLStage::NK_VERTEX, "NkUI.vert");
            NkShaderConvertResult fragSpv = CompileGlslToSpirv(
                kNkUI_Frag_GLSL_VK, NkSLStage::NK_FRAGMENT, "NkUI.frag");

            if (!vertSpv.success || !fragSpv.success) {
                logger.Infof("[NkUIRenderer] Vulkan SPIRV compilation failed\n");
                return false;
            }
            
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
        
        hShader = device->CreateShader(sd);
        if (!hShader.IsValid()) {
            logger.Infof("[NkUIRenderer] Shader compile failed\n");
            return false;
        }

        // Vertex layout — NkUIVertex : pos(vec2) + uv(vec2) + color(uint32)
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

        // Pipeline
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
            logger.Infof("[NkUIRenderer] Pipeline creation failed\n");
            return false;
        }

        // Buffers
        hVBO = device->CreateBuffer(NkBufferDesc::VertexDynamic(kVBOCap));
        hIBO = device->CreateBuffer(NkBufferDesc::IndexDynamic(kIBOCap));
        if (!hVBO.IsValid() || !hIBO.IsValid()) {
            logger.Infof("[NkUIRenderer] Buffer creation failed\n");
            return false;
        }

        // UBO viewport
        hUBO = device->CreateBuffer(NkBufferDesc::Uniform(16));
        if (!hUBO.IsValid()) {
            logger.Infof("[NkUIRenderer] UBO creation failed\n");
            return false;
        }

        // Texture blanche 1×1 (fallback)
        NkTextureDesc tdesc = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RGBA8_UNORM, 1);
        tdesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        tdesc.debugName = "NkUI_WhiteTex";
        hWhiteTex = device->CreateTexture(tdesc);
        if (hWhiteTex.IsValid()) {
            static const uint8 white[4] = {255, 255, 255, 255};
            device->WriteTexture(hWhiteTex, white, 4);
        }

        // Sampler
        NkSamplerDesc samplerDesc{};
        samplerDesc.magFilter  = NkFilter::NK_LINEAR;
        samplerDesc.minFilter  = NkFilter::NK_LINEAR;
        samplerDesc.mipFilter  = NkMipFilter::NK_NONE;
        samplerDesc.addressU   = NkAddressMode::NK_CLAMP_TO_EDGE;
        samplerDesc.addressV   = NkAddressMode::NK_CLAMP_TO_EDGE;
        samplerDesc.addressW   = NkAddressMode::NK_CLAMP_TO_EDGE;
        hSampler = device->CreateSampler(samplerDesc);

        // Descriptor set initial (texture blanche)
        hDescSet = device->AllocateDescriptorSet(hLayout);
        if (hDescSet.IsValid()) {
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

        logger.Infof("[NkUIRenderer] Initialized OK\n");
        return true;
    }

    // Soumet une NkUIContext → DrawCalls GPU
    void Submit(NkICommandBuffer* cmd, const NkUIContext& ctx,
                uint32 fbW, uint32 fbH)
    {
        if (!cmd || !hPipe.IsValid()) return;

        // Met à jour le viewport uniform
        float32 vp[4] = {(float32)fbW, (float32)fbH, 0.f, 0.f};
        device->WriteBuffer(hUBO, vp, 16);

        cmd->BindGraphicsPipeline(hPipe);
        if (hDescSet.IsValid()) cmd->BindDescriptorSet(hDescSet, 0);

        // Viewport plein écran
        NkViewport vport{0.f, 0.f, (float32)fbW, (float32)fbH, 0.f, 1.f};
        cmd->SetViewport(vport);

        // Pour chaque layer
        for (int32 l = 0; l < NkUIContext::LAYER_COUNT; ++l) {
            const NkUIDrawList& dl = ctx.layers[l];
            if (!dl.vtx || !dl.idx || dl.cmdCount == 0) continue;

            // Upload vertex et index
            const uint64 vsz = dl.vtxCount * sizeof(NkUIVertex);
            const uint64 isz = dl.idxCount * sizeof(uint32);

            // Vérifier et réallouer les buffers si nécessaire
            // NkBufferDesc vboDesc = device->GetBufferDesc(hVBO);
            // if (vsz > vboDesc.sizeBytes) {
            //     device->DestroyBuffer(hVBO);
            //     hVBO = device->CreateBuffer(NkBufferDesc::VertexDynamic(vsz * 2)); // *2 pour marge
            //     if (!hVBO.IsValid()) continue;
            // }
            
            // NkBufferDesc iboDesc = device->GetBufferDesc(hIBO);
            // if (isz > iboDesc.sizeBytes) {
            //     device->DestroyBuffer(hIBO);
            //     hIBO = device->CreateBuffer(NkBufferDesc::IndexDynamic(isz * 2));
            //     if (!hIBO.IsValid()) continue;
            // }

            if (vsz == 0 || isz == 0) continue;

            device->WriteBuffer(hVBO, dl.vtx, vsz);
            device->WriteBuffer(hIBO, dl.idx, isz);

            cmd->BindVertexBuffer(0, hVBO);
            cmd->BindIndexBuffer(hIBO, NkIndexFormat::NK_UINT32);

            NkRect2D currentScissor{0, 0, (int32)fbW, (int32)fbH};

            for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
                const NkUIDrawCmd& dc = dl.cmds[ci];

                if (dc.type == NkUIDrawCmdType::NK_CLIP_RECT) {
                    currentScissor = {
                        (int32)::fmaxf(dc.clipRect.x, 0),
                        (int32)::fmaxf(dc.clipRect.y, 0),
                        (int32)::fmaxf(dc.clipRect.w, 0),
                        (int32)::fmaxf(dc.clipRect.h, 0)
                    };
                    cmd->SetScissor(currentScissor);
                    continue;
                }

                if (dc.idxCount == 0) continue;
                cmd->SetScissor(currentScissor);
                cmd->DrawIndexed(dc.idxCount, 1, dc.idxOffset, 0, 0);
            }
        }
    }

    void Destroy() {
        if (!device) return;
        if (hDescSet.IsValid()) device->FreeDescriptorSet(hDescSet);
        if (hLayout.IsValid())  device->DestroyDescriptorSetLayout(hLayout);
        if (hPipe.IsValid())    device->DestroyPipeline(hPipe);
        if (hShader.IsValid())  device->DestroyShader(hShader);
        if (hVBO.IsValid())     device->DestroyBuffer(hVBO);
        if (hIBO.IsValid())     device->DestroyBuffer(hIBO);
        if (hUBO.IsValid())     device->DestroyBuffer(hUBO);
        if (hSampler.IsValid()) device->DestroySampler(hSampler);
        if (hWhiteTex.IsValid())device->DestroyTexture(hWhiteTex);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i=1;i<args.Size();i++) {
        if (args[i]=="--backend=vulkan"  || args[i]=="-bvk")  return NkGraphicsApi::NK_API_VULKAN;
        if (args[i]=="--backend=dx11"    || args[i]=="-bdx11") return NkGraphicsApi::NK_API_DIRECTX11;
        if (args[i]=="--backend=dx12"    || args[i]=="-bdx12") return NkGraphicsApi::NK_API_DIRECTX12;
        if (args[i]=="--backend=metal"   || args[i]=="-bmtl")  return NkGraphicsApi::NK_API_METAL;
        if (args[i]=="--backend=sw"      || args[i]=="-bsw")   return NkGraphicsApi::NK_API_SOFTWARE;
        if (args[i]=="--backend=opengl"  || args[i]=="-bgl")   return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL;
}

static NkUIInputState BuildNkUIInput(const NkUIInputState& prev,
                                      NkVec2 mousePos, NkVec2 lastPos)
{
    // Dans une vraie intégration, on remplirait depuis NkEventSystem
    // Ici c'est un stub — le vrai code repose sur les callbacks NKWindow
    NkUIInputState s = prev;
    s.SetMousePos(mousePos.x, mousePos.y);
    (void)lastPos;
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// nkmain
// ─────────────────────────────────────────────────────────────────────────────
int nkmain(const nkentseu::NkEntryState& state) {
    logger.Infof("");
    const NkGraphicsApi api = ParseBackend(state.GetArgs());
    logger.Info("[NkUIDemoEngine] Backend: {0}\n", NkGraphicsApiName(api));

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
    NkUIContext       ctx;
    NkUIFontManager   fonts;
    NkUIWindowManager wm;
    NkUIDockManager   dock;
    NkUILayoutStack   ls;

    ctx.Init((int32)W, (int32)H);
    ctx.SetTheme(NkUITheme::Dark());
    fonts.Init();
    wm.Init();
    dock.Init({0.f, ctx.theme.metrics.titleBarHeight, (float32)W, (float32)H - ctx.theme.metrics.titleBarHeight});

    mem::NkUniquePtr<NkUIDemo> demo = mem::NkMakeUnique<NkUIDemo>();
    demo->Init(ctx, wm, fonts);

    // ── NkUI GPU Renderer ─────────────────────────────────────────────────
    NkUIRHIRenderer uiRenderer;
    if (!uiRenderer.Init(device, device->GetSwapchainRenderPass(), api)) {
        logger.Info("[NkUIDemoEngine] NkUIRHIRenderer init failed\n");
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── Command buffer ────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);

    // ── État ──────────────────────────────────────────────────────────────
    bool running = true;
    NkUIInputState input;
    NkVec2 mousePos{};
    NkClock clock;
    NkEventSystem& events = NkEvents();

    // Callbacks
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
        mousePos = {(float32)e->GetX(), (float32)e->GetY()};
        input.SetMousePos(mousePos.x, mousePos.y);
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        int32 btn = (int32)e->GetButton();
        if (btn >= 0 && btn < 5) input.SetMouseButton(btn, true);
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        int32 btn = (int32)e->GetButton();
        if (btn >= 0 && btn < 5) input.SetMouseButton(btn, false);
    });
    events.AddEventCallback<NkMouseScrollEvent>([&](NkMouseScrollEvent* e) {
        input.AddMouseWheel((float32)e->GetDeltaX(), (float32)e->GetDeltaY());
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        // Mapper NkKey → NkUIKey (les enums sont identiques dans ce projet)
        const int32 k = (int32)e->GetKey();
        if (k >= 0 && k < (int32)NkKey::NK_KEY_MAX)
            input.SetKey((NkKey)k, true);
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        const int32 k = (int32)e->GetKey();
        if (k >= 0 && k < (int32)NkKey::NK_KEY_MAX)
            input.SetKey((NkKey)k, false);
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        const char* utf8 = e->GetUtf8();
        for (int32 i = 0; utf8[i] && i < 32; ++i)
            input.AddInputChar((uint32)(uint8)utf8[i]);
    });

    logger.Info("[NkUIDemoEngine] Loop started. ESC=exit.\n");

    // ── Boucle principale ─────────────────────────────────────────────────
    while (running) {
        events.PollEvents();
        if (!running) break;

        if (W == 0 || H == 0) continue;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight())
            device->OnResize(W, H);

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;
        input.dt = dt;

        // ── NkUI frame ────────────────────────────────────────────────────
        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);

        demo->Render(ctx, wm, dock, ls, dt);

        ctx.EndFrame();
        wm.EndFrame(ctx);

        // Reset input transients
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

        // Passe UI (par-dessus tout)
        const NkRect2D area{0, 0, (int32)W, (int32)H};
        if (cmd->BeginRenderPass(rp, fbo, area)) {
            NkViewport vp{0.f, 0.f, (float32)W, (float32)H};
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
