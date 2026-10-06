#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRagdollBlend.h — LA BASCULE ANIMATION <-> RAGDOLL, avec fondu, et le
// RELEVEMENT.
//
// NkPoseRagdoll : un ragdoll LEGER, propre a NKAnima (aucune dependance a la
// physique du monde) : une particule par joint, des contraintes de distance
// (les os, rigides ; la flexion de chaque articulation, bornee ; le bassin et la
// poitrine, rigides grace a une particule virtuelle hors du plan), la gravite,
// le sol de l'hote (lancer de rayon), le frottement. Dynamique a positions
// (PBD, Verlet), en sous-pas fixes : stable, bon marche, deterministe.
// Les ROTATIONS des os se relisent sur les particules : chaque os tourne de la
// rotation minimale qui porte sa direction de depart sur sa direction courante,
// apres la rotation de son parent (la torsion vient du parent) ; le bassin et la
// poitrine lisent un repere complet.
//
// NkRagdollBlend : la machine a etats.
//   Animated  -> (choc) Falling : le ragdoll PART DE LA POSE ANIMEE COURANTE, avec
//                sa VITESSE (pose d'avant) -- aucun saut de pose ; le fondu monte
//                de 0 a 1 en `fadeIn` ;
//   Ragdoll   : la physique seule ; immobile pendant `getUpDelay` -> GettingUp ;
//   GettingUp : de la pose figee du ragdoll vers l'animation de relevement (que
//                l'hote joue : ventre ou dos, FaceUp()), RECALEE au sol : le bassin
//                de l'animation est pose sous le bassin du ragdoll, tourne vers
//                son cap. Le recalage reste applique ensuite (Relocation()) ; l'hote
//                le reprend dans sa propre transformation (TakeRelocation).
// =============================================================================

#include "NKAnima/Physics/NkPoseMass.h"
#include "NKAnima/Realtime/NkRtGround.h"
#include "NKAnima/Realtime/NkRtRig.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			struct NkRagdollSettings {
					float32 gravity = -9.81f;
					float32 substep = 1.f / 120.f;
					uint32 iterations = 8u;
					float32 damping = 0.35f;	   ///< amortissement de la vitesse (1/s)
					float32 friction = 0.5f;	   ///< part de la vitesse tangente perdue par sous-pas au contact
					float32 radius = 0.06f;		   ///< rayon des particules (m)
					float32 limbMaxBend = 2.6f;	   ///< flexion max d'un membre (rad, genou, coude)
					float32 spineMaxBend = 0.9f;   ///< flexion max de la colonne et du cou (rad)
					float32 settleSpeed = 0.12f;   ///< sous cette vitesse (m/s), le corps « repose »
			};

			class NkPoseRagdoll {
				public:
					bool Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld, const NkPoseMass *mass = nullptr);
					/// Demarre depuis la pose `now` ; la vitesse vient de `prev` (il y a `dtPrev`).
					void Start(const math::NkMat4f *now, const math::NkMat4f *prev, float32 dtPrev);
					/// Un changement de vitesse sur un joint (a moitie sur ses voisins).
					void ApplyImpulse(uint32 joint, const NkVec3f &deltaVelocity);
					void Step(float32 dt, const NkRtGround &ground);
					/// La pose du ragdoll (monde).
					void ReadPose(math::NkMat4f *worldOut) const;

					NkRagdollSettings settings;
					[[nodiscard]] float32 MaxSpeed() const noexcept {
						return mMaxSpeed;
					}
					[[nodiscard]] float32 RestTime() const noexcept {
						return mRestTime;
					}
					[[nodiscard]] bool Ready() const noexcept {
						return mRig != nullptr;
					}
					/// La position de la particule `j` (le joint `j`).
					[[nodiscard]] NkVec3f Particle(uint32 j) const {
						return mX[j];
					}
					/// Le pire ecart relatif des os rigides (0 : longueurs exactes).
					[[nodiscard]] float32 MaxBoneStretch() const;
					/// L'avant du corps au repos (des chevilles vers les orteils, sinon +Z).
					[[nodiscard]] NkVec3f BindForward() const noexcept {
						return mForward0;
					}
					/// La rotation de chaque os depuis le depart (relue a ReadPose).
					[[nodiscard]] NkRtQuat DeltaRotation(uint32 j) const {
						return mDelta[j];
					}

				private:
					struct Contrainte {
							uint32 a = 0, b = 0;
							float32 mini = 0.f, maxi = 0.f;
							bool os = false; ///< un os (rigide), pour MaxBoneStretch
					};
					void Contraindre(uint32 a, uint32 b, float32 mini, float32 maxi, bool os);
					void Recalculer() const;

					const NkRtRig *mRig = nullptr;
					NkVector<math::NkMat4f> mBind;
					uint32 mJoints = 0;			   ///< les particules 0..mJoints-1 sont les joints
					NkVector<NkVec3f> mX, mXPrev;  ///< joints, puis particules virtuelles
					NkVector<float32> mInvMass;
					NkVector<uint8> mSolOk;		   ///< le sol sous chaque particule (ce sous-pas)
					NkVector<NkVec3f> mSolP, mSolN;
					NkVector<Contrainte> mC;
					NkVector<int32> mVirtual;	   ///< par joint : sa particule virtuelle, ou -1
					NkVector<NkVec3f> mVirtLocal;  ///< la particule virtuelle dans le repere du joint
					NkVector<uint32> mVirtOwner;   ///< par particule virtuelle : son joint
					// Le depart : rotations, echelles, directions, reperes.
					NkVector<NkRtQuat> mR0;
					NkVector<NkVec3f> mS0, mD0;
					NkVector<NkRtQuat> mF0;
					mutable NkVector<NkRtQuat> mDelta;
					NkVec3f mForward0 = {0.f, 0.f, 1.f};
					float32 mMaxSpeed = 0.f, mRestTime = 0.f;
			};

			enum class NkRagdollPhase : uint8 { Animated = 0, Falling, Ragdoll, GettingUp };

			struct NkRagdollBlendSettings {
					float32 fadeIn = 0.10f;		   ///< s : de l'animation au ragdoll
					float32 getUpDelay = 0.5f;	   ///< s immobile avant de se relever
					float32 getUpDuration = 1.1f;  ///< s : de la pose figee a l'animation
					bool autoGetUp = true;
			};

			class NkRagdollBlend {
				public:
					bool Setup(const NkRtRig &rig, const math::NkMat4f *bindWorld, const NkPoseMass *mass = nullptr);
					/// LE CHOC : le ragdoll part de `now` (la derniere pose montree), a la
					/// vitesse tiree de `prev`, et recoit `deltaVelocity` sur `joint`.
					void Trigger(const math::NkMat4f *now, const math::NkMat4f *prev, float32 dtPrev, uint32 joint,
								 const NkVec3f &deltaVelocity);
					/// Le relevement, tout de suite (sinon automatique apres `getUpDelay`).
					void StartGetUp();
					/// Une image : `world` = la pose animee (deja recalee par Relocation() si
					/// l'hote ne l'a pas reprise) ; sortie : la pose finale, en place.
					void Update(const NkRtRig &rig, math::NkMat4f *world, float32 dt, const NkRtGround &ground);

					NkPoseRagdoll ragdoll;
					NkRagdollBlendSettings settings;

					[[nodiscard]] NkRagdollPhase Phase() const noexcept {
						return mPhase;
					}
					[[nodiscard]] float32 Blend() const noexcept {
						return mBlend;
					}
					[[nodiscard]] float32 PhaseTime() const noexcept {
						return mTime;
					}
					/// Couche sur le dos (vrai) ou sur le ventre (faux), au debut du relevement.
					[[nodiscard]] bool FaceUp() const noexcept {
						return mFaceUp;
					}
					/// Le recalage du personnage (espace de l'animation -> monde) depuis le
					/// dernier relevement ; identite sinon.
					[[nodiscard]] const math::NkMat4f &Relocation() const noexcept {
						return mReloc;
					}
					[[nodiscard]] bool RelocationActive() const noexcept {
						return mRelocActive;
					}
					/// L'hote reprend le recalage dans sa transformation : il est rendu et
					/// remis a l'identite. Faux s'il n'y en avait pas.
					bool TakeRelocation(math::NkMat4f &out);

				private:
					void Melanger(const NkRtRig &rig, const math::NkMat4f *a, const math::NkMat4f *b, float32 t,
								  math::NkMat4f *out);
					void CalculerRecalage(const NkRtRig &rig, const math::NkMat4f *anim);

					NkRagdollPhase mPhase = NkRagdollPhase::Animated;
					float32 mTime = 0.f, mBlend = 0.f;
					bool mFaceUp = true;
					bool mRelocActive = false;
					bool mRecalagePret = false;
					math::NkMat4f mReloc = math::NkMat4f::Identity();
					math::NkMat4f mDeltaFrame = math::NkMat4f::Identity(); ///< le recalage ajoute a l'image du relevement
					math::NkMat4f mBindPelvis = math::NkMat4f::Identity();
					NkVector<math::NkMat4f> mRag, mFrozen, mTmp, mLa, mLb;
			};

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
