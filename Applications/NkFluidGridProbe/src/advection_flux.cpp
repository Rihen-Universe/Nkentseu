// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// advection_flux.cpp — (f1) LE CONTRÔLE NÉGATIF DE L'ADVECTION CONSERVATIVE.
//
// Pré-enregistrement complet : PLAN_ADVECTION_FLUX.md, à côté du solveur, écrit
// et commité AVANT la première ligne de code du schéma.
//
// ⚠️ (f1) EST ÉCRIT AVANT LE SCHÉMA, ET IL EST ROUGE AUJOURD'HUI. C'est sa raison
// d'être. L'advection semi-lagrangienne n'a AUCUN bilan : son interpolation
// trilinéaire redistribue la densité sans jamais vérifier que ce qui part d'une
// cellule arrive dans une autre. Elle perd des POURCENTS. Un schéma en FLUX, lui,
// est conservatif PAR CONSTRUCTION — on calcule UN flux par face, on le retranche
// à une cellule et on l'ajoute à l'autre — donc (f1) doit devenir VERT au premier
// jet, ou le portage est faux.
//
// ── LE MONTAGE, et pourquoi il ne se négocie pas ────────────────────────────
// Le champ de vitesse n'est pas « à peu près » à divergence nulle : il est DÉRIVÉ
// D'UNE FONCTION DE COURANT posée sur les ARÊTES (les coins du maillage), ce qui
// rend la divergence MAC identiquement nulle — les quatre termes se télescopent :
//
//     u(i,j) = -( psi(i,  j+1) - psi(i,j) ) / h      (sur la face x, en (i-1)h ; y au centre)
//     v(i,j) = +( psi(i+1,j  ) - psi(i,j) ) / h      (sur la face y, en (j-1)h ; x au centre)
//
//     div*h = (u(i+1,j)-u(i,j)) + (v(i,j+1)-v(i,j))
//           = [ -psi(i+1,j+1)+psi(i+1,j)+psi(i,j+1)-psi(i,j)
//               +psi(i+1,j+1)-psi(i,j+1)-psi(i+1,j)+psi(i,j) ] / h   ==  0
//
// avec psi = sin^2(pi x/L) * sin^2(pi y/L), NULLE sur tout le bord. Conséquence
// gratuite et essentielle : u.n = 0 aux parois **sans avoir à l'imposer**, donc
// rien ne peut sortir du domaine — et une masse qui varierait ne pourrait
// s'expliquer que par le schéma.
//
// ⚠️ CE QUE LA CONSERVATION EXIGE VRAIMENT, dit pour ne pas se tromper de preuve :
// un schéma en flux conserve la masse quelle que soit la divergence — cela vient
// du bilan par face, pas de l'incompressibilité. La divergence nulle sert à autre
// chose : elle garantit qu'aucune COMPRESSION parasite ne vient masquer une perte
// par une création. Les deux propriétés sont distinctes et on ne les confond pas.
//
// ── CE QUE (f1) NE PROUVE PAS ───────────────────────────────────────────────
// Rien sur le PRIX du schéma (c'est (f2) : enstrophie, contraste, transport),
// rien sur sa STABILITÉ (c'est (f3) : le semi-lagrangien est inconditionnellement
// stable, le flux ne l'est pas), rien sur la vitesse — qui reste semi-lagrangienne
// et que ce lot ne touche pas.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGrid.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);

// Amplitude du tourbillon (m^2/s) : le gradient de psi vaut ~2 pi / L, donc cette
// constante fixe |u| autour de 0,3 m/s — bien en deçà de toute limite CFL, pour
// que (f1) juge la CONSERVATION et non la stabilité, qui est la question de (f3).
static const float32 kAmplitude = 0.02f;
// Seuil de (f1), fixé dans le plan AVANT la mesure : ce n'est pas une tolérance de
// confort, c'est le bruit de la simple précision. Chaque flux apparaît deux fois
// avec des signes opposés, mais la somme de 200 pas x N cellules en float32 porte
// son propre arrondi. Le semi-lagrangien, lui, perd des POURCENTS.
static const float32 kSeuilDerive = 1.0e-5f;

// =============================================================================
// Le champ dérivé de la fonction de courant, posé JUSQUE SUR LES FANTÔMES.
// =============================================================================
static void PoserChampSansDivergence(NkFluidGrid &g, const NkFluidGridParams &p) {
	const float32 h = g.CellSize();
	const float32 Lx = p.boundsMax.x - p.boundsMin.x;
	const float32 Ly = p.boundsMax.y - p.boundsMin.y;
	const float32 PI = 3.14159265358979f;

	// psi au COIN (i,j) : x = boundsMin.x + (i-1)h, y = boundsMin.y + (j-1)h.
	// C'est exactement l'abscisse de la face x d'indice i et l'ordonnée de la face
	// y d'indice j — c'est ce qui fait que les différences tombent au bon endroit.
	auto psi = [&](uint32 i, uint32 j) {
		const float32 x = ((float32)i - 1.f) * h / Lx;
		const float32 y = ((float32)j - 1.f) * h / Ly;
		const float32 sx = NkSin(PI * x), sy = NkSin(PI * y);
		return kAmplitude * sx * sx * sy * sy;
	};

	float32 *u = const_cast<float32 *>(g.VelocityX());
	float32 *v = const_cast<float32 *>(g.VelocityY());
	float32 *w = const_cast<float32 *>(g.VelocityZ());
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i) {
				const uint32 id = g.Idx(i, j, k);
				u[id] = -(psi(i, j + 1) - psi(i, j)) / h;
				v[id] = (psi(i + 1, j) - psi(i, j)) / h;
				w[id] = 0.f;
			}
}

// =============================================================================
// (f1) — la masse se conserve-t-elle à l'epsilon machine ?
// =============================================================================
static void ControleConservation() {
	printf("\n--- (f1) CONTRÔLE NÉGATIF : divergence nulle, parois fermées, aucune source ---\n");
	printf("    Champ dérivé d'une fonction de courant : divergence MAC identiquement nulle,\n");
	printf("    et u.n = 0 aux parois SANS l'imposer. Rien ne peut sortir du domaine : une\n");
	printf("    masse qui varie ne peut venir que du SCHÉMA.\n");

	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {0.4f, 0.4f, 0.4f};
	p.cellSize = 0.02f; // 20 x 20 x 20
	p.projectionEnabled = false;	// le champ est DÉJÀ à divergence nulle
	p.buoyancyEnabled = false;		// aucune force : le champ ne doit pas bouger
	p.densityDissipation = 0.f;		// aucune disparition légitime
	p.temperatureDissipation = 0.f; // sinon on mesurerait la dissipation

	NkFluidGrid g;
	if (!g.Init(p)) {
		ProbeCheck(false, "(f1) Init de la grille", "Init a rendu false");
		return;
	}
	char buf[460];

	// ── GARDE DU MONTAGE : le champ POSÉ est-il vraiment à divergence nulle ? ──
	// « Par construction » est une AFFIRMATION. Tant qu'elle n'est pas mesurée,
	// (f1) tout entier reposerait sur une croyance — et si le champ n'était pas à
	// divergence nulle, une perte de masse pourrait être compensée par une
	// compression, ou l'inverse. On la mesure donc sur une grille INERTE (advection
	// ET projection coupées), où seule la MESURE travaille.
	{
		NkFluidGridParams q = p;
		q.advectionEnabled = false;
		NkFluidGrid gd;
		if (gd.Init(q)) {
			PoserChampSansDivergence(gd, q);
			gd.Step(1.0e-6f);
			snprintf(buf, sizeof(buf),
					 "champ POSÉ, grille inerte : |div|*h moyen %.3e m/s, max %.3e m/s sur %u cellules "
					 "STRICTES — la fonction de courant rend la divergence MAC identiquement nulle, et "
					 "c'est ici MESURÉ au lieu d'être affirmé",
					 (double)gd.Stats().divAfterMeanStrict, (double)gd.Stats().divAfterMaxStrict,
					 gd.Stats().cellsStrict);
			ProbeCheck(gd.Stats().divAfterMaxStrict < 1.0e-6f, "GARDE de (f1) : le champ POSÉ est à divergence nulle",
					   buf);
		}
	}

	// Une bulle posée UNE fois, au centre. Aucune source ensuite : « aucune
	// source » est une condition du contrôle, pas un détail de montage.
	g.EmitSphere({0.2f, 0.2f, 0.2f}, 0.06f, 1.f, 0.f, 0.f);
	const float32 masse0 = g.TotalMass();

	const float32 dt = 1.f / 120.f;
	const uint32 pas = 200;

	// ── COURSE 1 : LE SEMI-LAGRANGIEN, publié comme RÉFÉRENCE ───────────────
	// Elle ne rend AUCUN verdict : elle dit d'où l'on part. Et elle tourne dans la
	// MÊME course du banc que le verdict — un chiffre et ce à quoi on le compare ne
	// valent que s'ils viennent du même endroit.
	float32 divS = 0.f, paroiS = 0.f;
	for (uint32 s = 0; s < pas; ++s) {
		// Le champ est RÉ-IMPOSÉ à chaque pas : l'advection de la vitesse le
		// perturberait sinon, et on mesurerait sa dérive au lieu de mesurer le
		// bilan de masse. (Les parois restent étanches dans tous les cas.)
		PoserChampSansDivergence(g, p);
		g.Step(dt);
		if (g.Stats().divAfterMean > divS)
			divS = g.Stats().divAfterMean;
		const float32 paroi = g.WallLayerMass();
		if (paroi > paroiS)
			paroiS = paroi;
	}
	const float32 deriveSemi = (masse0 > 0.f) ? ProbeAbs(g.TotalMass() - masse0) / masse0 : 1.f;

	// ── COURSE 2 : LE SCHÉMA EN FLUX — c'est LUI qui est jugé ───────────────
	NkFluidGridParams q = p;
	q.advectFluxConservative = true;
	NkFluidGrid gf;
	if (!gf.Init(q)) {
		ProbeCheck(false, "(f1) Init de la grille en flux", "Init a rendu false");
		return;
	}
	gf.EmitSphere({0.2f, 0.2f, 0.2f}, 0.06f, 1.f, 0.f, 0.f);
	const float32 masseF0 = gf.TotalMass();

	// On garde le champ INITIAL : sans lui, impossible de savoir si le champ a
	// seulement BOUGÉ — et une masse conservée parce que RIEN n'a été transporté
	// serait le pire des verts (voir la garde (f1b) plus bas).
	const uint32 totalF = gf.Stats().cellsTotal;
	NkVector<float32> dInit;
	dInit.Resize(totalF, 0.f);
	{
		const float32 *d0 = gf.Density();
		for (uint32 i = 0; i < totalF; ++i)
			dInit[i] = d0[i];
	}

	float32 divF = 0.f, paroiF = 0.f, cflF = 0.f;
	uint32 sousPasF = 0;
	bool capF = false;
	for (uint32 s = 0; s < pas; ++s) {
		PoserChampSansDivergence(gf, q);
		gf.Step(dt);
		if (gf.Stats().divAfterMean > divF)
			divF = gf.Stats().divAfterMean;
		const float32 paroi = gf.WallLayerMass();
		if (paroi > paroiF)
			paroiF = paroi;
		if (gf.Stats().advectCFL > cflF)
			cflF = gf.Stats().advectCFL;
		if (gf.Stats().advectSubsteps > sousPasF)
			sousPasF = gf.Stats().advectSubsteps;
		if (gf.Stats().advectSubstepCapHit)
			capF = true;
	}
	const float32 masseF1 = gf.TotalMass();
	const float32 deriveFlux = (masseF0 > 0.f) ? ProbeAbs(masseF1 - masseF0) / masseF0 : 1.f;

	// ── (f1b) GARDE : LE CHAMP A-T-IL SEULEMENT BOUGÉ ? ─────────────────────
	// ⚠️ Sans elle, (f1) serait VERT pour la pire des raisons : une masse conservée
	// parce que RIEN n'a été transporté. C'est le piège du cas Z du banc — « vitesse
	// nulle -> masse conservée » ne prouve que « rien ne bouge ». Et ce montage-ci
	// l'aggrave : le centre du tourbillon est un point STATIONNAIRE de la fonction
	// de courant, et c'est justement là que la bulle est posée.
	// On mesure donc la variation L1 du champ, rapportée à la masse : la part de la
	// masse qui a CHANGÉ DE PLACE. Si elle est nulle, (f1) est HORS SUJET.
	{
		float64 l1 = 0.0;
		const float32 *dfin = gf.Density();
		for (uint32 k = 1; k <= gf.Nz(); ++k)
			for (uint32 j = 1; j <= gf.Ny(); ++j)
				for (uint32 i = 1; i <= gf.Nx(); ++i) {
					const uint32 id = gf.Idx(i, j, k);
					l1 += (float64)ProbeAbs(dfin[id] - dInit[id]);
				}
		const float32 h3 = gf.CellSize() * gf.CellSize() * gf.CellSize();
		const float32 bouge = (masseF0 > 0.f) ? (float32)(l1 * (float64)h3) / masseF0 : 0.f;
		snprintf(buf, sizeof(buf),
				 "variation L1 du champ = %.2f %% de la masse initiale (exigé > 5 %%) — sans cette garde, "
				 "« la masse se conserve » serait VRAI d'un schéma qui ne transporte RIEN, et (f1) serait "
				 "vert pour la pire des raisons",
				 (double)(bouge * 100.f));
		ProbeCheck(bouge > 0.05f, "(f1b) GARDE : le champ a réellement été TRANSPORTÉ", buf);
	}

	printf("    RÉFÉRENCE (aucun verdict) — semi-lagrangien sur le MÊME montage : dérive %.3e ;\n"
		   "    |div|*h max %.3e m/s ; masse paroi %.3e\n",
		   (double)deriveSemi, (double)divS, (double)paroiS);

	snprintf(buf, sizeof(buf),
			 "masse %.9f -> %.9f, dérive relative %.3e sur %u pas (seuil %.0e) ; CFL max %.3f, %u sous-pas%s ; "
			 "|div|*h max APRÈS advection de la vitesse %.3e m/s (le champ POSÉ est à divergence nulle — "
			 "c'est la GARDE ci-dessus) ; masse paroi %.3e ; le SEMI-LAGRANGIEN, lui, dérive de %.3e",
			 (double)masseF0, (double)masseF1, (double)deriveFlux, pas, (double)kSeuilDerive, (double)cflF,
			 sousPasF, capF ? " (BORNE ATTEINTE)" : "", (double)divF, (double)paroiF, (double)deriveSemi);
	ProbeCheck(deriveFlux < kSeuilDerive, "(f1) la masse se conserve à l'epsilon machine", buf);
}

// =============================================================================
// (f2) LE PRIX — pré-enregistré dans PLAN_ADVECTION_FLUX.md, § 2, AVANT mesure.
//
// ⚠️ LA QUESTION N'EST PAS « la masse se conserve-t-elle ». Elle se conservera :
// c'est une propriété du schéma, et (f1) l'a déjà prouvée. La question est À QUEL
// PRIX. Conserver la masse d'une fumée qui ne tourbillonne plus n'a AUCUNE valeur.
//
// ⚠️ ET ON MESURE AVEC LE SOUS-CYCLAGE QUE LA COURSE (e) RÉCLAME. Mesurer à un dt
// confortable sans sous-cyclage, puis l'activer pour (e), reviendrait à mesurer
// DEUX SCHÉMAS DIFFÉRENTS : le sous-cyclage change la diffusion effective.
//
// ── PRÉDICTION CALCULÉE DEPUIS LE MÉCANISME, écrite avant la course ─────────
// La diffusion numérique du donor-cell d'ordre 1 vaut D = (u·h/2)(1 − CFL), et la
// largeur ajoutée après un temps T vaut sigma^2 = 2·D·T.
// Sur la scène du panache (u ~ 0,5 m/s, h = 0,025 m, T = 3 s, CFL ~ 0,16) :
//     D      ~ 0,00525 m^2/s
//     sigma  ~ racine(2 × 0,00525 × 3) ~ 0,177 m
// à comparer au rayon actuel du panache, 0,03453 m. J'attends donc :
//   - un rayon ~ racine(0,0345^2 + 0,177^2) ~ 0,18 m, soit x 5,2 ;
//   - une enstrophie DIVISÉE PAR ~25 (elle va comme le CARRÉ du gradient de
//     température, lissé d'un facteur ~5), soit ~0,8 contre un plancher de 9,6 ;
//   - la fumée qui TOUCHE LES PAROIS, donc la garde de (a) qui rougit — sans que
//     la masse sorte, puisque les faces de paroi portent u = 0.
// AUTREMENT DIT : JE PRÉDIS QUE LE PREMIER ORDRE NU ÉCHOUE (f2). Si la mesure me
// contredit, tant mieux, et c'est elle qui gagne.
// =============================================================================
struct ResultatPrix {
		float32 enstrophieMoy = 0.f, vorticiteMoy = 0.f, concentration = 0.f;
		float32 rayon = 0.f, masseTranche = 0.f, hauteurBary = 0.f;
		float32 deriveMasse = 0.f, paroiMax = 0.f;
		float32 cflMax = 0.f, msParPas = 0.f, vmax = 0.f, tmax = 0.f;
		uint32 sousPasMax = 1, nan = 0, cellulesStrictes = 0;
		bool capHit = false, rayonValide = false;
};

// Les compteurs communs à toutes les scènes, relevés à chaque pas.
static void RelevePas(const NkFluidGrid &g, ResultatPrix &r, float64 &ensSum, float64 &vortSum, float64 &msSum) {
	ensSum += (float64)g.Stats().enstrophy;
	vortSum += (float64)g.Stats().vorticityMean;
	msSum += (float64)g.Stats().ms;
	if (g.Stats().advectCFL > r.cflMax)
		r.cflMax = g.Stats().advectCFL;
	if (g.Stats().advectSubsteps > r.sousPasMax)
		r.sousPasMax = g.Stats().advectSubsteps;
	if (g.Stats().advectSubstepCapHit)
		r.capHit = true;
	if (g.Stats().maxSpeed > r.vmax)
		r.vmax = g.Stats().maxSpeed;
	if (g.Stats().maxTemperature > r.tmax)
		r.tmax = g.Stats().maxTemperature;
	r.nan += g.Stats().nanCount;
}

// ── SCÈNE P : le panache établi. C'est ICI que le détail se paie. ───────────
// Montage identique à la course A du palier ④ (source continue, epsilon = 0),
// pour que les chiffres soient comparables aux références du 12/09.
static ResultatPrix ScenePanache(bool flux, uint32 pas) {
	ResultatPrix r;
	NkFluidGridParams p;
	p.boundsMin = {-0.35f, 0.f, -0.35f};
	p.boundsMax = {0.35f, 1.4f, 0.35f};
	p.cellSize = 0.025f;
	p.pressureTolerance = 1.0e-4f;
	p.pressureIterations = 400;
	p.densityDissipation = 0.f;
	p.temperatureDissipation = 0.f;
	p.advectFluxConservative = flux;
	NkFluidGrid g;
	if (!g.Init(p))
		return r;
	const float32 dt = 1.f / 120.f;
	float64 ensSum = 0.0, vortSum = 0.0, msSum = 0.0;
	for (uint32 s = 0; s < pas; ++s) {
		g.EmitSphere({0.f, 0.06f, 0.f}, 0.05f, 4.f * dt, 1400.f * dt, 0.f);
		g.Step(dt);
		RelevePas(g, r, ensSum, vortSum, msSum);
	}
	r.enstrophieMoy = (float32)(ensSum / (float64)pas);
	r.vorticiteMoy = (float32)(vortSum / (float64)pas);
	r.msParPas = (float32)(msSum / (float64)pas);
	r.cellulesStrictes = g.Stats().cellsStrict;
	uint32 rangee = 0;
	r.rayonValide = g.PlumeRadius(0.60f, r.rayon, r.masseTranche, rangee);
	NkVec3f c;
	if (g.DensityCentroid(c))
		r.hauteurBary = c.y;
	// P = rms(|omega|) / moy(|omega|) : la CONCENTRATION, qui ne dépend pas de
	// l'échelle de l'écoulement — la seule grandeur comparable entre deux schémas
	// qui ne produisent pas le même panache.
	const float32 V = (float32)r.cellulesStrictes * 0.025f * 0.025f * 0.025f;
	if (V > 0.f && r.vorticiteMoy > 0.f)
		r.concentration = NkSqrt(r.enstrophieMoy / V) / r.vorticiteMoy;
	return r;
}

// ── SCÈNE A : la MASSE, le but du lot. Bulle posée, aucune source. ──────────
static ResultatPrix SceneMasse(bool flux) {
	ResultatPrix r;
	NkFluidGridParams p;
	p.boundsMin = {-0.3f, 0.f, -0.3f};
	p.boundsMax = {0.3f, 1.8f, 0.3f};
	p.cellSize = 0.03f;
	p.densityDissipation = 0.f;
	p.temperatureDissipation = 0.f;
	p.pressureIterations = 600;
	p.pressureTolerance = 1.0e-5f;
	p.advectFluxConservative = flux;
	NkFluidGrid g;
	if (!g.Init(p))
		return r;
	g.EmitSphere({0.f, 0.15f, 0.f}, 0.07f, 1.f, 150.f, 0.f);
	const float32 m0 = g.TotalMass();
	const float32 dt = 1.f / 120.f;
	float64 ensSum = 0.0, vortSum = 0.0, msSum = 0.0;
	for (uint32 s = 0; s < 500; ++s) {
		g.Step(dt);
		RelevePas(g, r, ensSum, vortSum, msSum);
		const float32 paroi = g.WallLayerMass();
		if (paroi > r.paroiMax)
			r.paroiMax = paroi;
	}
	r.msParPas = (float32)(msSum / 500.0);
	r.deriveMasse = (m0 > 0.f) ? (g.TotalMass() - m0) / m0 : 1.f;
	return r;
}

// ── SCÈNE E : celle de (e) — LA SEULE où le sous-cyclage MORD vraiment ──────
// vmax y monte à 7,782 m/s : à dt = 1/60 et h = 0,02, CFL vaut ~6,5.
static ResultatPrix SceneDixSecondes(bool flux) {
	ResultatPrix r;
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, 0.f, -0.25f};
	p.boundsMax = {0.25f, 1.f, 0.25f};
	p.cellSize = 0.02f;
	p.densityDissipation = 0.2f;
	p.temperatureDissipation = 0.5f;
	p.buoyancyAlpha = 0.3f;
	p.advectFluxConservative = flux;
	NkFluidGrid g;
	if (!g.Init(p))
		return r;
	const float32 dt = 1.f / 60.f;
	float64 ensSum = 0.0, vortSum = 0.0, msSum = 0.0;
	for (uint32 s = 0; s < 600; ++s) {
		g.EmitSphere({0.f, 0.05f, 0.f}, 0.05f, 6.f * dt, 900.f * dt, 0.f);
		g.Step(dt);
		RelevePas(g, r, ensSum, vortSum, msSum);
	}
	r.enstrophieMoy = (float32)(ensSum / 600.0);
	r.msParPas = (float32)(msSum / 600.0);
	NkVec3f c;
	if (g.DensityCentroid(c))
		r.hauteurBary = c.y;
	return r;
}

// =============================================================================
// (f3) LA STABILITÉ — elle CHANGE DE NATURE, et il faut la nommer.
//
// Le semi-lagrangien de Stam est INCONDITIONNELLEMENT stable : c'est sa raison
// d'être, et c'est ce que ce lot abandonne. Un flux explicite ne l'est pas.
//
// ⚠️ DEUX PIÈGES DE MESURE, tous deux évités ici et dits plutôt que contournés :
//
//  1. AVEC LE SOUS-CYCLAGE ACTIF, LE SCHÉMA NE CASSE JAMAIS — il subdivise.
//     Mesurer « à quel dt ça casse » avec le filet en place reviendrait à mesurer
//     LE FILET, pas le schéma. On coupe donc le filet (advectMaxSubsteps = 1)
//     pour mesurer la condition NUE, puis on vérifie SÉPARÉMENT que le filet la
//     respecte — c'est (f3b).
//
//  2. LA MASSE NE PEUT PAS SERVIR DE DÉTECTEUR. Le donor-cell est conservatif
//     MÊME quand il est instable : un flux retranché ici est ajouté là, que le
//     schéma oscille ou non. La masse resterait donc parfaitement conservée
//     pendant que le champ explose. Le bon détecteur est la DENSITÉ NÉGATIVE :
//     le décentrement amont est MONOTONE tant que CFL <= 1, et produit des
//     sous-dépassements au-delà. C'est net, précoce, et analytique.
//
// PRÉDICTION CALCULÉE, écrite avant la course — et cette fois la grandeur prédite
// EST celle que la théorie donne, ce qui n'était pas le cas en (f2) :
//     CFL = (|u| + |v| + |w|) * dt / h  <=  1
// J'attends donc un dernier dt sain juste SOUS CFL = 1, et un premier dt cassé
// juste AU-DESSUS.
// =============================================================================
struct EtatStabilite {
		float32 cflMax = 0.f, dMin = 0.f, dMax = 0.f;
		uint32 nan = 0, sousPasMax = 1;
		bool casse = false;
};

static EtatStabilite CourseStabilite(float32 dt, uint32 maxSousPas, uint32 pas) {
	EtatStabilite e;
	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {0.4f, 0.4f, 0.4f};
	p.cellSize = 0.02f;
	p.projectionEnabled = false;
	p.buoyancyEnabled = false;
	p.densityDissipation = 0.f;
	p.temperatureDissipation = 0.f;
	p.advectFluxConservative = true;
	p.advectMaxSubsteps = maxSousPas; // 1 = FILET COUPÉ
	NkFluidGrid g;
	if (!g.Init(p))
		return e;
	g.EmitSphere({0.2f, 0.2f, 0.2f}, 0.06f, 1.f, 0.f, 0.f);

	for (uint32 s = 0; s < pas; ++s) {
		PoserChampSansDivergence(g, p);
		g.Step(dt);
		if (g.Stats().advectCFL > e.cflMax)
			e.cflMax = g.Stats().advectCFL;
		if (g.Stats().advectSubsteps > e.sousPasMax)
			e.sousPasMax = g.Stats().advectSubsteps;
		e.nan += g.Stats().nanCount;
	}
	// Densités extrêmes sur l'INTÉRIEUR : c'est le sous-dépassement qui signale
	// la perte de monotonie, bien avant que des NaN n'apparaissent.
	const float32 *d = g.Density();
	e.dMin = 1.0e30f;
	e.dMax = -1.0e30f;
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i) {
				const float32 v = d[g.Idx(i, j, k)];
				if (v < e.dMin)
					e.dMin = v;
				if (v > e.dMax)
					e.dMax = v;
			}
	e.casse = (e.nan > 0) || (e.dMin < -1.0e-6f) || (e.dMax > 10.f);
	return e;
}

void EnqueteStabilite() {
	printf("\n=== (f3) LA STABILITÉ — le schéma CHANGE DE NATURE, et on la NOMME ===\n");
	printf("    Le semi-lagrangien est INCONDITIONNELLEMENT stable ; le flux ne l'est pas.\n");
	printf("    ⚠️ Le FILET est COUPÉ ici (advectMaxSubsteps = 1) : avec le sous-cyclage, le\n");
	printf("    schéma ne casse JAMAIS, et on mesurerait le filet au lieu du schéma.\n");
	printf("    ⚠️ Le détecteur n'est PAS la masse : le donor-cell est conservatif MÊME\n");
	printf("    instable. C'est la DENSITÉ NÉGATIVE qui trahit la perte de monotonie.\n");
	printf("    PRÉDICTION CALCULÉE : rupture au voisinage de CFL = 1.\n");
	printf("      dt (s)      CFL max    densite min    densite max   NaN   verdict\n");

	// Le balayage est RAFFINE AUTOUR DE CFL = 1, et c'est une correction de MONTAGE
	// faite AVANT la mesure, pas un critere deplace : sur ce montage (f1) donnait
	// CFL = 0,092 a dt = 1/120, donc CFL vaut environ 11 * dt. Un balayage qui
	// sauterait de 1/15 (CFL ~ 0,74) a 1/10 (CFL ~ 1,10) n'echantillonnerait RIEN
	// autour de 1, et le verdict dependrait de la RESOLUTION du balayage au lieu de
	// la physique. Les deux derniers pas montent franchement au-dessus pour garantir
	// qu'une rupture soit atteinte -- sans quoi (f3) ne prouverait rien.
	const float32 dts[13] = {1.f / 120.f, 1.f / 60.f, 1.f / 40.f, 1.f / 30.f, 1.f / 25.f,
							 1.f / 20.f,  1.f / 16.f, 1.f / 14.f, 1.f / 12.f, 1.f / 11.f,
							 1.f / 10.f,  1.f / 8.f,  1.f / 6.f};
	float32 dernierSain = 0.f, premierCasse = 0.f;
	float32 dtDernierSain = 0.f, dtPremierCasse = 0.f;
	for (uint32 c = 0; c < 13; ++c) {
		const EtatStabilite e = CourseStabilite(dts[c], 1u, 100u);
		printf("      1/%-6.0f  %8.3f   %+.3e    %+.3e   %3u   %s\n", (double)(1.f / dts[c]), (double)e.cflMax,
			   (double)e.dMin, (double)e.dMax, e.nan, e.casse ? "CASSE" : "sain");
		fflush(stdout);
		if (!e.casse) {
			dernierSain = e.cflMax;
			dtDernierSain = dts[c];
		} else if (premierCasse == 0.f) {
			premierCasse = e.cflMax;
			dtPremierCasse = dts[c];
		}
	}

	char buf[520];
	snprintf(buf, sizeof(buf),
			 "dernier dt SAIN : 1/%.0f s a CFL %.3f ; premier dt CASSE : 1/%.0f s a CFL %.3f. La condition "
			 "attendue est CFL <= 1, et la rupture tombe bien dans son voisinage — NOMMEE, c'est une "
			 "propriete connue du schema ; TUE, c'eut ete une regression de robustesse",
			 (double)(dtDernierSain > 0.f ? 1.f / dtDernierSain : 0.f), (double)dernierSain,
			 (double)(dtPremierCasse > 0.f ? 1.f / dtPremierCasse : 0.f), (double)premierCasse);
	ProbeCheck(dernierSain >= 0.8f && premierCasse > 0.f && premierCasse <= 2.5f,
			   "(f3) la rupture tombe au VOISINAGE de CFL = 1", buf);

	// ── (f3b) LE FILET : le MÊME dt, avec le sous-cyclage ───────────────────
	// Sans ce contrôle, (f3) dirait « ça casse » sans jamais prouver que la parade
	// fonctionne. C'est lui qui justifie le sous-cyclage.
	if (dtPremierCasse > 0.f) {
		const EtatStabilite avecFilet = CourseStabilite(dtPremierCasse, 64u, 100u);
		snprintf(buf, sizeof(buf),
				 "au MEME dt (1/%.0f s) qui cassait sans filet : CFL %.3f decoupe en %u sous-pas, densite "
				 "min %+.3e, %u NaN -> %s. Le sous-cyclage n'est donc pas un contournement : c'est la "
				 "parade, et elle est MESUREE",
				 (double)(1.f / dtPremierCasse), (double)avecFilet.cflMax, avecFilet.sousPasMax,
				 (double)avecFilet.dMin, avecFilet.nan, avecFilet.casse ? "CASSE ENCORE" : "SAIN");
		ProbeCheck(!avecFilet.casse, "(f3b) LE FILET tient : le sous-cyclage rattrape le dt qui cassait", buf);
	} else {
		ProbeCheck(false, "(f3b) LE FILET n'a pas pu etre eprouve",
				   "aucun dt du balayage n'a casse : le balayage ne monte pas assez haut, et (f3) ne "
				   "prouve donc PAS que le schema est conditionnellement stable");
	}
}

void EnqueteLePrix() {
	printf("\n=== (f2) LE PRIX DU DONOR-CELL NU — planchers PRÉ-ENREGISTRÉS (§ 2 du plan) ===\n");
	printf("    PRÉDICTION CALCULÉE depuis le mécanisme, écrite AVANT la course :\n");
	printf("      D = (u·h/2)(1-CFL) ~ 0,00525 m^2/s  ->  sigma = rac(2·D·T) ~ 0,177 m sur 3 s\n");
	printf("      => rayon attendu ~ 0,18 m (x 5,2) et enstrophie divisée par ~25, soit ~0,8\n");
	printf("      contre un plancher de 9,6. JE PRÉDIS DONC QUE LE PREMIER ORDRE NU ÉCHOUE.\n");
	printf("    Le semi-lagrangien tourne dans la MÊME course, comme référence.\n");
	char buf[520];

	// ── LE BUT : la masse ───────────────────────────────────────────────────
	printf("\n--- (f2) LE BUT : la masse ---\n");
	const ResultatPrix mS = SceneMasse(false);
	const ResultatPrix mF = SceneMasse(true);
	printf("      schéma            dérive de masse   masse paroi max   CFL    sous-pas   ms/pas\n");
	printf("      semi-lagrangien   %+9.4f %%        %.3e        %.3f  %5u      %6.1f\n",
		   (double)(mS.deriveMasse * 100.f), (double)mS.paroiMax, (double)mS.cflMax, mS.sousPasMax,
		   (double)mS.msParPas);
	printf("      FLUX (donor-cell) %+9.4f %%        %.3e        %.3f  %5u      %6.1f%s\n",
		   (double)(mF.deriveMasse * 100.f), (double)mF.paroiMax, (double)mF.cflMax, mF.sousPasMax,
		   (double)mF.msParPas, mF.capHit ? "  (BORNE)" : "");
	snprintf(buf, sizeof(buf),
			 "dérive %+.4f %% contre %+.4f %% pour le semi-lagrangien, sur la MÊME scène et dans la MÊME "
			 "course (critère < 1 %%, seuil NON déplacé)",
			 (double)(mF.deriveMasse * 100.f), (double)(mS.deriveMasse * 100.f));
	ProbeCheck(ProbeAbs(mF.deriveMasse) < 0.01f, "(f2) LE BUT : la masse est enfin conservée", buf);

	// ── LE PRIX : l'enstrophie et le détail ─────────────────────────────────
	printf("\n--- (f2) LE PRIX : l'enstrophie et le détail de la fumée ---\n");
	const ResultatPrix pS = ScenePanache(false, 360);
	const ResultatPrix pF = ScenePanache(true, 360);
	printf("      schéma            enstrophie   |omega| moy   P (concentr.)   rayon 0,60 m   y bary   ms/pas\n");
	printf("      semi-lagrangien   %10.6f   %9.4f    %9.3f     %8.5f m   %6.3f   %6.1f\n",
		   (double)pS.enstrophieMoy, (double)pS.vorticiteMoy, (double)pS.concentration, (double)pS.rayon,
		   (double)pS.hauteurBary, (double)pS.msParPas);
	printf("      FLUX (donor-cell) %10.6f   %9.4f    %9.3f     %8.5f m   %6.3f   %6.1f\n",
		   (double)pF.enstrophieMoy, (double)pF.vorticiteMoy, (double)pF.concentration, (double)pF.rayon,
		   (double)pF.hauteurBary, (double)pF.msParPas);

	snprintf(buf, sizeof(buf),
			 "enstrophie du PANACHE %.6f contre %.6f au semi-lagrangien (plancher 9,600 = perte de 50 %% "
			 "max, pré-enregistré). ⚠️ Ce plancher ne porte PAS sur l'ancrage 1,458000, qui mesure rot(u) "
			 "sur une rotation solide SANS aucun scalaire et doit rester inchangé",
			 (double)pF.enstrophieMoy, (double)pS.enstrophieMoy);
	ProbeCheck(pF.enstrophieMoy >= 9.6f, "(f2) le détail SURVIT : enstrophie du panache >= 9,6", buf);

	snprintf(buf, sizeof(buf),
			 "P = rms(|omega|)/moy(|omega|) = %.3f contre %.3f (plancher 2,000). P ne dépend pas de "
			 "l'échelle de l'écoulement : c'est la seule grandeur comparable entre deux schémas qui ne "
			 "produisent pas le même panache",
			 (double)pF.concentration, (double)pS.concentration);
	ProbeCheck(pF.concentration >= 2.0f, "(f2) la vorticité se RASSEMBLE encore : P >= 2,0", buf);

	// ── LÀ OÙ LE SOUS-CYCLAGE MORD ──────────────────────────────────────────
	printf("\n--- (f2) LA SCÈNE (e) : la SEULE où le sous-cyclage mord ---\n");
	const ResultatPrix eS = SceneDixSecondes(false);
	const ResultatPrix eF = SceneDixSecondes(true);
	printf("      schéma            CFL max   sous-pas   vmax      Tmax     NaN   y bary   ms/pas\n");
	printf("      semi-lagrangien   %7.3f   %8u   %6.3f   %7.1f   %3u   %6.3f   %6.1f\n", (double)eS.cflMax,
		   eS.sousPasMax, (double)eS.vmax, (double)eS.tmax, eS.nan, (double)eS.hauteurBary, (double)eS.msParPas);
	printf("      FLUX (donor-cell) %7.3f   %8u   %6.3f   %7.1f   %3u   %6.3f   %6.1f%s\n", (double)eF.cflMax,
		   eF.sousPasMax, (double)eF.vmax, (double)eF.tmax, eF.nan, (double)eF.hauteurBary, (double)eF.msParPas,
		   eF.capHit ? "  (BORNE ATTEINTE)" : "");
	snprintf(buf, sizeof(buf),
			 "CFL max %.3f, %u sous-pas%s, %u NaN, %.1f ms/pas contre %.1f au semi-lagrangien — le "
			 "sous-cyclage est NOMMÉ et son coût PUBLIÉ : tu, il serait une régression de performance "
			 "qu'on découvrirait dans six mois",
			 (double)eF.cflMax, eF.sousPasMax, eF.capHit ? " (BORNE ATTEINTE)" : "", eF.nan, (double)eF.msParPas,
			 (double)eS.msParPas);
	ProbeCheck(eF.nan == 0 && !eF.capHit, "(f2) la scène (e) tient : ni NaN ni borne de sous-pas atteinte", buf);

	printf("\n    ⚠️ NON MESURÉ DANS CE LOT, et je le dis plutôt que de l'omettre : le plancher\n");
	printf("    (2.1) contraste > 10 exige le chemin de RENDU. Il reste donc un critère\n");
	printf("    pré-enregistré NON JUGÉ, et le lot ne peut pas se dire complet sans lui.\n");
}

// =============================================================================
void PalierAdvectionFlux() {
	printf("\n=== (f) ADVECTION CONSERVATIVE EN FLUX — Lentine, Aanjaneya & Fedkiw, SCA 2011 ===\n");
	printf("    (f1) juge le schéma en FLUX. Sur le MÊME montage et dans la MÊME course, le\n");
	printf("    semi-lagrangien est publié comme RÉFÉRENCE et ne rend aucun verdict : il n'a\n");
	printf("    AUCUN bilan — son interpolation redistribue sans vérifier que ce qui part d'une\n");
	printf("    cellule arrive dans une autre — et il dérivait de 1,132e-01 le 12/09, quand le\n");
	printf("    schéma en flux n'existait pas encore. Un schéma en FLUX est conservatif PAR\n");
	printf("    CONSTRUCTION : il doit verdir au PREMIER jet, sinon le portage est faux.\n");
	printf("    Voir PLAN_ADVECTION_FLUX.md, § 1.\n");

	ControleConservation();
}
