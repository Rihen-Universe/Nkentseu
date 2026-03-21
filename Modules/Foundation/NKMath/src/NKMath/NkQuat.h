//
// NkQuat.h
// Quaternion unitaire générique — variantes float32 (NkQuatf) et float64 (NkQuatd).
//
// ── Représentation ────────────────────────────────────────────────────────────
//
//   q = (x, y, z, w) = (v, s) où v = partie vectorielle, s = partie scalaire
//   Convention : w est le scalaire (comme XNA, Unity, GLM).
//
// ── Algorithmes implémentés ───────────────────────────────────────────────────
//
//   Rotation de vecteur : formule de Rodrigues optimisée (15 multiplications)
//     v' = 2(v·q_v)q_v + (s²-|q_v|²)v + 2s(q_v×v)
//   source: Vince (2011) "Quaternions for Computer Graphics"
//
//   Produit Hamilton : (this ⊗ r)
//     Ordre convention : r appliqué APRÈS this (comme GLM).
//
//   SLerp : via exponentiation q*(q⁻¹*r)^t (Shoemake 1985)
//     - Chemin court garanti : si Dot < 0, on prend -end.
//     - Fallback NLerp si les quaternions sont quasi-identiques.
//
//   operator^ (exponentiation) :
//     q^t = (sin(acos(w)*t)/sin(acos(w))) * v_n + cos(acos(w)*t)
//     - w clampé dans [-1,1] avant acos pour éviter les NaN.
//
//   LookAt : deux rotations successives from-to (forward puis up).
//
//   Conversion → NkEulerAngle : extraction directe depuis le quaternion
//     normalisé avec détection du pôle de Gimbal (singularité ±90° Yaw).
//
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_QUATERNION_H__
#define __NKENTSEU_QUATERNION_H__

#include "NKMath/NkLegacySystem.h"
#include "NkVec.h"
#include "NkMat.h"
#include "NkEulerAngle.h"

namespace nkentseu {

    namespace math {

        // =====================================================================
        // NkQuatT<T>
        //
        // Quaternion unitaire générique.
        // En production, on travaille toujours avec des quaternions normalisés.
        // Inverse() == Conjugate() est donc valide et plus rapide.
        // =====================================================================

        template<typename T>
        struct NkQuatT {

                union {
                    struct { T x, y, z, w; };

                    struct {
                        NkVec3T<T> vector; ///< Partie vectorielle (x, y, z)
                        T         scalar; ///< Partie scalaire (w)
                    };

                    NkVec4T<T> quat; ///< Accès NkVec4T complet
                    T data[4];
                };

                // ── Constructeurs ─────────────────────────────────────────────────

                /// Quaternion identité par défaut
                NkQuatT() noexcept
                    : x(T(0)), y(T(0)), z(T(0)), w(T(1)) {}

                /// Construit un quaternion rempli (identity=true → w=val, xyz=0)
                explicit NkQuatT(T val, bool identity = true) noexcept
                    : w(val)
                    , x(identity ? T(0) : val)
                    , y(identity ? T(0) : val)
                    , z(identity ? T(0) : val) {}

                /// Constructeur composant par composant
                NkQuatT(T x, T y, T z, T w) noexcept
                    : x(x), y(y), z(z), w(w) {}

                /// Construit depuis un angle-axe
                NkQuatT(const NkAngle& angle, const NkVec3T<T>& axis) noexcept;

                /// Construit depuis des angles d'Euler (convention YXZ)
                NkQuatT(const NkEulerAngle& euler) noexcept;

                /// Construit depuis partie vectorielle + scalaire
                NkQuatT(const NkVec3T<T>& vec, T sc) noexcept
                    : vector(vec), scalar(sc) {}

                /// Construit depuis une matrice de rotation
                explicit NkQuatT(const NkMat4T<T>& mat) noexcept;

                /// Construit depuis NkVec4T (x,y,z,w)
                explicit NkQuatT(const NkVec4T<T>& v) noexcept
                    : quat(v) {}

                /// Rotation minimale de from vers to
                NkQuatT(const NkVec3T<T>& from, const NkVec3T<T>& to) noexcept;

                NkQuatT(const NkQuatT&) noexcept            = default;
                NkQuatT(NkQuatT&&) noexcept                 = default;
                NkQuatT& operator=(const NkQuatT&) noexcept = default;
                NkQuatT& operator=(NkQuatT&&) noexcept      = default;

                // ── Accesseurs bruts ──────────────────────────────────────────────

                operator T*()             noexcept { return data; }
                operator const T*() const noexcept { return data; }

                NK_FORCE_INLINE T&       operator[](size_t i)       noexcept { return data[i & 3]; }
                NK_FORCE_INLINE const T& operator[](size_t i) const noexcept { return data[i & 3]; }

                // ── Arithmétique ──────────────────────────────────────────────────

                NK_FORCE_INLINE NkQuatT operator+(const NkQuatT& r) const noexcept {
                    return { x + r.x, y + r.y, z + r.z, w + r.w };
                }

                NK_FORCE_INLINE NkQuatT operator-(const NkQuatT& r) const noexcept {
                    return { x - r.x, y - r.y, z - r.z, w - r.w };
                }

                /// Produit de Hamilton (this ⊗ r) — r est appliqué APRÈS this
                NkQuatT operator*(const NkQuatT& r) const noexcept;

                NK_FORCE_INLINE NkQuatT operator*(T s) const noexcept {
                    return { x * s, y * s, z * s, w * s };
                }

                NK_FORCE_INLINE NkQuatT operator/(T s) const noexcept {
                    T inv = T(1) / s;
                    return { x * inv, y * inv, z * inv, w * inv };
                }

                NK_FORCE_INLINE NkQuatT& operator+=(const NkQuatT& r) noexcept { *this = *this + r; return *this; }
                NK_FORCE_INLINE NkQuatT& operator-=(const NkQuatT& r) noexcept { *this = *this - r; return *this; }
                NK_FORCE_INLINE NkQuatT& operator*=(const NkQuatT& r) noexcept { *this = *this * r; return *this; }
                NK_FORCE_INLINE NkQuatT& operator*=(T s)              noexcept { *this = *this * s; return *this; }

                // ── Rotation de vecteur ───────────────────────────────────────────

                /// Applique la rotation à v (formule de Rodrigues optimisée, 15 mul)
                NkVec3T<T> operator*(const NkVec3T<T>& v) const noexcept;

                friend NK_FORCE_INLINE NkQuatT    operator*(T s, const NkQuatT& q) noexcept { return q * s; }
                friend NK_FORCE_INLINE NkVec3T<T> operator*(const NkVec3T<T>& v, const NkQuatT& q) noexcept { return q * v; }

                // ── Exponentiation et opérateurs spéciaux ────────────────────────

                /// q^t — exponentiation scalaire, utilisé par SLerp
                friend NkQuatT operator^(const NkQuatT& q, T t) noexcept;

                friend NK_FORCE_INLINE bool operator==(const NkQuatT& a, const NkQuatT& b) noexcept {
                    return a.quat == b.quat;
                }

                friend NK_FORCE_INLINE bool operator!=(const NkQuatT& a, const NkQuatT& b) noexcept {
                    return !(a == b);
                }

                // ── Conversions ───────────────────────────────────────────────────

                /// Conversion vers matrice de rotation 4×4
                explicit operator NkMat4T<T>()   const noexcept;

                /// Conversion vers angles d'Euler avec gestion du gimbal lock
                explicit operator NkEulerAngle() const noexcept;

                // ── Requêtes ──────────────────────────────────────────────────────

                NK_FORCE_INLINE T Dot(const NkQuatT& r) const noexcept {
                    return x * r.x + y * r.y + z * r.z + w * r.w;
                }

                NK_FORCE_INLINE T LenSq() const noexcept {
                    return x * x + y * y + z * z + w * w;
                }

                NK_FORCE_INLINE T Len() const noexcept {
                    T lsq = LenSq();
                    return (lsq < T(NkQuatEpsilon))
                        ? T(0)
                        : static_cast<T>(NkSqrt(static_cast<float32>(lsq)));
                }

                NK_FORCE_INLINE bool IsNormalized() const noexcept {
                    return NkFabs(static_cast<float32>(LenSq()) - 1.0f)
                        < 2.0f * static_cast<float32>(NkEpsilon);
                }

                /// Détecte le pôle de Gimbal : +1=Nord, -1=Sud, 0=normal
                int GimbalPole() const noexcept;

                /// Angle de rotation (en radians internes, retourné comme NkAngle)
                NkAngle   Angle() const noexcept;

                /// Axe de rotation normalisé
                NkVec3T<T> Axis()  const noexcept;

                // ── Opérations non-mutantes ───────────────────────────────────────

                void    Normalize() noexcept;
                NkQuatT Normalized() const noexcept { NkQuatT q(*this); q.Normalize(); return q; }

                /// Conjugué (inverse de partie vectorielle)
                NK_FORCE_INLINE NkQuatT Conjugate() const noexcept {
                    return { -x, -y, -z, w };
                }

                /// Inverse == Conjugué pour un quaternion unitaire (hypothèse production)
                NK_FORCE_INLINE NkQuatT Inverse() const noexcept {
                    return Conjugate();
                }

                // ── Interpolation ─────────────────────────────────────────────────

                /// Mix linéaire (non normalisé)
                NK_FORCE_INLINE NkQuatT Mix(const NkQuatT& to, T t) const noexcept {
                    return (*this) * (T(1) - t) + to * t;
                }

                /// Lerp = Mix (alias sémantique)
                NK_FORCE_INLINE NkQuatT Lerp(const NkQuatT& to, T t) const noexcept {
                    return Mix(to, t);
                }

                /// NLerp : Lerp normalisé — rapide, non-constant en vitesse
                NK_FORCE_INLINE NkQuatT NLerp(const NkQuatT& to, T t) const noexcept {
                    return Mix(to, t).Normalized();
                }

                /// SLerp (Shoemake 1985) — chemin constant en vitesse sur la sphère unitaire
                NkQuatT SLerp(const NkQuatT& to, T t) const noexcept;

                // ── Directions locales ────────────────────────────────────────────

                NK_FORCE_INLINE NkVec3T<T> Forward()  const noexcept { return (*this) * NkVec3T<T>( T(0), T(0),  T(1)); }
                NK_FORCE_INLINE NkVec3T<T> Back()     const noexcept { return (*this) * NkVec3T<T>( T(0), T(0), T(-1)); }
                NK_FORCE_INLINE NkVec3T<T> Up()       const noexcept { return (*this) * NkVec3T<T>( T(0), T(1),  T(0)); }
                NK_FORCE_INLINE NkVec3T<T> Down()     const noexcept { return (*this) * NkVec3T<T>( T(0),T(-1),  T(0)); }
                NK_FORCE_INLINE NkVec3T<T> Right()    const noexcept { return (*this) * NkVec3T<T>( T(1), T(0),  T(0)); }
                NK_FORCE_INLINE NkVec3T<T> Left()     const noexcept { return (*this) * NkVec3T<T>(T(-1), T(0),  T(0)); }

                // ── Fabriques statiques ───────────────────────────────────────────

                static NK_FORCE_INLINE NkQuatT Identity() noexcept {
                    return { T(0), T(0), T(0), T(1) };
                }

                static NkQuatT RotateX(const NkAngle& pitch) noexcept;
                static NkQuatT RotateY(const NkAngle& yaw)   noexcept;
                static NkQuatT RotateZ(const NkAngle& roll)  noexcept;

                /// LookAt depuis une direction + un vecteur up de référence
                static NkQuatT LookAt(const NkVec3T<T>& direction, const NkVec3T<T>& up) noexcept;

                /// LookAt depuis position vers target
                static NkQuatT LookAt(const NkVec3T<T>& position,
                                    const NkVec3T<T>& target,
                                    const NkVec3T<T>& up) noexcept;

                /// Quaternion de réflexion par rapport au plan de normale n
                static NkQuatT Reflection(const NkVec3T<T>& normal) noexcept;

                // ── I/O ───────────────────────────────────────────────────────────

                NkString ToString() const {
                    return NkFormat("[{0}, {1}, {2}; {3}]", x, y, z, w);
                }

                friend NkString ToString(const NkQuatT& q) {
                    return q.ToString();
                }

                friend std::ostream& operator<<(std::ostream& os, const NkQuatT& q) {
                    return os << q.ToString().CStr();
                }

            private:

                /// Remet à zéro les composantes inférieures à epsilon (appelé après calcul)
                void ClampEpsilon() noexcept;

        }; // struct NkQuatT


        // =====================================================================
        // Implémentations des méthodes template
        // (toutes dans le header car template)
        // =====================================================================

        // ── ClampEpsilon ──────────────────────────────────────────────────────

        template<typename T>
        void NkQuatT<T>::ClampEpsilon() noexcept {
            const T eps = static_cast<T>(NkEpsilon);
            if (NkFabs(static_cast<float32>(x)) <= static_cast<float32>(eps)) { x = T(0); }
            if (NkFabs(static_cast<float32>(y)) <= static_cast<float32>(eps)) { y = T(0); }
            if (NkFabs(static_cast<float32>(z)) <= static_cast<float32>(eps)) { z = T(0); }
            if (NkFabs(static_cast<float32>(w)) <= static_cast<float32>(eps)) { w = T(0); }
        }

        // ── Constructeur angle-axe ────────────────────────────────────────────

        template<typename T>
        NkQuatT<T>::NkQuatT(const NkAngle& angle, const NkVec3T<T>& axis) noexcept {
            NkVec3T<T> n = axis.Normalized();
            if (n.Len() == T(0)) { *this = Identity(); return; }
            float32 half = angle.Rad() * 0.5f;
            vector = n * static_cast<T>(NkSin(half));
            scalar = static_cast<T>(NkCos(half));
            ClampEpsilon();
        }

        // ── Constructeur Euler ────────────────────────────────────────────────

        template<typename T>
        NkQuatT<T>::NkQuatT(const NkEulerAngle& e) noexcept {
            // Convention YXZ : yaw d'abord, puis pitch, puis roll
            float32 cy = NkCos(e.yaw.Rad()   * 0.5f),  sy = NkSin(e.yaw.Rad()   * 0.5f);
            float32 cx = NkCos(e.pitch.Rad() * 0.5f),  sx = NkSin(e.pitch.Rad() * 0.5f);
            float32 cz = NkCos(e.roll.Rad()  * 0.5f),  sz = NkSin(e.roll.Rad()  * 0.5f);

            w = static_cast<T>(cz * cy * cx + sz * sy * sx);
            x = static_cast<T>(cz * cy * sx - sz * sy * cx);
            y = static_cast<T>(cz * sy * cx + sz * cy * sx);
            z = static_cast<T>(sz * cy * cx - cz * sy * sx);
            ClampEpsilon();
        }

        // ── Constructeur depuis matrice ───────────────────────────────────────

        template<typename T>
        NkQuatT<T>::NkQuatT(const NkMat4T<T>& mat) noexcept {
            NkVec3T<T> up      = mat.up.xyz().Normalized();
            NkVec3T<T> forward = mat.forward.xyz().Normalized();
            NkVec3T<T> right   = up.Cross(forward);
            up = forward.Cross(right);
            *this = LookAt(forward, up);
            ClampEpsilon();
        }

        // ── Constructeur from-to ──────────────────────────────────────────────

        template<typename T>
        NkQuatT<T>::NkQuatT(const NkVec3T<T>& from, const NkVec3T<T>& to) noexcept {
            NkVec3T<T> f = from.Normalized();
            NkVec3T<T> t = to.Normalized();

            if (f == t) { *this = Identity(); return; }

            if (f == t * T(-1)) {
                // Rotation de 180° — axe arbitraire perpendiculaire à f
                NkVec3T<T> ortho = { T(1), T(0), T(0) };
                if (NkFabs(static_cast<float32>(f.y)) < NkFabs(static_cast<float32>(f.x))) {
                    ortho = { T(0), T(1), T(0) };
                }
                if (NkFabs(static_cast<float32>(f.z)) < NkFabs(static_cast<float32>(f.y))
                 && NkFabs(static_cast<float32>(f.z)) < NkFabs(static_cast<float32>(f.x))) {
                    ortho = { T(0), T(0), T(1) };
                }
                vector = f.Cross(ortho).Normalized();
                scalar = T(0);
            } else {
                NkVec3T<T> half = (f + t).Normalized();
                vector = f.Cross(half);
                scalar = f.Dot(half);
            }
            ClampEpsilon();
        }

        // ── Produit de Hamilton ───────────────────────────────────────────────

        template<typename T>
        NkQuatT<T> NkQuatT<T>::operator*(const NkQuatT& r) const noexcept {
            return {
                 r.x * w + r.y * z - r.z * y + r.w * x,
                -r.x * z + r.y * w + r.z * x + r.w * y,
                 r.x * y - r.y * x + r.z * w + r.w * z,
                -r.x * x - r.y * y - r.z * z + r.w * w
            };
        }

        // ── Rotation de vecteur (Rodrigues optimisé) ──────────────────────────

        template<typename T>
        NkVec3T<T> NkQuatT<T>::operator*(const NkVec3T<T>& v) const noexcept {
            // v' = 2(q_v·v)q_v + (s²-|q_v|²)v + 2s(q_v×v)
            // 15 multiplications — plus rapide que q*[v,0]*q⁻¹ (28 mul)
            return vector * (T(2) * vector.Dot(v))
                 + v      * (scalar * scalar - vector.Dot(vector))
                 + vector.Cross(v) * (T(2) * scalar);
        }

        // ── Exponentiation q^t ────────────────────────────────────────────────

        template<typename T>
        NkQuatT<T> operator^(const NkQuatT<T>& q, T t) noexcept {
            // Clamp w dans [-1,1] pour protéger acos contre les erreurs fp
            float32 safeW = NkClamp(static_cast<float32>(q.scalar), -1.0f, 1.0f);
            float32 half  = NkAcos(safeW) * static_cast<float32>(t);
            NkVec3T<T> ax  = q.vector.Normalized();
            return {
                ax.x * static_cast<T>(NkSin(half)),
                ax.y * static_cast<T>(NkSin(half)),
                ax.z * static_cast<T>(NkSin(half)),
                static_cast<T>(NkCos(half))
            };
        }

        // ── Conversion → NkMat4T ───────────────────────────────────────────────

        template<typename T>
        NkQuatT<T>::operator NkMat4T<T>() const noexcept {
            T xx = x * x, yy = y * y, zz = z * z;
            T xy = x * y, xz = x * z, yz = y * z;
            T wx = w * x, wy = w * y, wz = w * z;

            return NkMat4T<T>(
                // Colonne 0,         Colonne 1,         Colonne 2,         Colonne 3
                T(1) - T(2)*(yy+zz), T(2)*(xy+wz),      T(2)*(xz-wy),      T(0),
                T(2)*(xy-wz),        T(1) - T(2)*(xx+zz),T(2)*(yz+wx),      T(0),
                T(2)*(xz+wy),        T(2)*(yz-wx),       T(1) - T(2)*(xx+yy),T(0),
                T(0),                T(0),               T(0),              T(1)
            );
        }

        // ── Conversion → NkEulerAngle ─────────────────────────────────────────

        template<typename T>
        NkQuatT<T>::operator NkEulerAngle() const noexcept {
            NkQuatT<T> q = Normalized();
            NkEulerAngle angles;

            // Détection du pôle de Gimbal
            float32 gimbalTest = static_cast<float32>(q.y * q.x + q.z * q.w);

            if (gimbalTest > 0.499f) {
                // Singularité Nord : Yaw = +90°
                angles.yaw   = NkAngle::FromRad( static_cast<float32>(NkPis2));
                angles.pitch = NkAngle::FromRad( 2.0f * NkAtan2(
                    static_cast<float32>(q.x), static_cast<float32>(q.w)));
                angles.roll  = NkAngle(0.0f);
            } else if (gimbalTest < -0.499f) {
                // Singularité Sud : Yaw = −90°
                angles.yaw   = NkAngle::FromRad(-static_cast<float32>(NkPis2));
                angles.pitch = NkAngle::FromRad(-2.0f * NkAtan2(
                    static_cast<float32>(q.x), static_cast<float32>(q.w)));
                angles.roll  = NkAngle(0.0f);
            } else {
                // Cas général
                float32 sinr = 2.0f * (static_cast<float32>(q.w * q.x) + static_cast<float32>(q.y * q.z));
                float32 cosr = 1.0f - 2.0f * (static_cast<float32>(q.x * q.x) + static_cast<float32>(q.y * q.y));
                angles.pitch = NkAngle::FromRad(NkAtan2(sinr, cosr));

                float32 sinp = NkClamp(2.0f * static_cast<float32>(q.w * q.y - q.z * q.x), -1.0f, 1.0f);
                angles.yaw   = NkAngle::FromRad(NkAsin(sinp));

                float32 siny = 2.0f * (static_cast<float32>(q.w * q.z) + static_cast<float32>(q.x * q.y));
                float32 cosy = 1.0f - 2.0f * (static_cast<float32>(q.y * q.y) + static_cast<float32>(q.z * q.z));
                angles.roll  = NkAngle::FromRad(NkAtan2(siny, cosy));
            }

            return angles;
        }

        // ── Requêtes ──────────────────────────────────────────────────────────

        template<typename T>
        int NkQuatT<T>::GimbalPole() const noexcept {
            float32 t = static_cast<float32>(y * x + z * w);
            return (t > 0.499f) ? 1 : (t < -0.499f) ? -1 : 0;
        }

        template<typename T>
        NkAngle NkQuatT<T>::Angle() const noexcept {
            float32 safeW = NkClamp(static_cast<float32>(w), -1.0f, 1.0f);
            return NkAngle::FromRad(2.0f * NkAcos(NkFabs(safeW)));
        }

        template<typename T>
        NkVec3T<T> NkQuatT<T>::Axis() const noexcept {
            float32 s2 = 1.0f - static_cast<float32>(w * w);
            if (s2 < static_cast<float32>(NkEpsilon)) {
                return { T(0), T(1), T(0) }; // Quaternion identité → axe Y arbitraire
            }
            float32 invS = 1.0f / NkSqrt(s2);
            return {
                x * static_cast<T>(invS),
                y * static_cast<T>(invS),
                z * static_cast<T>(invS)
            };
        }

        // ── Normalisation ─────────────────────────────────────────────────────

        template<typename T>
        void NkQuatT<T>::Normalize() noexcept {
            float32 lsq = static_cast<float32>(LenSq());
            if (lsq < static_cast<float32>(NkQuatEpsilon)) { return; }
            T inv = T(1) / static_cast<T>(NkSqrt(lsq));
            x *= inv; y *= inv; z *= inv; w *= inv;
        }

        // ── SLerp ─────────────────────────────────────────────────────────────

        template<typename T>
        NkQuatT<T> NkQuatT<T>::SLerp(const NkQuatT<T>& endIn, T t) const noexcept {
            // Garantit le chemin court : si dot < 0, on prend -end
            NkQuatT<T> end = endIn;
            float32 dotVal = static_cast<float32>(Dot(end));
            if (dotVal < 0.0f) {
                end   = { -end.x, -end.y, -end.z, -end.w };
                dotVal = -dotVal;
            }
            // Si les deux quaternions sont quasi-identiques → NLerp plus stable
            if (dotVal > 1.0f - static_cast<float32>(NkQuatEpsilon)) {
                return NLerp(end, t);
            }
            // Exponentiation : q * (q⁻¹ * end)^t
            NkQuatT<T> delta = Inverse() * end;
            return ((delta ^ t) * (*this)).Normalized();
        }

        // ── Fabriques statiques ───────────────────────────────────────────────

        template<typename T>
        NkQuatT<T> NkQuatT<T>::RotateX(const NkAngle& pitch) noexcept {
            float32 h = pitch.Rad() * 0.5f;
            return { static_cast<T>(NkSin(h)), T(0), T(0), static_cast<T>(NkCos(h)) };
        }

        template<typename T>
        NkQuatT<T> NkQuatT<T>::RotateY(const NkAngle& yaw) noexcept {
            float32 h = yaw.Rad() * 0.5f;
            return { T(0), static_cast<T>(NkSin(h)), T(0), static_cast<T>(NkCos(h)) };
        }

        template<typename T>
        NkQuatT<T> NkQuatT<T>::RotateZ(const NkAngle& roll) noexcept {
            float32 h = roll.Rad() * 0.5f;
            return { T(0), T(0), static_cast<T>(NkSin(h)), static_cast<T>(NkCos(h)) };
        }

        template<typename T>
        NkQuatT<T> NkQuatT<T>::LookAt(const NkVec3T<T>& direction,
                                       const NkVec3T<T>& up) noexcept {
            NkVec3T<T> f = direction.Normalized();
            NkVec3T<T> u = up.Normalized();
            NkVec3T<T> r = u.Cross(f);
            u = f.Cross(r);

            // Étape 1 : aligner forward monde (0,0,1) → f
            NkQuatT<T> q1(NkVec3T<T>{ T(0), T(0), T(1) }, f);

            // Étape 2 : tourner l'up obtenu vers l'up désiré
            NkVec3T<T>  objUp = q1 * NkVec3T<T>{ T(0), T(1), T(0) };
            NkQuatT<T> q2(objUp, u);

            return (q1 * q2).Normalized();
        }

        template<typename T>
        NkQuatT<T> NkQuatT<T>::LookAt(const NkVec3T<T>& position,
                                       const NkVec3T<T>& target,
                                       const NkVec3T<T>& up) noexcept {
            return LookAt(target - position, up);
        }

        template<typename T>
        NkQuatT<T> NkQuatT<T>::Reflection(const NkVec3T<T>& normal) noexcept {
            return { normal.Normalized(), T(0) };
        }


        // =====================================================================
        // Aliases de types — f=float32, d=float64
        // =====================================================================

        using NkQuatf       = NkQuatT<float32>;
        using NkQuatd       = NkQuatT<float64>;
        using NkQuaternion  = NkQuatf;  ///< Alias legacy (float32 par défaut)
        using NkQuat        = NkQuatf;
        using NkQuaternionf = NkQuatf;  ///< Alias legacy (float32 par défaut)
        using NkQuaterniond = NkQuatd;  ///< Alias legacy (float32 par défaut)
        using NkQuatd       = NkQuatd;

    } // namespace math


    // =========================================================================
    // NkToString<NkQuatT>
    // =========================================================================

    template<typename T>
    struct NkToString<math::NkQuatT<T>> {
        static NkString Convert(const math::NkQuatT<T>& q, const NkFormatProps& props) {
            return NkApplyFormatProps(q.ToString(), props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_QUATERNION_H__