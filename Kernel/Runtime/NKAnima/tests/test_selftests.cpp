// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_selftests.cpp — LA suite du module (2026-09-04).
//
// NKAnima n'avait pas de suite alors qu'il venait de recevoir l'unification
// des squelettes, la conversion pose→monde unique et Physics/. Ses bancs
// existaient — dans l'application NkAnimaTest, qui les rejouait a la main.
// Ici, les MEMES temoins (aucun cas nouveau) deviennent des TEST_CASE que
// `jenga test --project NKAnima_Tests` compile et lance : chaque SelfTest est
// celui du module, y compris le test 0 du reciblage (conversion jugee contre
// la geometrie a la main, court-circuit si elle est fausse) que la mutation
// `world[j] = local[j]` fait rougir.
//
// Restent dans NkAnimaTest (hors module) : NkRoleContext (NKRenderer), les deux
// SelfTests NKAudio, et les trois temoins de CABLAGE de l'editeur, dont XBot.glb
// qui demande un actif.
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"

using namespace nkentseu;

// M3.1 — barycentre, pondere, monotonie, anthropometrie, gardes
TEST_CASE(NKAnima, M31_NkPoseMass) {
	ASSERT_TRUE(anim::NkPoseMass::SelfTest());
}

// M3.2 — dedans/dehors/bord, 2 pieds segment, direction de bascule
TEST_CASE(NKAnima, M32_NkBalance) {
	ASSERT_TRUE(anim::NkBalance::SelfTest());
}

// M3.3 — contact sol, points de support, INTEGRATION debout/penche
TEST_CASE(NKAnima, M33_NkContactDetector) {
	ASSERT_TRUE(anim::NkContactDetector::SelfTest());
}

// M3.4 — deseq->equilibre, strength 0/0.5/1, pose deja equilibree
TEST_CASE(NKAnima, M34_NkPoseBalancer) {
	ASSERT_TRUE(anim::NkPoseBalancer::SelfTest());
}

// M3.5 — lerp brut deseq -> BlendBalanced equilibre, pieds plantes, bornes t=0/1
TEST_CASE(NKAnima, M35_NkAutoPose) {
	ASSERT_TRUE(anim::NkAutoPose::SelfTest());
}

// Courbe : spline passe par les points, longueur/tangente droite, path-follow loop/once
TEST_CASE(NKAnima, NkMotionPath) {
	ASSERT_TRUE(anim::NkMotionCurve::SelfTest());
}

// M3.6 — clip qui bascule -> corrige frame par frame, pieds fixes, lissage borne
TEST_CASE(NKAnima, M36_NkClipBalancePass) {
	ASSERT_TRUE(anim::NkClipBalancePass::SelfTest());
}

// M2 — reciblage : test 0 = LA conversion pose->monde jugee a la main (mutation
// world[j] = local[j] -> rouge), appariement par nom, delta au repos, os non
// etires, racine a l'echelle, cycle refuse.
TEST_CASE(NKAnima, M2_NkAnimRetarget) {
	ASSERT_TRUE(anim::NkAnimRetarget::SelfTest());
}
