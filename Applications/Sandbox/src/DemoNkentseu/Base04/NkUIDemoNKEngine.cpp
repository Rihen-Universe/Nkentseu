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

#include "NKLogger/NkLog.h"
#include "NKUI/NKUI.h"
#include "NKUI/NkUILayout2.h"

#include "NkUIDemo.h"
#include "NkUINKRHIBackend.h"

#include <memory>

using namespace nkentseu;
using namespace nkentseu::nkui;

// ─── Helpers locaux ───────────────────────────────────────────────────────────

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); ++i) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")   return NkGraphicsApi::NK_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11")  return NkGraphicsApi::NK_API_DIRECTX11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12")  return NkGraphicsApi::NK_API_DIRECTX12;
        if (args[i] == "--backend=metal"   || args[i] == "-bmtl")   return NkGraphicsApi::NK_API_METAL;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")    return NkGraphicsApi::NK_API_SOFTWARE;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")    return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL;
}

static void AddUtf8ToInput(NkUIInputState& input, const char* utf8) {
    if (!utf8) return;
    for (int32 i = 0; utf8[i]; ) {
        const uint8 c0 = static_cast<uint8>(utf8[i]);
        uint32 cp = '?';
        int32  n  = 1;
        if (c0 < 0x80u) {
            cp = c0; n = 1;
        } else if ((c0 & 0xE0u) == 0xC0u && utf8[i+1]) {
            cp = ((c0 & 0x1Fu) << 6) | (static_cast<uint8>(utf8[i+1]) & 0x3Fu); n = 2;
        } else if ((c0 & 0xF0u) == 0xE0u && utf8[i+1] && utf8[i+2]) {
            cp = ((c0 & 0x0Fu) << 12) |
                 ((static_cast<uint8>(utf8[i+1]) & 0x3Fu) << 6) |
                 (static_cast<uint8>(utf8[i+2]) & 0x3Fu); n = 3;
        } else if ((c0 & 0xF8u) == 0xF0u && utf8[i+1] && utf8[i+2] && utf8[i+3]) {
            cp = ((c0 & 0x07u) << 18) |
                 ((static_cast<uint8>(utf8[i+1]) & 0x3Fu) << 12) |
                 ((static_cast<uint8>(utf8[i+2]) & 0x3Fu) << 6) |
                 (static_cast<uint8>(utf8[i+3]) & 0x3Fu); n = 4;
        }
        input.AddInputChar(cp);
        i += n;
    }
}

static int32 ToUiMouseButton(NkMouseButton b) {
    switch (b) {
        case NkMouseButton::NK_MB_LEFT:    return 0;
        case NkMouseButton::NK_MB_RIGHT:   return 1;
        case NkMouseButton::NK_MB_MIDDLE:  return 2;
        case NkMouseButton::NK_MB_BACK:    return 3;
        case NkMouseButton::NK_MB_FORWARD: return 4;
        default: {
            const int32 raw = static_cast<int32>(b);
            return (raw >= 0 && raw < 5) ? raw : -1;
        }
    }
}

// ─── Curseur souris plateforme ────────────────────────────────────────────────

#if defined(NKENTSEU_PLATFORM_WINDOWS) && \
    !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
static HCURSOR UiCursorToWin32(NkUIMouseCursor cursor) {
    switch (cursor) {
        case NkUIMouseCursor::NK_TEXT_INPUT:  return LoadCursorW(nullptr, IDC_IBEAM);
        case NkUIMouseCursor::NK_HAND:        return LoadCursorW(nullptr, IDC_HAND);
        case NkUIMouseCursor::NK_RESIZE_NS:   return LoadCursorW(nullptr, IDC_SIZENS);
        case NkUIMouseCursor::NK_RESIZE_WE:   return LoadCursorW(nullptr, IDC_SIZEWE);
        case NkUIMouseCursor::NK_RESIZE_NWSE: return LoadCursorW(nullptr, IDC_SIZENWSE);
        case NkUIMouseCursor::NK_RESIZE_NESW: return LoadCursorW(nullptr, IDC_SIZENESW);
        case NkUIMouseCursor::NK_ARROW:
        default:                              return LoadCursorW(nullptr, IDC_ARROW);
    }
}
static void ApplyUiCursor(NkWindow& window, const NkUIContext& ctx) {
    const HCURSOR cur = UiCursorToWin32(ctx.GetMouseCursor());
    if (!cur || !window.mData.mHwnd) return;
    POINT p{}; if (!GetCursorPos(&p)) return;
    RECT  wr{}; if (!GetWindowRect(window.mData.mHwnd, &wr)) return;
    if (p.x < wr.left || p.x >= wr.right || p.y < wr.top || p.y >= wr.bottom) return;
    SetCursor(cur);
}
#else
static void ApplyUiCursor(NkWindow&, const NkUIContext&) {}
#endif

// ─── Chargement des polices via NkUIFontManager ───────────────────────────────
//
// On stocke un pointeur vers le backend pour l'utiliser dans le callback
// d'upload Gray8 → RGBA8 que NkUIFontAtlas::UploadToGPU appelle.
//
// Architecture :
//   ctx.fontManager.LoadFromFile(path, size)
//     └── NkUIFontBridge::BuildAtlas()   (remplit atlas.pixels en Gray8)
//   ctx.fontManager.UploadDirtyAtlases(uploadGrayFn)
//     └── NkUIFontAtlas::UploadToGPU(uploadGrayFn)
//         └── uploadGrayFn(texId, pixels, W, H)
//             └── NkUINKRHIBackend::UploadTextureGray8()
//                 └── convertit Gray8→RGBA8 + crée texture GPU

// Pointeur global vers le backend pour le callback C (sans capture lambda).
// On utilise un simple pointeur statique — ce fichier est le point d'entrée
// unique, donc pas de problème de concurrent access.
static NkUINKRHIBackend* sBackendForUpload = nullptr;

// Callback C compatible avec void(*)(uint32, const uint8*, int32, int32)
// attendu par NkUIFontAtlas::UploadToGPU.
static void FontAtlasUploadGray8(uint32 texId, const uint8* data, int32 w, int32 h) {
    if (sBackendForUpload) {
        sBackendForUpload->UploadTextureGray8(texId, data, w, h);
    }
}

// Callback pour les textures RGBA8 de la démo (images, etc.)
// Signature : bool(*)(void* device, uint32 texId, const uint8*, int32, int32)
static bool DemoTextureUploadRGBA8(void* /*device*/, uint32 texId,
                                   const uint8* data, int32 w, int32 h) {
    if (!sBackendForUpload) return false;
    return sBackendForUpload->UploadTextureRGBA8(texId, data, w, h);
}

// ─── Chargement des polices ───────────────────────────────────────────────────

static void LoadFonts(NkUIFontManager& mgr) {
    // Liste de polices candidates dans l'ordre de préférence.
    // On essaie plusieurs tailles pour avoir une hiérarchie typographique.
    struct FontEntry { const char* path; float32 size; const char* name; };

    static const FontEntry kCandidates[] = {
        // Polices projet
        {"Resources/Fonts/ProggyClean.ttf",       13.f, "Proggy-13"},
        {"Resources/Fonts/Roboto-Regular.ttf",    16.f, "Roboto-16"},
        {"Resources/Fonts/Roboto-Regular.ttf",    13.f, "Roboto-13"},
        {"Resources/Fonts/Roboto-Medium.ttf",     16.f, "Roboto-16"},
        {"Resources/Fonts/Roboto-Medium.ttf",     13.f, "Roboto-13"},
        {"Resources/Fonts/DroidSans.ttf",         16.f, "DroidSans-16"},
        {"Resources/Fonts/Karla-Regular.ttf",     16.f, "Karla-16"},
        {"Resources/Fonts/Cousine-Regular.ttf",   13.f, "Cousine-13"},
        // Polices système Windows
        {"C:/Windows/Fonts/segoeui.ttf",          16.f, "Segoe-16"},
        {"C:/Windows/Fonts/segoeui.ttf",          13.f, "Segoe-13"},
        {"C:/Windows/Fonts/arial.ttf",            16.f, "Arial-16"},
        {"C:/Windows/Fonts/tahoma.ttf",           13.f, "Tahoma-13"},
        // Polices système Linux
        {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",    16.f, "DejaVu-16"},
        {"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 16.f, "Liberation-16"},
        // Polices système macOS
        {"/System/Library/Fonts/SFNS.ttf",        16.f, "SF-16"},
        {"/Library/Fonts/Arial.ttf",              16.f, "Arial-16"},
        {nullptr, 0.f, nullptr}
    };

    int32 loaded = 0;

    for (int32 i = 0; kCandidates[i].path && loaded < 4; ++i) {
        const int32 idx = mgr.LoadFromFile(
            kCandidates[i].path,
            kCandidates[i].size,
            kCandidates[i].name);

        if (idx >= 0) {
            logger.Infof("[nkmain] Police chargée: %s (%.0fpx, idx=%d)\n", kCandidates[i].path, kCandidates[i].size, idx);
            ++loaded;
            // On s'arrête après avoir chargé au moins une police de taille 16
            // et une de taille 13 (ou deux polices minimum).
        }
    }

    if (loaded == 0) {
        int32 fontIdx = mgr.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 18.0f, "ProggyCleanEmbedded");
        ++loaded;
    }

    if (loaded == 0) {
        logger.Warn("[nkmain] Aucune police TTF trouvée — fallback bitmap uniquement\n");
    } else {
        logger.Infof("[nkmain] %d police(s) TTF chargée(s)\n", loaded);
    }
}

// ─── nkmain ──────────────────────────────────────────────────────────────────

int nkmain(const NkEntryState& state) {
    logger.Infof("[nkmain] Démarrage\n");

    // ── 1. Cache shader ───────────────────────────────────────────────────────
    {
        NkShaderCache& cache = NkShaderCache::Global();
        cache.SetCacheDir("Build/ShaderCache");
    }

    // ── 2. Backend graphique ──────────────────────────────────────────────────
    const NkGraphicsApi api = ParseBackend(state.GetArgs());
    logger.Infof("[nkmain] Backend: %s\n", NkGraphicsApiName(api));

    // ── 3. Fenêtre ────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title    = NkFormat("Nkentseu UI Demo - {0}", NkGraphicsApiName(api));
    winCfg.width    = 1600;
    winCfg.height   = 900;
    winCfg.centered = true;
    winCfg.resizable= true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Errorf("[nkmain] Fenêtre : échec\n");
        return 1;
    }

    // ── 4. Device RHI ─────────────────────────────────────────────────────────
    NkDeviceInitInfo deviceInfo;
    deviceInfo.api                       = api;
    deviceInfo.surface                   = window.GetSurfaceDesc();
    deviceInfo.width                     = window.GetSize().width;
    deviceInfo.height                    = window.GetSize().height;
    deviceInfo.context.vulkan.appName    = "NkUIDemoEngine";
    deviceInfo.context.vulkan.engineName = "Unkeny";

    NkIDevice* device = NkDeviceFactory::Create(deviceInfo);
    if (!device || !device->IsValid()) {
        logger.Errorf("[nkmain] NkIDevice : échec (%s)\n", NkGraphicsApiName(api));
        window.Close();
        return 1;
    }

    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── 5. Contexte NkUI ──────────────────────────────────────────────────────
    NkUIFontConfig fontConfig;
    fontConfig.yAxisUp            = false; // Y-down (convention UI standard)
    fontConfig.enableAtlas        = true;
    fontConfig.enableBitmapFallback = true;
    fontConfig.defaultFontSize    = 16.f;

    NkUIContext       ctx;
    NkUIWindowManager wm;
    NkUIDockManager   dock;
    NkUILayoutStack   ls;

    if (!ctx.Init(static_cast<int32>(W), static_cast<int32>(H), fontConfig)) {
        logger.Errorf("[nkmain] NkUIContext::Init : échec\n");
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }
    ctx.SetTheme(NkUITheme::Dark());

    wm.Init();
    dock.Init({
        0.f,
        ctx.theme.metrics.titleBarHeight,
        static_cast<float32>(W),
        static_cast<float32>(H) - ctx.theme.metrics.titleBarHeight
    });

    // ── 6. Backend NKRHI ──────────────────────────────────────────────────────
    NkUINKRHIBackend uiBackend;
    if (!uiBackend.Init(device, device->GetSwapchainRenderPass(), api)) {
        logger.Errorf("[nkmain] NkUINKRHIBackend::Init : échec\n");
        ctx.Destroy(); wm.Destroy(); dock.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // Expose le backend pour le callback d'upload des atlas de polices.
    sBackendForUpload = &uiBackend;

    // ── 7. Chargement des polices TTF via NkUIFontManager ────────────────────
    //
    // ctx.fontManager a été initialisé par ctx.Init() avec la police bitmap.
    // On charge maintenant les polices TTF via NKFont + NkUIFontBridge.
    // Chaque LoadFromFile :
    //   1. Alloue un slot dans fontManager.fonts[] et fontManager.atlases[]
    //   2. Crée un NkUIFontBridge en placement new dans mBridgeStorage[]
    //   3. NkUIFontBridge::BuildAtlas() rastérise les glyphes en Gray8
    //      directement dans l'atlas correspondant
    //   4. Marque atlas.dirty = true
    //
    // UploadDirtyAtlases() est appelé ensuite avec FontAtlasUploadGray8
    // qui délègue à NkUINKRHIBackend::UploadTextureGray8() (Gray8 → RGBA8).
    {
        LoadFonts(ctx.fontManager);

        // Upload de tous les atlas sales vers le GPU.
        // La fonction signature attendue par UploadToGPU :
        //   void(*)(uint32 texId, const uint8* data, int32 w, int32 h)
        ctx.fontManager.UploadDirtyAtlases(
            reinterpret_cast<void*>(&FontAtlasUploadGray8));

        logger.Infof("[nkmain] Polices uploadées sur le GPU (%d polices, %d atlas)\n",
                     ctx.fontManager.numFonts, ctx.fontManager.numAtlases);
    }

    // ── 8. Démo UI ────────────────────────────────────────────────────────────
    auto demo = std::make_unique<NkUIDemo>();

    // Donne à la démo le callback pour uploader ses propres textures (images, etc.)
    // La signature de SetGPUUploadFunc est : void SetGPUUploadFunc(void* func, void* device)
    // Le callback RGBA8 a la signature : bool(*)(void* device, uint32 texId, const uint8*, int32, int32)
    demo->SetGPUUploadFunc(
        reinterpret_cast<void*>(&DemoTextureUploadRGBA8),
        nullptr); // device non utilisé dans notre callback

    if (!demo->Init(ctx, wm)) {
        logger.Errorf("[nkmain] NkUIDemo::Init : échec\n");
        uiBackend.Destroy();
        ctx.Destroy(); wm.Destroy(); dock.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── 9. Command buffer ─────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd || !cmd->IsValid()) {
        logger.Errorf("[nkmain] CommandBuffer : échec\n");
        demo->Destroy();
        uiBackend.Destroy();
        ctx.Destroy(); wm.Destroy(); dock.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // ── 10. Boucle principale ─────────────────────────────────────────────────
    bool running = true;
    NkUIInputState input;
    NkClock clock;
    NkEventSystem& events = NkEvents();

    // ── Callbacks événements ──────────────────────────────────────────────────
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        running = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = static_cast<uint32>(e->GetWidth());
        H = static_cast<uint32>(e->GetHeight());
        ctx.viewW = static_cast<int32>(W);
        ctx.viewH = static_cast<int32>(H);
    });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()),
                          static_cast<float32>(e->GetY()));
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()),
                          static_cast<float32>(e->GetY()));
        const int32 btn = ToUiMouseButton(e->GetButton());
        if (btn >= 0) input.SetMouseButton(btn, true);
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()),
                          static_cast<float32>(e->GetY()));
        const int32 btn = ToUiMouseButton(e->GetButton());
        if (btn >= 0) input.SetMouseButton(btn, false);
    });
    events.AddEventCallback<NkMouseDoubleClickEvent>([&](NkMouseDoubleClickEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()),
                          static_cast<float32>(e->GetY()));
        const int32 btn = ToUiMouseButton(e->GetButton());
        if (btn >= 0) {
            input.SetMouseButton(btn, true);
            input.mouseDblClick[btn] = true;
        }
    });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()),
                          static_cast<float32>(e->GetY()));
        input.AddMouseWheel(static_cast<float32>(e->GetOffsetY()),
                            static_cast<float32>(e->GetOffsetX()));
    });
    events.AddEventCallback<NkMouseWheelHorizontalEvent>([&](NkMouseWheelHorizontalEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()),
                          static_cast<float32>(e->GetY()));
        input.AddMouseWheel(0.f, static_cast<float32>(e->GetOffsetX()));
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        const NkKey key = e->GetKey();
        if (key != NkKey::NK_UNKNOWN) input.SetKey(key, true);
        if (key == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        const NkKey key = e->GetKey();
        if (key != NkKey::NK_UNKNOWN) input.SetKey(key, false);
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        AddUtf8ToInput(input, e->GetUtf8());
    });

    logger.Infof("[nkmain] Boucle principale...\n");

    while (running) {
        input.BeginFrame();
        events.PollEvents();
        if (!running) break;
        if (W == 0 || H == 0) continue;

        // Resize swapchain si nécessaire
        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) {
            device->OnResize(W, H);
        }

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        input.dt = dt;

        // ── Frame logique UI ──────────────────────────────────────────────────
        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);
        ls.depth = 0;

        demo->Render(ctx, wm, dock, ls, dt);

        ctx.EndFrame();
        wm.EndFrame(ctx);

        // ── Upload atlas sales (polices rechargées en cours de route) ─────────
        // Normalement les polices sont toutes uploadées au démarrage.
        // Ce re-upload gère le cas où une police serait ajoutée dynamiquement.
        bool anyDirty = false;
        for (int32 i = 0; i < ctx.fontManager.numAtlases; ++i) {
            if (ctx.fontManager.atlases[i].dirty) { anyDirty = true; break; }
        }
        if (anyDirty) {
            ctx.fontManager.UploadDirtyAtlases(
                reinterpret_cast<void*>(&FontAtlasUploadGray8));
        }

        // ── Curseur souris ────────────────────────────────────────────────────
        ApplyUiCursor(window, ctx);

        // ── Rendu GPU ─────────────────────────────────────────────────────────
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;

        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) { device->EndFrame(frame); continue; }

        const NkRenderPassHandle  rp  = device->GetSwapchainRenderPass();
        const NkFramebufferHandle fbo = device->GetSwapchainFramebuffer();
        const NkRect2D            area{0, 0, static_cast<int32>(W), static_cast<int32>(H)};

        cmd->Reset();
        cmd->Begin();
        if (cmd->BeginRenderPass(rp, fbo, area)) {
            NkViewport vp{0.f, 0.f,
                          static_cast<float32>(W),
                          static_cast<float32>(H), 0.f, 1.f};
            cmd->SetViewport(vp);
            cmd->SetScissor(area);
            uiBackend.Submit(cmd, ctx, W, H);
            cmd->EndRenderPass();
        }
        cmd->End();

        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();
    demo->Destroy();
    sBackendForUpload = nullptr;
    uiBackend.Destroy();
    ctx.Destroy();
    wm.Destroy();
    dock.Destroy();
    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();
    logger.Infof("[nkmain] Terminé.\n");
    return 0;
}