// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_entete_parapluie.cpp
//
// NKThreading/NKThreading.h, l'en-tete « un seul include pour tout le module » (sa propre documentation),
// compile SEUL et s'emploie. Cette unite de traduction n'inclut QUE lui (et le cadre de test).
//
// POURQUOI (22/10/2026) : l'epreuve des kits du 21/10 (un programme qui inclut l'en-tete de la bibliotheque)
// l'a trouve casse sur toutes les plateformes : platform::NkGetLogicalCoreCount sans NkCPUFeatures.h, un
// alias vers threading::NkSpinLock (inexistant), et, sous Windows, la macro « Yield() » de winbase.h qui
// effacait threading::Yield(). Personne dans le depot ne l'incluait.
//
// PRE-ENREGISTREMENT :
//   (p1) GetVersionString() non vide ; GetHardwareConcurrency() >= 1 ;
//   (p2) threading::Yield(), nkentseu::Yield() et Pause() s'appellent ;
//   (p3) NkMutex + NkScopedLockMutex, memory::NkSpinLock : verrouiller et rendre ;
//   (p4) un NkThread incremente un compteur protege, rejoint ; NkPromise<int> / NkFuture<int> : 42 arrive ;
//   (p5) memory::NkSpinLock sous contention (deux fils) : le compte est juste.
// Contre-epreuve : retirer l'inclusion de NkCPUFeatures.h, ou remettre l'alias threading::NkSpinLock, ou le
// « #undef Yield » : ce fichier ne compile plus ; retirer NkSpinLock::LockSlow (NkSpinLock.cpp) : il ne se
// lie plus.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKThreading/NKThreading.h"

using namespace nkentseu;

TEST_CASE(NKThreadingEnteteParapluie, P1_VersionEtCoeurs) {
	const nk_char *v = threading::GetVersionString();
	ASSERT_TRUE(v != nullptr && v[0] != '\0');
	ASSERT_TRUE(threading::GetHardwareConcurrency() >= 1u);
	ASSERT_TRUE(GetHardwareConcurrency() >= 1u);
}

TEST_CASE(NKThreadingEnteteParapluie, P2_YieldEtPause) {
	threading::Yield();
	Yield();
	threading::Pause();
	Pause();
	ASSERT_TRUE(true);
}

TEST_CASE(NKThreadingEnteteParapluie, P3_Verrous) {
	threading::NkMutex m;
	{
		threading::NkScopedLockMutex garde(m);
	}
	memory::NkSpinLock s;
	s.Lock();
	s.Unlock();
	ASSERT_TRUE(true);
}

TEST_CASE(NKThreadingEnteteParapluie, P4_FilEtFutur) {
	threading::NkMutex m;
	int compteur = 0;
	threading::NkThread fil([&](void *) {
		for (int i = 0; i < 1000; ++i) {
			threading::NkScopedLockMutex garde(m);
			++compteur;
		}
	});
	fil.Join();
	ASSERT_EQUAL(1000, compteur);

	threading::NkPromise<int> promesse;
	threading::NkFuture<int> futur = promesse.GetFuture();
	promesse.SetValue(42);
	ASSERT_EQUAL(42, futur.Get());
}

// (p5, ajoute le 22/10) memory::NkSpinLock SOUS CONTENTION : deux fils, 100 000 increments chacun. Le
// chemin lent (LockSlow) n'etait defini nulle part : ce test ne se liait pas. Il doit rendre 200 000.
TEST_CASE(NKThreadingEnteteParapluie, P5_SpinLockSousContention) {
	memory::NkSpinLock verrou;
	int total = 0;
	auto travail = [&](void *) {
		for (int i = 0; i < 100000; ++i) {
			verrou.Lock();
			++total;
			verrou.Unlock();
		}
	};
	threading::NkThread a(travail);
	threading::NkThread b(travail);
	a.Join();
	b.Join();
	ASSERT_EQUAL(200000, total);
}
