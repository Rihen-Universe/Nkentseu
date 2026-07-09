// =============================================================================
// NkAnimPhysTest — tests headless de la couche physique d'animation (NkAnima M3).
// AUCUN device GPU : pur CPU, sûr à lancer même pendant un entraînement GPU.
// =============================================================================
#include "NKRenderer/Tools/Animation/NkPoseMass.h"
#include "NKRenderer/Tools/Animation/NkBalance.h"
#include "NKRenderer/Tools/Animation/NkContactDetector.h"
#include "NKRenderer/Tools/Animation/NkPoseBalancer.h"
#include "NKAudio/NkAudioCapture.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

int main() {
	logger.Info("=== NkAnimPhysTest — physique d'animation NkAnima (headless, sans GPU) ===");

	int nbOk = 0;
	int nbTotal = 0;

	// M3.1 — distribution de masse + centre de masse (COM).
	{
		++nbTotal;
		const bool ok = renderer::NkPoseMass::SelfTest();
		logger.Info("[{0}] M3.1 NkPoseMass::SelfTest (barycentre, pondere, monotonie, anthropometrie, gardes)",
					ok ? " OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// M3.2 — solveur d'équilibre (polygone de support + COM dedans ? + marge).
	{
		++nbTotal;
		const bool ok = renderer::NkBalance::SelfTest();
		logger.Info("[{0}] M3.2 NkBalance::SelfTest (dedans/dehors/bord, 2 pieds segment, direction de bascule)",
					ok ? " OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// M3.3 — solveur de contacts (+ intégration M3.1+M3.2+M3.3 : debout=équilibré, penché=non).
	{
		++nbTotal;
		const bool ok = renderer::NkContactDetector::SelfTest();
		logger.Info("[{0}] M3.3 NkContactDetector::SelfTest (contact sol, points de support, INTEGRATION debout/penche)",
					ok ? " OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// M3.4 — optimiseur de pose sous contrainte (ajuste une pose pour respecter l'équilibre).
	{
		++nbTotal;
		const bool ok = renderer::NkPoseBalancer::SelfTest();
		logger.Info("[{0}] M3.4 NkPoseBalancer::SelfTest (deseq->equilibre, strength 0/0.5/1, pose deja equilibree)",
					ok ? " OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	// NKAudio — enregistrement : ring buffer SPSC de la capture (headless, sans micro).
	{
		++nbTotal;
		const bool ok = audio::NkAudioCapture::SelfTest();
		logger.Info("[{0}] NKAudio NkAudioCapture::SelfTest (ring buffer SPSC : write/read/wrap/overflow)",
					ok ? " OK " : "FAIL");
		if (ok)
			++nbOk;
	}

	logger.Info("=== Resultat : {0}/{1} suites OK ===", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
