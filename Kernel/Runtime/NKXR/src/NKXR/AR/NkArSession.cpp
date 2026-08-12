//
// NkArSession.cpp
// =============================================================================
// Description :
//   Conversion en gris, détection, appariement par identifiant, lissage, et
//   gestion de la vie d'un marqueur (apparu / suivi / perdu).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#include "NKXR/AR/NkArSession.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace xr {

		namespace {

			// Luminance perçue (Rec. 601) : le vert domine parce que l'œil y
			// est le plus sensible. Une moyenne naïve écraserait le contraste
			// d'un marqueur imprimé sur fond coloré.
			inline uint8 Luminance(uint8 r, uint8 g, uint8 b) {
				return uint8((uint32(r) * 77u + uint32(g) * 151u + uint32(b) * 28u) >> 8);
			}

		} // namespace

		bool NkArSession::Initialize(const NkArSessionConfig &config, uint32 imageWidth, uint32 imageHeight) {
			if (imageWidth < 16u || imageHeight < 16u) {
				return false;
			}
			mConfig = config;
			mWidth = imageWidth;
			mHeight = imageHeight;
			mGray.Resize(imageWidth * imageHeight);
			mMask.Resize(imageWidth * imageHeight);
			mTracked.Clear();
			mIntrinsics = config.intrinsics;
			if (mIntrinsics.fx <= 0.f) {
				// Non calibrée : on suppose un champ. Approximatif et ASSUMÉ —
				// les distances seront justes à ~10 % près, l'orientation bien
				// mieux. Une calibration au damier viendra.
				mIntrinsics = NkArCameraIntrinsics::FromFovX(imageWidth, imageHeight, config.fallbackFovXDegrees);
				logger.Infof("[NkAr] Caméra non calibrée : intrinsèques supposées depuis un champ de %.0f° "
							 "(fx=%.1f) — distances approximatives.\n",
							 config.fallbackFovXDegrees, mIntrinsics.fx);
			}
			return true;
		}

		void NkArSession::Shutdown() {
			mGray.Clear();
			mMask.Clear();
			mDetections.Clear();
			mTracked.Clear();
			mWidth = 0;
			mHeight = 0;
		}

		const NkArTrackedMarker *NkArSession::Find(int32 id) const {
			for (nk_size i = 0; i < mTracked.Size(); ++i) {
				if (mTracked[i].id == id) {
					return &mTracked[i];
				}
			}
			return nullptr;
		}

		bool NkArSession::Forget(int32 id) {
			for (nk_size i = 0; i < mTracked.Size(); ++i) {
				if (mTracked[i].id == id) {
					mTracked[i] = mTracked[mTracked.Size() - 1u];
					mTracked.PopBack();
					return true;
				}
			}
			return false;
		}

		void NkArSession::ForgetAll() {
			mTracked.Clear();
		}

		uint32 NkArSession::ProcessFrame(const uint8 *pixels, uint32 width, uint32 height, uint32 stride,
										 NkArImageFormat format) {
			if (pixels == nullptr || width != mWidth || height != mHeight) {
				return 0;
			}
			if (stride == 0u) {
				stride = width * ((format == NkArImageFormat::NK_AR_GRAY8) ? 1u : 4u);
			}

			// ── Conversion en niveaux de gris ────────────────────────────────
			for (uint32 y = 0; y < height; ++y) {
				const uint8 *row = pixels + nk_size(y) * stride;
				uint8 *dst = &mGray[nk_size(y) * width];
				switch (format) {
					case NkArImageFormat::NK_AR_GRAY8: {
						for (uint32 x = 0; x < width; ++x) {
							dst[x] = row[x];
						}
						break;
					}
					case NkArImageFormat::NK_AR_RGBA8: {
						for (uint32 x = 0; x < width; ++x) {
							dst[x] = Luminance(row[x * 4u + 0u], row[x * 4u + 1u], row[x * 4u + 2u]);
						}
						break;
					}
					case NkArImageFormat::NK_AR_BGRA8: {
						for (uint32 x = 0; x < width; ++x) {
							dst[x] = Luminance(row[x * 4u + 2u], row[x * 4u + 1u], row[x * 4u + 0u]);
						}
						break;
					}
				}
			}

			NkArDetectMarkers(&mGray[0], width, height, mConfig.detector, mDetections, &mMask[0]);

			// ── Vieillissement : tout le monde est présumé absent ────────────
			for (nk_size i = 0; i < mTracked.Size(); ++i) {
				mTracked[i].visibleThisFrame = false;
				++mTracked[i].framesSinceSeen;
			}

			uint32 visible = 0;
			for (nk_size d = 0; d < mDetections.Size(); ++d) {
				const NkArDetection &detection = mDetections[d];
				NkXrPose pose;
				if (!NkArPoseFromDetection(detection, mConfig.markerSizeMeters, mIntrinsics, pose)) {
					continue;
				}
				++visible;

				// Appariement par IDENTIFIANT : c'est ce que le marqueur porte
				// en propre, donc l'appariement le plus sûr qui soit — pas
				// besoin de proximité ni de prédiction.
				NkArTrackedMarker *tracked = nullptr;
				for (nk_size i = 0; i < mTracked.Size(); ++i) {
					if (mTracked[i].id == detection.id) {
						tracked = &mTracked[i];
						break;
					}
				}
				if (tracked == nullptr) {
					NkArTrackedMarker fresh;
					fresh.id = detection.id;
					fresh.pose = pose; // première vue : pas de lissage, sinon il « arrive » en glissant
					mTracked.PushBack(fresh);
					tracked = &mTracked[mTracked.Size() - 1u];
				}
				else {
					const float32 alpha = math::NkClamp(mConfig.smoothing, 0.f, 0.99f);
					if (tracked->framesSinceSeen > 1u) {
						// Revenu après une absence : reprendre la pose brute.
						// Lisser depuis une pose vieille ferait glisser l'objet
						// depuis son ancienne place, ce qui se voit énormément.
						tracked->pose = pose;
					}
					else {
						tracked->pose.position = tracked->pose.position * alpha + pose.position * (1.f - alpha);
						// Slerp et non interpolation composante par composante :
						// sur les quaternions, la seconde raccourcit les
						// rotations et fait « accélérer » l'objet au passage.
						tracked->pose.orientation = tracked->pose.orientation.SLerp(pose.orientation, 1.f - alpha);
						tracked->pose.orientation = tracked->pose.orientation.Normalized();
					}
				}
				for (uint32 c = 0; c < 4u; ++c) {
					tracked->corners[c] = detection.corners[c];
				}
				tracked->visibleThisFrame = true;
				tracked->framesSinceSeen = 0;
				++tracked->framesTracked;
			}

			// ── Oubli des marqueurs trop longtemps absents ───────────────────
			// En mode ANCRÉ, on n'oublie JAMAIS de soi-même : c'est
			// l'application qui décide, par Forget(). Une scène posée ne doit
			// pas s'évaporer parce que la carte a été rangée.
			if (mConfig.anchorMode == NkArAnchorMode::NK_AR_ANCHOR_PERSISTENT) {
				return visible;
			}
			for (nk_size i = mTracked.Size(); i > 0; --i) {
				const nk_size index = i - 1u;
				if (mTracked[index].framesSinceSeen > mConfig.lostToleranceFrames) {
					mTracked[index] = mTracked[mTracked.Size() - 1u];
					mTracked.PopBack();
				}
			}
			return visible;
		}

	} // namespace xr
} // namespace nkentseu
