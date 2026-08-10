//
// PdfRasterTest.cpp — verification NUMERIQUE du rastériseur de traces.
//
// On ne juge pas « a l'oeil » : chaque forme a une aire CONNUE, et la somme des
// couvertures produites doit la retrouver. Un rastériseur anti-aliase correct
// conserve l'aire — c'est sa propriete la plus facile a violer sans que ca se
// voie, et la plus facile a mesurer.
//
// Usage :  NkPdfRasterTest
// Retour : nombre de tests en echec.
//
#include "NKMedia/Pdf/NkPdfRaster.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::media::pdf;

static int32 gFail = 0;

// Aire couverte = somme des « noirceurs ». On peint en NOIR opaque sur un fond
// BLANC : la couverture d'un pixel est donc (255 - rouge) / 255.
static double CoveredArea(const NkPdfCanvas &c) {
	double sum = 0.0;
	const uint8 *p = c.Pixels();
	const usize n = static_cast<usize>(c.Width()) * static_cast<usize>(c.Height());
	for (usize i = 0; i < n; ++i)
		sum += static_cast<double>(255 - p[i * 4]) / 255.0;
	return sum;
}

static void Check(const char *nom, double obtenu, double attendu, double tolerance) {
	const double ecart = obtenu > attendu ? obtenu - attendu : attendu - obtenu;
	const double pct = attendu > 0.0 ? 100.0 * ecart / attendu : 0.0;
	if (ecart <= tolerance) {
		std::printf("  [OK]    %-34s aire = %9.2f  (attendu %9.2f, ecart %.2f %%)\n", nom, obtenu,
					attendu, pct);
	} else {
		std::printf("  [ECHEC] %-34s aire = %9.2f  (attendu %9.2f, ecart %.2f %%)\n", nom, obtenu,
					attendu, pct);
		++gFail;
	}
}

// Cercle approxime par quatre beziers cubiques. La constante 0,5522847 est le
// rapport classique qui minimise l'erreur d'une approximation de quart de
// cercle par une cubique (erreur relative < 0,02 %) : l'aire mesuree doit donc
// coller a pi*r^2 de tres pres, sinon c'est le rastériseur qui derive.
static void AjouteCercle(NkPdfPath &p, double cx, double cy, double r) {
	const double k = 0.5522847498307936 * r;
	p.MoveTo(cx + r, cy);
	p.CurveTo(cx + r, cy + k, cx + k, cy + r, cx, cy + r);
	p.CurveTo(cx - k, cy + r, cx - r, cy + k, cx - r, cy);
	p.CurveTo(cx - r, cy - k, cx - k, cy - r, cx, cy - r);
	p.CurveTo(cx + k, cy - r, cx + r, cy - k, cx + r, cy);
	p.Close();
}

int main() {
	std::printf("=== Rastériseur de traces : verification par les AIRES ===\n\n");

	// ── 1. Rectangle aligne : aire exacte, aucune excuse ──
	{
		NkPdfCanvas c;
		c.Create(200, 120);
		NkPdfPath p;
		p.Rect(10, 10, 100, 50);
		c.FillPath(p, false, 0, 0, 0, 255);
		Check("rectangle 100x50", CoveredArea(c), 5000.0, 1.0);
	}

	// ── 2. Rectangle a bords FRACTIONNAIRES : teste l'anti-aliasing ──
	// Sans couverture partielle exacte, l'aire serait arrondie a l'entier.
	{
		NkPdfCanvas c;
		c.Create(200, 120);
		NkPdfPath p;
		p.Rect(10.5, 10.25, 100.5, 50.5);
		c.FillPath(p, false, 0, 0, 0, 255);
		Check("rectangle a bords fractionnaires", CoveredArea(c), 100.5 * 50.5, 3.0);
	}

	// ── 3. Cercle : valide l'aplatissement des beziers ──
	{
		NkPdfCanvas c;
		c.Create(200, 200);
		NkPdfPath p;
		AjouteCercle(p, 100, 100, 60);
		c.FillPath(p, false, 0, 0, 0, 255);
		Check("cercle r=60", CoveredArea(c), 3.14159265358979 * 60.0 * 60.0, 60.0);
	}

	// ── 4. Regles de remplissage : NON NUL contre PAIR-IMPAIR ──
	// Deux carres concentriques traces dans le MEME sens. En non-nul le trou
	// interieur se remplit (enroulement 2), en pair-impair il reste vide.
	{
		NkPdfPath p;
		p.Rect(20, 20, 100, 100);  // exterieur, aire 10000
		p.Rect(45, 45, 50, 50);	   // interieur, aire 2500
		NkPdfCanvas c1;
		c1.Create(160, 160);
		c1.FillPath(p, false, 0, 0, 0, 255);
		Check("2 carres, regle NON NULLE", CoveredArea(c1), 10000.0, 20.0);

		NkPdfCanvas c2;
		c2.Create(160, 160);
		c2.FillPath(p, true, 0, 0, 0, 255);
		Check("2 carres, regle PAIR-IMPAIR", CoveredArea(c2), 10000.0 - 2500.0, 20.0);
	}

	// ── 5. Decoupage : l'intersection doit borner le remplissage ──
	{
		NkPdfCanvas c;
		c.Create(200, 200);
		NkPdfPath clip;
		clip.Rect(50, 50, 60, 60); // 3600
		c.SetClipFromPath(clip, false);
		NkPdfPath p;
		p.Rect(0, 0, 200, 200); // toute la page
		c.FillPath(p, false, 0, 0, 0, 255);
		Check("remplissage borne par le decoupage", CoveredArea(c), 3600.0, 20.0);
	}

	// ── 6. Decoupages EMPILES : le second doit INTERSECTER, pas remplacer ──
	{
		NkPdfCanvas c;
		c.Create(200, 200);
		NkPdfPath a, b;
		a.Rect(50, 50, 100, 100); // 100x100
		b.Rect(100, 100, 100, 100);
		c.SetClipFromPath(a, false);
		c.SetClipFromPath(b, false); // intersection = 50x50 = 2500
		NkPdfPath p;
		p.Rect(0, 0, 200, 200);
		c.FillPath(p, false, 0, 0, 0, 255);
		Check("2 decoupages empiles (intersection)", CoveredArea(c), 2500.0, 20.0);
	}

	// ── 7. Alpha : un remplissage a 50 % doit couvrir une demi-aire ──
	{
		NkPdfCanvas c;
		c.Create(200, 120);
		NkPdfPath p;
		p.Rect(10, 10, 100, 50);
		c.FillPath(p, false, 0, 0, 0, 128);
		Check("rectangle a alpha 128", CoveredArea(c), 5000.0 * 128.0 / 255.0, 15.0);
	}

	// ── 8. Contour : un trait d'epaisseur e sur une longueur L couvre ~ e*L ──
	{
		NkPdfCanvas c;
		c.Create(200, 120);
		NkPdfPath p;
		p.MoveTo(20, 60);
		p.LineTo(180, 60);
		c.StrokePath(p, 4.0, 0, 0, 0, 255);
		Check("trait horizontal, epaisseur 4", CoveredArea(c), 160.0 * 4.0, 20.0);
	}

	// ── 9. Hors cadre : un trace deborde ne doit RIEN corrompre ──
	// Cas classique de depassement de tableau ; l'aire attendue est la partie
	// visible, et le programme doit surtout ne pas planter.
	{
		NkPdfCanvas c;
		c.Create(100, 100);
		NkPdfPath p;
		p.Rect(-50, -50, 100, 100); // seul le quart bas-droit est visible
		c.FillPath(p, false, 0, 0, 0, 255);
		Check("trace debordant du cadre", CoveredArea(c), 2500.0, 20.0);
	}

	// ── 10. Trace vide et cadre degenere : ne doit pas planter ──
	{
		NkPdfCanvas c;
		c.Create(50, 50);
		NkPdfPath vide;
		c.FillPath(vide, false, 0, 0, 0, 255);
		Check("trace vide (aucun remplissage)", CoveredArea(c), 0.0, 0.01);
		NkPdfCanvas bad;
		const bool refuse = !bad.Create(0, 10) && !bad.Create(-5, 5) && !bad.Create(50000, 50000);
		std::printf("  [%s]    %-34s\n", refuse ? "OK" : "ECHEC", "dimensions invalides refusees");
		if (!refuse)
			++gFail;
	}

	std::printf("\n================================================\n");
	std::printf("  echecs : %d\n", gFail);
	return gFail;
}
