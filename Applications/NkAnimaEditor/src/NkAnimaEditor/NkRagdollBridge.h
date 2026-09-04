#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRagdollBridge.h — Pont RAGDOLL <-> SQUELETTE skinné (NkAnima). [couplage M*]
// Construit LE NkRagdoll de NKPhysics (un seul constructeur, 2026-09-04) depuis
// glTF (bindGlobal[] + jointParent[]) et synchronise la pose PHYSIQUE vers les
// matrices monde par joint (worldEdit[]) -> le mesh skinné suit le ragdoll.
//
//   Passif  : Step(dt) puis SyncToSkeleton(worldEdit) -> le perso s'effondre/réagit.
//   (Actif/pose-driven : variante revolute + SetPoseTargets, étape suivante.)
//
// Générique (toute morphologie : humanoïde, animal, créature) — rien de figé.
// =============================================================================
#include "NKPhysics/NKPhysics.h"
#include "NKMath/NkMat.h"

namespace nkanima {

	using nkentseu::float32;
	using nkentseu::int32;
	using nkentseu::uint32;
	using NkMat4f = nkentseu::math::NkMat4f;
	using NkQuatf = nkentseu::math::NkQuatf;
	using NkVec3f = nkentseu::math::NkVec3f;
	namespace physics = nkentseu::physics;
	namespace collision = nkentseu::collision;

	class NkRagdollBridge {
		public:
			// Construit le ragdoll par LE constructeur de NKPhysics (2026-09-04) :
			// la vue (parents + repos monde) se lit dans bindGlobal[], la table
			// d'attributs par defaut vient de NkRagdollAttrsFromSkeleton (capsule
			// joint->parent, racine KINEMATIC). Ce pont ne cree plus aucun corps.
			void Build(physics::NkPhysicsWorld &world, const nkentseu::NkVector<NkMat4f> &bindGlobal,
					   const nkentseu::NkVector<int32> &jointParent, float32 boneRadius = 0.04f) {
				mWorld = &world;
				const uint32 n = (uint32)bindGlobal.Size();
				mParents.Resize(n);
				mRestPos.Resize(n);
				mRestRot.Resize(n);
				for (uint32 j = 0; j < n; ++j) {
					const NkMat4f &M = bindGlobal[j];
					mRestPos[j] = NkVec3f{M.m30, M.m31, M.m32};
					mRestRot[j] = NkQuatf(M);
					mParents[j] = (j < (uint32)jointParent.Size()) ? jointParent[j] : -1;
				}
				physics::NkSkeletonView vue;
				vue.parent = mParents.Data();
				vue.restPos = mRestPos.Data();
				vue.restRot = mRestRot.Data();
				vue.count = n;
				nkentseu::NkVector<physics::NkRagdollBoneAttr> attrs;
				attrs.Resize(n);
				physics::NkRagdollAttrsFromSkeleton(vue, boneRadius, attrs.Data());
				mRag.Build(world, vue, attrs.Data());
			}
			// Physique -> squelette : worldEdit[j] = pose du corps j (le skin suivra).
			void SyncToSkeleton(nkentseu::NkVector<NkMat4f> &worldEdit) const {
				if (!mWorld)
					return;
				nkentseu::NkVector<NkVec3f> pos;
				nkentseu::NkVector<NkQuatf> rot;
				mRag.ReadPose(*mWorld, pos, rot);
				const uint32 n = (uint32)pos.Size();
				for (uint32 j = 0; j < n && j < (uint32)worldEdit.Size(); ++j) {
					NkMat4f m = (NkMat4f)rot[j];
					m.m30 = pos[j].x;
					m.m31 = pos[j].y;
					m.m32 = pos[j].z;
					worldEdit[j] = m;
				}
			}
			// Pilote la racine (KINEMATIC) -> permet de deplacer le ragdoll a la main / a l'anim.
			void SetRootTarget(const NkVec3f &pos) {
				if (mWorld && mRag.Count() > 0)
					if (auto *b = mWorld->GetBody(mRag.Body(0)))
						b->position = pos;
			}
			bool Built() const noexcept {
				return mWorld != nullptr && mRag.Count() > 0;
			}
			uint32 Count() const noexcept {
				return mRag.Count();
			}
			void Clear() {
				mRag = physics::NkRagdoll{};
				mParents.Clear();
				mRestPos.Clear();
				mRestRot.Clear();
				mWorld = nullptr;
			}
		private:
			physics::NkPhysicsWorld *mWorld = nullptr;
			physics::NkRagdoll mRag; // LE ragdoll de NKPhysics -- plus de corps fabriques ici
			nkentseu::NkVector<int32> mParents; // la vue pointe dedans : ils vivent avec le pont
			nkentseu::NkVector<NkVec3f> mRestPos;
			nkentseu::NkVector<NkQuatf> mRestRot;
	};
} // namespace nkanima
