// =============================================================================
// Games/Common/MouBackground.cpp
// =============================================================================
#include "Games/Common/MouBackground.h"
#include "Games/Common/MouFrame.h"
#include "UI/MouUIColor.h"
#include <cmath>

namespace mou {

    using namespace nkentseu;

    namespace {
        struct Theme {
            math::NkColor sky, ground, groundEdge, sun;
            bool night;   // true = soir (étoiles au lieu du soleil)
        };
        Theme ThemeOf(int32 t) noexcept {
            switch (((t % MouBackground::ThemeCount) + MouBackground::ThemeCount) % MouBackground::ThemeCount) {
                case 0:  // La cour (jour)
                    return { math::NkColor{205,236,255,255}, math::NkColor{150,210,120,255},
                             math::NkColor{120,185,95,255},  math::NkColor{255,221,99,255}, false };
                case 1:  // Le marché (chaud)
                    return { math::NkColor{255,239,210,255}, math::NkColor{226,196,140,255},
                             math::NkColor{201,168,108,255}, math::NkColor{255,200,80,255}, false };
                default: // La fête (soir)
                    return { math::NkColor{222,212,247,255}, math::NkColor{183,151,212,255},
                             math::NkColor{150,120,180,255}, math::NkColor{255,255,255,255}, true };
            }
        }
        // Nuage = 3 bulles blanches.
        void Cloud(const MouFrame& f, float32 cx, float32 cy, float32 s) noexcept {
            const math::NkColor w{ 255, 255, 255, 230 };
            f.Circle(math::NkVec2f{ cx,            cy        }, s,        w);
            f.Circle(math::NkVec2f{ cx - s * 0.9f, cy + s*0.2f}, s*0.7f,  w);
            f.Circle(math::NkVec2f{ cx + s * 0.9f, cy + s*0.2f}, s*0.7f,  w);
            f.Rect  (cx - s * 1.4f, cy + s*0.1f, s * 2.8f, s, w, s);
        }
    }  // namespace

    void MouBackground::Draw(const MouFrame& f, int32 theme, float32 time) noexcept {
        const float32 W = f.width, H = f.height;
        const Theme th = ThemeOf(theme);

        // Ciel (plein écran).
        f.Rect(0.f, 0.f, W, H, th.sky);

        // Sol arrondi en bas (~38% de hauteur), bord plus foncé.
        const float32 groundY = H * 0.64f;
        f.Rect(-40.f, groundY + 10.f, W + 80.f, H, th.groundEdge, 90.f);
        f.Rect(-40.f, groundY,        W + 80.f, H, th.ground,     90.f);

        // Soleil (jour) ou petites étoiles (soir), coin haut-droit (évite la mascotte à gauche).
        if (!th.night) {
            const math::NkVec2f sun{ W - 130.f, 96.f };
            f.Circle(sun, 56.f, ui::WithAlpha(th.sun, 90));   // halo
            f.Circle(sun, 40.f, th.sun);
        } else {
            const math::NkColor st{ 255, 255, 255, 220 };
            f.Circle(math::NkVec2f{ W - 160.f,  80.f }, 5.f, st);
            f.Circle(math::NkVec2f{ W - 110.f, 130.f }, 4.f, st);
            f.Circle(math::NkVec2f{ W -  70.f,  70.f }, 6.f, st);
            f.Circle(math::NkVec2f{ W - 210.f, 110.f }, 4.f, st);
        }

        // Nuages qui dérivent doucement (modulo pour reboucler).
        const float32 drift = time * 12.f;
        float32 c1 = std::fmod(120.f + drift, W + 240.f) - 120.f;
        float32 c2 = std::fmod(W * 0.55f + drift * 0.7f, W + 240.f) - 120.f;
        Cloud(f, c1, 130.f, 26.f);
        Cloud(f, c2, 200.f, 20.f);
    }

}  // namespace mou
