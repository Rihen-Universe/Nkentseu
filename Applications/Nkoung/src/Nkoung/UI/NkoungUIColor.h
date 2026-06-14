// =============================================================================
// UI/NkoungUIColor.h
// Palette de couleurs flat gaming pour Nkoung
// =============================================================================
#pragma once

#include "NKMath/NkColor.h"

namespace nkoung::ui {

using namespace nkentseu;
using namespace nkentseu::math;

// ============================================================================
// Classe des couleurs Nkoung — Flat Modern Gaming Palette
// ============================================================================

class NkoungUIColor {
public:
    // ========================================================================
    // Fond & Interfaces
    // ========================================================================
    static constexpr NkColor BG_DARK() { return { 15, 20, 25, 255 };        }  // #0F1419
    static constexpr NkColor BG_SECONDARY() { return { 26, 31, 46, 255 };   }  // #1A1F2E
    static constexpr NkColor BG_TERTIARY() { return { 42, 49, 66, 255 };    }  // #2A3142

    // ========================================================================
    // Accents primaires
    // ========================================================================
    static constexpr NkColor CYAN_BRIGHT() { return { 0, 217, 255, 255 };   }  // #00D9FF (cyan électrique)
    static constexpr NkColor CYAN_MID() { return { 0, 153, 204, 255 };      }  // #0099CC (cyan moyen)
    static constexpr NkColor CYAN_DARK() { return { 0, 100, 120, 255 };     }  // #006478 (cyan sombre)

    static constexpr NkColor PINK_BRIGHT() { return { 255, 0, 110, 255 };   }  // #FF006E (rose électrique)
    static constexpr NkColor PINK_MID() { return { 255, 51, 102, 255 };     }  // #FF3366 (rose moyen)
    static constexpr NkColor PINK_DARK() { return { 200, 0, 80, 255 };      }  // #C80050 (rose sombre)

    // ========================================================================
    // Statuts
    // ========================================================================
    static constexpr NkColor GREEN_SUCCESS() { return { 0, 208, 132, 255 }; }  // #00D084 (vert menthe)
    static constexpr NkColor ORANGE_WARNING() { return { 255, 165, 0, 255 };  }  // #FFA500 (orange)
    static constexpr NkColor RED_ERROR() { return { 255, 51, 51, 255 };     }  // #FF3333 (rouge)

    // ========================================================================
    // Texte
    // ========================================================================
    static constexpr NkColor TEXT_PRIMARY() { return { 255, 255, 255, 255 };}  // Blanc pur
    static constexpr NkColor TEXT_SECONDARY() { return { 160, 168, 192, 255 };} // #A0A8C0 (gris clair)
    static constexpr NkColor TEXT_TERTIARY() { return { 90, 100, 120, 255 };}   // #5A6478 (gris sombre)
    static constexpr NkColor TEXT_DISABLED() { return { 90, 100, 120, 128 };}   // Semi-transparent

    // ========================================================================
    // Éléments spécialisés
    // ========================================================================
    static constexpr NkColor BORDER_SUBTLE() { return { 42, 49, 66, 200 };  }  // Bordure normale
    static constexpr NkColor BORDER_HOVER() { return { 0, 217, 255, 220 };  }  // Bordure hover (cyan)
    static constexpr NkColor BORDER_ACTIVE() { return { 255, 0, 110, 240 }; }  // Bordure active (rose)
    static constexpr NkColor BORDER_DISABLED() { return { 90, 100, 120, 100 };} // Bordure disabled

    static constexpr NkColor CARD_BG() { return { 26, 31, 46, 255 };        }  // Card background
    static constexpr NkColor OVERLAY_DARK() { return { 15, 20, 25, 200 };   }  // Semi-transparent overlay

    // ========================================================================
    // Dégradés (simples, représentés comme couleur start)
    // ========================================================================
    static constexpr NkColor GRADIENT_CYAN_START() { return { 0, 217, 255, 255 }; }
    static constexpr NkColor GRADIENT_CYAN_END() { return { 0, 153, 204, 255 };   }

    static constexpr NkColor GRADIENT_PINK_START() { return { 255, 0, 110, 255 }; }
    static constexpr NkColor GRADIENT_PINK_END() { return { 255, 51, 102, 255 };  }

    static constexpr NkColor GRADIENT_GREEN_START() { return { 0, 208, 132, 255 };  }
    static constexpr NkColor GRADIENT_GREEN_END() { return { 0, 153, 102, 255 };    }
};

// ============================================================================
// Fonction utilitaire pour créer une couleur avec alpha personnalisé
// ============================================================================

inline NkColor WithAlpha(const NkColor& color, uint8 alpha) noexcept {
    return { color.r, color.g, color.b, alpha };
}

// ============================================================================
// Interpolation linéaire de couleur (simple lerp pour dégradés)
// ============================================================================

inline NkColor LerpColor(const NkColor& from, const NkColor& to, float32 t) noexcept {
    t = (t < 0.f) ? 0.f : (t > 1.f) ? 1.f : t;
    return {
        static_cast<uint8>(from.r + (to.r - from.r) * t),
        static_cast<uint8>(from.g + (to.g - from.g) * t),
        static_cast<uint8>(from.b + (to.b - from.b) * t),
        static_cast<uint8>(from.a + (to.a - from.a) * t)
    };
}

}  // namespace nkoung::ui
