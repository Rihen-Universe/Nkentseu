#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtHingeIK.h — L'IK A DEUX OS DONT LE MILIEU RESTE UNE CHARNIERE
// (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le corps du patient virtuel de PV3DE passe sur NKAnima
// (decision de Rodolf, 13/10). PV3DE avait sa propre IK des bras, ecrite pour
// qu'un coude ne fasse que PLIER ; NKAnima n'offrait que NkRtSolveTwoBone
// (NkFootIK.h), ecrite pour les jambes. La regle vient donc ici, et PV3DE
// l'appelle.
//
// CE QUI MANQUAIT. NkRtSolveTwoBone tourne l'os du haut, puis l'os du bas, chacun
// par le plus court chemin. Le bout arrive a la cible, mais l'os du bas a tourne
// autour d'un axe quelconque : un coude qui se tord ou qui plie de cote. Un coude
// (un genou) est une charniere.
//
// LA REGLE (NkRtSolveTwoBoneHinge) :
//   1. l'os du haut (`a`) tourne pour que le milieu (`b`) aille a sa place (loi
//      des cosinus, du cote de `poleDir`) ;
//   2. il ROULE autour de son axe (a -> b) pour amener le bout (`c`) dans le plan
//      (a, b, cible) -- c'est l'os du haut qui prend le roulis (un humerus tourne
//      sur son axe ; ses os de torsion le repartissent : NkRtTwist.h) ;
//   3. l'os du milieu ne fait plus que plier dans ce plan.
//
// Fonction pure, sur des matrices MONDE, sans rien supposer des reperes des os.
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtRig.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			/// L'allonge par defaut : le bout ne va pas au-dela de 99,9 % de la longueur
			/// des deux os (un membre tendu a fond n'a plus de plan de flexion).
			constexpr float32 kNkRtHingeReach = 0.999f;

			/// L'IK a deux os a charniere : la chaine `a` (epaule, hanche), `b` (coude,
			/// genou), `c` (poignet, cheville) est tournee pour que `c` atteigne `target`,
			/// le milieu du cote de `poleDir`. Les sous-arbres sont rafraichis depuis
			/// `local` (qui n'est PAS mis a jour : l'appelant relit le local de `a` et de
			/// `b` dans `world`). Rend la distance restante a la cible.
			float32 NkRtSolveTwoBoneHinge(const NkRtRig &rig, const math::NkMat4f *local, math::NkMat4f *world, uint32 a, uint32 b,
										  uint32 c, const NkVec3f &target, const NkVec3f &poleDir, float32 reach = kNkRtHingeReach);

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
