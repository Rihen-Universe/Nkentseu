// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_palette.cpp — banc de NK_AREA et de NkImagePalette (images synthetiques).
//
// PRE-ENREGISTREMENT :
//   (a1) 4x2 : moitie gauche rouge opaque, moitie droite blanc TRANSPARENT ; reduite
//        en 1x1 par NK_AREA : le rouge reste rouge (g = 0, pas de halo blanc) et
//        l'alpha vaut ~128
//   (a2) une image unie reduite par NK_AREA garde sa couleur
//   (p1) EstimateBg : fond blanc, un bloc sombre touche un coin : la mediane rend blanc
//   (p2) Quantize k = 3 sur trois plages de couleurs : 3 centres, chaque pixel de
//        sortie est l'un d'eux, et deux appels de meme graine rendent la meme palette
//   (p3) RemoveBackground : un anneau noir sur fond blanc ; l'exterieur devient
//        transparent, l'interieur de l'anneau (blanc aussi) reste OPAQUE
//   (p4) Spans : deux blocs separes par 6 lignes vides : 2 bandes aux positions
//        exactes ; avec ecartMin = 7 elles fusionnent (le 3e bloc, a 7 lignes, non) ;
//        tailleMin = 5 ecarte une bande de 2 lignes
// Contre-epreuve : (a1) sans ponderation par l'alpha -> g != 0, rouge ; (p3) en
// remplissant sans exiger la liaison au bord -> l'interieur est efface, rouge ;
// (p4) en inversant `>=` en `>` dans le test d'ecart -> la fusion change, rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKImage/Core/NkImagePalette.h"

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	NkImage Fond(int32 w, int32 h, uint8 r, uint8 g, uint8 b, uint8 a = 255) {
		NkImage im = NkImage::Create(static_cast<uint32>(w), static_cast<uint32>(h), NkImagePixelFormat::NK_RGBA32, 0u);
		for (int32 y = 0; y < h; ++y) {
			uint8 *p = im.RowPtr(y);
			for (int32 x = 0; x < w; ++x) {
				p[x * 4 + 0] = r;
				p[x * 4 + 1] = g;
				p[x * 4 + 2] = b;
				p[x * 4 + 3] = a;
			}
		}
		return im;
	}

	void Rect(NkImage &im, int32 x0, int32 y0, int32 x1, int32 y1, uint8 r, uint8 g, uint8 b) {
		for (int32 y = y0; y < y1; ++y) {
			uint8 *p = im.RowPtr(y);
			for (int32 x = x0; x < x1; ++x) {
				p[x * 4 + 0] = r;
				p[x * 4 + 1] = g;
				p[x * 4 + 2] = b;
				p[x * 4 + 3] = 255;
			}
		}
	}

} // namespace

TEST_CASE(NKImageArea, A1_PondereParAlpha) {
	NkImage im = Fond(4, 2, 255, 255, 255, 0);
	Rect(im, 0, 0, 2, 2, 255, 0, 0);
	NkImage p = im.Resize(1, 1, NkResizeFilter::NK_AREA);
	ASSERT_TRUE(p.IsValid());
	const uint8 *q = p.RowPtr(0);
	ASSERT_EQUAL(255, static_cast<int>(q[0]));
	ASSERT_EQUAL(0, static_cast<int>(q[1]));
	ASSERT_EQUAL(0, static_cast<int>(q[2]));
	ASSERT_TRUE(q[3] >= 127 && q[3] <= 129);
}

TEST_CASE(NKImageArea, A2_UniResteUni) {
	NkImage im = Fond(8, 8, 10, 120, 200);
	NkImage p = im.Resize(2, 2, NkResizeFilter::NK_AREA);
	ASSERT_TRUE(p.IsValid());
	const uint8 *q = p.RowPtr(1);
	ASSERT_EQUAL(10, static_cast<int>(q[4]));
	ASSERT_EQUAL(120, static_cast<int>(q[5]));
	ASSERT_EQUAL(200, static_cast<int>(q[6]));
	ASSERT_EQUAL(255, static_cast<int>(q[7]));
}

TEST_CASE(NKImagePalette, P1_FondParMediane) {
	NkImage im = Fond(20, 20, 250, 250, 250);
	Rect(im, 0, 0, 4, 4, 10, 10, 10); // un bloc sombre sur un coin
	const NkColor c = NkImagePalette::EstimateBg(im);
	ASSERT_EQUAL(250, static_cast<int>(c.r));
	ASSERT_EQUAL(250, static_cast<int>(c.g));
}

TEST_CASE(NKImagePalette, P2_QuantifieEnTroisCouleurs) {
	NkImage im = Fond(30, 10, 0, 0, 0);
	Rect(im, 0, 0, 10, 10, 250, 10, 10);
	Rect(im, 10, 0, 20, 10, 10, 250, 10);
	Rect(im, 20, 0, 30, 10, 10, 10, 250);
	// un peu de bruit
	for (int32 x = 0; x < 30; x += 3) {
		uint8 *p = im.RowPtr(x % 10) + x * 4;
		p[0] = static_cast<uint8>(p[0] > 20 ? p[0] - 12 : p[0] + 6);
	}
	NkImage a, b;
	NkVector<NkColor> pa, pb;
	ASSERT_TRUE(NkImagePalette::Quantize(im, 3, a, &pa, 5u));
	ASSERT_TRUE(NkImagePalette::Quantize(im, 3, b, &pb, 5u));
	ASSERT_EQUAL(3, static_cast<int>(pa.Size()));
	ASSERT_EQUAL(3, static_cast<int>(pb.Size()));
	for (usize i = 0; i < 3; ++i) {
		ASSERT_EQUAL(static_cast<int>(pa[i].r), static_cast<int>(pb[i].r));
		ASSERT_EQUAL(static_cast<int>(pa[i].b), static_cast<int>(pb[i].b));
	}
	// chaque pixel de sortie est l'un des trois centres
	bool tous = true;
	for (int32 y = 0; y < 10; ++y) {
		for (int32 x = 0; x < 30; ++x) {
			const uint8 *q = a.RowPtr(y) + x * 4;
			bool trouve = false;
			for (usize i = 0; i < 3; ++i) {
				if (q[0] == pa[i].r && q[1] == pa[i].g && q[2] == pa[i].b) {
					trouve = true;
				}
			}
			tous = tous && trouve;
		}
	}
	ASSERT_TRUE(tous);
}

TEST_CASE(NKImagePalette, P3_FondRelieAuBordSeulement) {
	NkImage im = Fond(20, 20, 255, 255, 255);
	Rect(im, 5, 5, 15, 15, 0, 0, 0);	// anneau noir plein...
	Rect(im, 8, 8, 12, 12, 255, 255, 255); // ... creuse d'un trou blanc ferme
	const int32 n = NkImagePalette::RemoveBackground(im, NkColor(255, 255, 255, 255), 10);
	ASSERT_EQUAL(400 - 100, n); // 300 pixels exterieurs ; le trou (16) et l'anneau restent
	ASSERT_EQUAL(0, static_cast<int>(im.RowPtr(0)[3]));
	ASSERT_EQUAL(255, static_cast<int>(im.RowPtr(10)[10 * 4 + 3])); // le trou interieur reste opaque
	ASSERT_EQUAL(255, static_cast<int>(im.RowPtr(6)[6 * 4 + 3]));	// l'anneau reste opaque
}

TEST_CASE(NKImagePalette, P4_BandesOccupees) {
	NkImage im = Fond(30, 40, 255, 255, 255);
	Rect(im, 3, 4, 20, 12, 0, 0, 0);	// lignes 4..11
	Rect(im, 5, 18, 25, 30, 0, 0, 0);	// lignes 18..29 (6 lignes vides entre : 12..17)
	Rect(im, 2, 37, 6, 39, 0, 0, 0);	// bande de 2 lignes : 37..38 (7 lignes vides avant)
	NkVector<NkImageSpan> s;
	const NkColor blanc(255, 255, 255, 255);

	NkImagePalette::Spans(im, blanc, 10, NkImageAxis::NK_ROWS, s, 1, 1);
	ASSERT_EQUAL(3, static_cast<int>(s.Size()));
	ASSERT_EQUAL(4, s[0].debut);
	ASSERT_EQUAL(12, s[0].fin);
	ASSERT_EQUAL(18, s[1].debut);
	ASSERT_EQUAL(30, s[1].fin);

	NkImagePalette::Spans(im, blanc, 10, NkImageAxis::NK_ROWS, s, 7, 1); // ecart 6 < 7 : fusion ; ecart 7 : non
	ASSERT_EQUAL(2, static_cast<int>(s.Size()));
	ASSERT_EQUAL(4, s[0].debut);
	ASSERT_EQUAL(30, s[0].fin);

	NkImagePalette::Spans(im, blanc, 10, NkImageAxis::NK_ROWS, s, 1, 5); // tailleMin 5 ecarte la bande de 2 lignes
	ASSERT_EQUAL(2, static_cast<int>(s.Size()));

	NkImagePalette::Spans(im, blanc, 10, NkImageAxis::NK_COLUMNS, s, 1, 1);
	ASSERT_EQUAL(1, static_cast<int>(s.Size())); // colonnes 2..24 d'un seul tenant
	ASSERT_EQUAL(2, s[0].debut);
	ASSERT_EQUAL(25, s[0].fin);
}
