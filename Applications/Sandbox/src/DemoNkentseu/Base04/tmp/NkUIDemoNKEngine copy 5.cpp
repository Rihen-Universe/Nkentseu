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
#include "NKUI/NkUI.h"
#include "NKUI/NkUILayout2.h"

#include "NkUIDemo.h"
#include "NkUINKRHIBackend.h"

#include <memory>

using namespace nkentseu;

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i = 1; i < args.Size(); ++i) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")  return NkGraphicsApi::NK_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11") return NkGraphicsApi::NK_API_DIRECTX11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12") return NkGraphicsApi::NK_API_DIRECTX12;
        if (args[i] == "--backend=metal"   || args[i] == "-bmtl")  return NkGraphicsApi::NK_API_METAL;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")   return NkGraphicsApi::NK_API_SOFTWARE;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")   return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL;
}

static void AddUtf8ToInput(nkui::NkUIInputState& input, const char* utf8) {
    if (!utf8) return;
    for (int32 i = 0; utf8[i]; ) {
        const uint8 c0 = static_cast<uint8>(utf8[i]);
        uint32 cp = '?';
        int32 n = 1;
        if (c0 < 0x80) {
            cp = c0;
            n = 1;
        } else if ((c0 & 0xE0) == 0xC0 && utf8[i + 1]) {
            cp = ((c0 & 0x1Fu) << 6) |
                 (static_cast<uint8>(utf8[i + 1]) & 0x3Fu);
            n = 2;
        } else if ((c0 & 0xF0) == 0xE0 && utf8[i + 1] && utf8[i + 2]) {
            cp = ((c0 & 0x0Fu) << 12) |
                 ((static_cast<uint8>(utf8[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<uint8>(utf8[i + 2]) & 0x3Fu);
            n = 3;
        } else if ((c0 & 0xF8) == 0xF0 && utf8[i + 1] && utf8[i + 2] && utf8[i + 3]) {
            cp = ((c0 & 0x07u) << 18) |
                 ((static_cast<uint8>(utf8[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<uint8>(utf8[i + 2]) & 0x3Fu) << 6) |
                 (static_cast<uint8>(utf8[i + 3]) & 0x3Fu);
            n = 4;
        }
        input.AddInputChar(cp);
        i += n;
    }
}

static int32 ToUiMouseButton(nkentseu::NkMouseButton b) {
    switch (b) {
        case NkMouseButton::NK_MB_LEFT: return 0;
        case NkMouseButton::NK_MB_RIGHT: return 1;
        case NkMouseButton::NK_MB_MIDDLE: return 2;
        case NkMouseButton::NK_MB_BACK: return 3;
        case NkMouseButton::NK_MB_FORWARD: return 4;
        default: {
            const int32 raw = static_cast<int32>(b);
            if (raw >= 0 && raw < 5) return raw;
            if (raw > 0 && raw <= 5) return raw - 1;
            return -1;
        }
    }
}

#if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
static HCURSOR UiCursorToWin32(nkui::NkUIMouseCursor cursor) {
    switch (cursor) {
        case nkui::NkUIMouseCursor::NK_TEXT_INPUT:  return LoadCursorW(nullptr, IDC_IBEAM);
        case nkui::NkUIMouseCursor::NK_HAND:        return LoadCursorW(nullptr, IDC_HAND);
        case nkui::NkUIMouseCursor::NK_RESIZE_NS:   return LoadCursorW(nullptr, IDC_SIZENS);
        case nkui::NkUIMouseCursor::NK_RESIZE_WE:   return LoadCursorW(nullptr, IDC_SIZEWE);
        case nkui::NkUIMouseCursor::NK_RESIZE_NWSE: return LoadCursorW(nullptr, IDC_SIZENWSE);
        case nkui::NkUIMouseCursor::NK_RESIZE_NESW: return LoadCursorW(nullptr, IDC_SIZENESW);
        case nkui::NkUIMouseCursor::NK_ARROW:
        default:                                    return LoadCursorW(nullptr, IDC_ARROW);
    }
}

static void ApplyUiCursor(NkWindow& window, const nkui::NkUIContext& ctx) {
    const HCURSOR cursor = UiCursorToWin32(ctx.GetMouseCursor());
    if (!cursor || !window.mData.mHwnd) return;

    POINT p{};
    if (!GetCursorPos(&p)) return;
    RECT wr{};
    if (!GetWindowRect(window.mData.mHwnd, &wr)) return;
    if (p.x < wr.left || p.x >= wr.right || p.y < wr.top || p.y >= wr.bottom) return;

    SetCursor(cursor);
}
#else
static void ApplyUiCursor(NkWindow&, const nkui::NkUIContext&) {}
#endif

int nkmain(const nkentseu::NkEntryState& state) {
    logger.Infof("Begin");
    const NkGraphicsApi api = ParseBackend(state.GetArgs());
    logger.Info("[NkUIDemoEngine] Backend: {0}\n", NkGraphicsApiName(api));

    {
        NkShaderCache& cache = NkShaderCache::Global();
        cache.SetCacheDir("Build/ShaderCache");
    }

    NkWindowConfig winCfg;
    winCfg.title = NkFormat("Nkentseu UI Demo - {0}", NkGraphicsApiName(api));
    winCfg.width = 1600;
    winCfg.height = 900;
    winCfg.centered = true;
    winCfg.resizable = true;

    NkWindow window;
    if (!window.Create(winCfg)) {
        logger.Errorf("[NkUIDemoEngine] Window creation failed\n");
        return 1;
    }

    NkDeviceInitInfo deviceInfo;
    deviceInfo.api = api;
    deviceInfo.surface = window.GetSurfaceDesc();
    deviceInfo.width = window.GetSize().width;
    deviceInfo.height = window.GetSize().height;
    deviceInfo.context.vulkan.appName = "NkUIDemoEngine";
    deviceInfo.context.vulkan.engineName = "Unkeny";

    NkIDevice* device = NkDeviceFactory::Create(deviceInfo);
    if (!device || !device->IsValid()) {
        logger.Errorf("[NkUIDemoEngine] NkIDevice creation failed (%s)\n", NkGraphicsApiName(api));
        window.Close();
        return 1;
    }

    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    nkui::NkUIContext ctx;
    nkui::NkUIWindowManager wm;
    nkui::NkUIDockManager dock;
    nkui::NkUILayoutStack ls;

    nkui::NkUIFontConfig fontConfig;
    fontConfig.yAxisUp = (api == NkGraphicsApi::NK_API_OPENGL) ? true : false; // OpenGL a l'axe Y vers le bas, les autres backends l'ont vers le haut
    fontConfig.enableAtlas = true;
    fontConfig.enableBitmapFallback = true;

    ctx.Init(static_cast<int32>(W), static_cast<int32>(H), fontConfig);
    ctx.SetTheme(nkui::NkUITheme::Dark());

    wm.Init();
    dock.Init({
        0.f,
        ctx.theme.metrics.titleBarHeight,
        static_cast<float32>(W),
        static_cast<float32>(H) - ctx.theme.metrics.titleBarHeight
    });

    auto demo = std::make_unique<nkui::NkUIDemo>();

    nkui::NkUINKRHIBackend uiBackend;
    if (!uiBackend.Init(device, device->GetSwapchainRenderPass(), api)) {
        logger.Errorf("[NkUIDemoEngine] NkUINKRHIBackend init failed\n");
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    nkui::NkUIFontLoader& fontLoader = demo->GetFontLoader();
    fontLoader.SetGPUDevice(&uiBackend);
    fontLoader.SetGPUUploadFunc([](void* backendPtr, uint32 texId, const uint8* data, int32 w, int32 h) -> bool {
        auto* backend = static_cast<nkui::NkUINKRHIBackend*>(backendPtr);
        if (!backend) return false;
        return backend->UploadTextureRGBA8(texId, data, w, h);
    });

    nkui::NkUIFontConfig fontConfig;
    fontConfig.yAxisUp = false;
    fontConfig.enableAtlas = true;
    fontConfig.enableBitmapFallback = true;
    if (!fontLoader.Init(fontConfig, 8 * 1024 * 1024, 2 * 1024 * 1024, 512, 512, &uiBackend, fontLoader.GetGPUUploadFunc())) {
        logger.Errorf("[NkUIDemoEngine] Font loader init failed\n");
        uiBackend.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    // if (!demo->Init(ctx, wm, fontLoader)) {
    if (!demo->Init(ctx, wm)) {
        logger.Errorf("[NkUIDemoEngine] Demo init failed\n");
        uiBackend.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd || !cmd->IsValid()) {
        logger.Errorf("[NkUIDemoEngine] Command buffer creation failed\n");
        demo->Destroy();
        uiBackend.Destroy();
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 1;
    }

    bool running = true;
    nkui::NkUIInputState input;
    NkClock clock;
    NkEventSystem& events = NkEvents();

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
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        const int32 btn = ToUiMouseButton(e->GetButton());
        if (btn >= 0 && btn < 5) input.SetMouseButton(btn, true);
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        const int32 btn = ToUiMouseButton(e->GetButton());
        if (btn >= 0 && btn < 5) input.SetMouseButton(btn, false);
    });
    events.AddEventCallback<NkMouseDoubleClickEvent>([&](NkMouseDoubleClickEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        const int32 btn = ToUiMouseButton(e->GetButton());
        if (btn >= 0 && btn < 5) {
            input.SetMouseButton(btn, true);
            input.mouseDblClick[btn] = true;
        }
    });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        input.AddMouseWheel(static_cast<float32>(e->GetOffsetY()), static_cast<float32>(e->GetOffsetX()));
    });
    events.AddEventCallback<NkMouseWheelHorizontalEvent>([&](NkMouseWheelHorizontalEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        input.AddMouseWheel(0.f, static_cast<float32>(e->GetOffsetX()));
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        nkui::NkKey key = e->GetKey();
        if (key != nkui::NkKey::NK_UNKNOWN) input.SetKey(key, true);
        if (key == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        nkui::NkKey key = e->GetKey();
        if (key != nkui::NkKey::NK_UNKNOWN) input.SetKey(key, false);
    });
    events.AddEventCallback<NkTextInputEvent>([&](NkTextInputEvent* e) {
        AddUtf8ToInput(input, e->GetUtf8());
    });

    while (running) {
        input.BeginFrame();
        events.PollEvents();
        if (!running) break;
        if (W == 0 || H == 0) continue;

        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight()) {
            device->OnResize(W, H);
        }

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        input.dt = dt;

        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);
        ls.depth = 0; // layout stack is immediate/frame-local state
        demo->Render(ctx, wm, dock, ls, dt);
        ctx.EndFrame();
        wm.EndFrame(ctx);
        ApplyUiCursor(window, ctx);

        fontLoader.UploadAtlas();

        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;

        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) {
            device->EndFrame(frame);
            continue;
        }

        const NkRenderPassHandle rp = device->GetSwapchainRenderPass();
        const NkFramebufferHandle fbo = device->GetSwapchainFramebuffer();
        const NkRect2D area{0, 0, static_cast<int32>(W), static_cast<int32>(H)};

        cmd->Reset();
        cmd->Begin();
        if (cmd->BeginRenderPass(rp, fbo, area)) {
            NkViewport vp{0.f, 0.f, static_cast<float32>(W), static_cast<float32>(H), 0.f, 1.f};
            cmd->SetViewport(vp);
            cmd->SetScissor(area);
            uiBackend.Submit(cmd, ctx, W, H);
            cmd->EndRenderPass();
        }
        cmd->End();

        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    device->WaitIdle();
    demo->Destroy();
    uiBackend.Destroy();
    ctx.Destroy();
    wm.Destroy();
    dock.Destroy();
    device->DestroyCommandBuffer(cmd);
    NkDeviceFactory::Destroy(device);
    window.Close();
    return 0;
}
