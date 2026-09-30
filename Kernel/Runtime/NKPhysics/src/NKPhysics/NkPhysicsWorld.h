#pragma once
// =============================================================================
// NkPhysicsWorld.h — Monde de simulation du corps rigide. [SCAFFOLD / SPEC]
//
// Possède EN INTERNE un collision::NkWorld (détection) ; chaque NkRigidBody
// référence un body de collision (collisionId). Boucle Step(dt) :
//   1. intégrer forces -> vitesses          (NkIntegrator, gravité/damping)
//   2. collision.Step()                      (broadphase DBVH + manifolds NKCollision)
//   3. solveur de contacts (vitesse)         (NkContactSolver : impulses + warm-start)
//   4. intégrer vitesses -> positions        (NkIntegrator)
//   5. correction positionnelle              (anti-enfoncement)
//   6. re-synchroniser les shapes collision  (collision.SetShape)
//   7. sommeil / réveil des îlots            (M6)
//
// Les requêtes (raycast/overlap/shapecast/sweep) sont déléguées à NKCollision et
// remontées au niveau NkRigidBody.
// =============================================================================
#include "NKPhysics/NkRigidBody.h"
#include "NKPhysics/NkContactSolver.h"
#include "NKPhysics/NkJoint.h"
#include "NKCollision/NKCollision.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace physics {

		// Événement de zone de détection : `trigger` (le corps trigger) chevauche `other`.
		struct NkTriggerEvent {
				NkBodyId trigger = NK_INVALID_BODY;
				NkBodyId other = NK_INVALID_BODY;
		};

		// Un contact d'un corps donne, lu dans les paires du dernier pas (2026-09-29).
		// C'est la requete « suis-je au sol » d'un controleur de personnage : elle
		// vaut pour toute forme, sans lancer de rayon.
		struct NkBodyContact {
				NkBodyId other = NK_INVALID_BODY;
				NkVec3f normal{};	  ///< unitaire, de l'AUTRE corps VERS le corps demande (sol -> (0, 1, 0))
				float32 depth = 0.f;  ///< penetration la plus forte de la paire
				int32 points = 0;
		};

		// Nombre maximal de sommets d'un polygone 2D DYNAMIQUE (copie tenue par le
		// monde). NkColClip n'en lit deja que 8 ; au-dela, le polygone est tronque.
		static constexpr uint32 NK_SOMMETS_2D_MAX = 16u;

		class NkPhysicsWorld {
			public:
				explicit NkPhysicsWorld(const NkPhysicsConfig &cfg = {}) noexcept;

				// Crée un corps (def + forme de collision) ; renvoie son id. [spec M0]
				// ⚠️ `shape` est en repère MONDE (déjà placée à def.position) : la forme de
				// repos LOCALE en est dérivée par l'inverse de la pose. Une forme centrée
				// à l'origine pour un corps créé ailleurs naît DÉCALÉE — et entre en
				// contact avec tout ce qui a fait la même erreur (mesuré le 2026-09-03).
				NkBodyId CreateBody(const NkBodyDef &def, const collision::NkShape &shape);
				void DestroyBody(NkBodyId id);
				NkRigidBody *GetBody(NkBodyId id) noexcept;
				const NkRigidBody *GetBody(NkBodyId id) const noexcept;

				// Avance la simulation de `dt` (découpé en `config.subSteps` sous-pas internes).
				void Step(float32 dt);
				// Avance à PAS FIXE (déterministe) : accumule `realDt` et exécute des Step de
				// `config.fixedTimeStep` (au plus `maxSubSteps`). Renvoie le nb de pas exécutés.
				int32 Advance(float32 realDt);

				// ── Articulations (M7) ───────────────────────────────────────
				// Joint à distance : garde les 2 ancres (monde) à leur distance courante.
				NkJointId CreateDistanceJoint(NkBodyId a, NkBodyId b, const NkVec3f &anchorAWorld,
											  const NkVec3f &anchorBWorld);
				// Joint ball (point-à-point) : les 2 corps partagent le pivot (monde).
				NkJointId CreateBallJoint(NkBodyId a, NkBodyId b, const NkVec3f &pivotWorld);
				// Joint revolute (charnière 1 DOF) : pivot + axe de rotation (monde) — coude, genou, trappe.
				NkJointId CreateRevoluteJoint(NkBodyId a, NkBodyId b, const NkVec3f &pivotWorld,
											  const NkVec3f &axisWorld);
				// Joint weld (soudure rigide) : verrouille position ET orientation relatives.
				NkJointId CreateWeldJoint(NkBodyId a, NkBodyId b, const NkVec3f &pivotWorld);
				// M8 : moteur/drive PD vers un angle cible (ragdoll actif) + limites d'angle, sur un REVOLUTE.
				void SetRevoluteMotor(NkJointId id, float32 targetAngle, float32 kp, float32 maxTorque);
				void SetRevoluteLimit(NkJointId id, float32 lower, float32 upper);
				void DestroyJoint(NkJointId id);

				NkVector<NkJoint> &Joints() noexcept {
					return mJoints;
				}

				const NkVector<NkJoint> &Joints() const noexcept {
					return mJoints;
				}

				// Pilotage (utile pour les corps KINEMATIC : plateformes, ascenseurs…).
				void SetLinearVelocity(NkBodyId id, const NkVec3f &v) noexcept {
					if (NkRigidBody *b = GetBody(id))
						b->linearVelocity = v;
				}

				void SetAngularVelocity(NkBodyId id, const NkVec3f &w) noexcept {
					if (NkRigidBody *b = GetBody(id))
						b->angularVelocity = w;
				}

				// ── Requêtes physiques (M13) : renvoient des NkBodyId ────────
				// Raycast : remplit `outBody` (corps touché) + `hit`. Filtre par layer.
				bool Raycast(const collision::NkRay3D &ray, NkBodyId &outBody, collision::NkRayHit3D &hit,
							 uint32 layerMask = 0xFFFFFFFFu) const;
				// Tous les corps chevauchant la forme `s` -> ids physiques.
				uint32 OverlapShape(const collision::NkShape &s, NkVector<NkBodyId> &out,
									uint32 layerMask = 0xFFFFFFFFu) const;
				// Rayon 2D EXACT (NKCollision, 2026-09-29), dans le plan XY : rend le
				// corps touche. `direction` est normalisee ici. Les zones (triggers)
				// sont ignorees par defaut, et `ignorer` (le corps qui tire) aussi.
				bool Raycast2D(const NkVec2f &origin, const NkVec2f &direction, float32 maxDistance, NkBodyId &outBody,
							   collision::NkRayHit2D &hit, uint32 layerMask = 0xFFFFFFFFu, bool ignoreTriggers = true,
							   NkBodyId ignore = NK_INVALID_BODY) const;
				// Les contacts du corps `id` au DERNIER sous-pas (paires de NKCollision,
				// zones exclues), normale orientee vers lui. Rend leur nombre.
				uint32 BodyContacts(NkBodyId id, NkVector<NkBodyContact> &out) const;
				// Meme requete dans un tableau de l'appelant (au plus `max`), sans
				// allocation : celle d'un controleur, a chaque pas fixe. Rend le
				// nombre ECRIT.
				uint32 BodyContacts(NkBodyId id, NkBodyContact *out, uint32 max) const;

				// Événements de trigger (zones) calculés par Step (corps flag NK_BODY_TRIGGER).
				const NkVector<NkTriggerEvent> &TriggerEnter() const noexcept {
					return mTrigEnter;
				}

				const NkVector<NkTriggerEvent> &TriggerStay() const noexcept {
					return mTrigStay;
				}

				const NkVector<NkTriggerEvent> &TriggerExit() const noexcept {
					return mTrigExit;
				}

				// Contacts entre corps SOLIDES (ni l'un ni l'autre trigger), calcules
				// par Step. `trigger` y designe simplement le premier corps de la
				// paire. Ajoute le 2026-09-29 : les triggers avaient leurs
				// evenements, pas les chocs — un jeu ne pouvait pas savoir qu'une
				// balle avait touche un mur sans interroger chaque paire lui-meme.
				const NkVector<NkTriggerEvent> &ContactEnter() const noexcept {
					return mContactEnter;
				}
				const NkVector<NkTriggerEvent> &ContactExit() const noexcept {
					return mContactExit;
				}

				// ── Validation « physiquement correct » (M10) ────────────────
				// Requêtes sur les corps DYNAMIQUES filtrés par `layerMask` (passer le `group`
				// d'un ragdoll pour ne mesurer que lui). Base de la validation type Cascadeur.
				float32 TotalMass(uint32 layerMask = 0xFFFFFFFFu) const;
				NkVec3f CenterOfMass(uint32 layerMask = 0xFFFFFFFFu) const;			// somme(m·p)/somme(m)
				NkVec3f LinearMomentum(uint32 layerMask = 0xFFFFFFFFu) const;		// somme(m·v)
				NkVec3f CenterOfMassVelocity(uint32 layerMask = 0xFFFFFFFFu) const; // P/M
				NkVec3f AngularMomentum(const NkVec3f &about,
										uint32 layerMask = 0xFFFFFFFFu) const; // somme(r×m·v + I·ω)

				// Réglages.
				// ── Véhicules (2026-09-03) : mis à jour DANS le sous-pas fixe ─────
				// Le jeu n'appelle jamais leur Step : il appelle Advance(dt), et le
				// monde fait avancer les véhicules au pas fixe, AVANT l'intégration.
				void RegisterVehicle(class NkVehicle *v) noexcept;
				void UnregisterVehicle(class NkVehicle *v) noexcept;

				void SetGravity(const NkVec3f &g) noexcept {
					mConfig.gravity = g;
				}

				const NkPhysicsConfig &Config() const noexcept {
					return mConfig;
				}

				NkVector<NkRigidBody> &Bodies() noexcept {
					return mBodies;
				}

				const NkVector<NkRigidBody> &Bodies() const noexcept {
					return mBodies;
				}

				// Requêtes (déléguées à NKCollision, résultat -> NkBodyId). [spec M10]
				// bool Raycast(const collision::NkRay3D& r, NkBodyId& hitBody, ...);
				// uint32 OverlapShape(const collision::NkShape& s, NkVector<NkBodyId>& out, ...);

			private:
				// Impulse accumulée d'un point de contact, conservée pour le warm-start
				// (clé = paire de corps + feature-id NKCollision).
				struct NkWarmEntry {
						uint32 a = 0, b = 0, id = 0;
						float32 n = 0.f, t1 = 0.f, t2 = 0.f;
				};

				// ── Polygones 2D (2026-09-29) ───────────────────────────────────
				// ⚠️ LE DEFAUT QUE CECI CORRIGE, MESURE : NkShape::Polygon2D ne
				// POSSEDE pas ses sommets (pointeur vers ceux de l'appelant, en
				// MONDE), et NkTransformShape ne transforme que p0. Un polygone
				// dynamique gardait donc sa forme de collision a l'endroit de sa
				// creation : lache de 2 m, il traversait le sol et se trouvait a
				// y = -42 m apres 3 s (test_rigides2d, r2, avant correctif).
				// Le monde tient desormais une COPIE des sommets (repere local +
				// monde) et recalcule les sommets monde a chaque sous-pas.
				struct NkSommets2D {
						NkBodyId corps = NK_INVALID_BODY;
						uint32 nombre = 0;
						NkVec3f local[NK_SOMMETS_2D_MAX];
						NkVec3f monde[NK_SOMMETS_2D_MAX];
				};
				NkVector<NkSommets2D> mSommets2D;
				NkSommets2D *Sommets2D(NkBodyId id) noexcept;
				// Refait les sommets monde de chaque polygone et repointe sa forme de
				// collision : le tableau `mSommets2D` peut avoir ete REALLOUE.
				void SynchroniserSommets2D() noexcept;
				// enable2D : vitesses, pose et orientation ramenees au plan XY.
				void ContraindrePlan(NkRigidBody &b) const noexcept;

				NkRigidBody *FindByCollisionId(uint32 cid) noexcept;
				const NkRigidBody *FindByCollisionId(uint32 cid) const noexcept;
				void SolveContacts(float32 dt); // M1..M3 : impulses séquentielles + warm-start
				void CorrectPositions();		// M4 : split-impulse (projection positionnelle)
				void WakeContacts();			// M6 : réveiller les corps touchés par un perturbateur
				void UpdateSleep(float32 dt);	// M6 : endormir les corps immobiles
				void SolveJoints(float32 dt);	// M7 : contraintes d'articulation
				void Substep(float32 h);		// M12 : un pas de simulation atomique
				NkVector<class NkVehicle *> mVehicles; // roues : forces DANS le pas fixe
				void ProcessTriggers();			// M13 : mappe les events collision -> triggers

				NkPhysicsConfig mConfig;
				collision::NkWorld mCollision; // détection (DBVH, manifolds)
				NkVector<NkRigidBody> mBodies;
				// ⚠️ MEMBRE MORT : jamais appelé. Le solveur qui tourne est
				//    `SolveContacts` ci-dessus. Conservé parce qu'il dit une
				//    intention (jalons M1..M4) ; voir NkContactSolver.h.
				NkContactSolver mSolver;
				NkVector<NkWarmEntry> mWarm;							   // cache d'impulses (frame précédente)
				NkVector<NkJoint> mJoints;								   // articulations (M7)
				NkVector<NkTriggerEvent> mTrigEnter, mTrigStay, mTrigExit; // M13
				NkVector<NkTriggerEvent> mContactEnter, mContactExit;	   // chocs entre solides
				float32 mAccumulator = 0.f;								   // pas fixe (M12)
				NkBodyId mNextId = 1u;
				NkJointId mNextJointId = 1u;
		};

	} // namespace physics
} // namespace nkentseu
