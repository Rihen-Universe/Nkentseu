// =============================================================================
// NkDrawableDemo.cpp — Classe dessinable CUSTOM (style SFML)
// -----------------------------------------------------------------------------
// Montre comment l'utilisateur cree son PROPRE objet dessinable : on herite de
// NkTransformable (position/rotation/scale/origin) + NkDrawable (override de
// Draw(target, states)). L'objet construit ses vertices (NkVertexArray) avec une
// couleur par sommet, puis se soumet via target.Draw(vertexArray, states) en
// composant son transform local avec celui du parent — exactement comme
// sf::Drawable + sf::Transformable. Aucun NKUI.
//
//   NkDrawableDemo.exe --backend=dx11   (vk / dx11 / dx12 / sw / opengl)
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

#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkGraphicsApi.h"
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"   // NkRenderWindow : public NkRenderTarget
#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKCanvas/Renderer/Core/NkVertexArray.h"
#include "NKCanvas/Renderer/Core/NkRenderStates.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkDrawable Demo";
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

// =============================================================================
// NkStar — objet dessinable CUSTOM, exactement comme on ferait avec SFML :
//   class MyShape : public sf::Drawable, public sf::Transformable { ... };
// L'etoile construit ses triangles (fan depuis le centre) avec une couleur
// degradee pointe/creux, et se dessine en composant son transform avec le parent.
// =============================================================================
class NkStar : public NkTransformable, public NkDrawable {
    public:
        NkStar() : mVerts(NkPrimitiveType::NK_TRIANGLES) {}

        void Build(float32 outerR, float32 innerR, int32 spikes,
                   const NkColor2D& tip, const NkColor2D& valley) {
            mVerts.SetPrimitiveType(NkPrimitiveType::NK_TRIANGLES);
            mVerts.Clear();
            const int32   ring = spikes * 2;
            const float32 step = 6.2831853f / static_cast<float32>(ring);
            // Centre : moyenne des deux teintes, opaque.
            const NkVertex center{ 0.f, 0.f, 0.f, 0.f,
                static_cast<uint8>((tip.r + valley.r) / 2),
                static_cast<uint8>((tip.g + valley.g) / 2),
                static_cast<uint8>((tip.b + valley.b) / 2), 255 };
            for (int32 i = 0; i < ring; ++i) {
                const float32 a0 = static_cast<float32>(i)     * step - 1.5707963f; // pointe vers le haut
                const float32 a1 = static_cast<float32>(i + 1) * step - 1.5707963f;
                const float32 r0 = (i % 2 == 0) ? outerR : innerR;
                const float32 r1 = ((i + 1) % 2 == 0) ? outerR : innerR;
                const NkColor2D& c0 = (i % 2 == 0) ? tip : valley;
                const NkColor2D& c1 = ((i + 1) % 2 == 0) ? tip : valley;
                const NkVertex v0{ r0 * math::NkCos(a0), r0 * math::NkSin(a0), 0.f, 0.f,
                                   c0.r, c0.g, c0.b, c0.a };
                const NkVertex v1{ r1 * math::NkCos(a1), r1 * math::NkSin(a1), 0.f, 0.f,
                                   c1.r, c1.g, c1.b, c1.a };
                mVerts.Append(center);
                mVerts.Append(v0);
                mVerts.Append(v1);
            }
        }

        // L'override NkDrawable : compose le transform local avec le parent, puis
        // soumet le vertex array. Strictement le pattern sf::Drawable::draw.
        void Draw(NkRenderTarget& target, const NkRenderStates& states) const override {
            NkRenderStates local = states;
            local.transform *= GetTransform();   // parent * local (NkTransformable)
            target.Draw(mVerts, local);
        }

    private:
        NkVertexArray mVerts;
};

int nkmain(const NkEntryState& state) {
    NkWindowConfig cfg;
    cfg.title     = "NkDrawable Demo (NkTransformable + NkDrawable, style SFML)";
    cfg.width     = 900;
    cfg.height    = 600;
    cfg.centered  = true;
    cfg.resizable = true;
    NkWindow window;
    if (!window.Create(cfg)) { logger.Error("[nkdrawable] window failed"); return -1; }

    NkContextDesc desc;
    desc.api = ParseBackend(state.args);
    NkRenderWindow target(window, desc);
    if (!target.IsValid()) { logger.Error("[nkdrawable] NkRenderWindow init FAILED"); window.Close(); return -2; }
    logger.Infof("[nkdrawable] backend = %s", NkGraphicsApiName(desc.api));

    // ── Quelques etoiles custom, chacune avec sa taille / couleur ───────────────
    static const int32 N = 5;
    NkStar stars[N];
    static const NkColor2D tips[N] = {
        {255,200,80,255}, {120,210,255,255}, {255,120,150,255}, {150,255,170,255}, {220,160,255,255}
    };
    static const NkColor2D valleys[N] = {
        {200,90,40,255}, {40,90,160,255}, {150,40,80,255}, {40,140,90,255}, {120,70,160,255}
    };
    for (int32 i = 0; i < N; ++i) {
        const int32   spikes = 5 + i;
        const float32 outer  = 70.f - static_cast<float32>(i) * 6.f;
        stars[i].Build(outer, outer * 0.45f, spikes, tips[i], valleys[i]);
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
        const float32 orbit = 0.30f * (W < H ? W : H);

        // Transform (Transformable) : position en orbite + rotation propre + pulsation.
        for (int32 i = 0; i < N; ++i) {
            const float32 a = t * 0.5f + static_cast<float32>(i) * (6.2831853f / N);
            stars[i].SetPosition(cx + orbit * math::NkCos(a), cy + orbit * math::NkSin(a));
            stars[i].SetRotation(t * (0.8f + 0.2f * static_cast<float32>(i)));   // radians
            const float32 s = 0.9f + 0.25f * math::NkSin(t * 1.5f + static_cast<float32>(i));
            stars[i].SetScale(s, s);
        }

        // ── Rendu SFML-like : target.Draw(drawable) ─────────────────────────────
        target.Clear(NkColor2D{ 14, 16, 24, 255 });
        for (int32 i = 0; i < N; ++i)
            target.Draw(stars[i]);   // appelle NkStar::Draw(target, states)
        target.Display();
    }

    logger.Info("[nkdrawable] exit propre");
    window.Close();
    return 0;
}