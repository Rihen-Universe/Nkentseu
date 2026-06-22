#pragma once
// =============================================================================
// UIScale.h — Scale UI responsive (copie exacte du repo Nkentseu Songoo)
// Base de référence : 1600×900. Sur Android on adapte selon la densité.
// =============================================================================

namespace nkentseu { namespace songoo {

    inline float GetUIScale(int viewportW, int viewportH) {
        float sx = (float)viewportW / 1600.f;
        float sy = (float)viewportH / 900.f;
        float s  = sx < sy ? sx : sy;
        return s < 0.3f ? 0.3f : s > 3.f ? 3.f : s;
    }

}} // namespace nkentseu::songoo
