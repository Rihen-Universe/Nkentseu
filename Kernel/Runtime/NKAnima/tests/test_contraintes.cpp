// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_contraintes.cpp : LE BANC DES CONTRAINTES D'OS
// (Realtime/NkRtConstraints.h, 14/10/2026) : l'IK d'une chaine de N os, la visee,
// les limites d'angle (balancement en ellipse + torsion).
//
// D'ou il vient : le document produit de NkAnimaEditor (design/07, § 4.4, manque
// n° 7) demande une chaine IK a plus de deux os et des limites d'angle ; les yeux
// du patient de PV3DE et les personnages de Nogee ont besoin de regarder une cible.
//
// Chaque critere est MESURE (la valeur est imprimee) ; sa mutation, posee dans le
// MEME binaire par NK_RT_MUTATION, doit le faire rougir :
//
//   le bout de la chaine atteint la cible, la base tient     chain_no_anchor (CT1)
//   les joints interieurs plient du cote du pole             chain_pole_off  (CT2)
//   le dosage a 0,5 fait la moitie de l'angle                weight_full     (CT3, CT5)
//   la visee garde le haut (pas de roulis)                   aim_no_up       (CT4)
//   la visee ne sort pas de son cone                         aim_no_limit    (CT5)
//   le balancement reste dans l'ellipse, la torsion bornee   limit_off       (CT6)
//   la copie ignore la rotation du parent de la source       copy_world_naive (CT7) ; weight_full (CT7)
//   la chaine suit la courbe, pas le polygone des points     spline_chord    (CT8)
//   une chaine fausse est refusee                            chain_no_check  (CT9)
//   le genou ne claque pas pres de la jambe tendue           soft_reach_off  (CT10)
//   le passage FK -> IK ne fait pas sauter le membre         pole_flat       (CT11)
//   le pli de chaque joint reste borne pendant la resolution bend_free       (CT12)
//
//   NK_RT_MUTATION=chain_no_anchor ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKAnima/Realtime/NkRtConstraints.h"
#include "NKAnima/Realtime/NkRtHingeIK.h"

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

	constexpr float32 kDeg = 3.14159265358979f / 180.f;

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				uint32 bits = 0u;
				if (e != nullptr) {
					bits |= std::strstr(e, "chain_no_anchor") ? (uint32)NK_RTC_MUT_CHAIN_NO_ANCHOR : 0u;
					bits |= std::strstr(e, "chain_pole_off") ? (uint32)NK_RTC_MUT_CHAIN_POLE_OFF : 0u;
					bits |= std::strstr(e, "aim_no_limit") ? (uint32)NK_RTC_MUT_AIM_NO_LIMIT : 0u;
					bits |= std::strstr(e, "aim_no_up") ? (uint32)NK_RTC_MUT_AIM_NO_UP : 0u;
					bits |= std::strstr(e, "copy_world_naive") ? (uint32)NK_RTC_MUT_COPY_WORLD_NAIVE : 0u;
					bits |= std::strstr(e, "spline_chord") ? (uint32)NK_RTC_MUT_SPLINE_CHORD : 0u;
					bits |= std::strstr(e, "chain_no_check") ? (uint32)NK_RTC_MUT_CHAIN_NO_CHECK : 0u;
					bits |= std::strstr(e, "soft_reach_off") ? (uint32)NK_RTC_MUT_SOFT_REACH_OFF : 0u;
					bits |= std::strstr(e, "pole_flat") ? (uint32)NK_RTC_MUT_POLE_FLAT : 0u;
					bits |= std::strstr(e, "bend_free") ? (uint32)NK_RTC_MUT_BEND_FREE : 0u;
					bits |= std::strstr(e, "limit_off") ? (uint32)NK_RTC_MUT_LIMIT_OFF : 0u;
					bits |= std::strstr(e, "weight_full") ? (uint32)NK_RTC_MUT_WEIGHT_FULL : 0u;
				}
				NkRtConstraintSetMutations(bits);
			}
			~Mutations() {
				NkRtConstraintSetMutations(0u);
			}
	};

	float32 AngleDeg(const NkVec3f &a, const NkVec3f &b) {
		// atan2 et non acos : pres de 0, acos en float perd 0,02 degre (l'instrument mentait).
		return std::atan2(Length(Cross(a, b)), Dot(a, b)) / kDeg;
	}
	float32 EcartMat(const NkMat4f &a, const NkMat4f &b) {
		float32 m = 0.f;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				m = std::fmax(m, std::fabs(a[i][j] - b[i][j]));
		return m;
	}

	/// Un generateur reproductible (le banc ne depend pas de l'horloge).
	struct Lcg {
			uint32 s = 12345u;
			float32 U() { // [0, 1)
				s = s * 1664525u + 1013904223u;
				return (float32)(s >> 8) / 16777216.f;
			}
			float32 S() { // [-1, 1)
				return 2.f * U() - 1.f;
			}
	};

	/// UN TENTACULE de neuf joints (0..8) aux reperes QUELCONQUES, plus une ventouse
	/// (9) au bout : la chaine est courbe au repos, ses os font 15 cm.
	struct Tentacule {
			NkRtRig rig;
			NkVector<NkMat4f> local, monde;
			static constexpr uint32 kN = 10u;
			bool Monter(float32 courbure = 1.f) {
				int32 parents[kN];
				const char *noms[kN] = {"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "ventouse"};
				for (uint32 j = 0; j < kN; ++j) {
					parents[j] = (int32)j - 1;
				}
				if (!rig.Build(parents, kN, noms)) {
					return false;
				}
				local.Resize(kN);
				monde.Resize(kN);
				for (uint32 j = 0; j < kN; ++j) {
					const float32 a = 0.9f + 1.7f * (float32)j;
					const NkVec3f axe = Normalize(V3(std::sin(a), std::cos(0.7f * a), std::sin(1.9f * a + 0.3f)), V3(1.f, 0.f, 0.f));
					const NkRtQuat q = QFromAxisAngle(axe, courbure * (0.25f + 0.05f * (float32)(j % 3u)));
					const NkVec3f o = j == 0 ? V3(0.f, 1.f, 0.f) : V3(0.f, j == kN - 1 ? 0.05f : 0.15f, 0.f);
					local[j] = MatFrom(q, V3(1.f, 1.f, 1.f), o);
				}
				rig.LocalToWorld(local.Data(), monde.Data());
				return true;
			}
	};

	/// Les distances entre joints successifs d'une chaine.
	void Longueurs(const NkVector<NkMat4f> &m, const uint32 *c, uint32 n, float32 *out) {
		for (uint32 k = 0; k + 1 < n; ++k) {
			out[k] = Distance(Origin(m[c[k]]), Origin(m[c[k + 1]]));
		}
	}

	/// L'humanoide d'essai aux reperes quelconques, au repos.
	struct Humain {
			NkRtTestHumanoid h;
			NkRtRig rig;
			NkVector<NkMat4f> local, monde;
			bool Monter() {
				NkRtMakeTestHumanoid(h, true);
				if (!rig.Build(h.parents.Data(), (uint32)h.parents.Size(), h.names.Data())) {
					return false;
				}
				local.Resize(rig.Count());
				monde.Resize(rig.Count());
				for (uint32 j = 0; j < rig.Count(); ++j) {
					local[j] = h.bindLocal[j];
				}
				rig.LocalToWorld(local.Data(), monde.Data());
				return true;
			}
	};

} // namespace

// -----------------------------------------------------------------------------
// CT1 : L'IK D'UNE CHAINE DE NEUF JOINTS. Douze cibles a portee : le bout arrive
// (0,1 mm), la base ne bouge pas, aucun os ne s'allonge, la ventouse suit son os.
// Puis une chaine a os INTERMEDIAIRES (un joint sur deux) : memes criteres, et les
// os sautes restent rigides sur leur ancetre. Hors d'allonge : la chaine se tend
// droit vers la cible.                                   [mutation : chain_no_anchor]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT1_IK_d_une_chaine_de_neuf_joints) {
	Mutations _m;
	Tentacule t;
	ASSERT_TRUE(t.Monter());
	const uint32 chaine[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	const uint32 sautee[5] = {0, 2, 4, 6, 8};
	float32 pireBout = 0.f, pireBase = 0.f, pireLong = 0.f, pireVentouse = 0.f, pireRigide = 0.f;
	uint32 passesMax = 0;
	Lcg g;
	for (int32 essai = 0; essai < 24; ++essai) {
		const bool intermediaires = essai >= 12;
		const uint32 *c = intermediaires ? sautee : chaine;
		const uint32 n = intermediaires ? 5u : 9u;
		NkVector<NkMat4f> m = t.monde;
		float32 avant[9], apres[9];
		Longueurs(m, c, n, avant);
		const NkVec3f base = Origin(m[0]);
		const NkVec3f dir = Normalize(V3(g.S(), g.S(), g.S()), V3(1.f, 0.f, 0.f));
		const NkVec3f cible = Madd(base, dir, 0.25f + 0.55f * g.U()); // la chaine fait 1,20 m
		NkRtChainIKSettings s;
		const NkRtChainIKResult r = NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), c, n, cible, s);
		Longueurs(m, c, n, apres);
		pireBout = std::fmax(pireBout, Distance(Origin(m[c[n - 1]]), cible));
		pireBase = std::fmax(pireBase, Distance(Origin(m[0]), base));
		for (uint32 k = 0; k + 1 < n; ++k) {
			pireLong = std::fmax(pireLong, std::fabs(apres[k] - avant[k]));
		}
		pireVentouse = std::fmax(pireVentouse, EcartMat(m[9], m[8] * t.local[9]));
		if (intermediaires) {
			for (uint32 j = 1; j < 9; j += 2) {
				pireRigide = std::fmax(pireRigide, EcartMat(m[j], m[j - 1] * t.local[j]));
			}
		}
		passesMax = passesMax > r.iterations ? passesMax : r.iterations;
	}
	std::printf("  [CT1] 24 cibles (12 sur 9 joints, 12 sur 5 joints a os intermediaires) : bout a %.3f mm, base a %.6f mm, os allonges de "
				"%.6f mm au pire, ventouse a %.2e, os sautes a %.2e de leur ancetre ; %u passes au plus\n",
				(double)(pireBout * 1000.f), (double)(pireBase * 1000.f), (double)(pireLong * 1000.f), (double)pireVentouse, (double)pireRigide,
				passesMax);
	ASSERT_TRUE_MSG(pireBout < 1e-4f, "le bout de la chaine atteint la cible (0,1 mm)");
	ASSERT_TRUE_MSG(pireBase < 1e-6f, "la base ne bouge pas");
	ASSERT_TRUE_MSG(pireLong < 1e-5f, "aucun os ne s'allonge");
	ASSERT_TRUE_MSG(pireVentouse < 1e-5f && pireRigide < 1e-5f, "les os hors de la chaine suivent leur parent");

	// Hors d'allonge : tendue droit vers la cible.
	NkVector<NkMat4f> m = t.monde;
	const NkVec3f base = Origin(m[0]);
	const NkVec3f cible = Add(base, V3(2.f, 0.5f, -1.f));
	const NkRtChainIKResult r = NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), chaine, 9u, cible, NkRtChainIKSettings());
	float32 pireDroit = 0.f;
	for (uint32 k = 1; k < 9; ++k) {
		pireDroit = std::fmax(pireDroit, AngleDeg(Sub(Origin(m[k]), base), Sub(cible, base)));
	}
	const float32 attendu = Length(Sub(cible, base)) - 1.2f;
	std::printf("  [CT1] hors d'allonge : joints a %.4f deg de la droite base-cible ; il manque %.4f m (attendu %.4f)\n", (double)pireDroit,
				(double)r.distance, (double)attendu);
	ASSERT_TRUE_MSG(pireDroit < 0.01f && std::fabs(r.distance - attendu) < 1e-4f && !r.reached, "hors d'allonge, la chaine se tend vers la cible");
}

// -----------------------------------------------------------------------------
// CT2 : LE POLE. La chaine a cinq joints part pliee d'un cote ; le pole est mis de
// l'AUTRE cote : chaque joint interieur finit du cote du pole, vu de la droite de
// ses deux voisins (l'angle entre le joint et le pole autour de cette droite est
// nul) ; le bout atteint toujours la cible.                 [mutation : chain_pole_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT2_Le_pole_choisit_le_cote_du_pli) {
	Mutations _m;
	Tentacule t;
	ASSERT_TRUE(t.Monter());
	const uint32 c[5] = {0, 2, 4, 6, 8};
	float32 pireCote = 0.f, pireBout = 0.f;
	for (int32 essai = 0; essai < 2; ++essai) {
		NkVector<NkMat4f> m = t.monde;
		const NkVec3f base = Origin(m[0]), bout = Origin(m[8]);
		// La cible : aux deux tiers de la droite base-bout (la chaine doit plier).
		const NkVec3f axe = Normalize(Sub(bout, base), V3(0.f, 1.f, 0.f));
		const NkVec3f cible = Madd(base, axe, 0.66f * Distance(base, bout));
		// Le cote ou la chaine plie au depart, et le pole a l'oppose (puis du meme cote).
		NkVec3f milieu = Sub(Origin(m[4]), base);
		milieu = Normalize(Sub(milieu, Scale(axe, Dot(milieu, axe))), AnyPerpendicular(axe));
		const NkVec3f pole = Madd(Lerp(base, cible, 0.5f), milieu, essai == 0 ? -1.f : 1.f);
		NkRtChainIKSettings s;
		s.usePole = true;
		s.pole = pole;
		NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), c, 5u, cible, s);
		pireBout = std::fmax(pireBout, Distance(Origin(m[8]), cible));
		// Autour de la droite base-cible : l'angle entre chaque joint interieur et le pole.
		const NkVec3f d = Normalize(Sub(cible, base), V3(0.f, 1.f, 0.f));
		NkVec3f vp = Sub(pole, base);
		vp = Sub(vp, Scale(d, Dot(vp, d)));
		for (uint32 k = 1; k + 1 < 5u; ++k) {
			NkVec3f vj = Sub(Origin(m[c[k]]), base);
			vj = Sub(vj, Scale(d, Dot(vj, d)));
			pireCote = std::fmax(pireCote, AngleDeg(vj, vp));
		}
	}
	std::printf("  [CT2] pole a l'oppose du pli de depart, puis du meme cote : joints interieurs a %.4f deg du demi-plan du pole au pire "
				"(autour de la droite base-cible) ; bout a %.3f mm\n",
				(double)pireCote, (double)(pireBout * 1000.f));
	ASSERT_TRUE_MSG(pireCote < 1.f, "chaque joint interieur plie du cote du pole, dans le plan qui le contient");
	ASSERT_TRUE_MSG(pireBout < 1e-4f, "avec le pole, le bout atteint toujours la cible");
}

// -----------------------------------------------------------------------------
// CT3 : LE DOSAGE DE LA CHAINE EST UN VRAI POIDS. Au poids 0 : la pose donnee, au
// bit pres. Au poids 0,5 : chaque rotation locale de la chaine a fait la MOITIE de
// l'angle qu'elle fait au poids 1, autour du meme axe (le point milieu : c'est lui
// qui distingue un poids d'un tout-ou-rien, note du 26/09).    [mutation : weight_full]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT3_Le_dosage_de_la_chaine_est_un_vrai_poids) {
	Mutations _m;
	Tentacule t;
	ASSERT_TRUE(t.Monter());
	const uint32 c[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	const NkVec3f cible = Add(Origin(t.monde[0]), V3(0.45f, 0.35f, 0.3f));
	NkRtChainIKSettings s;
	NkVector<NkMat4f> zero = t.monde, demi = t.monde, plein = t.monde;
	s.weight = 0.f;
	NkRtSolveChainIK(t.rig, t.local.Data(), zero.Data(), c, 9u, cible, s);
	const bool intact = std::memcmp(zero.Data(), t.monde.Data(), sizeof(NkMat4f) * t.monde.Size()) == 0;
	s.weight = 1.f;
	NkRtSolveChainIK(t.rig, t.local.Data(), plein.Data(), c, 9u, cible, s);
	s.weight = 0.5f;
	NkRtSolveChainIK(t.rig, t.local.Data(), demi.Data(), c, 9u, cible, s);
	NkVector<NkMat4f> lp, ld;
	lp.Resize(t.rig.Count());
	ld.Resize(t.rig.Count());
	t.rig.WorldToLocal(plein.Data(), lp.Data());
	t.rig.WorldToLocal(demi.Data(), ld.Data());
	float32 pire = 0.f, angleMax = 0.f;
	for (uint32 k = 0; k < 9; ++k) {
		const NkRtQuat q0 = QFromMat(t.local[k]);
		const NkVec3f vPlein = QToRotVec(QMul(QConj(q0), QFromMat(lp[k])));
		const NkVec3f vDemi = QToRotVec(QMul(QConj(q0), QFromMat(ld[k])));
		// La moitie du vecteur rotation : meme axe, moitie de l'angle.
		pire = std::fmax(pire, Length(Sub(vDemi, Scale(vPlein, 0.5f))) / kDeg);
		angleMax = std::fmax(angleMax, Length(vPlein) / kDeg);
	}
	std::printf("  [CT3] poids 0 : pose intacte au bit pres = %d ; poids 0,5 : chaque os a %.4f deg de la moitie de sa rotation au poids 1 "
				"(la plus grande y fait %.2f deg)\n",
				intact ? 1 : 0, (double)pire, (double)angleMax);
	ASSERT_TRUE_MSG(intact, "au poids 0, la pose donnee au bit pres");
	ASSERT_TRUE_MSG(angleMax > 5.f, "le cas mesure fait vraiment tourner la chaine");
	ASSERT_TRUE_MSG(pire < 0.01f, "au poids 0,5, chaque os fait la moitie de son angle");
}

// -----------------------------------------------------------------------------
// CT4 : LA VISEE, SANS ROULIS. La tete de l'humanoide aux reperes quelconques vise
// quinze cibles devant elle (lacet de -60 a 60, tangage de -35 a 35 degres) : son
// axe d'avant les atteint (0,01 deg), son haut reste dans le plan vertical de la
// visee (aucun roulis) et du cote du ciel, la queue de cheval suit.
//                                                          [mutation : aim_no_up]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT4_La_visee_sans_roulis) {
	Mutations _m;
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	const uint32 tete = (uint32)hu.h.head;
	const NkRtQuat q0 = QFromMat(hu.monde[tete]);
	NkRtAimSettings s;
	s.aimAxis = QRotate(QConj(q0), V3(0.f, 0.f, 1.f)); // l'avant du personnage, dit dans le repere de la tete
	s.upAxis = QRotate(QConj(q0), V3(0.f, 1.f, 0.f));
	float32 pireVisee = 0.f, pireRoulis = 0.f, pireQueue = 0.f, minHaut = 1.f;
	for (int32 i = 0; i < 15; ++i) {
		const float32 lacet = (-60.f + 120.f * (float32)(i % 5) / 4.f) * kDeg;
		const float32 tangage = (-35.f + 35.f * (float32)(i / 5)) * kDeg;
		const NkVec3f dir = V3(std::sin(lacet) * std::cos(tangage), std::sin(tangage), std::cos(lacet) * std::cos(tangage));
		NkVector<NkMat4f> m = hu.monde;
		const NkVec3f cible = Madd(Origin(m[tete]), dir, 2.f);
		const float32 reste = NkRtAimBone(hu.rig, hu.local.Data(), m.Data(), tete, cible, s);
		const NkRtQuat q = QFromMat(m[tete]);
		const NkVec3f avant = QRotate(q, s.aimAxis), haut = QRotate(q, s.upAxis);
		pireVisee = std::fmax(pireVisee, std::fmax(AngleDeg(avant, dir), reste / kDeg));
		const NkVec3f cote = Normalize(Cross(avant, V3(0.f, 1.f, 0.f)), V3(1.f, 0.f, 0.f));
		pireRoulis = std::fmax(pireRoulis, std::fabs(std::asin(Clamp(Dot(haut, cote), -1.f, 1.f))) / kDeg);
		minHaut = std::fmin(minHaut, haut.y);
		pireQueue = std::fmax(pireQueue, EcartMat(m[(uint32)hu.h.tail0], m[tete] * hu.local[(uint32)hu.h.tail0]));
	}
	std::printf("  [CT4] 15 cibles : visee a %.4f deg, roulis %.4f deg au pire, haut de la tete a y >= %.3f, queue de cheval a %.2e de la tete\n",
				(double)pireVisee, (double)pireRoulis, (double)minHaut, (double)pireQueue);
	ASSERT_TRUE_MSG(pireVisee < 0.01f, "l'axe d'avant atteint chaque cible");
	ASSERT_TRUE_MSG(pireRoulis < 0.01f && minHaut > 0.f, "le haut de la tete reste dans le plan vertical de la visee, du cote du ciel");
	ASSERT_TRUE_MSG(pireQueue < 1e-5f, "le sous-arbre de l'os suit");
}

// -----------------------------------------------------------------------------
// CT5 : LE CONE ET LE DOSAGE DE LA VISEE. Un cone de 30 degres : une cible a 20
// degres est atteinte ; une cible a 100 degres arrete l'axe a 30 degres EXACTEMENT,
// dans le plan (visee de depart, cible), et la fonction rend les 70 qui manquent.
// Au poids 0,5 (sans cone), la tete fait la moitie de la rotation du poids 1.
//                                       [mutations : aim_no_limit ; weight_full]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT5_Le_cone_et_le_dosage_de_la_visee) {
	Mutations _m;
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	const uint32 tete = (uint32)hu.h.head;
	const NkRtQuat q0 = QFromMat(hu.monde[tete]);
	NkRtAimSettings s;
	s.aimAxis = QRotate(QConj(q0), V3(0.f, 0.f, 1.f));
	s.upAxis = QRotate(QConj(q0), V3(0.f, 1.f, 0.f));
	s.maxAngle = 30.f * kDeg;
	const NkVec3f a0 = V3(0.f, 0.f, 1.f);
	// Dans le cone.
	NkVector<NkMat4f> m = hu.monde;
	const NkVec3f d20 = V3(std::sin(20.f * kDeg), 0.f, std::cos(20.f * kDeg));
	const float32 r20 = NkRtAimBone(hu.rig, hu.local.Data(), m.Data(), tete, Madd(Origin(m[tete]), d20, 2.f), s);
	// Hors du cone : 100 degres a gauche et un peu plus haut.
	m = hu.monde;
	const NkVec3f d100 = Normalize(V3(std::sin(100.f * kDeg), 0.2f, std::cos(100.f * kDeg)), V3(1.f, 0.f, 0.f));
	const float32 r100 = NkRtAimBone(hu.rig, hu.local.Data(), m.Data(), tete, Madd(Origin(m[tete]), d100, 2.f), s);
	const NkVec3f avant = QRotate(QFromMat(m[tete]), s.aimAxis);
	const float32 ouverture = AngleDeg(a0, avant);
	const float32 horsPlan = std::fabs(Dot(avant, Normalize(Cross(a0, d100), V3(0.f, 1.f, 0.f))));
	const float32 attenduReste = AngleDeg(a0, d100) - 30.f;
	std::printf("  [CT5] cone de 30 deg : cible a 20 deg atteinte a %.4f deg ; cible a %.2f deg : axe a %.4f deg du depart, hors du plan %.2e, "
				"reste %.4f deg (attendu %.4f)\n",
				(double)(r20 / kDeg), (double)AngleDeg(a0, d100), (double)ouverture, (double)horsPlan, (double)(r100 / kDeg), (double)attenduReste);
	ASSERT_TRUE_MSG(r20 / kDeg < 0.01f, "une cible dans le cone est atteinte");
	ASSERT_TRUE_MSG(std::fabs(ouverture - 30.f) < 0.01f && horsPlan < 1e-4f, "hors du cone, l'axe s'arrete au bord, vers la cible");
	ASSERT_TRUE_MSG(std::fabs(r100 / kDeg - attenduReste) < 0.01f, "la fonction rend l'angle qui manque");

	// Le dosage.
	s.maxAngle = 3.14159265f;
	const NkVec3f d = Normalize(V3(std::sin(50.f * kDeg), std::sin(15.f * kDeg), std::cos(50.f * kDeg)), V3(0.f, 0.f, 1.f));
	NkVector<NkMat4f> plein = hu.monde, demi = hu.monde;
	s.weight = 1.f;
	NkRtAimBone(hu.rig, hu.local.Data(), plein.Data(), tete, Madd(Origin(plein[tete]), d, 2.f), s);
	s.weight = 0.5f;
	NkRtAimBone(hu.rig, hu.local.Data(), demi.Data(), tete, Madd(Origin(demi[tete]), d, 2.f), s);
	const NkVec3f vPlein = QToRotVec(QMul(QConj(q0), QFromMat(plein[tete])));
	const NkVec3f vDemi = QToRotVec(QMul(QConj(q0), QFromMat(demi[tete])));
	const float32 ecart = Length(Sub(vDemi, Scale(vPlein, 0.5f))) / kDeg;
	std::printf("  [CT5] poids 0,5 : la tete a %.4f deg de la moitie de sa rotation au poids 1 (%.2f deg)\n", (double)ecart,
				(double)(Length(vPlein) / kDeg));
	ASSERT_TRUE_MSG(ecart < 0.01f, "au poids 0,5, la tete fait la moitie de la rotation");
}

// -----------------------------------------------------------------------------
// CT6 : LES LIMITES D'ANGLE. Un balancement admis en ellipse (40 x 20 degres) et
// une torsion de -30 a 50 degres, autour d'une reference quelconque. 400 rotations
// tirees au hasard (jusqu'a 150 degres) : apres la borne, toutes sont dans
// l'ellipse et dans la torsion ; celles qui y etaient sont rendues au bit pres ; un
// balancement borne garde sa direction. Puis sur un os de la pose monde (l'avant-
// bras) : borne, et la main suit.                             [mutation : limit_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT6_Les_limites_d_angle) {
	Mutations _m;
	NkRtSwingTwistLimit lim;
	lim.twistAxis = Normalize(V3(0.2f, 1.f, 0.1f), V3(0.f, 1.f, 0.f));
	lim.swingAxis = V3(0.f, 0.f, 1.f);
	lim.swing1 = 40.f * kDeg;
	lim.swing2 = 20.f * kDeg;
	lim.twistMin = -30.f * kDeg;
	lim.twistMax = 50.f * kDeg;
	const NkRtQuat ref = QFromAxisAngle(Normalize(V3(0.4f, -0.3f, 0.8f), V3(1.f, 0.f, 0.f)), 1.1f);
	Lcg g;
	uint32 dedans = 0, bornees = 0, bitFaux = 0;
	float32 pireEllipse = 0.f, pireTorsion = 0.f, pireDirection = 0.f;
	for (int32 i = 0; i < 400; ++i) {
		const NkVec3f v = Scale(Normalize(V3(g.S(), g.S(), g.S()), V3(1.f, 0.f, 0.f)), 150.f * kDeg * g.U());
		const NkRtQuat avant = QMul(ref, QFromRotVec(v));
		float32 s1a, s2a, ta;
		NkRtMeasureSwingTwist(avant, ref, lim, s1a, s2a, ta);
		NkRtQuat q = avant;
		const bool borne = NkRtLimitSwingTwist(q, ref, lim);
		float32 s1, s2, tw;
		NkRtMeasureSwingTwist(q, ref, lim, s1, s2, tw);
		const float32 e = (s1 / lim.swing1) * (s1 / lim.swing1) + (s2 / lim.swing2) * (s2 / lim.swing2);
		pireEllipse = std::fmax(pireEllipse, e);
		pireTorsion = std::fmax(pireTorsion, std::fmax(lim.twistMin - tw, tw - lim.twistMax) / kDeg);
		if (!borne) {
			++dedans;
			bitFaux += std::memcmp(&q, &avant, sizeof(NkRtQuat)) != 0 ? 1u : 0u;
		} else {
			++bornees;
			if (s1a * s1a + s2a * s2a > (2.f * kDeg) * (2.f * kDeg)) {
				const float32 da = std::atan2(s2a, s1a), dp = std::atan2(s2, s1);
				float32 dd = std::fabs(da - dp);
				dd = std::fmin(dd, 2.f * 3.14159265f - dd);
				pireDirection = std::fmax(pireDirection, dd / kDeg);
			}
		}
	}
	std::printf("  [CT6] 400 rotations : %u admises (%u changees), %u bornees ; ellipse a %.6f au pire (1 = le bord), torsion a %.4f deg au-dela "
				"au pire, direction du balancement a %.4f deg\n",
				dedans, bitFaux, bornees, (double)pireEllipse, (double)pireTorsion, (double)pireDirection);
	ASSERT_TRUE_MSG(dedans > 20u && bornees > 20u, "le tirage exerce les deux cas");
	ASSERT_TRUE_MSG(pireEllipse < 1.001f, "le balancement reste dans l'ellipse");
	ASSERT_TRUE_MSG(pireTorsion < 0.01f, "la torsion reste dans ses bornes");
	ASSERT_TRUE_MSG(bitFaux == 0u, "une rotation admise est rendue au bit pres");
	ASSERT_TRUE_MSG(pireDirection < 0.05f, "un balancement borne garde sa direction");

	// Sur un os de la pose monde : l'avant-bras gauche plie de 120 degres.
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	const uint32 avantBras = (uint32)hu.h.foreArmL, main = (uint32)hu.h.handL;
	const NkRtQuat refLocal = QFromMat(hu.local[avantBras]);
	NkVector<NkMat4f> local = hu.local, m;
	m.Resize(hu.rig.Count());
	NkRtSwingTwistLimit coude;
	coude.twistAxis = Normalize(Origin(hu.local[main]), V3(0.f, 1.f, 0.f)); // l'axe de l'avant-bras, dans son repere
	coude.swingAxis = Normalize(Cross(coude.twistAxis, V3(1.f, 0.f, 0.f)), V3(0.f, 0.f, 1.f));
	local[avantBras] = MatFrom(QMul(refLocal, QFromAxisAngle(coude.swingAxis, 120.f * kDeg)), V3(1.f, 1.f, 1.f), Origin(local[avantBras]));
	hu.rig.LocalToWorld(local.Data(), m.Data());
	coude.swing1 = 60.f * kDeg;
	coude.swing2 = 60.f * kDeg;
	const bool borne = NkRtLimitBone(hu.rig, local.Data(), m.Data(), avantBras, refLocal, coude);
	NkVector<NkMat4f> l2;
	l2.Resize(hu.rig.Count());
	hu.rig.WorldToLocal(m.Data(), l2.Data());
	float32 s1, s2, tw;
	NkRtMeasureSwingTwist(QFromMat(l2[avantBras]), refLocal, coude, s1, s2, tw);
	const float32 bal = std::sqrt(s1 * s1 + s2 * s2) / kDeg;
	const float32 suit = EcartMat(m[main], m[avantBras] * local[main]);
	std::printf("  [CT6] avant-bras plie de 120 deg, borne a 60 : balancement %.4f deg, la main a %.2e de son os\n", (double)bal, (double)suit);
	ASSERT_TRUE_MSG(borne && std::fabs(bal - 60.f) < 0.01f, "l'os de la pose est ramene au bord de sa limite");
	ASSERT_TRUE_MSG(suit < 1e-5f, "la main suit l'avant-bras borne");
}

// -----------------------------------------------------------------------------
// CT7 : LA COPIE DE ROTATION. La tete (cible) copie l'avant-bras gauche (source),
// sur l'humanoide aux reperes quelconques. L'avant-bras tourne de 50 degres autour
// d'un axe quelconque de son repere : (a) la tete tourne de la MEME rotation du
// monde, autour de son pivot, et la queue de cheval suit ; (b) au facteur 0,5, de
// la moitie ; (c) le thorax (parent de la source) tourne de 40 degres, la source
// restant a sa reference : la tete ne bouge pas (l'oeil ne bouge pas quand la tete
// tourne : la paupiere non plus) ; (d) torsion seule : la tete ne prend que les 25
// degres de torsion, autour de l'axe de la source ; (e) en repere local : local
// de la tete = local donne * ecart.     [mutations : copy_world_naive ; weight_full]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT7_La_copie_de_rotation) {
	Mutations _m;
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	const uint32 src = (uint32)hu.h.foreArmL, tete = (uint32)hu.h.head, queue = (uint32)hu.h.tail0;
	const NkRtQuat ref = QFromMat(hu.local[src]);
	const NkVec3f axe = Normalize(V3(0.6f, -0.3f, 0.75f), V3(1.f, 0.f, 0.f));
	const NkRtQuat D = QFromAxisAngle(axe, 50.f * kDeg);
	NkVector<NkMat4f> local = hu.local, avant;
	avant.Resize(hu.rig.Count());
	local[src] = MatFrom(QMul(ref, D), V3(1.f, 1.f, 1.f), Origin(local[src]));
	hu.rig.LocalToWorld(local.Data(), avant.Data());
	const NkRtQuat refMonde = QMul(QFromMat(avant[(uint32)hu.rig.Parent(src)]), ref);
	const NkRtQuat Dw = QMul(QMul(refMonde, D), QConj(refMonde));
	const NkRtQuat teteAvant = QFromMat(avant[tete]);
	NkRtCopyRotationSettings s;
	// (a) et (b)
	float32 ecartPlein = 0.f, ecartDemi = 0.f, pivot = 0.f, suit = 0.f;
	{
		NkVector<NkMat4f> m = avant;
		NkRtCopyRotation(hu.rig, local.Data(), m.Data(), src, ref, tete, s);
		const NkVec3f fait = QToRotVec(QMul(QFromMat(m[tete]), QConj(teteAvant)));
		ecartPlein = Length(Sub(fait, QToRotVec(Dw))) / kDeg;
		pivot = Distance(Origin(m[tete]), Origin(avant[tete]));
		suit = EcartMat(m[queue], m[tete] * local[queue]);
		m = avant;
		s.factor = 0.5f;
		NkRtCopyRotation(hu.rig, local.Data(), m.Data(), src, ref, tete, s);
		const NkVec3f demi = QToRotVec(QMul(QFromMat(m[tete]), QConj(teteAvant)));
		ecartDemi = Length(Sub(demi, Scale(QToRotVec(Dw), 0.5f))) / kDeg;
		s.factor = 1.f;
	}
	// (c) le parent de la source tourne, la source a sa reference.
	float32 bougeParent = 0.f;
	{
		NkVector<NkMat4f> l = hu.local, m;
		m.Resize(hu.rig.Count());
		const uint32 thorax = (uint32)hu.h.chest;
		l[thorax] = MatFrom(QMul(QFromMat(l[thorax]), QFromAxisAngle(V3(0.f, 1.f, 0.f), 40.f * kDeg)), V3(1.f, 1.f, 1.f), Origin(l[thorax]));
		hu.rig.LocalToWorld(l.Data(), m.Data());
		const NkMat4f t0 = m[tete];
		NkRtCopyRotation(hu.rig, l.Data(), m.Data(), src, ref, tete, s);
		bougeParent = EcartMat(m[tete], t0);
	}
	// (d) torsion seule : 30 degres de balancement puis 25 de torsion autour de l'axe de l'os.
	float32 ecartTorsion = 0.f;
	{
		const NkVec3f axeOs = Normalize(Origin(hu.local[(uint32)hu.h.handL]), V3(0.f, 1.f, 0.f));
		const NkRtQuat sw = QFromAxisAngle(Normalize(Cross(axeOs, V3(0.f, 0.f, 1.f)), V3(1.f, 0.f, 0.f)), 30.f * kDeg);
		const NkRtQuat tw = QFromAxisAngle(axeOs, 25.f * kDeg);
		NkVector<NkMat4f> l = hu.local, m;
		m.Resize(hu.rig.Count());
		l[src] = MatFrom(QMul(ref, QMul(sw, tw)), V3(1.f, 1.f, 1.f), Origin(l[src]));
		hu.rig.LocalToWorld(l.Data(), m.Data());
		const NkRtQuat t0 = QFromMat(m[tete]);
		NkRtCopyRotationSettings st;
		st.twistOnly = true;
		st.twistAxis = axeOs;
		NkRtCopyRotation(hu.rig, l.Data(), m.Data(), src, ref, tete, st);
		const NkRtQuat rm = QMul(QFromMat(m[(uint32)hu.rig.Parent(src)]), ref);
		const NkVec3f attendu = Scale(QRotate(rm, axeOs), 25.f * kDeg);
		ecartTorsion = Length(Sub(QToRotVec(QMul(QFromMat(m[tete]), QConj(t0))), attendu)) / kDeg;
	}
	// (e) en repere local.
	float32 ecartLocal = 0.f;
	{
		NkVector<NkMat4f> m = avant, l2;
		l2.Resize(hu.rig.Count());
		NkRtCopyRotationSettings sl;
		sl.space = NkRtCopySpace::NK_LOCAL;
		NkRtCopyRotation(hu.rig, local.Data(), m.Data(), src, ref, tete, sl);
		hu.rig.WorldToLocal(m.Data(), l2.Data());
		ecartLocal = Length(QToRotVec(QMul(QConj(QMul(QFromMat(local[tete]), D)), QFromMat(l2[tete])))) / kDeg;
	}
	std::printf("  [CT7] (a) meme rotation du monde a %.4f deg, pivot a %.2e m, queue de cheval a %.2e ; (b) facteur 0,5 a %.4f deg de la moitie ; "
				"(c) le thorax tourne de 40 deg : la tete bouge de %.2e ; (d) torsion seule a %.4f deg ; (e) repere local a %.4f deg\n",
				(double)ecartPlein, (double)pivot, (double)suit, (double)ecartDemi, (double)bougeParent, (double)ecartTorsion, (double)ecartLocal);
	ASSERT_TRUE_MSG(ecartPlein < 0.01f && pivot < 1e-6f && suit < 1e-5f, "la cible prend la rotation du monde de la source, autour de son pivot");
	ASSERT_TRUE_MSG(ecartDemi < 0.01f, "au facteur 0,5, la moitie de la rotation");
	ASSERT_TRUE_MSG(bougeParent < 1e-5f, "la rotation du parent de la source n'est pas copiee");
	ASSERT_TRUE_MSG(ecartTorsion < 0.01f, "torsion seule : la cible ne prend que la torsion");
	ASSERT_TRUE_MSG(ecartLocal < 0.01f, "en repere local : local donne * ecart");
}

namespace {
	/// La Catmull-Rom uniforme du banc, ecrite ICI (independante de celle du produit),
	/// bouts prolonges par reflexion, echantillonnee tres finement.
	void CourbeTemoin(const NkVec3f *q, uint32 m, NkVector<NkVec3f> &out) {
		out.Clear();
		for (uint32 i = 0; i + 1 < m; ++i) {
			const NkVec3f p1 = q[i], p2 = q[i + 1];
			const NkVec3f p0 = i > 0 ? q[i - 1] : Sub(Scale(p1, 2.f), p2);
			const NkVec3f p3 = i + 2 < m ? q[i + 2] : Sub(Scale(p2, 2.f), p1);
			for (uint32 j = 0; j <= 500; ++j) {
				const float32 t = (float32)j / 500.f;
				// La forme matricielle de Catmull-Rom (tension 1/2).
				NkVec3f r = Scale(p1, 2.f);
				r = Add(r, Scale(Sub(p2, p0), t));
				r = Add(r, Scale(Add(Sub(Scale(p0, 2.f), Scale(p1, 5.f)), Sub(Scale(p2, 4.f), p3)), t * t));
				r = Add(r, Scale(Add(Sub(Scale(p1, 3.f), p0), Sub(p3, Scale(p2, 3.f))), t * t * t));
				out.PushBack(Scale(r, 0.5f));
			}
		}
	}
	/// La distance d'un point a une ligne brisee.
	float32 DistanceALigne(const NkVec3f &x, const NkVector<NkVec3f> &l) {
		float32 d = 1e9f;
		for (usize i = 0; i + 1 < l.Size(); ++i) {
			const NkVec3f ab = Sub(l[i + 1], l[i]);
			const float32 aa = Dot(ab, ab);
			const float32 t = aa > 0.f ? Clamp(Dot(Sub(x, l[i]), ab) / aa, 0.f, 1.f) : 0.f;
			d = std::fmin(d, Distance(x, Madd(l[i], ab, t)));
		}
		return d;
	}
} // namespace

// -----------------------------------------------------------------------------
// CT8 : LA CHAINE SUR UNE COURBE (la colonne, la queue, le serpent). Le tentacule de
// neuf joints (1,20 m) sur une courbe en S de cinq points, donnee a 0,4 m de la base
// (elle est ramenee sur la base) : chaque joint est SUR la courbe (mesuree par une
// Catmull-Rom ecrite dans le banc), la base ne bouge pas, aucun os ne s'allonge,
// rien ne depasse. Sur une courbe trop courte (0,5 m), la suite continue droit et
// la fonction le dit. Au poids 0 : la pose donnee au bit pres.   [mutation : spline_chord]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT8_La_chaine_sur_une_courbe) {
	Mutations _m;
	Tentacule t;
	ASSERT_TRUE(t.Monter());
	const uint32 c[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	const NkVec3f decal = V3(0.4f, 0.f, 0.f);
	const NkVec3f pts[5] = {Add(V3(0.f, 1.f, 0.f), decal), Add(V3(0.3f, 1.3f, 0.f), decal), Add(V3(0.f, 1.6f, 0.1f), decal),
							Add(V3(-0.3f, 1.9f, 0.f), decal), Add(V3(0.f, 2.3f, 0.f), decal)};
	NkVec3f surBase[5];
	for (int32 i = 0; i < 5; ++i) {
		surBase[i] = Sub(pts[i], decal);
	}
	NkVector<NkVec3f> temoin;
	CourbeTemoin(surBase, 5u, temoin);
	NkVector<NkMat4f> m = t.monde;
	float32 avant[9], apres[9];
	Longueurs(m, c, 9u, avant);
	NkRtSplineIKSettings s;
	const float32 hors = NkRtSolveSplineIK(t.rig, t.local.Data(), m.Data(), c, 9u, pts, 5u, s);
	Longueurs(m, c, 9u, apres);
	float32 pireCourbe = 0.f, pireLong = 0.f;
	for (uint32 k = 0; k < 9; ++k) {
		pireCourbe = std::fmax(pireCourbe, DistanceALigne(Origin(m[k]), temoin));
	}
	for (uint32 k = 0; k < 8; ++k) {
		pireLong = std::fmax(pireLong, std::fabs(apres[k] - avant[k]));
	}
	const float32 base = Distance(Origin(m[0]), Origin(t.monde[0]));
	std::printf("  [CT8] courbe en S : joints a %.4f mm de la courbe au pire, base a %.2e m, os allonges de %.2e mm, hors courbe %.4f m\n",
				(double)(pireCourbe * 1000.f), (double)base, (double)(pireLong * 1000.f), (double)hors);
	ASSERT_TRUE_MSG(pireCourbe < 5e-4f, "chaque joint est sur la courbe (0,5 mm)");
	ASSERT_TRUE_MSG(base < 1e-6f && pireLong < 1e-5f, "la base ne bouge pas, aucun os ne s'allonge");
	ASSERT_TRUE_MSG(hors == 0.f, "la courbe est assez longue : rien ne depasse");

	// Une courbe trop courte : droit dans la tangente de fin.
	NkVector<NkMat4f> m2 = t.monde;
	const NkVec3f courts[2] = {V3(0.f, 1.f, 0.f), V3(0.f, 1.5f, 0.f)};
	const float32 hors2 = NkRtSolveSplineIK(t.rig, t.local.Data(), m2.Data(), c, 9u, courts, 2u, s);
	float32 pireDroit = 0.f;
	for (uint32 k = 1; k < 9; ++k) {
		const NkVec3f o = Origin(m2[k]);
		pireDroit = std::fmax(pireDroit, std::sqrt(o.x * o.x + o.z * o.z));
	}
	std::printf("  [CT8] courbe de 0,5 m : %.4f m de chaine hors de la courbe (environ 0,75), joints a %.2e m de la droite\n", (double)hors2,
				(double)pireDroit);
	ASSERT_TRUE_MSG(hors2 > 0.6f && hors2 < 0.9f && pireDroit < 1e-5f, "une courbe trop courte : la suite continue droit, et c'est dit");

	// Au poids 0 : la pose donnee au bit pres.
	NkVector<NkMat4f> m3 = t.monde;
	s.weight = 0.f;
	NkRtSolveSplineIK(t.rig, t.local.Data(), m3.Data(), c, 9u, pts, 5u, s);
	ASSERT_TRUE_MSG(std::memcmp(m3.Data(), t.monde.Data(), sizeof(NkMat4f) * t.monde.Size()) == 0, "au poids 0, la pose donnee au bit pres");
}

namespace {
	bool PoseFinie(const NkVector<NkMat4f> &m) {
		for (usize j = 0; j < m.Size(); ++j)
			for (int a = 0; a < 4; ++a)
				for (int b = 0; b < 4; ++b)
					if (!std::isfinite(m[j][a][b]))
						return false;
		return true;
	}
} // namespace

// -----------------------------------------------------------------------------
// CT9 : LES CAS DEGENERES NE CASSENT RIEN (un jeu les rencontre : une cible posee sur
// l'epaule, un regard sur son propre oeil). Une chaine FAUSSE (deux freres, un
// joint repete, 33 joints) est REFUSEE et la pose rendue au bit pres. Cible sur la
// base, pole sur la droite base-cible, visee de son propre centre, cone nul,
// limites nulles, courbe de longueur nulle : aucune valeur non finie, aucun os
// allonge, un cone nul ne tourne pas l'axe.                  [mutation : chain_no_check]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT9_Les_cas_degeneres_ne_cassent_rien) {
	Mutations _m;
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	// Les chaines fausses.
	const uint32 freres[3] = {(uint32)hu.h.upperArmL, (uint32)hu.h.upperArmR, (uint32)hu.h.foreArmR};
	const uint32 repete[3] = {(uint32)hu.h.upperArmL, (uint32)hu.h.upperArmL, (uint32)hu.h.foreArmL};
	uint32 longue[33];
	for (uint32 k = 0; k < 33; ++k) {
		longue[k] = k % 5u;
	}
	uint32 changees = 0;
	{
		NkVector<NkMat4f> m = hu.monde;
		NkRtSolveChainIK(hu.rig, hu.local.Data(), m.Data(), freres, 3u, V3(0.5f, 1.f, 0.5f), NkRtChainIKSettings());
		NkRtSolveChainIK(hu.rig, hu.local.Data(), m.Data(), repete, 3u, V3(0.5f, 1.f, 0.5f), NkRtChainIKSettings());
		NkRtSolveChainIK(hu.rig, hu.local.Data(), m.Data(), longue, 33u, V3(0.5f, 1.f, 0.5f), NkRtChainIKSettings());
		const NkVec3f pts[2] = {V3(0.f, 0.f, 0.f), V3(1.f, 0.f, 0.f)};
		NkRtSolveSplineIK(hu.rig, hu.local.Data(), m.Data(), freres, 3u, pts, 2u, NkRtSplineIKSettings());
		changees = std::memcmp(m.Data(), hu.monde.Data(), sizeof(NkMat4f) * m.Size()) != 0 ? 1u : 0u;
	}
	// Les cas limites, sur un tentacule.
	Tentacule t;
	ASSERT_TRUE(t.Monter());
	const uint32 c[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	bool fini = true;
	float32 pireLong = 0.f;
	float32 avant[9], apres[9];
	Longueurs(t.monde, c, 9u, avant);
	auto Verifier = [&](const NkVector<NkMat4f> &m) {
		fini = fini && PoseFinie(m);
		Longueurs(m, c, 9u, apres);
		for (uint32 k = 0; k < 8; ++k) {
			pireLong = std::fmax(pireLong, std::fabs(apres[k] - avant[k]));
		}
	};
	{
		NkVector<NkMat4f> m = t.monde;
		NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), c, 9u, Origin(m[0]), NkRtChainIKSettings()); // la cible sur la base
		Verifier(m);
		m = t.monde;
		NkRtChainIKSettings s;
		s.usePole = true;
		s.pole = Lerp(Origin(m[0]), Origin(m[8]), 0.3f); // le pole sur la droite base-bout
		NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), c, 9u, Lerp(Origin(m[0]), Origin(m[8]), 0.8f), s);
		Verifier(m);
		m = t.monde;
		const NkVec3f memes[3] = {V3(1.f, 1.f, 1.f), V3(1.f, 1.f, 1.f), V3(1.f, 1.f, 1.f)}; // une courbe de longueur nulle
		NkRtSolveSplineIK(t.rig, t.local.Data(), m.Data(), c, 9u, memes, 3u, NkRtSplineIKSettings());
		Verifier(m);
	}
	// La visee de son propre centre, et un cone nul.
	float32 coneNul = 0.f;
	{
		NkVector<NkMat4f> m = hu.monde;
		const uint32 tete = (uint32)hu.h.head;
		NkRtAimSettings s;
		NkRtAimBone(hu.rig, hu.local.Data(), m.Data(), tete, Origin(m[tete]), s);
		fini = fini && PoseFinie(m);
		m = hu.monde;
		s.maxAngle = 0.f;
		NkRtAimBone(hu.rig, hu.local.Data(), m.Data(), tete, Add(Origin(m[tete]), V3(1.f, 0.f, 0.f)), s);
		fini = fini && PoseFinie(m);
		coneNul = AngleDeg(TransformDir(m[tete], s.aimAxis), TransformDir(hu.monde[tete], s.aimAxis));
	}
	// Des limites nulles : tout ecart est retire.
	float32 resteLimite = 0.f;
	{
		NkRtSwingTwistLimit nul;
		nul.swing1 = nul.swing2 = 0.f;
		nul.twistMin = nul.twistMax = 0.f;
		const NkRtQuat ref = QFromAxisAngle(V3(0.f, 1.f, 0.f), 0.3f);
		NkRtQuat q = QMul(ref, QFromRotVec(V3(0.4f, -0.2f, 0.9f)));
		NkRtLimitSwingTwist(q, ref, nul);
		resteLimite = Length(QToRotVec(QMul(QConj(ref), q))) / kDeg;
		fini = fini && std::isfinite(q.x) && std::isfinite(q.w);
	}
	std::printf("  [CT9] chaines fausses : pose changee = %u ; cas limites : tout fini = %d, os allonges de %.2e mm ; cone nul : axe tourne de %.4f deg ; "
				"limites nulles : il reste %.4f deg\n",
				changees, fini ? 1 : 0, (double)(pireLong * 1000.f), (double)coneNul, (double)resteLimite);
	ASSERT_TRUE_MSG(changees == 0u, "une chaine fausse est refusee, la pose rendue au bit pres");
	ASSERT_TRUE_MSG(fini, "aucune valeur non finie");
	ASSERT_TRUE_MSG(pireLong < 1e-5f, "aucun os ne s'allonge, meme dans les cas limites");
	ASSERT_TRUE_MSG(coneNul < 0.01f, "un cone nul ne tourne pas l'axe");
	ASSERT_TRUE_MSG(resteLimite < 0.01f, "des limites nulles retirent tout l'ecart");
}

// -----------------------------------------------------------------------------
// CT10 : L'ALLONGE DOUCE : LE GENOU NE CLAQUE PAS. La jambe gauche de l'humanoide
// (IK a charniere, allonge 1) suit une cible qui s'eloigne de la hanche de 0,85 L a
// 1,10 L par pas de 1 mm. Sans allonge douce, le dernier millimetre avant la jambe
// tendue deplie le genou de plusieurs degres (le temoin le voit : > 3 degres) ; avec
// une douceur de 5 cm, jamais plus de 1 degre par millimetre. La distance douce est
// continue et de pente 1 a L - s, et ne depasse jamais L.   [mutation : soft_reach_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT10_L_allonge_douce_le_genou_ne_claque_pas) {
	Mutations _m;
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	const uint32 a = (uint32)hu.h.thighL, b = (uint32)hu.h.shinL, c = (uint32)hu.h.footL;
	const NkVec3f hanche = Origin(hu.monde[a]);
	const float32 L = Distance(hanche, Origin(hu.monde[b])) + Distance(Origin(hu.monde[b]), Origin(hu.monde[c]));
	const NkVec3f dir = Normalize(V3(0.f, -1.f, 0.15f), V3(0.f, -1.f, 0.f));
	const float32 douceur = 0.05f;
	float32 pire[2] = {0.f, 0.f}; // 0 : sans allonge douce, 1 : avec
	for (int32 mode = 0; mode < 2; ++mode) {
		float32 avant = -1.f;
		for (int32 i = 0; i <= 250; ++i) {
			const float32 d = 0.85f * L + 0.001f * (float32)i;
			NkVector<NkMat4f> m = hu.monde;
			const NkVec3f cible = NkRtSoftReachTarget(hanche, Madd(hanche, dir, d), L, mode == 0 ? 0.f : douceur);
			NkRtSolveTwoBoneHinge(hu.rig, hu.local.Data(), m.Data(), a, b, c, cible, V3(0.f, 0.f, 1.f), 1.f);
			const float32 pli = AngleDeg(Sub(Origin(m[b]), Origin(m[a])), Sub(Origin(m[c]), Origin(m[b])));
			if (avant >= 0.f) {
				pire[mode] = std::fmax(pire[mode], std::fabs(pli - avant));
			}
			avant = pli;
		}
	}
	// La fonction elle-meme : continue, de pente 1 a L - s des deux cotes (derivees a
	// droite et a gauche ; une difference centree a cheval sur le raccord lirait la
	// moyenne des deux pentes, pas la continuite), sous L.
	const float32 ds = L - douceur, e = 1e-4f;
	const float32 f0 = NkRtSoftReachDistance(ds, L, douceur);
	const float32 penteD = (NkRtSoftReachDistance(ds + e, L, douceur) - f0) / e;
	const float32 penteG = (f0 - NkRtSoftReachDistance(ds - e, L, douceur)) / e;
	const float32 pente = std::fabs(penteD - 1.f) > std::fabs(penteG - 1.f) ? penteD : penteG;
	const float32 loin = NkRtSoftReachDistance(2.f * L, L, douceur);
	std::printf("  [CT10] jambe de %.3f m, cible de 0,85 L a 1,10 L par mm : le genou bouge au pire de %.3f deg/mm sans allonge douce, de %.3f "
				"deg/mm avec (douceur 5 cm) ; pente a L - s : %.4f ; a 2 L la distance douce vaut %.5f m (< L)\n",
				(double)L, (double)pire[0], (double)pire[1], (double)pente, (double)loin);
	ASSERT_TRUE_MSG(pire[0] > 3.f, "sans allonge douce, le temoin voit le genou claquer");
	ASSERT_TRUE_MSG(pire[1] < 1.f, "avec l'allonge douce, jamais plus d'un degre par millimetre");
	// « Sous L » est vrai en reel ; en float, exp(-17) se perd et une cible a 2 L donne
	// L exactement (mesure) : le critere est « jamais au-dela ».
	ASSERT_TRUE_MSG(std::fabs(pente - 1.f) < 0.01f && loin <= L, "la distance douce est continue, de pente 1 a L - s, et ne depasse jamais L");
}

// -----------------------------------------------------------------------------
// CT11 : LE PASSAGE FK -> IK SANS SAUT. Le bras gauche est pose en FK (bras leve,
// coude plie de 70 degres, main tournee). On bascule en IK : cible = la main, son
// orientation = celle de la main, pole = NkRtPoleFromPose. (a) Depuis la pose FK
// elle-meme (l'image du passage) : l'IK rend la pose, os par os. (b) Depuis le repos
// (bras presque tendu) : le coude et la main reviennent a leur place FK, la main a
// son orientation ; le roulis du bras, lui, vient du depart de l'IK : il est imprime,
// pas juge (c'est la limite dite).                             [mutation : pole_flat]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT11_Le_passage_FK_IK_sans_saut) {
	Mutations _m;
	Humain hu;
	ASSERT_TRUE(hu.Monter());
	const uint32 a = (uint32)hu.h.upperArmL, b = (uint32)hu.h.foreArmL, c = (uint32)hu.h.handL;
	// La pose FK.
	NkVector<NkMat4f> lfk = hu.local, fk;
	fk.Resize(hu.rig.Count());
	const NkVec3f axeAvant = Normalize(Origin(hu.local[c]), V3(0.f, 1.f, 0.f));
	lfk[a] = MatFrom(QMul(QFromMat(hu.local[a]), QFromAxisAngle(Normalize(V3(0.3f, 0.2f, 0.9f), V3(1.f, 0.f, 0.f)), 1.1f)), V3(1.f, 1.f, 1.f),
					 Origin(hu.local[a]));
	lfk[b] = MatFrom(QMul(QFromMat(hu.local[b]), QFromAxisAngle(AnyPerpendicular(axeAvant), 70.f * kDeg)), V3(1.f, 1.f, 1.f), Origin(hu.local[b]));
	lfk[c] = MatFrom(QMul(QFromMat(hu.local[c]), QFromAxisAngle(V3(0.f, 1.f, 0.f), 0.6f)), V3(1.f, 1.f, 1.f), Origin(hu.local[c]));
	hu.rig.LocalToWorld(lfk.Data(), fk.Data());
	// Le passage.
	const uint32 chaine[3] = {a, b, c};
	NkRtChainIKSettings s;
	s.usePole = true;
	s.pole = NkRtPoleFromPose(fk.Data(), a, b, c, 0.3f, V3(0.f, 0.f, -1.f));
	s.useEndRotation = true;
	s.endRotation = QFromMat(fk[c]);
	const NkVec3f cible = Origin(fk[c]);
	// (a) depuis la pose FK.
	NkVector<NkMat4f> m = fk;
	NkRtSolveChainIK(hu.rig, lfk.Data(), m.Data(), chaine, 3u, cible, s);
	float32 depuisFk = 0.f;
	for (uint32 k = 0; k < 3; ++k) {
		depuisFk = std::fmax(depuisFk, EcartMat(m[chaine[k]], fk[chaine[k]]));
	}
	// (b) depuis le repos.
	NkVector<NkMat4f> r = hu.monde;
	NkRtSolveChainIK(hu.rig, hu.local.Data(), r.Data(), chaine, 3u, cible, s);
	const float32 coude = Distance(Origin(r[b]), Origin(fk[b])), main = Distance(Origin(r[c]), Origin(fk[c]));
	const float32 orientMain = AngleDeg(TransformDir(r[c], V3(1.f, 0.f, 0.f)), TransformDir(fk[c], V3(1.f, 0.f, 0.f))) +
							   AngleDeg(TransformDir(r[c], V3(0.f, 1.f, 0.f)), TransformDir(fk[c], V3(0.f, 1.f, 0.f)));
	const float32 roulis = Length(QToRotVec(QMul(QConj(QFromMat(fk[a])), QFromMat(r[a])))) / kDeg;
	std::printf("  [CT11] (a) depuis la pose FK : os a %.2e de la FK ; (b) depuis le repos : coude a %.4f mm, main a %.4f mm, orientation de la main "
				"a %.4f deg ; roulis du bras %.2f deg (non juge : il vient du depart)\n",
				(double)depuisFk, (double)(coude * 1000.f), (double)(main * 1000.f), (double)orientMain, (double)roulis);
	ASSERT_TRUE_MSG(depuisFk < 1e-4f, "a l'image du passage, l'IK rend la pose FK");
	ASSERT_TRUE_MSG(coude < 1e-4f && main < 1e-4f, "depuis le repos, le coude et la main reviennent a leur place FK");
	ASSERT_TRUE_MSG(orientMain < 0.02f, "et la main a son orientation FK");
}

namespace {
	/// Le plus grand pli (degres) aux joints interieurs d'une chaine.
	float32 PliMax(const NkVector<NkMat4f> &m, const uint32 *c, uint32 n) {
		float32 pire = 0.f;
		for (uint32 k = 1; k + 1 < n; ++k) {
			pire = std::fmax(pire, AngleDeg(Sub(Origin(m[c[k]]), Origin(m[c[k - 1]])), Sub(Origin(m[c[k + 1]]), Origin(m[c[k]]))));
		}
		return pire;
	}
} // namespace

// -----------------------------------------------------------------------------
// CT12 : LE PLI BORNE PENDANT LA RESOLUTION. Le tentacule (ses joints plient de 14 a
// 20 degres au repos), pli maximal 15 degres. (a) Une cible a 0,35 de sa longueur :
// il faudrait s'enrouler ; la chaine ne plie nulle part au-dela de 15 degres et dit
// qu'elle n'atteint pas. (b) Une cible a 0,9 de sa longueur, a 20 degres de sa
// direction : atteinte (0,1 mm), sans plier au-dela de 15.        [mutation : bend_free]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, CT12_Le_pli_borne_pendant_la_resolution) {
	Mutations _m;
	Tentacule t;
	ASSERT_TRUE(t.Monter());
	const uint32 c[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	const float32 pliRepos = PliMax(t.monde, c, 9u);
	NkRtChainIKSettings s;
	s.maxBend = 15.f * kDeg;
	s.iterations = 64; // une chaine bornee converge plus lentement (mesure : 0,15 mm apres 24 passes)
	const NkVec3f base = Origin(t.monde[0]);
	const NkVec3f corde = Normalize(Sub(Origin(t.monde[8]), base), V3(0.f, 1.f, 0.f));
	// (a) trop pres : il faudrait s'enrouler.
	NkVector<NkMat4f> m = t.monde;
	const NkRtChainIKResult ra = NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), c, 9u, Madd(base, corde, 0.35f * 1.2f), s);
	const float32 pliA = PliMax(m, c, 9u);
	// (b) loin, a 20 degres de la corde.
	m = t.monde;
	const NkVec3f dirB = QRotate(QFromAxisAngle(AnyPerpendicular(corde), 20.f * kDeg), corde);
	const NkRtChainIKResult rb = NkRtSolveChainIK(t.rig, t.local.Data(), m.Data(), c, 9u, Madd(base, dirB, 0.9f * 1.2f), s);
	const float32 pliB = PliMax(m, c, 9u);
	std::printf("  [CT12] pli au repos %.2f deg ; (a) cible a 0,35 L : pli max %.4f deg, il manque %.4f m (atteint = %d) ; (b) cible a 0,9 L : "
				"pli max %.4f deg, bout a %.4f mm en %u passes\n",
				(double)pliRepos, (double)pliA, (double)ra.distance, ra.reached ? 1 : 0, (double)pliB, (double)(rb.distance * 1000.f), rb.iterations);
	ASSERT_TRUE_MSG(pliRepos > 15.f, "le cas mesure part au-dela de la borne");
	ASSERT_TRUE_MSG(pliA < 15.01f && !ra.reached && ra.distance > 0.05f, "trop pres : aucun joint ne plie au-dela, et la chaine dit qu'elle n'atteint pas");
	ASSERT_TRUE_MSG(pliB < 15.01f && rb.distance < 1e-4f, "une cible atteignable est atteinte sans plier au-dela");
}
