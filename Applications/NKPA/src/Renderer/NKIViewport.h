#pragma once
/**
 * @File    NKIViewport.h
 * @Brief   Interface d'un viewport NKPA — zone de rendu + conversion NDC
 *          consciente du backend graphique.
 *
 *  Un viewport encapsule DEUX responsabilités :
 *    1. La ZONE de rendu (rectangle en pixels dans la fenêtre) → NkViewport RHI
 *       + scissor à poser sur le command buffer.
 *    2. La CONVERSION coordonnées-contenu (pixels, origine haut-gauche) → NDC,
 *       en gérant la convention d'axe Y qui DIFFÈRE selon le backend :
 *         • OpenGL / Vulkan / Software → NDC Y+ vers le haut (le RHI/le viewport
 *           Vulkan à hauteur négative normalisent tout sur la convention GL).
 *         • DirectX 11 / 12 → le générateur HLSL de NkSL émet un
 *           `output._Position.y = -_Position.y` dans le vertex shader ; on
 *           PRÉ-INVERSE donc le Y ici pour que le rendu final soit à l'endroit.
 *
 *  Interface pure (NKI) → on peut fournir plusieurs implémentations (plein
 *  écran, sous-fenêtre, split-screen…) sans toucher au code de rendu.
 */

#include "NKCore/NkTypes.h"
#include "NKMath/NkVec.h"
#include "NKRHI/Core/NkGraphicsApi.h"
#include "NKRHI/Core/NkTypes.h" // NkViewport, NkRect2D

namespace nkentseu {
	namespace nkpa {

		using namespace math;

		class NKIViewport {
			public:
				virtual ~NKIViewport() = default;

				/// Configure la SOUS-ZONE de rendu (x,y,w,h en pixels) DANS une fenêtre
				/// (winW×winH) et le backend cible. Le contenu (0..w, 0..h) est mappé
				/// dans le sous-rectangle NDC correspondant → on utilise un viewport RHI
				/// PLEIN CADRE (pas de sous-viewport → pas de souci d'origine Y GL/SW).
				virtual void Configure(NkGraphicsApi api, int32 x, int32 y, int32 w, int32 h, int32 winW,
									   int32 winH) = 0;

				/// Dimensions logiques du contenu (= dimensions de la sous-zone).
				virtual float32 ContentWidth() const = 0;
				virtual float32 ContentHeight() const = 0;

				/// Convertit un point pixel-contenu (0..w, 0..h, origine haut-gauche) en
				/// NDC (placé dans le sous-rect central ; gère le flip Y DX).
				virtual NkVector2f ToNDC(float32 px, float32 py) const = 0;

				/// Viewport RHI plein cadre (SetViewport).
				virtual NkViewport RHIViewport() const = 0;

				/// Scissor plein cadre (SetScissor).
				virtual NkRect2D ScissorRect() const = 0;
		};

	} // namespace nkpa
} // namespace nkentseu
