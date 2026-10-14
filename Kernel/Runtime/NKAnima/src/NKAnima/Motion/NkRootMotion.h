#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Motion/NkRootMotion.h : LE MOUVEMENT DE LA RACINE (« root motion ») d'un clip
// (14/10/2026).
//
// D'OU VIENT CE FICHIER. Un clip de marche avance : sa racine (le bassin) part de
// z = 0 et finit a z = 1,4 m. Un jeu veut l'inverse : le personnage joue sa marche
// SUR PLACE et c'est son controleur qui avance, de ce que le clip a parcouru. Noge
// le DECLARE (`NkAnimationClip::hasRootMotion`, `NkAnimator::applyRootMotion`,
// Engine/Noge/src/Noge/ECS/Components/Animation/NkAnimation.h l. 251 et 325) et
// aucun .cpp ne le lit ; NKAnima n'avait AUCUNE occurrence de « root motion »
// (mesure du 14/10). NKScena (un acteur qui marche le long d'un plan), Nogee et
// PV3DE (le patient qui fait trois pas) en ont besoin. C'est ici, sans graphe ni
// composant : un clip et deux temps donnent un deplacement.
//
// LE MODELE. Le mouvement de la racine a l'instant t du clip est une transformation
// rigide M(t) = (lacet y(t) autour du haut, translation tau(t)) avec M(0) =
// identite :
//   - h(t)   = l'ecart de l'origine de la racine depuis t = 0, reduit aux
//              composantes choisies (le plan du sol, et la verticale si demandee) ;
//   - y(t)   = le lacet de la racine depuis t = 0 : la torsion autour du haut de
//              q(t) q(0)^-1, CUMULEE de cle en cle (un demi-tour sur place fait
//              bien 180 degres, un tour complet 360, pas 0) ;
//   - la rotation se fait autour de la verticale de la racine au depart (c) :
//              M(t) x = R(y) (x - c) + c + h, soit tau = c + h - R(y) c.
// La pose « sur place » est M(t)^-1 o racine(t) : le bassin garde sa hauteur et
// son balancement, il perd son avance et son lacet (NkRootMotionStrip).
// Le DEPLACEMENT entre deux instants est M(t0)^-1 o M(t1) : dans le repere du
// personnage a t0 (un personnage tourne vers l'est avance vers l'est). Un clip qui
// boucle compose ses cycles entiers (NkRootMotionBetween prend des temps de
// LECTURE, non replies). Rien n'est suppose des reperes des os.
//
// CE QUI N'EST PAS FAIT, et dit : le tangage et le roulis de la racine restent
// dans la pose (un personnage ne s'incline pas par son controleur) ; le
// mouvement de la racine d'un clip pose dans un autre (NLA) n'est pas cumule.
// =============================================================================

#include "NKAnima/Clip/NkAnimation.h"

namespace nkentseu {
	namespace anim {

		// ── Les MUTATIONS des bancs (le negatif dans le meme binaire) ─────────────
		enum NkRootMotionMutation : uint32 {
			NK_RM_MUT_NONE = 0u,
			NK_RM_MUT_NO_WRAP = 1u << 0,	 ///< boucle : les cycles ne sont pas composes (temps replies, difference brute)
			NK_RM_MUT_WORLD_FRAME = 1u << 1, ///< le deplacement n'est pas tourne dans le repere du personnage a t0
			NK_RM_MUT_STRIP_OFF = 1u << 2,	 ///< la pose sur place garde le mouvement de la racine
			NK_RM_MUT_YAW_WRAPPED = 1u << 3, ///< le lacet n'est pas cumule de cle en cle (il se replie a +-180 degres)
		};
		void NkRootMotionSetMutations(uint32 bits) noexcept;
		[[nodiscard]] uint32 NkRootMotionMutations() noexcept;

		struct NkRootMotionSettings {
				int32 bone = -1;		   ///< l'os qui porte le mouvement ; -1 : la premiere racine du clip
				bool horizontal = true;	   ///< le deplacement dans le plan du sol
				bool vertical = false;	   ///< le deplacement le long du haut (un escalier) ; faux : il reste dans la pose
				bool yaw = true;		   ///< le lacet autour du haut
				NkVec3f up = {0.f, 1.f, 0.f};
		};

		/// Une transformation rigide autour du haut : x -> R(yaw) x + translation.
		struct NkRootMotionDelta {
				NkVec3f translation = {0.f, 0.f, 0.f};
				float32 yaw = 0.f; ///< radians, sens direct autour du haut
		};

		/// L'os qui porte le mouvement (reglage, sinon la premiere racine), ou -1 si le
		/// clip n'a pas de piste pour lui.
		[[nodiscard]] int32 NkRootMotionBone(const NkAnimationClip &clip, const NkRootMotionSettings &settings);

		/// M(t) : le mouvement de la racine a l'instant t du clip (borne a [0, duree])
		/// depuis t = 0. Faux si le clip n'a pas de racine animee.
		bool NkRootMotionAt(const NkAnimationClip &clip, float32 t, const NkRootMotionSettings &settings, NkRootMotionDelta &out);

		/// LE DEPLACEMENT entre deux temps de LECTURE (`elapsed`, non replies : ils
		/// croissent avec le temps joue), dans le repere du personnage a `elapsed0`. Un
		/// clip qui boucle compose ses cycles ; un clip qui ne boucle pas s'arrete a ses
		/// bornes. `elapsed1 < elapsed0` (lecture a rebours) rend l'inverse.
		[[nodiscard]] NkRootMotionDelta NkRootMotionBetween(const NkAnimationClip &clip, float32 elapsed0, float32 elapsed1,
															const NkRootMotionSettings &settings);

		/// a puis b (b dit dans le repere d'arrivee de a).
		[[nodiscard]] NkRootMotionDelta NkRootMotionCompose(const NkRootMotionDelta &a, const NkRootMotionDelta &b, const NkVec3f &up);
		/// L'inverse : Compose(d, Inverse(d)) = identite.
		[[nodiscard]] NkRootMotionDelta NkRootMotionInverse(const NkRootMotionDelta &d, const NkVec3f &up);

		/// Fait avancer un personnage (position et lacet du MONDE) du deplacement `d`.
		void NkRootMotionApply(const NkRootMotionDelta &d, const NkVec3f &up, NkVec3f &position, float32 &yaw);

		/// LA POSE SUR PLACE : `rootTransform` (la transformation de l'os racine a
		/// l'instant t, celle que le clip donne) devient M(t)^-1 o rootTransform. Faux si
		/// le clip n'a pas de racine animee (rootTransform intact).
		bool NkRootMotionStrip(const NkAnimationClip &clip, float32 t, const NkRootMotionSettings &settings, NkMat4f &rootTransform);

	} // namespace anim
} // namespace nkentseu
