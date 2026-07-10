// =============================================================================
// NkAnimPhysTest — tests headless de la couche physique d'animation (NkAnima M3).
// AUCUN device GPU : pur CPU, sûr à lancer même pendant un entraînement GPU.
// Sortie via printf (sortie directe console, comme NKMeshAITest).
// =============================================================================
#include "NKRenderer/Tools/Animation/NkPoseMass.h"
#include "NKRenderer/Tools/Animation/NkBalance.h"
#include "NKRenderer/Tools/Animation/NkContactDetector.h"
#include "NKRenderer/Tools/Animation/NkPoseBalancer.h"
#include "NKRenderer/Tools/Animation/NkAutoPose.h"
#include "NKAudio/NkAudioCapture.h"
#include "NKAudio/NkDenoiser.h"

#include <cstdio>

using namespace nkentseu;

namespace {
	void Report(const char *tag, const char *desc, bool ok, int &nbOk, int &nbTotal) {
		++nbTotal;
		if (ok)
			++nbOk;
		printf("[ %s ] %s : %s\n", ok ? "OK " : "FAIL", tag, desc);
	}
} // namespace

int main() {
	printf("=== NkAnimPhysTest — physique d'animation NkAnima (headless, sans GPU) ===\n\n");

	int nbOk = 0;
	int nbTotal = 0;

	// M3.1 — distribution de masse + centre de masse (COM).
	Report("M3.1 NkPoseMass", "barycentre, pondere, monotonie, anthropometrie, gardes",
		   renderer::NkPoseMass::SelfTest(), nbOk, nbTotal);

	// M3.2 — solveur d'équilibre (polygone de support + COM dedans ? + marge).
	Report("M3.2 NkBalance", "dedans/dehors/bord, 2 pieds segment, direction de bascule",
		   renderer::NkBalance::SelfTest(), nbOk, nbTotal);

	// M3.3 — solveur de contacts (+ intégration M3.1+M3.2+M3.3 : debout=équilibré, penché=non).
	Report("M3.3 NkContactDetector", "contact sol, points de support, INTEGRATION debout/penche",
		   renderer::NkContactDetector::SelfTest(), nbOk, nbTotal);

	// M3.4 — optimiseur de pose sous contrainte (ajuste une pose pour respecter l'équilibre).
	Report("M3.4 NkPoseBalancer", "deseq->equilibre, strength 0/0.5/1, pose deja equilibree",
		   renderer::NkPoseBalancer::SelfTest(), nbOk, nbTotal);

	// M3.5 — auto-posing (poses intermediaires equilibrees entre deux cles).
	Report("M3.5 NkAutoPose", "lerp brut deseq -> BlendBalanced equilibre, pieds plantes, bornes t=0/1",
		   renderer::NkAutoPose::SelfTest(), nbOk, nbTotal);

	// NKAudio — enregistrement : ring buffer SPSC de la capture (headless, sans micro).
	Report("NKAudio NkAudioCapture", "ring buffer SPSC : write/read/wrap/overflow",
		   audio::NkAudioCapture::SelfTest(), nbOk, nbTotal);

	// NKAudio — débruitage + normalisation (soustraction spectrale + gate + auto-gain).
	Report("NKAudio NkDenoiser", "soustraction spectrale (plancher bruit chute), sinus survit, normalisation",
		   audio::NkDenoiser::SelfTest(), nbOk, nbTotal);

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
