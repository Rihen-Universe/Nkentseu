/**
 * @File    NkPAViewport.cpp
 * @Brief   Implémentation du viewport plein-cadre NKPA.
 */

#include "Renderer/NkPAViewport.h"

namespace nkentseu {
	namespace nkpa {

		void NkPAViewport::Configure(NkGraphicsApi api, int32 x, int32 y, int32 w, int32 h, int32 winW, int32 winH) {
			mApi = api;
			mX = x;
			mY = y;
			mW = w > 0 ? w : 1;
			mH = h > 0 ? h : 1;
			mWinW = winW > 0 ? winW : 1;
			mWinH = winH > 0 ? winH : 1;
			// DX : le vertex shader HLSL généré par NkSL négative _Position.y
			// (convention moteur). On pré-inverse le NDC pour compenser → à l'endroit.
			mFlipYForDX = (api == NkGraphicsApi::NK_GFX_API_DX11 || api == NkGraphicsApi::NK_GFX_API_DX12);
		}

		NkVector2f NkPAViewport::ToNDC(float32 px, float32 py) const {
			// Pixel-contenu → pixel-fenêtre → NDC plein cadre (place la sim dans le
			// sous-rect central). Viewport RHI plein cadre → aucun souci d'origine Y
			// GL/Software ; seul le flip DX (NDC) est appliqué.
			const float32 winX = (float32)mX + px;
			const float32 winY = (float32)mY + py;
			const float32 nx = (winX / (float32)mWinW) * 2.0f - 1.0f;
			float32 ny = 1.0f - (winY / (float32)mWinH) * 2.0f; // haut de fenêtre = +1
			if (mFlipYForDX)
				ny = -ny; // DX : compense le -_Position.y du VS HLSL généré
			return {nx, ny};
		}

		NkViewport NkPAViewport::RHIViewport() const {
			// PLEIN CADRE (0,0,winW,winH) : identique sur tous les backends (y=0 → pas
			// de différence top-left/bottom-left). Le sous-rect est fait en NDC.
			NkViewport vp;
			vp.x = 0.f;
			vp.y = 0.f;
			vp.width = (float32)mWinW;
			vp.height = (float32)mWinH;
			vp.minDepth = 0.f;
			vp.maxDepth = 1.f;
			return vp;
		}

		NkRect2D NkPAViewport::ScissorRect() const {
			// Plein cadre : la sim est déjà confinée au sous-rect central via le NDC.
			return NkRect2D{0, 0, mWinW, mWinH};
		}

	} // namespace nkpa
} // namespace nkentseu
