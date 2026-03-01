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

#include "NKCore/NkTypes.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Entiers fixes
// ---------------------------------------------------------------------------

using NkU8 = nk_uint8;
using NkU16 = nk_uint16;
using NkU32 = nk_uint32;
using NkU64 = nk_uint64;
using NkI8 = nk_int8;
using NkI16 = nk_int16;
using NkI32 = nk_int32;
using NkI64 = nk_int64;

// ---------------------------------------------------------------------------
// NkVec2u - vecteur 2D non-signé
// ---------------------------------------------------------------------------

struct NkVec2u {
	NkU32 x = 0;
	NkU32 y = 0;

	NkVec2u() = default;
	NkVec2u(NkU32 x, NkU32 y) : x(x), y(y) {
	}

	bool operator==(const NkVec2u &other) const {
		return x == other.x && y == other.y;
	}
	bool operator!=(const NkVec2u &other) const {
		return !(*this == other);
	}

	template <typename T> NkVec2u operator*(T s) const {
		return {static_cast<NkU32>(x * s), static_cast<NkU32>(y * s)};
	}

	template <typename T> NkVec2u operator/(T s) const {
		return {static_cast<NkU32>(x / s), static_cast<NkU32>(y / s)};
	}
};

// ---------------------------------------------------------------------------
// NkVec2i - vecteur 2D signé
// ---------------------------------------------------------------------------

struct NkVec2i {
	NkI32 x = 0;
	NkI32 y = 0;

	NkVec2i() = default;
	NkVec2i(NkI32 x, NkI32 y) : x(x), y(y) {
	}
};

// ---------------------------------------------------------------------------
// NkRect - rectangle entier
// ---------------------------------------------------------------------------

struct NkRect {
	NkI32 x = 0;
	NkI32 y = 0;
	NkU32 width = 0;
	NkU32 height = 0;

	NkRect() = default;
	NkRect(NkI32 x, NkI32 y, NkU32 w, NkU32 h) : x(x), y(y), width(w), height(h) {
	}
};

// ---------------------------------------------------------------------------
// NkVec2f - vecteur 2D flottant
// ---------------------------------------------------------------------------

struct NkVec2f {
	float x = 0.f;
	float y = 0.f;

	NkVec2f() = default;
	NkVec2f(float x, float y) : x(x), y(y) {
	}

	NkVec2f operator+(const NkVec2f &o) const {
		return {x + o.x, y + o.y};
	}
	NkVec2f operator-(const NkVec2f &o) const {
		return {x - o.x, y - o.y};
	}
	NkVec2f operator*(float s) const {
		return {x * s, y * s};
	}
	NkVec2f operator/(float s) const {
		return {x / s, y / s};
	}
	NkVec2f &operator+=(const NkVec2f &o) {
		x += o.x;
		y += o.y;
		return *this;
	}
	NkVec2f &operator-=(const NkVec2f &o) {
		x -= o.x;
		y -= o.y;
		return *this;
	}
	NkVec2f &operator*=(float s) {
		x *= s;
		y *= s;
		return *this;
	}

	float LengthSq() const {
		return x * x + y * y;
	}
	float Length() const; // impl inline ci-dessous
	NkVec2f Normalized() const;
	float Dot(const NkVec2f &o) const {
		return x * o.x + y * o.y;
	}
};

// ---------------------------------------------------------------------------
// NkVec3f - vecteur 3D flottant (utile pour homogène 2D)
// ---------------------------------------------------------------------------

struct NkVec3f {
	float x = 0.f, y = 0.f, z = 0.f;

	NkVec3f() = default;
	NkVec3f(float x, float y, float z) : x(x), y(y), z(z) {
	}
	explicit NkVec3f(const NkVec2f &v, float z = 1.f) : x(v.x), y(v.y), z(z) {
	}

	NkVec2f ToVec2() const {
		return {x, y};
	}
	NkVec3f operator+(const NkVec3f &o) const {
		return {x + o.x, y + o.y, z + o.z};
	}
	NkVec3f operator*(float s) const {
		return {x * s, y * s, z * s};
	}
};

// ---------------------------------------------------------------------------
// Inline implementations
// ---------------------------------------------------------------------------

#include <cmath>

inline float NkVec2f::Length() const {
	return std::sqrt(LengthSq());
}
inline NkVec2f NkVec2f::Normalized() const {
	float l = Length();
	return l > 1e-8f ? NkVec2f{x / l, y / l} : NkVec2f{};
}

// ---------------------------------------------------------------------------
// NkPixelFormat - formats de pixel supportés
// ---------------------------------------------------------------------------

enum class NkPixelFormat : NkU32 {
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
	NkU32 code = 0;
	std::string message = "";

	NkError() = default;
	NkError(NkU32 code, std::string msg) : code(code), message(std::move(msg)) {
	}

	bool IsOk() const {
		return code == 0;
	}
	std::string ToString() const {
		return code == 0 ? "OK" : "[" + std::to_string(code) + "] " + message;
	}

	static NkError Ok() {
		return NkError(0, "OK");
	}
};

// ---------------------------------------------------------------------------
// NkRendererApi - backends graphiques disponibles
// ---------------------------------------------------------------------------

enum class NkRendererApi : NkU32 {
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
