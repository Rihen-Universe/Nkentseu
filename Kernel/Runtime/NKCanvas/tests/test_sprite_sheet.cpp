// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_sprite_sheet.cpp — banc de NkSpriteSheet (grille, detection, noms).
//
// PRE-ENREGISTREMENT :
//   (s1) grille 4 x 2 sur 400 x 200, marges 10, ecart 0, zone utile pleine :
//        8 rectangles ; le 6e (colonne 1, ligne 1) est (10 + 95, 10 + 90, 95, 90) ; le 3e et le 5e
//        distinguent colonne et ligne
//   (s2) zone utile {x 0.5, y 0, l 0.5, h 0.5} : le rectangle est la moitie
//        haut-droite de la case
//   (s3) noms : Find("Burger") rend le rectangle de l'indice 0, un nom inconnu rend
//        nullptr, un nom absent s'appelle case_<n>
//   (s4) detection sur image synthetique : 3 carres noirs sur fond blanc, deux
//        sur la premiere ligne et un sur la seconde : 3 rectangles exacts, ordre de lecture
//   (s5) PLANCHES REELLES (lues depuis le dossier de Rodolf par les variables
//        NK_PLANCHE_1 et NK_PLANCHE_2 ; jamais copiees, ignorees si absentes) :
//        planche 1 en grille 4 x 4 : 16 rectangles, aucun ne touche la case a
//        cocher (x < 0.30 de la case) ni l'etiquette (y > 0.80) ;
//        planche 2 en detection : 72 rectangles (6 x 12)
//   (s6) EXTRACT (07/11) : image 40 x 40 blanche, un carre rouge 10 x 10 en (10, 10) dont le centre est BLANC (un blanc interieur : il ne doit pas
//        devenir transparent), une case unique = toute l'image : la sous-image fait 12 x 12 (le carre + 1 pixel de marge), le pixel de marge est
//        transparent (alpha 0), le coin du carre opaque, le blanc interieur OPAQUE ; un indice hors planche rend faux
// Contre-epreuve : dans Extract retirer le recadrage (out = sub) -> (s6) rouge ; dans BuildGrid inverser `+ c * (l + g.ecartX)` en `+ r * ...`
// -> (s1) rouge ; dans BuildAuto ignorer la bande (zoneBande = image entiere) ->
// (s4) rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCanvas/Renderer/Resources/NkSpriteSheet.h"
#include "NKImage/Core/NkImagePalette.h"
#include "NKPlatform/NkEnv.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

namespace {

	bool Egal(const math::NkRect2i &r, int32 x, int32 y, int32 w, int32 h) {
		return r.left == x && r.top == y && r.width == w && r.height == h;
	}

} // namespace

TEST_CASE(NKCanvasSpriteSheet, S1_GrilleExacte) {
	NkSheetGrid g;
	g.colonnes = 4;
	g.lignes = 2;
	g.margeGauche = g.margeHaut = g.margeDroite = g.margeBas = 10;
	NkSpriteSheet s;
	ASSERT_EQUAL(8, s.BuildGrid(400, 200, g));
	// largeur de case (400 - 20) / 4 = 95 ; hauteur (200 - 20) / 2 = 90
	ASSERT_TRUE(Egal(s.Get(5)->rect, 10 + 95, 10 + 90, 95, 90));
	ASSERT_TRUE(Egal(s.Get(0)->rect, 10, 10, 95, 90));
	ASSERT_TRUE(Egal(s.Get(2)->rect, 10 + 2 * 95, 10, 95, 90)); // colonne 2, ligne 0
	ASSERT_TRUE(Egal(s.Get(4)->rect, 10, 10 + 90, 95, 90));	  // colonne 0, ligne 1
}

TEST_CASE(NKCanvasSpriteSheet, S2_ZoneUtile) {
	NkSheetGrid g;
	g.colonnes = 2;
	g.lignes = 2;
	g.utileX = 0.5f;
	g.utileY = 0.f;
	g.utileL = 0.5f;
	g.utileH = 0.5f;
	NkSpriteSheet s;
	s.BuildGrid(200, 100, g); // cases de 100 x 50
	ASSERT_TRUE(Egal(s.Get(0)->rect, 50, 0, 50, 25));
	ASSERT_TRUE(Egal(s.Get(3)->rect, 150, 50, 50, 25));
}

TEST_CASE(NKCanvasSpriteSheet, S3_Noms) {
	NkSheetGrid g;
	g.colonnes = 3;
	g.lignes = 1;
	NkSpriteSheet s;
	s.BuildGrid(90, 30, g);
	const char *noms[2] = {"Burger", "Pizza"};
	s.SetNames(noms, 2);
	ASSERT_TRUE(s.Find("Burger") != nullptr);
	ASSERT_EQUAL(0, s.Find("Burger")->left);
	ASSERT_EQUAL(30, s.Find("Pizza")->left);
	ASSERT_TRUE(s.Find("Inconnu") == nullptr);
	ASSERT_TRUE(s.Find("case_2") != nullptr);
	s.Rename(2, "Salade");
	ASSERT_TRUE(s.Find("Salade") != nullptr);
	ASSERT_TRUE(s.Find("case_2") == nullptr);
}

TEST_CASE(NKCanvasSpriteSheet, S4_DetectionSynthetique) {
	NkImage im = NkImage::Create(100, 60, NkImagePixelFormat::NK_RGBA32, 0u);
	for (int32 y = 0; y < 60; ++y) {
		for (int32 x = 0; x < 100; ++x) {
			uint8 *p = im.RowPtr(y) + x * 4;
			p[0] = p[1] = p[2] = 255;
			p[3] = 255;
		}
	}
	auto carre = [&](int32 x0, int32 y0, int32 x1, int32 y1) {
		for (int32 y = y0; y < y1; ++y) {
			for (int32 x = x0; x < x1; ++x) {
				uint8 *p = im.RowPtr(y) + x * 4;
				p[0] = p[1] = p[2] = 0;
			}
		}
	};
	carre(5, 5, 25, 25);
	carre(60, 8, 90, 20);
	carre(30, 40, 50, 55);
	NkSpriteSheet s;
	ASSERT_EQUAL(3, s.BuildAuto(im, math::NkColor(255, 255, 255, 255), 20, 3, 4));
	ASSERT_TRUE(Egal(s.Get(0)->rect, 5, 5, 20, 20));
	ASSERT_TRUE(Egal(s.Get(1)->rect, 60, 8, 30, 12));
	ASSERT_TRUE(Egal(s.Get(2)->rect, 30, 40, 20, 15));
}

TEST_CASE(NKCanvasSpriteSheet, S5_PlanchesReelles) {
	const char *p1 = env::GetEnvVar("NK_PLANCHE_1");
	const char *p2 = env::GetEnvVar("NK_PLANCHE_2");
	const char *sortie = env::GetEnvVar("NK_PLANCHE_SORTIE");
	const math::NkColor blanc(255, 255, 255, 255);
	if (p1 != nullptr) {
		NkImage im;
		ASSERT_TRUE(im.Load(p1, 4));
		NkSheetGrid g;
		g.colonnes = 4;
		g.lignes = 4;
		g.margeGauche = 35;
		g.margeHaut = 172;
		g.margeDroite = 35;
		g.margeBas = 48;
		g.utileX = 0.31f;
		g.utileY = 0.04f;
		g.utileL = 0.69f;
		g.utileH = 0.76f;
		NkSpriteSheet s;
		ASSERT_EQUAL(16, s.BuildGrid(im.Width(), im.Height(), g));
		const char *noms[16] = {"Burger", "Ice Cream", "Salad", "Donut", "Pizza", "Croissant", "Fried Eggs", "Fries",
								"French bread", "Pancake", "Noodles", "Hot Dog", "Cheese", "Bread", "Sandwich", "Milk"};
		s.SetNames(noms, 16);
		NkSheetGrid brute = g; // les cases entieres, pour verifier l'exclusion
		brute.utileX = 0.f;
		brute.utileY = 0.f;
		brute.utileL = 1.f;
		brute.utileH = 1.f;
		NkSpriteSheet cases;
		cases.BuildGrid(im.Width(), im.Height(), brute);
		s.Tighten(im, blanc, 60);
		for (int32 i = 0; i < 16; ++i) {
			const math::NkRect2i r = s.Get(i)->rect;
			const math::NkRect2i c = cases.Get(i)->rect;
			ASSERT_TRUE(r.width >= 30 && r.height >= 30);
			ASSERT_TRUE(r.left >= c.left + static_cast<int32>(0.30f * static_cast<float32>(c.width)));
			ASSERT_TRUE(r.top + r.height <= c.top + static_cast<int32>(0.81f * static_cast<float32>(c.height)));
			if (sortie != nullptr) {
				NkImage rec = im.Crop(r.left, r.top, r.width, r.height);
				char chemin[400];
				int32 k = 0;
				for (; sortie[k] != 0 && k < 380; ++k) {
					chemin[k] = sortie[k];
				}
				chemin[k++] = '/';
				chemin[k++] = 'a';
				chemin[k++] = static_cast<char>('0' + i / 10);
				chemin[k++] = static_cast<char>('0' + i % 10);
				chemin[k++] = '.';
				chemin[k++] = 'p';
				chemin[k++] = 'n';
				chemin[k++] = 'g';
				chemin[k] = 0;
				rec.Save(chemin);
			}
		}
	}
	if (p2 != nullptr) {
		NkImage im;
		ASSERT_TRUE(im.Load(p2, 4));
		NkSpriteSheet s;
		ASSERT_EQUAL(72, s.BuildAuto(im, blanc, 45, 12, 8));
	}
}

TEST_CASE(NKCanvasSpriteSheet, S6_Extract) {
	NkImage im = NkImage::Create(40, 40, NkImagePixelFormat::NK_RGBA32, 0u);
	for (int32 y = 0; y < 40; ++y) {
		for (int32 x = 0; x < 40; ++x) {
			uint8 *p = im.RowPtr(y) + x * 4;
			p[0] = p[1] = p[2] = 255;
			p[3] = 255;
		}
	}
	for (int32 y = 10; y < 20; ++y) {
		for (int32 x = 10; x < 20; ++x) {
			uint8 *p = im.RowPtr(y) + x * 4;
			p[0] = 255;
			p[1] = p[2] = 0;
		}
	}
	for (int32 y = 13; y < 17; ++y) { // le blanc INTERIEUR
		for (int32 x = 13; x < 17; ++x) {
			uint8 *p = im.RowPtr(y) + x * 4;
			p[0] = p[1] = p[2] = 255;
		}
	}
	NkSheetGrid g;
	g.colonnes = 1;
	g.lignes = 1;
	NkSpriteSheet s;
	ASSERT_EQUAL(1, s.BuildGrid(40, 40, g));
	NkImage out;
	ASSERT_TRUE(s.Extract(im, 0, math::NkColor(255, 255, 255, 255), 20, out));
	ASSERT_EQUAL(12, out.Width());
	ASSERT_EQUAL(12, out.Height());
	ASSERT_EQUAL(0, static_cast<int32>(out.RowPtr(0)[3]));		// la marge : transparente
	ASSERT_EQUAL(255, static_cast<int32>(out.RowPtr(1)[1 * 4 + 3])); // le coin du carre : opaque
	ASSERT_EQUAL(255, static_cast<int32>(out.RowPtr(6)[6 * 4 + 3])); // le blanc interieur : opaque
	NkImage rien;
	ASSERT_FALSE(s.Extract(im, 5, math::NkColor(255, 255, 255, 255), 20, rien));
}
