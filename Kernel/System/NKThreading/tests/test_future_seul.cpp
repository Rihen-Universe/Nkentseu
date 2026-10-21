// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_future_seul.cpp
//
// NkFuture.h doit compiler SEUL : cette unite de traduction n'inclut que lui
// (et le cadre de test). Retour de deux apprenants (kit Canvas du 13/10/2026) :
// l'en-tete ne compilait pas. Personne dans le depot ne l'incluait, donc aucune
// construction ne le voyait.
//
// Defauts constates en lisant l'en-tete (21/10) :
//   - « NkScopedLock lock(...) » : NkScopedLock est un modele (NkScopedLock.h,
//     non inclus) et NkConditionVariable n'accepte que NkScopedLockMutex ;
//   - l'appel de memory:: « MakeShared » suivi de « Ptr » n'existe pas : seul
//     memory::NkMakeShared existe (NKMemory/NkSharedPtr.h) ;
//   - NkPromise<void> n'existait pas : le modele general declarait
//     SetValue(const void &), ce qui ne compile pas ; l'exemple de l'en-tete
//     appelle pourtant promise.SetValue() ;
//   - le « #endif » de la garde d'inclusion fermait AVANT les implementations ;
//   - mReady et mException n'avaient pas de valeur initiale ecrite.
//
// PRE-ENREGISTREMENT :
//   (f1) futur vide : IsReady() faux, WaitFor(1) faux, sans bloquer
//   (f2) promesse<int> : pas prete, WaitFor(5) expire (faux), SetValue(42),
//        prete, Get() == 42, WaitFor(0) vrai ; un second SetValue est ignore
//   (f3) SetValue(T&&) : la valeur deplacee arrive
//   (f4) promesse<void> : SetValue() sans argument, le futur devient pret
//   (f5) SetException : le futur devient pret
//   (f6) copie d'un futur : les deux voient le meme etat
// Contre-epreuve : remettre l'ancien appel (ou « NkScopedLock lock ») dans
// l'en-tete fait echouer la COMPILATION de ce fichier.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKThreading/NkFuture.h"

using namespace nkentseu;
using namespace nkentseu::threading;

TEST_CASE(NKThreadingFutureSeul, F1_FuturVide) {
	NkFuture<int> vide;
	ASSERT_FALSE(vide.IsReady());
	ASSERT_FALSE(vide.WaitFor(1));
	NkFuture<void> videV;
	ASSERT_FALSE(videV.IsReady());
	ASSERT_FALSE(videV.WaitFor(1));
}

TEST_CASE(NKThreadingFutureSeul, F2_PromesseEntier) {
	NkPromise<int> promesse;
	NkFuture<int> futur = promesse.GetFuture();
	ASSERT_FALSE(futur.IsReady());
	ASSERT_FALSE(futur.WaitFor(5));
	promesse.SetValue(42);
	ASSERT_TRUE(futur.IsReady());
	ASSERT_EQUAL(42, futur.Get());
	ASSERT_TRUE(futur.WaitFor(0));
	const int autre = 7;
	promesse.SetValue(autre); // deja satisfaite : ignore
	ASSERT_EQUAL(42, futur.Get());
}

TEST_CASE(NKThreadingFutureSeul, F3_ValeurDeplacee) {
	NkPromise<int> promesse;
	NkFuture<int> futur = promesse.GetFuture();
	int v = 1234;
	promesse.SetValue(static_cast<int &&>(v));
	futur.Wait();
	ASSERT_EQUAL(1234, futur.Get());
}

TEST_CASE(NKThreadingFutureSeul, F4_PromesseVide) {
	NkPromise<void> promesse;
	NkFuture<void> futur = promesse.GetFuture();
	ASSERT_FALSE(futur.IsReady());
	ASSERT_FALSE(futur.WaitFor(5));
	promesse.SetValue();
	ASSERT_TRUE(futur.IsReady());
	futur.Wait();
	ASSERT_TRUE(futur.WaitFor(0));
}

TEST_CASE(NKThreadingFutureSeul, F5_Exception) {
	static int temoin = 0;
	NkPromise<int> promesse;
	NkFuture<int> futur = promesse.GetFuture();
	promesse.SetException(&temoin);
	ASSERT_TRUE(futur.IsReady());

	NkPromise<void> promesseV;
	NkFuture<void> futurV = promesseV.GetFuture();
	promesseV.SetException(&temoin);
	ASSERT_TRUE(futurV.IsReady());
}

TEST_CASE(NKThreadingFutureSeul, F6_CopiePartageLEtat) {
	NkPromise<int> promesse;
	NkFuture<int> a = promesse.GetFuture();
	NkFuture<int> b = a;
	ASSERT_FALSE(b.IsReady());
	promesse.SetValue(5);
	ASSERT_TRUE(a.IsReady());
	ASSERT_TRUE(b.IsReady());
	ASSERT_EQUAL(5, b.Get());
}
