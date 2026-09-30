//
// NkJoueurEntrees.cpp
// =============================================================================
// Description :
//   Les entrees du joueur autonome (voir l'en-tete) : une facade sur
//   unkeny::NkEntreesJeu, qui calcule tout sur la carte du noyau.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Joueur/NkJoueurEntrees.h"

#include "NKEvent/NkKeyboardEvent.h"
#include "Unkeny/Jeu/NkUnkenyControleurs.h"

namespace nkentseu {
	namespace joueur {

		NkJoueurEntrees::NkJoueurEntrees() noexcept {
			unkeny::NkLiaisonsStandard(jeu.Liaisons());
		}

		unkeny::NkRapportLiaisons NkJoueurEntreesLire(NkJoueurEntrees &e, const NkString &texte) {
			unkeny::NkLiaisons &l = e.jeu.Liaisons();
			if (texte.Empty()) {
				unkeny::NkLiaisonsStandard(l);
				e.pause = NK_JOUEUR_PAUSE;
				return unkeny::NkRapportLiaisons();
			}
			unkeny::NkNommerActionsStandard(l);
			const unkeny::NkRapportLiaisons r = l.Lire(texte);
			// Pause par son NOM : un jeu qui l'a renommee ou deplacee reste pausable.
			const int32 p = unkeny::NkIndiceAction(l, "Pause");
			e.pause = p >= 0 ? p : NK_JOUEUR_PAUSE;
			return r;
		}

		uint32 NkJoueurEntreesBrancher(NkJoueurEntrees &e, unkeny::NkScene &scene) {
			return unkeny::NkAjouterControleurs2D(scene, &e.jeu.Actions(0));
		}

		void NkJoueurEntreesSurface(NkJoueurEntrees &e, float32 largeur, float32 hauteur) {
			e.jeu.PoserSurface(0.f, 0.f, largeur, hauteur);
		}

		NkDemandeJoueur NkJoueurEntreesEvenement(NkJoueurEntrees &e, const NkEvent &evenement) {
			// La touche de la COQUILLE : elle ne va pas au jeu. Echap quitte sur
			// ordinateur ; sur Android, le « retour » non consomme quitte deja
			// (NkCanvasApp), il n'y a rien a ajouter.
			if (const auto *p = evenement.As<NkKeyPressEvent>()) {
				if (p->GetKey() == NkKey::NK_ESCAPE) {
					return NkDemandeJoueur::NK_QUITTER;
				}
			}
			(void)e.jeu.Lire(evenement);
			return NkDemandeJoueur::NK_RIEN;
		}

		NkDemandeJoueur NkJoueurEntreesTrame(NkJoueurEntrees &e, const NkGamepadSystem *manettes, float32 dt) {
			e.jeu.Trame(manettes, dt);
			return e.jeu.Actions(0).VientDEtrePressee(e.pause) ? NkDemandeJoueur::NK_BASCULER_PAUSE : NkDemandeJoueur::NK_RIEN;
		}

		void NkJoueurEntreesRelacher(NkJoueurEntrees &e) {
			e.jeu.ToutRelacher();
		}

	} // namespace joueur
} // namespace nkentseu
