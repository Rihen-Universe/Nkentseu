// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKAnima/tests/test_pli.cpp — LE BANC DE L'OS D'AIDE D'UNE ARTICULATION QUI PLIE
// (Realtime/NkRtBend.h, 13/10/2026).
//
// D'ou il vient : le coude du patient virtuel de PV3DE s'ecrasait a forte flexion
// (Rodolf, 13/10 : « on doit l'ameliorer »). Une peau melangee lineairement entre
// deux os tombe sur la corde : cos(A / 2) de son rayon. Un os d'aide a mi-flexion
// la ramene a cos(A / 4) au pire, et a son rayon entier au milieu.
//
// Chaque critere est MESURE (la valeur est imprimee) ; la mutation, posee dans le
// MEME binaire par NK_RT_MUTATION :
//
//   l'os d'aide prend la moitie de la flexion          bend_no_half   (PL1, PL3)
//
//   NK_RT_MUTATION=bend_no_half ./Build/Tests/Debug-Windows/NKAnima_Tests.exe
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
	float32 Deg(float32 rad) {
		return rad * 180.f / 3.14159265358979f;
	}

	struct Mutations {
			Mutations() {
				const char *e = std::getenv("NK_RT_MUTATION");
				NkRtSetMutations((e != nullptr && std::strstr(e, "bend_no_half") != nullptr) ? (uint32)NK_RT_MUT_BEND_NO_HALF : 0u);
			}
			~Mutations() {
				NkRtSetMutations(0u);
			}
	};

} // namespace

// -----------------------------------------------------------------------------
// PL1 — LA ROTATION DE L'OS D'AIDE : la moitie de la flexion, autour du meme axe,
// sur des reperes quelconques ; sans flexion, la reference au bit pres.
//                                                        [mutation : bend_no_half]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, PL1_Os_d_aide_a_la_moitie_de_la_flexion) {
	Mutations _m;
	float32 pireAngle = 0.f, pireAxe = 0.f;
	for (int32 tordu = 0; tordu < 2; ++tordu) {
		NkRtTestHumanoid h;
		NkRtMakeTestHumanoid(h, tordu != 0);
		const NkRtQuat ref = QFromMat(h.bindLocal[(usize)h.foreArmR]);
		const NkVec3f axeAvant = Normalize(Origin(h.bindLocal[(usize)h.handR]), V3(0.f, 1.f, 0.f));
		const NkVec3f charniere = AnyPerpendicular(axeAvant);
		for (float32 deg : {20.f, 90.f, 140.f, 160.f}) {
			const NkRtQuat plie = QMul(ref, QFromAxisAngle(charniere, Rad(deg)));
			const NkRtQuat aide = NkRtBendHelperRotation(ref, plie);
			const NkVec3f r = QToRotVec(QMul(QConj(ref), aide));
			pireAngle = std::fmax(pireAngle, std::fabs(Deg(Dot(r, charniere)) - deg * 0.5f));
			pireAxe = std::fmax(pireAxe, Length(Sub(r, Scale(charniere, Dot(r, charniere)))));
		}
		const NkRtQuat repos = NkRtBendHelperRotation(ref, ref);
		ASSERT_TRUE_MSG(std::memcmp(&repos, &ref, sizeof(ref)) == 0, "sans flexion, l'os d'aide est a la reference, au bit pres");
	}
	std::printf("  [PL1] flexions de 20, 90, 140 et 160 deg, deux jeux de reperes : l'os d'aide a %.3f deg de la moitie, a %.2e rad de la "
				"charniere\n",
				(double)pireAngle, (double)pireAxe);
	ASSERT_TRUE_MSG(pireAngle < 0.05f, "l'os d'aide prend la moitie de la flexion (a 0,05 degre)");
	ASSERT_TRUE_MSG(pireAxe < 1e-3f, "autour du meme axe que l'articulation");
}

// -----------------------------------------------------------------------------
// PL2 — LA REPARTITION DES POIDS : la somme est gardee, le nombre de cases
// n'augmente pas (quatre cases pleines : l'os du haut et son os de torsion, l'os du
// bas et le sien), un sommet porte d'un seul cote n'est pas touche (au bit pres),
// un sommet a parts egales passe en entier sur l'os d'aide.
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, PL2_Repartition_des_poids_autour_de_l_articulation) {
	Mutations _m;
	// Les joints : 0 l'os du haut, 1 son os de torsion, 2 l'os du bas, 3 le sien, 9 l'os d'aide, 5 un voisin.
	const uint32 haut[2] = {0u, 1u}, bas[2] = {2u, 3u};
	{
		uint32 j[4] = {0u, 1u, 2u, 3u};
		float32 w[4] = {0.45f, 0.25f, 0.20f, 0.10f}; // haut 0,70 ; bas 0,30
		const float32 passe = NkRtBendReweight(j, w, 4u, haut, 2u, bas, 2u, 9u);
		float32 somme = 0.f, surAide = 0.f, surHaut = 0.f, surBas = 0.f;
		uint32 cases = 0;
		for (int32 i = 0; i < 4; ++i) {
			somme += w[i];
			cases += w[i] > 0.f ? 1u : 0u;
			surAide += (w[i] > 0.f && j[i] == 9u) ? w[i] : 0.f;
			surHaut += (w[i] > 0.f && (j[i] == 0u || j[i] == 1u)) ? w[i] : 0.f;
			surBas += (w[i] > 0.f && (j[i] == 2u || j[i] == 3u)) ? w[i] : 0.f;
		}
		std::printf("  [PL2] haut 0,70 / bas 0,30, quatre cases pleines : os d'aide %.3f (passe %.3f), haut %.3f, bas %.3f, somme %.6f, %u "
					"cases\n",
					(double)surAide, (double)passe, (double)surHaut, (double)surBas, (double)somme, cases);
		ASSERT_TRUE_MSG(std::fabs(somme - 1.f) < 1e-6f, "la somme des poids est gardee");
		ASSERT_TRUE_MSG(std::fabs(surAide - 0.6f) < 1e-6f && std::fabs(passe - 0.6f) < 1e-6f, "l'os d'aide recoit deux fois le plus petit cote");
		ASSERT_TRUE_MSG(std::fabs(surHaut - 0.4f) < 1e-6f && surBas == 0.f, "le grand cote garde la difference, le petit est vide");
		ASSERT_TRUE_MSG(cases <= 4u, "le nombre de cases occupees n'augmente pas");
		// Les proportions du grand cote sont gardees : 0,45 / 0,25.
		float32 a = 0.f, b = 0.f;
		for (int32 i = 0; i < 4; ++i) {
			a += (w[i] > 0.f && j[i] == 0u) ? w[i] : 0.f;
			b += (w[i] > 0.f && j[i] == 1u) ? w[i] : 0.f;
		}
		ASSERT_TRUE_MSG(std::fabs(a / b - 0.45f / 0.25f) < 1e-4f, "les joints du grand cote gardent leurs proportions");
	}
	{
		uint32 j[4] = {0u, 5u, 0u, 0u};
		float32 w[4] = {0.8f, 0.2f, 0.f, 0.f};
		const uint32 j0[4] = {j[0], j[1], j[2], j[3]};
		const float32 w0[4] = {w[0], w[1], w[2], w[3]};
		const float32 passe = NkRtBendReweight(j, w, 4u, haut, 2u, bas, 2u, 9u);
		ASSERT_TRUE_MSG(passe == 0.f && std::memcmp(j, j0, sizeof(j0)) == 0 && std::memcmp(w, w0, sizeof(w0)) == 0,
						"un sommet porte d'un seul cote n'est pas touche, au bit pres");
	}
	{
		uint32 j[4] = {0u, 2u, 5u, 0u};
		float32 w[4] = {0.4f, 0.4f, 0.2f, 0.f};
		(void)NkRtBendReweight(j, w, 4u, haut, 2u, bas, 2u, 9u);
		float32 surAide = 0.f, autres = 0.f;
		for (int32 i = 0; i < 4; ++i) {
			surAide += (w[i] > 0.f && j[i] == 9u) ? w[i] : 0.f;
			autres += (w[i] > 0.f && (j[i] == 0u || j[i] == 2u)) ? w[i] : 0.f;
		}
		ASSERT_TRUE_MSG(std::fabs(surAide - 0.8f) < 1e-6f && autres == 0.f, "a parts egales, le sommet passe en entier sur l'os d'aide");
	}
}

// -----------------------------------------------------------------------------
// PL3 — LE VOLUME TIENT. Un anneau de peau autour du coude de l'humanoide d'essai
// (reperes quelconques), ses points portes par le bras et l'avant-bras a toutes les
// fractions ; le coude plie de 140 degres. Sans os d'aide, le point du milieu ne
// garde que cos(70) = 34 % de sa distance au pivot ; avec lui, il la garde toute, et
// le pire de l'anneau est a cos(35) = 82 %. La peau reelle (matrices de peau) est
// comparee a la mesure analytique (NkRtBlendRadius, NkRtBendRadius). Sans flexion,
// la peau avec os d'aide est celle sans.                 [mutation : bend_no_half]
// -----------------------------------------------------------------------------
TEST_CASE(NKAnima, PL3_Le_volume_du_coude_tient_a_forte_flexion) {
	Mutations _m;
	NkRtTestHumanoid h;
	NkRtMakeTestHumanoid(h, true);
	NkRtRig rig;
	ASSERT_TRUE(rig.Build(h.parents.Data(), (uint32)h.parents.Size(), h.names.Data()));
	const uint32 n = rig.Count();
	NkVector<NkMat4f> local, monde, liaison;
	local.Resize(n);
	monde.Resize(n);
	liaison.Resize(n);
	for (uint32 j = 0; j < n; ++j) {
		local[j] = h.bindLocal[j];
	}
	rig.LocalToWorld(local.Data(), liaison.Data());
	const uint32 U = (uint32)h.upperArmR, F = (uint32)h.foreArmR;
	const NkRtQuat ref = QFromMat(h.bindLocal[F]);
	const NkVec3f axeAvant = Normalize(Origin(h.bindLocal[(usize)h.handR]), V3(0.f, 1.f, 0.f));
	const NkVec3f charniere = AnyPerpendicular(axeAvant);
	const NkVec3f pivot = Origin(liaison[F]);
	// Un point de peau a 4 cm du pivot, perpendiculaire a la charniere (dans le monde, a la liaison).
	const NkVec3f axeMonde = Normalize(QRotate(QFromMat(liaison[F]), charniere), V3(1.f, 0.f, 0.f));
	const NkVec3f rayon = Scale(AnyPerpendicular(axeMonde), 0.04f);
	const NkVec3f point = Add(pivot, rayon);
	const NkMat4f invU = liaison[U].Inverse(), invF = liaison[F].Inverse();
	for (float32 deg : {0.f, 140.f}) {
		const NkRtQuat plie = QMul(ref, QFromAxisAngle(charniere, Rad(deg)));
		local[F] = MatFrom(plie, V3(1.f, 1.f, 1.f), Origin(h.bindLocal[F]));
		rig.LocalToWorld(local.Data(), monde.Data());
		// L'os d'aide : enfant du bras, au pivot du coude, a la moitie de la flexion ; la
		// matrice de liaison de l'avant-bras.
		const NkMat4f aide = monde[U] * MatFrom(NkRtBendHelperRotation(ref, plie), V3(1.f, 1.f, 1.f), Origin(h.bindLocal[F]));
		const NkMat4f peauU = monde[U] * invU, peauF = monde[F] * invF, peauA = aide * invF;
		float32 pireSans = 1e9f, pireAvec = 1e9f, milieuSans = 0.f, milieuAvec = 0.f, pireEcartAnalytique = 0.f, pireEcartRepos = 0.f;
		for (int32 k = 0; k <= 20; ++k) {
			const float32 b = (float32)k / 20.f;
			// Sans os d'aide : le melange lineaire des deux os.
			const NkVec3f pu = TransformPoint(peauU, point), pf = TransformPoint(peauF, point), pa = TransformPoint(peauA, point);
			const NkVec3f sans = Add(Scale(pu, 1.f - b), Scale(pf, b));
			// Avec : les poids repartis par l'outil.
			uint32 j[4] = {0u, 2u, 0u, 0u};
			float32 w[4] = {1.f - b, b, 0.f, 0.f};
			const uint32 haut[1] = {0u}, bas[1] = {2u};
			(void)NkRtBendReweight(j, w, 4u, haut, 1u, bas, 1u, 9u);
			NkVec3f avec = V3(0.f, 0.f, 0.f);
			for (int32 i = 0; i < 4; ++i) {
				if (w[i] > 0.f) {
					avec = Madd(avec, j[i] == 0u ? pu : (j[i] == 2u ? pf : pa), w[i]);
				}
			}
			const float32 rs = Distance(sans, pivot) / 0.04f, ra = Distance(avec, pivot) / 0.04f;
			pireSans = std::fmin(pireSans, rs);
			pireAvec = std::fmin(pireAvec, ra);
			if (k == 10) {
				milieuSans = rs;
				milieuAvec = ra;
			}
			pireEcartAnalytique = std::fmax(pireEcartAnalytique, std::fabs(rs - NkRtBlendRadius(Rad(deg), b)));
			pireEcartAnalytique = std::fmax(pireEcartAnalytique, std::fabs(ra - NkRtBendRadius(Rad(deg), b)));
			pireEcartRepos = std::fmax(pireEcartRepos, Distance(sans, avec));
		}
		if (deg == 0.f) {
			std::printf("  [PL3] sans flexion : la peau avec os d'aide a %.2e m de la peau sans\n", (double)pireEcartRepos);
			ASSERT_TRUE_MSG(pireEcartRepos < 1e-6f, "sans flexion, la peau avec os d'aide est celle sans");
			continue;
		}
		std::printf("  [PL3] coude plie de 140 deg, 21 points de l'anneau : SANS os d'aide le pire garde %.3f de son rayon (le milieu %.3f = "
					"cos 70) ; AVEC, %.3f (le milieu %.3f) ; mesure analytique a %.1e pres\n",
					(double)pireSans, (double)milieuSans, (double)pireAvec, (double)milieuAvec, (double)pireEcartAnalytique);
		ASSERT_TRUE_MSG(std::fabs(milieuSans - std::cos(Rad(70.f))) < 2e-3f, "sans os d'aide, le point du milieu tombe a cos(70) de son rayon");
		ASSERT_TRUE_MSG(milieuAvec > 0.999f, "avec l'os d'aide, le point du milieu garde tout son rayon");
		ASSERT_TRUE_MSG(pireAvec > 0.81f, "avec l'os d'aide, aucun point de l'anneau ne garde moins de 81 % de son rayon (cos 35)");
		ASSERT_TRUE_MSG(pireEcartAnalytique < 2e-3f, "la peau reelle suit la mesure analytique (NkRtBlendRadius, NkRtBendRadius)");
	}
}
