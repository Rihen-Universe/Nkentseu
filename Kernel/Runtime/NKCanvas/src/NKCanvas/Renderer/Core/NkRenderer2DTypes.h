#pragma once
// =============================================================================
// NkRenderer2DTypes.h — Shared types for the NKRenderer 2D system
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace renderer {

        // ── Color (RGBA, 0-255) ──────────────────────────────────────────────────
        using NkColor2D = math::NkColor;

        // ── 2D integer vector ────────────────────────────────────────────────────
        using NkVec2i = math::NkVec2i;

        // ── 2D float vector ──────────────────────────────────────────────────────
        using NkVec2f = math::NkVec2f;

        // ── Integer rectangle ────────────────────────────────────────────────────
        using NkRect2i = math::NkIntRect;

        // ── Float rectangle ──────────────────────────────────────────────────────
        using NkRect2f = math::NkFloatRect;

        // ── 2D transform (position, rotation in radians, scale, origin) ──────────
        struct NkTransform2D {
            NkVec2f position    = {0.f, 0.f};
            float32 rotation    = 0.f;         // radians
            NkVec2f scale       = {1.f, 1.f};
            NkVec2f origin      = {0.f, 0.f};  // pivot point (local coords)

            // Returns a 4x4 (column-major, but we store as float[16]) matrix
            // compatible with standard OpenGL/Vulkan/DX uniform layouts.
            void ToMatrix4(float32 out[16]) const;
        };

        // ── View (2D camera orthographic) ────────────────────────────────────────
        struct NkView2D {
            NkVec2f center   = {0.f, 0.f};   // center of view in world coords
            NkVec2f size     = {800.f, 600.f}; // visible area
            float32 rotation = 0.f;

            void ToProjectionMatrix(float32 out[16]) const;
        };

        // ── Blending mode ────────────────────────────────────────────────────────
        enum class NkBlendMode : uint8 {
            NK_ALPHA,      // standard alpha blending (premul src, 1-src_alpha dst)
            NK_ADD,        // additive (fire, glow effects)
            NK_MULTIPLY,   // multiply (shadows)
            NK_NONE,       // no blending (overwrite)
        };

        // ── Vertex for 2D rendering ──────────────────────────────────────────────
        struct NkVertex2D {
            float32 x, y;           // position
            float32 u, v;           // texture coords
            uint8   r, g, b, a;     // color
        };

        // ── Render stats (per frame) ─────────────────────────────────────────────
        struct NkRenderStats2D {
            uint32 drawCalls   = 0;
            uint32 vertexCount = 0;
            uint32 indexCount  = 0;
            uint32 textureSwap = 0;
        };

    } // namespace renderer
} // namespace nkentseu