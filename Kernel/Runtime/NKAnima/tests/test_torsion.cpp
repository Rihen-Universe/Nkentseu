// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_torsion.cpp — LE BANC DE LA TORSION DES OS (Realtime/
// NkRtTwist.h, 13/10/2026).
//
// D'ou il vient : les bras du patient virtuel de PV3DE se tordaient « comme un
// elastique » (Rodolf, 13/10). Rien ne mesurait une torsion (le banc de PV3DE
// compare des longueurs d'aretes), rien ne la bornait ni ne la repartissait.
//
// Chaque critere est MESURE (la valeur est imprimee). TW1 etalonne l'instrument
// sur des angles CONNUS (2000 rotations fabriquees) ; les trois autres ont LEUR
// mutation, posee dans le MEME binaire par la variable d'environnement
// NK_RT_MUTATION :
//
//   la torsion d'un os se lit dans SON repere        twist_parent_frame  (TW2)
//   la borne reporte l'excedent sur le parent        twist_no_carry      (TW3)
//   ... ou le rend a l'enfant (l'os ne se tord pas)  twist_no_carry      (TW5)
//   les os de torsion repartissent la torsion        twist_no_spread     (TW4)
//
//   NK_RT_MUTATION=twist_no_carry ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
//   -> TW3 doit ROUGIR (l'enfant change d'orientation dans le monde).
//
// L'humanoide d'essai est pris AVEC des rotations de repos quelconques
// (twistedBind) : l'outil ne doit rien supposer des reperes des os.
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

	constexpr float32 kPi = 3.14159265358979f;

	float32 Deg(float32 rad) {
		return rad * 180.f / kPi;
	}
	float32 Rad(float32 deg) {
		return deg * kPi / 180.f;
	}

	uint32 MutationsDemandees() {
		const char *e = std::getenv("NK_RT_MUTATION");
		if (e == nullptr)
			return 0u;
		uint32 b = 0u;
		if (std::strstr(e, "twist_no_carry") != nullptr)
			b |= NK_RT_MUT_TWIST_NO_CARRY;
		if (std::strstr(e, "twist_no_spread") != nullptr)
			b |= NK_RT_MUT_TWIST_NO_SPREAD;
		if (std::strstr(e, "twist_parent_frame") != nullptr)
			b |= NK_RT_MUT_TWIST_PARENT_FRAME;
		return b;
	}
	struct Mutations {
			Mutations() {
				NkRtSetMutations(MutationsDemandees());
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	/// Un generateur de nombres (deterministe) dans [0 ; 1[.
	struct Alea {
			uint32 e = 20261013u;
			float32 Tirer() {
				e = e * 1664525u + 1013904223u;
				return (float32)((e >> 8) & 0xFFFFu) / 65536.f;
			}
			NkVec3f Axe() {
				for (;;) {
					const NkVec3f v = V3(Tirer() * 2.f - 1.f, Tirer() * 2.f - 1.f, Tirer() * 2.f - 1.f);
					const float32 l = Length(v);
					if (l > 0.2f && l <= 1.f)
						return Scale(v, 1.f / l);
				}
			}
	};

	/// L'angle (radians) entre deux rotations.
	float32 Ecart(const NkRtQuat &a, const NkRtQuat &b) {
		return QAngle(QMul(QConj(a), b));
	}

	/// L'axe d'un os dans SON repere : la ou est attache l'os suivant.
	NkVec3f AxeOs(const NkRtTestHumanoid &h, int32 suivant) {
		return Normalize(Origin(h.bindLocal[(usize)suivant]), V3(0.f, 1.f, 0.f));
	}

} // namespace

// -----------------------------------------------------------------------------
// TW1 — LA DECOMPOSITION est exacte : q = balancement * torsion, la torsion tourne
// autour de l'axe donne, l'axe du balancement lui est perpendiculaire, et l'angle
// rendu est celui qu'on a mis.
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TW1_Decomposition_balancement_torsion) {
	Mutations _m;
	Alea alea;
	float32 pireRecomposition = 0.f, pireAxe = 0.f, pirePerp = 0.f, pireAngle = 0.f, pireSigne = 0.f;
	for (int32 k = 0; k < 2000; ++k) {
		const NkVec3f a = alea.Axe();
		// Un balancement autour d'un axe PERPENDICULAIRE a `a`, puis une torsion connue.
		const NkVec3f p = Normalize(Cross(a, alea.Axe()), AnyPerpendicular(a));
		const float32 balancement = (alea.Tirer() * 2.f - 1.f) * Rad(150.f);
		const float32 torsion = (alea.Tirer() * 2.f - 1.f) * Rad(170.f);
		const NkRtQuat q = QMul(QFromAxisAngle(p, balancement), QFromAxisAngle(a, torsion));
		NkRtQuat s, t;
		NkRtSwingTwist(q, a, s, t);
		pireRecomposition = std::fmax(pireRecomposition, Ecart(QMul(s, t), q));
		// La torsion ne deplace pas son axe ; l'axe du balancement est perpendiculaire.
		pireAxe = std::fmax(pireAxe, Length(Sub(QRotate(t, a), a)));
		pirePerp = std::fmax(pirePerp, std::fabs(s.x * a.x + s.y * a.y + s.z * a.z));
		pireAngle = std::fmax(pireAngle, std::fabs(NkRtTwistAngle(q, a) - torsion));
		// Le meme angle pour q et -q (la meme rotation).
		pireSigne = std::fmax(pireSigne, std::fabs(NkRtTwistAngle(Q(-q.x, -q.y, -q.z, -q.w), a) - NkRtTwistAngle(q, a)));
	}
	std::printf("  [TW1] 2000 rotations : recomposition %.2e rad, axe de la torsion %.2e, balancement perpendiculaire %.2e, "
				"angle rendu a %.2e rad pres, q et -q a %.2e\n",
				(double)pireRecomposition, (double)pireAxe, (double)pirePerp, (double)pireAngle, (double)pireSigne);
	ASSERT_TRUE_MSG(pireRecomposition < 1e-3f, "balancement * torsion redonne la rotation (< 1e-3 rad)");
	ASSERT_TRUE_MSG(pireAxe < 1e-4f, "la torsion tourne autour de l'axe donne");
	ASSERT_TRUE_MSG(pirePerp < 1e-4f, "l'axe du balancement est perpendiculaire a l'axe de l'os");
	ASSERT_TRUE_MSG(pireAngle < 1e-3f, "l'angle de torsion rendu est celui qu'on a mis (< 1e-3 rad)");
	ASSERT_TRUE_MSG(pireSigne < 1e-4f, "q et -q ont la meme torsion");
}

// -----------------------------------------------------------------------------
// TW2 — LA TORSION D'UN OS, sur l'humanoide d'essai aux reperes quelconques : la
// main tournee de 70 degres autour de l'avant-bras rend 70 ; le coude plie (un
// balancement pur de l'avant-bras) ne rend aucune torsion ; le bras tourne sur son
// axe rend l'angle mis.                        [mutation : twist_parent_frame]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TW2_Torsion_d_un_os_reperes_quelconques) {
	Mutations _m;
	for (int32 tordu = 0; tordu < 2; ++tordu) {
		NkRtTestHumanoid h;
		NkRtMakeTestHumanoid(h, tordu != 0);
		const NkRtQuat mainRef = QFromMat(h.bindLocal[(usize)h.handR]), avantRef = QFromMat(h.bindLocal[(usize)h.foreArmR]);
		const NkRtQuat brasRef = QFromMat(h.bindLocal[(usize)h.upperArmR]);
		const NkVec3f axeAvantBras = AxeOs(h, h.handR);	   // dans le repere de l'avant-bras
		const NkVec3f axeBras = AxeOs(h, h.foreArmR);	   // dans le repere du bras
		// La main tourne de 70 degres autour de l'axe de l'avant-bras (repere du parent).
		const NkRtQuat main70 = QMul(QFromAxisAngle(axeAvantBras, Rad(70.f)), mainRef);
		const float32 tMain = Deg(NkRtChildTwist(mainRef, main70, axeAvantBras));
		// Le coude plie de 80 degres autour d'un axe perpendiculaire a l'avant-bras.
		const NkVec3f pli = AnyPerpendicular(axeAvantBras);
		const NkRtQuat coude80 = QMul(avantRef, QFromAxisAngle(pli, Rad(80.f)));
		const float32 tCoude = Deg(NkRtBoneTwist(avantRef, coude80, axeAvantBras));
		// Le bras tourne de -55 degres sur son axe (rotation de l'humerus).
		const NkRtQuat bras55 = QMul(brasRef, QFromAxisAngle(axeBras, Rad(-55.f)));
		const float32 tBras = Deg(NkRtBoneTwist(brasRef, bras55, axeBras));
		std::printf("  [TW2] reperes %s : main tournee de 70 -> %.3f deg ; coude plie de 80 -> torsion %.4f deg ; bras tourne de "
					"-55 -> %.3f deg\n",
					tordu ? "quelconques" : "droits", (double)tMain, (double)tCoude, (double)tBras);
		ASSERT_TRUE_MSG(std::fabs(tMain - 70.f) < 0.05f, "la main tournee de 70 degres autour de l'avant-bras rend 70");
		ASSERT_TRUE_MSG(std::fabs(tCoude) < 0.05f, "plier le coude n'est pas une torsion");
		ASSERT_TRUE_MSG(std::fabs(tBras + 55.f) < 0.05f, "le bras tourne de -55 degres sur son axe rend -55");
	}
}

// -----------------------------------------------------------------------------
// TW3 — BORNER ET REPORTER. La main est tournee de 80 degres (et pliee) ; la borne
// du poignet est +-20. Apres : la main est a 20, l'avant-bras a pris 60 autour de
// son axe, et la main n'a bouge NI de place NI d'orientation dans le monde. Dans la
// borne, rien n'est touche (au bit pres).      [mutation : twist_no_carry]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TW3_Borne_de_torsion_et_report_sur_le_parent) {
	Mutations _m;
	for (int32 tordu = 0; tordu < 2; ++tordu) {
		NkRtTestHumanoid h;
		NkRtMakeTestHumanoid(h, tordu != 0);
		NkRtRig rig;
		ASSERT_TRUE(rig.Build(h.parents.Data(), (uint32)h.parents.Size(), h.names.Data()));
		const uint32 n = rig.Count();
		NkVector<NkMat4f> local, monde, mondeAvant;
		local.Resize(n);
		monde.Resize(n);
		mondeAvant.Resize(n);
		for (uint32 j = 0; j < n; ++j)
			local[j] = h.bindLocal[j];
		const NkRtQuat mainRef = QFromMat(h.bindLocal[(usize)h.handR]), avantRef = QFromMat(h.bindLocal[(usize)h.foreArmR]);
		const NkVec3f axe = AxeOs(h, h.handR); // l'axe de l'avant-bras, dans son repere
		// Le coude plie de 50 degres ; la main plie de 25 degres puis tourne de 80.
		NkRtQuat avant = QMul(avantRef, QFromAxisAngle(AnyPerpendicular(axe), Rad(50.f)));
		NkRtQuat mainQ = QMul(QFromAxisAngle(axe, Rad(80.f)), QMul(QFromAxisAngle(AnyPerpendicular(axe), Rad(25.f)), mainRef));
		auto Poser = [&](const NkRtQuat &qa, const NkRtQuat &qm) {
			local[(usize)h.foreArmR] = MatFrom(qa, V3(1.f, 1.f, 1.f), Origin(h.bindLocal[(usize)h.foreArmR]));
			local[(usize)h.handR] = MatFrom(qm, V3(1.f, 1.f, 1.f), Origin(h.bindLocal[(usize)h.handR]));
		};
		Poser(avant, mainQ);
		rig.LocalToWorld(local.Data(), mondeAvant.Data());
		NkRtTwistLimit borne;
		borne.minAngle = Rad(-20.f);
		borne.maxAngle = Rad(20.f);
		const float32 avantBrasAvant = NkRtBoneTwist(avantRef, avant, axe);
		const NkRtTwistCarry r = NkRtLimitTwistCarry(avant, mainQ, mainRef, axe, borne);
		Poser(avant, mainQ);
		rig.LocalToWorld(local.Data(), monde.Data());
		const float32 orientation = Ecart(QFromMat(mondeAvant[(usize)h.handR]), QFromMat(monde[(usize)h.handR]));
		const float32 place = Distance(Origin(mondeAvant[(usize)h.handR]), Origin(monde[(usize)h.handR]));
		const float32 pris = NkRtBoneTwist(avantRef, avant, axe) - avantBrasAvant;
		std::printf("  [TW3] reperes %s : main %.2f -> %.2f deg (borne 20), reportes %.2f deg, l'avant-bras a tourne de %.2f deg sur son "
					"axe ; la main dans le monde : orientation a %.2e rad, place a %.2e m\n",
					tordu ? "quelconques" : "droits", (double)Deg(r.before), (double)Deg(r.after), (double)Deg(r.carried), (double)Deg(pris),
					(double)orientation, (double)place);
		ASSERT_TRUE_MSG(std::fabs(Deg(r.before) - 80.f) < 0.1f, "la torsion de la main est mesuree a 80 degres");
		ASSERT_TRUE_MSG(std::fabs(Deg(r.after) - 20.f) < 0.1f, "apres : la main est a sa borne (20 degres)");
		ASSERT_TRUE_MSG(std::fabs(Deg(pris) - 60.f) < 0.1f, "l'avant-bras a pris l'excedent (60 degres) autour de son axe");
		ASSERT_TRUE_MSG(orientation < 1e-3f, "la main garde son orientation dans le monde (< 1e-3 rad)");
		ASSERT_TRUE_MSG(place < 1e-5f, "la main garde sa place dans le monde");
		// Dans la borne : rien n'est touche, au bit pres.
		NkRtQuat a2 = avantRef, m2 = QMul(QFromAxisAngle(axe, Rad(12.f)), mainRef);
		const NkRtQuat a0 = a2, m0 = m2;
		const NkRtTwistCarry r2 = NkRtLimitTwistCarry(a2, m2, mainRef, axe, borne);
		ASSERT_TRUE_MSG(r2.carried == 0.f && std::memcmp(&a2, &a0, sizeof(a0)) == 0 && std::memcmp(&m2, &m0, sizeof(m0)) == 0,
						"dans la borne, les deux rotations sont rendues telles quelles");
	}
}

// -----------------------------------------------------------------------------
// TW4 — LES OS DE TORSION REPARTISSENT. Un avant-bras d'essai : des anneaux de
// peau le long de l'os, attaches a l'os ; la main tourne de 90 degres. Sans os de
// torsion, la peau ne tourne pas du tout le long de l'avant-bras et saute de 90
// degres au poignet. Avec deux os de torsion (repartition des poids, angle de
// chaque station), chaque anneau tourne de s x 90, et le plus grand saut entre
// deux anneaux voisins est celui d'une rampe.     [mutation : twist_no_spread]
// De meme pour le bras (la torsion de l'os lui-meme, retenue a l'epaule).
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TW4_Les_os_de_torsion_repartissent_la_torsion) {
	Mutations _m;
	constexpr uint32 kAnneaux = 11u;
	const float32 torsion = Rad(90.f);
	for (int32 regime = 0; regime < 2; ++regime) {
		const NkRtTwistMode mode = regime == 0 ? NK_RT_TWIST_FOLLOW_CHILD : NK_RT_TWIST_HOLD_PARENT;
		const NkRtTwistChain chaine = NkRtMakeTwistChain(mode, 2u);
		// Les joints : 0 l'os, 1 et 2 ses os de torsion, 3 un voisin (le coude, l'epaule).
		uint32 station[kNkRtTwistMaxStations] = {};
		for (uint32 k = 0, t = 1u; k < chaine.count; ++k)
			station[k] = k == chaine.self ? 0u : t++;
		float32 pireEcart = 0.f, pireSaut = 0.f, pireSomme = 0.f, deplace = 0.f;
		float32 avant = 0.f;
		for (uint32 a = 0; a < kAnneaux; ++a) {
			const float32 s = (float32)a / (float32)(kAnneaux - 1u);
			// Le sommet de l'anneau : 85 % sur l'os, 15 % sur un voisin (quatre cases).
			uint32 joints[4] = {0u, 3u, 0u, 0u};
			float32 poids[4] = {0.85f, 0.15f, 0.f, 0.f};
			deplace += NkRtTwistReweight(joints, poids, 4u, 0u, s, chaine, station);
			float32 somme = 0.f, angle = 0.f;
			for (uint32 i = 0; i < 4u; ++i) {
				somme += poids[i];
				// Ce que la peau tourne autour de l'axe : la somme ponderee de ce que
				// chaque joint tourne (l'os : la torsion entiere en HOLD_PARENT, rien en
				// FOLLOW_CHILD ; une station : en plus, son angle ; le voisin : rien).
				if (poids[i] <= 0.f || joints[i] == 3u)
					continue;
				uint32 k = 0;
				for (; k < chaine.count && station[k] != joints[i]; ++k) {
				}
				const float32 deLOs = mode == NK_RT_TWIST_HOLD_PARENT ? torsion : 0.f;
				angle += poids[i] * (deLOs + NkRtTwistStationAngle(chaine, k, torsion));
			}
			angle /= 0.85f; // la part portee par la chaine
			pireSomme = std::fmax(pireSomme, std::fabs(somme - 1.f));
			pireEcart = std::fmax(pireEcart, std::fabs(angle - s * torsion));
			if (a > 0u)
				pireSaut = std::fmax(pireSaut, std::fabs(angle - avant));
			avant = angle;
		}
		// Le dernier saut : au bout de l'os en FOLLOW_CHILD (la main tourne de toute la
		// torsion), a son origine en HOLD_PARENT (le parent ne tourne pas).
		const float32 bord = mode == NK_RT_TWIST_FOLLOW_CHILD ? std::fabs(torsion - avant) : 0.f;
		pireSaut = std::fmax(pireSaut, bord);
		std::printf("  [TW4] %s, 2 os de torsion, torsion de 90 deg, %u anneaux : la peau a %.2f deg de s x 90 au plus ; le plus grand saut "
					"entre voisins %.1f deg (une rampe : 9) ; somme des poids a %.1e de 1 ; poids deplace faute de case %.3f\n",
					regime == 0 ? "avant-bras (suit la main)" : "bras (retenu a l'epaule)", kAnneaux, (double)Deg(pireEcart),
					(double)Deg(pireSaut), (double)pireSomme, (double)deplace);
		ASSERT_TRUE_MSG(pireSomme < 1e-5f, "la somme des poids de chaque sommet reste 1");
		ASSERT_TRUE_MSG(Deg(pireEcart) < 0.5f, "chaque anneau tourne de s x la torsion (a 0,5 degre)");
		ASSERT_TRUE_MSG(Deg(pireSaut) < 15.f, "aucun saut de torsion entre anneaux voisins (< 15 degres pour 90)");
		ASSERT_TRUE_MSG(deplace == 0.f, "aucun poids deplace : quatre cases suffisent a ce sommet");
		ASSERT_TRUE_MSG(std::fabs(NkRtTwistSkinShare(chaine, 0.5f) - 0.5f) < 1e-5f, "a mi-os la peau suit la moitie de la torsion");
		// Le plus grand saut entre stations voisines : la moitie de la torsion avec deux
		// os de torsion poses ; la torsion ENTIERE si aucune station n'a tourne.
		float32 poses[kNkRtTwistMaxStations] = {}, immobiles[kNkRtTwistMaxStations] = {};
		for (uint32 k = 0; k < chaine.count; ++k) {
			poses[k] = NkRtTwistStationAngle(chaine, k, torsion);
		}
		const float32 sautPose = NkRtTwistMaxStep(chaine, torsion, poses), sautBrut = NkRtTwistMaxStep(chaine, torsion, immobiles);
		std::printf("  [TW4] le plus grand saut entre stations : %.1f deg posees, %.1f deg immobiles (torsion 90)\n", (double)Deg(sautPose),
					(double)Deg(sautBrut));
		ASSERT_TRUE_MSG(std::fabs(Deg(sautPose) - 45.f) < 0.1f, "deux os de torsion poses : le plus grand saut est la moitie de la torsion");
		ASSERT_TRUE_MSG(std::fabs(Deg(sautBrut) - 90.f) < 0.1f, "des stations immobiles : le saut est la torsion entiere");
		// L'articulation garde sa part : deux os de torsion font TROIS sauts egaux (30
		// degres pour 90), et la somme des angles ne depasse jamais la torsion.
		const NkRtTwistChain part = NkRtMakeTwistChain(mode, 2u, true);
		float32 posesPart[kNkRtTwistMaxStations] = {};
		for (uint32 k = 0; k < part.count; ++k) {
			posesPart[k] = NkRtTwistStationAngle(part, k, torsion);
		}
		const float32 sautPart = NkRtTwistMaxStep(part, torsion, posesPart);
		std::printf("  [TW4] l'articulation garde sa part : stations a %.0f, %.0f, %.0f deg ; le plus grand saut %.1f deg (torsion 90)\n",
					(double)Deg(posesPart[0]), (double)Deg(posesPart[1]), (double)Deg(posesPart[2]), (double)Deg(sautPart));
		ASSERT_TRUE_MSG(std::fabs(Deg(sautPart) - 30.f) < 0.1f, "l'articulation garde sa part : deux os de torsion, trois sauts de 30 degres");
	}
	// Quatre cases pleines : la repartition fond la plus faible influence, la somme tient.
	{
		const NkRtTwistChain chaine = NkRtMakeTwistChain(NK_RT_TWIST_FOLLOW_CHILD, 2u);
		const uint32 station[3] = {0u, 1u, 2u};
		uint32 joints[4] = {0u, 5u, 6u, 7u};
		float32 poids[4] = {0.60f, 0.25f, 0.10f, 0.05f};
		const float32 deplace = NkRtTwistReweight(joints, poids, 4u, 0u, 0.25f, chaine, station);
		const float32 somme = poids[0] + poids[1] + poids[2] + poids[3];
		std::printf("  [TW4] quatre cases pleines, s = 0,25 : poids deplace %.3f, somme %.6f\n", (double)deplace, (double)somme);
		ASSERT_TRUE_MSG(std::fabs(somme - 1.f) < 1e-5f, "quatre cases pleines : la somme des poids reste 1");
		ASSERT_TRUE_MSG(deplace > 0.f && deplace <= 0.051f, "seule la plus faible influence (5 %) est fondue");
	}
}

// -----------------------------------------------------------------------------
// TW5 — RENDRE A L'ENFANT la torsion d'un os qui ne doit pas se tordre. L'avant-bras
// (plie de 60 degres) a ete tordu de 30 degres par une IK ; sa borne est 0. Apres :
// l'avant-bras est a 0, la main a pris 30 degres autour de l'axe de l'avant-bras, et
// elle n'a bouge NI de place NI d'orientation dans le monde.  [mutation : twist_no_carry]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TW5_La_torsion_d_un_os_rendue_a_son_enfant) {
	Mutations _m;
	for (int32 tordu = 0; tordu < 2; ++tordu) {
		NkRtTestHumanoid h;
		NkRtMakeTestHumanoid(h, tordu != 0);
		NkRtRig rig;
		ASSERT_TRUE(rig.Build(h.parents.Data(), (uint32)h.parents.Size(), h.names.Data()));
		const uint32 n = rig.Count();
		NkVector<NkMat4f> local, monde, mondeAvant;
		local.Resize(n);
		monde.Resize(n);
		mondeAvant.Resize(n);
		for (uint32 j = 0; j < n; ++j)
			local[j] = h.bindLocal[j];
		const NkRtQuat mainRef = QFromMat(h.bindLocal[(usize)h.handR]), avantRef = QFromMat(h.bindLocal[(usize)h.foreArmR]);
		const NkVec3f axe = AxeOs(h, h.handR);
		NkRtQuat avant = QMul(avantRef, QMul(QFromAxisAngle(AnyPerpendicular(axe), Rad(60.f)), QFromAxisAngle(axe, Rad(30.f))));
		NkRtQuat mainQ = QMul(QFromAxisAngle(AnyPerpendicular(axe), Rad(-15.f)), mainRef);
		auto Poser = [&](const NkRtQuat &qa, const NkRtQuat &qm) {
			local[(usize)h.foreArmR] = MatFrom(qa, V3(1.f, 1.f, 1.f), Origin(h.bindLocal[(usize)h.foreArmR]));
			local[(usize)h.handR] = MatFrom(qm, V3(1.f, 1.f, 1.f), Origin(h.bindLocal[(usize)h.handR]));
		};
		Poser(avant, mainQ);
		rig.LocalToWorld(local.Data(), mondeAvant.Data());
		const float32 mainAvant = NkRtChildTwist(mainRef, mainQ, axe);
		NkRtTwistLimit borne;
		borne.minAngle = 0.f;
		borne.maxAngle = 0.f;
		const NkRtTwistCarry r = NkRtLimitTwistToChild(avant, mainQ, avantRef, axe, borne);
		Poser(avant, mainQ);
		rig.LocalToWorld(local.Data(), monde.Data());
		const float32 orientation = Ecart(QFromMat(mondeAvant[(usize)h.handR]), QFromMat(monde[(usize)h.handR]));
		const float32 place = Distance(Origin(mondeAvant[(usize)h.handR]), Origin(monde[(usize)h.handR]));
		const float32 pris = NkRtChildTwist(mainRef, mainQ, axe) - mainAvant;
		std::printf("  [TW5] reperes %s : avant-bras %.2f -> %.2f deg (borne 0), la main a pris %.2f deg ; la main dans le monde : "
					"orientation a %.2e rad, place a %.2e m\n",
					tordu ? "quelconques" : "droits", (double)Deg(r.before), (double)Deg(r.after), (double)Deg(pris), (double)orientation,
					(double)place);
		ASSERT_TRUE_MSG(std::fabs(Deg(r.before) - 30.f) < 0.1f, "la torsion de l'avant-bras est mesuree a 30 degres");
		ASSERT_TRUE_MSG(std::fabs(Deg(r.after)) < 0.1f, "apres : l'avant-bras ne se tord plus");
		ASSERT_TRUE_MSG(std::fabs(Deg(pris) - 30.f) < 0.1f, "la main a pris les 30 degres autour de l'axe de l'avant-bras");
		ASSERT_TRUE_MSG(orientation < 1e-3f, "la main garde son orientation dans le monde (< 1e-3 rad)");
		ASSERT_TRUE_MSG(place < 1e-5f, "la main garde sa place dans le monde");
	}
}
