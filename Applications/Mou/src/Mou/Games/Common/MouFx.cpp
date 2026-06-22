// =============================================================================
// Games/Common/MouFx.cpp
// =============================================================================
#include "Games/Common/MouFx.h"
#include "Games/Common/MouFrame.h"
#include "UI/MouUIColor.h"
#include <cmath>

namespace mou {

    using namespace nkentseu;
    using C = ui::MouUIColor;

    float32 MouFx::Rnd() noexcept {
        mRng = mRng * 1664525u + 1013904223u;
        return (float32)((mRng >> 8) & 0xFFFFFF) / 16777216.f;
    }

    math::NkColor MouFx::PickWarm() noexcept {
        switch ((int32)(Rnd() * 6.f) % 6) {
            case 0: return C::CORAL();
            case 1: return C::SUNNY();
            case 2: return C::LEAF();
            case 3: return C::SKYB();
            case 4: return C::GRAPE();
            default: return C::ORANGE();
        }
    }

    void MouFx::Add(const P& p) noexcept {
        if (mCount < MAX) mP[mCount++] = p;
        else mP[(mRng) % MAX] = p;   // recycle si plein
    }

    void MouFx::Confetti(float32 x, float32 y, int32 count) noexcept {
        for (int32 i = 0; i < count; ++i) {
            const float32 ang = -1.5707963f + (Rnd() - 0.5f) * 2.4f;   // vers le haut, étalé
            const float32 spd = 220.f + Rnd() * 320.f;
            P p{};
            p.x = x; p.y = y;
            p.vx = std::cos((double)ang) * spd;
            p.vy = std::sin((double)ang) * spd;
            p.maxlife = p.life = 0.9f + Rnd() * 0.7f;
            p.size = 9.f + Rnd() * 9.f;
            p.rot = Rnd() * 6.28f; p.vrot = (Rnd() - 0.5f) * 16.f;
            p.col = PickWarm(); p.kind = 0;
            Add(p);
        }
    }

    void MouFx::Sparkle(float32 x, float32 y, int32 count) noexcept {
        for (int32 i = 0; i < count; ++i) {
            const float32 ang = Rnd() * 6.28f;
            const float32 spd = 120.f + Rnd() * 200.f;
            P p{};
            p.x = x; p.y = y;
            p.vx = std::cos((double)ang) * spd;
            p.vy = std::sin((double)ang) * spd;
            p.maxlife = p.life = 0.35f + Rnd() * 0.35f;
            p.size = 6.f + Rnd() * 6.f;
            p.col = (Rnd() < 0.5f) ? C::SUNNY() : math::NkColor{ 255, 255, 255, 255 };
            p.kind = 1;
            Add(p);
        }
    }

    void MouFx::Puff(float32 x, float32 y, int32 count) noexcept {
        for (int32 i = 0; i < count; ++i) {
            const float32 ang = Rnd() * 6.28f;
            const float32 spd = 60.f + Rnd() * 130.f;
            P p{};
            p.x = x; p.y = y;
            p.vx = std::cos((double)ang) * spd;
            p.vy = std::sin((double)ang) * spd;
            p.maxlife = p.life = 0.35f + Rnd() * 0.3f;
            p.size = 10.f + Rnd() * 10.f;
            p.col = (Rnd() < 0.5f) ? C::CORAL() : math::NkColor{ 170, 160, 150, 255 };
            p.kind = 2;
            Add(p);
        }
    }

    void MouFx::Rain(float32 w) noexcept {
        const int32 n = 70;
        for (int32 i = 0; i < n; ++i) {
            P p{};
            p.x = Rnd() * w; p.y = -20.f - Rnd() * 200.f;
            p.vx = (Rnd() - 0.5f) * 80.f;
            p.vy = 150.f + Rnd() * 220.f;
            p.maxlife = p.life = 1.8f + Rnd() * 1.2f;
            p.size = 9.f + Rnd() * 10.f;
            p.rot = Rnd() * 6.28f; p.vrot = (Rnd() - 0.5f) * 16.f;
            p.col = PickWarm(); p.kind = 0;
            Add(p);
        }
    }

    void MouFx::Update(float32 dt) noexcept {
        int32 w = 0;
        for (int32 i = 0; i < mCount; ++i) {
            P& p = mP[i];
            p.life -= dt;
            if (p.life <= 0.f) continue;   // mort -> compacté hors tableau
            p.x += p.vx * dt; p.y += p.vy * dt;
            if (p.kind == 0) p.vy += 1100.f * dt;   // gravité confettis
            else if (p.kind == 1) { p.vx *= 0.92f; p.vy *= 0.92f; }
            p.rot += p.vrot * dt;
            if (w != i) mP[w] = p;
            ++w;
        }
        mCount = w;
    }

    void MouFx::Render(const MouFrame& frame) const noexcept {
        for (int32 i = 0; i < mCount; ++i) {
            const P& p = mP[i];
            float32 a = p.life / p.maxlife; if (a < 0.f) a = 0.f; if (a > 1.f) a = 1.f;
            const uint8 av = (uint8)(255.f * a);
            if (p.kind == 0) {
                // confetti : carré qui "tourne" (largeur = size*|cos(rot)|)
                const float32 cw = p.size * (0.25f + 0.75f * std::fabs((float32)std::cos((double)p.rot)));
                const float32 ch = p.size;
                math::NkColor c = p.col; c.a = av;
                frame.Rect(p.x - cw * 0.5f, p.y - ch * 0.5f, cw, ch, c, 2.f);
            } else if (p.kind == 1) {
                // étincelle : petit cercle qui rétrécit
                math::NkColor c = p.col; c.a = av;
                frame.Circle(math::NkVec2f{ p.x, p.y }, p.size * a * 0.6f + 1.f, c);
            } else {
                // puff : anneau qui grossit et s'efface
                const float32 t = 1.f - a;
                math::NkColor c = p.col; c.a = (uint8)(av * 0.6f);
                frame.CircleOutline(math::NkVec2f{ p.x, p.y }, p.size * (1.f + t * 1.8f), c, 3.f);
            }
        }
    }

}  // namespace mou
