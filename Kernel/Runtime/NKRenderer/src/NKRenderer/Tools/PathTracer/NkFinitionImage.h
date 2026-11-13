#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkFinitionImage.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LA FINITION DE L'IMAGE DU TRACEUR (13/11/2026, rendu-realiste)
// =============================================================================
//  Ce qu'un objectif et un etalonneur ajoutent a l'image physique, applique a l'image
//  EXPOSEE (lineaire, 1 = le blanc) avant le rendu des tons ACES. Chaque effet a son
//  reglage ; TOUS A ZERO PAR DEFAUT : l'image d'avant, au pixel (l'image brute du
//  traceur n'est jamais touchee). Dans l'ordre de la lumiere :
//    1. LE HALO (bloom) : la part au-dessus du seuil diffuse dans l'optique, en trois
//       gaussiennes (r, 2r, 4r). L'energie est CONSERVEE : ce qui part en halo est
//       retire de la source (une lentille diffuse, elle ne cree pas de lumiere).
//    2. L'EBLOUISSEMENT (etoiles) : la diffraction des lamelles du diaphragme, N branches
//       (N pair : N/2 directions) a decroissance exponentielle, energie conservee aussi.
//    3. L'ABERRATION CHROMATIQUE LATERALE : le rouge un peu plus loin du centre, le bleu
//       un peu plus pres, d'un decalage qui croit avec le rayon (`aberration` pixels aux
//       coins). Coupee par defaut.
//    4. LA VIGNETTE : l'eclairement d'un capteur suit cos^4 de l'angle au centre (loi
//       naturelle) ; `vignette` dose la loi (1 : toute la loi pour la focale de la vue).
//    5. L'ETALONNAGE : contraste autour du gris moyen 0,18 (en log), saturation autour de
//       la luminance, chaleur (gains rouge / bleu).
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		struct NkFinition {
				float32 bloom = 0.f;		 ///< part de la lumiere au-dessus du seuil rediffusee en halo (0 : aucun)
				float32 bloomSeuil = 1.f;	 ///< luminance exposee au-dessus de laquelle la lumiere diffuse (1 = le blanc)
				float32 bloomRayon = 0.01f;	 ///< rayon de la premiere gaussienne, en fraction de la hauteur de l'image
				float32 eblouissement = 0.f; ///< part rediffusee en etoiles (0 : aucune)
				int32 branches = 6;			 ///< branches des etoiles (paire, 4 a 16)
				float32 vignette = 0.f;		 ///< dosage de la loi cos^4 (0 : aucune, 1 : toute)
				float32 aberration = 0.f;	 ///< decalage rouge / bleu aux coins, en pixels (0 : aucune)
				float32 contraste = 1.f;	 ///< puissance du rapport au gris 0,18 (1 : neutre)
				float32 saturation = 1.f;	 ///< 1 : neutre ; 0 : niveaux de gris
				float32 chaleur = 0.f;		 ///< + rechauffe, - refroidit (gains R x (1 + 0,1 c), B x (1 - 0,1 c))
				bool Actif() const {
					return bloom > 0.f || eblouissement > 0.f || vignette > 0.f || aberration > 0.f || contraste != 1.f ||
						   saturation != 1.f || chaleur != 0.f;
				}
		};

		/// Applique la finition a `rgb` (L x H x 3, lineaire, deja expose). `tanDemiV` : la tangente du demi-angle
		/// vertical de la vue (la vignette suit l'angle vrai de chaque pixel).
		void NkFinitionAppliquer(const NkFinition &f, float32 *rgb, int32 L, int32 H, float32 tanDemiV);

		/// LE BANC (sans scene) : images synthetiques -- energie du halo et des etoiles conservee, profil radial
		/// decroissant, etoiles sur leurs axes, vignette = cos^4, decalage rouge / bleu mesure, gris 0,18 fixe et
		/// saturation nulle grise, finition neutre = identite au bit. NK_FINITION_MUTATION=1 : la contre-epreuve
		/// (le halo s'ajoute sans se retirer, la vignette en cos^2, l'aberration inversee). Rend le nombre d'echecs.
		int32 NkFinitionBanc();

	} // namespace renderer
} // namespace nkentseu
