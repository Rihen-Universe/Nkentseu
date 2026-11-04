// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_stable_sort.cpp
//
// NkStableSort : trie, et conserve l'ordre d'origine des elements equivalents.
//
// PRE-ENREGISTREMENT :
//   (c1) 2000 elements, 7 cles seulement (beaucoup d'egalites) : la sortie est
//        croissante en cle ET, a cle egale, croissante en numero d'origine
//   (c2) un tableau deja trie reste identique ; un tableau decroissant s'inverse
//   (c3) tailles 0, 1, 2, 17 (juste au-dessus du seuil d'insertion) valides
// Contre-epreuve : a la fusion, remplacer `avant(*d, *g)` par `!avant(*g, *d)`
// (la droite passe devant en cas d'egalite) -> (c1) devient rouge.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/Utilities/NkSort.h"

using namespace nkentseu;

namespace {

	struct Item {
			int cle;
			int ordre;
	};

	bool CleAvant(const Item &a, const Item &b) {
		return a.cle < b.cle;
	}

	// Remplit n items avec des cles pseudo aleatoires (congruentiel local)
	void Remplir(Item *t, int n, int nbCles) {
		unsigned int s = 12345u;
		for (int i = 0; i < n; ++i) {
			s = s * 1664525u + 1013904223u;
			t[i].cle = static_cast<int>((s >> 16) % static_cast<unsigned int>(nbCles));
			t[i].ordre = i;
		}
	}

	bool EstStable(const Item *t, int n) {
		for (int i = 1; i < n; ++i) {
			if (t[i - 1].cle > t[i].cle) {
				return false;
			}
			if (t[i - 1].cle == t[i].cle && t[i - 1].ordre > t[i].ordre) {
				return false;
			}
		}
		return true;
	}

} // namespace

TEST_CASE(NKContainersStableSort, C1_StableSurEgalites) {
	static Item t[2000];
	Remplir(t, 2000, 7);
	NkStableSort(t, t + 2000, CleAvant);
	ASSERT_TRUE(EstStable(t, 2000));
}

TEST_CASE(NKContainersStableSort, C2_DejaTrieEtDecroissant) {
	static Item t[300];
	for (int i = 0; i < 300; ++i) {
		t[i].cle = i;
		t[i].ordre = i;
	}
	NkStableSort(t, t + 300, CleAvant);
	ASSERT_TRUE(EstStable(t, 300));
	ASSERT_EQUAL(299, t[299].cle);
	for (int i = 0; i < 300; ++i) {
		t[i].cle = 299 - i;
		t[i].ordre = i;
	}
	NkStableSort(t, t + 300, CleAvant);
	ASSERT_TRUE(EstStable(t, 300));
	ASSERT_EQUAL(299, t[0].ordre);
}

TEST_CASE(NKContainersStableSort, C3_PetitesTailles) {
	static Item t[17];
	const int tailles[4] = {0, 1, 2, 17};
	for (int k = 0; k < 4; ++k) {
		Remplir(t, tailles[k], 3);
		NkStableSort(t, t + tailles[k], CleAvant);
		ASSERT_TRUE(EstStable(t, tailles[k]));
	}
}
