#pragma once
// =============================================================================
// NkRectangleShape.h — Rectangle 2D (style SFML)
//
// 4 sommets (top-left, top-right, bottom-right, bottom-left) en coords locales,
// dimensionne par SetSize(NkVec2f). Combine avec NkTransformable : la position
// finale = position du transform + origin (typiquement (0,0) ou centre).
//
//   NkRectangleShape r({100, 50});
//   r.SetPosition({200, 200});
//   r.SetFillColor({255, 128, 0, 255});
//   r.SetOutlineColor(NkColor2D::White);
//   r.SetOutlineThickness(2.f);
//   target.Draw(r);
// =============================================================================

#include "NkShape.h"

namespace nkentseu {
    namespace renderer {

        class NkRectangleShape : public NkShape {
            public:
                NkRectangleShape() noexcept = default;
                explicit NkRectangleShape(NkVec2f size) noexcept : mSize(size) {}

                void    SetSize(NkVec2f size) noexcept { mSize = size; }
                NkVec2f GetSize() const noexcept       { return mSize; }

                uint32 GetPointCount() const override { return 4; }

                NkVec2f GetPoint(uint32 index) const override {
                    switch (index) {
                        case 0: return {0.f,     0.f};
                        case 1: return {mSize.x, 0.f};
                        case 2: return {mSize.x, mSize.y};
                        case 3: return {0.f,     mSize.y};
                        default: return {0.f, 0.f};
                    }
                }

            private:
                NkVec2f mSize{0.f, 0.f};
        };

    } // namespace renderer
} // namespace nkentseu
