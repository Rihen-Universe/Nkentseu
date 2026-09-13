// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// volutes.cpp — LA TAILLE DES TOURBILLONS, EN CELLULES (2026-09-12).
//
// LA QUESTION : le 07/09, les deux images du confinement montraient un panache
// « plus large et STRUCTURÉ, mais sans grosses volutes qui s'enroulent (boîte de
// 0,5 m, caméra proche : les tourbillons entretenus restent de la taille de
// quelques cellules) ». Cette parenthèse est une PRÉDICTION MESURABLE. Est-ce la
// BOÎTE (résolution : la source fait 3 cellules de rayon, rien de plus gros ne
// peut s'enrouler autour) ou le SCHÉMA (la dissipation numérique et le
// confinement en epsilon·h accrochent les structures à la grille, quel que soit h) ?
//
// PRÉ-ENREGISTREMENT COMPLET : PLAN_VOLUTES.md, à côté du solveur, commité AVANT
// ce fichier. Ce qui doit être lu ICI, parce que c'est ce qui décide :
//
//  (n1) L'INSTRUMENT. Sur l'intérieur STRICT, avec omega TEL QUE LE SOLVEUR LE
//       CALCULE (accesseurs en lecture seule, aucune physique touchée) :
//          R(r) = somme omega(i)·omega(i+r) / somme omega(i)·omega(i)
//       produit SCALAIRE signé (deux tourbillons contra-rotatifs sont ANTI-
//       corrélés : c'est ce qui sépare « un tourbillon » d'« une nappe »).
//       L = intégrale de R jusqu'au PREMIER ZÉRO (trapèzes, zéro interpolé).
//       Pas de zéro -> l'instrument REND FALSE, il ne fabrique pas un chiffre.
//       DIAMÈTRE CALIBRÉ : sur un tube de rotation solide de rayon R,
//       L = 8R/(3 pi) analytiquement, donc D = 2R = (3 pi / 4) · L.
//       Le facteur est DÉRIVÉ, pas ajusté ; deux rayons différents le vérifient.
//       Contrôles : POSITIF R = 6 (D = 12 ± 12 %), POSITIF R = 3 (D = 6 ± 20 %,
//       rapport dans [1,8 ; 2,2]), NÉGATIFS : bruit blanc (D < 2), rotation
//       uniforme (false), grille vide (false).
//  (n2) BOÎTE OU SCHÉMA. (n2a) boîte ÷ 2 à cellules constantes (littéral, parois
//       plus proches : confondant DIT). (n2b) même boîte, h : 2 -> 1 cm, seule
//       chose qui change. RÈGLE, sur (n2b) scène B, écrite avant :
//          r = D_cellules(1 cm) / D_cellules(2 cm)
//          r >= 1,6 -> C'EST LA BOÎTE (la taille est fixée par la physique, en
//                      mètres ; la grille était trop grosse). ON S'ARRÊTE.
//          r <= 1,3 -> C'EST LE SCHÉMA (la taille est accrochée à la grille).
//          entre    -> indéterminé, dit tel quel.
//  (n3) CONTRÔLES NÉGATIFS : déterminisme (identique au dernier chiffre),
//       translation de la boîte et de la source (1e-4 relatif), fidélité de la
//       copie contre ConstruirePanache de rendu.cpp appelée TELLE QUELLE
//       (identique au dernier chiffre). La caméra n'entre pas : aucun pixel n'est
//       lu — dit, pas éprouvé (un témoin qui ne peut pas rougir ne témoigne pas).
//
// CE QUE CE FICHIER NE FAIT PAS : aucune ligne du solveur ne change ; aucun
// verdict sur « ça tournoie à l'œil » (l'image à 1 cm est rendue avec la MÊME
// caméra que celles du 07/09, comme aide à l'œil, pas comme preuve) ; aucun GPU.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGrid.h"
#include "NKRenderer/Tools/VFX/NkFluidGridRaymarch.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);
float32 EpsilonConfinement();
// rendu.cpp — appelées telles quelles (voir (n3c) et l'image de (n2b)).
void ConstruirePanache(NkFluidGrid &g, bool avecFeu, uint32 pas, float32 epsilon);
bool EcrirePng(const NkVector<uint8> &rgba, uint32 W, uint32 H, const char *chemin);

// Le facteur de calibration, DÉRIVÉ (PLAN_VOLUTES.md § 1.2) : D = (3 pi / 4) L.
static const float32 kPiSur4Fois3 = 2.35619449f;
static const uint32 kFenetre = 60; // les 60 DERNIERS pas, moyennés

// =============================================================================
// L'INSTRUMENT
// =============================================================================
struct EchelleVolutes {
		float32 Lx = 0.f, Lz = 0.f; // échelles intégrales, en cellules
		float32 L = 0.f;			// (Lx + Lz) / 2
		float32 D = 0.f;			// diamètre calibré, en cellules
		float32 r0x = 0.f, r0z = 0.f; // premier zéro (interpolé), en cellules
		bool valide = false;
};

// R(r) le long d'un axe (0 = x, 2 = z), population STRICTE. `R` reçoit rmax+1
// valeurs. Rend false si le dénominateur est nul.
static bool Autocorrelation(const NkFluidGrid &g, int axe, float64 *R, uint32 rmax) {
	const uint32 nx = g.Nx(), ny = g.Ny(), nz = g.Nz();
	if (nx < 3 || ny < 3 || nz < 3)
		return false;
	const float32 *ox = g.OmegaX();
	const float32 *oy = g.OmegaY();
	const float32 *oz = g.OmegaZ();
	float64 denom = 0.0;
	for (uint32 k = 2; k <= nz - 1; ++k)
		for (uint32 j = 2; j <= ny - 1; ++j)
			for (uint32 i = 2; i <= nx - 1; ++i) {
				const uint32 id = g.Idx(i, j, k);
				denom += (float64)ox[id] * ox[id] + (float64)oy[id] * oy[id] + (float64)oz[id] * oz[id];
			}
	if (denom <= 0.0)
		return false;
	for (uint32 r = 0; r <= rmax; ++r) {
		float64 num = 0.0;
		for (uint32 k = 2; k <= nz - 1; ++k)
			for (uint32 j = 2; j <= ny - 1; ++j)
				for (uint32 i = 2; i <= nx - 1; ++i) {
					const uint32 i2 = (axe == 0) ? i + r : i;
					const uint32 k2 = (axe == 2) ? k + r : k;
					if (i2 > nx - 1 || k2 > nz - 1)
						continue;
					const uint32 a = g.Idx(i, j, k), b = g.Idx(i2, j, k2);
					num += (float64)ox[a] * ox[b] + (float64)oy[a] * oy[b] + (float64)oz[a] * oz[b];
				}
		R[r] = num / denom;
	}
	return true;
}

// Intégrale de R jusqu'au premier zéro. Rend false s'il n'y en a pas.
static bool EchelleIntegrale(const float64 *R, uint32 rmax, float32 &Lout, float32 &r0out) {
	float64 L = 0.0;
	for (uint32 r = 0; r < rmax; ++r) {
		if (R[r + 1] <= 0.0) {
			// zéro interpolé entre r (R > 0) et r+1 (R <= 0)
			const float64 t = R[r] / (R[r] - R[r + 1]); // dans ]0, 1]
			L += 0.5 * R[r] * t;
			Lout = (float32)L;
			r0out = (float32)r + (float32)t;
			return true;
		}
		L += 0.5 * (R[r] + R[r + 1]);
	}
	return false; // jamais passé par zéro sur la plage disponible
}

static EchelleVolutes MesurerEchelle(const NkFluidGrid &g) {
	EchelleVolutes e;
	const uint32 nx = g.Nx(), nz = g.Nz();
	if (nx < 4 || nz < 4)
		return e;
	const uint32 rmaxX = nx - 3, rmaxZ = nz - 3; // toute la plage stricte
	NkVector<float64> Rx, Rz;
	Rx.Resize(rmaxX + 1, 0.0);
	Rz.Resize(rmaxZ + 1, 0.0);
	if (!Autocorrelation(g, 0, Rx.Data(), rmaxX) || !Autocorrelation(g, 2, Rz.Data(), rmaxZ))
		return e;
	if (!EchelleIntegrale(Rx.Data(), rmaxX, e.Lx, e.r0x) || !EchelleIntegrale(Rz.Data(), rmaxZ, e.Lz, e.r0z))
		return e;
	e.L = 0.5f * (e.Lx + e.Lz);
	e.D = kPiSur4Fois3 * e.L;
	e.valide = true;
	return e;
}

// =============================================================================
// (n1') LE SECOND INSTRUMENT — le critère Q (Hunt, Wray & Moin 1988), ADDENDUM
// DATÉ du 12/09 20h30 (PLAN_VOLUTES.md § 5bis), écrit APRÈS la première course
// de (n1) et AVANT sa propre première mesure. Ce que (n1) a appris sur un
// panache : il s'arrête au premier zéro de la corrélation, c'est-à-dire à
// l'ÉPAISSEUR de la nappe de cisaillement — grid-mince à toute résolution dans
// un solveur sans viscosité. Une volute est une nappe mince qui S'ENROULE, et
// c'est l'enroulement qu'il faut voir : Q = ½(|Omega|² − |S|²) > 0 là où la
// rotation domine le cisaillement (le cœur d'une spire), Q = 0 dans un
// cisaillement simple (un jet droit). (n1) reste tel quel ; il se DOUBLE.
//   d = 2 sqrt(A/pi), A = V / nj (section horizontale moyenne d'une composante
//   connexe de Q >= 0,01 Qmax, 6-voisinage) — EXACT sur un cylindre. Publié :
//   la moyenne des d pondérée par l'enstrophie des composantes, le d de la plus
//   lourde, leur nombre, la fraction d'enstrophie portée par Q > 0. FALSE si la
//   plus lourde touche une paroi LATÉRALE (seule une paroi latérale tronque une
//   section horizontale) ; sol et plafond sont des DRAPEAUX.
// =============================================================================
struct EchelleQ {
		float32 dPondere = 0.f; // moyenne des d, pondérée par l'enstrophie des composantes
		float32 dLourde = 0.f;	// d de la composante la plus lourde
		uint32 composantes = 0;
		uint32 cellulesQ = 0;		 // cellules retenues (Q >= 0,01 Qmax)
		float32 fractionEnstrophie = 0.f; // part de l'enstrophie stricte dans Q > 0
		float32 qMax = 0.f;
		bool toucheSol = false, touchePlafond = false;
		bool valide = false;
};

static EchelleQ MesurerQ(const NkFluidGrid &g) {
	EchelleQ e;
	const uint32 nx = g.Nx(), ny = g.Ny(), nz = g.Nz();
	if (nx < 4 || ny < 4 || nz < 4)
		return e;
	const float32 *U = g.VelocityX();
	const float32 *V = g.VelocityY();
	const float32 *W = g.VelocityZ();
	const float32 *om = g.OmegaMag();
	const float32 inv2h = 1.f / (2.f * g.CellSize());
	// vitesses AU CENTRE (moyenne des deux faces), le pochoir de ComputeVorticity
	auto uc = [&](uint32 a, uint32 b, uint32 c) { return 0.5f * (U[g.Idx(a, b, c)] + U[g.Idx(a + 1, b, c)]); };
	auto vc = [&](uint32 a, uint32 b, uint32 c) { return 0.5f * (V[g.Idx(a, b, c)] + V[g.Idx(a, b + 1, c)]); };
	auto wc = [&](uint32 a, uint32 b, uint32 c) { return 0.5f * (W[g.Idx(a, b, c)] + W[g.Idx(a, b, c + 1)]); };
	const uint32 total = (nx + 2) * (ny + 2) * (nz + 2);
	NkVector<float32> Q;
	Q.Resize(total, 0.f);
	float32 qmax = 0.f;
	float64 ensStrict = 0.0;
	for (uint32 k = 2; k <= nz - 1; ++k)
		for (uint32 j = 2; j <= ny - 1; ++j)
			for (uint32 i = 2; i <= nx - 1; ++i) {
				// G[a][b] = d u_b / d x_a
				float32 G[3][3];
				G[0][0] = (uc(i + 1, j, k) - uc(i - 1, j, k)) * inv2h;
				G[0][1] = (vc(i + 1, j, k) - vc(i - 1, j, k)) * inv2h;
				G[0][2] = (wc(i + 1, j, k) - wc(i - 1, j, k)) * inv2h;
				G[1][0] = (uc(i, j + 1, k) - uc(i, j - 1, k)) * inv2h;
				G[1][1] = (vc(i, j + 1, k) - vc(i, j - 1, k)) * inv2h;
				G[1][2] = (wc(i, j + 1, k) - wc(i, j - 1, k)) * inv2h;
				G[2][0] = (uc(i, j, k + 1) - uc(i, j, k - 1)) * inv2h;
				G[2][1] = (vc(i, j, k + 1) - vc(i, j, k - 1)) * inv2h;
				G[2][2] = (wc(i, j, k + 1) - wc(i, j, k - 1)) * inv2h;
				float32 o2 = 0.f, s2 = 0.f;
				for (int a = 0; a < 3; ++a)
					for (int b = 0; b < 3; ++b) {
						const float32 oab = 0.5f * (G[a][b] - G[b][a]);
						const float32 sab = 0.5f * (G[a][b] + G[b][a]);
						o2 += oab * oab;
						s2 += sab * sab;
					}
				const float32 q = 0.5f * (o2 - s2);
				const uint32 id = g.Idx(i, j, k);
				Q[id] = q;
				if (q > qmax)
					qmax = q;
				ensStrict += (float64)om[id] * om[id];
			}
	e.qMax = qmax;
	if (qmax <= 0.f)
		return e; // aucune rotation dominante nulle part (cisaillement pur, grille vide)
	const float32 seuil = 0.01f * qmax;
	// composantes connexes, 6-voisinage, parcours par pile explicite
	NkVector<int32> label;
	label.Resize(total, -1);
	NkVector<uint32> pile;
	float64 dPond = 0.0, poidsTotal = 0.0, ensQ = 0.0;
	float64 poidsLourde = -1.0;
	for (uint32 k = 2; k <= nz - 1; ++k)
		for (uint32 j = 2; j <= ny - 1; ++j)
			for (uint32 i = 2; i <= nx - 1; ++i) {
				const uint32 id0 = g.Idx(i, j, k);
				if (Q[id0] < seuil || label[id0] >= 0)
					continue;
				const int32 c = (int32)e.composantes++;
				label[id0] = c;
				pile.Clear();
				pile.PushBack(id0);
				uint32 vol = 0, jmin = ny, jmax = 0;
				bool lateral = false, sol = false, plafond = false;
				float64 ens = 0.0;
				const uint32 sx = nx + 2, sxy = (nx + 2) * (ny + 2);
				while (pile.Size() > 0) {
					const uint32 id = pile[pile.Size() - 1];
					pile.PopBack();
					++vol;
					ens += (float64)om[id] * om[id];
					const uint32 ci = id % sx, cj = (id / sx) % (ny + 2), ck = id / sxy;
					if (cj < jmin) jmin = cj;
					if (cj > jmax) jmax = cj;
					if (ci == 2 || ci == nx - 1 || ck == 2 || ck == nz - 1) lateral = true;
					if (cj == 2) sol = true;
					if (cj == ny - 1) plafond = true;
					const uint32 voisins[6] = {id - 1, id + 1, id - sx, id + sx, id - sxy, id + sxy};
					const bool ok[6] = {ci > 2, ci < nx - 1, cj > 2, cj < ny - 1, ck > 2, ck < nz - 1};
					for (int n = 0; n < 6; ++n) {
						if (!ok[n]) continue;
						const uint32 v = voisins[n];
						if (Q[v] >= seuil && label[v] < 0) {
							label[v] = c;
							pile.PushBack(v);
						}
					}
				}
				const float32 nj = (float32)(jmax - jmin + 1);
				const float32 A = (float32)vol / nj;
				const float32 d = 2.f * NkSqrt(A / 3.14159265f);
				e.cellulesQ += vol;
				dPond += (float64)d * ens;
				poidsTotal += ens;
				ensQ += ens;
				if (ens > poidsLourde) {
					poidsLourde = ens;
					e.dLourde = d;
					e.toucheSol = sol;
					e.touchePlafond = plafond;
					e.valide = !lateral;
				}
			}
	if (e.composantes == 0 || poidsTotal <= 0.0) {
		e.valide = false;
		return e;
	}
	e.dPondere = (float32)(dPond / poidsTotal);
	e.fractionEnstrophie = (ensStrict > 0.0) ? (float32)(ensQ / ensStrict) : 0.f;
	return e;
}

// =============================================================================
// 0. CONTRÔLES DE L'INSTRUMENT
// =============================================================================
// Un tourbillon de RANKINE d'axe y, rayon de cœur `rayonCellules`, posé sur les
// FACES (u en face x, w en face z) : rotation solide u = Omega x r DANS le cœur,
// champ IRROTATIONNEL u_theta = Omega R^2 / r AU-DEHORS. C'est le seul champ qui
// soit « omega uniforme dans un disque, nul dehors », l'hypothèse de la
// calibration 8R/(3 pi).
// ⚠️ PIÈGE PAYÉ le 12/09, à la première course : une rotation solide TRONQUÉE
// (vitesse nulle hors du disque) n'est PAS ce champ. La vitesse y saute de
// Omega R à 0 sur le bord : c'est une NAPPE de vorticité de signe opposé, dont la
// circulation annule celle du disque, et dont l'enstrophie DOMINE (à R = 6 elle
// vaut ~ Omega R / h par cellule contre 2 Omega dans le cœur). L'instrument, qui
// est juste, mesurait alors la nappe : D = 3,65 pour 12 attendu, D = 2,77 pour 6,
// rapport 1,32. Les seuils n'ont pas bougé d'un chiffre ; le montage a été mis
// en accord avec sa propre dérivation. Un contrôle positif qui rougit sur un
// montage faux est exactement ce à quoi il sert.
// `rayonCellules` <= 0 : rotation solide sur TOUT le domaine (contrôle négatif 2).
static void PoserTube(NkFluidGrid &g, float32 rayonCellules, float32 omega0) {
	const float32 h = g.CellSize();
	const NkVec3f bmin = g.Params().boundsMin;
	const float32 cx = bmin.x + 0.5f * (float32)g.Nx() * h;
	const float32 cz = bmin.z + 0.5f * (float32)g.Nz() * h;
	const float32 R = rayonCellules * h;
	float32 *u = const_cast<float32 *>(g.VelocityX());
	float32 *w = const_cast<float32 *>(g.VelocityZ());
	for (uint32 k = 0; k <= g.Nz() + 1; ++k)
		for (uint32 j = 0; j <= g.Ny() + 1; ++j)
			for (uint32 i = 0; i <= g.Nx() + 1; ++i) {
				const uint32 id = g.Idx(i, j, k);
				// face x d'indice i : x = bmin.x + (i-1) h, z au centre (k-0,5) h
				{
					const float32 x = bmin.x + ((float32)i - 1.f) * h - cx;
					const float32 z = bmin.z + ((float32)k - 0.5f) * h - cz;
					const float32 r2 = x * x + z * z;
					const bool dedans = (R <= 0.f) || (r2 <= R * R);
					// dedans : Omega z ; dehors : Omega R^2 z / r^2 (u_theta = Omega R^2 / r)
					u[id] = dedans ? omega0 * z : omega0 * R * R * z / r2;
				}
				// face z d'indice k : z = bmin.z + (k-1) h, x au centre (i-0,5) h
				{
					const float32 x = bmin.x + ((float32)i - 0.5f) * h - cx;
					const float32 z = bmin.z + ((float32)k - 1.f) * h - cz;
					const float32 r2 = x * x + z * z;
					const bool dedans = (R <= 0.f) || (r2 <= R * R);
					w[id] = dedans ? -omega0 * x : -omega0 * R * R * x / r2;
				}
			}
}

static bool GrilleInerte(NkFluidGrid &g, float32 h, uint32 nx, uint32 ny, uint32 nz) {
	NkFluidGridParams p;
	p.boundsMin = {0.f, 0.f, 0.f};
	p.boundsMax = {(float32)nx * h, (float32)ny * h, (float32)nz * h};
	p.cellSize = h;
	p.projectionEnabled = false;
	p.advectionEnabled = false;
	p.buoyancyEnabled = false;
	return g.Init(p) && g.Nx() == nx && g.Ny() == ny && g.Nz() == nz;
}

static void ControlesInstrument() {
	printf("\n=== 0ter. CONTRÔLES DE L'INSTRUMENT D'ÉCHELLE (PLAN_VOLUTES.md § 1.3) ===\n");
	printf("    D = (3 pi / 4) L, facteur DÉRIVÉ sur un tube de rotation solide, pas ajusté.\n");
	char buf[400];
	NkFluidGrid g;
	// 40 x 20 x 40 : assez large pour qu'un tube de rayon 6 (zéro attendu à r = 12)
	// tienne avec sa plage de corrélation, sans que la troncature du domaine morde.
	if (!GrilleInerte(g, 0.025f, 40, 20, 40)) {
		ProbeCheck(false, "Init (contrôles d'échelle)", "Init a rendu false ou mauvaise population");
		return;
	}

	// --- POSITIF 1 : R = 6 cellules -> D = 12 ------------------------------
	g.Reset();
	PoserTube(g, 6.f, 2.f);
	g.Step(1.0e-5f); // pas inerte : seul ComputeVorticity travaille
	const EchelleVolutes e6 = MesurerEchelle(g);
	snprintf(buf, sizeof(buf), "Rankine R = 6 : Lx %.4f, Lz %.4f (zéros à r = %.2f / %.2f) -> D = %.4f cellules, attendu 12 (± 12 %%)",
			 (double)e6.Lx, (double)e6.Lz, (double)e6.r0x, (double)e6.r0z, (double)e6.D);
	ProbeCheck(e6.valide && ProbeAbs(e6.D - 12.f) <= 0.12f * 12.f, "contrôle POSITIF 1 de l'échelle (Rankine R = 6)", buf);
	snprintf(buf, sizeof(buf), "Lx / Lz = %.4f (les deux axes horizontaux doivent s'accorder à 5 %%)",
			 (double)(e6.Lz > 0.f ? e6.Lx / e6.Lz : 0.f));
	ProbeCheck(e6.valide && e6.Lz > 0.f && ProbeAbs(e6.Lx / e6.Lz - 1.f) <= 0.05f,
			   "contrôle POSITIF 1bis : isotropie x / z", buf);
	const EchelleQ q6 = MesurerQ(g);
	snprintf(buf, sizeof(buf), "Rankine R = 6 : %u composante(s) de Q >= 0,01 Qmax (%u cellules), d pondéré %.4f, d lourde %.4f "
							   "(attendu 12 ± 10 %%, UNE composante ; sol %d plafond %d)",
			 q6.composantes, q6.cellulesQ, (double)q6.dPondere, (double)q6.dLourde, (int)q6.toucheSol,
			 (int)q6.touchePlafond);
	ProbeCheck(q6.valide && q6.composantes == 1 && ProbeAbs(q6.dPondere - 12.f) <= 0.10f * 12.f,
			   "(n1') contrôle POSITIF 1 du critère Q (Rankine R = 6)", buf);

	// --- POSITIF 2 : R = 3 cellules -> D = 6, et le RAPPORT ----------------
	g.Reset();
	PoserTube(g, 3.f, 2.f);
	g.Step(1.0e-5f);
	const EchelleVolutes e3 = MesurerEchelle(g);
	snprintf(buf, sizeof(buf), "Rankine R = 3 : Lx %.4f, Lz %.4f (zéros à r = %.2f / %.2f) -> D = %.4f cellules, attendu 6 (± 20 %%)",
			 (double)e3.Lx, (double)e3.Lz, (double)e3.r0x, (double)e3.r0z, (double)e3.D);
	ProbeCheck(e3.valide && ProbeAbs(e3.D - 6.f) <= 0.20f * 6.f, "contrôle POSITIF 2 de l'échelle (Rankine R = 3)", buf);
	const float32 rapport = (e3.D > 0.f) ? e6.D / e3.D : 0.f;
	snprintf(buf, sizeof(buf), "D(R=6) / D(R=3) = %.4f (attendu 2, tolérance [1,8 ; 2,2]) — c'est CE contrôle qui prouve "
							   "que l'instrument mesure une TAILLE",
			 (double)rapport);
	ProbeCheck(e3.valide && e6.valide && rapport >= 1.8f && rapport <= 2.2f,
			   "contrôle POSITIF 2bis : l'échelle suit le rayon", buf);
	const EchelleQ q3 = MesurerQ(g);
	snprintf(buf, sizeof(buf), "Rankine R = 3 : %u composante(s), d pondéré %.4f (attendu 6 ± 20 %%)", q3.composantes,
			 (double)q3.dPondere);
	ProbeCheck(q3.valide && ProbeAbs(q3.dPondere - 6.f) <= 0.20f * 6.f,
			   "(n1') contrôle POSITIF 2 du critère Q (Rankine R = 3)", buf);
	const float32 rapportQ = (q3.dPondere > 0.f) ? q6.dPondere / q3.dPondere : 0.f;
	snprintf(buf, sizeof(buf), "d(R=6) / d(R=3) = %.4f (attendu 2, tolérance [1,8 ; 2,2])", (double)rapportQ);
	ProbeCheck(q3.valide && q6.valide && rapportQ >= 1.8f && rapportQ <= 2.2f,
			   "(n1') contrôle POSITIF 2bis du critère Q : d suit le rayon", buf);

	// --- NÉGATIF 1 : bruit blanc -> aucune corrélation, D < 2 ---------------
	g.Reset();
	{
		uint32 graine = 20260912u; // LCG déterministe, graine dite
		float32 *u = const_cast<float32 *>(g.VelocityX());
		float32 *v = const_cast<float32 *>(g.VelocityY());
		float32 *w = const_cast<float32 *>(g.VelocityZ());
		const uint32 n = (g.Nx() + 2) * (g.Ny() + 2) * (g.Nz() + 2);
		auto tirage = [&]() {
			graine = graine * 1664525u + 1013904223u;
			return ((float32)(graine >> 8) / 16777216.f) * 2.f - 1.f;
		};
		for (uint32 i = 0; i < n; ++i) {
			u[i] = tirage();
			v[i] = tirage();
			w[i] = tirage();
		}
	}
	g.Step(1.0e-5f);
	const EchelleVolutes eb = MesurerEchelle(g);
	snprintf(buf, sizeof(buf), "bruit blanc (graine 20260912) : Lx %.4f, Lz %.4f -> D = %.4f cellules (doit être < 2)",
			 (double)eb.Lx, (double)eb.Lz, (double)eb.D);
	ProbeCheck(eb.valide && eb.D < 2.f, "contrôle NÉGATIF 1 de l'échelle (bruit blanc)", buf);
	const EchelleQ qb = MesurerQ(g);
	snprintf(buf, sizeof(buf), "bruit blanc : valide %s, %u composantes, d pondéré %.4f, d lourde %.4f — attendu : false "
							   "(amas percolant jusqu'aux parois) OU d < 2 ; ROUGE si un d >= 2 valide sort du bruit",
			 qb.valide ? "true" : "false", qb.composantes, (double)qb.dPondere, (double)qb.dLourde);
	ProbeCheck(!qb.valide || qb.dPondere < 2.f, "(n1') contrôle NÉGATIF 1 du critère Q (bruit blanc)", buf);

	// --- NÉGATIF 2 : rotation solide sur TOUT le domaine -> pas de zéro -> false
	g.Reset();
	PoserTube(g, 0.f, 2.f);
	g.Step(1.0e-5f);
	const EchelleVolutes eu = MesurerEchelle(g);
	snprintf(buf, sizeof(buf), "omega uniforme : valide = %s, D = %.4f (l'instrument doit REFUSER : R(r) ne passe jamais par zéro)",
			 eu.valide ? "true" : "false", (double)eu.D);
	ProbeCheck(!eu.valide, "contrôle NÉGATIF 2 de l'échelle (rotation uniforme -> false)", buf);
	const EchelleQ qu = MesurerQ(g);
	snprintf(buf, sizeof(buf), "rotation uniforme : valide %s, %u composante(s), d lourde %.2f (doit toucher les parois latérales -> false)",
			 qu.valide ? "true" : "false", qu.composantes, (double)qu.dLourde);
	ProbeCheck(!qu.valide && qu.composantes >= 1, "(n1') contrôle NÉGATIF 2 du critère Q (rotation uniforme -> false)", buf);

	// --- NÉGATIF 3 : grille vide -> dénominateur nul -> false ---------------
	g.Reset();
	g.Step(1.0e-5f);
	const EchelleVolutes ev = MesurerEchelle(g);
	ProbeCheck(!ev.valide && ev.D == 0.f, "contrôle NÉGATIF 3 de l'échelle (grille vide -> false)",
			   "rend false et ne fabrique pas de point");
	const EchelleQ qv = MesurerQ(g);
	ProbeCheck(!qv.valide && qv.composantes == 0, "(n1') contrôle NÉGATIF 3 du critère Q (grille vide -> false)",
			   "rend false, zéro composante");

	// --- (n1') NÉGATIF 4, LE DÉCISIF : cisaillement simple u = a z -> Q = 0 partout
	// |Omega|² et |S|² y sont le MÊME nombre (a²/2) : un instrument qui verrait un
	// tourbillon dans un cisaillement pur ne séparerait pas un jet d'une volute.
	g.Reset();
	{
		const float32 h = g.CellSize();
		const float32 a = 3.f;
		float32 *u = const_cast<float32 *>(g.VelocityX());
		for (uint32 k = 0; k <= g.Nz() + 1; ++k)
			for (uint32 j = 0; j <= g.Ny() + 1; ++j)
				for (uint32 i = 0; i <= g.Nx() + 1; ++i)
					u[g.Idx(i, j, k)] = a * (((float32)k - 0.5f) * h);
	}
	g.Step(1.0e-5f);
	const EchelleQ qs = MesurerQ(g);
	const EchelleVolutes es = MesurerEchelle(g);
	snprintf(buf, sizeof(buf), "u = a z : Qmax = %.3e, %u cellule(s) retenue(s), valide %s — et (n1) y voit une « échelle » : "
							   "valide %s, D = %.2f (c'est ce que (n1) ne sépare pas)",
			 (double)qs.qMax, qs.cellulesQ, qs.valide ? "true" : "false", es.valide ? "true" : "false", (double)es.D);
	ProbeCheck(!qs.valide && qs.cellulesQ == 0, "(n1') contrôle NÉGATIF 4 du critère Q (cisaillement pur -> false)", buf);
}

// =============================================================================
// LA SCÈNE — celle des deux images du 07/09, à l'identique (rendu.cpp,
// ConstruirePanache) ; seuls la boîte, h et epsilon sont des paramètres, pour
// (n2) et (n3). Tout le reste est figé ici et le contrôle (n3c) garde la copie.
// =============================================================================
struct SceneVolutes {
		NkVec3f bmin = {-0.25f, 0.f, -0.25f};
		NkVec3f bmax = {0.25f, 1.6f, 0.25f};
		float32 h = 0.02f;
		float32 epsilon = 0.f;
		NkVec3f source = {0.f, 0.05f, 0.f};
		float32 rayonSource = 0.06f;
		uint32 pas = 255;
		float32 dt = 1.f / 60.f;
};

struct ResultatVolutes {
		float32 Dmoy = 0.f, Dmin = 0.f, Dmax = 0.f; // sur les kFenetre derniers pas
		float32 Lxmoy = 0.f, Lzmoy = 0.f;
		float32 Dfinal = 0.f; // l'instantané du dernier pas (pour (n3c))
		uint32 invalides = 0; // pas où l'instrument a rendu false
		// (n1') le critère Q, sur la même fenêtre
		float32 dQmoy = 0.f, dQmin = 0.f, dQmax = 0.f; // d pondéré par l'enstrophie
		float32 dQlourdeMoy = 0.f;						 // d de la composante la plus lourde
		float32 composantesMoy = 0.f, fractionQmoy = 0.f;
		float32 dQfinal = 0.f;
		uint32 invalidesQ = 0, solQ = 0, plafondQ = 0;
		uint32 cellules = 0;
		float32 h = 0.f;
		float32 cflMax = 0.f;
		float32 msParPas = 0.f;
		float32 masseFinale = 0.f;
		bool ok = false;
};

static void ParametresScene(NkFluidGridParams &p, const SceneVolutes &s) {
	p.boundsMin = s.bmin;
	p.boundsMax = s.bmax;
	p.cellSize = s.h;
	p.densityDissipation = 0.5f;
	p.temperatureDissipation = 0.5f;
	p.buoyancyAlpha = 0.25f;
	p.pressureTolerance = 1.0e-4f;
	p.vorticityConfinement = s.epsilon;
}

static ResultatVolutes Course(const SceneVolutes &s, const char *cheminImage) {
	ResultatVolutes r;
	NkFluidGridParams p;
	ParametresScene(p, s);
	NkFluidGrid g;
	if (!g.Init(p))
		return r;
	r.cellules = g.Stats().cellsInterior;
	r.h = g.CellSize();
	const uint32 debutFenetre = (s.pas > kFenetre) ? s.pas - kFenetre : 0u;
	float64 dSum = 0.0, lxSum = 0.0, lzSum = 0.0, msSum = 0.0;
	float64 qSum = 0.0, qlSum = 0.0, compSum = 0.0, fracSum = 0.0;
	uint32 nMes = 0, nMesQ = 0;
	r.Dmin = 1.0e30f;
	r.Dmax = 0.f;
	r.dQmin = 1.0e30f;
	r.dQmax = 0.f;
	for (uint32 st = 0; st < s.pas; ++st) {
		g.EmitSphere(s.source, s.rayonSource, 7.f * s.dt, 400.f * s.dt, 0.f);
		g.Step(s.dt);
		msSum += (float64)g.Stats().ms;
		if (g.Stats().advectCFL > r.cflMax)
			r.cflMax = g.Stats().advectCFL;
		if (st >= debutFenetre) {
			const EchelleVolutes e = MesurerEchelle(g);
			if (!e.valide) {
				++r.invalides;
			} else {
				dSum += (float64)e.D;
				lxSum += (float64)e.Lx;
				lzSum += (float64)e.Lz;
				if (e.D < r.Dmin)
					r.Dmin = e.D;
				if (e.D > r.Dmax)
					r.Dmax = e.D;
				++nMes;
			}
			if (st + 1 == s.pas)
				r.Dfinal = e.valide ? e.D : 0.f;
			const EchelleQ q = MesurerQ(g);
			if (!q.valide) {
				++r.invalidesQ;
			} else {
				qSum += (float64)q.dPondere;
				qlSum += (float64)q.dLourde;
				compSum += (float64)q.composantes;
				fracSum += (float64)q.fractionEnstrophie;
				if (q.dPondere < r.dQmin)
					r.dQmin = q.dPondere;
				if (q.dPondere > r.dQmax)
					r.dQmax = q.dPondere;
				if (q.toucheSol)
					++r.solQ;
				if (q.touchePlafond)
					++r.plafondQ;
				++nMesQ;
			}
			if (st + 1 == s.pas)
				r.dQfinal = q.valide ? q.dPondere : 0.f;
		}
	}
	if (nMesQ > 0) {
		r.dQmoy = (float32)(qSum / (float64)nMesQ);
		r.dQlourdeMoy = (float32)(qlSum / (float64)nMesQ);
		r.composantesMoy = (float32)(compSum / (float64)nMesQ);
		r.fractionQmoy = (float32)(fracSum / (float64)nMesQ);
	} else {
		r.dQmin = 0.f;
	}
	r.msParPas = (float32)(msSum / (float64)s.pas);
	r.masseFinale = g.TotalMass();
	if (nMes > 0) {
		r.Dmoy = (float32)(dSum / (float64)nMes);
		r.Lxmoy = (float32)(lxSum / (float64)nMes);
		r.Lzmoy = (float32)(lzSum / (float64)nMes);
		r.ok = true;
	} else {
		r.Dmin = 0.f;
	}
	if (cheminImage != nullptr) {
		// MÊME caméra que ImagesDuConfinement (rendu.cpp) : aide à l'œil, pas preuve.
		NkFluidRaymarchParams rp;
		rp.width = 480;
		rp.height = 360;
		rp.cameraPos = {0.f, 0.38f, 2.10f};
		rp.cameraTarget = {0.f, 0.30f, 0.f};
		rp.fovDegrees = 40.f;
		rp.shadowMaxDistance = 0.35f;
		NkVector<uint8> img;
		NkFluidRaymarchStats st;
		NkFluidRaymarchRender(g, rp, img, st);
		EcrirePng(img, rp.width, rp.height, cheminImage);
		printf("    image écrite : %s (480 x 360, rendu CPU, %.0f ms)\n", cheminImage, (double)st.ms);
	}
	return r;
}

static void Ligne(const char *nom, const ResultatVolutes &r, float32 diametreSource) {
	printf("    %-30s %7u cellules  h %.3f  D = %7.3f cellules [%6.3f ; %6.3f]  = %.4f m = %.2f x source  "
		   "(Lx %.3f, Lz %.3f, %u refus, CFL %.2f, %.0f ms/pas)\n",
		   nom, r.cellules, (double)r.h, (double)r.Dmoy, (double)r.Dmin, (double)r.Dmax, (double)(r.Dmoy * r.h),
		   (double)(r.Dmoy * r.h / diametreSource), (double)r.Lxmoy, (double)r.Lzmoy, r.invalides, (double)r.cflMax,
		   (double)r.msParPas);
	printf("    %-30s (n1') critère Q : d = %7.3f cellules [%6.3f ; %6.3f] = %.4f m = %.2f x source ; d lourde %.3f ; "
		   "%.1f composantes ; %.1f %% de l'enstrophie dans Q > 0 ; %u refus ; sol %u / plafond %u pas\n",
		   "", (double)r.dQmoy, (double)r.dQmin, (double)r.dQmax, (double)(r.dQmoy * r.h),
		   (double)(r.dQmoy * r.h / diametreSource), (double)r.dQlourdeMoy, (double)r.composantesMoy,
		   (double)(r.fractionQmoy * 100.f), r.invalidesQ, r.solQ, r.plafondQ);
	fflush(stdout);
}

static void Verdict(const char *nom, float32 rr, float32 dm2, float32 dm1) {
	printf("    %s : r = %.3f en CELLULES ; en MÈTRES %.4f -> %.4f m (x %.3f)\n", nom, (double)rr, (double)dm2,
		   (double)dm1, (double)(dm2 > 0.f ? dm1 / dm2 : 0.f));
	if (rr <= 0.f)
		printf("    -> PAS DE VERDICT : la mesure a échoué.\n");
	else if (rr >= 1.6f)
		printf("    -> r >= 1,6 : C'EST LA BOÎTE (la résolution). La taille est fixée par la physique, en mètres ;\n"
			   "       la grille de 2 cm était trop grosse pour la montrer. ON S'ARRÊTE.\n");
	else if (rr <= 1.3f)
		printf("    -> r <= 1,3 : C'EST LE SCHÉMA. La taille reste accrochée à la grille quel que soit h.\n");
	else
		printf("    -> r entre 1,3 et 1,6 : INDÉTERMINÉ, dit tel quel. Aucun solveur n'est écrit.\n");
}

// =============================================================================
// LE PALIER — (n1), (n3), (n2a) toujours ; (n2b) sous le mode (son coût).
// =============================================================================
void PalierVolutes(bool complet) {
	printf("\n=== ⑤ LA TAILLE DES TOURBILLONS, EN CELLULES — boîte ou schéma ? (PLAN_VOLUTES.md) ===\n");
	printf("    L'hypothèse du 07/09 : « les tourbillons restent de la taille de quelques cellules ».\n");
	printf("    Prédiction inscrite : D(B) entre 2 et 8 cellules. Règle (n2b) : r >= 1,6 boîte, r <= 1,3 schéma.\n");
	ControlesInstrument();

	const float32 eps = EpsilonConfinement();
	const float32 diamSource = 2.f * 0.06f;
	char buf[500];

	// ---------------------------------------------------------------- (n1)
	printf("\n    (n1) la scène des deux images, D moyenné sur les %u derniers pas de 255 :\n", kFenetre);
	SceneVolutes sA;
	sA.epsilon = 0.f;
	SceneVolutes sB;
	sB.epsilon = eps;
	const ResultatVolutes A = Course(sA, nullptr);
	const ResultatVolutes B = Course(sB, nullptr);
	Ligne("A  jet, epsilon = 0", A, diamSource);
	Ligne("B  panache, epsilon = 8", B, diamSource);
	snprintf(buf, sizeof(buf), "A : D = %.3f cellules ; B : D = %.3f cellules (%u + %u refus sur %u pas)", (double)A.Dmoy,
			 (double)B.Dmoy, A.invalides, B.invalides, kFenetre);
	ProbeCheck(A.ok && B.ok && A.invalides == 0 && B.invalides == 0, "(n1) l'échelle est MESURÉE sur les deux scènes",
			   buf);
	printf("    [note] prédiction inscrite « quelques cellules » (2 à 8) : B rend %.3f -> %s\n", (double)B.Dmoy,
		   (B.Dmoy >= 2.f && B.Dmoy <= 8.f) ? "la phrase du 07/09 était JUSTE" : "la phrase du 07/09 était FAUSSE");
	snprintf(buf, sizeof(buf), "A : d = %.3f ; B : d = %.3f cellules (%u + %u refus sur %u pas)", (double)A.dQmoy,
			 (double)B.dQmoy, A.invalidesQ, B.invalidesQ, kFenetre);
	ProbeCheck(A.invalidesQ == 0 && B.invalidesQ == 0, "(n1') le critère Q est MESURÉ sur les deux scènes", buf);

	// ---------------------------------------------------------------- (n3)
	printf("\n    (n3) les contrôles négatifs de la mesure :\n");
	const ResultatVolutes B2 = Course(sB, nullptr);
	snprintf(buf, sizeof(buf), "deux courses identiques : D = %.6f et %.6f cellules", (double)B.Dmoy, (double)B2.Dmoy);
	ProbeCheck(B2.ok && B2.Dmoy == B.Dmoy && B2.Dfinal == B.Dfinal && B2.dQmoy == B.dQmoy,
			   "(n3a) DÉTERMINISME : identique au dernier chiffre", buf);

	SceneVolutes sT = sB;
	const NkVec3f T = {1.3f, 0.2f, -0.7f};
	sT.bmin = {sB.bmin.x + T.x, sB.bmin.y + T.y, sB.bmin.z + T.z};
	sT.bmax = {sB.bmax.x + T.x, sB.bmax.y + T.y, sB.bmax.z + T.z};
	sT.source = {sB.source.x + T.x, sB.source.y + T.y, sB.source.z + T.z};
	const ResultatVolutes BT = Course(sT, nullptr);
	const float32 ecartT = (B.Dmoy > 0.f) ? ProbeAbs(BT.Dmoy - B.Dmoy) / B.Dmoy : 1.f;
	snprintf(buf, sizeof(buf), "boîte et source déplacées de (+1,3 ; +0,2 ; -0,7) m : D = %.6f contre %.6f, écart %.2e "
							   "relatif (seuil 1e-4) — un instrument qui lirait le MONDE rougirait ici",
			 (double)BT.Dmoy, (double)B.Dmoy, (double)ecartT);
	ProbeCheck(BT.ok && ecartT <= 1.0e-4f, "(n3b) INVARIANCE PAR TRANSLATION", buf);
	{
		// LA CAUSE DU ROUGE DE (n3b), MESURÉE (addendum § 5bis) : la source discrétisée
		// est-elle la MÊME dans les deux repères ? EmitSphere retient une cellule si
		// |centre − source|² <= r², et le centre vaut bmin + (i − 0,5) h en float32 :
		// sur le BORD de la sphère, l'arrondi peut décider différemment.
		NkFluidGridParams pB, pT;
		ParametresScene(pB, sB);
		ParametresScene(pT, sT);
		NkFluidGrid gB, gT;
		gB.Init(pB);
		gT.Init(pT);
		gB.EmitSphere(sB.source, sB.rayonSource, 7.f * sB.dt, 400.f * sB.dt, 0.f);
		gT.EmitSphere(sT.source, sT.rayonSource, 7.f * sT.dt, 400.f * sT.dt, 0.f);
		uint32 differentes = 0;
		const float32 *dB = gB.Density();
		const float32 *dT = gT.Density();
		const uint32 n = (gB.Nx() + 2) * (gB.Ny() + 2) * (gB.Nz() + 2);
		for (uint32 i = 0; i < n; ++i)
			if (dB[i] != dT[i])
				++differentes;
		snprintf(buf, sizeof(buf), "après UNE émission : %u cellule(s) de densité différente entre les deux repères, masse %.9f "
								   "contre %.9f — si non nul, les deux courses ne sont pas le même problème discret et l'écart de "
								   "(n3b) mesure la SENSIBILITÉ de l'écoulement, pas l'instrument",
				 differentes, (double)gB.TotalMass(), (double)gT.TotalMass());
		ProbeCheck(differentes != 0, "(n3b') la cause du rouge de (n3b) est dans la SOURCE discrétisée", buf);
	}

	{
		NkFluidGrid gOrig;
		ConstruirePanache(gOrig, false, 255, eps); // rendu.cpp, TELLE QUELLE
		const EchelleVolutes eo = MesurerEchelle(gOrig);
		const EchelleQ qo = MesurerQ(gOrig);
		// Le même filtre de validité des deux côtés : `dQfinal` vaut 0 quand le critère
		// Q a REFUSÉ au dernier pas, donc l'original se lit avec le même filtre. (À la
		// course 2, ce témoin a rougi sur SON PROPRE câblage — D identique au dernier
		// chiffre des deux côtés, mais un 0 filtré comparé à une valeur non filtrée.)
		const float32 qFinalOrig = qo.valide ? qo.dPondere : 0.f;
		snprintf(buf, sizeof(buf), "ConstruirePanache (rendu.cpp) au pas 255 : D = %.6f, d(Q) = %.6f (valide %d) ; ma copie au "
								   "pas 255 : D = %.6f, d(Q) = %.6f",
				 (double)eo.D, (double)qFinalOrig, (int)qo.valide, (double)B.Dfinal, (double)B.dQfinal);
		ProbeCheck(eo.valide && eo.D == B.Dfinal && qFinalOrig == B.dQfinal,
				   "(n3c) FIDÉLITÉ DE LA COPIE : identique au dernier chiffre", buf);
	}
	printf("    (n3d) la caméra : aucun pixel n'est lu par l'instrument — par construction, dit, pas éprouvé.\n");

	// ---------------------------------------------------------------- (n2a)
	printf("\n    (n2a) LITTÉRAL : la boîte divisée par 2, à nombre de cellules CONSTANT (25 x 80 x 25),\n");
	printf("          source inchangée en mètres. ⚠️ confondant DIT : les parois passent de 0,25 à 0,125 m de l'axe.\n");
	SceneVolutes s2a = sB;
	s2a.bmin = {-0.125f, 0.f, -0.125f};
	s2a.bmax = {0.125f, 0.8f, 0.125f};
	s2a.h = 0.01f;
	const ResultatVolutes B2a = Course(s2a, nullptr);
	Ligne("B  boîte / 2, h = 1 cm", B2a, diamSource);
	const float32 r2a = (B.Dmoy > 0.f && B2a.ok) ? B2a.Dmoy / B.Dmoy : 0.f;
	const float32 r2aQ = (B.dQmoy > 0.f && B2a.dQmoy > 0.f) ? B2a.dQmoy / B.dQmoy : 0.f;
	printf("    r(n2a) = D(1 cm, boîte/2) / D(2 cm) = %.3f ; en mètres %.4f -> %.4f m\n", (double)r2a,
		   (double)(B.Dmoy * B.h), (double)(B2a.Dmoy * B2a.h));
	printf("    r'(n2a) critère Q = %.3f ; en mètres %.4f -> %.4f m\n", (double)r2aQ, (double)(B.dQmoy * B.h),
		   (double)(B2a.dQmoy * B2a.h));

	// ---------------------------------------------------------------- (n2b)
	if (!complet) {
		printf("\n    (n2b) ne tourne que sous NK_FLUID_VOLUTES=1 (400 000 cellules : son coût).\n");
		return;
	}
	printf("\n    (n2b) PROPRE : même boîte, même source, même dt, même epsilon ; SEUL h passe de 2 à 1 cm.\n");
	SceneVolutes s2b = sB;
	s2b.h = 0.01f;
	const ResultatVolutes B2b = Course(s2b, "Captures/fumee_panache_h1cm_2026-09-12.png");
	Ligne("B  même boîte, h = 1 cm", B2b, diamSource);
	const float32 r2b = (B.Dmoy > 0.f && B2b.ok) ? B2b.Dmoy / B.Dmoy : 0.f;
	const float32 r2bQ = (B.dQmoy > 0.f && B2b.dQmoy > 0.f) ? B2b.dQmoy / B.dQmoy : 0.f;
	snprintf(buf, sizeof(buf), "%u + %u refus sur %u pas ; CFL max %.2f ; %.0f ms/pas", B2b.invalides, B2b.invalidesQ,
			 kFenetre, (double)B2b.cflMax, (double)B2b.msParPas);
	ProbeCheck(B2b.ok && B2b.invalides == 0 && B2b.invalidesQ == 0, "(n2b) les deux échelles sont MESURÉES à h = 1 cm",
			   buf);
	printf("\n    VERDICTS (règle écrite AVANT, PLAN_VOLUTES.md § 3 et § 5bis) :\n");
	Verdict("(n1)  nappe, autocorrélation", r2b, B.Dmoy * B.h, B2b.Dmoy * B2b.h);
	Verdict("(n1') volutes, critère Q     ", r2bQ, B.dQmoy * B.h, B2b.dQmoy * B2b.h);
	printf("    (n2a) disait r = %.3f et r' = %.3f (parois à 0,125 m : confondant dit).\n", (double)r2a, (double)r2aQ);
	printf("    Si (n1) et (n1') ne disent pas la même chose, c'est (n1') qui parle des VOLUTES : lui seul\n");
	printf("    sépare rotation et cisaillement ; (n1) parle de l'épaisseur de la nappe.\n");
	fflush(stdout);
}
