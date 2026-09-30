// AUTEUR : Rihen
// =============================================================================
// test_rigides2d.cpp — la physique RIGIDE dans le plan (2026-09-29).
//
// ⚠️ POURQUOI CE FICHIER
//   La feuille de route d'UnkenyEditor (design/00, § 1 constat 3) listait, EN
//   LISANT le code, quatre doutes sur le rigide 2D : `enable2D` lu nulle part ;
//   un polygone 2D dynamique qui ne suivrait pas son corps ; un Raycast2D exact
//   pour le cercle seulement ; des joints a API 3D jamais essayes dans le plan.
//   Ces temoins les EXECUTENT. Premiere passe (avant tout correctif) : les
//   chiffres sont dans le message de commit, pour que la difference entre « lu »
//   et « mesure » reste visible.
//
// PRE-ENREGISTREMENT — ecrit AVANT d'avoir lu un chiffre :
//   (r1)  enable2D : un corps lance HORS du plan (vz = 1 m/s, wx = 1 rad/s) y
//         est ramene : apres 1 s, |z| < 1 mm et wx = wy = 0 ;
//         (r1n) sans enable2D, il derive (z > 0,5 m) — le drapeau fait quelque chose
//   (r2)  un POLYGONE 2D dynamique (carre de 0,5 m) lache sur un sol statique s'y
//         pose : centre a 0,25 m +/- 3 cm apres 3 s ; et sa forme de collision
//         l'a SUIVI (un recouvrement a sa position touche, a sa position de
//         depart ne touche plus)
//   (r2b) le tampon de sommets de l'appelant peut disparaitre apres CreateBody
//   (r3)  Raycast2D exact : une boite tournee de 45 degres n'est PAS touchee par
//         un rayon qui passe dans le coin de sa boite englobante ; touchee de face,
//         t et normale justes a 1 mm ; capsule, segment, polygone, triangle :
//         t juste a 1 mm ; bodyId rempli ; (r3n) l'ancien chemin
//         (Raycast2D) touche le coin — la difference est mesuree
//   (r4)  joints dans le plan : distance (pendule, longueur a 5 %), pivot
//         (l'ancre tient a 1 cm, le corps tourne), soudure (angle relatif a
//         0,02 rad), rotule ; tous restent dans le plan (|z| < 1 mm)
//   (r5)  BodyContacts : une caisse posee a UN contact, normale (0, 1) vers elle ;
//         (r5n) en l'air, aucun
//   (r6)  NkPhysicsWorld::Raycast2D rend le CORPS touche et ignore les zones
// =============================================================================
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKCollision/NkCollisionWorld.h"
#include "NKMath/NkFunctions.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::physics;

namespace {
	int *gPass = nullptr;
	int *gFail = nullptr;

	void Temoin(bool ok, const char *quoi, float32 valeur) {
		std::fprintf(stderr, "  [%s] %-70s %9.4f\n", ok ? "ok" : "FAIL", quoi, static_cast<double>(valeur));
		if (ok) {
			++*gPass;
		} else {
			++*gFail;
		}
	}

	float32 Absf(float32 v) {
		return v < 0.f ? -v : v;
	}

	float32 AngleZ(const math::NkQuatf &q) {
		return 2.f * math::NkAtan2(q.z, q.w);
	}

	math::NkQuatf Rotation(float32 angle) {
		math::NkQuatf q;
		q.x = 0.f;
		q.y = 0.f;
		q.z = math::NkSin(angle * 0.5f);
		q.w = math::NkCos(angle * 0.5f);
		return q;
	}

	NkBodyId Sol(NkPhysicsWorld &w) {
		NkBodyDef sol;
		sol.type = NkBodyType::STATIC;
		sol.position = NkVec3f(0.f, -0.5f, 0.f);
		return w.CreateBody(sol, collision::NkShape::Box2D(NkVec2f(0.f, -0.5f), NkVec2f(10.f, 0.5f)));
	}

	// (r1)
	void TemoinPlan() {
		for (int32 k = 0; k < 2; ++k) {
			NkPhysicsConfig cfg;
			cfg.gravity = NkVec3f(0.f, 0.f, 0.f);
			cfg.enable2D = k == 0;
			NkPhysicsWorld w(cfg);
			NkBodyDef d;
			d.position = NkVec3f(0.f, 2.f, 0.f);
			d.linearVelocity = NkVec3f(0.5f, 0.f, 1.f);
			d.angularVelocity = NkVec3f(1.f, 0.f, 0.5f);
			d.angularDamping = 0.f;
			const NkBodyId id = w.CreateBody(d, collision::NkShape::Circle2D(NkVec2f(0.f, 2.f), 0.3f));
			for (int32 i = 0; i < 60; ++i) {
				w.Step(1.f / 60.f);
			}
			const NkRigidBody *b = w.GetBody(id);
			const float32 z = b->position.z;
			const float32 hors = Absf(b->orientation.x) + Absf(b->orientation.y);
			if (k == 0) {
				Temoin(Absf(z) < 1.0e-3f && Absf(b->angularVelocity.x) < 1.0e-6f && Absf(b->angularVelocity.y) < 1.0e-6f &&
						   hors < 1.0e-4f && Absf(b->position.x - 0.5f) < 0.01f,
					   "(r1) enable2D : ramene dans le plan (z m apres 1 s)", z);
			} else {
				Temoin(z > 0.5f, "(r1n) sans enable2D : il sort du plan (z m)", z);
			}
		}
	}

	// (r2)
	void TemoinPolygone() {
		NkPhysicsWorld w;
		Sol(w);
		NkBodyDef d;
		d.position = NkVec3f(0.f, 2.f, 0.f);
		NkBodyId id = NK_INVALID_BODY;
		{
			// Le tampon de l'appelant MEURT a la fin de ce bloc : (r2b).
			NkVec3f carre[4] = {NkVec3f(-0.25f, 1.75f, 0.f), NkVec3f(0.25f, 1.75f, 0.f), NkVec3f(0.25f, 2.25f, 0.f),
								NkVec3f(-0.25f, 2.25f, 0.f)};
			id = w.CreateBody(d, collision::NkShape::Polygon2D(carre, 4));
			for (int32 i = 0; i < 4; ++i) {
				carre[i] = NkVec3f(99.f, 99.f, 0.f); // on l'abime : le monde ne doit plus s'en servir
			}
		}
		for (int32 i = 0; i < 180; ++i) {
			w.Step(1.f / 60.f);
		}
		const NkRigidBody *b = w.GetBody(id);
		const float32 y = b->position.y;
		Temoin(Absf(y - 0.25f) < 0.03f, "(r2) polygone dynamique : pose sur le sol (centre y m)", y);
		NkVector<NkBodyId> ici, depart;
		w.OverlapShape(collision::NkShape::Circle2D(NkVec2f(b->position.x, b->position.y + 0.1f), 0.05f), ici);
		w.OverlapShape(collision::NkShape::Circle2D(NkVec2f(0.f, 2.f), 0.05f), depart);
		bool trouve = false;
		for (uint32 i = 0; i < ici.Size(); ++i) {
			trouve = trouve || ici[i] == id;
		}
		Temoin(trouve && depart.Size() == 0u, "(r2) sa forme de collision l'a suivi (touches au depart)",
			   static_cast<float32>(depart.Size()));
	}

	// (r3)
	void TemoinRayons() {
		using namespace collision;
		NkWorld w;
		// Boite 1 x 1 tournee de 45 degres, centree en (0, 0).
		const uint32 boite = w.AddBody(NkShape::Box2D(NkVec2f(0.f, 0.f), NkVec2f(0.5f, 0.5f), 0.7853982f));
		// Rayon qui passe dans le COIN VIDE de la boite englobante (demi-cote
		// 0,707) : la droite x + y = 1,2, parcourue en diagonale. Le losange est
		// |x| + |y| <= 0,707 : il n'est jamais touche ; l'englobante l'est, entre
		// (0,493 ; 0,707) et (0,707 ; 0,493). ⚠️ Premiere version de ce temoin :
		// un rayon VERTICAL en x = 0,6, qui touche bel et bien le losange (en
		// y = -0,107) -- tout rayon parallele a un axe qui traverse l'englobante
		// traverse aussi le losange. Le temoin etait faux, pas le code.
		NkRay2D coin;
		coin.origin = NkVec2f(-1.4f, 2.6f);
		coin.dir = NkVec2f(0.7071068f, -0.7071068f);
		coin.maxT = 10.f;
		NkRayHit2D h;
		const bool touche = w.Raycast2D(coin, h);
		Temoin(!touche, "(r3) boite tournee : un rayon dans le coin de l'englobante la manque", touche ? h.t : 0.f);
		NkRayHit2D hb;
		const bool approche = w.Raycast2DBoites(coin, hb);
		Temoin(approche, "(r3n) l'ancien chemin (englobante) la touche au coin (t)", approche ? hb.t : -1.f);
		// De face : le long de l'axe y = 0 depuis la gauche, la boite tournee est
		// un losange de sommet (-0,707, 0) ; normale (-0,707, +/-0,707).
		NkRay2D face;
		face.origin = NkVec2f(-3.f, 0.1f);
		face.dir = NkVec2f(1.f, 0.f);
		face.maxT = 10.f;
		NkRayHit2D hf;
		const bool okF = w.Raycast2D(face, hf);
		// A y = 0,1, le bord gauche du losange est en x = -(0,707 - 0,1) = -0,607.
		Temoin(okF && Absf(hf.t - (3.f - 0.6071068f)) < 1.0e-3f && hf.bodyId == boite && Absf(hf.normal.x + 0.7071068f) < 1.0e-3f &&
				   Absf(hf.normal.y - 0.7071068f) < 1.0e-3f,
			   "(r3) boite tournee de face : t, normale, bodyId (t)", okF ? hf.t : -1.f);

		NkWorld w2;
		const uint32 caps = w2.AddBody(NkShape::Capsule2D(NkVec2f(5.f, 0.f), NkVec2f(7.f, 0.f), 0.5f));
		NkRay2D rc;
		rc.origin = NkVec2f(6.f, 5.f);
		rc.dir = NkVec2f(0.f, -1.f);
		rc.maxT = 20.f;
		NkRayHit2D hc;
		const bool okC = w2.Raycast2D(rc, hc);
		NkRay2D rc2; // bout arrondi : touche le cercle de (7, 0) a 45 degres
		rc2.origin = NkVec2f(7.f + 0.5f * 0.7071068f, 5.f);
		rc2.dir = NkVec2f(0.f, -1.f);
		rc2.maxT = 20.f;
		NkRayHit2D hc2;
		const bool okC2 = w2.Raycast2D(rc2, hc2);
		Temoin(okC && Absf(hc.t - 4.5f) < 1.0e-3f && hc.bodyId == caps && okC2 && Absf(hc2.t - (5.f - 0.5f * 0.7071068f)) < 1.0e-3f,
			   "(r3) capsule : flanc et bout arrondi (t du bout)", okC2 ? hc2.t : -1.f);

		NkWorld w3;
		const NkVec3f tri[3] = {NkVec3f(0.f, 0.f, 0.f), NkVec3f(2.f, 0.f, 0.f), NkVec3f(0.f, 2.f, 0.f)};
		const NkVec3f penta[5] = {NkVec3f(10.f, 0.f, 0.f), NkVec3f(11.f, 0.f, 0.f), NkVec3f(11.5f, 1.f, 0.f), NkVec3f(10.5f, 1.8f, 0.f),
								  NkVec3f(9.5f, 1.f, 0.f)};
		const uint32 idTri = w3.AddBody(NkShape::Triangle2D(tri));
		const uint32 idPenta = w3.AddBody(NkShape::Polygon2D(penta, 5));
		w3.AddBody(NkShape::Segment2D(NkVec2f(20.f, -1.f), NkVec2f(20.f, 1.f)));
		// Diagonale du triangle : x + y = 2. Rayon vertical en x = 1,5 depuis le
		// haut : entre en y = 0,5, t = 5 - 0,5 = 4,5 ; dans l'englobante (y = 2),
		// l'ancien chemin disait t = 3.
		NkRay2D rt;
		rt.origin = NkVec2f(1.5f, 5.f);
		rt.dir = NkVec2f(0.f, -1.f);
		rt.maxT = 20.f;
		NkRayHit2D ht;
		const bool okT = w3.Raycast2D(rt, ht);
		NkRay2D rp; // pentagone, par le haut en x = 10,5 : le sommet (10,5 ; 1,8)
		rp.origin = NkVec2f(10.5f, 5.f);
		rp.dir = NkVec2f(0.f, -1.f);
		rp.maxT = 20.f;
		NkRayHit2D hp;
		const bool okP = w3.Raycast2D(rp, hp);
		NkRay2D rs;
		rs.origin = NkVec2f(15.f, 0.5f);
		rs.dir = NkVec2f(1.f, 0.f);
		rs.maxT = 20.f;
		NkRayHit2D hs;
		const bool okS = w3.Raycast2D(rs, hs);
		Temoin(okT && Absf(ht.t - 4.5f) < 1.0e-3f && ht.bodyId == idTri && Absf(ht.normal.x - 0.7071068f) < 1.0e-3f,
			   "(r3) triangle : la diagonale, pas l'englobante (t)", okT ? ht.t : -1.f);
		Temoin(okP && Absf(hp.t - 3.2f) < 1.0e-3f && hp.bodyId == idPenta && okS && Absf(hs.t - 5.f) < 1.0e-3f,
			   "(r3) polygone et segment : t juste (t du polygone)", okP ? hp.t : -1.f);
	}

	// (r4)
	void TemoinJoints() {
		// Distance : un pendule de 2 m.
		{
			NkPhysicsConfig cfg;
			cfg.enable2D = true;
			NkPhysicsWorld w(cfg);
			NkBodyDef a;
			a.type = NkBodyType::STATIC;
			a.position = NkVec3f(0.f, 5.f, 0.f);
			const NkBodyId ia = w.CreateBody(a, collision::NkShape::Circle2D(NkVec2f(0.f, 5.f), 0.1f));
			NkBodyDef b;
			b.position = NkVec3f(2.f, 5.f, 0.f);
			const NkBodyId ib = w.CreateBody(b, collision::NkShape::Circle2D(NkVec2f(2.f, 5.f), 0.2f));
			w.CreateDistanceJoint(ia, ib, NkVec3f(0.f, 5.f, 0.f), NkVec3f(2.f, 5.f, 0.f));
			float32 ecart = 0.f;
			float32 zMax = 0.f;
			float32 yMin = 99.f;
			for (int32 i = 0; i < 180; ++i) {
				w.Step(1.f / 60.f);
				const NkRigidBody *r = w.GetBody(ib);
				const float32 dx = r->position.x;
				const float32 dy = r->position.y - 5.f;
				ecart = math::NkMax(ecart, Absf(math::NkSqrt(dx * dx + dy * dy) - 2.f));
				zMax = math::NkMax(zMax, Absf(r->position.z));
				yMin = math::NkMin(yMin, r->position.y);
			}
			Temoin(ecart < 0.1f && zMax < 1.0e-3f && yMin < 3.5f, "(r4) distance 2D : pendule, longueur tenue a 5 % (ecart m)", ecart);
		}
		// Pivot : une planche de 2 m accrochee par un bout.
		{
			NkPhysicsConfig cfg;
			cfg.enable2D = true;
			NkPhysicsWorld w(cfg);
			NkBodyDef a;
			a.type = NkBodyType::STATIC;
			a.position = NkVec3f(0.f, 5.f, 0.f);
			const NkBodyId ia = w.CreateBody(a, collision::NkShape::Circle2D(NkVec2f(0.f, 5.f), 0.05f));
			NkBodyDef b;
			b.position = NkVec3f(1.f, 5.f, 0.f);
			b.layer = 0x2u;
			b.mask = 0x2u; // la planche ne touche pas l'ancre
			const NkBodyId ib = w.CreateBody(b, collision::NkShape::Box2D(NkVec2f(1.f, 5.f), NkVec2f(1.f, 0.05f)));
			w.CreateRevoluteJoint(ia, ib, NkVec3f(0.f, 5.f, 0.f), NkVec3f(0.f, 0.f, 1.f));
			float32 ecart = 0.f;
			float32 zMax = 0.f;
			float32 angleMax = 0.f; // la planche OSCILLE (periode ~2,3 s) : on garde le plus grand angle
			for (int32 i = 0; i < 180; ++i) {
				w.Step(1.f / 60.f);
				const NkRigidBody *r = w.GetBody(ib);
				// L'ancre sur la planche : son bout gauche, (-1, 0) en local.
				const NkVec3f bout = r->position + r->orientation * NkVec3f(-1.f, 0.f, 0.f);
				ecart = math::NkMax(ecart, math::NkSqrt(bout.x * bout.x + (bout.y - 5.f) * (bout.y - 5.f)));
				zMax = math::NkMax(zMax, Absf(r->position.z));
				angleMax = math::NkMax(angleMax, Absf(AngleZ(r->orientation)));
			}
			Temoin(ecart < 0.01f && zMax < 1.0e-3f && angleMax > 0.5f,
				   "(r4) pivot 2D : l'ancre tient a 1 cm, la planche tourne (ecart m)", ecart);
		}
		// Soudure : deux caisses soudees lachees sur le sol, l'une decalee.
		{
			NkPhysicsConfig cfg;
			cfg.enable2D = true;
			NkPhysicsWorld w(cfg);
			Sol(w);
			NkBodyDef a;
			a.position = NkVec3f(0.f, 2.f, 0.f);
			a.layer = 0x2u;
			a.mask = 0x1u;
			const NkBodyId ia = w.CreateBody(a, collision::NkShape::Box2D(NkVec2f(0.f, 2.f), NkVec2f(0.3f, 0.3f)));
			NkBodyDef b;
			b.position = NkVec3f(0.6f, 2.3f, 0.f);
			b.layer = 0x2u;
			b.mask = 0x1u;
			const NkBodyId ib = w.CreateBody(b, collision::NkShape::Box2D(NkVec2f(0.6f, 2.3f), NkVec2f(0.3f, 0.3f)));
			w.CreateWeldJoint(ia, ib, NkVec3f(0.3f, 2.15f, 0.f));
			float32 dAngle = 0.f;
			float32 dDist = 0.f;
			float32 zMax = 0.f;
			const float32 d0 = math::NkSqrt(0.6f * 0.6f + 0.3f * 0.3f);
			for (int32 i = 0; i < 180; ++i) {
				w.Step(1.f / 60.f);
				const NkRigidBody *ra = w.GetBody(ia);
				const NkRigidBody *rb = w.GetBody(ib);
				dAngle = math::NkMax(dAngle, Absf(AngleZ(rb->orientation) - AngleZ(ra->orientation)));
				const NkVec3f d = rb->position - ra->position;
				dDist = math::NkMax(dDist, Absf(math::NkSqrt(d.x * d.x + d.y * d.y) - d0));
				zMax = math::NkMax(zMax, math::NkMax(Absf(ra->position.z), Absf(rb->position.z)));
			}
			const bool tombe = w.GetBody(ia)->position.y < 1.f;
			Temoin(dAngle < 0.02f && dDist < 0.02f && zMax < 1.0e-3f && tombe, "(r4) soudure 2D : angle relatif tenu (rad)", dAngle);
		}
		// Rotule : meme pendule qu'au pivot, par CreateBallJoint.
		{
			NkPhysicsConfig cfg;
			cfg.enable2D = true;
			NkPhysicsWorld w(cfg);
			NkBodyDef a;
			a.type = NkBodyType::STATIC;
			a.position = NkVec3f(0.f, 5.f, 0.f);
			const NkBodyId ia = w.CreateBody(a, collision::NkShape::Circle2D(NkVec2f(0.f, 5.f), 0.05f));
			NkBodyDef b;
			b.position = NkVec3f(1.f, 5.f, 0.f);
			b.layer = 0x2u;
			b.mask = 0x2u;
			const NkBodyId ib = w.CreateBody(b, collision::NkShape::Box2D(NkVec2f(1.f, 5.f), NkVec2f(1.f, 0.05f)));
			w.CreateBallJoint(ia, ib, NkVec3f(0.f, 5.f, 0.f));
			float32 ecart = 0.f;
			float32 zMax = 0.f;
			float32 angleMax = 0.f;
			for (int32 i = 0; i < 180; ++i) {
				w.Step(1.f / 60.f);
				const NkRigidBody *r = w.GetBody(ib);
				const NkVec3f bout = r->position + r->orientation * NkVec3f(-1.f, 0.f, 0.f);
				ecart = math::NkMax(ecart, math::NkSqrt(bout.x * bout.x + (bout.y - 5.f) * (bout.y - 5.f)));
				zMax = math::NkMax(zMax, Absf(r->position.z));
				angleMax = math::NkMax(angleMax, Absf(AngleZ(r->orientation)));
			}
			Temoin(ecart < 0.01f && zMax < 1.0e-3f && angleMax > 0.5f, "(r4) rotule 2D : l'ancre tient a 1 cm, elle tourne (ecart m)",
				   ecart);
		}
	}

	// (r5) (r6)
	void TemoinRequetes() {
		NkPhysicsConfig cfg;
		cfg.enable2D = true;
		NkPhysicsWorld w(cfg);
		const NkBodyId sol = Sol(w);
		NkBodyDef d;
		d.position = NkVec3f(0.f, 0.3f, 0.f);
		const NkBodyId caisse = w.CreateBody(d, collision::NkShape::Box2D(NkVec2f(0.f, 0.3f), NkVec2f(0.3f, 0.3f)));
		NkBodyDef z;
		z.type = NkBodyType::STATIC;
		z.flags = NK_BODY_TRIGGER;
		z.position = NkVec3f(3.f, 1.f, 0.f);
		const NkBodyId zone = w.CreateBody(z, collision::NkShape::Box2D(NkVec2f(3.f, 1.f), NkVec2f(0.5f, 0.5f)));
		for (int32 i = 0; i < 60; ++i) {
			w.Step(1.f / 60.f);
		}
		NkVector<NkBodyContact> cs;
		const uint32 n = w.BodyContacts(caisse, cs);
		const bool bon = n == 1u && cs[0].other == sol && cs[0].normal.y > 0.99f;
		Temoin(bon, "(r5) caisse posee : un contact, normale (0, 1) vers elle (ny)", n > 0u ? cs[0].normal.y : -1.f);
		NkVector<NkBodyContact> cs2;
		const uint32 n2 = w.BodyContacts(sol, cs2);
		Temoin(n2 == 1u && cs2[0].other == caisse && cs2[0].normal.y < -0.99f, "(r5) vu du sol : la normale s'inverse (ny)",
			   n2 > 0u ? cs2[0].normal.y : 1.f);
		// En l'air : aucun contact.
		NkPhysicsWorld v(cfg);
		Sol(v);
		NkBodyDef e;
		e.position = NkVec3f(0.f, 5.f, 0.f);
		const NkBodyId haut = v.CreateBody(e, collision::NkShape::Circle2D(NkVec2f(0.f, 5.f), 0.3f));
		v.Step(1.f / 60.f);
		NkVector<NkBodyContact> cs3;
		Temoin(v.BodyContacts(haut, cs3) == 0u, "(r5n) en l'air : aucun contact", static_cast<float32>(cs3.Size()));

		// (r6) Un rayon vers le bas depuis la zone : il la traverse (ignoree) et
		// touche le sol ; sans l'ignorer, il touche la zone.
		NkBodyId touche = NK_INVALID_BODY;
		collision::NkRayHit2D h;
		const bool okSol = w.Raycast2D(NkVec2f(3.f, 3.f), NkVec2f(0.f, -2.f), 10.f, touche, h);
		const NkBodyId toucheSol = touche;
		const float32 tSol = h.t;
		NkBodyId toucheZone = NK_INVALID_BODY;
		collision::NkRayHit2D hz;
		const bool okZone = w.Raycast2D(NkVec2f(3.f, 3.f), NkVec2f(0.f, -1.f), 10.f, toucheZone, hz, 0xFFFFFFFFu, false);
		// Et depuis la caisse elle-meme, en s'ignorant : le sol sous elle.
		NkBodyId sous = NK_INVALID_BODY;
		collision::NkRayHit2D hs;
		const bool okSous = w.Raycast2D(NkVec2f(0.f, 0.3f), NkVec2f(0.f, -1.f), 2.f, sous, hs, 0xFFFFFFFFu, true, caisse);
		Temoin(okSol && toucheSol == sol && Absf(tSol - 3.f) < 1.0e-3f && okZone && toucheZone == zone && okSous && sous == sol,
			   "(r6) Raycast2D physique : corps rendu, zones et soi ignores (t sol)", tSol);
	}
} // namespace

int RunRigides2DTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	const int avant = fail;
	std::fprintf(stderr, "=== RIGIDES 2D : plan, polygones, rayons, joints ===\n");
	TemoinPlan();
	TemoinPolygone();
	TemoinRayons();
	TemoinJoints();
	TemoinRequetes();
	std::fprintf(stderr, "=== rigides 2D : %d rouge(s) sur ce bloc ===\n", fail - avant);
	return fail - avant;
}
