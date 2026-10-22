// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_entete_liveness.cpp
//
// NKVFX/NkVfxLiveness.h compile SEUL et s'emploie : cette unite de traduction n'inclut QUE lui (et le cadre
// de test). Premier banc du module NKVFX.
//
// POURQUOI (22/10/2026) : l'epreuve des kits du 21/10 (un programme qui inclut l'en-tete) l'a trouve non
// autonome : NK_FORCE_INLINE est defini par NKMath/NkFunctions.h, qu'il n'incluait pas. Les deux seuls
// fichiers du depot qui l'emploient (test_ocean_grille.cpp, NkEauEcsProbe) incluaient d'autres en-tetes avant.
//
// PRE-ENREGISTREMENT :
//   (l1) detail::NkMedian : impair, pair, vide ;
//   (l2) NkMeasureLiveness sur un tick dont le travail est proportionnel a la resolution : un rapport
//        resHigh/resLow nettement superieur a 1 (le tick s'execute) ; sur un tick qui ne fait rien de la
//        resolution : un rapport proche de 1 ; tick nul : rapport 0.
// Contre-epreuve : retirer l'inclusion de NKMath/NkFunctions.h de l'en-tete : ce fichier ne compile plus.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKVFX/NkVfxLiveness.h"

using namespace nkentseu;

namespace {
	volatile uint64 gPuits = 0;

	void TickProportionnel(void *, uint32 resolution) {
		uint64 s = 0;
		for (uint32 i = 0; i < resolution * 2000u; ++i)
			s += (uint64)i * 2654435761u;
		gPuits = s;
	}

	void TickConstant(void *, uint32) {
		uint64 s = 0;
		for (uint32 i = 0; i < 20000u; ++i)
			s += (uint64)i * 2654435761u;
		gPuits = s;
	}
} // namespace

TEST_CASE(NKVFXEnteteLiveness, L1_Mediane) {
	float64 impair[5] = {5.0, 1.0, 4.0, 2.0, 3.0};
	ASSERT_TRUE(vfx::detail::NkMedian(impair, 5) == 3.0);
	float64 pair[4] = {4.0, 1.0, 3.0, 2.0};
	ASSERT_TRUE(vfx::detail::NkMedian(pair, 4) == 2.5);
	ASSERT_TRUE(vfx::detail::NkMedian(pair, 0) == 0.0);
}

TEST_CASE(NKVFXEnteteLiveness, L2_LeTickSExecute) {
	const vfx::NkLivenessReport vivant = vfx::NkMeasureLiveness(&TickProportionnel, nullptr, 8u, 32u, 7u, 3u);
	ASSERT_TRUE_MSG(vivant.ratio > 2.0, "un tick proportionnel a la resolution doit rendre un rapport > 2");
	const vfx::NkLivenessReport plat = vfx::NkMeasureLiveness(&TickConstant, nullptr, 8u, 32u, 7u, 3u);
	ASSERT_TRUE_MSG(plat.ratio > 0.0 && plat.ratio < 2.0, "un tick qui ignore la resolution doit rendre un rapport < 2");
	const vfx::NkLivenessReport rien = vfx::NkMeasureLiveness(nullptr, nullptr, 8u, 32u, 7u, 3u);
	ASSERT_TRUE(rien.ratio == 0.0);
}
