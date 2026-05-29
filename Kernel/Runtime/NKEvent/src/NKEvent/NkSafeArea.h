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

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkStringUtils.h"
#include <string>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------
	// NkSafeAreaInsets — marges en pixels physiques (logiques × DPI)
	// ---------------------------------------------------------------------------

	struct NkSafeAreaInsets {
		float32 top = 0.f;	///< Marge haute  (barre de statut / Dynamic Island)
		float32 bottom = 0.f; ///< Marge basse  (home indicator / barre navigation)
		float32 left = 0.f;	///< Marge gauche (encoche paysage)
		float32 right = 0.f;	///< Marge droite (encoche paysage)

		NkSafeAreaInsets() = default;
		NkSafeAreaInsets(float32 t, float32 b, float32 l, float32 r) : top(t), bottom(b), left(l), right(r) {
		}

		nk_bool IsZero() const {
			return top == 0.f && bottom == 0.f && left == 0.f && right == 0.f;
		}

		/// Surface utilisable (en pixels)
		uint32 UsableWidth(uint32 totalWidth) const {
			float32 w = totalWidth - left - right;
			return w > 0 ? static_cast<uint32>(w) : 0;
		}

		uint32 UsableHeight(uint32 totalHeight) const {
			float32 h = totalHeight - top - bottom;
			return h > 0 ? static_cast<uint32>(h) : 0;
		}

		/// Clipe un point dans la zone sûre (returns false si hors zone)
		nk_bool ClipPoint(float32 x, float32 y, float32 totalW, float32 totalH) const {
			return x >= left && x <= totalW - right && y >= top && y <= totalH - bottom;
		}

		NkString ToString() const {
			return NkString::Fmt("SafeArea(T={0:.2} B={1:.2} L={2:.2} R={3:.2})", top, bottom, left, right);
		}
	};

	// ---------------------------------------------------------------------------
	// NkSafeAreaData — événement quand les insets changent (rotation, etc.)
	// ---------------------------------------------------------------------------

	struct NkSafeAreaData {
		NkSafeAreaInsets insets;
		uint32 displayWidth = 0;
		uint32 displayHeight = 0;

		NkSafeAreaData() = default;
		NkSafeAreaData(const NkSafeAreaInsets &i, uint32 w, uint32 h) : insets(i), displayWidth(w), displayHeight(h) {
		}
	};

	enum class NkScreenOrientation : uint32 {
		NK_SCREEN_ORIENTATION_AUTO = 0,
		NK_SCREEN_ORIENTATION_PORTRAIT,
		NK_SCREEN_ORIENTATION_LANDSCAPE,
	};

} // namespace nkentseu