// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_cloth.cpp — les TÉMOINS du tissu XPBD (ROADMAP_PRODUITS.md §6.4, lot du
// 2026-09-05), écrits AVANT l'image, scènes fixées une fois. Appelé depuis le
// main() de test_physics.cpp (la suite fournit son propre main).
//
// Les chiffres sont imprimés sur stderr (comme les sondes de la démo) : le
// puits console de NKLogger n'a rien montré à travers un tuyau dans cet arbre
// (mesuré : `NKPhysics_Tests.exe 2>&1 | tail` = vide, sortie 0) ; un témoin
// dont on ne peut pas lire le chiffre n'est pas un témoin.
//
// Scènes (toutes : nappe 1 m x 1 m, 32 x 32 particules, 0,2 kg, dt = 1/60 s) :
//   (a) drapé au repos : épinglée par deux coins hauts, 5 s -> étirement max des
//       arêtes structurelles < 1 % (rouge au-delà), vitesse résiduelle < 0,01 m/s,
//       énergie (cinétique + potentielle) décroissante d'une seconde à l'autre ;
//   (b) pas de traversée : lâchée à plat sur une sphère R = 0,3 m -> pénétration
//       max des centres < 1 mm, le drapé épouse la sphère (>= 10 % des particules
//       à moins de 5 mm de la surface, la particule centrale posée dessus) ;
//   (c) auto-collision : nappe pliée en deux sur un plan -> les deux pans restent
//       séparés d'au moins 0,8 x (2 x épaisseur), distance minimale entre
//       particules non voisines >= 0,9 x (2 x épaisseur) ; contre-épreuve :
//       auto-collision coupée -> les pans se confondent (doit rougir) ;
//   (d) raideur indépendante du pas (LE témoin XPBD) : même compliance, dt et
//       dt/2 -> même élongation moyenne à 5 % près ; contre-épreuve `xpbd = false`
//       (PBD pur, compliance ignorée) -> l'élongation dépend du pas (doit rougir) ;
//   (e) vent uniforme : rangée haute épinglée, force horizontale F par particule
//       -> angle d'équilibre atan(F / m g) à 10 % près (pendule de particules :
//       toutes les forces sont parallèles, le drapé est un plan incliné) ;
//   (f) conservation : nombre de particules et masse totale constants ;
//   (g) les corps d'un monde deviennent des colliders (AddCollidersFromWorld) :
//       une capsule cinématique porte la nappe sans traversée.
// =============================================================================
#include "NKPhysics/NKPhysics.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::physics;

namespace {

	int *gPass = nullptr, *gFail = nullptr;
#define CCHECK(cond, msg)                                                                                              \
	do {                                                                                                               \
		if (cond) {                                                                                                    \
			++*gPass;                                                                                                  \
			std::fprintf(stderr, "  [OK]   %s\n", msg);                                                                \
		} else {                                                                                                       \
			++*gFail;                                                                                                  \
			std::fprintf(stderr, "  [FAIL] %s\n", msg);                                                                \
		}                                                                                                              \
	} while (0)

	constexpr uint32 kN = 32;
	constexpr float32 kSize = 1.f;
	constexpr float32 kMass = 0.2f;
	constexpr float32 kDt = 1.f / 60.f;
	constexpr float32 kSpacing = kSize / (float32)(kN - 1);

	float32 Abs(float32 x) {
		return x < 0.f ? -x : x;
	}

	// INSTRUMENTS (pas la scène) : NK_TISSU_SUB / NK_TISSU_IT forcent sous-pas et itérations
	// de toutes les scènes ; défaut = ce que le lot fixe (dit sur chaque ligne).
	uint32 EnvU(const char *name, uint32 def) {
		const char *e = std::getenv(name);
		return (e && e[0]) ? (uint32)std::atoi(e) : def;
	}
	void Solveur(NkCloth &c, uint32 sub, uint32 it) {
		c.params.substeps = EnvU("NK_TISSU_SUB", sub);
		c.params.iterations = EnvU("NK_TISSU_IT", it);
		// NK_TISSU_BEND : compliance de flexion (m/N) de toutes les scènes -- instrument du lot budget
		if (const char *b = std::getenv("NK_TISSU_BEND"); b && b[0])
			c.params.bendCompliance = (float32)std::atof(b);
	}

	// Nappe VERTICALE dans le plan XY (x de -0,5 à 0,5, y de 0 à 1, z = 0), rangée haute j = kN-1.
	void BuildVertical(NkCloth &c) {
		c.BuildGrid(kN, kN, {-0.5f, 0.f, 0.f}, {kSpacing, 0.f, 0.f}, {0.f, kSpacing, 0.f}, kMass);
	}

	// Nappe HORIZONTALE (plan XZ) à la hauteur y0, centrée sur l'origine.
	void BuildHorizontal(NkCloth &c, float32 y0) {
		c.BuildGrid(kN, kN, {-0.5f, y0, -0.5f}, {kSpacing, 0.f, 0.f}, {0.f, 0.f, kSpacing}, kMass);
	}

	void Run(NkCloth &c, float32 dt, float32 seconds, float32 t0 = 0.f) {
		const uint32 frames = (uint32)(seconds / dt + 0.5f);
		float32 t = t0;
		for (uint32 f = 0; f < frames; ++f) {
			c.Step(dt, t);
			t += dt;
		}
	}

	// ── (a) + (f) : drapé au repos ──────────────────────────────────────────
	void TemoinRepos() {
		std::fprintf(stderr, "[TISSU (a)] drape au repos : 32x32, deux coins hauts epingles, 5 s\n");
		NkCloth c;
		BuildVertical(c);
		c.params.compliance = 0.f;
		c.params.shearCompliance = 0.f;
		c.params.damping = 2.f;
		// SCÈNE SINGULIÈRE, dit : les deux coins sont épinglés à exactement la largeur de la nappe, la
		// rangée haute est une corde tendue à sa longueur (tension infinie sans sag). Elle garde 32 x 2
		// (table de NkCloth.h : 16 x 2 -> 3,4 %, 32 x 2 -> 0,88 %) ; le défaut du solveur est 16 x 2.
		Solveur(c, 32, 2);
		c.Pin(c.GridIndex(0, kN - 1));
		c.Pin(c.GridIndex(kN - 1, kN - 1));
		float32 E[6] = {0, 0, 0, 0, 0, 0};
		bool decroit = true;
		for (uint32 s = 0; s < 5; ++s) {
			Run(c, kDt, 1.f, (float32)s);
			const NkClothStats &st = c.Stats();
			E[s + 1] = st.kineticEnergy + st.potentialEnergy;
			std::fprintf(stderr, "  t=%us : etirement max %.3f %% moyen %.3f %% | vmax %.4f m/s | E = %.6f J (Ec %.6f)\n",
						 s + 1, 100.f * st.maxStretch, 100.f * st.meanStretch, st.maxSpeed, E[s + 1], st.kineticEnergy);
			// tolérance RELATIVE 1e-4 : une nappe posée a une énergie constante au bruit du float32
			// (mesuré : +-1e-5 J sur 0,98 J entre deux secondes, sans aucun mouvement)
			if (s >= 1 && E[s + 1] > E[s] * (1.f + 1e-4f))
				decroit = false;
		}
		const NkClothStats &st = c.Stats();
		CCHECK(st.maxStretch < 0.01f, "(a) etirement max des aretes < 1 % apres 5 s");
		CCHECK(st.maxSpeed < 0.01f, "(a) vitesse residuelle < 0,01 m/s apres 5 s");
		CCHECK(decroit, "(a) energie decroissante d'une seconde a l'autre (t >= 1 s)");
		CCHECK(st.particles == kN * kN, "(f) nombre de particules constant (1024)");
		CCHECK(Abs(st.mass - kMass) < 1e-5f, "(f) masse totale constante (0,2 kg)");
		CCHECK(st.pinned == 2 && st.structural == 2 * kN * (kN - 1), "(a) 2 epinglees, 1984 aretes structurelles");
		std::fprintf(stderr, "  sous-pas %u x iterations %u ; contraintes %u (struct %u, cisaillement %u, flexion %u)\n",
					 st.substeps, st.iterations, st.constraints, st.structural, st.shear, st.bend);
	}

	// ── (b) : pas de traversée d'une sphère ────────────────────────────────
	void TemoinSphere() {
		std::fprintf(stderr, "[TISSU (b)] nappe lachee sur une sphere R = 0,3 m, 5 s\n");
		NkCloth c;
		BuildHorizontal(c, 0.6f);
		c.params.thickness = 0.005f;
		c.params.damping = 1.f;
		c.params.friction = 0.3f;
		Solveur(c, 16, 2);
		const float32 R = 0.3f;
		c.colliders.PushBack(collision::NkShape::Sphere({0.f, 0.f, 0.f}, R));
		for (uint32 s = 0; s < 5; ++s) {
			Run(c, kDt, 1.f, (float32)s);
			const NkVec3f cp = c.Positions()[c.GridIndex(kN / 2, kN / 2)];
			std::fprintf(stderr, "  t=%us : centre (%.3f, %.3f, %.3f) | vmax %.3f | contacts %u | penetration %.3f mm | etirement max %.2f %%\n",
						 s + 1, cp.x, cp.y, cp.z, c.Stats().maxSpeed, c.Stats().contacts, 1000.f * c.Stats().maxPenetration,
						 100.f * c.Stats().maxStretch);
		}
		const NkClothStats &st = c.Stats();
		// combien de particules sont posées sur la sphère (à moins de 5 mm de la surface, épaisseur comprise)
		uint32 posees = 0;
		const NkVec3f *X = c.Positions();
		for (uint32 i = 0; i < c.ParticleCount(); ++i) {
			const float32 d = X[i].Len() - R - c.params.thickness;
			if (Abs(d) < 0.005f)
				++posees;
		}
		const NkVec3f centre = X[c.GridIndex(kN / 2, kN / 2)];
		std::fprintf(stderr, "  penetration max %.4f mm | posees %u / %u (%.1f %%) | centre y = %.4f (attendu %.4f) | vmax %.4f\n",
					 1000.f * st.maxPenetration, posees, st.particles, 100.f * posees / (float32)st.particles, centre.y,
					 R + c.params.thickness, st.maxSpeed);
		CCHECK(st.maxPenetration < 0.001f, "(b) penetration max des centres < 1 mm");
		CCHECK(posees * 10 >= st.particles, "(b) le drape epouse la sphere : >= 10 % des particules a la surface");
		CCHECK(Abs(centre.y - (R + c.params.thickness)) < 0.005f, "(b) la particule centrale est posee sur le sommet");
		CCHECK(st.particles == kN * kN && Abs(st.mass - kMass) < 1e-5f, "(f) conservation (sphere)");
	}

	// ── (c) : auto-collision d'un pli ───────────────────────────────────────
	// Renvoie l'écart moyen entre les deux pans et la distance minimale non voisine.
	void Pli(bool self, float32 &ecart, float32 &dmin, uint32 &contacts) {
		NkCloth c;
		const float32 r = 0.4f * kSpacing;
		c.BuildGrid(kN, kN, {0.f, 0.3f, -0.5f}, {kSpacing, 0.f, 0.f}, {0.f, 0.f, kSpacing}, kMass);
		c.params.thickness = r;
		c.params.selfCollision = self;
		c.params.damping = 2.f;
		c.params.friction = 0.5f;
		Solveur(c, 16, 2);
		c.colliders.PushBack(collision::NkShape::Plane3D({0.f, 0.f, 0.f}, {0.f, 1.f, 0.f}));
		// pli : la moitié i >= 16 est rabattue par-dessus la moitié i < 16 (longueurs de repos intactes).
		// L'écart initial vaut au moins un espacement : l'arête de charnière n'est pas comprimée
		// (mesuré à 2r + 2 mm : elle poussait le pan du dessus hors du pan du dessous).
		const float32 gap = (2.f * r + 0.002f > kSpacing) ? 2.f * r + 0.002f : kSpacing;
		for (uint32 j = 0; j < kN; ++j)
			for (uint32 i = kN / 2; i < kN; ++i) {
				const NkVec3f p = c.Positions()[c.GridIndex(i, j)];
				c.SetPosition(c.GridIndex(i, j), {1.f - p.x, p.y + gap, p.z});
			}
		const NkVec3f *X = c.Positions();
		float32 yBas = 0.f, yHaut = 0.f;
		for (uint32 s = 0; s < 5; ++s) {
			Run(c, kDt, 1.f, (float32)s);
			yBas = yHaut = 0.f;
			for (uint32 j = 0; j < kN; ++j)
				for (uint32 i = 0; i < kN; ++i)
					(i < kN / 2 ? yBas : yHaut) += X[c.GridIndex(i, j)].y;
			std::fprintf(stderr, "  t=%us : y moyen pan bas %.1f mm, pan haut %.1f mm | dmin %.2f mm | contacts %u | vmax %.3f | x moyen pan haut %.3f\n",
						 s + 1, 1000.f * yBas / (float32)(kN * kN / 2), 1000.f * yHaut / (float32)(kN * kN / 2),
						 1000.f * c.Stats().minSelfDistance, c.Stats().selfContacts, c.Stats().maxSpeed,
						 [&] { float32 xm = 0.f; for (uint32 j = 0; j < kN; ++j) for (uint32 i = kN / 2; i < kN; ++i) xm += X[c.GridIndex(i, j)].x; return xm / (float32)(kN * kN / 2); }());
		}
		yBas = yHaut = 0.f;
		for (uint32 j = 0; j < kN; ++j)
			for (uint32 i = 0; i < kN; ++i)
				(i < kN / 2 ? yBas : yHaut) += X[c.GridIndex(i, j)].y;
		yBas /= (float32)(kN * kN / 2);
		yHaut /= (float32)(kN * kN / 2);
		ecart = yHaut - yBas;
		dmin = c.Stats().minSelfDistance;
		contacts = c.Stats().selfContacts;
	}

	void TemoinPli() {
		std::fprintf(stderr, "[TISSU (c)] nappe pliee en deux sur un plan, 5 s (epaisseur 2r = %.1f mm)\n", 1000.f * 0.8f * kSpacing);
		const float32 r2 = 0.8f * kSpacing;
		float32 ecart = 0.f, dmin = 0.f;
		uint32 contacts = 0;
		Pli(true, ecart, dmin, contacts);
		std::fprintf(stderr, "  AVEC auto-collision : ecart moyen des pans %.2f mm | dmin non voisines %.2f mm | contacts %u\n",
					 1000.f * ecart, 1000.f * dmin, contacts);
		CCHECK(ecart >= 0.8f * r2, "(c) les deux pans restent separes (ecart >= 0,8 x 2r)");
		CCHECK(dmin >= 0.9f * r2, "(c) aucune paire non voisine a moins de 0,9 x 2r");
		float32 ecart0 = 0.f, dmin0 = 0.f;
		uint32 contacts0 = 0;
		Pli(false, ecart0, dmin0, contacts0);
		std::fprintf(stderr, "  SANS auto-collision (contre-epreuve) : ecart %.2f mm | dmin %.2f mm\n", 1000.f * ecart0, 1000.f * dmin0);
		CCHECK(ecart0 < 0.8f * r2, "(c) contre-epreuve : sans auto-collision les pans se confondent (le temoin sait rougir)");
	}

	// ── (d) : raideur indépendante du pas ───────────────────────────────────
	constexpr float32 kComplianceD = 0.05f; // m/N
	float32 Elongation(bool xpbd, float32 dt) {
		NkCloth c;
		BuildVertical(c);
		// 0,05 m/N : alpha~ = alpha / h² = 2 880 à h = 1/240 s, comparable à 1/m = 5 120 kg^-1 --
		// à 0,002 (alpha~/w = 1 %) le témoin ne mesurait que la non-convergence (mesuré : XPBD et
		// PBD pur donnaient les MÊMES chiffres, 0,45 % / 0,15 %).
		c.params.compliance = kComplianceD;
		c.params.shearCompliance = kComplianceD;
		c.params.damping = 2.f;
		Solveur(c, 16, 2); // le budget (lot du 05/09) : 0,64 % d'écart ici, 9,6 % avec la flexion à 0,01
		c.params.xpbd = xpbd;
		for (uint32 i = 0; i < kN; ++i)
			c.Pin(c.GridIndex(i, kN - 1)); // chaque colonne : un pendule de 31 maillons, tension k m g au maillon k
		Run(c, dt, 5.f);
		return c.Stats().meanStretch;
	}

	void TemoinRaideur() {
		std::fprintf(stderr, "[TISSU (d)] raideur independante du pas : rangee haute epinglee, compliance %.3f m/N, dt = 1/60 et 1/120, 5 s\n", kComplianceD);
		// Borne analytique (dite, pas assertée) : si SEULES les arêtes structurelles portaient la charge,
		// C = -alpha F avec F = k m g au maillon k -> moyenne structurelle (1/2) x alpha x 16 m g / s. Mesuré
		// convergé avec la flexion à 0,01 m/N : 0,128 %, 18x moins (la flexion quasi rigide portait la charge) ;
		// à 1 m/N : 1,7 % -- les diagonales de cisaillement portent le reste. Le témoin est l'indépendance au pas.
		const float32 mP = kMass / (float32)(kN * kN);
		std::fprintf(stderr, "  borne haute analytique (structurelles seules) : moyenne %.3f %% ; les chemins paralleles (flexion, cisaillement) la reduisent\n",
					 50.f * kComplianceD * 16.f * mP * 9.81f / kSpacing);
		const float32 e1 = Elongation(true, kDt), e2 = Elongation(true, kDt * 0.5f);
		const float32 emax = e1 > e2 ? e1 : e2;
		const float32 ecart = emax > 0.f ? Abs(e1 - e2) / emax : 0.f;
		std::fprintf(stderr, "  XPBD : elongation moyenne %.4f %% (dt) et %.4f %% (dt/2) -> ecart %.2f %%\n", 100.f * e1, 100.f * e2, 100.f * ecart);
		CCHECK(e1 > 0.001f, "(d) l'elongation est mesurable (> 0,1 %) : le temoin peut departager");
		CCHECK(ecart <= 0.05f, "(d) XPBD : meme elongation a 5 % pres entre dt et dt/2");
		const float32 p1 = Elongation(false, kDt), p2 = Elongation(false, kDt * 0.5f);
		const float32 pmax = p1 > p2 ? p1 : p2;
		const float32 pecart = pmax > 0.f ? Abs(p1 - p2) / pmax : 0.f;
		std::fprintf(stderr, "  PBD pur (contre-epreuve, compliance ignoree) : %.4f %% et %.4f %% -> ecart %.2f %%\n", 100.f * p1, 100.f * p2, 100.f * pecart);
		CCHECK(pecart > 0.05f, "(d) contre-epreuve : en PBD pur l'elongation depend du pas (le temoin sait rougir)");
	}

	// ── (e) : vent uniforme ─────────────────────────────────────────────────
	void TemoinVent() {
		std::fprintf(stderr, "[TISSU (e)] vent uniforme : rangee haute epinglee, F = m g tan(30 deg) par particule, 10 s\n");
		NkCloth c;
		BuildVertical(c);
		c.params.damping = 2.f;
		Solveur(c, 16, 2);
		for (uint32 i = 0; i < kN; ++i)
			c.Pin(c.GridIndex(i, kN - 1));
		const float32 m = kMass / (float32)(kN * kN);
		const float32 g = 9.81f;
		const float32 tan30 = 0.57735027f;
		// la nappe est dans le plan XY : le vent souffle selon z (sa normale). Un vent dans le plan
		// de la nappe ne ferait que la cisailler (mesuré : 1,17 deg avec F selon x).
		math::NkUniformForceField vent({0.f, 0.f, m * g * tan30});
		c.forceField = &vent;
		c.params.forceOnNormal = false;
		Run(c, kDt, 10.f); // 10 s : avec la flexion à 1 m/N la nappe flotte encore à 8 s (mesuré 0,032 m/s)
		const NkVec3f *X = c.Positions();
		const NkVec3f haut = X[c.GridIndex(kN / 2, kN - 1)], bas = X[c.GridIndex(kN / 2, 0)];
		const NkVec3f d = bas - haut;
		const float32 angle = math::NkAtan2(d.z, -d.y) * 57.2957795f;
		std::fprintf(stderr, "  haut (%.3f, %.3f, %.3f) bas (%.3f, %.3f, %.3f)\n", haut.x, haut.y, haut.z, bas.x, bas.y, bas.z);
		std::fprintf(stderr, "  angle mesure %.2f deg (attendu 30, tolerance 10 %% = 3 deg) | vmax %.4f | etirement max %.3f %%\n",
					 angle, c.Stats().maxSpeed, 100.f * c.Stats().maxStretch);
		CCHECK(Abs(angle - 30.f) <= 3.f, "(e) angle d'equilibre atan(F / m g) a 10 % pres");
		CCHECK(c.Stats().maxSpeed < 0.05f, "(e) la nappe est a l'equilibre (vmax < 0,05 m/s)");
	}

	// ── (g) : les corps du monde deviennent des colliders ───────────────────
	void TemoinMonde() {
		std::fprintf(stderr, "[TISSU (g)] AddCollidersFromWorld : une capsule cinematique porte la nappe\n");
		NkPhysicsWorld world(NkPhysicsConfig{{0.f, -9.81f, 0.f}});
		NkBodyDef bd;
		bd.type = NkBodyType::KINEMATIC;
		bd.position = {0.f, 0.f, 0.f};
		bd.layer = 0x2u;
		world.CreateBody(bd, collision::NkShape::Capsule3D({-0.3f, 0.f, 0.f}, {0.3f, 0.f, 0.f}, 0.15f));
		NkBodyDef sol;
		sol.type = NkBodyType::STATIC;
		sol.position = {0.f, -2.f, 0.f};
		world.CreateBody(sol, collision::NkShape::Box3D({0.f, -2.f, 0.f}, {5.f, 0.5f, 5.f}));
		NkCloth c;
		BuildHorizontal(c, 0.5f);
		c.params.thickness = 0.005f;
		c.params.friction = 0.3f;
		c.AddCollidersFromWorld(world, 0x2u); // seulement la capsule
		CCHECK(c.colliders.Size() == 1 && c.colliders[0].type == collision::NkShapeType::NK_CAPSULE3D,
			   "(g) une seule forme prise, la capsule (filtre de couche)");
		c.AddCollidersFromWorld(world); // tout : + la boite du sol
		CCHECK(c.colliders.Size() == 3, "(g) sans filtre, les deux corps (capsule x2 + boite)");
		Run(c, kDt, 3.f);
		std::fprintf(stderr, "  penetration max %.4f mm | contacts %u | formes ignorees %u\n", 1000.f * c.Stats().maxPenetration,
					 c.Stats().contacts, c.Stats().collidersIgnored);
		CCHECK(c.Stats().maxPenetration < 0.001f, "(g) pas de traversee de la capsule du monde");
		CCHECK(c.Stats().collidersIgnored == 0, "(g) aucune forme ignoree (sphere/capsule/plan/boite alignee traitees)");
	}

} // namespace

int RunClothTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	std::fprintf(stderr, "=== TISSU XPBD (NkCloth) : temoins §6.4 ===\n");
	TemoinRepos();
	TemoinSphere();
	TemoinPli();
	TemoinRaideur();
	TemoinVent();
	TemoinMonde();
	return fail;
}
