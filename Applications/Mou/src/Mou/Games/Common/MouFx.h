// =============================================================================
// Games/Common/MouFx.h
// Petit système de particules pour le "juice" (confettis, étincelles, puff).
// Rendu via MouFrame (primitives NKUI). Partagé par tous les jeux Mú.
// =============================================================================
#pragma once

#ifndef MOU_FX_H
#define MOU_FX_H

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace mou {

    struct MouFrame;

    class MouFx {
    public:
        void Clear() noexcept { mCount = 0; }

        void Confetti(nkentseu::float32 x, nkentseu::float32 y, nkentseu::int32 count) noexcept;
        void Sparkle (nkentseu::float32 x, nkentseu::float32 y, nkentseu::int32 count) noexcept;
        void Puff    (nkentseu::float32 x, nkentseu::float32 y, nkentseu::int32 count) noexcept;
        void Rain    (nkentseu::float32 w) noexcept;   // pluie de confettis (célébration)

        void Update(nkentseu::float32 dt) noexcept;
        void Render(const MouFrame& frame) const noexcept;

    private:
        static constexpr nkentseu::int32 MAX = 420;
        struct P {
            nkentseu::float32 x, y, vx, vy, life, maxlife, size, rot, vrot;
            nkentseu::math::NkColor col;
            nkentseu::int32 kind;   // 0=confetti 1=sparkle 2=puff
        };
        P mP[MAX];
        nkentseu::int32 mCount = 0;
        nkentseu::uint32 mRng = 1234567u;

        nkentseu::float32 Rnd() noexcept;
        nkentseu::math::NkColor PickWarm() noexcept;
        void Add(const P& p) noexcept;
    };

}  // namespace mou

#endif // MOU_FX_H
