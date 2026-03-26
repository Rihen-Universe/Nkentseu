#pragma once
// =============================================================================
// NkSprite.h — Données d'un sprite 2D.
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKMath/NkRectangle.h"

namespace nkentseu {

    // =========================================================================
    // NkUVRect — sous-région d'une texture atlas [0,1]
    // =========================================================================
    struct NkUVRect {
        float32 u0=0, v0=0;  // coin haut-gauche
        float32 u1=1, v1=1;  // coin bas-droit

        static NkUVRect Full() { return {0,0,1,1}; }
        static NkUVRect FromPixels(float32 x, float32 y, float32 w, float32 h,
                                   float32 atlasW, float32 atlasH) noexcept {
            return {x/atlasW, y/atlasH, (x+w)/atlasW, (y+h)/atlasH};
        }
    };

    // =========================================================================
    // NkFlipFlags — retournement horizontal/vertical
    // =========================================================================
    enum class NkFlipFlags : uint8 {
        NK_NONE       = 0,
        NK_HORIZONTAL = 1 << 0,
        NK_VERTICAL   = 1 << 1,
        NK_BOTH       = NK_HORIZONTAL | NK_VERTICAL,
    };

    inline NkFlipFlags operator|(NkFlipFlags a, NkFlipFlags b) {
        return (NkFlipFlags)((uint8)a|(uint8)b);
    }
    inline bool NkHasFlip(NkFlipFlags flags, NkFlipFlags bit) {
        return ((uint8)flags & (uint8)bit) != 0;
    }

    // =========================================================================
    // NkSprite — données d'un sprite
    // =========================================================================
    struct NKRENDERER_API NkSprite {
        NkTextureRef texture;           ///< Texture source (atlas ou standalone)
        NkUVRect     uv        = NkUVRect::Full(); ///< Région dans la texture
        NkVec2       position  = {};    ///< Position monde 2D (coin haut-gauche ou centre selon pivot)
        NkVec2       size      = {64,64}; ///< Taille en pixels monde
        NkVec2       pivot     = {0.5f, 0.5f}; ///< Pivot normalisé [0,1] — 0.5/0.5 = centre
        float32      rotation  = 0.f;   ///< Rotation en degrés (autour du pivot)
        NkVec2       scale     = {1,1}; ///< Mise à l'échelle
        NkColor4f    tint      = {1,1,1,1}; ///< Teinte RGBA [0,1]
        NkFlipFlags  flip      = NkFlipFlags::NK_NONE;
        int32        sortOrder = 0;     ///< Ordre de rendu (plus grand = devant)
        bool         visible   = true;  ///< Sprite visible ou non

        // ── Helpers ──────────────────────────────────────────────────────────

        // Calcule les UV finaux en tenant compte du flip
        NkUVRect GetFinalUV() const noexcept {
            NkUVRect r = uv;
            if (NkHasFlip(flip, NkFlipFlags::NK_HORIZONTAL)) {
                float32 tmp = r.u0; r.u0 = r.u1; r.u1 = tmp;
            }
            if (NkHasFlip(flip, NkFlipFlags::NK_VERTICAL)) {
                float32 tmp = r.v0; r.v0 = r.v1; r.v1 = tmp;
            }
            return r;
        }

        // Retourne le rectangle monde (boîte englobante AABB avant rotation)
        NkVec2 GetWorldOrigin() const noexcept {
            return {
                position.x - pivot.x * size.x * scale.x,
                position.y - pivot.y * size.y * scale.y
            };
        }
    };

    // =========================================================================
    // NkAnimatedSprite — sprite avec atlas d'animation
    // =========================================================================
    struct NKRENDERER_API NkAnimatedSprite : NkSprite {
        uint32  frameCount   = 1;
        uint32  frameColumns = 1;   // nombre de colonnes dans l'atlas
        uint32  frameRows    = 1;
        uint32  currentFrame = 0;
        float32 fps          = 12.f;
        float32 elapsed      = 0.f;
        bool    loop         = true;
        bool    playing      = true;

        void Advance(float32 dt) noexcept {
            if (!playing) return;
            elapsed += dt;
            float32 frameDuration = 1.f / fps;
            while (elapsed >= frameDuration) {
                elapsed -= frameDuration;
                ++currentFrame;
                if (currentFrame >= frameCount) {
                    currentFrame = loop ? 0 : frameCount - 1;
                    if (!loop) playing = false;
                }
            }
            // Recalcul UV
            uint32 col = currentFrame % frameColumns;
            uint32 row = currentFrame / frameColumns;
            float32 fw = 1.f / frameColumns;
            float32 fh = 1.f / frameRows;
            uv = {col*fw, row*fh, (col+1)*fw, (row+1)*fh};
        }
    };

} // namespace nkentseu
