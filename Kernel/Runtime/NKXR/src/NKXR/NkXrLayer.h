//
// NkXrLayer.h
// =============================================================================
// Description :
//   Les couches de composition XR : ce que l'application SOUMET à EndFrame.
//   Une couche de projection (une vue par œil) pour la scène, des couches
//   quad (un rectangle posé dans le monde) pour les UI.
//
// Caractéristiques :
//   - La couche de projection reprend pose + FOV utilisés AU RENDU : le
//     compositeur d'un vrai casque s'en sert pour la reprojection (timewarp)
//     quand la tête a bougé entre le rendu et l'affichage. Les transmettre
//     dès l'étage 0 — même si le simulateur ne reprojette pas — garantit que
//     l'application les a sous la main le jour où ça compte.
//   - Pointeurs non possédants : les couches décrivent, la session consomme
//     pendant EndFrame, rien ne survit à l'appel.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRLAYER_H__
#define __NKENTSEU_XR_NKXRLAYER_H__

#include "NKXR/NkXrTypes.h"
#include "NKXR/NkXrPose.h"
#include "NKXR/NkXrSwapchain.h"

namespace nkentseu {
	namespace xr {

		// ── Une vue de la couche de projection (un œil) ──────────────────────
		struct NkXrProjectionLayerView {
			NkXrPose pose{};             ///< Pose de l'œil utilisée au rendu.
			NkXrFov fov{};               ///< FOV utilisé au rendu.
			NkXrSwapchain *swapchain = nullptr;
			uint32 imageIndex = 0;
		};

		// ── Couche de projection : la scène stéréo ───────────────────────────
		struct NkXrLayerProjection {
			// Espace dans lequel les poses des vues sont exprimées.
			NkXrSpaceType space = NkXrSpaceType::NK_XR_SPACE_STAGE;
			NkXrProjectionLayerView views[NK_XR_EYE_COUNT]{};
		};

		// ── Couche quad : un rectangle dans le monde ─────────────────────────
		struct NkXrLayerQuad {
			NkXrSpaceType space = NkXrSpaceType::NK_XR_SPACE_LOCAL;
			NkXrPose pose{};             ///< Centre du quad dans l'espace.
			NkVec2f sizeMeters{ 1.f, 1.f };
			NkXrSwapchain *swapchain = nullptr;
			uint32 imageIndex = 0;
		};

		// ── Ce qu'EndFrame reçoit ────────────────────────────────────────────
		struct NkXrFrameEndInfo {
			// Reprendre le predictedDisplayTime du WaitFrame de CETTE frame :
			// c'est lui qui date les poses soumises.
			NkXrTime displayTime = 0;
			const NkXrLayerProjection *projection = nullptr;
			const NkXrLayerQuad *quads = nullptr;
			uint32 quadCount = 0;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRLAYER_H__
