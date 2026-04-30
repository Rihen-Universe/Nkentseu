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

#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

// NkUI
#include "NKUI/NkUI.h"
#include "NKUI/NkUILayout2.h"

// La démo (header-only)
#include "NkUIDemo.h"

#include <memory>

// SPIR-V pour NkUI (si Vulkan — généré depuis les shaders ci-dessous)
// #include "NkUIDemoVkSpv.inl"

using namespace nkentseu;
using namespace nkentseu::math;

// ─────────────────────────────────────────────────────────────────────────────
// Shaders NkUI GLSL
// ─────────────────────────────────────────────────────────────────────────────
static constexpr const char* kNkUI_Vert_GLSL = R"GLSL(
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
uniform vec2 uViewport;
out vec2  vUV;
out vec4  vColor;
void main() {
    // Décompresse RGBA packed
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    // NDC avec Y flippé (convention OpenGL)
    vec2 ndc = (aPos / uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

static constexpr const char* kNkUI_Frag_GLSL = R"GLSL(
#version 460 core
in  vec2 vUV;
in  vec4 vColor;
out vec4 fragColor;
layout(binding=0) uniform sampler2D uTex;
void main() {
    // UV = {0,0} → pas de texture (couleur solide)
    vec4 tc = (vUV.x > 0.0 || vUV.y > 0.0)
            ? texture(uTex, vUV)
            : vec4(1.0);
    fragColor = vColor * tc;
}
)GLSL";

static constexpr const char* kNkUI_Vert_HLSL = R"HLSL(
cbuffer Constants : register(b0) { float2 uViewport; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    o.col = v.col;
    return o;
}
)HLSL";

static constexpr const char* kNkUI_Frag_HLSL = R"HLSL(
Texture2D    uTex     : register(t0);
SamplerState uSampler : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tc = (i.uv.x > 0 || i.uv.y > 0)
              ? uTex.Sample(uSampler, i.uv) : float4(1,1,1,1);
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
    static constexpr uint64 kVBOCap = 1024 * 1024;  // 1 Mo
    static constexpr uint64 kIBOCap = 1024 * 1024;

    bool Init(NkIDevice* dev, NkRenderPassHandle rp, NkGraphicsApi api) {
        device = dev;
        hRP    = rp;

        // Shader
        NkShaderDesc sd;
        sd.debugName = "NkUI";
        if (api == NkGraphicsApi::NK_GFX_API_OPENGL || api == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_GLSL);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_GLSL);
        } else if (api==NkGraphicsApi::NK_GFX_API_D3D11||api==NkGraphicsApi::NK_GFX_API_D3D12) {
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kNkUI_Vert_HLSL, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kNkUI_Frag_HLSL, "PSMain");
        }
        hShader = device->CreateShader(sd);
        if (!hShader.IsValid()) {
            logger.Info("[NkUIRenderer] Shader compile failed\n");
            return false;
        }

        // Vertex layout
        NkVertexLayout vtxLayout;
        vtxLayout
            .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT,   0,  "POSITION", 0)
            .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT,   8,  "TEXCOORD", 0)
            .AddAttribute(2, 0, NkGPUFormat::NK_R32_UINT,     16, "COLOR",    0)
            .AddBinding(0, sizeof(nkui::NkUIVertex));

        // Descriptor layout : binding 0 = texture+sampler, binding 1 = UBO viewport
        NkDescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_VERTEX);
        hLayout = device->CreateDescriptorSetLayout(layoutDesc);

        // Pipeline alpha blending (Porter-Duff src-over)
        NkGraphicsPipelineDesc pd;
        pd.shader       = hShader;
        pd.vertexLayout = vtxLayout;
        pd.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer   = NkRasterizerDesc::NoCull();
        pd.rasterizer.scissorTest = true;
        pd.depthStencil = NkDepthStencilDesc::NoDepth();
        pd.blend        = NkBlendDesc::Alpha();
        pd.renderPass   = hRP;
        pd.debugName    = "NkUIPipeline";
        if (hLayout.IsValid())
            pd.descriptorSetLayouts.PushBack(hLayout);
        hPipe = device->CreateGraphicsPipeline(pd);
        if (!hPipe.IsValid()) {
            logger.Info("[NkUIRenderer] Pipeline creation failed\n");
            return false;
        }

        // Buffers
        hVBO = device->CreateBuffer({kVBOCap, NkBufferType::NK_VERTEX,
                                      NkResourceUsage::NK_UPLOAD,
                                      NkBindFlags::NK_VERTEX_BUFFER});
        hIBO = device->CreateBuffer({kIBOCap, NkBufferType::NK_INDEX,
                                      NkResourceUsage::NK_UPLOAD,
                                      NkBindFlags::NK_INDEX_BUFFER});
        hUBO = device->CreateBuffer(NkBufferDesc::Uniform(16)); // vec2 viewport

        // Texture blanche 1x1
        NkTextureDesc tdesc = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RGBA8_UNORM, 1);
        tdesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
        hWhiteTex = device->CreateTexture(tdesc);
        static const uint8 white[4] = {255,255,255,255};
        device->WriteTexture(hWhiteTex, white, 4);

        // Sampler linéaire
        NkSamplerDesc samplerDesc{};
        samplerDesc.addressU = samplerDesc.addressV = samplerDesc.addressW
            = NkAddressMode::NK_CLAMP_TO_EDGE;
        hSampler = device->CreateSampler(samplerDesc);

        // Descriptor set
        hDescSet = device->AllocateDescriptorSet(hLayout);
        if (hDescSet.IsValid()) {
            NkDescriptorWrite writes[2] = {};
            writes[0].set     = hDescSet;
            writes[0].binding = 0;
            writes[0].type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[0].texture = hWhiteTex;
            writes[0].sampler = hSampler;
            writes[0].textureLayout = NkResourceState::NK_SHADER_READ;
            writes[1].set     = hDescSet;
            writes[1].binding = 1;
            writes[1].type    = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[1].buffer  = hUBO;
            writes[1].bufferRange = 16;
            device->UpdateDescriptorSets(writes, 2);
        }

        logger.Info("[NkUIRenderer] Initialized.\n");
        return true;
    }

    // Soumet une NkUIContext → DrawCalls GPU
    void Submit(NkICommandBuffer* cmd, const nkui::NkUIContext& ctx, uint32 fbW, uint32 fbH) {
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
        for (int32 l = 0; l < nkui::NkUIContext::LAYER_COUNT; ++l) {
            const nkui::NkUIDrawList& dl = ctx.layers[l];
            if (!dl.vtx || !dl.idx || dl.cmdCount == 0) continue;

            // Upload vertex et index
            const uint64 vsz = dl.vtxCount * sizeof(nkui::NkUIVertex);
            const uint64 isz = dl.idxCount * sizeof(uint32);
            if (vsz == 0 || isz == 0) continue;

            device->WriteBuffer(hVBO, dl.vtx, vsz);
            device->WriteBuffer(hIBO, dl.idx, isz);

            cmd->BindVertexBuffer(0, hVBO);
            cmd->BindIndexBuffer(hIBO, NkIndexFormat::NK_UINT32);

            NkRect2D currentScissor{0, 0, (int32)fbW, (int32)fbH};

            for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
                const nkui::NkUIDrawCmd& dc = dl.cmds[ci];

                if (dc.type == nkui::NkUIDrawCmdType::NK_CLIP_RECT) {
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
        if (args[i]=="--backend=vulkan"  || args[i]=="-bvk")  return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (args[i]=="--backend=dx11"    || args[i]=="-bdx11") return NkGraphicsApi::NK_GFX_API_D3D11;
        if (args[i]=="--backend=dx12"    || args[i]=="-bdx12") return NkGraphicsApi::NK_GFX_API_D3D12;
        if (args[i]=="--backend=metal"   || args[i]=="-bmtl")  return NkGraphicsApi::NK_GFX_API_METAL;
        if (args[i]=="--backend=sw"      || args[i]=="-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (args[i]=="--backend=opengl"  || args[i]=="-bgl")   return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}

static nkui::NkUIInputState BuildNkUIInput(const nkui::NkUIInputState& prev, NkVec2 mousePos, NkVec2 lastPos) {
    // Dans une vraie intégration, on remplirait depuis NkEventSystem
    // Ici c'est un stub — le vrai code repose sur les callbacks NKWindow
    nkui::NkUIInputState s = prev;
    s.SetMousePos(mousePos.x, mousePos.y);
    (void)lastPos;
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// nkmain
// ─────────────────────────────────────────────────────────────────────────────
int nkmain(const nkentseu::NkEntryState& state) {
    logger.Info("Begin");
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

    // // ── NkUI init ─────────────────────────────────────────────────────────
    nkui::NkUIContext       ctx;
    nkui::NkUIFontManager   fonts;
    nkui::NkUIWindowManager wm;
    nkui::NkUIDockManager   dock;
    nkui::NkUILayoutStack   ls;

    ctx.Init((int32)W, (int32)H);
    ctx.SetTheme(nkui::NkUITheme::Dark());
    fonts.Init();
    wm.Init();
    dock.Init({0.f, ctx.theme.metrics.titleBarHeight, (float32)W, (float32)H - ctx.theme.metrics.titleBarHeight});
    
    
    // nkui::NkUIDemo demo;
    auto demo = std::make_unique<nkui::NkUIDemo>();
    
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
    nkui::NkUIInputState input;
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
    // events.AddEventCallback<NkMouseScrollEvent>([&](NkMouseScrollEvent* e) {
    //     input.AddMouseWheel((float32)e->GetOffsetY(), (float32)e->GetOffsetX());
    // });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        input.AddMouseWheel((float32)e->GetOffsetY(), (float32)e->GetOffsetX());
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        // Mapper NkKey → NkUIKey (les enums sont identiques dans ce projet)
        const int32 k = (int32)e->GetKey();
        if (k >= 0 && k < (int32)NkKey::NK_KEY_MAX) input.SetKey((nkui::NkKey)k, true);
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        const int32 k = (int32)e->GetKey();
        if (k >= 0 && k < (int32)NkKey::NK_KEY_MAX) input.SetKey((nkui::NkKey)k, false);
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        for (int32 i=0; e->GetUtf8()[i] && i<32; ++i)
            input.AddInputChar((uint32)e->GetUtf8()[i]);
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

        // Rendu de la scène 3D principale (optionnel — ici juste fond)
        // ... (votre code de rendu 3D ici) ...

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