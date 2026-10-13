// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_ik_charniere.cpp — LE BANC DE L'IK A DEUX OS A CHARNIERE
// (Realtime/NkRtHingeIK.h, 13/10/2026).
//
// D'ou il vient : le corps du patient virtuel de PV3DE passe sur NKAnima. Son IK des
// bras gardait le coude en charniere ; celle de NKAnima (NkRtSolveTwoBone, ecrite
// pour les jambes) tourne l'os du bas par le plus court chemin. La regle de PV3DE
// vient ici, eprouvee ici.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   l'os du haut roule, le milieu ne fait que plier      twobone_no_roll   (HK1)
//
//   NK_RT_MUTATION=twobone_no_roll ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
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

	float32 Rad(float32 deg) {
		return deg * 3.14159265358979f / 180.f;
	}

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "twobone_no_roll") != nullptr) ? (uint32)NK_RT_MUT_TWOBONE_NO_ROLL : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	/// Le bras droit de l'humanoide d'essai, le coude DEJA plie de 30 degres. SA
	/// charniere est alors la normale du plan (bras, avant-bras), dans le repere de
	/// l'avant-bras : c'est autour d'elle, et d'elle seule, que l'IK doit le tourner.
	struct Bras {
			NkRtTestHumanoid h;
			NkRtRig rig;
			NkVector<NkMat4f> local, monde;
			NkRtQuat avantRef, avantPlie;
			NkVec3f charniere;
			float32 l1 = 0.f, l2 = 0.f;
			bool Monter(bool tordu) {
				NkRtMakeTestHumanoid(h, tordu);
				if (!rig.Build(h.parents.Data(), (uint32)h.parents.Size(), h.names.Data())) {
					return false;
				}
				local.Resize(rig.Count());
				monde.Resize(rig.Count());
				for (uint32 j = 0; j < rig.Count(); ++j) {
					local[j] = h.bindLocal[j];
				}
				avantRef = QFromMat(h.bindLocal[(usize)h.foreArmR]);
				const NkVec3f axeAvant = Normalize(Origin(h.bindLocal[(usize)h.handR]), V3(0.f, 1.f, 0.f));
				avantPlie = QMul(avantRef, QFromAxisAngle(AnyPerpendicular(axeAvant), Rad(30.f)));
				// L'axe du bras vu du repere de l'avant-bras plie, et la normale de leur plan.
				const NkVec3f axeBras = QRotate(QConj(avantPlie), Normalize(Origin(h.bindLocal[(usize)h.foreArmR]), V3(0.f, 1.f, 0.f)));
				charniere = Normalize(Cross(axeAvant, axeBras), AnyPerpendicular(axeAvant));
				local[(usize)h.foreArmR] = MatFrom(avantPlie, V3(1.f, 1.f, 1.f), Origin(h.bindLocal[(usize)h.foreArmR]));
				rig.LocalToWorld(local.Data(), monde.Data());
				l1 = Distance(Origin(monde[(usize)h.upperArmR]), Origin(monde[(usize)h.foreArmR]));
				l2 = Distance(Origin(monde[(usize)h.foreArmR]), Origin(monde[(usize)h.handR]));
				return true;
			}
			/// Ce que l'IK a fait tourner l'avant-bras (depuis le coude plie), en vecteur rotation, dans son repere.
			NkVec3f FlexionActuelle() {
				NkVector<NkMat4f> l;
				l.Resize(rig.Count());
				rig.WorldToLocal(monde.Data(), l.Data());
				return QToRotVec(QMul(QConj(avantPlie), QFromMat(l[(usize)h.foreArmR])));
			}
	};

} // namespace

// -----------------------------------------------------------------------------
// HK1 — LE COUDE RESTE UNE CHARNIERE. Douze cibles autour de l'epaule, sur des
// reperes droits puis quelconques : la main arrive (a 0,1 mm), les os gardent leur
// longueur, et l'avant-bras n'a tourne qu'autour de SA charniere (la part de sa
// rotation hors de l'axe est nulle).          [mutation : twobone_no_roll]
// Pour memoire, la meme cible par NkRtSolveTwoBone (l'IK des jambes) : la part hors
// de l'axe y est imprimee, elle n'est pas jugee.
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, HK1_IK_a_deux_os_le_coude_reste_une_charniere) {
	Mutations _m;
	for (int32 tordu = 0; tordu < 2; ++tordu) {
		float32 pireReste = 0.f, pireLongueur = 0.f, pireHorsAxe = 0.f, pireJambes = 0.f, plusGrandPli = 0.f;
		for (int32 k = 0; k < 12; ++k) {
			Bras b;
			ASSERT_TRUE(b.Monter(tordu != 0));
			const NkVec3f S = Origin(b.monde[(usize)b.h.upperArmR]);
			// Une cible a 55 a 90 % de l'allonge, tout autour de l'epaule (devant, dessous,
			// en travers du corps).
			const float32 az = Rad(30.f * (float32)k), el = Rad(-50.f + 12.f * (float32)(k % 5));
			const float32 portee = (b.l1 + b.l2) * (0.55f + 0.035f * (float32)(k % 11));
			const NkVec3f cible = Add(S, Scale(V3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az)), portee));
			const NkVec3f pole = V3(0.3f, -1.f, -0.4f);
			// L'IK des jambes, sur une copie : pour memoire.
			{
				Bras j;
				ASSERT_TRUE(j.Monter(tordu != 0));
				(void)NkRtSolveTwoBone(j.rig, j.local.Data(), j.monde.Data(), (uint32)j.h.upperArmR, (uint32)j.h.foreArmR, (uint32)j.h.handR,
									   cible, pole);
				const NkVec3f r = j.FlexionActuelle();
				pireJambes = std::fmax(pireJambes, Length(Sub(r, Scale(j.charniere, Dot(r, j.charniere)))));
			}
			const float32 reste = NkRtSolveTwoBoneHinge(b.rig, b.local.Data(), b.monde.Data(), (uint32)b.h.upperArmR, (uint32)b.h.foreArmR,
														(uint32)b.h.handR, cible, pole);
			const NkVec3f E = Origin(b.monde[(usize)b.h.foreArmR]), W = Origin(b.monde[(usize)b.h.handR]);
			pireReste = std::fmax(pireReste, reste);
			pireLongueur = std::fmax(pireLongueur, std::fmax(std::fabs(Distance(S, E) - b.l1), std::fabs(Distance(E, W) - b.l2)));
			const NkVec3f r = b.FlexionActuelle();
			pireHorsAxe = std::fmax(pireHorsAxe, Length(Sub(r, Scale(b.charniere, Dot(r, b.charniere)))));
			plusGrandPli = std::fmax(plusGrandPli, std::fabs(Dot(r, b.charniere)));
		}
		std::printf("  [HK1] reperes %s, 12 cibles : la main a %.2e m de la cible, les os a %.2e m de leur longueur, l'avant-bras hors de sa "
					"charniere de %.2e rad (il a plie jusqu'a %.2f rad de plus) ; pour memoire, l'IK des jambes : %.3f rad hors de l'axe\n",
					tordu ? "quelconques" : "droits", (double)pireReste, (double)pireLongueur, (double)pireHorsAxe, (double)plusGrandPli,
					(double)pireJambes);
		ASSERT_TRUE_MSG(pireReste < 1e-4f, "la main arrive a la cible (a 0,1 mm)");
		ASSERT_TRUE_MSG(pireLongueur < 1e-4f, "les deux os gardent leur longueur");
		ASSERT_TRUE_MSG(pireHorsAxe < 2e-3f, "l'avant-bras ne tourne qu'autour de sa charniere (< 2e-3 rad hors de l'axe)");
		ASSERT_TRUE_MSG(plusGrandPli > 0.3f, "le coude plie vraiment (le critere n'est pas vide)");
	}
}

// -----------------------------------------------------------------------------
// HK2 — HORS DE PORTEE : la cible est a deux longueurs de bras. Le bras se tend vers
// elle, jusqu'a 99,9 % de son allonge, et la distance restante est rendue ; le milieu
// se place du cote du pole.
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, HK2_IK_a_charniere_hors_de_portee_et_pole) {
	Mutations _m;
	Bras b;
	ASSERT_TRUE(b.Monter(true));
	const NkVec3f S = Origin(b.monde[(usize)b.h.upperArmR]);
	const NkVec3f versLoin = Normalize(V3(-0.4f, 0.2f, 1.f), V3(0.f, 0.f, 1.f));
	const NkVec3f loin = Add(S, Scale(versLoin, 2.f * (b.l1 + b.l2)));
	const float32 reste = NkRtSolveTwoBoneHinge(b.rig, b.local.Data(), b.monde.Data(), (uint32)b.h.upperArmR, (uint32)b.h.foreArmR,
												(uint32)b.h.handR, loin, V3(0.f, -1.f, 0.f));
	const NkVec3f W = Origin(b.monde[(usize)b.h.handR]);
	const float32 allonge = Distance(S, W) / (b.l1 + b.l2);
	const float32 visee = Dot(Normalize(Sub(W, S), V3(0.f, 1.f, 0.f)), versLoin);
	std::printf("  [HK2] hors de portee : le bras tendu a %.4f de son allonge, vers la cible a %.6f pres (cosinus), reste %.3f m\n",
				(double)allonge, (double)visee, (double)reste);
	ASSERT_TRUE_MSG(std::fabs(allonge - kNkRtHingeReach) < 1e-3f, "le bras se tend a 99,9 % de son allonge, pas au-dela");
	ASSERT_TRUE_MSG(visee > 0.99999f, "le bras tendu vise la cible");
	ASSERT_TRUE_MSG(std::fabs(reste - (2.f - kNkRtHingeReach) * (b.l1 + b.l2)) < 1e-3f, "la distance restante est rendue");
	// Le pole : la meme cible proche, deux poles opposes -- le coude change de cote.
	float32 cote[2] = {0.f, 0.f};
	for (int32 p = 0; p < 2; ++p) {
		Bras c;
		ASSERT_TRUE(c.Monter(true));
		const NkVec3f s = Origin(c.monde[(usize)c.h.upperArmR]);
		const NkVec3f cible = Add(s, Scale(V3(0.f, -0.3f, 1.f), 0.6f * (c.l1 + c.l2)));
		const NkVec3f pole = V3(p == 0 ? 1.f : -1.f, 0.f, 0.f);
		(void)NkRtSolveTwoBoneHinge(c.rig, c.local.Data(), c.monde.Data(), (uint32)c.h.upperArmR, (uint32)c.h.foreArmR, (uint32)c.h.handR, cible,
									pole);
		cote[p] = Dot(Sub(Origin(c.monde[(usize)c.h.foreArmR]), s), pole);
	}
	std::printf("  [HK2] le coude du cote du pole : %+.3f m (pole a droite), %+.3f m (pole a gauche)\n", (double)cote[0], (double)cote[1]);
	ASSERT_TRUE_MSG(cote[0] > 0.01f && cote[1] > 0.01f, "le coude se place du cote du pole");
}
