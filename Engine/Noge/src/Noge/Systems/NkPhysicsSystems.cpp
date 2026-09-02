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
		if (jb.boneIndex >= skeleton.boneCount)
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

} // namespace nkentseu
