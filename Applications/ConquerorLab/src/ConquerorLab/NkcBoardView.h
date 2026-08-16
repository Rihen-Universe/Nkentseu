#pragma once
// =============================================================================
// NkcBoardView — zoom, deplacement, et grands plateaux.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// Le panneau se contentait du cadrage automatique : le plateau remplissait le
// panneau, point. C'est parfait pour 42 cases et intenable au-dela.
//
//   - un plateau de 30x30 tient a l'ecran avec des cellules de 6 pixels : on
//     voit une mosaique, on ne joue plus ;
//   - il n'y avait AUCUN moyen de regarder un coin de pres ;
//   - toutes les cellules etaient dessinees a chaque image, meme celles hors du
//     panneau — 900 hexagones dont 60 visibles.
//
// CE QU'IL APPORTE
//   zoom      molette, ou Ctrl+molette, centre SOUS LE CURSEUR
//   deplacement  glisser au bouton du milieu, ou a la barre d'espace
//   recadrage    un bouton et un raccourci, pour revenir au cadrage automatique
//   culling      ne dessine que ce qui touche le panneau
//
// LE PRINCIPE : LE CADRAGE AUTOMATIQUE RESTE LE DEFAUT
// ----------------------------------------------------
// `zoom == 1, pan == 0` redonne exactement le comportement d'avant. Tant que
// personne ne touche a la molette, rien ne change — et le jour ou l'on charge un
// plateau de 900 cases, l'outil est la. On n'a pas remplace un comportement qui
// marchait, on lui a ajoute une sortie de secours.
//
// POURQUOI LE ZOOM EST UN FACTEUR, PAS UNE TAILLE DE CELLULE
// ----------------------------------------------------------
// Stocker « cellule = 18 px » obligerait a recalculer ce nombre a chaque
// redimensionnement de fenetre, a chaque changement de plateau. Un FACTEUR
// applique au cadrage automatique survit a tout cela : la fenetre change, le
// plateau change, le rapport voulu par l'utilisateur reste.
// =============================================================================

#include "ConquerorLab/NkcBoardRender.h"

namespace nkentseu {
	namespace conqueror {

		/// Bornes du zoom. En dessous de 0.2 on ne distingue plus rien et le
		/// picking devient une loterie ; au-dela de 12 une seule cellule remplit
		/// l'ecran, ce qui n'a plus d'usage.
		inline constexpr float32 kZoomMin = 0.2f;
		inline constexpr float32 kZoomMax = 12.f;

		/// Ce que l'utilisateur a fait de sa vue. Persiste entre les images, se
		/// remet a zero au recadrage.
		struct NkcBoardView {
				float32 zoom = 1.f;			 ///< facteur applique au cadrage automatique
				NkVec2	pan	 = {0.f, 0.f};	 ///< decalage en pixels
				bool	dragging = false;
				NkVec2	dragOrigin = {0.f, 0.f};
				NkVec2	panOrigin  = {0.f, 0.f};

				bool IsDefault() const noexcept {
					return zoom > 0.999f && zoom < 1.001f &&
						   pan.x > -0.5f && pan.x < 0.5f && pan.y > -0.5f && pan.y < 0.5f;
				}

				void Reset() noexcept {
					zoom	 = 1.f;
					pan		 = {0.f, 0.f};
					dragging = false;
				}
		};

		/// Applique zoom et deplacement a un cadrage automatique.
		///
		/// L'ORDRE COMPTE. On multiplie la taille de cellule PUIS on corrige
		/// l'origine pour que le centre du panneau reste le centre : sans cette
		/// correction, zoomer fait fuir le plateau vers le bas a droite, et
		/// l'utilisateur passe son temps a le rattraper.
		inline NkcBoardLayout NkcApplyView(const NkcBoardLayout &fit, const NkcBoardView &v,
										   const NkRect &area) noexcept {
			NkcBoardLayout L = fit;
			L.cell = fit.cell * v.zoom;

			const NkVec2 c = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
			L.origin.x = c.x + (fit.origin.x - c.x) * v.zoom + v.pan.x;
			L.origin.y = c.y + (fit.origin.y - c.y) * v.zoom + v.pan.y;
			return L;
		}

		/// Zoome autour d'un point d'ancrage — le curseur, pas le centre.
		///
		/// C'est la difference entre un zoom utilisable et un zoom qu'on subit :
		/// on vise une case, on tourne la molette, et cette case reste sous le
		/// pointeur. Techniquement : on veut que le point ecran `anchor` designe
		/// la meme position du plateau avant et apres.
		inline void NkcZoomAt(NkcBoardView &v, const NkcBoardLayout &fit, const NkRect &area,
							  const NkVec2 &anchor, float32 factor) noexcept {
			float32 z = v.zoom * factor;
			if (z < kZoomMin) z = kZoomMin;
			if (z > kZoomMax) z = kZoomMax;
			if (z == v.zoom) return;

			// Position du plateau (en pixels du cadrage automatique) actuellement
			// sous `anchor`, puis correction de `pan` pour l'y ramener.
			const NkVec2 c = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
			const float32 bx = (anchor.x - v.pan.x - c.x) / v.zoom + c.x;
			const float32 by = (anchor.y - v.pan.y - c.y) / v.zoom + c.y;

			v.zoom	= z;
			v.pan.x = anchor.x - c.x - (bx - c.x) * z;
			v.pan.y = anchor.y - c.y - (by - c.y) * z;
		}

		/// Faut-il dessiner cette cellule ? Rejette ce qui ne touche pas `area`.
		///
		/// Sans ce test, un plateau de 900 cases produit 900 polygones par image
		/// dont l'immense majorite est hors ecran. Avec, le cout suit ce qu'on
		/// VOIT, pas ce qui existe — et un grand plateau zoome coute moins qu'un
		/// petit plateau entier.
		inline bool NkcCellVisible(const NkVec2 &center, float32 cell, const NkRect &area) noexcept {
			const float32 m = cell * 1.5f;	 // marge : la cellule deborde de son centre
			return center.x + m >= area.x && center.x - m <= area.x + area.w &&
				   center.y + m >= area.y && center.y - m <= area.y + area.h;
		}

	} // namespace conqueror
} // namespace nkentseu
