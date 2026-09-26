// =============================================================================
// @File    Applications/NkFilMoteurProbe/src/main.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   TRAVERSER LE FIL DE `NkEngineLayer` — une scène porte `NkSkeleton` +
//          `NkFootIK`, et **rien ici ne configure l'IK**. C'est la couche qui
//          doit le faire, ou le défaut est là.
//
// POURQUOI CE BANC EXISTE
//   `NkEngineLayer::RegisterCoreSystems` enregistre `NkPhysicsSystem` et, depuis
//   le 2026-09-26, `NkFootIKSystem` avec son `SetPhysicsWorld`. Mais **aucune
//   application ne portait d'entité `NkFootIK`** : le système tournait sur zéro
//   entité. Un fil posé, enregistré, jamais traversé — exactement la situation
//   qui a laissé le pont physique cassé sans témoin pendant des mois.
//
//   ⚠️ CE BANC NE FAIT AUCUN APPEL À `SetPhysicsWorld`, NI À `AddSystem`. Il
//      monte une scène et demande à la couche de tourner. Si les pieds épousent
//      le relief, le fil traverse. Sinon, le défaut est dans la couche — et
//      c'est ce qu'on cherche.
//
// CE QUE CE BANC N'EST PAS
//   Ce n'est pas une démo : il n'affiche rien, il rend un verdict chiffré. La
//   démo visible de l'IK des pieds est `NkDemoPiedsSurLaPente`.
//
// ⚠️ SANS FENÊTRE. `NkEngineLayer::OnAttach` refusait de s'attacher hors d'une
//    `NkApplication` (déréférencement de `sInstance`). Corrigé le même jour :
//    elle continue sans rendu et le dit. *Une couche qu'on ne peut pas attacher
//    sans fenêtre n'est pas éprouvable — et c'est une des raisons pour
//    lesquelles son fil n'avait jamais été traversé.*
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "NKECS/World/NkWorld.h"
#include "Noge/Layers/NkEngineLayer.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/Anim/NkLocomotion.h"
#include "NKLogger/NkLog.h"

#include <cstdio>

using namespace nkentseu;

namespace {

	int gEchecs = 0;

	void Critere(const char *quoi, bool ok, const char *detail) {
		std::printf("  [%s] %s\n", ok ? "OK" : "ECHEC", quoi);
		if (detail && detail[0] != '\0')
			std::printf("        %s\n", detail);
		if (!ok)
			++gEchecs;
	}

	const float32 kPenteDeg = 14.f;
	const float32 kPI = 3.14159265358979f;
	const float32 kFootHeight = 0.02f;
	const uint32 kOs = 9;

	math::NkQuatf QuatZ(float32 rad) noexcept {
		math::NkQuatf q;
		q.x = 0.f;
		q.y = 0.f;
		q.z = math::NkSin(rad * 0.5f);
		q.w = math::NkCos(rad * 0.5f);
		return q;
	}

	// Un squelette plat de neuf os, indices alignés sur les défauts de `NkFootIK`
	// (même montage que `NkLocomotionDemo`, et que l'éprouvette de la pente).
	const NkVec3f kRepos[kOs] = {
		{0.f, 1.00f, 0.f},		// hip
		{-0.15f, 0.90f, 0.f},	// leftThigh
		{-0.15f, 0.50f, 0.f},	// leftCalf
		{-0.15f, 0.05f, 0.f},	// leftFoot
		{-0.15f, 0.00f, 0.15f}, // leftToe
		{0.15f, 0.90f, 0.f},	// rightThigh
		{0.15f, 0.50f, 0.f},	// rightCalf
		{0.15f, 0.05f, 0.f},	// rightFoot
		{0.15f, 0.00f, 0.15f},	// rightToe
	};

	// La scène : une pente statique et un personnage. RIEN de plus — pas un
	// seul appel à un système.
	void MonterLaScene(ecs::NkWorld &monde, ecs::NkEntityId &perso, float32 x) {
		{
			const ecs::NkEntityId pente = monde.CreateEntity();
			ecs::NkTransform tf;
			tf.localRotation = QuatZ(kPenteDeg * kPI / 180.f);
			ecs::NkRigidbody3D rb;
			rb.bodyType = ecs::NkBodyType::Static;
			ecs::NkCollider3D col;
			col.shape = ecs::NkCollider3DShape::Box;
			col.boxSize = {40.f, 1.f, 40.f};
			monde.Add<ecs::NkTransform>(pente, tf);
			monde.Add<ecs::NkRigidbody3D>(pente, rb);
			monde.Add<ecs::NkCollider3D>(pente, col);
		}

		perso = monde.CreateEntity();
		{
			memory::NkSharedPtr<anim::NkSkeletonDef> def(new anim::NkSkeletonDef());
			def->bones.Resize((NkVector<anim::NkBoneDef>::SizeType)kOs);
			for (uint32 i = 0; i < kOs; ++i) {
				anim::NkBoneDef &b = def->bones[(NkVector<anim::NkBoneDef>::SizeType)i];
				b.parent = -1;
				b.bindPose = math::NkMat4f::Translate(kRepos[i]);
				b.inverseBindPose = b.bindPose.Inverse();
			}
			ecs::NkSkeleton sk = ecs::NkSkeleton::FromDef(def);
			const float32 leve = x * math::NkTan(kPenteDeg * kPI / 180.f);
			for (uint32 i = 0; i < kOs; ++i)
				sk.Pose(i).localPosition = {kRepos[i].x + x, kRepos[i].y + leve, kRepos[i].z};

			NkFootIK fk;
			fk.footHeight = kFootHeight;
			fk.rayLength = 4.f;
			fk.blendSpeed = 30.f;

			monde.Add<ecs::NkTransform>(perso, ecs::NkTransform{});
			monde.Add<ecs::NkSkeleton>(perso, sk);
			monde.Add<NkFootIK>(perso, fk);
		}
	}

} // namespace

int main() {
	std::printf("=== NkFilMoteurProbe : le fil de NkEngineLayer traverse-t-il ?\n");
	std::printf("compile le %s a %s\n\n", __DATE__, __TIME__);

	// ⚠️ La couche est attachée SANS NkApplication. Elle doit le dire et
	//    continuer ; si elle plante ici, c'est le premier défaut.
	NkEngineLayer *couche = new NkEngineLayer();
	couche->OnAttach();

	Critere("la couche s'attache SANS fenetre", NkEngineLayer::IsReady(),
			"sinon : InitRenderer dereference NkApplication::sInstance");
	if (!NkEngineLayer::IsReady()) {
		std::printf("\nStatus: ROUGE (%d echec(s)) -- rien d'autre n'est mesurable\n", gEchecs);
		return 1;
	}

	ecs::NkWorld &monde = couche->GetWorld();

	// ── Combien de systemes la couche a-t-elle enregistres ? ─────────────────
	const uint32 nbSys = couche->GetScheduler().SystemCount();
	char d0[160];
	std::snprintf(d0, sizeof(d0), "%u systemes enregistres par RegisterCoreSystems", nbSys);
	Critere("la couche enregistre ses systemes", nbSys >= 6u, d0);

	// ── La scene, montee A LA MAIN mais SANS TOUCHER AUX SYSTEMES ───────────
	ecs::NkEntityId perso{};
	const float32 kX = 3.f;
	MonterLaScene(monde, perso, kX);

	// ⚠️ ET C'EST TOUT. On demande a la couche de tourner. Aucun
	//    `SetPhysicsWorld`, aucun `AddSystem`, aucun `Execute` direct.
	const float32 kFixe = 1.f / 120.f;
	for (int32 i = 0; i < 240; ++i) {
		couche->OnFixedUpdate(kFixe);
		couche->OnUpdate(kFixe);
	}

	// ── Ou est le pied ? ────────────────────────────────────────────────────
	const ecs::NkSkeleton *sk = monde.Get<ecs::NkSkeleton>(perso);
	const NkFootIK *fk = monde.Get<NkFootIK>(perso);
	if (sk == nullptr || fk == nullptr) {
		Critere("l'entite porte encore Skeleton + FootIK", false, "composant disparu");
		std::printf("\nStatus: ROUGE (%d echec(s))\n", gEchecs);
		return 1;
	}

	// Le sommet de la pente a cette abscisse, calcule geometriquement. On ne
	// peut pas raycaster ici : le monde physique appartient au systeme de la
	// couche, et ce banc a justement pour regle de ne rien lui demander.
	const float32 solAttendu = 0.5f / math::NkCos(kPenteDeg * kPI / 180.f)
							   + kX * math::NkTan(kPenteDeg * kPI / 180.f);
	const float32 piedY = sk->Pose(fk->leftFootIdx).localPosition.y;
	const float32 ecart = piedY - (solAttendu + kFootHeight);

	char d1[224];
	std::snprintf(d1, sizeof(d1),
				  "x=%+.1f : sol attendu %.4f   pied %.4f   ecart %+.4f   (grounded %s)", kX,
				  solAttendu, piedY, ecart, fk->leftFoot.isGrounded ? "oui" : "NON");
	// Tolerance large : ce banc mesure SI LE FIL TRAVERSE, pas la precision de
	// l'IK -- celle-la est mesuree par NkDemoPiedsSurLaPente, au centimetre.
	Critere("LE FIL TRAVERSE : le pied a quitte sa pose de depart pour le relief",
			ecart > -0.15f && ecart < 0.15f, d1);

	// ── Le NEGATIF : sans le monde physique, le pied suit un sol a y = 0 ────
	// On ne peut pas retirer l'enregistrement du systeme depuis ici sans
	// modifier la couche ; on mesure donc l'autre bout du meme fil : le pied
	// doit etre LOIN du plan plat, sinon c'est le repli qui l'a place.
	char d2[160];
	std::snprintf(d2, sizeof(d2), "pied %.4f contre %.4f si le repli plan-plat l'avait place",
				  piedY, kFootHeight);
	Critere("ce n'est PAS le repli plan-plat qui a place le pied",
			(piedY > kFootHeight + 0.20f) || (piedY < kFootHeight - 0.20f), d2);

	couche->OnDetach();
	delete couche;

	std::printf("\nStatus: %s (%d echec(s))\n", gEchecs == 0 ? "VERT" : "ROUGE", gEchecs);
	return gEchecs == 0 ? 0 : 1;
}
