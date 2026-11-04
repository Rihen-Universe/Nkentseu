// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkImagePalette.h — palette, fond et bandes occupées d'une image 8 bits.
//
// Quatre outils, tous sur le même modèle : l'image est lue pixel par pixel,
// sans aucune dépendance en dehors de NkImage.
//
//   Quantize         réduit l'image à k couleurs (k-means) : l'allure « pixel art »
//                    d'un aliment photographié.
//   EstimateBg       devine la couleur de fond : médiane des pixels du bord.
//   RemoveBackground rend transparent le fond relié au bord de l'image.
//   Spans            bandes occupées : sur quelles lignes (ou colonnes) l'image
//                    contient autre chose que le fond ; sert à découper une planche
//                    de dessins en cases (voir NkSpriteSheet).
//
// Les pixels sont lus en RGB (3 premiers canaux) et, s'il existe, en alpha
// (dernier canal) : GRAY8 est traité comme R = G = B.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkRectangle.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

	/// Intervalle [debut, fin[ de lignes ou de colonnes.
	struct NkImageSpan {
			int32 debut = 0;
			int32 fin = 0; ///< exclu
	};

	/// Axe de projection pour Spans.
	enum class NkImageAxis : uint8 {
		NK_ROWS,   ///< bandes de lignes (découpe en hauteur)
		NK_COLUMNS ///< bandes de colonnes (découpe en largeur)
	};

	class NKENTSEU_IMAGE_API NkImagePalette {
		public:
			/**
			 * Quantification par k-means (k <= 64, pixels d'alpha >= 128 seulement).
			 *
			 * Problème : trouver k couleurs c_1..c_k qui minimisent la somme, sur les
			 * pixels p, de la distance au carré au centre le plus proche,
			 *     J = somme_p  min_j || p - c_j ||^2 .
			 * Algorithme de Lloyd (1982), répété jusqu'à stabilité ou `iterations` :
			 *   1. affectation : chaque pixel va au centre le plus proche ;
			 *   2. mise à jour : chaque centre devient la moyenne de ses pixels.
			 * J ne peut que décroître à chaque tour : l'algorithme converge, vers un
			 * minimum LOCAL, qui dépend de l'initialisation. Initialisation choisie :
			 * premier centre tiré par un NkLcg32 de graine `graine` (rejouable), puis
			 * chaque centre suivant est le pixel le plus éloigné des centres déjà
			 * posés (« maximin ») : les couleurs rares mais distinctes ne sont pas
			 * absorbées. Au plus 65 536 pixels sont échantillonnés (pas régulier).
			 * `sortie` reçoit l'image dont chaque pixel opaque est remplacé par son
			 * centre ; l'alpha est conservé. `palette` (facultatif) reçoit les centres.
			 */
			static bool Quantize(const NkImage &source, int32 k, NkImage &sortie, NkVector<math::NkColor> *palette = nullptr,
								 uint32 graine = 1u, int32 iterations = 12);

			/// Couleur de fond : médiane, canal par canal, des pixels du bord de l'image.
			static math::NkColor EstimateBg(const NkImage &image);

			/**
			 * Rend transparents les pixels proches de `fond` (écart maximal par canal
			 * <= tolerance) ET reliés au bord par un chemin de pixels du même type
			 * (remplissage par diffusion, 4 voisins). Un blanc à l'INTÉRIEUR du dessin
			 * (une assiette, un œil) reste donc opaque. L'image doit avoir un alpha ;
			 * sinon elle est convertie en RGBA32. Rend le nombre de pixels effacés.
			 */
			static int32 RemoveBackground(NkImage &image, const math::NkColor &fond, int32 tolerance);

			/**
			 * Bandes occupées. Un pixel est « occupé » s'il est opaque (alpha >= 128)
			 * et plus loin que `tolerance` du fond. Chaque ligne (axe NK_ROWS) ou
			 * colonne est occupée si elle contient un pixel occupé dans `zone`
			 * (image entière si nullptr). Des lignes occupées consécutives forment une
			 * bande ; deux bandes séparées par moins de `ecartMin` lignes vides sont
			 * fusionnées (un dessin à trous reste d'un seul tenant) ; une bande de
			 * moins de `tailleMin` lignes est ignorée (poussière, filigrane fin).
			 * Les positions rendues sont dans le repère de l'image entière.
			 */
			static void Spans(const NkImage &image, const math::NkColor &fond, int32 tolerance, NkImageAxis axe,
							  NkVector<NkImageSpan> &sortie, int32 ecartMin = 1, int32 tailleMin = 1,
							  const math::NkIntRect *zone = nullptr);
	};

} // namespace nkentseu
