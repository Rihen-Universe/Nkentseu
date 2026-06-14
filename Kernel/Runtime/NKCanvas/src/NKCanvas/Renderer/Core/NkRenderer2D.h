#pragma once
// =============================================================================
// NkRenderer2D.h — Facade concrete user-facing pour le moteur 2D de NKCanvas
//
// ROLE
//   NkIRenderer2D est l'interface abstraite implementee par les 5 backends
//   (Software, OpenGL, Vulkan, DX11, DX12). C'est un detail d'implementation.
//   NkRenderer2D (cette classe) est la facade publique, concrete, que les
//   utilisateurs manipulent — c'est ce nom-la qui apparait dans le code
//   client (« preferer NkRenderer2D a NkIRenderer2D », decision archi 2026-05-29).
//
// HIERARCHIE
//   NkRenderer2D (concrete, facade)
//        │ wraps
//        ▼
//   NkIRenderer2D* (interface backend, non-owning)
//        │ implemente par
//        ▼
//   NkSoftwareRenderer2D / NkOpenGLRenderer2D / NkVulkanRenderer2D / NkDX11Renderer2D / NkDX12Renderer2D
//
// USAGE
//   NkRenderWindow target(window, desc);
//   NkRenderer2D& r = target.GetRenderer2D();    // facade exposee par le target
//
//   r.Clear(NkColor2D::Black);
//   r.DrawLine({10, 10}, {200, 50}, NkColor2D::Red, 2.f);
//   r.DrawFilledRect({50, 50, 100, 100}, NkColor2D::Blue);
//   r.Draw(sprite);                              // NkDrawable nouveau style
//   r.Draw(spriteOld);                           // NkIDrawable2D compat
//
// PROPRIETE
//   NkRenderer2D ne POSSEDE pas son backend NkIRenderer2D* — le NkRenderTarget
//   qui le contient est proprietaire (typiquement via memory::NkUniquePtr).
//   Le NkRenderer2D est juste une projection. La duree de vie suit celle du
//   NkRenderTarget englobant.
//
// EVOLUTION
//   La facade ajoute par-dessus NkIRenderer2D :
//     - Draw(NkDrawable&, NkRenderStates) — nouveau pattern target.Draw
//     - Acces a NkRenderTarget* pour les drawables qui ont besoin d'y
//       repasser (ex. composition parent-enfant)
//   Aucun cout de virtualisation supplementaire : tous les appels sont
//   forwardes inline a mBackend.
// =============================================================================

#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"   // delegation : on a besoin du type complet
#include "NKCanvas/Renderer/Core/NkRenderStates.h"
#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"  // surcharge DrawTexturedRect en pixels (GetTexCoords)

namespace nkentseu {
    namespace renderer {

        class NkRenderTarget;
        class NkVertexArray;

        class NkRenderer2D {
            public:
                /// Construit la facade dans un etat invalide (backend/target nuls).
                /// Utile pour la composition en valeur dans NkRenderTarget concret
                /// (Bind() ensuite quand le backend est cree).
                NkRenderer2D() noexcept = default;

                /// Construit la facade sur un backend existant + son target.
                /// Aucun des deux pointeurs n'est possede ; ils doivent rester
                /// vivants au moins le temps de vie de cette NkRenderer2D
                /// (typiquement : NkRenderer2D vit a l'interieur d'un
                /// NkRenderTarget qui possede son backend).
                NkRenderer2D(NkIRenderer2D* backend, NkRenderTarget* target) noexcept
                    : mBackend(backend), mTarget(target) {}

                /// Rebind a posteriori (utile pour les NkRenderTarget dont le
                /// backend est cree en plusieurs etapes dans le constructeur).
                void Bind(NkIRenderer2D* backend, NkRenderTarget* target) noexcept {
                    mBackend = backend;
                    mTarget  = target;
                }

                // ── Etat / validite ─────────────────────────────────────────────
                bool IsValid() const noexcept { return mBackend && mBackend->IsValid(); }

                // ── Frame management (forwardes au backend) ────────────────────
                bool Begin()              { return mBackend ? mBackend->Begin() : false; }
                void End()                { if (mBackend) mBackend->End(); }
                void Flush()              { if (mBackend) mBackend->Flush(); }
                void Clear(const NkColor2D& c = NkColor2D::Black) { if (mBackend) mBackend->Clear(c); }

                // ── View / Viewport ────────────────────────────────────────────
                void     SetView(const NkView2D& v) { if (mBackend) mBackend->SetView(v); }
                NkView2D GetView()        const     { return mBackend ? mBackend->GetView()        : NkView2D{}; }
                NkView2D GetDefaultView() const     { return mBackend ? mBackend->GetDefaultView() : NkView2D{}; }
                void     SetViewport(NkRect2i v)    { if (mBackend) mBackend->SetViewport(v); }
                NkRect2i GetViewport() const        { return mBackend ? mBackend->GetViewport() : NkRect2i{}; }

                // ── Clip / Scissor (pixels, origine haut-gauche) ───────────────
                void     SetClip(const NkRect2i& r) { if (mBackend) mBackend->SetClip(r); }
                void     PopClip()                  { if (mBackend) mBackend->PopClip(); }
                void     ResetClip()                { if (mBackend) mBackend->ResetClip(); }
                bool     HasClip() const            { return mBackend ? mBackend->HasClip() : false; }
                NkRect2i GetClip() const            { return mBackend ? mBackend->GetClip() : NkRect2i{}; }

                // ── Blend mode ─────────────────────────────────────────────────
                void        SetBlendMode(NkBlendMode m) { if (mBackend) mBackend->SetBlendMode(m); }
                NkBlendMode GetBlendMode() const        { return mBackend ? mBackend->GetBlendMode() : NkBlendMode::NK_ALPHA; }

                // ── Draw — nouveau pattern NkDrawable ──────────────────────────

                /// Dessine un NkDrawable avec un etat compose, en utilisant le
                /// NkRenderTarget englobant comme target effectif.
                void Draw(const NkDrawable& drawable,
                          const NkRenderStates& states = NkRenderStates::Default()) {
                    if (mTarget) drawable.Draw(*mTarget, states);
                }

                // ── Draw — compat ancien NkIDrawable2D ─────────────────────────

                void Draw(const NkIDrawable2D& drawable) {
                    if (mBackend) mBackend->Draw(drawable);
                }

                // ── Draw — convenience primitives (forwardes au backend) ───────

                void DrawPoint(NkVec2f pos, const NkColor2D& color = NkColor2D::White,
                               float32 size = 1.f) {
                    if (mBackend) mBackend->DrawPoint(pos, color, size);
                }

                void DrawLine(NkVec2f a, NkVec2f b,
                              const NkColor2D& color = NkColor2D::White,
                              float32 thickness = 1.f) {
                    if (mBackend) mBackend->DrawLine(a, b, color, thickness);
                }

                void DrawRect(NkRect2f rect,
                              const NkColor2D& color = NkColor2D::White,
                              float32 outline = 0.f,
                              const NkColor2D& outlineColor = NkColor2D::Black) {
                    if (mBackend) mBackend->DrawRect(rect, color, outline, outlineColor);
                }

                void DrawFilledRect(NkRect2f rect, const NkColor2D& color = NkColor2D::White) {
                    if (mBackend) mBackend->DrawFilledRect(rect, color);
                }

                void DrawCircle(NkVec2f center, float32 radius,
                                const NkColor2D& color = NkColor2D::White,
                                uint32 segments = 32,
                                float32 outline = 0.f,
                                const NkColor2D& outlineColor = NkColor2D::Black) {
                    if (mBackend) mBackend->DrawCircle(center, radius, color, segments, outline, outlineColor);
                }

                void DrawFilledCircle(NkVec2f center, float32 radius,
                                      const NkColor2D& color = NkColor2D::White,
                                      uint32 segments = 32) {
                    if (mBackend) mBackend->DrawFilledCircle(center, radius, color, segments);
                }

                void DrawTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                  const NkColor2D& color = NkColor2D::White,
                                  float32 outline = 0.f,
                                  const NkColor2D& outlineColor = NkColor2D::Black) {
                    if (mBackend) mBackend->DrawTriangle(a, b, c, color, outline, outlineColor);
                }

                void DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c,
                                        const NkColor2D& color = NkColor2D::White) {
                    if (mBackend) mBackend->DrawFilledTriangle(a, b, c, color);
                }

                /// Contour seul (sans remplissage) — style SFML.
                void DrawRectOutline(NkRect2f rect, const NkColor2D& color,
                                     float32 thickness = 1.f) {
                    if (mBackend) mBackend->DrawRectOutline(rect, color, thickness);
                }

                void DrawCircleOutline(NkVec2f center, float32 radius, const NkColor2D& color,
                                       float32 thickness = 1.f, uint32 segments = 32) {
                    if (mBackend) mBackend->DrawCircleOutline(center, radius, color, thickness, segments);
                }

                /// Submit raw vertex batch (advanced).
                void DrawVertices(const NkVertex* vertices, uint32 vertexCount,
                                  const uint32* indices, uint32 indexCount,
                                  const NkTexture* texture = nullptr) {
                    if (mBackend) mBackend->DrawVertices(vertices, vertexCount, indices, indexCount, texture);
                }

                /// Dessine une texture sur un rectangle (modulee par color, UV
                /// normalisee = texture entiere par defaut). Pratique sprites/UI.
                void DrawTexturedRect(NkRect2f rect, const NkTexture* texture,
                                      const NkColor2D& color = NkColor2D::White,
                                      NkRect2f uv = NkRect2f{ 0.f, 0.f, 1.f, 1.f }) {
                    if (mBackend) mBackend->DrawTexturedRect(rect, texture, color, uv);
                }

                /// Surcharge : sous-region exprimee en PIXELS (texels) de la
                /// texture. On normalise nous-meme en [0..1] via la taille de la
                /// texture (NkTexture::GetTexCoords) — pratique pour piocher dans
                /// un atlas/spritesheet sans calculer les ratios a la main.
                void DrawTexturedRect(NkRect2f rect, const NkTexture* texture,
                                      NkRect2i sourcePixels,
                                      const NkColor2D& color = NkColor2D::White) {
                    if (mBackend && texture) {
                        mBackend->DrawTexturedRect(rect, texture, color,
                                                   texture->GetTexCoords(sourcePixels));
                    }
                }

                // ── Sprite / Text (forwardes — anciens drawables) ──────────────
                void Draw(const NkSprite& sprite) { if (mBackend) mBackend->Draw(sprite); }
                void Draw(const NkText& text)    { if (mBackend) mBackend->Draw(text); }

                // ── Stats ──────────────────────────────────────────────────────
                NkRenderStats2D GetStats() const { return mBackend ? mBackend->GetStats() : NkRenderStats2D{}; }
                void            ResetStats()     { if (mBackend) mBackend->ResetStats(); }

                // ── Coord mapping ──────────────────────────────────────────────
                NkVec2f MapPixelToCoords(NkVec2i pixel) const {
                    return mBackend ? mBackend->MapPixelToCoords(pixel) : NkVec2f{0.f, 0.f};
                }
                NkVec2i MapCoordsToPixel(NkVec2f point) const {
                    return mBackend ? mBackend->MapCoordsToPixel(point) : NkVec2i{0, 0};
                }

                // ── Acces interne (advanced) ───────────────────────────────────
                NkIRenderer2D*  GetBackend()       noexcept { return mBackend; }
                const NkIRenderer2D* GetBackend() const noexcept { return mBackend; }
                NkRenderTarget* GetTarget()        noexcept { return mTarget; }
                const NkRenderTarget* GetTarget() const noexcept { return mTarget; }

            private:
                NkIRenderer2D*  mBackend{nullptr};  ///< non-owning
                NkRenderTarget* mTarget{nullptr};   ///< non-owning
        };

    } // namespace renderer
} // namespace nkentseu
