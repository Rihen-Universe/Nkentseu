//
// NkUnkenyEntreesJeu.cpp
// =============================================================================
// Description :
//   Clavier, souris, doigts et manettes -> NkActions, par la table de
//   liaisons. Depuis le 30/09, le CALCUL est celui du noyau (NkInputMap, une
//   carte par joueur) ; ce fichier ne fait plus que traduire la table et
//   recopier les valeurs dans les NkActions d'Unkeny.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Entree/NkUnkenyEntreesJeu.h"

namespace nkentseu {
	namespace unkeny {

		NkEntreesJeu::NkEntreesJeu() noexcept {
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				mManetteDe[j] = j;
				mCartes[j].SetGamepad(j);
				// Un contexte qui NE consomme PAS : la table d'Unkeny ADDITIONNE
				// toutes ses liaisons, aucune n'en cache une autre.
				mContexte = mCartes[j].AddContext("Unkeny", 0, false);
			}
		}

		NkActions &NkEntreesJeu::Actions(int32 joueur) noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return mActions[0];
			}
			return mActions[joueur];
		}

		const NkActions &NkEntreesJeu::Actions(int32 joueur) const noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return mActions[0];
			}
			return mActions[joueur];
		}

		NkInputMap &NkEntreesJeu::Carte(int32 joueur) noexcept {
			Synchroniser();
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return mCartes[0];
			}
			return mCartes[joueur];
		}

		void NkEntreesJeu::AssignerManette(int32 joueur, int32 manette) noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return;
			}
			mManetteDe[joueur] = (manette >= 0 && manette < static_cast<int32>(NK_MAX_GAMEPADS)) ? manette : -1;
			mCartes[joueur].SetGamepad(mManetteDe[joueur] >= 0 ? mManetteDe[joueur] : NK_INPUT_NO_GAMEPAD);
		}

		int32 NkEntreesJeu::ManetteDe(int32 joueur) const noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return -1;
			}
			return mManetteDe[joueur];
		}

		void NkEntreesJeu::PoserSurface(float32 x, float32 y, float32 l, float32 h) noexcept {
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				mCartes[j].SetScreenSurface(x, y, l, h);
			}
		}

		bool NkEntreesJeu::ToucheTenue(NkKey touche) const noexcept {
			return mCartes[0].IsKeyHeld(touche);
		}

		uint32 NkEntreesJeu::DoigtsSuivis() const noexcept {
			return mCartes[0].TouchCount();
		}

		void NkEntreesJeu::Synchroniser() noexcept {
			if (mLiaisons.Version() == mVersionVue) {
				return;
			}
			mVersionVue = mLiaisons.Version();
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				mLiaisons.RemplirCarte(mCartes[j], mContexte, j, &mOrigine[j]);
			}
		}

		// =====================================================================
		// La capture d'une entree (le menu « appuyez sur une touche »)
		// =====================================================================

		bool NkEntreesJeu::CapturerProchaineEntree(int32 indice) noexcept {
			if (indice < 0 || indice >= mLiaisons.Nombre() ||
				mLiaisons[indice].nature != NkNatureLiaison::NK_ENTREE) {
				return false;
			}
			Synchroniser();
			// La premiere carte qui porte cette liaison fait la capture : c'est la
			// manette de SON joueur qui peut repondre.
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				for (uint32 k = 0; k < mOrigine[j].Size(); ++k) {
					if (mOrigine[j][k] != indice) {
						continue;
					}
					// Echap ne fait pas exception : la table d'Unkeny l'accepte comme
					// n'importe quelle touche, et c'est son contrat d'avant.
					if (!mCartes[j].BeginRebind(mContexte, static_cast<int32>(k), -1, false)) {
						return false;
					}
					mCapture = indice;
					mCaptureCarte = j;
					return true;
				}
			}
			return false;
		}

		void NkEntreesJeu::AnnulerCapture() noexcept {
			if (mCaptureCarte >= 0) {
				mCartes[mCaptureCarte].CancelRebind();
			}
			mCapture = -1;
			mCaptureCarte = -1;
		}

		void NkEntreesJeu::TerminerCapture() noexcept {
			if (mCapture < 0 || mCaptureCarte < 0 || mCartes[mCaptureCarte].IsRebinding()) {
				return;
			}
			const NkInputMap &carte = mCartes[mCaptureCarte];
			if (carte.LastRebindSucceeded()) {
				// La table reste la VERITE : on y ecrit, et les cartes suivront.
				mLiaisons.Relier(mCapture, carte.LastRebindCode());
			}
			mCapture = -1;
			mCaptureCarte = -1;
		}

		// =====================================================================
		// Les evenements et la trame
		// =====================================================================

		bool NkEntreesJeu::Lire(const NkEvent &e) noexcept {
			Synchroniser();
			if (mCapture >= 0 && mCaptureCarte >= 0) {
				// Pendant une capture, l'entree est a la capture SEULE : aucune
				// autre carte ne doit la croire tenue.
				mCartes[mCaptureCarte].Read(e);
				TerminerCapture();
				return true;
			}
			bool pris = false;
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				pris = mCartes[j].Read(e) || pris;
			}
			return pris;
		}

		void NkEntreesJeu::Trame(const NkGamepadSystem *manettes) noexcept {
			Trame(manettes, 1.f / 60.f);
		}

		void NkEntreesJeu::Trame(const NkGamepadSystem *manettes, float32 dt) noexcept {
			Synchroniser();
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				NkInputMap &carte = mCartes[j];
				carte.Update(dt, manettes);
				NkActions &actions = mActions[j];
				actions.NouvelleTrame();
				// ⚠️ SEULES LES ACTIONS LIEES sont reecrites. Une action qu'aucune
				//    liaison ne vise reste celle que le programme a posee : un
				//    banc, un script ou une IA la pilotent sans qu'une trame la
				//    remette a zero derriere eux.
				for (int32 a = 0; a < NK_UNKENY_ACTIONS_MAX && a < carte.ActionCount(); ++a) {
					if (carte.IsBound(a)) {
						actions.Poser(a, carte.Value(a));
					}
				}
			}
			TerminerCapture();
		}

		void NkEntreesJeu::ToutRelacher() noexcept {
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				mCartes[j].CancelRebind();
				mCartes[j].ReleaseAll();
				mActions[j].ToutRelacher();
			}
			mCapture = -1;
			mCaptureCarte = -1;
		}

	} // namespace unkeny
} // namespace nkentseu
