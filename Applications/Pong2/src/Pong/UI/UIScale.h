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

            // Clamp adaptatif (2026-05-19) : un seul couple [min, max] cassait
            // l'UI sur les ecrans extremes.
            //   - Plancher precedent 0.55 -> les ecrans tres petits (< 720p)
            //     debordaient parce que le scale ne pouvait pas descendre.
            //   - Plafond precedent 1.4 -> les ecrans desktop (>=1080p) ne
            //     profitaient pas du full screen, l'UI restait minuscule.
            // On adapte selon le ratio ecran et la taille absolue :
            //   - Paysage (PC ou mobile landscape) : on autorise un plafond
            //     plus haut (2.0x) puisque le layout est concu pour 1280x720.
            //   - Portrait : on garde 1.4 car le layout n'est pas optimise
            //     pour les rapports hauteur > largeur.
            //   - Plancher : on descend a 0.35x sur les tres petits ecrans
            //     pour eviter les overlays qui debordent.
            const float aspect = (vH > 0) ? (float)vW / (float)vH : 1.0f;
            const float maxScale = (aspect > 1.5f) ? 2.0f : 1.4f;
            const float minScale = (vW < 700 || vH < 420) ? 0.35f : 0.55f;
            return math::NkClamp(baseScale, minScale, maxScale);
        }

    } // namespace pong
} // namespace nkentseu
