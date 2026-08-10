//
// NkXrPose.h
// =============================================================================
// Description :
//   La pose XR : position + orientation (quaternion unitaire), horodatée, et
//   son outillage — matrices monde/vue, composition, vitesses, extrapolation.
//
// Caractéristiques :
//   - Une pose LOCALISE une entité dans un espace de base :
//     p_espace = orientation * p_entité + position (rotation PUIS translation,
//     la convention XrPosef). La matrice de VUE est donc l'INVERSE de la pose
//     de l'œil — écrite ici une seule fois pour que personne ne la re-dérive.
//   - Prédiction par extrapolation à vitesses constantes (linéaire +
//     angulaire). C'est le modèle honnête d'un étage 0 : les vrais runtimes
//     filtrent l'IMU en plus, mais l'API — « donne-moi la pose à l'instant
//     T futur » — est déjà la bonne, et c'est elle qui structure l'appelant.
//
// Algorithmes implémentés :
//   - Vitesse angulaire depuis deux quaternions horodatés (log du delta,
//     via axe-angle) et intégration inverse (exp) pour extrapoler.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRPOSE_H__
#define __NKENTSEU_XR_NKXRPOSE_H__

#include "NKXR/NkXrTypes.h"

namespace nkentseu {
	namespace xr {

		// ── Pose : position + orientation, dans un espace de base ────────────
		struct NkXrPose {
			NkVec3f position{ 0.f, 0.f, 0.f };
			NkQuatf orientation{};

			static NkXrPose Identity() noexcept {
				return NkXrPose{};
			}

			// Matrice MONDE de la pose (place l'entité dans l'espace de base).
			NkMat4f ToMat4() const noexcept {
				return NkMat4f::Translation(position) * orientation.ToMat4();
			}

			// Matrice de VUE si cette pose est celle d'un œil : l'inverse
			// analytique (conjugué + translation opposée) plutôt que
			// NkMat4::Inverse() — exacte, et sans le garde-fou « singulière »
			// qui rendrait silencieusement l'identité en cas de bug amont.
			NkMat4f ToViewMatrix() const noexcept {
				return orientation.Conjugate().ToMat4() * NkMat4f::Translation(-position);
			}

			// Applique la pose à un point de l'entité (rotation PUIS translation).
			NkVec3f Transform(const NkVec3f &point) const noexcept {
				return orientation * point + position;
			}

			// Composition : le résultat localise « other » exprimée dans CETTE
			// pose, dans l'espace de base de celle-ci (this ∘ other).
			NkXrPose operator*(const NkXrPose &other) const noexcept {
				NkXrPose result;
				result.position = Transform(other.position);
				result.orientation = (orientation * other.orientation).Normalized();
				return result;
			}

			// Pose inverse : exprime l'espace de base dans l'espace de l'entité.
			NkXrPose Inverted() const noexcept {
				NkXrPose result;
				result.orientation = orientation.Conjugate();
				result.orientation = result.orientation.Normalized();
				result.position = result.orientation * (position * -1.f);
				return result;
			}
		};

		// ── Vitesses d'une pose ──────────────────────────────────────────────
		struct NkXrVelocity {
			NkVec3f linear{ 0.f, 0.f, 0.f };  ///< m/s dans l'espace de base.
			NkVec3f angular{ 0.f, 0.f, 0.f }; ///< rad/s, axe*vitesse, espace de base.
		};

		// Vitesse angulaire moyenne entre deux orientations séparées de dt
		// secondes : axe-angle du delta (to * from^-1, delta dans l'espace de
		// BASE — cohérent avec l'extrapolation ci-dessous), divisé par dt.
		inline NkVec3f NkXrAngularVelocity(const NkQuatf &from, const NkQuatf &to, float32 dtSeconds) noexcept {
			if (dtSeconds <= 0.f) {
				return NkVec3f(0.f, 0.f, 0.f);
			}
			NkQuatf delta = (to * from.Conjugate()).Normalized();
			// Les deux hémisphères du quaternion codent la même rotation ; on
			// force le chemin court, sinon un delta minuscule peut se lire
			// comme un tour presque complet dans l'autre sens.
			if (delta.w < 0.f) {
				delta = NkQuatf(-delta.x, -delta.y, -delta.z, -delta.w);
			}
			const float32 angleRad = delta.Angle().Rad();
			if (angleRad <= 1e-6f) {
				return NkVec3f(0.f, 0.f, 0.f);
			}
			return delta.Axis() * (angleRad / dtSeconds);
		}

		// Extrapole une pose de dt secondes avec des vitesses constantes.
		inline NkXrPose NkXrExtrapolate(const NkXrPose &pose, const NkXrVelocity &velocity, float32 dtSeconds) noexcept {
			NkXrPose result = pose;
			result.position = pose.position + velocity.linear * dtSeconds;
			const float32 speedRad = velocity.angular.Len();
			if (speedRad > 1e-6f) {
				const NkVec3f axis = velocity.angular * (1.f / speedRad);
				// Delta en espace de BASE : il se compose à GAUCHE de
				// l'orientation courante (même convention que la mesure).
				const NkQuatf delta(math::NkAngle::FromRad(speedRad * dtSeconds), axis);
				result.orientation = (delta * pose.orientation).Normalized();
			}
			return result;
		}

		// ── Pose horodatée : l'unité de tracking ─────────────────────────────
		struct NkXrTimedPose {
			NkXrPose pose{};
			NkXrVelocity velocity{};
			NkXrTime time = 0;

			// Pose prédite à l'instant demandé. La fenêtre d'extrapolation est
			// bornée : au-delà de 100 ms le modèle « vitesses constantes » ment
			// plus qu'il n'aide (et OpenXR borne pareil côté runtimes).
			NkXrPose PredictAt(NkXrTime when) const noexcept {
				float32 dt = float32(float64(when - time) * 1e-9);
				dt = math::NkClamp(dt, -0.1f, 0.1f);
				return NkXrExtrapolate(pose, velocity, dt);
			}
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRPOSE_H__
