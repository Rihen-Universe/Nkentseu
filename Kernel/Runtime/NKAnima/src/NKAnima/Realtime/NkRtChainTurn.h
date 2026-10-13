#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtChainTurn.h — UNE ROTATION REPARTIE SUR UNE CHAINE D'OS (le regard :
// le haut du tronc, le cou, la tete) (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le patient virtuel de PV3DE tourne la tete : pas la tete
// seule (un cou qui reste droit sous une tete qui pivote se voit tout de suite),
// mais un peu le haut du thorax, davantage le cou, le reste la tete. La regle etait
// ecrite a la main dans PV3DE (PvPoserHaut) ; NKAnima n'avait rien d'equivalent en
// temps reel. Elle est ici.
//
// LA REGLE. La rotation voulue est dite en TROIS ANGLES autour de trois axes du
// MONDE (le lacet autour de la verticale, le tangage autour de l'axe lateral, le
// roulis autour de l'avant), composes dans cet ordre : R = R0 * R1 * R2. L'os `k`
// de la chaine en prend la part `shares[k]` : la rotation faite des trois angles
// MULTIPLIES par sa part -- pas une fraction de la rotation entiere (un quart de
// lacet et un quart de tangage, c'est ce qu'un cou fait). Cette rotation du monde
// est donnee a l'os comme si son parent etait encore dans sa pose de reference :
//
//        local = parent^-1 * rotation * parent * reference
//
// Autour d'UN seul axe les parts s'ajoutent exactement le long de la chaine (le
// dernier os a tourne de la somme des parts). Avec plusieurs angles, des fractions
// d'angles ne se composent pas exactement : le dernier os arrive a quelques degres
// de la rotation entiere (mesure dans le banc, TC2) -- c'est voulu, la tete ne vise
// rien, elle se tourne.
//
// CE QUE CE FICHIER OFFRE, en fonctions pures, sans rien supposer des reperes :
//   - NkRtTurnRotation    : la rotation du monde d'une part ;
//   - NkRtTurnBoneInWorld : la rotation locale d'un os qui prend une rotation du monde ;
//   - NkRtDistributeTurn  : la chaine entiere.
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			/// UNE ROTATION EN TROIS ANGLES (radians) autour de trois axes UNITAIRES du monde,
			/// composes dans l'ordre : R = R(axes[0], angles[0]) * R(axes[1], angles[1]) *
			/// R(axes[2], angles[2]).
			struct NkRtTurn {
					NkVec3f axes[3] = {{0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}};
					float32 angles[3] = {0.f, 0.f, 0.f};
			};

			/// La rotation du monde de la part `share` : les trois angles multiplies par elle.
			[[nodiscard]] NkRtQuat NkRtTurnRotation(const NkRtTurn &turn, float32 share = 1.f) noexcept;

			/// LA ROTATION LOCALE D'UN OS QUI PREND UNE ROTATION DU MONDE, son parent etant
			/// oriente `parentWorld` : parentWorld^-1 * worldRotation * parentWorld * local.
			[[nodiscard]] NkRtQuat NkRtTurnBoneInWorld(const NkRtQuat &parentWorld, const NkRtQuat &worldRotation, const NkRtQuat &local) noexcept;

			/// REPARTIT LA ROTATION SUR LA CHAINE. Pour chaque os `k` (de la base vers le
			/// bout) : `shares[k]` sa part, `parentWorld[k]` l'orientation de SON parent dans
			/// la pose de reference, `refLocal[k]` sa rotation locale de reference ;
			/// `outLocal[k]` recoit sa rotation locale. Sans angle (les trois a zero), ou pour
			/// une part nulle : la reference, AU BIT PRES.
			void NkRtDistributeTurn(const NkRtTurn &turn, const float32 *shares, const NkRtQuat *parentWorld, const NkRtQuat *refLocal, uint32 count,
									NkRtQuat *outLocal) noexcept;

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
