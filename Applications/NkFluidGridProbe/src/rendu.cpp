// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkFluidGridProbe — paliers ② (RENDU DE VOLUME) et ③ (FEU) : les témoins.
//
// PRÉ-ENREGISTREMENT (écrit AVANT de lire le moindre chiffre) :
//
// PALIER ② — le rendu, jugé EN PIXELS, jamais « ça a l'air bien » :
//   ②.0 CONTRÔLE NÉGATIF de l'instrument : une grille de densité NULLE doit
//       rendre un écran EXACTEMENT égal au fond. Critère : 0 pixel différent.
//   ②.1 Une colonne de fumée est PLUS DENSE AU CENTRE : sur la bande de lignes
//       qui traverse le panache, l'opacité moyenne de la fenêtre CENTRALE doit
//       dépasser celle des deux fenêtres latérales de même taille.
//       Critère : centre > 1,5 x max(gauche, droite).
//   ②.2 Elle S'ATTÉNUE EN S'ÉLEVANT : opacité moyenne d'une bande BASSE
//       strictement supérieure à celle d'une bande HAUTE. Critère : basse > haute.
//   ②.3 COÛT : ms de rendu pour trois définitions, avec le nombre
//       d'échantillons de la marche principale et de la marche d'ombre.
//   « Opacité » d'un pixel = distance de Manhattan de sa couleur au fond, sur
//   0-765. La population est dite pour chaque fenêtre (en pixels).
//
// PALIER ③ — le feu :
//   ③.0 CONTRÔLES du chemin colorimétrique, chacun INDÉPENDANT de la couleur
//       qu'on veut prouver :
//         - un spectre d'ÉNERGIE ÉGALE doit tomber sur le point blanc E
//           (x = y = 1/3) : contrôle des fonctions colorimétriques SEULES ;
//         - le maximum de Planck TROUVÉ PAR BALAYAGE doit tomber sur la loi de
//           WIEN (b/T) : contrôle de Planck SEUL ;
//         - la chromaticité d'un corps noir à 6504 K doit tomber sur D65
//           (x = 0,3127 ; y = 0,3290 — valeurs de la norme sRGB IEC 61966-2-1) :
//           contrôle des DEUX ENSEMBLE.
//   ③.1 COULEUR à 1 000 / 2 000 / 3 000 K : rouge sombre -> orange -> jaune-blanc.
//       Exprimé en critères MESURABLES : R > G > B aux trois ; G/R et B/R
//       STRICTEMENT croissants avec T ; bandes chiffrées dites ci-dessous.
//   ③.2 MUTATION : la table du corps noir remplacée par une RAMPE LINÉAIRE
//       rouge -> blanc. Les contrôles ③.0 et le critère ③.1 doivent la REFUSER.
//   ③.3 Sans source, la TEMPÉRATURE DÉCROÎT (courbe mesurée, monotone).
//   ③.4 Le CARBURANT s'épuise et la flamme s'éteint (retour à l'ambiante).
//
// CE QUE CES TÉMOINS NE PROUVENT PAS : rien sur le GPU. Le rendu est celui de
// RÉFÉRENCE, sur CPU ; le portage NKRHI/NkSL (texture 3D + compute) n'est pas
// fait. La variante « billboards » n'est PAS mesurée ici et ne peut pas l'être
// honnêtement : elle passe par le chemin GPU des particules (NkVFXSystem), et
// comparer une marche CPU à un dessin GPU ne comparerait pas deux méthodes mais
// deux machines. C'est dit, pas sous-entendu.
// =============================================================================
#include "NKRenderer/Tools/VFX/NkFluidGridRaymarch.h"
#include "NKImage/Core/NkImage.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::renderer;
using namespace nkentseu::math;

void ProbeCheck(bool ok, const char *nom, const char *detail);
float32 ProbeAbs(float32 v);

// ── outils de mesure sur le tampon RGBA ─────────────────────────────────────
static uint32 Opacite(const NkVector<uint8> &rgba, uint32 W, uint32 x, uint32 y, const uint8 bg[3]) {
	const nk_size o = ((nk_size)y * (nk_size)W + (nk_size)x) * 4u;
	const int32 dr = (int32)rgba[o + 0] - (int32)bg[0];
	const int32 dg = (int32)rgba[o + 1] - (int32)bg[1];
	const int32 db = (int32)rgba[o + 2] - (int32)bg[2];
	return (uint32)((dr < 0 ? -dr : dr) + (dg < 0 ? -dg : dg) + (db < 0 ? -db : db));
}

static float32 OpaciteMoyenne(const NkVector<uint8> &rgba, uint32 W, uint32 x0, uint32 x1, uint32 y0, uint32 y1,
							  const uint8 bg[3], uint32 &population) {
	float64 s = 0.0;
	population = 0;
	for (uint32 y = y0; y < y1; ++y)
		for (uint32 x = x0; x < x1; ++x) {
			s += (float64)Opacite(rgba, W, x, y, bg);
			++population;
		}
	return (population > 0) ? (float32)(s / (float64)population) : 0.f;
}

static bool EcrirePng(const NkVector<uint8> &rgba, uint32 W, uint32 H, const char *chemin) {
	NkImage img = NkImage::Create(W, H, 4, 0x00000000u);
	if (!img.IsValid())
		return false;
	for (uint32 y = 0; y < H; ++y)
		for (uint32 x = 0; x < W; ++x) {
			const nk_size o = ((nk_size)y * (nk_size)W + (nk_size)x) * 4u;
			img.SetPixel((int32)x, (int32)y, NkColor(rgba[o + 0], rgba[o + 1], rgba[o + 2], rgba[o + 3]));
		}
	return img.Save(chemin, 100);
}

// La BOITE ENGLOBANTE du panache DANS L'IMAGE : les fenetres des temoins (2.1)
// et (2.2) sont calees dessus, jamais sur des coordonnees ecrites a la main.
// Sinon le critere ne DEPARTAGE pas : la premiere version comparait le coeur du
// panache a du vide (rapport 9 174), ce qui ne prouve rien sur un gradient.
struct BoiteImage {
		uint32 x0, x1, y0, y1;
		bool valide;
};

static BoiteImage BoitePanache(const NkVector<uint8> &rgba, uint32 W, uint32 H, const uint8 bg[3], uint32 seuil) {
	BoiteImage b = {W, 0, H, 0, false};
	for (uint32 y = 0; y < H; ++y)
		for (uint32 x = 0; x < W; ++x)
			if (Opacite(rgba, W, x, y, bg) > seuil) {
				if (x < b.x0) b.x0 = x;
				if (x + 1 > b.x1) b.x1 = x + 1;
				if (y < b.y0) b.y0 = y;
				if (y + 1 > b.y1) b.y1 = y + 1;
				b.valide = true;
			}
	return b;
}

// ── la scène commune aux paliers ② et ③ : un panache établi ─────────────────
static void ConstruirePanache(NkFluidGrid &g, bool avecFeu, uint32 pas, float32 epsilon = 0.f) {
	NkFluidGridParams p;
	p.boundsMin = {-0.25f, 0.f, -0.25f};
	// Boite HAUTE : le panache doit se DILUER en montant, pas s'ecraser sous un
	// plafond. Avec une boite de 1 m il s'y accumulait, et le temoin (2.2)
	// mesurait un tas de fumee au plafond au lieu d'un panache qui monte.
	p.boundsMax = {0.25f, 1.6f, 0.25f};
	p.cellSize = 0.02f; // 25 x 80 x 25
	p.densityDissipation = 0.5f;
	p.temperatureDissipation = 0.5f;
	p.buoyancyAlpha = 0.25f;
	p.pressureTolerance = 1.0e-4f;
	p.vorticityConfinement = epsilon; // Fedkiw 2001, § 4 -- 0 = le jet d'avant
	if (avecFeu) {
		// REGLAGE CHIFFRE, pas choisi a l'oeil : avec un debit de carburant r (1/s),
		// un taux de reaction b et un refroidissement c, l'etat d'equilibre est
		//   T - T_amb = heatPerFuel * (r / b) * (1 - exp(-b dt)) / (c dt)
		// soit ici ~1 460 K -> une flamme a ~1 760 K, et une poussee d'Archimede de
		// g (T - T_amb) / T_amb ~ 48 m/s^2 : c'est l'ordre de grandeur d'une VRAIE
		// flamme (un facteur ~5 g), pas un reglage force.
		p.burnRate = 9.f;	   // 1/s
		p.heatPerFuel = 900.f; // K par unité de carburant
		p.sootPerFuel = 1.0f;  // peu de suie : sinon la flamme est ENTERREE sous sa propre fumee
		p.coolingRate = 2.5f;
		p.temperatureDissipation = 0.f; // le refroidissement suffit
	}
	g.Init(p);
	const float32 dt = 1.f / 60.f;
	for (uint32 s = 0; s < pas; ++s) {
		if (avecFeu)
			g.EmitSphere({0.f, 0.07f, 0.f}, 0.07f, 0.05f * dt, 0.f, 5.f * dt);
		else
			// 400 K de plus que l'ambiante, pas 900 : une poussee plus douce laisse le
			// panache s'ELARGIR au lieu de partir en jet d'un pixel de large.
			g.EmitSphere({0.f, 0.05f, 0.f}, 0.06f, 7.f * dt, 400.f * dt, 0.f);
		g.Step(dt);
	}
}

// =============================================================================
// =============================================================================
// L'IMAGE DU CONFINEMENT — deux rendus de la MEME scene, seul epsilon change.
//
// C'est le seul livrable que Rodolf peut juger DE SES YEUX ; le reste de ce banc
// juge des nombres. Mais « ca a l'air mieux » ne vaut rien : la largeur du
// panache est donc AUSSI mesuree EN PIXELS, sur une ligne d'image donnee, par un
// instrument INDEPENDANT de celui de la grille (qui, lui, lit le champ de
// densite). Deux instruments, deux chemins, le meme verdict — ou alors il y a un
// probleme, et on le saura.
//
// L'instrument : sur la ligne y de l'image, la racine de la moyenne des carres
// des distances a la colonne barycentre, ponderee par l'OPACITE (distance de
// Manhattan au fond, 0-765). Il ne sait rien de la grille.
// Son CONTROLE POSITIF est celui du palier (2.0), deja passe : densite nulle ->
// zero pixel different du fond, donc l'instrument ne fabrique pas de largeur sur
// une image vide (il rend false).
//
// SEUIL : le meme que le temoin (v2) du fichier vorticite.cpp — x 1,15. Il n'est
// pas choisi apres coup : c'est le seuil deja pre-enregistre pour « le panache
// s'elargit », applique a un second instrument.
// =============================================================================
static bool LargeurEnPixels(const NkVector<uint8> &rgba, uint32 W, uint32 y, const uint8 bg[3], uint32 seuil,
							float32 &rayonOut, float32 &opaciteOut, uint32 &pixelsOut) {
	float64 w = 0.0, sx = 0.0;
	pixelsOut = 0;
	for (uint32 x = 0; x < W; ++x) {
		const uint32 o = Opacite(rgba, W, x, y, bg);
		if (o <= seuil)
			continue;
		w += (float64)o;
		sx += (float64)o * (float64)x;
		++pixelsOut;
	}
	if (w <= 0.0 || pixelsOut == 0)
		return false;
	const float64 cx = sx / w;
	float64 s2 = 0.0;
	for (uint32 x = 0; x < W; ++x) {
		const uint32 o = Opacite(rgba, W, x, y, bg);
		if (o <= seuil)
			continue;
		s2 += (float64)o * ((float64)x - cx) * ((float64)x - cx);
	}
	rayonOut = (float32)NkSqrt((float32)(s2 / w));
	opaciteOut = (float32)(w / (float64)pixelsOut);
	return true;
}

void ImagesDuConfinement(float32 epsilon) {
	printf("\n=== L'IMAGE : le JET (epsilon = 0) contre le PANACHE (epsilon = %.2f) ===\n", (double)epsilon);
	printf("    Meme scene, meme graine, meme nombre de pas : SEUL epsilon change.\n");

	NkFluidRaymarchParams rp;
	rp.width = 480;
	rp.height = 360;
	rp.cameraPos = {0.f, 0.38f, 2.10f};
	rp.cameraTarget = {0.f, 0.30f, 0.f};
	rp.fovDegrees = 40.f;
	rp.shadowMaxDistance = 0.35f;
	const uint8 bg[3] = {(uint8)(rp.background.x * 255.f + 0.5f), (uint8)(rp.background.y * 255.f + 0.5f),
						 (uint8)(rp.background.z * 255.f + 0.5f)};

	const uint32 pas = 255;
	NkFluidGrid gA, gB;
	ConstruirePanache(gA, false, pas, 0.f);
	ConstruirePanache(gB, false, pas, epsilon);

	NkVector<uint8> imgA, imgB;
	NkFluidRaymarchStats stA, stB;
	NkFluidRaymarchRender(gA, rp, imgA, stA);
	NkFluidRaymarchRender(gB, rp, imgB, stB);
	EcrirePng(imgA, rp.width, rp.height, "Captures/fumee_jet_sans_confinement_2026-09-07.png");
	EcrirePng(imgB, rp.width, rp.height, "Captures/fumee_panache_confinement_2026-09-07.png");

	const BoiteImage bA = BoitePanache(imgA, rp.width, rp.height, bg, 8);
	const BoiteImage bB = BoitePanache(imgB, rp.width, rp.height, bg, 8);
	printf("    boite du panache dans l'image : SANS x [%u, %u[ (%u px de large) ; AVEC x [%u, %u[ (%u px)\n",
		   bA.x0, bA.x1, bA.x1 - bA.x0, bB.x0, bB.x1, bB.x1 - bB.x0);

	// La ligne mesuree : le TIERS SUPERIEUR de la boite du panache SANS
	// confinement -- c'est la, loin de la source, que le jet et le panache se
	// separent. Elle est calee sur la boite mesuree, jamais ecrite a la main.
	const uint32 ligne = bA.valide ? (bA.y0 + (bA.y1 - bA.y0) / 3u) : (rp.height / 3u);
	float32 rA = 0.f, rB = 0.f, oA = 0.f, oB = 0.f;
	uint32 nA = 0, nB = 0;
	const bool okA = LargeurEnPixels(imgA, rp.width, ligne, bg, 8, rA, oA, nA);
	const bool okB = LargeurEnPixels(imgB, rp.width, ligne, bg, 8, rB, oB, nB);
	const float32 rapport = (rA > 0.f) ? (rB / rA) : 0.f;

	char buf[420];
	snprintf(buf, sizeof(buf),
			 "ligne y = %u (tiers superieur de la boite SANS) : rayon en pixels %.3f (%u px, opacite moyenne %.1f) "
			 "contre %.3f (%u px, opacite %.1f) -- rapport %.2f (seuil x 1,15, le meme que (v2))",
			 ligne, (double)rA, nA, (double)oA, (double)rB, nB, (double)oB, (double)rapport);
	ProbeCheck(okA && okB && rapport >= 1.15f, "(i1) A L'IMAGE, le panache est plus LARGE que le jet", buf);

	printf("    images : Captures/fumee_jet_sans_confinement_2026-09-07.png et\n");
	printf("             Captures/fumee_panache_confinement_2026-09-07.png (480 x 360, rendu CPU)\n");
	printf("    cout : %.0f ms et %.0f ms\n", (double)stA.ms, (double)stB.ms);
	fflush(stdout);
}

// =============================================================================
// PALIER ② — LE RENDU DU VOLUME
// =============================================================================
void PalierRendu() {
	printf("\n=== PALIER (2) : RENDU DU VOLUME PAR MARCHE DE RAYON ===\n");
	printf("    Kajiya & Von Herzen, « Ray Tracing Volume Densities », SIGGRAPH 1984.\n");

	NkFluidRaymarchParams rp;
	rp.width = 480;
	rp.height = 360;
	rp.cameraPos = {0.f, 0.38f, 2.10f};
	rp.cameraTarget = {0.f, 0.30f, 0.f};
	rp.fovDegrees = 40.f;
	rp.shadowMaxDistance = 0.35f;
	const uint8 bg[3] = {(uint8)(rp.background.x * 255.f + 0.5f), (uint8)(rp.background.y * 255.f + 0.5f),
						 (uint8)(rp.background.z * 255.f + 0.5f)};

	char buf[400];

	// ── ②.0 CONTRÔLE NÉGATIF : densité nulle -> écran = fond, exactement --------
	{
		NkFluidGrid vide;
		NkFluidGridParams p;
		p.boundsMin = {-0.25f, 0.f, -0.25f};
		p.boundsMax = {0.25f, 1.6f, 0.25f};
		p.cellSize = 0.02f;
		vide.Init(p);
		NkVector<uint8> img;
		NkFluidRaymarchStats st;
		NkFluidRaymarchRender(vide, rp, img, st);
		uint32 differents = 0;
		for (uint32 y = 0; y < rp.height; ++y)
			for (uint32 x = 0; x < rp.width; ++x)
				if (Opacite(img, rp.width, x, y, bg) != 0)
					++differents;
		snprintf(buf, sizeof(buf), "%u pixel(s) different(s) du fond sur %u ; %u rayons, %u coupent la boite",
				 differents, rp.width * rp.height, st.rays, st.raysHit);
		ProbeCheck(differents == 0, "(2.0) CONTROLE NEGATIF : densite nulle -> ecran vide", buf);
	}

	// ── la colonne de fumée -----------------------------------------------------
	NkFluidGrid g;
	ConstruirePanache(g, false, 255);

	NkVector<uint8> img;
	NkFluidRaymarchStats st;
	NkFluidRaymarchRender(g, rp, img, st);
	EcrirePng(img, rp.width, rp.height, "Captures/fumee_colonne_2026-09-05.png");

	const BoiteImage bp = BoitePanache(img, rp.width, rp.height, bg, 8);
	if (!bp.valide) {
		ProbeCheck(false, "(2.1/2.2) le panache est visible dans l'image", "aucun pixel au-dessus du seuil 8/765");
		return;
	}
	printf("    boite du panache dans l'image : x [%u, %u[, y [%u, %u[ (%u x %u px)\n", bp.x0, bp.x1, bp.y0, bp.y1,
		   bp.x1 - bp.x0, bp.y1 - bp.y0);

	// ── ②.1 plus dense au centre ------------------------------------------------
	// Les trois fenetres sont DANS la boite du panache et de MEME taille : le
	// centre contre les deux bords de la colonne, pas contre le vide.
	{
		const uint32 bw = bp.x1 - bp.x0, bh = bp.y1 - bp.y0;
		const uint32 w = (bw / 5 > 4) ? bw / 5 : 4;
		const uint32 cx = (bp.x0 + bp.x1) / 2;
		const uint32 y0 = bp.y0 + bh / 3, y1 = bp.y0 + 2 * bh / 3;
		uint32 nC = 0, nG = 0, nD = 0;
		const float32 c = OpaciteMoyenne(img, rp.width, cx - w / 2, cx + w / 2, y0, y1, bg, nC);
		const float32 gch = OpaciteMoyenne(img, rp.width, bp.x0, bp.x0 + w, y0, y1, bg, nG);
		const float32 dr = OpaciteMoyenne(img, rp.width, bp.x1 - w, bp.x1, y0, y1, bg, nD);
		const float32 cote = (gch > dr) ? gch : dr;
		snprintf(buf, sizeof(buf),
				 "centre %.2f (%u px) contre bord gauche %.2f (%u px) et bord droit %.2f (%u px) — rapport %.2f (critere > 1,5)",
				 (double)c, nC, (double)gch, nG, (double)dr, nD, (double)((cote > 0.f) ? c / cote : 999.f));
		ProbeCheck(c > 1.5f * cote, "(2.1) la colonne est PLUS DENSE AU CENTRE", buf);
	}

	// ── ②.2 s'atténue en s'élevant ----------------------------------------------
	// Les deux bandes sont DANS la boite du panache (quart bas, quart haut), sur
	// la moitie centrale de sa largeur. La ligne 0 est en HAUT de l'image : la
	// bande BASSE du monde a donc les plus GRANDS y.
	{
		const uint32 bw = bp.x1 - bp.x0, bh = bp.y1 - bp.y0;
		const uint32 x0 = bp.x0 + bw / 4, x1 = bp.x1 - bw / 4;
		uint32 nB = 0, nH = 0;
		const float32 basse = OpaciteMoyenne(img, rp.width, x0, x1, bp.y1 - bh / 4, bp.y1, bg, nB);
		const float32 haute = OpaciteMoyenne(img, rp.width, x0, x1, bp.y0, bp.y0 + bh / 4, bg, nH);
		snprintf(buf, sizeof(buf), "bande basse %.2f (%u px) contre bande haute %.2f (%u px) — rapport %.2f",
				 (double)basse, nB, (double)haute, nH, (double)((haute > 0.f) ? basse / haute : 999.f));
		ProbeCheck(basse > haute, "(2.2) elle S'ATTENUE EN S'ELEVANT", buf);
	}

	// ── ②.3 coût pour trois définitions -----------------------------------------
	printf("    cout du rendu (CPU, un seul fil, pas de marche = h/2 = %.3f m) :\n", (double)(g.CellSize() * 0.5f));
	const uint32 defs[3][2] = {{240, 180}, {480, 360}, {960, 720}};
	for (uint32 i = 0; i < 3; ++i) {
		NkFluidRaymarchParams q = rp;
		q.width = defs[i][0];
		q.height = defs[i][1];
		NkVector<uint8> tmp;
		NkFluidRaymarchStats s2;
		NkFluidRaymarchRender(g, q, tmp, s2);
		printf("      %4u x %4u : %8.1f ms — %u rayons (%u touchent), %llu echantillons + %llu d'ombre\n", q.width,
			   q.height, (double)s2.ms, s2.rays, s2.raysHit, (unsigned long long)s2.samples,
			   (unsigned long long)s2.shadowSamples);
		fflush(stdout);
	}
	printf("    image : Captures/fumee_colonne_2026-09-05.png (480 x 360)\n");
}

// =============================================================================
// PALIER ③ — LE FEU
// =============================================================================

// La MUTATION : une rampe linéaire rouge -> blanc à la place du corps noir.
// Elle doit ECHOUER les mêmes contrôles ; si elle passe, le témoin ne vaut rien.
static NkVec3f RampeLineaire(float32 kelvin) {
	float32 t = (kelvin - 1000.f) / 5504.f; // 1000 K -> 0, 6504 K -> 1
	t = NkClamp(t, 0.f, 1.f);
	return {1.f, t, t};
}

void PalierFeu() {
	printf("\n=== PALIER (3) : LE FEU — combustion et couleur du corps noir ===\n");
	printf("    Planck 1901 ; fonctions CIE 1931 approchees par Wyman, Sloan & Shirley,\n");
	printf("    JCGT 2013 ; matrice et gamma sRGB de la norme IEC 61966-2-1.\n");
	char buf[400];

	// ── ③.0 contrôles du chemin colorimétrique ---------------------------------
	{
		float32 x = 0.f, y = 0.f;
		NkEqualEnergyChromaticity(x, y);
		snprintf(buf, sizeof(buf), "spectre d'energie egale : x = %.4f, y = %.4f (point blanc E attendu : 1/3, 1/3)",
				 (double)x, (double)y);
		ProbeCheck(ProbeAbs(x - 0.3333f) < 0.02f && ProbeAbs(y - 0.3333f) < 0.02f,
				   "(3.0a) CONTROLE des fonctions colorimetriques SEULES", buf);
	}
	{
		// Wien : b/T. A 3000 K le maximum tombe a 966 nm, hors du visible : on balaie
		// large (200-4000 nm) pour que l'instrument puisse le trouver.
		bool ok = true;
		float32 pireEcart = 0.f;
		float32 tests[3] = {1000.f, 2000.f, 3000.f};
		for (uint32 i = 0; i < 3; ++i) {
			const float32 wien = NkWienPeakWavelength(tests[i]);
			const float32 scan = NkPlanckPeakWavelengthScanned(tests[i], 200.f, 6000.f, 0.5f);
			const float32 ecart = ProbeAbs(scan - wien) / wien;
			if (ecart > pireEcart)
				pireEcart = ecart;
			if (ecart > 0.005f)
				ok = false;
		}
		snprintf(buf, sizeof(buf), "pic de Planck balaye contre b/T (b = 2,897771955e-3 m.K) : ecart max %.4f %%",
				 (double)(pireEcart * 100.f));
		ProbeCheck(ok, "(3.0b) CONTROLE de la loi de Planck SEULE (Wien)", buf);
	}
	{
		float32 x = 0.f, y = 0.f;
		NkBlackBodyChromaticity(6504.f, x, y);
		snprintf(buf, sizeof(buf), "corps noir a 6504 K : x = %.4f, y = %.4f (D65 de la norme sRGB : 0,3127 / 0,3290)",
				 (double)x, (double)y);
		ProbeCheck(ProbeAbs(x - 0.3127f) < 0.02f && ProbeAbs(y - 0.3290f) < 0.02f,
				   "(3.0c) CONTROLE des deux ensemble (D65)", buf);
	}

	// ── ③.1 la couleur aux trois températures ----------------------------------
	{
		const float32 K[3] = {1000.f, 2000.f, 3000.f};
		NkVec3f c[3];
		for (uint32 i = 0; i < 3; ++i) {
			c[i] = NkBlackBodyColor(K[i]);
			printf("      %5.0f K -> sRGB (%.3f, %.3f, %.3f) = 8 bits (%3.0f, %3.0f, %3.0f)\n", (double)K[i],
				   (double)c[i].x, (double)c[i].y, (double)c[i].z, (double)(c[i].x * 255.f), (double)(c[i].y * 255.f),
				   (double)(c[i].z * 255.f));
		}
		const bool ordre = c[0].x > c[0].y && c[0].y >= c[0].z && c[1].x > c[1].y && c[1].y > c[1].z &&
						   c[2].x >= c[2].y && c[2].y > c[2].z;
		const bool montee = (c[0].y / c[0].x) < (c[1].y / c[1].x) && (c[1].y / c[1].x) < (c[2].y / c[2].x) &&
							(c[0].z / c[0].x) < (c[1].z / c[1].x) && (c[1].z / c[1].x) < (c[2].z / c[2].x);
		snprintf(buf, sizeof(buf), "G/R : %.3f -> %.3f -> %.3f ; B/R : %.3f -> %.3f -> %.3f",
				 (double)(c[0].y / c[0].x), (double)(c[1].y / c[1].x), (double)(c[2].y / c[2].x),
				 (double)(c[0].z / c[0].x), (double)(c[1].z / c[1].x), (double)(c[2].z / c[2].x));
		ProbeCheck(ordre && montee, "(3.1) rouge sombre -> orange -> jaune-blanc (R>G>B, teinte monotone)", buf);

		// ③.2 MUTATION : la rampe lineaire doit ECHOUER le meme critere.
		NkVec3f r[3];
		for (uint32 i = 0; i < 3; ++i)
			r[i] = RampeLineaire(K[i]);
		float32 ecartMax = 0.f;
		for (uint32 i = 0; i < 3; ++i) {
			const float32 e = ProbeAbs(r[i].y - c[i].y) + ProbeAbs(r[i].z - c[i].z);
			if (e > ecartMax)
				ecartMax = e;
		}
		// La rampe est refusee si elle s'ecarte visiblement de la table calculee.
		const float32 seuil = 0.15f; // ~38 niveaux sur 255, largement au-dessus du bruit
		snprintf(buf, sizeof(buf),
				 "rampe lineaire rouge->blanc : ecart max sur (G,B) = %.3f (seuil de refus %.2f) ; a 2000 K elle donne "
				 "(%.3f, %.3f, %.3f) au lieu de (%.3f, %.3f, %.3f)",
				 (double)ecartMax, (double)seuil, (double)r[1].x, (double)r[1].y, (double)r[1].z, (double)c[1].x,
				 (double)c[1].y, (double)c[1].z);
		ProbeCheck(ecartMax > seuil, "MUTATION (3.2) : la rampe lineaire est bien REFUSEE", buf);
	}

	// ── ③.3 sans source, la température décroît --------------------------------
	{
		NkFluidGridParams p;
		p.boundsMin = {-0.25f, 0.f, -0.25f};
		p.boundsMax = {0.25f, 1.f, 0.25f};
		p.cellSize = 0.025f;
		p.coolingRate = 1.4f;
		p.pressureTolerance = 1.0e-4f;
		NkFluidGrid g;
		g.Init(p);
		g.EmitSphere({0.f, 0.2f, 0.f}, 0.06f, 1.f, 1500.f, 0.f);
		const float32 dt = 1.f / 60.f;
		float32 prec = 1.0e30f;
		bool monotone = true;
		printf("      courbe de Tmax (K) :");
		for (uint32 s = 0; s < 240; ++s) {
			g.Step(dt);
			const float32 t = g.Stats().maxTemperature;
			if (t > prec + 1.0e-3f)
				monotone = false;
			prec = t;
			if (s % 40 == 39)
				printf("  %.0f", (double)t);
		}
		printf("\n");
		snprintf(buf, sizeof(buf), "Tmax finale %.1f K (ambiante %.0f K), decroissance monotone : %s", (double)prec,
				 (double)p.ambientTemperature, monotone ? "oui" : "NON");
		ProbeCheck(monotone && prec < 400.f, "(3.3) sans source, la temperature DECROIT", buf);
	}

	// ── ③.4 le carburant s'épuise et la flamme s'éteint -------------------------
	{
		NkFluidGridParams p;
		p.boundsMin = {-0.25f, 0.f, -0.25f};
		p.boundsMax = {0.25f, 1.f, 0.25f};
		p.cellSize = 0.025f;
		p.burnRate = 12.f;
		p.heatPerFuel = 2200.f;
		p.sootPerFuel = 2.5f;
		p.coolingRate = 1.4f;
		p.pressureTolerance = 1.0e-4f;
		NkFluidGrid g;
		g.Init(p);
		g.EmitSphere({0.f, 0.1f, 0.f}, 0.06f, 0.f, 0.f, 3.f); // du carburant, UNE fois
		const float32 carburant0 = g.TotalFuel();
		const float32 dt = 1.f / 60.f;
		float32 tPic = 0.f;
		for (uint32 s = 0; s < 300; ++s) {
			g.Step(dt);
			if (g.Stats().maxTemperature > tPic)
				tPic = g.Stats().maxTemperature;
		}
		const float32 carburant1 = g.TotalFuel();
		snprintf(buf, sizeof(buf),
				 "carburant %.6f -> %.6f (%.3f %% restant) ; Tmax a culmine a %.0f K puis est redescendue a %.1f K",
				 (double)carburant0, (double)carburant1, (double)(carburant1 / carburant0 * 100.f), (double)tPic,
				 (double)g.Stats().maxTemperature);
		ProbeCheck(carburant1 < 0.01f * carburant0 && tPic > 1200.f && g.Stats().maxTemperature < 400.f,
				   "(3.4) le carburant s'epuise et la flamme s'eteint", buf);
	}

	// ── l'image de la flamme, et le DEGRADE lu dedans ---------------------------
	// Scene PROPRE au feu : une boite courte et une grille fine, sinon la flamme
	// est une aiguille -- la poussee d'Archimede a 1 760 K vaut ~48 m/s^2, elle
	// etire tout ce qui est chaud. Le seuil d'emission est monte a 900 K parce
	// qu'en dessous une flamme reelle ne rayonne pas dans le visible : a 450 K le
	// halo tiede, bien plus VOLUMINEUX que le coeur, noyait tout en rouge sombre.
	{
		NkFluidGridParams p;
		p.boundsMin = {-0.2f, 0.f, -0.2f};
		p.boundsMax = {0.2f, 0.8f, 0.2f};
		p.cellSize = 0.0125f; // 32 x 64 x 32
		p.densityDissipation = 0.4f;
		p.buoyancyAlpha = 0.2f;
		p.burnRate = 9.f;
		// 2 600 K par unite de carburant : l'estimation a zero dimension
		// (heatPerFuel * (r/b) * (1-exp(-b dt)) / (1-exp(-c dt))) donne 4 500 K, la
		// grille en rend 2 100 -- l'advection emporte la chaleur avant qu'elle
		// s'accumule. C'est la MESURE qui a fixe le chiffre, pas la formule : a
		// 900 K la flamme culminait a 736 K et n'emettait RIEN.
		p.heatPerFuel = 5000.f;
		p.sootPerFuel = 1.5f;
		p.coolingRate = 3.5f;
		p.pressureTolerance = 1.0e-4f;
		NkFluidGrid g;
		g.Init(p);
		const float32 dt = 1.f / 120.f;
		for (uint32 s2 = 0; s2 < 200; ++s2) {
			g.EmitSphere({0.f, 0.05f, 0.f}, 0.05f, 0.05f * dt, 0.f, 5.f * dt);
			g.Step(dt);
		}

		NkFluidRaymarchParams rp;
		rp.width = 480;
		rp.height = 360;
		rp.cameraPos = {0.f, 0.24f, 0.78f};
		rp.cameraTarget = {0.f, 0.20f, 0.f};
		rp.fovDegrees = 44.f;
		rp.shadowMaxDistance = 0.25f;
		rp.emission = true;
		rp.emissionMinTemperature = 850.f;
		rp.emissionStrength = 2.2f;
		rp.absorption = 6.f;
		rp.scattering = 8.f;
		NkVector<uint8> img;
		NkVector<float32> tpix; // temperature max le long de CHAQUE rayon, publiee par le rendu
		NkFluidRaymarchStats st;
		NkFluidRaymarchRender(g, rp, img, st, &tpix);
		EcrirePng(img, rp.width, rp.height, "Captures/feu_degrade_2026-09-05.png");

		// LA MEME IMAGE SANS EMISSION : sa difference avec la precedente EST la
		// lumiere emise, isolee de la fumee eclairee. Sans cette soustraction, on
		// mesure surtout le gris de la fumee : mesure du 05/09, le rapport G/R des
		// pixels « chauds » sortait PLUS BAS que celui des pixels froids, non pas
		// parce que la table etait fausse mais parce que la diffusion dominait et
		// que le rouge SATURAIT a 255. Un temoin qui melange deux sources ne juge
		// aucune des deux.
		NkFluidRaymarchParams rs = rp;
		rs.emission = false;
		NkVector<uint8> sansFeu;
		NkFluidRaymarchStats st2;
		NkFluidRaymarchRender(g, rs, sansFeu, st2);
		printf("    Tmax de la scene : %.0f K ; rendu %.0f ms a 480 x 360\n", (double)g.Stats().maxTemperature,
			   (double)st.ms);

		// Le DEGRADE se lit sur la DIFFERENCE (avec emission moins sans emission),
		// confrontee PIXEL PAR PIXEL a la temperature que le RENDU LUI-MEME a
		// rencontree le long du rayon (tpix). Aucune hypothese sur « ou est le coeur
		// de la flamme » : la premiere version supposait le bas de l'image le plus
		// chaud -- FAUX, la zone de combustion est AU-DESSUS de l'injection.
		char b2[400];
		float32 tmin = 1.0e30f, tmax = 0.f;
		uint32 nEmis = 0;
		const uint32 NP = rp.width * rp.height;
		for (uint32 i = 0; i < NP; ++i) {
			const nk_size o = (nk_size)i * 4u;
			const int32 dR = (int32)img[o + 0] - (int32)sansFeu[o + 0];
			if (dR > 20) {
				++nEmis;
				if (tpix[i] < tmin) tmin = tpix[i];
				if (tpix[i] > tmax) tmax = tpix[i];
			}
		}
		if (nEmis < 200 || tmax <= tmin + 50.f) {
			snprintf(b2, sizeof(b2), "%u pixels emissifs (difference R > 20), rayons entre %.0f et %.0f K", nEmis,
					 (double)((tmin > 1.0e29f) ? 0.f : tmin), (double)tmax);
			ProbeCheck(false, "(3.5) a l'ecran, la couleur EMISE suit la temperature", b2);
		} else {
			const float32 s1 = tmin + (tmax - tmin) / 3.f, s2 = tmin + 2.f * (tmax - tmin) / 3.f;
			float64 rF = 0.0, gF = 0.0, rC = 0.0, gC = 0.0;
			uint32 nF = 0, nC = 0, sat = 0;
			for (uint32 i = 0; i < NP; ++i) {
				const nk_size o = (nk_size)i * 4u;
				const int32 dR = (int32)img[o + 0] - (int32)sansFeu[o + 0];
				const int32 dG = (int32)img[o + 1] - (int32)sansFeu[o + 1];
				if (dR <= 20)
					continue;
				if (img[o + 0] >= 255)
					++sat;
				if (tpix[i] <= s1) {
					rF += dR;
					gF += (dG > 0 ? dG : 0);
					++nF;
				} else if (tpix[i] >= s2) {
					rC += dR;
					gC += (dG > 0 ? dG : 0);
					++nC;
				}
			}
			const float32 froid = (nF > 0 && rF > 0.0) ? (float32)(gF / rF) : -1.f;
			const float32 chaud = (nC > 0 && rC > 0.0) ? (float32)(gC / rC) : -1.f;
			snprintf(b2, sizeof(b2),
					 "%u px emissifs (%u satures en R), rayons de %.0f a %.0f K : G/R emis du tiers FROID (<= %.0f K) "
					 "%.3f (%u px) contre tiers CHAUD (>= %.0f K) %.3f (%u px)",
					 nEmis, sat, (double)tmin, (double)tmax, (double)s1, (double)froid, nF, (double)s2, (double)chaud,
					 nC);
			ProbeCheck(nF > 50 && nC > 50 && chaud > froid,
					   "(3.5) a l'ecran, plus le rayon est CHAUD plus la couleur EMISE est jaune", b2);
		}
		printf("    image : Captures/feu_degrade_2026-09-05.png (480 x 360)\n");
	}
}
