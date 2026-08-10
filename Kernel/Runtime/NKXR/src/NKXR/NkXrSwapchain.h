//
// NkXrSwapchain.h
// =============================================================================
// Description :
//   La swapchain XR : la file d'images d'UN œil, avec la discipline
//   Acquire → rendre → Release → soumettre en couche.
//
// Caractéristiques :
//   - À l'étage 0 (simulateur), les images GPU appartiennent à l'application
//     (cibles offscreen NKRenderer) : la swapchain ne porte que des handles
//     OPAQUES (uint64) posés par l'app. Ce n'est pas un trou dans le design,
//     c'est le design : à l'étage 2 c'est le runtime OpenXR qui FOURNIT les
//     images, et ce qui doit être en place côté application d'ici là, c'est
//     précisément la discipline d'acquisition — elle, est déjà réelle ici
//     (double Acquire refusé, Release exigé avant soumission).
//   - Zéro allocation : tableau d'images à taille fixe. Quatre suffisent, les
//     runtimes réels en donnent 2 ou 3.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKXRSWAPCHAIN_H__
#define __NKENTSEU_XR_NKXRSWAPCHAIN_H__

#include "NKXR/NkXrTypes.h"

namespace nkentseu {
	namespace xr {

		struct NkXrSwapchainDesc {
			uint32 width = 0;
			uint32 height = 0;
			// 1 image = rendu direct dans une cible unique (le cas simulateur :
			// le compositeur lit la cible après le rendu, pas en parallèle).
			uint32 imageCount = 1;
			NkXrEye eye = NkXrEye::NK_XR_EYE_LEFT;
			const char *name = "";
		};

		class NkXrSwapchain {
			public:
				static constexpr uint32 kMaxImages = 4u;

				bool Init(const NkXrSwapchainDesc &desc) noexcept {
					if (desc.width == 0u || desc.height == 0u) {
						return false;
					}
					if (desc.imageCount == 0u || desc.imageCount > kMaxImages) {
						return false;
					}
					mDesc = desc;
					mNextImage = 0u;
					mAcquired = false;
					for (uint32 i = 0u; i < kMaxImages; ++i) {
						mNativeImages[i] = 0u;
					}
					return true;
				}

				uint32 GetWidth() const noexcept { return mDesc.width; }
				uint32 GetHeight() const noexcept { return mDesc.height; }
				uint32 GetImageCount() const noexcept { return mDesc.imageCount; }
				NkXrEye GetEye() const noexcept { return mDesc.eye; }

				// L'application enregistre SES images GPU (handle opaque —
				// NkTexHandle, id RHI, peu importe : la swapchain ne les
				// interprète pas, elle les restitue à la composition).
				void SetImageNative(uint32 index, uint64 native) noexcept {
					if (index < mDesc.imageCount) {
						mNativeImages[index] = native;
					}
				}

				uint64 GetImageNative(uint32 index) const noexcept {
					return (index < mDesc.imageCount) ? mNativeImages[index] : 0u;
				}

				// Refuse le double-Acquire : c'est l'erreur que les runtimes
				// réels sanctionnent, autant qu'elle soit impossible dès ici.
				bool AcquireImage(uint32 &outIndex) noexcept {
					if (mAcquired) {
						return false;
					}
					outIndex = mNextImage;
					mAcquired = true;
					return true;
				}

				bool ReleaseImage() noexcept {
					if (!mAcquired) {
						return false;
					}
					mNextImage = (mNextImage + 1u) % mDesc.imageCount;
					mAcquired = false;
					return true;
				}

				bool IsAcquired() const noexcept { return mAcquired; }

				// Index de la DERNIÈRE image relâchée : celle qu'une couche
				// peut légitimement référencer à EndFrame.
				uint32 GetLastReleasedIndex() const noexcept {
					return (mNextImage + mDesc.imageCount - 1u) % mDesc.imageCount;
				}

			private:
				NkXrSwapchainDesc mDesc{};
				uint64 mNativeImages[kMaxImages]{};
				uint32 mNextImage = 0u;
				bool mAcquired = false;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKXRSWAPCHAIN_H__
