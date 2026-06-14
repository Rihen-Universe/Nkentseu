#pragma once
// =============================================================================
// NkSculptBrush.h  — NKRenderer v5.0  (Tools/PixolSculpt/)
//
// Description d'une brosse de sculpt et d'un "dab" (un tampon unique le long
// d'un trace). NkSculptBrushGPU est le bloc compact pousse au kernel compute
// via push constants : son layout DOIT matcher le bloc std430 declare dans
// shaders/sculpt_brush.comp.glsl.
//
// ⚠️ SQUELETTE — structures de donnees uniquement, pas de logique ici.
// =============================================================================
#include "NKMath/NKMath.h"
#include "NKRenderer/Tools/PixolSculpt/NkSculptTypes.h"

namespace nkentseu {
    namespace renderer {

        using namespace math;

        // Parametres d'une brosse, cote CPU / outil.
        struct NkSculptBrush {
            NkSculptBrushMode mode      = NkSculptBrushMode::NK_RAISE;
            NkSculptFalloff   falloff   = NkSculptFalloff::NK_SMOOTH;
            float32           radiusPx  = 48.f;       ///< Rayon en pixels ecran.
            float32           strength  = 0.5f;       ///< Intensite [0..1].
            float32           hardness  = 0.5f;       ///< Durete du profil [0..1].
            bool              invert    = false;      ///< Inverse le sens (raise<->lower).
            NkVec4f           color     = {1, 1, 1, 1};///< Pour NK_PAINT.
            float32           depthBias = 0.f;        ///< Decalage Z additionnel.
        };

        // Un tampon unique. Un trace = une suite de dabs interpolee et espacee.
        struct NkSculptDab {
            NkVec2f screenPos = {0, 0};  ///< Centre en pixels ecran.
            float32 radiusPx  = 48.f;
            float32 pressure  = 1.f;     ///< Pression stylet [0..1] (module strength/radius).
        };

        // ─────────────────────────────────────────────────────────────────────
        // Bloc GPU (push constants). Aligne 16 octets, layout std430.
        // ⚠️ Toute modif ici doit etre repercutee dans sculpt_brush.comp.glsl.
        // ─────────────────────────────────────────────────────────────────────
        struct NkSculptBrushGPU {
            NkVec2f center;      // offset 0   — centre en pixels
            float32 radius;      // offset 8
            float32 strength;    // offset 12
            NkVec4f color;       // offset 16  — albedo (paint)
            uint32  mode;        // offset 32  — NkSculptBrushMode
            uint32  falloff;     // offset 36  — NkSculptFalloff
            float32 hardness;    // offset 40
            float32 depthBias;   // offset 44
            int32   tileOffsetX; // offset 48  — origine de la tuile dispatchee
            int32   tileOffsetY; // offset 52
            uint32  _pad0;       // offset 56
            uint32  _pad1;       // offset 60  (taille totale 64, align 16)
        };

        // Remplit le bloc push-constant a partir de la brosse + du dab + de
        // l'origine de la tuile dispatchee. La pression module l'intensite.
        inline NkSculptBrushGPU MakeBrushGPU(const NkSculptBrush& b, const NkSculptDab& dab,
                                             int32 tileOffX, int32 tileOffY) noexcept {
            NkSculptBrushGPU g{};
            g.center      = dab.screenPos;
            g.radius      = dab.radiusPx;
            g.strength    = b.strength * dab.pressure;
            g.color       = b.color;
            g.mode        = (uint32)b.mode;
            g.falloff     = (uint32)b.falloff;
            g.hardness    = b.hardness;
            g.depthBias   = b.depthBias;
            g.tileOffsetX = tileOffX;
            g.tileOffsetY = tileOffY;
            return g;
        }

    } // namespace renderer
} // namespace nkentseu
