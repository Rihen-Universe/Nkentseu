#pragma once
/**
 * @File    NkPAViewport.h
 * @Brief   Viewport plein-cadre NKPA (implémentation de NKIViewport).
 *
 *  Mappe le contenu (pixels, origine haut-gauche) vers le NDC du backend et
 *  produit le NkViewport RHI + le scissor. Gère la convention Y par backend
 *  (flip DX). Réutilisable tel quel ou comme base pour des viewports partiels.
 */

#include "Renderer/NKIViewport.h"

namespace nkentseu {
	namespace nkpa {

		class NkPAViewport : public NKIViewport {
			public:
				void Configure(NkGraphicsApi api, int32 x, int32 y, int32 w, int32 h, int32 winW,
							   int32 winH) override;

				float32 ContentWidth() const override {
					return (float32)mW;
				}

				float32 ContentHeight() const override {
					return (float32)mH;
				}

				NkVector2f ToNDC(float32 px, float32 py) const override;
				NkViewport RHIViewport() const override;
				NkRect2D ScissorRect() const override;

				NkGraphicsApi Api() const {
					return mApi;
				}

			private:
				NkGraphicsApi mApi = NkGraphicsApi::NK_GFX_API_OPENGL;
				int32 mX = 0, mY = 0;	   ///< origine de la sous-zone (pixels, haut-gauche)
				int32 mW = 1280, mH = 720; ///< dimensions de la sous-zone (= monde de la sim)
				int32 mWinW = 1280, mWinH = 720; ///< dimensions de la fenêtre (viewport plein cadre)
				bool mFlipYForDX = false;		 ///< true si backend DX (HLSL VS négatif _Position.y)
		};

	} // namespace nkpa
} // namespace nkentseu
