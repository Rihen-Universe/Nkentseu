// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_uniqueptr_conversion.cpp
//
// NkUniquePtr<Derivee> se convertit en NkUniquePtr<Base> par deplacement
// (construction et affectation), comme std::unique_ptr. Cause : l'erreur de
// compilation du 02/11 (un conteneur de NkUniquePtr<Base> recevant un Derivee).
//
// PRE-ENREGISTREMENT :
//   (c1) construction : la source devient vide, la destruction a lieu UNE fois
//   (c2) affectation : l'ancien contenu de la cible est detruit, la source vide
//   (c3) la conversion n'existe que si Base est une base de Derivee
// Contre-epreuve : retirer les deux gabarits de conversion de NkUniquePtr ->
// ce fichier ne compile plus ; mettre un double Delete -> (c1) donne 2.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkUniquePtr.h"

using namespace nkentseu;
using namespace nkentseu::memory;

namespace {

	int gDetruits = 0;

	struct Base {
			virtual ~Base() = default;
			int valeur = 5;
	};
	struct Derivee : Base {
			~Derivee() override {
				++gDetruits;
			}
	};
	struct Etrangere {};

	// Vrai si NkUniquePtr<A> se construit depuis NkUniquePtr<B>&& (comme Essai de NkSharedPtr)
	template <typename A, typename B> struct Convertible {
		private:
			static char Essai(NkUniquePtr<A>) noexcept;
			static long Essai(...) noexcept;

		public:
			static constexpr bool value = sizeof(Essai(static_cast<NkUniquePtr<B> &&>(*static_cast<NkUniquePtr<B> *>(nullptr)))) == sizeof(char);
	};

} // namespace

TEST_CASE(NKMemoryUniquePtrConversion, C1_Construction) {
	gDetruits = 0;
	{
		NkUniquePtr<Derivee> d = NkMakeUnique<Derivee>();
		Derivee *brut = d.Get();
		NkUniquePtr<Base> b(static_cast<NkUniquePtr<Derivee> &&>(d));
		ASSERT_TRUE(d.Get() == nullptr);
		ASSERT_TRUE(static_cast<Base *>(brut) == b.Get());
		ASSERT_EQUAL(5, b->valeur);
	}
	ASSERT_EQUAL(1, gDetruits);
}

TEST_CASE(NKMemoryUniquePtrConversion, C2_Affectation) {
	gDetruits = 0;
	{
		NkUniquePtr<Base> b = NkMakeUnique<Derivee>();
		NkUniquePtr<Derivee> d = NkMakeUnique<Derivee>();
		b = static_cast<NkUniquePtr<Derivee> &&>(d);
		ASSERT_EQUAL(1, gDetruits); // l'ancien contenu de b est parti
		ASSERT_TRUE(d.Get() == nullptr);
		ASSERT_TRUE(b.Get() != nullptr);
	}
	ASSERT_EQUAL(2, gDetruits);
}

TEST_CASE(NKMemoryUniquePtrConversion, C3_SeulementVersUneBase) {
	ASSERT_TRUE((Convertible<Base, Derivee>::value));
	ASSERT_FALSE((Convertible<Derivee, Base>::value));
	ASSERT_FALSE((Convertible<Etrangere, Derivee>::value));
}
