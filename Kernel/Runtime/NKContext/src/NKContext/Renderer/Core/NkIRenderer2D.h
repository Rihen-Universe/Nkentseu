#pragma once
// =============================================================================
// NkIRenderer2D.h — Abstract 2D renderer interface (SFML-like)
//
// Usage pattern:
//   auto* renderer = NkRenderer2DFactory::Create(graphicsCtx);
//   renderer->Begin();
//       renderer->Draw(sprite);
//       renderer->Draw(text);
//       renderer->DrawRect(...);
//   renderer->End();
//
// All backends share this interface. Implementations live in:
//   Backends/Software/NkSoftwareRenderer2D
//   Backends/OpenGL/NkOpenGLRenderer2D
//   Backends/Vulkan/NkVulkanRenderer2D
//   Backends/DirectX/NkDX11Renderer2D
//   Backends/DirectX/NkDX12Renderer2D
// =============================================================================
#include "NkRenderer2DTypes.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkentseu {

    class NkIGraphicsContext;

    namespace renderer {

        // Forward declarations
        class NkTexture;
        class NkSprite;
        class NkText;

        // =========================================================================
        // NkIDrawable2D — anything that can be drawn by the renderer
        // =========================================================================
        class NkIDrawable2D {
            public:
                virtual ~NkIDrawable2D() = default;
                virtual void Draw(class NkIRenderer2D& renderer) const = 0;
        };

        // =========================================================================
        // NkIRenderer2D — main 2D renderer interface
        // =========================================================================
        class NkIRenderer2D {
            public:
                virtual ~NkIRenderer2D() = default;

                // ── Lifecycle ─────────────────────────────────────────────────────────
                // Initialize from an existing graphics context (shares device/queue).
                virtual bool Initialize(NkIGraphicsContext* ctx) = 0;
                virtual void Shutdown() = 0;
                virtual bool IsValid() const = 0;

                // ── Frame management ──────────────────────────────────────────────────
                // Call Begin() before any Draw calls, End() to flush and submit.
                // Internally manages batching — you don't need to flush manually.
                virtual bool Begin() = 0;
                virtual void End() = 0;

                // Explicit flush: submit current batch, reset accumulators.
                // Called automatically by End(). Call manually when switching views.
                virtual void Flush() = 0;

                // ── Clear ─────────────────────────────────────────────────────────────
                virtual void Clear(const NkColor2D& color = NkColor2D::Black()) = 0;

                // ── View (camera) ─────────────────────────────────────────────────────
                virtual void SetView(const NkView2D& view) = 0;
                virtual NkView2D GetView() const = 0;
                virtual NkView2D GetDefaultView() const = 0;

                // ── Viewport ──────────────────────────────────────────────────────────
                virtual void SetViewport(const NkRect2i& viewport) = 0;
                virtual NkRect2i GetViewport() const = 0;

                // ── Blend mode ────────────────────────────────────────────────────────
                virtual void SetBlendMode(NkBlendMode mode) = 0;
                virtual NkBlendMode GetBlendMode() const = 0;

                // ── Draw primitives ───────────────────────────────────────────────────

                // Drawables (sprites, text, shapes)
                virtual void Draw(const NkIDrawable2D& drawable) {
                    drawable.Draw(*this);
                }

                // Sprites (textured quads with transform)
                virtual void Draw(const NkSprite& sprite) = 0;

                // Text
                virtual void Draw(const NkText& text) = 0;

                // Geometry primitives
                virtual void DrawPoint(NkVec2f pos, const NkColor2D& color = NkColor2D::White(), float32 size = 1.f) = 0;

                virtual void DrawLine(NkVec2f a, NkVec2f b, const NkColor2D& color = NkColor2D::White(), float32 thickness = 1.f) = 0;

                virtual void DrawRect(NkRect2f rect, const NkColor2D& color = NkColor2D::White(), float32 outline = 0.f, const NkColor2D& outlineColor = NkColor2D::Black()) = 0;

                virtual void DrawFilledRect(NkRect2f rect, const NkColor2D& color = NkColor2D::White()) = 0;

                virtual void DrawCircle(NkVec2f center, float32 radius, const NkColor2D& color = NkColor2D::White(), uint32 segments = 32,
                                        float32 outline = 0.f, const NkColor2D& outlineColor = NkColor2D::Black()) = 0;

                virtual void DrawFilledCircle(NkVec2f center, float32 radius, const NkColor2D& color = NkColor2D::White(),
                                            uint32 segments = 32) = 0;

                virtual void DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D& color = NkColor2D::White(),
                                        float32 outline = 0.f, const NkColor2D& outlineColor = NkColor2D::Black()) = 0;

                virtual void DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D& color = NkColor2D::White()) = 0;

                // Custom vertex batch (advanced usage)
                virtual void DrawVertices(const NkVertex2D* vertices, uint32 vertexCount,
                                        const uint32* indices, uint32 indexCount, const NkTexture* texture = nullptr) = 0;

                // ── Stats ─────────────────────────────────────────────────────────────
                virtual NkRenderStats2D GetStats() const = 0;
                virtual void ResetStats() = 0;

                // ── Coordinate conversion ─────────────────────────────────────────────
                virtual NkVec2f MapPixelToCoords(NkVec2i pixel) const = 0;
                virtual NkVec2i MapCoordsToPixel(NkVec2f point) const = 0;
        };

        using NkRenderer2DPtr = memory::NkUniquePtr<NkIRenderer2D>;

    } // namespace renderer
} // namespace nkentseu