#pragma once
// =============================================================================
// NkConvexShape.h — Polygone convexe arbitraire (style SFML)
//
// Permet de definir un polygone convexe avec un nombre quelconque de sommets
// fournis par l'utilisateur (3+). Pour les polygones concaves, il faut
// pre-trianguler cote utilisateur ou utiliser plusieurs NkConvexShape.
//
//   NkConvexShape pentagon;
//   pentagon.SetPointCount(5);
//   pentagon.SetPoint(0, { 50.f,   0.f});
//   pentagon.SetPoint(1, {100.f,  35.f});
//   pentagon.SetPoint(2, { 80.f,  90.f});
//   pentagon.SetPoint(3, { 20.f,  90.f});
//   pentagon.SetPoint(4, {  0.f,  35.f});
//   pentagon.SetFillColor(NkColor2D::Magenta);
//   target.Draw(pentagon);
//
// LIMITES
//   - NkShape::Draw triangule en TRIANGLE_FAN, valable UNIQUEMENT pour convexes.
//     Polygone concave -> rendu incorrect (auto-intersection visuelle des
//     triangles). Pour le concave, utiliser une vraie triangulation
//     (ear-clipping / Delaunay) cote app et passer le resultat via NkVertexArray.
// =============================================================================

#include "NkShape.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        class NkConvexShape : public NkShape {
            public:
                NkConvexShape() noexcept = default;
                explicit NkConvexShape(uint32 pointCount) { mPoints.Resize(pointCount); }

                void SetPointCount(uint32 n) { mPoints.Resize(n); }
                void SetPoint(uint32 index, NkVec2f p) {
                    if (index < mPoints.Size()) mPoints[index] = p;
                }

                uint32  GetPointCount() const override { return static_cast<uint32>(mPoints.Size()); }
                NkVec2f GetPoint(uint32 index) const override {
                    return (index < mPoints.Size()) ? mPoints[index] : NkVec2f{0.f, 0.f};
                }

            private:
                NkVector<NkVec2f> mPoints;
        };

    } // namespace renderer
} // namespace nkentseu
