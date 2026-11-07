// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_view25d.cpp — banc de NkView25D (la vue 2.5D, montee dans NKCanvas le 07/11/2026 ; elle vivait dans FoodHunt).
//
// PRE-ENREGISTREMENT (chaque valeur est calculee A LA MAIN, pas lue dans le code) :
//   (v1) vue NEUTRE 540 x 1000, tilt 0.65 : Project(100, 200, 30) = (100, 200 * 0.65 - 30) = (100, 100)
//   (v2) aller-retour : Unproject(Project(x, y, 0)) rend (x, y) a 1e-3 pres, sur une vue ADAPTEE (Adapt 1080 x 1920, monde 540 x 960)
//   (v3) Adapt : echelle min(1080 / 540, 1920 / 960) = 2 ; zone visible 540 x 960 ; Scale() = 2
//        et sur une fenetre PLUS LARGE (1600 x 960, monde 540 x 960) : echelle min(2.96, 1) = 1, zone visible 1600 x 960 (on montre plus de monde)
//   (v4) plancher : ScaleFor(10, 10, 540, 960) = 0.2 (jamais nulle)
//   (v5) ombre : ShadowPos(x, y, z) = Project(x + 0.55 z, y + 0.30 z, 0) : pour (100, 200, 40) sur la vue neutre = (122, 212 * 0.65) = (122, 137.8)
//   (v6) profondeur : Depth(_, y, z) = y + z * tilt : (0, 10, 20) -> 10 + 13 = 23 ; un point plus haut est plus pres
// Contre-epreuve : dans Unproject retirer la division par `tilt` -> (v2) rouge ; dans ShadowPos oublier `lightDir.y * z` -> (v5) rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCanvas/Renderer/Core/NkView25D.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

TEST_CASE(NKCanvasView25D, V1_ProjectionNeutre) {
	NkView25D v;
	v.Neutral(540.f, 1000.f);
	const NkView25D::NkVec2f p = v.Project(100.f, 200.f, 30.f);
	ASSERT_NEAR(100.f, p.x, 1e-3f);
	ASSERT_NEAR(100.f, p.y, 1e-3f);
}

TEST_CASE(NKCanvasView25D, V2_AllerRetour) {
	NkView25D v;
	v.Adapt(1080.f, 1920.f, 540.f, 960.f);
	const float xs[3] = {0.f, 123.4f, 500.f}, ys[3] = {0.f, 77.7f, 900.f};
	for (int i = 0; i < 3; ++i) {
		const NkView25D::NkVec2f p = v.Project(xs[i], ys[i], 0.f);
		const NkView25D::NkVec2f q = v.Unproject(p.x, p.y);
		ASSERT_NEAR(xs[i], q.x, 1e-3f);
		ASSERT_NEAR(ys[i], q.y, 1e-3f);
	}
}

TEST_CASE(NKCanvasView25D, V3_Adapt) {
	NkView25D v;
	v.Adapt(1080.f, 1920.f, 540.f, 960.f);
	ASSERT_NEAR(2.f, v.Scale(), 1e-4f);
	ASSERT_NEAR(540.f, v.size.x, 1e-3f);
	ASSERT_NEAR(960.f, v.size.y, 1e-3f);
	v.Adapt(1600.f, 960.f, 540.f, 960.f); // fenetre plus large : l'echelle reste 1, on voit plus de monde en largeur
	ASSERT_NEAR(1.f, v.Scale(), 1e-4f);
	ASSERT_NEAR(1600.f, v.size.x, 1e-3f);
	ASSERT_NEAR(960.f, v.size.y, 1e-3f);
}

TEST_CASE(NKCanvasView25D, V4_Plancher) {
	ASSERT_NEAR(0.2f, NkView25D::ScaleFor(10.f, 10.f, 540.f, 960.f), 1e-6f);
}

TEST_CASE(NKCanvasView25D, V5_Ombre) {
	NkView25D v;
	v.Neutral(540.f, 1000.f);
	const NkView25D::NkVec2f o = v.ShadowPos(100.f, 200.f, 40.f);
	ASSERT_NEAR(122.f, o.x, 1e-3f);
	ASSERT_NEAR(137.8f, o.y, 1e-3f);
}

TEST_CASE(NKCanvasView25D, V6_Profondeur) {
	NkView25D v;
	ASSERT_NEAR(23.f, v.Depth(0.f, 10.f, 20.f), 1e-4f);
	ASSERT_TRUE(v.Depth(0.f, 10.f, 30.f) > v.Depth(0.f, 10.f, 20.f));
}
