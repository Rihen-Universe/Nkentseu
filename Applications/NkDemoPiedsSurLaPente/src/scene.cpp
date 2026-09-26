// =============================================================================
// @File    Applications/NkDemoPiedsSurLaPente/src/scene.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   La scène : deux jambes procédurales, une pente ORIENTÉE, le pont ECS
//          de Noge et l'IK des pieds. ⚠️ N'inclut JAMAIS NKCanvas — voir scene.h.
//
// ÉPROUVETTE, pas un personnage. Aucun humanoïde riggé n'est atteignable :
// `CesiumMan.glb` est versionné, mais `NkGLTFIO` reconstruit un `ecs::NkSkeleton`
// dont `NkBone::name` reste VIDE, et `NkFootIK` veut des INDICES de cuisse, de
// mollet et de pied. *Un indice n'est pas un nom* : les deviner fabriquerait un
// faux signal. Le squelette est donc procédural — neuf os, mêmes indices que les
// valeurs par défaut de `NkFootIK`, repris de `Applications/NkLocomotionDemo`.
//
// CONDITION DE RETRAIT : le jour où un humanoïde riggé sera chargeable avec des
// os NOMMÉS, cette éprouvette est remplacée par lui.
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "scene.h"

#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"
#include "Noge/Anim/NkLocomotion.h"

#include <cstdio>

namespace eprouvette {

	using namespace nkentseu;

	namespace {

		const float32 kPasFixe = 1.f / 120.f;
		const float32 kPenteDeg = 14.f;
		const float32 kFootHeight = 0.02f;
		const float32 kDemiEcart = 0.15f; // demi-écartement des pieds en x
		const float32 kPI = 3.14159265358979f;

		math::NkQuatf QuatZ(float32 rad) noexcept {
			math::NkQuatf q;
			q.x = 0.f;
			q.y = 0.f;
			q.z = math::NkSin(rad * 0.5f);
			q.w = math::NkCos(rad * 0.5f);
			return q;
		}

		// Pose de repos, debout, centrée sur x = 0.
		const NkVec3f kRepos[kOs] = {
			{0.f, 1.00f, 0.f},				// hip
			{-kDemiEcart, 0.90f, 0.f},		// leftThigh
			{-kDemiEcart, 0.50f, 0.f},		// leftCalf
			{-kDemiEcart, 0.05f, 0.f},		// leftFoot
			{-kDemiEcart, 0.00f, 0.15f},	// leftToe
			{kDemiEcart, 0.90f, 0.f},		// rightThigh
			{kDemiEcart, 0.50f, 0.f},		// rightCalf
			{kDemiEcart, 0.05f, 0.f},		// rightFoot
			{kDemiEcart, 0.00f, 0.15f},		// rightToe
		};

		ecs::NkWorld *gMonde = nullptr; // ⚠️ sur le TAS : NkWorld pèse 791 640 o
		NkPhysicsSystem *gPhys = nullptr;
		NkFootIKSystem *gFoot = nullptr;
		ecs::NkEntityId gPente{}, gPerso{};

		bool SolY(float32 x, float32 &y, float32 &normY) noexcept {
			if (!gPhys)
				return false;
			collision::NkRay3D r;
			r.origin = {x, 40.f, 0.f};
			r.dir = {0.f, -1.f, 0.f};
			r.maxT = 200.f;
			physics::NkBodyId b{};
			collision::NkRayHit3D h;
			if (!gPhys->World().Raycast(r, b, h))
				return false;
			y = h.point.y;
			normY = h.normal.y;
			return true;
		}

		bool PenteLue(float32 &deg) noexcept {
			float32 y1 = 0.f, y2 = 0.f, n1 = 0.f, n2 = 0.f;
			if (!SolY(-4.f, y1, n1) || !SolY(4.f, y2, n2))
				return false;
			deg = math::NkAtan2(y2 - y1, 8.f) * 180.f / kPI;
			return true;
		}

		float32 PiedY(uint32 idx) noexcept {
			const ecs::NkSkeleton *sk = gMonde ? gMonde->Get<ecs::NkSkeleton>(gPerso) : nullptr;
			return sk ? sk->Pose(idx).localPosition.y : 0.f;
		}

	} // namespace

	const char *DateCompilationScene() noexcept {
		return __DATE__ " a " __TIME__;
	}

	float PenteConsigneDeg() noexcept {
		return kPenteDeg;
	}

	float HauteurSemelle() noexcept {
		return kFootHeight;
	}

	float DemiEcartementPieds() noexcept {
		return kDemiEcart;
	}

	void Detruire() noexcept {
		delete gFoot;
		delete gPhys;
		delete gMonde;
		gFoot = nullptr;
		gPhys = nullptr;
		gMonde = nullptr;
	}

	void Construire(bool branche) noexcept {
		Detruire();
		gMonde = new ecs::NkWorld();
		gPhys = new NkPhysicsSystem();
		gFoot = new NkFootIKSystem();

		// ── la pente : une boîte ORIENTÉE ────────────────────────────────────
		// ⚠️ L'orientation ne suffit pas à la faire exister : il a fallu que
		//    `NkPhysicsSystem::MakeShape` la reporte sur la FORME (corrigé le
		//    26/09). Une forme droite sous un corps incliné s'annulent, et l'on
		//    mesure une pente sur un sol plat. C'est pour cela que la pente est
		//    LUE plus bas, et jamais supposée.
		gPente = gMonde->CreateEntity();
		{
			ecs::NkTransform tf;
			tf.localPosition = {0.f, 0.f, 0.f};
			tf.localRotation = QuatZ(kPenteDeg * kPI / 180.f);
			ecs::NkRigidbody3D rb;
			rb.bodyType = ecs::NkBodyType::Static;
			ecs::NkCollider3D col;
			col.shape = ecs::NkCollider3DShape::Box;
			col.boxSize = {40.f, 1.f, 40.f};
			gMonde->Add<ecs::NkTransform>(gPente, tf);
			gMonde->Add<ecs::NkRigidbody3D>(gPente, rb);
			gMonde->Add<ecs::NkCollider3D>(gPente, col);
		}

		// ── les jambes ───────────────────────────────────────────────────────
		gPerso = gMonde->CreateEntity();
		{
			memory::NkSharedPtr<anim::NkSkeletonDef> def(new anim::NkSkeletonDef());
			def->bones.Resize((NkVector<anim::NkBoneDef>::SizeType)kOs);
			for (uint32 i = 0; i < (uint32)kOs; ++i) {
				anim::NkBoneDef &b = def->bones[(NkVector<anim::NkBoneDef>::SizeType)i];
				b.parent = -1; // squelette PLAT : ce que le pont suppose
				b.bindPose = math::NkMat4f::Translate(kRepos[i]);
				b.inverseBindPose = b.bindPose.Inverse();
			}
			ecs::NkSkeleton sk = ecs::NkSkeleton::FromDef(def);
			for (uint32 i = 0; i < (uint32)kOs; ++i)
				sk.Pose(i).localPosition = kRepos[i];

			NkFootIK fk; // ⚠️ dans `nkentseu`, PAS dans `nkentseu::ecs`
			fk.footHeight = kFootHeight;
			fk.rayLength = 4.f;
			fk.blendSpeed = 30.f; // converge vite : une éprouvette n'attend pas

			gMonde->Add<ecs::NkTransform>(gPerso, ecs::NkTransform{});
			gMonde->Add<ecs::NkSkeleton>(gPerso, sk);
			gMonde->Add<NkFootIK>(gPerso, fk);
		}

		// ⚠️ LE FIL, ET C'EST TOUT LE SUJET DE CETTE ÉPROUVETTE.
		gFoot->SetPhysicsWorld(branche ? &gPhys->World() : nullptr);
		gFoot->SetFallbackGroundY(0.f);

		// Un pas de physique : les corps naissent et leurs formes se posent
		// avant le premier raycast. Sans lui, le rayon ne toucherait rien.
		gPhys->Execute(*gMonde, kPasFixe);
	}

	void Placer(float x, int pas) noexcept {
		if (!gMonde || !gFoot)
			return;
		ecs::NkSkeleton *sk = gMonde->Get<ecs::NkSkeleton>(gPerso);
		if (!sk)
			return;
		// Les jambes montent avec la pente : sinon, à droite, elles naîtraient
		// SOUS le sol et le rayon (qui descend) ne toucherait rien. Ce n'est pas
		// une triche : c'est la hanche qu'un contrôleur de personnage porterait.
		const float32 leve = x * math::NkTan(kPenteDeg * kPI / 180.f);
		for (uint32 i = 0; i < (uint32)kOs; ++i)
			sk->Pose(i).localPosition = {kRepos[i].x + x, kRepos[i].y + leve, kRepos[i].z};
		for (int i = 0; i < pas; ++i)
			gFoot->Execute(*gMonde, kPasFixe);
	}

	void Lire(float x, Etat &out) noexcept {
		const ecs::NkSkeleton *sk = gMonde ? gMonde->Get<ecs::NkSkeleton>(gPerso) : nullptr;
		if (sk)
			for (uint32 i = 0; i < (uint32)kOs; ++i) {
				out.osX[i] = sk->Pose(i).localPosition.x;
				out.osY[i] = sk->Pose(i).localPosition.y;
			}

		float32 d = 0.f;
		out.penteLisible = PenteLue(d);
		out.penteLueDeg = d;

		float32 y = 0.f, n = 0.f;
		out.solLisible = SolY(x - kDemiEcart, y, n);
		out.solSousPiedG = y;

		float32 ya = 0.f, yb = 0.f, na = 0.f, nb = 0.f;
		out.solTracable = SolY(-7.f, ya, na) && SolY(7.f, yb, nb);
		out.solGaucheY = ya;
		out.solDroiteY = yb;
	}

	// -------------------------------------------------------------------------
	int Mesurer() noexcept {
		int echecs = 0;
		Construire(true);

		// -- 0. LA CONDITION D'ESSAI : la pente existe-t-elle vraiment ? ------
		// Sans ce critère, tout ce qui suit pourrait porter sur un sol plat en
		// restant parfaitement cohérent. C'est arrivé le 13/09.
		float32 penteLue = 0.f;
		const bool aPente = PenteLue(penteLue);
		const bool penteOk = aPente && penteLue > kPenteDeg - 0.5f && penteLue < kPenteDeg + 0.5f;
		std::printf("  [%s] la PENTE EXISTE : lue %.3f deg, consigne %.3f deg\n",
					penteOk ? "OK" : "ECHEC", penteLue, kPenteDeg);
		if (!penteOk) {
			std::printf("        rayon sans contact : %s\n", aPente ? "non" : "OUI");
			std::printf("        si 0,000 deg, le sol est PLAT : la forme sans orientation a\n");
			std::printf("        annule la rotation du corps, et rien d'autre n'est mesurable.\n");
			++echecs;
		}

		// -- 1. BRANCHE : le pied epouse la pente, a trois abscisses ----------
		const float32 abscisses[3] = {-4.f, 0.f, 4.f};
		int bons = 0;
		for (int k = 0; k < 3; ++k) {
			Placer(abscisses[k], 90);
			float32 solY = 0.f, nY = 0.f;
			if (!SolY(abscisses[k] - kDemiEcart, solY, nY))
				continue;
			const float32 pied = PiedY(kLFoot);
			const float32 ecart = pied - (solY + kFootHeight);
			std::printf("      x=%+5.1f : sol %7.4f   pied gauche %7.4f   ecart %+7.4f\n",
						abscisses[k], solY, pied, ecart);
			if (ecart > -0.06f && ecart < 0.06f)
				++bons;
		}
		std::printf("  [%s] BRANCHE : le pied se pose sur la pente (%d/3 abscisses, tol. 6 cm)\n",
					bons == 3 ? "OK" : "ECHEC", bons);
		if (bons != 3)
			++echecs;

		// -- 2. LE NEGATIF : debranche, le pied vise un sol FANTOME a y = 0 ---
		//
		// ⚠️ ON MESURE LES DEUX EXTREMITES, et c'est le point. A droite la pente
		//    est HAUTE : le pied reste dessous, il TRAVERSE. A gauche elle est
		//    BASSE : le pied reste au-dessus, il FLOTTE. Ne montrer qu'un cote
		//    laisserait croire que « le pied suit le plan plat » -- ce qui est
		//    faux : la cible du repli (y = 0,02) est souvent HORS DE PORTEE de la
		//    jambe, et l'IK deux os ne peut pas l'allonger. Le pied reste donc
		//    la ou la pose l'avait mis. Deux erreurs de SIGNE OPPOSE prouvent
		//    qu'il ne suit pas le relief ; une seule aurait pu venir d'autre chose.
		Construire(false);
		float32 enfonce = 0.f, flotte = 0.f;
		{
			Placer(4.f, 90);
			float32 s = 0.f, n = 0.f;
			SolY(4.f - kDemiEcart, s, n);
			const float32 p = PiedY(kLFoot);
			enfonce = s - p;
			std::printf("      x= +4.0 : sol %7.4f   pied %7.4f   ENFONCE de %+7.4f\n", s, p, enfonce);
		}
		{
			Placer(-4.f, 90);
			float32 s = 0.f, n = 0.f;
			SolY(-4.f - kDemiEcart, s, n);
			const float32 p = PiedY(kLFoot);
			flotte = p - (s + kFootHeight);
			std::printf("      x= -4.0 : sol %7.4f   pied %7.4f   FLOTTE de  %+7.4f\n", s, p, flotte);
		}
		const bool traverse = enfonce > 0.30f;
		const bool auDessus = flotte > 0.30f;
		std::printf("  [%s] NEGATIF a droite : le pied TRAVERSE la pente (exige > 0,30 m)\n",
					traverse ? "OK" : "ECHEC");
		std::printf("  [%s] NEGATIF a gauche : le pied FLOTTE au-dessus (exige > 0,30 m)\n",
					auDessus ? "OK" : "ECHEC");
		if (!traverse)
			++echecs;
		if (!auDessus)
			++echecs;

		Detruire();
		return echecs;
	}

} // namespace eprouvette
