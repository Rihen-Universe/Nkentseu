#pragma once
// =============================================================================
// NkCircleShape.h — Cercle 2D approxime par polygone regulier (style SFML)
//
// Genere `segments` sommets sur un cercle de rayon `radius` centre en (0, 0).
// Plus le nombre de segments est eleve, plus le cercle parait lisse (defaut 32).
//
//   NkCircleShape c(50.f);
//   c.SetPointCount(64);          // plus lisse
//   c.SetPosition({400, 300});
//   c.SetFillColor(NkColor2D::Cyan);
//   target.Draw(c);
// =============================================================================

#include "NkShape.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        class NkCircleShape : public NkShape {
            public:
                NkCircleShape() noexcept = default;
                explicit NkCircleShape(float32 radius, uint32 segments = 32) noexcept
                    : mRadius(radius), mSegments(segments) {}

                void    SetRadius(float32 r) noexcept { mRadius = r; }
                float32 GetRadius() const noexcept    { return mRadius; }

                /// Nombre de segments (vertices du polygone). 30+ pour visuellement
                /// lisse a taille moyenne ; 64 pour grand cercle.
                void   SetPointCount(uint32 n) noexcept { mSegments = n; }
                uint32 GetPointCount() const override   { return mSegments; }

                NkVec2f GetPoint(uint32 index) const override {
                    if (mSegments == 0) return {0.f, 0.f};
                    const float32 angle = (2.f * math::NK_PI_F * static_cast<float32>(index)) / static_cast<float32>(mSegments);
                    const float32 c = math::NkCos(angle);
                    const float32 s = math::NkSin(angle);
                    // Centre du cercle a (radius, radius) pour que la bbox locale
                    // commence a (0,0) — coherent avec NkRectangleShape (origin
                    // facile a regler avec SetOrigin({r, r}) pour centrer).
                    return {mRadius + mRadius * c, mRadius + mRadius * s};
                }

            private:
                float32 mRadius{0.f};
                uint32  mSegments{32};
        };

    } // namespace renderer
} // namespace nkentseu
