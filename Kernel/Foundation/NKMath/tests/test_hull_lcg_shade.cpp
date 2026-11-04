// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_hull_lcg_shade.cpp
//
// Banc de NkColor::Shade, NkLcg32 et NkConvexHull2D.
//
// PRE-ENREGISTREMENT :
//   (s1) Shade(0.5) divise le RGB par 2 (arrondi), garde l'alpha ; Shade(2) borne a 255 ; Shade(0) = noir
//   (l1) deux NkLcg32 de meme graine donnent la meme suite ; Seed() rejoue ; graines
//        differentes divergent ; NextFloat32 reste dans [0, 1[ sur 10000 tirages
//   (l2) valeur exacte : graine 1 -> premier tirage 1015568748 (1664525 * 1 + 1013904223)
//   (h1) carre avec points interieurs ET points sur les cotes : 4 sommets, anti-horaire
//   (h2) points tous alignes : 2 extremites ; 1 point : 1 ; doublons ignores
// Contre-epreuve : dans NkConvexHull2D remplacer `<= 0.0f` par `< 0.0f` -> (h1)
// rouge (les points sur les cotes restent) ; dans NkLcg32 changer 1664525u -> (l2) rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"
#include "NKMath/NkConvexHull2D.h"

using namespace nkentseu;
using namespace nkentseu::math;

TEST_CASE(NKMathShade, S1_BorneEtAlpha) {
	const NkColor c(100, 200, 50, 77);
	const NkColor m = c.Shade(0.5f);
	ASSERT_EQUAL(50, static_cast<int>(m.r));
	ASSERT_EQUAL(100, static_cast<int>(m.g));
	ASSERT_EQUAL(25, static_cast<int>(m.b));
	ASSERT_EQUAL(77, static_cast<int>(m.a));
	const NkColor h = c.Shade(2.0f);
	ASSERT_EQUAL(200, static_cast<int>(h.r));
	ASSERT_EQUAL(255, static_cast<int>(h.g));
	const NkColor z = c.Shade(0.0f);
	ASSERT_EQUAL(0, static_cast<int>(z.r + z.g + z.b));
}

TEST_CASE(NKMathLcg32, L1_Rejouable) {
	NkLcg32 a(42u), b(42u), c(43u);
	bool divergent = false;
	for (int i = 0; i < 100; ++i) {
		const uint32 va = a.NextUInt32();
		ASSERT_EQUAL(va, b.NextUInt32());
		if (va != c.NextUInt32()) {
			divergent = true;
		}
	}
	ASSERT_TRUE(divergent);
	a.Seed(42u);
	b.Seed(42u);
	ASSERT_EQUAL(a.NextUInt32(), b.NextUInt32());
	NkLcg32 f(7u);
	for (int i = 0; i < 10000; ++i) {
		const float32 v = f.NextFloat32();
		ASSERT_TRUE(v >= 0.0f && v < 1.0f);
	}
}

TEST_CASE(NKMathLcg32, L2_ValeurExacte) {
	NkLcg32 g(1u);
	ASSERT_EQUAL(1015568748u, g.NextUInt32());
}

TEST_CASE(NKMathConvexHull2D, H1_CarreAvecPointsSurLesCotes) {
	NkVector<NkVec2f> pts;
	pts.PushBack(NkVec2f(0, 0));
	pts.PushBack(NkVec2f(4, 0));
	pts.PushBack(NkVec2f(4, 4));
	pts.PushBack(NkVec2f(0, 4));
	pts.PushBack(NkVec2f(2, 2)); // interieur
	pts.PushBack(NkVec2f(2, 0)); // sur un cote
	pts.PushBack(NkVec2f(4, 2)); // sur un cote
	pts.PushBack(NkVec2f(1, 3)); // interieur
	NkVector<NkVec2f> h;
	NkConvexHull2D(pts, h);
	ASSERT_EQUAL(4, static_cast<int>(h.Size()));
	// aire signee (formule du lacet) positive = anti-horaire, egale a 16
	float32 aire2 = 0.0f;
	for (usize i = 0; i < h.Size(); ++i) {
		const NkVec2f &p = h[i];
		const NkVec2f &q = h[(i + 1) % h.Size()];
		aire2 += p.x * q.y - q.x * p.y;
	}
	ASSERT_NEAR(32.0f, aire2, 0.001f);
}

TEST_CASE(NKMathConvexHull2D, H2_CasLimites) {
	NkVector<NkVec2f> pts, h;
	for (int i = 0; i < 5; ++i) {
		pts.PushBack(NkVec2f(static_cast<float32>(i), static_cast<float32>(i)));
	}
	NkConvexHull2D(pts, h);
	ASSERT_EQUAL(2, static_cast<int>(h.Size()));
	pts.Clear();
	pts.PushBack(NkVec2f(1, 1));
	pts.PushBack(NkVec2f(1, 1));
	NkConvexHull2D(pts, h);
	ASSERT_EQUAL(1, static_cast<int>(h.Size()));
}
