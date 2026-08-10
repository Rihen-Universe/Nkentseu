//
// NkXrTypes.h
// =============================================================================
// Description :
//   Types fondamentaux du runtime XR de Nkentseu : temps XR, yeux, champs de
//   vision asymétriques, vues par œil, état de frame, états de session.
//
// Caractéristiques :
//   - ZÉRO STL — repose sur NKMath/NKCore uniquement.
//   - Conventions spatiales : MAIN DROITE, l'avant regarde -Z, +Y vers le
//     haut, +X vers la droite (mêmes conventions qu'OpenXR ET que la matrice
//     de vue du moteur — NkMat4::LookAt regarde -Z). Attention : le +Z de
//     NkQuat::Forward() est l'ARRIÈRE d'une pose XR, d'où les helpers
//     NkXrForward/NkXrUp/NkXrRight qui fixent la convention une fois pour
//     toutes au lieu de la laisser se redécider à chaque appelant.
//   - Les angles de FOV sont en RADIANS signés (left/down négatifs), comme
//     XrFovf : c'est la seule représentation qui décrit un frustum décentré
//     sans perte, et elle rend la symétrie testable (left == -right).
//
// Algorithmes implémentés :
//   - Projection perspective ASYMÉTRIQUE depuis 4 demi-angles (l'équivalent
//     de xrCreateProjectionFov) — absente de NKMath, écrite ici pour ne pas
//     toucher un module partagé par tout le moteur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRTYPES_H__
#define __NKENTSEU_XR_NKXRTYPES_H__

#include "NKMath/NKMath.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace xr {

		using nkentseu::float32;
		using nkentseu::float64;
		using nkentseu::int64;
		using nkentseu::int32;
		using nkentseu::uint32;
		using nkentseu::uint64;
		using nkentseu::uint8;
		using nkentseu::math::NkVec2f;
		using nkentseu::math::NkVec3f;
		using nkentseu::math::NkVec4f;
		using nkentseu::math::NkMat4f;
		using nkentseu::math::NkQuatf;

		// ── Temps XR ─────────────────────────────────────────────────────────
		// Nanosecondes monotones, origine arbitraire (celle de NkChrono).
		// Entier signé et non flottant : les horodatages servent à des
		// SOUSTRACTIONS (prédiction) où l'erreur d'un float32 serait déjà
		// visible, et OpenXR (XrTime) impose de toute façon l'entier 64 bits —
		// autant que l'étage 2 n'ait aucune conversion à inventer.
		using NkXrTime = int64;

		// ── Yeux ─────────────────────────────────────────────────────────────
		enum class NkXrEye : uint8 {
			NK_XR_EYE_LEFT = 0,
			NK_XR_EYE_RIGHT = 1,
		};
		inline constexpr uint32 NK_XR_EYE_COUNT = 2u;

		// ── Types d'espace de référence ──────────────────────────────────────
		// Les trois espaces d'OpenXR, car ce sont les trois besoins réels :
		//   VIEW  = solidaire de la tête (HUD, réticule) ;
		//   LOCAL = origine à la pose initiale de la tête (expérience assise) ;
		//   STAGE = origine au SOL, au centre de la zone de jeu (roomscale).
		enum class NkXrSpaceType : uint8 {
			NK_XR_SPACE_VIEW = 0,
			NK_XR_SPACE_LOCAL = 1,
			NK_XR_SPACE_STAGE = 2,
		};

		// ── Cycle de vie de session ──────────────────────────────────────────
		// Machine d'états d'OpenXR, conservée telle quelle DÈS le simulateur :
		// c'est en la vivant à l'étage 0 que la démo sera déjà correcte quand
		// le backend Quest arrivera (une app qui ignore STOPPING est tuée par
		// le runtime du casque, autant l'apprendre sur desktop).
		enum class NkXrSessionState : uint8 {
			NK_XR_STATE_IDLE = 0,       ///< Créée, système pas encore prêt.
			NK_XR_STATE_READY = 1,      ///< Le système accepte Begin().
			NK_XR_STATE_SYNCHRONIZED = 2, ///< La boucle de frame est calée.
			NK_XR_STATE_VISIBLE = 3,    ///< Rendu affiché, entrées PAS routées.
			NK_XR_STATE_FOCUSED = 4,    ///< Affiché + entrées routées à l'app.
			NK_XR_STATE_STOPPING = 5,   ///< L'app DOIT appeler End().
			NK_XR_STATE_EXITING = 6,    ///< Fin définitive demandée.
		};

		// ── Événements de session ────────────────────────────────────────────
		enum class NkXrEventType : uint8 {
			NK_XR_EVENT_NONE = 0,
			NK_XR_EVENT_STATE_CHANGED = 1,
		};

		struct NkXrEvent {
			NkXrEventType type = NkXrEventType::NK_XR_EVENT_NONE;
			NkXrSessionState state = NkXrSessionState::NK_XR_STATE_IDLE;
			NkXrTime time = 0;
		};

		// ── Champ de vision asymétrique (radians signés) ─────────────────────
		// angleLeft/angleDown sont NÉGATIFS pour un frustum qui s'étend à
		// gauche/en bas — la convention XrFovf, gardée à l'identique pour que
		// l'étage 2 recopie les valeurs du runtime sans transformation.
		struct NkXrFov {
			float32 angleLeft = 0.f;
			float32 angleRight = 0.f;
			float32 angleUp = 0.f;
			float32 angleDown = 0.f;

			// Frustum symétrique depuis deux demi-angles positifs (radians).
			static constexpr NkXrFov Symmetric(float32 halfHorizontal, float32 halfVertical) noexcept {
				return NkXrFov{ -halfHorizontal, halfHorizontal, halfVertical, -halfVertical };
			}

			constexpr bool IsSymmetric() const noexcept {
				return angleLeft == -angleRight && angleDown == -angleUp;
			}
		};

		// ── Vue d'un œil : pose + FOV ────────────────────────────────────────
		// Ce que LocateViews rend pour chaque œil, l'équivalent de XrView.
		struct NkXrView {
			// Pose de l'œil DANS l'espace de référence demandé.
			NkVec3f position{ 0.f, 0.f, 0.f };
			NkQuatf orientation{};
			NkXrFov fov{};
		};

		// ── État de frame (WaitFrame) ────────────────────────────────────────
		struct NkXrFrameState {
			// Instant PRÉDIT d'affichage : c'est pour CET instant que les poses
			// doivent être demandées, pas pour « maintenant » — sinon l'image
			// est en retard d'une frame sur la tête (la nausée vient de là).
			NkXrTime predictedDisplayTime = 0;
			NkXrTime predictedDisplayPeriod = 0;
			// false = ne pas rendre (fenêtre réduite, casque posé) : soumettre
			// quand même EndFrame pour garder la boucle synchronisée.
			bool shouldRender = false;
		};

		// ── Configuration recommandée d'une vue ──────────────────────────────
		struct NkXrViewConfigView {
			uint32 recommendedWidth = 0;
			uint32 recommendedHeight = 0;
		};

		// ── Description du système XR ────────────────────────────────────────
		struct NkXrSystemInfo {
			// Nom lisible du « matériel » (simulateur : « NKXR Simulator »).
			const char *systemName = "";
			NkXrViewConfigView views[NK_XR_EYE_COUNT]{};
			float32 ipdMeters = 0.063f;
		};

		// ── Conventions directionnelles d'une orientation XR ─────────────────
		// L'avant XR est -Z ; NkQuat::Forward() du moteur rend +Z. Ces trois
		// fonctions sont l'UNIQUE endroit où cette réconciliation existe.
		NK_FORCE_INLINE NkVec3f NkXrForward(const NkQuatf &orientation) noexcept {
			return orientation * NkVec3f(0.f, 0.f, -1.f);
		}
		NK_FORCE_INLINE NkVec3f NkXrUp(const NkQuatf &orientation) noexcept {
			return orientation * NkVec3f(0.f, 1.f, 0.f);
		}
		NK_FORCE_INLINE NkVec3f NkXrRight(const NkQuatf &orientation) noexcept {
			return orientation * NkVec3f(1.f, 0.f, 0.f);
		}

		// ── Projection perspective asymétrique ───────────────────────────────
		// L'équivalent de xrCreateProjectionFov : matrice colonne-majeure,
		// main droite (w_clip = -z_vue), profondeur [-1,1] (GL) ou [0,1]
		// (Vulkan/DX) selon depthZeroToOne — le même interrupteur que
		// NkMat4::Perspective pour que les deux se substituent sans surprise.
		// Vit dans NKXR et pas dans NKMath : un HMD est le seul consommateur
		// de frustums décentrés aujourd'hui, et NKMath est partagé par tout.
		inline NkMat4f NkXrProjectionFromFov(const NkXrFov &fov, float32 nearPlane, float32 farPlane, bool depthZeroToOne) noexcept {
			const float32 tanLeft = math::NkTan(fov.angleLeft);
			const float32 tanRight = math::NkTan(fov.angleRight);
			const float32 tanUp = math::NkTan(fov.angleUp);
			const float32 tanDown = math::NkTan(fov.angleDown);
			const float32 tanWidth = tanRight - tanLeft;
			const float32 tanHeight = tanUp - tanDown;

			// NkMat4f() est la matrice NULLE (pas l'identité) : exactement la
			// base voulue ici, on ne pose que les 8 coefficients non nuls.
			NkMat4f result;
			result.mat[0][0] = 2.f / tanWidth;
			result.mat[1][1] = 2.f / tanHeight;
			// Le décentrage vit dans la 3e colonne : il se multiplie par z_vue
			// et déplace le centre du frustum sans cisailler l'image.
			result.mat[2][0] = (tanRight + tanLeft) / tanWidth;
			result.mat[2][1] = (tanUp + tanDown) / tanHeight;
			result.mat[2][3] = -1.f;
			if (depthZeroToOne) {
				result.mat[2][2] = farPlane / (nearPlane - farPlane);
				result.mat[3][2] = (farPlane * nearPlane) / (nearPlane - farPlane);
			}
			else {
				result.mat[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
				result.mat[3][2] = (2.f * farPlane * nearPlane) / (nearPlane - farPlane);
			}
			return result;
		}

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRTYPES_H__
