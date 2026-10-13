#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Blend/NkPoseMirror.h — LE MIROIR D'UNE POSE (gauche <-> droite) (13/10/2026).
//
// D'OU VIENT CE FICHIER. Le patient virtuel de PV3DE ferme le poing de la main
// DROITE de l'artiste ; la main gauche fait le meme geste EN MIROIR. Ce miroir
// etait ecrit a la main dans PV3DE ((x, y, z, w) -> (-x, -y, z, w), mesure sur le
// Biped). La regle generale est ici, a cote du melange de poses (NkAnimMix.h).
//
// LA REGLE. Deux os symetriques (la main gauche, la main droite) ont des reperes
// LOCAUX images l'un de l'autre par un plan : sa normale `n`, dite dans le repere
// local, est une donnee du RIG (X pour un rig construit par symetrie X dont les
// reperes suivent le monde ; Z pour le Biped de 3ds Max, mesure). Le miroir d'une
// rotation R par ce plan est S * R * S (S = I - 2 n n^T, un retournement) : encore
// une rotation, de meme angle, d'axe -S(axe). En quaternion (v, w) :
//
//        (v, w)  ->  (-v + 2 (v . n) n,  w)
//
// et une position t devient S t = t - 2 (t . n) n ; l'echelle est gardee.
//
// CE QUE CE FICHIER OFFRE, en fonctions pures :
//   - NkMirrorRotation : le miroir d'une rotation locale ;
//   - NkMirrorBone     : le miroir d'une place locale (T, R, S) ;
//   - NkMirrorPose     : la pose miroir, os par os, d'apres la table des paires.
// Il ne devine PAS quelle normale convient a un rig : l'appelant la donne.
// =============================================================================

#include "NKAnima/Blend/NkAnimMix.h"

namespace nkentseu {
	namespace anim {

		/// LE MIROIR D'UNE ROTATION LOCALE par le plan de normale `normal` (unitaire, dans
		/// le repere local) : (v, w) -> (-v + 2 (v . n) n, w). L'identite reste l'identite ;
		/// applique deux fois, il rend la rotation de depart (exactement si la normale est
		/// un axe du repere).
		[[nodiscard]] NkQuatf NkMirrorRotation(const NkQuatf &q, const NkVec3f &normal) noexcept;

		/// LE MIROIR D'UNE PLACE LOCALE : la position par le plan, la rotation par
		/// NkMirrorRotation, l'echelle telle quelle.
		[[nodiscard]] NkBoneTRS NkMirrorBone(const NkBoneTRS &b, const NkVec3f &normal) noexcept;

		/// LA POSE MIROIR. `pairs[j]` : l'os dont l'os `j` de `out` recoit le miroir
		/// (l'os de l'autre cote ; lui-meme pour un os du milieu) ; negatif ou hors de la
		/// pose : l'os `j` est copie tel quel. `out` prend la taille de `in` ; les morphs
		/// et les proprietes sont copies. `in` et `out` peuvent etre la meme pose.
		void NkMirrorPose(const NkAnimPose &in, const NkVector<int32> &pairs, const NkVec3f &normal, NkAnimPose &out);

	} // namespace anim
} // namespace nkentseu
