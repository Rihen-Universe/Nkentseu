// =============================================================================
// bench_sleep.cpp — Ce que `NkChrono::Sleep(n)` dort VRAIMENT.
//
// POURQUOI : `NkCameraDemos --demo=viewer` affiche un débit qui varie de 35 à
// 55 img/s sur une boucle plafonnée à 60 — 57 % d'amplitude, inexpliquée, et
// devenue le plancher de bruit d'au moins deux bancs d'essai (le mien et celui
// de l'agent NK3DModeler). On ne cherche pas ici à la résoudre : on cherche à
// SAVOIR SI LE SOMMEIL EN EST LA CAUSE, ou à l'éliminer.
//
// L'HYPOTHÈSE, et elle est écrite noir sur blanc dans le moteur :
//   `NkChrono.cpp:277` — « Granularité typique : ~15.6 ms sans timeBeginPeriod »
//   `NkRendererImpl.cpp:116` — `timeBeginPeriod(1)` … mais SEULEMENT là.
// Une application qui n'initialise pas NKRenderer — le viewer caméra tourne en
// OpenGL direct — garde donc la résolution de minuterie PAR DÉFAUT de Windows.
// Or la boucle se cale par `Sleep(16 - travail)`. Si un `Sleep(11)` dort en
// réalité 15,6 ms, la trame dure 5 + 15,6 = 20,6 ms, soit 48 img/s au lieu de 60
// — sans qu'aucune ligne de code ne paraisse fautive.
//
// CE QU'ON MESURE : la durée réelle de chaque `Sleep(n)` pour n de 1 à 16 ms.
// Deux régimes : sans, puis avec `timeBeginPeriod(1)`. La différence entre les
// deux EST la contribution de la minuterie — le reste est ailleurs.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKTime/NkChrono.h"

#include <cstdio>

#if defined(_WIN32)
extern "C" unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
extern "C" unsigned int __stdcall timeEndPeriod(unsigned int uPeriod);
#endif

using namespace nkentseu;

namespace {

	constexpr int kRepets = 40;

	// Durée réelle moyenne d'un Sleep(demandeMs), en millisecondes.
	float64 MesurerSommeil(int64 demandeMs, float64 &pire) {
		float64 total = 0.0;
		pire = 0.0;
		for (int i = 0; i < kRepets; ++i) {
			const NkElapsedTime t0 = NkChrono::Now();
			NkChrono::SleepMilliseconds(demandeMs);
			const float64 ms = (float64)(NkChrono::Now() - t0).ToMicroseconds() / 1000.0;
			total += ms;
			if (ms > pire)
				pire = ms;
		}
		return total / (float64)kRepets;
	}

	void Passe(const char *titre) {
		::printf("\n--- %s ---\n", titre);
		::printf("  demande   reel moyen   pire   surcout\n");
		for (int64 d = 1; d <= 16; d += (d < 4 ? 1 : 4)) {
			float64 pire = 0.0;
			const float64 reel = MesurerSommeil(d, pire);
			::printf("  %4lld ms   %8.2f ms  %6.2f  %+7.2f\n", (long long)d, reel, pire,
					 reel - (float64)d);
		}
	}

} // namespace

int main() {
	::printf("Sommeil reel de NkChrono::SleepMilliseconds — %d repetitions par palier.\n", kRepets);

	Passe("SANS timeBeginPeriod (ce que vit toute app SANS NKRenderer)");

#if defined(_WIN32)
	timeBeginPeriod(1);
	Passe("AVEC timeBeginPeriod(1) (ce que fait NKRenderer a son init)");
	timeEndPeriod(1);
#endif

	// Ce que la boucle du viewer subit reellement : travail + Sleep(16 - travail).
	::printf("\n--- Trame simulee : Sleep(16 - travail) ---\n");
	::printf("  travail   trame reelle   img/s\n");
	for (int64 travail = 1; travail <= 13; travail += 3) {
		float64 total = 0.0;
		for (int i = 0; i < kRepets; ++i) {
			const NkElapsedTime t0 = NkChrono::Now();
			// On occupe le processeur `travail` ms sans dormir, comme une image.
			while ((float64)(NkChrono::Now() - t0).ToMicroseconds() / 1000.0 < (float64)travail) {
			}
			NkChrono::SleepMilliseconds(16 - travail);
			total += (float64)(NkChrono::Now() - t0).ToMicroseconds() / 1000.0;
		}
		const float64 trame = total / (float64)kRepets;
		::printf("  %4lld ms   %9.2f ms   %5.1f\n", (long long)travail, trame, 1000.0 / trame);
	}

	return 0;
}
