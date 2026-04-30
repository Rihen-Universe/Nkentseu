// =============================================================================
// NkRendererDemo.cpp
// Démo complète NKRenderer — valide toute la chaîne :
//   Device → Renderer → Shader → Texture → Mesh → Material → Camera → Draw
//
// Commande : --backend=opengl|vulkan|dx11|dx12|sw
// =============================================================================
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
#include "NKLogger/NkLog.h"

// En-tête unique NKRenderer
#include "NKRenderer/NKRenderer.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// ── Sélection backend ─────────────────────────────────────────────────────────
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i=1; i<args.Size(); ++i) {
        if (args[i]=="--backend=vulkan"  || args[i]=="-bvk")  return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (args[i]=="--backend=dx11"    || args[i]=="-bdx11") return NkGraphicsApi::NK_GFX_API_D3D11;
        if (args[i]=="--backend=dx12"    || args[i]=="-bdx12") return NkGraphicsApi::NK_GFX_API_D3D12;
        if (args[i]=="--backend=metal"   || args[i]=="-bmtl")  return NkGraphicsApi::NK_GFX_API_METAL;
        if (args[i]=="--backend=sw"      || args[i]=="-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
    }
    return NkGraphicsApi::NK_GFX_API_OPENGL;
}

int nkmain(const nkentseu::NkEntryState& state) {

    // ── Device RHI ────────────────────────────────────────────────────────────
    const NkGraphicsApi api = ParseBackend(state.GetArgs());

    NkWindowConfig winCfg;
    winCfg.title    = NkFormat("NKRenderer Demo — {0}", NkGraphicsApiName(api));
    winCfg.width    = 1280;
    winCfg.height   = 720;
    winCfg.centered = true;
    winCfg.resizable= true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Errorf("[NKRendererDemo] Fenêtre non créée\n");
        return 1;
    }

    NkDeviceInitInfo devInfo;
    devInfo.api     = api;
    devInfo.surface = window.GetSurfaceDesc();
    devInfo.width   = window.GetSize().width;
    devInfo.height  = window.GetSize().height;
    devInfo.context.vulkan.appName    = "NKRendererDemo";
    devInfo.context.vulkan.engineName = "Unkeny";

    NkIDevice* device = NkDeviceFactory::Create(devInfo);
    if (!device || !device->IsValid()) {
        logger.Errorf("[NKRendererDemo] Device invalide\n");
        window.Close();
        return 1;
    }

    // ── NKRenderer ────────────────────────────────────────────────────────────
    NkIRenderer* renderer = NkRendererFactory::Create(device);
    if (!renderer) {
        logger.Errorf("[NKRendererDemo] Renderer non créé\n");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── Shaders (toutes les variantes backends dans un seul NkShaderDesc) ─────
    // Shader PBR
    NkShaderDesc pbrShaderDesc;
    pbrShaderDesc.debugName = "PBR";
    pbrShaderDesc
        .AddGL  (NkShaderStageType::Vertex,   shaders::opengl::kPBR_Vert)
        .AddGL  (NkShaderStageType::Fragment, shaders::opengl::kPBR_Frag)
        .AddVK  (NkShaderStageType::Vertex,   shaders::vulkan::kPBR_Vert)
        .AddVK  (NkShaderStageType::Fragment, shaders::vulkan::kPBR_Frag)
        .AddDX11(NkShaderStageType::Vertex,   shaders::dx11::kPBR_VS, "VSMain")
        .AddDX11(NkShaderStageType::Fragment, shaders::dx11::kPBR_PS, "PSMain")
        .AddDX12(NkShaderStageType::Vertex,   shaders::dx12::kPBR_VS, "VSMain")
        .AddDX12(NkShaderStageType::Fragment, shaders::dx12::kPBR_PS, "PSMain");

    // Shader Unlit
    NkShaderDesc unlitDesc;
    unlitDesc.debugName = "Unlit";
    unlitDesc
        .AddGL  (NkShaderStageType::Vertex,   shaders::opengl::kUnlit_Vert)
        .AddGL  (NkShaderStageType::Fragment, shaders::opengl::kUnlit_Frag)
        .AddVK  (NkShaderStageType::Vertex,   shaders::vulkan::kUnlit_Vert)
        .AddVK  (NkShaderStageType::Fragment, shaders::vulkan::kUnlit_Frag)
        .AddDX11(NkShaderStageType::Vertex,   shaders::dx11::kUnlit_VS, "VSMain")
        .AddDX11(NkShaderStageType::Fragment, shaders::dx11::kUnlit_PS, "PSMain")
        .AddDX12(NkShaderStageType::Vertex,   shaders::dx12::kUnlit_VS, "VSMain")
        .AddDX12(NkShaderStageType::Fragment, shaders::dx12::kUnlit_PS, "PSMain");

    // Shader 2D Sprite
    NkShaderDesc sprite2DDesc;
    sprite2DDesc.debugName = "Sprite2D";
    sprite2DDesc
        .AddGL  (NkShaderStageType::Vertex,   shaders::opengl::k2D_Vert)
        .AddGL  (NkShaderStageType::Fragment, shaders::opengl::k2D_Frag)
        .AddVK  (NkShaderStageType::Vertex,   shaders::vulkan::k2D_Vert)
        .AddVK  (NkShaderStageType::Fragment, shaders::vulkan::k2D_Frag)
        .AddDX11(NkShaderStageType::Vertex,   shaders::dx11::k2D_VS, "VSMain")
        .AddDX11(NkShaderStageType::Fragment, shaders::dx11::k2D_PS, "PSMain")
        .AddDX12(NkShaderStageType::Vertex,   shaders::dx12::k2D_VS, "VSMain")
        .AddDX12(NkShaderStageType::Fragment, shaders::dx12::k2D_PS, "PSMain");

    // Shader Text
    NkShaderDesc textDesc;
    textDesc.debugName = "Text";
    textDesc
        .AddGL  (NkShaderStageType::Vertex,   shaders::opengl::k2D_Vert)
        .AddGL  (NkShaderStageType::Fragment, shaders::opengl::kText_Frag)
        .AddVK  (NkShaderStageType::Vertex,   shaders::vulkan::k2D_Vert)
        .AddVK  (NkShaderStageType::Fragment, shaders::vulkan::kText_Frag)
        .AddDX11(NkShaderStageType::Vertex,   shaders::dx11::k2D_VS,  "VSMain")
        .AddDX11(NkShaderStageType::Fragment, shaders::dx11::kText_PS,"PSMain")
        .AddDX12(NkShaderStageType::Vertex,   shaders::dx12::k2D_VS,  "VSMain")
        .AddDX12(NkShaderStageType::Fragment, shaders::dx12::kText_PS,"PSMain");

    // Shader Shadow
    NkShaderDesc shadowDesc;
    shadowDesc.debugName = "Shadow";
    shadowDesc
        .AddGL  (NkShaderStageType::Vertex,   shaders::opengl::kShadow_Vert)
        .AddGL  (NkShaderStageType::Fragment, shaders::opengl::kShadow_Frag)
        .AddVK  (NkShaderStageType::Vertex,   shaders::vulkan::kShadow_Vert)
        .AddVK  (NkShaderStageType::Fragment, shaders::vulkan::kShadow_Frag)
        .AddDX11(NkShaderStageType::Vertex,   shaders::dx11::kShadow_VS)
        .AddDX11(NkShaderStageType::Fragment, shaders::dx11::kShadow_PS)
        .AddDX12(NkShaderStageType::Vertex,   shaders::dx12::kShadow_VS)
        .AddDX12(NkShaderStageType::Fragment, shaders::dx12::kShadow_PS);

    // Shader Debug
    NkShaderDesc debugDesc;
    debugDesc.debugName = "Debug";
    debugDesc
        .AddGL  (NkShaderStageType::Vertex,   shaders::opengl::kDebug_Vert)
        .AddGL  (NkShaderStageType::Fragment, shaders::opengl::kDebug_Frag)
        .AddVK  (NkShaderStageType::Vertex,   shaders::vulkan::kDebug_Vert)
        .AddVK  (NkShaderStageType::Fragment, shaders::vulkan::kDebug_Frag)
        .AddDX11(NkShaderStageType::Vertex,   shaders::dx11::kDebug_VS)
        .AddDX11(NkShaderStageType::Fragment, shaders::dx11::kDebug_PS)
        .AddDX12(NkShaderStageType::Vertex,   shaders::dx12::kDebug_VS)
        .AddDX12(NkShaderStageType::Fragment, shaders::dx12::kDebug_PS);

    NkShaderHandle hPBRShader    = renderer->CreateShader(pbrShaderDesc);
    NkShaderHandle hUnlitShader  = renderer->CreateShader(unlitDesc);
    NkShaderHandle hSprite2D     = renderer->CreateShader(sprite2DDesc);
    NkShaderHandle hTextShader   = renderer->CreateShader(textDesc);
    NkShaderHandle hShadowShader = renderer->CreateShader(shadowDesc);
    NkShaderHandle hDebugShader  = renderer->CreateShader(debugDesc);

    logger.Info("[NKRendererDemo] Shaders créés: PBR={0} Unlit={1} Sprite2D={2} Text={3}\n",
                hPBRShader.IsValid()?1:0,
                hUnlitShader.IsValid()?1:0,
                hSprite2D.IsValid()?1:0,
                hTextShader.IsValid()?1:0);

    // ── Bibliothèque de matériaux ─────────────────────────────────────────────
    NkMaterialLibrary matLib;
    matLib.Init(renderer,
                hPBRShader, hUnlitShader, hSprite2D,
                hTextShader, hShadowShader, hDebugShader);

    // ── Textures ──────────────────────────────────────────────────────────────
    // Texture procédurale checkerboard 256×256
    NkVector<uint8> checkPx;
    checkPx.Resize(256*256*4);
    for (int y=0; y<256; ++y) {
        for (int x=0; x<256; ++x) {
            const bool dark = ((x/32 + y/32) & 1) != 0;
            const uint8 v   = dark ? 40 : 210;
            checkPx[(y*256+x)*4+0] = v;
            checkPx[(y*256+x)*4+1] = v;
            checkPx[(y*256+x)*4+2] = v;
            checkPx[(y*256+x)*4+3] = 255;
        }
    }
    NkTextureDesc checkDesc = NkTextureDesc::Tex2D(256, 256, NkTextureFormat::RGBA8_SRGB, 0);
    checkDesc.initialData = checkPx.Begin();
    checkDesc.rowPitch    = 256*4;
    NkTextureHandle hCheckTex = renderer->CreateTexture(checkDesc);
    renderer->GenerateMipmaps(hCheckTex);

    // ── Instances de matériaux ────────────────────────────────────────────────
    NkMaterialInstHandle hPBRInst = NkMaterialInstanceBuilder(matLib.GetPBR(), "PBR_Checker")
        .Set("useAlbedoMap",    true)
        .Set("metallicFactor",  0.0f)
        .Set("roughnessFactor", 0.5f)
        .Set("uAlbedoMap",      hCheckTex)
        .Build(renderer);

    NkMaterialInstHandle hUnlitInst = NkMaterialInstanceBuilder(matLib.GetUnlit(), "Unlit_Red")
        .Set("baseColorFactor", NkVec4f{1.0f, 0.2f, 0.1f, 1.0f})
        .Build(renderer);

    // ── Géométrie ─────────────────────────────────────────────────────────────
    NkMeshHandle hCube   = renderer->CreateCube3D(0.5f);
    NkMeshHandle hSphere = renderer->CreateSphere3D(0.5f, 16, 24);
    NkMeshHandle hPlane  = renderer->CreatePlane3D(4.f, 4.f, 4, 4);
    NkMeshHandle hQuad   = renderer->CreateQuad2D(1.f, 1.f);

    logger.Info("[NKRendererDemo] Meshes créés: cube={0} sphere={1} plane={2}\n",
                hCube.IsValid()?1:0, hSphere.IsValid()?1:0, hPlane.IsValid()?1:0);

    // ── Caméra 3D ────────────────────────────────────────────────────────────
    NkCamera3D cam3D(renderer);
    cam3D.Create();
    cam3D.LookAt({0.f, 2.f, 5.f}, {0.f, 0.f, 0.f});
    cam3D.SetFOV(60.f);
    cam3D.SetAspect(1280.f/720.f);

    // ── Caméra 2D ────────────────────────────────────────────────────────────
    NkCamera2D cam2D(renderer);
    cam2D.Create(1280, 720);

    // ── Font ──────────────────────────────────────────────────────────────────
    NkFontHandle hFont;
    {
        NkFontDesc fd{};
        fd.filePath = "C:/Windows/Fonts/segoeui.ttf";
        fd.sizePt   = 18.f;
        hFont = renderer->CreateFont(fd);
        if (!hFont.IsValid()) {
            fd.filePath = "C:/Windows/Fonts/arial.ttf";
            hFont = renderer->CreateFont(fd);
        }
    }

    // ── Helpers 2D/3D ─────────────────────────────────────────────────────────
    NkRenderer2D r2D(renderer);
    r2D.Init(1280, 720);

    NkRenderer3D r3D(renderer);
    r3D.Init({.shadowMapping=true, .shadowMapSize=1024});

    // ── État de la démo ───────────────────────────────────────────────────────
    bool    running   = true;
    float32 rotAngle  = 0.f;
    float32 camYaw    = 0.f, camPitch = 20.f, camDist = 5.f;
    bool    keys[512] = {};
    NkClock clock;
    NkEventSystem& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = true;
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        if ((uint32)e->GetKey() < 512) keys[(uint32)e->GetKey()] = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        renderer->OnResize((uint32)e->GetWidth(), (uint32)e->GetHeight());
        cam2D.SetViewport((uint32)e->GetWidth(), (uint32)e->GetHeight());
    });

    logger.Info("[NKRendererDemo] Boucle principale. ESC=quitter, WASD=caméra\n");

    // ── Boucle principale ─────────────────────────────────────────────────────
    while (running) {
        events.PollEvents();
        if (!running) break;

        const float32 dt = [&]{
            float32 d = clock.Tick().delta;
            return (d <= 0.f || d > 0.25f) ? 1.f/60.f : d;
        }();

        // Contrôles caméra
        const float32 sp = 60.f;
        if (keys[(uint32)NkKey::NK_A]) camYaw   -= sp*dt;
        if (keys[(uint32)NkKey::NK_D]) camYaw   += sp*dt;
        if (keys[(uint32)NkKey::NK_W]) camPitch += sp*dt;
        if (keys[(uint32)NkKey::NK_S]) camPitch -= sp*dt;
        camPitch = NkClamp(camPitch, -80.f, 80.f);
        rotAngle += 45.f * dt;

        // Mise à jour caméra
        const float32 ex = camDist*NkCos(NkToRadians(camPitch))*NkSin(NkToRadians(camYaw));
        const float32 ey = camDist*NkSin(NkToRadians(camPitch));
        const float32 ez = camDist*NkCos(NkToRadians(camPitch))*NkCos(NkToRadians(camYaw));
        const float32 aspect = (float32)device->GetSwapchainWidth()
                              / (float32)device->GetSwapchainHeight();
        cam3D.LookAt({ex,ey,ez}, {0,0,0});
        cam3D.SetAspect(aspect);

        // ── Frame ─────────────────────────────────────────────────────────────
        if (!renderer->BeginFrame()) continue;

        NkRenderPassInfo pass{};
        pass.camera           = cam3D.GetHandle();
        pass.clearColor       = true;
        pass.clearColorValue  = {0.1f, 0.1f, 0.12f, 1.f};

        if (!renderer->BeginRenderPass(pass)) {
            renderer->EndFrame();
            continue;
        }

        NkRendererCommand& cmd = renderer->GetCommand();

        // ── Lumières ─────────────────────────────────────────────────────────
        cmd.AddDirectionalLight({0.3f,-1.f,0.5f}, {1.f,0.97f,0.9f}, 1.f, true);
        cmd.AddPointLight({2.f,1.f,2.f}, {0.2f,0.5f,1.f}, 2.f, 8.f);

        // ── Grille debug ─────────────────────────────────────────────────────
        r3D.Begin(cmd);
        r3D.DrawGrid(20, 1.f, {0.35f,0.35f,0.35f,0.8f});
        r3D.DrawAxes({0,0,0}, 1.f);
        r3D.End();

        // ── Draw calls 3D ────────────────────────────────────────────────────
        // Cube rotatif PBR
        cmd.Draw3D(hCube, hPBRInst,
                   NkMat4f::RotationY(NkAngle(rotAngle))
                   * NkMat4f::RotationX(NkAngle(rotAngle*0.5f)));

        // Sphère unlit rouge
        cmd.Draw3D(hSphere, hUnlitInst,
                   NkMat4f::Translation({2.f, 0.f, 0.f}));

        // Plan sol PBR
        cmd.Draw3D(hPlane, hPBRInst,
                   NkMat4f::Translation({0.f,-1.f,0.f}));

        // ── Draw calls 2D ────────────────────────────────────────────────────
        r2D.Begin(cmd);

        // Sprite checkerboard en coin
        NkMaterialInstHandle hSpriteInst = matLib.CreateInstance(renderer, "nk/2d_sprite");
        if (hSpriteInst.IsValid()) {
            renderer->SetMaterialTexture(hSpriteInst, "uTexture", hCheckTex);
            renderer->SetMaterialBool   (hSpriteInst, "useTexture", true);
        }
        r2D.DrawSprite(hCheckTex, {120.f,80.f}, {200.f,150.f}, {1,1,1,0.85f}, 0.f);

        // Primitives 2D
        r2D.FillRect  ({10.f,10.f}, {60.f,30.f}, {0.2f,0.8f,0.4f,1.f});
        r2D.DrawRect  ({80.f,10.f}, {60.f,30.f}, {1.f,0.8f,0.f,1.f}, 2.f);
        r2D.DrawCircle({250.f,30.f}, 22.f,       {0.3f,0.6f,1.f,1.f}, 2.f, 32);
        r2D.DrawArrow ({300.f,70.f},{400.f,40.f},{1.f,0.3f,0.1f,1.f}, 2.f, 14.f);
        r2D.DrawLine  ({10.f,55.f}, {70.f,55.f}, {1.f,1.f,0.f,1.f},  1.5f);

        r2D.End();

        // ── Texte ─────────────────────────────────────────────────────────────
        if (hFont.IsValid()) {
            char info[128];
            snprintf(info, sizeof(info), "NKRenderer Demo — %s | FPS: %.0f",
                     NkGraphicsApiName(api), 1.f/dt);
            cmd.DrawText(hFont, info, {10.f, (float32)device->GetSwapchainHeight()-30.f},
                         18.f, {1,1,1,1});
            cmd.DrawText(hFont, "WASD = caméra | ESC = quitter",
                         {10.f, (float32)device->GetSwapchainHeight()-56.f},
                         14.f, {0.8f,0.8f,0.8f,1.f});
        }

        // ── Debug stats ───────────────────────────────────────────────────────
        const NkRendererStats& stats = renderer->GetLastFrameStats();
        char statsStr[128];
        snprintf(statsStr, sizeof(statsStr), "Draw3D:%u Draw2D:%u Tri:%u",
                 stats.drawCalls3D, stats.drawCalls2D, stats.triangles);
        if (hFont.IsValid())
            cmd.DrawText(hFont, statsStr, {10.f, 10.f}, 14.f, {0.7f,1.f,0.7f,1.f});

        renderer->EndRenderPass();
        renderer->EndFrame();
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();

    cam3D.Destroy();
    cam2D.Destroy();
    r2D.Destroy();
    r3D.Destroy();
    matLib.Shutdown();

    renderer->DestroyTexture(hCheckTex);
    renderer->DestroyMesh(hQuad);
    renderer->DestroyMesh(hPlane);
    renderer->DestroyMesh(hSphere);
    renderer->DestroyMesh(hCube);
    if (hFont.IsValid()) renderer->DestroyFont(hFont);
    renderer->DestroyShader(hPBRShader);
    renderer->DestroyShader(hUnlitShader);
    renderer->DestroyShader(hSprite2D);
    renderer->DestroyShader(hTextShader);
    renderer->DestroyShader(hShadowShader);
    renderer->DestroyShader(hDebugShader);

    NkRendererFactory::Destroy(renderer);
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[NKRendererDemo] Terminé proprement.\n");
    return 0;
}