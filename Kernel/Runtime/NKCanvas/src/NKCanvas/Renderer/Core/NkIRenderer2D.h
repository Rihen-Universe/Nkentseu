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
                virtual void Clear(const NkColor2D& color = NkColor2D::Black) = 0;

                // ── View (camera) ─────────────────────────────────────────────────────
                virtual void SetView(const NkView2D& view) = 0;
                virtual NkView2D GetView() const = 0;
                virtual NkView2D GetDefaultView() const = 0;

                // ── Viewport ──────────────────────────────────────────────────────────
                virtual void SetViewport(const NkRect2i& viewport) = 0;
                virtual NkRect2i GetViewport() const = 0;

                // ── Resize (cible) ────────────────────────────────────────────────────
                // Notifie le renderer de la nouvelle taille de framebuffer. La VUE PAR
                // DEFAUT suit l'ecran (auto) ; une vue CUSTOM posee par SetView reste
                // intacte (resize uniquement si l'utilisateur le veut). Met aussi a jour
                // le viewport plein-cadre. Defaut vide pour les implementeurs sans vue.
                virtual void OnResize(uint32 width, uint32 height) noexcept { (void)width; (void)height; }

                // ── Clip / Scissor ────────────────────────────────────────────────────
                // Restreint le rendu a un rectangle (scissor test GPU), en pixels,
                // origine haut-gauche de la surface. Tout ce qui sort du rect est
                // ecarte. Utile pour les panneaux UI scrollables, le masquage, etc.
                //
                // Pile : SetClip empile le rect (intersecte avec le clip courant) ;
                // PopClip depile. ResetClip vide la pile (plus aucune restriction).
                // Implementation backend : glScissor (GL), VkRect2D dynamique
                // (Vulkan), RSSetScissorRects (DX11/12), clamp CPU (Software).
                //
                // Defaut : no-op (backend qui ne gere pas encore le clip rend tout).
                virtual void SetClip(const NkRect2i& rectPixels) { (void)rectPixels; }
                virtual void PopClip() {}
                virtual void ResetClip() {}
                virtual bool HasClip() const { return false; }
                virtual NkRect2i GetClip() const { return NkRect2i{}; }

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
                virtual void DrawPoint(NkVec2f pos, const NkColor2D& color = NkColor2D::White, float32 size = 1.f) = 0;

                virtual void DrawLine(NkVec2f a, NkVec2f b, const NkColor2D& color = NkColor2D::White, float32 thickness = 1.f) = 0;

                virtual void DrawRect(NkRect2f rect, const NkColor2D& color = NkColor2D::White, float32 outline = 0.f, const NkColor2D& outlineColor = NkColor2D::Black) = 0;

                virtual void DrawFilledRect(NkRect2f rect, const NkColor2D& color = NkColor2D::White) = 0;

                virtual void DrawCircle(NkVec2f center, float32 radius, const NkColor2D& color = NkColor2D::White, uint32 segments = 32,
                                        float32 outline = 0.f, const NkColor2D& outlineColor = NkColor2D::Black) = 0;

                virtual void DrawFilledCircle(NkVec2f center, float32 radius, const NkColor2D& color = NkColor2D::White,
                                            uint32 segments = 32) = 0;

                virtual void DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D& color = NkColor2D::White,
                                        float32 outline = 0.f, const NkColor2D& outlineColor = NkColor2D::Black) = 0;

                virtual void DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D& color = NkColor2D::White) = 0;

                // Contour seul (sans remplissage) — pratique style SFML. Impl par
                // defaut composee a partir des primitives existantes (aucun backend
                // a modifier). Geometrie alignee sur l'outline de DrawRect.
                virtual void DrawRectOutline(NkRect2f rect, const NkColor2D& color, float32 thickness = 1.f) {
                    if (thickness <= 0.f) return;
                    const float32 t = thickness;
                    DrawFilledRect({ rect.left - t,          rect.top - t,           rect.width + 2*t, t }, color); // haut
                    DrawFilledRect({ rect.left - t,          rect.top + rect.height, rect.width + 2*t, t }, color); // bas
                    DrawFilledRect({ rect.left - t,          rect.top,               t, rect.height },     color); // gauche
                    DrawFilledRect({ rect.left + rect.width, rect.top,               t, rect.height },     color); // droite
                }

                // Anneau (contour de cercle) via segments de ligne.
                virtual void DrawCircleOutline(NkVec2f center, float32 radius, const NkColor2D& color,
                                               float32 thickness = 1.f, uint32 segments = 32) {
                    if (thickness <= 0.f || segments < 3) return;
                    NkVec2f prev{ center.x + radius, center.y };
                    for (uint32 i = 1; i <= segments; ++i) {
                        const float32 a = (float32)i / (float32)segments * 6.28318530718f;
                        NkVec2f cur{ center.x + math::NkCos(a) * radius, center.y + math::NkSin(a) * radius };
                        DrawLine(prev, cur, color, thickness);
                        prev = cur;
                    }
                }

                // Custom vertex batch (advanced usage)
                virtual void DrawVertices(const NkVertex2D* vertices, uint32 vertexCount,
                                        const uint32* indices, uint32 indexCount, const NkTexture* texture = nullptr) = 0;

                // Dessine une texture sur un rectangle (style SFML), modulee par
                // color. uv = sous-region normalisee [0..1] (defaut = texture
                // entiere). Impl par defaut composee sur DrawVertices (aucun
                // backend a modifier). Pratique pour sprites/logos/icones UI.
                virtual void DrawTexturedRect(NkRect2f rect, const NkTexture* texture,
                                              const NkColor2D& color = NkColor2D::White,
                                              NkRect2f uv = NkRect2f{ 0.f, 0.f, 1.f, 1.f }) {
                    if (texture == nullptr) return;
                    const float32 x0 = rect.left,        y0 = rect.top;
                    const float32 x1 = rect.left + rect.width, y1 = rect.top + rect.height;
                    const float32 u0 = uv.left,          t0 = uv.top;
                    const float32 u1 = uv.left + uv.width, t1 = uv.top + uv.height;
                    NkVertex2D v[4];
                    for (int i = 0; i < 4; ++i) { v[i].r = color.r; v[i].g = color.g; v[i].b = color.b; v[i].a = color.a; }
                    v[0].x = x0; v[0].y = y0; v[0].u = u0; v[0].v = t0; // TL
                    v[1].x = x1; v[1].y = y0; v[1].u = u1; v[1].v = t0; // TR
                    v[2].x = x1; v[2].y = y1; v[2].u = u1; v[2].v = t1; // BR
                    v[3].x = x0; v[3].y = y1; v[3].u = u0; v[3].v = t1; // BL
                    const uint32 idx[6] = { 0, 1, 2, 0, 2, 3 };
                    DrawVertices(v, 4, idx, 6, texture);
                }

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