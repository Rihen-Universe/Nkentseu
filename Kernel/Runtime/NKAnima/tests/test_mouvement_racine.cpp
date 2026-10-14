// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_mouvement_racine.cpp : LE BANC DU MOUVEMENT DE LA RACINE
// (Motion/NkRootMotion.h, 14/10/2026).
//
// D'ou il vient : Noge declare `hasRootMotion` / `applyRootMotion` et rien ne les
// lit ; NKAnima n'avait aucune occurrence de « root motion ». Un jeu (Nogee), une
// sequence (NKScena), le patient qui marche (PV3DE) jouent la marche SUR PLACE et
// avancent le personnage de ce que le clip a parcouru.
//
// Les clips sont fabriques par le calcul (30 cles par seconde, une racine aux
// reperes QUELCONQUES qui se balance et tangue, un os enfant). Chaque critere est
// MESURE (la valeur est imprimee) ; sa mutation, dans le MEME binaire par
// NK_RT_MUTATION, doit le faire rougir :
//
//   un clip qui boucle compose ses cycles              rootmotion_no_wrap      (RM1)
//   le deplacement est dans le repere du personnage    rootmotion_world_frame  (RM2)
//   le lacet se cumule (un trois-quarts de tour)       rootmotion_yaw_wrapped  (RM3)
//   la pose sur place perd l'avance et le lacet        rootmotion_strip_off    (RM4)
//
//   NK_RT_MUTATION=rootmotion_no_wrap ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKAnima/Motion/NkRootMotion.h"
#include "NKAnima/Realtime/NkRtMath.h"
#include "NKAnima/Realtime/NkRtTwist.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;
using math::NkMat4f;
using math::NkVec3f;

namespace {

	constexpr float32 kPi = 3.14159265358979f;
	constexpr float32 kDeg = kPi / 180.f;

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				uint32 bits = 0u;
				if (e != nullptr) {
					bits |= std::strstr(e, "rootmotion_no_wrap") ? (uint32)NK_RM_MUT_NO_WRAP : 0u;
					bits |= std::strstr(e, "rootmotion_world_frame") ? (uint32)NK_RM_MUT_WORLD_FRAME : 0u;
					bits |= std::strstr(e, "rootmotion_strip_off") ? (uint32)NK_RM_MUT_STRIP_OFF : 0u;
					bits |= std::strstr(e, "rootmotion_yaw_wrapped") ? (uint32)NK_RM_MUT_YAW_WRAPPED : 0u;
				}
				NkRootMotionSetMutations(bits);
			}
			~Mutations() {
				NkRootMotionSetMutations(0u);
			}
	};

	/// La forme d'un clip : position et lacet de la racine a l'instant t (0..1 s).
	struct Forme {
			float32 avance = 0.f;	 ///< metres par seconde, droit devant (+Z au depart)
			float32 tour = 0.f;		 ///< lacet par seconde (radians) : la marche tourne
			float32 rayon = 0.f;	 ///< si tour != 0 et rayon > 0 : un arc de ce rayon
			float32 depart = 0.f;	 ///< l'abscisse X de la racine a t = 0 (un clip qui ne part pas de l'origine)
	};

	/// Un clip LOCAL d'une seconde, deux os (la racine, un enfant), 31 cles. La racine
	/// a une rotation de repos quelconque, tangue de +-6 degres et monte et descend de
	/// 3 cm (deux pas par seconde) : rien de cela n'est du mouvement de racine.
	void Fabriquer(const Forme &f, NkAnimationClip &c, bool boucle = true) {
		c.name = "marche";
		c.duration = 1.f;
		c.loop = boucle;
		c.skeletalLocal = true;
		c.boneCount = 2;
		c.jointParent.PushBack(-1);
		c.jointParent.PushBack(0);
		c.jointNames.PushBack(NkString("hips"));
		c.jointNames.PushBack(NkString("spine"));
		c.boneTracks.Resize(2);
		const rt::NkRtQuat repos = rt::QFromAxisAngle(rt::Normalize(rt::V3(0.3f, -0.8f, 0.5f), rt::V3(1.f, 0.f, 0.f)), 1.3f);
		for (int32 i = 0; i <= 30; ++i) {
			const float32 t = (float32)i / 30.f;
			const float32 lacet = f.tour * t;
			NkVec3f p;
			if (f.tour != 0.f && f.rayon > 0.f) {
				p = rt::V3(f.rayon * (1.f - std::cos(lacet)), 0.f, f.rayon * std::sin(lacet));
			} else {
				p = rt::V3(0.f, 0.f, f.avance * t);
			}
			p.x += f.depart;
			p.y = 0.95f + 0.03f * std::sin(4.f * kPi * t);
			const rt::NkRtQuat tangage = rt::QFromAxisAngle(rt::V3(1.f, 0.f, 0.f), 6.f * kDeg * std::sin(4.f * kPi * t));
			const rt::NkRtQuat q = rt::QMul(rt::QFromAxisAngle(rt::V3(0.f, 1.f, 0.f), lacet), rt::QMul(tangage, repos));
			c.boneTracks[0].AddKey(t, rt::MatFrom(q, rt::V3(1.f, 1.f, 1.f), p));
			c.boneTracks[1].AddKey(t, rt::MatFrom(rt::QIdentity(), rt::V3(1.f, 1.f, 1.f), rt::V3(0.f, 0.12f, 0.f)));
		}
	}

	float32 EcartV(const NkVec3f &a, const NkVec3f &b) {
		return rt::Length(rt::Sub(a, b));
	}

} // namespace

// -----------------------------------------------------------------------------
// RM1 : LA MARCHE DROITE, ET LA BOUCLE. 1,4 m par cycle d'une seconde : de 0,2 a
// 0,7 s le personnage avance de 0,70 m ; de 0,8 a 1,3 s (a travers la fin du
// cycle) AUSSI ; en 3,5 s de 4,90 m ; a rebours de -0,70 m. Le balancement
// vertical et le tangage restent dans la pose (ni montee ni lacet). Image par
// image (60 Hz, 2,5 s), la somme des deplacements fait le total. Un clip qui ne
// boucle pas s'arrete a sa fin.                            [mutation : rootmotion_no_wrap]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, RM1_La_marche_droite_et_la_boucle) {
	Mutations _m;
	Forme f;
	f.avance = 1.4f;
	NkAnimationClip clip;
	Fabriquer(f, clip);
	NkRootMotionSettings s;
	ASSERT_TRUE(NkRootMotionBone(clip, s) == 0);
	const NkRootMotionDelta a = NkRootMotionBetween(clip, 0.2f, 0.7f, s);
	const NkRootMotionDelta b = NkRootMotionBetween(clip, 0.8f, 1.3f, s);
	const NkRootMotionDelta c = NkRootMotionBetween(clip, 0.f, 3.5f, s);
	const NkRootMotionDelta r = NkRootMotionBetween(clip, 1.3f, 0.8f, s);
	std::printf("  [RM1] 0,2->0,7 s : (%.4f, %.4f, %.4f) lacet %.5f ; 0,8->1,3 s : (%.4f, %.4f, %.4f) ; 0->3,5 s : z %.4f ; 1,3->0,8 s : z %.4f\n",
				(double)a.translation.x, (double)a.translation.y, (double)a.translation.z, (double)a.yaw, (double)b.translation.x,
				(double)b.translation.y, (double)b.translation.z, (double)c.translation.z, (double)r.translation.z);
	ASSERT_TRUE_MSG(EcartV(a.translation, rt::V3(0.f, 0.f, 0.7f)) < 1e-4f && std::fabs(a.yaw) < 1e-5f,
					"0,70 m droit devant, ni montee ni lacet");
	ASSERT_TRUE_MSG(EcartV(b.translation, rt::V3(0.f, 0.f, 0.7f)) < 1e-4f, "a travers la fin du cycle, toujours 0,70 m");
	ASSERT_TRUE_MSG(std::fabs(c.translation.z - 4.9f) < 1e-3f, "trois cycles et demi : 4,90 m");
	ASSERT_TRUE_MSG(std::fabs(r.translation.z + 0.7f) < 1e-4f, "a rebours : -0,70 m");

	// Image par image.
	NkVec3f pos = rt::V3(0.f, 0.f, 0.f);
	float32 lacet = 0.f;
	float32 e = 0.f;
	for (int32 i = 0; i < 150; ++i) {
		const float32 e1 = (float32)(i + 1) / 60.f;
		NkRootMotionApply(NkRootMotionBetween(clip, e, e1, s), s.up, pos, lacet);
		e = e1;
	}
	std::printf("  [RM1] 150 images a 60 Hz : le personnage a avance de %.5f m (attendu 3,5)\n", (double)pos.z);
	ASSERT_TRUE_MSG(EcartV(pos, rt::V3(0.f, 0.f, 3.5f)) < 1e-3f, "image par image, la somme fait le total");

	NkAnimationClip une;
	Fabriquer(f, une, false);
	const NkRootMotionDelta u = NkRootMotionBetween(une, 0.5f, 3.f, s);
	std::printf("  [RM1] clip sans boucle, 0,5->3 s : z %.4f (attendu 0,70 : il s'arrete a sa fin)\n", (double)u.translation.z);
	ASSERT_TRUE_MSG(std::fabs(u.translation.z - 0.7f) < 1e-4f, "un clip qui ne boucle pas s'arrete a sa fin");
}

// -----------------------------------------------------------------------------
// RM2 : LA MARCHE QUI TOURNE. Un quart de tour par cycle, sur un arc de 2 m de
// rayon : en quatre cycles le personnage a fait le tour et revient a son point de
// depart, tourne de 360 degres (d'un coup, puis image par image a 60 Hz). Un
// personnage tourne vers l'est avance vers l'est.       [mutation : rootmotion_world_frame]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, RM2_La_marche_qui_tourne_revient_a_son_depart) {
	Mutations _m;
	Forme f;
	f.tour = 0.5f * kPi;
	f.rayon = 2.f;
	NkAnimationClip clip;
	Fabriquer(f, clip);
	NkRootMotionSettings s;
	// Le milieu d'un cycle, de 0,2 a 0,8 s (deux cles : entre ses cles le clip est une
	// ligne brisee, on ne la compare pas a l'arc), 18 a 72 degres de virage : la corde
	// (2 r sin 27 = 1,8160 m) vue du personnage a 0,2 s fait 27 degres avec son avant.
	// ⚠️ La fermeture du tour seule ne suffit pas : une erreur de repere identique a
	// chaque cycle se referme aussi, par symetrie d'ordre quatre (mesure : la
	// mutation rootmotion_world_frame la laissait verte).
	const NkRootMotionDelta mi = NkRootMotionBetween(clip, 0.2f, 0.8f, s);
	const float32 corde = 2.f * f.rayon * std::sin(27.f * kDeg);
	const NkVec3f miAttendu = rt::V3(corde * std::sin(27.f * kDeg), 0.f, corde * std::cos(27.f * kDeg));
	std::printf("  [RM2] 0,2->0,8 s : (%.4f, %.4f, %.4f) lacet %.4f deg ; attendu (%.4f, 0, %.4f) et 54\n", (double)mi.translation.x,
				(double)mi.translation.y, (double)mi.translation.z, (double)(mi.yaw / kDeg), (double)miAttendu.x, (double)miAttendu.z);
	ASSERT_TRUE_MSG(EcartV(mi.translation, miAttendu) < 1e-4f && std::fabs(mi.yaw / kDeg - 54.f) < 0.01f,
					"le deplacement est dit dans le repere du personnage a son depart");
	const NkRootMotionDelta tour = NkRootMotionBetween(clip, 0.f, 4.f, s);
	NkVec3f pos = rt::V3(0.f, 0.f, 0.f);
	float32 lacet = 0.f, e = 0.f;
	NkVec3f apresUn = pos;
	for (int32 i = 0; i < 240; ++i) {
		const float32 e1 = (float32)(i + 1) / 60.f;
		NkRootMotionApply(NkRootMotionBetween(clip, e, e1, s), s.up, pos, lacet);
		e = e1;
		if (i == 59) {
			apresUn = pos;
		}
	}
	std::printf("  [RM2] un cycle image par image : (%.4f, %.4f, %.4f), attendu (2, 0, 2) : le quart de cercle de 2 m\n", (double)apresUn.x,
				(double)apresUn.y, (double)apresUn.z);
	ASSERT_TRUE_MSG(EcartV(apresUn, rt::V3(f.rayon, 0.f, f.rayon)) < 2e-3f, "un cycle image par image suit l'arc");
	std::printf("  [RM2] quatre quarts de tour : d'un coup a %.5f m du depart, lacet %.4f deg ; image par image a %.5f m, lacet %.4f deg\n",
				(double)rt::Length(tour.translation), (double)(tour.yaw / kDeg), (double)rt::Length(pos), (double)(lacet / kDeg));
	ASSERT_TRUE_MSG(rt::Length(tour.translation) < 1e-3f && std::fabs(tour.yaw - 2.f * kPi) < 1e-4f, "le tour complet ramene au depart");
	ASSERT_TRUE_MSG(rt::Length(pos) < 2e-3f && std::fabs(lacet - 2.f * kPi) < 1e-3f, "image par image aussi");

	Forme droit;
	droit.avance = 1.4f;
	NkAnimationClip marche;
	Fabriquer(droit, marche);
	NkVec3f p = rt::V3(5.f, 0.f, 5.f);
	float32 est = 0.5f * kPi; // tourne vers +X
	NkRootMotionApply(NkRootMotionBetween(marche, 0.f, 1.f, s), s.up, p, est);
	std::printf("  [RM2] tourne vers l'est, un cycle : (%.4f, %.4f, %.4f)\n", (double)p.x, (double)p.y, (double)p.z);
	ASSERT_TRUE_MSG(EcartV(p, rt::V3(6.4f, 0.f, 5.f)) < 1e-4f, "un personnage tourne vers l'est avance vers l'est");
}

// -----------------------------------------------------------------------------
// RM3 : LE LACET SE CUMULE. Un trois-quarts de tour sur place en une seconde : a la
// fin, le lacet vaut 270 degres, pas -90 (un repli a +-180 ferait tourner le
// personnage dans le mauvais sens au milieu du geste).  [mutation : rootmotion_yaw_wrapped]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, RM3_Le_lacet_se_cumule) {
	Mutations _m;
	Forme f;
	f.tour = 1.5f * kPi;
	NkAnimationClip clip;
	Fabriquer(f, clip);
	NkRootMotionSettings s;
	NkRootMotionDelta fin, milieu;
	ASSERT_TRUE(NkRootMotionAt(clip, 1.f, s, fin));
	ASSERT_TRUE(NkRootMotionAt(clip, 0.5f, s, milieu));
	std::printf("  [RM3] trois-quarts de tour sur place : lacet %.4f deg a la fin (attendu 270), %.4f deg au milieu (135) ; avance %.2e m\n",
				(double)(fin.yaw / kDeg), (double)(milieu.yaw / kDeg), (double)rt::Length(fin.translation));
	ASSERT_TRUE_MSG(std::fabs(fin.yaw / kDeg - 270.f) < 0.01f && std::fabs(milieu.yaw / kDeg - 135.f) < 0.01f, "le lacet se cumule de cle en cle");
	ASSERT_TRUE_MSG(rt::Length(fin.translation) < 1e-4f, "tourner sur place n'avance pas");
}

// -----------------------------------------------------------------------------
// RM4 : LA POSE SUR PLACE. La marche qui tourne, partie de x = 0,4 m : a 31
// instants, la racine depouillee reste a l'aplomb de son depart (0,1 mm), garde sa
// hauteur au bit pres de la mesure (le balancement reste), et ne tourne plus
// autour du haut ; M(t) remis sur la racine depouillee rend la racine du clip.
//                                                       [mutation : rootmotion_strip_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, RM4_La_pose_sur_place) {
	Mutations _m;
	Forme f;
	f.tour = 0.5f * kPi;
	f.rayon = 2.f;
	f.depart = 0.4f;
	NkAnimationClip clip;
	Fabriquer(f, clip);
	NkRootMotionSettings s;
	const NkMat4f r0 = clip.boneTracks[0].Evaluate(0.f);
	const rt::NkRtQuat q0 = rt::QFromMat(r0);
	float32 pireAplomb = 0.f, pireHauteur = 0.f, pireLacet = 0.f, pireRecompose = 0.f;
	for (int32 i = 0; i <= 30; ++i) {
		const float32 t = (float32)i / 30.f; // les instants des cles : la racine du clip y est exacte
		const NkMat4f r = clip.boneTracks[0].Evaluate(t);
		NkMat4f sur = r;
		ASSERT_TRUE(NkRootMotionStrip(clip, t, s, sur));
		const NkVec3f o = rt::Origin(sur), o0 = rt::Origin(r0);
		pireAplomb = std::fmax(pireAplomb, std::sqrt((o.x - o0.x) * (o.x - o0.x) + (o.z - o0.z) * (o.z - o0.z)));
		pireHauteur = std::fmax(pireHauteur, std::fabs(o.y - rt::Origin(r).y));
		pireLacet = std::fmax(pireLacet, std::fabs(rt::NkRtTwistAngle(rt::QMul(rt::QFromMat(sur), rt::QConj(q0)), rt::V3(0.f, 1.f, 0.f))) / kDeg);
		// M(t) o sur place = la racine du clip.
		NkRootMotionDelta m;
		NkRootMotionAt(clip, t, s, m);
		NkMat4f re = sur;
		rt::RotateAbout(re, rt::QFromAxisAngle(s.up, m.yaw), rt::V3(0.f, 0.f, 0.f));
		rt::SetOrigin(re, rt::Add(rt::Origin(re), m.translation));
		for (int a = 0; a < 4; ++a)
			for (int b = 0; b < 4; ++b)
				pireRecompose = std::fmax(pireRecompose, std::fabs(re[a][b] - r[a][b]));
	}
	std::printf("  [RM4] 31 instants : racine sur place a %.4f mm de l'aplomb du depart, hauteur a %.2e m, lacet residuel %.4f deg ; "
				"recomposee a %.2e\n",
				(double)(pireAplomb * 1000.f), (double)pireHauteur, (double)pireLacet, (double)pireRecompose);
	ASSERT_TRUE_MSG(pireAplomb < 1e-4f, "la racine sur place reste a l'aplomb de son depart");
	ASSERT_TRUE_MSG(pireHauteur < 1e-5f, "le balancement vertical reste dans la pose");
	ASSERT_TRUE_MSG(pireLacet < 0.01f, "la racine sur place ne tourne plus autour du haut");
	ASSERT_TRUE_MSG(pireRecompose < 1e-4f, "le mouvement remis sur la pose sur place rend le clip");
}
