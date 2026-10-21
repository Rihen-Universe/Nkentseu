// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_sharedptr_conversion.cpp
//
// NkSharedPtr<Derivee> se convertit en NkSharedPtr<Base>, comme
// std::shared_ptr : copie (compteur partage) et deplacement (source videe).
//
// POURQUOI (21/10/2026) : les exemples de NKLogger (NkLogger.h l. 107, 181,
// 186, signales par des apprenants) passent NkMakeShared<NkConsoleSink>() a
// NkLogger::AddSink(NkSharedPtr<NkISink>). Sans conversion vers la base, cet
// usage ne compilait pas : le code du depot contournait par
// « NkSharedPtr<NkISink> s(new NkConsoleSink()) ».
//
// PRE-ENREGISTREMENT :
//   (c1) copie Derivee -> Base : meme objet, UseCount 2 des deux cotes
//   (c2) deplacement Derivee -> Base : source vide, UseCount 1, meme objet
//   (c3) destruction UNE fois, par le type REEL, meme sans destructeur virtuel
//        dans la base (le bloc de controle garde NkSharedControlBlock<Derivee>)
//   (c4) Derivee -> const Base, et pointeur nul converti reste nul
//   (c5) la conversion n'existe QUE si Derivee* -> Base* existe : deux
//        surcharges Choisir(NkSharedPtr<A>) et Choisir(NkSharedPtr<B>), un
//        NkSharedPtr<DeriveeDeA> choisit A sans ambiguite (une conversion non
//        contrainte rendrait l'appel ambigu : erreur de compilation)
// Contre-epreuve : retirer les deux constructeurs de conversion -> ce fichier
// ne compile plus ; retirer la contrainte -> (c5) ne compile plus (ambigu).
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkSharedPtr.h"

using namespace nkentseu;
using namespace nkentseu::memory;

namespace {

	int gDetruitsV = 0;
	int gDetruitsNV = 0;

	struct BaseV {
			virtual ~BaseV() = default;
			int valeur = 7;
	};
	struct DeriveeV : BaseV {
			~DeriveeV() override {
				++gDetruitsV;
			}
	};

	// Base SANS destructeur virtuel : la destruction doit quand meme passer par
	// ~DeriveeNV, parce que le bloc de controle connait le type reel.
	struct BaseNV {
			int x = 3;
	};
	struct DeriveeNV : BaseNV {
			~DeriveeNV() {
				++gDetruitsNV;
			}
	};

	struct A {
			int a = 1;
	};
	struct B {
			int b = 2;
	};
	struct DeriveeDeA : A {};

	int Choisir(const NkSharedPtr<A> &) {
		return 1;
	}
	int Choisir(const NkSharedPtr<B> &) {
		return 2;
	}

} // namespace

TEST_CASE(NKMemorySharedPtrConversion, C1_CopieVersLaBase) {
	gDetruitsV = 0;
	{
		NkSharedPtr<DeriveeV> d = NkMakeShared<DeriveeV>();
		NkSharedPtr<BaseV> b = d;
		ASSERT_TRUE(static_cast<BaseV *>(d.Get()) == b.Get());
		ASSERT_EQUAL(2, d.UseCount());
		ASSERT_EQUAL(2, b.UseCount());
		ASSERT_EQUAL(7, b->valeur);
	}
	ASSERT_EQUAL(1, gDetruitsV);
}

TEST_CASE(NKMemorySharedPtrConversion, C2_DeplacementVersLaBase) {
	gDetruitsV = 0;
	{
		NkSharedPtr<DeriveeV> d = NkMakeShared<DeriveeV>();
		DeriveeV *brut = d.Get();
		NkSharedPtr<BaseV> b = static_cast<NkSharedPtr<DeriveeV> &&>(d);
		ASSERT_TRUE(d.Get() == nullptr);
		ASSERT_EQUAL(0, d.UseCount());
		ASSERT_EQUAL(1, b.UseCount());
		ASSERT_TRUE(static_cast<BaseV *>(brut) == b.Get());
	}
	ASSERT_EQUAL(1, gDetruitsV);
}

TEST_CASE(NKMemorySharedPtrConversion, C3_DestructionParLeTypeReel) {
	gDetruitsNV = 0;
	{
		NkSharedPtr<BaseNV> b = NkMakeShared<DeriveeNV>();
		ASSERT_EQUAL(1, b.UseCount());
		ASSERT_EQUAL(3, b->x);
	}
	ASSERT_EQUAL(1, gDetruitsNV);

	gDetruitsNV = 0;
	{
		NkSharedPtr<BaseNV> b;
		{
			NkSharedPtr<DeriveeNV> d = NkMakeShared<DeriveeNV>();
			b = d; // conversion puis affectation
			ASSERT_EQUAL(2, b.UseCount());
		}
		ASSERT_EQUAL(0, gDetruitsNV); // b le garde vivant
		ASSERT_EQUAL(1, b.UseCount());
	}
	ASSERT_EQUAL(1, gDetruitsNV);
}

TEST_CASE(NKMemorySharedPtrConversion, C4_ConstEtNul) {
	NkSharedPtr<DeriveeV> d = NkMakeShared<DeriveeV>();
	NkSharedPtr<const BaseV> cb = d;
	ASSERT_EQUAL(2, cb.UseCount());
	ASSERT_EQUAL(7, cb->valeur);

	NkSharedPtr<DeriveeV> nul;
	NkSharedPtr<BaseV> bnul = nul;
	ASSERT_TRUE(bnul.Get() == nullptr);
	ASSERT_EQUAL(0, bnul.UseCount());
}

TEST_CASE(NKMemorySharedPtrConversion, C5_ConversionContrainte) {
	NkSharedPtr<DeriveeDeA> p = NkMakeShared<DeriveeDeA>();
	ASSERT_EQUAL(1, Choisir(p));
}
