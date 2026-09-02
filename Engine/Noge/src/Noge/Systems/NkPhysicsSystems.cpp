// =============================================================================
// Noge/Systems/NkPhysicsSystems.cpp
// =============================================================================
// PREMIER CORPS DU MODULE `Noge/Systems` (2026-09-02).
//
// ⚠️ CE QUE CE FICHIER CORRIGE, ET POURQUOI IL EST ARRIVE SI TARD.
// Mesure du 2026-09-02 : `Noge/Systems/NkPhysicsSystems.h` declarait SIX
// systemes ECS dont le `Describe()` etait ECRIT ET COMPLET -- lectures,
// ecritures, groupe PostUpdate, priorites 400 a 700, nom -- pendant qu'AUCUN
// `Execute()` n'avait de corps. La moitie qui DECRIT etait faite, la moitie qui
// FAIT ne l'etait pas. L'editeur de liens ne disait rien parce que PERSONNE
// n'appelait ces systemes : un module sans corps et sans appelant ne casse
// rien, ne se voit pas, et se lit exactement comme une fonctionnalite livree.
//
// 📌 CE FICHIER N'IMPLEMENTE QU'UN SEUL DES SIX, DELIBEREMENT.
// Les six se separent en deux familles que le nom du fichier confondait :
//   - CPU pur          : NkJiggleBoneSystem, NkRagdollSystem, NkMocapSystem
//   - GPU compute      : NkClothSystem, NkHairSystem, NkSoftBodySystem
// Les trois derniers portent un `NkIDevice*` et des pipelines de calcul. Or
// **WebGL2 n'a PAS de compute** (mesure du meme jour : les requetes de capacites
// `GL_MAX_COMPUTE_*` echouent toutes sous Emscripten). Sur une cible des sept,
// ces trois-la ne pourront JAMAIS tourner tels quels : il leur faudra une
// doublure CPU, ce qui est une decision de conception, pas un corps a ecrire.
//
// On commence donc par le moins cher et le plus autonome. Le but n'est pas de
// finir le module aujourd'hui : c'est de faire passer `Systems` de « declare
// sans corps » a « existe et tourne » sur un cas, avec un banc, pour que le
// reste se juge sur un patron eprouve plutot que sur une intention.
// =============================================================================
#include "NkPhysicsSystems.h"

namespace nkentseu {

	// -------------------------------------------------------------------------
	// NkJiggleBoneSystem — os secondaires avec inertie
	// -------------------------------------------------------------------------
	// Modele : ressort amorti (spring-damper) qui tire la position courante du
	// bout de l'os vers sa position d'animation, avec inertie et gravite. C'est
	// le modele decrit par les champs du composant (stiffness / damping / mass /
	// gravity / maxAngleDeg) -- on n'invente rien, on execute ce que la
	// specification portait deja.
	// -------------------------------------------------------------------------

	NkQuatf NkJiggleBoneSystem::ClampAngle(const NkQuatf &q, float32 maxDeg) const noexcept {
		// L'angle d'un quaternion unitaire vaut 2*acos(|w|). On borne la
		// deviation en ramenant le quaternion vers l'identite.
		NkQuatf n = q.Normalized();
		float32 w = n.w < 0.f ? -n.w : n.w; // |w| : meme rotation, chemin court
		if (w > 1.f)
			w = 1.f;
		const float32 angle = 2.f * ::acosf(w);
		const float32 maxRad = maxDeg * 3.14159265358979323846f / 180.f;
		if (maxRad <= 0.f)
			return NkQuatf::Identity();
		if (angle <= maxRad)
			return n;
		// Facteur d'interpolation vers l'identite : on garde la direction, on
		// coupe l'amplitude.
		const float32 t = 1.f - (maxRad / angle);
		return NkQuatf::Identity().Mix(n, 1.f - t).Normalized();
	}

	void NkJiggleBoneSystem::UpdateJiggle(NkJiggleBone &jb, NkSkeleton &skeleton, const NkTransform &tf,
										  float32 dt) noexcept {
		if (!jb.enabled || dt <= 0.f)
			return;
		if (jb.boneIndex >= skeleton.BoneCount())
			return; // indice hors squelette : on ne touche a rien

		// La cible est la pose d'animation, exprimee dans le repere de l'entite.
		// `targetPos` est renseignee par l'etage d'animation ; si personne ne
		// l'a posee, elle vaut l'origine et le ressort ramene simplement l'os
		// a sa pose de repos -- comportement neutre, jamais un saut.
		const NkVec3f target = jb.targetPos + tf.localPosition;

		// ── Ressort amorti, integration semi-implicite (stable) ──────────────
		const float32 mass = (jb.mass > 0.0001f) ? jb.mass : 1.f;
		const NkVec3f ecart = target - jb.currentPos;

		// Rappel proportionnel a l'ecart, amortissement proportionnel a la
		// vitesse. stiffness et damping sont normalises [0..1] par le composant :
		// on les remet a une echelle utilisable par seconde.
		const float32 k = jb.stiffness * 60.f;
		const float32 c = jb.damping * 10.f;

		NkVec3f accel = ecart * (k / mass);
		accel = accel - jb.velocity * c;
		accel.y -= jb.gravity;

		// Semi-implicite : la vitesse d'abord, la position avec la NOUVELLE
		// vitesse. Plus stable qu'Euler explicite pour un ressort raide, et
		// c'est ce qui evite que l'os parte en oscillation au premier pas long.
		jb.velocity = jb.velocity + accel * dt;
		jb.currentPos = jb.currentPos + jb.velocity * dt;

		// ── Contraintes d'axe : un axe non contraint ne bouge pas ────────────
		if (!jb.constrainX) {
			jb.currentPos.x = target.x;
			jb.velocity.x = 0.f;
		}
		if (!jb.constrainY) {
			jb.currentPos.y = target.y;
			jb.velocity.y = 0.f;
		}
		if (!jb.constrainZ) {
			jb.currentPos.z = target.z;
			jb.velocity.z = 0.f;
		}

		// ── Contrainte d'angle ───────────────────────────────────────────────
		jb.currentRot = ClampAngle(jb.currentRot, jb.maxAngleDeg);
	}

	void NkJiggleBoneSystem::Execute(NkWorld &world, float32 dt) noexcept {
		world.Query<NkJiggleBone, NkSkeleton, NkTransform>().ForEach(
			[&](NkEntityId, NkJiggleBone &jb, NkSkeleton &sk, NkTransform &tf) { UpdateJiggle(jb, sk, tf, dt); });
	}


	// -------------------------------------------------------------------------
	// NkMocapSystem — relecture d'une capture de mouvement
	// -------------------------------------------------------------------------
	// Deuxieme corps du module. CPU pur : aucune dependance GPU, donc il tourne
	// sur les sept cibles sans doublure — contrairement aux trois systemes de
	// calcul (cloth / hair / softbody) dont WebGL2 ne veut pas.
	// -------------------------------------------------------------------------

	NkMat4f NkMocapSystem::InterpolateFrames(const NkMocapFrame &a, const NkMocapFrame &b, float32 t,
											 uint32 boneIdx) const noexcept {
		// ⚠️ INTERPOLATION LINEAIRE PAR ELEMENT, ET C'EST UN CHOIX ASSUME.
		// L'interpolation juste d'une pose passe par une decomposition
		// translation/rotation/echelle puis un slerp sur la rotation. Ici on
		// melange les 16 elements. Entre deux images ADJACENTES d'une capture a
		// 30 Hz ou plus, l'ecart angulaire est petit et la difference est
		// invisible ; sur un grand ecart, la matrice se contracte legerement.
		// C'est donc valable pour de la RELECTURE, pas pour du reciblage ni pour
		// un ralenti extreme. Ecrit ici pour que personne n'ait a le redecouvrir
		// sur une animation qui « fond » au ralenti.
		const bool aOk = boneIdx < (uint32)a.boneTransforms.Size();
		const bool bOk = boneIdx < (uint32)b.boneTransforms.Size();
		if (!aOk && !bOk)
			return NkMat4f::Identity();
		if (!aOk)
			return b.boneTransforms[(decltype(b.boneTransforms)::SizeType)boneIdx];
		if (!bOk)
			return a.boneTransforms[(decltype(a.boneTransforms)::SizeType)boneIdx];

		const NkMat4f &ma = a.boneTransforms[(decltype(a.boneTransforms)::SizeType)boneIdx];
		const NkMat4f &mb = b.boneTransforms[(decltype(b.boneTransforms)::SizeType)boneIdx];
		NkMat4f out;
		for (int c = 0; c < 4; ++c)
			for (int r = 0; r < 4; ++r)
				out[c][r] = ma[c][r] * (1.f - t) + mb[c][r] * t;
		return out;
	}

	void NkMocapSystem::PlaybackMocap(NkMotionCapture &mc, NkSkeleton &sk, float32 dt) noexcept {
		if (!mc.playing || mc.frames.Empty() || dt <= 0.f)
			return;

		// Duree : celle declaree, sinon celle deduite des images.
		float32 duree = mc.duration;
		if (duree <= 0.f) {
			const float32 fps = (mc.fps > 0.001f) ? mc.fps : 30.f;
			duree = (float32)mc.frames.Size() / fps;
		}
		if (duree <= 0.f)
			return;

		mc.currentTime += dt * mc.speed;

		if (mc.currentTime >= duree) {
			if (mc.loop) {
				// Modulo par soustraction : une capture longue avec un pas court
				// ne boucle jamais plus d'une fois, la boucle est donc bornee.
				while (mc.currentTime >= duree)
					mc.currentTime -= duree;
			} else {
				mc.currentTime = duree;
				mc.playing = false; // arrivee en fin : on s'arrete, on ne rejoue pas
			}
		}
		if (mc.currentTime < 0.f)
			mc.currentTime = 0.f;

		// Encadrement de l'instant courant par deux images.
		const uint32 n = (uint32)mc.frames.Size();
		const float32 fps = (mc.fps > 0.001f) ? mc.fps : 30.f;
		float32 pos = mc.currentTime * fps;
		uint32 i0 = (uint32)pos;
		if (i0 >= n)
			i0 = n - 1;
		uint32 i1 = (i0 + 1 < n) ? (i0 + 1) : i0;
		float32 t = pos - (float32)i0;
		if (t < 0.f)
			t = 0.f;
		if (t > 1.f)
			t = 1.f;

		const NkMocapFrame &f0 = mc.frames[(decltype(mc.frames)::SizeType)i0];
		const NkMocapFrame &f1 = mc.frames[(decltype(mc.frames)::SizeType)i1];

		const uint32 bones = sk.BoneCount();
		for (uint32 b = 0; b < bones; ++b) {
			const NkMat4f m = InterpolateFrames(f0, f1, t, b);
			// blendWeight : melange avec la pose deja en place (animation
			// normale). A 1 la capture gagne entierement, a 0 elle n'ecrit rien.
			if (mc.blendWeight >= 0.999f) {
				sk.skinMatrices[b] = m;
			} else if (mc.blendWeight > 0.001f) {
				const NkMat4f &prev = sk.skinMatrices[b];
				NkMat4f mix;
				for (int c = 0; c < 4; ++c)
					for (int r = 0; r < 4; ++r)
						mix[c][r] = prev[c][r] * (1.f - mc.blendWeight) + m[c][r] * mc.blendWeight;
				sk.skinMatrices[b] = mix;
			}
		}
	}

	void NkMocapSystem::Execute(NkWorld &world, float32 dt) noexcept {
		world.Query<NkMotionCapture, NkSkeleton>().ForEach(
			[&](NkEntityId, NkMotionCapture &mc, NkSkeleton &sk) { PlaybackMocap(mc, sk, dt); });
	}


	// -------------------------------------------------------------------------
	// NkRagdollSystem — bascule animation <-> physique
	// -------------------------------------------------------------------------
	// Troisieme corps du module, et le dernier des trois CPU purs.
	//
	// Le systeme ne SIMULE pas : la simulation appartient a NKPhysics. Il fait le
	// PONT — il lit la pose des corps rigides deja simules (chaque os pointe une
	// entite ECS par `rigidbodyEntity`) et l'ecrit dans les matrices de peau, en
	// dosant par `blendWeight`. Aucune dependance GPU, aucune dependance directe
	// a NKPhysics : tout passe par le monde ECS.
	// -------------------------------------------------------------------------

	void NkRagdollSystem::TransitionToRagdoll(NkRagdoll &rd, NkSkeleton &sk, NkWorld &world, float32 dt) noexcept {
		(void)sk;
		(void)world;
		// Machine a etats du poids de melange. C'est la SEULE chose qui avance
		// le temps ici : le reste ne fait que lire l'etat.
		switch (rd.state) {
			case NkRagdoll::State::Animated:
				rd.blendWeight = 0.f;
				break;
			case NkRagdoll::State::Blending:
				rd.blendWeight += rd.blendSpeed * dt;
				if (rd.blendWeight >= 1.f) {
					rd.blendWeight = 1.f;
					rd.state = NkRagdoll::State::FullRagdoll; // la transition se termine seule
				}
				break;
			case NkRagdoll::State::FullRagdoll:
				rd.blendWeight = 1.f;
				break;
			case NkRagdoll::State::Kinematic:
				// Ragdoll actif : le squelette PILOTE les corps, il ne les subit
				// pas. Le poids reste a 0 cote lecture.
				rd.blendWeight = 0.f;
				break;
		}
		if (rd.blendWeight < 0.f)
			rd.blendWeight = 0.f;
	}

	void NkRagdollSystem::ApplyRagdollToSkeleton(NkRagdoll &rd, NkSkeleton &sk, NkWorld &world) noexcept {
		// Pose PLEINE : la physique gagne entierement.
		const uint32 n = (rd.boneCount < NkRagdoll::kMaxBones) ? rd.boneCount : NkRagdoll::kMaxBones;
		for (uint32 i = 0; i < n; ++i) {
			const NkRagdollBoneLink &lien = rd.bones[i];
			if (lien.skeletonBoneIdx >= sk.BoneCount())
				continue;
			const NkTransform *tfCorps = world.Get<NkTransform>(lien.rigidbodyEntity);
			if (!tfCorps)
				continue; // corps absent : on ne touche PAS l'os (jamais d'invention de pose)
			sk.skinMatrices[lien.skeletonBoneIdx] = tfCorps->ComputeLocalMatrix() * lien.boneToBody;
		}
	}

	void NkRagdollSystem::BlendAnimRagdoll(NkRagdoll &rd, NkSkeleton &sk, NkWorld &world, float32 dt) noexcept {
		(void)dt;
		const uint32 n = (rd.boneCount < NkRagdoll::kMaxBones) ? rd.boneCount : NkRagdoll::kMaxBones;
		const float32 w = rd.blendWeight;
		for (uint32 i = 0; i < n; ++i) {
			const NkRagdollBoneLink &lien = rd.bones[i];
			if (lien.skeletonBoneIdx >= sk.BoneCount())
				continue;
			const NkTransform *tfCorps = world.Get<NkTransform>(lien.rigidbodyEntity);
			if (!tfCorps)
				continue;
			const NkMat4f cible = tfCorps->ComputeLocalMatrix() * lien.boneToBody;
			NkMat4f &dst = sk.skinMatrices[lien.skeletonBoneIdx];
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
					dst[c][r] = dst[c][r] * (1.f - w) + cible[c][r] * w;
		}
	}

	void NkRagdollSystem::Execute(NkWorld &world, float32 dt) noexcept {
		world.Query<NkRagdoll, NkSkeleton>().ForEach([&](NkEntityId, NkRagdoll &rd, NkSkeleton &sk) {
			TransitionToRagdoll(rd, sk, world, dt);
			if (rd.state == NkRagdoll::State::FullRagdoll) {
				ApplyRagdollToSkeleton(rd, sk, world);
			} else if (rd.blendWeight > 0.001f) {
				BlendAnimRagdoll(rd, sk, world, dt);
			}
			// Animated / Kinematic a poids nul : on n'ecrit RIEN, l'animation
			// garde la main. Ecrire un melange a poids nul serait un travail
			// inutile ET un ecrasement de la pose par elle-meme.
		});
	}

} // namespace nkentseu
