//
// NkArSession.h
// =============================================================================
// Description :
//   La session AR : une image de caméra entre, des poses de marqueurs en
//   sortent, prêtes à être consommées comme n'importe quelle pose XR.
//   Elle tient ce que la détection seule ne peut pas tenir : la conversion en
//   niveaux de gris, la CONTINUITÉ d'un marqueur d'une image à l'autre, et le
//   lissage qui rend une pose regardable.
//
// Caractéristiques :
//   - Pas de dépendance à NKCamera : l'application fournit les octets. Une
//     webcam, un téléphone, un fichier ou une image de synthèse marchent donc
//     à l'identique — c'est ce qui a permis de tout prouver sans caméra.
//   - Le suivi donne à chaque marqueur une VIE : apparu, suivi, perdu. Sans
//     lui, un objet virtuel disparaîtrait à la première image ratée ; avec
//     lui, il tient quelques images le temps que la détection revienne.
//   - Lissage exponentiel sur la position ET l'orientation (slerp) : une pose
//     brute tremble, et le tremblement se voit bien plus qu'une petite erreur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKARSESSION_H__
#define __NKENTSEU_XR_NKARSESSION_H__

#include "NKXR/AR/NkArMarker.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		// ── Format des octets fournis par l'application ──────────────────────
		enum class NkArImageFormat : uint8 {
			NK_AR_GRAY8 = 0,  ///< Déjà en niveaux de gris : rien à convertir.
			NK_AR_RGBA8 = 1,
			NK_AR_BGRA8 = 2,
		};

		// ── Réglages de la session AR ────────────────────────────────────────
		struct NkArSessionConfig {
			NkArDetectorConfig detector;
			NkArCameraIntrinsics intrinsics;   ///< fx=0 → déduites du FOV ci-dessous
			float32 fallbackFovXDegrees = 60.f;///< champ supposé si non calibrée
			float32 markerSizeMeters = 0.10f;  ///< côté du marqueur imprimé
			// Combien d'images un marqueur survit sans être revu. 0 = il
			// disparaît dès la première image ratée (scintillement garanti).
			uint32 lostToleranceFrames = 8;
			// Lissage : 0 = pose brute (tremble), 1 = figée. 0,35 tient le
			// compromis entre stabilité et réactivité, mesuré à l'œil.
			float32 smoothing = 0.35f;
		};

		// ── Un marqueur SUIVI (pas seulement détecté) ────────────────────────
		struct NkArTrackedMarker {
			int32 id = -1;
			NkXrPose pose{};            ///< Lissée, dans le repère caméra.
			NkVec2f corners[4]{};       ///< Derniers coins vus, en pixels.
			bool visibleThisFrame = false;
			uint32 framesSinceSeen = 0;
			uint32 framesTracked = 0;   ///< Ancienneté : un marqueur jeune est moins sûr.
		};

		// ── La session ───────────────────────────────────────────────────────
		class NkArSession {
			public:
				bool Initialize(const NkArSessionConfig &config, uint32 imageWidth, uint32 imageHeight);
				void Shutdown();

				// Une image → des poses. Rend le nombre de marqueurs VISIBLES.
				uint32 ProcessFrame(const uint8 *pixels, uint32 width, uint32 height, uint32 stride,
									NkArImageFormat format);

				// Tous les marqueurs suivis, y compris ceux momentanément
				// perdus (framesSinceSeen > 0) : à l'application de décider si
				// elle les affiche encore — elle a le chiffre pour trancher.
				const NkVector<NkArTrackedMarker> &GetTracked() const { return mTracked; }
				const NkArTrackedMarker *Find(int32 id) const;

				// L'image en niveaux de gris de la dernière frame : utile pour
				// afficher ce que la détection a réellement vu (diagnostic).
				const uint8 *GetGray() const { return mGray.Size() ? &mGray[0] : nullptr; }
				uint32 GetGrayWidth() const { return mWidth; }
				uint32 GetGrayHeight() const { return mHeight; }

				NkArSessionConfig &Config() { return mConfig; }
				const NkArCameraIntrinsics &GetIntrinsics() const { return mIntrinsics; }

			private:
				NkArSessionConfig mConfig{};
				NkArCameraIntrinsics mIntrinsics{};
				NkVector<uint8> mGray;
				NkVector<NkArDetection> mDetections;
				NkVector<NkArTrackedMarker> mTracked;
				uint32 mWidth = 0;
				uint32 mHeight = 0;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKARSESSION_H__
