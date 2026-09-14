// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
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
			mHalf = half; // l'aire frontale s'en déduit (traînée)
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
			// Empattement et voie, DÉRIVÉS des ancres — l'auteur décrit une voiture
			// (où sont les roues), pas une géométrie de direction. Ackermann s'en sert.
			{
				float32 zF = 0.f, zR = 0.f, xF = 0.f;
				uint32 nF = 0, nR = 0;
				for (uint32 i = 0; i < WheelCount(); ++i) {
					const NkWheel &w = mWheels[(NkVector<NkWheel>::SizeType)i];
					if (w.flags & NkWheel::kSteered) {
						zF += w.localPos.z;
						xF += std::fabs(w.localPos.x);
						++nF;
					} else {
						zR += w.localPos.z;
						++nR;
					}
				}
				mWheelBase = (nF && nR) ? (zF / (float32)nF - zR / (float32)nR) : 0.f;
				mTrack = nF ? (2.f * xF / (float32)nF) : 0.f;
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

			// ── TRAÎNÉE AÉRODYNAMIQUE (2026-09-13) ─────────────────────────
			// F = ½·ρ·Cd·A·v², opposée à la vitesse, appliquée AU CENTRE DE MASSE et
			// UNE SEULE FOIS par sous-pas — ce n'est pas une force de roue. Elle ne
			// passe donc PAS par le cercle de friction (l'air ne pousse pas la gomme)
			// et elle agit aussi roues en l'air. Sans elle, mesure du 13/09 : plein
			// gaz 90,7 s → 1 097 km/h, et ça montait encore de 1,64 m/s².
			if (mTuning.dragCd > 0.f && mTuning.airDensity > 0.f) {
				const float32 sp = Len(b->linearVelocity);
				if (sp > 1e-3f) {
					const float32 area = (mTuning.frontalArea > 0.f) ? mTuning.frontalArea : (4.f * mHalf.x * mHalf.y);
					const float32 F = 0.5f * mTuning.airDensity * mTuning.dragCd * area * sp * sp;
					b->ApplyForce(b->linearVelocity * (-F / sp));
				}
			}

			// BALAYAGE ALTERNÉ : un sous-pas sur deux parcourt les roues à l'envers.
			// Voir NkVehicle.h — sans cela, la roue traitée en premier laisse un lacet
			// résiduel du même signe à chaque pas, et il s'accumule.
			const bool aLEnvers = mTuning.alternateSweep && ((mSweep++ & 1u) != 0u);
			for (uint32 k = 0; k < WheelCount(); ++k) {
				const uint32 i = aLEnvers ? (WheelCount() - 1u - k) : k;
				NkWheel &w = mWheels[(NkVector<NkWheel>::SizeType)i];

				// ── braquage : lissé vers la consigne (un créneau sur l'axe de
				//    frottement fait sauter la voiture) ─────────────────────
				// ── ACKERMANN (2026-09-13) ── la roue INTÉRIEURE braque PLUS. Sans lui,
				// les deux roues avant sont parallèles : elles demandent deux rayons
				// différents, se contredisent, et la voiture « racle » (conception §4).
				// R = L/tan(δ) ; intérieure : atan(L/(R−T/2)), extérieure : atan(L/(R+T/2)).
				float32 target = 0.f;
				if (w.flags & NkWheel::kSteered) {
					const float32 base = mSteer * maxSteer; // la roue VIRTUELLE du centre
					target = base;
					if (mTuning.ackermann > 0.f && mWheelBase > 1e-3f && std::fabs(base) > 1e-4f) {
						const float32 R = mWheelBase / std::tan(std::fabs(base));
						// intérieure = du côté vers lequel on tourne (steer > 0 → vers la droite,
						// cf. wheelFwd = fwd·cos + right·sin), donc localPos.x de même signe.
						const bool interieure = (w.localPos.x * base) > 0.f;
						const float32 Ri = interieure ? (R - mTrack * 0.5f) : (R + mTrack * 0.5f);
						const float32 geo = (Ri > 1e-3f) ? std::atan(mWheelBase / Ri) : (kPi * 0.5f);
						const float32 signe = (base > 0.f) ? 1.f : -1.f;
						// mélange : 0 = parallèle (le comportement d'avant), 1 = géométrie exacte.
						target = base + mTuning.ackermann * (signe * geo - base);
						// plafond de sûreté : quand R passe sous T/2, la géométrie exige un
						// angle qui tend vers 90°. Aucune direction ne fait ça.
						target = Clamp(target, -2.f * maxSteer, 2.f * maxSteer);
					}
				}
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
				w.slipLatPre = vLat; // diagnostic : ce qui ENTRE, pour juger du dépassement
				const float32 vLong = vC.Dot(wheelFwd);
				const float32 mLat = EffMass(*b, r, lat);
				const float32 mLong = EffMass(*b, r, wheelFwd);

				// latéral : l'impulsion qui ANNULE le glissement pendant ce pas
				float32 Jlat = -vLat * mLat;
				// longitudinal : moteur + frein (le frein ne peut pas inverser le sens)
				float32 Jlong = 0.f;
				const bool gazLache = std::fabs(mThrottle) < 0.05f;
				if (w.flags & NkWheel::kPowered)
					Jlong += mThrottle * mTuning.engineForce * h;
				// ── TOUT CE QUI RETARDE, PLAFONNÉ ENSEMBLE (2026-09-13) ──────────
				// frein pilote + résistance au roulement + frein moteur. Le plafond
				// « de quoi l'arrêter, pas plus » porte sur leur SOMME, et c'est le
				// point : plafonnés chacun dans son coin, deux freinages qui s'arrêtent
				// chacun juste à temps INVERSENT la voiture à eux deux. Le frein seul
				// était plafonné dans son propre `if` : le défaut était latent, il ne
				// s'est jamais montré parce qu'il n'y avait qu'un seul freinage.
				// Les trois restent à l'INTÉRIEUR du cercle de friction ci-dessous :
				// sur la glace, aucun d'eux ne peut freiner plus que mu ne le permet.
				float32 Jret = 0.f;
				if (mBrake > 0.f)
					Jret += mBrake * mTuning.brakeForce * h;
				if (mTuning.rollingResistance > 0.f)
					Jret += mTuning.rollingResistance * Fs * h; // ∝ la CHARGE : nulle roue en l'air
				if (gazLache && mTuning.engineBrake > 0.f && (w.flags & NkWheel::kPowered))
					Jret += mTuning.engineBrake * mTuning.engineForce * h;
				// ── LE FROTTEMENT STATIQUE (2026-09-13) ────────────────────
				// Mesuré : frein à fond sur 10°, la voiture FLUAIT en arrière à
				// 3,9 cm/s — 14 cm toutes les quatre secondes, visible à l'œil.
				// La cause n'est pas « rien ne freine » : c'est UN PAS DE RETARD.
				// `StepFixed` applique les impulsions AVANT que l'intégrateur
				// n'ajoute la gravité. À l'arrêt vLong vaut 0 au début du sous-pas,
				// donc le plafond « de quoi l'arrêter » vaut 0 et AUCUNE impulsion
				// ne part ; la gravité ajoute ensuite g·sinθ·h, que le sous-pas
				// SUIVANT annule — après que la voiture a avancé. La vitesse fait
				// une dent de scie entre 0 et g·sinθ·h, et la voiture descend.
				// La retenue vise donc la vitesse que la roue AURA à la fin du pas.
				// Le cercle de friction, lui, borne le tout juste en dessous, PAR
				// ROUE : c'est lui qui donne le frottement statique, et c'est lui
				// qui refusera de tenir une pente au-delà de tanθ = mu.
				// ⚠️ Sur le PLAT, gravité·wheelFwd = 0 exactement (gravité verticale,
				// wheelFwd horizontal) : le comportement plat est inchangé AU BIT.
				// Seule la gravité est anticipée : la suspension est portée par la
				// normale (projection nulle sur wheelFwd) et la poussée moteur est
				// déjà dans Jlong.
				const float32 vLongFin =
					mTuning.staticFriction ? (vLong + mWorld.Config().gravity.Dot(wheelFwd) * h) : vLong;
				if (Jret > 0.f) {
					const float32 Jstop = std::fabs(vLongFin) * mLong; // de quoi l'arrêter, pas plus
					if (Jret > Jstop)
						Jret = Jstop;
					Jlong += (vLongFin > 0.f ? -Jret : Jret);
				}
				// ── TRAÎNÉE INDUITE DE VIRAGE (2026-09-14) ─────────────────
				// F_drag = c · F_lat² / Fs (voir NkVehicle.h). Calculée sur le Jlat
				// AVANT le cercle, puis versée dans Jlong AVANT lui aussi : la traînée
				// induite est une force de PNEU, elle appartient donc au même budget
				// d'adhérence. À la limite, la demander revient à en retirer au latéral
				// — ce qui est vrai d'une vraie voiture.
				// Plafonnée par « de quoi l'arrêter, pas plus » : elle ne peut donc ni
				// inverser la marche, ni fabriquer de l'énergie.
				if (mTuning.corneringDrag > 0.f && Fs > 1e-3f) {
					const float32 Flat = std::fabs(w.latForceFilt);
					float32 Jd = mTuning.corneringDrag * (Flat * Flat / Fs) * h;
					const float32 JdStop = std::fabs(vLongFin) * mLong;
					if (Jd > JdStop)
						Jd = JdStop;
					Jlong += (vLongFin > 0.f ? -Jd : Jd);
					w.dragLat = Jd / h; // N, lu par les bancs
				} else
					w.dragLat = 0.f;

				// LE CERCLE DE FRICTION — la borne unique qui donne le survirage
				const float32 Jmax = mTuning.mu * Fs * h;
				const float32 Jn = std::sqrt(Jlat * Jlat + Jlong * Jlong);
				if (Jn > Jmax && Jn > 1e-12f) {
					const float32 s = Jmax / Jn;
					Jlat *= s;
					Jlong *= s;
				}
				// ⚠️ LA TRAÎNÉE SE CALCULE SUR LA FORCE RÉELLE, PAS SUR LA DEMANDE.
				// Première version : le filtre lisait Jlat AVANT le cercle de friction.
				// Or `Jlat = -vLat·mLat` est l'impulsion qui annule le glissement en UN
				// sous-pas : divisée par h elle vaut des dizaines de milliers de newtons
				// (18 000 N pour 0,5 m/s de glissement), ce qui n'est pas une force de
				// pneu mais une grandeur impulsionnelle. Élevée au carré, elle demandait
				// **8,36 m/s²** de traînée là où la loi en prédit 0,078 — cent fois trop,
				// et seul le plafond « de quoi l'arrêter » empêchait l'absurde.
				// Le filtre lit donc désormais l'impulsion APRÈS le cercle : celle que la
				// gomme a vraiment pu passer, bornée par mu·Fs. La traînée de ce sous-pas
				// sert au suivant — il n'y a donc aucune circularité, et le lissage sur
				// 0,1 s rend ce décalage d'un pas invisible.
				{
					const float32 tau = mTuning.corneringDragTau > 1e-4f ? mTuning.corneringDragTau : 1e-4f;
					const float32 aF = 1.f - std::exp(-h / tau);
					w.latForceFilt += ((Jlat / h) - w.latForceFilt) * aF;
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
