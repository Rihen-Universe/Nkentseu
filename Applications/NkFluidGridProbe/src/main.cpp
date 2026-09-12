// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGridProbe — les témoins de la GRILLE de fumée (NkFluidGrid, Stam 1999).
//
// PRÉ-ENREGISTREMENT (écrit AVANT de lire le moindre chiffre) :
//   0. CONTRÔLES POSITIFS DES INSTRUMENTS — un instrument neuf se calibre avant
//      qu'on croie son chiffre. On soumet à chaque compteur un cas dont la
//      réponse est connue analytiquement, ET un cas dont la réponse doit être
//      ZÉRO. La POPULATION comptée est dite : nx*ny*nz cellules INTÉRIEURES,
//      jamais la couche de bord.
//   a. MASSE : boîte close, aucune source, dissipation nulle, 500 pas.
//      Critère : |masse(500) - masse(0)| / masse(0) < 1 %.
//   b. DIVERGENCE APRÈS PROJECTION : moyenne de |div|*h rapportée à |u| moyen.
//      Critère : < 0,1 % (0,001). Itérations et résidu DITS.
//   c. FLOTTABILITÉ : accélération verticale mesurée contre BOUSSINESQ /
//      Archimède a = g (T - T_amb) / T_amb (Fedkiw, Stam & Jensen 2001, eq. 8).
//      Critère c1 (loi seule, projection et advection coupées) : écart < 1 %.
//      Critère c2 (solveur complet) : le barycentre de température MONTE
//      (> 1 cellule en 1 s) ; contrôle négatif : ΔT = 0 -> il ne bouge pas.
//   d. TRANSPORT : vitesse uniforme imposée, blob de densité, N pas.
//      Critère : |déplacement mesuré - u*t| < 1 cellule.
//   e. 10 s (600 pas à 1/60) avec source continue : 0 NaN, vitesse bornée.
//   MUTATIONS : projection coupée -> (b) DOIT rougir ; advection coupée ->
//      (d) DOIT rougir. Une mutation qui ne rougit pas invalide le témoin.
//
// CE QUE CE BANC NE PROUVE PAS : rien sur le rendu (palier ②), rien sur le GPU,
// rien sur la couleur (palier ③). Il ne juge que le solveur, sur CPU.
//
// `printf` est utilisé ici comme dans `Applications/NkGpuProbe/src/main.cpp` :
// c'est la sortie d'un banc console. Le solveur lui-même, `NkFluidGrid.cpp`,
// n'a aucun `std::` ni aucune fonction C.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGrid.h"
#include "NKPlatform/NkEnv.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

static int gFailures = 0;
static int gChecks = 0;

// Le banc du rendu (src/rendu.cpp) partage le meme compteur de rouges.
void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);
void PalierRendu();
void PalierFeu();
void PalierVorticite();
void EnqueteConfinement();
void PalierBranchement();
void PalierGrilleMAC(); // (m1) et (m2) — les deux contrôles de la bascule MAC
void EnqueteBascule();	  // les deux ENQUÊTES de la bascule (NK_FLUID_MAC=3)
void PalierAdvectionFlux(); // (f1) le contrôle négatif de l'advection en flux
void EnqueteLePrix();		// (f2) LE PRIX du donor-cell (NK_FLUID_MAC=4)
void EnqueteStabilite();	// (f3) LA STABILITÉ, filet coupé (NK_FLUID_MAC=5)
void ImagesDuConfinement(float32 epsilon);
float32 EpsilonConfinement();

static void Check(bool ok, const char *nom, const char *detail) {
	++gChecks;
	if (!ok)
		++gFailures;
	printf("  [%s] %-52s %s\n", ok ? "VERT" : "ROUGE", nom, detail);
	fflush(stdout);
}

static float32 Absf(float32 v) {
	return v < 0.f ? -v : v;
}

void ProbeCheck(bool ok, const char *nom, const char *detail) {
	Check(ok, nom, detail);
}

float32 ProbeAbs(float32 v) {
	return Absf(v);
}

// =============================================================================
// 0. CONTRÔLES POSITIFS — calibrer les instruments avant de croire leurs chiffres
// =============================================================================
static void ControlesPositifs() {
	printf("\n=== 0. CONTRÔLES POSITIFS DES INSTRUMENTS ===\n");
	printf("    Population comptée par TOUS les compteurs : les cellules INTÉRIEURES.\n");

	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {0.4f, 0.4f, 0.4f};
	p.cellSize = 0.05f; // 8 x 8 x 8 = 512 cellules intérieures
	NkFluidGrid g;
	if (!g.Init(p)) {
		Check(false, "Init de la grille", "Init a rendu false");
		return;
	}
	char buf[256];
	snprintf(buf, sizeof(buf), "%u x %u x %u = %u intérieures, %u au total (bords compris)", g.Nx(), g.Ny(), g.Nz(),
			 g.Stats().cellsInterior, g.Stats().cellsTotal);
	Check(g.Stats().cellsInterior == 512 && g.Nx() == 8, "population de la grille (8x8x8 attendu)", buf);

	// --- compteur de MASSE : réponse analytique connue -----------------------
	const float32 h = g.CellSize();
	const float32 vol = h * h * h;
	float32 *d = g.Density();
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i)
				d[g.Idx(i, j, k)] = 1.f;
	const float32 attendu = 512.f * vol;
	const float32 mesure = g.TotalMass();
	snprintf(buf, sizeof(buf), "mesuré %.9f, attendu %.9f (512 x %g m^3)", (double)mesure, (double)attendu, (double)vol);
	Check(Absf(mesure - attendu) <= 1.0e-6f * attendu, "contrôle POSITIF du compteur de masse", buf);

	// --- contrôle NÉGATIF : la couche de bord ne doit PAS être comptée -------
	d[g.Idx(0, 1, 1)] = 1000.f;
	d[g.Idx(g.Nx() + 1, 1, 1)] = 1000.f;
	const float32 mesure2 = g.TotalMass();
	snprintf(buf, sizeof(buf), "2000 posés sur le bord : masse %.9f (inchangée)", (double)mesure2);
	Check(Absf(mesure2 - attendu) <= 1.0e-6f * attendu, "contrôle NÉGATIF : le bord n'est pas compté", buf);

	// --- compteur de DIVERGENCE : champ analytique u = (a x, 0, 0) -----------
	// div = a (1/s) -> l'instrument rapporte div*h = a*h (m/s).
	g.Reset();
	const float32 a = 3.f;
	float32 *u = const_cast<float32 *>(g.VelocityX());
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i) {
				const float32 x = ((float32)i - 0.5f) * h;
				u[g.Idx(i, j, k)] = a * x;
			}
	// On lit l'instrument sans projeter : un pas de dt nul ne tourne pas, donc on
	// passe par un pas complet avec projection COUPÉE et advection COUPÉE.
	g.Params().projectionEnabled = false;
	g.Params().advectionEnabled = false;
	g.Params().buoyancyEnabled = false;
	g.Step(1.0e-4f);
	const float32 attenduDiv = a * h;
	snprintf(buf, sizeof(buf), "mesuré %.6f m/s, attendu a*h = %.6f m/s (a = %g 1/s)", (double)g.Stats().divAfterMean,
			 (double)attenduDiv, (double)a);
	Check(Absf(g.Stats().divAfterMean - attenduDiv) <= 0.02f * attenduDiv,
		  "contrôle POSITIF du compteur de divergence", buf);

	// --- contrôle NÉGATIF : champ uniforme -> divergence nulle ---------------
	g.Reset();
	g.SetUniformVelocity({1.5f, -0.5f, 0.25f});
	g.Step(1.0e-4f);
	snprintf(buf, sizeof(buf), "champ uniforme (1,5 ; -0,5 ; 0,25) : |div|*h moyen = %.3e m/s",
			 (double)g.Stats().divAfterMean);
	Check(g.Stats().divAfterMean <= 1.0e-5f, "contrôle NÉGATIF du compteur de divergence", buf);

	// --- compteur de BARYCENTRE : blob à une place connue --------------------
	g.Reset();
	const NkVec3f centre = {0.225f, 0.125f, 0.275f};
	g.EmitSphere(centre, 0.03f, 1.f, 0.f, 0.f);
	NkVec3f c;
	const bool got = g.DensityCentroid(c);
	snprintf(buf, sizeof(buf), "posé (%.3f, %.3f, %.3f), lu (%.3f, %.3f, %.3f)", (double)centre.x, (double)centre.y,
			 (double)centre.z, (double)c.x, (double)c.y, (double)c.z);
	Check(got && Absf(c.x - centre.x) < h && Absf(c.y - centre.y) < h && Absf(c.z - centre.z) < h,
		  "contrôle POSITIF du barycentre (± 1 cellule)", buf);

	// --- contrôle NÉGATIF : grille vide -> pas de barycentre -----------------
	g.Reset();
	const bool vide = g.DensityCentroid(c);
	Check(!vide, "contrôle NÉGATIF du barycentre (grille vide)", "rend false, ne fabrique pas de point");
}

// =============================================================================
// L'ORDRE DE LA PERTE DE MASSE — on fait varier dt SEUL, puis h SEUL, sur la
// MEME scene et la MEME duree physique. Un effet du pas de temps se divise avec
// dt ; un effet de l'interpolation spatiale ne bouge pas quand dt change.
// Sans ce tableau, « l'advection semi-lagrangienne perd de la masse » est une
// phrase, pas une mesure : on ne saurait pas quel correctif nommer.
// =============================================================================
static void OrdreDeLaPerte() {
	printf("\n=== ORDRE DE LA PERTE DE MASSE (meme scene, meme duree physique 4,167 s) ===\n");
	printf("    schema        h (m)   dt        pas    derive de masse   (paroi)\n");
	const float32 duree = 500.f / 120.f;
	const float32 dts[2] = {1.f / 120.f, 1.f / 240.f};
	const float32 hs[2] = {0.03f, 0.045f};
	const char *noms[3] = {"ordre 1  ", "RK2      ", "RK2+MacC."};

	for (uint32 sch = 0; sch < 3; ++sch) {
		for (uint32 ih = 0; ih < 2; ++ih) {
			for (uint32 id = 0; id < 2; ++id) {
				NkFluidGridParams p;
				p.boundsMin = {-0.3f, 0.f, -0.3f};
				p.boundsMax = {0.3f, 1.8f, 0.3f};
				p.cellSize = hs[ih];
				p.pressureTolerance = 1.0e-5f;
				p.pressureIterations = 600;
				p.advectRK2 = (sch >= 1);
				p.advectMacCormack = (sch == 2);
				NkFluidGrid g;
				g.Init(p);
				g.EmitSphere({0.f, 0.15f, 0.f}, 0.07f, 1.f, 150.f, 0.f);
				const float32 m0 = g.TotalMass();
				const uint32 pas = (uint32)(duree / dts[id] + 0.5f);
				for (uint32 s2 = 0; s2 < pas; ++s2)
					g.Step(dts[id]);
				printf("    %-12s  %.3f   1/%-4.0f s  %5u   %+9.3f %%       (%.1e)\n", noms[sch],
					   (double)hs[ih], (double)(1.f / dts[id]), pas, (double)((g.TotalMass() - m0) / m0 * 100.f),
					   (double)g.WallLayerMass());
				fflush(stdout);
			}
		}
	}
}

// =============================================================================
// (a) MASSE + (b) DIVERGENCE — la meme course, boite close
//     mutation : projection coupee -> (b) doit rougir
//
// ATTENTION AU SUJET DE LA MESURE (mesure du 05/09, DiagnosticMasse ci-dessous) :
// l'advection semi-lagrangienne n'est PAS conservative AU CONTACT D'UNE PAROI --
// la cellule du bord advecte son contenu vers la couche fantome, ou rien ne le
// recupere. Une masse mesuree apres que la fumee a touche le plafond ne mesure
// donc plus le solveur, elle mesure cet artefact. Le temoin (a) porte donc un
// GARDE : il verifie, a chaque pas, que la couche collee aux parois est restee
// VIDE. S'il ne l'est pas, le temoin le DIT et se declare hors sujet plutot que
// de rendre un vert ou un rouge qui ne parlerait pas du solveur.
// =============================================================================
static void MasseEtDivergence(bool mutationProjectionCoupee) {
	printf("\n=== %s ===\n",
		   mutationProjectionCoupee ? "MUTATION 1 : PROJECTION COUPEE (le temoin (b) DOIT rougir)"
									: "(a) MASSE CONSERVEE  +  (b) DIVERGENCE NULLE APRES PROJECTION");

	NkFluidGridParams p;
	// Boite HAUTE : le panache doit pouvoir monter 500 pas sans toucher le plafond,
	// sinon le temoin (a) change de sujet (voir l'avertissement ci-dessus).
	p.boundsMin = {-0.3f, 0.f, -0.3f};
	p.boundsMax = {0.3f, 1.8f, 0.3f};
	p.cellSize = 0.03f; // 20 x 60 x 20 = 24 000 cellules interieures
	p.densityDissipation = 0.f;
	p.temperatureDissipation = 0.f;
	p.projectionEnabled = !mutationProjectionCoupee;
	p.pressureIterations = 600;
	p.pressureTolerance = 1.0e-5f; // relatif au residu de depart

	NkFluidGrid g;
	if (!g.Init(p)) {
		Check(false, "Init", "Init a rendu false");
		return;
	}
	printf("    grille %u x %u x %u (h = %g m), %u cellules interieures\n", g.Nx(), g.Ny(), g.Nz(), (double)p.cellSize,
		   g.Stats().cellsInterior);

	// Une bulle chaude et fumee en bas : il FAUT un ecoulement, sinon une masse
	// conservee ne prouverait que « rien ne bouge » (cas Z du diagnostic).
	g.EmitSphere({0.f, 0.15f, 0.f}, 0.07f, 1.f, 150.f, 0.f);
	const float32 masse0 = g.TotalMass();
	const float32 seuilParoi = 1.0e-4f * masse0; // 0,01 % de la masse initiale

	const float32 dt = 1.f / 120.f;
	const uint32 pas = 500;
	float64 iterSum = 0.0, ratioSum = 0.0, ratioStrictSum = 0.0, msSum = 0.0;
	float32 ratioMax = 0.f, residuMax = 0.f, paroiMax = 0.f;
	uint32 capHits = 0, premierContact = 0;
	for (uint32 s = 0; s < pas; ++s) {
		g.Step(dt);
		ratioStrictSum += (float64)g.Stats().divRatioStrict;
		const float32 paroi = g.WallLayerMass();
		if (paroi > paroiMax)
			paroiMax = paroi;
		if (paroi > seuilParoi && premierContact == 0)
			premierContact = s + 1;
		iterSum += (float64)g.Stats().pressureIters;
		ratioSum += (float64)g.Stats().divRatio;
		msSum += (float64)g.Stats().ms;
		if (g.Stats().divRatio > ratioMax)
			ratioMax = g.Stats().divRatio;
		if (g.Stats().pressureResidual > residuMax)
			residuMax = g.Stats().pressureResidual;
		if (g.Stats().pressureCapHit)
			++capHits;
	}
	const float32 masse1 = g.TotalMass();
	const float32 derive = (masse0 > 0.f) ? Absf(masse1 - masse0) / masse0 : 1.f;
	const float32 ratioMoyen = (float32)(ratioSum / (float64)pas);

	char buf[400];
	// GARDE : le temoin (a) n'a de sens que si la fumee n'a jamais touche la paroi.
	snprintf(buf, sizeof(buf), "masse paroi max %.3e (seuil %.3e), premier contact au pas %u (0 = jamais)",
			 (double)paroiMax, (double)seuilParoi, premierContact);
	if (!mutationProjectionCoupee)
		Check(premierContact == 0, "GARDE de (a) : la fumee n'a jamais touche la paroi", buf);

	snprintf(buf, sizeof(buf), "masse %.9f -> %.9f, derive %.4f %% sur %u pas (critere < 1 %%)", (double)masse0,
			 (double)masse1, (double)(derive * 100.f), pas);
	if (!mutationProjectionCoupee)
		Check(derive < 0.01f, "(a) masse conservee, boite close", buf);
	else
		printf("  [note] (a) sous mutation : derive %.4f %%\n", (double)(derive * 100.f));

	const float32 ratioStrict = (float32)(ratioStrictSum / (float64)pas);
	snprintf(buf, sizeof(buf),
			 "STRICT %.6f %% sur %u cellules ; TOUT L'INTERIEUR %.6f %% sur %u (max %.4f %%) ; %.1f balayages SOR/pas "
			 "(omega %.4f), residu %.3e -> %.3e m/s, %u fois la borne",
			 (double)(ratioStrict * 100.f), g.Stats().cellsStrict, (double)(ratioMoyen * 100.f),
			 g.Stats().cellsInterior, (double)(ratioMax * 100.f), (double)(iterSum / (float64)pas),
			 (double)g.Stats().pressureOmega, (double)g.Stats().pressureResidual0, (double)residuMax, capHits);
	if (!mutationProjectionCoupee)
		Check(ratioStrict < 0.001f, "(b) |div|*h / |u| < 0,1 % sur l'interieur STRICT", buf);
	else
		Check(ratioStrict >= 0.001f, "MUTATION : (b) rougit bien sans projection", buf);

	snprintf(buf, sizeof(buf), "%.2f ms/pas (CPU, un seul fil), %u NaN, vmax %.3f m/s, %u bornees",
			 (double)(msSum / (float64)pas), g.Stats().nanCount, (double)g.Stats().maxSpeed, g.Stats().speedClamped);
	printf("    cout : %s\n", buf);
}

// =============================================================================
// DIAGNOSTIC DE LA MASSE — trois regimes, pour savoir QUI perd la masse :
// l'advection seule (vitesse nulle), le transport uniforme, ou l'ecoulement.
// Ecrit apres avoir vu (a) rougir a 47 % : on mesure, on ne suppose pas.
// =============================================================================
static void DiagnosticMasse() {
	printf("\n=== DIAGNOSTIC : d'ou vient la derive de masse ? ===\n");
	const float32 dt = 1.f / 120.f;
	const uint32 pas = 500;

	struct Cas {
			const char *nom;
			bool vitesseNulle;
			bool uniforme;
			bool flottabilite;
			bool projection;
	};
	const Cas cas[6] = {
		{"Z : vitesse NULLE (advection identite)", true, false, false, false},
		{"T : vitesse UNIFORME 0,02 m/s, LOIN des parois", false, true, false, false},
		{"U : vitesse UNIFORME 0,5 m/s vers la paroi", false, true, false, false},
		{"P : panache, boite BASSE, projection COUPEE", false, false, true, false},
		{"F : panache, boite BASSE, solveur COMPLET", false, false, true, true},
		{"G : panache, boite HAUTE, solveur COMPLET", false, false, true, true},
	};

	for (uint32 c = 0; c < 6; ++c) {
		NkFluidGridParams p;
		p.boundsMin = {-0.25f, 0.f, -0.25f};
		p.boundsMax = {0.25f, 0.5f, 0.25f};
		if (c == 5) { // G : la boite HAUTE, ou la fumee ne touche aucune paroi
			p.boundsMin = {-0.3f, 0.f, -0.3f};
			p.boundsMax = {0.3f, 1.8f, 0.3f};
		}
		p.cellSize = 0.02f;
		p.buoyancyEnabled = cas[c].flottabilite;
		p.projectionEnabled = cas[c].projection;
		p.pressureTolerance = 1.0e-4f;
		NkFluidGrid g;
		g.Init(p);
		g.EmitSphere({0.f, (c == 1) ? 0.25f : 0.15f, 0.f}, (c == 5) ? 0.07f : 0.06f, 1.f,
					 cas[c].flottabilite ? 150.f : 0.f, 0.f);
		const float32 m0 = g.TotalMass();
		for (uint32 s = 0; s < pas; ++s) {
			if (cas[c].uniforme)
				g.SetUniformVelocity({(c == 1) ? 0.02f : 0.5f, 0.f, 0.f});
			g.Step(dt);
		}
		const float32 m1 = g.TotalMass();
		printf("    %-44s masse %.9f -> %.9f, derive %+8.4f %%  (paroi %.2e, |div|*h %.2e)\n", cas[c].nom, (double)m0,
			   (double)m1, (double)((m1 - m0) / m0 * 100.f), (double)g.WallLayerMass(),
			   (double)g.Stats().divAfterMean);
		fflush(stdout);
	}
}

// =============================================================================
// LE PLANCHER DU SOLVEUR DE PRESSION — le temoin (b) est ROUGE : est-ce qu'il
// manque des BALAYAGES, ou est-ce qu'on est au PLANCHER de la simple precision ?
// On relache la borne d'iterations d'un facteur 10 sur une grille plus legere et
// on regarde si le residu descend encore. Sans ce tableau, « il faudrait un
// meilleur solveur » serait une opinion.
// =============================================================================
void PlancherDuSolveur(float32 epsilon) {
	printf("\n=== PLANCHER DU SOLVEUR DE PRESSION (memes 40 pas, grille allegee, epsilon = %.1f) ===\n",
		   (double)epsilon);
	printf("    borne   tolerance   balayages/pas   residu final   |div|*h/|u| moyen\n");
	printf("    (si le rapport ne bouge pas quand le residu chute, ce n'est pas le solveur)\n");
	const uint32 bornes[3] = {200, 800, 4000};
	const float32 tols[3] = {1.0e-4f, 1.0e-6f, 1.0e-8f};
	for (uint32 c = 0; c < 3; ++c) {
		NkFluidGridParams p;
		p.boundsMin = {-0.3f, 0.f, -0.3f};
		p.boundsMax = {0.3f, 1.2f, 0.3f};
		p.cellSize = 0.04f; // 15 x 30 x 15
		p.pressureIterations = bornes[c];
		p.pressureTolerance = tols[c];
		p.vorticityConfinement = epsilon;
		NkFluidGrid g;
		g.Init(p);
		g.EmitSphere({0.f, 0.15f, 0.f}, 0.08f, 1.f, 150.f, 0.f);
		float64 iters = 0.0, ratio = 0.0, strict = 0.0, residu = 0.0;
		uint32 caps = 0;
		for (uint32 s2 = 0; s2 < 40; ++s2) {
			g.Step(1.f / 120.f);
			iters += (float64)g.Stats().pressureIters;
			ratio += (float64)g.Stats().divRatio;
			strict += (float64)g.Stats().divRatioStrict;
			residu += (float64)g.Stats().pressureResidual;
			if (g.Stats().pressureCapHit)
				++caps;
		}
		printf("    %5u   %.0e      %9.1f       %.3e      strict %.6f %% / tout %.6f %%  (%u fois la borne)\n",
			   bornes[c], (double)tols[c], (double)(iters / 40.0), (double)(residu / 40.0),
			   (double)(strict / 40.0 * 100.0), (double)(ratio / 40.0 * 100.0), caps);
		fflush(stdout);
	}
}

// =============================================================================
// (c) FLOTTABILITÉ contre Boussinesq / Archimède
// =============================================================================
static void Flottabilite() {
	printf("\n=== (c) FLOTTABILITÉ — Boussinesq : a = g (T - T_amb) / T_amb ===\n");
	printf("    Fedkiw, Stam & Jensen, « Visual Simulation of Smoke », SIGGRAPH 2001, eq. (8).\n");

	// --- c1 : la LOI seule (projection et advection coupées) ----------------
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, 0.f, -0.25f};
	p.boundsMax = {0.25f, 0.5f, 0.25f};
	p.cellSize = 0.025f;
	p.ambientTemperature = 300.f;
	p.gravity = 9.81f;
	p.buoyancyBeta = -1.f; // Boussinesq
	p.projectionEnabled = false;
	p.advectionEnabled = false;
	NkFluidGrid g;
	if (!g.Init(p)) {
		Check(false, "Init (c1)", "Init a rendu false");
		return;
	}
	const float32 dT = 100.f;
	g.EmitSphere({0.f, 0.25f, 0.f}, 0.08f, 1.f, dT, 0.f);
	const float32 dt = 1.f / 240.f;
	const uint32 pas = 240; // 1 s
	for (uint32 s = 0; s < pas; ++s)
		g.Step(dt);
	const float32 t = dt * (float32)pas;
	const float32 attendu = (p.gravity * dT / p.ambientTemperature) * t; // a * t
	const float32 mesure = g.HotVerticalVelocity();
	const float32 ecart = Absf(mesure - attendu) / attendu;
	char buf[320];
	snprintf(buf, sizeof(buf), "v mesurée %.6f m/s, attendue g*ΔT/T_amb*t = %.6f m/s (ΔT = %g K, t = %g s) — écart %.4f %%",
			 (double)mesure, (double)attendu, (double)dT, (double)t, (double)(ecart * 100.f));
	Check(ecart < 0.01f, "(c1) accélération = Archimède/Boussinesq (< 1 %)", buf);

	// --- c2 : le solveur COMPLET — la bulle chaude monte --------------------
	NkFluidGridParams q = p;
	q.projectionEnabled = true;
	q.advectionEnabled = true;
	NkFluidGrid g2;
	g2.Init(q);
	g2.EmitSphere({0.f, 0.12f, 0.f}, 0.06f, 1.f, dT, 0.f);
	NkVec3f c0;
	g2.TemperatureCentroid(c0);
	for (uint32 s = 0; s < 240; ++s)
		g2.Step(dt);
	NkVec3f c1;
	g2.TemperatureCentroid(c1);
	const float32 montee = c1.y - c0.y;
	snprintf(buf, sizeof(buf), "barycentre de température : y %.4f -> %.4f m, soit +%.4f m = %.2f cellules en 1 s",
			 (double)c0.y, (double)c1.y, (double)montee, (double)(montee / q.cellSize));
	Check(montee > q.cellSize, "(c2) solveur complet : la bulle chaude MONTE", buf);

	// --- contrôle NÉGATIF : sans écart de température, rien ne monte ---------
	NkFluidGrid g3;
	g3.Init(q);
	g3.EmitSphere({0.f, 0.12f, 0.f}, 0.06f, 1.f, 0.f, 0.f); // densité seule, T = ambiante
	NkVec3f d0;
	g3.DensityCentroid(d0);
	for (uint32 s = 0; s < 240; ++s)
		g3.Step(dt);
	NkVec3f d1;
	g3.DensityCentroid(d1);
	snprintf(buf, sizeof(buf), "ΔT = 0 : le barycentre de densité bouge de %.6f m (%.3f cellule)",
			 (double)(d1.y - d0.y), (double)((d1.y - d0.y) / q.cellSize));
	Check(Absf(d1.y - d0.y) < 0.05f * q.cellSize, "contrôle NÉGATIF : sans ΔT, rien ne monte", buf);
}

// =============================================================================
// (d) TRANSPORT — un champ porté par une vitesse uniforme arrive au bon endroit
//     mutation : advection coupée -> doit rougir
// =============================================================================
static void Transport(bool mutationAdvectionCoupee) {
	printf("\n=== %s ===\n", mutationAdvectionCoupee ? "MUTATION 2 : ADVECTION COUPÉE (le témoin (d) DOIT rougir)"
													 : "(d) TRANSPORT SEMI-LAGRANGIEN");

	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {1.f, 0.5f, 0.5f};
	p.cellSize = 0.02f; // 50 x 25 x 25
	p.projectionEnabled = false; // la vitesse uniforme est déjà à divergence nulle
	p.buoyancyEnabled = false;
	p.advectionEnabled = !mutationAdvectionCoupee;
	NkFluidGrid g;
	if (!g.Init(p)) {
		Check(false, "Init (d)", "Init a rendu false");
		return;
	}
	const NkVec3f depart = {0.25f, 0.25f, 0.25f};
	g.EmitSphere(depart, 0.06f, 1.f, 0.f, 0.f);
	NkVec3f c0;
	g.DensityCentroid(c0);

	const NkVec3f vitesse = {0.5f, 0.f, 0.f};
	const float32 dt = 1.f / 120.f;
	const uint32 pas = 120; // 1 s
	for (uint32 s = 0; s < pas; ++s) {
		g.SetUniformVelocity(vitesse); // ré-imposée : la mesure porte sur le TRANSPORT
		g.Step(dt);
	}
	NkVec3f c1;
	g.DensityCentroid(c1);
	const float32 attendu = vitesse.x * dt * (float32)pas;
	const float32 mesure = c1.x - c0.x;
	const float32 erreurCellules = Absf(mesure - attendu) / p.cellSize;

	char buf[320];
	snprintf(buf, sizeof(buf), "x %.4f -> %.4f m : déplacement %.4f m, attendu %.4f m, erreur %.3f cellule",
			 (double)c0.x, (double)c1.x, (double)mesure, (double)attendu, (double)erreurCellules);
	if (!mutationAdvectionCoupee)
		Check(erreurCellules < 1.f, "(d) arrivé au bon endroit (± 1 cellule)", buf);
	else
		Check(erreurCellules >= 1.f, "MUTATION : (d) rougit bien sans advection", buf);
}

// =============================================================================
// (e) 10 s sans NaN ni explosion, source continue
// =============================================================================
static void DixSecondes() {
	printf("\n=== (e) 10 s DE PANACHE, SOURCE CONTINUE ===\n");
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, 0.f, -0.25f};
	p.boundsMax = {0.25f, 1.f, 0.25f};
	p.cellSize = 0.02f; // 25 x 50 x 25
	p.densityDissipation = 0.2f;
	p.temperatureDissipation = 0.5f;
	p.buoyancyAlpha = 0.3f;
	NkFluidGrid g;
	if (!g.Init(p)) {
		Check(false, "Init (e)", "Init a rendu false");
		return;
	}
	const float32 dt = 1.f / 60.f;
	const uint32 pas = 600;
	uint32 nan = 0, clamped = 0, caps = 0;
	float32 vmax = 0.f, tmax = 0.f;
	float64 msSum = 0.0;
	for (uint32 s = 0; s < pas; ++s) {
		g.EmitSphere({0.f, 0.05f, 0.f}, 0.05f, 6.f * dt, 900.f * dt, 0.f);
		g.Step(dt);
		nan += g.Stats().nanCount;
		clamped += g.Stats().speedClamped;
		if (g.Stats().pressureCapHit)
			++caps;
		if (g.Stats().maxSpeed > vmax)
			vmax = g.Stats().maxSpeed;
		if (g.Stats().maxTemperature > tmax)
			tmax = g.Stats().maxTemperature;
		msSum += (float64)g.Stats().ms;
	}
	char buf[320];
	snprintf(buf, sizeof(buf), "%u NaN/Inf, vmax %.3f m/s, Tmax %.1f K, %u bornées, %u fois la borne d'itérations",
			 nan, (double)vmax, (double)tmax, clamped, caps);
	Check(nan == 0 && clamped == 0 && vmax < 50.f, "(e) 600 pas (10 s) : ni NaN ni explosion", buf);
	printf("    coût : %.2f ms/pas sur %u cellules intérieures (CPU, un seul fil)\n", (double)(msSum / (float64)pas),
		   g.Stats().cellsInterior);

	NkVec3f c;
	if (g.DensityCentroid(c))
		printf("    barycentre de fumée après 10 s : y = %.3f m (source à 0,05 m)\n", (double)c.y);
}

// =============================================================================
int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	printf("=============================================================\n");
	printf("NkFluidGridProbe — la GRILLE de fumée (Stam, « Stable Fluids »,\n");
	printf("SIGGRAPH 1999) ; flottabilité : Fedkiw, Stam & Jensen, SIGGRAPH\n");
	printf("2001, eq. (8). Banc CPU, aucun GPU, aucune fenêtre.\n");
	printf("=============================================================\n");

	// Mode BALAYAGE : seul le tableau qui CHOISIT epsilon tourne. C'est une
	// enquete de parametre, pas un temoin -- elle ne rend aucun verdict.
	const char *sweep = ::nkentseu::env::GetEnvVar("NK_FLUID_SWEEP");
	if (sweep != nullptr && sweep[0] == '1') {
		EnqueteConfinement();
		return 0;
	}

	// Mode BASCULE MAC : seuls les controles de la bascule et le CRITERE DECISIF
	// tournent. Ce n'est PAS un raccourci de complaisance, et il ne rend aucun
	// verdict definitif : la course COMPLETE coute ~15 minutes (mesure du 12/09),
	// et juger le § 4.3 du plan n'exige que deux tableaux. Une boucle de mesure
	// courte est ce qui permet de corriger sans changer de sujet entre deux essais.
	// Le verdict, lui, reste celui de la course complete, qu'on relance AVANT de
	// conclure quoi que ce soit -- sans quoi on jugerait la bascule sur un banc
	// qu'on aurait choisi parce qu'il est rapide.
	const char *mac = ::nkentseu::env::GetEnvVar("NK_FLUID_MAC");
	// NK_FLUID_MAC=3 : les deux ENQUETES de la bascule. Ce ne sont PAS des temoins
	// et elles ne rendent AUCUN verdict : elles font varier la RESOLUTION pour
	// departager un effet de BORD d'un defaut de SCHEMA. Une question posee a une
	// seule resolution ne peut pas trancher entre les deux.
	if (mac != nullptr && mac[0] == '3') {
		EnqueteBascule();
		return 0;
	}
	// NK_FLUID_MAC=4 : (f2) LE PRIX du donor-cell, mesure sur le PREMIER ORDRE NU
	// et AVEC le sous-cyclage que la course (e) reclame. Mesurer sans, puis activer
	// le sous-cyclage pour (e), reviendrait a mesurer DEUX SCHEMAS DIFFERENTS : le
	// sous-cyclage change la diffusion effective. Les planchers sont ceux du § 2 du
	// plan, ecrits AVANT la mesure et jamais deplaces depuis.
	if (mac != nullptr && mac[0] == '4') {
		EnqueteLePrix();
		printf("\n=============================================================\n");
		printf("BILAN (mode NK_FLUID_MAC=4, (f2) LE PRIX) : %d controles, %d ROUGES\n", gChecks, gFailures);
		printf("=============================================================\n");
		return gFailures == 0 ? 0 : 1;
	}
	// NK_FLUID_MAC=5 : (f3) LA STABILITE, qui CHANGE DE NATURE avec ce schema.
	// ⚠️ Le FILET y est COUPE (advectMaxSubsteps = 1) pour mesurer la condition NUE :
	// avec le sous-cyclage, le schema ne casse JAMAIS -- il subdivise -- et on
	// mesurerait LE FILET au lieu du schema. (f3b) verifie ensuite, separement, que
	// le filet rattrape bien le dt qui cassait sans lui.
	// ⚠️ Le detecteur n'est PAS la masse : le donor-cell reste conservatif MEME
	// instable. C'est la DENSITE NEGATIVE qui trahit la perte de monotonie.
	if (mac != nullptr && mac[0] == '5') {
		EnqueteStabilite();
		printf("\n=============================================================\n");
		printf("BILAN (mode NK_FLUID_MAC=5, (f3) LA STABILITE) : %d controles, %d ROUGES\n", gChecks, gFailures);
		printf("=============================================================\n");
		return gFailures == 0 ? 0 : 1;
	}
	if (mac != nullptr && (mac[0] == '1' || mac[0] == '2')) {
		ControlesPositifs();
		PalierGrilleMAC();
		PalierAdvectionFlux();
		// NK_FLUID_MAC=2 : le JALON LE PLUS COURT -- seuls (m1), (m2) et (f1)
		// tournent, en quelques secondes. Il repond a des questions de SCHEMA, sur
		// des champs analytiques : le stockage sur les FACES est-il pose, la
		// divergence est-elle compacte, et le bilan de masse est-il ferme ? Il ne
		// dit RIEN du PRIX du schema, RIEN de sa stabilite, et surtout RIEN du
		// critere decisif du § 1 -- pour celui-la il faut NK_FLUID_MAC=1, et pour un
		// verdict, la course complete.
		if (mac[0] == '1') {
			MasseEtDivergence(false);
			PlancherDuSolveur(0.f);
		}
		printf("\n=============================================================\n");
		printf("BILAN (mode NK_FLUID_MAC, PARTIEL — pas un verdict) : %d controles, %d ROUGES\n", gChecks, gFailures);
		printf("=============================================================\n");
		return gFailures == 0 ? 0 : 1;
	}

	ControlesPositifs();
	// (m1) et (m2) : les deux controles de la BASCULE MAC. Ils sont ROUGES tant que
	// la grille est COLOCALISEE, et c'est exactement leur raison d'etre -- un temoin
	// qui naitrait vert le jour de la bascule ne dirait pas s'il juge la bascule ou
	// s'il juge que « ca compile ». Voir PLAN_GRILLE_MAC.md, § 2 et § 4.
	PalierGrilleMAC();
	// (f1) le controle negatif de l'ADVECTION CONSERVATIVE EN FLUX. ROUGE tant que
	// l'advection reste semi-lagrangienne : elle n'a AUCUN bilan. Un schema en flux
	// est conservatif par construction et doit le faire verdir au PREMIER jet.
	// Voir PLAN_ADVECTION_FLUX.md, § 1.
	PalierAdvectionFlux();
	// Les deux ENQUETES (six regimes de masse, table des schemas) coutent a elles
	// seules plus que tous les temoins reunis : elles tournent sous NK_FLUID_DIAG=1.
	// Ce ne sont pas des temoins -- ce sont les mesures qui ont DESIGNE la cause de
	// la perte de masse, et elles restent rejouables telles quelles.
	const char *diag = ::nkentseu::env::GetEnvVar("NK_FLUID_DIAG");
	if (diag != nullptr && diag[0] == '1') {
		DiagnosticMasse();
		OrdreDeLaPerte();
		EnqueteConfinement();
	} else {
		printf("\n(les deux enquetes de masse ne tournent que sous NK_FLUID_DIAG=1)\n");
	}
	MasseEtDivergence(false);
	PlancherDuSolveur(0.f);
	Flottabilite();
	Transport(false);
	DixSecondes();
	PalierRendu();
	PalierFeu();
	PalierVorticite();
	PalierBranchement();
	ImagesDuConfinement(EpsilonConfinement());
	MasseEtDivergence(true); // mutation 1
	Transport(true);		 // mutation 2

	printf("\n=============================================================\n");
	printf("BILAN : %d contrôles, %d ROUGES\n", gChecks, gFailures);
	printf("=============================================================\n");
	return gFailures == 0 ? 0 : 1;
}
