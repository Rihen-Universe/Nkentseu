// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// grille_mac.cpp — (m1) et (m2) : LES DEUX CONTRÔLES QUI NAISSENT AVEC LA
// BASCULE vers la GRILLE DÉCALÉE (MAC) de Francis H. HARLOW & J. Eddie WELCH,
// « Numerical Calculation of Time-Dependent Viscous Incompressible Flow of Fluid
// with Free Surface », Physics of Fluids 8 (1965), p. 2182-2189.
//
// ⚠️ CES DEUX CONTRÔLES SONT ÉCRITS AVANT LA BASCULE, ET ILS SONT ROUGES
// AUJOURD'HUI. C'EST LEUR RAISON D'ÊTRE. Le plan (PLAN_GRILLE_MAC.md, § 4) fixe
// l'ordre : les deux contrôles neufs d'abord, SEULS, avant que la moindre
// équation ne bouge. Un témoin qui naîtrait VERT le jour de la bascule ne
// prouverait rien — on ne saurait pas s'il juge la bascule ou s'il juge que
// « ça compile ». Ils se vérifient sur des champs ANALYTIQUES et ne dépendent
// d'AUCUN solveur : ni projection, ni advection, ni flottabilité.
//
// ── LA CONVENTION DE FACE, écrite ici parce que c'est elle qui décide de tout ─
// Elle ne change PAS l'allocation : les trois tableaux gardent leurs
// (nx+2)(ny+2)(nz+2) cases et leur `Idx(i,j,k)`. C'est leur SENS qui change.
//   AUJOURD'HUI (colocalisée)  : mU[Idx(i,j,k)] = u au CENTRE de la cellule.
//   APRÈS LA BASCULE (décalée) : mU[Idx(i,j,k)] = u sur la face GAUCHE de la
//                                cellule (i,j,k), à x = boundsMin.x + (i-1)*h.
// Le centre de la cellule (i,j,k) reste à x = boundsMin.x + (i-0.5)*h. La face
// DROITE de la cellule i est donc la face GAUCHE de la cellule i+1 : une seule
// case pour une seule face, jamais deux — c'est exactement ce qui rend la
// divergence COMPACTE et l'adjonction exacte avec le Laplacien de pression.
// Idem pour v (faces y, indice j) et w (faces z, indice k).
//
// ── CE QUE (m1) ET (m2) PROUVENT, ET DANS QUEL SENS ──────────────────────────
// (m1) L'INTERPOLATION FACE -> CENTRE, le chemin neuf le plus dangereux : tout
//      ce qui lit la vitesse au centre d'une cellule (flottabilité, vorticité,
//      statistiques, rendu, vent) passera désormais par une moyenne de DEUX
//      faces. Deux volets, et le second n'est pas décoratif :
//        - LINÉAIRE : la moyenne des deux faces vaut EXACTEMENT la valeur au
//          centre (u(x) = a*x : (a*x_g + a*x_d)/2 = a*(x_g+x_d)/2 = a*x_c).
//        - QUADRATIQUE : l'erreur doit être NON NULLE et valoir h^2/8 * f''.
//          En effet (f(x_c - h/2) + f(x_c + h/2))/2 = f(x_c) + (h/2)^2/2 * f''.
//          Sans ce second volet, le premier ne prouverait que « ça compile » :
//          une fonction qui rendrait bêtement la valeur d'UNE face passerait le
//          volet linéaire dès que la face coïncide avec le centre.
//
// (m2) LE DAMIER — LE SEUL QUI PROUVE QUE LA BASCULE A SERVI À QUELQUE CHOSE.
//      Un champ +1/-1 alterné d'une face à l'autre a une divergence ÉNORME et
//      parfaitement connue : div*h = u[i+1] - u[i] = -/+ 2 m/s dans CHAQUE
//      cellule. La divergence CENTRÉE d'aujourd'hui lit u[i+1] - u[i-1], deux
//      cases de MÊME parité, donc deux valeurs ÉGALES : elle rend ZÉRO. Elle est
//      AVEUGLE au mode damier, et c'est précisément cet aveuglement qu'on
//      supprime — c'est lui qui découple la grille en sous-réseaux pair/impair
//      et qui pose le plancher de discrétisation du témoin (b).
//      Donc : ROUGE avant la bascule (mesure 0 au lieu de 2), VERT après.
//
// ⚠️ CE QUE CES DEUX CONTRÔLES NE PROUVENT PAS, et il faut le dire avant de les
// lire : RIEN sur la divergence résiduelle après projection, rien sur le critère
// décisif du § 1 du plan, rien sur la MASSE (l'advection reste
// semi-lagrangienne, et sa perte de 45,9 % est SPATIALE — la bascule n'est pas
// censée la corriger). Ils ne jugent que DEUX opérateurs, sur des champs dont la
// réponse est écrite à la main.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGrid.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);

// Les coefficients des champs analytiques. Trois valeurs DIFFÉRENTES, et une
// négative : un contrôle qui prendrait a = b = c ne verrait pas un axe recopié
// sur un autre, et c'est l'erreur la plus probable quand on écrit trois branches
// qui se ressemblent.
static const float32 kA = 3.f;	// composante x
static const float32 kB = -2.f; // composante y
static const float32 kC = 5.f;	// composante z

// =============================================================================
// (m1) INTERPOLATION FACE -> CENTRE
// =============================================================================
static void ControleInterpolationFaceCentre() {
	printf("\n--- (m1) INTERPOLATION FACE -> CENTRE (les deux volets) ---\n");
	printf("    face x d'indice i : x = boundsMin.x + (i-1)*h ; centre de la cellule i : (i-0.5)*h\n");

	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {0.4f, 0.4f, 0.4f};
	p.cellSize = 0.05f; // 8 x 8 x 8
	p.projectionEnabled = false;
	p.advectionEnabled = false;
	p.buoyancyEnabled = false;

	NkFluidGrid g;
	if (!g.Init(p)) {
		ProbeCheck(false, "(m1) Init de la grille", "Init a rendu false");
		return;
	}
	const float32 h = g.CellSize();
	char buf[420];

	float32 *u = const_cast<float32 *>(g.VelocityX());
	float32 *v = const_cast<float32 *>(g.VelocityY());
	float32 *w = const_cast<float32 *>(g.VelocityZ());

	// La position de la FACE d'indice i/j/k, dans la convention ci-dessus.
	auto faceX = [&](uint32 i) { return p.boundsMin.x + ((float32)i - 1.f) * h; };
	auto faceY = [&](uint32 j) { return p.boundsMin.y + ((float32)j - 1.f) * h; };
	auto faceZ = [&](uint32 k) { return p.boundsMin.z + ((float32)k - 1.f) * h; };
	// La position du CENTRE de la cellule i/j/k — elle, ne change pas.
	auto centreX = [&](uint32 i) { return p.boundsMin.x + ((float32)i - 0.5f) * h; };
	auto centreY = [&](uint32 j) { return p.boundsMin.y + ((float32)j - 0.5f) * h; };
	auto centreZ = [&](uint32 k) { return p.boundsMin.z + ((float32)k - 0.5f) * h; };

	// ── VOLET POSITIF : champ LINÉAIRE, l'erreur doit être NULLE ─────────────
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i) {
				const uint32 id = g.Idx(i, j, k);
				u[id] = kA * faceX(i);
				v[id] = kB * faceY(j);
				w[id] = kC * faceZ(k);
			}

	float32 pireLin = 0.f;
	uint32 iPire = 0, jPire = 0, kPire = 0;
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i) {
				float32 cx = 0.f, cy = 0.f, cz = 0.f;
				g.VelocityAtCenter(i, j, k, cx, cy, cz);
				const float32 ex = ProbeAbs(cx - kA * centreX(i));
				const float32 ey = ProbeAbs(cy - kB * centreY(j));
				const float32 ez = ProbeAbs(cz - kC * centreZ(k));
				const float32 e = (ex > ey) ? ((ex > ez) ? ex : ez) : ((ey > ez) ? ey : ez);
				if (e > pireLin) {
					pireLin = e;
					iPire = i;
					jPire = j;
					kPire = k;
				}
			}
	// Les valeurs du champ montent à 5 * 0,4 = 2 m/s : 1e-6 est bien au-dessus de
	// l'epsilon machine de la simple précision à cette échelle, et bien en dessous
	// du demi-pas (a*h/2 = 0,075 m/s) qu'une grille colocalisée rendrait.
	snprintf(buf, sizeof(buf),
			 "champ u = (%.0f*x, %.0f*y, %.0f*z) posé sur les FACES : erreur max %.3e m/s au centre (cellule %u,%u,%u) "
			 "— exigé <= 1,0e-06 (la moyenne de deux faces est EXACTE sur un champ linéaire)",
			 (double)kA, (double)kB, (double)kC, (double)pireLin, iPire, jPire, kPire);
	ProbeCheck(pireLin <= 1.0e-6f, "(m1) POSITIF : champ LINÉAIRE, moyenne des faces = valeur au centre", buf);

	// ── VOLET NÉGATIF : champ QUADRATIQUE, l'erreur doit être h^2/8 * f'' ────
	// f = a*x^2  =>  f'' = 2a  =>  erreur attendue = h^2/8 * 2a = a*h^2/4.
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i) {
				const uint32 id = g.Idx(i, j, k);
				u[id] = kA * faceX(i) * faceX(i);
				v[id] = kB * faceY(j) * faceY(j);
				w[id] = kC * faceZ(k) * faceZ(k);
			}

	const float32 attX = kA * h * h * 0.25f;
	const float32 attY = kB * h * h * 0.25f;
	const float32 attZ = kC * h * h * 0.25f;
	float32 pireQuad = 0.f;
	for (uint32 k = 1; k <= g.Nz(); ++k)
		for (uint32 j = 1; j <= g.Ny(); ++j)
			for (uint32 i = 1; i <= g.Nx(); ++i) {
				float32 cx = 0.f, cy = 0.f, cz = 0.f;
				g.VelocityAtCenter(i, j, k, cx, cy, cz);
				// L'ERREUR SIGNÉE de l'interpolation, comparée à sa valeur analytique.
				const float32 ex = (cx - kA * centreX(i) * centreX(i)) - attX;
				const float32 ey = (cy - kB * centreY(j) * centreY(j)) - attY;
				const float32 ez = (cz - kC * centreZ(k) * centreZ(k)) - attZ;
				const float32 e = (ProbeAbs(ex) > ProbeAbs(ey))
									  ? ((ProbeAbs(ex) > ProbeAbs(ez)) ? ProbeAbs(ex) : ProbeAbs(ez))
									  : ((ProbeAbs(ey) > ProbeAbs(ez)) ? ProbeAbs(ey) : ProbeAbs(ez));
				if (e > pireQuad)
					pireQuad = e;
			}
	snprintf(buf, sizeof(buf),
			 "champ u = (%.0f*x^2, %.0f*y^2, %.0f*z^2) : erreur d'interpolation attendue h^2/8*f'' = a*h^2/4 = "
			 "(%+.6f, %+.6f, %+.6f) m/s ; écart max à CETTE valeur %.3e (exigé <= 1,0e-06) — et elle est NON NULLE, "
			 "sans quoi le volet positif ne prouverait que « ça compile »",
			 (double)kA, (double)kB, (double)kC, (double)attX, (double)attY, (double)attZ, (double)pireQuad);
	ProbeCheck(pireQuad <= 1.0e-6f && ProbeAbs(attX) > 1.0e-6f,
			   "(m1) NÉGATIF : champ QUADRATIQUE, l'erreur vaut h^2/8 * f''", buf);
}

// =============================================================================
// (m2) LE DAMIER
// =============================================================================
static void ControleDamier() {
	printf("\n--- (m2) LE DAMIER — la divergence est-elle AVEUGLE au mode pair/impair ? ---\n");

	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {0.4f, 0.4f, 0.4f};
	p.cellSize = 0.05f; // 8 x 8 x 8 -> 6^3 = 216 cellules STRICTES
	p.projectionEnabled = false;
	p.advectionEnabled = false;
	p.buoyancyEnabled = false;

	NkFluidGrid g;
	if (!g.Init(p)) {
		ProbeCheck(false, "(m2) Init de la grille", "Init a rendu false");
		return;
	}
	char buf[420];

	// Le damier : +1 sur les faces x d'indice PAIR, -1 sur les impaires. v et w
	// restent nuls — la divergence attendue ne vient que de la composante x, et
	// une contribution parasite en y ou z se verrait donc immédiatement.
	float32 *u = const_cast<float32 *>(g.VelocityX());
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i)
				u[g.Idx(i, j, k)] = ((i % 2u) == 0u) ? 1.f : -1.f;

	// Un pas INERTE : projection, advection et flottabilité sont coupées, donc
	// seule la MESURE travaille. C'est le même montage que les autres contrôles
	// d'instrument du banc, et c'est le VRAI compteur de divergence qui répond —
	// pas une formule recopiée dans le test.
	g.Step(1.0e-4f);

	// div*h attendu, cellule par cellule : u[i+1] - u[i] = -/+ 2 m/s.
	const float32 attendu = 2.f;
	const float32 mesureStrict = g.Stats().divAfterMeanStrict;
	const float32 mesureTout = g.Stats().divAfterMean;
	snprintf(buf, sizeof(buf),
			 "|div|*h moyen : STRICT %.6f m/s sur %u cellules, tout l'intérieur %.6f m/s — attendu %.6f m/s "
			 "(u[i+1]-u[i] = -/+2). ZÉRO = la divergence CENTRÉE lit u[i+1]-u[i-1], deux cases de MÊME parité, "
			 "donc deux valeurs ÉGALES : elle ne VOIT PAS le damier",
			 (double)mesureStrict, g.Stats().cellsStrict, (double)mesureTout, (double)attendu);
	ProbeCheck(ProbeAbs(mesureStrict - attendu) <= 1.0e-4f, "(m2) le DAMIER rend une divergence NON NULLE et connue",
			   buf);

	// ── contrôle NÉGATIF du damier : un champ UNIFORME reste à divergence nulle.
	// Sans lui, (m2) serait passé au vert par n'importe quel opérateur bruyant :
	// « non nul » n'est pas un critère, « non nul ICI et nul LÀ » en est un.
	g.Reset();
	g.SetUniformVelocity({1.f, -1.f, 1.f});
	g.Step(1.0e-4f);
	snprintf(buf, sizeof(buf), "champ uniforme (1, -1, 1) : |div|*h STRICT = %.3e m/s (exigé <= 1,0e-05) — le même "
							   "opérateur qui doit VOIR le damier doit rester AVEUGLE à un champ uniforme",
			 (double)g.Stats().divAfterMeanStrict);
	ProbeCheck(g.Stats().divAfterMeanStrict <= 1.0e-5f, "contrôle NÉGATIF de (m2) : champ uniforme -> divergence nulle",
			   buf);
}

// =============================================================================
void PalierGrilleMAC() {
	printf("\n=== (m) LES DEUX CONTRÔLES DE LA BASCULE MAC (Harlow & Welch 1965) ===\n");
	printf("    ⚠️ ROUGES ATTENDUS TANT QUE LA GRILLE EST COLOCALISÉE. Ils sont écrits AVANT\n");
	printf("    la bascule, exprès : un témoin qui naîtrait vert ne dirait pas s'il juge la\n");
	printf("    bascule ou s'il juge que « ça compile ». Voir PLAN_GRILLE_MAC.md, § 2 et § 4.\n");

	ControleInterpolationFaceCentre();
	ControleDamier();
}
