// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_tourner_chaine.cpp — LE BANC DE LA ROTATION REPARTIE SUR UNE
// CHAINE D'OS (Realtime/NkRtChainTurn.h, 13/10/2026).
//
// D'ou il vient : le patient virtuel de PV3DE tourne la tete en repartissant la
// rotation sur le haut du thorax, le cou et la tete. La regle, ecrite a la main
// dans PV3DE, est passee dans NKAnima.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   chaque os de la chaine prend sa part               turn_no_spread   (TC1, TC2)
//
//   NK_RT_MUTATION=turn_no_spread ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
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
using math::NkVec3f;

namespace {

	constexpr float32 kRad = 3.14159265358979f / 180.f;

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "turn_no_spread") != nullptr) ? (uint32)NK_RT_MUT_TURN_NO_SPREAD : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	/// L'angle (degres) entre deux rotations : celui de a^-1 * b.
	float32 EcartDeg(const NkRtQuat &a, const NkRtQuat &b) {
		return Length(QToRotVec(QMul(QConj(a), b))) / kRad;
	}

	/// Une chaine de trois os (le haut du thorax, le cou, la tete) sous une racine,
	/// aux reperes QUELCONQUES : les rotations locales de reference, l'orientation du
	/// parent de chacun, et l'orientation de chacun dans le monde.
	struct Chaine {
			NkRtQuat racine, ref[3], parent[3], monde[3];
	};
	Chaine Faire() {
		Chaine c;
		c.racine = QFromAxisAngle(Normalize(V3(0.3f, 1.f, -0.2f), V3(0.f, 1.f, 0.f)), 0.8f);
		c.ref[0] = QFromAxisAngle(Normalize(V3(1.f, 0.2f, 0.4f), V3(1.f, 0.f, 0.f)), -0.5f);
		c.ref[1] = QFromAxisAngle(Normalize(V3(-0.3f, 0.1f, 1.f), V3(0.f, 0.f, 1.f)), 1.1f);
		c.ref[2] = QFromAxisAngle(Normalize(V3(0.5f, -1.f, 0.3f), V3(0.f, 1.f, 0.f)), 0.4f);
		NkRtQuat p = c.racine;
		for (int32 k = 0; k < 3; ++k) {
			c.parent[k] = p;
			c.monde[k] = QMul(p, c.ref[k]);
			p = c.monde[k];
		}
		return c;
	}
	/// L'orientation de chaque os dans le monde, les rotations locales etant `local`.
	void Monde(const Chaine &c, const NkRtQuat *local, NkRtQuat *monde) {
		NkRtQuat p = c.racine;
		for (int32 k = 0; k < 3; ++k) {
			monde[k] = QMul(p, local[k]);
			p = monde[k];
		}
	}

} // namespace

// -----------------------------------------------------------------------------
// TC1 — AUTOUR D'UN SEUL AXE, LES PARTS S'AJOUTENT. Un lacet de 60 degres reparti
// en 20 %, 40 %, 40 % sur une chaine aux reperes quelconques : dans le monde, le
// premier os a tourne de 12 degres, le deuxieme de 36, le dernier de 60, autour de
// l'axe demande.                                       [mutation : turn_no_spread]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TC1_Autour_d_un_axe_les_parts_s_ajoutent) {
	Mutations _m;
	const Chaine c = Faire();
	NkRtTurn tour;
	tour.axes[0] = Normalize(V3(0.1f, 1.f, 0.05f), V3(0.f, 1.f, 0.f));
	tour.angles[0] = 60.f * kRad;
	const float32 parts[3] = {0.2f, 0.4f, 0.4f};
	NkRtQuat local[3], monde[3];
	NkRtDistributeTurn(tour, parts, c.parent, c.ref, 3u, local);
	Monde(c, local, monde);
	const float32 attendus[3] = {12.f, 36.f, 60.f};
	float32 pire = 0.f;
	for (int32 k = 0; k < 3; ++k) {
		// monde[k] doit etre R(axe, attendu) * monde de reference.
		const NkRtQuat voulu = QMul(QFromAxisAngle(tour.axes[0], attendus[k] * kRad), c.monde[k]);
		pire = std::fmax(pire, EcartDeg(voulu, monde[k]));
	}
	std::printf("  [TC1] lacet de 60 deg en 20 / 40 / 40 %% : les trois os a %.4f deg de 12, 36 et 60 deg autour de l'axe\n", (double)pire);
	ASSERT_TRUE_MSG(pire < 0.01f, "autour d'un seul axe, chaque os a tourne de la somme des parts jusqu'a lui");
}

// -----------------------------------------------------------------------------
// TC2 — TROIS ANGLES. Toute la part au dernier os : il prend la rotation ENTIERE
// (R0 * R1 * R2) et les autres gardent leur reference, au bit pres. En 20 / 40 /
// 40 % : chaque os prend la rotation faite de ses fractions d'angles (verifie os
// par os), et le dernier arrive a quelques degres de la rotation entiere -- des
// fractions d'angles ne se composent pas exactement (la valeur est imprimee).
//                                                      [mutation : turn_no_spread]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, TC2_Trois_angles_repartis_sur_la_chaine) {
	Mutations _m;
	const Chaine c = Faire();
	NkRtTurn tour;
	tour.axes[0] = V3(0.f, 1.f, 0.f);
	tour.axes[1] = V3(1.f, 0.f, 0.f);
	tour.axes[2] = V3(0.f, 0.f, 1.f);
	tour.angles[0] = 40.f * kRad;
	tour.angles[1] = -20.f * kRad;
	tour.angles[2] = 10.f * kRad;
	const NkRtQuat entiere = QMul(QMul(QFromAxisAngle(tour.axes[0], tour.angles[0]), QFromAxisAngle(tour.axes[1], tour.angles[1])),
								  QFromAxisAngle(tour.axes[2], tour.angles[2]));
	ASSERT_TRUE_MSG(EcartDeg(entiere, NkRtTurnRotation(tour, 1.f)) < 1e-3f, "la rotation d'une part entiere est R0 * R1 * R2");
	NkRtQuat local[3], monde[3];
	{
		const float32 bout[3] = {0.f, 0.f, 1.f};
		NkRtDistributeTurn(tour, bout, c.parent, c.ref, 3u, local);
		Monde(c, local, monde);
		const float32 e = EcartDeg(QMul(entiere, c.monde[2]), monde[2]);
		std::printf("  [TC2] toute la part au dernier os : il est a %.4f deg de la rotation entiere\n", (double)e);
		ASSERT_TRUE_MSG(e < 0.01f, "le dernier os seul prend la rotation entiere");
		ASSERT_TRUE_MSG(std::memcmp(&local[0], &c.ref[0], sizeof(NkRtQuat)) == 0 && std::memcmp(&local[1], &c.ref[1], sizeof(NkRtQuat)) == 0,
						"une part nulle : la reference, au bit pres");
	}
	{
		const float32 parts[3] = {0.2f, 0.4f, 0.4f};
		NkRtDistributeTurn(tour, parts, c.parent, c.ref, 3u, local);
		float32 pire = 0.f;
		for (int32 k = 0; k < 3; ++k) {
			// L'os k, SEUL (son parent dans la pose de reference), a pris la rotation de sa part.
			const NkRtQuat seul = QMul(c.parent[k], local[k]);
			pire = std::fmax(pire, EcartDeg(QMul(NkRtTurnRotation(tour, parts[k]), c.monde[k]), seul));
		}
		Monde(c, local, monde);
		const float32 bout = EcartDeg(QMul(entiere, c.monde[2]), monde[2]);
		std::printf("  [TC2] en 20 / 40 / 40 %% : chaque os a %.4f deg de la rotation de sa part ; le dernier arrive a %.2f deg de la rotation entiere "
					"(lacet 40, tangage -20, roulis 10)\n",
					(double)pire, (double)bout);
		ASSERT_TRUE_MSG(pire < 0.01f, "chaque os prend la rotation faite de ses fractions d'angles");
		ASSERT_TRUE_MSG(bout < 8.f, "le dernier os arrive a quelques degres de la rotation entiere");
	}
	{
		// Sans angle : la reference, au bit pres.
		NkRtTurn rien;
		const float32 parts[3] = {0.2f, 0.4f, 0.4f};
		NkRtDistributeTurn(rien, parts, c.parent, c.ref, 3u, local);
		ASSERT_TRUE_MSG(std::memcmp(local, c.ref, sizeof(c.ref)) == 0, "sans angle, la chaine garde sa reference, au bit pres");
	}
}
