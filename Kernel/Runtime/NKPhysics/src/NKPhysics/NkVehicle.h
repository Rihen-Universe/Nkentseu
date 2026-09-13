#pragma once
// =============================================================================
// NkVehicle.h — véhicule à roues par RAYCAST, mis à jour DANS le pas fixe.
//
// Conception : Engine/Noge/CONCEPTION_VEHICULE.md (2026-09-03). L'essentiel :
//   - une roue = un rayon vers le bas + trois forces (suspension, adhérence
//     longitudinale, adhérence latérale) appliquées EN UN POINT du châssis ;
//   - l'adhérence se calcule en VITESSE À ANNULER (impulsion), puis se borne
//     par le cercle de friction |J| <= mu * Fsusp * h. Une impulsion qui vise
//     l'annulation exacte ne peut pas dépasser sa cible, donc pas osciller ;
//   - le jeu n'appelle JAMAIS Step() : le véhicule s'enregistre auprès du
//     monde et avance dans NkPhysicsWorld::Substep, au pas fixe. L'appelant
//     n'a pas de bouton pour se tromper de cadence.
// =============================================================================
#include "NKPhysics/NkPhysicsWorld.h"

namespace nkentseu {
	namespace physics {

		struct NkWheel {
				enum : uint32 { kSteered = 1u << 0, kPowered = 1u << 1 };
				NkVec3f localPos{};		// ancrage de suspension, repère châssis
				uint32 flags = 0;
				// état (lu par le rendu / les bancs)
				bool grounded = false;
				float32 compression = 0.f; // 0 = détendue, 1 = butée
				float32 steerAngle = 0.f;  // radians, courant (lissé)
				NkVec3f worldPos{};		// centre de la roue, monde
				NkVec3f contactPoint{}, contactNormal{};
				float32 suspForce = 0.f;   // N, dernière valeur
				float32 slipLat = 0.f, slipLong = 0.f; // m/s résiduels APRÈS impulsion
		};

		struct NkVehicleTuning {
				float32 restLength = 0.35f;		// course de suspension (m)
				float32 wheelRadius = 0.35f;	// m
				float32 stiffness = 0.f;		// N/m — 0 = dérivé de la masse (Autotune)
				float32 damping = 0.f;			// N.s/m — 0 = dérivé (60 % du critique)
				float32 maxSuspFactor = 4.f;	// plafond : x fois le poids porté par roue
				float32 engineForce = 0.f;		// N par roue motrice — 0 = dérivé (0.8 g)
				float32 brakeForce = 0.f;		// N par roue — 0 = dérivé (1.2 g)
				float32 maxSteerDeg = 30.f;
				float32 steerRateDegPerSec = 180.f; // lissage d'une consigne créneau
				float32 mu = 0.f;				// 0 = friction dynamique du matériau châssis
				float32 freezeSpeed = 0.05f;	// m/s : sous ce glissement, on annule sec
		};

		class NkVehicle {
			public:
				explicit NkVehicle(NkPhysicsWorld &world) noexcept;
				~NkVehicle();
				NkVehicle(const NkVehicle &) = delete;
				NkVehicle &operator=(const NkVehicle &) = delete;

				// ── la surface — voir CONCEPTION_VEHICULE.md §5 (16 lignes) ──
				void SetChassisBox(const NkVec3f &worldPos, const NkVec3f &halfExtents, float32 massKg);
				uint32 AddWheel(const NkVec3f &localPos, uint32 flags);
				// steer ∈ [-1,1], throttle ∈ [-1,1] (négatif = marche arrière), brake ∈ [0,1]
				void SetInput(float32 steer, float32 throttle, float32 brake) noexcept;

				// ── lecture ────────────────────────────────────────────────
				NkBodyId Chassis() const noexcept { return mChassis; }
				uint32 WheelCount() const noexcept { return (uint32)mWheels.Size(); }
				const NkWheel &Wheel(uint32 i) const noexcept { return mWheels[(NkVector<NkWheel>::SizeType)i]; }
				NkVehicleTuning &Tuning() noexcept { return mTuning; }
				const NkVehicleTuning &Tuning() const noexcept { return mTuning; }
				float32 ForwardSpeed() const noexcept; // m/s, signé, le long de l'axe du châssis

				// Appelé par le monde, DANS le sous-pas fixe. Pas par le jeu.
				void StepFixed(float32 h);

				// Couche de collision réservée aux châssis : les rayons des roues
				// l'ignorent, sinon chaque rayon touche son propre châssis.
				static constexpr uint32 kChassisLayer = 1u << 8;

			private:
				void Autotune() noexcept;

				NkPhysicsWorld &mWorld;
				NkBodyId mChassis = NK_INVALID_BODY;
				float32 mMass = 0.f;
				NkVector<NkWheel> mWheels;
				NkVehicleTuning mTuning;
				float32 mSteer = 0.f, mThrottle = 0.f, mBrake = 0.f;
				bool mTuned = false;
		};

	} // namespace physics
} // namespace nkentseu
