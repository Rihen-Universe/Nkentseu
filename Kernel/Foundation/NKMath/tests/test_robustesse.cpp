// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
//
// ROBUSTESSE DE NKMATH (08/10/2026). Cinq defauts trouves en confrontant le code aux
// livres (ETUDE_LIVRES/SYNTHESE.md, lignes A1 a A5 : Tremblay ch. 19, Dunn et Parberry
// ch. 10). Chaque cas ci-dessous a d'abord ECHOUE sur le code d'avant : c'etait la
// condition pour le garder.
//   A1  un vecteur `double` calculait sa longueur en `float32` (7 chiffres) ;
//   A2  (PAS ICI) sous une longueur de 1e-6, `Normalize()` ne fait rien et `Len()` rend 0,
//       sans rien dire. Prouve (le cas a ete ecrit, il rougit), mais descendre le seuil
//       deplace le tissu de NKPhysics : voir NkVec.h, « CONNU, PROUVE, ET PAS ENCORE
//       CORRIGE », et sa condition de retrait. Le cas reviendra ici avec le correctif ;
//   A3  « de (0,0,1) vers -Z a l'arrondi pres » rendait un quaternion de norme 1e-7,
//       donc aucune rotation au lieu d'un demi-tour ;
//   A4  l'inverse d'une matrice d'echelle 5e-5 etait... l'identite (determinant sous
//       un seuil ABSOLU) ;
//   A5  NkFabs(-0) rendait -0.
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::math;

TEST_CASE(NKMathRobustesse, A1_LongueurEnDouble) {
	// 1 + 1e-10 ne tient pas en float32 : la longueur doit etre calculee en double
	const NkVec3d v(1.0, 1.0e-5, 0.0);
	ASSERT_NEAR(1.00000000005, v.Len(), 1.0e-13);
	const NkVec3d n = v.Normalized();
	ASSERT_NEAR(1.0, n.x * n.x + n.y * n.y + n.z * n.z, 1.0e-14);
}

TEST_CASE(NKMathRobustesse, A3_VersLOppose) {
	// de (0,0,1) vers (sin e, 0, -cos e) : un quaternion UNITAIRE, qui amene bien l'un sur l'autre
	const NkVec3f de(0.0f, 0.0f, 1.0f);
	const float32 ecarts[8] = {0.0f, 1.0e-9f, 1.0e-7f, 1.0e-6f, 1.0e-5f, 1.0e-4f, 1.0e-3f, 1.0e-2f};
	for (int i = 0; i < 8; ++i) {
		const NkVec3f vers(NkSin(ecarts[i]), 0.0f, -NkCos(ecarts[i]));
		const NkQuatf q(de, vers);
		const float32 norme2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
		ASSERT_NEAR(1.0f, norme2, 1.0e-4f);
		const NkVec3f r = q * de;
		ASSERT_NEAR(vers.x, r.x, 1.0e-3f);
		ASSERT_NEAR(vers.y, r.y, 1.0e-3f);
		ASSERT_NEAR(vers.z, r.z, 1.0e-3f);
	}
}

TEST_CASE(NKMathRobustesse, A4_InverseDUnePetiteEchelle) {
	const float32 echelles[4] = {5.0e-5f, 1.0e-3f, 1.0f, 2.0e4f};
	for (int i = 0; i < 4; ++i) {
		const float32 s = echelles[i];
		const NkMat4f m = NkMat4f::Scaling(NkVec3f(s, s, s));
		const NkMat4f p = m.Inverse() * m;
		for (int c = 0; c < 4; ++c)
			for (int l = 0; l < 4; ++l)
				ASSERT_NEAR(c == l ? 1.0f : 0.0f, p.mat[c][l], 1.0e-4f);
	}
	// une matrice VRAIMENT singuliere (une colonne nulle) rend toujours l'identite de secours
	const NkMat4f plate = NkMat4f::Scaling(NkVec3f(1.0f, 0.0f, 1.0f));
	const NkMat4f secours = plate.Inverse();
	ASSERT_NEAR(1.0f, secours.mat[1][1], 1.0e-6f);
}

TEST_CASE(NKMathRobustesse, A5_ValeurAbsolueDeMoinsZero) {
	volatile float32 moinsZero = -0.0f; // volatile : le compilateur ne plie pas la division
	const float32 inverse = 1.0f / NkFabs(moinsZero);
	ASSERT_TRUE(inverse > 0.0f); // +infini, pas -infini
	volatile float64 moinsZeroD = -0.0;
	const float64 inverseD = 1.0 / NkFabs(moinsZeroD);
	ASSERT_TRUE(inverseD > 0.0);
}
