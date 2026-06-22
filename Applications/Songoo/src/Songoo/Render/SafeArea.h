#pragma once
// =============================================================================
// SafeArea.h — Copie EXACTE du repo Nkentseu Songoo
// Adapté pour Songo'o (même code, renommage du namespace uniquement si besoin)
// =============================================================================

#include "NKEvent/NkSafeArea.h"
#include "NKCore/NkTypes.h"

namespace nkentseu { class NkWindow; }

namespace nkentseu { namespace songoo {

    struct SafeArea {
        float top    = 0.0f;
        float bottom = 0.0f;
        float left   = 0.0f;
        float right  = 0.0f;
        float vpW    = 0.0f;
        float vpH    = 0.0f;

        static SafeArea From(const NkWindow& window, uint32 viewportW, uint32 viewportH);

        float TopY()    const noexcept { return top; }
        float BottomY() const noexcept { return vpH - bottom; }
        float LeftX()   const noexcept { return left; }
        float RightX()  const noexcept { return vpW - right; }
        float SafeW()   const noexcept { return vpW - left - right; }
        float SafeH()   const noexcept { return vpH - top - bottom; }
        float SafeCX()  const noexcept { return left + SafeW() * 0.5f; }
        float SafeCY()  const noexcept { return top  + SafeH() * 0.5f; }

        float TopY(float padding)    const noexcept { return top + padding; }
        float BottomY(float padding) const noexcept { return vpH - bottom - padding; }
        float LeftX(float padding)   const noexcept { return left + padding; }
        float RightX(float padding)  const noexcept { return vpW - right - padding; }
    };

}} // namespace nkentseu::songoo
