// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Ui/NkUnkenyWidgets.cpp
// DESCRIPTION: Boutons et panneaux. Ils dessinent, ils ne testent rien.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Ui/NkUnkenyWidgets.h"

namespace nkentseu {
	namespace unkeny {

		void NkPanneau(NkGuiDrawList &dl, const NkRect &box, const NkTheme &th, bool actif) {
			// L'arrondi est une FRACTION de la hauteur : une valeur en pixels
			// serait fausse des que la taille change, donc a chaque rotation.
			const float32 r = box.h * th.arrondi;
			dl.AddRectFilled(box, actif ? th.panneauActif : th.panneau, r);
			dl.AddRect(box, th.bord, th.epaisseurBord, r);
		}

		void NkBouton(NkGuiDrawList &dl, const NkRect &box, NkGuiFont *police, const char *libelle, const NkTheme &th,
					  bool actif) {
			NkPanneau(dl, box, th, actif);
			renderer::NkTexteDansBoite(dl, police, box, libelle, th.texte);
		}

		void NkBoutonPastille(NkGuiDrawList &dl, const NkRect &box, NkGuiFont *police, const char *libelle,
							  const NkColor &pastille, const NkTheme &th, bool actif, bool souligne) {
			const float32 r = box.h * th.arrondi;
			dl.AddRectFilled(box, actif ? th.panneauActif : th.panneau, r);
			// Le liseré d'accent marque celui dont c'est le tour : c'est
			// l'information qu'on cherche en permanence quand plusieurs joueurs
			// partagent l'ecran.
			dl.AddRect(box, souligne ? th.accent : th.bord, souligne ? th.epaisseurBord * 1.7f : th.epaisseurBord, r);

			dl.AddCircleFilled(NkVec2f(box.x + box.h * 0.40f, box.y + box.h * 0.5f), box.h * 0.19f, pastille);

			// Le texte commence APRES la pastille et s'arrete avant le bord : un
			// libelle qui passe sous la pastille est illisible, et celui qui
			// deborde a droite fait croire a un defaut de mise en page.
			const NkRect zone{box.x + box.h * 0.66f, box.y, box.w - box.h * 0.72f, box.h};
			renderer::NkTexteDansBoite(dl, police, zone, libelle, actif ? th.texte : th.texteFaible);
		}

		void NkBoutonMenu(NkGuiDrawList &dl, const NkRect &box, const NkTheme &th) {
			NkPanneau(dl, box, th, true);
			for (int32 i = 0; i < 3; ++i) {
				const float32 y = box.y + box.h * (0.32f + static_cast<float32>(i) * 0.18f);
				dl.AddRectFilled(NkRect{box.x + box.w * 0.26f, y, box.w * 0.48f, box.h * 0.07f}, th.texte,
								 box.h * 0.035f);
			}
		}

		NkRect NkVoileEtPanneau(NkGuiDrawList &dl, const NkRect &ecran, const NkTheme &th, float32 largeur,
								float32 hauteur) {
			// Le voile couvre TOUT l'ecran, zone sure comprise : c'est un fond,
			// et un fond va jusqu'au bord. Seul ce qui se lit ou se touche
			// s'ancre sur la zone sure.
			dl.AddRectFilled(ecran, th.voile);

			const float32 w = ecran.w * largeur;
			const float32 h = ecran.h * hauteur;
			const NkRect panneau{ecran.x + (ecran.w - w) * 0.5f, ecran.y + (ecran.h - h) * 0.5f, w, h};
			dl.AddRectFilled(panneau, th.panneau, h * 0.14f);
			dl.AddRect(panneau, th.or_, 2.f, h * 0.14f);
			return panneau;
		}

	} // namespace unkeny
} // namespace nkentseu
