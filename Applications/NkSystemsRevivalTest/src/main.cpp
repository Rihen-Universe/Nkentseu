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

	float Norme(const math::NkVec3f &v) {
		return ::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	}

} // namespace

int main() {
	std::printf("=== NkSystemsRevivalTest : Noge/Systems tourne-t-il vraiment ? ===\n");

	// -------------------------------------------------------------------------
	// NkJiggleBoneSystem
	// -------------------------------------------------------------------------
	std::printf("-- NkJiggleBoneSystem --\n");

	NkWorld world;
	NkScheduler sched;

	// (2) APPELANT REEL : le systeme est enregistre dans l'ordonnanceur, avec
	// son Describe() (groupe PostUpdate, priorite 700). On ne l'appelle jamais
	// a la main : c'est `sched.Run()` qui le declenche.
	sched.AddSystem<NkJiggleBoneSystem>();

	// Une entite portant les trois composants que le systeme lit/ecrit.
	const ecs::NkEntityId e = world.CreateEntity();

	ecs::NkTransform tf;
	tf.localPosition = {0.f, 0.f, 0.f};
	world.Add<ecs::NkTransform>(e, tf);

	ecs::NkSkeleton sk;
	sk.boneCount = 4; // le systeme refuse un boneIndex hors squelette
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
		NkWorld w2;
		NkScheduler s2;
		s2.AddSystem<NkMocapSystem>(); // (2) APPELANT REEL

		const ecs::NkEntityId em = w2.CreateEntity();

		ecs::NkSkeleton sk2;
		sk2.boneCount = 2;
		sk2.skinMatrices[0] = math::NkMat4f::Identity();
		sk2.skinMatrices[1] = math::NkMat4f::Identity();
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

	std::printf("=== Resultat : %d OK / %d FAIL ===\n", gPass, gFail);
	return gFail == 0 ? 0 : 1;
}
