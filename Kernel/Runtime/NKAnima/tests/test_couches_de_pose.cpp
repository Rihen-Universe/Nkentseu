// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_couches_de_pose.cpp — LE BANC DE LA PILE DE COUCHES DE POSES
// (Blend/NkPoseLayers.h, 13/10/2026).
//
// D'ou il vient : le patient virtuel de PV3DE fabrique ses gestes (une image d'un
// mouvement de l'artiste, un poing) et les melangeait par des appels epars. La pile
// range ces poses en couches : l'ordre est la priorite, chaque couche a son poids et
// un poids par os.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   une couche pese ce que dit son poids            layers_no_weight   (CP1, CP2, CP3)
//
//   NK_RT_MUTATION=layers_no_weight ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
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

namespace {

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "layers_no_weight") != nullptr) ? (uint32)NK_RT_MUT_LAYERS_NO_WEIGHT : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	NkQuatf Tour(float32 x, float32 y, float32 z, float32 angle) {
		const NkRtQuat q = QFromAxisAngle(Normalize(V3(x, y, z), V3(0.f, 1.f, 0.f)), angle);
		return NkQuatf(q.x, q.y, q.z, q.w);
	}
	/// L'angle (radians) entre deux rotations.
	float32 Ecart(const NkQuatf &a, const NkQuatf &b) {
		return Length(QToRotVec(QMul(QConj(Q(a.x, a.y, a.z, a.w)), Q(b.x, b.y, b.z, b.w))));
	}
	bool Pareil(const NkQuatf &a, const NkQuatf &b) {
		return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
	}
	/// Une pose de quatre os, chacun tourne a sa facon (`graine` change les angles).
	NkAnimPose Pose(float32 graine) {
		NkAnimPose p;
		p.bones.Resize(4);
		for (int32 j = 0; j < 4; ++j) {
			p.bones[(usize)j].r = Tour(1.f + (float32)j, graine, 0.5f - (float32)j, 0.3f * graine + 0.2f * (float32)j);
			p.bones[(usize)j].t = {0.1f * (float32)j, graine, 0.f};
		}
		return p;
	}

} // namespace

// -----------------------------------------------------------------------------
// CP1 — UNE COUCHE DE REMPLACEMENT PESE CE QUE DIT SON POIDS. A poids 0 la pose est
// celle de la base, au bit pres ; a poids 1, celle de la couche ; a poids 0,4, la
// meme que NkBlendPose (au bit pres : la pile n'a pas d'autre melange que lui). Un
// poids par os : les os a 0 gardent la base, au bit pres.
//                                                    [mutation : layers_no_weight]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CP1_Une_couche_pese_ce_que_dit_son_poids) {
	Mutations _m;
	const NkAnimPose base = Pose(1.f), cible = Pose(2.2f);
	for (float32 w : {0.f, 0.4f, 1.f}) {
		NkPoseLayerStack pile;
		pile.Add("geste", NkPoseLayerMode::NK_POSE_OVERRIDE, w).pose = cible;
		NkAnimPose out, attendu = base;
		pile.Evaluate(base, out);
		NkBlendPose(attendu, cible, w);
		bool pareil = out.bones.Size() == 4u;
		float32 versBase = 0.f, versCible = 0.f;
		for (usize j = 0; j < 4 && pareil; ++j) {
			pareil = Pareil(out.bones[j].r, attendu.bones[j].r) && out.bones[j].t.x == attendu.bones[j].t.x;
			versBase = std::fmax(versBase, Ecart(out.bones[j].r, base.bones[j].r));
			versCible = std::fmax(versCible, Ecart(out.bones[j].r, cible.bones[j].r));
		}
		std::printf("  [CP1] poids %.1f : a %.3f rad de la base, a %.3f rad de la couche ; pareil que NkBlendPose : %s\n", (double)w, (double)versBase,
					(double)versCible, pareil ? "oui" : "NON");
		ASSERT_TRUE_MSG(pareil, "la pile melange par NkBlendPose, au bit pres");
		if (w == 0.f) {
			// Au BIT pres (l'angle entre un quaternion et lui-meme n'est pas exactement 0 :
			// l'instrument `Ecart` ne peut pas le dire, la comparaison des quatre nombres si).
			bool intacte = true;
			for (usize j = 0; j < 4; ++j) {
				intacte = intacte && Pareil(out.bones[j].r, base.bones[j].r) && out.bones[j].t.x == base.bones[j].t.x && out.bones[j].t.y == base.bones[j].t.y;
			}
			ASSERT_TRUE_MSG(intacte, "a poids 0, la couche ne touche a rien, au bit pres");
		}
		if (w == 1.f) {
			ASSERT_TRUE_MSG(versCible < 1e-3f, "a poids 1, la pose est celle de la couche");
		}
	}
	// Un poids par os : les os 0 et 3 gardent la base.
	NkPoseLayerStack pile;
	NkPoseLayer &l = pile.Add("bras droit seul");
	l.pose = cible;
	for (float32 w : {0.f, 1.f, 1.f, 0.f}) {
		l.boneWeights.PushBack(w);
	}
	NkAnimPose out;
	pile.Evaluate(base, out);
	ASSERT_TRUE_MSG(Pareil(out.bones[0].r, base.bones[0].r) && Pareil(out.bones[3].r, base.bones[3].r), "un os au poids 0 garde la base, au bit pres");
	ASSERT_TRUE_MSG(Ecart(out.bones[1].r, cible.bones[1].r) < 1e-3f && Ecart(out.bones[2].r, cible.bones[2].r) < 1e-3f, "un os au poids 1 prend la couche");
}

// -----------------------------------------------------------------------------
// CP2 — L'ORDRE EST LA PRIORITE. Deux couches de remplacement sur les memes os : a
// poids 1 la DERNIERE l'emporte ; a poids 0,5 elle laisse la moitie de la premiere.
// « Qui tient cet os ? » : les parts de la base et des couches font 1.
//                                                    [mutation : layers_no_weight]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CP2_L_ordre_des_couches_est_la_priorite) {
	Mutations _m;
	const NkAnimPose base = Pose(1.f), a = Pose(2.2f), b = Pose(-1.4f);
	{
		NkPoseLayerStack pile;
		pile.Add("premiere").pose = a;
		pile.Add("derniere").pose = b;
		NkAnimPose out;
		pile.Evaluate(base, out);
		float32 pire = 0.f;
		for (usize j = 0; j < 4; ++j) {
			pire = std::fmax(pire, Ecart(out.bones[j].r, b.bones[j].r));
		}
		std::printf("  [CP2] deux couches a poids 1 : la pose est a %.2e rad de la DERNIERE\n", (double)pire);
		ASSERT_TRUE_MSG(pire < 1e-3f, "a poids 1, la derniere couche l'emporte");
	}
	{
		NkPoseLayerStack pile;
		pile.Add("premiere", NkPoseLayerMode::NK_POSE_OVERRIDE, 0.8f).pose = a;
		pile.Add("derniere", NkPoseLayerMode::NK_POSE_OVERRIDE, 0.5f).pose = b;
		NkAnimPose out, attendu = base;
		pile.Evaluate(base, out);
		NkBlendPose(attendu, a, 0.8f);
		NkBlendPose(attendu, b, 0.5f);
		bool pareil = true;
		for (usize j = 0; j < 4; ++j) {
			pareil = pareil && Pareil(out.bones[j].r, attendu.bones[j].r);
		}
		NkVector<float32> parts;
		float32 partBase = 0.f;
		pile.Contributions(1u, parts, partBase);
		std::printf("  [CP2] poids 0,8 puis 0,5 : la base tient %.2f de l'os, la premiere %.2f, la derniere %.2f (somme %.3f)\n", (double)partBase,
					(double)parts[0], (double)parts[1], (double)(partBase + parts[0] + parts[1]));
		ASSERT_TRUE_MSG(pareil, "deux couches : la base, puis la premiere, puis la derniere (NkBlendPose deux fois)");
		ASSERT_TRUE_MSG(std::fabs(partBase - 0.1f) < 1e-6f && std::fabs(parts[0] - 0.4f) < 1e-6f && std::fabs(parts[1] - 0.5f) < 1e-6f,
						"la derniere tient 0,5, la premiere 0,8 du reste, la base le reste");
		ASSERT_TRUE_MSG(std::fabs(partBase + parts[0] + parts[1] - 1.f) < 1e-6f, "les parts font 1");
	}
}

// -----------------------------------------------------------------------------
// CP3 — UNE COUCHE ADDITIVE AJOUTE SON ECART sans rien retirer aux autres : la pose
// est celle de NkApplyAdditive (au bit pres) ; l'ecart d'un geste (NkMakeAdditive)
// rajoute a sa propre reference rend le geste.
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CP3_Une_couche_additive_ajoute_son_ecart) {
	Mutations _m;
	const NkAnimPose base = Pose(1.f), geste = Pose(2.2f), autre = Pose(-0.6f);
	NkAnimPose ecart;
	NkMakeAdditive(geste, base, ecart);
	NkPoseLayerStack pile;
	pile.Add("ecart", NkPoseLayerMode::NK_POSE_ADDITIVE, 1.f).pose = ecart;
	NkAnimPose out;
	pile.Evaluate(base, out);
	float32 pire = 0.f;
	for (usize j = 0; j < 4; ++j) {
		pire = std::fmax(pire, Ecart(out.bones[j].r, geste.bones[j].r));
	}
	std::printf("  [CP3] l'ecart d'un geste rajoute a sa reference : a %.2e rad du geste\n", (double)pire);
	ASSERT_TRUE_MSG(pire < 1e-3f, "la reference plus l'ecart rend le geste");
	// Sur une autre base, a poids 0,5 : NkApplyAdditive, au bit pres.
	NkPoseLayerStack p2;
	p2.Add("ecart", NkPoseLayerMode::NK_POSE_ADDITIVE, 0.5f).pose = ecart;
	NkAnimPose o2, attendu = autre;
	p2.Evaluate(autre, o2);
	NkApplyAdditive(attendu, ecart, 0.5f);
	bool pareil = true;
	for (usize j = 0; j < 4; ++j) {
		pareil = pareil && Pareil(o2.bones[j].r, attendu.bones[j].r);
	}
	NkVector<float32> parts;
	float32 partBase = 0.f;
	p2.Contributions(0u, parts, partBase);
	ASSERT_TRUE_MSG(pareil, "une couche additive est NkApplyAdditive, au bit pres");
	ASSERT_TRUE_MSG(partBase == 1.f && std::fabs(parts[0] - 0.5f) < 1e-6f, "une couche additive ne retire rien a la base");
}
