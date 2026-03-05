#pragma once

// =============================================================================
// NKMath/NkTypes.h
// Types mathématiques fondamentaux du framework Nkentseu.
//
// Convention :
//   - Structs/classes : PascalCase préfixé Nk
//   - Membres publics : camelCase
//   - Membres privés  : mPascalCase
//
// Dépend de NKCore/NkTypes.h pour les primitives fixes (nk_uint8, etc.)
// =============================================================================

#include <cmath>
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // ---------------------------------------------------------------------------
    // Entiers fixes
    // ---------------------------------------------------------------------------

    using NkU8  = nk_uint8;
    using NkU16 = nk_uint16;
    using NkU32 = nk_uint32;
    using NkU64 = nk_uint64;
    using NkI8  = nk_int8;
    using NkI16 = nk_int16;
    using NkI32 = nk_int32;
    using NkI64 = nk_int64;
    using NkF32 = float32;
    using NkF64 = float64;

    // ---------------------------------------------------------------------------
    // NkVec2u — vecteur 2D non-signé (uint32)
    // ---------------------------------------------------------------------------

    struct NkVec2u {
        NkU32 x = 0;
        NkU32 y = 0;

        NkVec2u() = default;
        NkVec2u(NkU32 x, NkU32 y) : x(x), y(y) {}

        bool operator==(const NkVec2u& other) const { return x == other.x && y == other.y; }
        bool operator!=(const NkVec2u& other) const { return !(*this == other); }

        template <typename T> NkVec2u operator*(T s) const {
            return { static_cast<NkU32>(x * s), static_cast<NkU32>(y * s) };
        }
        template <typename T> NkVec2u operator/(T s) const {
            return { static_cast<NkU32>(x / s), static_cast<NkU32>(y / s) };
        }
    };

    // ---------------------------------------------------------------------------
    // NkVec2i — vecteur 2D signé (int32)
    // ---------------------------------------------------------------------------

    struct NkVec2i {
        NkI32 x = 0;
        NkI32 y = 0;

        NkVec2i() = default;
        NkVec2i(NkI32 x, NkI32 y) : x(x), y(y) {}

        bool operator==(const NkVec2i& other) const { return x == other.x && y == other.y; }
        bool operator!=(const NkVec2i& other) const { return !(*this == other); }

        NkVec2i operator+(const NkVec2i& o) const { return { x + o.x, y + o.y }; }
        NkVec2i operator-(const NkVec2i& o) const { return { x - o.x, y - o.y }; }
    };

    // ---------------------------------------------------------------------------
    // NkRect — rectangle entier (position + taille)
    // ---------------------------------------------------------------------------

    struct NkRect {
        NkI32 x      = 0;
        NkI32 y      = 0;
        NkU32 width  = 0;
        NkU32 height = 0;

        NkRect() = default;
        NkRect(NkI32 x, NkI32 y, NkU32 w, NkU32 h) : x(x), y(y), width(w), height(h) {}

        bool Contains(NkI32 px, NkI32 py) const {
            return px >= x && px < x + static_cast<NkI32>(width) &&
                py >= y && py < y + static_cast<NkI32>(height);
        }
    };

    // ---------------------------------------------------------------------------
    // NkVec2f — vecteur 2D flottant
    // ---------------------------------------------------------------------------

    struct NkVec2f {
        float x = 0.f;
        float y = 0.f;

        NkVec2f() = default;
        NkVec2f(float x, float y) : x(x), y(y) {}

        NkVec2f operator+(const NkVec2f& o) const { return { x + o.x, y + o.y }; }
        NkVec2f operator-(const NkVec2f& o) const { return { x - o.x, y - o.y }; }
        NkVec2f operator*(float s)          const { return { x * s, y * s }; }
        NkVec2f operator/(float s)          const { return { x / s, y / s }; }

        NkVec2f& operator+=(const NkVec2f& o) { x += o.x; y += o.y; return *this; }
        NkVec2f& operator-=(const NkVec2f& o) { x -= o.x; y -= o.y; return *this; }
        NkVec2f& operator*=(float s)          { x *= s;   y *= s;   return *this; }

        float LengthSq() const { return x * x + y * y; }
        float Length()   const;       // defined below
        NkVec2f Normalized() const;   // defined below
        float Dot(const NkVec2f& o) const { return x * o.x + y * o.y; }

        bool operator==(const NkVec2f& o) const { return x == o.x && y == o.y; }
        bool operator!=(const NkVec2f& o) const { return !(*this == o); }
    };

    struct NkVec2d {
        double x = 0.0;
        double y = 0.0;

        NkVec2d() = default;
        NkVec2d(double x, double y) : x(x), y(y) {}

        NkVec2d operator+(const NkVec2d& o) const { return { x + o.x, y + o.y }; }
        NkVec2d operator-(const NkVec2d& o) const { return { x - o.x, y - o.y }; }
        NkVec2d operator*(double s)          const { return { x * s, y * s }; }
        NkVec2d operator/(double s)          const { return { x / s, y / s }; }

        NkVec2d& operator+=(const NkVec2d& o) { x += o.x; y += o.y; return *this; }
        NkVec2d& operator-=(const NkVec2d& o) { x -= o.x; y -= o.y; return *this; }
        NkVec2d& operator*=(double s)          { x *= s;   y *= s;   return *this; }

        double LengthSq() const { return x * x + y * y; }
        double Length()   const;       // defined below
        NkVec2d Normalized() const;   // defined below
        double Dot(const NkVec2d& o) const { return x * o.x + y * o.y; }

        bool operator==(const NkVec2d& o) const { return x == o.x && y == o.y; }
        bool operator!=(const NkVec2d& o) const { return !(*this == o); }
    };

    // ---------------------------------------------------------------------------
    // NkVec3f — vecteur 3D flottant (utile pour coordonnées homogènes 2D)
    // ---------------------------------------------------------------------------

    struct NkVec3f {
        float x = 0.f, y = 0.f, z = 0.f;

        NkVec3f() = default;
        NkVec3f(float x, float y, float z) : x(x), y(y), z(z) {}
        explicit NkVec3f(const NkVec2f& v, float z = 1.f) : x(v.x), y(v.y), z(z) {}

        NkVec2f ToVec2()            const { return { x, y }; }
        NkVec3f operator+(const NkVec3f& o) const { return { x + o.x, y + o.y, z + o.z }; }
        NkVec3f operator*(float s)          const { return { x * s, y * s, z * s }; }
    };

    // ---------------------------------------------------------------------------
    // NkVec3f — vecteur 3D flottant (utile pour coordonnées homogènes 2D)
    // ---------------------------------------------------------------------------

    struct NkVec3d {
        double x = 0.0, y = 0.0, z = 0.0;

        NkVec3d() = default;
        NkVec3d(double x, double y, double z) : x(x), y(y), z(z) {}
        explicit NkVec3d(const NkVec2d& v, double z = 1.0) : x(v.x), y(v.y), z(z) {}

        NkVec2d ToVec2()            const { return { x, y }; }
        NkVec3d operator+(const NkVec3d& o) const { return { x + o.x, y + o.y, z + o.z }; }
        NkVec3d operator*(double s)          const { return { x * s, y * s, z * s }; }
    };

    // ---------------------------------------------------------------------------
    // Inline implementations
    // ---------------------------------------------------------------------------

    inline float NkVec2f::Length() const {
        return std::sqrt(LengthSq());
    }

    inline NkVec2f NkVec2f::Normalized() const {
        float l = Length();
        return l > 1e-8f ? NkVec2f{ x / l, y / l } : NkVec2f{};
    }

} // namespace nkentseu
