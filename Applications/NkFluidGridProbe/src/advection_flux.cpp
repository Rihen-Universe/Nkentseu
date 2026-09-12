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
	float32 divMax = 0.f, paroiMax = 0.f;
	for (uint32 s = 0; s < pas; ++s) {
		// Le champ est RÉ-IMPOSÉ à chaque pas : l'advection de la vitesse le
		// perturberait sinon, et on mesurerait sa dérive au lieu de mesurer le
		// bilan de masse. (Les parois restent étanches dans tous les cas.)
		PoserChampSansDivergence(g, p);
		g.Step(dt);
		if (g.Stats().divAfterMean > divMax)
			divMax = g.Stats().divAfterMean;
		const float32 paroi = g.WallLayerMass();
		if (paroi > paroiMax)
			paroiMax = paroi;
	}
	const float32 masse1 = g.TotalMass();
	const float32 derive = (masse0 > 0.f) ? ProbeAbs(masse1 - masse0) / masse0 : 1.f;

	snprintf(buf, sizeof(buf),
			 "masse %.9f -> %.9f, dérive relative %.3e sur %u pas (seuil %.0e) ; |div|*h max APRÈS "
			 "advection de la vitesse %.3e m/s (le champ POSÉ, lui, est à divergence nulle — c'est la "
			 "GARDE ci-dessus qui le mesure) ; masse touchant la paroi %.3e (elle ne peut pas SORTIR : u.n = 0)",
			 (double)masse0, (double)masse1, (double)derive, pas, (double)kSeuilDerive, (double)divMax,
			 (double)paroiMax);
	ProbeCheck(derive < kSeuilDerive, "(f1) la masse se conserve à l'epsilon machine", buf);
}

// =============================================================================
void PalierAdvectionFlux() {
	printf("\n=== (f) ADVECTION CONSERVATIVE EN FLUX — Lentine, Aanjaneya & Fedkiw, SCA 2011 ===\n");
	printf("    ⚠️ (f1) est ROUGE tant que l'advection reste SEMI-LAGRANGIENNE : elle n'a aucun\n");
	printf("    bilan, son interpolation redistribue sans vérifier que ce qui part arrive.\n");
	printf("    Un schéma en FLUX est conservatif PAR CONSTRUCTION — il doit donc verdir au\n");
	printf("    PREMIER jet, sinon le portage est faux. Voir PLAN_ADVECTION_FLUX.md, § 1.\n");

	ControleConservation();
}
