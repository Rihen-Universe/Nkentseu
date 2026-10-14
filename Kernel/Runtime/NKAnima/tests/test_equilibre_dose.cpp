// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_equilibre_dose.cpp — L'EQUILIBRE SE DOSE ET SE COUPE
// (Realtime/NkLiveBalance.h ; banc du 13/10/2026).
//
// D'ou il vient : le patient virtuel de PV3DE, debout sur une jambe, doit porter son
// bassin au-dessus du pied d'appui. Rodolf (13/10) : « je veux le realiste avec
// possibilite d'etre libre dans nkanima pour le cas de certains types
// d'animations. » NkLiveBalance le fait deja ; ce banc FIXE ce qui rend l'outil
// libre : il ne s'impose a personne. Son dosage (settings.weight, de 0 a 1) se
// regle avant chaque appel ; a 0 la pose n'est pas touchee, AU BIT PRES -- une
// danse, une chute, un geste stylise le laissent a zero. Et ses valeurs par defaut,
// celles des autres utilisateurs, ne changent pas.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   l'equilibre ramene le centre de masse sur l'appui      balance_off   (BL1)
//
//   NK_RT_MUTATION=balance_off ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;
using namespace nkentseu::anim::rt;
using math::NkMat4f;
using math::NkVec3f;

namespace {

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "balance_off") != nullptr) ? (uint32)NK_RT_MUT_BALANCE_OFF : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

} // namespace

// -----------------------------------------------------------------------------
// BL1 — DEBOUT SUR UN PIED, L'EQUILIBRE SE DOSE. Le personnage d'essai, debout ; le
// seul appui est son pied droit (quatre points autour de la semelle). Son centre de
// masse, entre les deux pieds, est HORS de cet appui. A poids 1 le bassin glisse
// vers le pied d'appui jusqu'a la marge voulue ; a poids 0,5, de la moitie ; a poids
// 0, la pose n'est pas touchee, au bit pres. Les reglages par defaut sont ceux
// d'avant (poids 1, marge 3 cm, 15 cm au plus).         [mutation : balance_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, BL1_Debout_sur_un_pied_l_equilibre_se_dose_et_se_coupe) {
	Mutations _m;
	NkRtTestHumanoid h;
	NkRtMakeTestHumanoid(h, false);
	NkRtRig rig;
	ASSERT_TRUE(rig.Build(h.parents.Data(), (uint32)h.parents.Size(), h.names.Data()));
	const uint32 n = rig.Count();
	NkVector<NkMat4f> local, repos, monde;
	local.Resize(n);
	repos.Resize(n);
	monde.Resize(n);
	for (uint32 j = 0; j < n; ++j) {
		local[j] = h.bindLocal[j];
	}
	rig.LocalToWorld(local.Data(), repos.Data());
	// L'appui : la semelle du pied droit, du talon aux orteils, 9 cm de large.
	const NkVec3f cheville = Origin(repos[(uint32)h.footR]), orteil = Origin(repos[(uint32)h.toeR]);
	NkVec3f devant = Sub(orteil, cheville);
	devant.y = 0.f;
	devant = Normalize(devant, V3(0.f, 0.f, 1.f));
	const NkVec3f cote = Cross(V3(0.f, 1.f, 0.f), devant);
	const NkVec3f talon = Madd(cheville, devant, -0.06f), bout = Madd(cheville, devant, 0.18f);
	const NkVec3f appui[4] = {Madd(talon, cote, 0.045f), Madd(talon, cote, -0.045f), Madd(bout, cote, 0.045f), Madd(bout, cote, -0.045f)};

	NkLiveBalance defaut;
	ASSERT_TRUE_MSG(defaut.settings.weight == 1.f && defaut.settings.targetMargin == 0.03f && defaut.settings.maxShift == 0.15f,
					"les reglages par defaut sont ceux d'avant : poids 1, marge 3 cm, 15 cm au plus");

	const uint32 jambe[1] = {(uint32)h.footR}, genou[1] = {(uint32)h.shinR};
	NkVec3f glissement[3], centreDeMasse = V3(0.f, 0.f, 0.f);
	float32 marge[3] = {0.f, 0.f, 0.f}, margeAvant = 0.f;
	bool intacte = false;
	const float32 poids[3] = {0.f, 0.5f, 1.f};
	for (int32 k = 0; k < 3; ++k) {
		NkLiveBalance b;
		ASSERT_TRUE(b.Setup(rig, h.hips));
		b.SetPinnedLegs(rig, jambe, genou, 1u);
		b.settings.weight = poids[k];
		for (uint32 j = 0; j < n; ++j) {
			monde[j] = repos[j];
		}
		marge[k] = b.Apply(rig, local.Data(), monde.Data(), appui, 4u, 1.f / 60.f);
		glissement[k] = b.Shift();
		if (k == 0) {
			margeAvant = b.MarginBefore();
			centreDeMasse = b.Com();
			intacte = std::memcmp(monde.Data(), repos.Data(), sizeof(NkMat4f) * n) == 0;
		}
	}
	// Le bassin glisse du centre de masse vers le CENTRE de l'appui (le milieu de la semelle).
	const NkVec3f milieu = Scale(Add(Add(appui[0], appui[1]), Add(appui[2], appui[3])), 0.25f);
	const NkVec3f versAppui = Normalize(V3(milieu.x - centreDeMasse.x, 0.f, milieu.z - centreDeMasse.z), V3(1.f, 0.f, 0.f));
	const float32 entier = Length(glissement[2]), moitie = Length(glissement[1]);
	std::printf("  [BL1] debout sur le pied droit : marge du centre de masse %.3f m avant ; a poids 1 le bassin glisse de %.3f m (marge predite "
				"%.3f m), a poids 0,5 de %.3f m, a poids 0 de %.3f m\n",
				(double)margeAvant, (double)entier, (double)marge[2], (double)moitie, (double)Length(glissement[0]));
	ASSERT_TRUE_MSG(margeAvant < 0.f, "le cas est desequilibre au depart : le centre de masse est hors du pied d'appui");
	ASSERT_TRUE_MSG(intacte && Length(glissement[0]) == 0.f, "a poids 0, la pose n'est pas touchee, au bit pres : l'outil ne s'impose pas");
	ASSERT_TRUE_MSG(std::fabs(marge[0] - margeAvant) < 1e-6f, "a poids 0, la marge reste celle d'avant");
	ASSERT_TRUE_MSG(entier > 0.01f && Dot(glissement[2], versAppui) > 0.999f * entier, "a poids 1, le bassin glisse VERS le milieu du pied d'appui");
	ASSERT_TRUE_MSG(marge[2] >= 0.03f - 1e-3f || entier >= 0.15f - 1e-4f, "a poids 1, la marge voulue est atteinte (ou le glissement est a sa borne)");
	ASSERT_TRUE_MSG(std::fabs(moitie - 0.5f * entier) < 1e-5f, "a poids 0,5, le bassin glisse de la moitie");
	ASSERT_TRUE_MSG(entier <= 0.15f + 1e-5f, "le glissement ne depasse pas sa borne");
}
