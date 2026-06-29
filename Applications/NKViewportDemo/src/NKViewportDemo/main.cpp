// =============================================================================
// NKViewportDemo — demo AUTONOME du viewport 3D (rendu LOGICIEL via NKGui).
//   Fenetre minimale plein ecran : grille au sol + cube ombre + gizmo d'axes,
//   camera ORBITALE (glisser = orbite, molette = zoom). Projection PINHOLE faite
//   main (zero dependance au renderer 3D GPU). Bac a sable 3D isole, hors NKCode.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKTime/NkClock.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKGui/NKGui.h"
#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::nkgui;
using namespace nkentseu::renderer;
namespace mth = nkentseu::math;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NKViewportDemo";
    d.appVersion = "0.1.0";
    return d;
})());

// Dessine la scene 3D dans `area` : grille, cube ombre, gizmo. Camera orbitale
// definie par (yaw, pitch, dist). Rendu 100 % via la draw-list (AddLine/Triangle).
static void DrawScene3D(NkGuiContext& ctx, const NkRect& area, float32 yaw, float32 pitch, float32 dist) {
    auto& dl = ctx.DL();
    dl.AddRectFilled(area, NkColor{ 24, 24, 26, 255 });
    if (area.w < 8.f || area.h < 8.f) return;

    // Camera pinhole faite main (base forward/right/up + projection perspective).
    const float32 tx = 0.f, ty = 1.f, tz = 0.f;
    const float32 cp = mth::NkCos(pitch), sp = mth::NkSin(pitch);
    const float32 cyaw = mth::NkCos(yaw), syaw = mth::NkSin(yaw);
    const float32 ex = tx + dist * cp * syaw, ey = ty + dist * sp, ez = tz + dist * cp * cyaw;
    auto norm3 = [](float32& a, float32& b, float32& c) {
        const float32 l = mth::NkSqrt(a * a + b * b + c * c); if (l > 1e-6f) { a /= l; b /= l; c /= l; }
    };
    float32 fx = tx - ex, fy = ty - ey, fz = tz - ez; norm3(fx, fy, fz);     // forward
    float32 rx = fz, ry = 0.f, rz = -fx; norm3(rx, ry, rz);                  // right = up(0,1,0) x fwd
    const float32 ux = fy * rz - fz * ry, uy = fz * rx - fx * rz, uz = fx * ry - fy * rx;  // up = fwd x right
    const float32 focal = (area.h * 0.5f) / mth::NkTan(mth::NkRadiansFromDegrees(50.f) * 0.5f);
    const float32 ccx = area.x + area.w * 0.5f, ccy = area.y + area.h * 0.5f;
    const float32 kNear = 0.05f;

    // Coordonnees vue (vx,vy,vz=profondeur) d'un point monde.
    auto viewOf = [&](float32 x, float32 y, float32 z, float32& vx, float32& vy, float32& vz) {
        const float32 dx = x - ex, dy = y - ey, dz = z - ez;
        vx = dx * rx + dy * ry + dz * rz;
        vy = dx * ux + dy * uy + dz * uz;
        vz = dx * fx + dy * fy + dz * fz;
    };
    auto projVS = [&](float32 vx, float32 vy, float32 vz, NkVec2& out) {
        out.x = ccx + vx / vz * focal;
        out.y = ccy - vy / vz * focal;
    };
    auto project = [&](float32 x, float32 y, float32 z, NkVec2& out) -> bool {
        float32 vx, vy, vz; viewOf(x, y, z, vx, vy, vz);
        if (vz <= kNear) return false;
        projVS(vx, vy, vz, out); return true;
    };
    // Ligne 3D avec CLIPPING au plan proche : si un bout passe derriere la camera,
    // on coupe le segment au plan near au lieu de le supprimer (sinon la grille
    // "perd" des lignes au zoom).
    auto drawLine3D = [&](float32 ax, float32 ay, float32 az, float32 bx, float32 by, float32 bz,
                          const NkColor& col, float32 th) {
        float32 avx, avy, avz, bvx, bvy, bvz;
        viewOf(ax, ay, az, avx, avy, avz);
        viewOf(bx, by, bz, bvx, bvy, bvz);
        if (avz <= kNear && bvz <= kNear) return;                            // entierement derriere
        if (avz <= kNear) { const float32 t = (kNear - avz) / (bvz - avz); avx += (bvx - avx) * t; avy += (bvy - avy) * t; avz = kNear; }
        else if (bvz <= kNear) { const float32 t = (kNear - bvz) / (avz - bvz); bvx += (avx - bvx) * t; bvy += (avy - bvy) * t; bvz = kNear; }
        NkVec2 pa, pb; projVS(avx, avy, avz, pa); projVS(bvx, bvy, bvz, pb);
        dl.AddLine(pa, pb, col, th);
    };

    dl.PushClipRect(area, true);

    // Grille au sol (y=0), large + clippee au near (ne disparait pas au zoom).
    const int32 G = 24;
    for (int32 i = -G; i <= G; ++i) {
        const float32 t = static_cast<float32>(i), e = static_cast<float32>(G);
        const NkColor gc = (i == 0) ? NkColor{ 90, 90, 96, 255 } : NkColor{ 52, 52, 58, 255 };
        drawLine3D(t, 0.f, -e, t, 0.f, e, gc, 1.f);
        drawLine3D(-e, 0.f, t, e, 0.f, t, gc, 1.f);
    }

    // Cube ombre (centre en y=1, demi-taille 1 -> pose sur la grille).
    const float32 V[8][3] = {
        { -1, 0, -1 }, { 1, 0, -1 }, { 1, 2, -1 }, { -1, 2, -1 },
        { -1, 0,  1 }, { 1, 0,  1 }, { 1, 2,  1 }, { -1, 2,  1 } };
    const int32 F[6][4] = { {4,5,6,7}, {1,0,3,2}, {5,1,2,6}, {0,4,7,3}, {7,6,2,3}, {0,1,5,4} };
    const float32 N[6][3] = { {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0} };
    float32 L[3] = { 0.40f, 0.82f, 0.40f };
    { const float32 il = 1.f / mth::NkSqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]); L[0]*=il; L[1]*=il; L[2]*=il; }
    const NkColor base{ 118, 142, 204, 255 };
    for (int32 f = 0; f < 6; ++f) {
        float32 cx = 0, cyc = 0, cz = 0;
        for (int32 k = 0; k < 4; ++k) { cx += V[F[f][k]][0]; cyc += V[F[f][k]][1]; cz += V[F[f][k]][2]; }
        cx *= 0.25f; cyc *= 0.25f; cz *= 0.25f;
        const float32 vex = ex - cx, vey = ey - cyc, vez = ez - cz;
        if (N[f][0]*vex + N[f][1]*vey + N[f][2]*vez <= 0.f) continue;         // back-face cull
        float32 nd = N[f][0]*L[0] + N[f][1]*L[1] + N[f][2]*L[2]; if (nd < 0.f) nd = 0.f;
        const float32 sh = 0.35f + 0.65f * nd;
        const NkColor col{ static_cast<uint8>(base.r * sh), static_cast<uint8>(base.g * sh),
                           static_cast<uint8>(base.b * sh), 255 };
        NkVec2 p[4]; bool ok = true;
        for (int32 k = 0; k < 4; ++k)
            if (!project(V[F[f][k]][0], V[F[f][k]][1], V[F[f][k]][2], p[k])) { ok = false; break; }
        if (!ok) continue;
        dl.AddTriangleFilled(p[0], p[1], p[2], col);
        dl.AddTriangleFilled(p[0], p[2], p[3], col);
        for (int32 k = 0; k < 4; ++k) dl.AddLine(p[k], p[(k + 1) % 4], NkColor{ 36, 40, 56, 255 }, 1.f);
    }

    // Gizmo d'axes a l'origine (X rouge, Y vert, Z bleu) — clippe au near.
    drawLine3D(0, 0, 0, 1.6f, 0, 0, NkColor{ 232, 80, 80, 255 }, 2.f);
    drawLine3D(0, 0, 0, 0, 1.6f, 0, NkColor{ 96, 210, 96, 255 }, 2.f);
    drawLine3D(0, 0, 0, 0, 0, 1.6f, NkColor{ 88, 140, 240, 255 }, 2.f);
    dl.PopClipRect();

    if (ctx.font && ctx.font->Valid())
        dl.AddText(ctx.font->Face(), ctx.font->TexId(), { area.x + 10.f, area.y + 8.f + ctx.font->Ascent() },
                   "NKViewportDemo  -  glisser: orbite  -  molette: zoom", NkColor{ 170, 170, 175, 255 });
}

int nkmain(const NkEntryState& state) {
    (void)state;

    NkWindow window;
    NkWindowConfig cfg;
    cfg.title     = "NKViewportDemo - Viewport 3D";
    cfg.width     = 1000;
    cfg.height    = 720;
    cfg.centered  = true;
    cfg.resizable = true;
    if (!window.Create(cfg)) return -1;

    NkContextDesc desc;
    desc.api = NkGraphicsApi::NK_GFX_API_AUTO;
    if (desc.api == NkGraphicsApi::NK_GFX_API_AUTO) {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        desc.api = NkGraphicsApi::NK_GFX_API_DX11;
#else
        desc.api = NkGraphicsApi::NK_GFX_API_OPENGL;
#endif
    }
    auto target = memory::NkMakeUnique<NkRenderWindow>(window, desc);
    if (!target || !target->IsValid()) return -1;

    auto ctxPtr = memory::NkMakeUnique<NkGuiContext>();
    if (!ctxPtr) return -1;
    NkGuiContext& ctx = *ctxPtr;
    ctx.Init(static_cast<int32>(cfg.width), static_cast<int32>(cfg.height));
    SetCurrentContext(&ctx);

    NkGuiCanvasBackend backend;
    if (!backend.Init(target->GetRenderer())) return -1;

    auto fontPtr = memory::NkMakeUnique<NkGuiFont>();
    if (!fontPtr->LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f))
        fontPtr->LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f);
    ctx.font = fontPtr.Get();
    if (fontPtr->Valid())
        backend.UploadFontGray8(fontPtr->TexId(), fontPtr->pixels, fontPtr->atlasW, fontPtr->atlasH);

    // ── Etat camera + glue d'entree (souris) ─────────────────────────────────
    float32 yaw = 0.8f, pitch = 0.5f, dist = 9.f;
    bool    dragging = false;
    NkVec2  prevMouse{ 0.f, 0.f };

    auto& events = NkEvents();
    bool running = true;
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        ctx.input.mousePos = { static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY()) };
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.mouseDown[0] = true;
    });
    events.AddEventCallback<NkMouseButtonReleaseEvent>([&](NkMouseButtonReleaseEvent* e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) ctx.input.mouseDown[0] = false;
    });
    events.AddEventCallback<NkMouseWheelVerticalEvent>([&](NkMouseWheelVerticalEvent* e) {
        ctx.input.wheel += static_cast<float32>(e->GetDeltaY());
    });

    NkClock clock;
    uint32  lastW = 0, lastH = 0;
    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt <= 0.f) dt = 1.f / 60.f;
        if (dt > 0.1f) dt = 0.1f;

        while (NkEvent* ev = NkEvents().PollEvent()) { (void)ev; }
        if (!running) break;

        const math::NkVec2u wsz = target->GetWindow().GetSize();
        if (wsz.x > 0 && wsz.y > 0 && (wsz.x != lastW || wsz.y != lastH)) {
            target->OnResize(wsz.x, wsz.y); lastW = wsz.x; lastH = wsz.y;
        }
        const math::NkVec2u sz = target->GetSize();
        if (sz.x > 0 && sz.y > 0) { ctx.viewW = static_cast<int32>(sz.x); ctx.viewH = static_cast<int32>(sz.y); }

        ctx.BeginFrame(dt);
        const float32 W = static_cast<float32>(ctx.viewW), H = static_cast<float32>(ctx.viewH);

        // Orbite (delta souris) + zoom (molette). Sens naturel (non inverse).
        const NkVec2 mp = ctx.input.mousePos;
        if (ctx.input.mouseDown[0]) {
            if (dragging) { yaw += (mp.x - prevMouse.x) * 0.01f; pitch -= (mp.y - prevMouse.y) * 0.01f; }
            dragging = true;
        } else dragging = false;
        prevMouse = mp;
        const float32 lim = 1.5f; if (pitch > lim) pitch = lim; if (pitch < -lim) pitch = -lim;
        if (ctx.input.wheel != 0.f) {
            dist *= (ctx.input.wheel > 0.f) ? 0.88f : 1.13f;   // plage large : tout pres -> tres loin
            if (dist < 1.5f) dist = 1.5f; if (dist > 120.f) dist = 120.f;
            ctx.input.wheel = 0.f;
        }

        DrawScene3D(ctx, { 0.f, 0.f, W, H }, yaw, pitch, dist);

        ctx.EndFrame();

        target->Clear();
        backend.Submit(ctx.dl, sz.x, sz.y);
        backend.Submit(ctx.dlOverlay, sz.x, sz.y);
        target->Display();
    }
    return 0;
}
