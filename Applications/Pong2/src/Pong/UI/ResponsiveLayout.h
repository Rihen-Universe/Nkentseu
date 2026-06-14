#pragma once
// =============================================================================
// ResponsiveLayout.h
// -----------------------------------------------------------------------------
// Helpers de layout responsive en POURCENTAGE de viewport.
//
// Principe : au lieu de positionner les elements UI en `px * uiScale`, on les
// exprime en fraction de W (largeur viewport) ou H (hauteur viewport). Cela
// garantit :
//   - Aucun debordement sur petits ecrans (le contenu se ramasse).
//   - Pas d'UI minuscule sur grand ecran (le contenu s'etale).
//   - Adaptation automatique au ratio (paysage / portrait).
//
// Pattern d'utilisation :
//   const float btnW  = Pct::W(W, 0.30f, 200.0f, 420.0f);
//   const float btnH  = Pct::H(H, 0.075f, 38.0f, 70.0f);
//   const float gap   = Pct::H(H, 0.012f, 6.0f, 16.0f);
//   const float x     = W * 0.5f - btnW * 0.5f;     // centre
//   const float y     = H * 0.40f;                   // 40% du haut
//
// Les clamps doux (min / max) evitent les degeneres aux extremes (ex: bouton
// de 50px de large sur 4K, ou bouton de 800px de large sur ultrawide). Si on
// ne veut pas de clamp, passer min=0 max=FLT_MAX.
//
// Convention : on garde l'echelle de police (mScale via GetUIScale) parce que
// les glyphes ne sont pas vectoriels mais des bitmaps d'atlas — ils ont besoin
// d'un scale geometrique pour rester nets a grande resolution.
// =============================================================================

#include "NKMath/NkFunctions.h"

namespace nkentseu
{
    namespace pong
    {

        /// Helpers de calcul de dimensions en pourcentage de viewport.
        /// Toutes les methodes sont inline + noexcept pour zero overhead.
        struct Pct
        {
            /// Largeur en % du viewport, clampee dans [minPx, maxPx].
            /// @param vW  Largeur viewport (en pixels).
            /// @param frac Fraction de vW (ex: 0.30f = 30 % de la largeur).
            /// @param minPx Cap inferieur en pixels (eviter degenere petit).
            /// @param maxPx Cap superieur en pixels (eviter ultrawide).
            static inline float W(int vW, float frac,
                                  float minPx = 0.0f,
                                  float maxPx = 1.0e6f) noexcept
            {
                const float v = (float)vW * frac;
                return math::NkClamp(v, minPx, maxPx);
            }

            /// Hauteur en % du viewport, clampee dans [minPx, maxPx].
            static inline float H(int vH, float frac,
                                  float minPx = 0.0f,
                                  float maxPx = 1.0e6f) noexcept
            {
                const float v = (float)vH * frac;
                return math::NkClamp(v, minPx, maxPx);
            }

            /// Helper centre horizontal d'un element de largeur `w` dans `vW`.
            static inline float CenterX(int vW, float w) noexcept
            {
                return (float)vW * 0.5f - w * 0.5f;
            }

            /// Helper centre vertical d'un element de hauteur `h` dans `vH`.
            static inline float CenterY(int vH, float h) noexcept
            {
                return (float)vH * 0.5f - h * 0.5f;
            }

            /// Plus petite dimension du viewport (utile pour les elements
            /// dont la taille doit dependre du plus petit cote, ex: pastilles
            /// circulaires).
            static inline float MinDim(int vW, int vH) noexcept
            {
                return (vW < vH) ? (float)vW : (float)vH;
            }

            /// Plus grande dimension du viewport.
            static inline float MaxDim(int vW, int vH) noexcept
            {
                return (vW > vH) ? (float)vW : (float)vH;
            }

            /// Ratio largeur / hauteur (>1 = paysage, <1 = portrait).
            static inline float Aspect(int vW, int vH) noexcept
            {
                return (vH > 0) ? (float)vW / (float)vH : 1.0f;
            }

            /// Vrai si le viewport est en mode portrait (H > W).
            static inline bool IsPortrait(int vW, int vH) noexcept
            {
                return vH > vW;
            }
        };

    } // namespace pong
} // namespace nkentseu
