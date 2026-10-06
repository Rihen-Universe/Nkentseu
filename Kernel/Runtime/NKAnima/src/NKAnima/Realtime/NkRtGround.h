#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Realtime/NkRtGround.h — LE SOL, FOURNI PAR L'HOTE.
//
// NKAnima ne connait ni le monde, ni la physique, ni les maillages : c'est la
// condition pour que Noge, NKScena et PV3DE l'appellent sans le tirer vers leur
// moteur. L'IK des pieds et le ragdoll demandent donc le sol a l'HOTE, par un
// lancer de rayon : une fonction et un pointeur d'utilisateur. Noge peut y brancher
// NkPhysicsWorld::Raycast, une demo ses boites, un banc un escalier analytique.
//
// Sans lancer (raycast nul) : un plan horizontal a `planeY`, normal +Y.
// =============================================================================

#include "NKAnima/Realtime/NkRtMath.h"

namespace nkentseu {
	namespace anim {
		namespace rt {

			struct NkRtRayHit {
					NkVec3f point = {0.f, 0.f, 0.f};
					NkVec3f normal = {0.f, 1.f, 0.f};
					float32 distance = 0.f;
			};

			/// Le lancer de l'hote : depuis `origin`, le long de `dir` (unitaire), jusqu'a
			/// `maxDistance`. Rend vrai et remplit `hit` s'il touche.
			using NkRtRaycastFn = bool (*)(void *user, const NkVec3f &origin, const NkVec3f &dir, float32 maxDistance,
										   NkRtRayHit &hit);

			struct NkRtGround {
					NkRtRaycastFn raycast = nullptr;
					void *user = nullptr;
					float32 planeY = 0.f; ///< le plan de repli, sans lancer

					bool Cast(const NkVec3f &origin, const NkVec3f &dir, float32 maxDistance, NkRtRayHit &hit) const {
						if (raycast != nullptr)
							return raycast(user, origin, dir, maxDistance, hit);
						// Le plan y = planeY.
						if (std::fabs(dir.y) < 1e-6f)
							return false;
						const float32 t = (planeY - origin.y) / dir.y;
						if (t < 0.f || t > maxDistance)
							return false;
						hit.point = Madd(origin, dir, t);
						hit.normal = V3(0.f, 1.f, 0.f);
						hit.distance = t;
						return true;
					}
			};

		} // namespace rt
	} // namespace anim
} // namespace nkentseu
