// =============================================================================
// NkUICanvasDemo.cpp — Demo d'integration NKUI -> NKCanvas (NkUICanvasBackend)
// -----------------------------------------------------------------------------
// Pilote un NkUIContext (immediate-mode) et rend ses draw-lists via NKCanvas, en
// utilisant NkRenderWindow (MEME voie que Pong : la cible possede le cycle de
// frame -> Clear() ouvre+efface, on dessine, Display() termine+presente. Aucun
// Begin/End/SetView/SetViewport a la main : NkRenderWindow s'en charge).
//
//   NkUICanvasDemo.exe --backend=dx11   (vk / dx11 / dx12 / sw / opengl)
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"   // feed input souris -> widgets interactifs
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"   // cible : contexte + renderer2D + cycle de frame
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"

#include "NKUI/NKUI.h"
#include "NKUI/NkUIWidgets.h"   // NkUI::Text/Button/SliderFloat...
#include "NKCanvas/UI/NkUICanvasBackend.h"
#include "NKFont/Embedded/NkFontEmbedded.h"   // polices embarquees (ProggyClean, DroidSans...)
#include <cstdio>                              // snprintf (compteur de clics)

#if defined(NKENTSEU_PLATFORM_WINDOWS) && \
    !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>                         // curseurs resize (SetCursor / IDC_SIZE*)
#endif

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::nkui;   // NkUIContext / NkUIFontConfig / NkUITheme / NkUIInputState / NkUIDrawList

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkUICanvas Demo";
    d.appVersion = "1.0.0";
    return d;
})());

// ── Backend NKUI -> NKCanvas expose au callback d'upload d'atlas police ──────
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

// ── Curseur plateforme : applique le curseur souhaite par NKUI (fleches resize,
//    main, I-beam...) a la fenetre. Sinon le curseur reste une fleche fixe meme en
//    survolant un bord redimensionnable. (Win32 ; no-op ailleurs.)
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

// Balle 2D : vel = direction de base (magnitude ~1), mise a l'echelle par le
// multiplicateur global de vitesse (slider). Rendue via NkRenderer2D::DrawFilledCircle.
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
        balls[i].vel = math::NkVec2f{ math::NkCos(a), math::NkSin(a) };   // direction unitaire
        balls[i].col = palette[i];
    }
}

int nkmain(const NkEntryState& state) {
    // ── 1. Fenetre ──────────────────────────────────────────────────────────────
    NkWindowConfig cfg;
    cfg.title     = "NkUICanvas Demo";
    cfg.width     = 1024;
    cfg.height    = 640;
    cfg.centered  = true;
    cfg.resizable = true;
    NkWindow window;
    if (!window.Create(cfg)) { logger.Error("[nkuicanvas] window failed"); return -1; }

    // ── 2. Cible de rendu NKCanvas (contexte + renderer2D) — voie Pong ──────────
    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) {
        logger.Error("[nkuicanvas] NkRenderWindow init FAILED");
        window.Close();
        return -2;
    }
    logger.Infof("[nkuicanvas] backend = %s", NkGraphicsApiName(desc.api));

    // ── 3. NKUI : contexte + backend NKCanvas ───────────────────────────────────
    NkUIFontConfig fontConfig;
    fontConfig.yAxisUp              = false;   // Y-down (UI standard)
    fontConfig.enableAtlas          = true;
    fontConfig.enableBitmapFallback = true;    // police bitmap integree (pas de TTF requis)
    fontConfig.defaultFontSize      = 16.f;

    NkUIContext ctx;
    if (!ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height), fontConfig)) {
        logger.Error("[nkuicanvas] NkUIContext::Init failed");
        window.Close();
        return -3;
    }
    ctx.SetTheme(NkUITheme::Dark());

    NkUICanvasBackend ui;
    ui.Init(target.GetRenderer());   // NkIRenderer2D* de la cible
    gBackend = &ui;

    // Police : priorite a l'EMBARQUEE NKFont (portable, pas de fichier externe),
    // repli sur une police SYSTEME. L'atlas bitmap builtin est vide (glyphs=0).
    int32 uiFontId = ctx.fontManager.LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f);
    if (uiFontId < 0) uiFontId = ctx.fontManager.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    if (uiFontId < 0) uiFontId = ctx.fontManager.LoadFromFile("C:/Windows/Fonts/segoeui.ttf", 18.f);
    if (uiFontId < 0) { logger.Warn("[nkuicanvas] aucune police chargee -> texte indisponible"); uiFontId = 0; }

    ctx.fontManager.UploadDirtyAtlases(reinterpret_cast<void*>(&UploadGray8));
    logger.Infof("[nkuicanvas] fonts: %d, atlas: %d, fontId=%d",
                 ctx.fontManager.numFonts, ctx.fontManager.numAtlases, uiFontId);

    // ── 4. UI (window manager + layout) + balle 2D (vitesse pilotee par slider) ──
    NkUIWindowManager wm;
    wm.Init();
    NkUILayoutStack  ls;
    NkUIFont*        font = ctx.fontManager.Get(static_cast<uint32>(uiFontId));

    // Balles rendues DIRECTEMENT en NkRenderer2D (pas via NKUI) + collisions entre
    // elles ; la vitesse globale est pilotee en live par le slider de l'UI.
    Ball    balls[NB_BALLS];
    float32 speedMul = 180.f;                 // multiplicateur global (slider 0..600)
    {
        const math::NkVec2u s0 = target.GetSize();
        ScatterBalls(balls, static_cast<float32>(s0.x), static_cast<float32>(s0.y));
    }

    // ── 5. Boucle (jusqu'a fermeture de la fenetre) ─────────────────────────────
    bool running = true;
    NkUIInputState input;
    NkClock clock;
    auto& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    // (resize gere dans la boucle : detection robuste via GetSize, fiable au drag.)
    // Souris -> input NKUI (widgets interactifs : slider, bouton)
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

    uint32 lastW = 0, lastH = 0;
    int32  frame = 0;
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt > 0.1f) dt = 1.0f / 60.0f;

        // Ordre input CORRECT : BeginFrame (vide les deltas clic/release) PUIS les
        // events (re-remplissent pos/boutons/clics) -> les widgets voient le clic.
        input.BeginFrame();
        while (NkEvent* ev = events.PollEvent()) { (void)ev; }
        if (!running) break;
        input.dt = dt;

        // Resize robuste (drag) : detecte le changement + suit la fenetre (vue par
        // defaut adaptee a l'ecran via OnResize) + maj des bornes de balle.
        const math::NkVec2u sz = target.GetSize();
        if (sz.x != lastW || sz.y != lastH) {
            if (lastW != 0 && sz.x > 0 && sz.y > 0) target.OnResize(sz.x, sz.y);
            lastW = sz.x; lastH = sz.y;
            ctx.viewW = static_cast<int32>(sz.x);
            ctx.viewH = static_cast<int32>(sz.y);
        }
        const float32 W = static_cast<float32>(sz.x), H = static_cast<float32>(sz.y);

        // ── Physique : mouvement + rebond murs + collisions balle-balle (elastiques) ──
        for (int32 i = 0; i < NB_BALLS; ++i) {
            Ball& b = balls[i];
            b.pos.x += b.vel.x * speedMul * dt;
            b.pos.y += b.vel.y * speedMul * dt;
            if (b.pos.x < b.r)     { b.pos.x = b.r;     b.vel.x = -b.vel.x; }
            if (b.pos.x > W - b.r) { b.pos.x = W - b.r; b.vel.x = -b.vel.x; }
            if (b.pos.y < b.r)     { b.pos.y = b.r;     b.vel.y = -b.vel.y; }
            if (b.pos.y > H - b.r) { b.pos.y = H - b.r; b.vel.y = -b.vel.y; }
        }
        for (int32 i = 0; i < NB_BALLS; ++i) {
            for (int32 j = i + 1; j < NB_BALLS; ++j) {
                Ball& a = balls[i]; Ball& c = balls[j];
                const float32 dx = c.pos.x - a.pos.x, dy = c.pos.y - a.pos.y;
                const float32 d2 = dx * dx + dy * dy;
                const float32 rr = a.r + c.r;
                if (d2 > 0.0001f && d2 < rr * rr) {
                    const float32 d  = math::NkSqrt(d2);
                    const float32 nx = dx / d, ny = dy / d;
                    const float32 ov = (rr - d) * 0.5f;            // separe le chevauchement
                    a.pos.x -= nx * ov; a.pos.y -= ny * ov;
                    c.pos.x += nx * ov; c.pos.y += ny * ov;
                    const float32 rvn = (c.vel.x - a.vel.x) * nx + (c.vel.y - a.vel.y) * ny;
                    if (rvn < 0.f) {                                // elastique, masses egales
                        a.vel.x += rvn * nx; a.vel.y += rvn * ny;
                        c.vel.x -= rvn * nx; c.vel.y -= rvn * ny;
                    }
                }
            }
        }

        // ── UI immediate-mode : fenetre NKUI avec le slider de vitesse ──
        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);
        if (font && ctx.dl) {
            NkUIDrawList& dl = *ctx.dl;
            if (frame == 0) {   // pos/taille INITIALES seulement -> deplacable + redimensionnable
                NkUIWindow::SetNextWindowPos(NkVec2{ 30.f, 30.f });
                NkUIWindow::SetNextWindowSize(NkVec2{ 320.f, 210.f });
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *font, ls, "Controles balles")) {
                NkUI::Text(ctx, ls, dl, *font, "7 balles en NkRenderer2D direct + collisions.");
                NkUI::SliderFloat(ctx, ls, dl, *font, "Vitesse", speedMul, 0.f, 600.f);
                char buf[64];
                snprintf(buf, sizeof(buf), "Vitesse globale = %.0f px/s", speedMul);
                NkUI::Text(ctx, ls, dl, *font, buf);
                if (NkUI::Button(ctx, ls, dl, *font, "Disperser")) {
                    ScatterBalls(balls, W, H);
                }
            }
            NkUIWindow::End(ctx, wm, dl, ls);
        }
        ctx.EndFrame();
        wm.EndFrame(ctx);
        ApplyUiCursor(window, ctx);   // fleches resize / main / I-beam selon le survol NKUI

        // ── Rendu : Clear ouvre la frame -> balles (NkRenderer2D direct) -> UI -> Display ──
        target.Clear(NkColor2D{ 22, 22, 30, 255 });
        {
            NkRenderer2D& r2d = target.GetRenderer2D();
            for (int32 i = 0; i < NB_BALLS; ++i)
                r2d.DrawFilledCircle(balls[i].pos, balls[i].r, balls[i].col, 32);
        }
        ui.Submit(ctx, sz.x, sz.y);
        target.Display();
        ++frame;
    }

    logger.Infof("[nkuicanvas] OK - %d frames, exit propre", frame);

    // ── 5. Cleanup (target sur la pile : destructeur auto via NKMemory) ─────────
    gBackend = nullptr;
    ui.Destroy();
    ctx.Destroy();
    window.Close();
    return 0;
}
