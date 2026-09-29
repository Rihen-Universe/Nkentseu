// AUTEUR : Rihen
// =============================================================================
// test_particules2d.cpp — les temoins de NkParticules2D (2026-09-29).
//
// ⚠️ POURQUOI ICI : comme test_eau_couplage.cpp, `NKPhysics_Tests` ramasse
// `tests/**.cpp` et est deja dans la liste `allow` : aucune cible neuve.
//
// PRE-ENREGISTREMENT — ecrit AVANT d'avoir lu un chiffre. Chaque temoin qui
// peut etre vert par accident a son NEGATIF, qui doit rougir s'il ne mesure
// rien :
//   (q1) chaque preset seul, 10 s : positions finies, dans la boite, au repos
//   (q2) eau : densite INTERIEURE entre 0,85 et 1,45 rho0 (ni gaz, ni ecrasee)
//   (q3) ballon : aire / aire de repos entre 0,8 et 1,15 ;
//        (q3n) COUPE, il se degonfle sous 0,6
//   (q4) blob : il COULE — sa hauteur perd plus de 12 % entre 0,5 s et 10 s ;
//        (q4n) plasticite nulle : il en perd moins de 8 % (elastique)
//   (q5) cristal a T = 100 : plus de 20 liaisons rompues ;
//        (q5n) a T = 0 : AUCUNE
//   (q6) couplage, apesanteur : un blob lance sur une caisse rigide libre la
//        met en mouvement ET la quantite de mouvement totale est conservee a
//        10 % pres ; (q6n) couplage coupe : la caisse ne bouge pas
//   (q7) eau versee sur une boite STATIQUE inclinee : a AUCUNE image une
//        particule n'a son centre dans la boite ; (q7n) sans le monde rigide, oui
//   (q8) boite 2D inclinee de 0,6 rad lachee sur un sol : elle bascule a plat
//        (correctif NkTransformShape du 2026-09-29, NkRigidBody.h)
// =============================================================================
#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkParticules2DFabrique.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKMath/NkFunctions.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::physics;

namespace {
	int *gPass = nullptr;
	int *gFail = nullptr;

	void Temoin(bool ok, const char *quoi, float32 valeur) {
		std::fprintf(stderr, "  [%s] %-70s %9.3f\n", ok ? "ok" : "FAIL", quoi, static_cast<double>(valeur));
		if (ok) {
			++*gPass;
		} else {
			++*gFail;
		}
	}

	void Avancer(NkParticules2D &p, float32 secondes, NkPhysicsWorld *w = nullptr) {
		const int32 n = static_cast<int32>(secondes * 60.f + 0.5f);
		for (int32 k = 0; k < n; ++k) {
			p.Pas(1.f / 60.f, w);
			if (w != nullptr) {
				w->Step(1.f / 60.f);
			}
		}
	}

	float32 Absf(float32 v) {
		return v < 0.f ? -v : v;
	}

	float32 AngleZ(const math::NkQuatf &q) {
		return 2.f * math::NkAtan2(q.z, q.w);
	}

	void TemoinPresets() {
		struct Cas {
				const char *nom;
				int32 (*creer)(NkParticules2D &);
		};
		const Cas cas[] = {
			{"(q1) ballon", [](NkParticules2D &p) { return NkCreerBallonP2D(p, NkVec2f(0.f, 2.f)); }},
			{"(q1) blob", [](NkParticules2D &p) { return NkCreerBlobP2D(p, NkVec2f(0.f, 1.f), 0.42f, NkPresetP2D::NK_BLOB); }},
			{"(q1) gelee", [](NkParticules2D &p) { return NkCreerGeleeP2D(p, NkVec2f(0.f, 2.f)); }},
			{"(q1) eau", [](NkParticules2D &p) { return NkCreerFluideP2D(p, NkVec2f(0.f, 1.f), 0.6f, NkPresetP2D::NK_EAU); }},
			{"(q1) miel", [](NkParticules2D &p) { return NkCreerFluideP2D(p, NkVec2f(0.f, 1.f), 0.5f, NkPresetP2D::NK_MIEL); }},
			{"(q1) sable", [](NkParticules2D &p) { return NkCreerFluideP2D(p, NkVec2f(0.f, 1.f), 0.5f, NkPresetP2D::NK_SABLE); }},
			{"(q1) cristal", [](NkParticules2D &p) { return NkCreerCristalP2D(p, NkVec2f(0.f, 0.6f), false); }},
			{"(q1) tissu", [](NkParticules2D &p) { return NkCreerTissuP2D(p, NkVec2f(0.f, 10.f)); }},
			{"(q1) corde", [](NkParticules2D &p) { return NkCreerCordeP2D(p, NkVec2f(0.f, 10.f)); }},
			{"(q1) pont", [](NkParticules2D &p) { return NkCreerPontP2D(p, NkVec2f(-2.f, 3.f), NkVec2f(2.f, 3.f)); }},
		};
		for (const Cas &c : cas) {
			NkParticules2D p;
			const int32 ci = c.creer(p);
			const uint32 n0 = static_cast<uint32>(p.particules.Size());
			Avancer(p, 10.f);
			bool fini = ci >= 0 && n0 > 0;
			bool dedans = true;
			float32 vmax = 0.f;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				const NkParticule2D &q = p.particules[i];
				fini = fini && q.pos.x == q.pos.x && q.pos.y == q.pos.y;
				dedans = dedans && q.pos.y > -0.01f && q.pos.y < 14.01f;
				vmax = vmax > q.vit.Len() ? vmax : q.vit.Len();
			}
			char quoi[96];
			std::snprintf(quoi, sizeof(quoi), "%s : fini, dans la boite, au repos (vmax m/s)", c.nom);
			Temoin(fini && dedans && vmax < 4.f && p.particules.Size() == n0, quoi, vmax);
		}
	}

	void TemoinEau() {
		NkParticules2D p;
		const int32 ci = NkCreerFluideP2D(p, NkVec2f(0.f, 0.8f), 0.8f, NkPresetP2D::NK_EAU);
		p.reglages.limites.gauche = -1.5f; // un bac etroit : de la profondeur
		p.reglages.limites.droite = 1.5f;
		Avancer(p, 8.f);
		const NkCorpsP2D &c = p.corps[static_cast<uint32>(ci)];
		const float32 rho0 = p.ParamsFluide(NkMateriauP2D::NK_EAU).rho0;
		float32 somme = 0.f;
		uint32 n = 0;
		for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
			if (p.particules[i].densite > 0.8f * rho0) {
				somme += p.particules[i].densite;
				++n;
			}
		}
		const float32 r = n > 0 ? somme / static_cast<float32>(n) / rho0 : 0.f;
		Temoin(r > 0.85f && r < 1.45f && n > c.nombre / 3u, "(q2) eau : densite interieure / rho0", r);
	}

	float32 AireBallon(const NkParticules2D &p, uint32 ci) {
		const NkCorpsP2D &c = p.corps[ci];
		float32 a = 0.f;
		for (uint32 k = 0; k < c.nombre; ++k) {
			const NkVec2f &p0 = p.particules[c.debut + k].pos;
			const NkVec2f &p1 = p.particules[c.debut + (k + 1) % c.nombre].pos;
			a += p0.x * p1.y - p0.y * p1.x;
		}
		return 0.5f * a / c.aireRepos;
	}

	void TemoinBallon() {
		{
			NkParticules2D p;
			const int32 ci = NkCreerBallonP2D(p, NkVec2f(0.f, 2.f));
			Avancer(p, 6.f);
			const float32 r = AireBallon(p, static_cast<uint32>(ci));
			Temoin(r > 0.8f && r < 1.15f, "(q3) ballon pose : aire / aire de repos", r);
		}
		{
			NkParticules2D p;
			const int32 ci = NkCreerBallonP2D(p, NkVec2f(0.f, 2.f));
			p.Couper(NkVec2f(-1.f, 2.f), NkVec2f(1.f, 2.05f));
			Avancer(p, 6.f);
			const float32 r = AireBallon(p, static_cast<uint32>(ci));
			Temoin(r < 0.6f && p.corps[static_cast<uint32>(ci)].pression == 0.f, "(q3n) ballon coupe : il se degonfle", r);
		}
	}

	float32 HauteurBlob(float32 plasticite, float32 &h05) {
		NkParticules2D p;
		const int32 ci = NkCreerBlobP2D(p, NkVec2f(0.f, 0.6f), 0.42f, NkPresetP2D::NK_BLOB);
		p.corps[static_cast<uint32>(ci)].plasticite = plasticite;
		NkVec2f mn, mx;
		Avancer(p, 0.5f);
		p.BoiteCorps(static_cast<uint32>(ci), mn, mx);
		h05 = mx.y - mn.y;
		Avancer(p, 9.5f);
		p.BoiteCorps(static_cast<uint32>(ci), mn, mx);
		return mx.y - mn.y;
	}

	void TemoinBlob() {
		float32 h0 = 0.f;
		const float32 h1 = HauteurBlob(7.f, h0);
		const float32 perte = 1.f - h1 / h0;
		Temoin(perte > 0.12f, "(q4) blob : il coule (perte de hauteur 0,5 s -> 10 s)", perte);
		const float32 h1e = HauteurBlob(0.f, h0);
		const float32 perteE = 1.f - h1e / h0;
		Temoin(perteE < 0.08f, "(q4n) blob sans plasticite : il reste (perte de hauteur)", perteE);
	}

	void TemoinChaleur() {
		for (int32 k = 0; k < 2; ++k) {
			NkParticules2D p;
			NkCreerCristalP2D(p, NkVec2f(0.f, 0.6f), false);
			Avancer(p, 2.f);
			const uint32 avant = p.rupturesTotal;
			p.reglages.temperature = k == 0 ? 100.f : 0.f;
			Avancer(p, 10.f);
			const float32 n = static_cast<float32>(p.rupturesTotal - avant);
			if (k == 0) {
				Temoin(n > 20.f, "(q5) cristal a T = 100 : liaisons rompues", n);
			} else {
				Temoin(n == 0.f, "(q5n) cristal a T = 0 : aucune rupture", n);
			}
		}
	}

	void TemoinCouplage() {
		for (int32 k = 0; k < 2; ++k) {
			const bool couple = k == 0;
			NkPhysicsConfig cfg;
			cfg.gravity = NkVec3f(0.f, 0.f, 0.f);
			NkPhysicsWorld w(cfg);
			NkBodyDef def;
			def.position = NkVec3f(1.2f, 5.f, 0.f);
			def.material.dynamicFriction = 0.f;
			def.material.staticFriction = 0.f;
			const NkBodyId id = w.CreateBody(def, collision::NkShape::Box2D(NkVec2f(1.2f, 5.f), NkVec2f(0.3f, 0.3f)));
			NkRigidBody *b = w.GetBody(id);
			const float32 mCaisse = b->invMass > 0.f ? 1.f / b->invMass : 0.f;

			NkParticules2D p;
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			p.reglages.limites.actif = false;
			p.reglages.amortAir = 0.f;
			const int32 ci = NkCreerBlobP2D(p, NkVec2f(0.f, 5.f), 0.3f, NkPresetP2D::NK_BLOB);
			p.corps[static_cast<uint32>(ci)].couplageRigide = couple;
			p.corps[static_cast<uint32>(ci)].friction = 0.f;
			p.AppliquerVitesse(static_cast<uint32>(ci), NkVec2f(2.f, 0.f));
			float32 mBlob = 0.f;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				mBlob += p.particules[i].masse;
			}
			const float32 qAvant = mBlob * 2.f;
			Avancer(p, 1.5f, &w);
			b = w.GetBody(id);
			const float32 vCaisse = b->linearVelocity.x;
			float32 qBlob = 0.f;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				qBlob += p.particules[i].masse * p.particules[i].vit.x;
			}
			const float32 qApres = qBlob + mCaisse * vCaisse;
			if (couple) {
				const float32 ecart = Absf(qApres - qAvant) / qAvant;
				Temoin(vCaisse > 0.1f, "(q6) couplage : la caisse est mise en mouvement (m/s)", vCaisse);
				Temoin(ecart < 0.10f, "(q6) couplage : quantite de mouvement conservee (ecart relatif)", ecart);
			} else {
				Temoin(Absf(vCaisse) < 1.0e-4f, "(q6n) couplage coupe : la caisse ne bouge pas (m/s)", vCaisse);
			}
		}
	}

	void TemoinStatique() {
		for (int32 k = 0; k < 2; ++k) {
			const bool avecMonde = k == 0;
			NkPhysicsWorld w;
			NkBodyDef def;
			def.type = NkBodyType::STATIC;
			def.position = NkVec3f(0.f, 1.2f, 0.f);
			const float32 a0 = 0.4f;
			def.orientation.x = 0.f;
			def.orientation.y = 0.f;
			def.orientation.z = math::NkSin(a0 * 0.5f);
			def.orientation.w = math::NkCos(a0 * 0.5f);
			w.CreateBody(def, collision::NkShape::Box2D(NkVec2f(0.f, 1.2f), NkVec2f(1.0f, 0.2f)));

			NkParticules2D p;
			NkCreerFluideP2D(p, NkVec2f(0.f, 2.6f), 0.5f, NkPresetP2D::NK_EAU);
			// On compte a CHAQUE image (le maximum) : a la fin, l'eau a glisse au
			// sol dans les deux cas, et un compte final ne mesurerait rien.
			uint32 dedans = 0;
			const float32 c = math::NkCos(a0), s = math::NkSin(a0);
			for (int32 image = 0; image < 240; ++image) {
				p.Pas(1.f / 60.f, avecMonde ? &w : nullptr);
				uint32 n = 0;
				for (uint32 i = 0; i < p.particules.Size(); ++i) {
					const NkVec2f d = p.particules[i].pos - NkVec2f(0.f, 1.2f);
					const float32 lx = d.x * c + d.y * s;
					const float32 ly = -d.x * s + d.y * c;
					n += (Absf(lx) < 1.0f && Absf(ly) < 0.2f) ? 1u : 0u;
				}
				dedans = n > dedans ? n : dedans;
			}
			if (avecMonde) {
				Temoin(dedans == 0, "(q7) eau sur boite statique inclinee : max. de particules dedans", static_cast<float32>(dedans));
			} else {
				Temoin(dedans > 0, "(q7n) sans monde rigide : l'eau traverse (particules dedans)", static_cast<float32>(dedans));
			}
		}
	}

	void TemoinBoite2D() {
		NkPhysicsWorld w;
		NkBodyDef sol;
		sol.type = NkBodyType::STATIC;
		sol.position = NkVec3f(0.f, -0.5f, 0.f);
		w.CreateBody(sol, collision::NkShape::Box2D(NkVec2f(0.f, -0.5f), NkVec2f(10.f, 0.5f)));
		NkBodyDef b;
		b.position = NkVec3f(0.f, 2.f, 0.f);
		b.orientation.x = 0.f;
		b.orientation.y = 0.f;
		b.orientation.z = math::NkSin(0.3f);
		b.orientation.w = math::NkCos(0.3f);
		const NkBodyId id = w.CreateBody(b, collision::NkShape::Box2D(NkVec2f(0.f, 2.f), NkVec2f(0.3f, 0.3f)));
		for (int32 k = 0; k < 600; ++k) {
			w.Step(1.f / 60.f);
		}
		const NkRigidBody *r = w.GetBody(id);
		float32 a = AngleZ(r->orientation);
		// A plat = un multiple de 90 degres.
		const float32 quart = 1.5707963f;
		while (a > quart * 0.5f) {
			a -= quart;
		}
		while (a < -quart * 0.5f) {
			a += quart;
		}
		Temoin(Absf(a) < 0.05f && Absf(r->position.y - 0.3f) < 0.03f, "(q8) boite 2D inclinee : bascule a plat (angle residuel rad)", a);
	}
} // namespace

int RunParticules2DTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	const int avant = fail;
	std::fprintf(stderr, "=== PARTICULES 2D (NkParticules2D) : corps mous, fluides, couplage rigide ===\n");
	TemoinPresets();
	TemoinEau();
	TemoinBallon();
	TemoinBlob();
	TemoinChaleur();
	TemoinCouplage();
	TemoinStatique();
	TemoinBoite2D();
	std::fprintf(stderr, "=== particules 2D : %d rouge(s) sur ce bloc ===\n", fail - avant);
	return fail - avant;
}
