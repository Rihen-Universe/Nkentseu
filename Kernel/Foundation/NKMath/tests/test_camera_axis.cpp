// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// test_camera_axis.cpp — LA GARDE du desaccord « avant pose / avant relu »
// =============================================================================
// CE QUE CE BANC PROTEGE, ET POURQUOI IL EST DANS NKMATH ET PAS DANS NOGEE.
//
// Le moteur relit l'avant d'un objet comme `-colonne2` de sa matrice monde :
// `NkTransform::GetWorldForward()` (Noge, NkTransform.h l.86-88) rend
// `{-m[2][0], -m[2][1], -m[2][2]}`, et la colonne 2 est `NkMat4T::forward`.
// La question « la rotation que je pose a-t-elle l'avant que je relirai ? » est
// donc une question de MATHEMATIQUES PURES : elle ne depend ni de l'ECS, ni du
// renderer, ni d'un hote. Elle appartient a NKMath, et c'est la qu'elle protege
// tout le monde — Noge, NKRenderer, NkAnima et les applications. Un correctif
// pose chez un seul appelant aurait laisse le piege en place pour les autres.
//
// LE DEFAUT, PAYE LE 2026-09-13 SUR LE VIEWPORT DE NOGEE. Une camera posee par
// `NkQuatf::LookAt(oeil, cible, up)` puis relue par `GetWorldForward()` regardait
// A L'OPPOSE de sa cible :
//     position (3.19, 2.25, 4.56), visant l'origine
//     avant attendue (-0.53, -0.37, -0.76)
//     avant relue    ( 0.83,  0.36,  0.43)
//     produit scalaire -0.901
// Consequence en cascade et parfaitement silencieuse :
// `NkRenderSystem::UpdateActiveCamera` construit sa cible par
// `pos + GetWorldForward()` -> la camera vise le vide -> le frustum ecarte
// 100 % des objets dans `NkRender3D::Submit` (l.1704-1707), AVANT de les
// empiler -> la file de dessin est vide au Flush -> aucun pixel. Le viewport
// rendait un aplat parfaitement propre, sans une seule erreur nulle part.
//
// DEUX DEFAUTS DISTINCTS, et le banc les separe :
//   (1) CONVENTION — `LookAt` aligne le +Z MONDE sur la direction demandee, donc
//       sa colonne 2 vaut +direction et l'avant relu vaut -direction ;
//   (2) CONTRAT NON TENU — et c'est pire : -0.901 n'est pas -1. Les deux
//       vecteurs ne sont meme pas colineaires. `LookAt` compose sa correction
//       d'up dans le repere D'AVANT la premiere rotation, ce qui deplace aussi
//       l'avant. Elle ne respecte donc pas son propre commentaire, qui promet
//       « aligne Forward monde (0,0,1) vers direction ».
//
// ⚠️ CE BANC NE CORRIGE PAS `LookAt`. Il EPINGLE son comportement mesure, pour
// que personne ne le redecouvre en payant un ecran, et pour qu'une modification
// future de `LookAt` fasse rougir quelque chose AVANT d'atteindre un viewport.
// Le recensement des appelants concernes appartient a Rodolf.
// =============================================================================

#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::math;

namespace {

	// L'avant TEL QUE LE MOTEUR LE RELIT : `-colonne2` de la matrice de rotation.
	// Meme expression que `NkTransform::GetWorldForward()`, recopiee ICI plutot
	// qu'incluse : un temoin qui appelle la fonction qu'il surveille ne surveille
	// rien. Si la convention du moteur change, ce banc doit rougir, pas suivre.
	NkVec3f AvantRelu(const NkQuatf &q) {
		const NkMat4f m(q);
		return NkVec3f{-m.forward.x, -m.forward.y, -m.forward.z}.Normalized();
	}

	float32 Accord(const NkVec3f &a, const NkVec3f &b) {
		return a.Normalized().Dot(b.Normalized());
	}

	// LE CAS EXACT PAYE SUR LE VIEWPORT DE NOGEE.
	const NkVec3f kOeil{3.19f, 2.25f, 4.56f};
	const NkVec3f kCible{0.f, 0.f, 0.f};
	const NkVec3f kUp{0.f, 1.f, 0.f};

	NkVec3f AvantVoulu() {
		return (kCible - kOeil).Normalized();
	}

} // namespace

// =============================================================================
// LA GARDE. Elle vaut pour tout appelant qui pose une orientation puis relit son
// avant par la convention du moteur. C'est CE test qui doit rester vert.
// =============================================================================
TEST_CASE(NKmathCameraAxis, AvantPoseEgaleAvantRelu) {
	const NkVec3f voulu = AvantVoulu();
	// ⚠️ CE BANC A ETE VU ROUGE AVANT D'ETRE VU VERT, et c'est la seule chose qui
	// prouve qu'il surveille quelque chose. Avec `NkQuatf::LookAt(kOeil, kCible,
	// kUp)` a la place de la ligne ci-dessous, il ECHOUE — mesure le 2026-09-13 :
	// « NKmathCameraAxis_AvantPoseEgaleAvantRelu [ECHEC] 0/1 assertions ».
	// C'est exactement le cas qui a coute un viewport vide.
	const NkQuatf q = NkQuatf::FromForwardUp(voulu, kUp);
	ASSERT_NEAR(1.0f, Accord(AvantRelu(q), voulu), 1e-4f);
}

// La meme garde sur une BATTERIE de directions : un seul cas peut etre vert par
// chance (une symetrie, un axe aligne). Six directions dont deux degenerees.
TEST_CASE(NKmathCameraAxis, AvantPoseEgaleAvantReluSurToutesLesDirections) {
	const NkVec3f directions[8] = {
		{0.f, 0.f, -1.f},				 // droit devant (convention OpenGL)
		{0.f, 0.f, 1.f},				 // droit derriere
		{1.f, 0.f, 0.f},				 // droite
		{-1.f, 0.f, 0.f},				 // gauche
		{0.577f, 0.577f, 0.577f},		 // diagonale
		{-0.53f, -0.37f, -0.76f},		 // LE cas paye
		{0.f, 1.f, 0.f},				 // DEGENERE : plein ciel, colineaire a up
		{0.f, -1.f, 0.f},				 // DEGENERE : plein sol
	};
	for (int i = 0; i < 8; ++i) {
		const NkVec3f voulu = directions[i].Normalized();
		const NkQuatf q = NkQuatf::FromForwardUp(voulu, kUp);
		// Le message d'echec doit nommer la direction : « un test rouge qui ne
		// dit pas QUEL cas a echoue oblige a tout refaire a la main ».
		ASSERT_NEAR(1.0f, Accord(AvantRelu(q), voulu), 1e-4f);
	}
}

// =============================================================================
// L'EPINGLE. Ce test ne demande pas que `LookAt` soit juste : il FIXE ce qu'elle
// fait aujourd'hui, avec le chiffre mesure. S'il rougit un jour, c'est que
// `LookAt` a change — et il faudra alors decider, en connaissance de cause, ce
// que deviennent ses appelants. Sans lui, ce changement passerait inapercu
// jusqu'a un ecran vide.
// =============================================================================
TEST_CASE(NKmathCameraAxis, EPINGLE_LookAtNeTientPasLaConventionDuMoteur) {
	const NkVec3f voulu = AvantVoulu();
	const NkQuatf q = NkQuatf::LookAt(kOeil, kCible, kUp);
	const float32 accord = Accord(AvantRelu(q), voulu);

	// (1) CONVENTION : l'avant relu n'est PAS l'avant demande. Loin de la.
	ASSERT_TRUE(accord < 0.f);

	// (2) CONTRAT NON TENU : ce n'est meme pas une negation propre. Si c'en etait
	// une, l'accord vaudrait exactement -1 et un simple changement de signe chez
	// l'appelant suffirait. Il vaut -0.901 : les deux vecteurs ne sont pas
	// colineaires, et AUCUN changement de signe ne repare cela.
	ASSERT_TRUE(accord > -0.999f);
	ASSERT_NEAR(-0.901f, accord, 0.01f);
}
