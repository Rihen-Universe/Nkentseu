#pragma once
// =============================================================================
// NkShape.h — Base abstraite pour les formes geometriques 2D (style SFML)
//
// Equivalent de sf::Shape. Toute forme (rectangle, cercle, polygone convexe,
// segment epais) herite de NkShape et expose juste GetPointCount/GetPoint.
// NkShape s'occupe :
//   - de l'heritage NkTransformable (position/rotation/scale/origin)
//   - de l'heritage NkDrawable (signature Draw(NkRenderTarget&, NkRenderStates))
//   - du remplissage : triangulation en fan (convex only) avec fillColor
//   - du contour (outline) en lignes paire-a-paire avec outlineColor
//   - de la texture optionnelle + textureRect (UV mapping)
//
// CONTRAT POUR LES SOUS-CLASSES
//   - GetPointCount() retourne le nombre de sommets du contour (>= 3 pour avoir
//     une aire, 2 pour un segment).
//   - GetPoint(i) retourne la position du i-eme sommet en coords LOCALES (avant
//     le transform). L'origine et la rotation sont appliquees via le transform
//     herite de NkTransformable.
//
// LIMITES (premiere passe)
//   - Triangulation en fan : valide UNIQUEMENT pour polygones convexes. Les
//     formes concaves doivent passer par NkConvexShape pre-triangule (a venir).
//   - Outline : utilise NK_LINES (paires consecutives). Pas de joints/caps
//     stylises (round/bevel) — c'est suffisant pour le pattern SFML simple.
//   - Pas encore d'antialiasing dedie ; depend du MSAA du contexte.
// =============================================================================

#include "NKCanvas/Renderer/Core/NkDrawable.h"
#include "NKCanvas/Renderer/Core/NkTransformable.h"
#include "NKCanvas/Renderer/Core/NkRenderStates.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"

namespace nkentseu {
    namespace renderer {

        class NkRenderTarget;
        class NkTexture;

        class NkShape : public NkTransformable, public NkDrawable {
            public:
                NkShape() noexcept = default;
                ~NkShape() noexcept override = default;

                // ── Couleurs / contour ─────────────────────────────────────────
                void SetFillColor(const NkColor2D& c) noexcept    { mFillColor = c; }
                NkColor2D GetFillColor() const noexcept           { return mFillColor; }

                void SetOutlineColor(const NkColor2D& c) noexcept { mOutlineColor = c; }
                NkColor2D GetOutlineColor() const noexcept        { return mOutlineColor; }

                void    SetOutlineThickness(float32 t) noexcept   { mOutlineThickness = t; }
                float32 GetOutlineThickness() const noexcept      { return mOutlineThickness; }

                // ── Texture ────────────────────────────────────────────────────
                void               SetTexture(const NkTexture* tex) noexcept { mTexture = tex; }
                const NkTexture*   GetTexture() const noexcept               { return mTexture; }

                void     SetTextureRect(const NkRect2f& r) noexcept { mTextureRect = r; mHasTextureRect = true; }
                NkRect2f GetTextureRect() const noexcept            { return mTextureRect; }

                // ── Points (a override par les sous-classes) ───────────────────
                virtual uint32  GetPointCount() const = 0;
                virtual NkVec2f GetPoint(uint32 index) const = 0;

                /// Retourne l'UV (texture coord) du point i, dans l'espace
                /// normalise [0..1]^2. Comportement par defaut :
                ///   - Si mTextureRect a ete defini par l'utilisateur, l'UV
                ///     est interpole entre les bornes du rect (en pixels
                ///     normalises 0..1 sur la texture).
                ///   - Sinon, l'UV est l'auto-mapping classique : point local
                ///     normalise sur la bounding box locale [0..1]^2.
                /// Les sous-classes peuvent override pour des mappings custom
                /// (ex. polaires, cylindriques, etc.).
                virtual NkVec2f GetPointUV(uint32 index, const NkRect2f& localBounds) const noexcept {
                    const NkVec2f p = GetPoint(index);
                    if (mHasTextureRect) {
                        // Map local point [bounds] -> [textureRect].
                        const float32 invW = localBounds.width  > 0.f ? 1.f / localBounds.width  : 0.f;
                        const float32 invH = localBounds.height > 0.f ? 1.f / localBounds.height : 0.f;
                        const float32 tx = (p.x - localBounds.left) * invW;
                        const float32 ty = (p.y - localBounds.top)  * invH;
                        return NkVec2f(mTextureRect.left + tx * mTextureRect.width,
                                       mTextureRect.top  + ty * mTextureRect.height);
                    }
                    const float32 invW = localBounds.width  > 0.f ? 1.f / localBounds.width  : 0.f;
                    const float32 invH = localBounds.height > 0.f ? 1.f / localBounds.height : 0.f;
                    return NkVec2f((p.x - localBounds.left) * invW,
                                   (p.y - localBounds.top)  * invH);
                }

                // ── Bounding box locale (avant transform) ──────────────────────
                NkRect2f GetLocalBounds() const noexcept {
                    const uint32 n = GetPointCount();
                    if (n == 0) return NkRect2f(0.f, 0.f, 0.f, 0.f);
                    NkVec2f p0 = GetPoint(0);
                    float32 minX = p0.x, maxX = p0.x;
                    float32 minY = p0.y, maxY = p0.y;
                    for (uint32 i = 1; i < n; ++i) {
                        const NkVec2f p = GetPoint(i);
                        if      (p.x < minX) minX = p.x;
                        else if (p.x > maxX) maxX = p.x;
                        if      (p.y < minY) minY = p.y;
                        else if (p.y > maxY) maxY = p.y;
                    }
                    return NkRect2f(minX, minY, maxX - minX, maxY - minY);
                }

                /// Bounding box dans l'espace monde apres application du transform.
                NkRect2f GetGlobalBounds() const noexcept {
                    return GetTransform().TransformRect(GetLocalBounds());
                }

                // ── Draw (impl dans .cpp pour le dispatch target) ──────────────
                void Draw(NkRenderTarget& target,
                          const NkRenderStates& states) const override;

            protected:
                NkColor2D        mFillColor{NkColor2D::White};
                NkColor2D        mOutlineColor{NkColor2D::Black};
                float32          mOutlineThickness{0.f};
                const NkTexture* mTexture{nullptr};
                NkRect2f         mTextureRect{};
                bool             mHasTextureRect{false};
        };

    } // namespace renderer
} // namespace nkentseu
