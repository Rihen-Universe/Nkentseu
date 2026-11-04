// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_trait_effile.cpp — banc de NkGuiDrawList::AddTaperedLine et de
// NkTexteCentreTaille (sans contexte graphique : on lit la liste de sommets).
//
// PRE-ENREGISTREMENT :
//   (t1) un trait horizontal de (0,10) a (100,10), largeurs 8 -> 2 : 4 sommets, 6
//        indices ; le bord au point A mesure 8 de haut, au point B 2 de haut
//   (t2) un trait de longueur nulle n'emet rien
//   (t3) largeurs egales (6, 6) : les 4 sommets forment un rectangle de 6 de haut
//        (meme resultat qu'un trait epais ordinaire)
//   (t4) NkTexteCentreTaille sans police : ne dessine rien et ne plante pas
// Contre-epreuve : dans AddTaperedLine remplacer `hb` par `ha` -> (t1) rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKGui/Core/NkGuiDrawList.h"
#include "NKCanvas/App/NkCanvasTexte.h"

using namespace nkentseu;
using namespace nkentseu::nkgui;

namespace {

	float32 Haut(const NkGuiVertex &p, const NkGuiVertex &q) {
		const float32 d = p.pos.y - q.pos.y;
		return d < 0.f ? -d : d;
	}

} // namespace

TEST_CASE(NKCanvasTraitEffile, T1_LargeurVarieDeAaB) {
	NkGuiDrawList dl;
	dl.AddTaperedLine(math::NkVec2f(0.f, 10.f), math::NkVec2f(100.f, 10.f), NkColor(255, 0, 0, 255), 8.f, 2.f);
	ASSERT_EQUAL(4, static_cast<int>(dl.vtx.Size()));
	ASSERT_EQUAL(6, static_cast<int>(dl.idx.Size()));
	ASSERT_NEAR(8.f, Haut(dl.vtx[0], dl.vtx[3]), 0.001f);
	ASSERT_NEAR(2.f, Haut(dl.vtx[1], dl.vtx[2]), 0.001f);
}

TEST_CASE(NKCanvasTraitEffile, T2_LongueurNulle) {
	NkGuiDrawList dl;
	dl.AddTaperedLine(math::NkVec2f(5.f, 5.f), math::NkVec2f(5.f, 5.f), NkColor(255, 0, 0, 255), 4.f, 2.f);
	ASSERT_EQUAL(0, static_cast<int>(dl.vtx.Size()));
}

TEST_CASE(NKCanvasTraitEffile, T3_LargeursEgales) {
	NkGuiDrawList dl;
	dl.AddTaperedLine(math::NkVec2f(0.f, 0.f), math::NkVec2f(50.f, 0.f), NkColor(0, 255, 0, 255), 6.f, 6.f);
	ASSERT_NEAR(6.f, Haut(dl.vtx[0], dl.vtx[3]), 0.001f);
	ASSERT_NEAR(6.f, Haut(dl.vtx[1], dl.vtx[2]), 0.001f);
}

TEST_CASE(NKCanvasTraitEffile, T4_TexteSansPolice) {
	NkGuiDrawList dl;
	nkentseu::renderer::NkTexteCentreTaille(dl, nullptr, 50.f, 10.f, "Salut", NkColor(255, 255, 255, 255), 24.f);
	ASSERT_EQUAL(0, static_cast<int>(dl.vtx.Size()));
}
