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

        // ── Rectangles : noms canoniques domicilies dans NKMath, re-exportes ici ──
        // (cf. NKMath/NkRectangle.h — ce sont des types purement math). NkRect2i et
        // NkRect2f restent identiques a math::NkIntRect / NkFloatRect (meme NkRectT).
        using NkRect2i = math::NkRect2i;   // int32  — clip, viewport, UI
        using NkRect2f = math::NkRect2f;   // float32 — geometrie 2D
        using NkRect2u = math::NkRect2u;   // uint32
        using NkRect2d = math::NkRect2d;   // float64

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

        // ── Primitive type ───────────────────────────────────────────────────────
        // Type de primitive consommee par NkVertexArray et le backend.
        // Calque SFML : POINTS, LINES, LINE_STRIP, TRIANGLES, TRIANGLE_STRIP,
        // TRIANGLE_FAN. Pas de QUADS — les quads sont decomposes en 2 triangles
        // par le batcher (compatible cross-API : Vulkan/DX/Metal n'ont pas de
        // QUADS natif).
        enum class NkPrimitiveType : uint8 {
            NK_POINTS         = 0,
            NK_LINES          = 1,
            NK_LINE_STRIP     = 2,
            NK_TRIANGLES      = 3,
            NK_TRIANGLE_STRIP = 4,
            NK_TRIANGLE_FAN   = 5,
        };

        // ── Vertex for 2D rendering ──────────────────────────────────────────────
        struct NkVertex2D {
            float32 x, y;           // position
            float32 u, v;           // texture coords
            uint8   r, g, b, a;     // color
        };

        // Alias SFML-friendly : NkVertex (sans le « 2D ») pour les nouvelles APIs.
        // NkVertex2D est conserve comme nom canonique du POD pour la compat des
        // sous-systemes existants (NkIRenderer2D::DrawVertices, batchers, etc.).
        using NkVertex = NkVertex2D;

        // ── Render stats (per frame) ─────────────────────────────────────────────
        struct NkRenderStats2D {
            uint32 drawCalls   = 0;
            uint32 vertexCount = 0;
            uint32 indexCount  = 0;
            uint32 textureSwap = 0;
        };

    } // namespace renderer
} // namespace nkentseu