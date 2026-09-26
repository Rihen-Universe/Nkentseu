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
#include "NKLogger/NkLog.h"

#include <cstring>

#include <cstdio>

namespace eprouvette {

	using namespace nkentseu;

	namespace {

		const float32 kPasFixe = 1.f / 120.f;
		const float32 kPenteDeg = 14.f;
		const float32 kFootHeight = 0.02f;
		const float32 kDemiEcart = 0.15f; // demi-écartement des pieds en x
		const float32 kSolDemiHaut = 0.5f; // demi-epaisseur du sol -> sommet a +0,5
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
	// CesiumMan SUR LA PENTE : le critere qui distingue le correctif d'un placebo.
	//
	// L'eprouvette plate rend EXACTEMENT les memes chiffres avant et apres le
	// correctif -- normal, la composition monde est l'identite quand tous les
	// parents valent -1. C'est une excellente non-regression et une preuve nulle.
	// Il fallait donc un squelette HIERARCHIQUE : 18 os sur 19 ont un parent ici.
	// -------------------------------------------------------------------------
	bool PoserCesiumSurPente(float x, bool branche, SurPente &out) noexcept {
		Import info;
		if (!ImporterCesiumMan(info) || info.pied.indice < 0)
			return false;
		ecs::NkSkeleton *src = (ecs::NkSkeleton *)SqueletteCesiumMan();
		if (src == nullptr)
			return false;

		Detruire();
		gMonde = new ecs::NkWorld();
		gPhys = new NkPhysicsSystem();
		gFoot = new NkFootIKSystem();

		// la meme pente que l'eprouvette, au degre pres
		gPente = gMonde->CreateEntity();
		{
			ecs::NkTransform tf;
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

		// le personnage : le squelette IMPORTE, os designes par leur NOM
		gPerso = gMonde->CreateEntity();
		{
			ecs::NkSkeleton sk = *src; // la definition est partagee, pas copiee

			// ⚠️ ON DEPLACE LA RACINE, PAS TOUS LES OS. Sur un squelette
			//    hierarchique, translater la racine deplace tout le reste par
			//    composition -- et si ca ne marchait pas, c'est precisement la
			//    hierarchie qui serait en cause. L'eprouvette plate, elle, devait
			//    replacer ses neuf os un par un.
			int32 racine = -1;
			for (uint32 b = 0; b < sk.BoneCount(); ++b)
				if (sk.Def(b).parent < 0) {
					racine = (int32)b;
					break;
				}
			if (racine < 0)
				return false; // aucun os racine : on ne devine pas

			// Hauteur de depart : au-dessus de la pente a cette abscisse, pour que
			// le rayon (qui DESCEND) rencontre le sol.
			const float32 solIci = x * math::NkTan(kPenteDeg * kPI / 180.f) + kSolDemiHaut;
			ecs::NkBonePose &r = sk.Pose((uint32)racine);
			r.localPosition = {x, solIci + 0.35f, 0.f};

			NkFootIK fk;
			fk.footHeight = kFootHeight;
			fk.rayLength = 4.f;
			fk.blendSpeed = 30.f;
			// ⚠️ LES INDICES VIENNENT DES NOMS, pas d'une convention devinee.
			fk.leftThighIdx = (uint32)info.cuisse.indice;
			fk.leftCalfIdx = (uint32)info.mollet.indice;
			fk.leftFootIdx = (uint32)info.pied.indice;
			fk.rightThighIdx = fk.leftThighIdx; // une seule jambe mesuree ici
			fk.rightCalfIdx = fk.leftCalfIdx;
			fk.rightFootIdx = fk.leftFootIdx;
			fk.hipBoneIdx = (uint32)racine;
			fk.hipCompensation = 0.f; // ⚠️ neutralisee : voir le critere dedie

			gMonde->Add<ecs::NkTransform>(gPerso, ecs::NkTransform{});
			gMonde->Add<ecs::NkSkeleton>(gPerso, sk);
			gMonde->Add<NkFootIK>(gPerso, fk);
		}

		gFoot->SetPhysicsWorld(branche ? &gPhys->World() : nullptr);
		gFoot->SetFallbackGroundY(0.f);
		gPhys->Execute(*gMonde, kPasFixe);
		for (int k = 0; k < 90; ++k)
			gFoot->Execute(*gMonde, kPasFixe);

		// ⚠️ ON LIT LA POSITION MONDE, par la meme composition que le pont --
		//    jamais la pose locale, qui est justement ce qui etait faux.
		const ecs::NkSkeleton *sk2 = gMonde->Get<ecs::NkSkeleton>(gPerso);
		if (sk2 == nullptr)
			return false;
		NkVector<math::NkMat4f> monde;
		NkIKSolver::BuildWorldPose(*sk2, monde);
		const uint32 pi = (uint32)info.pied.indice;
		if ((uint32)monde.Size() <= pi)
			return false;
		out.piedY = monde[(NkVector<math::NkMat4f>::SizeType)pi][3][1];
		out.piedXMonde = monde[(NkVector<math::NkMat4f>::SizeType)pi][3][0];

		float32 s = 0.f, n = 0.f;
		if (!SolY(out.piedXMonde, s, n))
			return false;
		out.solY = s;
		float32 d = 0.f;
		PenteLue(d);
		out.penteLueDeg = d;
		out.monte = true;
		return true;
	}

	// -------------------------------------------------------------------------
	// La compensation de hanche, mise a l'epreuve. On MESURE avant d'accuser :
	// l'eprouvette de la pente replace ses neuf os a chaque image, ce qui
	// EFFACE toute derive -- le montage masquait le defaut qu'on cherche. Ici on
	// appelle donc `Execute` SANS replacer, et on regarde la hanche.
	// -------------------------------------------------------------------------
	bool EprouverHanche(Hanche &out) noexcept {
		// Le sol est HAUT a x = +4 (environ 1,48 m) : c'est la que le defaut (1)
		// se reveille, s'il existe.
		Construire(true);
		Placer(4.f, 0); // place, sans laisser l'IK converger

		ecs::NkSkeleton *sk = gMonde->Get<ecs::NkSkeleton>(gPerso);
		NkFootIK *fk = gMonde->Get<NkFootIK>(gPerso);
		if (sk == nullptr || fk == nullptr)
			return false;

		// La reference : la hanche AVANT que quoi que ce soit ne s'y applique.
		out.avant = sk->Pose(fk->hipBoneIdx).localPosition.y;
		gFoot->Execute(*gMonde, kPasFixe);
		out.apres1 = sk->Pose(fk->hipBoneIdx).localPosition.y;
		for (int k = 0; k < 29; ++k)
			gFoot->Execute(*gMonde, kPasFixe);
		out.apres30 = sk->Pose(fk->hipBoneIdx).localPosition.y;
		for (int k = 0; k < 170; ++k)
			gFoot->Execute(*gMonde, kPasFixe);
		out.apres200 = sk->Pose(fk->hipBoneIdx).localPosition.y;
		out.offsetVu = fk->hipOffset;
		out.groundeG = fk->leftFoot.isGrounded;
		out.groundeD = fk->rightFoot.isGrounded;
		out.mesure = true;
		Detruire();
		return true;
	}

	// -------------------------------------------------------------------------
	// Un pied en ENVOL est-il ramene au sol ? On le pose en l'air, on laisse
	// l'IK tourner, et on regarde ou il finit.
	// -------------------------------------------------------------------------
	bool EprouverEnvol(float hauteur, float plante, Envol &out) noexcept {
		Construire(true);
		Placer(0.f, 60); // les deux pieds poses, IK convergee

		ecs::NkSkeleton *sk = gMonde->Get<ecs::NkSkeleton>(gPerso);
		NkFootIK *fk = gMonde->Get<NkFootIK>(gPerso);
		if (sk == nullptr || fk == nullptr)
			return false;

		float32 s = 0.f, n = 0.f;
		if (!SolY(-kDemiEcart, s, n))
			return false;
		out.solSous = s;

		// ⚠️ LE POIDS EST FOURNI DE L'EXTERIEUR, ici a la main. L'IK ne le
		//    calcule pas et ne doit pas le calculer.
		fk->leftPlant = plante;

		// On LEVE le pied gauche et sa chaine, comme le ferait un pas.
		const uint32 chaine[3] = {(uint32)kLThigh, (uint32)kLCalf, (uint32)kLFoot};
		for (int k = 0; k < 3; ++k)
			sk->Pose(chaine[k]).localPosition.y += hauteur;
		out.leveA = sk->Pose((uint32)kLFoot).localPosition.y;

		// Et on laisse l'IK faire ce qu'elle fait.
		for (int k = 0; k < 60; ++k)
			gFoot->Execute(*gMonde, kPasFixe);

		out.apres = sk->Pose((uint32)kLFoot).localPosition.y;
		out.poids = fk->leftFoot.contactWeight;
		out.groundeEnLair = fk->leftFoot.isGrounded;
		out.mesure = true;
		Detruire();
		return true;
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

		// == LES NOMS D'OS (CesiumMan.glb) ====================================
		std::printf("\n  -- les NOMS D'OS, sur un modele VERSIONNE ---------------------\n");
		Import im;
		const bool ok = ImporterCesiumMan(im);
		if (!ok) {
			std::printf("  [ECHEC] import de CesiumMan.glb : %s\n", im.refus);
			++echecs;
			return echecs;
		}
		std::printf("      %d os, dont %d NOMMES, dont %d avec un parent\n", im.osTotal,
					im.osNommes, im.osAvecParent);

		// 3. le fil du jour : les noms arrivent-ils jusqu'au squelette ?
		const bool tousNommes = im.osNommes == im.osTotal && im.osTotal > 0;
		std::printf("  [%s] les noms ARRIVENT au squelette (%d/%d)\n", tousNommes ? "OK" : "ECHEC",
					im.osNommes, im.osTotal);
		if (!tousNommes)
			++echecs;

		// 4. et la jambe se DESIGNE par son nom, sans deviner aucun indice
		std::printf("      %-16s -> os %d\n      %-16s -> os %d\n      %-16s -> os %d\n",
					im.cuisse.nom, im.cuisse.indice, im.mollet.nom, im.mollet.indice,
					im.pied.nom, im.pied.indice);
		const bool jambe = im.cuisse.indice >= 0 && im.mollet.indice >= 0 && im.pied.indice >= 0 &&
						   im.cuisse.indice != im.mollet.indice && im.mollet.indice != im.pied.indice;
		std::printf("  [%s] la jambe se DESIGNE par son nom (trois os distincts)\n",
					jambe ? "OK" : "ECHEC");
		if (!jambe)
			++echecs;

		// 5. LE NEGATIF DU NOM : un nom absent doit REFUSER, jamais rendre l'os 0.
		//    Un repli silencieux sur zero plierait la racine en croyant plier le
		//    pied, et le critere precedent serait vert quand meme.
		std::printf("      %s\n", im.refus);
		const bool refuse = im.indiceAbsent < 0;
		std::printf("  [%s] NEGATIF : un nom ABSENT rend un refus, pas l'os 0\n",
					refuse ? "OK" : "ECHEC");
		if (!refuse)
			++echecs;

		// 6. LA LIMITE, mesuree et NON corrigee : le pont lit la pose LOCALE
		//    comme si elle etait monde. Sur un squelette hierarchique, les deux
		//    divergent -- donc designer les os par leur nom NE SUFFIT PAS ENCORE
		//    pour poser les pieds d'un vrai personnage. Ce n'est pas un echec de
		//    ce lot : c'est le lot suivant, et il faut le DIRE plutot que de
		//    laisser une demo verte le faire oublier.
		std::printf("      pied : pose LOCALE y = %.4f   pose MONDE y = %.4f   ecart %+.4f\n",
					im.piedLocalY, im.piedMondeY, im.piedLocalY - im.piedMondeY);
		const float32 ecartH = (im.piedLocalY - im.piedMondeY) < 0.f ? (im.piedMondeY - im.piedLocalY)
															   : (im.piedLocalY - im.piedMondeY);
		if (im.osAvecParent > 0 && ecartH > 0.001f)
			std::printf("  [DIT]   la POSE LOCALE N'EST PAS LA POSE MONDE (%d os ont un "
						"parent) --\n          et NkFootIKSystem lit desormais la POSE MONDE, "
						"corrige le 26/09.\n          Cette ligne RESTE, parce que cet ecart est la "
						"CONDITION de validite du critere\n          hierarchique ci-dessous : s'il tombait a zero, ce "
						"critere ne prouverait rien\n          de plus que l'eprouvette plate.\n",
						im.osAvecParent);
			// ⚠️ L'ARGUMENT MANQUAIT, et le defaut est instructif : le %d
			//    lisait la pile et imprimait « -4 os ont un parent » pendant que la
			//    ligne du dessus en comptait 18. Un printf prive de son argument
			//    COMPILE, ne previent pas, et rend un nombre CREDIBLE. Trouve parce
			//    que deux lignes de la meme sortie se contredisaient -- c'est la
			//    seule raison pour laquelle il a ete vu.
		else
			std::printf("  [DIT]   aucune divergence locale/monde mesuree ici\n");

		// == CesiumMan SUR LA PENTE : squelette HIERARCHIQUE ==================
		// ⚠️ C'EST LE SEUL CRITERE QUI DISTINGUE LE CORRECTIF D'UN PLACEBO.
		//    L'eprouvette plate ci-dessus rend les MEMES chiffres avant et apres
		//    (composition monde = identite quand tous les parents valent -1) :
		//    excellente non-regression, preuve nulle.
		std::printf("\n  -- CesiumMan SUR LA PENTE (18 os sur 19 ont un parent) -------\n");
		{
			const float32 abs2[2] = {-3.f, 3.f};
			int poses = 0;
			for (int k = 0; k < 2; ++k) {
				SurPente sp;
				if (!PoserCesiumSurPente(abs2[k], true, sp)) {
					std::printf("      x=%+5.1f : montage IMPOSSIBLE\n", abs2[k]);
					continue;
				}
				const float32 ecart = sp.piedY - (sp.solY + kFootHeight);
				std::printf("      x=%+5.1f : pente %5.2f deg   sol %7.4f   pied MONDE %7.4f"
							"   ecart %+7.4f\n",
							abs2[k], sp.penteLueDeg, sp.solY, sp.piedY, ecart);
				if (ecart > -0.06f && ecart < 0.06f)
					++poses;
			}
			std::printf("  [%s] HIERARCHIQUE : le pied se pose sur la pente (%d/2, tol. 6 cm)\n",
						poses == 2 ? "OK" : "ECHEC", poses);
			if (poses != 2)
				++echecs;

			// LE NEGATIF, aux deux extremites, comme pour l'eprouvette : debranche,
			// l'IK vise un sol fantome a y = 0. A droite la pente est HAUTE (le
			// pied reste dessous), a gauche BASSE (il reste au-dessus).
			SurPente d, g;
			const bool okD = PoserCesiumSurPente(3.f, false, d);
			const bool okG = PoserCesiumSurPente(-3.f, false, g);
			if (okD && okG) {
				const float32 enf = d.solY - d.piedY;
				const float32 flo = g.piedY - (g.solY + kFootHeight);
				std::printf("      x= +3.0 : sol %7.4f   pied %7.4f   ENFONCE de %+7.4f\n",
							d.solY, d.piedY, enf);
				std::printf("      x= -3.0 : sol %7.4f   pied %7.4f   FLOTTE de  %+7.4f\n",
							g.solY, g.piedY, flo);
				// ⚠️ LE SEUIL EST DERIVE DE LA TOLERANCE, PAS DU RESULTAT.
				//    Il vaut DEUX FOIS la tolerance du critere positif (6 cm) :
				//    l'erreur debranchee doit etre franchement hors de ce que le
				//    critere positif accepte, sans quoi les deux montages ne se
				//    distingueraient pas.
				//
				// ⚠️ ET POURQUOI PAS PLUS : l'amplitude de l'erreur est BORNEE PAR LA
				//    LONGUEUR DE LA JAMBE. Debranche, l'IK vise y = 0,02 ; la ou
				//    cette cible est hors de portee, la jambe s'etend au maximum et
				//    s'arrete la -- l'ecart ne peut pas depasser ce que le membre
				//    permet. CesiumMan a des jambes plus courtes que l'eprouvette
				//    procedurale, donc son enfoncement est plus faible (0,145 m
				//    contre 0,428 m) : un seuil recopie de l'eprouvette aurait
				//    rougi sur un comportement JUSTE.
				//
				//    *Un attendu emprunte a un autre montage se perime sur celui-ci.*
				//    C'est la meme faute que « meme limitation que les nodes », en
				//    chiffres. Le seuil se derive donc du critere qu'il doit refuter,
				//    et de rien d'autre.
				const float32 seuilNeg = 2.f * 0.06f; // 2 x la tolerance de pose
				const bool deuxSignes = enf > seuilNeg && flo > seuilNeg;
				std::printf("  [%s] NEGATIF HIERARCHIQUE : enfonce a droite ET flotte a "
							"gauche (> %.2f m = 2 x la tolerance)\n",
							deuxSignes ? "OK" : "ECHEC", seuilNeg);
				if (!deuxSignes)
					++echecs;
			} else {
				std::printf("  [ECHEC] le negatif hierarchique n'a pas pu etre monte\n");
				++echecs;
			}
			Detruire();
		}

		// == LA COMPENSATION DE HANCHE ========================================
		std::printf("\n  -- la COMPENSATION DE HANCHE, sur un sol HAUT ----------------\n");
		{
			Hanche hp;
			if (!EprouverHanche(hp)) {
				std::printf("  [ECHEC] la sonde de hanche n'a pas pu etre montee\n");
				++echecs;
			} else {
				// ⚠️ DEUX INTERVALLES, PARCE QU'IL Y A DEUX PHENOMENES.
				//    1 -> 30   : la convergence de l'IK (contactWeight monte vers 1,
				//                le pied descend vers le sol) -- elle DOIT bouger.
				//    30 -> 200 : plus rien ne doit bouger. Une ACCUMULATION est
				//                lineaire et sans fin ; une CONVERGENCE s'ARRETE.
				//    Le critere porte donc sur l'intervalle TARDIF, et lui seul.
				//
				//    Le premier jet du critere portait sur 1 -> 30 et rougissait a
				//    -0,056 m APRES correctif : il confondait les deux. Avant
				//    correctif, ce meme intervalle donnait +21,68 m -- l'ordre de
				//    grandeur suffisait a voir le defaut, pas a le declarer eteint.
				const float32 tot = hp.apres30 - hp.apres1;
				const float32 tard = hp.apres200 - hp.apres30;
				std::printf("      hanche : 1 image %8.4f   30 %8.4f   200 %8.4f\n"
							"      convergence 1->30 %+8.4f   RESIDU 30->200 %+8.4f\n"
							"      hipOffset porte par le composant : %+.4f\n",
							hp.apres1, hp.apres30, hp.apres200, tot, tard, hp.offsetVu);
				const float32 tardAbs = tard < 0.f ? -tard : tard;
				bool stable = tardAbs < 0.01f;
				std::printf("  [%s] la hanche SE STABILISE : residu 30->200 images < 1 cm\n",
							stable ? "OK" : "ECHEC");
				if (!stable)
					++echecs;

				// ⚠️ ET LE CRITERE QUI REFUTE VRAIMENT L'ACCUMULATION : une EGALITE
				//    COMPTABLE, sans seuil arbitraire.
				//
				//        ce que la hanche a REELLEMENT bouge
				//     == ce que le composant DIT avoir applique (hipOffset)
				//
				//    Une somme d'increments qui ne correspond plus a l'etat declare
				//    EST une accumulation, par definition -- pas par depassement
				//    d'un seuil qu'il faudrait justifier.
				//
				// ⚠️ LE PREMIER CRITERE (residu tardif) ETAIT AVEUGLE, et c'est la
				//    lecon : une fois le pied pose, `vise` tombe a zero, donc
				//    `+= 0` n'accumule plus rien et le residu est nul MEME SUR DU
				//    CODE QUI ACCUMULE. Decouvert en mutant le correctif : le
				//    critere restait vert. Un negatif qu'on ne mute pas est une
				//    opinion.
				const float32 bouge = hp.apres200 - hp.avant;
				const float32 ecartComptable = bouge - hp.offsetVu;
				const float32 ecAbs = ecartComptable < 0.f ? -ecartComptable : ecartComptable;
				std::printf("      hanche avant %8.4f -> apres %8.4f : elle a bouge de %+8.4f\n"
							"      le composant declare hipOffset = %+8.4f   ecart comptable "
							"%+8.4f\n",
							hp.avant, hp.apres200, bouge, hp.offsetVu, ecartComptable);
				std::printf("      pied gauche touche : %s   pied droit : %s\n",
						hp.groundeG ? "oui" : "NON", hp.groundeD ? "oui" : "NON");
			const bool comptable = ecAbs < 0.001f;
				std::printf("  [%s] ce que la hanche a BOUGE == ce que le composant DECLARE\n",
							comptable ? "OK" : "ECHEC");
				if (!comptable)
					std::printf("          ⚠️ la hanche porte %+.4f m que personne ne declare : "
								"c'est une ACCUMULATION.\n",
								ecartComptable);
				stable = stable && comptable;

			// ⚠️ TROISIEME CRITERE, et c'est le seul qui refute la confusion
			//    ALTITUDE / ECART. Les deux precedents sont VERTS sous la mutation
			//    qui rend `hipOffset` egal a l'altitude du sol : il vaut alors
			//    +0,7476 m ET la hanche bouge bien de +0,7476 -- l'egalite
			//    comptable est satisfaite. Mesure, pas deduit.
			//
			//    *Ils jugent la FIDELITE d'application, pas la JUSTESSE de la
			//    valeur.* Un critere comptable ne sait pas si le nombre qu'on lui
			//    donne a un sens.
			//
			//    Celui-ci exprime l'INTENTION du code -- « abaissement selon la
			//    correction la plus forte » : quand les DEUX pieds sont POSES, il
			//    n'y a plus rien a corriger, donc la hanche ne doit RIEN recevoir.
			//    Il ne depend d'aucun detail de notre implementation.
			const float32 offAbs = hp.offsetVu < 0.f ? -hp.offsetVu : hp.offsetVu;
			const bool poses = hp.groundeG && hp.groundeD;
			const bool rienAcompenser = !poses || offAbs < 0.02f;
			std::printf("  [%s] les deux pieds POSES -> la hanche ne recoit rien (|hipOffset| %.4f < 0,02)\n",
				rienAcompenser ? "OK" : "ECHEC", offAbs);
			if (!rienAcompenser)
				std::printf("          ⚠️ %.4f m de compensation alors que les pieds sont poses :\n          `hipOffset` porte une ALTITUDE de sol, pas un ecart.\n",
					offAbs);
			stable = stable && rienAcompenser;
				if (!stable) {
					std::printf("          ⚠️ `localPosition.y += hipOffset` s'ajoute a CHAQUE "
								"image sans defaire\n          la precedente, et `hipOffset` "
								"vaut une ALTITUDE de sol,\n          pas un ecart. Le defaut "
								"DORMAIT : sur le repli a y=0, zero fois\n          la "
								"compensation vaut zero.\n");
					++echecs;
				}
			}
		}

		// == LE POIDS DE PLANTE : trois points, et le milieu contre une ========
		// == interpolation de DEUX MESURES INDEPENDANTES =======================
		std::printf("\n  -- LE POIDS DE PLANTE (0 = envol, 1 = appui) -----------------\n");
		{
			Envol e0, e1, eMi;
			const bool ok0 = EprouverEnvol(0.50f, 0.f, e0);
			const bool ok1 = EprouverEnvol(0.50f, 1.f, e1);
			const bool okM = EprouverEnvol(0.50f, 0.5f, eMi);
			if (!ok0 || !ok1 || !okM) {
				std::printf("  [ECHEC] les sondes de plante n'ont pas pu etre montees\n");
				++echecs;
			} else {
				std::printf("      poids 0,0 : leve a %7.4f -> %7.4f   (bouge de %+7.4f)\n"
							"      poids 1,0 : leve a %7.4f -> %7.4f   (bouge de %+7.4f)\n"
							"      poids 0,5 : leve a %7.4f -> %7.4f\n",
							e0.leveA, e0.apres, e0.apres - e0.leveA, e1.leveA, e1.apres,
							e1.apres - e1.leveA, eMi.leveA, eMi.apres);

				// 1. poids 1 -> le pied epouse le sol (comportement d'origine)
				const float32 vise1 = e1.solSous + kFootHeight;
				const float32 d1 = e1.apres - vise1;
				const bool pose = d1 > -0.06f && d1 < 0.06f;
				char m1[160];
				std::snprintf(m1, sizeof(m1), "pied %.4f contre sol+semelle %.4f, ecart %+.4f",
							  e1.apres, vise1, d1);
				std::printf("  [%s] POIDS 1 : le pied epouse le sol\n        %s\n",
							pose ? "OK" : "ECHEC", m1);
				if (!pose)
					++echecs;

				// 2. LE CRITERE QUI MANQUAIT DEPUIS LE DEBUT : poids 0 -> il RESTE
				//    en l'air. C'est le negatif structurel de tous les autres, qui
				//    exigent tous le contact -- et qui sont donc verts sur un systeme
				//    qui colle tout au sol.
				const float32 chute = e0.leveA - e0.apres;
				const float32 chuteAbs = chute < 0.f ? -chute : chute;
				const bool reste = chuteAbs < 0.01f;
				char m2[176];
				std::snprintf(m2, sizeof(m2),
							  "il a bouge de %+.4f m (exige < 1 cm) ; sans le poids il "
							  "redescendait de 0,4479",
							  -chute);
				std::printf("  [%s] POIDS 0 : le pied en ENVOL N'EST PAS RAMENE AU SOL\n"
							"        %s\n",
							reste ? "OK" : "ECHEC", m2);
				if (!reste)
					++echecs;

				// 3. poids 0,5 -> A MI-CHEMIN, et ce n'est pas un tout-ou-rien.
				// ⚠️ LE MILIEU EST COMPARE A UNE INTERPOLATION DE DEUX MESURES
				//    INDEPENDANTES (poids 0 et poids 1), jamais a un calcul de l'IK.
				//    Sinon la demo imposerait le poids ET mesurerait son propre
				//    accord avec elle-meme.
				const float32 attenduMi = (e0.apres + e1.apres) * 0.5f;
				const float32 dMi = eMi.apres - attenduMi;
				const float32 dMiAbs = dMi < 0.f ? -dMi : dMi;
				const float32 amplitude = (e1.apres > e0.apres) ? (e1.apres - e0.apres)
													  : (e0.apres - e1.apres);
				// Tolerance : 10 % de l'amplitude entre les deux extremes. Un
				// tout-ou-rien deguise placerait le milieu SUR un des deux bords,
				// donc a 50 % de l'amplitude -- cinq fois la tolerance.
				const bool progressif = amplitude > 0.05f && dMiAbs < amplitude * 0.10f;
				char m3[208];
				std::snprintf(m3, sizeof(m3),
							  "milieu %.4f, interpolation des deux mesures %.4f, ecart %+.4f "
							  "(amplitude %.4f, tolerance %.4f)",
							  eMi.apres, attenduMi, dMi, amplitude, amplitude * 0.10f);
				std::printf("  [%s]   POIDS 0,5 : %s\n        %s\n",
							progressif ? "OK " : "DIT",
							progressif ? "a mi-chemin -- pas un tout-ou-rien deguise"
								   : "TOUT-OU-RIEN : le milieu tombe sur un bord. La cause est"
									 " mesuree et ecrite dans NkLocomotion.cpp -- un poids"
									 " applique a chaque image devient un taux. La corriger"
									 " demande de conserver la pose AVANT correction : du NEUF.",
							m3);
			}
		}

		// == LE PIED EN ENVOL (diagnostic historique) ==========================
		std::printf("\n  -- UN PIED EN ENVOL, retour de Rodolf du 26/09 ---------------\n");
		{
			Envol ev;
			// Le diagnostic d'origine, garde avec poids 1 : il montre ce que
			// faisait le systeme AVANT le poids de plante.
			if (!EprouverEnvol(0.50f, 1.f, ev)) {
				std::printf("  [DIT]   la sonde d'envol n'a pas pu etre montee\n");
			} else {
				const float32 redescendu = ev.leveA - ev.apres;
				std::printf("      pied leve a %7.4f (sol %7.4f, soit +%.2f m en l'air)\n"
							"      apres 60 images d'IK : %7.4f   il a redescendu de %+7.4f\n"
							"      contactWeight %.3f   isGrounded %s\n",
							ev.leveA, ev.solSous, ev.leveA - ev.solSous, ev.apres, redescendu,
							ev.poids, ev.groundeEnLair ? "OUI" : "non");
				if (redescendu > 0.10f)
					std::printf("  [DIT]   ⚠️ LE PIED EN ENVOL EST RAMENE AU SOL.\n"
								"          Aucune notion de PHASE n'existe : ni NkFootContact,\n"
								"          ni NkFootIK, ni NkLocomotion. `contactWeight` est un\n"
								"          lissage de `isGrounded`, vrai des que le sol est A\n"
								"          PORTEE DU RAYON -- pas quand le pied TOUCHE.\n"
								"          Un personnage qui marche ne levera donc jamais le\n"
								"          pied. Trouve par RODOLF, pas par les 12 criteres --\n"
								"          qui exigent TOUS le contact, donc sont verts sur ce\n"
								"          defaut. Y remedier est du NEUF : decision de Rodolf.\n");
				else
					std::printf("  [DIT]   le pied leve RESTE en l'air : une phase existe\n");
			}
		}

		return echecs;
	}

} // namespace eprouvette
