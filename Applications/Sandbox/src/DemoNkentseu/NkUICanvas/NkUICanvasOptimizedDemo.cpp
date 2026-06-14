// =============================================================================
// NkUICanvasOptimizedDemo.cpp — Demo NKUI -> NKCanvas, version OPTIMISEE
// -----------------------------------------------------------------------------
// Identique a NkUICanvasDemo (7 balles + collisions + fenetre NKUI), MAIS la
// frame est factorisee dans RenderDemoFrame() et enregistree comme callback
// "rendre pendant le drag" (WM_ENTERSIZEMOVE) -> le rendu ne GELE PLUS pendant
// qu'on deplace / redimensionne la fenetre OS (boucle modale Windows).
//
//   NkUICanvasOptimizedDemo.exe --backend=dx11   (vk / dx11 / dx12 / sw / opengl)
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"

#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"
#include "NKCanvas/UI/NkUICanvasBackend.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
#include <cstdio>

#if defined(NKENTSEU_PLATFORM_WINDOWS) && \
    !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::nkui;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkUICanvas Optimized Demo";
    d.appVersion = "1.0.0";
    return d;
})());

static NkUICanvasBackend* gBackend = nullptr;
static void UploadGray8(uint32 texId, const uint8* data, int32 w, int32 h) {
    if (gBackend) gBackend->UploadTextureGray8(texId, data, w, h);
}

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); ++i) {
        const NkString& a = args[i];
        if (a == "--backend=vulkan" || a == "-bvk")   return NkGraphicsApi::NK_GFX_API_VULKAN;
        if (a == "--backend=dx11"   || a == "-bdx11") return NkGraphicsApi::NK_GFX_API_DX11;
        if (a == "--backend=dx12"   || a == "-bdx12") return NkGraphicsApi::NK_GFX_API_DX12;
        if (a == "--backend=sw"     || a == "-bsw")   return NkGraphicsApi::NK_GFX_API_SOFTWARE;
        if (a == "--backend=opengl" || a == "-bgl")   return NkGraphicsApi::NK_GFX_API_OPENGL;
    }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    return NkGraphicsApi::NK_GFX_API_DX11;
#else
    return NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
}

static int32 ToUiMouseButton(NkMouseButton b) {
    switch (b) {
        case NkMouseButton::NK_MB_LEFT:   return 0;
        case NkMouseButton::NK_MB_RIGHT:  return 1;
        case NkMouseButton::NK_MB_MIDDLE: return 2;
        default:                          return -1;
    }
}

#if defined(NKENTSEU_PLATFORM_WINDOWS) && \
    !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
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
    const HCURSOR cur = UiCursorToWin32(ctx.GetMouseCursor());
    if (!cur || !window.mData.mHwnd) return;
    POINT p{}; if (!GetCursorPos(&p)) return;
    RECT  wr{}; if (!GetWindowRect(window.mData.mHwnd, &wr)) return;
    if (p.x < wr.left || p.x >= wr.right || p.y < wr.top || p.y >= wr.bottom) return;
    SetCursor(cur);
}
#else
static void ApplyUiCursor(NkWindow&, const nkui::NkUIContext&) {}
#endif

struct Ball {
    math::NkVec2f pos;
    math::NkVec2f vel;
    float32       r;
    NkColor2D     col;
};

static const int32 NB_BALLS = 7;

static void ScatterBalls(Ball* balls, float32 W, float32 H) {
    static const NkColor2D palette[NB_BALLS] = {
        {255,130,70,255}, {90,200,255,255}, {130,230,140,255}, {255,210,90,255},
        {220,120,255,255}, {255,100,140,255}, {120,180,255,255}
    };
    for (int32 i = 0; i < NB_BALLS; ++i) {
        const float32 a = static_cast<float32>(i) * 1.7f + 0.3f;
        balls[i].r   = 20.f + static_cast<float32>(i % 3) * 9.f;
        balls[i].pos = math::NkVec2f{ 0.15f * W + 0.7f * W * (static_cast<float32>(i) / (NB_BALLS - 1)),
                                      0.30f * H + 0.4f * H * static_cast<float32>((i * 37) % 100) / 100.f };
        balls[i].vel = math::NkVec2f{ math::NkCos(a), math::NkSin(a) };
        balls[i].col = palette[i];
    }
}

// État d'une frame, partagé entre la boucle principale et le callback modal.
struct DemoFrame {
    NkWindow*          window;
    NkRenderWindow*    target;
    NkUIContext*       ctx;
    NkUICanvasBackend* ui;
    NkUIWindowManager* wm;
    NkUILayoutStack*   ls;
    NkUIFont*          font;
    NkUIInputState*    input;
    NkClock*           clock;
    Ball*              balls;
    float32            speedMul;
    int32              frame;
    uint32             lastW, lastH;
};

// Rend UNE frame (physique + UI + dessin). Ne pompe PAS les events. Appelee par
// la boucle ET par le callback WM_ENTERSIZEMOVE -> le rendu ne gele plus au drag.
static void RenderDemoFrame(DemoFrame& f) {
    float32 dt = f.clock->Tick().delta;
    if (dt > 0.1f) dt = 1.0f / 60.0f;
    f.input->dt = dt;

    const math::NkVec2u sz = f.target->GetSize();
    if (sz.x != f.lastW || sz.y != f.lastH) {
        if (f.lastW != 0 && sz.x > 0 && sz.y > 0) f.target->OnResize(sz.x, sz.y);
        f.lastW = sz.x; f.lastH = sz.y;
        f.ctx->viewW = static_cast<int32>(sz.x);
        f.ctx->viewH = static_cast<int32>(sz.y);
    }
    const float32 W = static_cast<float32>(sz.x), H = static_cast<float32>(sz.y);

    for (int32 i = 0; i < NB_BALLS; ++i) {
        Ball& b = f.balls[i];
        b.pos.x += b.vel.x * f.speedMul * dt;
        b.pos.y += b.vel.y * f.speedMul * dt;
        if (b.pos.x < b.r)     { b.pos.x = b.r;     b.vel.x = -b.vel.x; }
        if (b.pos.x > W - b.r) { b.pos.x = W - b.r; b.vel.x = -b.vel.x; }
        if (b.pos.y < b.r)     { b.pos.y = b.r;     b.vel.y = -b.vel.y; }
        if (b.pos.y > H - b.r) { b.pos.y = H - b.r; b.vel.y = -b.vel.y; }
    }
    for (int32 i = 0; i < NB_BALLS; ++i) {
        for (int32 j = i + 1; j < NB_BALLS; ++j) {
            Ball& a = f.balls[i]; Ball& c = f.balls[j];
            const float32 dx = c.pos.x - a.pos.x, dy = c.pos.y - a.pos.y;
            const float32 d2 = dx * dx + dy * dy;
            const float32 rr = a.r + c.r;
            if (d2 > 0.0001f && d2 < rr * rr) {
                const float32 d  = math::NkSqrt(d2);
                const float32 nx = dx / d, ny = dy / d;
                const float32 ov = (rr - d) * 0.5f;
                a.pos.x -= nx * ov; a.pos.y -= ny * ov;
                c.pos.x += nx * ov; c.pos.y += ny * ov;
                const float32 rvn = (c.vel.x - a.vel.x) * nx + (c.vel.y - a.vel.y) * ny;
                if (rvn < 0.f) {
                    a.vel.x += rvn * nx; a.vel.y += rvn * ny;
                    c.vel.x -= rvn * nx; c.vel.y -= rvn * ny;
                }
            }
        }
    }

    f.ctx->BeginFrame(*f.input, dt);
    f.wm->BeginFrame(*f.ctx);
    if (f.font && f.ctx->dl) {
        NkUIDrawList& dl = *f.ctx->dl;
        if (f.frame == 0) {
            NkUIWindow::SetNextWindowPos(NkVec2{ 30.f, 30.f });
            NkUIWindow::SetNextWindowSize(NkVec2{ 340.f, 210.f });
        }
        if (NkUIWindow::Begin(*f.ctx, *f.wm, dl, *f.font, *f.ls, "Controles (optimise)")) {
            NkUI::Text(*f.ctx, *f.ls, dl, *f.font, "Version OPTIMISEE : rendu fluide pendant le drag.");
            NkUI::SliderFloat(*f.ctx, *f.ls, dl, *f.font, "Vitesse", f.speedMul, 0.f, 600.f);
            char buf[64];
            snprintf(buf, sizeof(buf), "Vitesse globale = %.0f px/s", f.speedMul);
            NkUI::Text(*f.ctx, *f.ls, dl, *f.font, buf);
            if (NkUI::Button(*f.ctx, *f.ls, dl, *f.font, "Disperser")) {
                ScatterBalls(f.balls, W, H);
            }
        }
        NkUIWindow::End(*f.ctx, *f.wm, dl, *f.ls);
    }
    f.ctx->EndFrame();
    f.wm->EndFrame(*f.ctx);
    ApplyUiCursor(*f.window, *f.ctx);

    f.target->Clear(NkColor2D{ 22, 22, 30, 255 });
    {
        NkRenderer2D& r2d = f.target->GetRenderer2D();
        for (int32 i = 0; i < NB_BALLS; ++i)
            r2d.DrawFilledCircle(f.balls[i].pos, f.balls[i].r, f.balls[i].col, 32);
    }
    f.ui->Submit(*f.ctx, sz.x, sz.y);
    f.target->Display();
    ++f.frame;
}

static void SizeMoveFrameCb(void* user) {
    RenderDemoFrame(*static_cast<DemoFrame*>(user));
}

int nkmain(const NkEntryState& state) {
    NkWindowConfig cfg;
    cfg.title     = "NkUICanvas Optimized Demo (rendu pendant le drag)";
    cfg.width     = 1024;
    cfg.height    = 640;
    cfg.centered  = true;
    cfg.resizable = true;
    NkWindow window;
    if (!window.Create(cfg)) { logger.Error("[nkuiopt] window failed"); return -1; }

    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) { logger.Error("[nkuiopt] NkRenderWindow init FAILED"); window.Close(); return -2; }
    logger.Infof("[nkuiopt] backend = %s", NkGraphicsApiName(desc.api));

    NkUIFontConfig fontConfig;
    fontConfig.yAxisUp              = false;
    fontConfig.enableAtlas          = true;
    fontConfig.enableBitmapFallback = true;
    fontConfig.defaultFontSize      = 16.f;

    NkUIContext ctx;
    if (!ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height), fontConfig)) {
        logger.Error("[nkuiopt] NkUIContext::Init failed"); window.Close(); return -3;
    }
    ctx.SetTheme(NkUITheme::Dark());

    NkUICanvasBackend ui;
    ui.Init(target.GetRenderer());
    gBackend = &ui;

    int32 uiFontId = ctx.fontManager.LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f);
    if (uiFontId < 0) uiFontId = ctx.fontManager.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    if (uiFontId < 0) uiFontId = ctx.fontManager.LoadFromFile("C:/Windows/Fonts/segoeui.ttf", 18.f);
    if (uiFontId < 0) { logger.Warn("[nkuiopt] aucune police chargee"); uiFontId = 0; }

    ctx.fontManager.UploadDirtyAtlases(reinterpret_cast<void*>(&UploadGray8));

    NkUIWindowManager wm;
    wm.Init();
    NkUILayoutStack  ls;
    NkUIFont*        font = ctx.fontManager.Get(static_cast<uint32>(uiFontId));

    Ball balls[NB_BALLS];
    {
        const math::NkVec2u s0 = target.GetSize();
        ScatterBalls(balls, static_cast<float32>(s0.x), static_cast<float32>(s0.y));
    }

    bool running = true;
    NkUIInputState input;
    NkClock clock;
    auto& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        input.SetMousePos(static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()));
        const int32 b = ToUiMouseButton(e->GetButton());
        if (b >= 0) input.SetMouseButton(b, true);
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        const int32 b = ToUiMouseButton(e->GetButton());
        if (b >= 0) input.SetMouseButton(b, false);
    });

    // L'OPTIMISATION : la frame est factorisee + enregistree comme callback rendu
    // pendant la boucle modale Windows (WM_ENTERSIZEMOVE) -> pas de gel au drag.
    DemoFrame fdata{ &window, &target, &ctx, &ui, &wm, &ls, font, &input, &clock,
                     balls, 180.f, 0, 0, 0 };
    events.SetSizeMoveFrameCallback(&SizeMoveFrameCb, &fdata);

    while (running && window.IsOpen()) {
        input.BeginFrame();
        while (NkEvent* ev = events.PollEvent()) { (void)ev; }
        if (!running) break;
        RenderDemoFrame(fdata);   // appelee aussi par le callback pendant le drag
    }

    logger.Infof("[nkuiopt] OK - %d frames, exit propre", fdata.frame);

    gBackend = nullptr;
    ui.Destroy();
    ctx.Destroy();
    window.Close();
    return 0;
}
