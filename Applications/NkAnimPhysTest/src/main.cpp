// =============================================================================
// NkAnimPhysTest — tests headless de la couche physique d'animation (NkAnima M3).
// AUCUN device GPU : pur CPU, sûr à lancer même pendant un entraînement GPU.
// =============================================================================
#include "NKRenderer/Tools/Animation/NkPoseMass.h"
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

	logger.Info("=== Resultat : {0}/{1} suites OK ===", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
