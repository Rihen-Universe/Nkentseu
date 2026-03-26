//
// NkMatrix.h
// Matrices 2x2, 3x3 et 4x4 génériques — variantes f=float32 et d=float64.
//
// ── Convention de stockage ────────────────────────────────────────────────────
//
//   COLUMN-MAJOR (comme OpenGL / Vulkan).
//   mat[col][row] — la colonne 0 est stockée en premier en mémoire.
//
//   Pour DirectX (row-major) ou tout shader qui attend la transposée,
//   utiliser ToRowMajor() avant d'envoyer les données au GPU.
//   L'opération M*v reste valable pour les deux API (c'est le layout mémoire
//   qui diffère, pas la sémantique mathématique).
//
// ── Extraction TRS robuste (NkMat4T) ──────────────────────────────────────────
//
//   L'extraction Euler directe depuis une matrice de rotation souffre du
//   gimbal lock (singularité à ±90° sur l'axe Y/X). Notre Extract() passe
//   par un quaternion intermédiaire (méthode Shoemake 1994) qui est bien
//   conditionné numériquement, puis convertit ce quaternion en Euler avec
//   détection de la singularité via GimbalPole().
//
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_MATRIX_H__
#define __NKENTSEU_MATRIX_H__

#include "NKMath/NkLegacySystem.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkEulerAngle.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

    namespace math {

        // Forward-déclare NkAngle (inclus via NkEulerAngle.h → NkAngle.h)
        class NkAngle;

        // =====================================================================
        // NkMat2T<T>
        //
        // Matrice 2×2 column-major.
        // Layout mémoire : [ m00, m01,  m10, m11 ]
        //                    col0       col1
        // =====================================================================

        template<typename T>
        struct NkMat2T {

            union {
                T data[4]; ///< Accès linéaire column-major

                struct {
                    T m00, m01; ///< Colonne 0
                    T m10, m11; ///< Colonne 1
                };

                NkVec2T<T> col[2]; ///< col[0] = première colonne
            };

            // ── Constructeurs ─────────────────────────────────────────────────

            /// Matrice identité par défaut
            NkMat2T() noexcept {
                col[0] = { T(1), T(0) };
                col[1] = { T(0), T(1) };
            }

            explicit NkMat2T(T diag) noexcept {
                col[0] = { diag, T(0) };
                col[1] = { T(0), diag };
            }

            /// Constructeur colonne par colonne
            NkMat2T(const NkVec2T<T>& c0, const NkVec2T<T>& c1) noexcept {
                col[0] = c0;
                col[1] = c1;
            }

            /// Constructeur élément par élément (ordre column-major)
            NkMat2T(T m00, T m01,
                   T m10, T m11) noexcept
                : m00(m00), m01(m01),
                  m10(m10), m11(m11) {}

            NkMat2T(const NkMat2T&) noexcept            = default;
            NkMat2T& operator=(const NkMat2T&) noexcept = default;

            // ── Accesseurs ────────────────────────────────────────────────────

            /// Retourne un pointeur sur la colonne col (column-major)
            NK_FORCE_INLINE T*       operator[](int c)       noexcept { return &data[c * 2]; }
            NK_FORCE_INLINE const T* operator[](int c) const noexcept { return &data[c * 2]; }

            operator T*()             noexcept { return data; }
            operator const T*() const noexcept { return data; }

            // ── Arithmétique ──────────────────────────────────────────────────

            NkMat2T operator+(const NkMat2T& o) const noexcept {
                NkMat2T r;
                for (int i = 0; i < 4; ++i) { r.data[i] = data[i] + o.data[i]; }
                return r;
            }

            NkMat2T operator-(const NkMat2T& o) const noexcept {
                NkMat2T r;
                for (int i = 0; i < 4; ++i) { r.data[i] = data[i] - o.data[i]; }
                return r;
            }

            /// Produit matriciel
            NkMat2T operator*(const NkMat2T& o) const noexcept {
                return {
                    m00 * o.m00 + m10 * o.m01,
                    m01 * o.m00 + m11 * o.m01,
                    m00 * o.m10 + m10 * o.m11,
                    m01 * o.m10 + m11 * o.m11
                };
            }

            NkMat2T operator*(T s) const noexcept {
                NkMat2T r;
                for (int i = 0; i < 4; ++i) { r.data[i] = data[i] * s; }
                return r;
            }

            /// Transformation d'un vecteur colonne M * v
            NkVec2T<T> operator*(const NkVec2T<T>& v) const noexcept {
                return {
                    m00 * v.x + m10 * v.y,
                    m01 * v.x + m11 * v.y
                };
            }

            NkMat2T& operator+=(const NkMat2T& o) noexcept { *this = *this + o; return *this; }
            NkMat2T& operator-=(const NkMat2T& o) noexcept { *this = *this - o; return *this; }
            NkMat2T& operator*=(const NkMat2T& o) noexcept { *this = *this * o; return *this; }
            NkMat2T& operator*=(T s)             noexcept { *this = *this * s; return *this; }

            friend NkMat2T operator*(T s, const NkMat2T& m) noexcept { return m * s; }

            // ── Comparaison ───────────────────────────────────────────────────

            bool operator==(const NkMat2T& o) const noexcept {
                for (int i = 0; i < 4; ++i) {
                    if (NkFabs(static_cast<float32>(data[i] - o.data[i]))
                            > static_cast<float32>(NkMatrixEpsilon)) { return false; }
                }
                return true;
            }

            bool operator!=(const NkMat2T& o) const noexcept { return !(*this == o); }

            // ── Algèbre linéaire ──────────────────────────────────────────────

            static NkMat2T Identity() noexcept { return {}; }

            NkMat2T Transpose() const noexcept {
                return { m00, m10, m01, m11 };
            }

            T Determinant() const noexcept {
                return m00 * m11 - m10 * m01;
            }

            NkMat2T Inverse() const noexcept {
                T det = Determinant();
                if (NkFabs(static_cast<float32>(det)) < static_cast<float32>(NkMatrixEpsilon)) {
                    return Identity(); // Matrice singulière → identité de secours
                }
                T inv = T(1) / det;
                return {  m11 * inv, -m01 * inv,
                         -m10 * inv,  m00 * inv };
            }

            NK_FORCE_INLINE T Trace() const noexcept { return m00 + m11; }

            // ── Conversion Row-Major (pour DirectX / HLSL) ────────────────────

            /// Retourne les 4 valeurs en row-major dans le tableau out[4]
            void ToRowMajor(T out[4]) const noexcept {
                out[0] = m00; out[1] = m10; // ligne 0
                out[2] = m01; out[3] = m11; // ligne 1
            }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat("NkMat2T[\n  [{0}, {1}]\n  [{2}, {3}]\n]",
                    m00, m10,
                    m01, m11);
            }

            friend NkString ToString(const NkMat2T& m) { return m.ToString(); }

            friend std::ostream& operator<<(std::ostream& os, const NkMat2T& m) {
                return os << m.ToString().CStr();
            }

        }; // struct NkMat2T


        // =====================================================================
        // NkMat3T<T>
        //
        // Matrice 3×3 column-major.
        // Layout mémoire : [ col0.xyz, col1.xyz, col2.xyz ]
        // =====================================================================

        template<typename T>
        struct NkMat3T {

            union {
                T data[9]; ///< Accès linéaire column-major

                struct {
                    T m00, m01, m02; ///< Colonne 0
                    T m10, m11, m12; ///< Colonne 1
                    T m20, m21, m22; ///< Colonne 2
                };

                NkVec3T<T> col[3]; ///< col[i] = i-ème colonne
            };

            // ── Constructeurs ─────────────────────────────────────────────────

            /// Matrice identité par défaut
            NkMat3T() noexcept {
                col[0] = { T(1), T(0), T(0) };
                col[1] = { T(0), T(1), T(0) };
                col[2] = { T(0), T(0), T(1) };
            }

            explicit NkMat3T(T diag) noexcept {
                col[0] = { diag, T(0), T(0) };
                col[1] = { T(0), diag, T(0) };
                col[2] = { T(0), T(0), diag };
            }

            NkMat3T(const NkVec3T<T>& c0,
                   const NkVec3T<T>& c1,
                   const NkVec3T<T>& c2) noexcept {
                col[0] = c0;
                col[1] = c1;
                col[2] = c2;
            }

            /// Constructeur élément par élément (ordre column-major)
            NkMat3T(T m00, T m01, T m02,
                   T m10, T m11, T m12,
                   T m20, T m21, T m22) noexcept
                : m00(m00), m01(m01), m02(m02),
                  m10(m10), m11(m11), m12(m12),
                  m20(m20), m21(m21), m22(m22) {}

            NkMat3T(const NkMat3T&) noexcept            = default;
            NkMat3T& operator=(const NkMat3T&) noexcept = default;

            // ── Accesseurs ────────────────────────────────────────────────────

            NK_FORCE_INLINE T*       operator[](int c)       noexcept { return &data[c * 3]; }
            NK_FORCE_INLINE const T* operator[](int c) const noexcept { return &data[c * 3]; }

            operator T*()             noexcept { return data; }
            operator const T*() const noexcept { return data; }

            // ── Arithmétique ──────────────────────────────────────────────────

            NkMat3T operator+(const NkMat3T& o) const noexcept {
                NkMat3T r;
                for (int i = 0; i < 9; ++i) { r.data[i] = data[i] + o.data[i]; }
                return r;
            }

            NkMat3T operator-(const NkMat3T& o) const noexcept {
                NkMat3T r;
                for (int i = 0; i < 9; ++i) { r.data[i] = data[i] - o.data[i]; }
                return r;
            }

            NkMat3T operator*(const NkMat3T& o) const noexcept {
                NkMat3T r;
                for (int c = 0; c < 3; ++c) {
                    for (int row = 0; row < 3; ++row) {
                        T s = T(0);
                        for (int k = 0; k < 3; ++k) { s += data[k * 3 + row] * o.data[c * 3 + k]; }
                        r.data[c * 3 + row] = s;
                    }
                }
                return r;
            }

            NkMat3T operator*(T s) const noexcept {
                NkMat3T r;
                for (int i = 0; i < 9; ++i) { r.data[i] = data[i] * s; }
                return r;
            }

            NkVec3T<T> operator*(const NkVec3T<T>& v) const noexcept {
                return {
                    m00 * v.x + m10 * v.y + m20 * v.z,
                    m01 * v.x + m11 * v.y + m21 * v.z,
                    m02 * v.x + m12 * v.y + m22 * v.z
                };
            }

            NkMat3T& operator+=(const NkMat3T& o) noexcept { *this = *this + o; return *this; }
            NkMat3T& operator-=(const NkMat3T& o) noexcept { *this = *this - o; return *this; }
            NkMat3T& operator*=(const NkMat3T& o) noexcept { *this = *this * o; return *this; }
            NkMat3T& operator*=(T s)             noexcept { *this = *this * s; return *this; }

            friend NkMat3T operator*(T s, const NkMat3T& m) noexcept { return m * s; }

            // ── Comparaison ───────────────────────────────────────────────────

            bool operator==(const NkMat3T& o) const noexcept {
                for (int i = 0; i < 9; ++i) {
                    if (NkFabs(static_cast<float32>(data[i] - o.data[i]))
                            > static_cast<float32>(NkMatrixEpsilon)) { return false; }
                }
                return true;
            }

            bool operator!=(const NkMat3T& o) const noexcept { return !(*this == o); }

            // ── Algèbre linéaire ──────────────────────────────────────────────

            static NkMat3T Identity() noexcept { return {}; }

            NkMat3T Transpose() const noexcept {
                return {
                    m00, m10, m20,
                    m01, m11, m21,
                    m02, m12, m22
                };
            }

            T Determinant() const noexcept {
                return m00 * (m11 * m22 - m21 * m12)
                     - m10 * (m01 * m22 - m21 * m02)
                     + m20 * (m01 * m12 - m11 * m02);
            }

            NkMat3T Inverse() const noexcept {
                T det = Determinant();
                if (NkFabs(static_cast<float32>(det)) < static_cast<float32>(NkMatrixEpsilon)) {
                    return Identity();
                }
                T inv = T(1) / det;
                NkMat3T r;
                r.m00 =  (m11 * m22 - m21 * m12) * inv;
                r.m01 = -(m01 * m22 - m21 * m02) * inv;
                r.m02 =  (m01 * m12 - m11 * m02) * inv;
                r.m10 = -(m10 * m22 - m20 * m12) * inv;
                r.m11 =  (m00 * m22 - m20 * m02) * inv;
                r.m12 = -(m00 * m12 - m10 * m02) * inv;
                r.m20 =  (m10 * m21 - m20 * m11) * inv;
                r.m21 = -(m00 * m21 - m20 * m01) * inv;
                r.m22 =  (m00 * m11 - m10 * m01) * inv;
                return r;
            }

            NK_FORCE_INLINE T Trace()              const noexcept { return m00 + m11 + m22; }
            NK_FORCE_INLINE NkVec3T<T> Diagonal()   const noexcept { return { m00, m11, m22 }; }

            // ── Conversion Row-Major ──────────────────────────────────────────

            void ToRowMajor(T out[9]) const noexcept {
                // Ligne 0 : col[0][row0], col[1][row0], col[2][row0] ...
                out[0] = m00; out[1] = m10; out[2] = m20;
                out[3] = m01; out[4] = m11; out[5] = m21;
                out[6] = m02; out[7] = m12; out[8] = m22;
            }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat("NkMat3T[\n  [{0}, {1}, {2}]\n  [{3}, {4}, {5}]\n  [{6}, {7}, {8}]\n]",
                    m00, m10, m20,
                    m01, m11, m21,
                    m02, m12, m22);
            }

            friend NkString ToString(const NkMat3T& m) { return m.ToString(); }

            friend std::ostream& operator<<(std::ostream& os, const NkMat3T& m) {
                return os << m.ToString().CStr();
            }

        }; // struct NkMat3T


        // =====================================================================
        // NkMat4T<T>
        //
        // Matrice 4×4 column-major.
        // Layout mémoire : [ right(col0), up(col1), forward(col2), position(col3) ]
        //
        // ── Convention d'utilisation ──────────────────────────────────────────
        //
        //   OpenGL / Vulkan : column-major, vecteur colonne à droite → M * v
        //     glUniformMatrix4fv(..., GL_FALSE, data)  — pas de transposition
        //
        //   DirectX / HLSL  : row-major, vecteur ligne à gauche → v * M
        //     → appeler ToRowMajor() avant l'upload, ou transposer le buffer.
        //     Le calcul CPU reste M*v ; seul le layout mémoire envoyé au shader
        //     change.
        //
        // ── Extraction TRS ────────────────────────────────────────────────────
        //
        //   Extract() extrait Translation, Rotation (en quaternion puis Euler)
        //   et Scale d'une matrice TRS.
        //   La rotation passe par un quaternion intermédiaire (Shoemake) pour
        //   éviter le gimbal lock inhérent à la conversion directe matrice→Euler.
        //
        // =====================================================================

        template<typename T>
        struct NkMat4T {

            union {
                T data[16]; ///< Accès linéaire column-major

                T mat[4][4]; ///< mat[col][row]

                struct {
                    T m00, m01, m02, m03; ///< Colonne 0 (right)
                    T m10, m11, m12, m13; ///< Colonne 1 (up)
                    T m20, m21, m22, m23; ///< Colonne 2 (forward)
                    T m30, m31, m32, m33; ///< Colonne 3 (position / w)
                };

                struct {
                    NkVec4T<T> right;    ///< Colonne 0 — axe X local
                    NkVec4T<T> up;       ///< Colonne 1 — axe Y local
                    NkVec4T<T> forward;  ///< Colonne 2 — axe Z local
                    NkVec4T<T> position; ///< Colonne 3 — translation
                };

                NkVec4T<T> col[4]; ///< col[i] = i-ème colonne
            };

            // ── Constructeurs ─────────────────────────────────────────────────

            /// Matrice nulle par défaut
            NkMat4T() noexcept {
                for (int i = 0; i < 16; ++i) { data[i] = T(0); }
            }

            /// Matrice identité scalée si identity=true, remplie de 'val' sinon
            explicit NkMat4T(T val, bool identity = true) noexcept {
                for (int i = 0; i < 16; ++i) { data[i] = T(0); }
                if (identity) {
                    mat[0][0] = val;
                    mat[1][1] = val;
                    mat[2][2] = val;
                    mat[3][3] = val;
                } else {
                    for (int i = 0; i < 16; ++i) { data[i] = val; }
                }
            }

            /// Constructeur depuis 4 colonnes
            NkMat4T(const NkVec4T<T>& c0,
                   const NkVec4T<T>& c1,
                   const NkVec4T<T>& c2,
                   const NkVec4T<T>& c3) noexcept {
                col[0] = c0;
                col[1] = c1;
                col[2] = c2;
                col[3] = c3;
            }

            /// Constructeur depuis 3 axes (sans translation)
            NkMat4T(const NkVec3T<T>& r,
                   const NkVec3T<T>& u,
                   const NkVec3T<T>& f) noexcept {
                for (int i = 0; i < 16; ++i) { data[i] = T(0); }
                right    = NkVec4T<T>(r, T(0));
                up       = NkVec4T<T>(u, T(0));
                forward  = NkVec4T<T>(f, T(0));
                position = NkVec4T<T>(T(0), T(0), T(0), T(1));
            }

            /// Constructeur depuis 3 axes + translation
            NkMat4T(const NkVec3T<T>& r,
                   const NkVec3T<T>& u,
                   const NkVec3T<T>& f,
                   const NkVec3T<T>& pos) noexcept {
                right    = NkVec4T<T>(r, T(0));
                up       = NkVec4T<T>(u, T(0));
                forward  = NkVec4T<T>(f, T(0));
                position = NkVec4T<T>(pos, T(1));
            }

            /// Constructeur élément par élément (ordre column-major)
            NkMat4T(T m00, T m10, T m20, T m30,
                   T m01, T m11, T m21, T m31,
                   T m02, T m12, T m22, T m32,
                   T m03, T m13, T m23, T m33) noexcept
                : m00(m00), m01(m01), m02(m02), m03(m03),
                  m10(m10), m11(m11), m12(m12), m13(m13),
                  m20(m20), m21(m21), m22(m22), m23(m23),
                  m30(m30), m31(m31), m32(m32), m33(m33) {}

            explicit NkMat4T(const T* vals) noexcept {
                for (int i = 0; i < 16; ++i) { data[i] = vals[i]; }
            }

            NkMat4T(const NkMat4T&) noexcept            = default;
            NkMat4T& operator=(const NkMat4T&) noexcept = default;

            // ── Accesseurs ────────────────────────────────────────────────────

            /// Retourne la colonne c comme tableau de 4 T (column-major)
            NK_FORCE_INLINE T*       operator[](int c)       noexcept { return &data[c * 4]; }
            NK_FORCE_INLINE const T* operator[](int c) const noexcept { return &data[c * 4]; }

            operator T*()             noexcept { return data; }
            operator const T*() const noexcept { return data; }

            // ── Comparaison ───────────────────────────────────────────────────

            bool operator==(const NkMat4T& o) const noexcept {
                for (int i = 0; i < 16; ++i) { if (data[i] != o.data[i]) { return false; } }
                return true;
            }

            bool operator!=(const NkMat4T& o) const noexcept { return !(*this == o); }

            // ── Arithmétique ──────────────────────────────────────────────────

            NkMat4T operator+(const NkMat4T& o) const noexcept {
                NkMat4T r;
                for (int i = 0; i < 16; ++i) { r.data[i] = data[i] + o.data[i]; }
                return r;
            }

            NkMat4T operator-(const NkMat4T& o) const noexcept {
                NkMat4T r;
                for (int i = 0; i < 16; ++i) { r.data[i] = data[i] - o.data[i]; }
                return r;
            }

            /// Produit matriciel column-major correct
            NkMat4T operator*(const NkMat4T& o) const noexcept {
                NkMat4T r;
                for (int c = 0; c < 4; ++c) {
                    for (int row = 0; row < 4; ++row) {
                        T s = T(0);
                        for (int k = 0; k < 4; ++k) { s += mat[k][row] * o.mat[c][k]; }
                        r.mat[c][row] = s;
                    }
                }
                return r;
            }

            NkMat4T operator*(T s) const noexcept {
                NkMat4T r;
                for (int i = 0; i < 16; ++i) { r.data[i] = data[i] * s; }
                return r;
            }

            friend NkMat4T operator*(T s, const NkMat4T& m) noexcept { return m * s; }

            NkMat4T& operator+=(const NkMat4T& o) noexcept { *this = *this + o; return *this; }
            NkMat4T& operator-=(const NkMat4T& o) noexcept { *this = *this - o; return *this; }
            NkMat4T& operator*=(const NkMat4T& o) noexcept { *this = *this * o; return *this; }
            NkMat4T& operator*=(T s)             noexcept { *this = *this * s; return *this; }

            // ── Transformation de vecteurs ────────────────────────────────────

            /// M * v4 (colonne)
            NkVec4T<T> operator*(const NkVec4T<T>& v) const noexcept {
                return {
                    m00 * v.x + m10 * v.y + m20 * v.z + m30 * v.w,
                    m01 * v.x + m11 * v.y + m21 * v.z + m31 * v.w,
                    m02 * v.x + m12 * v.y + m22 * v.z + m32 * v.w,
                    m03 * v.x + m13 * v.y + m23 * v.z + m33 * v.w
                };
            }

            /// M * v3 homogène (w=1 → point, w=0 → vecteur)
            NkVec3T<T> operator*(const NkVec3T<T>& v) const noexcept {
                return {
                    m00 * v.x + m10 * v.y + m20 * v.z + m30,
                    m01 * v.x + m11 * v.y + m21 * v.z + m31,
                    m02 * v.x + m12 * v.y + m22 * v.z + m32
                };
            }

            /// v4 * M (row-major — utile pour DirectX sans upload transposé)
            friend NkVec4T<T> operator*(const NkVec4T<T>& v, const NkMat4T& m) noexcept {
                return {
                    v.x * m.mat[0][0] + v.y * m.mat[0][1] + v.z * m.mat[0][2] + v.w * m.mat[0][3],
                    v.x * m.mat[1][0] + v.y * m.mat[1][1] + v.z * m.mat[1][2] + v.w * m.mat[1][3],
                    v.x * m.mat[2][0] + v.y * m.mat[2][1] + v.z * m.mat[2][2] + v.w * m.mat[2][3],
                    v.x * m.mat[3][0] + v.y * m.mat[3][1] + v.z * m.mat[3][2] + v.w * m.mat[3][3]
                };
            }

            // ── Algèbre linéaire ──────────────────────────────────────────────

            static NkMat4T Identity() noexcept { return NkMat4T(T(1)); }

            NkMat4T Transpose() const noexcept {
                NkMat4T r;
                for (int c = 0; c < 4; ++c) {
                    for (int row = 0; row < 4; ++row) { r.mat[row][c] = mat[c][row]; }
                }
                return r;
            }

            /// Cofacteur de l'élément (row, col) — utilisé par Determinant/Inverse
            T Cofactor(int row, int col) const noexcept {
                T m[9];
                int idx = 0;
                for (int c = 0; c < 4; ++c) {
                    if (c == col) { continue; }
                    for (int r = 0; r < 4; ++r) {
                        if (r == row) { continue; }
                        m[idx++] = mat[c][r];
                    }
                }
                T det3 = m[0] * (m[4] * m[8] - m[5] * m[7])
                       - m[3] * (m[1] * m[8] - m[2] * m[7])
                       + m[6] * (m[1] * m[5] - m[2] * m[4]);
                return (((row + col) & 1) == 0) ? det3 : -det3;
            }

            T Determinant() const noexcept {
                T d = T(0);
                for (int c = 0; c < 4; ++c) { d += mat[c][0] * Cofactor(0, c); }
                return d;
            }

            NkMat4T Inverse() const noexcept {
                NkMat4T inv;
                // Matrice des cofacteurs transposée (adjugate)
                for (int c = 0; c < 4; ++c) {
                    for (int r = 0; r < 4; ++r) { inv.mat[c][r] = Cofactor(r, c); }
                }
                T det = Determinant();
                if (NkFabs(static_cast<float32>(det)) < static_cast<float32>(NkMatrixEpsilon)) {
                    return Identity(); // Matrice singulière → identité de secours
                }
                T invDet = T(1) / det;
                for (int i = 0; i < 16; ++i) { inv.data[i] *= invDet; }
                return inv;
            }

            void OrthoNormalize() noexcept {
                right.Normalize();
                up.Normalize();
                forward.Normalize();
            }

            NkMat4T OrthoNormalized() const noexcept {
                NkMat4T t(*this);
                t.OrthoNormalize();
                return t;
            }

            // ── Transformation de points / vecteurs / normales ────────────────

            /// Transforme un point (division par w si ≠ 0 et ≠ 1)
            NkVec3T<T> TransformPoint(const NkVec3T<T>& p) const noexcept {
                NkVec4T<T> r = (*this) * NkVec4T<T>(p, T(1));
                if (r.w == T(0) || r.w == T(1)) { return r.xyz(); }
                return r.xyz() / r.w;
            }

            /// Transforme un vecteur directionnel (w=0, ignore la translation)
            NkVec3T<T> TransformVector(const NkVec3T<T>& v) const noexcept {
                return (*this) * NkVec4T<T>(v, T(0)); // xyz() implicite
            }

            /// Transforme une normale (via l'inverse-transposée pour préserver ⊥)
            NkVec3T<T> TransformNormal(const NkVec3T<T>& n) const noexcept {
                NkMat4T normalMat = Inverse().Transpose();
                return NkVec3T<T>(
                    n.x * normalMat.m00 + n.y * normalMat.m10 + n.z * normalMat.m20,
                    n.x * normalMat.m01 + n.y * normalMat.m11 + n.z * normalMat.m21,
                    n.x * normalMat.m02 + n.y * normalMat.m12 + n.z * normalMat.m22
                ).Normalized();
            }

            // ── Extraction TRS robuste (via quaternion — résistant au gimbal) ──

            /// Extrait Translation, Rotation (Euler) et Scale d'une matrice TRS.
            ///
            /// Algorithme :
            ///   1. Scale = norme de chaque colonne-axe.
            ///   2. Matrice de rotation pure = colonnes normalisées.
            ///   3. Quaternion depuis la matrice de rotation (Shoemake 1994).
            ///   4. Euler depuis le quaternion avec détection de singularité.
            ///
            /// Cette approche évite le gimbal lock de la conversion
            /// directe matrice→Euler grâce à l'étape quaternion intermédiaire.
            void Extract(NkVec3T<T>&    outPosition,
                         NkEulerAngle& outEuler,
                         NkVec3T<T>&    outScale) const noexcept;

            // ── Fabrique statiques ────────────────────────────────────────────

            static NkMat4T Translation(const NkVec3T<T>& pos) noexcept {
                NkMat4T r(T(1));
                r.position = NkVec4T<T>(pos, T(1));
                return r;
            }

            static NkMat4T Scaling(const NkVec3T<T>& s) noexcept {
                NkMat4T r(T(1));
                r.mat[0][0] = s.x;
                r.mat[1][1] = s.y;
                r.mat[2][2] = s.z;
                return r;
            }

            /// Rotation autour d'un axe arbitraire (Rodrigues)
            static NkMat4T Rotation(const NkVec3T<T>& axis, const NkAngle& angle) noexcept {
                float32 c   = NkCos(angle.Rad());
                float32 s   = NkSin(angle.Rad());
                float32 omc = 1.0f - c;
                NkVec3T<T> v = axis.Normalized();
                NkMat4T r(T(1));
                r.mat[0][0] = T(c + v.x * v.x * omc);
                r.mat[0][1] = T(v.x * v.y * omc + v.z * s);
                r.mat[0][2] = T(v.x * v.z * omc - v.y * s);
                r.mat[1][0] = T(v.y * v.x * omc - v.z * s);
                r.mat[1][1] = T(c + v.y * v.y * omc);
                r.mat[1][2] = T(v.y * v.z * omc + v.x * s);
                r.mat[2][0] = T(v.z * v.x * omc + v.y * s);
                r.mat[2][1] = T(v.z * v.y * omc - v.x * s);
                r.mat[2][2] = T(c + v.z * v.z * omc);
                return r;
            }

            static NkMat4T RotationX(const NkAngle& pitch) noexcept {
                float32 c = NkCos(pitch.Rad()), s = NkSin(pitch.Rad());
                NkMat4T r(T(1));
                r.mat[1][1] = T(c);  r.mat[1][2] = T(s);
                r.mat[2][1] = T(-s); r.mat[2][2] = T(c);
                return r;
            }

            static NkMat4T RotationY(const NkAngle& yaw) noexcept {
                float32 c = NkCos(yaw.Rad()), s = NkSin(yaw.Rad());
                NkMat4T r(T(1));
                r.mat[0][0] = T(c);  r.mat[0][2] = T(-s);
                r.mat[2][0] = T(s);  r.mat[2][2] = T(c);
                return r;
            }

            static NkMat4T RotationZ(const NkAngle& roll) noexcept {
                float32 c = NkCos(roll.Rad()), s = NkSin(roll.Rad());
                NkMat4T r(T(1));
                r.mat[0][0] = T(c);  r.mat[0][1] = T(s);
                r.mat[1][0] = T(-s); r.mat[1][1] = T(c);
                return r;
            }

            /// Rotation depuis Euler (ordre ZYX : roll → yaw → pitch)
            static NkMat4T Rotation(const NkEulerAngle& euler) noexcept {
                return RotationZ(euler.roll) * RotationY(euler.yaw) * RotationX(euler.pitch);
            }

            /// Projection perspective (fov vertical en degrés, aspect = largeur/hauteur)
            static NkMat4T Perspective(const NkAngle& fovY, T aspect, T zNear, T zFar) noexcept {
                float32 tanHalf = NkTan(fovY.Rad() * 0.5f);
                NkMat4T r;
                r.mat[0][0] = T(1.0f / (aspect * tanHalf));
                r.mat[1][1] = T(1.0f / tanHalf);
                r.mat[2][2] = T((zFar + zNear) / (zNear - zFar));
                r.mat[3][2] = T(2.0f * zFar * zNear / (zNear - zFar));
                r.mat[2][3] = T(-1);
                return r;
            }

            /// Projection orthogonale (largeur × hauteur)
            static NkMat4T Orthogonal(T width, T height, T zNear, T zFar,
                                     bool depthZeroToOne = false) noexcept {
                NkMat4T r(T(1));
                r.mat[0][0] = T(2) / width;
                r.mat[1][1] = T(2) / height;
                if (depthZeroToOne) {
                    r.mat[2][2] = T(1) / (zNear - zFar);
                    r.mat[3][2] = zNear / (zNear - zFar);
                } else {
                    r.mat[2][2] = T(2) / (zNear - zFar);
                    r.mat[3][2] = (zFar + zNear) / (zNear - zFar);
                }
                return r;
            }

            /// Projection orthogonale avec coins explicites
            static NkMat4T Orthogonal(const NkVec2T<T>& bottomLeft,
                                     const NkVec2T<T>& topRight,
                                     T zNear, T zFar,
                                     bool depthZeroToOne = false) noexcept {
                NkMat4T r(T(1));
                r.mat[0][0] = T(2) / (topRight.x - bottomLeft.x);
                r.mat[1][1] = T(2) / (topRight.y - bottomLeft.y);
                r.mat[3][0] = -(topRight.x + bottomLeft.x) / (topRight.x - bottomLeft.x);
                r.mat[3][1] = -(topRight.y + bottomLeft.y) / (topRight.y - bottomLeft.y);
                if (depthZeroToOne) {
                    r.mat[2][2] = T(1) / (zNear - zFar);
                    r.mat[3][2] = zNear / (zNear - zFar);
                } else {
                    r.mat[2][2] = T(2) / (zNear - zFar);
                    r.mat[3][2] = (zFar + zNear) / (zNear - zFar);
                }
                return r;
            }

            /// Vue "regardant" center depuis eye (convention OpenGL, colonne-major)
            static NkMat4T LookAt(const NkVec3T<T>& eye,
                                 const NkVec3T<T>& center,
                                 const NkVec3T<T>& up) noexcept {
                NkVec3T<T> f = (eye - center).Normalized();      // forward (de cible vers œil)
                NkVec3T<T> r = up.Cross(f).Normalized();          // right
                NkVec3T<T> u = f.Cross(r).Normalized();           // up reorthogonalisé
                NkMat4T res;
                res.mat[0][0] = r.x;  res.mat[0][1] = u.x;  res.mat[0][2] = f.x;  res.mat[0][3] = T(0);
                res.mat[1][0] = r.y;  res.mat[1][1] = u.y;  res.mat[1][2] = f.y;  res.mat[1][3] = T(0);
                res.mat[2][0] = r.z;  res.mat[2][1] = u.z;  res.mat[2][2] = f.z;  res.mat[2][3] = T(0);
                res.mat[3][0] = -r.Dot(eye);
                res.mat[3][1] = -u.Dot(eye);
                res.mat[3][2] = -f.Dot(eye);
                res.mat[3][3] = T(1);
                return res;
            }

            /// Matrice de réflexion par rapport au plan de normale n
            static NkMat4T Reflection(const NkVec3T<T>& normal) noexcept {
                NkVec3T<T> n = normal.Normalized();
                NkMat4T r(T(1));
                r.mat[0][0] = T(1) - T(2) * n.x * n.x;
                r.mat[0][1] =      - T(2) * n.x * n.y;
                r.mat[0][2] =      - T(2) * n.x * n.z;
                r.mat[1][0] =      - T(2) * n.y * n.x;
                r.mat[1][1] = T(1) - T(2) * n.y * n.y;
                r.mat[1][2] =      - T(2) * n.y * n.z;
                r.mat[2][0] =      - T(2) * n.z * n.x;
                r.mat[2][1] =      - T(2) * n.z * n.y;
                r.mat[2][2] = T(1) - T(2) * n.z * n.z;
                return r;
            }

            // ── Conversion Row-Major (pour DirectX / HLSL) ────────────────────

            /// Copie les 16 valeurs en row-major dans out[16].
            /// Utiliser pour les cbuffer / uniform DirectX sans transposition HLSL.
            void ToRowMajor(T out[16]) const noexcept {
                for (int c = 0; c < 4; ++c) {
                    for (int row = 0; row < 4; ++row) { out[row * 4 + c] = mat[c][row]; }
                }
            }

            /// Retourne la transposée — équivaut à un layout row-major pour le GPU
            NK_FORCE_INLINE NkMat4T AsRowMajor() const noexcept { return Transpose(); }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat(
                    "NkMat4T[\n"
                    "  [{0:  .4}, {1:  .4}, {2:  .4}, {3:  .4}]\n"
                    "  [{4:  .4}, {5:  .4}, {6:  .4}, {7:  .4}]\n"
                    "  [{8:  .4}, {9:  .4}, {10: .4}, {11: .4}]\n"
                    "  [{12: .4}, {13: .4}, {14: .4}, {15: .4}]\n"
                    "]",
                    // Ligne 0 (row 0 de chaque colonne)
                    mat[0][0], mat[1][0], mat[2][0], mat[3][0],
                    // Ligne 1
                    mat[0][1], mat[1][1], mat[2][1], mat[3][1],
                    // Ligne 2
                    mat[0][2], mat[1][2], mat[2][2], mat[3][2],
                    // Ligne 3
                    mat[0][3], mat[1][3], mat[2][3], mat[3][3]
                );
            }

            friend NkString ToString(const NkMat4T& m) { return m.ToString(); }

            friend std::ostream& operator<<(std::ostream& os, const NkMat4T& m) {
                return os << m.ToString().CStr();
            }

        }; // struct NkMat4T


        // =====================================================================
        // NkMat4T::Extract — implémentation (dans le header car template)
        //
        // Extraction TRS robuste via quaternion intermédiaire (Shoemake 1994).
        //
        // Étapes :
        //   1. Extraire l'échelle (norme de chaque axe-colonne).
        //   2. Normaliser les 3 premières colonnes → matrice de rotation pure R.
        //   3. Construire le quaternion depuis R (méthode numériquement stable).
        //   4. Extraire les angles Euler depuis le quaternion avec gestion du
        //      gimbal lock via le test du "pôle de Gimbal" (t = y*x + z*w).
        //   5. La translation est directement mat[3][0..2].
        // =====================================================================

        template<typename T>
        void NkMat4T<T>::Extract(NkVec3T<T>&    outPosition,
                                NkEulerAngle& outEuler,
                                NkVec3T<T>&    outScale) const noexcept
        {
            // ── 1. Extraction de l'échelle ─────────────────────────────────────
            outScale.x = NkVec3T<T>(mat[0][0], mat[0][1], mat[0][2]).Len();
            outScale.y = NkVec3T<T>(mat[1][0], mat[1][1], mat[1][2]).Len();
            outScale.z = NkVec3T<T>(mat[2][0], mat[2][1], mat[2][2]).Len();

            // ── 2. Translation ─────────────────────────────────────────────────
            outPosition = NkVec3T<T>(mat[3][0], mat[3][1], mat[3][2]);

            // ── 3. Matrice de rotation pure (colonnes normalisées) ─────────────
            float32 invSx = (outScale.x > T(0)) ? T(1) / outScale.x : T(0);
            float32 invSy = (outScale.y > T(0)) ? T(1) / outScale.y : T(0);
            float32 invSz = (outScale.z > T(0)) ? T(1) / outScale.z : T(0);

            float32 r00 = static_cast<float32>(mat[0][0]) * invSx;
            float32 r01 = static_cast<float32>(mat[0][1]) * invSx;
            float32 r02 = static_cast<float32>(mat[0][2]) * invSx;
            float32 r10 = static_cast<float32>(mat[1][0]) * invSy;
            float32 r11 = static_cast<float32>(mat[1][1]) * invSy;
            float32 r12 = static_cast<float32>(mat[1][2]) * invSy;
            float32 r20 = static_cast<float32>(mat[2][0]) * invSz;
            float32 r21 = static_cast<float32>(mat[2][1]) * invSz;
            float32 r22 = static_cast<float32>(mat[2][2]) * invSz;

            // ── 4. Quaternion depuis la matrice de rotation (Shoemake 1994) ────
            //
            // On choisit la composante dominante (la plus grande en valeur absolue)
            // comme dénominateur pour éviter la division par zéro.
            float32 trace = r00 + r11 + r22;
            float32 qx, qy, qz, qw;

            if (trace > 0.0f) {
                float32 s = 0.5f / NkSqrt(trace + 1.0f);
                qw = 0.25f / s;
                qx = (r21 - r12) * s;
                qy = (r02 - r20) * s;
                qz = (r10 - r01) * s;
            } else if (r00 > r11 && r00 > r22) {
                float32 s = 2.0f * NkSqrt(1.0f + r00 - r11 - r22);
                qw = (r21 - r12) / s;
                qx = 0.25f * s;
                qy = (r01 + r10) / s;
                qz = (r02 + r20) / s;
            } else if (r11 > r22) {
                float32 s = 2.0f * NkSqrt(1.0f + r11 - r00 - r22);
                qw = (r02 - r20) / s;
                qx = (r01 + r10) / s;
                qy = 0.25f * s;
                qz = (r12 + r21) / s;
            } else {
                float32 s = 2.0f * NkSqrt(1.0f + r22 - r00 - r11);
                qw = (r10 - r01) / s;
                qx = (r02 + r20) / s;
                qy = (r12 + r21) / s;
                qz = 0.25f * s;
            }

            // Normalisation du quaternion
            float32 qlen = NkSqrt(qx * qx + qy * qy + qz * qz + qw * qw);
            if (qlen > static_cast<float32>(NkEpsilon)) {
                float32 invQ = 1.0f / qlen;
                qx *= invQ;
                qy *= invQ;
                qz *= invQ;
                qw *= invQ;
            }

            // ── 5. Quaternion → Euler avec gestion du gimbal lock ──────────────
            //
            // Détection du pôle de Gimbal : t = qy*qx + qz*qw
            //   t > +0.499 → singularité Nord (+90° Yaw)
            //   t < -0.499 → singularité Sud  (−90° Yaw)
            float32 gimbalTest = qy * qx + qz * qw;

            if (gimbalTest > 0.499f) {
                // Pôle Nord : yaw = +90°, roll dégénère avec pitch
                outEuler.yaw   = NkAngle::FromRad( static_cast<float32>(NkPis2));
                outEuler.pitch = NkAngle::FromRad( 2.0f * NkAtan2(qx, qw));
                outEuler.roll  = NkAngle(0.0f);
            } else if (gimbalTest < -0.499f) {
                // Pôle Sud : yaw = −90°, roll dégènere avec pitch
                outEuler.yaw   = NkAngle::FromRad(-static_cast<float32>(NkPis2));
                outEuler.pitch = NkAngle::FromRad(-2.0f * NkAtan2(qx, qw));
                outEuler.roll  = NkAngle(0.0f);
            } else {
                // Cas général — pas de singularité
                // Pitch (X)
                float32 sinr = 2.0f * (qw * qx + qy * qz);
                float32 cosr = 1.0f - 2.0f * (qx * qx + qy * qy);
                outEuler.pitch = NkAngle::FromRad(NkAtan2(sinr, cosr));

                // Yaw (Y)
                float32 sinp = 2.0f * (qw * qy - qz * qx);
                sinp = NkClamp(sinp, -1.0f, 1.0f); // protection NaN
                outEuler.yaw = NkAngle::FromRad(NkAsin(sinp));

                // Roll (Z)
                float32 siny = 2.0f * (qw * qz + qx * qy);
                float32 cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
                outEuler.roll = NkAngle::FromRad(NkAtan2(siny, cosy));
            }
        }


        // =====================================================================
        // Aliases de types — f=float32, d=float64 (matrices: pas de i/u)
        // =====================================================================

        using NkMatrix2f    = NkMat2T<float32>;
        using NkMatrix2d    = NkMat2T<float64>;
        using NkMatrix3f    = NkMat3T<float32>;
        using NkMatrix3d    = NkMat3T<float64>;
        using NkMatrix4f    = NkMat4T<float32>;
        using NkMatrix4d    = NkMat4T<float64>;

        // Alias courts
        using NkMat2f       = NkMatrix2f;
        using NkMat2d       = NkMatrix2d;
        using NkMat3f       = NkMatrix3f;
        using NkMat3d       = NkMatrix3d;
        using NkMat4f       = NkMatrix4f;
        using NkMat4d       = NkMatrix4d;

        // Alias legacy
        using NkMatrix2     = NkMatrix2f;
        using NkMatrix3     = NkMatrix3f;
        using NkMatrix4     = NkMatrix4f;
        using NkMatrice4    = NkMatrix4f;
        using NkMat2        = NkMatrix2f;
        using NkMat3        = NkMatrix3f;
        using NkMat4        = NkMatrix4f;

    } // namespace math


    // =========================================================================
    // NkToString — spécialisations pour les 3 templates de matrices
    // =========================================================================

    template<typename T>
    struct NkToString<math::NkMat2T<T>> {
        static NkString Convert(const math::NkMat2T<T>& m, const NkFormatProps& props) {
            return NkApplyFormatProps(m.ToString(), props);
        }
    };

    template<typename T>
    struct NkToString<math::NkMat3T<T>> {
        static NkString Convert(const math::NkMat3T<T>& m, const NkFormatProps& props) {
            return NkApplyFormatProps(m.ToString(), props);
        }
    };

    template<typename T>
    struct NkToString<math::NkMat4T<T>> {
        static NkString Convert(const math::NkMat4T<T>& m, const NkFormatProps& props) {
            return NkApplyFormatProps(m.ToString(), props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_MATRIX_H__