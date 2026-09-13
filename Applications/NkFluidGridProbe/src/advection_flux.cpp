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
		// ⚠️ AJOUT DU 13/09, pour l'ORDRE SUPÉRIEUR. La densité MINIMALE est le seul
		// détecteur qui sépare « conservatif » de « juste » : un schéma d'ordre 2
		// non limité conserve la masse EXACTEMENT tout en fabriquant des valeurs
		// NÉGATIVES (Godunov). Elle vaut 0,000e+00 EXACTEMENT sur tout schéma
		// monotone — un zéro qui ne serait jamais autre chose ne prouverait rien,
		// et c'est le mode `Aucun` qui prouve qu'il sait l'être.
		float32 densiteMin = 0.f;
		// ⚠️ AJOUT DU 13/09, pour l'ENQUÊTE (i). La masse de fumée FINALE et la
		// CHALEUR TOTALE, somme (T − T_ambiante)·h^3 sur l'intérieur. Deux schémas
		// dont les Tmax sont dans un rapport de 3 peuvent porter la MÊME chaleur :
		// alors la différence est une CONCENTRATION, pas une perte. Sans cette
		// seconde grandeur, on ne saurait pas laquelle des deux on regarde.
		float32 masseFinale = 0.f, chaleurTotale = 0.f;
		// ⚠️ AJOUT DU 13/09, pour le CONTRÔLE NÉGATIF (j1b). « La masse est
		// constante » ne se lit PAS sur la valeur finale : une masse qui monte puis
		// redescend rendrait exactement la même. On borne donc la course entière.
		float32 masseMin = 1.0e30f, masseMax = -1.0e30f;
};

// La chaleur au-dessus de l'ambiante, intégrée sur l'INTÉRIEUR (K·m^3).
static float32 ChaleurTotale(const NkFluidGrid &g, float32 ambiante) {
	const float32 *T = g.Temperature();
	const float32 h3 = g.CellSize() * g.CellSize() * g.CellSize();
	float64 s = 0.0;
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i)
				s += (float64)(T[g.Idx(i, j, k)] - ambiante);
	return (float32)(s * (float64)h3);
}

// La densité la plus basse de l'INTÉRIEUR. Les fantômes sont exclus : ils sont
// une recopie du bord, pas un état du fluide.
static float32 DensiteMinimale(const NkFluidGrid &g) {
	const float32 *d = g.Density();
	float32 mn = 1.0e30f;
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i) {
				const float32 v = d[g.Idx(i, j, k)];
				if (v < mn)
					mn = v;
			}
	return mn;
}

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
static ResultatPrix ScenePanache(bool flux, uint32 pas,
								 NkFluidFluxLimiter lim = NkFluidFluxLimiter::Ordre1) {
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
	p.advectFluxLimiter = lim;
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
static ResultatPrix SceneMasse(bool flux, NkFluidFluxLimiter lim = NkFluidFluxLimiter::Ordre1) {
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
	p.advectFluxLimiter = lim;
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
	r.densiteMin = DensiteMinimale(g);
	return r;
}

// ── SCÈNE E : celle de (e) — LA SEULE où le sous-cyclage MORD vraiment ──────
// vmax y monte à 7,782 m/s : à dt = 1/60 et h = 0,02, CFL vaut ~6,5.
//
// `cibleCFL` : 0 laisse le défaut du solveur. ⚠️ CETTE CIBLE EST DANS LES UNITÉS
// DU BANC, PAS DANS CELLES DE LA THÉORIE — voir la définition portée par
// `NkFluidGridParams::advectCFLTarget`. La rupture mesurée en (g1) est à 1,2433
// dans CES unités, pas à 1,0.
// `alpha` : < 0 laisse le défaut de la scène (0,3). 0 COUPE le poids de la fumée
// — c'est le seul paramètre que fait varier l'enquête (i) du § 9 du plan.
// `injectionUnique` : n'émet QU'AU PREMIER pas. `dissipDensite` : < 0 laisse le
// défaut de la scène (0,2). Les deux servent au contrôle négatif (j1b), où
// l'attendu analytique devient EXACT au lieu d'approché.
static ResultatPrix SceneDixSecondes(bool flux, float32 cibleCFL = 0.f,
									 NkFluidFluxLimiter lim = NkFluidFluxLimiter::Ordre1, float32 alpha = -1.f,
									 bool injectionUnique = false, float32 dissipDensite = -1.f) {
	ResultatPrix r;
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, 0.f, -0.25f};
	p.boundsMax = {0.25f, 1.f, 0.25f};
	p.cellSize = 0.02f;
	p.densityDissipation = 0.2f;
	p.temperatureDissipation = 0.5f;
	p.buoyancyAlpha = 0.3f;
	p.advectFluxConservative = flux;
	p.advectFluxLimiter = lim;
	if (alpha >= 0.f)
		p.buoyancyAlpha = alpha;
	if (dissipDensite >= 0.f)
		p.densityDissipation = dissipDensite;
	if (cibleCFL > 0.f)
		p.advectCFLTarget = cibleCFL;
	NkFluidGrid g;
	if (!g.Init(p))
		return r;
	const float32 dt = 1.f / 60.f;
	float64 ensSum = 0.0, vortSum = 0.0, msSum = 0.0;
	for (uint32 s = 0; s < 600; ++s) {
		if (!injectionUnique || s == 0)
			g.EmitSphere({0.f, 0.05f, 0.f}, 0.05f, 6.f * dt, 900.f * dt, 0.f);
		g.Step(dt);
		RelevePas(g, r, ensSum, vortSum, msSum);
		// La masse APRÈS chaque pas, pour (j1b) : « constante au dernier chiffre »
		// ne se lit pas sur la valeur finale, il faut la borne sur TOUTE la course.
		const float32 m = g.Stats().mass;
		if (m < r.masseMin)
			r.masseMin = m;
		if (m > r.masseMax)
			r.masseMax = m;
	}
	r.enstrophieMoy = (float32)(ensSum / 600.0);
	r.msParPas = (float32)(msSum / 600.0);
	r.densiteMin = DensiteMinimale(g);
	r.masseFinale = g.TotalMass();
	r.chaleurTotale = ChaleurTotale(g, p.ambientTemperature);
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

static EtatStabilite CourseStabilite(float32 dt, uint32 maxSousPas, uint32 pas,
									 NkFluidFluxLimiter lim = NkFluidFluxLimiter::Ordre1) {
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
	p.advectFluxLimiter = lim;
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

// =============================================================================
// (g1) RESSERRER LA RUPTURE PAR DICHOTOMIE.
//
// (f3) a laisse un trou que j'ai dit moi-meme : le balayage saute de 1/10 s
// (CFL 1,077, SAIN) a 1/8 s (CFL 1,353, CASSE). La rupture est entre les deux et
// je ne sais PAS ou. ⚠️ ON NE CHOISIT PAS UNE MARGE DE SECURITE CONTRE UNE BORNE
// QU'ON N'A PAS -- et c'est une marge qu'il faudra choisir en (g3).
//
// On publie donc un ENCADREMENT, pas un point : « entre X et Y » est une mesure,
// « environ Z » n'en est pas une.
//
// ── LES DEUX PIEGES DE (f3) SONT CONSERVES, parce qu'ils decidaient de tout ──
//   - le FILET reste COUPE (advectMaxSubsteps = 1) : avec le sous-cyclage le
//     schema ne casse jamais, il subdivise, et on mesurerait LE FILET ;
//   - la MASSE reste ecartee comme detecteur : le donor-cell est conservatif MEME
//     instable. Le detecteur est la DENSITE NEGATIVE, exact et non commode : elle
//     vaut 0,000e+00 EXACTEMENT tant que le schema est monotone.
//
// ── PREDICTION CALCULEE, ecrite avant la course ─────────────────────────────
// La theorie donne CFL <= 1 pour le donor-cell 3D non-splitte. Or (f3) a mesure
// SAIN a 1,077, donc AU-DESSUS de 1 -- et ce n'est pas une contradiction : mon
// estimateur de CFL prend, par axe, le MAX des deux faces de la cellule
// (max(|u[i]|, |u[i+1]|)), alors que la condition vraie porte sur la somme des
// flux SORTANTS. Il SURESTIME donc le CFL reel, et la rupture doit apparaitre
// au-dessus de 1 dans MES unites. J'attends l'encadrement quelque part entre
// 1,077 et 1,353, et je ne pretends pas savoir ou : c'est la question posee.
// =============================================================================
void EnqueteRuptureFine() {
	printf("\n=== (g1) RESSERRER LA RUPTURE PAR DICHOTOMIE ===\n");
	printf("    (f3) laissait la rupture ENTRE CFL 1,077 (sain) et 1,353 (casse), sans savoir\n");
	printf("    ou. On ne choisit pas une marge de securite contre une borne qu'on n'a pas.\n");
	printf("    ⚠️ Filet COUPE (advectMaxSubsteps = 1) et masse ECARTEE comme detecteur : c'est\n");
	printf("    la DENSITE NEGATIVE qui juge, et elle vaut 0,000e+00 EXACTEMENT tant que le\n");
	printf("    schema est monotone.\n");
	printf("    PREDICTION : l'encadrement tombe AU-DESSUS de 1 car mon estimateur de CFL prend\n");
	printf("    le MAX des deux faces par axe, quand la condition vraie porte sur les flux\n");
	printf("    SORTANTS -- il SURESTIME. Je n'affirme pas ou, entre 1,077 et 1,353.\n");
	char buf[520];

	float32 dtSain = 1.f / 10.f;  // (f3) : CFL 1,077, sain
	float32 dtCasse = 1.f / 8.f;  // (f3) : CFL 1,353, casse

	// ── GARDE : l'encadrement de depart EST-IL un encadrement ? ─────────────
	// Une dichotomie lancee sur des bornes qui ne se comportent pas comme annonce
	// convergerait proprement vers un nombre FAUX. On le verifie au lieu de
	// reprendre (f3) sur parole.
	const EtatStabilite bas0 = CourseStabilite(dtSain, 1u, 100u);
	const EtatStabilite haut0 = CourseStabilite(dtCasse, 1u, 100u);
	snprintf(buf, sizeof(buf),
			 "borne basse 1/10 s : CFL %.3f, densite min %+.3e -> %s ; borne haute 1/8 s : CFL %.3f, "
			 "densite min %+.3e -> %s. Une dichotomie lancee sur un faux encadrement convergerait "
			 "proprement vers un nombre FAUX",
			 (double)bas0.cflMax, (double)bas0.dMin, bas0.casse ? "CASSE" : "sain", (double)haut0.cflMax,
			 (double)haut0.dMin, haut0.casse ? "CASSE" : "sain");
	ProbeCheck(!bas0.casse && haut0.casse, "(g1) GARDE : l'encadrement de depart EST un encadrement", buf);
	// ⚠️ L'encadrement est INVALIDE si la borne basse CASSE, ou si la borne haute NE
	// CASSE PAS. Un premier jet écrivait `bas0.casse || haut0.casse` — qui sortait
	// dès que la borne haute cassait, c'est-à-dire dans le cas NORMAL, puisque
	// c'est sa raison d'être. La dichotomie ne tournait jamais et le mode rendait
	// un bilan VERT à UN SEUL contrôle. Un bilan vert qui ne prouve rien est
	// exactement ce que ce banc traque, et il était dans ma propre garde.
	// Le cas dégradé ROUGIT donc explicitement, au lieu de sortir en silence.
	if (bas0.casse || !haut0.casse) {
		ProbeCheck(false, "(g1) la rupture est ENCADREE a mieux que 0,01 en CFL",
				   "l'encadrement de depart est INVALIDE : la dichotomie n'a PAS tourne. Ce controle "
				   "rougit explicitement plutot que de laisser un bilan vert a un seul controle");
		return;
	}

	float32 cflSain = bas0.cflMax, cflCasse = haut0.cflMax;
	printf("      iter   dt (s)        CFL      densite min    verdict    encadrement CFL\n");
	const uint32 kIterations = 10; // 2^10 : l'intervalle initial (0,276) divise par 1024
	for (uint32 it = 0; it < kIterations; ++it) {
		const float32 dtm = 0.5f * (dtSain + dtCasse);
		const EtatStabilite e = CourseStabilite(dtm, 1u, 100u);
		if (e.casse) {
			dtCasse = dtm;
			cflCasse = e.cflMax;
		} else {
			dtSain = dtm;
			cflSain = e.cflMax;
		}
		printf("      %2u    %.6f   %8.4f   %+.3e    %-7s   [%.4f ; %.4f]\n", it + 1, (double)dtm, (double)e.cflMax,
			   (double)e.dMin, e.casse ? "CASSE" : "sain", (double)cflSain, (double)cflCasse);
		fflush(stdout);
	}

	const float32 largeur = cflCasse - cflSain;
	snprintf(buf, sizeof(buf),
			 "la rupture est ENTRE CFL %.4f (dernier SAIN, dt = 1/%.2f s) et CFL %.4f (premier CASSE, "
			 "dt = 1/%.2f s) — largeur %.5f apres %u dichotomies. C'est un ENCADREMENT, pas un point : "
			 "« environ » n'aurait pas ete une mesure",
			 (double)cflSain, (double)(1.f / dtSain), (double)cflCasse, (double)(1.f / dtCasse), (double)largeur,
			 kIterations);
	ProbeCheck(largeur < 0.01f, "(g1) la rupture est ENCADREE a mieux que 0,01 en CFL", buf);
}

// =============================================================================
// (g2) LA COURBE DIFFUSION / CIBLE, et (g3) LE CHOIX DE LA CIBLE.
//
// ⚠️⚠️ TOUTES LES CIBLES DE CETTE ENQUÊTE SONT DANS LES UNITÉS DU BANC. `MaxCFL`
// prend, par axe, le MAX des deux faces de la cellule, là où la condition vraie
// porte sur la SOMME DES FLUX SORTANTS : il SURESTIME, d'un facteur qui dépend du
// champ. Le schéma est encore SAIN à 1,077 dans ces unités, et (g1) a ENCADRÉ la
// rupture entre 1,2433 et 1,2436 — PAS à 1,0. Lire « cible 0,9 » comme un CFL de
// manuel serait une erreur d'un quart.
//
// ── (g3) LA RÈGLE DE MARGE, ÉCRITE AVANT D'EN CONNAÎTRE LE RÉSULTAT ─────────
// La cible retenue est la PLUS HAUTE valeur de la grille testée qui reste sous
//     kMargeSecurite * (borne BASSE de l'encadrement de (g1))
// JUSTIFICATION, et elle ne dépend pas du chiffre que la règle produira : la
// borne de rupture a été mesurée sur UNE scène (le tourbillon de (f1)). Or le
// facteur de surestimation de `MaxCFL` DÉPEND DU CHAMP — grand sous fort
// cisaillement, petit sur un champ lisse. Une borne mesurée sur une scène ne se
// transporte donc pas telle quelle sur une autre, où la rupture peut apparaître
// PLUS BAS dans ces mêmes unités. 20 % couvrent cette variabilité inter-scènes.
// C'est une POLITIQUE, pas un nombre choisi pour obtenir la réponse qui arrange.
// =============================================================================
static const float32 kMargeSecurite = 0.80f;	   // 20 % sous la borne basse
static const float32 kRuptureBorneBasse = 1.2433f; // (g1), UNITÉS DU BANC

void EnqueteCibleSousCyclage() {
	printf("\n=== (g2) LA COURBE DIFFUSION / CIBLE, puis (g3) LE CHOIX ===\n");
	printf("    ⚠️ Cibles dans les UNITÉS DU BANC : MaxCFL prend le max des deux faces par axe au\n");
	printf("    lieu de la somme des flux sortants — il SURESTIME. (g1) a encadré la rupture entre\n");
	printf("    1,2433 et 1,2436, PAS à 1,0.\n");
	printf("    RÈGLE DE MARGE, écrite AVANT d'en connaître le résultat : cible = la plus haute de\n");
	printf("    la grille telle que cible <= %.2f x %.4f = %.4f. Justification indépendante du\n",
		   (double)kMargeSecurite, (double)kRuptureBorneBasse, (double)(kMargeSecurite * kRuptureBorneBasse));
	printf("    résultat : le facteur de surestimation DÉPEND DU CHAMP, donc une borne mesurée sur\n");
	printf("    UNE scène ne se transporte pas telle quelle sur une autre.\n");
	printf("    PRÉDICTION : n = ceil(CFL/cible), donc la courbe aura des PALIERS — entre deux\n");
	printf("    cibles qui donnent le même n, RIEN ne change. Je prédis que le prix pourrait NE PAS\n");
	printf("    bouger. Mais le système est bouclé (moins de diffusion -> Tmax plus haut -> vitesse\n");
	printf("    plus haute -> CFL plus haut), donc je mesure au lieu de conclure.\n");
	char buf[560];

	// La RÉFÉRENCE semi-lagrangienne, dans la MÊME course : elle donne le
	// dénominateur du prix. Un rapport dont le numérateur et le dénominateur
	// viennent de deux courses différentes ne mesure rien.
	const ResultatPrix ref = SceneDixSecondes(false);
	printf("\n    RÉFÉRENCE semi-lagrangienne (MÊME course) : Tmax %.1f K, vmax %.3f m/s, %.1f ms/pas\n",
		   (double)ref.tmax, (double)ref.vmax, (double)ref.msParPas);

	const float32 cibles[6] = {0.40f, 0.60f, 0.80f, 0.90f, 1.00f, 1.20f};
	ResultatPrix res[6];
	printf("\n      cible   CFL max   sous-pas    Tmax (K)   Tmax/ref   vmax     NaN   ms/pas\n");
	for (uint32 c = 0; c < 6; ++c) {
		res[c] = SceneDixSecondes(true, cibles[c]);
		printf("      %.2f    %7.3f   %8u   %8.1f   %7.3f   %6.3f   %3u   %6.1f%s\n", (double)cibles[c],
			   (double)res[c].cflMax, res[c].sousPasMax, (double)res[c].tmax,
			   (double)(ref.tmax > 0.f ? res[c].tmax / ref.tmax : 0.f), (double)res[c].vmax, res[c].nan,
			   (double)res[c].msParPas, res[c].capHit ? "  (BORNE)" : "");
		fflush(stdout);
	}

	// (g2) LA TENDANCE, mesurée et non supposée.
	snprintf(buf, sizeof(buf),
			 "Tmax conservé : %.1f K à la cible la plus BASSE (%.2f) contre %.1f K à la plus HAUTE (%.2f). "
			 "La théorie dit que la diffusion du donor-cell, D = (u*h/2)(1-CFL), DÉCROÎT quand le CFL "
			 "effectif monte, donc Tmax doit CROÎTRE avec la cible. Si la courbe ne suit pas, c'est une "
			 "TROUVAILLE et elle vaut plus que le réglage",
			 (double)res[0].tmax, (double)cibles[0], (double)res[5].tmax, (double)cibles[5]);
	ProbeCheck(res[5].tmax >= res[0].tmax, "(g2) la diffusion DÉCROÎT quand la cible monte", buf);

	// ── (g3) LE CHOIX, par application MÉCANIQUE de la règle ────────────────
	const float32 plafond = kMargeSecurite * kRuptureBorneBasse;
	float32 cibleRetenue = cibles[0];
	for (uint32 c = 0; c < 6; ++c)
		if (cibles[c] <= plafond)
			cibleRetenue = cibles[c];
	printf("\n    (g3) CIBLE RETENUE : %.2f (UNITÉS DU BANC) — la plus haute de la grille sous %.4f\n",
		   (double)cibleRetenue, (double)plafond);

	// CONTRÔLE NÉGATIF : à la cible retenue, AUCUN cas de la batterie ne casse.
	const ResultatPrix ctrlE = SceneDixSecondes(true, cibleRetenue);
	const ResultatPrix ctrlM = SceneMasse(true);
	const ResultatPrix ctrlP = ScenePanache(true, 120);
	const EtatStabilite ctrlT = CourseStabilite(1.f / 120.f, 64u, 100u);
	const bool batterieSaine = (ctrlE.nan == 0 && !ctrlE.capHit) && (ctrlM.nan == 0 && !ctrlM.capHit) &&
							   (ctrlP.nan == 0 && !ctrlP.capHit) && !ctrlT.casse && ctrlT.dMin >= -1.0e-6f;
	snprintf(buf, sizeof(buf),
			 "à la cible %.2f : scène (e) %u NaN%s ; scène masse %u NaN%s ; panache %u NaN%s ; tourbillon "
			 "densité min %+.3e. Une cible qui ferait casser un seul cas ne serait pas un réglage, ce "
			 "serait une régression",
			 (double)cibleRetenue, ctrlE.nan, ctrlE.capHit ? " (BORNE)" : "", ctrlM.nan,
			 ctrlM.capHit ? " (BORNE)" : "", ctrlP.nan, ctrlP.capHit ? " (BORNE)" : "", (double)ctrlT.dMin);
	ProbeCheck(batterieSaine, "(g3) CONTRÔLE NÉGATIF : aucun cas de la batterie ne casse à la cible retenue", buf);

	// ── LE NOUVEAU PRIX — le seul chiffre destiné à Rodolf ───────────────────
	const float32 prixRetenu = (ctrlE.tmax > 0.f) ? ref.tmax / ctrlE.tmax : 0.f;
	printf("\n    ===> LE NOUVEAU PRIX : Tmax / %.2f   (annoncé Tmax / 3,2 en (f2))\n", (double)prixRetenu);
	printf("         référence %.1f K, à la cible retenue %.1f K — MÊME course.\n", (double)ref.tmax,
		   (double)ctrlE.tmax);
	printf("    ⚠️ Ce rapport vient d'une SEULE course, référence et mesure comprises.\n");
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
// (h) L'ORDRE SUPÉRIEUR — van Leer 1979, Sweby 1984.
// Pré-enregistré dans PLAN_ORDRE_SUPERIEUR.md le 13/09, AVANT toute mesure.
//
// ⚠️ LA QUESTION N'EST PAS « la masse se conserve-t-elle ». Elle se conservera,
// et le limiteur n'y est POUR RIEN : le flux antidiffusif est retranché à une
// cellule et ajouté à l'autre, exactement comme celui d'ordre 1. Le schéma SANS
// limiteur — qui est FAUX — la conservera tout aussi exactement.
// La question est : LE DÉTAIL REVIENT-IL, et le schéma reste-t-il BORNÉ.
// =============================================================================
static const char *NomLimiteur(NkFluidFluxLimiter lim) {
	switch (lim) {
		case NkFluidFluxLimiter::Ordre1: return "ordre 1 (donor-cell)";
		case NkFluidFluxLimiter::Aucun: return "SANS limiteur (temoin -)";
		case NkFluidFluxLimiter::MinMod: return "minmod";
		case NkFluidFluxLimiter::VanLeer: return "van Leer";
		case NkFluidFluxLimiter::Superbee: return "superbee";
	}
	return "?";
}

struct EtatConservation {
		float32 derive = 1.f, bouge = 0.f, dMin = 0.f;
		bool ok = false;
};

// Le montage de (f1), À L'IDENTIQUE, avec le limiteur en paramètre : fonction de
// courant, divergence MAC identiquement nulle, parois fermées, AUCUNE source,
// aucune dissipation. Une masse qui varie ne peut venir que du SCHÉMA.
static EtatConservation CourseConservation(NkFluidFluxLimiter lim) {
	EtatConservation e;
	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {0.4f, 0.4f, 0.4f};
	p.cellSize = 0.02f;
	p.projectionEnabled = false;
	p.buoyancyEnabled = false;
	p.densityDissipation = 0.f;
	p.temperatureDissipation = 0.f;
	p.advectFluxConservative = true;
	p.advectFluxLimiter = lim;
	NkFluidGrid g;
	if (!g.Init(p))
		return e;
	g.EmitSphere({0.2f, 0.2f, 0.2f}, 0.06f, 1.f, 0.f, 0.f);
	const float32 m0 = g.TotalMass();
	const uint32 total = g.Stats().cellsTotal;
	NkVector<float32> dInit;
	dInit.Resize(total, 0.f);
	{
		const float32 *d0 = g.Density();
		for (uint32 i = 0; i < total; ++i)
			dInit[i] = d0[i];
	}
	const float32 dt = 1.f / 120.f;
	for (uint32 s = 0; s < 200; ++s) {
		PoserChampSansDivergence(g, p);
		g.Step(dt);
	}
	const float32 m1 = g.TotalMass();
	e.derive = (m0 > 0.f) ? ProbeAbs(m1 - m0) / m0 : 1.f;
	float64 l1 = 0.0;
	const float32 *dfin = g.Density();
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i) {
				const uint32 id = g.Idx(i, j, k);
				l1 += (float64)ProbeAbs(dfin[id] - dInit[id]);
			}
	const float32 h3 = g.CellSize() * g.CellSize() * g.CellSize();
	e.bouge = (m0 > 0.f) ? (float32)(l1 * (float64)h3) / m0 : 0.f;
	e.dMin = DensiteMinimale(g);
	e.ok = true;
	return e;
}

// ── (h2) LES TROIS CONTRÔLES DE LA COURSE COMPLÈTE ─────────────────────────
// Ils sont les MOINS CHERS du lot (20^3 cellules, 200 pas) : s'ils sont rouges,
// (h1) et (h3) ne veulent rien dire. Ils AJOUTENT 3 contrôles au compte du banc,
// annoncés d'avance dans le plan — le nombre de ROUGES, lui, ne doit pas bouger.
void ControleOrdreSuperieur() {
	printf("\n--- (h) ORDRE SUPÉRIEUR : le flux ANTIDIFFUSIF limité (van Leer 1979) ---\n");
	printf("    Montage de (f1), À L'IDENTIQUE, avec le limiteur en paramètre.\n");
	printf("    ⚠️ LA CONSERVATION NE JUGE PAS LA JUSTESSE : le flux antidiffusif est\n");
	printf("    retranché à une cellule et ajouté à l'autre, donc elle ne dépend PAS du\n");
	printf("    limiteur. C'est la DENSITÉ NÉGATIVE qui sépare « conservatif » de « juste ».\n");
	char buf[520];

	const EtatConservation vl = CourseConservation(NkFluidFluxLimiter::VanLeer);
	const EtatConservation au = CourseConservation(NkFluidFluxLimiter::Aucun);

	printf("      schéma                     dérive de masse   variation L1   densité min\n");
	printf("      van Leer (limité)            %.3e        %7.2f %%    %+.3e\n", (double)vl.derive,
		   (double)(vl.bouge * 100.f), (double)vl.dMin);
	printf("      SANS limiteur (témoin -)     %.3e        %7.2f %%    %+.3e\n", (double)au.derive,
		   (double)(au.bouge * 100.f), (double)au.dMin);

	snprintf(buf, sizeof(buf),
			 "dérive relative %.3e sur 200 pas (seuil %.0e, celui de (f1), jamais déplacé) — le flux "
			 "antidiffusif est posé SUR LA FACE et compté deux fois avec des signes opposés, donc la "
			 "conservation ne dépend pas du limiteur",
			 (double)vl.derive, (double)kSeuilDerive);
	ProbeCheck(vl.ok && vl.derive < kSeuilDerive, "(h2) la masse tient AUSSI à l'ordre supérieur", buf);

	snprintf(buf, sizeof(buf),
			 "variation L1 = %.2f %% de la masse initiale (exigé > 5 %%) — sans cette garde, « la masse "
			 "se conserve » serait TRIVIALEMENT vrai d'un schéma qui ne transporte RIEN, et ce montage "
			 "l'aggrave : la bulle est posée au point STATIONNAIRE de la fonction de courant",
			 (double)(vl.bouge * 100.f));
	ProbeCheck(vl.ok && vl.bouge > 0.05f, "(h2b) CONTRÔLE POSITIF : le champ a réellement été TRANSPORTÉ",
			   buf);

	// ⚠️ CE CONTRÔLE EST LE SEUL QUI JUGE LE LIMITEUR, et il est DOUBLE : le schéma
	// non limité doit passer SOUS ZÉRO, et le schéma limité doit rester AU MOINS
	// CENT FOIS plus près de zéro. Un seul des deux versants ne prouverait rien —
	// « densité min = 0 » est vrai de tout schéma monotone, y compris d'un schéma
	// qui ne ferait rien.
	// ⚠️ POURQUOI UN RAPPORT ET NON UN ZÉRO ABSOLU, et c'est écrit AVANT la mesure :
	// ce schéma est MULTIDIMENSIONNEL NON SPLITTÉ. La propriété TVD de van Leer est
	// un résultat 1D ; en 3D sans transport de coin, de MINUSCULES sous-dépassements
	// restent possibles même avec limiteur. Exiger zéro exact serait exiger une
	// propriété que le schéma ne PRÉTEND PAS avoir, et un rouge obtenu ainsi ne
	// dirait rien du limiteur. Le rapport, lui, teste ce qui est réellement en jeu :
	// le limiteur réduit-il le sous-dépassement d'un ORDRE DE GRANDEUR ou non.
	snprintf(buf, sizeof(buf),
			 "SANS limiteur : densité min %+.3e (doit passer SOUS zéro — Godunov : aucun schéma linéaire "
			 "d'ordre > 1 n'est monotone) ; AVEC van Leer : %+.3e, exigé au moins 100 fois plus près de "
			 "zéro (rapport, pas seuil absolu : le schéma est multi-D non splitté, la TVD de van Leer est "
			 "un résultat 1D). Et les DEUX conservent la masse : %.3e contre %.3e — c'est la preuve que "
			 "(h2) ne juge PAS la justesse",
			 (double)au.dMin, (double)vl.dMin, (double)au.derive, (double)vl.derive);
	ProbeCheck(vl.ok && au.ok && au.dMin < -1.0e-6f && vl.dMin > au.dMin * 0.01f,
			   "(h2c) LE LIMITEUR SERT : sans lui la densité passe sous zéro, avec lui 100x moins", buf);
}

// =============================================================================
// (h1) LE DÉTAIL REVIENT-IL, et (h3) LA STABILITÉ RECULE-T-ELLE.
// Mode NK_FLUID_MAC=8. Seuils écrits dans PLAN_ORDRE_SUPERIEUR.md § 4.
// =============================================================================
static const float32 kSeuilTmaxRatio = 0.50f;  // (h1), repris de Q6 du 12/09
static const float32 kSeuilRuptureH3 = 0.90f;  // (h3), la cible de (g3)

void EnqueteOrdreSuperieur() {
	printf("\n=== (h) L'ORDRE SUPÉRIEUR — le PRIX du premier ordre, repayé ou non ===\n");
	printf("    DÉCISION DE RODOLF (13/09) : ne pas subir l'arbitrage masse/détail, mais le\n");
	printf("    SUPPRIMER. (g2) avait éliminé le réglage : faire varier la cible de 0,40 à\n");
	printf("    1,20 ne déplace Tmax que de 2,1 %%. Le prix vient du PREMIER ORDRE LUI-MÊME.\n");
	printf("    PRÉDICTION ÉCRITE AVANT LA COURSE (PLAN_ORDRE_SUPERIEUR.md § 4) :\n");
	printf("      Tmax/ref entre 0,55 et 0,85 (aujourd'hui 0,3147 = 1/3,18) ; seuil 0,50.\n");
	printf("      ⚠️ La fourchette est ÉLARGIE EXPRÈS : Q4, Q5 et Q6 ont produit TROIS\n");
	printf("      sous-estimations d'ampleur de suite. Le sens était bon, la fourchette\n");
	printf("      trop étroite à chaque fois.\n");
	printf("      ⚠️ Tmax/ref = 1 N'EST PAS L'OBJECTIF : la référence semi-lagrangienne perd\n");
	printf("      43,1 %% de la masse, son Tmax est en partie l'artefact d'une concentration\n");
	printf("      non conservative. Elle dit D'OÙ L'ON PART, pas où il faut arriver.\n");
	char buf[600];

	// ── (h1) LE PRIX, scène (e), TOUS LES BRAS DANS LA MÊME COURSE ─────────
	printf("\n--- (h1) LE DÉTAIL : scène (e), 600 pas, h = 0,02 m, dt = 1/60 s, cible CFL 0,90 ---\n");
	const ResultatPrix ref = SceneDixSecondes(false);
	printf("      schéma                     CFL max  sous-pas    vmax      Tmax    Tmax/ref   dens.min   ms/pas\n");
	printf("      SEMI-LAGRANGIEN (réf.)     %7.3f  %8u  %6.3f  %8.1f   %7s   %+.3e  %6.1f\n",
		   (double)ref.cflMax, ref.sousPasMax, (double)ref.vmax, (double)ref.tmax, "1,000 ref",
		   (double)ref.densiteMin, (double)ref.msParPas);
	fflush(stdout);

	const NkFluidFluxLimiter bras[5] = {NkFluidFluxLimiter::Ordre1, NkFluidFluxLimiter::MinMod,
										NkFluidFluxLimiter::VanLeer, NkFluidFluxLimiter::Superbee,
										NkFluidFluxLimiter::Aucun};
	ResultatPrix res[5];
	float32 ratio[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
	for (uint32 b = 0; b < 5; ++b) {
		res[b] = SceneDixSecondes(true, 0.f, bras[b]);
		ratio[b] = (ref.tmax > 0.f) ? res[b].tmax / ref.tmax : 0.f;
		printf("      %-26s %7.3f  %8u  %6.3f  %8.1f   %7.4f   %+.3e  %6.1f%s\n", NomLimiteur(bras[b]),
			   (double)res[b].cflMax, res[b].sousPasMax, (double)res[b].vmax, (double)res[b].tmax,
			   (double)ratio[b], (double)res[b].densiteMin, (double)res[b].msParPas,
			   res[b].capHit ? "  (BORNE)" : "");
		fflush(stdout);
	}

	// ⚠️ LE VOLET NÉGATIF DE (h1), ET IL PASSE EN PREMIER : si le chemin par défaut
	// a bougé, (h1) n'est pas LISIBLE, quelle que soit la valeur du limiteur.
	// Les chiffres comparés sont ceux PUBLIÉS le 12/09 (Q6 et Q9), à leur précision
	// publiée — 550,5 K, CFL 2,452, 3 sous-pas.
	const ResultatPrix &o1 = res[0];
	const bool defautIntact =
		(ProbeAbs(o1.tmax - 550.5f) < 0.05f) && (ProbeAbs(o1.cflMax - 2.452f) < 0.001f) && (o1.sousPasMax == 3u);
	snprintf(buf, sizeof(buf),
			 "ordre 1 rend Tmax %.1f K (publié 550,5), CFL max %.3f (publié 2,452), %u sous-pas (publié 3). "
			 "`AdvectFluxUnePasse` n'a pas été touchée d'une ligne : la bit-identité du défaut se LIT DANS "
			 "LE DIFF, ce contrôle ne fait que la confirmer par la mesure",
			 (double)o1.tmax, (double)o1.cflMax, o1.sousPasMax);
	ProbeCheck(defautIntact, "(h1-) VOLET NÉGATIF : le schéma DÉSACTIVÉ rend les chiffres du 12/09", buf);

	// Le bras jugé est van Leer : c'est le limiteur que le plan désigne AVANT la
	// mesure, pas celui qui obtient le meilleur chiffre. minmod et superbee sont
	// publiés pour situer van Leer, et ne rendent AUCUN verdict.
	snprintf(buf, sizeof(buf),
			 "Tmax/ref = %.4f avec van Leer, contre %.4f à l'ordre 1 (seuil 0,50, écrit AVANT et repris "
			 "de Q6 du 12/09 ; prédit 0,55-0,85). minmod %.4f, superbee %.4f — publiés pour SITUER van "
			 "Leer, aucun verdict. La perte passe de /%.2f à /%.2f",
			 (double)ratio[2], (double)ratio[0], (double)ratio[1], (double)ratio[3],
			 (ratio[0] > 0.f) ? (double)(1.f / ratio[0]) : 0.0, (ratio[2] > 0.f) ? (double)(1.f / ratio[2]) : 0.0);
	ProbeCheck(ratio[2] >= kSeuilTmaxRatio, "(h1) LE DÉTAIL REVIENT : Tmax/ref >= 0,50 avec van Leer", buf);

	// Le témoin négatif du détecteur, sur CETTE scène-ci : le schéma non limité
	// doit fabriquer une densité négative là où les schémas bornés rendent
	// exactement zéro. Sans lui, « densité min = 0,000e+00 » ne prouverait rien.
	snprintf(buf, sizeof(buf),
			 "SANS limiteur : densité min %+.3e sur la scène (e) ; van Leer %+.3e (exigé au moins 100 fois "
			 "plus près de zéro — un RAPPORT, pas un seuil absolu : le schéma est multi-D non splitté et "
			 "la TVD de van Leer est un résultat 1D) ; ordre 1 %+.3e. Un témoin qui rend exactement zéro "
			 "doit prouver qu'il peut rendre autre chose — c'est le schéma non limité qui le prouve, et "
			 "c'est sa seule raison d'exister dans ce code",
			 (double)res[4].densiteMin, (double)res[2].densiteMin, (double)res[0].densiteMin);
	ProbeCheck(res[4].densiteMin < -1.0e-6f && res[2].densiteMin > res[4].densiteMin * 0.01f,
			   "(h1b) LE DÉTECTEUR SAIT RENDRE AUTRE CHOSE QUE ZÉRO", buf);

	// ── (h3) LA STABILITÉ — mesurée, pas supposée ──────────────────────────
	printf("\n--- (h3) LA STABILITÉ : la rupture BOUGE-T-ELLE ? ---\n");
	printf("    Ordre 1 : rupture ENCADRÉE entre CFL 1,2433 et 1,2436 (g1), dans les UNITÉS DU\n");
	printf("    BANC — MaxCFL prend le max des deux faces par axe, il SURESTIME. Filet COUPÉ\n");
	printf("    (advectMaxSubsteps = 1), détecteur = DENSITÉ NÉGATIVE (le schéma reste\n");
	printf("    conservatif même instable, donc la masse ne peut pas juger).\n");
	printf("    PRÉDICTION : van Leer rompt entre 1,0 et 1,25 ; seuil de réussite 0,90 (la cible\n");
	printf("    de sous-cyclage de (g3)) — sous elle, le sous-cyclage ne garantirait plus rien.\n");

	// ⚠️ L'ENCADREMENT DE DÉPART DE (g1) N'EST PAS REPRIS SUR PAROLE : un schéma
	// d'ordre supérieur PEUT casser dès 1/10 s, et une dichotomie lancée sur un
	// faux encadrement convergerait proprement vers un nombre FAUX. On ÉLARGIT donc
	// les bornes jusqu'à en avoir de vraies, au lieu de supposer celles de (g1).
	float32 dtSain = 1.f / 10.f, dtCasse = 1.f / 8.f;
	EtatStabilite bas = CourseStabilite(dtSain, 1u, 100u, NkFluidFluxLimiter::VanLeer);
	uint32 elargi = 0;
	while (bas.casse && elargi < 8u) {
		dtSain *= 0.5f;
		bas = CourseStabilite(dtSain, 1u, 100u, NkFluidFluxLimiter::VanLeer);
		++elargi;
	}
	EtatStabilite haut = CourseStabilite(dtCasse, 1u, 100u, NkFluidFluxLimiter::VanLeer);
	uint32 elargiH = 0;
	while (!haut.casse && elargiH < 8u) {
		dtCasse *= 2.f;
		haut = CourseStabilite(dtCasse, 1u, 100u, NkFluidFluxLimiter::VanLeer);
		++elargiH;
	}
	snprintf(buf, sizeof(buf),
			 "borne basse dt = 1/%.2f s : CFL %.4f, densité min %+.3e -> %s (élargie %u fois) ; borne "
			 "haute dt = 1/%.2f s : CFL %.4f, densité min %+.3e -> %s (élargie %u fois). L'encadrement de "
			 "(g1) n'est PAS repris sur parole : un schéma d'ordre supérieur peut casser plus tôt",
			 (double)(1.f / dtSain), (double)bas.cflMax, (double)bas.dMin, bas.casse ? "CASSE" : "sain", elargi,
			 (double)(1.f / dtCasse), (double)haut.cflMax, (double)haut.dMin, haut.casse ? "CASSE" : "sain",
			 elargiH);
	ProbeCheck(!bas.casse && haut.casse, "(h3) GARDE : l'encadrement de départ EST un encadrement", buf);
	if (bas.casse || !haut.casse) {
		ProbeCheck(false, "(h3) LA STABILITÉ NE RECULE PAS : rupture >= 0,90",
				   "l'encadrement de départ est INVALIDE : la dichotomie n'a PAS tourné. Ce contrôle "
				   "ROUGIT explicitement plutôt que de laisser un bilan vert sans dichotomie — c'est la "
				   "faute exacte payée en (g1) le 12/09");
		return;
	}

	float32 cflSain = bas.cflMax, cflCasse = haut.cflMax;
	printf("      iter   dt (s)        CFL      densite min    verdict    encadrement CFL\n");
	const uint32 kIter = 10;
	for (uint32 it = 0; it < kIter; ++it) {
		const float32 dtm = 0.5f * (dtSain + dtCasse);
		const EtatStabilite e = CourseStabilite(dtm, 1u, 100u, NkFluidFluxLimiter::VanLeer);
		if (e.casse) {
			dtCasse = dtm;
			cflCasse = e.cflMax;
		} else {
			dtSain = dtm;
			cflSain = e.cflMax;
		}
		printf("      %2u    %.6f   %8.4f   %+.3e    %-7s   [%.4f ; %.4f]\n", it + 1, (double)dtm,
			   (double)e.cflMax, (double)e.dMin, e.casse ? "CASSE" : "sain", (double)cflSain, (double)cflCasse);
		fflush(stdout);
	}
	snprintf(buf, sizeof(buf),
			 "van Leer : rupture ENTRE CFL %.4f (dernier SAIN) et %.4f (premier CASSE), largeur %.5f après "
			 "%u dichotomies — contre [1,2433 ; 1,2436] à l'ordre 1. Seuil 0,90, la cible de sous-cyclage "
			 "de (g3) : sous elle, le sous-cyclage ne garantirait plus rien et la cible devrait bouger",
			 (double)cflSain, (double)cflCasse, (double)(cflCasse - cflSain), kIter);
	ProbeCheck(cflSain >= kSeuilRuptureH3, "(h3) LA STABILITÉ NE RECULE PAS : rupture >= 0,90", buf);

	// Volet négatif de (h3) : le schéma NON limité doit casser PLUS TÔT.
	const EtatStabilite sansLim = CourseStabilite(dtSain, 1u, 100u, NkFluidFluxLimiter::Aucun);
	snprintf(buf, sizeof(buf),
			 "au dt que van Leer traverse SAIN (1/%.2f s, CFL %.4f), le schéma SANS limiteur rend une "
			 "densité min de %+.3e -> %s. S'il ne cassait pas, mon détecteur ne verrait pas ce qu'il "
			 "prétend voir, et (h3) ne vaudrait rien",
			 (double)(1.f / dtSain), (double)sansLim.cflMax, (double)sansLim.dMin,
			 sansLim.casse ? "CASSE" : "sain");
	ProbeCheck(sansLim.casse, "(h3-) VOLET NÉGATIF : sans limiteur, le même dt CASSE", buf);

	printf("\n    ⚠️ CE QUE CE LOT NE FAIT PAS, et je le dis plutôt que de l'omettre :\n");
	printf("    la QUANTITÉ DE MOUVEMENT n'est pas conservée — seuls les SCALAIRES passent par\n");
	printf("    le flux, la VITESSE reste semi-lagrangienne. La chaîne température -> poussée ->\n");
	printf("    vitesse traverse donc toujours un champ non conservatif. Le schéma reste\n");
	printf("    CONDITIONNELLEMENT stable, le sous-cyclage reste EXIGÉ, et\n");
	printf("    `advectFluxConservative` reste ÉTEINT PAR DÉFAUT : l'allumer est une décision\n");
	printf("    de Rodolf, et elle attend ce chiffre.\n");
}

// =============================================================================
// (j1) LE COMPTAGE ANALYTIQUE — pré-enregistré au § 11 du plan, AVANT de coder.
//
// ⚠️⚠️ LA RÈGLE DE CE LOT, ET ELLE EST TOUT LE LOT :
//      LE NOMBRE ATTENDU NE SE DEMANDE JAMAIS AU SOLVEUR QU'ON JUGE.
// Cette fonction ne touche AUCUN objet de `NkFluidGrid`. Elle reçoit les
// paramètres de la SCÈNE — ceux qu'un lecteur peut lire dans `SceneDixSecondes` —
// et refait le calcul depuis zéro : la taille de grille, le dénombrement des
// cellules de la sphère, la masse injectée par pas, et la récurrence.
//
// LA RÉCURRENCE, plutôt qu'une formule fermée, et c'est délibéré : elle est le
// MODÈLE, pas son résumé. L'ordre exact d'un tour de boucle, lu dans `Step()` :
//      EmitSphere  ->  M <- M + A
//      Step        ->  advection (le flux CONSERVE), puis M <- M * f
// Une formule fermée cacherait cet ordre ; la récurrence l'expose, et c'est
// justement l'ordre qui est la seule chose qu'on puisse se tromper à écrire.
// =============================================================================
struct ComptageAnalytique {
		uint32 nx = 0, ny = 0, nz = 0, cellules = 0;
		float64 masseParPas = 0.0, facteur = 1.0, masse = 0.0;
};

static ComptageAnalytique CompterALaMain(NkVec3f bmin, NkVec3f bmax, float32 h, NkVec3f centre, float32 rayon,
										 float64 debit, float64 dt, uint32 pas, float64 dissipation,
										 bool injectionUnique) {
	ComptageAnalytique c;
	// La grille, RECOMPTÉE — même règle que `NkFluidGrid::Init`, réécrite ici.
	c.nx = (uint32)NkMax(1.f, NkCeil((bmax.x - bmin.x) / h));
	c.ny = (uint32)NkMax(1.f, NkCeil((bmax.y - bmin.y) / h));
	c.nz = (uint32)NkMax(1.f, NkCeil((bmax.z - bmin.z) / h));

	// Le dénombrement, REFAIT : même prédicat que `EmitSphere`, centre de cellule
	// dans la sphère, et SEULEMENT l'intérieur (1..n), jamais les fantômes.
	const float32 r2 = rayon * rayon;
	for (uint32 k = 1; k <= c.nz; ++k)
		for (uint32 j = 1; j <= c.ny; ++j)
			for (uint32 i = 1; i <= c.nx; ++i) {
				const float32 cx = bmin.x + ((float32)i - 0.5f) * h;
				const float32 cy = bmin.y + ((float32)j - 0.5f) * h;
				const float32 cz = bmin.z + ((float32)k - 0.5f) * h;
				const float32 dx = cx - centre.x, dy = cy - centre.y, dz = cz - centre.z;
				if (dx * dx + dy * dy + dz * dz <= r2)
					++c.cellules;
			}

	const float64 h3 = (float64)h * (float64)h * (float64)h;
	c.masseParPas = (float64)c.cellules * debit * dt * h3;
	c.facteur = (dissipation > 0.0) ? NkExp(-dissipation * dt) : 1.0;

	// LA RÉCURRENCE, pas à pas, dans l'ordre du solveur.
	float64 m = 0.0;
	for (uint32 s = 0; s < pas; ++s) {
		if (!injectionUnique || s == 0)
			m += c.masseParPas;
		m *= c.facteur;
	}
	c.masse = m;
	return c;
}

static float64 EcartRelatif(float64 mesure, float64 attendu) {
	if (attendu == 0.0)
		return (mesure == 0.0) ? 0.0 : 1.0e30;
	const float64 d = (mesure - attendu) / attendu;
	return (d < 0.0) ? -d : d;
}

static const float64 kSeuilEcartAnalytique = 1.0e-3; // (j1), § 11 du plan
static const float64 kFacteurSemiLagMin = 5.0;		 // (j1), § 11 du plan

void EnqueteComptageAnalytique() {
	printf("\n=== (j1) LE COMPTAGE ANALYTIQUE DE LA MASSE INJECTÉE ===\n");
	printf("    ⚠️ LE NOMBRE ATTENDU N'EST PAS DEMANDÉ AU SOLVEUR QU'ON JUGE. Il est\n");
	printf("    recalculé ici, depuis les paramètres de scène : taille de grille par\n");
	printf("    ceil((max-min)/h), dénombrement des cellules de la sphère par le même\n");
	printf("    prédicat géométrique, puis la RÉCURRENCE pas à pas — injection, puis\n");
	printf("    dissipation — dans l'ordre exact lu dans Step().\n");
	printf("    ÉCRIT AVANT LA COURSE (PLAN_ORDRE_SUPERIEUR.md § 11) :\n");
	printf("      N = 81 cellules, A = 6,480e-05 par pas, M attendue = 0,016781\n");
	printf("      (j1) conservatif : écart relatif < 1e-3 ; semi-lagrangien : facteur > 5\n");
	printf("    ⚠️ SI LE CONSERVATIF NE COLLE PAS NON PLUS, toute la chaîne de Q10 tombe,\n");
	printf("    et c'est MON comptage que je vérifie d'abord — un instrument neuf qui\n");
	printf("    contredit deux mesures anciennes est plus souvent faux que les deux.\n");
	char buf[640];

	const NkVec3f bmin = {-0.25f, 0.f, -0.25f};
	const NkVec3f bmax = {0.25f, 1.f, 0.25f};
	const float32 h = 0.02f;
	const NkVec3f src = {0.f, 0.05f, 0.f};
	const float32 rayon = 0.05f;
	const float64 dt = 1.0 / 60.0;
	const float64 debit = 6.0;
	const uint32 pas = 600;

	const ComptageAnalytique a = CompterALaMain(bmin, bmax, h, src, rayon, debit, dt, pas, 0.2, false);
	printf("\n    COMPTAGE À LA MAIN : grille %u x %u x %u ; %u cellules dans la sphère ;\n", a.nx, a.ny, a.nz,
		   a.cellules);
	printf("    A = %.6e par pas ; f = exp(-0,2/60) = %.9f ; M attendue = %.9f\n", a.masseParPas, a.facteur,
		   a.masse);

	// ── LES COURSES ────────────────────────────────────────────────────────
	const ResultatPrix semi = SceneDixSecondes(false);
	const ResultatPrix o1 = SceneDixSecondes(true, 0.f, NkFluidFluxLimiter::Ordre1);
	const ResultatPrix vl = SceneDixSecondes(true, 0.f, NkFluidFluxLimiter::VanLeer);

	// ⚠️ GARDE : le comptage à la main porte-t-il sur LA MÊME GRILLE ? Si non, il
	// serait faux tout en ayant l'air juste — et rien dans les chiffres ne le
	// dirait. On le demande au solveur parce que c'est la seule chose qu'on ait le
	// droit de lui demander : la FORME de son domaine, jamais la RÉPONSE.
	{
		NkFluidGridParams p;
		p.boundsMin = bmin;
		p.boundsMax = bmax;
		p.cellSize = h;
		NkFluidGrid g;
		const bool ok = g.Init(p);
		const bool memeGrille = ok && g.Nx() == a.nx && g.Ny() == a.ny && g.Nz() == a.nz;
		snprintf(buf, sizeof(buf),
				 "à la main %u x %u x %u ; le solveur %u x %u x %u. Un comptage exact sur la MAUVAISE "
				 "grille rend un nombre faux qui a l'air juste, et aucun des chiffres suivants ne le "
				 "dirait",
				 a.nx, a.ny, a.nz, ok ? g.Nx() : 0u, ok ? g.Ny() : 0u, ok ? g.Nz() : 0u);
		ProbeCheck(memeGrille, "(j1) GARDE : la grille RECOMPTÉE est celle du solveur", buf);
	}

	const float64 eSemi = EcartRelatif((float64)semi.masseFinale, a.masse);
	const float64 eO1 = EcartRelatif((float64)o1.masseFinale, a.masse);
	const float64 eVL = EcartRelatif((float64)vl.masseFinale, a.masse);
	const float64 fSemi = (a.masse > 0.0) ? (float64)semi.masseFinale / a.masse : 0.0;

	printf("\n      schéma              masse finale    écart relatif à l'analytique   facteur\n");
	printf("      ANALYTIQUE (main)    %.9f    —                              1,000\n", a.masse);
	printf("      FLUX ordre 1         %.9f    %.3e                      %.3f\n", (double)o1.masseFinale, eO1,
		   (a.masse > 0.0) ? (double)o1.masseFinale / a.masse : 0.0);
	printf("      FLUX van Leer        %.9f    %.3e                      %.3f\n", (double)vl.masseFinale, eVL,
		   (a.masse > 0.0) ? (double)vl.masseFinale / a.masse : 0.0);
	printf("      SEMI-LAGRANGIEN      %.9f    %.3e                      %.3f\n", (double)semi.masseFinale, eSemi,
		   fSemi);
	fflush(stdout);

	snprintf(buf, sizeof(buf),
			 "van Leer %.9f contre %.9f calculée À LA MAIN : écart relatif %.3e (seuil 1e-3, écrit AVANT) ; "
			 "l'ordre 1 %.3e. Le nombre attendu ne vient PAS du solveur — il vient de %u cellules, d'un "
			 "débit et d'une récurrence de %u pas",
			 (double)vl.masseFinale, a.masse, eVL, eO1, a.cellules, pas);
	ProbeCheck(eVL < kSeuilEcartAnalytique && eO1 < kSeuilEcartAnalytique,
			   "(j1) LE CONSERVATIF colle à la masse calculée À LA MAIN", buf);

	snprintf(buf, sizeof(buf),
			 "le semi-lagrangien finit à %.9f pour une masse injectée de %.9f : facteur %.3f (exigé > %.1f). "
			 "Il ne PERD pas de la matière ici — il en FABRIQUE, et c'est désormais MESURÉ contre un nombre "
			 "calculé hors de lui, non plus déduit",
			 (double)semi.masseFinale, a.masse, fSemi, kFacteurSemiLagMin);
	ProbeCheck(fSemi > kFacteurSemiLagMin, "(j1) LE SEMI-LAGRANGIEN s'en écarte d'un facteur > 5", buf);

	// ── (j1b) CONTRÔLE NÉGATIF : injection UNIQUE, dissipation COUPÉE ──────
	// ⚠️ CE N'EST PAS « débit = 0 ». Couper l'injection donnerait 0 = 0, un témoin
	// NUL — exactement le piège que ce banc traque depuis (f1b). Une injection
	// unique garde un nombre NON NUL à atteindre, et sans dissipation l'attendu
	// devient EXACT au lieu d'approché : la masse doit rester RIGOUREUSEMENT
	// constante pendant 600 pas.
	{
		const ComptageAnalytique b = CompterALaMain(bmin, bmax, h, src, rayon, debit, dt, pas, 0.0, true);
		const ResultatPrix u = SceneDixSecondes(true, 0.f, NkFluidFluxLimiter::VanLeer, -1.f, true, 0.f);
		const float64 eU = EcartRelatif((float64)u.masseFinale, b.masse);
		const float64 amplitude =
			(u.masseMax > 0.f) ? (float64)(u.masseMax - u.masseMin) / (float64)u.masseMax : 1.0e30;
		snprintf(buf, sizeof(buf),
				 "une SEULE injection (%.9f attendue, NON NULLE — ce n'est pas « débit = 0 », qui donnerait "
				 "0 = 0 et ne prouverait rien), dissipation coupée, 600 pas : masse finale %.9f, écart %.3e ; "
				 "amplitude sur TOUTE la course (max-min)/max = %.3e — une masse qui monte puis redescend "
				 "rendrait la même valeur finale, c'est pourquoi on borne la course entière",
				 b.masse, (double)u.masseFinale, eU, amplitude);
		ProbeCheck(eU < kSeuilEcartAnalytique && amplitude < kSeuilEcartAnalytique,
				   "(j1b) NÉGATIF : injection unique sans dissipation -> masse CONSTANTE et EXACTE", buf);
	}

	// ── (j1c) MUTATION : on FAUSSE le débit attendu, le compteur DOIT rougir ──
	// Un compteur qui ne sait pas rougir n'a jamais rien prouvé en verdissant.
	{
		const ComptageAnalytique faux =
			CompterALaMain(bmin, bmax, h, src, rayon, debit * 1.01, dt, pas, 0.2, false);
		const float64 eFaux = EcartRelatif((float64)vl.masseFinale, faux.masse);
		snprintf(buf, sizeof(buf),
				 "débit attendu faussé de +1 %% (%.1f au lieu de %.1f) : M attendue passe de %.9f à %.9f, et "
				 "l'écart du conservatif passe de %.3e à %.3e — soit AU-DESSUS du seuil 1e-3. Le compteur "
				 "REFUSE bien une masse fausse de 1 %%, donc son vert de (j1) n'est pas un vert de complaisance",
				 debit * 1.01, debit, a.masse, faux.masse, eVL, eFaux);
		ProbeCheck(eFaux >= kSeuilEcartAnalytique, "(j1c) MUTATION : un débit faussé de +1 % fait ROUGIR (j1)",
				   buf);
	}

	printf("\n    ⚠️ CE QUE (j1) NE MESURE PAS : la CHALEUR. Le facteur 8,7 de l'enquête (i)\n");
	printf("    n'est pas recompté ici — la température subit un rappel vers l'ambiante,\n");
	printf("    T <- T_amb + (T - T_amb)*f, ce qui en fait un SECOND modèle à écrire.\n");
	printf("    Et rien n'est allumé : advectFluxConservative reste FAUX, le limiteur Ordre1.\n");
}

// =============================================================================
// ENQUÊTE (i) — LA FUMÉE QUI PÈSE. Mode NK_FLUID_MAC=9.
// Pré-enregistrée au § 9 de PLAN_ORDRE_SUPERIEUR.md, AVANT d'être codée.
//
// ⚠️ CE N'EST PAS UN TÉMOIN, ET ELLE NE REND AUCUN VERDICT — comme
// `EnqueteBascule` et (g2). Elle fait varier UN paramètre pour départager deux
// causes : une question posée à un seul réglage ne peut pas trancher entre elles.
//
// L'équation (8) de Fedkiw, Stam & Jensen 2001 porte DEUX termes :
//     f = − alpha·s·ŷ  +  beta·(T − T_ambiante)·ŷ
//          ^^^^^^^^^^ la FUMÉE PÈSE
// Sur la scène (e), alpha = 0,3 : ce terme est ACTIF. Or le semi-lagrangien perd
// 43,1 % de la masse de fumée — donc 43 % du poids qui retient le panache.
// (H) : une part du gouffre sur Tmax ne serait pas un défaut du schéma
// conservatif, mais la conséquence PHYSIQUEMENT JUSTE de garder ce qu'il faut
// garder. Si c'est vrai, le « prix » était en partie une CORRECTION.
// =============================================================================
void EnqueteFumeeQuiPese() {
	printf("\n=== (i) ENQUÊTE : LA FUMÉE QUI PÈSE — AUCUN VERDICT RENDU ===\n");
	printf("    Fedkiw, Stam & Jensen 2001, eq. (8) : f = -alpha*s*y + beta*(T-T_amb)*y.\n");
	printf("    Le PREMIER terme fait PESER la fumée, et le semi-lagrangien en PERD 43,1 %%.\n");
	printf("    ATTENDU, ÉCRIT AVANT (PLAN_ORDRE_SUPERIEUR.md § 9) :\n");
	printf("      (H) vraie  -> à alpha = 0, Tmax/ref monte NETTEMENT au-dessus de 0,3268 (je dis > 0,50)\n");
	printf("      (H) fausse -> Tmax/ref reste à 0,33 +/- 0,03, et la cause est ailleurs\n");
	printf("    ⚠️ VOLET NÉGATIF : si les deux Tmax ABSOLUS bougent beaucoup pendant que leur\n");
	printf("    RAPPORT ne bouge pas, alpha n'est PAS la cause — j'aurais juste changé la scène.\n");
	printf("    C'est le RAPPORT qui répond, jamais les valeurs.\n");
	printf("    ⚠️ La CHALEUR TOTALE est le second instrument, et il peut me contredire : deux\n");
	printf("    schémas qui portent la MÊME chaleur avec des Tmax dans un rapport de 3 ne\n");
	printf("    diffèrent que par la CONCENTRATION — ni alpha, ni l'ordre du schéma.\n");

	const float32 alphas[2] = {0.3f, 0.f};
	printf("\n      alpha   schéma            vmax      Tmax    Tmax/ref   masse fin.   chaleur (K.m^3)   ms/pas\n");
	float32 rap[2] = {0.f, 0.f};
	for (uint32 a = 0; a < 2; ++a) {
		const ResultatPrix s = SceneDixSecondes(false, 0.f, NkFluidFluxLimiter::Ordre1, alphas[a]);
		const ResultatPrix f = SceneDixSecondes(true, 0.f, NkFluidFluxLimiter::VanLeer, alphas[a]);
		rap[a] = (s.tmax > 0.f) ? f.tmax / s.tmax : 0.f;
		printf("      %5.2f   SEMI-LAGRANGIEN  %6.3f  %8.1f   %7s   %10.6f   %13.4f   %6.1f\n",
			   (double)alphas[a], (double)s.vmax, (double)s.tmax, "1,000 ref", (double)s.masseFinale,
			   (double)s.chaleurTotale, (double)s.msParPas);
		printf("      %5.2f   FLUX van Leer    %6.3f  %8.1f   %7.4f   %10.6f   %13.4f   %6.1f\n",
			   (double)alphas[a], (double)f.vmax, (double)f.tmax, (double)rap[a], (double)f.masseFinale,
			   (double)f.chaleurTotale, (double)f.msParPas);
		fflush(stdout);
	}
	printf("\n    LE RAPPORT, qui est la SEULE grandeur qui réponde :\n");
	printf("      alpha = 0,30 (le défaut de la scène) : Tmax/ref = %.4f\n", (double)rap[0]);
	printf("      alpha = 0,00 (le poids de la fumée COUPÉ) : Tmax/ref = %.4f\n", (double)rap[1]);
	if (rap[1] > 0.50f)
		printf("      -> (H) est SOUTENUE : couper le poids de la fumée REND le détail.\n");
	else if (rap[1] > rap[0] + 0.03f)
		printf("      -> (H) est SOUTENUE EN PARTIE : le rapport monte, sans atteindre 0,50.\n");
	else
		printf("      -> (H) est RÉFUTÉE : le rapport ne bouge pas, alpha n'est PAS la cause.\n");
	printf("    ⚠️ Aucune de ces lignes n'est un verdict. C'est une enquête, et elle DÉSIGNE\n");
	printf("    où regarder ; elle ne clôt rien et ne change aucun défaut du solveur.\n");
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
	ControleOrdreSuperieur();
}
