// =============================================================================
// NkUnkenyActions.h — le jeu parle d'ACTIONS, jamais de touches
//
// A QUOI SERT CE FICHIER
//   Traduire « la touche D est enfoncee », « le stick est a droite » et « le
//   doigt est sur la moitie droite » en UNE seule chose : `Avancer` vaut 1.
//
// ⚠️ POURQUOI C'EST LA BRIQUE QU'ON REGRETTE DE NE PAS AVOIR ECRITE
//   Un jeu qui teste `NkInput.IsKeyDown(NK_D)` dans sa logique se condamne a
//   trois choses :
//     - il n'est pas reconfigurable (pas de touches personnalisees) ;
//     - il n'est pas jouable a la manette ni au doigt sans reecrire la logique ;
//     - il n'est pas TESTABLE : un banc ne peut pas appuyer sur une touche, mais
//       il peut poser une action.
//   Le troisieme point est le plus couteux et le moins prevu. Avec des actions,
//   un banc joue une partie entiere sans fenetre.
//
// ⚠️ UNE ACTION EST UN AXE, PAS UN BOOLEEN
//   `Avancer` vaut -1..1, pas vrai/faux. Un clavier donne -1, 0 ou 1 ; un stick
//   donne 0,37. Choisir le booleen d'abord oblige a tout reecrire le jour ou une
//   manette arrive — et ce jour arrive. Les actions binaires (sauter, tirer)
//   restent lisibles par `Enfoncee()`.
//
// ⚠️ ET IL FAUT LES TROIS ETATS
//   `Enfoncee` (maintenue), `VientDEtrePressee` (ce tour-ci), `VientDEtreRelachee`.
//   Un saut declenche sur `Enfoncee` se repete a chaque trame ; c'est le premier
//   defaut de tout jeu qui n'a que deux etats.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une source (manette, gyroscope) -> une methode Poser* de plus
//   - une action de jeu               -> chez le jeu, par son propre index
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		/// Nombre maximal d'actions. 32 couvre largement un jeu 2D ; le jeu
		/// definit ses propres index par une enumeration a lui.
		static const int32 NK_UNKENY_ACTIONS_MAX = 32;

		struct NkAction {
				float32 valeur = 0.f;	  ///< -1..1 pour un axe, 0/1 pour un bouton
				float32 valeurAvant = 0.f;
				float32 seuil = 0.5f;	  ///< au-dela, l'action compte comme enfoncee

				bool Enfoncee() const noexcept {
					return math::NkAbs(valeur) >= seuil;
				}
				bool EtaitEnfoncee() const noexcept {
					return math::NkAbs(valeurAvant) >= seuil;
				}
				bool VientDEtrePressee() const noexcept {
					return Enfoncee() && !EtaitEnfoncee();
				}
				bool VientDEtreRelachee() const noexcept {
					return !Enfoncee() && EtaitEnfoncee();
				}
		};

		/// La table des actions du jeu.
		///
		/// Le jeu ecrit dedans depuis ses entrees (clavier, manette, doigt), et
		/// lit ensuite des actions. Le moteur ne connait aucune touche : c'est ce
		/// qui rend un jeu reconfigurable ET testable sans fenetre.
		class NkActions {
			public:
				/// A appeler UNE FOIS par trame, AVANT d'ecrire les nouvelles
				/// valeurs. C'est ce basculement qui rend `VientDEtrePressee`
				/// possible ; l'oublier fait que rien ne se declenche jamais.
				void NouvelleTrame() noexcept {
					for (int32 i = 0; i < NK_UNKENY_ACTIONS_MAX; ++i) {
						mActions[i].valeurAvant = mActions[i].valeur;
					}
				}

				void Poser(int32 action, float32 valeur) noexcept {
					if (action >= 0 && action < NK_UNKENY_ACTIONS_MAX) {
						mActions[action].valeur = math::NkClamp(valeur, -1.f, 1.f);
					}
				}

				/// Deux touches opposees en un seul axe. C'est la forme qu'on
				/// veut : gauche et droite enfoncees ensemble donnent ZERO, pas
				/// un comportement indefini.
				void PoserAxe(int32 action, bool negatif, bool positif) noexcept {
					Poser(action, (positif ? 1.f : 0.f) - (negatif ? 1.f : 0.f));
				}

				const NkAction &operator[](int32 action) const noexcept {
					static const NkAction vide;
					if (action < 0 || action >= NK_UNKENY_ACTIONS_MAX) {
						return vide;
					}
					return mActions[action];
				}

				float32 Valeur(int32 action) const noexcept {
					return (*this)[action].valeur;
				}
				bool Enfoncee(int32 action) const noexcept {
					return (*this)[action].Enfoncee();
				}
				bool VientDEtrePressee(int32 action) const noexcept {
					return (*this)[action].VientDEtrePressee();
				}
				bool VientDEtreRelachee(int32 action) const noexcept {
					return (*this)[action].VientDEtreRelachee();
				}

				/// Deux actions lues comme une direction, NORMALISEE au-dela de 1.
				/// ⚠️ Sans normalisation, se deplacer en diagonale au clavier est
				/// 41 % plus rapide qu'en ligne droite — c'est le defaut le plus
				/// ancien et le plus repandu des jeux 2D.
				NkVec2f Direction(int32 axeX, int32 axeY) const noexcept {
					NkVec2f d(Valeur(axeX), Valeur(axeY));
					const float32 l2 = d.x * d.x + d.y * d.y;
					if (l2 > 1.f) {
						const float32 l = math::NkSqrt(l2);
						d.x /= l;
						d.y /= l;
					}
					return d;
				}

				void ToutRelacher() noexcept {
					for (int32 i = 0; i < NK_UNKENY_ACTIONS_MAX; ++i) {
						mActions[i].valeur = 0.f;
					}
				}

				void PoserSeuil(int32 action, float32 seuil) noexcept {
					if (action >= 0 && action < NK_UNKENY_ACTIONS_MAX) {
						mActions[action].seuil = seuil;
					}
				}

			private:
				NkAction mActions[NK_UNKENY_ACTIONS_MAX];
		};

	} // namespace unkeny
} // namespace nkentseu
