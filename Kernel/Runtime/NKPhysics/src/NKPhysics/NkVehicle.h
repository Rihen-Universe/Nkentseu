#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
				// ── LE RELACHEMENT (2026-09-13) ───────────────────────────
				// Mesuré le 13/09 dans renderdemo : gaz relâchés, la voiture passait de
				// 6,318 à 5,951 m/s en 3 s. Or 6,318·exp(-0,02·3) = 5,950 : la SEULE
				// décélération était le `linearDamping` du corps. Autrement dit le
				// véhicule n'avait NI résistance au roulement NI frein moteur, et roulait
				// 300 m au point mort. Les deux sont des impulsions LONGITUDINALES,
				// donc elles passent par le cercle de friction comme tout le reste :
				// sur la glace elles ne peuvent pas freiner plus que mu ne le permet.
				float32 rollingResistance = 0.015f; // C_rr, sans dimension. Force = C_rr·Fsusp, donc
													// proportionnelle à la CHARGE : elle croît au transfert
													// et vaut zéro roue en l'air, sans une ligne de plus.
													// Pneu sur asphalte : 0,010 à 0,015.
				// ── LA TRAINÉE AÉRODYNAMIQUE (2026-09-13) ─────────────────────
				// Mesuré avant de l'écrire : plein gaz 90,7 s, la voiture passait
				// 1 097 km/h en gagnant encore 1,64 m/s². L'asymptote sans traînée
				// vaut (2·engineForce − C_rr·mg) / (0,02·m) = 385 m/s = 1 386 km/h.
				// ⚠️ DIFFÉRENCE DE NATURE avec le roulement et le frein moteur : ceux-là
				// passent par la gomme, donc le cercle de friction les borne. L'air
				// pousse la CAISSE : la traînée s'applique au centre de masse, HORS du
				// cercle, et elle agit aussi roues en l'air.
				float32 dragCd = 0.30f;			// coefficient de traînée (voiture moderne)
				float32 airDensity = 1.225f;	// kg/m³, air au niveau de la mer
				float32 frontalArea = 0.f;		// m² — 0 = dérivé : 4·demiX·demiY, le RECTANGLE
												// englobant du châssis. La vraie aire frontale d'une
												// voiture vaut ~85 % de ce rectangle : ce défaut
												// SURESTIME d'environ 15 %. C'est dit plutôt que
												// corrigé par un 0,85 sorti de nulle part.
				// ── ACKERMANN (2026-09-13, conception §4 « une ligne ») ─────────
				// Les quatre roues doivent tourner autour du MÊME centre : la roue
				// intérieure braque PLUS que l'extérieure. L'empattement et la voie sont
				// DÉRIVÉS des ancres (Autotune), jamais saisis.
				// ⚠️ CE CHAMP CHANGE LE SENS DE maxSteerDeg : celui-ci devient l'angle de
				// la roue VIRTUELLE du centre, et la roue intérieure le DÉPASSE (34,19°
				// pour 30° sur la voiture du dépôt). Un plafond à 2·maxSteer empêche
				// l'absurde quand le rayon demandé descend sous la demi-voie.
				float32 ackermann = 1.f; // 0 = roues parallèles (avant le 13/09), 1 = géométrie exacte
				float32 engineBrake = 0.10f;		// fraction de engineForce, par roue MOTRICE, gaz
													// relâchés (|throttle| < 0,05). 0 = aucun frein moteur.
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
				NkVec3f mHalf{};				// demi-tailles du châssis (aire frontale dérivée)
				float32 mWheelBase = 0.f;		// empattement, dérivé des ancres (Ackermann)
				float32 mTrack = 0.f;			// voie avant, dérivée des ancres (Ackermann)
				float32 mSteer = 0.f, mThrottle = 0.f, mBrake = 0.f;
				bool mTuned = false;
		};

	} // namespace physics
} // namespace nkentseu
