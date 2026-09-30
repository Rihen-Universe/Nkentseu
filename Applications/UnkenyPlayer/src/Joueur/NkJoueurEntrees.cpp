//
// NkJoueurEntrees.cpp
// =============================================================================
// Description :
//   Le relais clavier -> actions du joueur autonome (voir l'en-tete : il cede
//   la place a unkeny::NkEntreesJeu a la fusion de comble/entrees-jeu).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Joueur/NkJoueurEntrees.h"

#include "NKEvent/NkKeyboardEvent.h"

namespace nkentseu {
	namespace joueur {

		namespace {
			bool Tenue(const NkJoueurEntrees &e, NkKey k) noexcept {
				const uint32 i = static_cast<uint32>(k);
				return i < 512u && e.tenues[i];
			}

			void Poser(NkJoueurEntrees &e, NkKey k, bool bas) noexcept {
				const uint32 i = static_cast<uint32>(k);
				if (i < 512u) {
					e.tenues[i] = bas;
				}
			}
		} // namespace

		NkDemandeJoueur NkJoueurEntreesEvenement(NkJoueurEntrees &e, const NkEvent &evenement) {
			if (const auto *p = evenement.As<NkKeyPressEvent>()) {
				const NkKey k = p->GetKey();
				Poser(e, k, true);
				// Les touches de la COQUILLE : elles ne vont pas au jeu. Echap
				// quitte sur ordinateur ; sur Android, le « retour » non consomme
				// quitte deja (NkCanvasApp), il n'y a rien a ajouter.
				if (k == NkKey::NK_ESCAPE) {
					return NkDemandeJoueur::NK_QUITTER;
				}
				if (k == NkKey::NK_P) {
					return NkDemandeJoueur::NK_BASCULER_PAUSE;
				}
				return NkDemandeJoueur::NK_RIEN;
			}
			if (const auto *r = evenement.As<NkKeyReleaseEvent>()) {
				Poser(e, r->GetKey(), false);
			}
			return NkDemandeJoueur::NK_RIEN;
		}

		void NkJoueurEntreesTrame(NkJoueurEntrees &e) {
			unkeny::NkActions &a = e.actions;
			a.NouvelleTrame();
			const bool gauche = Tenue(e, NkKey::NK_A) || Tenue(e, NkKey::NK_LEFT);
			const bool droite = Tenue(e, NkKey::NK_D) || Tenue(e, NkKey::NK_RIGHT);
			const bool bas = Tenue(e, NkKey::NK_S) || Tenue(e, NkKey::NK_DOWN);
			const bool haut = Tenue(e, NkKey::NK_W) || Tenue(e, NkKey::NK_UP);
			a.PoserAxe(NK_JOUEUR_AVANCER, gauche, droite);
			a.PoserAxe(NK_JOUEUR_MONTER, bas, haut);
			a.Poser(NK_JOUEUR_SAUTER, Tenue(e, NkKey::NK_SPACE) ? 1.f : 0.f);
			a.Poser(NK_JOUEUR_ACTION, Tenue(e, NkKey::NK_E) ? 1.f : 0.f);
		}

		void NkJoueurEntreesRelacher(NkJoueurEntrees &e) {
			for (uint32 i = 0; i < 512u; ++i) {
				e.tenues[i] = false;
			}
			e.actions.ToutRelacher();
		}

	} // namespace joueur
} // namespace nkentseu
