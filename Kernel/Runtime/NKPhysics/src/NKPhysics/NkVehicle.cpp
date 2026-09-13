// =============================================================================
// NkVehicle.cpp — roue par raycast, suspension à trois gardes, adhérence en
// vitesse à annuler bornée par le cercle de friction. Tout DANS le pas fixe.
// =============================================================================
#include "NKPhysics/NkVehicle.h"

#include <cmath>

namespace nkentseu {
	namespace physics {

		namespace {
			inline float32 Len(const NkVec3f &v) noexcept { return std::sqrt(v.Dot(v)); }
			inline NkVec3f Norm(const NkVec3f &v) noexcept {
				const float32 l = Len(v);
				return l > 1e-8f ? v * (1.f / l) : NkVec3f{0.f, 0.f, 0.f};
			}
			inline float32 Clamp(float32 x, float32 a, float32 b) noexcept { return x < a ? a : (x > b ? b : x); }
			// Masse effective du corps vue le long de `dir`, au point de bras `r`.
			// La MÊME formule que le solveur de contacts (nMass) — partagée, pas recopiée.
			inline float32 EffMass(const NkRigidBody &b, const NkVec3f &r, const NkVec3f &dir) noexcept {
				const NkVec3f rn = r.Cross(dir);
				const float32 k = b.invMass + rn.Dot(NkInvInertiaApply(b, rn));
				return k > 1e-9f ? 1.f / k : 0.f;
			}
			constexpr float32 kG = 9.81f;
			constexpr float32 kPi = 3.14159265358979f;
		} // namespace

		NkVehicle::NkVehicle(NkPhysicsWorld &world) noexcept : mWorld(world) {
			mWorld.RegisterVehicle(this);
		}
		NkVehicle::~NkVehicle() {
			mWorld.UnregisterVehicle(this);
		}

		void NkVehicle::SetChassisBox(const NkVec3f &worldPos, const NkVec3f &half, float32 massKg) {
			NkBodyDef d;
			d.position = worldPos;
			d.orientation = NkQuatf::Identity();
			d.layer = kChassisLayer; // les rayons des roues masquent cette couche
			d.angularDamping = 0.5f; // une caisse ne tourne pas librement dans l'air
			d.linearDamping = 0.02f;
			const float32 vol = 8.f * half.x * half.y * half.z;
			d.material.density = massKg / (vol > 1e-6f ? vol : 1e-6f);
			mMass = massKg;
			// forme en repère MONDE (contrat CreateBody)
			mChassis = mWorld.CreateBody(d, collision::NkShape::Box3D(worldPos, half));
			mTuned = false;
		}

		uint32 NkVehicle::AddWheel(const NkVec3f &localPos, uint32 flags) {
			NkWheel w;
			w.localPos = localPos;
			w.flags = flags;
			mWheels.PushBack(w);
			mTuned = false;
			return (uint32)mWheels.Size() - 1u;
		}

		void NkVehicle::SetInput(float32 steer, float32 throttle, float32 brake) noexcept {
			mSteer = Clamp(steer, -1.f, 1.f);
			mThrottle = Clamp(throttle, -1.f, 1.f);
			mBrake = Clamp(brake, 0.f, 1.f);
		}

		float32 NkVehicle::ForwardSpeed() const noexcept {
			const NkRigidBody *b = mWorld.GetBody(mChassis);
			return b ? b->linearVelocity.Dot(b->orientation.Forward()) : 0.f;
		}

		// Les réglages laissés à zéro sont DÉRIVÉS de la masse : l'auteur décrit
		// une voiture (masse, roues), pas une simulation (N/m). Il peut tout écraser.
		void NkVehicle::Autotune() noexcept {
			const uint32 n = WheelCount() ? WheelCount() : 4u;
			uint32 powered = 0;
			for (uint32 i = 0; i < WheelCount(); ++i)
				if (mWheels[(NkVector<NkWheel>::SizeType)i].flags & NkWheel::kPowered)
					++powered;
			if (!powered)
				powered = n;
			const float32 mPerWheel = mMass / (float32)n;
			// Raideur : la voiture s'enfonce d'un TIERS de la course sous son poids.
			if (mTuning.stiffness <= 0.f)
				mTuning.stiffness = (mPerWheel * kG) / (mTuning.restLength / 3.f);
			// Amortissement : 60 % du critique (2*sqrt(k*m)) — souple, sans rebond.
			if (mTuning.damping <= 0.f)
				mTuning.damping = 0.6f * 2.f * std::sqrt(mTuning.stiffness * mPerWheel);
			if (mTuning.engineForce <= 0.f)
				mTuning.engineForce = 0.8f * kG * mMass / (float32)powered;
			if (mTuning.brakeForce <= 0.f)
				mTuning.brakeForce = 1.2f * kG * mMass / (float32)n;
			if (mTuning.mu <= 0.f) {
				const NkRigidBody *b = mWorld.GetBody(mChassis);
				mTuning.mu = b ? b->material.dynamicFriction : 0.8f;
				if (mTuning.mu <= 0.f)
					mTuning.mu = 0.8f;
			}
			mTuned = true;
		}

		void NkVehicle::StepFixed(float32 h) {
			NkRigidBody *b = mWorld.GetBody(mChassis);
			if (!b || h <= 0.f || WheelCount() == 0)
				return;
			if (!mTuned)
				Autotune();
			// RÉVEIL — mesuré le 2026-09-03 : posée 2 s, la caisse s'endort
			// (UpdateSleep). Mes impulsions montaient alors sa vitesse à 5,9 m/s
			// pendant que l'intégration de POSITION, gardée par IsAwake(), était
			// sautée : dz = 0. Le solveur de contacts réveille via WakeContacts ;
			// une roue est une source de force externe, elle réveille pareil.
			b->flags &= ~NK_BODY_SLEEPING;
			b->sleepTimer = 0.f;

			const NkVec3f up = b->orientation.Up();
			const NkVec3f fwd = b->orientation.Forward();
			const NkVec3f right = b->orientation.Right();
			const float32 maxSteer = mTuning.maxSteerDeg * kPi / 180.f;
			const float32 steerStep = mTuning.steerRateDegPerSec * kPi / 180.f * h;
			const float32 rayLen = mTuning.restLength + mTuning.wheelRadius;
			const float32 weightPerWheel = mMass * kG / (float32)WheelCount();

			for (uint32 i = 0; i < WheelCount(); ++i) {
				NkWheel &w = mWheels[(NkVector<NkWheel>::SizeType)i];

				// ── braquage : lissé vers la consigne (un créneau sur l'axe de
				//    frottement fait sauter la voiture) ─────────────────────
				const float32 target = (w.flags & NkWheel::kSteered) ? mSteer * maxSteer : 0.f;
				w.steerAngle += Clamp(target - w.steerAngle, -steerStep, steerStep);

				// ── a. contact : un rayon vers le bas, en ignorant les châssis ──
				const NkVec3f anchor = b->position + b->orientation * w.localPos;
				collision::NkRay3D ray;
				ray.origin = anchor;
				ray.dir = up * -1.f;
				ray.maxT = rayLen;
				NkBodyId hitBody = NK_INVALID_BODY;
				collision::NkRayHit3D hit;
				const bool touche = mWorld.Raycast(ray, hitBody, hit, ~kChassisLayer) && hit.t <= rayLen;

				const float32 prevComp = w.compression;
				if (!touche) {
					// en l'air : AUCUNE force, et on le dit — pas de force fantôme
					w.grounded = false;
					w.compression = 0.f;
					w.suspForce = 0.f;
					w.slipLat = w.slipLong = 0.f;
					w.worldPos = anchor - up * mTuning.restLength;
					continue;
				}
				w.grounded = true;
				w.contactPoint = hit.point;
				w.contactNormal = Norm(hit.normal);
				w.worldPos = anchor - up * (hit.t - mTuning.wheelRadius);

				// ── b. suspension : ressort + amortisseur, trois gardes ────────
				w.compression = Clamp((rayLen - hit.t) / mTuning.restLength, 0.f, 1.f);
				// ⚠️ UNITÉS — mesuré le 2026-09-03 par la sonde du banc : la raideur est
				// en N/m, donc elle multiplie une DISTANCE (m), pas la fraction [0,1].
				// Avec la fraction, la voiture s'enfonçait de course/9 au lieu de course/3
				// (Fs juste, x faux d'un facteur restLength). Le banc « hauteur
				// d'équilibre = sag du ressort » l'a vu : 1,159 m au lieu de 1,083.
				const float32 x = w.compression * mTuning.restLength;			// m
				// vitesse de compression MESURÉE par différence entre sous-pas :
				// c'est ce qui rend l'amortisseur stable quand la caisse tourne.
				const float32 xVel = (w.compression - prevComp) * mTuning.restLength / h; // m/s
				float32 Fs = mTuning.stiffness * x + mTuning.damping * xVel;
				if (Fs < 0.f)
					Fs = 0.f; // un ressort ne TIRE pas vers le sol
				const float32 FsMax = mTuning.maxSuspFactor * weightPerWheel;
				if (Fs > FsMax)
					Fs = FsMax; // un enfoncement d'une image n'éjecte pas au ciel
				w.suspForce = Fs;
				b->ApplyForceAtPoint(w.contactNormal * Fs, hit.point);

				// ── c. adhérence : vitesse à annuler, bornée par le cercle ────
				const NkVec3f r = hit.point - b->position;
				const NkVec3f vC = b->linearVelocity + b->angularVelocity.Cross(r);
				const float32 cs = std::cos(w.steerAngle), sn = std::sin(w.steerAngle);
				NkVec3f wheelFwd = fwd * cs + right * sn;
				wheelFwd = Norm(wheelFwd - w.contactNormal * wheelFwd.Dot(w.contactNormal));
				const NkVec3f lat = Norm(w.contactNormal.Cross(wheelFwd));
				const float32 vLat = vC.Dot(lat);
				const float32 vLong = vC.Dot(wheelFwd);
				const float32 mLat = EffMass(*b, r, lat);
				const float32 mLong = EffMass(*b, r, wheelFwd);

				// latéral : l'impulsion qui ANNULE le glissement pendant ce pas
				float32 Jlat = -vLat * mLat;
				// longitudinal : moteur + frein (le frein ne peut pas inverser le sens)
				float32 Jlong = 0.f;
				if (w.flags & NkWheel::kPowered)
					Jlong += mThrottle * mTuning.engineForce * h;
				if (mBrake > 0.f) {
					float32 Jb = mBrake * mTuning.brakeForce * h;
					const float32 Jstop = std::fabs(vLong) * mLong; // de quoi l'arrêter, pas plus
					if (Jb > Jstop)
						Jb = Jstop;
					Jlong += (vLong > 0.f ? -Jb : Jb);
				}
				// LE CERCLE DE FRICTION — la borne unique qui donne le survirage
				const float32 Jmax = mTuning.mu * Fs * h;
				const float32 Jn = std::sqrt(Jlat * Jlat + Jlong * Jlong);
				if (Jn > Jmax && Jn > 1e-12f) {
					const float32 s = Jmax / Jn;
					Jlat *= s;
					Jlong *= s;
				}
				NkApplyImpulseAtPoint(*b, lat * Jlat + wheelFwd * Jlong, hit.point);

				// glissements RÉSIDUELS après impulsion (ce que les bancs lisent)
				const NkVec3f vC2 = b->linearVelocity + b->angularVelocity.Cross(r);
				w.slipLat = vC2.Dot(lat);
				w.slipLong = vC2.Dot(wheelFwd) - vLong;
				if (std::fabs(w.slipLat) < mTuning.freezeSpeed)
					w.slipLat = 0.f; // gel sous la vitesse plancher : pas de vibration à l'arrêt
			}
		}

	} // namespace physics
} // namespace nkentseu
