// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NKContainers.h"
#include "NKPlatform/NkFoundationLog.h"
#include "NKMemory/NkUtils.h" // NkMemSet : le poison de l'allocateur de NKContainersVectorAlias

#include <chrono>
#include <stdio.h>
#include <vector>

using namespace nkentseu;

namespace {

	template <typename Fn> double MeasureNs(Fn &&fn) {
		const auto t0 = std::chrono::high_resolution_clock::now();
		fn();
		const auto t1 = std::chrono::high_resolution_clock::now();
		return std::chrono::duration<double, std::nano>(t1 - t0).count();
	}

} // namespace

TEST_CASE(NKContainersVector, PushBackAndAccess) {
	NkVector<int> values;
	ASSERT_TRUE(values.Empty());

	values.PushBack(10);
	values.PushBack(20);
	values.PushBack(30);

	ASSERT_EQUAL(3, static_cast<int>(values.Size()));
	ASSERT_EQUAL(10, values.Front());
	ASSERT_EQUAL(30, values.Back());
	ASSERT_EQUAL(20, values[1]);
}

TEST_CASE(NKContainersVector, InsertEraseAndResize) {
	NkVector<int> values;
	values.PushBack(1);
	values.PushBack(3);
	values.Insert(values.begin() + 1, 2);

	ASSERT_EQUAL(3, static_cast<int>(values.Size()));
	ASSERT_EQUAL(2, values[1]);

	values.Erase(values.begin() + 1);
	ASSERT_EQUAL(2, static_cast<int>(values.Size()));
	ASSERT_EQUAL(3, values[1]);

	values.Resize(5, 9);
	ASSERT_EQUAL(5, static_cast<int>(values.Size()));
	ASSERT_EQUAL(9, values[4]);

	values.Resize(1);
	ASSERT_EQUAL(1, static_cast<int>(values.Size()));
	ASSERT_EQUAL(1, values[0]);
}

TEST_CASE(NKContainersBenchmark, VectorVsStdVector) {
	constexpr int kCount = 200000;

	long long nkSum = 0;
	const double nkTimeNs = MeasureNs([&]() {
		NkVector<int> values;
		values.Reserve(kCount);
		for (int i = 0; i < kCount; ++i) {
			values.PushBack(i);
		}
		for (int i = 0; i < kCount; ++i) {
			nkSum += values[static_cast<NkVector<int>::SizeType>(i)];
		}
	});

	long long stlSum = 0;
	const double stlTimeNs = MeasureNs([&]() {
		std::vector<int> values;
		values.reserve(static_cast<std::size_t>(kCount));
		for (int i = 0; i < kCount; ++i) {
			values.push_back(i);
		}
		for (int i = 0; i < kCount; ++i) {
			stlSum += values[static_cast<std::size_t>(i)];
		}
	});

	ASSERT_TRUE(nkSum == stlSum);
	ASSERT_TRUE(nkTimeNs > 0.0);
	ASSERT_TRUE(stlTimeNs > 0.0);

	NK_FOUNDATION_LOG_INFO("[NKContainers Benchmark] NkVector vs std::vector");
	NK_FOUNDATION_LOG_INFO("  NkVector   : %.2f ns total", nkTimeNs);
	NK_FOUNDATION_LOG_INFO("  std::vector: %.2f ns total", stlTimeNs);
}

// =============================================================================
// NKContainersVectorAlias — L'ARGUMENT D'UNE INSERTION PEUT ETRE UN ELEMENT DU
// VECTEUR LUI-MEME (08/10)
// -----------------------------------------------------------------------------
// `v.PushBack(v[0])` est permis par std::vector et s'ecrit sans y penser. Quand
// l'insertion REALLOUE, la reference designe un bloc que la reallocation vient
// de liberer : la lire ensuite, c'est lire de la memoire liberee. NkSVGCodec le
// faisait pour refermer chaque polygone (`xs.PushBack(xs[cStart])`) et les
// icones au trait de NKCode sortaient coupees sous Linux, intactes sous Windows.
//
// ⚠️ POURQUOI UN ALLOCATEUR QUI EMPOISONNE. Le defaut ne se VOIT que si le tas
//    abime le bloc rendu. Celui de Windows le laisse intact : un test naif
//    (`PushBack(v[0])` puis comparer) passe vert sur la machine meme qui a ecrit
//    le defaut. L'allocateur ci-dessous ecrit 0xDD sur tout bloc rendu et le
//    garde jusqu'a sa propre destruction : la valeur lue trop tard est fausse
//    PARTOUT, de la meme facon, sans lecture hors de nos propres blocs.
//
// ⚠️ CHAQUE CAS VERIFIE D'ABORD QU'UN BLOC A ETE RENDU. Sans reallocation le cas
//    ne prouve rien : il passerait vert par construction si la croissance du
//    vecteur changeait un jour.
//
// Contre-epreuve (08/10, WSL clang 18) : NkVector.h d'avant -- Reserve() puis
// ConstructAt(mData + mSize, value) -- fait echouer les sept cas.
// =============================================================================
namespace {

	class NkAllocateurEmpoisonneur final : public memory::NkAllocator {
		public:
			using memory::NkAllocator::Deallocate; // ne masque pas Deallocate(ptr, size)

			NkAllocateurEmpoisonneur() noexcept : memory::NkAllocator("NkAllocateurEmpoisonneur") {
			}

			~NkAllocateurEmpoisonneur() override {
				for (Bloc *b = mRendus; b != nullptr;) {
					Bloc *suivant = b->suivant;
					memory::NkFree(b);
					b = suivant;
				}
			}

			Pointer Allocate(SizeType size, AlignType alignment = memory::NK_MEMORY_DEFAULT_ALIGNMENT) override {
				(void)alignment; // NkAlloc + un en-tete aligne sur 16 : assez pour tout type fondamental
				Bloc *b = static_cast<Bloc *>(memory::NkAlloc(sizeof(Bloc) + size));
				if (b == nullptr) {
					return nullptr;
				}
				b->taille = size;
				b->suivant = nullptr;
				return b + 1;
			}

			/// Rend un bloc : EMPOISONNE, et garde (le tas ne le reprend pas avant la fin).
			void Deallocate(Pointer ptr) override {
				if (ptr == nullptr) {
					return;
				}
				Bloc *b = static_cast<Bloc *>(ptr) - 1;
				memory::NkMemSet(ptr, 0xDD, b->taille);
				b->suivant = mRendus;
				mRendus = b;
				++mNbRendus;
			}

			int NbRendus() const noexcept {
				return mNbRendus;
			}

		private:
			struct alignas(16) Bloc {
					SizeType taille;
					Bloc *suivant;
			};
			Bloc *mRendus = nullptr;
			int mNbRendus = 0;
	};

	/// Type NON trivial : NkVector le deplace element par element (pas de memcpy).
	struct Jeton {
			int valeur;
			explicit Jeton(int v) noexcept : valeur(v) {
			}
			Jeton(const Jeton &o) noexcept : valeur(o.valeur) {
			}
			Jeton(Jeton &&o) noexcept : valeur(o.valeur) {
				o.valeur = -1;
			}
			~Jeton() {
			}
	};

	/// Remplit `v` jusqu'a ce que la prochaine insertion doive reallouer.
	template <typename V, typename F> void RemplirJusquAuBord(V &v, F &&fabrique) {
		int i = 0;
		do {
			v.PushBack(fabrique(i++));
		} while (v.Size() < v.Capacity() || v.Size() < 4);
		while (v.Size() < v.Capacity()) {
			v.PushBack(fabrique(i++));
		}
	}

} // namespace

TEST_CASE(NKContainersVectorAlias, PushBackDeSonElementQuiRealloue) {
	NkAllocateurEmpoisonneur tas;
	NkVector<float> v(&tas);
	RemplirJusquAuBord(v, [](int i) { return 1.5f + static_cast<float>(i); });
	const float attendu = v[0];
	const int rendus = tas.NbRendus();

	v.PushBack(v[0]);

	ASSERT_TRUE(tas.NbRendus() > rendus); // la reallocation a eu lieu
	ASSERT_TRUE(v.Back() == attendu);
	ASSERT_TRUE(v[0] == attendu);
}

TEST_CASE(NKContainersVectorAlias, PushBackDeSonElementTypeNonTrivial) {
	NkAllocateurEmpoisonneur tas;
	NkVector<Jeton> v(&tas);
	RemplirJusquAuBord(v, [](int i) { return Jeton(100 + i); });
	const int attendu = v.Back().valeur;
	const int rendus = tas.NbRendus();

	v.PushBack(v.Back());

	ASSERT_TRUE(tas.NbRendus() > rendus);
	ASSERT_EQUAL(attendu, v.Back().valeur);
	ASSERT_EQUAL(attendu, v[v.Size() - 2].valeur);
}

TEST_CASE(NKContainersVectorAlias, PushBackParDeplacementDeSonElement) {
	NkAllocateurEmpoisonneur tas;
	NkVector<Jeton> v(&tas);
	RemplirJusquAuBord(v, [](int i) { return Jeton(200 + i); });
	const int attendu = v[1].valeur;
	const int rendus = tas.NbRendus();

	v.PushBack(traits::NkMove(v[1]));

	ASSERT_TRUE(tas.NbRendus() > rendus);
	ASSERT_EQUAL(attendu, v.Back().valeur);
}

TEST_CASE(NKContainersVectorAlias, EmplaceBackDepuisSonElement) {
	NkAllocateurEmpoisonneur tas;
	NkVector<Jeton> v(&tas);
	RemplirJusquAuBord(v, [](int i) { return Jeton(300 + i); });
	const int attendu = v[0].valeur;
	const int rendus = tas.NbRendus();

	v.EmplaceBack(v[0]);

	ASSERT_TRUE(tas.NbRendus() > rendus);
	ASSERT_EQUAL(attendu, v.Back().valeur);
}

TEST_CASE(NKContainersVectorAlias, InsertDeSonElement) {
	// (a) avec croissance : l'ancien buffer est rendu pendant l'insertion
	{
		NkAllocateurEmpoisonneur tas;
		NkVector<int> v(&tas);
		RemplirJusquAuBord(v, [](int i) { return 10 * (i + 1); });
		const int attendu = v.Back();
		const int avant1 = v[1];
		const int rendus = tas.NbRendus();

		v.Insert(v.Begin() + 1, v.Back());

		ASSERT_TRUE(tas.NbRendus() > rendus);
		ASSERT_EQUAL(attendu, v[1]);
		ASSERT_EQUAL(avant1, v[2]);
		ASSERT_EQUAL(attendu, v.Back());
	}
	// (b) SANS croissance : le decalage deplace l'element designe avant qu'on le
	//     lise. Faux sur toute machine, meme avec un tas qui n'abime rien.
	{
		NkVector<int> v;
		v.Reserve(8);
		v.PushBack(10);
		v.PushBack(20);
		v.PushBack(30);

		v.Insert(v.Begin(), v[2]);

		ASSERT_EQUAL(4, static_cast<int>(v.Size()));
		ASSERT_EQUAL(30, v[0]);
		ASSERT_EQUAL(10, v[1]);
		ASSERT_EQUAL(20, v[2]);
		ASSERT_EQUAL(30, v[3]);
	}
}

TEST_CASE(NKContainersVectorAlias, ResizeAvecSonElement) {
	NkAllocateurEmpoisonneur tas;
	NkVector<float> v(&tas);
	RemplirJusquAuBord(v, [](int i) { return 0.25f * static_cast<float>(i + 1); });
	const float attendu = v[0];
	const NkVector<float>::SizeType taille = v.Size();
	const int rendus = tas.NbRendus();

	v.Resize(taille + 5, v[0]);

	ASSERT_TRUE(tas.NbRendus() > rendus);
	ASSERT_EQUAL(static_cast<int>(taille + 5), static_cast<int>(v.Size()));
	for (NkVector<float>::SizeType i = taille; i < v.Size(); ++i) {
		ASSERT_TRUE(v[i] == attendu);
	}
}

TEST_CASE(NKContainersVectorAlias, AssignAvecSonElement) {
	NkAllocateurEmpoisonneur tas;
	NkVector<int> v(&tas);
	RemplirJusquAuBord(v, [](int i) { return 7 * (i + 1); });
	const int attendu = v[1];
	const NkVector<int>::SizeType n = v.Capacity() + 3;
	const int rendus = tas.NbRendus();

	v.Assign(v[1], n);

	ASSERT_TRUE(tas.NbRendus() > rendus);
	ASSERT_EQUAL(static_cast<int>(n), static_cast<int>(v.Size()));
	for (NkVector<int>::SizeType i = 0; i < v.Size(); ++i) {
		ASSERT_EQUAL(attendu, v[i]);
	}
}
