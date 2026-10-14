// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_inertie.cpp : LE BANC DE LA TRANSITION PAR INERTIE
// (Blend/NkInertialize.h, 14/10/2026).
//
// Un os que l'ancien etat lancait VERS la pose du nouvel etat (1,5 m/s, et 90
// degres par seconde), un os sans ecart, une forme. Le nouvel etat est immobile :
// tout ce qui bouge est l'ecart qui s'efface.
//
// Chaque critere est MESURE (la valeur est imprimee) ; sa mutation, dans le MEME
// binaire par NK_RT_MUTATION, doit le faire rougir :
//
//   aucun saut de pose au changement                   inertia_no_offset  (IN1)
//   aucun saut de vitesse au changement                inertia_linear     (IN1)
//   aucun rebond sous la pose d'arrivee                inertia_no_shorten (IN2)
//
//   NK_RT_MUTATION=inertia_linear ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKAnima/Blend/NkInertialize.h"
#include "NKAnima/Realtime/NkRtMath.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;
using math::NkVec3f;

namespace {

	constexpr float32 kDeg = 3.14159265358979f / 180.f;

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				uint32 bits = 0u;
				if (e != nullptr) {
					bits |= std::strstr(e, "inertia_linear") ? (uint32)NK_IN_MUT_LINEAR : 0u;
					bits |= std::strstr(e, "inertia_no_offset") ? (uint32)NK_IN_MUT_NO_OFFSET : 0u;
					bits |= std::strstr(e, "inertia_no_shorten") ? (uint32)NK_IN_MUT_NO_SHORTEN : 0u;
				}
				NkInertializeSetMutations(bits);
			}
			~Mutations() {
				NkInertializeSetMutations(0u);
			}
	};

	rt::NkRtQuat R(const NkQuatf &q) {
		return rt::Q(q.x, q.y, q.z, q.w);
	}
	NkQuatf Q(const rt::NkRtQuat &q) {
		return NkQuatf(q.x, q.y, q.z, q.w);
	}

	/// Les poses : la cible (le nouvel etat, immobile), et les deux dernieres sorties de
	/// l'ancien etat, l'os 0 arrivant a `vitesse` m/s et `vitesseAngle` rad/s VERS la cible.
	struct Scene {
			NkAnimPose cible, prev, prevPrev;
			NkVec3f axe;
			float32 dt = 1.f / 60.f;
			void Monter(float32 ecart, float32 vitesse, float32 angle, float32 vitesseAngle) {
				axe = rt::Normalize(rt::V3(0.3f, 0.8f, -0.5f), rt::V3(0.f, 1.f, 0.f));
				cible.bones.Resize(2);
				cible.bones[0].t = rt::V3(0.f, 1.f, 0.f);
				cible.bones[0].r = Q(rt::QFromAxisAngle(rt::Normalize(rt::V3(1.f, 0.2f, 0.f), rt::V3(1.f, 0.f, 0.f)), 0.7f));
				cible.bones[1].t = rt::V3(0.f, 0.3f, 0.f);
				cible.bones[1].r = Q(rt::QFromAxisAngle(rt::V3(0.f, 0.f, 1.f), 0.2f));
				cible.morphs.PushBack(0.8f);
				prev = cible;
				prevPrev = cible;
				prev.bones[0].t = rt::V3(ecart, 1.f, 0.f);
				prevPrev.bones[0].t = rt::V3(ecart + vitesse * dt, 1.f, 0.f);
				prev.bones[0].r = Q(rt::QMul(rt::QFromAxisAngle(axe, angle), R(cible.bones[0].r)));
				prevPrev.bones[0].r = Q(rt::QMul(rt::QFromAxisAngle(axe, angle + vitesseAngle * dt), R(cible.bones[0].r)));
				prev.morphs[0] = 0.2f;
				prevPrev.morphs[0] = 0.2f;
			}
			/// La pose sortie au temps t de la transition.
			NkAnimPose Sortie(const NkInertialBlend &b, float32 t) const {
				NkAnimPose p = cible;
				b.Apply(p, t);
				return p;
			}
	};

	float32 AngleDeg(const NkQuatf &a, const NkQuatf &b) {
		return rt::QAngle(rt::QMul(rt::QConj(R(a)), R(b))) / kDeg;
	}

} // namespace

// -----------------------------------------------------------------------------
// IN1 : NI SAUT DE POSE, NI SAUT DE VITESSE. Au changement, la pose sortie est la
// derniere de l'ancien etat (position, rotation, forme) ; juste apres, l'os va a
// 1,5 m/s et tourne a 90 degres par seconde, comme avant le changement (a 2 %).
//                                     [mutations : inertia_no_offset ; inertia_linear]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, IN1_Ni_saut_de_pose_ni_saut_de_vitesse) {
	Mutations _m;
	Scene s;
	s.Monter(0.5f, 1.5f, 40.f * kDeg, 90.f * kDeg);
	NkInertialBlend b;
	b.Start(s.prev, s.prevPrev, s.dt, s.cible, 0.3f);
	const NkAnimPose p0 = s.Sortie(b, 0.f);
	const float32 sautPos = rt::Distance(p0.bones[0].t, s.prev.bones[0].t);
	const float32 sautRot = AngleDeg(p0.bones[0].r, s.prev.bones[0].r);
	const float32 sautForme = std::fabs(p0.morphs[0] - s.prev.morphs[0]);
	const float32 h = 1e-4f;
	const NkAnimPose ph = s.Sortie(b, h);
	const float32 vitesse = (ph.bones[0].t.x - p0.bones[0].t.x) / h;
	// La vitesse angulaire : l'angle restant jusqu'a la cible, derive.
	const float32 w0 = AngleDeg(p0.bones[0].r, s.cible.bones[0].r), wh = AngleDeg(ph.bones[0].r, s.cible.bones[0].r);
	const float32 vitesseAngle = (wh - w0) / h;
	std::printf("  [IN1] au changement : saut de %.2e m, %.4f deg, forme %.2e ; juste apres : %.4f m/s (-1,5), %.3f deg/s (-90)\n", (double)sautPos,
				(double)sautRot, (double)sautForme, (double)vitesse, (double)vitesseAngle);
	ASSERT_TRUE_MSG(sautPos < 1e-6f && sautRot < 0.001f && sautForme < 1e-6f, "aucun saut de pose au changement");
	ASSERT_TRUE_MSG(std::fabs(vitesse + 1.5f) < 0.03f, "la position garde sa vitesse au changement");
	ASSERT_TRUE_MSG(std::fabs(vitesseAngle + 90.f) < 1.8f, "la rotation garde sa vitesse au changement");
}

// -----------------------------------------------------------------------------
// IN2 : L'ARRIVEE, SANS REBOND. Un os lance tres vite vers la cible (10 m/s, 0,5 m
// d'ecart : il la depasserait en 0,05 s a cette vitesse) : l'ecart decroit sans
// jamais passer de l'autre cote ; a la fin de la duree la pose est celle du nouvel
// etat AU BIT PRES ; l'os sans ecart ne bouge jamais ; la forme rejoint sa valeur.
//                                                       [mutation : inertia_no_shorten]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, IN2_L_arrivee_sans_rebond) {
	Mutations _m;
	Scene s;
	s.Monter(0.5f, 10.f, 40.f * kDeg, 600.f * kDeg);
	NkInertialBlend b;
	b.Start(s.prev, s.prevPrev, s.dt, s.cible, 0.3f);
	float32 plusBas = 1.f, plusHaut = 0.f, croissance = 0.f, angleMin = 1e9f, bougeOs1 = 0.f;
	float32 avant = 0.5f;
	for (int32 i = 0; i <= 300; ++i) {
		const float32 t = 0.3f * (float32)i / 300.f;
		const NkAnimPose p = s.Sortie(b, t);
		const float32 x = p.bones[0].t.x; // l'ecart, signe : la cible est en x = 0
		plusBas = std::fmin(plusBas, x);
		plusHaut = std::fmax(plusHaut, x);
		croissance = std::fmax(croissance, x - avant);
		avant = x;
		// L'ecart de rotation, signe le long de l'axe de depart.
		const rt::NkRtQuat d = rt::QMul(R(p.bones[0].r), rt::QConj(R(s.cible.bones[0].r)));
		angleMin = std::fmin(angleMin, rt::Dot(rt::QToRotVec(d), s.axe) / kDeg);
		bougeOs1 = std::fmax(bougeOs1, std::memcmp(&p.bones[1], &s.cible.bones[1], sizeof(NkBoneTRS)) != 0 ? 1.f : 0.f);
	}
	const NkAnimPose fin = s.Sortie(b, 0.3f);
	const bool finExacte = std::memcmp(fin.bones.Data(), s.cible.bones.Data(), sizeof(NkBoneTRS) * 2) == 0 && fin.morphs[0] == s.cible.morphs[0];
	std::printf("  [IN2] lance a 10 m/s : ecart entre %.2e et %.4f m (jamais sous -1e-6 : pas de rebond), croissance %.2e ; rotation jusqu'a %.4f deg ; "
				"fin au bit pres = %d ; os sans ecart touche = %d\n",
				(double)plusBas, (double)plusHaut, (double)croissance, (double)angleMin, finExacte ? 1 : 0, bougeOs1 > 0.f ? 1 : 0);
	ASSERT_TRUE_MSG(plusBas > -1e-6f && angleMin > -0.001f, "l'ecart ne passe jamais de l'autre cote de la cible");
	ASSERT_TRUE_MSG(croissance < 1e-6f, "l'ecart ne fait que decroitre");
	ASSERT_TRUE_MSG(finExacte, "a la fin de la duree, la pose du nouvel etat au bit pres");
	ASSERT_TRUE_MSG(bougeOs1 == 0.f, "un os sans ecart n'est jamais touche");
}
