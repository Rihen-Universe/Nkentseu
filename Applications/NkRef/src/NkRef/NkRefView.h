#pragma once
// ============================================================================
// NkRefView.h — la caméra 2D du canevas infini, PURE (aucune dépendance rendu).
// ----------------------------------------------------------------------------
// C'est la pièce qu'on GARDE : main.cpp n'est que la glue fenêtre/boucle.
// Les crochets d'agent (NK_AGENT_PAN/ZOOM) appellent LES MÊMES méthodes que la
// souris — il n'existe jamais un second chemin de test.
//
// Convention : `zoom` = pixels par unité monde (1 = 1:1, 2 = zoom avant ×2).
//   pixel = (world - center) * zoom + viewport/2
//   world = center + (pixel - viewport/2) / zoom
// Le viewport est passé en paramètre (jamais stocké) : la fenêtre peut être
// redimensionnée à tout moment, la vue n'a pas à le savoir.
// ============================================================================

#include "NKMath/NKMath.h"

namespace nkref {

	using nkentseu::float32;
	using nkentseu::math::NkVec2f;

	struct NkRefView {
			NkVec2f center{0.0f, 0.0f}; ///< point monde au centre du viewport
			float32 zoom = 1.0f;		///< pixels par unité monde

			// Bornes de zoom : 2 % (vue d'ensemble d'une planche géante) à
			// 6400 % (inspection d'un détail). Au-delà, le float32 et la grille
			// n'apportent plus rien d'utile.
			static constexpr float32 kZoomMin = 0.02f;
			static constexpr float32 kZoomMax = 64.0f;

			NkVec2f PixelToWorld(const NkVec2f &pixel, const NkVec2f &viewport) const {
				return {center.x + (pixel.x - viewport.x * 0.5f) / zoom,
						center.y + (pixel.y - viewport.y * 0.5f) / zoom};
			}

			NkVec2f WorldToPixel(const NkVec2f &world, const NkVec2f &viewport) const {
				return {(world.x - center.x) * zoom + viewport.x * 0.5f,
						(world.y - center.y) * zoom + viewport.y * 0.5f};
			}

			/// Pan : la planche SUIT le curseur (glisser vers la droite = le
			/// contenu part à droite), donc le centre recule du delta.
			void PanByPixels(float32 dxPixels, float32 dyPixels) {
				center.x -= dxPixels / zoom;
				center.y -= dyPixels / zoom;
			}

			/// La règle d'or du canevas infini : le point monde SOUS le curseur
			/// ne bouge pas pendant le zoom. On fige ce point, on change le
			/// zoom, puis on replace le centre pour que ce point retombe
			/// exactement sur le même pixel.
			void ZoomAtPixel(float32 factor, const NkVec2f &pixel, const NkVec2f &viewport) {
				const NkVec2f anchor = PixelToWorld(pixel, viewport);
				float32 z = zoom * factor;
				if (z < kZoomMin)
					z = kZoomMin;
				if (z > kZoomMax)
					z = kZoomMax;
				zoom = z;
				center.x = anchor.x - (pixel.x - viewport.x * 0.5f) / zoom;
				center.y = anchor.y - (pixel.y - viewport.y * 0.5f) / zoom;
			}

			void Reset() {
				center = {0.0f, 0.0f};
				zoom = 1.0f;
			}

			/// Espacement de grille ADAPTATIF : la plus petite puissance de 2
			/// (unités monde) dont la projection écran atteint `minPixels`.
			/// Sans ça, dézoomer transforme la grille en bouillie de lignes ;
			/// zoomer la fait disparaître. Puissances de 2 : chaque niveau
			/// contient exactement le précédent (les lignes ne « sautent » pas).
			float32 GridSpacing(float32 minPixels) const {
				float32 s = 1.0f;
				while (s * zoom < minPixels)
					s *= 2.0f; // borné par kZoomMin
				while (s * 0.5f * zoom >= minPixels)
					s *= 0.5f; // borné par kZoomMax
				return s;
			}
	};

} // namespace nkref
