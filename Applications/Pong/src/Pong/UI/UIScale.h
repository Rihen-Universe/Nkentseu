#pragma once
// =============================================================================
// UIScale.h
// -----------------------------------------------------------------------------
// Helper de mise a l'echelle de l'UI cross-platform.
//
// Principe : toutes les scenes sont concues pour un viewport de reference
// 1280x720 (cf. mockups HTML). En runtime, on calcule un facteur de scale
// proportionnel au viewport reel + un boost mobile pour compenser la haute
// densite d'ecran (smartphones modernes : 400-500 DPI vs 96 sur desktop).
//
// Sans boost, sur un Galaxy S22+ en landscape (2340x1080), on aurait
//   scale = min(2340/1280, 1080/720) = min(1.83, 1.50) = 1.50
// qui donne un paddle de 90 px sur 1080 (= 8 % de hauteur) — trop petit.
// Avec boost mobile *1.5, on monte a ~2.25 -> 135 px (12 %), confortable.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu
{
    namespace pong
    {

        /// Facteur de scale uniforme a appliquer aux dimensions UI / gameplay.
        /// @param vW Largeur du viewport en pixels physiques.
        /// @param vH Hauteur  du viewport en pixels physiques.
        /// @return Scale 1.0 sur 1280x720 desktop. Plus eleve sur grand
        ///         viewport ou sur mobile (avec boost densite).
        inline float GetUIScale(int vW, int vH) noexcept
        {
            const float refW = 1280.0f;
            const float refH = 720.0f;
            const float baseScale = math::NkMin((float)vW / refW,
                                                (float)vH / refH);

            // Note design : on garde un scale modere (max 1.4x). Les positions
            // absolues des scenes sont actuellement multipliees par mScale, ce
            // qui les fait deborder verticalement sur mobile si le boost est
            // trop fort. Refactor futur : positionner en % viewport plutot
            // qu'en pixels scaled — alors on pourra remonter le boost mobile.
            return math::NkClamp(baseScale, 0.55f, 1.4f);
        }

    } // namespace pong
} // namespace nkentseu
