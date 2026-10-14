// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_cycle_de_marche.cpp — LE BANC DU CYCLE DE MARCHE ANALYSE
// (Motion/NkGaitCycle.h, 13/10/2026).
//
// D'ou il vient : le patient virtuel de PV3DE marchait par un cycle fabrique a la main.
// Pour jouer la marche capturee par l'artiste sur un personnage dont le trajet est
// decide ailleurs, il faut separer ce que la racine fait a vitesse constante (la
// foulee) de ce qu'elle fait en plus (le reste), et donner les pieds dans le repere
// qui avance : un pied plante y recule a la vitesse de la marche.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   la foulee est retiree de la racine et des pieds      gait_no_stride   (GC1, GC2, GC3)
//
//   NK_RT_MUTATION=gait_no_stride ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
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
using nkentseu::math::NkVec3f;

namespace {

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "gait_no_stride") != nullptr) ? (uint32)NK_RT_MUT_GAIT_NO_STRIDE : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

	constexpr float32 kPi = 3.14159265358979f;
	constexpr float32 kFoulee = 1.2f;  ///< metres par cycle
	constexpr float32 kDuree = 1.4f;   ///< secondes par cycle
	constexpr float32 kAppui = 0.6f;   ///< part du cycle ou un pied est au sol
	constexpr float32 kCheville = 0.1f; ///< hauteur de la cheville, pied a plat
	constexpr usize kImages = 57;

	float32 Lisse(float32 u) {
		return u * u * (3.f - 2.f * u);
	}

	/// UNE MARCHE DE SYNTHESE, dont tout est connu : la racine avance d'une foulee par
	/// cycle, rebondit deux fois (2 cm) et prend de l'elan a chaque pas (3 cm) ; chaque
	/// pied est plante 60 % du cycle, puis vole d'une foulee. `echelle` : la taille du
	/// personnage.
	void Marche(float32 echelle, NkVector<NkVec3f> &racine, NkVector<NkVector<NkVec3f>> &pieds) {
		racine.Resize(kImages);
		pieds.Resize(2);
		for (int32 i = 0; i < 2; ++i) {
			pieds[(usize)i].Resize(kImages);
		}
		for (usize k = 0; k < kImages; ++k) {
			const float32 u = (float32)k / (float32)(kImages - 1);
			racine[k] = NkVec3f(0.f, (0.9f + 0.02f * (std::cos(4.f * kPi * u) - 1.f)) * echelle, (kFoulee * u + 0.03f * std::sin(4.f * kPi * u)) * echelle);
			for (int32 i = 0; i < 2; ++i) {
				// Le pied 1 est en opposition : il a une demi-foulee et un demi-cycle de retard.
				const float32 v0 = u + 0.5f * (float32)i;
				const float32 tours = std::floor(v0);
				const float32 v = v0 - tours;
				float32 z = kFoulee * (tours - 0.5f * (float32)i), y = kCheville;
				if (v >= kAppui) {
					const float32 w = (v - kAppui) / (1.f - kAppui);
					z += kFoulee * Lisse(w);
					y += 0.08f * std::sin(kPi * w);
				}
				pieds[(usize)i][k] = NkVec3f((i == 0 ? 0.1f : -0.1f) * echelle, y * echelle, z * echelle);
			}
		}
	}

	float32 Ecartement(const NkVec3f &a, const NkVec3f &b) {
		const float32 x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
		return std::sqrt(x * x + y * y + z * z);
	}

} // namespace

// -----------------------------------------------------------------------------
// GC1 — LA FOULEE, LA VITESSE, LE RESTE. La foulee d'un cycle est retrouvee (1,2 m),
// la vitesse aussi (0,857 m/s) ; le reste de la racine est nul aux deux bouts et
// vaut, au quart du cycle, le rebond connu (-4 cm) ; les pieds, dans le repere
// moyen, BOUCLENT (la derniere image vaut la premiere).
//                                                      [mutation : gait_no_stride]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, GC1_La_foulee_la_vitesse_et_le_reste_d_un_cycle_de_marche) {
	Mutations _m;
	NkVector<NkVec3f> racine;
	NkVector<NkVector<NkVec3f>> pieds;
	Marche(1.f, racine, pieds);
	NkGaitCycle g;
	ASSERT_TRUE_MSG(NkAnalyseGait(racine, pieds, kDuree, 0.f, g) && g.Valid(), "le cycle s'analyse");
	const NkVec3f quart = NkGaitRootResidual(g, 0.25f);
	const float32 bouts = std::fmax(Ecartement(g.residual[0], NkVec3f(0.f, 0.f, 0.f)), Ecartement(g.residual[kImages - 1], NkVec3f(0.f, 0.f, 0.f)));
	float32 boucle = 0.f;
	for (uint32 i = 0; i < 2u; ++i) {
		boucle = std::fmax(boucle, Ecartement(g.feet[i].track[0], g.feet[i].track[kImages - 1]));
	}
	std::printf("  [GC1] foulee %.4f m (attendue %.4f), vitesse %.4f m/s ; reste aux bouts %.1e ; rebond au quart du cycle %.4f m ; boucle des pieds %.1e m\n",
				(double)g.StrideLength(), (double)kFoulee, (double)g.Speed(), (double)bouts, (double)quart.y, (double)boucle);
	ASSERT_TRUE_MSG(std::fabs(g.StrideLength() - kFoulee) < 1e-5f, "la foulee est le deplacement de la racine sur un cycle");
	ASSERT_TRUE_MSG(std::fabs(g.Speed() - kFoulee / kDuree) < 1e-5f, "la vitesse est la foulee par la duree");
	ASSERT_TRUE_MSG(bouts < 1e-5f, "le reste de la racine est nul aux deux bouts");
	ASSERT_TRUE_MSG(std::fabs(quart.y + 0.04f) < 1e-4f && std::fabs(quart.x) < 1e-6f, "le reste rend le rebond du bassin");
	ASSERT_TRUE_MSG(boucle < 1e-5f, "dans le repere moyen, les pieds bouclent");
	ASSERT_TRUE_MSG(std::fabs(g.feet[0].floor - kCheville) < 1e-6f && NkGaitFootLift(g, 0u, 0.8f) > 0.07f && NkGaitFootLift(g, 0u, 0.3f) < 1e-5f,
					"la levee du pied : nulle en appui, 8 cm au milieu du vol");
}

// -----------------------------------------------------------------------------
// GC2 — UN HOTE QUI AVANCE A LA VITESSE DU CYCLE GARDE SES PIEDS PLANTES. L'hote
// avance en ligne droite a `Speed()` ; il pose chaque pied a « son repere + la piste
// du pied ». Sur trois cycles, un pied plante ne bouge pas dans le monde ; chaque
// pied est plante 60 % du cycle ; il y a toujours un pied au sol.
//                                                      [mutation : gait_no_stride]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, GC2_A_la_vitesse_du_cycle_un_pied_plante_reste_plante) {
	Mutations _m;
	NkVector<NkVec3f> racine;
	NkVector<NkVector<NkVec3f>> pieds;
	Marche(1.f, racine, pieds);
	NkGaitCycle g;
	ASSERT_TRUE_MSG(NkAnalyseGait(racine, pieds, kDuree, 0.f, g), "le cycle s'analyse");
	const float32 v = g.Speed();
	const NkVec3f avant = g.StrideLength() > 0.f ? NkVec3f(g.stride.x / g.StrideLength(), g.stride.y / g.StrideLength(), g.stride.z / g.StrideLength())
												 : NkVec3f(0.f, 0.f, 1.f);
	float32 glisse = 0.f;
	bool unAuSol = true;
	NkVec3f ancre[2];
	bool ancree[2] = {false, false};
	const int32 pas = 1200; // 400 par cycle
	for (int32 n = 0; n <= pas; ++n) {
		const float32 t = 3.f * kDuree * (float32)n / (float32)pas;
		const float32 phase = t / kDuree;
		const NkVec3f repere(g.origin.x + avant.x * v * t, g.origin.y + avant.y * v * t, g.origin.z + avant.z * v * t);
		bool auSol = false;
		for (uint32 i = 0; i < 2u; ++i) {
			const NkVec3f p = NkGaitFootPoint(g, i, phase);
			const NkVec3f monde(repere.x + p.x, repere.y + p.y, repere.z + p.z);
			// Plante, et loin des bords de son appui (une image de marge de chaque cote).
			const float32 marge = 1.5f / (float32)(kImages - 1);
			const bool plante = NkGaitFootPlanted(g, i, phase) && NkGaitFootPlanted(g, i, phase - marge) && NkGaitFootPlanted(g, i, phase + marge);
			auSol = auSol || NkGaitFootPlanted(g, i, phase);
			if (!plante) {
				ancree[i] = false;
				continue;
			}
			if (!ancree[i]) {
				ancree[i] = true;
				ancre[i] = monde;
			}
			glisse = std::fmax(glisse, Ecartement(monde, ancre[i]));
		}
		unAuSol = unAuSol && auSol;
	}
	std::printf("  [GC2] l'hote avance a %.4f m/s sur 3 cycles : un pied plante bouge de %.2e m au plus ; plantes %.0f %% et %.0f %% du cycle ; "
				"toujours un pied au sol : %s ; glissement du clip lui-meme : %.1e m\n",
				(double)v, (double)glisse, 100.0 * (double)g.feet[0].plantedShare, 100.0 * (double)g.feet[1].plantedShare, unAuSol ? "oui" : "NON",
				(double)std::fmax(g.feet[0].slip, g.feet[1].slip));
	ASSERT_TRUE_MSG(glisse < 2e-4f, "a la vitesse du cycle, un pied plante ne bouge pas dans le monde");
	ASSERT_TRUE_MSG(std::fabs(g.feet[0].plantedShare - kAppui) < 0.06f && std::fabs(g.feet[1].plantedShare - kAppui) < 0.06f,
					"chaque pied est plante la part du cycle qu'il faut");
	ASSERT_TRUE_MSG(unAuSol, "en marchant, il y a toujours un pied au sol");
	ASSERT_TRUE_MSG(g.feet[0].slip < 1e-5f && g.feet[1].slip < 1e-5f, "le clip de synthese ne glisse pas");
}

// -----------------------------------------------------------------------------
// GC3 — TOUT EST LINEAIRE : UN PERSONNAGE AUX JAMBES PLUS COURTES. La meme marche a
// l'echelle 0,6 : la foulee, le reste, les pistes des pieds et leur hauteur a plat
// sont ceux du grand, multiplies par 0,6 ; les parts d'appui sont les memes.
//                                                      [mutation : gait_no_stride]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, GC3_Un_cycle_de_marche_se_met_a_l_echelle_du_personnage) {
	Mutations _m;
	NkVector<NkVec3f> r1, r2;
	NkVector<NkVector<NkVec3f>> p1, p2;
	Marche(1.f, r1, p1);
	Marche(0.6f, r2, p2);
	NkGaitCycle grand, petit;
	ASSERT_TRUE_MSG(NkAnalyseGait(r1, p1, kDuree, 0.f, grand) && NkAnalyseGait(r2, p2, kDuree, 0.f, petit), "les deux cycles s'analysent");
	float32 pire = std::fabs(petit.StrideLength() - 0.6f * grand.StrideLength());
	for (usize k = 0; k < kImages; ++k) {
		const NkVec3f a = grand.residual[k], b = petit.residual[k];
		pire = std::fmax(pire, Ecartement(NkVec3f(0.6f * a.x, 0.6f * a.y, 0.6f * a.z), b));
		for (uint32 i = 0; i < 2u; ++i) {
			const NkVec3f c = grand.feet[i].track[k], d = petit.feet[i].track[k];
			pire = std::fmax(pire, Ecartement(NkVec3f(0.6f * c.x, 0.6f * c.y, 0.6f * c.z), d));
		}
	}
	std::printf("  [GC3] a l'echelle 0,6 : foulee %.4f m (0,6 x %.4f) ; ecart avec le grand cycle mis a l'echelle : %.1e m ; appui %.0f %% et %.0f %%\n",
				(double)petit.StrideLength(), (double)grand.StrideLength(), (double)pire, 100.0 * (double)petit.feet[0].plantedShare,
				100.0 * (double)grand.feet[0].plantedShare);
	ASSERT_TRUE_MSG(std::fabs(petit.StrideLength() - 0.6f * kFoulee) < 1e-5f, "la foulee suit la taille du personnage");
	ASSERT_TRUE_MSG(pire < 1e-5f, "le reste et les pistes des pieds suivent la taille du personnage");
	ASSERT_TRUE_MSG(std::fabs(petit.feet[0].floor - 0.6f * kCheville) < 1e-6f, "la hauteur de la cheville a plat aussi");
	ASSERT_TRUE_MSG(petit.feet[0].plantedShare == grand.feet[0].plantedShare && petit.feet[1].plantedShare == grand.feet[1].plantedShare,
					"les parts d'appui ne dependent pas de la taille");
	// Des suites qui ne se correspondent pas sont refusees.
	NkVector<NkVector<NkVec3f>> faux = p1;
	faux[1].Resize(kImages - 3);
	NkGaitCycle rien;
	ASSERT_TRUE_MSG(!NkAnalyseGait(r1, faux, kDuree, 0.f, rien) && !rien.Valid(), "une piste de pied plus courte que la racine est refusee");
}
