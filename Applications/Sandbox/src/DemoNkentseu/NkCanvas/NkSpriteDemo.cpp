// =============================================================================
// NkSpriteDemo.cpp — Demo SPRITES NKCanvas (style SFML)
// -----------------------------------------------------------------------------
// La texture est generee PROCEDURALEMENT via NkImage (dessin CPU : FillRect /
// FillCircle / DrawLine), uploadee en NkTexture, puis affichee par des NkSprite
// (SetPosition/Rotation/Scale/Origin/Color) qui orbitent et tournent.
// Aucun fichier image externe, aucun NKUI.
//
//   NkSpriteDemo.exe --backend=dx11   (vk / dx11 / dx12 / sw / opengl)
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NkColor.h"
#include "NKMath/NKMath.h"

#include "NKImage/Core/NkImage.h"

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Core/NkRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkSprite Demo";
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

// Construit une petite texture 64x64 "badge" via le dessin CPU de NkImage.
static void PaintBadge(NkImage& img) {
    using math::NkColor;
    img.Create(64, 64, NkColor{ 28, 30, 42, 255 }, 4);
    img.FillRect(3, 3, 58, 58, NkColor{ 64, 96, 180, 255 });
    img.DrawRect(3, 3, 58, 58, NkColor{ 150, 190, 255, 255 });
    img.FillCircle(32, 32, 20, NkColor{ 255, 200, 80, 255 });
    img.DrawCircle(32, 32, 20, NkColor{ 255, 255, 255, 255 });
    img.DrawLine(10, 10, 54, 54, NkColor{ 255, 90, 90, 255 });
    img.DrawLine(54, 10, 10, 54, NkColor{ 110, 255, 150, 255 });
}

int nkmain(const NkEntryState& state) {
    NkWindowConfig cfg;
    cfg.title     = "NkSprite Demo (texture procedurale NkImage + NkSprite)";
    cfg.width     = 900;
    cfg.height    = 600;
    cfg.centered  = true;
    cfg.resizable = true;
    NkWindow window;
    if (!window.Create(cfg)) { logger.Error("[nksprite] window failed"); return -1; }

    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) { logger.Error("[nksprite] NkRenderWindow init FAILED"); window.Close(); return -2; }
    logger.Infof("[nksprite] backend = %s", NkGraphicsApiName(desc.api));

    // ── Texture procedurale (NkImage -> NkTexture) ──────────────────────────────
    NkImage badge;
    PaintBadge(badge);
    NkTexture tex;
    if (!tex.LoadFromImage(*target.GetRenderer(), badge)) {
        logger.Error("[nksprite] LoadFromImage failed"); window.Close(); return -3;
    }

    // ── Sprites (origine au centre du badge pour tourner sur eux-memes) ─────────
    static const int32 N = 6;
    NkSprite sprites[N];
    static const NkColor2D tint[N] = {
        {255,255,255,255}, {255,180,180,255}, {180,255,200,255},
        {180,200,255,255}, {255,230,160,255}, {230,180,255,255}
    };
    for (int32 i = 0; i < N; ++i) {
        sprites[i].SetTexture(tex, true);
        sprites[i].SetOrigin(32.f, 32.f);   // centre du badge 64x64
        sprites[i].SetColor(tint[i]);
    }

    bool running = true;
    auto& events = NkEvents();
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });

    NkClock clock;
    uint32  lastW = 0, lastH = 0;
    float32 t = 0.f;

    while (running && window.IsOpen()) {
        float32 dt = clock.Tick().delta;
        if (dt > 0.1f) dt = 1.0f / 60.0f;
        t += dt;
        while (NkEvent* ev = events.PollEvent()) { (void)ev; }
        if (!running) break;

        const math::NkVec2u sz = target.GetSize();
        if (sz.x != lastW || sz.y != lastH) {
            if (lastW != 0 && sz.x > 0 && sz.y > 0) target.OnResize(sz.x, sz.y);
            lastW = sz.x; lastH = sz.y;
        }
        const float32 W = static_cast<float32>(sz.x), H = static_cast<float32>(sz.y);
        const float32 cx = W * 0.5f, cy = H * 0.5f;

        // Anime chaque sprite : orbite autour du centre + rotation + pulsation.
        const float32 orbit = 0.28f * (W < H ? W : H);
        for (int32 i = 0; i < N; ++i) {
            const float32 a = t * 0.6f + static_cast<float32>(i) * (6.2831853f / N);
            sprites[i].SetPosition(cx + orbit * math::NkCos(a), cy + orbit * math::NkSin(a));
            sprites[i].SetRotation((t * 90.f) + static_cast<float32>(i) * 30.f);   // degres
            const float32 s = 0.9f + 0.35f * math::NkSin(t * 2.f + static_cast<float32>(i));
            sprites[i].SetScale(s, s);
        }

        // ── Rendu : SFML-like -> target.Draw(sprite) (NkRenderWindow est un NkRenderTarget) ──
        target.Clear(NkColor2D{ 16, 18, 26, 255 });
        for (int32 i = 0; i < N; ++i)
            // NkSprite herite de NkIDrawable2D ET NkDrawable -> on choisit le chemin
            // NkDrawable (nouveau, SFML) pour lever l'ambiguite de surcharge.
            target.Draw(static_cast<const NkDrawable&>(sprites[i]));
        target.Display();
    }

    logger.Info("[nksprite] exit propre");
    window.Close();
    return 0;
}
