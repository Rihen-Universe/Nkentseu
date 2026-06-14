// =============================================================================
// NkCanvasDemo.cpp — Demo NKCanvas SEUL (style SFML / SDL)
// -----------------------------------------------------------------------------
// La demo minimale : une fenetre + des formes animees dessinees DIRECTEMENT via
// NkRenderer2D. AUCUN NKUI, aucune police. Montre la voie haut-niveau benie :
// NkRenderWindow possede le cycle de frame -> Clear() ouvre+efface, on dessine,
// Display() termine+presente (pas de Begin/End/SetView/SetViewport a la main).
//
//   NkCanvasDemo.exe --backend=dx11   (vk / dx11 / dx12 / sw / opengl)
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"
#include "NKMath/NKMath.h"   // math::NkCos / NkSin

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"   // cible : contexte + renderer2D + cycle de frame
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkCanvas Demo";
    d.appVersion = "1.0.0";
    return d;
})());

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

int nkmain(const NkEntryState& state) {
    // ── 1. Fenetre ──────────────────────────────────────────────────────────────
    NkWindowConfig cfg;
    cfg.title     = "NkCanvas Demo (NKCanvas seul, style SFML/SDL)";
    cfg.width     = 900;
    cfg.height    = 600;
    cfg.centered  = true;
    cfg.resizable = true;
    NkWindow window;
    if (!window.Create(cfg)) { logger.Error("[nkcanvas] window failed"); return -1; }

    // ── 2. Cible de rendu NKCanvas (la voie haut-niveau, comme Pong) ────────────
    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) {
        logger.Error("[nkcanvas] NkRenderWindow init FAILED");
        window.Close();
        return -2;
    }
    logger.Infof("[nkcanvas] backend = %s", NkGraphicsApiName(desc.api));

    // ── 3. Etat de la scene ─────────────────────────────────────────────────────
    bool running = true;
    auto& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });

    NkClock clock;
    uint32  lastW = 0, lastH = 0;
    float32 t = 0.f;

    // Balle qui rebondit sur les bords (le "hello world" facon SFML).
    math::NkVec2f ball{ 220.f, 200.f };
    math::NkVec2f vel { 260.f, 215.f };   // pixels / seconde
    const float32 R = 42.f;

    // ── 4. Boucle ───────────────────────────────────────────────────────────────
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt > 0.1f) dt = 1.0f / 60.0f;
        t += dt;
        while (NkEvent* ev = events.PollEvent()) { (void)ev; }
        if (!running) break;

        // Resize : la cible suit la fenetre (vue par defaut = ecran) -> pas de clip.
        const math::NkVec2u sz = target.GetSize();
        if (sz.x != lastW || sz.y != lastH) {
            if (lastW != 0 && sz.x > 0 && sz.y > 0) target.OnResize(sz.x, sz.y);
            lastW = sz.x; lastH = sz.y;
        }
        const float32 W = static_cast<float32>(sz.x), H = static_cast<float32>(sz.y);

        // Physique de la balle (rebond sur les bords).
        ball.x += vel.x * dt; ball.y += vel.y * dt;
        if (ball.x < R)     { ball.x = R;     vel.x = -vel.x; }
        if (ball.x > W - R) { ball.x = W - R; vel.x = -vel.x; }
        if (ball.y < R)     { ball.y = R;     vel.y = -vel.y; }
        if (ball.y > H - R) { ball.y = H - R; vel.y = -vel.y; }

        const float32 cx = W * 0.5f, cy = H * 0.5f;

        // ── 5. Rendu : NkRenderer2D direct (Clear -> formes -> Display) ──────────
        target.Clear(NkColor2D{ 18, 20, 28, 255 });
        NkRenderer2D& r = target.GetRenderer2D();

        // Rectangle "pulsant" au centre (remplissage + contour).
        const float32 hp = 60.f + 28.f * math::NkSin(t * 2.f);
        const NkRect2f box{ cx - hp, cy - hp, hp * 2.f, hp * 2.f };
        r.DrawFilledRect (box, NkColor2D{ 52, 84, 150, 255 });
        r.DrawRectOutline(box, NkColor2D{ 140, 180, 255, 255 }, 2.f);

        // Triangle qui tourne autour du centre.
        const float32 tr = 95.f;
        const math::NkVec2f p0{ cx + tr * math::NkCos(t),          cy + tr * math::NkSin(t) };
        const math::NkVec2f p1{ cx + tr * math::NkCos(t + 2.094f), cy + tr * math::NkSin(t + 2.094f) };
        const math::NkVec2f p2{ cx + tr * math::NkCos(t + 4.188f), cy + tr * math::NkSin(t + 4.188f) };
        r.DrawFilledTriangle(p0, p1, p2, NkColor2D{ 255, 180, 60, 200 });

        // Eventail de lignes depuis le coin haut-gauche.
        for (int32 i = 0; i < 7; ++i) {
            const float32 a = t * 0.6f + static_cast<float32>(i) * 0.22f;
            r.DrawLine(math::NkVec2f{ 0.f, 0.f },
                       math::NkVec2f{ 110.f + 90.f * math::NkCos(a), 110.f + 90.f * math::NkSin(a) },
                       NkColor2D{ 80, 200, 160, 180 }, 1.5f);
        }

        // La balle, par-dessus (remplissage + contour clair).
        r.DrawFilledCircle (ball, R, NkColor2D{ 255, 110, 80, 255 }, 48);
        r.DrawCircleOutline(ball, R, NkColor2D{ 255, 220, 200, 255 }, 2.f, 48);

        target.Display();
    }

    logger.Info("[nkcanvas] exit propre");
    window.Close();
    return 0;
}
