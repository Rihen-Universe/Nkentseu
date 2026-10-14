#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Blend/NkInertialize.h : LA TRANSITION PAR INERTIE (« inertialization »)
// (14/10/2026).
//
// D'OU VIENT CE FICHIER. Changer d'animation par un FONDU demande d'evaluer les
// deux poses pendant tout le fondu, et le fondu lineaire casse la vitesse : un
// bras lance vers l'avant s'arrete net au premier instant du fondu, puis repart.
// La transition par inertie ne joue QUE le nouvel etat ; elle garde l'ecart entre
// la derniere pose sortie et la premiere pose du nouvel etat, et le fait decroitre
// jusqu'a zero, en partant de la VITESSE qu'avait l'ancien mouvement (D. Bollo,
// « Inertialization », GDC 2018 ; le noeud du meme nom d'UE5). Pour un jeu de Nogee
// (un coup qui interrompt une course), pour le patient de PV3DE (un geste qui en
// interrompt un autre), pour la machine a etats de Noge.
//
// LA LOI. Par os, l'ecart de position (sa norme x0, sa direction) et l'ecart de
// rotation (son angle x0, son axe) decroissent par le polynome de degre 5 qui part
// de x0 a la vitesse v0 et arrive a 0 en T, vitesse et acceleration nulles :
// aucun saut de position ni de vitesse au depart, aucun a l'arrivee. v0 vient des
// deux dernieres poses sorties. Un ecart qui GRANDIT (v0 > 0) est pris a vitesse
// nulle, et T est raccourci si la vitesse ferait depasser zero (pas de rebond) :
// les deux regles de l'article. L'echelle et les poids des formes decroissent par
// la meme loi, sans vitesse.
//
// CE QUI EST SUPPOSE, et dit : la vitesse de la NOUVELLE animation a l'instant du
// changement n'entre pas dans v0 (l'ecart est mesure contre sa premiere pose).
// =============================================================================

#include "NKAnima/Blend/NkAnimMix.h"

namespace nkentseu {
	namespace anim {

		// ── Les MUTATIONS des bancs (le negatif dans le meme binaire) ─────────────
		enum NkInertializeMutation : uint32 {
			NK_IN_MUT_NONE = 0u,
			NK_IN_MUT_LINEAR = 1u << 0,	   ///< l'ecart decroit en ligne droite (la vitesse saute au depart)
			NK_IN_MUT_NO_OFFSET = 1u << 1, ///< aucun ecart garde (la pose saute au depart)
			NK_IN_MUT_NO_SHORTEN = 1u << 2, ///< T n'est pas raccourci pour une vitesse forte (l'ecart passe sous zero : rebond)
		};
		void NkInertializeSetMutations(uint32 bits) noexcept;
		[[nodiscard]] uint32 NkInertializeMutations() noexcept;

		/// La decroissance d'UN ecart : x(t) depuis x0 a la vitesse v0, nul en T (et apres).
		struct NkInertialCurve {
				float32 x0 = 0.f, v0 = 0.f, T = 0.f;
				float32 a = 0.f, b = 0.f, c = 0.f, a0 = 0.f; ///< les coefficients (t^5, t^4, t^3, et l'acceleration de depart)
				void Set(float32 x0, float32 v0, float32 duration) noexcept;
				[[nodiscard]] float32 Eval(float32 t) const noexcept;
		};

		class NkInertialBlend {
			public:
				/// ARME la transition. `prev` et `prevPrev` : les deux dernieres poses SORTIES
				/// (l'ancien etat), separees de `dt` secondes ; `target` : la premiere pose du
				/// nouvel etat ; `duration` : le temps voulu pour effacer l'ecart (raccourci par
				/// os si sa vitesse le demande). Les os en trop d'un cote sont ignores.
				void Start(const NkAnimPose &prev, const NkAnimPose &prevPrev, float32 dt, const NkAnimPose &target, float32 duration);
				/// Ajoute a `pose` (le nouvel etat, joue normalement) l'ecart restant au temps
				/// `elapsed` depuis Start. Au-dela de la duree : `pose` n'est pas touchee.
				void Apply(NkAnimPose &pose, float32 elapsed) const;
				[[nodiscard]] bool Active(float32 elapsed) const noexcept {
					return mArmed && elapsed < mDuration;
				}
				void Stop() noexcept {
					mArmed = false;
				}

			private:
				struct Os {
						NkVec3f dir = {0.f, 0.f, 0.f};	///< direction de l'ecart de position
						NkInertialCurve pos;
						NkVec3f axe = {0.f, 1.f, 0.f};	///< axe de l'ecart de rotation
						NkInertialCurve rot;
						NkVec3f ds = {0.f, 0.f, 0.f};	///< ecart d'echelle
				};
				NkVector<Os> mOs;
				NkVector<float32> mMorphs;	///< ecarts des poids des formes
				NkInertialCurve mUnite;		///< 1 -> 0 sans vitesse (echelle, formes)
				float32 mDuration = 0.f;
				bool mArmed = false;
		};

	} // namespace anim
} // namespace nkentseu
