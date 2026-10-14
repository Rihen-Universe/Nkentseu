// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_formes_combinaison.cpp : LE BANC DES FORMES DE COMBINAISON ET
// DES INTERMEDIAIRES (Morph/NkShapeCombos.h, 14/10/2026).
//
// Le critere est celui du facial (note du 13/09) : UN SOMMET BOUGE, mesure en
// position, jamais « les poids sont calcules ». Un maillage de trois sommets :
//   0 la machoire (forme pleine « bouche_ouverte » et son intermediaire a 0,5, qui
//     suit un ARC : il n'est pas le milieu de la base et de la forme pleine) ;
//   1 le coin des levres (« sourire ») ;
//   2 la joue, que seule la corrective « sourire_ouvert » deplace.
//
// Chaque critere est MESURE (la valeur est imprimee) ; sa mutation, dans le MEME
// binaire par NK_RT_MUTATION, doit le faire rougir :
//
//   au curseur 0,5 la machoire EST l'intermediaire          combo_no_inbetween (FC1)
//   la corrective vit du produit des curseurs              combo_off          (FC2, FC3)
//   la rampe douce n'est pas une droite                    combo_ramp_linear  (FC3)
//   une forme menee par deux maitres est refusee           combo_two_masters  (FC1)
//
//   NK_RT_MUTATION=combo_off ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKAnima/NKAnima.h"
#include "NKAnima/Morph/NkShapeCombos.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::anim;
using math::NkVec3f;

namespace {

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				uint32 bits = 0u;
				if (e != nullptr) {
					bits |= std::strstr(e, "combo_no_inbetween") ? (uint32)NK_SC_MUT_NO_INBETWEEN : 0u;
					bits |= std::strstr(e, "combo_off") ? (uint32)NK_SC_MUT_COMBO_OFF : 0u;
					bits |= std::strstr(e, "combo_ramp_linear") ? (uint32)NK_SC_MUT_RAMP_LINEAR : 0u;
					bits |= std::strstr(e, "combo_two_masters") ? (uint32)NK_SC_MUT_TWO_MASTERS : 0u;
				}
				NkShapeComboSetMutations(bits);
			}
			~Mutations() {
				NkShapeComboSetMutations(0u);
			}
	};

	NkVec3f P(float32 x, float32 y, float32 z) {
		NkVec3f v;
		v.x = x;
		v.y = y;
		v.z = z;
		return v;
	}
	float32 Ecart(const NkVec3f &a, const NkVec3f &b) {
		return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
	}

	/// Le visage de trois sommets et ses formes : 0 base, 1 bouche_ouverte, 2 son
	/// intermediaire, 3 sourire, 4 sourire_ouvert (corrective).
	struct Visage {
			NkShapeKeySet set;
			int32 ouverte = -1, milieu = -1, sourire = -1, correctif = -1;
			void Monter() {
				const NkVec3f base[3] = {P(0.f, 0.f, 0.f), P(1.f, 0.f, 0.f), P(0.f, 1.f, 0.f)};
				set.SetBasis(base, 3);
				NkVec3f f[3] = {base[0], base[1], base[2]};
				f[0] = P(0.f, -1.f, 0.f);
				ouverte = set.AddFromPositions("bouche_ouverte", f, 3);
				f[0] = P(0.3f, -0.5f, 0.f); // l'arc de la machoire : pas le milieu (0, -0,5)
				milieu = set.AddFromPositions("bouche_ouverte_50", f, 3);
				f[0] = base[0];
				f[1] = P(1.f, 0.2f, 0.f);
				sourire = set.AddFromPositions("sourire", f, 3);
				f[1] = base[1];
				f[2] = P(0.f, 1.3f, 0.f);
				correctif = set.AddFromPositions("sourire_ouvert", f, 3);
			}
			/// Le maillage pour des curseurs donnes, par les combinaisons.
			void Jouer(const NkShapeCombos &c, const float32 *curseurs, NkVector<NkVec3f> &out) const {
				float32 poids[8] = {};
				c.Resolve(curseurs, poids, set.Count());
				set.Evaluate(out, poids);
			}
	};

} // namespace

// -----------------------------------------------------------------------------
// FC1 : L'INTERMEDIAIRE. Le curseur de « bouche_ouverte » a 0 : la base au bit
// pres ; a 0,5 : la machoire EST l'intermediaire (sur son arc) ; a 1 : la forme
// pleine ; a 0,25 et 0,75 : les milieux des deux segments.  [mutation : combo_no_inbetween]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, FC1_L_intermediaire_est_atteint_a_sa_station) {
	Mutations _m;
	Visage v;
	v.Monter();
	ASSERT_TRUE(v.correctif == 4);
	NkShapeCombos c;
	NkShapeInBetween g;
	g.primary = v.ouverte;
	g.shapes.PushBack(v.milieu);
	g.at.PushBack(0.5f);
	c.inBetweens.PushBack(g);
	ASSERT_TRUE_MSG(c.Validate(v.set.Count()), "le groupe est coherent");
	float32 cur[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
	NkVector<NkVec3f> out;
	v.Jouer(c, cur, out);
	const bool baseIntacte = std::memcmp(out.Data(), v.set.keys[0].positions.Data(), sizeof(NkVec3f) * 3) == 0;
	const float32 stations[4] = {0.25f, 0.5f, 0.75f, 1.f};
	const NkVec3f attendus[4] = {P(0.15f, -0.25f, 0.f), P(0.3f, -0.5f, 0.f), P(0.15f, -0.75f, 0.f), P(0.f, -1.f, 0.f)};
	float32 pire = 0.f;
	NkVec3f a50 = P(0.f, 0.f, 0.f);
	for (int32 i = 0; i < 4; ++i) {
		cur[v.ouverte] = stations[i];
		v.Jouer(c, cur, out);
		pire = std::fmax(pire, Ecart(out[0], attendus[i]));
		if (i == 1) {
			a50 = out[0];
		}
	}
	std::printf("  [FC1] curseur 0 : base au bit pres = %d ; curseur 0,5 : machoire en (%.4f, %.4f), l'intermediaire en (0,3000, -0,5000) ; "
				"pire ecart aux 4 stations %.2e\n",
				baseIntacte ? 1 : 0, (double)a50.x, (double)a50.y, (double)pire);
	ASSERT_TRUE_MSG(baseIntacte, "curseur 0 : la base au bit pres");
	ASSERT_TRUE_MSG(pire < 1e-6f, "chaque station est atteinte, et la course est droite entre deux stations");

	// Un groupe incoherent est refuse et ignore.
	NkShapeCombos faux;
	NkShapeInBetween gf = g;
	gf.at[0] = 1.2f;
	faux.inBetweens.PushBack(gf);
	ASSERT_TRUE_MSG(!faux.Validate(v.set.Count()), "une station hors de ]0, 1[ est refusee");
	// Une forme menee deux fois (intermediaire ET corrective) est refusee.
	NkShapeCombos double_;
	double_.inBetweens.PushBack(g);
	NkShapeCombination kc;
	kc.shape = v.milieu;
	kc.drivers.PushBack(v.sourire);
	double_.combinations.PushBack(kc);
	ASSERT_TRUE_MSG(!double_.Validate(v.set.Count()), "une forme menee par deux maitres est refusee");
}

// -----------------------------------------------------------------------------
// FC2 : LA CORRECTIVE. « sourire_ouvert » = produit des curseurs « sourire » et
// « bouche_ouverte » : sourire seul, la joue ne bouge pas ; les deux a 1, elle
// monte de 0,3 ; les deux a 0,5, de 0,075 (produit) ou de 0,15 (minimum).
//                                                           [mutation : combo_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, FC2_La_corrective_vit_de_la_combinaison) {
	Mutations _m;
	Visage v;
	v.Monter();
	NkShapeCombos c;
	NkShapeCombination k;
	k.shape = v.correctif;
	k.drivers.PushBack(v.sourire);
	k.drivers.PushBack(v.ouverte);
	c.combinations.PushBack(k);
	ASSERT_TRUE(c.Validate(v.set.Count()));
	NkVector<NkVec3f> out;
	float32 cur[5] = {0.f, 0.f, 0.f, 1.f, 0.f};
	v.Jouer(c, cur, out);
	const float32 seul = out[2].y;
	cur[v.ouverte] = 1.f;
	v.Jouer(c, cur, out);
	const float32 deux = out[2].y;
	cur[v.ouverte] = 0.5f;
	cur[v.sourire] = 0.5f;
	v.Jouer(c, cur, out);
	const float32 produit = out[2].y;
	c.combinations[0].mode = NkShapeComboMode::NK_MIN;
	v.Jouer(c, cur, out);
	const float32 minimum = out[2].y;
	std::printf("  [FC2] joue : sourire seul y = %.4f (1), les deux a 1 y = %.4f (1,3), les deux a 0,5 : produit %.4f (1,075), minimum %.4f (1,15)\n",
				(double)seul, (double)deux, (double)produit, (double)minimum);
	ASSERT_TRUE_MSG(std::fabs(seul - 1.f) < 1e-6f, "un seul pilote : la corrective dort");
	ASSERT_TRUE_MSG(std::fabs(deux - 1.3f) < 1e-6f, "les deux pilotes pleins : la corrective pleine");
	ASSERT_TRUE_MSG(std::fabs(produit - 1.075f) < 1e-6f && std::fabs(minimum - 1.15f) < 1e-6f, "produit et minimum a mi-course");
}

// -----------------------------------------------------------------------------
// FC3 : UNE FORME PILOTEE PAR UNE AUTRE, PAR UNE COURBE. La corrective suit le seul
// « sourire » par une rampe douce de 0,2 a 0,8 : a 0,1 rien, a 0,5 la moitie, a
// 0,35 le quart de la course donne 0,15625 (3u^2 - 2u^3), pas 0,25 (la droite), a
// 0,9 tout.                              [mutations : combo_ramp_linear ; combo_off]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, FC3_Une_forme_pilotee_par_une_autre_par_une_courbe) {
	Mutations _m;
	Visage v;
	v.Monter();
	NkShapeCombos c;
	NkShapeCombination k;
	k.shape = v.correctif;
	k.drivers.PushBack(v.sourire);
	k.ramp.inMin = 0.2f;
	k.ramp.inMax = 0.8f;
	k.ramp.smooth = true;
	c.combinations.PushBack(k);
	const float32 entrees[4] = {0.1f, 0.35f, 0.5f, 0.9f};
	const float32 attendus[4] = {0.f, 0.15625f, 0.5f, 1.f};
	float32 pire = 0.f, a35 = 0.f;
	NkVector<NkVec3f> out;
	for (int32 i = 0; i < 4; ++i) {
		float32 cur[5] = {0.f, 0.f, 0.f, entrees[i], 0.f};
		v.Jouer(c, cur, out);
		const float32 poids = (out[2].y - 1.f) / 0.3f; // ce que la joue a parcouru de sa course
		pire = std::fmax(pire, std::fabs(poids - attendus[i]));
		if (i == 1) {
			a35 = poids;
		}
	}
	std::printf("  [FC3] rampe douce 0,2 -> 0,8 : a 0,35 la joue a fait %.5f de sa course (0,15625 ; la droite donnerait 0,25) ; pire ecart %.2e\n",
				(double)a35, (double)pire);
	ASSERT_TRUE_MSG(pire < 1e-5f, "la corrective suit la courbe de sa pilote");
}
