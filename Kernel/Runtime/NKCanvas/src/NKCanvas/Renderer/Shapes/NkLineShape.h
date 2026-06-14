#pragma once
// =============================================================================
// NkLineShape.h — Segment de droite epais represente comme un quad (style SFML)
//
// Equivalent fonctionnel d'un sprite ligne « thickline » : segment AB d'une
// certaine epaisseur, transforme en rectangle dont l'axe long suit AB.
//
// USAGE
//   NkLineShape ln({100, 100}, {300, 200}, 4.f);
//   ln.SetFillColor(NkColor2D::Yellow);
//   target.Draw(ln);
//
// IMPLEMENTATION
//   GetPointCount() = 4. Les 4 coins du quad sont calcules a partir du vecteur
//   normal a AB (perpendiculaire) etire de thickness/2. La rotation/scale du
//   transform de NkTransformable s'applique en plus comme d'habitude (utile
//   pour animer le segment apres sa creation).
//
// REMARQUE
//   Pour un trait fin sans epaisseur (1 pixel), utiliser NkRenderer2D::DrawLine
//   directement — plus efficient (1 segment LINE vs 4 vertices TRIANGLE_FAN).
// =============================================================================

#include "NkShape.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        class NkLineShape : public NkShape {
            public:
                NkLineShape() noexcept = default;
                NkLineShape(NkVec2f a, NkVec2f b, float32 thickness = 1.f) noexcept
                    : mA(a), mB(b), mThickness(thickness) { Recompute(); }

                void SetA(NkVec2f a)              noexcept { mA = a; Recompute(); }
                void SetB(NkVec2f b)              noexcept { mB = b; Recompute(); }
                void SetEndpoints(NkVec2f a, NkVec2f b) noexcept { mA = a; mB = b; Recompute(); }
                void SetThickness(float32 t)      noexcept { mThickness = t; Recompute(); }
                NkVec2f GetA()         const noexcept { return mA; }
                NkVec2f GetB()         const noexcept { return mB; }
                float32 GetThickness() const noexcept { return mThickness; }

                uint32  GetPointCount() const override { return 4; }
                NkVec2f GetPoint(uint32 index) const override {
                    if (index >= 4) return {0.f, 0.f};
                    return mCorners[index];
                }

            private:
                NkVec2f mA{0.f, 0.f};
                NkVec2f mB{0.f, 0.f};
                float32 mThickness{1.f};
                NkVec2f mCorners[4]{}; ///< quad : (A-n, B-n, B+n, A+n) ou n = normale*thickness/2

                void Recompute() noexcept {
                    const NkVec2f dir{mB.x - mA.x, mB.y - mA.y};
                    const float32 len2 = dir.x * dir.x + dir.y * dir.y;
                    if (len2 <= 0.f) {
                        // Degenere (A == B) : place 4 coins identiques pour eviter NaN.
                        mCorners[0] = mCorners[1] = mCorners[2] = mCorners[3] = mA;
                        return;
                    }
                    const float32 invLen = 1.f / math::NkSqrt(len2);
                    // Normale unitaire (rotation 90 degres du dir).
                    const float32 nx = -dir.y * invLen;
                    const float32 ny =  dir.x * invLen;
                    const float32 half = mThickness * 0.5f;
                    const NkVec2f off{nx * half, ny * half};
                    mCorners[0] = {mA.x - off.x, mA.y - off.y};
                    mCorners[1] = {mB.x - off.x, mB.y - off.y};
                    mCorners[2] = {mB.x + off.x, mB.y + off.y};
                    mCorners[3] = {mA.x + off.x, mA.y + off.y};
                }
        };

    } // namespace renderer
} // namespace nkentseu
