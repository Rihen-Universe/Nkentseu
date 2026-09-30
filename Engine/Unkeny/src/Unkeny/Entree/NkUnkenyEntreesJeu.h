//
// NkUnkenyEntreesJeu.h
// =============================================================================
// Description :
//   Les SOURCES des actions : clavier, souris, manettes, doigts. Elles lisent
//   les evenements de NKEvent et l'etat de NkGamepadSystem, appliquent la
//   table de liaisons (NkUnkenyLiaisons.h) et ecrivent dans les NkActions de
//   chaque joueur. Le jeu ne voit QUE les actions.
//
// Caracteristiques :
//   - L'etat du clavier et des doigts est tenu ICI, depuis les evenements
//     qu'on lui donne, et pas lu dans NkInput : c'est ce qui permet a un
//     editeur de ne donner au jeu que ce qui lui revient (la vue active en
//     jeu), et a un banc de jouer sans fenetre.
//   - Plusieurs liaisons sur une meme action s'ADDITIONNENT, puis la somme est
//     bornee a [-1, 1] : Gauche et Droite ensemble donnent zero (la regle de
//     PoserAxe), un stick a 0,37 et une touche relachee donnent 0,37.
//   - Zone morte RADIALE pour les sticks (sur la longueur du vecteur X,Y),
//     axiale pour les gachettes : une zone morte axiale sur un stick « colle »
//     la direction aux axes, et une diagonale douce devient une ligne droite.
//   - Joueurs 0..3, chacun sur SA manette (joueur j -> manette j par defaut,
//     reassignable). Le clavier se partage par les liaisons (j=0 sur ZQSD,
//     j=1 sur les fleches).
//   - Joystick virtuel FLOTTANT : son origine est le point ou le doigt s'est
//     pose dans la zone ; un seul doigt par joystick, capture a l'appui.
//   - Reconfiguration : CapturerProchaineEntree(i) -> la prochaine touche,
//     le prochain bouton ou le prochain axe franchement pousse devient
//     l'entree de la liaison i.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYENTREESJEU_H__
#define __NKENTSEU_UNKENY_NKUNKENYENTREESJEU_H__

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "Unkeny/Entree/NkUnkenyActions.h"
#include "Unkeny/Entree/NkUnkenyLiaisons.h"

namespace nkentseu {
	namespace unkeny {

		class NkEntreesJeu {
			public:
				NkEntreesJeu() noexcept;

				NkLiaisons &Liaisons() noexcept {
					return mLiaisons;
				}
				const NkLiaisons &Liaisons() const noexcept {
					return mLiaisons;
				}

				/// Les actions du joueur (0..3). Hors plage : celles du joueur 0.
				NkActions &Actions(int32 joueur = 0) noexcept;
				const NkActions &Actions(int32 joueur = 0) const noexcept;

				/// Le joueur `joueur` lit la manette `manette` (0..7), -1 = aucune.
				void AssignerManette(int32 joueur, int32 manette) noexcept;
				int32 ManetteDe(int32 joueur) const noexcept;

				/// La surface ou vivent les zones d'ecran, en PIXELS CLIENT (la vue
				/// du jeu, ou sa zone sure). Sans surface, zones et joysticks sont
				/// inertes : un doigt n'y a pas de sens.
				void PoserSurface(float32 x, float32 y, float32 l, float32 h) noexcept;

				/// Lit un evenement : clavier, souris (boutons, molette), doigts.
				/// Tout le reste est ignore.
				/// @return true si l'evenement touche une liaison : l'appelant peut
				///         alors le considerer comme pris par le jeu.
				bool Lire(const NkEvent &evenement) noexcept;

				/// Une trame : bascule les trois etats de chaque action, puis la
				/// recalcule depuis le clavier, la souris, les doigts et les
				/// manettes. `manettes` peut etre nul (aucune manette).
				/// ⚠️ UNE FOIS PAR TRAME, avant la logique du jeu -- la regle de
				///    NkActions::NouvelleTrame.
				void Trame(const NkGamepadSystem *manettes) noexcept;

				/// Relache tout : touches tenues, doigts, actions. A appeler quand le
				/// jeu perd la main (fenetre quittee, « Arreter », Echap) : sinon
				/// une touche relachee pendant que le jeu ne regardait pas resterait
				/// enfoncee pour toujours.
				void ToutRelacher() noexcept;

				/// La prochaine entree (touche, bouton de souris, bouton de manette,
				/// axe pousse au-dela de la moitie) remplacera celle de la liaison
				/// `indice`. false si l'indice n'est pas une liaison d'entree.
				bool CapturerProchaineEntree(int32 indice) noexcept;
				bool CaptureEnCours() const noexcept {
					return mCapture >= 0;
				}

				bool ToucheTenue(NkKey touche) const noexcept;
				uint32 DoigtsSuivis() const noexcept;

			private:
				struct NkDoigt {
						uint64 id = 0;
						bool actif = false;
						float32 u = 0.f;  ///< position normalisee dans la surface
						float32 v = 0.f;
						float32 u0 = 0.f; ///< point d'appui (origine du joystick flottant)
						float32 v0 = 0.f;
						int32 stick = -1; ///< liaison (X) du joystick qui l'a capture, ou -1
				};

				float32 Valeur(const NkLiaison &l, int32 joueur, const NkGamepadSystem *manettes) const noexcept;
				float32 ValeurStick(const NkLiaison &l) const noexcept;
				void Normaliser(float32 x, float32 y, float32 &u, float32 &v) const noexcept;
				int32 StickSous(float32 u, float32 v) const noexcept;
				void Capturer(const NkInputCode &code) noexcept;
				void CapturerManettes(const NkGamepadSystem *manettes) noexcept;

				NkLiaisons mLiaisons;
				NkActions mActions[NK_UNKENY_JOUEURS_MAX];
				int32 mManetteDe[NK_UNKENY_JOUEURS_MAX];

				bool mTouches[static_cast<uint32>(NkKey::NK_KEY_MAX)];
				bool mSouris[static_cast<uint32>(NkMouseButton::NK_MOUSE_BUTTON_MAX)];
				float32 mMolette = 0.f;
				float32 mMoletteH = 0.f;

				NkDoigt mDoigts[NK_MAX_TOUCH_POINTS];
				float32 mSurfX = 0.f;
				float32 mSurfY = 0.f;
				float32 mSurfL = 0.f;
				float32 mSurfH = 0.f;

				int32 mCapture = -1;
				/// Ce que les manettes avaient a la trame d'avant : la capture ne
				/// prend qu'un bouton qui VIENT d'etre enfonce, pas un bouton tenu
				/// depuis l'ouverture du menu.
				bool mCaptureBoutonsAvant[NK_MAX_GAMEPADS][static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)];
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYENTREESJEU_H__
