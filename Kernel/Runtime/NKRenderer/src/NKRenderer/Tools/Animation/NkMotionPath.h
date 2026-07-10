// =============================================================================
// NKRenderer/Tools/Animation/NkMotionPath.h
// -----------------------------------------------------------------------------
// ANIMATION PAR TRAÇAGE DE COURBE (NkAnima). On trace une COURBE dans la scène
// (points de contrôle) et une cible la suit dans le temps ; selon le mode, la cible
// pilote : le RIG ENTIER (root global), un SEUL OS, ou un EFFECTEUR IK (→ NkIKSystem
// fait suivre toute la chaîne d'os naturellement).
//
//   • NkMotionCurve  — spline Catmull-Rom (passe par les points de contrôle),
//                      échantillonnage position + tangente + reparamétrage par
//                      longueur d'arc (vitesse constante), ouverte ou fermée.
//   • NkPathFollow   — playhead : avance de `dt` à `speed`, renvoie la cible
//                      (position + direction) sur la courbe ; loop/ping-pong.
//
// Pure Foundation (NKMath + NKContainers) : AUCUN GPU, testable headless. Le rendu
// de la courbe (debug) et le solveur IK sont chez le consommateur (éditeur NkAnima).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkQuat.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace renderer {

		// Courbe lisse (Catmull-Rom) passant par une liste de points de contrôle.
		struct NkMotionCurve {
			public:
				NkVector<math::NkVec3f> points; // points de contrôle (tracés dans la scène)
				bool closed = false;			// true = boucle (dernier relié au premier)

				// Position sur la courbe. `t` ∈ [0,1] réparti UNIFORMÉMENT sur les segments
				// (pas la longueur d'arc — pour une vitesse constante, voir SampleByDistance).
				math::NkVec3f SamplePosition(float32 t) const;
				// Tangente normalisée (direction « avant ») au paramètre `t`.
				math::NkVec3f SampleTangent(float32 t) const;

				// Longueur approximative de la courbe (échantillonnage `samples` par segment).
				float32 Length(int32 samplesPerSeg = 24) const;

				// Position à la distance d'arc `dist` depuis le début (vitesse constante).
				// `dist` est clampée à [0, Length] (ou repliée si `closed`).
				math::NkVec3f SampleByDistance(float32 dist, int32 samplesPerSeg = 24) const;
				// Paramètre t correspondant à la distance d'arc `dist`.
				float32 DistanceToT(float32 dist, int32 samplesPerSeg = 24) const;

				static bool SelfTest();
		};

		// Comment la cible parcourt la courbe.
		enum class NkPathLoopMode {
			NK_ONCE,	 // s'arrête à la fin
			NK_LOOP,	 // recommence au début
			NK_PINGPONG	 // fait des allers-retours
		};

		// À quoi la cible est reliée (pour info / le consommateur agit en conséquence).
		enum class NkPathTargetMode {
			NK_GLOBAL_ROOT, // déplace le rig entier (translation du root)
			NK_SINGLE_BONE, // pilote la position d'un seul os
			NK_IK_EFFECTOR	// but d'un effecteur IK → NkIKSystem fait suivre la chaîne
		};

		// Playhead qui suit une NkMotionCurve dans le temps.
		struct NkPathFollow {
			public:
				NkMotionCurve curve;
				float32 speed = 1.0f; // unités/seconde (le long de l'arc)
				NkPathLoopMode loop = NkPathLoopMode::NK_LOOP;
				NkPathTargetMode target = NkPathTargetMode::NK_IK_EFFECTOR;

				// Orientation : up de référence pour aligner l'os sur la tangente (LookAt).
				math::NkVec3f up{0.f, 1.f, 0.f};
				bool orientToPath = true; // si false, rotation = identité

				// ÉCHELLE le long de la courbe : profil (x = t ∈ [0,1], y = facteur d'échelle uniforme),
				// échantillonné linéairement ; vide → échelle 1. Multiplié par baseScale.
				NkVector<math::NkVec2f> scaleProfile;
				math::NkVec3f baseScale{1.f, 1.f, 1.f};

				// Cible échantillonnée = TRANSFORM COMPLET (translation + ROTATION + ÉCHELLE).
				struct NkPathTarget {
						math::NkVec3f position{0.f, 0.f, 0.f};
						math::NkQuatf rotation{};			  // orientée sur la tangente (LookAt forward/up)
						math::NkVec3f scale{1.f, 1.f, 1.f};	  // échelle échantillonnée
						math::NkVec3f forward{0.f, 0.f, 1.f}; // tangente normalisée (pratique)
						bool finished = false;				  // true si NK_ONCE a atteint la fin
				};

				// Avance le playhead de `dt` secondes et renvoie la cible (T+R+S).
				NkPathTarget Advance(float32 dt);
				// Cible à une distance d'arc absolue (sans modifier l'état).
				NkPathTarget SampleAtDistance(float32 dist) const;
				void Reset();

				// Échelle uniforme au paramètre t (via scaleProfile).
				float32 SampleScale(float32 t) const;

				float32 CurrentDistance() const {
					return mDistance;
				}

			private:
				float32 mDistance = 0.f; // position d'arc courante
				int32 mDir = 1;			 // sens (pour ping-pong)
		};

	} // namespace renderer
} // namespace nkentseu
