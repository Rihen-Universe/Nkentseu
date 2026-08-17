// =============================================================================
// test_cpp11_gardes.cpp — TÉMOINS D'INSTANCIATION des gardes NK_CPP11.
// -----------------------------------------------------------------------------
// Les 914 lignes derrière `#if defined(NK_CPP11)` ont été écrites dès l'origine
// mais JAMAIS compilées : la macro n'était définie nulle part (verrou : un
// namespace `literals` ouvert non-inline dans NkTypeUtils.h, rouvert inline
// sous la garde — cf. NKContainers/ROADMAP.md §5). Ouverture : 2026-08-17.
//
// ⚠️ POURQUOI CES TÉMOINS EXISTENT : le build complet 202/203 après ouverture
// prouve la NON-RÉGRESSION, pas la CORRECTION — la plus grande part des lignes
// ouvertes vit dans des corps de templates, et un template non instancié n'est
// vérifié que partiellement (noms non dépendants, syntaxe). Ces témoins
// INSTANCIENT les chemins ouverts avec un type move-only : un corps faux casse
// ici, à la compilation ou à l'exécution, pas chez un consommateur dans six mois.
//
// Chemins couverts (la répartition mesurée : 15 blocs move, 13 forwarding,
// 3 noexcept, 6 divers) :
//   - ctor/assign move : NkPair, NkTuple, NkUnorderedSet, NkRingBuffer
//   - Push(T&&)/Emplace variadique : NkQueue, NkStack, NkPriorityQueue
//   - forwarding : NkPool::Create, NkMakePair
//   - littéraux : _sv / _hash32 / _u8 — le VERROU lui-même : ils n'existent que
//     gardes ouvertes, et leur visibilité sans qualification prouve que les
//     namespaces `literals` sont bien inline de bout en bout.
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NKContainers.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKContainers/Heterogeneous/NkTuple.h"
#include "NKContainers/Adapters/NkQueue.h"
#include "NKContainers/Adapters/NkStack.h"
#include "NKContainers/Associative/NkPriorityQueue.h"
#include "NKContainers/Associative/NkUnorderedSet.h"
#include "NKContainers/CacheFriendly/NkRingBuffer.h"
#include "NKContainers/CacheFriendly/NkPool.h"
#include "NKContainers/String/NkStringView.h"
#include "NKContainers/String/NkStringHash.h"

using namespace nkentseu;

namespace {

	// Type MOVE-ONLY : copie supprimée. Tout chemin qui copierait au lieu de
	// déplacer devient une ERREUR DE COMPILATION — le témoin discrimine à la
	// compilation, pas seulement à l'exécution.
	struct NkMoveOnly {
			int32 valeur = 0;
			bool vivant = false;

			NkMoveOnly() = default;
			explicit NkMoveOnly(int32 v) : valeur(v), vivant(true) {
			}
			NkMoveOnly(const NkMoveOnly &) = delete;
			NkMoveOnly &operator=(const NkMoveOnly &) = delete;
			NkMoveOnly(NkMoveOnly &&other) noexcept : valeur(other.valeur), vivant(other.vivant) {
				other.vivant = false; // la source est vidée : c'est OBSERVABLE
			}
			NkMoveOnly &operator=(NkMoveOnly &&other) noexcept {
				valeur = other.valeur;
				vivant = other.vivant;
				other.vivant = false;
				return *this;
			}
	};

	// Variante copiable pour les conteneurs dont le move est membre à membre
	// (NkUnorderedSet déplace ses seaux, pas les éléments un à un).
	struct NkTraceur {
			int32 valeur = 0;
			NkTraceur() = default;
			explicit NkTraceur(int32 v) : valeur(v) {
			}
			bool operator==(const NkTraceur &o) const {
				return valeur == o.valeur;
			}
	};

} // namespace

// La branche attendue est PRISE — mesuré, pas supposé (l'instrument n°3 de la
// ROADMAP : une macro mal orthographiée compile, elle compile juste l'autre
// branche). Si NK_CPP11 disparaît un jour, ce témoin le dit le jour même.
TEST_CASE(NKCpp11Gardes, LaBrancheOuverteEstPrise) {
#if defined(NK_CPP11)
	const bool gardesOuvertes = true;
#else
	const bool gardesOuvertes = false;
#endif
	ASSERT_TRUE(gardesOuvertes);
#if defined(NKENTSEU_HAS_CPP11)
	const bool sourceDeVerite = true;
#else
	const bool sourceDeVerite = false;
#endif
	// NK_CPP11 est DÉRIVÉE de NKENTSEU_HAS_CPP11 : les deux vont ensemble.
	ASSERT_EQUAL(sourceDeVerite, gardesOuvertes);
}

// NkPair : ctor move + operator= move (2 des 15 blocs move).
TEST_CASE(NKCpp11Gardes, PairMoveCtorEtAssign) {
	NkPair<NkMoveOnly, int32> a(NkMoveOnly(7), 42);
	ASSERT_TRUE(a.First.vivant);

	NkPair<NkMoveOnly, int32> b(traits::NkMove(a)); // ctor move gardé
	ASSERT_TRUE(b.First.vivant);
	ASSERT_FALSE(a.First.vivant); // la source est vidée : le move a EU LIEU
	ASSERT_EQUAL(7, b.First.valeur);

	NkPair<NkMoveOnly, int32> c;
	c = traits::NkMove(b); // operator= move gardé
	ASSERT_TRUE(c.First.vivant);
	ASSERT_FALSE(b.First.vivant);
	ASSERT_EQUAL(42, c.Second);
}

// NkMakePair : forwarding parfait + decay (1 des 13 blocs forwarding).
TEST_CASE(NKCpp11Gardes, MakePairForwarding) {
	auto p = NkMakePair(NkMoveOnly(3), 9); // rvalue forwardée, jamais copiée
	ASSERT_TRUE(p.First.vivant);
	ASSERT_EQUAL(3, p.First.valeur);
	ASSERT_EQUAL(9, p.Second);
}

// NkTuple : ctor move.
TEST_CASE(NKCpp11Gardes, TupleMoveCtor) {
	NkTuple<NkMoveOnly, int32> t(NkMoveOnly(11), 5);
	NkTuple<NkMoveOnly, int32> u(traits::NkMove(t));
	ASSERT_TRUE(NkGet<0>(u).vivant);
	ASSERT_FALSE(NkGet<0>(t).vivant);
	ASSERT_EQUAL(11, NkGet<0>(u).valeur);
}

// NkQueue / NkStack : Push(T&&) + Emplace variadique (adapters).
TEST_CASE(NKCpp11Gardes, QueueEtStackMovePushEmplace) {
	NkQueue<NkMoveOnly> q;
	q.Push(NkMoveOnly(1)); // Push(T&&) gardé — la surcharge copie ne compilerait pas
	q.Emplace(2);		   // Emplace(Args&&...) gardé
	ASSERT_EQUAL(2, static_cast<int>(q.Size()));
	ASSERT_EQUAL(1, q.Front().valeur);

	NkStack<NkMoveOnly> s;
	s.Push(NkMoveOnly(3));
	s.Emplace(4);
	ASSERT_EQUAL(2, static_cast<int>(s.Size()));
	ASSERT_EQUAL(4, s.Top().valeur);
}

// NkPriorityQueue : Push(T&&) + Emplace.
// ⚠️ Signature : NkPriorityQueue<T, Allocator, Compare> — l'ALLOCATEUR est le
// 2e paramètre, pas le conteneur. Le premier jet de ce témoin y avait mis un
// NkVector : la cascade d'erreurs dans NkVector.h qui a suivi ressemblait à
// des corps gardés faux, et n'était que l'instanciation d'un allocateur
// impossible. Vérifier l'appelant avant d'accuser le corps.
TEST_CASE(NKCpp11Gardes, PriorityQueueMovePush) {
	struct Cmp {
			bool operator()(const NkMoveOnly &a, const NkMoveOnly &b) const {
				return a.valeur < b.valeur;
			}
	};
	NkPriorityQueue<NkMoveOnly, memory::NkAllocator, Cmp> pq;
	pq.Push(NkMoveOnly(2));
	pq.Emplace(8);
	pq.Emplace(5);
	ASSERT_EQUAL(3, static_cast<int>(pq.Size()));
	ASSERT_EQUAL(8, pq.Top().valeur); // max-heap avec ce comparateur
}

// NkUnorderedSet : ctor move du CONTENEUR (déplace les seaux).
TEST_CASE(NKCpp11Gardes, UnorderedSetMoveCtor) {
	NkUnorderedSet<NkTraceur> a;
	a.Insert(NkTraceur(1));
	a.Insert(NkTraceur(2));
	NkUnorderedSet<NkTraceur> b(traits::NkMove(a));
	ASSERT_EQUAL(2, static_cast<int>(b.Size()));
	ASSERT_TRUE(b.Contains(NkTraceur(1)));
}

// NkRingBuffer : ctor move du conteneur (l'API dit Push, pas PushBack).
TEST_CASE(NKCpp11Gardes, RingBufferMoveCtor) {
	NkRingBuffer<int32> a(8);
	a.Push(10);
	a.Push(20);
	NkRingBuffer<int32> b(traits::NkMove(a));
	ASSERT_EQUAL(2, static_cast<int>(b.Size()));
	ASSERT_EQUAL(10, b.Front());
}

// NkPool : Construct avec forwarding parfait (placement new gardé).
TEST_CASE(NKCpp11Gardes, PoolConstructForwarding) {
	NkPool<NkMoveOnly> pool(4);
	NkMoveOnly *obj = pool.Construct(21);
	ASSERT_NOT_NULL(obj);
	ASSERT_EQUAL(21, obj->valeur);
	ASSERT_TRUE(obj->vivant);
	pool.Destroy(obj);
}

// ⭐ LES LITTÉRAUX — le verrou lui-même. Ces suffixes n'existent que gardes
// ouvertes, et leur visibilité SANS qualifier `literals` prouve que les trois
// namespaces sont inline de bout en bout (NkTypeUtils corrigé + NkStringView
// + NkStringHash). C'est le témoin direct de la correction du conflit.
TEST_CASE(NKCpp11Gardes, LitterauxVisiblesSansQualification) {
	// _sv : nkentseu::literals (inline) — via `using namespace nkentseu`.
	NkStringView sv = "nkentseu"_sv;
	ASSERT_EQUAL(8, static_cast<int>(sv.Size()));

	// _u8 : même namespace nkentseu::literals que _sv — le CONFLIT d'origine.
	// Les deux suffixes coexistent dans la même TU : c'est ça, la preuve.
	nk_uint8 u = 200_u8;
	ASSERT_EQUAL(200, static_cast<int>(u));

	// _hash32 : nkentseu::string::literals (inline).
	using namespace nkentseu::string;
	constexpr uint32 h = "nkentseu"_hash32;
	// Valeur de référence FNV-1a 32 calculée HORS du code testé (Python) :
	// comparer le littéral à sa propre fonction ne discriminerait rien.
	ASSERT_EQUAL(0x44EBF016u, h);
}
