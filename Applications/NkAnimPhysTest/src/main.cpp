// =============================================================================
// NkAnimPhysTest — tests headless de la couche physique d'animation (NkAnima M3).
// AUCUN device GPU : pur CPU, sûr à lancer même pendant un entraînement GPU.
// Sortie via printf (sortie directe console, comme NKMeshAITest).
// =============================================================================
#include "NKAnimPhysics/NkPoseMass.h"
#include "NKAnimPhysics/NkBalance.h"
#include "NKAnimPhysics/NkContactDetector.h"
#include "NKAnimPhysics/NkPoseBalancer.h"
#include "NKAnimPhysics/NkAutoPose.h"
#include "NKAnimation/NkMotionPath.h"
#include "NKAnimPhysics/NkClipBalancePass.h"
#include "NKAnimation/NkAnimRetarget.h"
#include "NKRenderer/Tools/Director/NkRoleContext.h"
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
		   animphys::NkPoseMass::SelfTest(), nbOk, nbTotal);

	// M3.2 — solveur d'équilibre (polygone de support + COM dedans ? + marge).
	Report("M3.2 NkBalance", "dedans/dehors/bord, 2 pieds segment, direction de bascule",
		   animphys::NkBalance::SelfTest(), nbOk, nbTotal);

	// M3.3 — solveur de contacts (+ intégration M3.1+M3.2+M3.3 : debout=équilibré, penché=non).
	Report("M3.3 NkContactDetector", "contact sol, points de support, INTEGRATION debout/penche",
		   animphys::NkContactDetector::SelfTest(), nbOk, nbTotal);

	// M3.4 — optimiseur de pose sous contrainte (ajuste une pose pour respecter l'équilibre).
	Report("M3.4 NkPoseBalancer", "deseq->equilibre, strength 0/0.5/1, pose deja equilibree",
		   animphys::NkPoseBalancer::SelfTest(), nbOk, nbTotal);

	// M3.5 — auto-posing (poses intermediaires equilibrees entre deux cles).
	Report("M3.5 NkAutoPose", "lerp brut deseq -> BlendBalanced equilibre, pieds plantes, bornes t=0/1",
		   animphys::NkAutoPose::SelfTest(), nbOk, nbTotal);

	// Animation par courbe : spline Catmull-Rom + path-follow (os/effecteur IK suit la courbe).
	Report("NkMotionPath", "spline passe par les points, longueur/tangente droite, path-follow loop/once",
		   anim::NkMotionCurve::SelfTest(), nbOk, nbTotal);

	// M3.6 — pont vers l'anim existante : correction physique non destructive d'un clip + lissage.
	Report("M3.6 NkClipBalancePass", "clip qui bascule -> corrige frame par frame (equilibre), pieds fixes, lissage borne",
		   animphys::NkClipBalancePass::SelfTest(), nbOk, nbTotal);

	// M4bis.1 — contexte de role (personnage/personnalite/emotion/objectif/historique),
	// round-trip Archive+JSON, schema strict rejette les variantes malformees (anti texte-libre).
	Report("M4bis.1 NkRoleContext", "FIFO historique, round-trip Archive+JSON, schema accepte/rejette",
		   renderer::NkRoleContext::SelfTest(), nbOk, nbTotal);

	// NKAudio — enregistrement : ring buffer SPSC de la capture (headless, sans micro).
	Report("NKAudio NkAudioCapture", "ring buffer SPSC : write/read/wrap/overflow",
		   audio::NkAudioCapture::SelfTest(), nbOk, nbTotal);

	// NKAudio — débruitage + normalisation (soustraction spectrale + gate + auto-gain).
	Report("NKAudio NkDenoiser", "soustraction spectrale (plancher bruit chute), sinus survit, normalisation",
		   audio::NkDenoiser::SelfTest(), nbOk, nbTotal);

	// M2 — RECIBLAGE d'animation entre squelettes (brique explicitement notee
	// « reellement non commencee » dans la roadmap NkAnima).
	Report("M2 NkAnimRetarget", "appariement par nom, delta au repos, os non etires, racine a l'echelle",
		   anim::NkAnimRetarget::SelfTest(), nbOk, nbTotal);

	printf("\n=== Resultat : %d/%d suites OK ===\n", nbOk, nbTotal);
	return (nbOk == nbTotal) ? 0 : 1;
}
