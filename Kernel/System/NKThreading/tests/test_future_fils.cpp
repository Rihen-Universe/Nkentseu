// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_future_fils.cpp
//
// NkFuture / NkPromise entre deux fils (complement de test_future_seul.cpp, qui
// prouve que l'en-tete compile seul). Ici : le consommateur bloque dans Get()
// ou Wait() jusqu'a ce que l'autre fil satisfasse la promesse, et WaitFor
// attend vraiment son delai avant d'expirer.
//
// PRE-ENREGISTREMENT (21/10/2026) :
//   (t1) un fil bloque dans Get() ; le fil principal fait SetValue(99) ; le fil
//        lit 99
//   (t2) meme chose avec NkPromise<void> et Wait()
//   (t3) WaitFor(30) sur une promesse jamais satisfaite rend faux apres au moins
//        25 ms (horloge monotone de NkConditionVariable)
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKThreading/NkFuture.h"
#include "NKThreading/NkThread.h"

using namespace nkentseu;
using namespace nkentseu::threading;

TEST_CASE(NKThreadingFutureFils, T1_GetAttendLAutreFil) {
	NkPromise<int> promesse;
	NkFuture<int> futur = promesse.GetFuture();
	int lu = 0;
	NkThread consommateur([&](void *) { lu = futur.Get(); });
	promesse.SetValue(99);
	consommateur.Join();
	ASSERT_EQUAL(99, lu);
}

TEST_CASE(NKThreadingFutureFils, T2_WaitVoidAttendLAutreFil) {
	NkPromise<void> promesse;
	NkFuture<void> futur = promesse.GetFuture();
	bool vu = false;
	NkThread consommateur([&](void *) {
		futur.Wait();
		vu = futur.IsReady();
	});
	promesse.SetValue();
	consommateur.Join();
	ASSERT_TRUE(vu);
}

TEST_CASE(NKThreadingFutureFils, T3_WaitForAttendSonDelai) {
	NkPromise<int> promesse;
	NkFuture<int> futur = promesse.GetFuture();
	const nk_uint64 t0 = NkConditionVariable::GetMonotonicTimeMs();
	const nk_bool pret = futur.WaitFor(30);
	const nk_uint64 t1 = NkConditionVariable::GetMonotonicTimeMs();
	ASSERT_FALSE(pret);
	ASSERT_TRUE(t1 - t0 >= 25u);
}
