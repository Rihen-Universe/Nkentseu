// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// vorticite.cpp — les témoins du CONFINEMENT DE VORTICITÉ (palier ④, 2026-09-06).
//
// LA QUESTION, et elle vient de Rodolf : les deux images du 05/09 montrent un JET
// FIN ET DROIT. La poussée d'Archimède à 1 700 K vaut ~46 m/s^2 et RIEN ne fait
// tournoyer le panache — parce que l'advection semi-lagrangienne DISSIPE la
// vorticité (c'est le prix de sa stabilité inconditionnelle). Le remède est écrit
// depuis vingt-cinq ans : Ronald FEDKIW, Jos STAM, Henrik Wann JENSEN, « Visual
// Simulation of Smoke », SIGGRAPH 2001, § 4, eq. (9)-(11) — le CONFINEMENT DE
// VORTICITÉ, qui réinjecte la vorticité là où elle est déjà.
//
// PRÉ-ENREGISTREMENT — écrit AVANT d'avoir lu le moindre chiffre.
//
//  0. CONTRÔLES D'INSTRUMENT. Deux instruments neufs, donc deux calibrations
//     avant de croire un chiffre, chacune contre une réponse ANALYTIQUE :
//     - ENSTROPHIE : sur une rotation solide u = Omega x r, rot(u) = 2*Omega
//       EXACTEMENT, et les différences centrées sont exactes sur un champ
//       linéaire. Contrôle NÉGATIF : champ uniforme -> |omega| = 0.
//     - RAYON DE GIRATION : sur une gaussienne 2D d'écart-type sigma, la racine
//       de la moyenne des carrés des distances au barycentre vaut sigma*rac(2).
//       Contrôle NÉGATIF : une cellule seule -> rayon nul ; tranche vide ->
//       l'instrument REND FALSE au lieu de fabriquer un point.
//
//  1. LE TÉMOIN. Trois courses sur LA MÊME scène (le solveur est déterministe :
//     aucun aléa, donc seul epsilon change) :
//        A : epsilon = 0        (le jet d'aujourd'hui)
//        B : epsilon = EPSILON  (le confinement armé)
//        C : epsilon = -EPSILON (la MUTATION : la force INVERSÉE)
//     Ce qu'on lira, et les seuils sont fixés ICI, avant la mesure :
//        (v1) enstrophie moyenne B / A         >= 2,0
//        (v2) rayon du panache à 0,60 m  B / A >= 1,15
//        (v3) la masse n'EMPIRE pas   : dérive(B) <= dérive(A) + 5 points
//        (v4) la divergence n'EMPIRE pas : divStrict(B) <= 1,5 x divStrict(A)
//        (v5) MUTATION : enstrophie C / A < 1,0
//     La MASSE DE LA TRANCHE est publiée à côté de chaque rayon : comparer deux
//     rayons sans comparer les masses comparerait deux populations différentes.
//
//  2. LE TÉMOIN (v6), ÉCRIT ET DATÉ LE 2026-09-07 À 01h, APRÈS QUE (v1) A ROUGI
//     ET AVANT LA COURSE QUI LE JUGERA. Ce n'est pas un seuil déplacé : (v1) reste
//     écrit tel quel et reste ROUGE ; (v6) est une AUTRE question, née de ce que
//     (v1) a appris.
//     Ce que (v1) a appris, et c'est le fait : au bout de 3 s, la course A et la
//     course B ne sont plus le MÊME ÉCOULEMENT — B monte deux fois moins haut.
//     Comparer leurs enstrophies TOTALES, c'est comparer deux sujets, pas deux
//     réglages (« un écart entre deux mesures n'est un fait que si les deux
//     mesurent la même chose »). Le balayage le montre en une ligne : sur les 60
//     PREMIERS pas — avant que les écoulements ne divergent — l'enstrophie MONTE
//     avec epsilon ; sur toute la course, elle descend.
//     La grandeur qui, elle, ne dépend pas de l'échelle de l'écoulement est la
//     CONCENTRATION de la vorticité :
//         P = racine( enstrophie / V ) / |omega| moyen     (sans dimension)
//     c'est-à-dire le rapport de la moyenne quadratique de |omega| à sa moyenne
//     arithmétique, sur la même population (l'intérieur STRICT). P = 1 si |omega|
//     est UNIFORME ; P grandit quand la vorticité se rassemble en filaments.
//     « Confiner » veut dire exactement ça, et rien d'autre.
//        (v6) P(B) >= 1,05 x P(A)   — le confinement CONCENTRE
//        (v6b) MUTATION : P(C) <= 0,70 x P(A) — la force inversée ÉTALE
//
//  ⚠️ CE QUE (v2) NE PEUT PAS FAIRE, MESURÉ LE 07/09 ET ÉCRIT ICI POUR QUE
//  PERSONNE NE S'Y FIE SEUL : le rayon du panache NE DÉPARTAGE PAS LE SIGNE de la
//  force. À epsilon = -8 — la force INVERSÉE, donc la faute qu'on chasse — le
//  panache est encore PLUS large qu'avec le confinement : 0,137 m contre 0,071 m,
//  soit x 3,59 sur A au lieu de x 1,85. C'est logique une fois vu : étaler la
//  vorticité étale aussi la fumée. **(v2) seul serait donc passé au vert sur un
//  signe faux.** Seule la CONCENTRATION (v6) sépare les deux, et c'est pour ça
//  qu'elle existe.
//
//  CE QUE LA MUTATION (v5) SERT, ET POURQUOI ELLE N'EST PAS DÉCORATIVE : sans
//  elle, « l'enstrophie augmente » ne prouverait que « du code tourne ». Le
//  signe du produit vectoriel N x omega est l'erreur la plus probable de cette
//  formule, et un signe faux LISSE la vorticité au lieu de la confiner. epsilon
//  négatif fabrique exactement cette faute, et le témoin doit la voir.
//
//  CE QUE CE BANC NE PROUVE PAS : rien sur le rendu (le panache tournoie-t-il À
//  L'ŒIL ? c'est l'image qui répond, pas ce fichier), rien sur le GPU, rien sur
//  le coût temps réel.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGrid.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);
void PlancherDuSolveur(float32 epsilon);

// Les seuils du pré-enregistrement, en un seul endroit, nommés.
// EPSILON RETENU : le plus PETIT du balayage (NK_FLUID_SWEEP=1) qui atteint le
// seuil de (v2) deja pre-enregistre -- x 1,15 sur la largeur du panache. La regle
// de choix est ecrite ici, pas appliquee au jugement : choisir un PARAMETRE et
// deplacer un CRITERE sont deux gestes differents, et les seuils ci-dessous n'ont
// pas bouge depuis qu'ils ont ete ecrits.
static const float32 kEpsilon = 8.f;
static const float32 kSeuilEnstrophie = 2.0f;
static const float32 kSeuilRayon = 1.15f;
static const float32 kMargeMasse = 0.05f; // 5 points de pourcentage
static const float32 kFacteurDivergence = 1.5f;
static const float32 kHauteurMesure = 0.60f; // m — où l'on mesure la largeur du panache

// L'epsilon retenu, lu par le banc d'images (rendu.cpp) : UNE seule valeur dans
// tout le banc, sinon l'image et le temoin ne parleraient pas du meme reglage.
float32 EpsilonConfinement() {
	return kEpsilon;
}

// =============================================================================
// La scène commune aux trois courses. Rien n'y change sauf `epsilon`.
// =============================================================================
struct ResultatPanache {
		float32 enstrophieMoy = 0.f;
		float32 enstrophieFin = 0.f;
		float32 vorticiteMoy = 0.f;
		float32 rayon = 0.f;
		float32 masseTranche = 0.f;
		uint32 rangee = 0;
		bool rayonValide = false;
		float32 deriveMasse = 0.f; // signée, en fraction
		float32 divStrict = 0.f;
		float32 accelConfinement = 0.f;
		float32 msParPas = 0.f;
		float32 hauteurBarycentre = 0.f;
		uint32 cellulesStrictes = 0;
		float32 enstrophieDebut = 0.f; // moyenne sur les 60 PREMIERS pas
		float32 deriveMasseTotale = 0.f;
};

static ResultatPanache CoursePanache(float32 epsilon, uint32 pas) {
	ResultatPanache r;
	NkFluidGridParams p;
	p.boundsMin = {-0.35f, 0.f, -0.35f};
	p.boundsMax = {0.35f, 1.4f, 0.35f};
	p.cellSize = 0.025f; // 28 x 56 x 28 = 43 904 cellules intérieures
	p.pressureTolerance = 1.0e-4f;
	p.pressureIterations = 400;
	p.vorticityConfinement = epsilon;
	p.densityDissipation = 0.f;
	p.temperatureDissipation = 0.f;

	NkFluidGrid g;
	if (!g.Init(p))
		return r;
	// Source CONTINUE : un panache s'établit, il ne se contente pas de monter une
	// fois. C'est le régime que Rodolf voit à l'image.
	const float32 dt = 1.f / 120.f;
	const float32 masse0debit = 4.f * dt;
	float64 ensSum = 0.0, vortSum = 0.0, divSum = 0.0, msSum = 0.0, accSum = 0.0, ensDebut = 0.0;
	const uint32 debut = (pas < 60u) ? pas : 60u;
	for (uint32 s = 0; s < pas; ++s) {
		g.EmitSphere({0.f, 0.06f, 0.f}, 0.05f, masse0debit, 1400.f * dt, 0.f);
		g.Step(dt);
		ensSum += (float64)g.Stats().enstrophy;
		if (s < debut)
			ensDebut += (float64)g.Stats().enstrophy;
		vortSum += (float64)g.Stats().vorticityMean;
		divSum += (float64)g.Stats().divRatioStrict;
		msSum += (float64)g.Stats().ms;
		accSum += (float64)g.Stats().confinementAccelMean;
	}
	r.enstrophieMoy = (float32)(ensSum / (float64)pas);
	r.enstrophieDebut = (float32)(ensDebut / (float64)debut);
	r.enstrophieFin = g.Stats().enstrophy;
	r.vorticiteMoy = (float32)(vortSum / (float64)pas);
	r.divStrict = (float32)(divSum / (float64)pas);
	r.msParPas = (float32)(msSum / (float64)pas);
	r.accelConfinement = (float32)(accSum / (float64)pas);
	r.cellulesStrictes = g.Stats().cellsStrict;
	r.rayonValide = g.PlumeRadius(kHauteurMesure, r.rayon, r.masseTranche, r.rangee);
	NkVec3f c;
	if (g.DensityCentroid(c))
		r.hauteurBarycentre = c.y;
	return r;
}

// La dérive de masse se mesure sur une course SANS source (sinon « masse finale
// moins masse initiale » mélange la perte du schéma et ce qu'on a injecté : deux
// populations sous un seul compteur). Même boîte, même epsilon, une bulle posée.
static float32 DeriveMasseSansSource(float32 epsilon, uint32 pas, float32 &divStrictOut) {
	NkFluidGridParams p;
	p.boundsMin = {-0.35f, 0.f, -0.35f};
	p.boundsMax = {0.35f, 1.4f, 0.35f};
	p.cellSize = 0.025f;
	p.pressureTolerance = 1.0e-4f;
	p.pressureIterations = 400;
	p.vorticityConfinement = epsilon;
	NkFluidGrid g;
	if (!g.Init(p)) {
		divStrictOut = 0.f;
		return 1.f;
	}
	g.EmitSphere({0.f, 0.12f, 0.f}, 0.07f, 1.f, 150.f, 0.f);
	const float32 m0 = g.TotalMass();
	const float32 dt = 1.f / 120.f;
	float64 divSum = 0.0;
	for (uint32 s = 0; s < pas; ++s) {
		g.Step(dt);
		divSum += (float64)g.Stats().divRatioStrict;
	}
	divStrictOut = (float32)(divSum / (float64)pas);
	return (m0 > 0.f) ? ((g.TotalMass() - m0) / m0) : 1.f;
}

// =============================================================================
// 0. CONTRÔLES D'INSTRUMENT
// =============================================================================
static void ControlesVorticite() {
	printf("\n=== 0bis. CONTRÔLES DES DEUX INSTRUMENTS NEUFS (enstrophie, rayon) ===\n");
	char buf[400];

	// --- ENSTROPHIE : rotation solide u = Omega x r, rot(u) = 2 Omega ---------
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, -0.25f, -0.25f};
	p.boundsMax = {0.25f, 0.25f, 0.25f};
	p.cellSize = 0.025f; // 20^3
	p.projectionEnabled = false;
	p.advectionEnabled = false;
	p.buoyancyEnabled = false;
	NkFluidGrid g;
	if (!g.Init(p)) {
		ProbeCheck(false, "Init (contrôles de vorticité)", "Init a rendu false");
		return;
	}
	const float32 h = g.CellSize();
	const float32 omega0 = 2.f; // rad/s autour de y
	float32 *u = const_cast<float32 *>(g.VelocityX());
	float32 *v = const_cast<float32 *>(g.VelocityY());
	float32 *w = const_cast<float32 *>(g.VelocityZ());
	// Le champ est posé JUSQUE SUR LA COUCHE FANTÔME : les différences centrées de
	// la couche i = 1 lisent i = 0. Sans ça on mesurerait la convention de bord.
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i) {
				const float32 x = p.boundsMin.x + ((float32)i - 0.5f) * h;
				const float32 z = p.boundsMin.z + ((float32)k - 0.5f) * h;
				const uint32 id = g.Idx(i, j, k);
				// Omega = (0, omega0, 0) ; u = Omega x r = (omega0*z, 0, -omega0*x)
				u[id] = omega0 * z;
				v[id] = 0.f;
				w[id] = -omega0 * x;
			}
	g.Step(1.0e-5f); // un pas inerte : seul ComputeVorticity travaille
	const float32 attenduVort = 2.f * omega0;
	const float32 mesureVort = g.VorticityMean();
	snprintf(buf, sizeof(buf), "|omega| mesuré %.6f 1/s, attendu 2*Omega = %.6f 1/s (écart %.4f %%)",
			 (double)mesureVort, (double)attenduVort,
			 (double)(ProbeAbs(mesureVort - attenduVort) / attenduVort * 100.f));
	ProbeCheck(ProbeAbs(mesureVort - attenduVort) <= 0.01f * attenduVort,
			   "contrôle POSITIF de l'enstrophie (rotation solide)", buf);

	const float32 volStrict = (float32)g.Stats().cellsStrict * h * h * h;
	const float32 attenduEns = attenduVort * attenduVort * volStrict;
	snprintf(buf, sizeof(buf), "enstrophie %.6f m^3/s^2, attendue (2*Omega)^2 * V = %.6f sur %u cellules STRICTES",
			 (double)g.Enstrophy(), (double)attenduEns, g.Stats().cellsStrict);
	ProbeCheck(ProbeAbs(g.Enstrophy() - attenduEns) <= 0.01f * attenduEns,
			   "contrôle POSITIF de l'enstrophie (valeur absolue)", buf);

	// --- contrôle NÉGATIF : champ uniforme -> vorticité nulle ----------------
	g.Reset();
	g.SetUniformVelocity({1.5f, -0.5f, 0.25f});
	g.Step(1.0e-5f);
	snprintf(buf, sizeof(buf), "champ uniforme : |omega| moyen = %.3e 1/s, enstrophie = %.3e",
			 (double)g.VorticityMean(), (double)g.Enstrophy());
	ProbeCheck(g.VorticityMean() <= 1.0e-4f, "contrôle NÉGATIF de l'enstrophie (champ uniforme)", buf);

	// --- RAYON DE GIRATION : gaussienne 2D d'écart-type connu ----------------
	// Pour une gaussienne 2D isotrope, racine(<r^2>) = sigma * racine(2).
	g.Reset();
	const float32 sigma = 0.06f;
	const uint32 jTest = 10;
	float32 *d = g.Density();
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 i = 1; i <= g.Nx(); ++i) {
			const float32 x = p.boundsMin.x + ((float32)i - 0.5f) * h;
			const float32 z = p.boundsMin.z + ((float32)k - 0.5f) * h;
			const float32 r2 = x * x + z * z;
			d[g.Idx(i, jTest, k)] = NkExp(-r2 / (2.f * sigma * sigma));
		}
	const float32 yTest = p.boundsMin.y + ((float32)jTest - 0.5f) * h;
	float32 rayon = 0.f, masse = 0.f;
	uint32 rangee = 0;
	const bool ok = g.PlumeRadius(yTest, rayon, masse, rangee);
	const float32 attenduRayon = sigma * 1.41421356f;
	snprintf(buf, sizeof(buf), "sigma = %.3f m : rayon mesuré %.5f m, attendu sigma*rac(2) = %.5f m (écart %.2f %%), "
							   "rangée lue %u (attendue %u), masse de tranche %.6f",
			 (double)sigma, (double)rayon, (double)attenduRayon,
			 (double)(ProbeAbs(rayon - attenduRayon) / attenduRayon * 100.f), rangee, jTest, (double)masse);
	ProbeCheck(ok && rangee == jTest && ProbeAbs(rayon - attenduRayon) <= 0.05f * attenduRayon,
			   "contrôle POSITIF du rayon de giration (gaussienne)", buf);

	// --- contrôle NÉGATIF 1 : une cellule seule -> rayon nul -----------------
	g.Reset();
	d = g.Density();
	d[g.Idx(9, jTest, 11)] = 1.f;
	float32 r2 = 0.f, m2 = 0.f;
	uint32 j2 = 0;
	const bool ok2 = g.PlumeRadius(yTest, r2, m2, j2);
	snprintf(buf, sizeof(buf), "une cellule seule : rayon %.6f m (cellule = %.3f m)", (double)r2, (double)h);
	ProbeCheck(ok2 && r2 < 0.5f * h, "contrôle NÉGATIF du rayon (une seule cellule)", buf);

	// --- contrôle NÉGATIF 2 : tranche vide -> l'instrument REND FALSE --------
	g.Reset();
	float32 r3 = -1.f, m3 = -1.f;
	uint32 j3 = 999;
	const bool ok3 = g.PlumeRadius(yTest, r3, m3, j3);
	ProbeCheck(!ok3 && r3 == -1.f, "contrôle NÉGATIF du rayon (tranche vide -> false)",
			   "rend false et ne touche pas ses sorties");
}

// =============================================================================
// ENQUÊTE (NK_FLUID_DIAG=1) — le BALAYAGE qui a choisi epsilon.
// Choisir un PARAMÈTRE n'est pas déplacer un CRITÈRE : les seuils sont écrits en
// tête de ce fichier et ne bougent pas. Ce tableau dit seulement à quel epsilon
// le confinement agit sans que la masse ou la divergence s'effondrent.
// =============================================================================
void EnqueteConfinement() {
	printf("\n=== ENQUÊTE : le BALAYAGE d'epsilon (choix du PARAMÈTRE, pas du CRITÈRE) ===\n");
	printf("    Les seuils du témoin sont écrits en tête de ce fichier et ne bougent pas.\n");
	printf("    Ce tableau dit seulement à quel epsilon le confinement agit SANS que la\n");
	printf("    divergence ni la masse s'effondrent. Deux colonnes d'enstrophie : les 60\n");
	printf("    PREMIERS pas (avant que les deux écoulements ne divergent) et toute la course.\n");
	printf("    eps    ens 60 pas   xA     ens totale   xA     rayon      xA     m tranche   div strict   dérive m   y bary   ms/pas\n");
	const float32 eps[8] = {0.f, 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f, 16.f};
	const uint32 pas = 240;
	ResultatPanache ref;
	for (uint32 c = 0; c < 8; ++c) {
		const ResultatPanache r = CoursePanache(eps[c], pas);
		float32 div = 0.f;
		const float32 dm = DeriveMasseSansSource(eps[c], pas, div);
		if (c == 0)
			ref = r;
		const float32 kd = (ref.enstrophieDebut > 0.f) ? (r.enstrophieDebut / ref.enstrophieDebut) : 0.f;
		const float32 ke = (ref.enstrophieMoy > 0.f) ? (r.enstrophieMoy / ref.enstrophieMoy) : 0.f;
		const float32 kr = (ref.rayon > 0.f) ? (r.rayon / ref.rayon) : 0.f;
		printf("    %5.2f  %10.5f  %5.2f  %10.5f  %5.2f  %8.5f m %5.2f  %9.6f  %9.5f %%  %+7.2f %%  %6.3f  %5.1f\n",
			   (double)eps[c], (double)r.enstrophieDebut, (double)kd, (double)r.enstrophieMoy, (double)ke,
			   (double)r.rayon, (double)kr, (double)r.masseTranche, (double)(div * 100.f), (double)(dm * 100.f),
			   (double)r.hauteurBarycentre, (double)r.msParPas);
		fflush(stdout);
	}
}

// =============================================================================
// LE TÉMOIN — trois courses, A / B / C
// =============================================================================
void PalierVorticite() {
	printf("\n=== ④ CONFINEMENT DE VORTICITÉ — Fedkiw, Stam & Jensen, SIGGRAPH 2001, § 4 ===\n");
	printf("    epsilon = %.1f (choisi par le balayage : NK_FLUID_SWEEP=1) ; seuils PRÉ-ENREGISTRÉS :\n",
		   (double)kEpsilon);
	printf("    (v1) enstrophie B/A >= %.2f · (v2) rayon B/A >= %.2f · (v3) masse : + %.0f points max ·\n",
		   (double)kSeuilEnstrophie, (double)kSeuilRayon, (double)(kMargeMasse * 100.f));
	printf("    (v4) divergence B/A <= %.2f · (v5) MUTATION : enstrophie C/A < 1,00\n", (double)kFacteurDivergence);

	ControlesVorticite();

	const uint32 pas = 360; // 3 s à 1/120
	printf("\n    trois courses de %u pas (3 s) sur la MÊME scène — le solveur est déterministe.\n", pas);
	const ResultatPanache A = CoursePanache(0.f, pas);
	const ResultatPanache B = CoursePanache(kEpsilon, pas);
	const ResultatPanache C = CoursePanache(-kEpsilon, pas);

	printf("    course              enstrophie moy   |omega| moy   rayon 0,60 m   masse tranche   ms/pas\n");
	printf("    A epsilon =  0      %12.6f   %9.4f    %8.5f m   %11.6f   %5.1f\n", (double)A.enstrophieMoy,
		   (double)A.vorticiteMoy, (double)A.rayon, (double)A.masseTranche, (double)A.msParPas);
	printf("    B epsilon = %+.0f     %12.6f   %9.4f    %8.5f m   %11.6f   %5.1f\n", (double)kEpsilon,
		   (double)B.enstrophieMoy, (double)B.vorticiteMoy, (double)B.rayon, (double)B.masseTranche,
		   (double)B.msParPas);
	printf("    C epsilon = %+.0f     %12.6f   %9.4f    %8.5f m   %11.6f   %5.1f   (MUTATION)\n", (double)-kEpsilon,
		   (double)C.enstrophieMoy, (double)C.vorticiteMoy, (double)C.rayon, (double)C.masseTranche,
		   (double)C.msParPas);
	printf("    accélération moyenne réellement ajoutée par le confinement : A %.4f, B %.4f, C %.4f m/s^2\n",
		   (double)A.accelConfinement, (double)B.accelConfinement, (double)C.accelConfinement);
	printf("    barycentre de fumée : A y = %.4f m, B y = %.4f m, C y = %.4f m\n", (double)A.hauteurBarycentre,
		   (double)B.hauteurBarycentre, (double)C.hauteurBarycentre);
	fflush(stdout);

	char buf[500];

	// (v1) enstrophie
	const float32 kEns = (A.enstrophieMoy > 0.f) ? (B.enstrophieMoy / A.enstrophieMoy) : 0.f;
	snprintf(buf, sizeof(buf), "%.6f -> %.6f m^3/s^2, soit x %.2f (seuil x %.2f) — population : les %u cellules "
							   "STRICTES, moyenne sur %u pas",
			 (double)A.enstrophieMoy, (double)B.enstrophieMoy, (double)kEns, (double)kSeuilEnstrophie,
			 A.cellulesStrictes, pas);
	ProbeCheck(kEns >= kSeuilEnstrophie, "(v1) l'enstrophie AUGMENTE d'un facteur nommé", buf);

	// (v2) rayon
	const float32 kRay = (A.rayon > 0.f) ? (B.rayon / A.rayon) : 0.f;
	const float32 kMasse = (A.masseTranche > 0.f) ? (B.masseTranche / A.masseTranche) : 0.f;
	snprintf(buf, sizeof(buf),
			 "à y = %.2f m (rangée %u) : %.5f -> %.5f m, soit x %.2f (seuil x %.2f) ; masse de la tranche x %.2f "
			 "(%.6f -> %.6f) — les deux populations sont dites",
			 (double)kHauteurMesure, B.rangee, (double)A.rayon, (double)B.rayon, (double)kRay, (double)kSeuilRayon,
			 (double)kMasse, (double)A.masseTranche, (double)B.masseTranche);
	ProbeCheck(A.rayonValide && B.rayonValide && kRay >= kSeuilRayon,
			   "(v2) le PANACHE S'ÉLARGIT à hauteur fixée", buf);

	// (v3) et (v4) : n'empire pas
	float32 divA = 0.f, divB = 0.f;
	const float32 mA = DeriveMasseSansSource(0.f, pas, divA);
	const float32 mB = DeriveMasseSansSource(kEpsilon, pas, divB);
	snprintf(buf, sizeof(buf), "sans source, %u pas : dérive A %+.3f %%, dérive B %+.3f %% (B ne doit pas dépasser A "
							   "de plus de %.0f points)",
			 pas, (double)(mA * 100.f), (double)(mB * 100.f), (double)(kMargeMasse * 100.f));
	ProbeCheck(ProbeAbs(mB) <= ProbeAbs(mA) + kMargeMasse, "(v3) la MASSE n'empire pas", buf);

	snprintf(buf, sizeof(buf), "|div|*h/|u| STRICT : A %.5f %%, B %.5f %% (B <= %.2f x A)", (double)(divA * 100.f),
			 (double)(divB * 100.f), (double)kFacteurDivergence);
	ProbeCheck(divB <= kFacteurDivergence * divA, "(v4) la DIVERGENCE n'empire pas", buf);

	// (v5) mutation : la force inversée doit DÉTRUIRE la vorticité
	const float32 kMut = (A.enstrophieMoy > 0.f) ? (C.enstrophieMoy / A.enstrophieMoy) : 1.f;
	snprintf(buf, sizeof(buf), "epsilon = %+.0f : enstrophie %.6f contre %.6f, soit x %.3f (doit être < 1,00 — c'est "
							   "le SIGNE de N x omega qui est éprouvé ici)",
			 (double)-kEpsilon, (double)C.enstrophieMoy, (double)A.enstrophieMoy, (double)kMut);
	ProbeCheck(kMut < 1.f, "MUTATION (v5) : la force INVERSÉE fait chuter l'enstrophie", buf);

	// (v6) LA CONCENTRATION — la grandeur qui ne dépend pas de l'échelle de
	// l'écoulement. V est le volume de la population comptée (l'intérieur STRICT).
	const float32 hh = 0.025f;
	const float32 V = (float32)A.cellulesStrictes * hh * hh * hh;
	auto concentration = [&](const ResultatPanache &r) {
		if (V <= 0.f || r.vorticiteMoy <= 0.f)
			return 0.f;
		return NkSqrt(r.enstrophieMoy / V) / r.vorticiteMoy;
	};
	const float32 pA = concentration(A), pB = concentration(B), pC = concentration(C);
	snprintf(buf, sizeof(buf),
			 "P = rms(|omega|)/moy(|omega|) sur %u cellules STRICTES (V = %.4f m^3) : A %.3f, B %.3f (x %.3f, seuil "
			 "1,050), C %.3f (x %.3f)",
			 A.cellulesStrictes, (double)V, (double)pA, (double)pB, (double)(pA > 0.f ? pB / pA : 0.f), (double)pC,
			 (double)(pA > 0.f ? pC / pA : 0.f));
	ProbeCheck(pA > 0.f && pB >= 1.05f * pA, "(v6) le confinement CONCENTRE la vorticité", buf);

	snprintf(buf, sizeof(buf), "P : A %.3f -> C %.3f, soit x %.3f (doit être <= 0,700 : la force inversée ÉTALE la "
							   "vorticité au lieu de la rassembler)",
			 (double)pA, (double)pC, (double)(pA > 0.f ? pC / pA : 0.f));
	ProbeCheck(pA > 0.f && pC <= 0.70f * pA, "MUTATION (v6b) : la force inversée ÉTALE la vorticité", buf);

	// LA CAUSE DE (v4), MESURÉE ET NON SUPPOSÉE. Le 05/09 on avait prouvé que la
	// divergence résiduelle de 0,39 % ne vient PAS du solveur : forcé 78 fois plus
	// loin, le rapport n'avait pas bougé. Si le confinement fait monter ce rapport
	// pour la MÊME raison — l'incompatibilité entre le Laplacien de pas 1 que la
	// projection résout et le Laplacien de pas 2 que la divergence centrée voit —
	// alors la même expérience doit rendre le même verdict AVEC le confinement :
	// le résidu s'effondre, le rapport ne bouge pas. Si au contraire le rapport
	// descendait, ce serait le solveur qui n'arrive plus à suivre, et le remède
	// serait tout autre (plus de balayages, pas une autre grille).
	// C'est la contre-épreuve de la conclusion, pas sa confirmation.
	PlancherDuSolveur(kEpsilon);
}
