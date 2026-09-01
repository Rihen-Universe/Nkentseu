// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Ui/NkUnkenyGeometrie.cpp
// DESCRIPTION: Le calcul de mise en page. Aucun dessin.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Ui/NkUnkenyGeometrie.h"

namespace nkentseu {
	namespace unkeny {

		void NkPlanEcran::Calculer(const renderer::NkLayoutInfo &info, int32 bandes) noexcept {
			const float32 W = static_cast<float32>(info.width);
			const float32 H = static_cast<float32>(info.height);

			// ⚠️ On s'ancre sur la ZONE SURE pour tout ce qui se lit ou se
			// touche. Sous l'encoche ou l'indicateur de geste, un bouton n'est
			// pas seulement mal place : il est INATTEIGNABLE.
			const float32 haut = info.safeArea.top;
			const float32 bas = info.safeArea.bottom;
			const float32 gauche = info.safeArea.left;
			const float32 droite = info.safeArea.right;

			marge = W * 0.03f;
			const int32 nbBandes = bandes < 1 ? 1 : (bandes > 2 ? 2 : bandes);

			// La hauteur du bandeau suit l'orientation : en paysage la hauteur
			// est rare, donc le bandeau prend une plus grande FRACTION mais reste
			// plus petit en pixels.
			const float32 hBandeau = (H - haut - bas) * (info.IsPortrait() ? 0.11f : 0.14f);
			bandeau = NkRect{gauche + marge, haut + marge, W - gauche - droite - marge * 2.f, hBandeau};

			// L'aire est CARREE : elle prend la plus petite des deux places
			// disponibles. C'est ce qui la fait tenir dans les deux orientations
			// sans deux mises en page separees a maintenir.
			const float32 hBande = hBandeau * 0.72f;
			const float32 placeBasse = static_cast<float32>(nbBandes) * hBande +
									   static_cast<float32>(nbBandes) * marge * 0.6f;
			const float32 dispoW = W - gauche - droite - marge * 2.f;
			const float32 dispoH = H - haut - bas - hBandeau - marge * 2.f - placeBasse;
			const float32 cote = dispoW < dispoH ? dispoW : dispoH;

			aire = NkRect{gauche + (W - gauche - droite - cote) * 0.5f, bandeau.y + bandeau.h + marge, cote, cote};

			float32 y = aire.y + cote + marge * 0.6f;
			bande1 = NkRect{aire.x, y, cote, hBande};
			y += hBande + marge * 0.5f;
			// La seconde bande existe TOUJOURS en tant que rectangle, meme quand
			// elle n'est pas demandee : une zone de hauteur nulle ne recoit aucun
			// clic (NkDansRect exclut la borne haute), donc elle est inoffensive.
			// Rendre un rectangle non initialise serait, lui, dangereux.
			bande2 = NkRect{aire.x, y, cote, nbBandes >= 2 ? hBande : 0.f};

			// Le bouton de retour occupe le coin droit du bandeau : c'est la
			// seule place qui ne bouge pas d'un ecran a l'autre.
			const float32 cote2 = bandeau.h * 0.56f;
			retour = NkRect{bandeau.x + bandeau.w - cote2 - bandeau.h * 0.22f,
							bandeau.y + (bandeau.h - cote2) * 0.5f, cote2, cote2};
		}

	} // namespace unkeny
} // namespace nkentseu
