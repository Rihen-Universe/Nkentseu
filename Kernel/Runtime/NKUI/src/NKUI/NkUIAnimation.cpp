// -----------------------------------------------------------------------------
// @File    NkUIAnimation.cpp
// @Brief   Implémentation des fonctions d'easing.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @License Apache-2.0
// -----------------------------------------------------------------------------

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Animation helper implementation.
 * Main data: Interpolation, easing and animation stepping logic.
 * Change this file when: Animation timing or easing behavior needs adjustment.
 */

#include "NKUI/NkUIAnimation.h"
#include <cmath>

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Applique une fonction d'easing
        // ============================================================

        float32 NkUIEasing::Apply(NkEase ease, float32 t) noexcept
        {
            t = t < 0 ? 0 : (t > 1 ? 1 : t);

            switch (ease)
            {
                case NkEase::NK_LINEAR:          return Linear(t);
                case NkEase::NK_IN_QUAD:         return InQuad(t);
                case NkEase::NK_OUT_QUAD:        return OutQuad(t);
                case NkEase::NK_IN_OUT_QUAD:     return InOutQuad(t);
                case NkEase::NK_IN_CUBIC:        return InCubic(t);
                case NkEase::NK_OUT_CUBIC:       return OutCubic(t);
                case NkEase::NK_IN_OUT_CUBIC:    return InOutCubic(t);
                case NkEase::NK_IN_QUART:        return t * t * t * t;
                case NkEase::NK_OUT_QUART:
                {
                    const float32 s = t - 1;
                    return 1 - s * s * s * s;
                }
                case NkEase::NK_IN_OUT_QUART:
                    return t < 0.5f
                           ? 8 * t * t * t * t
                           : 1 - 8 * (t - 1) * (t - 1) * (t - 1) * (t - 1);
                case NkEase::NK_IN_SINE:         return 1 - ::cosf(t * NKUI_PI * 0.5f);
                case NkEase::NK_OUT_SINE:        return ::sinf(t * NKUI_PI * 0.5f);
                case NkEase::NK_IN_OUT_SINE:     return -(::cosf(NKUI_PI * t) - 1) * 0.5f;
                case NkEase::NK_IN_EXPO:
                    return t == 0 ? 0 : ::powf(2, 10 * t - 10);
                case NkEase::NK_OUT_EXPO:
                    return t == 1 ? 1 : 1 - ::powf(2, -10 * t);
                case NkEase::NK_IN_OUT_EXPO:
                    if (t == 0 || t == 1) return t;
                    return t < 0.5f
                           ? ::powf(2, 20 * t - 10) * 0.5f
                           : (2 - ::powf(2, -20 * t + 10)) * 0.5f;
                case NkEase::NK_IN_ELASTIC:
                {
                    if (t <= 0 || t >= 1) return t;
                    return -::powf(2, 10 * t - 10) * ::sinf((t * 10 - 10.75f) * 2 * NKUI_PI / 3);
                }
                case NkEase::NK_OUT_ELASTIC:     return OutElastic(t);
                case NkEase::NK_IN_OUT_ELASTIC:
                {
                    if (t <= 0 || t >= 1) return t;
                    return t < 0.5f
                           ? -::powf(2, 20 * t - 10) * ::sinf((20 * t - 11.125f) * 2 * NKUI_PI / 4.5f) * 0.5f
                           : (::powf(2, -20 * t + 10) * ::sinf((20 * t - 11.125f) * 2 * NKUI_PI / 4.5f)) * 0.5f + 1;
                }
                case NkEase::NK_IN_BOUNCE:       return 1 - OutBounce(1 - t);
                case NkEase::NK_OUT_BOUNCE:      return OutBounce(t);
                case NkEase::NK_IN_OUT_BOUNCE:
                    return t < 0.5f
                           ? (1 - OutBounce(1 - 2 * t)) * 0.5f
                           : (1 + OutBounce(2 * t - 1)) * 0.5f;
                case NkEase::NK_IN_BACK:
                {
                    const float32 c = 1.70158f;
                    const float32 c2 = c + 1;
                    return c2 * t * t * t - c * t * t;
                }
                case NkEase::NK_OUT_BACK:        return OutBack(t);
                case NkEase::NK_IN_OUT_BACK:
                {
                    const float32 c = 1.70158f * 1.525f;
                    return t < 0.5f
                           ? (::powf(2 * t, 2) * ((c + 1) * 2 * t - c)) * 0.5f
                           : (::powf(2 * t - 2, 2) * ((c + 1) * (t * 2 - 2) + c) + 2) * 0.5f;
                }
                default:
                    return t;
            }
        }

    } // namespace nkui
} // namespace nkentseu