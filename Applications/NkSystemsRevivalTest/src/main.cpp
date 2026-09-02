// =============================================================================
// NkSystemsRevivalTest — reanimation de `Noge/Systems`, un systeme a la fois
// =============================================================================
// Definition de « fini » retenue avec le coordinateur, en TROIS conditions :
//   (1) un CORPS      -- l'Execute() n'est plus une declaration ;
//   (2) un APPELANT REEL -- le systeme est enregistre dans un NkScheduler et
//       tourne dans une vraie boucle, pas appele a la main ;
//   (3) un BANC QUI ECHOUE SI ON VIDE L'Execute() -- sans quoi « ca compile et
//       ca se lie » se ferait passer pour « ca tourne ».
//
// ⚠️ La condition (3) est celle qui coute, et c'est la seule qui distingue un
// systeme reanime d'un systeme qui se contente d'exister. Mesure du 2026-09-02 :
// les six systemes de NkPhysicsSystems.h avaient un Describe() complet et aucun
// Execute() -- l'editeur de liens ne disait rien parce que personne ne les
// appelait.
//
// CPU seul : aucun contexte GPU (la carte est a Ilyana).
// =============================================================================
#include "NKECS/World/NkWorld.h"
#include "NKECS/System/NkScheduler.h"
#include "Noge/Systems/NkPhysicsSystems.h"
#include "Noge/Physics/NkPhysicsMesh.h"
#include "Noge/ECS/Components/Animation/NkAnimation.h"
#include "Noge/ECS/Components/Core/NkTransform.h"

#include <cstdio>

using namespace nkentseu;
using nkentseu::ecs::NkScheduler;
using nkentseu::ecs::NkWorld;

namespace {

	int gPass = 0;
	int gFail = 0;

	void Check(bool cond, const char *libelle) {
		if (cond) {
			++gPass;
			std::printf("  [OK]   %s\n", libelle);
		} else {
			++gFail;
			std::printf("  [FAIL] %s\n", libelle);
		}
	}

	// Construit un squelette de n os par le chemin NOMINAL : une DEFINITION
	// partagee + FromDef. C'est le modele Unreal decide par Rodolf le 02/09 :
	// l'actif est reference, le par-instance est dimensionne au reel.
	ecs::NkSkeleton MakeSkeleton(uint32 n) {
		memory::NkSharedPtr<ecs::NkSkeletonDef> def(new ecs::NkSkeletonDef());
		def->bones.Resize((NkVector<ecs::NkBoneDef>::SizeType)n);
		return ecs::NkSkeleton::FromDef(def);
	}

	float Norme(const math::NkVec3f &v) {
		return ::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	}

} // namespace

int main() {
	std::printf("=== NkSystemsRevivalTest : Noge/Systems tourne-t-il vraiment ? ===\n");
	std::printf("     poids des composants : NkSkeleton=%zu o, NkRagdoll=%zu o, NkMotionCapture=%zu o\n",
				sizeof(ecs::NkSkeleton), sizeof(NkRagdoll), sizeof(NkMotionCapture));

	// -------------------------------------------------------------------------
	// NkJiggleBoneSystem
	// -------------------------------------------------------------------------
	// -------------------------------------------------------------------------
	// LE COMPOSANT LUI-MEME : partage reel, dimensionnement reel
	// -------------------------------------------------------------------------
	// Obligation du contrat « exactement comme Unreal » : un banc qui ECHOUE si
	// une copie d'archetype recopie plus que le necessaire.
	std::printf("-- NkSkeleton : actif partage, par-instance au reel --\n");
	{
		Check(sizeof(ecs::NkSkeleton) <= 128,
			  "sizeof(NkSkeleton) <= 128 o (etait 77 064 : c'est ce que copie CHAQUE changement d'archetype)");

		ecs::NkSkeleton a = MakeSkeleton(4);
		Check(a.BoneCount() == 4 && a.pose.Size() == 4 && a.skinMatrices.Size() == 4,
			  "un squelette a 4 os paie pour 4 (pose et matrices dimensionnees au reel)");

		// LA copie d'archetype : elle doit dupliquer la REFERENCE et le
		// par-instance, jamais la definition.
		// DEUX instances tirees du MEME actif doivent le PARTAGER. Cette
		// assertion a ete ajoutee par CONTRE-EPREUVE : une mutation qui faisait
		// copier la definition DANS FromDef (l'anti-Unreal) laissait le banc
		// VERT -- l'assertion de copie ci-dessous ne couvre que la copie de
		// composant, pas la creation. Un banc troue se decouvre en essayant de
		// le faire rougir, jamais en le regardant etre vert.
		{
			memory::NkSharedPtr<ecs::NkSkeletonDef> defCommun(new ecs::NkSkeletonDef());
			defCommun->bones.Resize((NkVector<ecs::NkBoneDef>::SizeType)3);
			ecs::NkSkeleton i1 = ecs::NkSkeleton::FromDef(defCommun);
			ecs::NkSkeleton i2 = ecs::NkSkeleton::FromDef(defCommun);
			Check(i1.def.Get() == defCommun.Get() && i2.def.Get() == defCommun.Get(),
				  "FromDef REFERENCE l'actif : deux instances partagent LE MEME NkSkeletonDef");
		}

		ecs::NkSkeleton b = a;
		Check(b.def.Get() == a.def.Get(),
			  "la copie PARTAGE la definition (meme actif, pas de copie profonde)");
		b.Pose(0).localPosition.x = 5.f;
		Check(a.Pose(0).localPosition.x == 0.f,
			  "la pose reste PAR INSTANCE : ecrire dans la copie ne touche pas l'original");
		b.skinMatrices[0][3][0] = 9.f;
		Check(a.skinMatrices[0][3][0] != 9.f,
			  "les matrices de peau restent PAR INSTANCE");

		// LE TEMOIN HISTORIQUE : quatre exemplaires sur la pile. Avant la
		// scission, ces quatre lignes suffisaient a deborder la pile (mesure :
		// 0xC00000FD, 4 x 77 064 o). Si le composant regonfle, ca replante ici.
		ecs::NkSkeleton t1 = MakeSkeleton(64), t2 = MakeSkeleton(64), t3 = MakeSkeleton(64), t4 = MakeSkeleton(64);
		Check(t1.BoneCount() + t2.BoneCount() + t3.BoneCount() + t4.BoneCount() == 256,
			  "quatre squelettes de 64 os TIENNENT sur la pile (temoin du debordement d'avant)");
	}

	std::printf("-- NkJiggleBoneSystem --\n");

	static NkWorld world;
	static NkScheduler sched;

	// (2) APPELANT REEL : le systeme est enregistre dans l'ordonnanceur, avec
	// son Describe() (groupe PostUpdate, priorite 700). On ne l'appelle jamais
	// a la main : c'est `sched.Run()` qui le declenche.
	sched.AddSystem<NkJiggleBoneSystem>();

	// Une entite portant les trois composants que le systeme lit/ecrit.
	const ecs::NkEntityId e = world.CreateEntity();

	ecs::NkTransform tf;
	tf.localPosition = {0.f, 0.f, 0.f};
	world.Add<ecs::NkTransform>(e, tf);

	// SUR LA PILE, deliberement : c'est le TEMOIN du redimensionnement. Avant
	// la scission, quatre NkSkeleton locaux (4 x 77 064 o) debordaient la pile
	// -- ce banc plantait en 0xC00000FD et les avait mis en static. Ils
	// reviennent sur la pile : si quelqu'un regonfle le composant, ce banc
	// replante au lieu de rester vert.
	ecs::NkSkeleton sk = MakeSkeleton(4); // le systeme refuse un boneIndex hors squelette
	world.Add<ecs::NkSkeleton>(e, sk);

	NkJiggleBone jb;
	jb.boneIndex = 1;
	jb.enabled = true;
	jb.stiffness = 0.7f;
	jb.damping = 0.3f;
	jb.mass = 1.f;
	jb.gravity = 0.f;
	jb.currentPos = {0.f, 0.f, 0.f};
	jb.targetPos = {1.f, 0.f, 0.f}; // cible a 1 unite sur X
	jb.velocity = {};
	world.Add<NkJiggleBone>(e, jb);

	// Temoin d'existence AVANT : sans lui, « la position a change » pourrait
	// etre vrai sur une entite jamais construite (face n2 de la grille).
	const NkJiggleBone *avant = world.Get<NkJiggleBone>(e);
	Check(avant != nullptr, "temoin : le composant NkJiggleBone existe dans le monde");
	if (!avant) {
		std::printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
		return 1;
	}
	Check(Norme(avant->currentPos) < 0.0001f, "temoin : la position de depart est bien l'origine");

	// (3) L'EFFET OBSERVABLE. Un ressort amorti tire currentPos vers targetPos.
	// Si Execute() est vide, currentPos reste a l'origine et TOUT ce qui suit
	// rougit. C'est exactement la garde qui manquait au module.
	for (int i = 0; i < 30; ++i) {
		sched.Run(world, 1.f / 60.f);
	}

	const NkJiggleBone *apres = world.Get<NkJiggleBone>(e);
	Check(apres != nullptr, "le composant est toujours la apres 30 pas");
	if (!apres) {
		std::printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
		return 1;
	}

	std::printf("     apres 30 pas : currentPos=(%.4f, %.4f, %.4f)  vitesse=%.4f\n", apres->currentPos.x,
				apres->currentPos.y, apres->currentPos.z, Norme(apres->velocity));

	Check(Norme(apres->currentPos) > 0.01f, "le systeme a REELLEMENT bouge l'os (Execute() non vide)");
	Check(apres->currentPos.x > 0.1f, "l'os se deplace vers sa cible sur X");
	Check(apres->currentPos.x <= 2.f, "le ressort ne diverge pas (pas d'explosion numerique)");

	// Un os desactive ne doit PAS bouger : le systeme honore son drapeau.
	const ecs::NkEntityId e2 = world.CreateEntity();
	world.Add<ecs::NkTransform>(e2, tf);
	world.Add<ecs::NkSkeleton>(e2, sk);
	NkJiggleBone off = jb;
	off.enabled = false;
	off.currentPos = {0.f, 0.f, 0.f};
	world.Add<NkJiggleBone>(e2, off);
	for (int i = 0; i < 30; ++i) {
		sched.Run(world, 1.f / 60.f);
	}
	const NkJiggleBone *inerte = world.Get<NkJiggleBone>(e2);
	Check(inerte && Norme(inerte->currentPos) < 0.0001f, "un os `enabled=false` ne bouge pas");

	// Un boneIndex hors squelette ne doit rien casser ni rien bouger.
	const ecs::NkEntityId e3 = world.CreateEntity();
	world.Add<ecs::NkTransform>(e3, tf);
	world.Add<ecs::NkSkeleton>(e3, sk);
	NkJiggleBone hors = jb;
	hors.boneIndex = 999; // sk.boneCount vaut 4
	hors.currentPos = {0.f, 0.f, 0.f};
	world.Add<NkJiggleBone>(e3, hors);
	for (int i = 0; i < 10; ++i) {
		sched.Run(world, 1.f / 60.f);
	}
	const NkJiggleBone *dehors = world.Get<NkJiggleBone>(e3);
	Check(dehors && Norme(dehors->currentPos) < 0.0001f, "un boneIndex hors squelette est ignore sans degat");


	// -------------------------------------------------------------------------
	// NkMocapSystem
	// -------------------------------------------------------------------------
	std::printf("-- NkMocapSystem --\n");
	{
		static NkWorld w2;
		static NkScheduler s2;
		s2.AddSystem<NkMocapSystem>(); // (2) APPELANT REEL

		const ecs::NkEntityId em = w2.CreateEntity();

		ecs::NkSkeleton sk2 = MakeSkeleton(2); // pile : cf. temoin ci-dessus
		w2.Add<ecs::NkSkeleton>(em, sk2);

		// Deux images : l'os 0 se translate de x=0 a x=10.
		NkMotionCapture mc;
		mc.fps = 10.f;      // une image toutes les 0,1 s
		mc.duration = 0.2f; // deux images
		mc.currentTime = 0.f;
		mc.playing = true;
		mc.loop = false;
		mc.speed = 1.f;
		mc.blendWeight = 1.f;

		NkMocapFrame f0;
		f0.time = 0.f;
		math::NkMat4f a0 = math::NkMat4f::Identity();
		math::NkMat4f a1 = math::NkMat4f::Identity();
		f0.boneTransforms.PushBack(a0);
		f0.boneTransforms.PushBack(a1);

		NkMocapFrame f1;
		f1.time = 0.1f;
		math::NkMat4f b0 = math::NkMat4f::Identity();
		b0[3][0] = 10.f; // translation x = 10 sur l'os 0
		math::NkMat4f b1 = math::NkMat4f::Identity();
		f1.boneTransforms.PushBack(b0);
		f1.boneTransforms.PushBack(b1);

		mc.frames.PushBack(f0);
		mc.frames.PushBack(f1);
		w2.Add<NkMotionCapture>(em, mc);

		// Temoins d'existence AVANT la mesure.
		const NkMotionCapture *mcAvant = w2.Get<NkMotionCapture>(em);
		const ecs::NkSkeleton *skAvant = w2.Get<ecs::NkSkeleton>(em);
		Check(mcAvant && mcAvant->frames.Size() == 2, "temoin : la capture porte bien 2 images");
		Check(skAvant && skAvant->skinMatrices[0][3][0] == 0.f, "temoin : l'os 0 part d'une translation nulle");

		// Un demi-pas d'image : on vise le MILIEU des deux images (t=0,05 s),
		// donc une translation attendue autour de 5 si l'interpolation marche.
		s2.Run(w2, 0.05f);

		const ecs::NkSkeleton *skApres = w2.Get<ecs::NkSkeleton>(em);
		const NkMotionCapture *mcApres = w2.Get<NkMotionCapture>(em);
		Check(skApres != nullptr && mcApres != nullptr, "les composants survivent au pas");
		if (skApres && mcApres) {
			const float tx = skApres->skinMatrices[0][3][0];
			std::printf("     t=%.3f s -> translation x de l'os 0 = %.4f (attendu ~5)\n", mcApres->currentTime, tx);

			// (3) L'EFFET OBSERVABLE : si Execute() est vide, tx reste a 0.
			Check(tx > 0.1f, "le systeme a REELLEMENT ecrit la pose (Execute() non vide)");
			Check(tx > 4.f && tx < 6.f, "l'interpolation tombe entre les deux images, pas sur l'une d'elles");
			Check(mcApres->currentTime > 0.f, "le temps de lecture a avance");
			Check(skApres->skinMatrices[1][3][0] == 0.f, "l'os 1, immobile dans la capture, n'a pas bouge");
		}

		// Fin de capture sans boucle : la lecture s'arrete d'elle-meme.
		for (int i = 0; i < 20; ++i)
			s2.Run(w2, 0.05f);
		const NkMotionCapture *fin = w2.Get<NkMotionCapture>(em);
		Check(fin && !fin->playing, "une capture non bouclee s'arrete a la fin (playing=false)");
		Check(fin && fin->currentTime <= 0.2001f, "le temps de lecture ne depasse pas la duree");
	}


	// -------------------------------------------------------------------------
	// NkRagdollSystem
	// -------------------------------------------------------------------------
	std::printf("-- NkRagdollSystem --\n");
	{
		static NkWorld w3;
		static NkScheduler s3;
		s3.AddSystem<NkRagdollSystem>(); // (2) APPELANT REEL

		// Le corps rigide : une entite ECS a x=7, que la physique aurait simulee.
		const ecs::NkEntityId corps = w3.CreateEntity();
		ecs::NkTransform tc;
		tc.localPosition = {7.f, 0.f, 0.f};
		w3.Add<ecs::NkTransform>(corps, tc);

		// Le personnage : squelette + ragdoll qui pointe le corps.
		const ecs::NkEntityId perso = w3.CreateEntity();
		ecs::NkSkeleton sk3 = MakeSkeleton(1);
		sk3.skinMatrices[0] = math::NkMat4f::Identity(); // le banc mesure des translations depuis 0
		w3.Add<ecs::NkSkeleton>(perso, sk3);

		NkRagdoll rd; // 5 152 o : tient sur la pile
		rd.boneCount = 1;
		rd.bones[0].skeletonBoneIdx = 0;
		rd.bones[0].rigidbodyEntity = corps;
		rd.bones[0].boneToBody = math::NkMat4f::Identity();
		rd.state = NkRagdoll::State::Animated;
		rd.blendWeight = 0.f;
		rd.blendSpeed = 2.f; // 0,5 s pour basculer entierement
		w3.Add<NkRagdoll>(perso, rd);

		Check(w3.Get<NkRagdoll>(perso) != nullptr, "temoin : le ragdoll existe dans le monde");
		Check(w3.Get<ecs::NkSkeleton>(perso)->skinMatrices[0][3][0] == 0.f, "temoin : l'os part a x=0");

		// ETAT ANIMATED : la physique ne doit RIEN ecrire.
		for (int i = 0; i < 5; ++i)
			s3.Run(w3, 1.f / 60.f);
		Check(w3.Get<ecs::NkSkeleton>(perso)->skinMatrices[0][3][0] == 0.f,
			  "etat Animated : l'animation garde la main, la physique n'ecrit rien");

		// BASCULE : on demande la transition.
		w3.Get<NkRagdoll>(perso)->state = NkRagdoll::State::Blending;

		// Un quart de seconde a blendSpeed=2 -> poids ~0,5 : l'os doit etre a
		// MI-CHEMIN entre sa pose d'animation (0) et le corps (7), donc ~3,5.
		// ⚠️ ON REMET LA POSE D'ANIMATION A CHAQUE PAS, ET C'EST INDISPENSABLE.
		// `BlendAnimRagdoll` melange la pose DEJA EN PLACE vers la pose physique.
		// Dans un vrai pipeline, le systeme d'animation ecrit les matrices juste
		// avant (priorite superieure) : la pose lue est donc la pose animee.
		// Sans ce rappel, le banc melangerait le resultat du pas precedent avec
		// lui-meme -- un filtre exponentiel qui converge vers le corps en
		// quelques images, et le poids de melange ne voudrait plus rien dire.
		//
		// 📌 C'est ce que la premiere version de ce banc mesurait sans le savoir :
		// a poids 0,500 elle lisait x=6,95 (le corps est a 7) au lieu de 3,5, et
		// l'assertion « x < 7 » etait assez lache pour laisser passer une
		// ATTENTE FAUSSE. Un banc vert dont on n'a pas verifie le CHIFFRE ne
		// prouve que l'absence de plantage.
		for (int i = 0; i < 15; ++i) {
			w3.Get<ecs::NkSkeleton>(perso)->skinMatrices[0] = math::NkMat4f::Identity();
			s3.Run(w3, 1.f / 60.f);
		}
		const float wMid = w3.Get<NkRagdoll>(perso)->blendWeight;
		const float xMid = w3.Get<ecs::NkSkeleton>(perso)->skinMatrices[0][3][0];
		std::printf("     en transition : poids=%.3f  x de l'os=%.4f\n", wMid, xMid);
		Check(wMid > 0.3f && wMid < 0.8f, "le poids de melange progresse (ni 0 ni 1)");
		// (3) L'EFFET OBSERVABLE : Execute() vide -> x reste a 0.
		Check(xMid > 0.5f, "le systeme a REELLEMENT ecrit la pose (Execute() non vide)");
		// Le CHIFFRE, pas seulement le signe : a poids w, l'os doit etre a w*7.
		// Une borne lache (x < 7) laissait passer 6,95 comme 3,5.
		const float attendu = wMid * 7.f;
		Check(xMid > attendu - 0.35f && xMid < attendu + 0.35f,
			  "en transition, la pose est PROPORTIONNELLE au poids de melange");

		// Fin de transition : l'etat bascule tout seul et l'os rejoint le corps.
		for (int i = 0; i < 60; ++i)
			s3.Run(w3, 1.f / 60.f);
		const NkRagdoll *fin3 = w3.Get<NkRagdoll>(perso);
		const float xFin = w3.Get<ecs::NkSkeleton>(perso)->skinMatrices[0][3][0];
		std::printf("     apres transition : etat=%d poids=%.3f  x de l'os=%.4f (corps a 7)\n", (int)fin3->state,
					fin3->blendWeight, xFin);
		Check(fin3->state == NkRagdoll::State::FullRagdoll, "la transition se termine SEULE en FullRagdoll");
		Check(fin3->blendWeight >= 0.999f, "le poids atteint 1 et s'y arrete");
		Check(xFin > 6.9f && xFin < 7.1f, "en ragdoll plein, l'os suit exactement le corps rigide");

		// Un os dont le corps rigide n'existe pas ne doit PAS etre invente.
		const ecs::NkEntityId perso2 = w3.CreateEntity();
		ecs::NkSkeleton sk4 = MakeSkeleton(1);
		sk4.skinMatrices[0] = math::NkMat4f::Identity();
		w3.Add<ecs::NkSkeleton>(perso2, sk4);
		NkRagdoll rd2;
		rd2.boneCount = 1;
		rd2.bones[0].skeletonBoneIdx = 0;
		rd2.bones[0].rigidbodyEntity = ecs::NkEntityId::Invalid(); // aucun corps
		rd2.state = NkRagdoll::State::FullRagdoll;
		rd2.blendWeight = 1.f;
		w3.Add<NkRagdoll>(perso2, rd2);
		for (int i = 0; i < 10; ++i)
			s3.Run(w3, 1.f / 60.f);
		Check(w3.Get<ecs::NkSkeleton>(perso2)->skinMatrices[0][3][0] == 0.f,
			  "un os sans corps rigide est laisse intact (aucune pose inventee)");
	}

	std::printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
