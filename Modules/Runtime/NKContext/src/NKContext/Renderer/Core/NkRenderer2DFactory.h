#pragma once
// =============================================================================
// NkRenderer2DFactory.h — Creates a 2D renderer from an existing graphics context
//
// Usage:
//   NkIGraphicsContext* gfx = NkContextFactory::Create(window, desc);
//   NkIRenderer2D* r2d = NkRenderer2DFactory::Create(gfx);
//   if (!r2d) { /* unsupported API */ }
//
//   // Each frame:
//   r2d->Clear(NkColor2D::Black());
//   r2d->Begin();
//     r2d->DrawFilledRect({100,100,200,50}, NkColor2D::Red());
//     r2d->DrawSprite(sprite);
//     r2d->DrawText(label);
//   r2d->End();
// =============================================================================
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKCore/NkCGXDetect.h"

namespace nkentseu {

    class NkIGraphicsContext;

    namespace renderer {

        using NkGraphicsApi = platform::graphics::NkGraphicsApi;

        class NkRenderer2DFactory {
            public:
                // Creates and initializes a 2D renderer backed by the given graphics context.
                // The renderer shares the device/command queue — no new GPU device is created.
                // Caller owns the returned pointer (delete or wrap in NkUniquePtr).
                // Returns nullptr if the API is not supported or initialization fails.
                static NkIRenderer2D* Create(NkIGraphicsContext* ctx);

                // Convenience: create and wrap in a unique pointer.
                static NkRenderer2DPtr CreateUnique(NkIGraphicsContext* ctx);

                // Check if the given API has a 2D renderer implementation.
                static bool IsApiSupported(NkGraphicsApi api);
        };

    } // namespace renderer
} // namespace nkentseu