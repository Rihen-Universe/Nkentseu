// =============================================================================
// NKSpeechTest — tests headless du module de parole NKSpeech (NKAI).
// AUCUN device GPU : pur CPU. Sortie via printf (sortie directe console).
// =============================================================================
#include "NKSpeech/NkAudioFeatures.h"
#include "NKSpeech/NkGriffinLim.h"

#include <cstdio>

using namespace nkentseu;

int main() {
	printf("=== NKSpeechTest — parole from-scratch (NKAI, headless) ===\n\n");

	int nbOk = 0, nbTotal = 0;

	{
		++nbTotal;
		const bool ok = ai::NkAudioFeatures::SelfTest();
		printf("[ %s ] NkAudioFeatures : MFCC/log-Mel (sinus 1kHz -> bon canal Mel, deterministe, silence fini)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	{
		++nbTotal;
		const bool ok = ai::NkGriffinLim::SelfTest();
		printf("[ %s ] NkGriffinLim : vocodeur (spectrogramme magnitude -> onde, phase iterative ; "
			   "magnitude reconstruite fidele + energie preservee)\n",
			   ok ? "OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
