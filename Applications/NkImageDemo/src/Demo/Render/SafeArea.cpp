// =============================================================================
// SafeArea.cpp
// =============================================================================

#include "SafeArea.h"
#include "NKWindow/Core/NkWindow.h"

namespace nkentseu
{
    namespace demo
    {

        // ─────────────────────────────────────────────────────────────────────
        // SafeArea::From
        // Lit les insets natifs (NkSafeAreaInsets) puis attache la taille
        // logique du viewport.
        // ─────────────────────────────────────────────────────────────────────
        SafeArea SafeArea::From(const NkWindow& window, uint32 viewportW, uint32 viewportH)
        {
            const NkSafeAreaInsets insets = window.GetSafeAreaInsets();
            SafeArea sa;
            sa.top    = insets.top;
            sa.bottom = insets.bottom;
            sa.left   = insets.left;
            sa.right  = insets.right;
            sa.vpW    = static_cast<float>(viewportW);
            sa.vpH    = static_cast<float>(viewportH);
            return sa;
        }

    } // namespace demo
} // namespace nkentseu
