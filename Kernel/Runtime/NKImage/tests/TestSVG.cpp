// =============================================================================
// @File    TestSVG.cpp
// @Brief   Le banc du codec SVG maison (NkSVGCodec) : un SVG par palier, ECRIT
//          PAR LE TEST, decode, puis compare EN PIXELS a des valeurs attendues.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI LE TEST ECRIT SES PROPRES FICHIERS
// =============================================================================
//  Un banc qui lit un .svg versionne prouve deux choses a la fois (le fichier
//  et le decodeur) et ment le jour ou le fichier bouge. Ici chaque cas ECRIT le
//  SVG qu'il decode : le contenu attendu est SOUS LES YEUX, a cote des pixels
//  qu'on exige. Le seul fichier venu d'ailleurs est le temoin croise (l'export
//  de NkUIDesign), et c'est precisement son role.
//
//  CHAQUE CAS EXIGE DES PIXELS, jamais « ca a decode » : au moins quatre points
//  par element, choisis pour que la panne se voie (un coin qui doit etre VIDE
//  vaut mieux qu'un centre qui doit etre plein — le centre est plein meme quand
//  le rayon est ignore).
// =============================================================================

#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKContainers/String/Encoding/NkBase64.h"
#include "NKMemory/NkAllocator.h"
// LE BANC lie NKFont pour eprouver l'INJECTION -- NKImage, lui, ne le connait
// pas (aucune arete dans les .jenga ; le cas « sans source » ci-dessous le prouve
// autrement : sans injection, le codec ne peut PAS peindre un glyphe).
#include "NKFont/Text/NkFontGlyphSource.h"

#include <cstdio>
#include <cstring>
#include <cmath>

using namespace nkentseu;

// ─────────────────────────────────────────────────────────────────────────────
// Le cadre : compter, dire, et rendre un code de sortie honnete
// ─────────────────────────────────────────────────────────────────────────────
namespace {

	int32 gPass = 0;
	int32 gTotal = 0;

	void Verifier(const char *nom, bool ok, const char *detail) {
		++gTotal;
		if (ok)
			++gPass;
		std::printf("%s %s\n", ok ? "  [ok]  " : "  [FAIL]", nom);
		if (detail && *detail)
			std::printf("         %s\n", detail);
	}

	/// Un pixel RGBA de l'image (0,0,0,0 hors bornes).
	void Pixel(const NkImage &im, int32 x, int32 y, uint8 out[4]) {
		out[0] = out[1] = out[2] = out[3] = 0;
		if (!im.IsValid() || !im.Pixels() || x < 0 || y < 0 || x >= im.Width() || y >= im.Height())
			return;
		const int32 ch = im.Channels();
		const uint8 *p = im.Pixels() + (usize)y * (usize)im.Stride() + (usize)x * (usize)ch;
		out[0] = p[0];
		out[1] = ch > 1 ? p[1] : p[0];
		out[2] = ch > 2 ? p[2] : p[0];
		out[3] = ch > 3 ? p[3] : 255;
	}

	/// Le pixel (x,y) est-il proche de (r,g,b) a @p tol pres, et OPAQUE ?
	bool Proche(const NkImage &im, int32 x, int32 y, int32 r, int32 g, int32 b, int32 tol) {
		uint8 c[4];
		Pixel(im, x, y, c);
		if (c[3] < 200)
			return false;
		return std::abs((int32)c[0] - r) <= tol && std::abs((int32)c[1] - g) <= tol &&
			   std::abs((int32)c[2] - b) <= tol;
	}

	/// Couverture : alpha du pixel (ce que « vide » veut dire, sans supposer une couleur).
	int32 AlphaDe(const NkImage &im, int32 x, int32 y) {
		uint8 c[4];
		Pixel(im, x, y, c);
		return (int32)c[3];
	}

	int32 Gris(const NkImage &im, int32 x, int32 y) {
		uint8 c[4];
		Pixel(im, x, y, c);
		return ((int32)c[0] + (int32)c[1] + (int32)c[2]) / 3;
	}

	/// Decode un SVG depuis une chaine C (la taille = strlen), a la taille voulue.
	NkImage Decoder(const char *svg, int32 w = 0, int32 h = 0) {
		return NkSVGCodec::Decode((const uint8 *)svg, std::strlen(svg), w, h);
	}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// PALIER 1 — rx / ry sur <rect> : la regle CSS, et des ARCS
// ─────────────────────────────────────────────────────────────────────────────
static void TestRxRy() {
	std::printf("\n== PALIER 1 : rx / ry sur <rect> ==\n");

	// (a) rx=20 seul : ry doit VALOIR 20 (regle CSS), les quatre coins evides.
	//     Le coin (11,11) est le point qui rougit si rx est ignore : sans arrondi
	//     il est rouge, avec arrondi il est vide. C'est LA mutation du palier.
	static const char *kRx =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" rx=\"20\" fill=\"#ff0000\"/>"
		"</svg>";
	NkImage a = Decoder(kRx);
	char det[512];
	const bool taille = a.IsValid() && a.Width() == 100 && a.Height() == 100;
	// centre plein ; milieux des quatre cotes pleins ; QUATRE COINS VIDES
	const bool plein = taille && Proche(a, 50, 50, 255, 0, 0, 2) && Proche(a, 50, 12, 255, 0, 0, 2) &&
					   Proche(a, 12, 50, 255, 0, 0, 2) && Proche(a, 87, 50, 255, 0, 0, 2) &&
					   Proche(a, 50, 87, 255, 0, 0, 2);
	const int32 c0 = AlphaDe(a, 11, 11), c1 = AlphaDe(a, 88, 11), c2 = AlphaDe(a, 11, 88), c3 = AlphaDe(a, 88, 88);
	const bool coinsVides = taille && c0 < 40 && c1 < 40 && c2 < 40 && c3 < 40;
	// et le coin n'est pas COUPE en biais : (25,25) est DANS le disque de l'arc
	// (centre 30,30 rayon 20) alors qu'un chanfrein droit l'aurait exclu
	const bool arcPasChanfrein = taille && Proche(a, 25, 25, 255, 0, 0, 2);
	std::snprintf(det, sizeof(det),
				  "100x100=%d ; centre et milieux pleins=%d ; alpha des quatre coins=%d/%d/%d/%d (vides=%d) ; "
				  "(25,25) dans l'arc=%d",
				  taille ? 1 : 0, plein ? 1 : 0, c0, c1, c2, c3, coinsVides ? 1 : 0, arcPasChanfrein ? 1 : 0);
	Verifier("1a. rx=\"20\" seul : ry en herite (regle CSS), les quatre coins sont EVIDES, et le coin est un ARC "
			 "(le point (25,25) y est, un chanfrein l'aurait coupe)",
			 taille && plein && coinsVides && arcPasChanfrein, det);

	// (b) le bornage a la moitie du cote : rx=999 sur un carre 80 -> 40 = un disque.
	static const char *kBorne =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" rx=\"999\" ry=\"999\" fill=\"#00ff00\"/>"
		"</svg>";
	NkImage b = Decoder(kBorne);
	// un disque de centre (50,50) rayon 40 : (50,50) plein, (50,12) plein (bord),
	// (20,20) VIDE (distance 42.4 > 40), (25,25) plein (distance 35.3 < 40)
	const bool disque = b.IsValid() && Proche(b, 50, 50, 0, 255, 0, 2) && Proche(b, 50, 12, 0, 255, 0, 2) &&
						AlphaDe(b, 20, 20) < 40 && Proche(b, 25, 25, 0, 255, 0, 2);
	std::snprintf(det, sizeof(det), "centre plein=%d ; (20,20) alpha=%d (hors du disque) ; (25,25) alpha=%d (dedans)",
				  b.IsValid() && Proche(b, 50, 50, 0, 255, 0, 2) ? 1 : 0, AlphaDe(b, 20, 20), AlphaDe(b, 25, 25));
	Verifier("1b. rx/ry BORNES a la moitie du cote : rx=999 sur un carre de 80 donne un disque de rayon 40", disque,
			 det);

	// (c) rx et ry DIFFERENTS : l'arrondi est elliptique, pas circulaire.
	static const char *kEllip =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" rx=\"40\" ry=\"8\" fill=\"#0000ff\"/>"
		"</svg>";
	NkImage c = Decoder(kEllip);
	// coin haut-gauche : l'ellipse a pour centre (50,18), demi-axes (40,8).
	//   (12,12) : ((12-50)/40)^2 + ((12-18)/8)^2 = 0.90 + 0.56 = 1.46 > 1 -> VIDE
	//   (12,25) : sous le centre de l'ellipse, dans la bande droite -> PLEIN
	const bool ellipse = c.IsValid() && AlphaDe(c, 12, 12) < 60 && Proche(c, 12, 25, 0, 0, 255, 2) &&
						 Proche(c, 50, 12, 0, 0, 255, 2);
	std::snprintf(det, sizeof(det), "(12,12) alpha=%d (hors de l'ellipse) ; (12,25) bleu=%d ; (50,12) bleu=%d",
				  AlphaDe(c, 12, 12), Proche(c, 12, 25, 0, 0, 255, 2) ? 1 : 0, Proche(c, 50, 12, 0, 0, 255, 2) ? 1 : 0);
	Verifier("1c. rx=40 ry=8 : l'arrondi est ELLIPTIQUE (le coin est vide la ou un arrondi circulaire serait plein)",
			 ellipse, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER 2 — <image> : « data: » base64 ET href relatif au fichier SVG
// ─────────────────────────────────────────────────────────────────────────────

/// Un damier 2x2 : magenta / vert / vert / magenta. Ses quatre texels sont des
/// couleurs PURES, donc reconnaissables apres n'importe quel etirement (le bord
/// est repete : les quatre coins de la boite rendent les quatre texels tels quels).
static NkImage Damier2x2() {
	NkImage im = NkImage::Create(2u, 2u, 4);
	if (!im.IsValid() || !im.Pixels())
		return im;
	static const uint8 kM[4] = {255, 0, 255, 255}, kV[4] = {0, 255, 0, 255};
	uint8 *px = im.Pixels();
	for (int32 y = 0; y < 2; ++y)
		for (int32 x = 0; x < 2; ++x) {
			const uint8 *c = ((x + y) & 1) ? kV : kM;
			for (int32 k = 0; k < 4; ++k)
				px[(y * 2 + x) * 4 + k] = c[k];
		}
	return im;
}

static void TestImage() {
	std::printf("\n== PALIER 2 : <image> ==\n");
	char det[640];

	NkImage src = Damier2x2();
	// le PNG du damier, en memoire, puis en base64 : la source des deux cas
	uint8 *pngOctets = nullptr;
	usize pngTaille = 0;
	const bool encode = src.IsValid() && src.SaveToMemory(pngOctets, pngTaille) && pngOctets && pngTaille > 0;
	NkString b64;
	if (encode)
		b64 = nkentseu::encoding::base64::NkEncode(pngOctets, pngTaille);

	// (a) data:image/png;base64 — etire (« none ») sur 40x40 : les quatre texels
	//     aux quatre coins, purs.
	static char svg[8192];
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
				  "<image x=\"0\" y=\"0\" width=\"40\" height=\"40\" preserveAspectRatio=\"none\" "
				  "href=\"data:image/png;base64,%s\"/></svg>",
				  encode ? b64.Data() : "");
	NkImage a = Decoder(svg);
	const bool quatreTexels = a.IsValid() && a.Width() == 40 && Proche(a, 3, 3, 255, 0, 255, 4) &&
							  Proche(a, 36, 3, 0, 255, 0, 4) && Proche(a, 3, 36, 0, 255, 0, 4) &&
							  Proche(a, 36, 36, 255, 0, 255, 4);
	std::snprintf(det, sizeof(det), "PNG encode=%d (%u octets, base64 %u car.) ; 40x40=%d ; quatre texels purs=%d",
				  encode ? 1 : 0, (uint32)pngTaille, (uint32)b64.Length(), a.IsValid() && a.Width() == 40 ? 1 : 0,
				  quatreTexels ? 1 : 0);
	Verifier("2a. <image href=\"data:image/png;base64,...\"> : le base64 est decode, le PNG relu par les codecs "
			 "NKImage, et les quatre texels du damier arrivent PURS aux quatre coins (preserveAspectRatio=none)",
			 encode && quatreTexels, det);

	// (b) un base64 ABIME ne doit RIEN peindre — et le dire. Un decodeur qui
	//     « devine » peindrait du bruit ; ici la boite reste vide.
	static char svgKo[8192];
	{
		NkString abime = b64;
		char *d = abime.Data();
		if (d && abime.Length() > 8)
			d[4] = '@'; // caractere hors alphabet base64
		std::snprintf(svgKo, sizeof(svgKo),
					  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
					  "<image x=\"0\" y=\"0\" width=\"40\" height=\"40\" href=\"data:image/png;base64,%s\"/></svg>",
					  d ? d : "");
	}
	NkImage ko = Decoder(svgKo);
	const bool rienPeint = ko.IsValid() && AlphaDe(ko, 3, 3) < 20 && AlphaDe(ko, 20, 20) < 20 &&
						   AlphaDe(ko, 36, 36) < 20;
	std::snprintf(det, sizeof(det), "alpha (3,3)=%d (20,20)=%d (36,36)=%d -- la boite reste VIDE",
				  AlphaDe(ko, 3, 3), AlphaDe(ko, 20, 20), AlphaDe(ko, 36, 36));
	Verifier("2b. un base64 ABIME ne peint RIEN (le decodeur relaie l'echec au lieu de deviner des octets)", rienPeint,
			 det);

	// (c) href RELATIF, resolu par rapport au dossier du .svg — pas au repertoire
	//     courant. On ecrit le PNG a cote, et on decode en disant ce dossier.
	const char *dossier = "Build";
	const char *chemin = "Build/nksvg_test_damier.png";
	const bool ecrit = src.IsValid() && src.SavePNG(chemin);
	static const char *kRel =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
		"<image x=\"0\" y=\"0\" width=\"40\" height=\"40\" preserveAspectRatio=\"none\" "
		"href=\"nksvg_test_damier.png\"/></svg>";
	NkSVGImage *rel = NkSVGImage::LoadFromMemory((const uint8 *)kRel, std::strlen(kRel), dossier);
	NkImage r = rel ? rel->Rasterize(0, 0) : NkImage();
	const bool relOk = ecrit && rel && Proche(r, 3, 3, 255, 0, 255, 4) && Proche(r, 36, 3, 0, 255, 0, 4) &&
					   Proche(r, 3, 36, 0, 255, 0, 4) && Proche(r, 36, 36, 255, 0, 255, 4);
	if (rel)
		rel->Free();
	std::snprintf(det, sizeof(det), "PNG ecrit dans %s=%d ; decode avec baseDir=\"%s\" -> quatre texels=%d", chemin,
				  ecrit ? 1 : 0, dossier, relOk ? 1 : 0);
	Verifier("2c. <image href=\"...png\"> RELATIF : resolu depuis le DOSSIER DU SVG (baseDir), pas depuis le "
			 "repertoire courant du processus",
			 relOk, det);

	// (d) preserveAspectRatio=\"xMidYMid meet\" : une image 4x2 dans une boite
	//     carree laisse des BANDES VIDES en haut et en bas, et ne se deforme pas.
	NkImage large = NkImage::Create(4u, 2u, 4, 0xFF0000FFu); // rouge opaque
	uint8 *lo = nullptr;
	usize lt = 0;
	NkString b64Large;
	if (large.IsValid() && large.SaveToMemory(lo, lt) && lo && lt)
		b64Large = nkentseu::encoding::base64::NkEncode(lo, lt);
	static char svgMeet[8192];
	std::snprintf(svgMeet, sizeof(svgMeet),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
				  "<image x=\"0\" y=\"0\" width=\"40\" height=\"40\" preserveAspectRatio=\"xMidYMid meet\" "
				  "href=\"data:image/png;base64,%s\"/></svg>",
				  b64Large.Length() ? b64Large.Data() : "");
	NkImage m = Decoder(svgMeet);
	// echelle = min(40/4, 40/2) = 10 -> l'image occupe 40 x 20, centree : y de 10 a 30
	const bool bandes = m.IsValid() && AlphaDe(m, 20, 3) < 20 && AlphaDe(m, 20, 36) < 20 &&
						Proche(m, 20, 20, 255, 0, 0, 4) && Proche(m, 3, 20, 255, 0, 0, 4);
	std::snprintf(det, sizeof(det), "bande haute alpha=%d ; bande basse alpha=%d ; centre rouge=%d ; bord gauche a "
								   "mi-hauteur rouge=%d",
				  AlphaDe(m, 20, 3), AlphaDe(m, 20, 36), Proche(m, 20, 20, 255, 0, 0, 4) ? 1 : 0,
				  Proche(m, 3, 20, 255, 0, 0, 4) ? 1 : 0);
	Verifier("2d. preserveAspectRatio=\"xMidYMid meet\" : une image 4x2 dans une boite carree garde son rapport et "
			 "laisse des BANDES VIDES (elle n'est pas etiree)",
			 bandes, det);

	// (e) l'image suit le TRANSFORM du groupe parent, et son `opacity`.
	static char svgRot[8192];
	std::snprintf(svgRot, sizeof(svgRot),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"80\" viewBox=\"0 0 80 80\">"
				  "<g transform=\"rotate(45 40 40)\">"
				  "<image x=\"20\" y=\"20\" width=\"40\" height=\"40\" preserveAspectRatio=\"none\" opacity=\"0.5\" "
				  "href=\"data:image/png;base64,%s\"/></g></svg>",
				  encode ? b64.Data() : "");
	NkImage rot = Decoder(svgRot);
	// tournee de 45 deg autour du centre, la boite devient un losange : le centre
	// reste couvert, mais le COIN de la boite droite (22,22) sort du losange.
	const int32 aCentre = AlphaDe(rot, 40, 40), aCoin = AlphaDe(rot, 22, 22);
	const bool tourne = rot.IsValid() && aCentre > 100 && aCentre < 160 && aCoin < 40;
	std::snprintf(det, sizeof(det),
				  "alpha au centre=%d (opacity 0.5 -> ~128, pas 255) ; alpha au coin de la boite DROITE (22,22)=%d "
				  "(hors du losange)",
				  aCentre, aCoin);
	Verifier("2e. l'<image> suit le transform du groupe parent (rotation : sa boite devient un losange) et son "
			 "`opacity` (alpha ~128, pas 255)",
			 tourne, det);

	if (pngOctets)
		nkentseu::memory::NkFree(pngOctets);
	if (lo)
		nkentseu::memory::NkFree(lo);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER 3 — <text> : les contours de NKFont, la ligne de base, l'ancrage
// ─────────────────────────────────────────────────────────────────────────────

/// L'ENCRE d'une zone : combien de pixels sombres, et ou. C'est la mesure juste
/// pour du texte -- exiger un pixel PRECIS sur un glyphe reviendrait a figer le
/// dessin d'une police, qui n'est pas notre contrat.
struct Encre {
		int32 sombres = 0;
		int32 xmin = 1 << 20, xmax = -1, ymin = 1 << 20, ymax = -1;
};

static Encre EncreDe(const NkImage &im, int32 x0, int32 y0, int32 x1, int32 y1) {
	Encre e;
	for (int32 y = y0; y < y1; ++y)
		for (int32 x = x0; x < x1; ++x) {
			if (Gris(im, x, y) < 128 && AlphaDe(im, x, y) > 128) {
				++e.sombres;
				if (x < e.xmin) e.xmin = x;
				if (x > e.xmax) e.xmax = x;
				if (y < e.ymin) e.ymin = y;
				if (y > e.ymax) e.ymax = y;
			}
		}
	return e;
}

/// Une page blanche de 200x80 avec le <text> demande dedans.
static void PageTexte(char *out, usize n, const char *attrsEtContenu) {
	std::snprintf(out, n,
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"80\" viewBox=\"0 0 200 80\">"
				  "<rect x=\"0\" y=\"0\" width=\"200\" height=\"80\" fill=\"#ffffff\"/>"
				  "%s</svg>",
				  attrsEtContenu);
}

static void TestTexte() {
	std::printf("\n== PALIER 3 : <text> ==\n");
	char det[640];
	static char svg[4096];

	// (a) LA LIGNE DE BASE. « y » est la ligne de base, pas le haut de la boite :
	//     un « H » (sans jambage) doit poser TOUTE son encre AU-DESSUS de y.
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"40\" fill=\"#000000\">H</text>");
	NkImage a = Decoder(svg);
	Encre ea = a.IsValid() ? EncreDe(a, 0, 0, 200, 80) : Encre();
	const bool auDessus = ea.sombres > 60 && ea.ymax <= 61 && ea.ymin > 20 && ea.xmin >= 19;
	std::snprintf(det, sizeof(det),
				  "encre=%d px ; boite x[%d..%d] y[%d..%d] ; la ligne de base est y=60 -> rien sous 61=%d",
				  ea.sombres, ea.xmin, ea.xmax, ea.ymin, ea.ymax, ea.ymax <= 61 ? 1 : 0);
	Verifier("3a. <text> est PEINT (des glyphes, pas une boite) et « y » est bien la LIGNE DE BASE : un H pose "
			 "toute son encre au-dessus",
			 auDessus, det);

	// (b) font-size = le CADRATIN : doubler le corps double la hauteur et
	//     quadruple (a peu pres) la quantite d'encre.
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\">H</text>");
	NkImage b = Decoder(svg);
	Encre eb = b.IsValid() ? EncreDe(b, 0, 0, 200, 80) : Encre();
	const int32 h40 = ea.ymax - ea.ymin + 1, h20 = eb.ymax - eb.ymin + 1;
	const bool proportionnel = eb.sombres > 10 && h20 > 0 && h40 >= 2 * h20 - 3 && h40 <= 2 * h20 + 3 &&
							   ea.sombres > (eb.sombres * 5) / 2 && ea.sombres < eb.sombres * 6;
	std::snprintf(det, sizeof(det), "corps 20 : %d px de haut, %d d'encre ; corps 40 : %d px de haut, %d d'encre",
				  h20, eb.sombres, h40, ea.sombres);
	Verifier("3b. font-size est une ECHELLE reelle : le corps double donne deux fois la hauteur et ~quatre fois "
			 "l'encre (les contours suivent, rien n'est etire depuis un atlas)",
			 proportionnel, det);

	// (c) text-anchor : start / middle / end deplacent le texte AUTOUR de x.
	PageTexte(svg, sizeof(svg), "<text x=\"100\" y=\"60\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
								"text-anchor=\"start\">Ab</text>");
	Encre s0 = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg), "<text x=\"100\" y=\"60\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
								"text-anchor=\"middle\">Ab</text>");
	Encre s1 = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg), "<text x=\"100\" y=\"60\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
								"text-anchor=\"end\">Ab</text>");
	Encre s2 = EncreDe(Decoder(svg), 0, 0, 200, 80);
	const int32 largeur = s0.xmax - s0.xmin;
	const bool ancrage = s0.sombres > 40 && s1.sombres > 40 && s2.sombres > 40 && s0.xmin >= 99 && s2.xmax <= 101 &&
						 s1.xmin < s0.xmin && s1.xmin > s2.xmin &&
						 std::abs((s0.xmin + s0.xmax) / 2 - (s1.xmin + s1.xmax) / 2 - largeur / 2) <= 3;
	std::snprintf(det, sizeof(det), "start x[%d..%d] ; middle x[%d..%d] ; end x[%d..%d] (x=100, largeur %d)", s0.xmin,
				  s0.xmax, s1.xmin, s1.xmax, s2.xmin, s2.xmax, largeur);
	Verifier("3c. text-anchor : « start » commence a x, « end » y finit, « middle » centre dessus -- la largeur "
			 "totale est donc bien mesuree AVANT de poser les glyphes",
			 ancrage, det);

	// (d) fill : le texte prend sa couleur (et pas le noir par defaut).
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"40\" fill=\"#ff0000\">H</text>");
	NkImage d = Decoder(svg);
	int32 rouges = 0;
	for (int32 y = 10; y < 62; ++y)
		for (int32 x = 18; x < 70; ++x)
			if (Proche(d, x, y, 255, 0, 0, 6))
				++rouges;
	std::snprintf(det, sizeof(det), "%d pixels rouges purs dans la boite du glyphe", rouges);
	Verifier("3d. <text fill=\"#ff0000\"> : les glyphes prennent la couleur demandee", rouges > 40, det);

	// (e) <tspan> : il continue le texte, et un x/y explicite REPOSITIONNE.
	PageTexte(svg, sizeof(svg), "<text x=\"20\" y=\"40\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\">"
								"H<tspan x=\"120\" y=\"70\">H</tspan></text>");
	NkImage e = Decoder(svg);
	Encre haut = EncreDe(e, 0, 0, 200, 50);
	Encre bas = EncreDe(e, 0, 50, 200, 80);
	const bool tspan = haut.sombres > 10 && bas.sombres > 10 && haut.xmin >= 19 && haut.xmin < 40 &&
					   bas.xmin >= 119 && bas.xmin < 145;
	std::snprintf(det, sizeof(det), "premier H : x[%d..%d] y[%d..%d] ; le <tspan x=120 y=70> : x[%d..%d] y[%d..%d]",
				  haut.xmin, haut.xmax, haut.ymin, haut.ymax, bas.xmin, bas.xmax, bas.ymin, bas.ymax);
	Verifier("3e. <tspan x= y=> REPOSITIONNE le curseur (les deux H sont a deux endroits distincts, pas empiles)",
			 tspan, det);

	// (f) une police INTROUVABLE : on rend quand meme, avec Inter, et ON LE DIT.
	static const char *kInconnue =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"80\" viewBox=\"0 0 200 80\">"
		"<rect x=\"0\" y=\"0\" width=\"200\" height=\"80\" fill=\"#ffffff\"/>"
		"<text x=\"20\" y=\"60\" font-family=\"UnePoliceQuiNExistePas\" font-size=\"40\">H</text></svg>";
	NkSVGImage *img = NkSVGImage::LoadFromMemory((const uint8 *)kInconnue, std::strlen(kInconnue));
	bool repliDit = false;
	int32 encreRepli = 0;
	if (img) {
		for (int32 i = 0; i < img->SkippedCount(); ++i) {
			const char *n = img->SkippedAt(i);
			if (n && std::strncmp(n, "police:", 7) == 0)
				repliDit = true;
		}
		NkImage r = img->Rasterize(0, 0);
		encreRepli = EncreDe(r, 0, 0, 200, 80).sombres;
		img->Free();
	}
	std::snprintf(det, sizeof(det), "repli nomme=%d ; encre du texte replie=%d px", repliDit ? 1 : 0, encreRepli);
	Verifier("3f. une font-family INTROUVABLE : le texte est peint avec la police de repli (Inter) ET le repli est "
			 "NOMME -- rendre autre chose que ce qui est demande sans le dire serait le vrai defaut",
			 repliDit && encreRepli > 60, det);

	// (g) l'INDENTATION d'un fichier ne devient pas du texte : sans
	//     xml:space=\"preserve\", les blancs sont reduits et les bords rognes.
	static const char *kBlancs =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"80\" viewBox=\"0 0 200 80\">"
		"<rect x=\"0\" y=\"0\" width=\"200\" height=\"80\" fill=\"#ffffff\"/>"
		"<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\">\n      H\n   </text></svg>";
	Encre blancs = EncreDe(Decoder(kBlancs), 0, 0, 200, 80);
	const bool rogne = blancs.sombres > 20 && blancs.xmin >= 19 && blancs.xmin < 32;
	std::snprintf(det, sizeof(det), "le H indente commence a x=%d (x demande : 20 -- les blancs de tete sont rognes)",
				  blancs.xmin);
	Verifier("3g. les blancs de mise en forme du FICHIER ne deviennent pas du texte : sans xml:space=\"preserve\" "
			 "ils sont rognes, le glyphe commence bien a x",
			 rogne, det);
	// (i) AUCUNE SOURCE DE GLYPHES : le texte est SAUTE **ET DIT**, jamais rendu
	//     vide en silence. C'est la contrepartie de « NKImage ne depend pas de
	//     NKFont » : qui ne fournit pas de police n'a pas de texte, et l'apprend.
	{
		NkIGlyphSource *garde = NkSVGCodec::GetDefaultGlyphSource();
		NkSVGCodec::SetDefaultGlyphSource(nullptr);
		static const char *kSansSource =
			"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"80\" viewBox=\"0 0 200 80\">"
			"<rect x=\"0\" y=\"0\" width=\"200\" height=\"80\" fill=\"#ffffff\"/>"
			"<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"40\" fill=\"#000000\">H</text></svg>";
		NkSVGImage *sans = NkSVGImage::LoadFromMemory((const uint8 *)kSansSource, std::strlen(kSansSource));
		bool texteDit = false;
		int32 encreSans = -1;
		if (sans) {
			for (int32 k = 0; k < sans->SkippedCount(); ++k) {
				const char *n = sans->SkippedAt(k);
				if (n && std::strcmp(n, "text") == 0)
					texteDit = true;
			}
			NkImage r = sans->Rasterize(0, 0);
			encreSans = EncreDe(r, 0, 0, 200, 80).sombres;
			sans->Free();
		}
		// et AVEC la source rendue, le meme document peint de nouveau
		NkSVGCodec::SetDefaultGlyphSource(garde);
		NkImage avec = Decoder(kSansSource);
		const int32 encreAvec = EncreDe(avec, 0, 0, 200, 80).sombres;
		std::snprintf(det, sizeof(det),
					  "sans source : « text » nomme=%d, encre=%d (le fond blanc est peint quand meme) ; avec la "
					  "source injectee : encre=%d",
					  texteDit ? 1 : 0, encreSans, encreAvec);
		Verifier("3i. SANS source de glyphes injectee, <text> est SAUTE ET NOMME (le reste du document est peint "
				 "normalement) ; la meme source injectee, il est peint -- l'injection est le seul chemin",
				 texteDit && encreSans == 0 && encreAvec > 60, det);
	}

	// (h) L'AVANCE : trois H cote a cote occupent ~trois fois la largeur d'un seul.
	//     Sans avance, les glyphes s'EMPILENT au meme x -- la boite resterait
	//     celle d'un seul glyphe, et l'encre serait a peine plus dense.
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\">H</text>");
	Encre un = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"60\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\">HHH</text>");
	Encre trois = EncreDe(Decoder(svg), 0, 0, 200, 80);
	const int32 l1 = un.xmax - un.xmin + 1, l3 = trois.xmax - trois.xmin + 1;
	const bool avance = un.sombres > 20 && l1 > 0 && l3 >= 2 * l1 && trois.sombres >= 2 * un.sombres &&
						trois.ymin == un.ymin && trois.ymax == un.ymax;
	std::snprintf(det, sizeof(det), "un H : %d px de large, %d d'encre ; trois H : %d px de large, %d d'encre "
								   "(meme hauteur y[%d..%d] contre y[%d..%d])",
				  l1, un.sombres, l3, trois.sombres, trois.ymin, trois.ymax, un.ymin, un.ymax);
	Verifier("3h. L'AVANCE d'un glyphe a l'autre : trois H occupent au moins deux fois la largeur d'un seul et "
			 "portent au moins deux fois l'encre -- ils ne s'empilent pas",
			 avance, det);
}


// ─────────────────────────────────────────────────────────────────────────────
// PALIER 4 — les degrades COMPLETS : gradientTransform partout, fx/fy, href
// ─────────────────────────────────────────────────────────────────────────────

/// Une page 100x100 avec des <defs> et un rect qui les utilise.
static void PageDegrade(char *out, usize n, const char *defs, const char *fill) {
	std::snprintf(out, n,
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
				  "<defs>%s</defs>"
				  "<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"%s\"/></svg>",
				  defs, fill);
}

static void TestDegrades() {
	std::printf("\n== PALIER 4 : degrades ==\n");
	char det[640];
	static char svg[4096];

	// (a) gradientTransform en objectBoundingBox. Un degrade HORIZONTAL tourne de
	//     90 degres doit devenir VERTICAL. C'etait le defaut : gradientTransform
	//     n'etait honore qu'en userSpaceOnUse, et IGNORE EN SILENCE ici.
	PageDegrade(svg, sizeof(svg),
				"<linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\" "
				"gradientTransform=\"rotate(90 0.5 0.5)\">"
				"<stop offset=\"0\" stop-color=\"#000000\"/><stop offset=\"1\" stop-color=\"#ffffff\"/>"
				"</linearGradient>",
				"url(#g)");
	NkImage a = Decoder(svg);
	const int32 hautG = Gris(a, 10, 10), hautD = Gris(a, 90, 10);
	const int32 basG = Gris(a, 10, 90), basD = Gris(a, 90, 90);
	// vertical : ca varie du haut vers le bas, et PAS de gauche a droite
	const bool vertical = a.IsValid() && hautG < basG - 100 && std::abs(hautG - hautD) < 12 &&
						  std::abs(basG - basD) < 12;
	std::snprintf(det, sizeof(det), "gris : haut %d/%d, bas %d/%d -- vertical (haut != bas, gauche == droite)=%d",
				  hautG, hautD, basG, basD, vertical ? 1 : 0);
	Verifier("4a. gradientTransform est honore AUSSI en objectBoundingBox : « rotate(90) » sur un degrade "
			 "horizontal le rend VERTICAL (il etait ignore en silence dans ce mode)",
			 vertical, det);

	// (b) le FOYER d'un radial (fx/fy) : le blanc est AU FOYER, pas au centre.
	PageDegrade(svg, sizeof(svg),
				"<radialGradient id=\"r\" cx=\"0.5\" cy=\"0.5\" r=\"0.5\" fx=\"0.2\" fy=\"0.2\">"
				"<stop offset=\"0\" stop-color=\"#ffffff\"/><stop offset=\"1\" stop-color=\"#000000\"/>"
				"</radialGradient>",
				"url(#r)");
	NkImage b = Decoder(svg);
	const int32 auFoyer = Gris(b, 20, 20), auCentre = Gris(b, 50, 50), oppose = Gris(b, 80, 80);
	const bool foyer = b.IsValid() && auFoyer > auCentre + 20 && auCentre > oppose;
	std::snprintf(det, sizeof(det), "gris au foyer (20,20)=%d ; au centre (50,50)=%d ; a l'oppose (80,80)=%d",
				  auFoyer, auCentre, oppose);
	Verifier("4b. <radialGradient fx fy> : le point le plus clair est LE FOYER, pas le centre du cercle", foyer, det);

	// (c) sans fx/fy le foyer EST le centre : le degrade est concentrique.
	PageDegrade(svg, sizeof(svg),
				"<radialGradient id=\"r2\" cx=\"0.5\" cy=\"0.5\" r=\"0.5\">"
				"<stop offset=\"0\" stop-color=\"#ffffff\"/><stop offset=\"1\" stop-color=\"#000000\"/>"
				"</radialGradient>",
				"url(#r2)");
	NkImage c = Decoder(svg);
	const int32 c0 = Gris(c, 50, 50), c1 = Gris(c, 20, 20), c2 = Gris(c, 80, 80), c3 = Gris(c, 80, 20);
	const bool concentrique = c.IsValid() && c0 > c1 + 20 && std::abs(c1 - c2) < 12 && std::abs(c1 - c3) < 12;
	std::snprintf(det, sizeof(det), "centre=%d ; les trois coins a egale distance : %d / %d / %d", c0, c1, c2, c3);
	Verifier("4c. sans fx/fy, le degrade radial est CONCENTRIQUE : trois points a egale distance du centre ont la "
			 "meme valeur",
			 concentrique, det);

	// (d) href : un degrade herite la GEOMETRIE de celui qu'il reference, pas
	//     seulement ses arrets. Ici la base est VERTICALE ; le fils ne dit rien.
	PageDegrade(svg, sizeof(svg),
				"<linearGradient id=\"base\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">"
				"<stop offset=\"0\" stop-color=\"#000000\"/><stop offset=\"1\" stop-color=\"#ffffff\"/>"
				"</linearGradient>"
				"<linearGradient id=\"fils\" href=\"#base\"/>",
				"url(#fils)");
	NkImage d = Decoder(svg);
	const int32 dh = Gris(d, 50, 10), db = Gris(d, 50, 90), dg = Gris(d, 10, 50), dd = Gris(d, 90, 50);
	const bool herite = d.IsValid() && dh < db - 100 && std::abs(dg - dd) < 12;
	std::snprintf(det, sizeof(det), "haut=%d bas=%d (la base est verticale) ; gauche=%d droite=%d (egales)", dh, db,
				  dg, dd);
	Verifier("4d. href herite la GEOMETRIE, pas seulement les arrets : un fils muet reprend le x1/y1/x2/y2 de sa "
			 "base (verticale) au lieu de retomber sur l'horizontale par defaut",
			 herite, det);

	// (e) spreadMethod=\"repeat\" en userSpaceOnUse : le motif se REPETE.
	PageDegrade(svg, sizeof(svg),
				"<linearGradient id=\"rep\" gradientUnits=\"userSpaceOnUse\" x1=\"0\" y1=\"0\" x2=\"25\" y2=\"0\" "
				"spreadMethod=\"repeat\">"
				"<stop offset=\"0\" stop-color=\"#000000\"/><stop offset=\"1\" stop-color=\"#ffffff\"/>"
				"</linearGradient>",
				"url(#rep)");
	NkImage e = Decoder(svg);
	const int32 p0 = Gris(e, 5, 50), p1 = Gris(e, 30, 50), p2 = Gris(e, 55, 50), p3 = Gris(e, 80, 50);
	const bool repete = e.IsValid() && std::abs(p0 - p1) < 14 && std::abs(p1 - p2) < 14 && std::abs(p2 - p3) < 14 &&
						Gris(e, 20, 50) > p0 + 100;
	std::snprintf(det, sizeof(det), "meme phase tous les 25 px : %d / %d / %d / %d ; fin de periode (20,50)=%d", p0,
				  p1, p2, p3, Gris(e, 20, 50));
	Verifier("4e. spreadMethod=\"repeat\" (userSpaceOnUse) : le motif se repete tous les 25 px, la meme phase rend "
			 "la meme valeur",
			 repete, det);

	// (f) stop-opacity : un arret transparent laisse voir CE QU'IL Y A DESSOUS.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
				  "<defs><linearGradient id=\"o\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\">"
				  "<stop offset=\"0\" stop-color=\"#ff0000\" stop-opacity=\"1\"/>"
				  "<stop offset=\"1\" stop-color=\"#ff0000\" stop-opacity=\"0\"/></linearGradient></defs>"
				  "<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"url(#o)\"/></svg>");
	NkImage f = Decoder(svg);
	const int32 aG = AlphaDe(f, 5, 50), aD = AlphaDe(f, 95, 50);
	const bool opacite = f.IsValid() && aG > 230 && aD < 25;
	std::snprintf(det, sizeof(det), "alpha a gauche (stop-opacity 1) = %d ; a droite (stop-opacity 0) = %d", aG, aD);
	Verifier("4f. stop-opacity : l'arret transparent rend le degrade transparent de son cote (l'alpha varie, pas "
			 "seulement la couleur)",
			 opacite, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER 5 — <filter><feDropShadow> : l'ombre est l'alpha du GROUPE
// ─────────────────────────────────────────────────────────────────────────────
static void TestOmbre() {
	std::printf("\n== PALIER 5 : <feDropShadow> ==\n");
	char det[640];
	static char svg[4096];

	// (a) une ombre NETTE (stdDeviation=0) decalee de +12,+12 : elle est SOUS la
	//     forme, decalee, et de la couleur demandee.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
				  "<defs><filter id=\"o\" x=\"-50%%\" y=\"-50%%\" width=\"200%%\" height=\"200%%\">"
				  "<feDropShadow dx=\"12\" dy=\"12\" stdDeviation=\"0\" flood-color=\"#ff0000\" "
				  "flood-opacity=\"1\"/></filter></defs>"
				  "<g filter=\"url(#o)\"><rect x=\"20\" y=\"20\" width=\"50\" height=\"50\" fill=\"#000000\"/></g>"
				  "</svg>");
	NkImage a = Decoder(svg);
	// la forme : (20..70) ; l'ombre : (32..82). En (75,75) il n'y a QUE l'ombre.
	const bool forme = Proche(a, 45, 45, 0, 0, 0, 4);
	const bool ombreLa = Proche(a, 75, 75, 255, 0, 0, 6);
	const bool ombreDessous = Proche(a, 45, 45, 0, 0, 0, 4); // au centre : la forme, pas l'ombre
	const bool rienAvant = AlphaDe(a, 10, 10) < 20;			 // avant la forme : rien
	const bool net = a.IsValid() && forme && ombreLa && ombreDessous && rienAvant;
	std::snprintf(det, sizeof(det), "forme noire au centre=%d ; ombre rouge en (75,75)=%d ; alpha en (10,10)=%d",
				  forme ? 1 : 0, ombreLa ? 1 : 0, AlphaDe(a, 10, 10));
	Verifier("5a. <feDropShadow dx dy> : une ombre de la couleur demandee, DECALEE, et posee SOUS la forme (le "
			 "centre reste noir, pas rouge)",
			 net, det);

	// (b) le FLOU : avec stdDeviation, le bord de l'ombre n'est plus franc --
	//     il existe des valeurs INTERMEDIAIRES la ou l'ombre nette n'en a pas.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
				  "<defs><filter id=\"o\"><feDropShadow dx=\"12\" dy=\"12\" stdDeviation=\"4\" "
				  "flood-color=\"#ff0000\"/></filter></defs>"
				  "<g filter=\"url(#o)\"><rect x=\"20\" y=\"20\" width=\"50\" height=\"50\" fill=\"#000000\"/></g>"
				  "</svg>");
	NkImage b = Decoder(svg);
	// LE BORD DE L'OMBRE EST A x=82 (le rect 20..70, decale de 12). Un flou se
	// mesure DE PART ET D'AUTRE de ce bord, a un demi ecart-type : DEHORS l'alpha
	// apparait, DEDANS il diminue. Une simple dilatation ne ferait que le premier ;
	// exiger les deux, c'est exiger une vraie transition.
	const int32 dehorsNet = AlphaDe(a, 84, 50), dehorsFlou = AlphaDe(b, 84, 50);
	const int32 dedansNet = AlphaDe(a, 80, 50), dedansFlou = AlphaDe(b, 80, 50);
	const int32 coeur = AlphaDe(b, 75, 75);
	const bool floute = b.IsValid() && dehorsNet == 0 && dehorsFlou > 40 && dedansNet > 250 &&
						dedansFlou < dedansNet - 40 && coeur > 150;
	std::snprintf(det, sizeof(det),
				  "a 2 px DEHORS du bord : net=%d flou=%d ; a 2 px DEDANS : net=%d flou=%d ; coeur de l'ombre=%d",
				  dehorsNet, dehorsFlou, dedansNet, dedansFlou, coeur);
	Verifier("5b. stdDeviation FLOUTE vraiment l'ombre : l'alpha apparait dehors ET diminue dedans (une "
			 "transition, pas une dilatation), sans vider le coeur",
			 floute, det);

	// (c) flood-opacity : l'ombre est plus transparente, la forme ne bouge pas.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
				  "<defs><filter id=\"o\"><feDropShadow dx=\"12\" dy=\"12\" stdDeviation=\"0\" "
				  "flood-color=\"#ff0000\" flood-opacity=\"0.25\"/></filter></defs>"
				  "<g filter=\"url(#o)\"><rect x=\"20\" y=\"20\" width=\"50\" height=\"50\" fill=\"#000000\"/></g>"
				  "</svg>");
	NkImage c = Decoder(svg);
	const int32 a25 = AlphaDe(c, 75, 75);
	const bool opac = c.IsValid() && a25 > 45 && a25 < 80 && Proche(c, 45, 45, 0, 0, 0, 4);
	std::snprintf(det, sizeof(det), "alpha de l'ombre a flood-opacity=0.25 : %d (attendu ~64) ; la forme est "
								   "intacte=%d",
				  a25, Proche(c, 45, 45, 0, 0, 0, 4) ? 1 : 0);
	Verifier("5c. flood-opacity attenue l'ombre SANS toucher a la forme", opac, det);

	// (d) une primitive de filtre non geree est NOMMEE (elle change l'image :
	//     la sauter en silence ferait croire que le filtre a ete applique).
	static const char *kAutre =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<defs><filter id=\"f\"><feGaussianBlur stdDeviation=\"3\"/><feTurbulence baseFrequency=\"0.1\"/>"
		"</filter></defs><g filter=\"url(#f)\"><rect x=\"5\" y=\"5\" width=\"30\" height=\"30\" fill=\"#000\"/></g>"
		"</svg>";
	NkSVGImage *img = NkSVGImage::LoadFromMemory((const uint8 *)kAutre, std::strlen(kAutre));
	bool blurEncoreDit = false, turbDit = false;
	bool formePeinte = false;
	if (img) {
		for (int32 i = 0; i < img->SkippedCount(); ++i) {
			const char *n = img->SkippedAt(i);
			if (n && std::strcmp(n, "feGaussianBlur") == 0)
				blurEncoreDit = true; // GERE depuis le palier du graphe : plus ici
			if (n && std::strcmp(n, "feTurbulence") == 0)
				turbDit = true;
		}
		// et le groupe est peint QUAND MEME : une primitive inconnue laisse passer
		// son entree, ce qui garde le graphe entier utilisable.
		NkImage r = img->Rasterize(0, 0);
		formePeinte = AlphaDe(r, 20, 20) > 100;
		img->Free();
	}
	std::snprintf(det, sizeof(det), "feTurbulence nomme=%d ; feGaussianBlur n'est PLUS annonce comme saute (il "
								   "est applique)=%d ; le groupe reste peint=%d",
				  turbDit ? 1 : 0, blurEncoreDit ? 0 : 1, formePeinte ? 1 : 0);
	Verifier("5d. une primitive de filtre INCONNUE est NOMMEE et laisse passer son entree (le groupe reste "
			 "peint) ; celles qui sont implementees ne sont plus annoncees comme sautees",
			 turbDit && !blurEncoreDit && formePeinte, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER 6 — opacites composees et mix-blend-mode
// ─────────────────────────────────────────────────────────────────────────────
static void TestOpacites() {
	std::printf("\n== PALIER 6 : opacites et fusion ==\n");
	char det[640];
	static char svg[4096];

	// (a) opacity ET fill-opacity se COMPOSENT : 0.5 x 0.5 = 0.25.
	//     Un noir a 25 % sur du blanc donne 191.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
				  "<rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ffffff\"/>"
				  "<rect x=\"10\" y=\"10\" width=\"40\" height=\"40\" fill=\"#000000\" opacity=\"0.5\" "
				  "fill-opacity=\"0.5\"/></svg>");
	NkImage a = Decoder(svg);
	const int32 g = Gris(a, 30, 30);
	const bool compose = a.IsValid() && g > 180 && g < 202;
	std::snprintf(det, sizeof(det), "gris obtenu %d (0.5 x 0.5 = 0.25 de noir sur blanc -> ~191 ; une seule des "
								   "deux opacites donnerait ~128)",
				  g);
	Verifier("6a. opacity ET fill-opacity se COMPOSENT (0,25 au total, pas 0,5)", compose, det);

	// (b) mix-blend-mode:multiply -- un gris a 50 % sur du rouge donne du rouge
	//     sombre : le vert et le bleu restent a zero, le rouge est divise par deux.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
				  "<rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ff0000\"/>"
				  "<rect x=\"10\" y=\"10\" width=\"40\" height=\"40\" fill=\"#808080\" "
				  "style=\"mix-blend-mode:multiply\"/></svg>");
	NkImage b = Decoder(svg);
	uint8 pb[4];
	{
		uint8 tmp[4];
		Pixel(b, 30, 30, tmp);
		pb[0] = tmp[0];
		pb[1] = tmp[1];
		pb[2] = tmp[2];
		pb[3] = tmp[3];
	}
	const bool multiplie = b.IsValid() && pb[0] > 118 && pb[0] < 138 && pb[1] < 8 && pb[2] < 8;
	std::snprintf(det, sizeof(det), "(%u,%u,%u) -- multiply de #808080 sur #ff0000 attend ~(128,0,0), le mode "
								   "ignore aurait laisse (128,128,128)",
				  pb[0], pb[1], pb[2]);
	Verifier("6b. mix-blend-mode:multiply est PEINT (le fond rouge assombrit la source grise)", multiplie, det);

	// (c) screen : la meme source ECLAIRCIT au lieu d'assombrir.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
				  "<rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ff0000\"/>"
				  "<rect x=\"10\" y=\"10\" width=\"40\" height=\"40\" fill=\"#808080\" "
				  "style=\"mix-blend-mode:screen\"/></svg>");
	NkImage c = Decoder(svg);
	uint8 pc[4];
	Pixel(c, 30, 30, pc);
	const bool ecran = c.IsValid() && pc[0] > 245 && pc[1] > 118 && pc[1] < 138 && pc[2] > 118 && pc[2] < 138;
	std::snprintf(det, sizeof(det), "(%u,%u,%u) -- screen attend ~(255,128,128)", pc[0], pc[1], pc[2]);
	Verifier("6c. mix-blend-mode:screen est PEINT (la source eclaircit le fond)", ecran, det);

	// (d) un mode NON gere est nomme et rendu en « normal » -- pas devine.
	static const char *kAutre =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ff0000\"/>"
		"<rect x=\"10\" y=\"10\" width=\"40\" height=\"40\" fill=\"#808080\" "
		"style=\"mix-blend-mode:overlay\"/></svg>";
	NkImage d = Decoder(kAutre);
	uint8 pd[4];
	Pixel(d, 30, 30, pd);
	const bool normal = d.IsValid() && pd[0] > 118 && pd[0] < 138 && pd[1] > 118 && pd[1] < 138;
	std::snprintf(det, sizeof(det), "(%u,%u,%u) -- « overlay » n'est pas peint : la source est posee telle quelle",
				  pd[0], pd[1], pd[2]);
	Verifier("6d. un mix-blend-mode non gere (overlay) est rendu en NORMAL, pas approche par un autre mode", normal,
			 det);
}

// ─────────────────────────────────────────────────────────────────────────────
// LE TEMOIN CROISE — deux lecteurs du MEME document, compares en pixels
// -----------------------------------------------------------------------------
//  `tests/temoins/` porte trois fichiers ECRITS PAR NkUIDesign (sa sonde 82, le
//  2026-09-05) : la page exportee en PNG par son peintre, la MEME page exportee
//  en SVG par son ecrivain, et l'image 2x2 que le document reference.
//
//  Ces trois fichiers sont le seul endroit du banc ou l'entree ne vient pas du
//  test -- et c'est precisement le point : ils viennent d'un AUTRE programme.
//  Le PNG est la verite du peintre ; le SVG re-rasterise par ce codec doit
//  tomber au meme endroit. Quand les deux different, l'un des deux a tort, et le
//  banc dit lequel au lieu de moyenner.
//
//  AVANT CE LOT, la sonde 82 de NkUIDesign ne pouvait comparer que les formes et
//  les degrades : « texte et image : STRUCTURE seulement, le parseur maison ne
//  sait ni <text> ni <image> ». C'est ce qui change ici.
// ─────────────────────────────────────────────────────────────────────────────
static void TestTemoinCroise() {
	std::printf("\n== TEMOIN CROISE : l'export de NkUIDesign ==\n");
	char det[900];
	const char *dossier = "Kernel/Runtime/NKImage/tests/temoins";
	const char *cheminSvg = "Kernel/Runtime/NKImage/tests/temoins/sonde_export_page.svg";
	const char *cheminPng = "Kernel/Runtime/NKImage/tests/temoins/sonde_export_page.png";

	NkImage png;
	const bool pngLu = png.Load(cheminPng, 4) && png.Width() == 200 && png.Height() == 120;
	NkSVGImage *doc = NkSVGImage::LoadFromFile(cheminSvg);
	NkImage svg = doc ? doc->Rasterize(0, 0) : NkImage();
	const bool svgLu = svg.IsValid() && svg.Width() == 200 && svg.Height() == 120;
	if (!pngLu || !svgLu) {
		std::snprintf(det, sizeof(det), "PNG lu=%d ; SVG lu=%d -- les temoins sont dans %s", pngLu ? 1 : 0,
					  svgLu ? 1 : 0, dossier);
		Verifier("T. les deux temoins de NkUIDesign sont lisibles", false, det);
		if (doc)
			doc->Free();
		return;
	}
	(void)dossier;

	// ── (1) LES FORMES : ce que la sonde 82 comparait deja ────────────────
	const bool rouge = Proche(svg, 15, 15, 255, 0, 0, 3) && Proche(svg, 64, 15, 255, 0, 0, 3) &&
					   Proche(svg, 15, 44, 255, 0, 0, 3) && Proche(svg, 64, 44, 255, 0, 0, 3);
	const int32 q15 = Gris(svg, 110, 15), q25 = Gris(svg, 110, 25), q35 = Gris(svg, 110, 35),
				q45 = Gris(svg, 110, 45);
	const int32 g15 = Gris(png, 110, 15), g25 = Gris(png, 110, 25), g35 = Gris(png, 110, 35),
				g45 = Gris(png, 110, 45);
	auto ecart24 = [](int32 a, int32 b) { return std::abs(a - b) <= 24; };
	const bool degrade = ecart24(q15, g15) && ecart24(q25, g25) && ecart24(q35, g35) && ecart24(q45, g45);
	const bool tourne = Proche(svg, 155, 80, 0, 0, 255, 3) && Proche(svg, 155, 63, 0, 0, 255, 3);
	std::snprintf(det, sizeof(det),
				  "rect rouge aux quatre points=%d ; degrade SVG %d/%d/%d/%d contre PNG %d/%d/%d/%d (le peintre "
				  "peint 24 bandes, le codec interpole : ecart <= 24)=%d ; carre tourne=%d",
				  rouge ? 1 : 0, q15, q25, q35, q45, g15, g25, g35, g45, degrade ? 1 : 0, tourne ? 1 : 0);
	Verifier("T1. FORMES ET DEGRADES : le SVG re-rasterise tombe sur les memes pixels que le PNG du peintre",
			 rouge && degrade && tourne, det);

	// ── (2) L'IMAGE : c'est ICI que « structure » devient « pixels » ──────
	//     Le document pose une image 2x2 (magenta / vert) etiree dans 40x40 en
	//     preserveAspectRatio="none". Les quatre texels doivent tomber aux memes
	//     quatre points que dans le PNG -- et etre les MEMES couleurs.
	uint8 pp[4][4], ps[4][4];
	const int32 pts[4][2] = {{82, 62}, {117, 62}, {82, 97}, {117, 97}};
	bool memeImage = true;
	for (int32 i = 0; i < 4; ++i) {
		Pixel(png, pts[i][0], pts[i][1], pp[i]);
		Pixel(svg, pts[i][0], pts[i][1], ps[i]);
		for (int32 k = 0; k < 3; ++k)
			if (std::abs((int32)pp[i][k] - (int32)ps[i][k]) > 6)
				memeImage = false;
	}
	std::snprintf(det, sizeof(det),
				  "PNG (%u,%u,%u) (%u,%u,%u) (%u,%u,%u) (%u,%u,%u) | SVG (%u,%u,%u) (%u,%u,%u) (%u,%u,%u) "
				  "(%u,%u,%u)",
				  pp[0][0], pp[0][1], pp[0][2], pp[1][0], pp[1][1], pp[1][2], pp[2][0], pp[2][1], pp[2][2], pp[3][0],
				  pp[3][1], pp[3][2], ps[0][0], ps[0][1], ps[0][2], ps[1][0], ps[1][1], ps[1][2], ps[2][0], ps[2][1],
				  ps[2][2], ps[3][0], ps[3][1], ps[3][2]);
	Verifier("T2. L'IMAGE, EN PIXELS (elle n'etait que « structure ») : les quatre texels du damier 2x2 tombent "
			 "aux memes points et aux memes couleurs que dans le PNG du peintre",
			 memeImage, det);

	// ── (3) LE TEXTE : de l'encre au meme endroit ─────────────────────────
	//     Le document pose « Ab » a 14 px sur un fond blanc, boite (10,60)-(70,90).
	Encre ep = EncreDe(png, 8, 58, 72, 92);
	Encre es = EncreDe(svg, 8, 58, 72, 92);
	const bool encrePresente = es.sombres > 20;
	// la ligne de base est la MEME : le bas de l'encre doit coincider a 2 px pres
	const bool memeBase = encrePresente && std::abs(es.ymax - ep.ymax) <= 2;
	// et le texte commence au meme x
	const bool memeDepart = encrePresente && std::abs(es.xmin - ep.xmin) <= 3;
	std::snprintf(det, sizeof(det),
				  "PNG : %d px d'encre, x[%d..%d] y[%d..%d] | SVG : %d px d'encre, x[%d..%d] y[%d..%d] -- meme "
				  "ligne de base (%d) et meme depart (%d)",
				  ep.sombres, ep.xmin, ep.xmax, ep.ymin, ep.ymax, es.sombres, es.xmin, es.xmax, es.ymin, es.ymax,
				  memeBase ? 1 : 0, memeDepart ? 1 : 0);
	Verifier("T3. LE TEXTE, EN PIXELS (il n'etait que « structure ») : de l'encre dans la meme boite, posee sur la "
			 "MEME ligne de base et au meme depart que le peintre",
			 encrePresente && memeBase && memeDepart, det);

	// ── (4) CE QUE LE TEMOIN MESURE ET QUI N'EST PAS EGAL : LA TAILLE ─────
	//     `font-size` vaut le CADRATIN (em) en SVG ; NkFontAtlas, lui, echelonne
	//     par (ascender - descender). Pour Inter les deux different, donc un
	//     `font-size="14"` ecrit par l'export ne rend PAS la meme hauteur que le
	//     peintre a 14 px. On MESURE l'ecart et on le NOMME plutot que de plier le
	//     codec : c'est le SVG qui doit etre juste pour un navigateur.
	const int32 hp = ep.ymax - ep.ymin + 1, hs = es.ymax - es.ymin + 1;
	const float32 rapport = (hp > 0) ? (float32)hs / (float32)hp : 0.f;
	const bool ecartConnu = encrePresente && hp > 0 && rapport > 1.05f && rapport < 1.45f;
	std::snprintf(det, sizeof(det),
				  "hauteur d'encre : peintre %d px, codec %d px -> rapport %.3f. LE CODEC SUIT LA NORME (echelle "
				  "= font-size / unitsPerEm) ; le peintre echelonne par (ascender - descender). L'export ecrit la "
				  "taille DU PEINTRE dans un attribut qui, en SVG, est un cadratin : un navigateur rendra ce "
				  "texte comme nous, plus grand que le PNG.",
				  hp, hs, (double)rapport);
	Verifier("T4. L'ECART DE TAILLE EST MESURE ET NOMME, pas masque : le texte du SVG est plus haut que celui du "
			 "PNG parce que l'export ecrit une taille de peintre dans un attribut qui vaut un cadratin",
			 ecartConnu, det);

	// ── (5) L'OMBRE PORTEE ET LE FOND DE PAGE ────────────────────────────
	uint8 fp[4], fs[4];
	Pixel(png, 5, 115, fp);
	Pixel(svg, 5, 115, fs);
	bool memeFond = true;
	for (int32 k = 0; k < 4; ++k)
		if (std::abs((int32)fp[k] - (int32)fs[k]) > 3)
			memeFond = false;
	// l'ombre du rect rouge (dy=4, flou 4) : juste SOUS lui, plus sombre que la page
	const int32 ombrePng = Gris(png, 40, 53), ombreSvg = Gris(svg, 40, 53);
	const bool ombre = ombreSvg < 250 && std::abs(ombrePng - ombreSvg) <= 40;
	std::snprintf(det, sizeof(det),
				  "fond de page PNG (%u,%u,%u,%u) = SVG (%u,%u,%u,%u), alpha compris -> %d ; sous le rect rouge "
				  "(l'ombre) : PNG %d, SVG %d",
				  fp[0], fp[1], fp[2], fp[3], fs[0], fs[1], fs[2], fs[3], memeFond ? 1 : 0, ombrePng, ombreSvg);
	Verifier("T5. le FOND DE PAGE est identique alpha compris, et l'OMBRE PORTEE assombrit le meme endroit dans "
			 "les deux lecteurs",
			 memeFond && ombre, det);

	// ── (6) et ce que le codec a saute sur un fichier REEL ────────────────
	std::snprintf(det, sizeof(det), "%d chose(s) sautee(s) : ", doc->SkippedCount());
	for (int32 i = 0; i < doc->SkippedCount(); ++i) {
		std::strncat(det, doc->SkippedAt(i) ? doc->SkippedAt(i) : "?", sizeof(det) - std::strlen(det) - 1);
		std::strncat(det, " ", sizeof(det) - std::strlen(det) - 1);
	}
	Verifier("T6. sur un fichier REEL produit par une autre application, le codec ne saute plus rien en silence "
			 "(la liste ci-dessous est vide, ou nommee)",
			 doc->SkippedCount() == 0, det);

	doc->Free();
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER <use> / <symbol> — instancier par reference
// ─────────────────────────────────────────────────────────────────────────────

/// Combien de pixels different entre deux images (au-dela de @p tol par canal) ?
/// C'est LE critere d'une instanciation : le fichier a <use> doit rendre CE QUE
/// RENDRAIT sa version aplatie -- pas « a peu pres », pas « aux points qu'on a
/// choisi de regarder ». On compare TOUT.
static int32 PixelsDifferents(const NkImage &a, const NkImage &b, int32 tol) {
	if (!a.IsValid() || !b.IsValid() || a.Width() != b.Width() || a.Height() != b.Height())
		return -1;
	int32 n = 0;
	for (int32 y = 0; y < a.Height(); ++y)
		for (int32 x = 0; x < a.Width(); ++x) {
			uint8 pa[4], pb[4];
			Pixel(a, x, y, pa);
			Pixel(b, x, y, pb);
			for (int32 k = 0; k < 4; ++k)
				if (std::abs((int32)pa[k] - (int32)pb[k]) > tol) {
					++n;
					break;
				}
		}
	return n;
}

static void TestUse() {
	std::printf("\n== PALIER <use> / <symbol> ==\n");
	char det[640];

	// (a) LE TEMOIN DU PALIER : un fichier a <use> IMBRIQUES contre sa version
	//     APLATIE, ecrite a la main juste en dessous. Les deux doivent rendre les
	//     memes pixels -- tous.
	//     « brique » = un carre rouge et un carre bleu ; « paire » = deux briques
	//     dont une decalee ; la page instancie « paire » deux fois, a deux places.
	static const char *kUse =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
		"<defs>"
		"<g id=\"brique\">"
		"<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#ff0000\"/>"
		"<rect x=\"10\" y=\"0\" width=\"10\" height=\"10\" fill=\"#0000ff\"/>"
		"</g>"
		"<g id=\"paire\">"
		"<use href=\"#brique\"/>"
		"<use href=\"#brique\" x=\"0\" y=\"20\"/>"
		"</g>"
		"</defs>"
		"<use href=\"#paire\" x=\"10\" y=\"10\"/>"
		"<use href=\"#paire\" x=\"60\" y=\"60\"/>"
		"</svg>";
	static const char *kPlat =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
		"<g><g><rect x=\"10\" y=\"10\" width=\"10\" height=\"10\" fill=\"#ff0000\"/>"
		"<rect x=\"20\" y=\"10\" width=\"10\" height=\"10\" fill=\"#0000ff\"/></g>"
		"<g><rect x=\"10\" y=\"30\" width=\"10\" height=\"10\" fill=\"#ff0000\"/>"
		"<rect x=\"20\" y=\"30\" width=\"10\" height=\"10\" fill=\"#0000ff\"/></g></g>"
		"<g><g><rect x=\"60\" y=\"60\" width=\"10\" height=\"10\" fill=\"#ff0000\"/>"
		"<rect x=\"70\" y=\"60\" width=\"10\" height=\"10\" fill=\"#0000ff\"/></g>"
		"<g><rect x=\"60\" y=\"80\" width=\"10\" height=\"10\" fill=\"#ff0000\"/>"
		"<rect x=\"70\" y=\"80\" width=\"10\" height=\"10\" fill=\"#0000ff\"/></g></g>"
		"</svg>";
	NkImage avecUse = Decoder(kUse);
	NkImage aplati = Decoder(kPlat);
	const int32 diff = PixelsDifferents(avecUse, aplati, 2);
	// et le rendu n'est pas vide : les quatre briques sont bien la
	const bool contenu = Proche(avecUse, 15, 15, 255, 0, 0, 3) && Proche(avecUse, 25, 15, 0, 0, 255, 3) &&
						 Proche(avecUse, 15, 35, 255, 0, 0, 3) && Proche(avecUse, 65, 85, 255, 0, 0, 3) &&
						 Proche(avecUse, 75, 65, 0, 0, 255, 3);
	std::snprintf(det, sizeof(det), "%d pixel(s) different(s) sur %d ; les quatre briques instanciees sont la=%d",
				  diff, avecUse.IsValid() ? avecUse.Width() * avecUse.Height() : 0, contenu ? 1 : 0);
	Verifier("U1. <use> IMBRIQUES (un <use> dans un <g> lui-meme instancie par <use>) rend EXACTEMENT la meme "
			 "image que la version aplatie ecrite a la main -- tous les pixels compares",
			 diff == 0 && contenu, det);

	// (b) un <defs> ne se rend PAS la ou il est defini : sans les <use>, la page
	//     est vide. C'est ce qui distingue « instancier » de « dessiner deux fois ».
	static const char *kDefsSeul =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<defs><g id=\"b\"><rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ff0000\"/></g></defs>"
		"<symbol id=\"s\"><rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#00ff00\"/></symbol>"
		"</svg>";
	NkImage vide = Decoder(kDefsSeul);
	const bool rienRendu = vide.IsValid() && AlphaDe(vide, 30, 30) < 20 && AlphaDe(vide, 5, 5) < 20;
	std::snprintf(det, sizeof(det), "alpha au centre=%d -- ni le <defs> ni le <symbol> ne se peignent d'eux-memes",
				  AlphaDe(vide, 30, 30));
	Verifier("U2. <defs> ET <symbol> ne se rendent PAS la ou ils sont definis (seul <use> les instancie)", rienRendu,
			 det);

	// (c) <symbol viewBox> instancie avec width/height : il est MIS A L'ECHELLE.
	static const char *kSymbole =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<symbol id=\"carre\" viewBox=\"0 0 10 10\">"
		"<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#ff0000\"/></symbol>"
		"<use href=\"#carre\" x=\"20\" y=\"20\" width=\"60\" height=\"60\"/></svg>";
	NkImage sym = Decoder(kSymbole);
	// le carre de 10 devient 60x60 a partir de (20,20) : plein a (50,50) et (25,25),
	// vide juste a cote (85,85) et avant (15,15)
	const bool echelle = sym.IsValid() && Proche(sym, 50, 50, 255, 0, 0, 3) && Proche(sym, 25, 25, 255, 0, 0, 3) &&
						 Proche(sym, 75, 75, 255, 0, 0, 3) && AlphaDe(sym, 85, 85) < 20 && AlphaDe(sym, 15, 15) < 20;
	std::snprintf(det, sizeof(det),
				  "rouge en (25,25) (50,50) (75,75) ; alpha en (15,15)=%d et (85,85)=%d -- le symbole de 10 unites "
				  "occupe bien 60 px a partir de (20,20)",
				  AlphaDe(sym, 15, 15), AlphaDe(sym, 85, 85));
	Verifier("U3. <symbol viewBox> instancie avec width/height est MIS A L'ECHELLE (sa viewBox devient le repere "
			 "de la boite du <use>)",
			 echelle, det);

	// (d) le <use> herite du STYLE et compose les TRANSFORMATIONS.
	static const char *kStyle =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><g id=\"c\"><rect x=\"0\" y=\"0\" width=\"20\" height=\"20\"/></g></defs>"
		"<g transform=\"translate(50 0)\"><use href=\"#c\" x=\"10\" y=\"10\" fill=\"#00ff00\"/></g></svg>";
	NkImage st = Decoder(kStyle);
	// translate(50) du parent + x/y du use : le carre va de (60,10) a (80,30), en VERT
	const bool compose = st.IsValid() && Proche(st, 70, 20, 0, 255, 0, 3) && AlphaDe(st, 20, 20) < 20;
	std::snprintf(det, sizeof(det), "vert en (70,20)=%d (translate du parent + x/y du use) ; rien en (20,20) : "
								   "alpha=%d",
				  Proche(st, 70, 20, 0, 255, 0, 3) ? 1 : 0, AlphaDe(st, 20, 20));
	Verifier("U4. <use> COMPOSE la transformation de son parent, la sienne et ses x/y, et transmet son style "
			 "(fill herite jusqu'au contenu instancie)",
			 compose, det);

	// (e) UNE REFERENCE CIRCULAIRE ne fait pas exploser la pile : elle est bornee
	//     ET NOMMEE. Un decodeur qui plante sur un fichier tordu est un decodeur
	//     qu'on ne peut pas exposer a des fichiers venus d'ailleurs.
	static const char *kBoucle =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<defs><g id=\"boucle\"><rect x=\"0\" y=\"0\" width=\"5\" height=\"5\" fill=\"#000\"/>"
		"<use href=\"#boucle\" x=\"1\" y=\"1\"/></g></defs>"
		"<use href=\"#boucle\"/></svg>";
	NkSVGImage *b = NkSVGImage::LoadFromMemory((const uint8 *)kBoucle, std::strlen(kBoucle));
	bool recursionDite = false;
	int32 formes = 0;
	if (b) {
		formes = b->ShapeCount();
		for (int32 i = 0; i < b->SkippedCount(); ++i) {
			const char *n = b->SkippedAt(i);
			if (n && std::strcmp(n, "use-recursion") == 0)
				recursionDite = true;
		}
		b->Free();
	}
	std::snprintf(det, sizeof(det), "%d forme(s) produites, la garde a arrete l'instanciation et l'a NOMMEE=%d",
				  formes, recursionDite ? 1 : 0);
	Verifier("U5. une REFERENCE CIRCULAIRE est bornee ET NOMMEE (le decodeur ne plante pas, et ne se tait pas)",
			 b != nullptr && recursionDite && formes > 0 && formes <= 16, det);

	// (f) une cible INTROUVABLE : rien n'est instancie, et c'est dit.
	static const char *kAbsent =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<use href=\"#jamaisDefini\"/></svg>";
	NkSVGImage *a2 = NkSVGImage::LoadFromMemory((const uint8 *)kAbsent, std::strlen(kAbsent));
	bool absentDit = false;
	if (a2) {
		for (int32 i = 0; i < a2->SkippedCount(); ++i) {
			const char *n = a2->SkippedAt(i);
			if (n && std::strcmp(n, "use-cible-absente") == 0)
				absentDit = true;
		}
		a2->Free();
	}
	std::snprintf(det, sizeof(det), "cible absente nommee=%d", absentDit ? 1 : 0);
	Verifier("U6. <use> vers une cible INTROUVABLE : rien n'est instancie, et c'est dit", absentDit, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER <clipPath> — ce qui est dehors n'existe pas
// ─────────────────────────────────────────────────────────────────────────────
static void TestClip() {
	std::printf("\n== PALIER <clipPath> ==\n");
	char det[640];

	// (a) un grand rectangle rouge decoupe par un petit carre : DEDANS il est
	//     peint, DEHORS il n'existe pas. C'est le cas qui rougit si clip-path est
	//     ignore -- toute la page serait rouge.
	static const char *kClip =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><clipPath id=\"c\"><rect x=\"30\" y=\"30\" width=\"40\" height=\"40\"/></clipPath></defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#ff0000\" clip-path=\"url(#c)\"/></svg>";
	NkImage a = Decoder(kClip);
	const bool dedans = Proche(a, 50, 50, 255, 0, 0, 3) && Proche(a, 32, 32, 255, 0, 0, 3) &&
						Proche(a, 68, 68, 255, 0, 0, 3);
	const bool dehors = AlphaDe(a, 10, 10) < 20 && AlphaDe(a, 90, 50) < 20 && AlphaDe(a, 50, 90) < 20 &&
						AlphaDe(a, 28, 50) < 20;
	std::snprintf(det, sizeof(det),
				  "dedans (50,50) (32,32) (68,68) rouge=%d ; dehors alpha (10,10)=%d (90,50)=%d (28,50)=%d",
				  dedans ? 1 : 0, AlphaDe(a, 10, 10), AlphaDe(a, 90, 50), AlphaDe(a, 28, 50));
	Verifier("C1. clip-path=\"url(#c)\" : la forme n'est peinte QUE dans la decoupe (une page entierement rouge "
			 "serait le signe d'un clip ignore)",
			 a.IsValid() && dedans && dehors, det);

	// (b) le clip s'herite d'un <g> et vaut pour TOUT son contenu.
	static const char *kGroupe =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><clipPath id=\"c\"><rect x=\"0\" y=\"0\" width=\"50\" height=\"100\"/></clipPath></defs>"
		"<g clip-path=\"url(#c)\">"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"40\" fill=\"#ff0000\"/>"
		"<rect x=\"0\" y=\"60\" width=\"100\" height=\"40\" fill=\"#0000ff\"/></g></svg>";
	NkImage b = Decoder(kGroupe);
	const bool herite = b.IsValid() && Proche(b, 25, 20, 255, 0, 0, 3) && Proche(b, 25, 80, 0, 0, 255, 3) &&
						AlphaDe(b, 75, 20) < 20 && AlphaDe(b, 75, 80) < 20;
	std::snprintf(det, sizeof(det), "les DEUX rects du groupe sont coupes a x=50 : rouge (25,20)=%d, bleu (25,80)=%d "
								   "; a droite alpha=%d / %d",
				  Proche(b, 25, 20, 255, 0, 0, 3) ? 1 : 0, Proche(b, 25, 80, 0, 0, 255, 3) ? 1 : 0,
				  AlphaDe(b, 75, 20), AlphaDe(b, 75, 80));
	Verifier("C2. un clip-path pose sur un <g> vaut pour TOUT son contenu", herite, det);

	// (c) DEUX clips imbriques s'INTERSECTENT : ne reste que ce que les deux gardent.
	static const char *kDeux =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs>"
		"<clipPath id=\"gauche\"><rect x=\"0\" y=\"0\" width=\"60\" height=\"100\"/></clipPath>"
		"<clipPath id=\"haut\"><rect x=\"0\" y=\"0\" width=\"100\" height=\"60\"/></clipPath>"
		"</defs>"
		"<g clip-path=\"url(#gauche)\"><g clip-path=\"url(#haut)\">"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#00aa00\"/></g></g></svg>";
	NkImage c = Decoder(kDeux);
	// ne survit que le quart haut-gauche (0..60, 0..60)
	const bool inter = c.IsValid() && Proche(c, 30, 30, 0, 170, 0, 4) && AlphaDe(c, 80, 30) < 20 &&
					   AlphaDe(c, 30, 80) < 20 && AlphaDe(c, 80, 80) < 20;
	std::snprintf(det, sizeof(det), "quart haut-gauche peint=%d ; les trois autres quarts : alpha %d / %d / %d",
				  Proche(c, 30, 30, 0, 170, 0, 4) ? 1 : 0, AlphaDe(c, 80, 30), AlphaDe(c, 30, 80), AlphaDe(c, 80, 80));
	Verifier("C3. deux clip-path IMBRIQUES s'intersectent (il ne reste que ce que les DEUX gardent)", inter, det);

	// (d) la decoupe suit une forme quelconque, pas seulement un rectangle : ici un
	//     cercle. Le coin de la boite sort du disque.
	static const char *kCercle =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><clipPath id=\"rond\"><circle cx=\"50\" cy=\"50\" r=\"30\"/></clipPath></defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#000000\" clip-path=\"url(#rond)\"/></svg>";
	NkImage d = Decoder(kCercle);
	// (50,50) dedans ; (50,25) sur le bord haut du disque -> dedans ; (28,28) hors
	// (distance 31,1 > 30) ; (10,10) tres loin
	const bool rond = d.IsValid() && AlphaDe(d, 50, 50) > 200 && AlphaDe(d, 50, 25) > 150 &&
					  AlphaDe(d, 28, 28) < 60 && AlphaDe(d, 10, 10) < 20;
	std::snprintf(det, sizeof(det), "alpha : centre=%d, bord haut=%d, coin du carre (28,28)=%d, loin (10,10)=%d",
				  AlphaDe(d, 50, 50), AlphaDe(d, 50, 25), AlphaDe(d, 28, 28), AlphaDe(d, 10, 10));
	Verifier("C4. la decoupe epouse une forme QUELCONQUE (un <circle> ici), pas seulement une boite", rond, det);

	// (e) le clip coupe AUSSI le trait, pas seulement le remplissage.
	static const char *kTrait =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><clipPath id=\"c\"><rect x=\"0\" y=\"0\" width=\"50\" height=\"100\"/></clipPath></defs>"
		"<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"none\" stroke=\"#ff0000\" stroke-width=\"8\" "
		"clip-path=\"url(#c)\"/></svg>";
	NkImage e = Decoder(kTrait);
	const bool traitCoupe = e.IsValid() && Proche(e, 12, 50, 255, 0, 0, 6) && AlphaDe(e, 88, 50) < 20;
	std::snprintf(det, sizeof(det), "trait gauche present=%d ; trait droit (hors decoupe) alpha=%d",
				  Proche(e, 12, 50, 255, 0, 0, 6) ? 1 : 0, AlphaDe(e, 88, 50));
	Verifier("C5. le decoupage s'applique AUSSI au trait (stroke), pas seulement au remplissage", traitCoupe, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER <mask> — un masque a des NUANCES, un decoupage n'en a pas
// ─────────────────────────────────────────────────────────────────────────────
static void TestMask() {
	std::printf("\n== PALIER <mask> ==\n");
	char det[640];

	// (a) LA DIFFERENCE AVEC UN CLIP, et c'est tout le palier : un <mask> qui
	//     contient un DEGRADE du noir au blanc produit un FONDU. Un <clipPath> ne
	//     sait pas exprimer ca -- il ne connait que dedans et dehors.
	static const char *kFondu =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"40\" viewBox=\"0 0 100 40\">"
		"<defs>"
		"<linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\">"
		"<stop offset=\"0\" stop-color=\"#000000\"/><stop offset=\"1\" stop-color=\"#ffffff\"/></linearGradient>"
		"<mask id=\"m\"><rect x=\"0\" y=\"0\" width=\"100\" height=\"40\" fill=\"url(#g)\"/></mask>"
		"</defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"40\" fill=\"#ff0000\" mask=\"url(#m)\"/></svg>";
	NkImage a = Decoder(kFondu);
	const int32 a5 = AlphaDe(a, 5, 20), a35 = AlphaDe(a, 35, 20), a65 = AlphaDe(a, 65, 20), a95 = AlphaDe(a, 95, 20);
	const bool fondu = a.IsValid() && a5 < 30 && a35 < a65 && a65 < a95 && a95 > 220 && a5 < a35;
	std::snprintf(det, sizeof(det), "alpha du rouge le long du masque degrade : %d / %d / %d / %d (croissant, du "
								   "transparent a l'opaque)",
				  a5, a35, a65, a95);
	Verifier("M1. <mask> avec un degrade : le resultat est un FONDU CONTINU (la LUMINANCE du masque devient "
			 "l'opacite) -- ce qu'un <clipPath> ne peut pas exprimer",
			 fondu, det);

	// (b) LUMINANCE, pas simple presence : un carre NOIR opaque dans un masque
	//     CACHE, alors qu'un carre BLANC opaque montre. Un masque lu comme un clip
	//     (couverture) montrerait les deux.
	static const char *kLum =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"50\" viewBox=\"0 0 100 50\">"
		"<defs><mask id=\"m\">"
		"<rect x=\"0\" y=\"0\" width=\"50\" height=\"50\" fill=\"#ffffff\"/>"
		"<rect x=\"50\" y=\"0\" width=\"50\" height=\"50\" fill=\"#000000\"/></mask></defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"50\" fill=\"#0000ff\" mask=\"url(#m)\"/></svg>";
	NkImage b = Decoder(kLum);
	const bool luminance = b.IsValid() && Proche(b, 25, 25, 0, 0, 255, 4) && AlphaDe(b, 75, 25) < 20;
	std::snprintf(det, sizeof(det), "sous le blanc du masque : bleu opaque=%d ; sous le NOIR du masque : alpha=%d "
								   "(un masque lu comme une simple couverture aurait montre les deux)",
				  Proche(b, 25, 25, 0, 0, 255, 4) ? 1 : 0, AlphaDe(b, 75, 25));
	Verifier("M2. c'est la LUMINANCE qui compte : du blanc montre, du NOIR cache -- meme opaque, meme couvrant",
			 luminance, det);

	// (c) mask-type=\"alpha\" : c'est alors l'ALPHA du masque qui compte, et le noir
	//     opaque montre.
	static const char *kAlpha =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"50\" viewBox=\"0 0 100 50\">"
		"<defs><mask id=\"m\" mask-type=\"alpha\">"
		"<rect x=\"0\" y=\"0\" width=\"50\" height=\"50\" fill=\"#000000\"/></mask></defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"50\" fill=\"#00ff00\" mask=\"url(#m)\"/></svg>";
	NkImage c = Decoder(kAlpha);
	const bool typeAlpha = c.IsValid() && Proche(c, 25, 25, 0, 255, 0, 4) && AlphaDe(c, 75, 25) < 20;
	std::snprintf(det, sizeof(det), "sous le noir OPAQUE en mask-type=alpha : vert visible=%d ; hors du masque : "
								   "alpha=%d",
				  Proche(c, 25, 25, 0, 255, 0, 4) ? 1 : 0, AlphaDe(c, 75, 25));
	Verifier("M3. mask-type=\"alpha\" : c'est l'ALPHA du masque qui compte, et un noir opaque MONTRE (l'inverse "
			 "du mode luminance -- ce n'est pas un detail, c'est le contraire)",
			 typeAlpha, det);

	// (d) un <mask> et un <clip-path> sur le meme element se CUMULENT.
	static const char *kDeux =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs>"
		"<clipPath id=\"c\"><rect x=\"0\" y=\"0\" width=\"100\" height=\"50\"/></clipPath>"
		"<mask id=\"m\"><rect x=\"0\" y=\"0\" width=\"50\" height=\"100\" fill=\"#ffffff\"/></mask>"
		"</defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"#ff8800\" clip-path=\"url(#c)\" "
		"mask=\"url(#m)\"/></svg>";
	NkImage d = Decoder(kDeux);
	const bool cumul = d.IsValid() && AlphaDe(d, 25, 25) > 200 && AlphaDe(d, 75, 25) < 20 &&
					   AlphaDe(d, 25, 75) < 20 && AlphaDe(d, 75, 75) < 20;
	std::snprintf(det, sizeof(det), "seul le quart haut-gauche survit : alpha %d / %d / %d / %d", AlphaDe(d, 25, 25),
				  AlphaDe(d, 75, 25), AlphaDe(d, 25, 75), AlphaDe(d, 75, 75));
	Verifier("M4. un mask ET un clip-path sur le meme element se CUMULENT (ils se multiplient, comme deux clips)",
			 cumul, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER CSS — ce qui rend lisibles les SVG des autres outils
// ─────────────────────────────────────────────────────────────────────────────
static void TestCSS() {
	std::printf("\n== PALIER CSS (<style>) ==\n");
	char det[640];

	// (a) LE CAS D'ILLUSTRATOR / FIGMA : les couleurs sont dans un <style>, les
	//     elements ne portent qu'un `class="st0"`. Sans CSS, tout retombe sur le
	//     noir par defaut -- le fichier s'ouvre « en silhouette ».
	static const char *kClasses =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"50\" viewBox=\"0 0 100 50\">"
		"<style>.st0{fill:#ff0000}.st1{fill:#0000ff}</style>"
		"<rect class=\"st0\" x=\"0\" y=\"0\" width=\"50\" height=\"50\"/>"
		"<rect class=\"st1\" x=\"50\" y=\"0\" width=\"50\" height=\"50\"/></svg>";
	NkImage a = Decoder(kClasses);
	const bool classes = a.IsValid() && Proche(a, 25, 25, 255, 0, 0, 3) && Proche(a, 75, 25, 0, 0, 255, 3);
	std::snprintf(det, sizeof(det), "gauche rouge=%d, droite bleu=%d (sans CSS les deux seraient NOIRS)",
				  Proche(a, 25, 25, 255, 0, 0, 3) ? 1 : 0, Proche(a, 75, 25, 0, 0, 255, 3) ? 1 : 0);
	Verifier("S1. selecteurs de CLASSE : les couleurs d'un <style> habillent les elements qui n'ont qu'un "
			 "`class=` (le cas de tous les exports Illustrator / Figma)",
			 classes, det);

	// (b) SPECIFICITE : id (100) bat classe (10) bat type (1), quel que soit
	//     l'ordre d'ecriture. Ici la regle la PLUS FAIBLE est ecrite en DERNIER :
	//     un codec qui appliquerait simplement dans l'ordre du fichier se tromperait.
	static const char *kSpec =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<style>#a{fill:#00ff00}.c{fill:#0000ff}rect{fill:#ff0000}</style>"
		"<rect id=\"a\" class=\"c\" x=\"0\" y=\"0\" width=\"60\" height=\"60\"/></svg>";
	NkImage b = Decoder(kSpec);
	const bool spec = b.IsValid() && Proche(b, 30, 30, 0, 255, 0, 3);
	uint8 pb[4];
	Pixel(b, 30, 30, pb);
	std::snprintf(det, sizeof(det), "(%u,%u,%u) -- l'id doit gagner (vert), meme si la regle de type est ecrite "
								   "en dernier",
				  pb[0], pb[1], pb[2]);
	Verifier("S2. SPECIFICITE : #id bat .classe bat type -- et pas « la derniere regle ecrite gagne »", spec, det);

	// (c) L'ORDRE DE LA NORME, contre-intuitif : une REGLE CSS ecrase un ATTRIBUT
	//     de presentation, et le `style=""` inline les bat tous les deux.
	static const char *kOrdre =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"90\" height=\"30\" viewBox=\"0 0 90 30\">"
		"<style>rect{fill:#00ff00}</style>"
		"<rect x=\"0\" y=\"0\" width=\"30\" height=\"30\" fill=\"#ff0000\"/>"
		"<rect x=\"30\" y=\"0\" width=\"30\" height=\"30\" fill=\"#ff0000\" style=\"fill:#0000ff\"/>"
		"<rect x=\"60\" y=\"0\" width=\"30\" height=\"30\"/></svg>";
	NkImage c = Decoder(kOrdre);
	const bool ordre = c.IsValid() && Proche(c, 15, 15, 0, 255, 0, 3) && Proche(c, 45, 15, 0, 0, 255, 3) &&
					   Proche(c, 75, 15, 0, 255, 0, 3);
	uint8 c1[4], c2[4];
	Pixel(c, 15, 15, c1);
	Pixel(c, 45, 15, c2);
	std::snprintf(det, sizeof(det),
				  "attribut fill=rouge + regle CSS verte -> (%u,%u,%u) : la REGLE gagne ; + style inline bleu -> "
				  "(%u,%u,%u) : l'INLINE gagne",
				  c1[0], c1[1], c1[2], c2[0], c2[1], c2[2]);
	Verifier("S3. l'ordre de la norme : attribut de presentation < regle CSS < style=\"\" inline (le codec faisait "
			 "gagner l'attribut sur le style inline : corrige)",
			 ordre, det);

	// (d) !important bat TOUT, y compris le style inline.
	static const char *kImportant =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"30\" viewBox=\"0 0 60 30\">"
		"<style>rect{fill:#00ff00 !important}</style>"
		"<rect x=\"0\" y=\"0\" width=\"60\" height=\"30\" fill=\"#ff0000\" style=\"fill:#0000ff\"/></svg>";
	NkImage d = Decoder(kImportant);
	uint8 pd[4];
	Pixel(d, 30, 15, pd);
	const bool important = d.IsValid() && Proche(d, 30, 15, 0, 255, 0, 3);
	std::snprintf(det, sizeof(det), "(%u,%u,%u) -- « !important » d'une regle bat meme le style inline", pd[0], pd[1],
				  pd[2]);
	Verifier("S4. !important bat TOUT, y compris le style=\"\" inline", important, det);

	// (e) `*`, les selecteurs groupes par virgule, un CDATA et un commentaire :
	//     la syntaxe reelle des fichiers, pas seulement celle des exemples.
	static const char *kSyntaxe =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<style><![CDATA[ /* un commentaire */ * { fill : #888888 } .a , .b { fill:#ff0000 } ]]></style>"
		"<rect class=\"b\" x=\"0\" y=\"0\" width=\"30\" height=\"60\"/>"
		"<rect x=\"30\" y=\"0\" width=\"30\" height=\"60\"/></svg>";
	NkImage e = Decoder(kSyntaxe);
	const bool syntaxe = e.IsValid() && Proche(e, 15, 30, 255, 0, 0, 3) && Proche(e, 45, 30, 136, 136, 136, 3);
	std::snprintf(det, sizeof(det), "classe groupee par virgule -> rouge=%d ; `*` -> gris=%d ; CDATA et "
								   "commentaire traverses",
				  Proche(e, 15, 30, 255, 0, 0, 3) ? 1 : 0, Proche(e, 45, 30, 136, 136, 136, 3) ? 1 : 0);
	Verifier("S5. la syntaxe REELLE des fichiers : CDATA, commentaires, espaces, `*` et selecteurs groupes par "
			 "virgule",
			 syntaxe, det);

	// (f) un selecteur qu'on ne sait pas lire est IGNORE **ET DIT** -- jamais
	//     apparie de travers : appliquer une regle au mauvais element est pire que
	//     ne pas l'appliquer.
	static const char *kInconnu =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<style>g > rect { fill:#ff0000 } rect:hover { fill:#00ff00 }</style>"
		"<rect x=\"0\" y=\"0\" width=\"60\" height=\"60\"/></svg>";
	NkSVGImage *img = NkSVGImage::LoadFromMemory((const uint8 *)kInconnu, std::strlen(kInconnu));
	bool selDit = false;
	if (img) {
		for (int32 i = 0; i < img->SkippedCount(); ++i) {
			const char *n = img->SkippedAt(i);
			if (n && std::strcmp(n, "css-selecteur") == 0)
				selDit = true;
		}
		img->Free();
	}
	NkImage f = Decoder(kInconnu);
	const bool pasApplique = f.IsValid() && !Proche(f, 30, 30, 255, 0, 0, 20) && !Proche(f, 30, 30, 0, 255, 0, 20);
	std::snprintf(det, sizeof(det), "selecteur non gere nomme=%d ; la regle n'a PAS ete appliquee de travers=%d",
				  selDit ? 1 : 0, pasApplique ? 1 : 0);
	Verifier("S6. un selecteur non gere (combinateur, pseudo-classe) est IGNORE et NOMME -- l'appliquer au "
			 "mauvais element serait pire que ne pas l'appliquer",
			 selDit && pasApplique, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER stroke-dasharray — un trait coupe, pas un trait troue
// ─────────────────────────────────────────────────────────────────────────────
static void TestDash() {
	std::printf("\n== PALIER stroke-dasharray ==\n");
	char det[640];

	// (a) une ligne horizontale en « 10 pleins / 10 vides » : on doit trouver de
	//     l'encre ET des trous, en ALTERNANCE, aux bons endroits.
	static const char *kDash =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"20\" viewBox=\"0 0 100 20\">"
		"<line x1=\"0\" y1=\"10\" x2=\"100\" y2=\"10\" stroke=\"#000000\" stroke-width=\"6\" "
		"stroke-dasharray=\"10 10\"/></svg>";
	NkImage a = Decoder(kDash);
	const int32 t0 = AlphaDe(a, 5, 10), v0 = AlphaDe(a, 15, 10);
	const int32 t1 = AlphaDe(a, 25, 10), v1 = AlphaDe(a, 35, 10);
	const int32 t2 = AlphaDe(a, 45, 10), v2 = AlphaDe(a, 55, 10);
	const bool alterne = a.IsValid() && t0 > 200 && v0 < 30 && t1 > 200 && v1 < 30 && t2 > 200 && v2 < 30;
	std::snprintf(det, sizeof(det), "alpha aux x=5,15,25,35,45,55 : %d %d %d %d %d %d (plein/vide alternes)", t0, v0,
				  t1, v1, t2, v2);
	Verifier("D1. stroke-dasharray=\"10 10\" : le trait ALTERNE pleins et vides aux bons endroits (sans lui, la "
			 "ligne serait continue d'un bout a l'autre)",
			 alterne, det);

	// (b) stroke-dashoffset DECALE le motif : ce qui etait plein devient vide.
	static const char *kOffset =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"20\" viewBox=\"0 0 100 20\">"
		"<line x1=\"0\" y1=\"10\" x2=\"100\" y2=\"10\" stroke=\"#000000\" stroke-width=\"6\" "
		"stroke-dasharray=\"10 10\" stroke-dashoffset=\"10\"/></svg>";
	NkImage b = Decoder(kOffset);
	const int32 o0 = AlphaDe(b, 5, 10), o1 = AlphaDe(b, 15, 10);
	const bool decale = b.IsValid() && o0 < 30 && o1 > 200;
	std::snprintf(det, sizeof(det), "avec offset=10 : x=5 -> alpha %d (etait %d), x=15 -> alpha %d (etait %d) : le "
								   "motif a bien glisse d'une demi-periode",
				  o0, t0, o1, v0);
	Verifier("D2. stroke-dashoffset decale le motif (ce qui etait plein devient vide)", decale, det);

	// (c) un motif IMPAIR est repete deux fois (norme) : « 10 » veut dire
	//     « 10 pleins, 10 vides », pas « 10 pleins puis plus rien ».
	static const char *kImpair =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"20\" viewBox=\"0 0 100 20\">"
		"<line x1=\"0\" y1=\"10\" x2=\"100\" y2=\"10\" stroke=\"#000000\" stroke-width=\"6\" "
		"stroke-dasharray=\"10\"/></svg>";
	NkImage c = Decoder(kImpair);
	const bool impair = c.IsValid() && AlphaDe(c, 5, 10) > 200 && AlphaDe(c, 15, 10) < 30 &&
						AlphaDe(c, 25, 10) > 200;
	std::snprintf(det, sizeof(det), "« 10 » seul : x=5 %d, x=15 %d, x=25 %d -- le motif impair est repete",
				  AlphaDe(c, 5, 10), AlphaDe(c, 15, 10), AlphaDe(c, 25, 10));
	Verifier("D3. un motif de longueur IMPAIRE est repete deux fois (« 10 » = 10 pleins / 10 vides)", impair, det);

	// (d) un motif de somme NULLE est un trait CONTINU, pas un trait invisible.
	static const char *kZero =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"20\" viewBox=\"0 0 100 20\">"
		"<line x1=\"0\" y1=\"10\" x2=\"100\" y2=\"10\" stroke=\"#000000\" stroke-width=\"6\" "
		"stroke-dasharray=\"0 0\"/></svg>";
	NkImage d = Decoder(kZero);
	const bool continu = d.IsValid() && AlphaDe(d, 5, 10) > 200 && AlphaDe(d, 50, 10) > 200 &&
						 AlphaDe(d, 95, 10) > 200;
	std::snprintf(det, sizeof(det), "somme nulle -> trait plein : %d %d %d (un trait DISPARU serait le defaut)",
				  AlphaDe(d, 5, 10), AlphaDe(d, 50, 10), AlphaDe(d, 95, 10));
	Verifier("D4. un motif de somme NULLE rend un trait CONTINU -- pas un trait qui disparait", continu, det);

	// (e) le motif suit le CONTOUR, pas seulement une droite : sur un rectangle, il
	//     tourne les coins et laisse des trous sur les quatre cotes.
	static const char *kRect =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<rect x=\"10\" y=\"10\" width=\"80\" height=\"80\" fill=\"none\" stroke=\"#000000\" stroke-width=\"6\" "
		"stroke-dasharray=\"8 8\"/></svg>";
	NkImage e = Decoder(kRect);
	int32 pleins = 0, vides = 0;
	for (int32 x = 12; x < 88; ++x) { // le long du bord HAUT
		const int32 al = AlphaDe(e, x, 10);
		if (al > 200)
			++pleins;
		else if (al < 30)
			++vides;
	}
	int32 pleinsG = 0, videsG = 0;
	for (int32 y = 12; y < 88; ++y) { // et le long du bord GAUCHE
		const int32 al = AlphaDe(e, 10, y);
		if (al > 200)
			++pleinsG;
		else if (al < 30)
			++videsG;
	}
	const bool contour = e.IsValid() && pleins > 20 && vides > 20 && pleinsG > 20 && videsG > 20;
	std::snprintf(det, sizeof(det), "bord haut : %d pleins / %d vides ; bord gauche : %d / %d", pleins, vides,
				  pleinsG, videsG);
	Verifier("D5. le motif suit LE CONTOUR (les quatre cotes d'un rectangle sont pointilles, pas seulement le "
			 "premier)",
			 contour, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER <pattern> — une tuile, repetee
// ─────────────────────────────────────────────────────────────────────────────
static void TestPattern() {
	std::printf("\n== PALIER <pattern> ==\n");
	char det[640];

	// (a) un damier : une tuile de 20x20 contenant un carre rouge de 10x10 en haut
	//     a gauche. Le motif doit se REPETER -- on verifie la meme chose a trois
	//     periodes de distance.
	static const char *kDamier =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><pattern id=\"p\" patternUnits=\"userSpaceOnUse\" x=\"0\" y=\"0\" width=\"20\" height=\"20\">"
		"<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#ff0000\"/></pattern></defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"url(#p)\"/></svg>";
	NkImage a = Decoder(kDamier);
	// dans chaque tuile : (5,5) rouge, (15,5) vide, (5,15) vide
	const bool t0 = Proche(a, 5, 5, 255, 0, 0, 6) && AlphaDe(a, 15, 5) < 40 && AlphaDe(a, 5, 15) < 40;
	const bool t1 = Proche(a, 25, 25, 255, 0, 0, 6) && AlphaDe(a, 35, 25) < 40;
	const bool t2 = Proche(a, 65, 45, 255, 0, 0, 6) && AlphaDe(a, 75, 45) < 40;
	std::snprintf(det, sizeof(det), "tuile 1 (5,5)=%d ; tuile 2 (25,25)=%d ; tuile 4 (65,45)=%d -- le motif se "
								   "repete a l'identique",
				  t0 ? 1 : 0, t1 ? 1 : 0, t2 ? 1 : 0);
	Verifier("P1. <pattern> : la tuile se REPETE sur toute la forme (meme dessin a trois periodes de distance)",
			 a.IsValid() && t0 && t1 && t2, det);

	// (b) le motif est ancre par x/y : le decaler decale TOUT le carrelage.
	static const char *kDecale =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><pattern id=\"p\" patternUnits=\"userSpaceOnUse\" x=\"10\" y=\"0\" width=\"20\" height=\"20\">"
		"<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#ff0000\"/></pattern></defs>"
		"<rect x=\"0\" y=\"0\" width=\"100\" height=\"100\" fill=\"url(#p)\"/></svg>";
	NkImage b = Decoder(kDecale);
	const bool decale = b.IsValid() && AlphaDe(b, 5, 5) < 40 && Proche(b, 15, 5, 255, 0, 0, 6);
	std::snprintf(det, sizeof(det), "avec x=10 : (5,5) alpha=%d (etait rouge), (15,5) rouge=%d", AlphaDe(b, 5, 5),
				  Proche(b, 15, 5, 255, 0, 0, 6) ? 1 : 0);
	Verifier("P2. x / y du <pattern> ancrent le carrelage (le decaler decale tout)", decale, det);

	// (c) le motif ne DEBORDE PAS de la forme qu'il remplit.
	static const char *kBorne =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
		"<defs><pattern id=\"p\" patternUnits=\"userSpaceOnUse\" width=\"20\" height=\"20\">"
		"<rect x=\"0\" y=\"0\" width=\"20\" height=\"20\" fill=\"#0000ff\"/></pattern></defs>"
		"<rect x=\"20\" y=\"20\" width=\"40\" height=\"40\" fill=\"url(#p)\"/></svg>";
	NkImage c = Decoder(kBorne);
	const bool borne = c.IsValid() && Proche(c, 40, 40, 0, 0, 255, 6) && AlphaDe(c, 10, 10) < 40 &&
					   AlphaDe(c, 80, 80) < 40;
	std::snprintf(det, sizeof(det), "dans la forme (40,40) bleu=%d ; dehors : alpha %d / %d",
				  Proche(c, 40, 40, 0, 0, 255, 6) ? 1 : 0, AlphaDe(c, 10, 10), AlphaDe(c, 80, 80));
	Verifier("P3. le motif ne remplit QUE la forme qui le reference (il ne deborde pas sur la page)", borne, det);

	// (d) un motif INTROUVABLE ne peint rien -- surtout pas un aplat noir. Une
	//     reference morte n'est pas une couleur.
	static const char *kAbsent =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
		"<rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"url(#jamais)\"/></svg>";
	NkImage d = Decoder(kAbsent);
	const bool rien = d.IsValid() && AlphaDe(d, 30, 30) < 20;
	std::snprintf(det, sizeof(det), "alpha au centre=%d (un aplat NOIR serait le defaut classique)",
				  AlphaDe(d, 30, 30));
	Verifier("P4. une reference de motif MORTE ne peint rien -- pas un aplat noir", rien, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER GRAPHE DE FILTRES — des primitives qui se CHAINENT
// ─────────────────────────────────────────────────────────────────────────────
static void TestGrapheFiltres() {
	std::printf("\n== PALIER graphe de filtres ==\n");
	char det[640];
	static char svg[3072];

	// (a) feFlood + feComposite operator="in" : un aplat DECOUPE par la forme.
	//     C'est le test du CHAINAGE : ni l'un ni l'autre ne donne ce resultat seul.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
				  "<defs><filter id=\"f\">"
				  "<feFlood flood-color=\"#00ff00\" result=\"aplat\"/>"
				  "<feComposite in=\"aplat\" in2=\"SourceGraphic\" operator=\"in\"/>"
				  "</filter></defs>"
				  "<g filter=\"url(#f)\"><rect x=\"25\" y=\"25\" width=\"50\" height=\"50\" fill=\"#ff0000\"/></g>"
				  "</svg>");
	NkImage a = Decoder(svg);
	const bool decoupe = a.IsValid() && Proche(a, 50, 50, 0, 255, 0, 8) && AlphaDe(a, 10, 10) < 30 &&
						 AlphaDe(a, 90, 90) < 30;
	std::snprintf(det, sizeof(det), "dans la forme : vert (l'aplat)=%d ; hors de la forme : alpha %d / %d (l'aplat "
								   "couvre TOUT le plan, seul le `in` le limite)",
				  Proche(a, 50, 50, 0, 255, 0, 8) ? 1 : 0, AlphaDe(a, 10, 10), AlphaDe(a, 90, 90));
	Verifier("G1. feFlood + feComposite operator=\"in\" : un aplat DECOUPE par la forme -- c'est le chainage qui "
			 "produit ca, aucune des deux primitives seule ne le peut",
			 decoupe, det);

	// (b) feGaussianBlur seul : la forme est floutee (bords adoucis DEDANS,
	//     debordement DEHORS).
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" viewBox=\"0 0 100 100\">"
				  "<defs><filter id=\"f\"><feGaussianBlur stdDeviation=\"4\"/></filter></defs>"
				  "<g filter=\"url(#f)\"><rect x=\"30\" y=\"30\" width=\"40\" height=\"40\" fill=\"#000000\"/></g>"
				  "</svg>");
	NkImage b = Decoder(svg);
	const int32 dedans = AlphaDe(b, 50, 50), bord = AlphaDe(b, 30, 50), dehors = AlphaDe(b, 24, 50);
	const bool floute = b.IsValid() && dedans > 230 && bord > 60 && bord < 200 && dehors > 5 && dehors < 90;
	std::snprintf(det, sizeof(det), "alpha : coeur=%d, sur le bord=%d (mi-chemin), au-dela=%d (deborde)", dedans,
				  bord, dehors);
	Verifier("G2. feGaussianBlur : le coeur reste opaque, le bord passe a mi-chemin, et l'alpha DEBORDE de la "
			 "forme (c'est ce qui distingue un flou d'une transparence)",
			 floute, det);

	// (c) feOffset + feMerge : l'ombre portee, ECRITE A LA MAIN avec les briques du
	//     graphe. Le meme resultat que feDropShadow, par un autre chemin -- c'est la
	//     preuve que le graphe fonctionne et pas seulement le raccourci.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
				  "<defs><filter id=\"f\">"
				  "<feOffset in=\"SourceAlpha\" dx=\"12\" dy=\"12\" result=\"dec\"/>"
				  "<feMerge><feMergeNode in=\"dec\"/><feMergeNode in=\"SourceGraphic\"/></feMerge>"
				  "</filter></defs>"
				  "<g filter=\"url(#f)\"><rect x=\"20\" y=\"20\" width=\"50\" height=\"50\" fill=\"#ff0000\"/></g>"
				  "</svg>");
	NkImage c = Decoder(svg);
	const bool aLaMain = c.IsValid() && Proche(c, 45, 45, 255, 0, 0, 6) && Proche(c, 75, 75, 0, 0, 0, 10) &&
						 AlphaDe(c, 10, 10) < 30;
	std::snprintf(det, sizeof(det), "la forme rouge au centre=%d ; l'ombre NOIRE decalee en (75,75)=%d ; rien "
								   "avant la forme : alpha=%d",
				  Proche(c, 45, 45, 255, 0, 0, 6) ? 1 : 0, Proche(c, 75, 75, 0, 0, 0, 10) ? 1 : 0,
				  AlphaDe(c, 10, 10));
	Verifier("G3. feOffset(SourceAlpha) + feMerge : une ombre portee ECRITE A LA MAIN avec les briques du graphe "
			 "-- le meme resultat que feDropShadow par un autre chemin",
			 aLaMain, det);

	// (d) feColorMatrix type=\"saturate\" values=\"0\" : la forme passe en gris. Une
	//     primitive qui touche LA COULEUR, pas la geometrie.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
				  "<defs><filter id=\"f\"><feColorMatrix type=\"saturate\" values=\"0\"/></filter></defs>"
				  "<g filter=\"url(#f)\"><rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ff0000\"/></g>"
				  "</svg>");
	NkImage d = Decoder(svg);
	uint8 pd[4];
	Pixel(d, 30, 30, pd);
	const bool gris = d.IsValid() && std::abs((int32)pd[0] - (int32)pd[1]) < 6 &&
					  std::abs((int32)pd[1] - (int32)pd[2]) < 6 && pd[0] > 30 && pd[0] < 100;
	std::snprintf(det, sizeof(det), "le rouge pur devient (%u,%u,%u) : les trois canaux egaux, a la LUMINANCE du "
								   "rouge (~54)",
				  pd[0], pd[1], pd[2]);
	Verifier("G4. feColorMatrix type=\"saturate\" values=\"0\" : la couleur est desaturee (les trois canaux "
			 "convergent vers la luminance)",
			 gris, det);

	// (e) feBlend mode=\"multiply\" entre deux entrees du graphe.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"60\" height=\"60\" viewBox=\"0 0 60 60\">"
				  "<defs><filter id=\"f\">"
				  "<feFlood flood-color=\"#808080\" result=\"gris\"/>"
				  "<feBlend in=\"gris\" in2=\"SourceGraphic\" mode=\"multiply\"/>"
				  "</filter></defs>"
				  "<g filter=\"url(#f)\"><rect x=\"0\" y=\"0\" width=\"60\" height=\"60\" fill=\"#ff0000\"/></g>"
				  "</svg>");
	NkImage e = Decoder(svg);
	uint8 pe[4];
	Pixel(e, 30, 30, pe);
	const bool multiplie = e.IsValid() && pe[0] > 110 && pe[0] < 145 && pe[1] < 20 && pe[2] < 20;
	std::snprintf(det, sizeof(det), "(%u,%u,%u) -- gris 50 %% multiplie par du rouge donne ~(128,0,0)", pe[0], pe[1],
				  pe[2]);
	Verifier("G5. feBlend mode=\"multiply\" entre deux entrees NOMMEES du graphe", multiplie, det);

	// (f) stdDeviation=\"x y\" : un flou ANISOTROPE. Le meme sigma dans les deux
	//     sens rendrait un flou rond la ou le fichier demande un ovale.
	std::snprintf(svg, sizeof(svg),
				  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"120\" height=\"120\" viewBox=\"0 0 120 120\">"
				  "<defs><filter id=\"f\"><feGaussianBlur stdDeviation=\"8 0\"/></filter></defs>"
				  "<g filter=\"url(#f)\"><rect x=\"40\" y=\"40\" width=\"40\" height=\"40\" fill=\"#000000\"/></g>"
				  "</svg>");
	NkImage f = Decoder(svg);
	const int32 horiz = AlphaDe(f, 30, 60); // 10 px a GAUCHE du bord : floute
	const int32 vert = AlphaDe(f, 60, 30);  // 10 px AU-DESSUS : net, donc vide
	const bool aniso = f.IsValid() && horiz > 20 && vert < 20;
	std::snprintf(det, sizeof(det), "a 10 px du bord : horizontalement alpha=%d (le flou deborde), verticalement "
								   "alpha=%d (aucun flou dans ce sens)",
				  horiz, vert);
	Verifier("G6. stdDeviation=\"8 0\" : le flou est ANISOTROPE -- il deborde horizontalement et pas "
			 "verticalement",
			 aniso, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER TEXTE AVANCE — dominant-baseline, textLength, <textPath>
// ─────────────────────────────────────────────────────────────────────────────
static void TestTexteAvance() {
	std::printf("\n== PALIER texte avance ==\n");
	char det[640];
	static char svg[3072];

	// (a) dominant-baseline : « hanging » met `y` AU-DESSUS du texte, « middle » au
	//     milieu. Sans lui, tout le monde est sur la ligne de base -- et un titre
	//     centre verticalement se retrouve trop bas.
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"40\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\">H</text>");
	Encre base = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg), "<text x=\"20\" y=\"40\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
								"dominant-baseline=\"hanging\">H</text>");
	Encre susp = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg), "<text x=\"20\" y=\"40\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
								"dominant-baseline=\"middle\">H</text>");
	Encre mil = EncreDe(Decoder(svg), 0, 0, 200, 80);
	// alphabetic : l'encre est AU-DESSUS de y=40. hanging : elle passe EN DESSOUS.
	// middle : entre les deux.
	const bool baseline = base.sombres > 20 && susp.sombres > 20 && mil.sombres > 20 && base.ymax <= 41 &&
						  susp.ymin >= 39 && mil.ymin > base.ymin && mil.ymax < susp.ymax;
	std::snprintf(det, sizeof(det), "alphabetic y[%d..%d] (au-dessus de 40) ; hanging y[%d..%d] (en dessous) ; "
								   "middle y[%d..%d] (entre les deux)",
				  base.ymin, base.ymax, susp.ymin, susp.ymax, mil.ymin, mil.ymax);
	Verifier("X1. dominant-baseline : « hanging » descend le texte sous `y`, « middle » le centre dessus -- `y` "
			 "n'est plus toujours la ligne de base",
			 baseline, det);

	// (b) textLength : le texte est ETIRE pour tenir la mesure demandee. Par
	//     defaut (« spacing »), c'est l'ESPACEMENT qui change, pas les glyphes.
	PageTexte(svg, sizeof(svg),
			  "<text x=\"10\" y=\"50\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\">HHH</text>");
	Encre libre = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg), "<text x=\"10\" y=\"50\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\" "
								"textLength=\"150\">HHH</text>");
	Encre tendu = EncreDe(Decoder(svg), 0, 0, 200, 80);
	const int32 lLibre = libre.xmax - libre.xmin + 1, lTendu = tendu.xmax - tendu.xmin + 1;
	// l'encre TOTALE ne change pas (memes glyphes), mais la boite s'elargit
	const bool etire = libre.sombres > 20 && lTendu > lLibre + 40 && tendu.xmax >= 150 &&
					   std::abs(tendu.sombres - libre.sombres) < libre.sombres / 3;
	std::snprintf(det, sizeof(det), "libre : %d px de large, %d d'encre ; textLength=150 : %d px de large, %d "
								   "d'encre (memes glyphes, plus d'espace)",
				  lLibre, libre.sombres, lTendu, tendu.sombres);
	Verifier("X2. textLength (mode « spacing ») : le texte occupe la largeur demandee en ecartant les glyphes -- "
			 "l'encre totale ne change pas",
			 etire, det);

	// (c) lengthAdjust=\"spacingAndGlyphs\" : les GLYPHES sont mis a l'echelle, donc
	//     l'encre AUGMENTE. C'est la difference avec le mode precedent.
	PageTexte(svg, sizeof(svg), "<text x=\"10\" y=\"50\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\" "
								"textLength=\"150\" lengthAdjust=\"spacingAndGlyphs\">HHH</text>");
	Encre gros = EncreDe(Decoder(svg), 0, 0, 200, 80);
	const bool glyphes = gros.sombres > libre.sombres + libre.sombres / 2;
	std::snprintf(det, sizeof(det), "spacing : %d d'encre ; spacingAndGlyphs : %d d'encre (les glyphes sont "
								   "AGRANDIS, pas seulement ecartes)",
				  tendu.sombres, gros.sombres);
	Verifier("X3. lengthAdjust=\"spacingAndGlyphs\" met les GLYPHES a l'echelle (l'encre augmente), la ou "
			 "« spacing » ne fait que les ecarter",
			 glyphes, det);

	// (d) <textPath> : le texte suit un chemin. Sur un arc, l'encre ne peut pas
	//     tenir sur une seule ligne -- c'est ce qui le distingue d'un texte droit.
	static const char *kPath =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"120\" viewBox=\"0 0 200 120\">"
		"<rect x=\"0\" y=\"0\" width=\"200\" height=\"120\" fill=\"#ffffff\"/>"
		"<defs><path id=\"arc\" d=\"M20 100 A 80 80 0 0 1 180 100\"/></defs>"
		"<text font-family=\"Inter\" font-size=\"20\" fill=\"#000000\">"
		"<textPath href=\"#arc\">HHHHHH</textPath></text></svg>";
	NkImage d = Decoder(kPath);
	Encre courbe = d.IsValid() ? EncreDe(d, 0, 0, 200, 120) : Encre();
	// LE MEME TEXTE EN LIGNE DROITE, pour comparer : c'est la seule reference qui
	// ne depend pas de la forme du chemin choisi.
	static const char *kDroit =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"120\" viewBox=\"0 0 200 120\">"
		"<rect x=\"0\" y=\"0\" width=\"200\" height=\"120\" fill=\"#ffffff\"/>"
		"<text x=\"20\" y=\"100\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\">HHHHHH</text></svg>";
	Encre droit = EncreDe(Decoder(kDroit), 0, 0, 200, 120);
	const int32 hCourbe = courbe.ymax - courbe.ymin + 1, hDroit = droit.ymax - droit.ymin + 1;
	// sur un demi-cercle, le DEBUT de l'arc est presque vertical : les lettres
	// MONTENT. La hauteur d'encre explose (le vrai marqueur d'un texte tourne),
	// alors qu'un texte droit tient dans la hauteur d'une capitale.
	const bool surChemin = courbe.sombres > 60 && hCourbe > 3 * hDroit && courbe.ymin < droit.ymin - 30;
	std::snprintf(det, sizeof(det), "courbe : %d px d'encre, y[%d..%d] soit %d de haut | droit : %d px d'encre, "
								   "%d de haut -- les lettres MONTENT le long de l'arc",
				  courbe.sombres, courbe.ymin, courbe.ymax, hCourbe, droit.sombres, hDroit);
	Verifier("X4. <textPath> : les glyphes suivent le chemin et TOURNENT avec lui -- sur un demi-cercle l'encre "
			 "occupe plus de trois fois la hauteur du meme texte pose droit",
			 surChemin, det);

	// (e) un <textPath> dont la cible est introuvable : le texte est pose en ligne
	//     droite ET on le dit -- plutot que de ne rien peindre.
	static const char *kAbsent =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"80\" viewBox=\"0 0 200 80\">"
		"<rect x=\"0\" y=\"0\" width=\"200\" height=\"80\" fill=\"#ffffff\"/>"
		"<text x=\"20\" y=\"50\" font-family=\"Inter\" font-size=\"20\" fill=\"#000000\">"
		"<textPath href=\"#nexistepas\">HH</textPath></text></svg>";
	NkSVGImage *img = NkSVGImage::LoadFromMemory((const uint8 *)kAbsent, std::strlen(kAbsent));
	bool cibleDite = false;
	int32 encreDroite = 0;
	if (img) {
		for (int32 i = 0; i < img->SkippedCount(); ++i) {
			const char *n = img->SkippedAt(i);
			if (n && std::strcmp(n, "textPath-cible") == 0)
				cibleDite = true;
		}
		NkImage r = img->Rasterize(0, 0);
		encreDroite = EncreDe(r, 0, 0, 200, 80).sombres;
		img->Free();
	}
	std::snprintf(det, sizeof(det), "cible nommee=%d ; le texte est pose quand meme : %d px d'encre",
				  cibleDite ? 1 : 0, encreDroite);
	Verifier("X5. <textPath> vers un chemin INTROUVABLE : le texte est pose en ligne droite ET c'est dit (ne rien "
			 "peindre serait pire)",
			 cibleDite && encreDroite > 20, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER preserveAspectRatio — les NEUF alignements, pas seulement le centre
// ─────────────────────────────────────────────────────────────────────────────
static void TestAlignements() {
	std::printf("\n== PALIER preserveAspectRatio (alignements) ==\n");
	char det[640];
	static char svg[8192];

	// une image 4x2 (rouge) dans une boite CARREE de 40x40 : en « meet », elle
	// occupe 40x20 et il reste 20 px de bandes. OU sont-elles ? C'est tout le
	// palier -- et ne gerer que « Mid » recentrait ce que le fichier voulait caler.
	NkImage large = NkImage::Create(4u, 2u, 4, 0xFF0000FFu);
	uint8 *lo = nullptr;
	usize lt = 0;
	NkString b64;
	if (large.IsValid() && large.SaveToMemory(lo, lt) && lo && lt)
		b64 = nkentseu::encoding::base64::NkEncode(lo, lt);

	auto poser = [&](const char *par) -> NkImage {
		std::snprintf(svg, sizeof(svg),
					  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
					  "<image x=\"0\" y=\"0\" width=\"40\" height=\"40\" preserveAspectRatio=\"%s\" "
					  "href=\"data:image/png;base64,%s\"/></svg>",
					  par, b64.Length() ? b64.Data() : "");
		return Decoder(svg);
	};

	NkImage min = poser("xMidYMin meet");   // collee EN HAUT
	NkImage mid = poser("xMidYMid meet");   // centree
	NkImage max = poser("xMidYMax meet");   // collee EN BAS
	const bool haut = min.IsValid() && Proche(min, 20, 5, 255, 0, 0, 6) && AlphaDe(min, 20, 35) < 30;
	const bool centre = mid.IsValid() && AlphaDe(mid, 20, 5) < 30 && Proche(mid, 20, 20, 255, 0, 0, 6) &&
						AlphaDe(mid, 20, 35) < 30;
	const bool bas = max.IsValid() && AlphaDe(max, 20, 5) < 30 && Proche(max, 20, 35, 255, 0, 0, 6);
	std::snprintf(det, sizeof(det), "YMin : haut plein=%d, bas vide (alpha %d) | YMid : centre=%d | YMax : haut "
								   "vide (alpha %d), bas plein=%d",
				  haut ? 1 : 0, AlphaDe(min, 20, 35), centre ? 1 : 0, AlphaDe(max, 20, 5), bas ? 1 : 0);
	Verifier("A1. preserveAspectRatio YMin / YMid / YMax : l'image est calee EN HAUT, CENTREE ou EN BAS -- trois "
			 "resultats distincts la ou seul « centre » existait",
			 haut && centre && bas, det);

	// et sur l'autre axe, avec une image HAUTE cette fois
	NkImage haute = NkImage::Create(2u, 4u, 4, 0x0000FFFFu);
	uint8 *ho = nullptr;
	usize ht = 0;
	NkString b64h;
	if (haute.IsValid() && haute.SaveToMemory(ho, ht) && ho && ht)
		b64h = nkentseu::encoding::base64::NkEncode(ho, ht);
	auto poserH = [&](const char *par) -> NkImage {
		std::snprintf(svg, sizeof(svg),
					  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
					  "<image x=\"0\" y=\"0\" width=\"40\" height=\"40\" preserveAspectRatio=\"%s\" "
					  "href=\"data:image/png;base64,%s\"/></svg>",
					  par, b64h.Length() ? b64h.Data() : "");
		return Decoder(svg);
	};
	NkImage gauche = poserH("xMinYMid meet");
	NkImage droite = poserH("xMaxYMid meet");
	const bool aGauche = gauche.IsValid() && Proche(gauche, 5, 20, 0, 0, 255, 6) && AlphaDe(gauche, 35, 20) < 30;
	const bool aDroite = droite.IsValid() && AlphaDe(droite, 5, 20) < 30 && Proche(droite, 35, 20, 0, 0, 255, 6);
	std::snprintf(det, sizeof(det), "xMin : gauche plein=%d, droite vide (alpha %d) | xMax : gauche vide (alpha "
								   "%d), droite plein=%d",
				  aGauche ? 1 : 0, AlphaDe(gauche, 35, 20), AlphaDe(droite, 5, 20), aDroite ? 1 : 0);
	Verifier("A2. xMin / xMax : l'alignement HORIZONTAL est honore lui aussi", aGauche && aDroite, det);

	if (lo)
		nkentseu::memory::NkFree(lo);
	if (ho)
		nkentseu::memory::NkFree(ho);
}

// ─────────────────────────────────────────────────────────────────────────────
// PALIER font-weight — une VRAIE coupe grasse quand on en declare une
// ─────────────────────────────────────────────────────────────────────────────
static void TestGraisse() {
	std::printf("\n== PALIER font-weight ==\n");
	char det[640];
	static char svg[3072];

	// MESURE DU 2026-09-05 : les dix polices embarquees dans NKFont sont TOUTES des
	// « Regular ». Sans coupe grasse declaree, le codec SIMULE (un trait autour du
	// glyphe) et le DIT. Avec une vraie coupe, il la prend.
	PageTexte(svg, sizeof(svg),
			  "<text x=\"20\" y=\"50\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\">HH</text>");
	Encre normal = EncreDe(Decoder(svg), 0, 0, 200, 80);
	PageTexte(svg, sizeof(svg), "<text x=\"20\" y=\"50\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
								"font-weight=\"700\">HH</text>");
	Encre simule = EncreDe(Decoder(svg), 0, 0, 200, 80);
	const bool plusEncre = normal.sombres > 20 && simule.sombres > normal.sombres;
	std::snprintf(det, sizeof(det), "regulier : %d px d'encre ; font-weight=700 SIMULE : %d px (plus d'encre, "
								   "meme forme)",
				  normal.sombres, simule.sombres);
	Verifier("W1. font-weight=\"700\" SANS coupe grasse declaree : la graisse est simulee par un trait (plus "
			 "d'encre) et c'est dit",
			 plusEncre, det);

	// (b) une VRAIE coupe grasse declaree par l'application : le codec la prend, et
	//     NE simule plus. Les formes d'un Bold dessine ne sont pas celles d'un
	//     contour epaissi -- les epaissir en plus le rendrait pateux.
	NkFontGlyphSource *avecBold = nkentseu::memory::NkGetDefaultAllocator().New<NkFontGlyphSource>();
	bool declaree = false, simulationAnnoncee = true;
	int32 encreVraie = 0;
	if (avecBold) {
		declaree = avecBold->AjouterFonteFichier("Inter", 700, "Resources/Fonts/newfonts/Inter_18pt-Bold.ttf");
		if (declaree) {
			NkIGlyphSource *garde = NkSVGCodec::GetDefaultGlyphSource();
			NkSVGCodec::SetDefaultGlyphSource(avecBold);
			static const char *kBold =
				"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"200\" height=\"80\" viewBox=\"0 0 200 80\">"
				"<rect x=\"0\" y=\"0\" width=\"200\" height=\"80\" fill=\"#ffffff\"/>"
				"<text x=\"20\" y=\"50\" font-family=\"Inter\" font-size=\"30\" fill=\"#000000\" "
				"font-weight=\"700\">HH</text></svg>";
			NkSVGImage *doc = NkSVGImage::LoadFromMemory((const uint8 *)kBold, std::strlen(kBold));
			if (doc) {
				// LE CRITERE FRANC : avec une vraie coupe, le codec ne doit PAS
				// annoncer de simulation. Comparer les quantites d'encre ne
				// distinguait pas un vrai Bold d'un vrai Bold ENCORE epaissi -- deux
				// choses tres differentes qui rendent toutes deux « plus d'encre ».
				simulationAnnoncee = false;
				for (int32 i = 0; i < doc->SkippedCount(); ++i) {
					const char *n = doc->SkippedAt(i);
					if (n && std::strcmp(n, "font-weight") == 0)
						simulationAnnoncee = true;
				}
				NkImage r = doc->Rasterize(0, 0);
				encreVraie = EncreDe(r, 0, 0, 200, 80).sombres;
				doc->Free();
			}
			NkSVGCodec::SetDefaultGlyphSource(garde);
		}
		nkentseu::memory::NkGetDefaultAllocator().Delete(avecBold);
	}
	const bool vraiBold = declaree && !simulationAnnoncee && encreVraie > normal.sombres;
	std::snprintf(det, sizeof(det), "coupe grasse declaree=%d ; simulation annoncee=%d (elle ne doit PAS l'etre) ; "
								   "encre : regulier %d, simule %d, VRAI Bold %d",
				  declaree ? 1 : 0, simulationAnnoncee ? 1 : 0, normal.sombres, simule.sombres, encreVraie);
	Verifier("W2. une VRAIE coupe grasse declaree (AjouterFonteFichier) est PRISE, et le codec n'ajoute alors "
			 "AUCUN trait de simulation -- epaissir un Bold dessine le rendrait pateux",
			 vraiBold, det);
}

// ─────────────────────────────────────────────────────────────────────────────
// CE QUE LE CODEC SAUTE : il doit le DIRE, une fois par nom
// ─────────────────────────────────────────────────────────────────────────────
static void TestNonGere() {
	std::printf("\n== CE QUI EST SAUTE SE DIT ==\n");
	static const char *kInconnu =
		"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"40\" viewBox=\"0 0 40 40\">"
		"<defs><symbol id=\"s\"><rect width=\"5\" height=\"5\"/></symbol>"
		"<pattern id=\"p\"><rect width=\"2\" height=\"2\"/></pattern>"
		"<marker id=\"k\"><rect width=\"2\" height=\"2\"/></marker>"
		"<marker id=\"k2\"><rect width=\"2\" height=\"2\"/></marker></defs>"
		"<use href=\"#s\"/><use href=\"#s\"/>"
		"<clipPath id=\"c\"><rect width=\"5\" height=\"5\"/></clipPath>"
		"<rect x=\"0\" y=\"0\" width=\"40\" height=\"40\" fill=\"#808080\" stroke=\"#000\" "
		"stroke-dasharray=\"4 2\"/>"
		"</svg>";
	NkSVGImage *img = NkSVGImage::LoadFromMemory((const uint8 *)kInconnu, std::strlen(kInconnu));
	char det[512] = {0};
	// LE REGISTRE SUIT LES PALIERS : ce qui vient d'etre implemente le QUITTE, ce
	// qui reste a faire y demeure. Verifier les deux sens, c'est empecher deux
	// mensonges opposes -- annoncer comme saute ce qu'on peint, et taire ce qu'on
	// saute vraiment.
	bool markerDit = false;
	bool useEncoreDit = false, clipEncoreDit = false, maskEncoreDit = false, dashEncoreDit = false,
		 patternEncoreDit = false;
	int32 nbMarker = 0, nb = 0;
	if (img) {
		nb = img->SkippedCount();
		for (int32 i = 0; i < nb; ++i) {
			const char *n = img->SkippedAt(i);
			if (!n)
				continue;
			// GERES depuis leur palier : ils ne doivent PLUS etre annonces comme sautes
			if (std::strcmp(n, "use") == 0)
				useEncoreDit = true;
			if (std::strcmp(n, "clipPath") == 0)
				clipEncoreDit = true;
			if (std::strcmp(n, "mask") == 0)
				maskEncoreDit = true;
			// pas encore faits : ils doivent l'etre, une fois chacun
			if (std::strcmp(n, "marker") == 0) {
				markerDit = true;
				++nbMarker;
			}
			if (std::strcmp(n, "pattern") == 0)
				patternEncoreDit = true; // GERE depuis son palier
			if (std::strcmp(n, "stroke-dasharray") == 0)
				dashEncoreDit = true; // GERE depuis son palier
		}
		std::snprintf(det, sizeof(det), "%d nom(s) saute(s) : ", nb);
		for (int32 i = 0; i < nb; ++i) {
			std::strncat(det, img->SkippedAt(i) ? img->SkippedAt(i) : "?", sizeof(det) - std::strlen(det) - 1);
			std::strncat(det, " ", sizeof(det) - std::strlen(det) - 1);
		}
		std::strncat(det, "| <use>, <clipPath>, <mask>, <pattern>, stroke-dasharray n'y sont plus ",
					 sizeof(det) - std::strlen(det) - 1);
		img->Free();
	}
	Verifier("Le REGISTRE DES SAUTS suit les paliers, dans LES DEUX SENS : ce qui reste a faire est nomme "
			 "(marker) une seule fois par nom -- DEUX <marker> ne donnent qu'une mention --, et <use>, "
			 "<clipPath>, <mask>, <pattern>, stroke-dasharray N'Y SONT PLUS depuis qu'ils sont peints",
			 img != nullptr && markerDit && !useEncoreDit && !clipEncoreDit && !maskEncoreDit && !dashEncoreDit &&
				 !patternEncoreDit && nbMarker == 1,
			 det);
}

// ─────────────────────────────────────────────────────────────────────────────
int TestSVG_Run() {
	gPass = 0;
	gTotal = 0;
	std::printf("\n===== BANC DU CODEC SVG (NkSVGCodec) =====\n");
	// L'INJECTION, telle qu'une application la ferait : une ligne, au demarrage.
	// NKImage ne connait aucune police ; NKFont en fournit une source ; c'est
	// l'appelant qui les met en presence.
	static NkFontGlyphSource glyphes;
	NkSVGCodec::SetDefaultGlyphSource(&glyphes);
	TestRxRy();
	TestImage();
	TestTexte();
	TestDegrades();
	TestOmbre();
	TestOpacites();
	TestUse();
	TestClip();
	TestMask();
	TestCSS();
	TestDash();
	TestPattern();
	TestGrapheFiltres();
	TestTexteAvance();
	TestAlignements();
	TestGraisse();
	TestNonGere();
	TestTemoinCroise();
	std::printf("\n===== SVG : %d / %d =====\n", gPass, gTotal);
	return (gPass == gTotal) ? 0 : 1;
}
