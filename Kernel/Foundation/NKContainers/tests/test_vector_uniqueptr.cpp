// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_vector_uniqueptr.cpp
//
// Un NkVector<NkUniquePtr<Base>> recoit un Derivee : c'est la forme exacte de
// l'erreur de compilation du 02/11 (Scene de FoodHunt). Le destructeur virtuel
// est appele UNE seule fois, y compris apres une reallocation du vecteur.
// Contre-epreuve : retirer les gabarits de conversion de NkUniquePtr ->
// ce fichier ne compile plus.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkUniquePtr.h"

using namespace nkentseu;
using namespace nkentseu::memory;

namespace {

	int gDetruits = 0;

	struct Base {
			virtual ~Base() = default;
			int valeur = 9;
	};
	struct Derivee : Base {
			~Derivee() override {
				++gDetruits;
			}
	};

} // namespace

TEST_CASE(NKContainersVectorUniquePtr, DeriveeDansVecteurDeBase) {
	gDetruits = 0;
	{
		NkVector<NkUniquePtr<Base>> v;
		for (int i = 0; i < 20; ++i) { // 20 : force plusieurs reallocations
			v.PushBack(NkUniquePtr<Base>(NkMakeUnique<Derivee>()));
		}
		ASSERT_EQUAL(20, static_cast<int>(v.Size()));
		ASSERT_EQUAL(9, v[7]->valeur);
		ASSERT_EQUAL(0, gDetruits);
	}
	ASSERT_EQUAL(20, gDetruits);
}
