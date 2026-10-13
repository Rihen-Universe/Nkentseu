// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_miroir.cpp — LE BANC DU MIROIR D'UNE POSE
// (Blend/NkPoseMirror.h, 13/10/2026).
//
// D'ou il vient : le patient virtuel de PV3DE ferme le poing gauche en miroir du
// poing droit de l'artiste. La regle ((v, w) -> (-v + 2 (v . n) n, w)) etait ecrite
// a la main dans PV3DE pour une seule normale ; elle est ici, pour toute normale.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   le miroir retourne la rotation                       mirror_off   (MI1, MI2)
//
//   NK_RT_MUTATION=mirror_off ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
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
				NkRtSetMutations((e != nullptr && std::strstr(e, "mirror_off") != nullptr) ? (uint32)NK_RT_MUT_MIRROR_OFF : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	NkQuatf VersQuat(const NkRtQuat &q) {
		return NkQuatf(q.x, q.y, q.z, q.w);
	}
	NkRtQuat DepuisQuat(const NkQuatf &q) {
		return Q(q.x, q.y, q.z, q.w);
	}
	/// Le retournement par le plan de normale n : p - 2 (p . n) n.
	NkVec3f Retourner(const NkVec3f &p, const NkVec3f &n) {
		return Madd(p, n, -2.f * Dot(p, n));
	}

} // namespace

// -----------------------------------------------------------------------------
// MI1 — LE MIROIR D'UNE ROTATION est S * R * S : tourner un vecteur par la rotation
// miroir, c'est le retourner, le tourner, le retourner. Trois normales (X, Z, une
// quelconque), six rotations, trois vecteurs. L'identite reste l'identite ; avec la
// normale Z c'est (x, y, z, w) -> (-x, -y, z, w) AU BIT PRES (la regle du Biped,
// mesuree dans PV3DE) ; deux miroirs rendent la rotation de depart.
//                                                          [mutation : mirror_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, MI1_Le_miroir_d_une_rotation_est_S_R_S) {
	Mutations _m;
	const NkVec3f normales[3] = {V3(1.f, 0.f, 0.f), V3(0.f, 0.f, 1.f), Normalize(V3(0.36f, 0.48f, 0.8f), V3(1.f, 0.f, 0.f))};
	const NkVec3f axes[3] = {Normalize(V3(1.f, 2.f, 3.f), V3(1.f, 0.f, 0.f)), Normalize(V3(-2.f, 0.5f, 1.f), V3(1.f, 0.f, 0.f)), V3(0.f, 1.f, 0.f)};
	const float32 angles[2] = {0.7f, 2.4f};
	const NkVec3f vecteurs[3] = {V3(1.f, 0.f, 0.f), V3(0.3f, -1.2f, 0.5f), V3(-0.4f, 0.2f, 2.f)};
	float32 pire = 0.f, pireDouble = 0.f;
	for (const NkVec3f &n : normales) {
		for (const NkVec3f &axe : axes) {
			for (float32 angle : angles) {
				const NkRtQuat q = QFromAxisAngle(axe, angle);
				const NkQuatf m = NkMirrorRotation(VersQuat(q), n);
				for (const NkVec3f &p : vecteurs) {
					const NkVec3f attendu = Retourner(QRotate(q, Retourner(p, n)), n);
					pire = std::fmax(pire, Distance(QRotate(DepuisQuat(m), p), attendu));
				}
				const NkQuatf mm = NkMirrorRotation(m, n);
				pireDouble = std::fmax(pireDouble, std::fabs(mm.x - q.x) + std::fabs(mm.y - q.y) + std::fabs(mm.z - q.z) + std::fabs(mm.w - q.w));
			}
		}
	}
	std::printf("  [MI1] 3 normales x 6 rotations x 3 vecteurs : la rotation miroir a %.2e de S * R * S ; deux miroirs a %.2e de la rotation de depart\n",
				(double)pire, (double)pireDouble);
	ASSERT_TRUE_MSG(pire < 1e-5f, "tourner par la rotation miroir = retourner, tourner, retourner");
	ASSERT_TRUE_MSG(pireDouble < 1e-6f, "deux miroirs rendent la rotation de depart");
	const NkQuatf id = NkMirrorRotation(NkQuatf(0.f, 0.f, 0.f, 1.f), normales[2]);
	ASSERT_TRUE_MSG(id.x == 0.f && id.y == 0.f && id.z == 0.f && id.w == 1.f, "l'identite reste l'identite");
	// La regle du Biped (normale Z), au bit pres.
	const NkQuatf q(0.31f, -0.22f, 0.57f, 0.73f);
	const NkQuatf z = NkMirrorRotation(q, V3(0.f, 0.f, 1.f));
	ASSERT_TRUE_MSG(z.x == -q.x && z.y == -q.y && z.z == q.z && z.w == q.w, "normale Z : (x, y, z, w) -> (-x, -y, z, w), au bit pres");
}

// -----------------------------------------------------------------------------
// MI2 — LA POSE MIROIR. Un rig symetrique (racine ; epaule, coude, main a droite ;
// les memes a gauche, reperes alignes sur le monde : la normale est X). Le bras
// droit est pose ; la pose miroir met le bras GAUCHE a l'image du droit par le plan
// x = 0 : la place de ses trois articulations et l'orientation de sa main (dans le
// monde). Deux miroirs rendent la pose de depart. Un os sans paire est copie.
//                                                          [mutation : mirror_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, MI2_La_pose_miroir_met_le_bras_gauche_a_l_image_du_droit) {
	Mutations _m;
	const int32 parents[7] = {-1, 0, 1, 2, 0, 4, 5};
	const NkVec3f places[7] = {V3(0.f, 0.f, 0.f),	V3(0.2f, 1.4f, 0.f),  V3(0.3f, 0.f, 0.f), V3(0.25f, 0.f, 0.f),
							   V3(-0.2f, 1.4f, 0.f), V3(-0.3f, 0.f, 0.f), V3(-0.25f, 0.f, 0.f)};
	NkAnimPose pose;
	pose.bones.Resize(7);
	for (int32 j = 0; j < 7; ++j) {
		pose.bones[(usize)j].t = places[j];
	}
	// Le bras DROIT pose ; le gauche au repos.
	pose.bones[1].r = VersQuat(QFromAxisAngle(Normalize(V3(0.2f, 0.3f, 1.f), V3(0.f, 0.f, 1.f)), -0.9f));
	pose.bones[2].r = VersQuat(QFromAxisAngle(V3(0.f, 1.f, 0.f), -1.3f));
	pose.bones[3].r = VersQuat(QFromAxisAngle(Normalize(V3(1.f, 0.4f, -0.2f), V3(1.f, 0.f, 0.f)), 0.6f));
	NkVector<int32> paires;
	for (int32 p : {0, 4, 5, 6, 1, 2, 3}) {
		paires.PushBack(p);
	}
	NkAnimPose miroir;
	const NkVec3f X = V3(1.f, 0.f, 0.f);
	NkMirrorPose(pose, paires, X, miroir);
	auto Monde = [&](const NkAnimPose &p, NkMat4f *monde) {
		NkVector<NkMat4f> local;
		p.ToLocalMatrices(local);
		for (int32 j = 0; j < 7; ++j) {
			monde[j] = parents[j] < 0 ? local[(usize)j] : monde[parents[j]] * local[(usize)j];
		}
	};
	NkMat4f a[7], b[7];
	Monde(pose, a);
	Monde(miroir, b);
	float32 pirePlace = 0.f, pireMain = 0.f;
	for (int32 k = 0; k < 3; ++k) { // epaule, coude, main : la gauche du miroir contre la droite de depart
		pirePlace = std::fmax(pirePlace, Distance(Origin(b[4 + k]), Retourner(Origin(a[1 + k]), X)));
	}
	// L'orientation de la main : un point porte par la main (a 10 cm, de travers).
	const NkVec3f doigt = V3(0.07f, 0.05f, -0.05f);
	pireMain = Distance(TransformPoint(b[6], Retourner(doigt, X)), Retourner(TransformPoint(a[3], doigt), X));
	std::printf("  [MI2] le bras gauche de la pose miroir : ses trois articulations a %.2e m de l'image du bras droit, un point de la main a %.2e m\n",
				(double)pirePlace, (double)pireMain);
	ASSERT_TRUE_MSG(pirePlace < 1e-5f, "l'epaule, le coude et la main gauches sont l'image des droits par le plan x = 0");
	ASSERT_TRUE_MSG(pireMain < 1e-5f, "la main gauche est orientee en miroir de la droite");
	// Deux miroirs : la pose de depart (dans la meme pose : in == out).
	NkAnimPose retour = miroir;
	NkMirrorPose(retour, paires, X, retour);
	float32 pireRetour = 0.f;
	for (int32 j = 0; j < 7; ++j) {
		const NkBoneTRS &u = retour.bones[(usize)j], &v = pose.bones[(usize)j];
		pireRetour = std::fmax(pireRetour, std::fabs(u.r.x - v.r.x) + std::fabs(u.r.y - v.r.y) + std::fabs(u.r.z - v.r.z) + std::fabs(u.r.w - v.r.w) +
											   std::fabs(u.t.x - v.t.x) + std::fabs(u.t.y - v.t.y) + std::fabs(u.t.z - v.t.z));
	}
	ASSERT_TRUE_MSG(pireRetour < 1e-6f, "deux miroirs rendent la pose de depart");
	// Un os sans paire est copie tel quel.
	NkVector<int32> sans;
	for (int32 j = 0; j < 7; ++j) {
		sans.PushBack(-1);
	}
	NkAnimPose copie;
	NkMirrorPose(pose, sans, X, copie);
	ASSERT_TRUE_MSG(copie.bones.Size() == 7u && copie.bones[2].r.y == pose.bones[2].r.y && copie.bones[1].t.x == pose.bones[1].t.x,
					"un os sans paire est copie tel quel");
}
