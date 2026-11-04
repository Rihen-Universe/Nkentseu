// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_grid_path.cpp — banc de NkGridPath (parcours en largeur + ligne de vue).
//
// PRE-ENREGISTREMENT (grille 10 x 10, mur vertical x = 5 de y = 0 a 8, passage en y = 9) :
//   (g1) de (0,0) a (9,0) sans lissage : 28 cases (27 pas = 14 + 13), chaque pas d'une
//        case, aucune case bloquee, extremites exactes
//   (g2) mur complet (y = 9 aussi) : aucun chemin, sortie vide, retour faux
//   (g3) avec lissage : moins de cases que 28, extremites conservees, chaque
//        paire consecutive a une ligne de vue, aucune case bloquee
//   (g4) grille ouverte : le chemin lisse n'a que 2 points
//   (g5) ligne de vue : deux cases bloquees en diagonale (4,4) et (5,5) ferment le
//        passage de (4,5) a (5,4) ; le segment (0,0)-(9,9) est coupe
// Contre-epreuve : retirer le voisin « haut » du BFS -> (g1) rouge ; retirer la
// verification des deux cases de l'angle dans LineOfSight -> (g5) rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKNavigation/NkGridPath.h"

using namespace nkentseu;
using namespace nkentseu::nav;

namespace {

	void Mur(NkGridPath &g, bool complet) {
		g.Init(10, 10);
		for (int32 y = 0; y < (complet ? 10 : 9); ++y) {
			g.SetBlocked(5, y, true);
		}
	}

} // namespace

TEST_CASE(NKNavigationGridPath, G1_BfsOptimal) {
	NkGridPath g;
	Mur(g, false);
	NkVector<NkGridCell> p;
	ASSERT_TRUE(g.FindPath(0, 0, 9, 0, p, false));
	ASSERT_EQUAL(28, static_cast<int>(p.Size()));
	ASSERT_EQUAL(0, p[0].x);
	ASSERT_EQUAL(0, p[0].y);
	ASSERT_EQUAL(9, p[27].x);
	ASSERT_EQUAL(0, p[27].y);
	for (usize i = 0; i < p.Size(); ++i) {
		ASSERT_FALSE(g.IsBlocked(p[i].x, p[i].y));
		if (i > 0) {
			const int32 d = (p[i].x > p[i - 1].x ? p[i].x - p[i - 1].x : p[i - 1].x - p[i].x) +
							(p[i].y > p[i - 1].y ? p[i].y - p[i - 1].y : p[i - 1].y - p[i].y);
			ASSERT_EQUAL(1, d);
		}
	}
}

TEST_CASE(NKNavigationGridPath, G2_SansChemin) {
	NkGridPath g;
	Mur(g, true);
	NkVector<NkGridCell> p;
	ASSERT_FALSE(g.FindPath(0, 0, 9, 0, p, true));
	ASSERT_EQUAL(0, static_cast<int>(p.Size()));
}

TEST_CASE(NKNavigationGridPath, G3_Lissage) {
	NkGridPath g;
	Mur(g, false);
	NkVector<NkGridCell> p;
	ASSERT_TRUE(g.FindPath(0, 0, 9, 0, p, true));
	ASSERT_TRUE(p.Size() < 28);
	ASSERT_TRUE(p.Size() >= 3); // il faut contourner le mur
	ASSERT_EQUAL(0, p[0].x);
	ASSERT_EQUAL(9, p[p.Size() - 1].x);
	for (usize i = 1; i < p.Size(); ++i) {
		ASSERT_TRUE(g.LineOfSight(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y));
		ASSERT_FALSE(g.IsBlocked(p[i].x, p[i].y));
	}
}

TEST_CASE(NKNavigationGridPath, G4_GrilleOuverte) {
	NkGridPath g;
	g.Init(10, 10);
	NkVector<NkGridCell> p;
	ASSERT_TRUE(g.FindPath(0, 0, 9, 7, p, true));
	ASSERT_EQUAL(2, static_cast<int>(p.Size()));
}

TEST_CASE(NKNavigationGridPath, G5_LigneDeVueEtCoins) {
	NkGridPath g;
	g.Init(10, 10);
	g.SetBlocked(4, 4, true);
	g.SetBlocked(5, 5, true);
	ASSERT_FALSE(g.LineOfSight(0, 0, 9, 9));
	// la diagonale (4,5) -> (5,4) passe entre les deux coins bloques : interdit
	ASSERT_FALSE(g.LineOfSight(4, 5, 5, 4));
	ASSERT_TRUE(g.LineOfSight(0, 9, 9, 9)); // un segment loin des blocs reste libre
}
