#pragma once
// =============================================================================
// SafeArea.h
// -----------------------------------------------------------------------------
// Petit wrapper sur NkWindow::GetSafeAreaInsets() pour exposer une zone sure
// d'affichage cross-plateforme. Sur Android, les insets incluent la status
// bar, la navigation bar, l'encoche / Dynamic Island.
// Sur desktop, tous les insets sont a 0.
//
// Usage type :
//   SafeArea sa = SafeArea::From(window, viewportW, viewportH);
//   float topY    = sa.TopY();            // y du premier pixel safe
//   float bottomY = sa.BottomY();         // y juste apres le dernier pixel safe
//   float leftX   = sa.LeftX();
//   float rightX  = sa.RightX();
// =============================================================================

#include "NKWindow/Core/NkSafeArea.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    class NkWindow;
}

namespace nkentseu
{
    namespace demo
    {

        // ─────────────────────────────────────────────────────────────────────
        // SafeArea — vue locale d'un NkSafeAreaInsets attache a une taille de
        // viewport donnee, avec des helpers pratiques pour la mise en page UI.
        // ─────────────────────────────────────────────────────────────────────
        struct SafeArea
        {
            float top    = 0.0f;
            float bottom = 0.0f;
            float left   = 0.0f;
            float right  = 0.0f;
            float vpW    = 0.0f;
            float vpH    = 0.0f;

            /// Construit depuis la fenetre courante.
            static SafeArea From(const NkWindow& window, uint32 viewportW, uint32 viewportH);

            // ── Helpers pour ancrer du contenu ───────────────────────────────
            float TopY()     const noexcept { return top; }
            float BottomY()  const noexcept { return vpH - bottom; }
            float LeftX()    const noexcept { return left; }
            float RightX()   const noexcept { return vpW - right; }
            float SafeW()    const noexcept { return vpW - left - right; }
            float SafeH()    const noexcept { return vpH - top - bottom; }
            float SafeCX()   const noexcept { return left + SafeW() * 0.5f; }
            float SafeCY()   const noexcept { return top  + SafeH() * 0.5f; }

            /// Marge supplementaire interieure (en plus des insets) — utile
            /// pour eloigner le contenu UI des bords sur tactile.
            float TopY(float padding)    const noexcept { return top + padding; }
            float BottomY(float padding) const noexcept { return vpH - bottom - padding; }
            float LeftX(float padding)   const noexcept { return left + padding; }
            float RightX(float padding)  const noexcept { return vpW - right - padding; }
        };

    } // namespace demo
} // namespace nkentseu
