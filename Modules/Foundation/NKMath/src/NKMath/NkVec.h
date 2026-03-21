//
// NkVec.h
// Vecteurs 2D, 3D et 4D génériques — un seul template, 4 variantes via aliases.
//
// Types disponibles (suffixe f=float32, d=float64, i=int32, u=uint32) :
//   NkVector2f / NkVec2Tf    NkVector2d / NkVec2Td
//   NkVector2i / NkVec2Ti    NkVector2u / NkVec2Tu
//   NkVector3f / NkVec3Tf    ...
//   NkVector4f / NkVec4Tf    ...
//
// Les algorithmes géométriques (SLerp, NLerp, Lerp, Project, etc.) sont
// implémentés uniquement pour NkVec3T car c'est là qu'ils ont un sens physique.
// NkVec2T expose les versions 2D correspondantes.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_VECTOR_H__
#define __NKENTSEU_VECTOR_H__

#include "NKMath/NkLegacySystem.h"
#include "NKMath/NkFunctions.h"
#include "NKContainers/String/NkFormat.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {

    namespace math {

        // Forward declarations pour les conversions croisées
        template<typename T> struct NkVec2T;
        template<typename T> struct NkVec3T;
        template<typename T> struct NkVec4T;
        class NkAngle;

        // =====================================================================
        // NkVec2T<T>
        //
        // Vecteur 2D générique. Utilisé pour coordonnées 2D, UV, tailles.
        // Les opérations de base (normalize, dot, lerp, reflect...) sont
        // disponibles. Pas de produit vectoriel en 2D (résultat scalaire,
        // voir Collinear()).
        // =====================================================================

        template<typename T>
        struct NkVec2T {

            union {
                struct { T x, y; };
                struct { T u, v; };
                struct { T width, height; };
                struct { T r, g; };
                T data[2];
            };

            // ── Constructeurs ─────────────────────────────────────────────────

            constexpr NkVec2T() noexcept
                : x(T(0)), y(T(0)) {}

            constexpr explicit NkVec2T(T scalar) noexcept
                : x(scalar), y(scalar) {}

            constexpr NkVec2T(T x, T y) noexcept
                : x(x), y(y) {}

            constexpr explicit NkVec2T(const T* p) noexcept
                : x(p[0]), y(p[1]) {}

            NkVec2T(const NkVec2T&) noexcept            = default;
            NkVec2T& operator=(const NkVec2T&) noexcept = default;

            /// Conversion cross-type explicite (float→int, double→float, etc.)
            template<typename U>
            constexpr explicit NkVec2T(const NkVec2T<U>& o) noexcept
                : x(static_cast<T>(o.x)), y(static_cast<T>(o.y)) {}

            template<typename U>
            NkVec2T& operator=(const NkVec2T<U>& o) {
                x = static_cast<T>(o.x);
                y = static_cast<T>(o.y);
                return *this;
            }

            // ── Accesseurs bruts ──────────────────────────────────────────────

            operator T*()             noexcept { return data; }
            operator const T*() const noexcept { return data; }

            template<typename U>
            operator NkVec2T<U>()             noexcept { return NkVec2T<U>(x, y); }
            template<typename U>
            operator const NkVec2T<U>() const noexcept { return NkVec2T<U>(x, y); }

            NK_FORCE_INLINE T&       operator[](size_t i)       noexcept { return data[i & 1]; }
            NK_FORCE_INLINE const T& operator[](size_t i) const noexcept { return data[i & 1]; }

            // ── Arithmétique ──────────────────────────────────────────────────

            NK_FORCE_INLINE NkVec2T operator-() const noexcept {
                return { -x, -y };
            }

            NK_FORCE_INLINE NkVec2T operator+(const NkVec2T& o) const noexcept { return { x + o.x, y + o.y }; }
            NK_FORCE_INLINE NkVec2T operator-(const NkVec2T& o) const noexcept { return { x - o.x, y - o.y }; }
            NK_FORCE_INLINE NkVec2T operator*(const NkVec2T& o) const noexcept { return { x * o.x, y * o.y }; }
            NK_FORCE_INLINE NkVec2T operator/(const NkVec2T& o) const noexcept { return { x / o.x, y / o.y }; }
            NK_FORCE_INLINE NkVec2T operator*(T s)             const noexcept { return { x * s,   y * s   }; }
            NK_FORCE_INLINE NkVec2T operator/(T s)             const noexcept {
                T inv = T(1) / s;
                return { x * inv, y * inv };
            }

            NK_FORCE_INLINE NkVec2T& operator+=(const NkVec2T& o) noexcept { x += o.x; y += o.y; return *this; }
            NK_FORCE_INLINE NkVec2T& operator-=(const NkVec2T& o) noexcept { x -= o.x; y -= o.y; return *this; }
            NK_FORCE_INLINE NkVec2T& operator*=(const NkVec2T& o) noexcept { x *= o.x; y *= o.y; return *this; }
            NK_FORCE_INLINE NkVec2T& operator/=(const NkVec2T& o) noexcept { x /= o.x; y /= o.y; return *this; }
            NK_FORCE_INLINE NkVec2T& operator*=(T s)             noexcept { x *= s; y *= s; return *this; }
            NK_FORCE_INLINE NkVec2T& operator/=(T s)             noexcept {
                T inv = T(1) / s;
                x *= inv; y *= inv;
                return *this;
            }

            friend NK_FORCE_INLINE NkVec2T operator*(T s, const NkVec2T& v) noexcept { return v * s; }

            // ── Incrément / Décrément ─────────────────────────────────────────

            NK_FORCE_INLINE NkVec2T& operator++()    noexcept { ++x; ++y; return *this; }
            NK_FORCE_INLINE NkVec2T& operator--()    noexcept { --x; --y; return *this; }
            NK_FORCE_INLINE NkVec2T  operator++(int) noexcept { NkVec2T t(*this); ++(*this); return t; }
            NK_FORCE_INLINE NkVec2T  operator--(int) noexcept { NkVec2T t(*this); --(*this); return t; }

            // ── Comparaison ───────────────────────────────────────────────────

            friend NK_FORCE_INLINE bool operator==(const NkVec2T& a, const NkVec2T& b) noexcept {
                if constexpr (traits::NkIsFloatingPoint_v<T>) {
                    return NkFabs(a.x - b.x) <= T(NkVectorEpsilon)
                        && NkFabs(a.y - b.y) <= T(NkVectorEpsilon);
                } else {
                    return a.x == b.x && a.y == b.y;
                }
            }

            friend NK_FORCE_INLINE bool operator!=(const NkVec2T& a, const NkVec2T& b) noexcept {
                return !(a == b);
            }

            // ── Produit scalaire / Longueur ───────────────────────────────────

            NK_FORCE_INLINE T Dot(const NkVec2T& o) const noexcept {
                return x * o.x + y * o.y;
            }

            NK_FORCE_INLINE T LenSq() const noexcept {
                return x * x + y * y;
            }

            NK_FORCE_INLINE T Len() const noexcept {
                T lsq = LenSq();
                return (lsq < T(NkVectorEpsilon))
                    ? T(0)
                    : static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
            }

            NK_FORCE_INLINE T Distance(const NkVec2T& o) const noexcept {
                return (*this - o).Len();
            }

            // ── Normalisation ─────────────────────────────────────────────────

            void Normalize() noexcept {
                T lsq = LenSq();
                if (lsq < T(NkVectorEpsilon)) { return; }
                T inv = T(1) / static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
                x *= inv;
                y *= inv;
            }

            NK_FORCE_INLINE NkVec2T Normalized() const noexcept {
                NkVec2T v(*this);
                v.Normalize();
                return v;
            }

            NK_FORCE_INLINE bool IsUnit() const noexcept {
                return NkFabs(static_cast<float32>(LenSq()) - 1.0f)
                     < 2.0f * static_cast<float32>(NkEpsilon);
            }

            // ── Géométrie ─────────────────────────────────────────────────────

            /// Normale perpendiculaire (rotation 90° anti-horaire)
            NK_FORCE_INLINE NkVec2T Normal()    const noexcept { return { -y,  x }; }
            NK_FORCE_INLINE NkVec2T Rotate90()  const noexcept { return { -y,  x }; }
            NK_FORCE_INLINE NkVec2T Rotate180() const noexcept { return { -x, -y }; }
            NK_FORCE_INLINE NkVec2T Rotate270() const noexcept { return {  y, -x }; }

            /// Projection orthogonale de *this sur 'onto'
            NK_FORCE_INLINE NkVec2T Project(const NkVec2T& onto) const noexcept {
                T d = onto.LenSq();
                if (d < T(NkVectorEpsilon)) { return {}; }
                return onto * (Dot(onto) / d);
            }

            /// Composante de *this perpendiculaire à 'onto'
            NK_FORCE_INLINE NkVec2T Reject(const NkVec2T& onto) const noexcept {
                return *this - Project(onto);
            }

            /// Réflexion de *this par rapport à la normale n (n doit être unitaire)
            NK_FORCE_INLINE NkVec2T Reflect(const NkVec2T& n) const noexcept {
                return *this - n * (T(2) * Dot(n));
            }

            // ── Interpolation ─────────────────────────────────────────────────

            /// Interpolation linéaire entre *this et 'to' (t∈[0,1])
            NK_FORCE_INLINE NkVec2T Lerp(const NkVec2T& to, float32 t) const noexcept {
                return *this + (to - *this) * static_cast<T>(t);
            }

            /// Lerp puis normalisation — pour directions sur un cercle
            NK_FORCE_INLINE NkVec2T NLerp(const NkVec2T& to, float32 t) const noexcept {
                return Lerp(to, t).Normalized();
            }

            /// Interpolation sphérique — chemin arc sur le cercle unitaire
            NkVec2T SLerp(const NkVec2T& to, float32 t) const noexcept {
                if (t < 0.01f) { return Lerp(to, t); }
                NkVec2T  from = Normalized();
                NkVec2T  tod  = NkVec2T(to).Normalized();
                // Clamp pour éviter les NaN dans acos si le dot dépasse ±1 (erreurs fp)
                float32 cosA = NkClamp(static_cast<float32>(from.Dot(tod)), -1.0f, 1.0f);
                if (cosA > 1.0f - static_cast<float32>(NkEpsilon)) {
                    return NLerp(to, t); // Quasi-parallèles → NLerp plus stable
                }
                float32 theta = NkAcos(cosA);
                float32 sinT  = NkSin(theta);
                float32 a     = NkSin((1.0f - t) * theta) / sinT;
                float32 b     = NkSin(t           * theta) / sinT;
                return from * static_cast<T>(a) + tod * static_cast<T>(b);
            }

            /// Vrai si les deux vecteurs sont colinéaires (produit croisé scalaire ≈ 0)
            NK_FORCE_INLINE bool Collinear(const NkVec2T& b) const noexcept {
                return NkFabs(static_cast<float32>(x * b.y - y * b.x))
                     < static_cast<float32>(NkEpsilon);
            }

            // ── Constantes statiques ──────────────────────────────────────────

            static constexpr NkVec2T Zero()  noexcept { return {};           }
            static constexpr NkVec2T One()   noexcept { return { T(1), T(1) }; }
            static constexpr NkVec2T UnitX() noexcept { return { T(1), T(0) }; }
            static constexpr NkVec2T UnitY() noexcept { return { T(0), T(1) }; }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat("({0}, {1})", x, y);
            }

            friend NkString ToString(const NkVec2T& v) {
                return v.ToString();
            }

            friend std::ostream& operator<<(std::ostream& os, const NkVec2T& v) {
                return os << v.ToString().CStr();
            }

        }; // struct NkVec2T


        // =====================================================================
        // NkVec3T<T>
        //
        // Vecteur 3D générique. Pilier de toute la géométrie 3D.
        // Inclut: produit vectoriel, Lerp/NLerp/SLerp, Project/Reject/Reflect.
        //
        // Algorithmes de référence:
        //   - SLerp: Shoemake 1985 via acos + sin(theta) — numeriquement stable
        //   - Normalize: calcul d'inverse sqrt, guard epsilon
        // =====================================================================

        template<typename T>
        struct NkVec3T {

            union {
                struct { T x, y, z; };
                struct { T r, g, b; };
                struct { T u, v, w; };
                struct { T pitch, yaw, roll; };
                T data[3];
            };

            // ── Constructeurs ─────────────────────────────────────────────────

            constexpr NkVec3T() noexcept
                : x(T(0)), y(T(0)), z(T(0)) {}

            constexpr explicit NkVec3T(T scalar) noexcept
                : x(scalar), y(scalar), z(scalar) {}

            constexpr NkVec3T(T x, T y, T z) noexcept
                : x(x), y(y), z(z) {}

            constexpr NkVec3T(const NkVec2T<T>& xy, T z) noexcept
                : x(xy.x), y(xy.y), z(z) {}

            constexpr NkVec3T(T x, const NkVec2T<T>& yz) noexcept
                : x(x), y(yz.x), z(yz.y) {}

            constexpr explicit NkVec3T(const T* p) noexcept
                : x(p[0]), y(p[1]), z(p[2]) {}

            NkVec3T(const NkVec3T&) noexcept            = default;
            NkVec3T& operator=(const NkVec3T&) noexcept = default;

            template<typename U>
            constexpr explicit NkVec3T(const NkVec3T<U>& o) noexcept
                : x(static_cast<T>(o.x)), y(static_cast<T>(o.y)), z(static_cast<T>(o.z)) {}

            template<typename U>
            NkVec3T& operator=(const NkVec3T<U>& o) {
                x = static_cast<T>(o.x);
                y = static_cast<T>(o.y);
                z = static_cast<T>(o.z);
                return *this;
            }

            // ── Accesseurs bruts ──────────────────────────────────────────────

            operator T*()             noexcept { return data; }
            operator const T*() const noexcept { return data; }

            template<typename U>
            operator NkVec3T<U>()             noexcept { return NkVec3T<U>(x, y, z); }
            template<typename U>
            operator const NkVec3T<U>() const noexcept { return NkVec3T<U>(x, y, z); }

            NK_FORCE_INLINE T&       operator[](size_t i)       noexcept { return data[i < 3 ? i : 2]; }
            NK_FORCE_INLINE const T& operator[](size_t i) const noexcept { return data[i < 3 ? i : 2]; }

            // ── Arithmétique ──────────────────────────────────────────────────

            NK_FORCE_INLINE NkVec3T operator-() const noexcept {
                return { -x, -y, -z };
            }

            NK_FORCE_INLINE NkVec3T operator+(const NkVec3T& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
            NK_FORCE_INLINE NkVec3T operator-(const NkVec3T& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
            NK_FORCE_INLINE NkVec3T operator*(const NkVec3T& o) const noexcept { return { x * o.x, y * o.y, z * o.z }; }
            NK_FORCE_INLINE NkVec3T operator/(const NkVec3T& o) const noexcept { return { x / o.x, y / o.y, z / o.z }; }
            NK_FORCE_INLINE NkVec3T operator+(T s)             const noexcept { return { x + s, y + s, z + s }; }
            NK_FORCE_INLINE NkVec3T operator-(T s)             const noexcept { return { x - s, y - s, z - s }; }
            NK_FORCE_INLINE NkVec3T operator*(T s)             const noexcept { return { x * s, y * s, z * s }; }
            NK_FORCE_INLINE NkVec3T operator/(T s)             const noexcept {
                T inv = T(1) / s;
                return { x * inv, y * inv, z * inv };
            }

            NK_FORCE_INLINE NkVec3T& operator+=(const NkVec3T& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
            NK_FORCE_INLINE NkVec3T& operator-=(const NkVec3T& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
            NK_FORCE_INLINE NkVec3T& operator*=(const NkVec3T& o) noexcept { x *= o.x; y *= o.y; z *= o.z; return *this; }
            NK_FORCE_INLINE NkVec3T& operator/=(const NkVec3T& o) noexcept { x /= o.x; y /= o.y; z /= o.z; return *this; }
            NK_FORCE_INLINE NkVec3T& operator+=(T s)             noexcept { x += s; y += s; z += s; return *this; }
            NK_FORCE_INLINE NkVec3T& operator-=(T s)             noexcept { x -= s; y -= s; z -= s; return *this; }
            NK_FORCE_INLINE NkVec3T& operator*=(T s)             noexcept { x *= s; y *= s; z *= s; return *this; }
            NK_FORCE_INLINE NkVec3T& operator/=(T s)             noexcept {
                T inv = T(1) / s;
                x *= inv; y *= inv; z *= inv;
                return *this;
            }

            friend NK_FORCE_INLINE NkVec3T operator+(T s, const NkVec3T& v) noexcept { return v + s; }
            friend NK_FORCE_INLINE NkVec3T operator-(T s, const NkVec3T& v) noexcept { return { s - v.x, s - v.y, s - v.z }; }
            friend NK_FORCE_INLINE NkVec3T operator*(T s, const NkVec3T& v) noexcept { return v * s; }
            friend NK_FORCE_INLINE NkVec3T operator/(T s, const NkVec3T& v) noexcept { return { s / v.x, s / v.y, s / v.z }; }

            // ── Incrément / Décrément ─────────────────────────────────────────

            NK_FORCE_INLINE NkVec3T& operator++()    noexcept { ++x; ++y; ++z; return *this; }
            NK_FORCE_INLINE NkVec3T& operator--()    noexcept { --x; --y; --z; return *this; }
            NK_FORCE_INLINE NkVec3T  operator++(int) noexcept { NkVec3T t(*this); ++(*this); return t; }
            NK_FORCE_INLINE NkVec3T  operator--(int) noexcept { NkVec3T t(*this); --(*this); return t; }

            // ── Comparaison ───────────────────────────────────────────────────

            friend NK_FORCE_INLINE bool operator==(const NkVec3T& a, const NkVec3T& b) noexcept {
                if constexpr (traits::NkIsFloatingPoint_v<T>) {
                    return NkFabs(a.x - b.x) <= T(NkVectorEpsilon)
                        && NkFabs(a.y - b.y) <= T(NkVectorEpsilon)
                        && NkFabs(a.z - b.z) <= T(NkVectorEpsilon);
                } else {
                    return a.x == b.x && a.y == b.y && a.z == b.z;
                }
            }

            friend NK_FORCE_INLINE bool operator!=(const NkVec3T& a, const NkVec3T& b) noexcept {
                return !(a == b);
            }

            // ── Produit scalaire / Longueur ───────────────────────────────────

            NK_FORCE_INLINE T Dot(const NkVec3T& o) const noexcept {
                return x * o.x + y * o.y + z * o.z;
            }

            NK_FORCE_INLINE T LenSq() const noexcept {
                return x * x + y * y + z * z;
            }

            NK_FORCE_INLINE T Len() const noexcept {
                T lsq = LenSq();
                return (lsq < T(NkVectorEpsilon))
                    ? T(0)
                    : static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
            }

            NK_FORCE_INLINE T Distance(const NkVec3T& o) const noexcept {
                return (*this - o).Len();
            }

            /// Produit vectoriel — résultat perpendiculaire à a et b
            NK_FORCE_INLINE NkVec3T Cross(const NkVec3T& o) const noexcept {
                return {
                    y * o.z - z * o.y,
                    z * o.x - x * o.z,
                    x * o.y - y * o.x
                };
            }

            // ── Normalisation ─────────────────────────────────────────────────

            void Normalize() noexcept {
                T lsq = LenSq();
                if (lsq < T(NkVectorEpsilon)) { return; }
                T inv = T(1) / static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
                x *= inv;
                y *= inv;
                z *= inv;
            }

            NK_FORCE_INLINE NkVec3T Normalized() const noexcept {
                NkVec3T v(*this);
                v.Normalize();
                return v;
            }

            NK_FORCE_INLINE bool IsUnit() const noexcept {
                return NkFabs(static_cast<float32>(LenSq()) - 1.0f)
                     < 2.0f * static_cast<float32>(NkEpsilon);
            }

            // ── Géométrie ─────────────────────────────────────────────────────

            /// Projection orthogonale de *this sur 'onto'
            NK_FORCE_INLINE NkVec3T Project(const NkVec3T& onto) const noexcept {
                T d = onto.LenSq();
                if (d < T(NkVectorEpsilon)) { return {}; }
                return onto * (Dot(onto) / d);
            }

            /// Composante de *this perpendiculaire à 'onto'
            NK_FORCE_INLINE NkVec3T Reject(const NkVec3T& onto) const noexcept {
                return *this - Project(onto);
            }

            /// Réflexion de *this par rapport à la normale n (n doit être unitaire)
            NK_FORCE_INLINE NkVec3T Reflect(const NkVec3T& n) const noexcept {
                return *this - n * (T(2) * Dot(n));
            }

            // ── Interpolation ─────────────────────────────────────────────────

            /// Interpolation linéaire entre *this et 'to' pour t∈[0,1]
            NK_FORCE_INLINE NkVec3T Lerp(const NkVec3T& to, float32 t) const noexcept {
                return *this + (to - *this) * static_cast<T>(t);
            }

            /// Lerp puis normalisation — directions sur une sphère unitaire
            NK_FORCE_INLINE NkVec3T NLerp(const NkVec3T& to, float32 t) const noexcept {
                return Lerp(to, t).Normalized();
            }

            /// Interpolation sphérique (Shoemake 1985) — arc géodésique exact
            NkVec3T SLerp(const NkVec3T& to, float32 t) const noexcept {
                if (t < 0.01f) { return Lerp(to, t); }
                NkVec3T  from = Normalized();
                NkVec3T  tod  = NkVec3T(to).Normalized();
                // Clamp pour protéger acos contre les erreurs fp hors de [-1,1]
                float32 cosA = NkClamp(static_cast<float32>(from.Dot(tod)), -1.0f, 1.0f);
                if (cosA > 1.0f - static_cast<float32>(NkEpsilon)) {
                    return NLerp(to, t); // Quasi-parallèles → NLerp plus stable
                }
                float32 theta = NkAcos(cosA);
                float32 sinT  = NkSin(theta);
                float32 a     = NkSin((1.0f - t) * theta) / sinT;
                float32 b     = NkSin(t           * theta) / sinT;
                return from * static_cast<T>(a) + tod * static_cast<T>(b);
            }

            // ── Collinéarité ──────────────────────────────────────────────────

            /// Vrai si a et b sont colinéaires (produit vectoriel ≈ vecteur nul)
            NK_FORCE_INLINE bool Collinear(const NkVec3T& b) const noexcept {
                return Cross(b).LenSq() < T(NkVectorEpsilon);
            }

            // ── Constantes directionnelles ────────────────────────────────────

            static constexpr NkVec3T Zero()     noexcept { return {};                  }
            static constexpr NkVec3T One()      noexcept { return { T(1), T(1), T(1) }; }
            static constexpr NkVec3T Up()       noexcept { return { T(0), T(1), T(0) }; }
            static constexpr NkVec3T Down()     noexcept { return { T(0), T(-1), T(0) }; }
            static constexpr NkVec3T Left()     noexcept { return { T(-1), T(0), T(0) }; }
            static constexpr NkVec3T Right()    noexcept { return { T(1), T(0), T(0) }; }
            static constexpr NkVec3T Forward()  noexcept { return { T(0), T(0), T(1) }; }
            static constexpr NkVec3T Backward() noexcept { return { T(0), T(0), T(-1) }; }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat("({0}, {1}, {2})", x, y, z);
            }

            friend NkString ToString(const NkVec3T& v) {
                return v.ToString();
            }

            friend std::ostream& operator<<(std::ostream& os, const NkVec3T& v) {
                return os << v.ToString().CStr();
            }

            // ── Swizzle setters ──────────────────────────────────────────────
            NK_FORCE_INLINE NkVec2T<T> xy() const { return NkVec2T<T>(x, y); }
			NK_FORCE_INLINE NkVec2T<T> xz() const { return NkVec2T<T>(x, z); }
			NK_FORCE_INLINE NkVec2T<T> yz() const { return NkVec2T<T>(y, z); }
			NK_FORCE_INLINE NkVec2T<T> yx() const { return NkVec2T<T>(y, x); }
			NK_FORCE_INLINE NkVec2T<T> zx() const { return NkVec2T<T>(z, x); }
			NK_FORCE_INLINE NkVec2T<T> zy() const { return NkVec2T<T>(z, y); }

			NK_FORCE_INLINE void xy(const NkVec2T<T>& v) { x = v.x; y = v.y; }
			NK_FORCE_INLINE void xy(float32 x, float32 y) { this->x = x; this->y = y; }
			NK_FORCE_INLINE void xz(const NkVec2T<T>& v) { x = v.x; z = v.y; }
			NK_FORCE_INLINE void xz(float32 x, float32 z) { this->x = x; this->z = z; }

			NK_FORCE_INLINE void yz(const NkVec2T<T>& v) { y = v.x; z = v.y; }
			NK_FORCE_INLINE void yz(float32 y, float32 z) { this->y = y; this->z = z; }
			NK_FORCE_INLINE void yx(const NkVec2T<T>& v) { y = v.x; x = v.y; }
			NK_FORCE_INLINE void yx(float32 y, float32 x) { this->y = y; this->x = x; }

			NK_FORCE_INLINE void zx(const NkVec2T<T>& v) { z = v.x; x = v.y; }
			NK_FORCE_INLINE void zx(float32 z, float32 x) { this->z = z; this->x = x; }
			NK_FORCE_INLINE void zy(const NkVec2T<T>& v) { z = v.x; y = v.y; }
			NK_FORCE_INLINE void zy(float32 z, float32 y) { this->z = z; this->y = y; }

        }; // struct NkVec3T


        // =====================================================================
        // NkVec4T<T>
        //
        // Vecteur 4D générique. Utilisé pour les coordonnées homogènes,
        // la couleur RGBA et le stockage interne des quaternions.
        // =====================================================================

        template<typename T>
        struct NkVec4T {

            union {
                struct { T x, y, z, w; };
                struct { T r, g, b, a; };
                T data[4];
            };

            // ── Constructeurs ─────────────────────────────────────────────────

            constexpr NkVec4T() noexcept
                : x(T(0)), y(T(0)), z(T(0)), w(T(1)) {}

            constexpr explicit NkVec4T(T scalar) noexcept
                : x(scalar), y(scalar), z(scalar), w(T(1)) {}

            constexpr NkVec4T(T x, T y, T z, T w) noexcept
                : x(x), y(y), z(z), w(w) {}

            constexpr NkVec4T(const NkVec3T<T>& v, T w) noexcept
                : x(v.x), y(v.y), z(v.z), w(w) {}

            constexpr NkVec4T(T x, const NkVec3T<T>& yzw) noexcept
                : x(x), y(yzw.x), z(yzw.y), w(yzw.z) {}

            constexpr explicit NkVec4T(const T* p) noexcept
                : x(p[0]), y(p[1]), z(p[2]), w(p[3]) {}

            NkVec4T(const NkVec4T&) noexcept            = default;
            NkVec4T& operator=(const NkVec4T&) noexcept = default;

            template<typename U>
            constexpr explicit NkVec4T(const NkVec4T<U>& o) noexcept
                : x(static_cast<T>(o.x)), y(static_cast<T>(o.y)),
                  z(static_cast<T>(o.z)), w(static_cast<T>(o.w)) {}

            template<typename U>
            NkVec4T& operator=(const NkVec4T<U>& o) {
                x = static_cast<T>(o.x);
                y = static_cast<T>(o.y);
                z = static_cast<T>(o.z);
                w = static_cast<T>(o.w);
                return *this;
            }

            // ── Accesseurs bruts ──────────────────────────────────────────────

            operator T*()             noexcept { return data; }
            operator const T*() const noexcept { return data; }

            template<typename U>
            operator NkVec4T<U>()             noexcept { return NkVec4T<U>(x, y, z, w); }
            template<typename U>
            operator const NkVec4T<U>() const noexcept { return NkVec4T<U>(x, y, z, w); }

            NK_FORCE_INLINE T&       operator[](size_t i)       noexcept { return data[i < 4 ? i : 3]; }
            NK_FORCE_INLINE const T& operator[](size_t i) const noexcept { return data[i < 4 ? i : 3]; }

            // ── Arithmétique ──────────────────────────────────────────────────

            NK_FORCE_INLINE NkVec4T operator-() const noexcept { return { -x, -y, -z, -w }; }

            NK_FORCE_INLINE NkVec4T operator+(const NkVec4T& o) const noexcept { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
            NK_FORCE_INLINE NkVec4T operator-(const NkVec4T& o) const noexcept { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
            NK_FORCE_INLINE NkVec4T operator*(const NkVec4T& o) const noexcept { return { x * o.x, y * o.y, z * o.z, w * o.w }; }
            NK_FORCE_INLINE NkVec4T operator/(const NkVec4T& o) const noexcept { return { x / o.x, y / o.y, z / o.z, w / o.w }; }
            NK_FORCE_INLINE NkVec4T operator*(T s)             const noexcept { return { x * s,   y * s,   z * s,   w * s   }; }
            NK_FORCE_INLINE NkVec4T operator/(T s)             const noexcept {
                T inv = T(1) / s;
                return { x * inv, y * inv, z * inv, w * inv };
            }

            NK_FORCE_INLINE NkVec4T& operator+=(const NkVec4T& o) noexcept { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
            NK_FORCE_INLINE NkVec4T& operator-=(const NkVec4T& o) noexcept { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
            NK_FORCE_INLINE NkVec4T& operator*=(const NkVec4T& o) noexcept { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
            NK_FORCE_INLINE NkVec4T& operator/=(const NkVec4T& o) noexcept { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }
            NK_FORCE_INLINE NkVec4T& operator*=(T s)             noexcept { x *= s; y *= s; z *= s; w *= s; return *this; }
            NK_FORCE_INLINE NkVec4T& operator/=(T s)             noexcept {
                T inv = T(1) / s;
                x *= inv; y *= inv; z *= inv; w *= inv;
                return *this;
            }

            friend NK_FORCE_INLINE NkVec4T operator*(T s, const NkVec4T& v) noexcept { return v * s; }

            // ── Comparaison ───────────────────────────────────────────────────

            friend NK_FORCE_INLINE bool operator==(const NkVec4T& a, const NkVec4T& b) noexcept {
                if constexpr (traits::NkIsFloatingPoint_v<T>) {
                    return NkFabs(a.x - b.x) <= T(NkVectorEpsilon)
                        && NkFabs(a.y - b.y) <= T(NkVectorEpsilon)
                        && NkFabs(a.z - b.z) <= T(NkVectorEpsilon)
                        && NkFabs(a.w - b.w) <= T(NkVectorEpsilon);
                } else {
                    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
                }
            }

            friend NK_FORCE_INLINE bool operator!=(const NkVec4T& a, const NkVec4T& b) noexcept {
                return !(a == b);
            }

            // ── Produit scalaire / Longueur ───────────────────────────────────

            NK_FORCE_INLINE T Dot(const NkVec4T& o) const noexcept {
                return x * o.x + y * o.y + z * o.z + w * o.w;
            }

            NK_FORCE_INLINE T LenSq() const noexcept {
                return x * x + y * y + z * z + w * w;
            }

            NK_FORCE_INLINE T Len() const noexcept {
                T lsq = LenSq();
                return (lsq < T(NkVectorEpsilon))
                    ? T(0)
                    : static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
            }

            void Normalize() noexcept {
                T lsq = LenSq();
                if (lsq < T(NkVectorEpsilon)) { return; }
                T inv = T(1) / static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
                x *= inv; y *= inv; z *= inv; w *= inv;
            }

            NK_FORCE_INLINE NkVec4T Normalized() const noexcept {
                NkVec4T v(*this);
                v.Normalize();
                return v;
            }

            // ── Constantes ────────────────────────────────────────────────────

            static constexpr NkVec4T Zero() noexcept { return { T(0), T(0), T(0), T(0) }; }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat("({0}, {1}, {2}, {3})", x, y, z, w);
            }

            friend NkString ToString(const NkVec4T& v) {
                return v.ToString();
            }

            friend std::ostream& operator<<(std::ostream& os, const NkVec4T& v) {
                return os << v.ToString().CStr();
            }

            // ── Swizzles ──────────────────────────────────────────────────────

			NK_FORCE_INLINE NkVec3T<T> xyz() const { return NkVec3T<T>(x, y, z); }
			NK_FORCE_INLINE NkVec3T<T> xzy() const { return NkVec3T<T>(x, z, y); }
			NK_FORCE_INLINE NkVec3T<T> xyw() const { return NkVec3T<T>(x, y, w); }
			NK_FORCE_INLINE NkVec3T<T> xwy() const { return NkVec3T<T>(x, w, y); }
			NK_FORCE_INLINE NkVec3T<T> xzw() const { return NkVec3T<T>(x, z, w); }
			NK_FORCE_INLINE NkVec3T<T> xwz() const { return NkVec3T<T>(x, w, z); }

			NK_FORCE_INLINE void xyz(const NkVec3T<T>& v) { x = v.x; y = v.y; z = v.z; }
			NK_FORCE_INLINE void xyz(float32 x, float32 y, float32 z) { this->x = x; this->y = y; this->z = z; }
			NK_FORCE_INLINE void xzy(const NkVec3T<T>& v) { x = v.x; y = v.z; z = v.y; }
			NK_FORCE_INLINE void xzy(float32 x, float32 z, float32 y) { this->x = x; this->y = y;  this->z = z; }
			NK_FORCE_INLINE void xyw(const NkVec3T<T>& v) { this->x = v.x; this->y = v.y;  this->w = v.z; }
			NK_FORCE_INLINE void xyw(float32 x, float32 y, float32 w) { this->x = x; this->y = y;  this->w = w; }
			NK_FORCE_INLINE void xwy(const NkVec3T<T>& v) { this->x = v.x; this->y = v.z;  this->w = v.y; }
			NK_FORCE_INLINE void xwy(float32 x, float32 w, float32 y) { this->x = x; this->y = y;  this->w = w; }
			NK_FORCE_INLINE void xzw(const NkVec3T<T>& v) { this->x = v.x; this->z = v.y;  this->w = v.z; }
			NK_FORCE_INLINE void xzw(float32 x, float32 z, float32 w) { this->x = x; this->z = z;  this->w = w; }
			NK_FORCE_INLINE void xwz(const NkVec3T<T>& v) { this->x = v.x; this->z = v.z;  this->w = v.y; }
			NK_FORCE_INLINE void xwz(float32 x, float32 w, float32 z) { this->x = x; this->z = z;  this->w = w; }

			NK_FORCE_INLINE NkVec3T<T> yxz() const { return NkVec3T<T>(y, x, z); }
			NK_FORCE_INLINE NkVec3T<T> yzx() const { return NkVec3T<T>(y, z, x); }
			NK_FORCE_INLINE NkVec3T<T> yxw() const { return NkVec3T<T>(y, x, w); }
			NK_FORCE_INLINE NkVec3T<T> ywx() const { return NkVec3T<T>(y, w, x); }
			NK_FORCE_INLINE NkVec3T<T> yzw() const { return NkVec3T<T>(y, z, w); }
			NK_FORCE_INLINE NkVec3T<T> ywz() const { return NkVec3T<T>(y, w, z); }

			NK_FORCE_INLINE void yxz(const NkVec3T<T>& v) { this->y = v.x; this->x = v.y; this->z = v.z; }
			NK_FORCE_INLINE void yxz(float32 y, float32 x, float32 z) { this->y = y; this->x = x; this->z = z; }
			NK_FORCE_INLINE void yzx(const NkVec3T<T>& v) { this->y = v.x; this->x = v.z; this->z = v.y; }
			NK_FORCE_INLINE void yzx(float32 y, float32 z, float32 x) { this->y = y; this->x = x;  this->z = z; }
			NK_FORCE_INLINE void yxw(const NkVec3T<T>& v) { this->y = v.x; this->x = v.y;  this->w = v.z; }
			NK_FORCE_INLINE void yxw(float32 y, float32 x, float32 w) { this->y = y; this->x = x;  this->w = w; }
			NK_FORCE_INLINE void ywx(const NkVec3T<T>& v) { this->y = v.x; this->x = v.z;  this->w = v.y; }
			NK_FORCE_INLINE void ywx(float32 y, float32 w, float32 x) { this->y = y; this->x = x;  this->w = w; }
			NK_FORCE_INLINE void yzw(const NkVec3T<T>& v) { this->y = v.x; this->z = v.y;  this->w = v.z; }
			NK_FORCE_INLINE void yzw(float32 y, float32 z, float32 w) { this->y = y; this->z = z;  this->w = w; }
			NK_FORCE_INLINE void ywz(const NkVec3T<T>& v) { this->y = v.x; this->z = v.z;  this->w = v.y; }
			NK_FORCE_INLINE void ywz(float32 y, float32 w, float32 z) { this->y = y; this->z = z;  this->w = w; }

			NK_FORCE_INLINE NkVec3T<T> zxy() const { return NkVec3T<T>(z, x, y); }
			NK_FORCE_INLINE NkVec3T<T> zyx() const { return NkVec3T<T>(z, y, x); }
			NK_FORCE_INLINE NkVec3T<T> zxw() const { return NkVec3T<T>(z, x, w); }
			NK_FORCE_INLINE NkVec3T<T> zwx() const { return NkVec3T<T>(z, w, x); }
			NK_FORCE_INLINE NkVec3T<T> zyw() const { return NkVec3T<T>(z, y, w); }
			NK_FORCE_INLINE NkVec3T<T> zwy() const { return NkVec3T<T>(z, w, y); }

			NK_FORCE_INLINE void zxy(const NkVec3T<T>& v) { this->z = v.x; this->x = v.y; this->y = v.z; }
			NK_FORCE_INLINE void zxy(float32 z, float32 x, float32 y) { this->z = z; this->x = x; this->y = y; }
			NK_FORCE_INLINE void zyx(const NkVec3T<T>& v) { this->z = v.x; this->x = v.z; this->y = v.y; }
			NK_FORCE_INLINE void zyx(float32 z, float32 y, float32 x) { this->z = z; this->x = x;  this->y = y; }
			NK_FORCE_INLINE void zxw(const NkVec3T<T>& v) { this->z = v.x; this->x = v.y;  this->w = v.z; }
			NK_FORCE_INLINE void zxw(float32 z, float32 x, float32 w) { this->z = z; this->x = x;  this->w = w; }
			NK_FORCE_INLINE void zwx(const NkVec3T<T>& v) { this->z = v.x; this->x = v.z;  this->w = v.y; }
			NK_FORCE_INLINE void zwx(float32 z, float32 w, float32 x) { this->z = z; this->x = x;  this->w = w; }
			NK_FORCE_INLINE void zyw(const NkVec3T<T>& v) { this->z = v.x; this->y = v.y;  this->w = v.z; }
			NK_FORCE_INLINE void zyw(float32 z, float32 y, float32 w) { this->z = z; this->y = y;  this->w = w; }
			NK_FORCE_INLINE void zwy(const NkVec3T<T>& v) { this->z = v.x; this->y = v.z;  this->w = v.y; }
			NK_FORCE_INLINE void zwy(float32 z, float32 w, float32 y) { this->z = z; this->y = y;  this->w = w; }

			NK_FORCE_INLINE NkVec3T<T> wxy() const { return NkVec3T<T>(w, x, y); }
			NK_FORCE_INLINE NkVec3T<T> wyx() const { return NkVec3T<T>(w, y, x); }
			NK_FORCE_INLINE NkVec3T<T> wxz() const { return NkVec3T<T>(w, x, z); }
			NK_FORCE_INLINE NkVec3T<T> wzx() const { return NkVec3T<T>(w, z, x); }
			NK_FORCE_INLINE NkVec3T<T> wyz() const { return NkVec3T<T>(w, y, z); }
			NK_FORCE_INLINE NkVec3T<T> wzy() const { return NkVec3T<T>(w, z, y); }

			NK_FORCE_INLINE void wxy(const NkVec3T<T>& v) { this->w = v.x; this->x = v.y; this->y = v.z; }
			NK_FORCE_INLINE void wxy(float32 w, float32 x, float32 y) { this->w = w; this->x = x; this->y = y; }
			NK_FORCE_INLINE void wyx(const NkVec3T<T>& v) { this->w = v.x; this->x = v.z; this->y = v.y; }
			NK_FORCE_INLINE void wyx(float32 w, float32 y, float32 x) { this->w = w; this->x = x;  this->y = y; }
			NK_FORCE_INLINE void wxz(const NkVec3T<T>& v) { this->w = v.x; this->x = v.y;  this->z = v.z; }
			NK_FORCE_INLINE void wxz(float32 w, float32 x, float32 z) { this->w = w; this->x = x;  this->z = z; }
			NK_FORCE_INLINE void wzx(const NkVec3T<T>& v) { this->w = v.x; this->x = v.z;  this->z = v.y; }
			NK_FORCE_INLINE void wzx(float32 w, float32 z, float32 x) { this->w = w; this->x = x;  this->z = z; }
			NK_FORCE_INLINE void wyz(const NkVec3T<T>& v) { this->w = v.x; this->y = v.y;  this->z = v.z; }
			NK_FORCE_INLINE void wyz(float32 w, float32 y, float32 z) { this->w = w; this->y = y;  this->z = z; }
			NK_FORCE_INLINE void wzy(const NkVec3T<T>& v) { this->w = v.x; this->y = v.z;  this->z = v.y; }
			NK_FORCE_INLINE void wzy(float32 w, float32 z, float32 y) { this->w = w; this->y = y;  this->z = z; }
        }; // struct NkVec4T


        // =====================================================================
        // Aliases de types — f=float32, d=float64, i=int32, u=uint32
        // =====================================================================

        using NkVector2f = NkVec2T<float32>;
        using NkVector2d = NkVec2T<float64>;
        using NkVector2i = NkVec2T<int32>;
        using NkVector2u = NkVec2T<uint32>;

        using NkVector3f = NkVec3T<float32>;
        using NkVector3d = NkVec3T<float64>;
        using NkVector3i = NkVec3T<int32>;
        using NkVector3u = NkVec3T<uint32>;

        using NkVector4f = NkVec4T<float32>;
        using NkVector4d = NkVec4T<float64>;
        using NkVector4i = NkVec4T<int32>;
        using NkVector4u = NkVec4T<uint32>;

        // Alias courts
        using NkVec2f = NkVector2f;
        using NkVec2d = NkVector2d;
        using NkVec2i = NkVector2i;
        using NkVec2u = NkVector2u;

        using NkVec3f = NkVector3f;
        using NkVec3d = NkVector3d;
        using NkVec3i = NkVector3i;
        using NkVec3u = NkVector3u;

        using NkVec4f = NkVector4f;
        using NkVec4d = NkVector4d;
        using NkVec4i = NkVector4i;
        using NkVec4u = NkVector4u;

        // Alias legacy
        using NkVector2  = NkVector2f;
        using NkVector3  = NkVector3f;
        using NkVector4  = NkVector4f;
        using NkVec2     = NkVector2f;
        using NkVec3     = NkVector3f;
        using NkVec4     = NkVector4f;

    } // namespace math


    // =========================================================================
    // NkToString — spécialisations pour les 3 templates de vecteurs
    // =========================================================================

    template<typename T>
    struct NkToString<math::NkVec2T<T>> {
        static NkString Convert(const math::NkVec2T<T>& v, const NkFormatProps& props) {
            NkString s = NkFormat("({0}, {1})", v.x, v.y);
            return NkApplyFormatProps(s, props);
        }
    };

    template<typename T>
    struct NkToString<math::NkVec3T<T>> {
        static NkString Convert(const math::NkVec3T<T>& v, const NkFormatProps& props) {
            NkString s = NkFormat("({0}, {1}, {2})", v.x, v.y, v.z);
            return NkApplyFormatProps(s, props);
        }
    };

    template<typename T>
    struct NkToString<math::NkVec4T<T>> {
        static NkString Convert(const math::NkVec4T<T>& v, const NkFormatProps& props) {
            NkString s = NkFormat("({0}, {1}, {2}, {3})", v.x, v.y, v.z, v.w);
            return NkApplyFormatProps(s, props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_VECTOR_H__