// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkPhysicsWorld.cpp — Monde de simulation du corps rigide. [M0]
// M0 : intégration semi-implicite (gravité) + délégation détection à NKCollision +
// synchronisation des shapes. Pas encore de solveur (les corps se traversent) -> M1.
// =============================================================================
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKPhysics/NkVehicle.h"
#include "NKPhysics/NkIntegrator.h"

namespace nkentseu {
	namespace physics {

		using collision::NkShapeType;

		// ── Masse + inertie (diagonale) depuis la forme et la densité ─────────
		static void NkComputeMassProps(const collision::NkShape &s, float32 density, NkBodyType type, uint32 flags,
									   float32 &invMass, NkVec3f &invInertiaDiag) noexcept {
			if (type != NkBodyType::DYNAMIC) {
				invMass = 0.f;
				invInertiaDiag = {0, 0, 0};
				return;
			}
			float32 mass = 1.f;
			NkVec3f I{1.f, 1.f, 1.f}; // défauts
			switch (s.type) {
				case NkShapeType::NK_SPHERE:
				case NkShapeType::NK_CIRCLE2D: {
					const float32 r = s.radius;
					const float32 vol = (s.type == NkShapeType::NK_SPHERE) ? (4.f / 3.f) * 3.14159265f * r * r * r
																		   : 3.14159265f * r * r;
					mass = density * vol;
					const float32 i = (s.type == NkShapeType::NK_SPHERE) ? 0.4f * mass * r * r : 0.5f * mass * r * r;
					I = {i, i, i};
					break;
				}
				case NkShapeType::NK_BOX3D: {
					const NkVec3f h = s.p1;
					const float32 a = 2.f * h.x, b = 2.f * h.y, c = 2.f * h.z;
					mass = density * (a * b * c);
					const float32 k = mass / 12.f;
					I = {k * (b * b + c * c), k * (a * a + c * c), k * (a * a + b * b)};
					break;
				}
				case NkShapeType::NK_BOX2D: {
					const NkVec3f h = s.p1;
					const float32 a = 2.f * h.x, b = 2.f * h.y;
					mass = density * (a * b);
					const float32 iz = mass * (a * a + b * b) / 12.f;
					I = {1.f, 1.f, iz};
					break; // 2D : seule la rotation Z compte
				}
				default: { // approx : boîte englobante (AABB)
					collision::NkAABB3D bb = collision::NkComputeAABB3D(s);
					NkVec3f d = bb.max - bb.min;
					if (d.x <= 0.f)
						d.x = 1.f;
					if (d.y <= 0.f)
						d.y = 1.f;
					if (d.z <= 0.f)
						d.z = 1.f;
					mass = density * (d.x * d.y * d.z);
					const float32 k = mass / 12.f;
					I = {k * (d.y * d.y + d.z * d.z), k * (d.x * d.x + d.z * d.z), k * (d.x * d.x + d.y * d.y)};
					break;
				}
			}
			if (mass < 1e-6f)
				mass = 1e-6f;
			invMass = 1.f / mass;
			const bool fixedRot = (flags & NK_BODY_FIXED_ROT) != 0;
			invInertiaDiag =
				fixedRot ? NkVec3f{0, 0, 0}
						 : NkVec3f{I.x > 0 ? 1.f / I.x : 0.f, I.y > 0 ? 1.f / I.y : 0.f, I.z > 0 ? 1.f / I.z : 0.f};
		}

		// NkTransformShape (forme MONDE depuis la forme de repos) vit dans NkRigidBody.h depuis le
		// 2026-09-05 : le tissu (NkCloth::AddCollidersFromWorld) en a besoin, et une fonction
		// `static` de ce fichier était invisible du dehors -- même geste que NkInvInertiaApply.

		// Forme de REPOS (locale) depuis la forme monde initiale + pose initiale (inverse).
		static collision::NkShape NkComputeRestShape(const collision::NkShape &world, const NkVec3f &pos,
													 const NkQuatf &q) noexcept {
			using T = NkShapeType;
			const NkQuatf cq = q.Conjugate();
			collision::NkShape s = world;
			switch (world.type) {
				case T::NK_BOX3D:
					s.p0 = cq * (world.p0 - pos);
					s.orientation = cq * world.orientation;
					break;
				case T::NK_CAPSULE3D:
				case T::NK_SEGMENT2D:
				case T::NK_CAPSULE2D:
					s.p0 = cq * (world.p0 - pos);
					s.p1 = cq * (world.p1 - pos);
					break;
				case T::NK_CYLINDER3D:
				case T::NK_CONE3D:
					s.p0 = cq * (world.p0 - pos);
					s.p1 = cq * world.p1;
					break;
				default:
					s.p0 = cq * (world.p0 - pos);
					break;
			}
			return s;
		}

		// Masse et inertie EXACTES d'un polygone 2D, autour de l'origine du corps
		// (sommets donnes relativement a elle). Eventail de triangles (0, a, b) :
		// aire = cross(a, b) / 2, inertie = rho * cross(a, b) / 12 * (a.a + a.b + b.b).
		// ⚠️ Le solveur prend la POSITION du corps pour centre de masse : un
		// polygone dont le centroide n'est pas a la position tourne autour d'un
		// point faux. A l'appelant de le centrer (c'est ce que fait une caisse).
		static void NkMassePolygone2D(const NkVec3f *local, uint32 n, float32 density, uint32 flags, float32 &invMass,
									  NkVec3f &invInertiaDiag) noexcept {
			float32 aire = 0.f;
			float32 inertie = 0.f;
			for (uint32 i = 0; i < n; ++i) {
				const NkVec3f &a = local[i];
				const NkVec3f &b = local[(i + 1u) % n];
				const float32 c = a.x * b.y - a.y * b.x;
				aire += 0.5f * c;
				inertie += c * (a.x * a.x + a.y * a.y + a.x * b.x + a.y * b.y + b.x * b.x + b.y * b.y) / 12.f;
			}
			// Sommets en sens horaire : les deux sommes changent de signe ensemble.
			aire = aire < 0.f ? -aire : aire;
			inertie = inertie < 0.f ? -inertie : inertie;
			float32 masse = density * aire;
			if (masse < 1e-6f)
				masse = 1e-6f;
			invMass = 1.f / masse;
			const float32 iz = density * inertie;
			const bool fixedRot = (flags & NK_BODY_FIXED_ROT) != 0;
			invInertiaDiag = fixedRot ? NkVec3f{0, 0, 0} : NkVec3f{1.f, 1.f, iz > 1e-12f ? 1.f / iz : 0.f};
		}

		NkPhysicsWorld::NkPhysicsWorld(const NkPhysicsConfig &cfg) noexcept : mConfig(cfg) {
		}

		NkPhysicsWorld::NkSommets2D *NkPhysicsWorld::Sommets2D(NkBodyId id) noexcept {
			for (uint32 i = 0; i < (uint32)mSommets2D.Size(); ++i)
				if (mSommets2D[i].corps == id)
					return &mSommets2D[i];
			return nullptr;
		}

		void NkPhysicsWorld::SynchroniserSommets2D() noexcept {
			for (uint32 i = 0; i < (uint32)mSommets2D.Size(); ++i) {
				NkSommets2D &s = mSommets2D[i];
				NkRigidBody *b = GetBody(s.corps);
				if (b == nullptr)
					continue;
				for (uint32 k = 0; k < s.nombre; ++k) {
					NkVec3f p = b->position + b->orientation * s.local[k];
					p.z = 0.f;
					s.monde[k] = p;
				}
				// La forme de repos pointe sur la copie LOCALE, la forme du monde de
				// collision sur la copie MONDE : les deux vivent ici, pas chez
				// l'appelant.
				b->restShape.verts = s.local;
				if (collision::NkBody *cb = mCollision.GetBody(b->collisionId)) {
					collision::NkShape monde = cb->shape;
					monde.verts = s.monde;
					monde.vertCount = s.nombre;
					mCollision.SetShape(b->collisionId, monde);
				}
			}
		}

		void NkPhysicsWorld::ContraindrePlan(NkRigidBody &b) const noexcept {
			b.linearVelocity.z = 0.f;
			b.angularVelocity.x = 0.f;
			b.angularVelocity.y = 0.f;
			b.position.z = 0.f;
			// La part de l'orientation autour de Z (decomposition « twist ») : on
			// garde (0, 0, z, w) renormalise.
			// ⚠️ SEULEMENT si elle a quitte le plan : renormaliser un quaternion deja
			// plan change ses derniers bits, et une simulation qui ne sortait pas du
			// plan divergeait de l'ancienne (mesure, blob sur caisse de Physic2D,
			// u3 : 0,8459 -> 0,8144 m). Sans sortie du plan, enable2D ne change RIEN.
			if (b.orientation.x == 0.f && b.orientation.y == 0.f)
				return;
			const float32 l = math::NkSqrt(b.orientation.z * b.orientation.z + b.orientation.w * b.orientation.w);
			if (l > 1e-9f) {
				b.orientation.x = 0.f;
				b.orientation.y = 0.f;
				b.orientation.z = b.orientation.z / l;
				b.orientation.w = b.orientation.w / l;
			}
		}

		NkBodyId NkPhysicsWorld::CreateBody(const NkBodyDef &def, const collision::NkShape &shape) {
			NkRigidBody b;
			b.id = mNextId++;
			b.type = def.type;
			b.position = def.position;
			b.orientation = def.orientation;
			b.linearVelocity = def.linearVelocity;
			b.angularVelocity = def.angularVelocity;
			b.material = def.material;
			b.flags = def.flags;
			b.linearDamping = def.linearDamping;
			b.angularDamping = def.angularDamping;
			b.gravityScale = def.gravityScale;
			b.user = def.user;
			b.layer = def.layer;
			NkComputeMassProps(shape, def.material.density, def.type, def.flags, b.invMass, b.invInertiaDiag);
			b.restShape = NkComputeRestShape(shape, def.position, def.orientation); // forme locale (pour la synchro)
			if (mConfig.enable2D)
				ContraindrePlan(b);
			// Polygone ou triangle 2D : le monde en garde une COPIE (voir NkSommets2D).
			// (2026-10-01) La CHAINE 2D aussi : elle ne possedait pas plus ses sommets
			// que le polygone, et un decor concave cree depuis un tampon de
			// l'appelant (Unkeny, AjouterCorps) aurait pointe dans une pile morte.
			const bool chaine = shape.type == NkShapeType::NK_CHAIN2D && shape.verts != nullptr && shape.vertCount >= 2u;
			const bool polygone = ((shape.type == NkShapeType::NK_POLYGON2D || shape.type == NkShapeType::NK_TRIANGLE2D) &&
								   shape.verts != nullptr && shape.vertCount >= 3u) ||
								  chaine;
			if (polygone) {
				NkSommets2D s;
				s.corps = b.id;
				s.nombre = shape.vertCount < NK_SOMMETS_2D_MAX ? shape.vertCount : NK_SOMMETS_2D_MAX;
				const NkQuatf cq = def.orientation.Conjugate();
				for (uint32 k = 0; k < s.nombre; ++k) {
					NkVec3f w = shape.verts[k];
					w.z = 0.f;
					s.monde[k] = w;
					s.local[k] = cq * (w - def.position);
				}
				// Une chaine n'a pas d'aire : sa masse reste celle de NkComputeMassProps.
				if (def.type == NkBodyType::DYNAMIC && !chaine)
					NkMassePolygone2D(s.local, s.nombre, def.material.density, def.flags, b.invMass, b.invInertiaDiag);
				mSommets2D.PushBack(s);
			}
			collision::NkShape formeMonde = shape;
			if (polygone)
				formeMonde.verts = nullptr; // repointee juste apres, sur la copie
			// (2026-10-01) L'angle d'une boite 2D est RELATIF au corps (le repos le
			// garde, NkTransformShape y ajoute l'orientation). Un corps STATIQUE
			// n'est jamais resynchronise (Step, etape 6) : son angle de MONDE est
			// pose ici, une fois. Sans cette ligne, une plateforme posee inclinee
			// restait DROITE pour les corps rigides, quand les particules (qui
			// passent par NkTransformShape) la voyaient inclinee. Angle nul : rien
			// ne change, au bit pres.
			if (def.type == NkBodyType::STATIC && shape.type == NkShapeType::NK_BOX2D &&
				(def.orientation.z != 0.f || def.orientation.x != 0.f || def.orientation.y != 0.f))
				formeMonde.rotation = shape.rotation + 2.f * nkentseu::math::NkAtan2(def.orientation.z, def.orientation.w);
			b.collisionId = mCollision.AddBody(formeMonde, def.layer, def.mask, def.user);
			if (def.flags & NK_BODY_TRIGGER)
				mCollision.SetTrigger(b.collisionId, true);
			mBodies.PushBack(b);
			if (polygone)
				SynchroniserSommets2D(); // aussi pour les AUTRES : le tableau a pu etre realloue
			return b.id;
		}

		void NkPhysicsWorld::DestroyBody(NkBodyId id) {
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
				if (mBodies[i].id == id) {
					mCollision.RemoveBody(mBodies[i].collisionId);
					mBodies.RemoveAt(i);
					break;
				}
			for (uint32 i = 0; i < (uint32)mSommets2D.Size(); ++i)
				if (mSommets2D[i].corps == id) {
					mSommets2D.RemoveAt(i);
					// Les suivants ont GLISSE d'une case : leurs formes pointaient a
					// cote.
					SynchroniserSommets2D();
					return;
				}
		}

		NkRigidBody *NkPhysicsWorld::GetBody(NkBodyId id) noexcept {
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
				if (mBodies[i].id == id)
					return &mBodies[i];
			return nullptr;
		}

		const NkRigidBody *NkPhysicsWorld::GetBody(NkBodyId id) const noexcept {
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
				if (mBodies[i].id == id)
					return &mBodies[i];
			return nullptr;
		}

		// ── Validation physique (M10) : COM + moments ────────────────────────
		// Inertie (forward) en repère monde appliquée à ω : I·ω = R·(Idiag ⊙ (Rᵀω)).
		// NkInertiaApply : remontee dans NkRigidBody.h (2026-09-03), partagee.

		float32 NkPhysicsWorld::TotalMass(uint32 lm) const {
			float32 m = 0.f;
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				const NkRigidBody &b = mBodies[i];
				if (b.invMass > 0.f && (b.layer & lm))
					m += 1.f / b.invMass;
			}
			return m;
		}

		NkVec3f NkPhysicsWorld::CenterOfMass(uint32 lm) const {
			NkVec3f s{};
			float32 m = 0.f;
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				const NkRigidBody &b = mBodies[i];
				if (b.invMass > 0.f && (b.layer & lm)) {
					const float32 mi = 1.f / b.invMass;
					s = s + b.position * mi;
					m += mi;
				}
			}
			return (m > 0.f) ? s * (1.f / m) : NkVec3f{};
		}

		NkVec3f NkPhysicsWorld::LinearMomentum(uint32 lm) const {
			NkVec3f p{};
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				const NkRigidBody &b = mBodies[i];
				if (b.invMass > 0.f && (b.layer & lm))
					p = p + b.linearVelocity * (1.f / b.invMass);
			}
			return p;
		}

		NkVec3f NkPhysicsWorld::CenterOfMassVelocity(uint32 lm) const {
			const float32 m = TotalMass(lm);
			return (m > 0.f) ? LinearMomentum(lm) * (1.f / m) : NkVec3f{};
		}

		NkVec3f NkPhysicsWorld::AngularMomentum(const NkVec3f &about, uint32 lm) const {
			NkVec3f L{};
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				const NkRigidBody &b = mBodies[i];
				if (b.invMass <= 0.f || !(b.layer & lm))
					continue;
				const NkVec3f r = b.position - about;
				L = L + r.Cross(b.linearVelocity * (1.f / b.invMass)) + NkInertiaApply(b, b.angularVelocity);
			}
			return L;
		}

		// ── Requêtes physiques (M13) ─────────────────────────────────────────
		bool NkPhysicsWorld::Raycast(const collision::NkRay3D &ray, NkBodyId &outBody, collision::NkRayHit3D &hit,
									 uint32 lm) const {
			if (!mCollision.Raycast3D(ray, hit, lm))
				return false;
			outBody = NK_INVALID_BODY;
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
				if (mBodies[i].collisionId == hit.bodyId) {
					outBody = mBodies[i].id;
					break;
				}
			return true;
		}

		uint32 NkPhysicsWorld::OverlapShape(const collision::NkShape &s, NkVector<NkBodyId> &out, uint32 lm) const {
			out.Clear();
			NkVector<uint32> cids;
			mCollision.Overlap(s, cids, lm); // ids de collision
			for (uint32 c = 0; c < (uint32)cids.Size(); ++c)
				for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
					if (mBodies[i].collisionId == cids[c]) {
						out.PushBack(mBodies[i].id);
						break;
					}
			return (uint32)out.Size();
		}

		// ── Plateformes a sens unique (2026-10-02) ─────────────────────────────
		// ⚠️ LA DECISION EST TENUE TANT QUE LA PAIRE DURE. Jugee a chaque sous-pas,
		//    une balle entree PAR DESSOUS, arrivee au sommet de sa course a moitie
		//    dans la plateforme, verrait la normale tournee vers le haut et serait
		//    EJECTEE au-dessus ; et une balle posee qui rebondit (vitesse vers le
		//    haut un instant) serait lachee puis traversee. Box2D tient de meme
		//    l'etat « active » d'un contact sur sa duree de vie.
		void NkPhysicsWorld::FiltrerSensUnique(float32 dt) {
			mSensUniquePrecedent = mSensUnique;
			mSensUnique.Clear();
			const auto &pairs = mCollision.Pairs();
			for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
				const collision::NkCollisionPair &p = pairs[i];
				const NkRigidBody *A = FindByCollisionId(p.a);
				const NkRigidBody *B = FindByCollisionId(p.b);
				if (!A || !B || ((A->flags | B->flags) & NK_BODY_ONE_WAY) == 0u)
					continue;
				NkDecisionSensUnique d;
				d.a = p.a;
				d.b = p.b;
				bool connue = false;
				for (uint32 k = 0; k < (uint32)mSensUniquePrecedent.Size(); ++k) {
					const NkDecisionSensUnique &v = mSensUniquePrecedent[k];
					if ((v.a == p.a && v.b == p.b) || (v.a == p.b && v.b == p.a)) {
						d.ignorer = v.ignorer;
						connue = true;
						break;
					}
				}
				if (!connue) {
					if (p.manifold.count <= 0)
						continue; // pas encore de contact : rien a decider
					// La plateforme et l'autre. Deux plateformes : on les laisse se
					// traverser (elles sont statiques d'ordinaire).
					const bool aEst = (A->flags & NK_BODY_ONE_WAY) != 0u;
					const bool bEst = (B->flags & NK_BODY_ONE_WAY) != 0u;
					if (aEst && bEst) {
						d.ignorer = true;
					} else {
						const NkRigidBody &plat = aEst ? *A : *B;
						const NkRigidBody &autre = aEst ? *B : *A;
						// Normale de la PLATEFORME vers l'autre (celle du manifold va de A vers B).
						const NkVec3f n = aEst ? p.manifold.normal : p.manifold.normal * -1.f;
						const NkVec3f haut = plat.orientation * NkVec3f{0.f, 1.f, 0.f};
						const float32 nHaut = n.Dot(haut);
						const float32 vRel = (autre.linearVelocity - plat.linearVelocity).Dot(haut);
						float32 profondeur = 0.f;
						for (int32 k = 0; k < p.manifold.count; ++k)
							profondeur = math::NkMax(profondeur, p.manifold.points[k].depth);
						// Arrive PAR DESSUS : normale vers le haut, ne monte pas, et a peine
						// enfonce (la tolerance suit la vitesse de chute sur ce sous-pas).
						const float32 tolerance = 0.05f + math::NkMax(0.f, -vRel) * dt * 2.f + mConfig.slop;
						const bool porte = nHaut > 0.5f && vRel <= 0.5f && profondeur <= tolerance;
						d.ignorer = !porte;
					}
				}
				mSensUnique.PushBack(d);
			}
		}

		bool NkPhysicsWorld::PaireIgnoree(uint32 a, uint32 b) const noexcept {
			for (uint32 k = 0; k < (uint32)mSensUnique.Size(); ++k) {
				const NkDecisionSensUnique &v = mSensUnique[k];
				if ((v.a == a && v.b == b) || (v.a == b && v.b == a))
					return v.ignorer;
			}
			return false;
		}

		bool NkPhysicsWorld::PaireIgnoreePrecedente(uint32 a, uint32 b) const noexcept {
			for (uint32 k = 0; k < (uint32)mSensUniquePrecedent.Size(); ++k) {
				const NkDecisionSensUnique &v = mSensUniquePrecedent[k];
				if ((v.a == a && v.b == b) || (v.a == b && v.b == a))
					return v.ignorer;
			}
			return false;
		}

		void NkPhysicsWorld::ProcessTriggers() {
			mTrigEnter.Clear();
			mTrigStay.Clear();
			mTrigExit.Clear();
			mContactEnter.Clear();
			mContactExit.Clear();
			auto mapEvents = [&](const NkVector<collision::NkCollisionEvent> &evs, NkVector<NkTriggerEvent> &out,
								 NkVector<NkTriggerEvent> *solides) {
				for (uint32 i = 0; i < (uint32)evs.Size(); ++i) {
					NkRigidBody *A = FindByCollisionId(evs[i].a);
					NkRigidBody *B = FindByCollisionId(evs[i].b);
					if (!A || !B)
						continue;
					if (A->flags & NK_BODY_TRIGGER)
						out.PushBack(NkTriggerEvent{A->id, B->id});
					else if (B->flags & NK_BODY_TRIGGER)
						out.PushBack(NkTriggerEvent{B->id, A->id});
					else if (solides != nullptr) {
						// (2026-10-02) Une paire TRAVERSEE (sens unique) n'est pas un choc :
						// ni debut, ni fin (la fin se juge sur la decision d'avant).
						const bool ignoree = solides == &mContactEnter ? PaireIgnoree(evs[i].a, evs[i].b)
																	   : PaireIgnoreePrecedente(evs[i].a, evs[i].b);
						if (!ignoree)
							solides->PushBack(NkTriggerEvent{A->id, B->id});
					}
				}
			};
			mapEvents(mCollision.EnterEvents(), mTrigEnter, &mContactEnter);
			mapEvents(mCollision.StayEvents(), mTrigStay, nullptr);
			mapEvents(mCollision.ExitEvents(), mTrigExit, &mContactExit);
		}

		// ── Articulations (M7) ───────────────────────────────────────────────
		NkJointId NkPhysicsWorld::CreateDistanceJoint(NkBodyId a, NkBodyId b, const NkVec3f &anchorAWorld,
													  const NkVec3f &anchorBWorld) {
			NkRigidBody *A = GetBody(a);
			NkRigidBody *B = GetBody(b);
			if (!A || !B)
				return NK_INVALID_JOINT;
			NkJoint j;
			j.type = NkJointType::DISTANCE;
			j.a = a;
			j.b = b;
			j.localAnchorA = A->orientation.Conjugate() * (anchorAWorld - A->position);
			j.localAnchorB = B->orientation.Conjugate() * (anchorBWorld - B->position);
			const NkVec3f d = anchorBWorld - anchorAWorld;
			j.restLength = math::NkSqrt(d.Dot(d));
			mJoints.PushBack(j);
			return mNextJointId++;
		}

		NkJointId NkPhysicsWorld::CreateBallJoint(NkBodyId a, NkBodyId b, const NkVec3f &pivotWorld) {
			NkRigidBody *A = GetBody(a);
			NkRigidBody *B = GetBody(b);
			if (!A || !B)
				return NK_INVALID_JOINT;
			NkJoint j;
			j.type = NkJointType::BALL;
			j.a = a;
			j.b = b;
			j.localAnchorA = A->orientation.Conjugate() * (pivotWorld - A->position);
			j.localAnchorB = B->orientation.Conjugate() * (pivotWorld - B->position);
			mJoints.PushBack(j);
			return mNextJointId++;
		}

		NkJointId NkPhysicsWorld::CreateRevoluteJoint(NkBodyId a, NkBodyId b, const NkVec3f &pivotWorld,
													  const NkVec3f &axisWorld) {
			NkRigidBody *A = GetBody(a);
			NkRigidBody *B = GetBody(b);
			if (!A || !B)
				return NK_INVALID_JOINT;
			NkJoint j;
			j.type = NkJointType::REVOLUTE;
			j.a = a;
			j.b = b;
			j.localAnchorA = A->orientation.Conjugate() * (pivotWorld - A->position);
			j.localAnchorB = B->orientation.Conjugate() * (pivotWorld - B->position);
			const float32 al = math::NkSqrt(axisWorld.Dot(axisWorld));
			const NkVec3f axis = (al > 1e-6f) ? axisWorld * (1.f / al) : NkVec3f{0, 0, 1};
			j.localAxisA = A->orientation.Conjugate() * axis;
			j.localAxisB = B->orientation.Conjugate() * axis;
			j.refRotation = A->orientation.Conjugate() * B->orientation; // référence pour mesurer l'angle
			mJoints.PushBack(j);
			return mNextJointId++;
		}

		void NkPhysicsWorld::SetRevoluteMotor(NkJointId id, float32 targetAngle, float32 kp, float32 maxTorque) {
			if (id == NK_INVALID_JOINT || id > (NkJointId)mJoints.Size())
				return;
			NkJoint &j = mJoints[id - 1];
			j.motorEnabled = true;
			j.targetAngle = targetAngle;
			j.motorKp = kp;
			j.maxMotorTorque = maxTorque;
		}

		void NkPhysicsWorld::SetRevoluteLimit(NkJointId id, float32 lower, float32 upper) {
			if (id == NK_INVALID_JOINT || id > (NkJointId)mJoints.Size())
				return;
			NkJoint &j = mJoints[id - 1];
			j.limitEnabled = true;
			j.lowerAngle = lower;
			j.upperAngle = upper;
		}

		NkJointId NkPhysicsWorld::CreateWeldJoint(NkBodyId a, NkBodyId b, const NkVec3f &pivotWorld) {
			NkRigidBody *A = GetBody(a);
			NkRigidBody *B = GetBody(b);
			if (!A || !B)
				return NK_INVALID_JOINT;
			NkJoint j;
			j.type = NkJointType::WELD;
			j.a = a;
			j.b = b;
			j.localAnchorA = A->orientation.Conjugate() * (pivotWorld - A->position);
			j.localAnchorB = B->orientation.Conjugate() * (pivotWorld - B->position);
			j.refRotation = A->orientation.Conjugate() * B->orientation; // orientation relative cible
			mJoints.PushBack(j);
			return mNextJointId++;
		}

		void NkPhysicsWorld::DestroyJoint(NkJointId id) {
			// id renvoyé = mNextJointId au moment de la création -> index = id-1 si pas de trous.
			if (id == NK_INVALID_JOINT)
				return;
			for (uint32 i = 0; i < (uint32)mJoints.Size(); ++i)
				if ((NkJointId)(i + 1) == id) {
					mJoints.RemoveAt(i);
					return;
				}
		}

		NkRigidBody *NkPhysicsWorld::FindByCollisionId(uint32 cid) noexcept {
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
				if (mBodies[i].collisionId == cid)
					return &mBodies[i];
			return nullptr;
		}

		const NkRigidBody *NkPhysicsWorld::FindByCollisionId(uint32 cid) const noexcept {
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
				if (mBodies[i].collisionId == cid)
					return &mBodies[i];
			return nullptr;
		}

		// ── Requetes 2D (2026-09-29) ──────────────────────────────────────────
		bool NkPhysicsWorld::Raycast2D(const NkVec2f &origin, const NkVec2f &direction, float32 maxDistance, NkBodyId &outBody,
									   collision::NkRayHit2D &hit, uint32 layerMask, bool ignoreTriggers, NkBodyId ignore) const {
			outBody = NK_INVALID_BODY;
			const float32 l = math::NkSqrt(direction.x * direction.x + direction.y * direction.y);
			if (l < 1e-9f || maxDistance <= 0.f)
				return false;
			collision::NkRay2D r;
			r.origin = origin;
			r.dir = NkVec2f(direction.x / l, direction.y / l);
			r.maxT = maxDistance;
			uint32 ignoreCid = 0u;
			if (ignore != NK_INVALID_BODY)
				if (const NkRigidBody *b = GetBody(ignore))
					ignoreCid = b->collisionId;
			if (!mCollision.Raycast2D(r, hit, layerMask, ignoreTriggers, ignoreCid))
				return false;
			if (const NkRigidBody *b = FindByCollisionId(hit.bodyId))
				outBody = b->id;
			return true;
		}

		uint32 NkPhysicsWorld::BodyContacts(NkBodyId id, NkVector<NkBodyContact> &out) const {
			out.Clear();
			NkBodyContact tampon[32];
			const uint32 n = BodyContacts(id, tampon, 32u);
			for (uint32 i = 0; i < n; ++i)
				out.PushBack(tampon[i]);
			return n;
		}

		uint32 NkPhysicsWorld::BodyContacts(NkBodyId id, NkBodyContact *out, uint32 max) const {
			uint32 ecrits = 0;
			const NkRigidBody *self = GetBody(id);
			if (self == nullptr || out == nullptr)
				return 0u;
			const auto &pairs = mCollision.Pairs();
			for (uint32 i = 0; i < (uint32)pairs.Size() && ecrits < max; ++i) {
				const collision::NkCollisionPair &p = pairs[i];
				const bool estA = p.a == self->collisionId;
				if (!estA && p.b != self->collisionId)
					continue;
				const NkRigidBody *autre = FindByCollisionId(estA ? p.b : p.a);
				if (autre == nullptr || (autre->flags & NK_BODY_TRIGGER) || (self->flags & NK_BODY_TRIGGER))
					continue;
				// (2026-10-02) Une plateforme traversee ne porte pas : « au sol » est faux.
				if (((autre->flags | self->flags) & NK_BODY_ONE_WAY) && PaireIgnoree(p.a, p.b))
					continue;
				NkBodyContact c;
				c.other = autre->id;
				// Normale du manifold : de A vers B. Vers le corps demande, donc
				// inversee s'il est A.
				c.normal = estA ? p.manifold.normal * -1.f : p.manifold.normal;
				c.points = p.manifold.count;
				for (int32 k = 0; k < p.manifold.count; ++k)
					c.depth = math::NkMax(c.depth, p.manifold.points[k].depth);
				out[ecrits++] = c;
			}
			return ecrits;
		}

		// Inertie inverse en repère MONDE appliquée à un vecteur (torque -> accel ang.) :
		//   invI_world * v = R * (invInertiaDiag ⊙ (Rᵀ v))   avec R = orientation.
		// NkInvInertiaApply : remontee dans NkRigidBody.h (2026-09-03), partagee.

		// ── M1+M2 : solveur de contacts (normale + frottement + angulaire) ────
		struct NkSolverPoint {
				NkVec3f rA, rB; // bras de levier (contact - centre de masse)
				float32 nMass = 0.f, t1Mass = 0.f, t2Mass = 0.f;
				float32 bias = 0.f; // biais normal (Baumgarte + restitution)
				float32 nImp = 0.f, t1Imp = 0.f, t2Imp = 0.f;
				uint32 id = 0; // feature-id NKCollision (matching warm-start)
		};

		struct NkSolverContact {
				NkRigidBody *a;
				NkRigidBody *b;
				NkVec3f n, t1, t2; // normale + 2 tangentes
				float32 friction;
				NkSolverPoint p[4];
				int32 count;
		};

		// Masse effective d'une contrainte le long de `dir` (linéaire + angulaire).
		static float32 NkEffMass(const NkRigidBody &A, const NkRigidBody &B, const NkVec3f &rA, const NkVec3f &rB,
								 const NkVec3f &dir) noexcept {
			const NkVec3f rnA = rA.Cross(dir), rnB = rB.Cross(dir);
			const float32 ang = rnA.Dot(NkInvInertiaApply(A, rnA)) + rnB.Dot(NkInvInertiaApply(B, rnB));
			const float32 k = A.invMass + B.invMass + ang;
			return k > 0.f ? 1.f / k : 0.f;
		}

		// Résout K·x = rhs (K donnée par ses 3 colonnes) par la règle de Cramer.
		static NkVec3f NkSolve3(const NkVec3f &kx, const NkVec3f &ky, const NkVec3f &kz, const NkVec3f &rhs) noexcept {
			const float32 det = kx.Dot(ky.Cross(kz));
			if (math::NkAbs(det) < 1e-12f)
				return {0, 0, 0};
			const float32 inv = 1.f / det;
			return {rhs.Dot(ky.Cross(kz)) * inv, kx.Dot(rhs.Cross(kz)) * inv, kx.Dot(ky.Cross(rhs)) * inv};
		}

		// Masse effective 3x3 d'un point-à-point (colonne i = K·e_i).
		static NkVec3f NkPointMassCol(const NkRigidBody &A, const NkRigidBody &B, const NkVec3f &rA, const NkVec3f &rB,
									  const NkVec3f &e) noexcept {
			return e * (A.invMass + B.invMass) - rA.Cross(NkInvInertiaApply(A, rA.Cross(e))) -
				   rB.Cross(NkInvInertiaApply(B, rB.Cross(e)));
		}

		void NkPhysicsWorld::SolveContacts(float32 dt) {
			(void)dt; // M4 : la vitesse n'utilise plus dt (Baumgarte retiré)
			const auto &pairs = mCollision.Pairs();
			NkVector<NkSolverContact> cs;
			for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
				const collision::NkCollisionPair &p = pairs[i];
				NkRigidBody *A = FindByCollisionId(p.a);
				NkRigidBody *B = FindByCollisionId(p.b);
				if (!A || !B)
					continue;
				if ((A->flags & NK_BODY_TRIGGER) || (B->flags & NK_BODY_TRIGGER))
					continue;
				if (A->invMass + B->invMass <= 0.f)
					continue;
				if (!A->IsAwake() && !B->IsAwake())
					continue; // M6 : deux corps endormis -> rien à faire
				if (((A->flags | B->flags) & NK_BODY_ONE_WAY) && PaireIgnoree(p.a, p.b))
					continue; // (2026-10-02) traversee : plateforme a sens unique
				NkSolverContact c;
				c.a = A;
				c.b = B;
				c.n = p.manifold.normal;
				c.count = 0;
				// base tangente orthonormée à n
				c.t1 = (math::NkAbs(c.n.x) > 0.9f) ? c.n.Cross(NkVec3f{0, 1, 0}) : c.n.Cross(NkVec3f{1, 0, 0});
				c.t1 = c.t1.Normalized();
				c.t2 = c.n.Cross(c.t1);
				c.friction = NkMixFriction(A->material, B->material);
				const float32 e = NkMixRestitution(A->material, B->material);
				for (int32 k = 0; k < p.manifold.count && k < 4; ++k) {
					const collision::NkContactPoint3D &mp = p.manifold.points[k];
					NkSolverPoint sp;
					sp.rA = mp.point - A->position;
					sp.rB = mp.point - B->position;
					sp.nMass = NkEffMass(*A, *B, sp.rA, sp.rB, c.n);
					sp.t1Mass = NkEffMass(*A, *B, sp.rA, sp.rB, c.t1);
					sp.t2Mass = NkEffMass(*A, *B, sp.rA, sp.rB, c.t2);
					// vitesse relative initiale (au point) pour la restitution
					const NkVec3f vA = A->linearVelocity + A->angularVelocity.Cross(sp.rA);
					const NkVec3f vB = B->linearVelocity + B->angularVelocity.Cross(sp.rB);
					const float32 vn0 = (vB - vA).Dot(c.n);
					// M4 : plus de biais Baumgarte dans la vitesse (géré par split-impulse
					// positionnel) -> pas d'injection d'énergie. On garde la restitution.
					const float32 rest = (vn0 < -1.f) ? -e * vn0 : 0.f;
					sp.bias = rest;
					sp.id = mp.id;
					// M3 : récupérer l'impulse accumulée de la frame précédente (même feature).
					for (uint32 w = 0; w < (uint32)mWarm.Size(); ++w) {
						const NkWarmEntry &we = mWarm[w];
						if (we.a == A->id && we.b == B->id && we.id == sp.id) {
							sp.nImp = we.n;
							sp.t1Imp = we.t1;
							sp.t2Imp = we.t2;
							break;
						}
					}
					c.p[c.count++] = sp;
				}
				if (c.count > 0)
					cs.PushBack(c);
			}
			// M3 : WARM-START — ré-appliquer les impulses accumulées avant d'itérer.
			for (uint32 i = 0; i < (uint32)cs.Size(); ++i) {
				NkSolverContact &c = cs[i];
				for (int32 k = 0; k < c.count; ++k) {
					NkSolverPoint &sp = c.p[k];
					const NkVec3f P = c.n * sp.nImp + c.t1 * sp.t1Imp + c.t2 * sp.t2Imp;
					c.a->linearVelocity = c.a->linearVelocity - P * c.a->invMass;
					c.a->angularVelocity = c.a->angularVelocity - NkInvInertiaApply(*c.a, sp.rA.Cross(P));
					c.b->linearVelocity = c.b->linearVelocity + P * c.b->invMass;
					c.b->angularVelocity = c.b->angularVelocity + NkInvInertiaApply(*c.b, sp.rB.Cross(P));
				}
			}
			// Itérations séquentielles : frottement (borné par μ·N) puis normale.
			for (int32 it = 0; it < mConfig.velocityIters; ++it) {
				for (uint32 i = 0; i < (uint32)cs.Size(); ++i) {
					NkSolverContact &c = cs[i];
					for (int32 k = 0; k < c.count; ++k) {
						NkSolverPoint &sp = c.p[k];
						// -- frottement (2 axes), borné par le cône de Coulomb --
						const float32 maxF = c.friction * sp.nImp;
						for (int32 ti = 0; ti < 2; ++ti) {
							const NkVec3f t = (ti == 0) ? c.t1 : c.t2;
							float32 &acc = (ti == 0) ? sp.t1Imp : sp.t2Imp;
							const float32 tm = (ti == 0) ? sp.t1Mass : sp.t2Mass;
							const NkVec3f vA = c.a->linearVelocity + c.a->angularVelocity.Cross(sp.rA);
							const NkVec3f vB = c.b->linearVelocity + c.b->angularVelocity.Cross(sp.rB);
							float32 lambda = -tm * (vB - vA).Dot(t);
							const float32 newImp = math::NkClamp(acc + lambda, -maxF, maxF);
							lambda = newImp - acc;
							acc = newImp;
							const NkVec3f P = t * lambda;
							c.a->linearVelocity = c.a->linearVelocity - P * c.a->invMass;
							c.a->angularVelocity = c.a->angularVelocity - NkInvInertiaApply(*c.a, sp.rA.Cross(P));
							c.b->linearVelocity = c.b->linearVelocity + P * c.b->invMass;
							c.b->angularVelocity = c.b->angularVelocity + NkInvInertiaApply(*c.b, sp.rB.Cross(P));
						}
						// -- normale (non-pénétration + restitution + Baumgarte) --
						const NkVec3f vA = c.a->linearVelocity + c.a->angularVelocity.Cross(sp.rA);
						const NkVec3f vB = c.b->linearVelocity + c.b->angularVelocity.Cross(sp.rB);
						const float32 vn = (vB - vA).Dot(c.n);
						float32 lambda = -sp.nMass * (vn - sp.bias);
						const float32 newImp = math::NkMax(0.f, sp.nImp + lambda);
						lambda = newImp - sp.nImp;
						sp.nImp = newImp;
						const NkVec3f P = c.n * lambda;
						c.a->linearVelocity = c.a->linearVelocity - P * c.a->invMass;
						c.a->angularVelocity = c.a->angularVelocity - NkInvInertiaApply(*c.a, sp.rA.Cross(P));
						c.b->linearVelocity = c.b->linearVelocity + P * c.b->invMass;
						c.b->angularVelocity = c.b->angularVelocity + NkInvInertiaApply(*c.b, sp.rB.Cross(P));
					}
				}
			}
			// M3 : sauvegarder les impulses pour le warm-start de la frame suivante.
			mWarm.Clear();
			for (uint32 i = 0; i < (uint32)cs.Size(); ++i) {
				const NkSolverContact &c = cs[i];
				for (int32 k = 0; k < c.count; ++k) {
					NkWarmEntry we;
					we.a = c.a->id;
					we.b = c.b->id;
					we.id = c.p[k].id;
					we.n = c.p[k].nImp;
					we.t1 = c.p[k].t1Imp;
					we.t2 = c.p[k].t2Imp;
					mWarm.PushBack(we);
				}
			}
		}

		// ── M4 : split-impulse — projection positionnelle (sans énergie) ─────
		// Pousse les corps en chevauchement le long de la normale, proportionnellement
		// à leur masse inverse. Agit sur la POSITION (pas la vitesse) -> pas de rebond
		// parasite. Une passe par frame, facteur < 1 -> la pénétration converge vers slop.
		void NkPhysicsWorld::CorrectPositions() {
			const auto &pairs = mCollision.Pairs();
			const float32 factor = 0.8f; // part de pénétration corrigée par frame
			for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
				const collision::NkCollisionPair &p = pairs[i];
				NkRigidBody *A = FindByCollisionId(p.a);
				NkRigidBody *B = FindByCollisionId(p.b);
				if (!A || !B)
					continue;
				if ((A->flags & NK_BODY_TRIGGER) || (B->flags & NK_BODY_TRIGGER))
					continue;
				if (((A->flags | B->flags) & NK_BODY_ONE_WAY) && PaireIgnoree(p.a, p.b))
					continue; // (2026-10-02) traversee : rien a corriger
				// M6 : un corps endormi est immovable (invMass effective = 0).
				const float32 imA = A->IsAwake() ? A->invMass : 0.f;
				const float32 imB = B->IsAwake() ? B->invMass : 0.f;
				const float32 kSum = imA + imB;
				if (kSum <= 0.f)
					continue;
				float32 maxPen = 0.f;
				for (int32 k = 0; k < p.manifold.count; ++k)
					maxPen = math::NkMax(maxPen, p.manifold.points[k].depth);
				const float32 pen = maxPen - mConfig.slop;
				if (pen <= 0.f)
					continue;
				const NkVec3f corr = p.manifold.normal * (factor * pen / kSum);
				A->position = A->position - corr * imA; // normale A->B : A recule, B avance
				B->position = B->position + corr * imB;
			}
		}

		// ── M6 : sommeil ──────────────────────────────────────────────────────
		// Un corps « perturbateur » peut réveiller ses voisins endormis.
		static bool NkIsDisturbing(const NkRigidBody &b, const NkPhysicsConfig &cfg) noexcept {
			if (b.type == NkBodyType::STATIC)
				return false;
			const float32 lv2 = b.linearVelocity.Dot(b.linearVelocity);
			const float32 av2 = b.angularVelocity.Dot(b.angularVelocity);
			const bool moving =
				lv2 > cfg.linearSleepTol * cfg.linearSleepTol || av2 > cfg.angularSleepTol * cfg.angularSleepTol;
			return (b.type == NkBodyType::KINEMATIC) ? moving : (b.IsAwake() && moving);
		}

		static void NkWake(NkRigidBody &b) noexcept {
			b.flags &= ~NK_BODY_SLEEPING;
			b.sleepTimer = 0.f;
		}

		void NkPhysicsWorld::WakeContacts() {
			const auto &pairs = mCollision.Pairs();
			for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
				NkRigidBody *A = FindByCollisionId(pairs[i].a);
				NkRigidBody *B = FindByCollisionId(pairs[i].b);
				if (!A || !B)
					continue;
				if (NkIsDisturbing(*A, mConfig) && B->IsDynamic() && !B->IsAwake())
					NkWake(*B);
				if (NkIsDisturbing(*B, mConfig) && A->IsDynamic() && !A->IsAwake())
					NkWake(*A);
			}
		}

		void NkPhysicsWorld::UpdateSleep(float32 dt) {
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				NkRigidBody &b = mBodies[i];
				if (!b.IsDynamic() || !b.IsAwake())
					continue;
				const float32 lv2 = b.linearVelocity.Dot(b.linearVelocity);
				const float32 av2 = b.angularVelocity.Dot(b.angularVelocity);
				if (lv2 < mConfig.linearSleepTol * mConfig.linearSleepTol &&
					av2 < mConfig.angularSleepTol * mConfig.angularSleepTol)
					b.sleepTimer += dt;
				else
					b.sleepTimer = 0.f;
				if (b.sleepTimer >= mConfig.sleepTime) {
					b.flags |= NK_BODY_SLEEPING;
					b.linearVelocity = {0, 0, 0};
					b.angularVelocity = {0, 0, 0};
				}
			}
		}

		// ── M7 : solveur d'articulations (contraintes séquentielles + warm-start) ──
		void NkPhysicsWorld::SolveJoints(float32 dt) {
			if (mJoints.Size() == 0)
				return;
			const float32 beta = 0.2f, invDt = 1.f / dt;
			// Préparation + warm-start.
			for (uint32 i = 0; i < (uint32)mJoints.Size(); ++i) {
				NkJoint &j = mJoints[i];
				if (!j.enabled)
					continue;
				j.motorImpulse = 0.f; // borne moteur RE-évaluée par pas
				NkRigidBody *A = GetBody(j.a);
				NkRigidBody *B = GetBody(j.b);
				if (!A || !B)
					continue;
				// un joint transmet le mouvement -> réveiller le partenaire dynamique.
				if (A->IsDynamic() && !A->IsAwake() && B->IsAwake())
					NkWake(*A);
				if (B->IsDynamic() && !B->IsAwake() && A->IsAwake())
					NkWake(*B);
				const NkVec3f rA = A->orientation * j.localAnchorA, rB = B->orientation * j.localAnchorB;
				NkVec3f P{};
				if (j.type == NkJointType::BALL || j.type == NkJointType::REVOLUTE || j.type == NkJointType::WELD)
					P = j.impulse; // point-à-point
				else if (j.type == NkJointType::DISTANCE) {
					const NkVec3f d = (B->position + rB) - (A->position + rA);
					const float32 L = math::NkSqrt(d.Dot(d));
					if (L > 1e-6f)
						P = d * (j.impulse.x / L);
				}
				if (j.type == NkJointType::WELD) { // couple de warm-start (verrou angulaire)
					A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, j.angImpulse);
					B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, j.angImpulse);
				}
				A->linearVelocity = A->linearVelocity - P * A->invMass;
				A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, rA.Cross(P));
				B->linearVelocity = B->linearVelocity + P * B->invMass;
				B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, rB.Cross(P));
			}
			// Itérations de vitesse.
			for (int32 it = 0; it < mConfig.velocityIters; ++it) {
				for (uint32 i = 0; i < (uint32)mJoints.Size(); ++i) {
					NkJoint &j = mJoints[i];
					if (!j.enabled)
						continue;
					NkRigidBody *A = GetBody(j.a);
					NkRigidBody *B = GetBody(j.b);
					if (!A || !B || (!A->IsAwake() && !B->IsAwake()))
						continue;
					const NkVec3f rA = A->orientation * j.localAnchorA, rB = B->orientation * j.localAnchorB;
					const NkVec3f wA = A->position + rA, wB = B->position + rB;
					if (j.type == NkJointType::BALL || j.type == NkJointType::REVOLUTE || j.type == NkJointType::WELD) {
						// (1) point-à-point : résolution de BLOC 3x3 exacte (stable même si
						// l'ancre est loin du centre de masse — cas des membres).
						const NkVec3f C = wB - wA;
						const NkVec3f vrel = (B->linearVelocity + B->angularVelocity.Cross(rB)) -
											 (A->linearVelocity + A->angularVelocity.Cross(rA));
						const NkVec3f kx = NkPointMassCol(*A, *B, rA, rB, {1, 0, 0});
						const NkVec3f ky = NkPointMassCol(*A, *B, rA, rB, {0, 1, 0});
						const NkVec3f kz = NkPointMassCol(*A, *B, rA, rB, {0, 0, 1});
						const NkVec3f P = NkSolve3(kx, ky, kz, (vrel + C * (beta * invDt)) * -1.f);
						j.impulse = j.impulse + P;
						A->linearVelocity = A->linearVelocity - P * A->invMass;
						A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, rA.Cross(P));
						B->linearVelocity = B->linearVelocity + P * B->invMass;
						B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, rB.Cross(P));
						// (2) REVOLUTE : aligner les axes de charnière (2 DOF angulaires)
						if (j.type == NkJointType::REVOLUTE) {
							const NkVec3f aA = (A->orientation * j.localAxisA).Normalized();
							const NkVec3f aB = (B->orientation * j.localAxisB).Normalized();
							const NkVec3f err = aA.Cross(aB); // ~ axe×sin(désalignement)
							NkVec3f t1 =
								(math::NkAbs(aA.x) > 0.9f) ? aA.Cross(NkVec3f{0, 1, 0}) : aA.Cross(NkVec3f{1, 0, 0});
							t1 = t1.Normalized();
							const NkVec3f t2 = aA.Cross(t1);
							for (int32 ti = 0; ti < 2; ++ti) {
								const NkVec3f t = (ti == 0) ? t1 : t2;
								const float32 angMass =
									1.f / (t.Dot(NkInvInertiaApply(*A, t)) + t.Dot(NkInvInertiaApply(*B, t)) + 1e-12f);
								const float32 wn = (B->angularVelocity - A->angularVelocity).Dot(t);
								const float32 lambda = -angMass * (wn + beta * invDt * err.Dot(t));
								const NkVec3f T = t * lambda; // couple pur (pas de force linéaire)
								A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, T);
								B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, T);
							}
							// (2b) M8 : moteur (drive PD vers un angle) + limites, autour de l'axe.
							if (j.motorEnabled || j.limitEnabled) {
								const NkQuatf qDelta =
									j.refRotation.Conjugate() * (A->orientation.Conjugate() * B->orientation);
								const float32 proj =
									qDelta.x * j.localAxisA.x + qDelta.y * j.localAxisA.y + qDelta.z * j.localAxisA.z;
								const float32 theta = 2.f * math::NkAtan2(proj, qDelta.w); // angle de charnière (rad)
								const float32 angMass = 1.f / (aA.Dot(NkInvInertiaApply(*A, aA)) +
															   aA.Dot(NkInvInertiaApply(*B, aA)) + 1e-12f);
								// MOTEUR : amène la vitesse relative autour de l'axe vers kp·(cible−θ), couple borné.
								if (j.motorEnabled) {
									const float32 desiredW = j.motorKp * (j.targetAngle - theta);
									const float32 cdot = (B->angularVelocity - A->angularVelocity).Dot(aA) - desiredW;
									float32 lambda = -angMass * cdot;
									const float32 maxImp = j.maxMotorTorque * dt;
									const float32 old = j.motorImpulse;
									j.motorImpulse = math::NkClamp(old + lambda, -maxImp, maxImp);
									lambda = j.motorImpulse - old;
									const NkVec3f T = aA * lambda;
									A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, T);
									B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, T);
								}
								// LIMITES : contrainte unilatérale quand θ sort de [lower, upper].
								if (j.limitEnabled) {
									const float32 wAxis = (B->angularVelocity - A->angularVelocity).Dot(aA);
									if (theta <= j.lowerAngle) {
										float32 lambda = -angMass * (wAxis + beta * invDt * (theta - j.lowerAngle));
										if (lambda < 0.f)
											lambda = 0.f; // ne pousse que vers le haut
										const NkVec3f T = aA * lambda;
										A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, T);
										B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, T);
									} else if (theta >= j.upperAngle) {
										float32 lambda = -angMass * (wAxis + beta * invDt * (theta - j.upperAngle));
										if (lambda > 0.f)
											lambda = 0.f; // ne pousse que vers le bas
										const NkVec3f T = aA * lambda;
										A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, T);
										B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, T);
									}
								}
							}
						}
						// (3) WELD : verrouiller l'orientation relative (3 DOF angulaires, bloc 3x3)
						if (j.type == NkJointType::WELD) {
							const NkQuatf qErr =
								j.refRotation.Conjugate() * (A->orientation.Conjugate() * B->orientation);
							const float32 s = (qErr.w < 0.f) ? -2.f : 2.f; // axe-angle petit (chemin court)
							const NkVec3f errWorld = A->orientation * NkVec3f{qErr.x * s, qErr.y * s, qErr.z * s};
							const NkVec3f wrel = B->angularVelocity - A->angularVelocity;
							const NkVec3f kx = NkInvInertiaApply(*A, {1, 0, 0}) + NkInvInertiaApply(*B, {1, 0, 0});
							const NkVec3f ky = NkInvInertiaApply(*A, {0, 1, 0}) + NkInvInertiaApply(*B, {0, 1, 0});
							const NkVec3f kz = NkInvInertiaApply(*A, {0, 0, 1}) + NkInvInertiaApply(*B, {0, 0, 1});
							const NkVec3f L = NkSolve3(kx, ky, kz, (wrel + errWorld * (beta * invDt)) * -1.f);
							j.angImpulse = j.angImpulse + L;
							A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, L);
							B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, L);
						}
					} else { // DISTANCE
						const NkVec3f d = wB - wA;
						const float32 L = math::NkSqrt(d.Dot(d));
						if (L < 1e-6f)
							continue;
						const NkVec3f n = d * (1.f / L);
						const float32 C = L - j.restLength;
						const NkVec3f vrel = (B->linearVelocity + B->angularVelocity.Cross(rB)) -
											 (A->linearVelocity + A->angularVelocity.Cross(rA));
						const float32 mass = NkEffMass(*A, *B, rA, rB, n);
						const float32 lambda = -mass * (vrel.Dot(n) + beta * invDt * C);
						j.impulse.x += lambda;
						const NkVec3f P = n * lambda;
						A->linearVelocity = A->linearVelocity - P * A->invMass;
						A->angularVelocity = A->angularVelocity - NkInvInertiaApply(*A, rA.Cross(P));
						B->linearVelocity = B->linearVelocity + P * B->invMass;
						B->angularVelocity = B->angularVelocity + NkInvInertiaApply(*B, rB.Cross(P));
					}
				}
			}
		}

		void NkPhysicsWorld::RegisterVehicle(NkVehicle *v) noexcept {
			for (uint32 i = 0; i < (uint32)mVehicles.Size(); ++i)
				if (mVehicles[(NkVector<NkVehicle *>::SizeType)i] == v)
					return;
			mVehicles.PushBack(v);
		}
		void NkPhysicsWorld::UnregisterVehicle(NkVehicle *v) noexcept {
			for (uint32 i = 0; i < (uint32)mVehicles.Size(); ++i)
				if (mVehicles[(NkVector<NkVehicle *>::SizeType)i] == v) {
					mVehicles[(NkVector<NkVehicle *>::SizeType)i] =
						mVehicles[(NkVector<NkVehicle *>::SizeType)(mVehicles.Size() - 1)];
					mVehicles.PopBack();
					return;
				}
		}

		void NkPhysicsWorld::Substep(float32 dt) {
			if (dt <= 0.f)
				return;
			// 0) roues : forces de suspension + impulsions d'adhérence, AU PAS FIXE,
			//    avant l'intégration : les forces entrent dans force/torque, les
			//    impulsions dans les vitesses -- comme celles du solveur.
			for (uint32 i = 0; i < (uint32)mVehicles.Size(); ++i)
				mVehicles[(NkVector<NkVehicle *>::SizeType)i]->StepFixed(dt);
			// 1) forces -> vitesses
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				NkRigidBody &b = mBodies[i];
				if (b.type == NkBodyType::DYNAMIC && b.IsAwake())
					NkIntegrateVelocity(b, mConfig.gravity, dt);
			}
			// 2) détection (broadphase DBVH + manifolds multi-points)
			mCollision.Step();
			// 2a) (2026-10-02) les plateformes a SENS UNIQUE : quelles paires laisser
			//     passer. Avant tout ce qui lit les paires (evenements, solveur).
			FiltrerSensUnique(dt);
			// 2b) réveiller les corps endormis touchés par un perturbateur
			WakeContacts();
			// 2c) événements de trigger (zones de détection) -> NkBodyId
			ProcessTriggers();
			// 3) solveur de contacts (vitesses, sans Baumgarte) + warm-start
			SolveContacts(dt);
			// 3b) solveur d'articulations (joints) + warm-start
			SolveJoints(dt);
			// 3c) enable2D (lu depuis le 2026-09-29 ; avant, il n'etait lu NULLE
			//     part) : tout ce que forces, contacts et joints ont mis hors du
			//     plan XY en est retire AVANT l'integration des positions.
			if (mConfig.enable2D)
				for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
					if (mBodies[i].type != NkBodyType::STATIC)
						ContraindrePlan(mBodies[i]);
			// 4) vitesses -> positions (DYNAMIC + KINEMATIC) avec CCD anti-tunneling
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				NkRigidBody &b = mBodies[i];
				if (b.type == NkBodyType::STATIC || !b.IsAwake())
					continue;
				NkIntegrateOrientation(b, dt);
				const NkVec3f tr = b.linearVelocity * dt;
				bool moved = false;
				// M11 : balayage des corps rapides marqués CCD (évite de traverser un mur fin).
				if ((b.flags & NK_BODY_CCD) && b.type == NkBodyType::DYNAMIC) {
					float32 minExt = 1e30f;
					if (collision::NkBody *cb = mCollision.GetBody(b.collisionId)) {
						const collision::NkAABB3D bb = collision::NkComputeAABB3D(cb->shape);
						const NkVec3f e = bb.max - bb.min;
						minExt = math::NkMin(e.x, math::NkMin(e.y, e.z));
					}
					const float32 len = math::NkSqrt(tr.Dot(tr));
					if (len > 0.5f * minExt) { // déplacement > moitié de l'épaisseur du corps
						collision::NkRayHit3D hit;
						if (mCollision.SweepBody(b.collisionId, tr, hit, 0xFFFFFFFFu)) {
							const float32 frac = math::NkClamp((hit.t - 0.01f) / len, 0.f, 1.f);
							b.position = b.position + tr * frac; // avance jusqu'au TOI (skin 1 cm)
							const float32 vn = b.linearVelocity.Dot(hit.normal);
							if (vn < 0.f)
								b.linearVelocity = b.linearVelocity - hit.normal * vn; // tue la composante entrante
							moved = true;
						}
					}
				}
				if (!moved)
					b.position = b.position + tr;
			}
			// 5) correction positionnelle (split-impulse) — n'affecte que les invMass>0
			CorrectPositions();
			if (mConfig.enable2D)
				for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
					if (mBodies[i].type != NkBodyType::STATIC)
						ContraindrePlan(mBodies[i]);
			// 6) re-synchroniser les shapes de collision sur la pose finale
			for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
				NkRigidBody &b = mBodies[i];
				if (b.type == NkBodyType::STATIC)
					continue; // static : jamais re-sync
				if (mCollision.GetBody(b.collisionId)) {
					// forme monde = forme de repos transformée par la pose (rotation correcte).
					mCollision.SetShape(b.collisionId, NkTransformShape(b.restShape, b.position, b.orientation));
				}
			}
			// 6b) polygones 2D : NkTransformShape ne sait pas deplacer des sommets
			//     (il n'en possede pas) ; le monde les refait, et repointe la forme
			//     que la ligne precedente vient de poser sur la copie LOCALE.
			if (mSommets2D.Size() > 0)
				SynchroniserSommets2D();
			// 7) mise en sommeil des corps immobiles
			UpdateSleep(dt);
		}

		// ── M12 : sous-pas internes (chaînes de joints raides plus stables) ──
		void NkPhysicsWorld::Step(float32 dt) {
			if (dt <= 0.f)
				return;
			const int32 n = (mConfig.subSteps > 1) ? mConfig.subSteps : 1;
			const float32 h = dt / (float32)n;
			for (int32 i = 0; i < n; ++i)
				Substep(h);
		}

		// ── M12 : avance à pas fixe (déterministe, découple sim/affichage) ───
		int32 NkPhysicsWorld::Advance(float32 realDt) {
			if (realDt > 0.f)
				mAccumulator += realDt;
			const float32 h = (mConfig.fixedTimeStep > 0.f) ? mConfig.fixedTimeStep : (1.f / 60.f);
			int32 n = 0;
			while (mAccumulator >= h && n < mConfig.maxSubSteps) {
				Step(h);
				mAccumulator -= h;
				++n;
			}
			return n;
		}

	} // namespace physics
} // namespace nkentseu
