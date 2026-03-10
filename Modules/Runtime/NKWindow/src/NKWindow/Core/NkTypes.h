#pragma once

// =============================================================================
// NkTypes.h
// Types mathématiques et énumérations fondamentaux.
// Convention :
//   - Structs/classes/enums : PascalCase préfixé Nk
//   - Valeurs d'énumération : NK_UPPER_SNAKE_CASE
//   - Membres publics       : camelCase
//   - Membres privés        : mPascalCase
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <memory>
#include <functional>
#include <algorithm>

#include "NKMath/NkTypes.h"
#include "NKContainers/String/NkStringUtils.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	// ---------------------------------------------------------------------------
	// NkPixelFormat - formats de pixel supportés
	// ---------------------------------------------------------------------------

	enum class NkPixelFormat : uint32 {
		NK_PIXEL_UNKNOWN = 0,
		NK_PIXEL_R8G8B8A8_UNORM,
		NK_PIXEL_B8G8R8A8_UNORM,
		NK_PIXEL_R8G8B8A8_SRGB,
		NK_PIXEL_B8G8R8A8_SRGB,
		NK_PIXEL_R16G16B16A16_FLOAT,
		NK_PIXEL_D24_UNORM_S8_UINT,
		NK_PIXEL_D32_FLOAT,
		NK_PIXEL_RGBA8 = 0, ///< 4 octets R G B A
		NK_PIXEL_BGRA8,		///< 4 octets B G R A (natif Win32/macOS)
		NK_PIXEL_RGB8,		///< 3 octets R G B
		NK_PIXEL_YUV420,	///< YUV 4:2:0 planar (Android Camera2)
		NK_PIXEL_NV12,		///< NV12 semi-planar (Media Foundation)
		NK_PIXEL_YUYV,		///< YUYV packed (V4L2)
		NK_PIXEL_MJPEG,		///< JPEG par frame
		NK_PIXEL_FORMAT_MAX
	};

	// ---------------------------------------------------------------------------
	// NkError - résultat d'opération et message d'erreur
	// ---------------------------------------------------------------------------

	struct NkError {
		uint32 code = 0;
		NkString message = "";

		NkError() = default;
		NkError(uint32 code, NkString msg) : code(code), message(std::move(msg)) {
		}

		bool IsOk() const {
			return code == 0;
		}
		NkString ToString() const {
			if (code == 0) return NkString("OK");
			return NkString::Fmtf("[%u] ", code) + message;
		}

		static NkError Ok() {
			return NkError(0, "OK");
		}
	};

	// ---------------------------------------------------------------------------
	// NkRendererApi - backends graphiques disponibles
	// ---------------------------------------------------------------------------

	enum class NkRendererApi : uint32 {
		NK_NONE = 0,
		NK_SOFTWARE,
		NK_OPENGL,
		NK_VULKAN,
		NK_DIRECTX11,
		NK_DIRECTX12,
		NK_METAL,
		NK_RENDERER_API_MAX
	};

	inline const char *NkRendererApiToString(NkRendererApi api) {
		switch (api) {
			case NkRendererApi::NK_SOFTWARE:
				return "Software";
			case NkRendererApi::NK_OPENGL:
				return "OpenGL";
			case NkRendererApi::NK_VULKAN:
				return "Vulkan";
			case NkRendererApi::NK_DIRECTX11:
				return "DirectX 11";
			case NkRendererApi::NK_DIRECTX12:
				return "DirectX 12";
			case NkRendererApi::NK_METAL:
				return "Metal";
			default:
				return "None";
		}
	}

} // namespace nkentseu
