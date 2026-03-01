#pragma once

// =============================================================================
// NkSafeArea.h
// Zone sûre d'affichage cross-platform.
//
// Sur mobile, certains bords de l'écran sont masqués ou inutilisables
// (encoche, barre de statut, barre de navigation, home indicator, coins
// arrondis, Dynamic Island, etc.).  NkSafeAreaInsets donne les marges
// à respecter pour ne pas y dessiner de contenu interactif.
//
// Sources par plateforme :
//   iOS/iPadOS : UIView.safeAreaInsets (UIEdgeInsets)
//   Android    : WindowInsets.getSystemWindowInsets() (API 20+)
//                ou WindowInsetsCompat
//   macOS      : Toujours {0,0,0,0}
//   Win32/Linux: Toujours {0,0,0,0}
//   WASM       : Toujours {0,0,0,0}
// =============================================================================

#include "NkTypes.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// NkSafeAreaInsets — marges en pixels physiques (logiques × DPI)
// ---------------------------------------------------------------------------

struct NkSafeAreaInsets {
	float top = 0.f;	///< Marge haute  (barre de statut / Dynamic Island)
	float bottom = 0.f; ///< Marge basse  (home indicator / barre navigation)
	float left = 0.f;	///< Marge gauche (encoche paysage)
	float right = 0.f;	///< Marge droite (encoche paysage)

	NkSafeAreaInsets() = default;
	NkSafeAreaInsets(float t, float b, float l, float r) : top(t), bottom(b), left(l), right(r) {
	}

	bool IsZero() const {
		return top == 0.f && bottom == 0.f && left == 0.f && right == 0.f;
	}

	/// Surface utilisable (en pixels)
	NkU32 UsableWidth(NkU32 totalWidth) const {
		float w = totalWidth - left - right;
		return w > 0 ? static_cast<NkU32>(w) : 0;
	}

	NkU32 UsableHeight(NkU32 totalHeight) const {
		float h = totalHeight - top - bottom;
		return h > 0 ? static_cast<NkU32>(h) : 0;
	}

	/// Clipe un point dans la zone sûre (returns false si hors zone)
	bool ClipPoint(float x, float y, float totalW, float totalH) const {
		return x >= left && x <= totalW - right && y >= top && y <= totalH - bottom;
	}

	std::string ToString() const {
		return "SafeArea(T=" + std::to_string(top) + " B=" + std::to_string(bottom) + " L=" + std::to_string(left) +
			   " R=" + std::to_string(right) + ")";
	}
};

// ---------------------------------------------------------------------------
// NkSafeAreaData — événement quand les insets changent (rotation, etc.)
// ---------------------------------------------------------------------------

struct NkSafeAreaData {
	NkSafeAreaInsets insets;
	NkU32 displayWidth = 0;
	NkU32 displayHeight = 0;

	NkSafeAreaData() = default;
	NkSafeAreaData(const NkSafeAreaInsets &i, NkU32 w, NkU32 h) : insets(i), displayWidth(w), displayHeight(h) {
	}
};

} // namespace nkentseu
