#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtRig.h — LE SQUELETTE vu par la physique d'animation EN TEMPS REEL.
//
// Une vue construite UNE fois (a l'installation du personnage) depuis les parents
// et les noms : l'ordre en profondeur (prefixe), la fin du sous-arbre de chaque
// joint, le premier enfant. C'est ce qui permet aux solveurs de ne recalculer, a
// chaque image, que le SOUS-ARBRE d'un joint qu'ils viennent de tourner, au lieu
// de toute la pose : le cout par image reste proportionnel a ce qui change.
//
// Conventions (les memes que NkSkeletonDef et le clip) :
//   world[j] = world[parent(j)] * local[j]   (une racine : world = local) ;
//   unites : le metre ; le haut : +Y sauf reglage contraire des solveurs.
//
// Pour les bancs, les demos et un premier essai sans actif : un HUMANOIDE D'ESSAI
// fabrique par le calcul (1,75 m, de face vers +Z, une queue de cheval de quatre
// os pour le mouvement secondaire), avec, sur demande, des rotations de repos
// quelconques -- les solveurs ne doivent rien supposer des reperes des os.
// =============================================================================

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			class NkRtRig {
				public:
					/// Construit la vue. `names` peut etre nul. Faux si un parent est hors
					/// bornes ou si la hierarchie a un cycle.
					bool Build(const int32 *parents, uint32 count, const char *const *names = nullptr);

					[[nodiscard]] uint32 Count() const noexcept {
						return (uint32)mParent.Size();
					}
					[[nodiscard]] int32 Parent(uint32 j) const noexcept {
						return j < Count() ? mParent[j] : -1;
					}
					/// Le premier enfant (dans l'ordre des indices), ou -1.
					[[nodiscard]] int32 FirstChild(uint32 j) const noexcept {
						return j < Count() ? mFirstChild[j] : -1;
					}
					[[nodiscard]] uint32 ChildCount(uint32 j) const noexcept {
						return j < Count() ? mChildCount[j] : 0u;
					}
					/// L'ordre en profondeur (prefixe) : un joint, puis tout son sous-arbre.
					[[nodiscard]] const uint32 *Order() const noexcept {
						return mOrder.Data();
					}
					/// Le rang de `j` dans Order(), et la fin (exclue) de son sous-arbre.
					[[nodiscard]] uint32 OrderPos(uint32 j) const noexcept {
						return mPos[j];
					}
					[[nodiscard]] uint32 SubtreeEnd(uint32 j) const noexcept {
						return mEnd[j];
					}
					/// `a` est-il `j` ou un ancetre de `j` ?
					[[nodiscard]] bool IsInSubtree(uint32 j, uint32 a) const noexcept {
						return mPos[j] >= mPos[a] && mPos[j] < mEnd[a];
					}
					[[nodiscard]] const char *Name(uint32 j) const noexcept {
						return j < (uint32)mNames.Size() ? mNames[j].CStr() : "";
					}
					/// Le joint de ce nom (sans tenir compte de la casse), ou -1.
					[[nodiscard]] int32 Find(const char *name) const noexcept;
					/// Le premier joint dont le nom CONTIENT l'un des mots (casse ignoree)
					/// et, si `suffix` n'est pas nul, se termine par lui (« .L », « _l »...).
					[[nodiscard]] int32 FindLike(const char *const *words, uint32 wordCount, const char *const *suffixes,
												 uint32 suffixCount) const noexcept;

					/// Toute la pose : local -> monde.
					void LocalToWorld(const math::NkMat4f *local, math::NkMat4f *world) const noexcept;
					/// Les DESCENDANTS de `j` (pas `j`) recalcules depuis leur local : a
					/// appeler apres avoir change le monde de `j`.
					void RefreshDescendants(uint32 j, const math::NkMat4f *local, math::NkMat4f *world) const noexcept;
					/// Monde -> local (inverse general : hors de la boucle chaude).
					void WorldToLocal(const math::NkMat4f *world, math::NkMat4f *local) const noexcept;

				private:
					NkVector<int32> mParent;
					NkVector<int32> mFirstChild;
					NkVector<uint32> mChildCount;
					NkVector<uint32> mOrder, mPos, mEnd;
					NkVector<NkString> mNames;
			};

			// ── Les MUTATIONS des bancs (le negatif dans le meme binaire) ─────────
			// Chaque critere des bancs a SA mutation, qui doit le faire rougir. Elles
			// sont dans le code du produit (un entier compare, rien d'autre) parce
			// qu'une seconde construction pourrait differer par autre chose. Le
			// produit ne les pose jamais : seuls les bancs appellent NkRtSetMutations.
			enum NkRtMutation : uint32 {
				NK_RT_MUT_NONE = 0u,
				NK_RT_MUT_SPRING_EXPLICIT = 1u << 0,   ///< ressort : Euler explicite, sans sous-pas
				NK_RT_MUT_FOOTIK_NO_PELVIS = 1u << 1,  ///< IK des pieds : le bassin ne descend jamais
				NK_RT_MUT_FOOTIK_OFF = 1u << 2,		   ///< IK des pieds : aucune correction
				NK_RT_MUT_RAGDOLL_FROM_BIND = 1u << 3, ///< ragdoll : demarre de la pose de REPOS
				NK_RT_MUT_RAGDOLL_NO_VELOCITY = 1u << 4, ///< ragdoll : demarre a vitesse nulle
				NK_RT_MUT_GETUP_SNAP = 1u << 5,		   ///< relevement : la fin saute (pas de recalage)
				NK_RT_MUT_COST = 1u << 6,			   ///< cout : ressort a 20 000 sous-pas par seconde de plus
				NK_RT_MUT_BALANCE_OFF = 1u << 7,	   ///< equilibre : aucun deplacement du bassin
			};
			void NkRtSetMutations(uint32 bits) noexcept;
			[[nodiscard]] uint32 NkRtMutations() noexcept;
			[[nodiscard]] inline bool NkRtMutated(uint32 bit) noexcept {
				return (NkRtMutations() & bit) != 0u;
			}

			// ── L'humanoide d'essai ───────────────────────────────────────────────
			struct NkRtTestHumanoid {
					NkVector<int32> parents;
					NkVector<math::NkMat4f> bindLocal;
					NkVector<const char *> names;
					// Les joints utiles aux bancs et aux demos.
					int32 hips = -1, spine = -1, chest = -1, neck = -1, head = -1;
					int32 upperArmL = -1, foreArmL = -1, handL = -1, upperArmR = -1, foreArmR = -1, handR = -1;
					int32 thighL = -1, shinL = -1, footL = -1, toeL = -1, thighR = -1, shinR = -1, footR = -1, toeR = -1;
					int32 tail0 = -1; ///< le premier os de la queue de cheval (4 os)
			};
			/// Le personnage d'essai, debout, pieds au sol a y = 0. `twistedBind` : chaque
			/// os recoit une rotation de repos arbitraire (positions du repos identiques).
			void NkRtMakeTestHumanoid(NkRtTestHumanoid &out, bool twistedBind = false);

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
