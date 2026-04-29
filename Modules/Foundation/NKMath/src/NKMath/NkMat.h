//
// NkMat.h
// =============================================================================
// Description :
//   Définition des structures de matrices génériques NkMat2T<T>, NkMat3T<T>,
//   et NkMat4T<T> pour les transformations linéaires et projectives 2D/3D/4D.
//
// Convention de stockage :
//   COLUMN-MAJOR (compatible OpenGL / Vulkan) :
//   - Layout mémoire : mat[col][row] — colonne 0 stockée en premier
//   - Pour DirectX/HLSL (row-major) : utiliser ToRowMajor() avant upload GPU
//   - L'opération mathématique M*v reste valable pour les deux API
//
// Extraction TRS robuste (NkMat4T) :
//   - Extract() utilise un quaternion intermédiaire (méthode Shoemake 1994)
//   - Évite le gimbal lock inhérent à la conversion directe matrice→Euler
//   - Détection de singularité via GimbalPole() pour stabilité numérique
//
// Types disponibles (suffixes) :
//   f = float32, d = float64 (pas de variants entiers pour les matrices)
//   Exemple : NkMat4f = NkMat4T<float32>
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_MATRIX_H__
#define __NKENTSEU_MATRIX_H__

    // =====================================================================
    // Inclusions des dépendances du projet
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"      // Types fondamentaux : float32, float64, etc.
    #include "NKMath/NkVec.h"               // Types vectoriels : NkVec2T, NkVec3T, NkVec4T
    #include "NKMath/NkAngle.h"             // Type NkAngle pour les rotations angulaires
    #include "NKMath/NkEulerAngle.h"        // Type NkEulerAngle pour les angles d'Euler
    #include "NKMath/NkFunctions.h"         // Fonctions mathématiques : NkSin, NkCos, etc.

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : types et opérations matricielles
        // =================================================================
        namespace math {

            // =================================================================
            // Structure template : NkMat2T<T>
            // =================================================================
            // Matrice 2×2 générique en column-major pour transformations 2D.
            //
            // Layout mémoire (column-major) :
            //   [ m00, m01,  m10, m11 ]
            //     col0       col1
            //
            // Utilisation typique : rotations 2D, mises à l'échelle, shearing.
            // =================================================================
            template <typename T>
            struct NkMat2T {

                // -----------------------------------------------------------------
                // Section : Union de données (accès flexible)
                // -----------------------------------------------------------------
                union {

                    // Accès linéaire column-major pour opérations bas-niveau
                    T data[4];

                    // Accès nommé par éléments individuels
                    struct {
                        T m00;  ///< Élément ligne 0, colonne 0
                        T m01;  ///< Élément ligne 1, colonne 0
                        T m10;  ///< Élément ligne 0, colonne 1
                        T m11;  ///< Élément ligne 1, colonne 1
                    };

                    // Accès par vecteurs colonnes pour opérations géométriques
                    NkVec2T<T> col[2];  ///< col[0] = première colonne, col[1] = seconde

                };  // union

                // -----------------------------------------------------------------
                // Section : Constructeurs
                // -----------------------------------------------------------------
                // Constructeur par défaut : initialise à la matrice identité 2×2
                NkMat2T() noexcept
                {
                    col[0] = { T(1), T(0) };
                    col[1] = { T(0), T(1) };
                }

                // Constructeur scalaire : matrice diagonale avec valeur uniforme
                explicit NkMat2T(T diagonalValue) noexcept
                {
                    col[0] = { diagonalValue, T(0) };
                    col[1] = { T(0), diagonalValue };
                }

                // Constructeur par colonnes : initialise avec deux vecteurs 2D
                NkMat2T(
                    const NkVec2T<T>& column0,
                    const NkVec2T<T>& column1
                ) noexcept
                {
                    col[0] = column0;
                    col[1] = column1;
                }

                // Constructeur élément par élément (ordre column-major explicite)
                NkMat2T(
                    T element00, T element01,
                    T element10, T element11
                ) noexcept
                    : m00(element00)
                    , m01(element01)
                    , m10(element10)
                    , m11(element11)
                {
                }

                // Constructeur de copie : généré par défaut (optimisation compilateur)
                NkMat2T(const NkMat2T&) noexcept = default;

                // Opérateur d'affectation : généré par défaut (optimisation compilateur)
                NkMat2T& operator=(const NkMat2T&) noexcept = default;

                // -----------------------------------------------------------------
                // Section : Accesseurs et conversions
                // -----------------------------------------------------------------
                // Opérateur d'indexation mutable : retourne pointeur vers colonne c
                NK_FORCE_INLINE T* operator[](int columnIndex) noexcept
                {
                    return &data[columnIndex * 2];
                }

                // Opérateur d'indexation constant : retourne pointeur vers colonne c
                NK_FORCE_INLINE const T* operator[](int columnIndex) const noexcept
                {
                    return &data[columnIndex * 2];
                }

                // Conversion implicite vers pointeur mutable sur données brutes
                operator T*() noexcept
                {
                    return data;
                }

                // Conversion implicite vers pointeur constant sur données brutes
                operator const T*() const noexcept
                {
                    return data;
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs arithmétiques binaires
                // -----------------------------------------------------------------
                // Addition composante par composante
                NkMat2T operator+(const NkMat2T& other) const noexcept
                {
                    NkMat2T result;
                    for (int index = 0; index < 4; ++index) {
                        result.data[index] = data[index] + other.data[index];
                    }
                    return result;
                }

                // Soustraction composante par composante
                NkMat2T operator-(const NkMat2T& other) const noexcept
                {
                    NkMat2T result;
                    for (int index = 0; index < 4; ++index) {
                        result.data[index] = data[index] - other.data[index];
                    }
                    return result;
                }

                // Produit matriciel : multiplication de deux matrices 2×2
                NkMat2T operator*(const NkMat2T& other) const noexcept
                {
                    return {
                        m00 * other.m00 + m10 * other.m01,
                        m01 * other.m00 + m11 * other.m01,
                        m00 * other.m10 + m10 * other.m11,
                        m01 * other.m10 + m11 * other.m11
                    };
                }

                // Multiplication par scalaire : scale chaque élément de la matrice
                NkMat2T operator*(T scalar) const noexcept
                {
                    NkMat2T result;
                    for (int index = 0; index < 4; ++index) {
                        result.data[index] = data[index] * scalar;
                    }
                    return result;
                }

                // Transformation d'un vecteur colonne : calcule M * v
                NkVec2T<T> operator*(const NkVec2T<T>& vector) const noexcept
                {
                    return {
                        m00 * vector.x + m10 * vector.y,
                        m01 * vector.x + m11 * vector.y
                    };
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs d'affectation composée
                // -----------------------------------------------------------------
                NkMat2T& operator+=(const NkMat2T& other) noexcept
                {
                    *this = *this + other;
                    return *this;
                }

                NkMat2T& operator-=(const NkMat2T& other) noexcept
                {
                    *this = *this - other;
                    return *this;
                }

                NkMat2T& operator*=(const NkMat2T& other) noexcept
                {
                    *this = *this * other;
                    return *this;
                }

                NkMat2T& operator*=(T scalar) noexcept
                {
                    *this = *this * scalar;
                    return *this;
                }

                // Multiplication scalaire commutative (fonction amie)
                friend NkMat2T operator*(T scalar, const NkMat2T& matrix) noexcept
                {
                    return matrix * scalar;
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs de comparaison
                // -----------------------------------------------------------------
                // Égalité avec tolérance flottante via NkMatrixEpsilon
                bool operator==(const NkMat2T& other) const noexcept
                {
                    for (int index = 0; index < 4; ++index) {
                        if (NkFabs(
                                static_cast<float64>(data[index] - other.data[index])
                            ) > static_cast<float64>(NkMatrixEpsilon)) {
                            return false;
                        }
                    }
                    return true;
                }

                // Inégalité déléguée à l'opérateur d'égalité (principe DRY)
                bool operator!=(const NkMat2T& other) const noexcept
                {
                    return !(*this == other);
                }

                // -----------------------------------------------------------------
                // Section : Algèbre linéaire (méthodes statiques et membres)
                // -----------------------------------------------------------------
                // Matrice identité 2×2 statique
                static NkMat2T Identity() noexcept
                {
                    return {};
                }

                // Transposée : échange lignes et colonnes
                NkMat2T Transpose() const noexcept
                {
                    return { m00, m10, m01, m11 };
                }

                // Déterminant : mesure du facteur de scaling de l'aire
                T Determinant() const noexcept
                {
                    return m00 * m11 - m10 * m01;
                }

                // Inverse : matrice inverse via formule analytique 2×2
                NkMat2T Inverse() const noexcept
                {
                    T determinant = Determinant();
                    if (NkFabs(
                            static_cast<float64>(determinant)
                        ) < static_cast<float64>(NkMatrixEpsilon)) {
                        return Identity();  // Matrice singulière → identité de secours
                    }
                    T inverseDet = T(1) / determinant;
                    return {
                        m11 * inverseDet,
                        -m01 * inverseDet,
                        -m10 * inverseDet,
                        m00 * inverseDet
                    };
                }

                // Trace : somme des éléments diagonaux
                NK_FORCE_INLINE T Trace() const noexcept
                {
                    return m00 + m11;
                }

                // -----------------------------------------------------------------
                // Section : Conversion Row-Major (pour DirectX / HLSL)
                // -----------------------------------------------------------------
                // Copie les 4 valeurs en layout row-major dans le tableau de sortie
                // Utiliser avant upload vers un cbuffer DirectX/HLSL
                void ToRowMajor(T output[4]) const noexcept
                {
                    output[0] = m00;  // Ligne 0, Colonne 0
                    output[1] = m10;  // Ligne 0, Colonne 1
                    output[2] = m01;  // Ligne 1, Colonne 0
                    output[3] = m11;  // Ligne 1, Colonne 1
                }

                // -----------------------------------------------------------------
                // Section : Sérialisation et E/S
                // -----------------------------------------------------------------
                // Conversion en chaîne formatée pour débogage et logging
                NkString ToString() const
                {
                    return NkFormat(
                        "NkMat2T[\n  [{0}, {1}]\n  [{2}, {3}]\n]",
                        m00, m10,
                        m01, m11
                    );
                }

                // Surcharge globale de ToString pour appel fonctionnel libre
                friend NkString ToString(const NkMat2T& matrix)
                {
                    return matrix.ToString();
                }

                // Opérateur de flux pour affichage dans std::ostream
                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkMat2T& matrix
                ) {
                    return outputStream << matrix.ToString().CStr();
                }

            };  // struct NkMat2T

            // =================================================================
            // Structure template : NkMat3T<T>
            // =================================================================
            // Matrice 3×3 générique en column-major pour transformations 3D.
            //
            // Layout mémoire (column-major) :
            //   [ col0.xyz, col1.xyz, col2.xyz ]
            //
            // Utilisation typique : rotations 3D, projections 2D, transformations
            // affines dans l'espace 3D (sans translation homogène).
            // =================================================================
            template <typename T>
            struct NkMat3T {

                // -----------------------------------------------------------------
                // Union de données (accès flexible)
                // -----------------------------------------------------------------
                union {

                    // Accès linéaire column-major pour opérations bas-niveau
                    T data[9];

                    // Accès nommé par éléments individuels
                    struct {
                        T m00, m01, m02;  ///< Colonne 0 : éléments lignes 0,1,2
                        T m10, m11, m12;  ///< Colonne 1 : éléments lignes 0,1,2
                        T m20, m21, m22;  ///< Colonne 2 : éléments lignes 0,1,2
                    };

                    // Accès par vecteurs colonnes pour opérations géométriques
                    NkVec3T<T> col[3];  ///< col[i] = i-ème colonne de la matrice

                };  // union

                // -----------------------------------------------------------------
                // Constructeurs
                // -----------------------------------------------------------------
                // Constructeur par défaut : matrice identité 3×3
                NkMat3T() noexcept
                {
                    col[0] = { T(1), T(0), T(0) };
                    col[1] = { T(0), T(1), T(0) };
                    col[2] = { T(0), T(0), T(1) };
                }

                // Constructeur scalaire : matrice diagonale avec valeur uniforme
                explicit NkMat3T(T diagonalValue) noexcept
                {
                    col[0] = { diagonalValue, T(0), T(0) };
                    col[1] = { T(0), diagonalValue, T(0) };
                    col[2] = { T(0), T(0), diagonalValue };
                }

                // Constructeur par colonnes : initialise avec trois vecteurs 3D
                NkMat3T(
                    const NkVec3T<T>& column0,
                    const NkVec3T<T>& column1,
                    const NkVec3T<T>& column2
                ) noexcept
                {
                    col[0] = column0;
                    col[1] = column1;
                    col[2] = column2;
                }

                // Constructeur élément par élément (ordre column-major explicite)
                NkMat3T(
                    T e00, T e01, T e02,
                    T e10, T e11, T e12,
                    T e20, T e21, T e22
                ) noexcept
                    : m00(e00), m01(e01), m02(e02)
                    , m10(e10), m11(e11), m12(e12)
                    , m20(e20), m21(e21), m22(e22)
                {
                }

                // Constructeur de copie : généré par défaut
                NkMat3T(const NkMat3T&) noexcept = default;

                // Opérateur d'affectation : généré par défaut
                NkMat3T& operator=(const NkMat3T&) noexcept = default;

                // -----------------------------------------------------------------
                // Accesseurs
                // -----------------------------------------------------------------
                NK_FORCE_INLINE T* operator[](int columnIndex) noexcept
                {
                    return &data[columnIndex * 3];
                }

                NK_FORCE_INLINE const T* operator[](int columnIndex) const noexcept
                {
                    return &data[columnIndex * 3];
                }

                operator T*() noexcept { return data; }
                operator const T*() const noexcept { return data; }

                // -----------------------------------------------------------------
                // Arithmétique
                // -----------------------------------------------------------------
                NkMat3T operator+(const NkMat3T& other) const noexcept
                {
                    NkMat3T result;
                    for (int index = 0; index < 9; ++index) {
                        result.data[index] = data[index] + other.data[index];
                    }
                    return result;
                }

                NkMat3T operator-(const NkMat3T& other) const noexcept
                {
                    NkMat3T result;
                    for (int index = 0; index < 9; ++index) {
                        result.data[index] = data[index] - other.data[index];
                    }
                    return result;
                }

                // Produit matriciel 3×3 : multiplication standard
                NkMat3T operator*(const NkMat3T& other) const noexcept
                {
                    NkMat3T result;
                    for (int col = 0; col < 3; ++col) {
                        for (int row = 0; row < 3; ++row) {
                            T sum = T(0);
                            for (int k = 0; k < 3; ++k) {
                                sum += data[k * 3 + row] * other.data[col * 3 + k];
                            }
                            result.data[col * 3 + row] = sum;
                        }
                    }
                    return result;
                }

                NkMat3T operator*(T scalar) const noexcept
                {
                    NkMat3T result;
                    for (int index = 0; index < 9; ++index) {
                        result.data[index] = data[index] * scalar;
                    }
                    return result;
                }

                // Transformation d'un vecteur 3D : calcule M * v
                NkVec3T<T> operator*(const NkVec3T<T>& vector) const noexcept
                {
                    return {
                        m00 * vector.x + m10 * vector.y + m20 * vector.z,
                        m01 * vector.x + m11 * vector.y + m21 * vector.z,
                        m02 * vector.x + m12 * vector.y + m22 * vector.z
                    };
                }

                // -----------------------------------------------------------------
                // Affectation composée
                // -----------------------------------------------------------------
                NkMat3T& operator+=(const NkMat3T& other) noexcept
                {
                    *this = *this + other;
                    return *this;
                }

                NkMat3T& operator-=(const NkMat3T& other) noexcept
                {
                    *this = *this - other;
                    return *this;
                }

                NkMat3T& operator*=(const NkMat3T& other) noexcept
                {
                    *this = *this * other;
                    return *this;
                }

                NkMat3T& operator*=(T scalar) noexcept
                {
                    *this = *this * scalar;
                    return *this;
                }

                friend NkMat3T operator*(T scalar, const NkMat3T& matrix) noexcept
                {
                    return matrix * scalar;
                }

                // -----------------------------------------------------------------
                // Comparaison
                // -----------------------------------------------------------------
                bool operator==(const NkMat3T& other) const noexcept
                {
                    for (int index = 0; index < 9; ++index) {
                        if (NkFabs(
                                static_cast<float64>(data[index] - other.data[index])
                            ) > static_cast<float64>(NkMatrixEpsilon)) {
                            return false;
                        }
                    }
                    return true;
                }

                bool operator!=(const NkMat3T& other) const noexcept
                {
                    return !(*this == other);
                }

                // -----------------------------------------------------------------
                // Algèbre linéaire
                // -----------------------------------------------------------------
                static NkMat3T Identity() noexcept { return {}; }

                NkMat3T Transpose() const noexcept
                {
                    return {
                        m00, m10, m20,
                        m01, m11, m21,
                        m02, m12, m22
                    };
                }

                T Determinant() const noexcept
                {
                    return m00 * (m11 * m22 - m21 * m12)
                         - m10 * (m01 * m22 - m21 * m02)
                         + m20 * (m01 * m12 - m11 * m02);
                }

                NkMat3T Inverse() const noexcept
                {
                    T determinant = Determinant();
                    if (NkFabs(
                            static_cast<float64>(determinant)
                        ) < static_cast<float64>(NkMatrixEpsilon)) {
                        return Identity();
                    }
                    T inverseDet = T(1) / determinant;
                    NkMat3T result;
                    result.m00 =  (m11 * m22 - m21 * m12) * inverseDet;
                    result.m01 = -(m01 * m22 - m21 * m02) * inverseDet;
                    result.m02 =  (m01 * m12 - m11 * m02) * inverseDet;
                    result.m10 = -(m10 * m22 - m20 * m12) * inverseDet;
                    result.m11 =  (m00 * m22 - m20 * m02) * inverseDet;
                    result.m12 = -(m00 * m12 - m10 * m02) * inverseDet;
                    result.m20 =  (m10 * m21 - m20 * m11) * inverseDet;
                    result.m21 = -(m00 * m21 - m20 * m01) * inverseDet;
                    result.m22 =  (m00 * m11 - m10 * m01) * inverseDet;
                    return result;
                }

                NK_FORCE_INLINE T Trace() const noexcept
                {
                    return m00 + m11 + m22;
                }

                NK_FORCE_INLINE NkVec3T<T> Diagonal() const noexcept
                {
                    return { m00, m11, m22 };
                }

                // -----------------------------------------------------------------
                // Conversion Row-Major
                // -----------------------------------------------------------------
                void ToRowMajor(T output[9]) const noexcept
                {
                    // Ligne 0 : éléments [col0][row0], [col1][row0], [col2][row0]
                    output[0] = m00;
                    output[1] = m10;
                    output[2] = m20;
                    // Ligne 1
                    output[3] = m01;
                    output[4] = m11;
                    output[5] = m21;
                    // Ligne 2
                    output[6] = m02;
                    output[7] = m12;
                    output[8] = m22;
                }

                // -----------------------------------------------------------------
                // E/S
                // -----------------------------------------------------------------
                NkString ToString() const
                {
                    return NkFormat(
                        "NkMat3T[\n  [{0}, {1}, {2}]\n  [{3}, {4}, {5}]\n  [{6}, {7}, {8}]\n]",
                        m00, m10, m20,
                        m01, m11, m21,
                        m02, m12, m22
                    );
                }

                friend NkString ToString(const NkMat3T& matrix) { return matrix.ToString(); }

                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkMat3T& matrix
                ) {
                    return outputStream << matrix.ToString().CStr();
                }

            };  // struct NkMat3T

            // =================================================================
            // Structure template : NkMat4T<T>
            // =================================================================
            // Matrice 4×4 générique en column-major pour transformations 3D
            // complètes avec coordonnées homogènes (translation, rotation, scale).
            //
            // Layout mémoire (column-major) :
            //   [ right(col0), up(col1), forward(col2), position(col3) ]
            //
            // Convention d'utilisation GPU :
            //   OpenGL/Vulkan : column-major natif → glUniformMatrix4fv(..., GL_FALSE, ...)
            //   DirectX/HLSL  : row-major attendu → appeler ToRowMajor() avant upload
            //
            // Extraction TRS :
            //   Extract() utilise quaternion intermédiaire (Shoemake 1994) pour
            //   éviter le gimbal lock lors de la conversion matrice→Euler.
            // =================================================================
            template <typename T>
            struct NkMat4T {

                // -----------------------------------------------------------------
                // Union de données (accès flexible)
                // -----------------------------------------------------------------
                union {

                    // Accès linéaire column-major pour opérations bas-niveau/SIMD
                    T data[16];

                    // Accès bidimensionnel mat[col][row] pour lisibilité
                    T mat[4][4];

                    // Accès nommé par éléments individuels (column-major)
                    struct {
                        T m00, m01, m02, m03;  ///< Colonne 0 : axe X local (right)
                        T m10, m11, m12, m13;  ///< Colonne 1 : axe Y local (up)
                        T m20, m21, m22, m23;  ///< Colonne 2 : axe Z local (forward)
                        T m30, m31, m32, m33;  ///< Colonne 3 : translation + w
                    };

                    // Accès sémantique par axes de transformation
                    struct {
                        NkVec4T<T> right;     ///< Colonne 0 — axe X du repère local
                        NkVec4T<T> up;        ///< Colonne 1 — axe Y du repère local
                        NkVec4T<T> forward;   ///< Colonne 2 — axe Z du repère local
                        NkVec4T<T> position;  ///< Colonne 3 — vecteur de translation
                    };

                    // Accès par vecteurs colonnes pour opérations géométriques
                    NkVec4T<T> col[4];  ///< col[i] = i-ème colonne de la matrice

                };  // union

                // -----------------------------------------------------------------
                // Constructeurs
                // -----------------------------------------------------------------
                // Constructeur par défaut : matrice nulle (tous éléments à 0)
                NkMat4T() noexcept
                {
                    for (int index = 0; index < 16; ++index) {
                        data[index] = T(0);
                    }
                }

                // Constructeur scalaire avec option identité
                // Si identity=true : matrice diagonale avec 'value' sur la diagonale
                // Si identity=false : tous les éléments initialisés à 'value'
                explicit NkMat4T(T value, bool identity = true) noexcept
                {
                    for (int index = 0; index < 16; ++index) {
                        data[index] = T(0);
                    }
                    if (identity) {
                        mat[0][0] = value;
                        mat[1][1] = value;
                        mat[2][2] = value;
                        mat[3][3] = value;
                    } else {
                        for (int index = 0; index < 16; ++index) {
                            data[index] = value;
                        }
                    }
                }

                // Constructeur depuis 4 colonnes vectorielles
                NkMat4T(
                    const NkVec4T<T>& column0,
                    const NkVec4T<T>& column1,
                    const NkVec4T<T>& column2,
                    const NkVec4T<T>& column3
                ) noexcept
                {
                    col[0] = column0;
                    col[1] = column1;
                    col[2] = column2;
                    col[3] = column3;
                }

                // Constructeur depuis 3 axes orthonormés (sans translation)
                // Initialise la 4ème colonne à (0,0,0,1) pour transformation affine
                NkMat4T(
                    const NkVec3T<T>& rightAxis,
                    const NkVec3T<T>& upAxis,
                    const NkVec3T<T>& forwardAxis
                ) noexcept
                {
                    for (int index = 0; index < 16; ++index) {
                        data[index] = T(0);
                    }
                    right = NkVec4T<T>(rightAxis, T(0));
                    up = NkVec4T<T>(upAxis, T(0));
                    forward = NkVec4T<T>(forwardAxis, T(0));
                    position = NkVec4T<T>(T(0), T(0), T(0), T(1));
                }

                // Constructeur depuis 3 axes + translation explicite
                NkMat4T(
                    const NkVec3T<T>& rightAxis,
                    const NkVec3T<T>& upAxis,
                    const NkVec3T<T>& forwardAxis,
                    const NkVec3T<T>& translation
                ) noexcept
                {
                    right = NkVec4T<T>(rightAxis, T(0));
                    up = NkVec4T<T>(upAxis, T(0));
                    forward = NkVec4T<T>(forwardAxis, T(0));
                    position = NkVec4T<T>(translation, T(1));
                }

                // Constructeur élément par élément (ordre column-major explicite)
                // Paramètres dans l'ordre : m00,m10,m20,m30, m01,m11,m21,m31, ...
                NkMat4T(
                    T e00, T e10, T e20, T e30,
                    T e01, T e11, T e21, T e31,
                    T e02, T e12, T e22, T e32,
                    T e03, T e13, T e23, T e33
                ) noexcept
                    : m00(e00), m01(e01), m02(e02), m03(e03)
                    , m10(e10), m11(e11), m12(e12), m13(e13)
                    , m20(e20), m21(e21), m22(e22), m23(e23)
                    , m30(e30), m31(e31), m32(e32), m33(e33)
                {
                }

                // Constructeur depuis tableau brut de 16 valeurs column-major
                explicit NkMat4T(const T* values) noexcept
                {
                    for (int index = 0; index < 16; ++index) {
                        data[index] = values[index];
                    }
                }

                // Constructeur de copie : généré par défaut
                NkMat4T(const NkMat4T&) noexcept = default;

                // Opérateur d'affectation : généré par défaut
                NkMat4T& operator=(const NkMat4T&) noexcept = default;

                // -----------------------------------------------------------------
                // Accesseurs
                // -----------------------------------------------------------------
                NK_FORCE_INLINE T* operator[](int columnIndex) noexcept
                {
                    return &data[columnIndex * 4];
                }

                NK_FORCE_INLINE const T* operator[](int columnIndex) const noexcept
                {
                    return &data[columnIndex * 4];
                }

                operator T*() noexcept { return data; }
                operator const T*() const noexcept { return data; }

                // -----------------------------------------------------------------
                // Comparaison
                // -----------------------------------------------------------------
                bool operator==(const NkMat4T& other) const noexcept
                {
                    for (int index = 0; index < 16; ++index) {
                        if (NkFabs(
                                static_cast<float64>(data[index] - other.data[index])
                            ) > static_cast<float64>(NkMatrixEpsilon)) {
                            return false;
                        }
                    }
                    return true;
                }

                bool operator!=(const NkMat4T& other) const noexcept
                {
                    return !(*this == other);
                }

                // -----------------------------------------------------------------
                // Arithmétique
                // -----------------------------------------------------------------
                NkMat4T operator+(const NkMat4T& other) const noexcept
                {
                    NkMat4T result;
                    for (int index = 0; index < 16; ++index) {
                        result.data[index] = data[index] + other.data[index];
                    }
                    return result;
                }

                NkMat4T operator-(const NkMat4T& other) const noexcept
                {
                    NkMat4T result;
                    for (int index = 0; index < 16; ++index) {
                        result.data[index] = data[index] - other.data[index];
                    }
                    return result;
                }

                // Produit matriciel 4×4 column-major correct
                NkMat4T operator*(const NkMat4T& other) const noexcept
                {
                    NkMat4T result;
                    for (int col = 0; col < 4; ++col) {
                        for (int row = 0; row < 4; ++row) {
                            T sum = T(0);
                            for (int k = 0; k < 4; ++k) {
                                sum += mat[k][row] * other.mat[col][k];
                            }
                            result.mat[col][row] = sum;
                        }
                    }
                    return result;
                }

                NkMat4T operator*(T scalar) const noexcept
                {
                    NkMat4T result;
                    for (int index = 0; index < 16; ++index) {
                        result.data[index] = data[index] * scalar;
                    }
                    return result;
                }

                friend NkMat4T operator*(T scalar, const NkMat4T& matrix) noexcept
                {
                    return matrix * scalar;
                }

                NkMat4T& operator+=(const NkMat4T& other) noexcept
                {
                    *this = *this + other;
                    return *this;
                }

                NkMat4T& operator-=(const NkMat4T& other) noexcept
                {
                    *this = *this - other;
                    return *this;
                }

                NkMat4T& operator*=(const NkMat4T& other) noexcept
                {
                    *this = *this * other;
                    return *this;
                }

                NkMat4T& operator*=(T scalar) noexcept
                {
                    *this = *this * scalar;
                    return *this;
                }

                // -----------------------------------------------------------------
                // Transformation de vecteurs
                // -----------------------------------------------------------------
                // Multiplication matrice × vecteur colonne 4D : M * v
                NkVec4T<T> operator*(const NkVec4T<T>& vector) const noexcept
                {
                    return {
                        m00 * vector.x + m10 * vector.y + m20 * vector.z + m30 * vector.w,
                        m01 * vector.x + m11 * vector.y + m21 * vector.z + m31 * vector.w,
                        m02 * vector.x + m12 * vector.y + m22 * vector.z + m32 * vector.w,
                        m03 * vector.x + m13 * vector.y + m23 * vector.z + m33 * vector.w
                    };
                }

                // Multiplication matrice × vecteur 3D homogène (w=1 implicite)
                NkVec3T<T> operator*(const NkVec3T<T>& vector) const noexcept
                {
                    return {
                        m00 * vector.x + m10 * vector.y + m20 * vector.z + m30,
                        m01 * vector.x + m11 * vector.y + m21 * vector.z + m31,
                        m02 * vector.x + m12 * vector.y + m22 * vector.z + m32
                    };
                }

                // Multiplication vecteur ligne × matrice (row-major) : v * M
                // Utile pour DirectX sans transposition préalable du buffer GPU
                friend NkVec4T<T> operator*(
                    const NkVec4T<T>& vector,
                    const NkMat4T& matrix
                ) noexcept {
                    return {
                        vector.x * matrix.mat[0][0] + vector.y * matrix.mat[0][1]
                            + vector.z * matrix.mat[0][2] + vector.w * matrix.mat[0][3],
                        vector.x * matrix.mat[1][0] + vector.y * matrix.mat[1][1]
                            + vector.z * matrix.mat[1][2] + vector.w * matrix.mat[1][3],
                        vector.x * matrix.mat[2][0] + vector.y * matrix.mat[2][1]
                            + vector.z * matrix.mat[2][2] + vector.w * matrix.mat[2][3],
                        vector.x * matrix.mat[3][0] + vector.y * matrix.mat[3][1]
                            + vector.z * matrix.mat[3][2] + vector.w * matrix.mat[3][3]
                    };
                }

                // -----------------------------------------------------------------
                // Algèbre linéaire
                // -----------------------------------------------------------------
                static NkMat4T Identity() noexcept { return NkMat4T(T(1)); }

                NkMat4T Transpose() const noexcept
                {
                    NkMat4T result;
                    for (int col = 0; col < 4; ++col) {
                        for (int row = 0; row < 4; ++row) {
                            result.mat[row][col] = mat[col][row];
                        }
                    }
                    return result;
                }

                // Cofacteur de l'élément (row, col) pour calcul du déterminant/inverse
                T Cofactor(int row, int col) const noexcept
                {
                    T minor[9];
                    int index = 0;
                    for (int c = 0; c < 4; ++c) {
                        if (c == col) { continue; }
                        for (int r = 0; r < 4; ++r) {
                            if (r == row) { continue; }
                            minor[index++] = mat[c][r];
                        }
                    }
                    T det3 = minor[0] * (minor[4] * minor[8] - minor[5] * minor[7])
                           - minor[3] * (minor[1] * minor[8] - minor[2] * minor[7])
                           + minor[6] * (minor[1] * minor[5] - minor[2] * minor[4]);
                    return (((row + col) & 1) == 0) ? det3 : -det3;
                }

                T Determinant() const noexcept
                {
                    T determinant = T(0);
                    for (int col = 0; col < 4; ++col) {
                        determinant += mat[col][0] * Cofactor(0, col);
                    }
                    return determinant;
                }

                NkMat4T Inverse() const noexcept
                {
                    NkMat4T adjugate;
                    // Calcul de la matrice adjointe (cofacteurs transposés)
                    for (int col = 0; col < 4; ++col) {
                        for (int row = 0; row < 4; ++row) {
                            adjugate.mat[col][row] = Cofactor(row, col);
                        }
                    }
                    T determinant = Determinant();
                    if (NkFabs(
                            static_cast<float64>(determinant)
                        ) < static_cast<float64>(NkMatrixEpsilon)) {
                        return Identity();  // Matrice singulière → identité de secours
                    }
                    T inverseDet = T(1) / determinant;
                    for (int index = 0; index < 16; ++index) {
                        adjugate.data[index] *= inverseDet;
                    }
                    return adjugate;
                }

                // -----------------------------------------------------------------
                // Orthogonalisation des axes de rotation
                // -----------------------------------------------------------------
                // OrthoNormalize : réorthonormalise les axes right/up/forward
                // Préserve l'orientation mais force l'orthogonalité parfaite
                void OrthoNormalize() noexcept
                {
                    // Extraction des axes 3D depuis les colonnes
                    NkVec3T<T> rightAxis(right.x, right.y, right.z);
                    NkVec3T<T> upAxis(up.x, up.y, up.z);
                    NkVec3T<T> forwardAxis(forward.x, forward.y, forward.z);

                    // Étape 1 : normaliser l'axe right
                    rightAxis.Normalize();

                    // Étape 2 : projeter up sur right et soustraire, puis normaliser
                    upAxis = (upAxis - upAxis.Dot(rightAxis) * rightAxis).Normalized();

                    // Étape 3 : projeter forward sur right et up, soustraire, normaliser
                    forwardAxis = (
                        forwardAxis
                        - forwardAxis.Dot(rightAxis) * rightAxis
                        - forwardAxis.Dot(upAxis) * upAxis
                    ).Normalized();

                    // Reconstruction de la matrice avec axes orthonormés
                    right = NkVec4T<T>(rightAxis, T(0));
                    up = NkVec4T<T>(upAxis, T(0));
                    forward = NkVec4T<T>(forwardAxis, T(0));
                }

                // OrthoNormalizeScaled : comme OrthoNormalize mais préserve les scales
                void OrthoNormalizeScaled() noexcept
                {
                    NkVec3T<T> rightAxis(right.x, right.y, right.z);
                    NkVec3T<T> upAxis(up.x, up.y, up.z);
                    NkVec3T<T> forwardAxis(forward.x, forward.y, forward.z);

                    // Extraction des facteurs d'échelle depuis les normes des axes
                    NkVec3T<T> scaleFactors = NkVec3T<T>(
                        rightAxis.Len(),
                        upAxis.Len(),
                        forwardAxis.Len()
                    );

                    // Orthogonalisation des directions
                    rightAxis.Normalize();
                    upAxis = (upAxis - upAxis.Dot(rightAxis) * rightAxis).Normalized();
                    forwardAxis = (
                        forwardAxis
                        - forwardAxis.Dot(rightAxis) * rightAxis
                        - forwardAxis.Dot(upAxis) * upAxis
                    ).Normalized();

                    // Réapplication des facteurs d'échelle aux axes normalisés
                    right = NkVec4T<T>(rightAxis * scaleFactors.x, T(0));
                    up = NkVec4T<T>(upAxis * scaleFactors.y, T(0));
                    forward = NkVec4T<T>(forwardAxis * scaleFactors.z, T(0));
                }

                // Versions immuables retournant une nouvelle matrice
                NkMat4T OrthoNormalized() const noexcept
                {
                    NkMat4T copy(*this);
                    copy.OrthoNormalize();
                    return copy;
                }

                NkMat4T OrthoNormalizeScaled() const noexcept
                {
                    NkMat4T copy(*this);
                    copy.OrthoNormalizeScaled();
                    return copy;
                }

                // -----------------------------------------------------------------
                // Transformation de points / vecteurs / normales
                // -----------------------------------------------------------------
                // TransformPoint : transforme un point 3D avec division perspective
                // Gère correctement les coordonnées homogènes (w != 1)
                NkVec3T<T> TransformPoint(const NkVec3T<T>& point) const noexcept
                {
                    NkVec4T<T> result = (*this) * NkVec4T<T>(point, T(1));
                    if (result.w == T(0) || result.w == T(1)) {
                        return result.xyz();
                    }
                    return result.xyz() / result.w;
                }

                // TransformVector : transforme un vecteur directionnel (w=0)
                // Ignore la translation, applique uniquement rotation+scale
                NkVec3T<T> TransformVector(const NkVec3T<T>& vector) const noexcept
                {
                    return (*this) * NkVec4T<T>(vector, T(0));
                }

                // TransformNormal : transforme une normale via inverse-transposée
                // Préserve la perpendicularité après transformations non-uniformes
                NkVec3T<T> TransformNormal(const NkVec3T<T>& normal) const noexcept
                {
                    NkMat4T normalMatrix = Inverse().Transpose();
                    return NkVec3T<T>(
                        normal.x * normalMatrix.m00 + normal.y * normalMatrix.m10
                            + normal.z * normalMatrix.m20,
                        normal.x * normalMatrix.m01 + normal.y * normalMatrix.m11
                            + normal.z * normalMatrix.m21,
                        normal.x * normalMatrix.m02 + normal.y * normalMatrix.m12
                            + normal.z * normalMatrix.m22
                    ).Normalized();
                }

                // -----------------------------------------------------------------
                // Extraction TRS robuste (via quaternion — résistant au gimbal)
                // -----------------------------------------------------------------
                // Extract : décompose la matrice en Translation, Rotation (Euler), Scale
                // Algorithme : Shoemake 1994 via quaternion intermédiaire
                // Évite le gimbal lock grâce à la détection de singularité
                void Extract(
                    NkVec3T<T>& outPosition,
                    NkEulerAngle& outEuler,
                    NkVec3T<T>& outScale
                ) const noexcept;

                // -----------------------------------------------------------------
                // Fabriques statiques : matrices de transformation prédéfinies
                // -----------------------------------------------------------------
                // Translation : matrice de translation pure
                static NkMat4T Translation(const NkVec3T<T>& position) noexcept
                {
                    NkMat4T result(T(1));
                    result.position = NkVec4T<T>(position, T(1));
                    return result;
                }

                // Scaling : matrice de mise à l'échelle diagonale
                static NkMat4T Scaling(const NkVec3T<T>& scaleFactors) noexcept
                {
                    NkMat4T result(T(1));
                    result.mat[0][0] = scaleFactors.x;
                    result.mat[1][1] = scaleFactors.y;
                    result.mat[2][2] = scaleFactors.z;
                    return result;
                }

                // Rotation autour d'un axe arbitraire (formule de Rodrigues)
                static NkMat4T Rotation(
                    const NkVec3T<T>& axis,
                    const NkAngle& angle
                ) noexcept {
                    float64 cosine = NkCos(angle.Rad());
                    float64 sine = NkSin(angle.Rad());
                    float64 oneMinusCos = 1.0f - cosine;
                    NkVec3T<T> normalizedAxis = axis.Normalized();
                    NkMat4T result(T(1));
                    result.mat[0][0] = T(cosine + normalizedAxis.x * normalizedAxis.x * oneMinusCos);
                    result.mat[0][1] = T(normalizedAxis.x * normalizedAxis.y * oneMinusCos + normalizedAxis.z * sine);
                    result.mat[0][2] = T(normalizedAxis.x * normalizedAxis.z * oneMinusCos - normalizedAxis.y * sine);
                    result.mat[1][0] = T(normalizedAxis.y * normalizedAxis.x * oneMinusCos - normalizedAxis.z * sine);
                    result.mat[1][1] = T(cosine + normalizedAxis.y * normalizedAxis.y * oneMinusCos);
                    result.mat[1][2] = T(normalizedAxis.y * normalizedAxis.z * oneMinusCos + normalizedAxis.x * sine);
                    result.mat[2][0] = T(normalizedAxis.z * normalizedAxis.x * oneMinusCos + normalizedAxis.y * sine);
                    result.mat[2][1] = T(normalizedAxis.z * normalizedAxis.y * oneMinusCos - normalizedAxis.x * sine);
                    result.mat[2][2] = T(cosine + normalizedAxis.z * normalizedAxis.z * oneMinusCos);
                    return result;
                }

                // Rotation autour de l'axe X (pitch / tangage)
                static NkMat4T RotationX(const NkAngle& pitch) noexcept
                {
                    float64 cosine = NkCos(pitch.Rad());
                    float64 sine = NkSin(pitch.Rad());
                    NkMat4T result(T(1));
                    result.mat[1][1] = T(cosine);
                    result.mat[1][2] = T(-sine);
                    result.mat[2][1] = T(sine);
                    result.mat[2][2] = T(cosine);
                    return result;
                }

                // Rotation autour de l'axe Y (yaw / lacet)
                static NkMat4T RotationY(const NkAngle& yaw) noexcept
                {
                    float64 cosine = NkCos(yaw.Rad());
                    float64 sine = NkSin(yaw.Rad());
                    NkMat4T result(T(1));
                    result.mat[0][0] = T(cosine);
                    result.mat[0][2] = T(-sine);
                    result.mat[2][0] = T(sine);
                    result.mat[2][2] = T(cosine);
                    return result;
                }

                // Rotation autour de l'axe Z (roll / roulis)
                static NkMat4T RotationZ(const NkAngle& roll) noexcept
                {
                    float64 cosine = NkCos(roll.Rad());
                    float64 sine = NkSin(roll.Rad());
                    NkMat4T result(T(1));
                    result.mat[0][0] = T(cosine);
                    result.mat[0][1] = T(sine);
                    result.mat[1][0] = T(-sine);
                    result.mat[1][1] = T(cosine);
                    return result;
                }

                // Rotation depuis angles d'Euler (ordre ZYX : roll → yaw → pitch)
                static NkMat4T Rotation(const NkEulerAngle& euler) noexcept
                {
                    return RotationZ(euler.roll)
                         * RotationY(euler.yaw)
                         * RotationX(euler.pitch);
                }

                // Projection perspective (FOV vertical, aspect ratio, plans near/far)
                static NkMat4T Perspective(
                    const NkAngle& fieldOfViewY,
                    T aspectRatio,
                    T nearPlane,
                    T farPlane,
                    bool depthZeroToOne = false
                ) noexcept {
                    float64 tanHalfFov = NkTan(fieldOfViewY.Rad() * 0.5f);
                    NkMat4T result;
                    result.mat[0][0] = T(1.0f / (aspectRatio * tanHalfFov));
                    result.mat[1][1] = T(1.0f / tanHalfFov);
                    result.mat[2][3] = T(-1);
                    if (depthZeroToOne) {
                        // Vulkan / DirectX 11/12 : profondeur dans [0, 1]
                        result.mat[2][2] = T(farPlane / (nearPlane - farPlane));
                        result.mat[3][2] = T(farPlane * nearPlane / (nearPlane - farPlane));
                    } else {
                        // OpenGL : profondeur dans [-1, 1]
                        result.mat[2][2] = T((farPlane + nearPlane) / (nearPlane - farPlane));
                        result.mat[3][2] = T(2.0f * farPlane * nearPlane / (nearPlane - farPlane));
                    }
                    return result;
                }

                // Projection orthogonale (largeur × hauteur, plans near/far)
                static NkMat4T Orthogonal(
                    T width,
                    T height,
                    T nearPlane,
                    T farPlane,
                    bool depthZeroToOne = false
                ) noexcept {
                    NkMat4T result(T(1));
                    result.mat[0][0] = T(2) / width;
                    result.mat[1][1] = T(2) / height;
                    if (depthZeroToOne) {
                        result.mat[2][2] = T(1) / (nearPlane - farPlane);
                        result.mat[3][2] = nearPlane / (nearPlane - farPlane);
                    } else {
                        result.mat[2][2] = T(2) / (nearPlane - farPlane);
                        result.mat[3][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
                    }
                    return result;
                }

                // Projection orthogonale avec coins explicits (2D UI, ortho camera)
                static NkMat4T Orthogonal(
                    const NkVec2T<T>& bottomLeft,
                    const NkVec2T<T>& topRight,
                    T nearPlane,
                    T farPlane,
                    bool depthZeroToOne = false
                ) noexcept {
                    NkMat4T result(T(1));
                    T width = topRight.x - bottomLeft.x;
                    T height = topRight.y - bottomLeft.y;
                    result.mat[0][0] = T(2) / width;
                    result.mat[1][1] = T(2) / height;
                    result.mat[3][0] = -(topRight.x + bottomLeft.x) / width;
                    result.mat[3][1] = -(topRight.y + bottomLeft.y) / height;
                    if (depthZeroToOne) {
                        result.mat[2][2] = T(1) / (nearPlane - farPlane);
                        result.mat[3][2] = nearPlane / (nearPlane - farPlane);
                    } else {
                        result.mat[2][2] = T(2) / (nearPlane - farPlane);
                        result.mat[3][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
                    }
                    return result;
                }

                // Matrice de vue LookAt (convention OpenGL, column-major)
                static NkMat4T LookAt(
                    const NkVec3T<T>& eyePosition,
                    const NkVec3T<T>& targetCenter,
                    const NkVec3T<T>& worldUp
                ) noexcept {
                    // Calcul des axes du repère caméra
                    NkVec3T<T> forwardAxis = (targetCenter - eyePosition).Normalized();
                    NkVec3T<T> rightAxis = forwardAxis.Cross(worldUp).Normalized();
                    NkVec3T<T> upAxis = rightAxis.Cross(forwardAxis);
                    // Construction de la matrice de vue
                    NkMat4T viewMatrix;
                    viewMatrix.col[0] = NkVec4T<T>(rightAxis, T(0));
                    viewMatrix.col[1] = NkVec4T<T>(upAxis, T(0));
                    viewMatrix.col[2] = NkVec4T<T>(-forwardAxis, T(0));  // Z = -forward
                    viewMatrix.col[3] = NkVec4T<T>(
                        -rightAxis.Dot(eyePosition),
                        -upAxis.Dot(eyePosition),
                        forwardAxis.Dot(eyePosition),
                        T(1)
                    );
                    return viewMatrix;
                }

                // Matrice de réflexion par rapport à un plan de normale donnée
                static NkMat4T Reflection(const NkVec3T<T>& planeNormal) noexcept
                {
                    NkVec3T<T> normalizedNormal = planeNormal.Normalized();
                    NkMat4T result(T(1));
                    T nx = normalizedNormal.x;
                    T ny = normalizedNormal.y;
                    T nz = normalizedNormal.z;
                    result.mat[0][0] = T(1) - T(2) * nx * nx;
                    result.mat[0][1] = -T(2) * nx * ny;
                    result.mat[0][2] = -T(2) * nx * nz;
                    result.mat[1][0] = -T(2) * ny * nx;
                    result.mat[1][1] = T(1) - T(2) * ny * ny;
                    result.mat[1][2] = -T(2) * ny * nz;
                    result.mat[2][0] = -T(2) * nz * nx;
                    result.mat[2][1] = -T(2) * nz * ny;
                    result.mat[2][2] = T(1) - T(2) * nz * nz;
                    return result;
                }

                // -----------------------------------------------------------------
                // Conversion Row-Major (pour DirectX / HLSL)
                // -----------------------------------------------------------------
                // ToRowMajor : copie les 16 valeurs en layout row-major dans output
                // Utiliser avant upload vers un cbuffer DirectX/HLSL sans transposition
                void ToRowMajor(T output[16]) const noexcept
                {
                    for (int col = 0; col < 4; ++col) {
                        for (int row = 0; row < 4; ++row) {
                            output[row * 4 + col] = mat[col][row];
                        }
                    }
                }

                // AsRowMajor : alias sémantique retournant la transposée
                // Équivaut à un layout row-major pour le GPU sans modifier this
                NK_FORCE_INLINE NkMat4T AsRowMajor() const noexcept
                {
                    return Transpose();
                }

                // -----------------------------------------------------------------
                // Sérialisation et E/S
                // -----------------------------------------------------------------
                // ToString : conversion en chaîne formatée avec précision .4
                NkString ToString() const
                {
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

                friend NkString ToString(const NkMat4T& matrix) { return matrix.ToString(); }

                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkMat4T& matrix
                ) {
                    return outputStream << matrix.ToString().CStr();
                }

            };  // struct NkMat4T

            // =================================================================
            // Implémentation de NkMat4T<T>::Extract (dans header car template)
            // =================================================================
            // Extraction TRS robuste via quaternion intermédiaire (Shoemake 1994).
            //
            // Étapes de l'algorithme :
            //   1. Extraire l'échelle : norme de chaque axe-colonne
            //   2. Extraire la translation : colonne 3, éléments 0-2
            //   3. Normaliser les 3 premières colonnes → matrice de rotation pure R
            //   4. Construire quaternion depuis R (méthode numériquement stable)
            //   5. Extraire Euler depuis quaternion avec gestion du gimbal lock
            //      via test du "pôle de Gimbal" (t = qy*qx + qz*qw)
            //
            // Cette approche évite le gimbal lock de la conversion directe
            // matrice→Euler grâce à l'étape quaternion intermédiaire.
            // =================================================================
            template <typename T>
            void NkMat4T<T>::Extract(
                NkVec3T<T>& outPosition,
                NkEulerAngle& outEuler,
                NkVec3T<T>& outScale
            ) const noexcept {

                // ── 1. Extraction de l'échelle (norme de chaque axe-colonne) ───
                outScale.x = NkVec3T<T>(mat[0][0], mat[0][1], mat[0][2]).Len();
                outScale.y = NkVec3T<T>(mat[1][0], mat[1][1], mat[1][2]).Len();
                outScale.z = NkVec3T<T>(mat[2][0], mat[2][1], mat[2][2]).Len();

                // ── 2. Extraction de la translation (colonne 3, éléments 0-2) ──
                outPosition = NkVec3T<T>(mat[3][0], mat[3][1], mat[3][2]);

                // ── 3. Construction de la matrice de rotation pure R ───────────
                // Normalisation des colonnes pour isoler la rotation du scale
                float64 inverseScaleX = (outScale.x > T(0)) ? T(1) / outScale.x : T(0);
                float64 inverseScaleY = (outScale.y > T(0)) ? T(1) / outScale.y : T(0);
                float64 inverseScaleZ = (outScale.z > T(0)) ? T(1) / outScale.z : T(0);

                float64 r00 = static_cast<float64>(mat[0][0]) * inverseScaleX;
                float64 r01 = static_cast<float64>(mat[0][1]) * inverseScaleX;
                float64 r02 = static_cast<float64>(mat[0][2]) * inverseScaleX;
                float64 r10 = static_cast<float64>(mat[1][0]) * inverseScaleY;
                float64 r11 = static_cast<float64>(mat[1][1]) * inverseScaleY;
                float64 r12 = static_cast<float64>(mat[1][2]) * inverseScaleY;
                float64 r20 = static_cast<float64>(mat[2][0]) * inverseScaleZ;
                float64 r21 = static_cast<float64>(mat[2][1]) * inverseScaleZ;
                float64 r22 = static_cast<float64>(mat[2][2]) * inverseScaleZ;

                // ── 4. Quaternion depuis matrice de rotation (Shoemake 1994) ───
                // Sélection de la composante dominante pour stabilité numérique
                float64 trace = r00 + r11 + r22;
                float64 quaternionX, quaternionY, quaternionZ, quaternionW;

                if (trace > 0.0f) {
                    float64 scale = 0.5f / NkSqrt(trace + 1.0f);
                    quaternionW = 0.25f / scale;
                    quaternionX = (r21 - r12) * scale;
                    quaternionY = (r02 - r20) * scale;
                    quaternionZ = (r10 - r01) * scale;
                } else if (r00 > r11 && r00 > r22) {
                    float64 scale = 2.0f * NkSqrt(1.0f + r00 - r11 - r22);
                    quaternionW = (r21 - r12) / scale;
                    quaternionX = 0.25f * scale;
                    quaternionY = (r01 + r10) / scale;
                    quaternionZ = (r02 + r20) / scale;
                } else if (r11 > r22) {
                    float64 scale = 2.0f * NkSqrt(1.0f + r11 - r00 - r22);
                    quaternionW = (r02 - r20) / scale;
                    quaternionX = (r01 + r10) / scale;
                    quaternionY = 0.25f * scale;
                    quaternionZ = (r12 + r21) / scale;
                } else {
                    float64 scale = 2.0f * NkSqrt(1.0f + r22 - r00 - r11);
                    quaternionW = (r10 - r01) / scale;
                    quaternionX = (r02 + r20) / scale;
                    quaternionY = (r12 + r21) / scale;
                    quaternionZ = 0.25f * scale;
                }

                // Normalisation du quaternion pour garantir l'unité
                float64 quaternionLength = NkSqrt(
                    quaternionX * quaternionX
                    + quaternionY * quaternionY
                    + quaternionZ * quaternionZ
                    + quaternionW * quaternionW
                );
                if (quaternionLength > static_cast<float64>(NkEpsilon)) {
                    float64 inverseLength = 1.0f / quaternionLength;
                    quaternionX *= inverseLength;
                    quaternionY *= inverseLength;
                    quaternionZ *= inverseLength;
                    quaternionW *= inverseLength;
                }

                // ── 5. Quaternion → Euler avec gestion du gimbal lock ─────────
                // Détection du pôle de Gimbal : t = qy*qx + qz*qw
                float64 gimbalTest = quaternionY * quaternionX + quaternionZ * quaternionW;

                if (gimbalTest > 0.499f) {
                    // Pôle Nord : singularité à +90° de Yaw
                    // Roll et Pitch dégénèrent, on fixe Roll à 0
                    outEuler.yaw = NkAngle::FromRad(static_cast<float64>(NkPis2));
                    outEuler.pitch = NkAngle::FromRad(2.0f * NkAtan2(quaternionX, quaternionW));
                    outEuler.roll = NkAngle(0.0f);
                } else if (gimbalTest < -0.499f) {
                    // Pôle Sud : singularité à -90° de Yaw
                    // Roll et Pitch dégénèrent, on fixe Roll à 0
                    outEuler.yaw = NkAngle::FromRad(-static_cast<float64>(NkPis2));
                    outEuler.pitch = NkAngle::FromRad(-2.0f * NkAtan2(quaternionX, quaternionW));
                    outEuler.roll = NkAngle(0.0f);
                } else {
                    // Cas général : pas de singularité, conversion standard
                    // Pitch (rotation autour de X)
                    float64 sinPitch = 2.0f * (quaternionW * quaternionX + quaternionY * quaternionZ);
                    float64 cosPitch = 1.0f - 2.0f * (quaternionX * quaternionX + quaternionY * quaternionY);
                    outEuler.pitch = NkAngle::FromRad(NkAtan2(sinPitch, cosPitch));

                    // Yaw (rotation autour de Y)
                    float64 sinYaw = 2.0f * (quaternionW * quaternionY - quaternionZ * quaternionX);
                    sinYaw = NkClamp(sinYaw, -1.0, 1.0);  // Protection contre NaN
                    outEuler.yaw = NkAngle::FromRad(NkAsin(sinYaw));

                    // Roll (rotation autour de Z)
                    float64 sinRoll = 2.0f * (quaternionW * quaternionZ + quaternionX * quaternionY);
                    float64 cosRoll = 1.0f - 2.0f * (quaternionY * quaternionY + quaternionZ * quaternionZ);
                    outEuler.roll = NkAngle::FromRad(NkAtan2(sinRoll, cosRoll));
                }

            }  // NkMat4T<T>::Extract

            // =================================================================
            // Aliases de types : variantes f=float32, d=float64
            // =================================================================
            // Note : pas de variants entiers pour les matrices (pas de sens géométrique)
            // =================================================================
            using NkMatrix2f = NkMat2T<float32>;
            using NkMatrix2d = NkMat2T<float64>;
            using NkMatrix3f = NkMat3T<float32>;
            using NkMatrix3d = NkMat3T<float64>;
            using NkMatrix4f = NkMat4T<float32>;
            using NkMatrix4d = NkMat4T<float64>;

            // Alias courts pour usage courant
            using NkMat2f = NkMatrix2f;
            using NkMat2d = NkMatrix2d;
            using NkMat3f = NkMatrix3f;
            using NkMat3d = NkMatrix3d;
            using NkMat4f = NkMatrix4f;
            using NkMat4d = NkMatrix4d;

            // Alias legacy pour compatibilité avec ancien codebase
            using NkMatrix2 = NkMatrix2f;
            using NkMatrix3 = NkMatrix3f;
            using NkMatrix4 = NkMatrix4f;
            using NkMatrice4 = NkMatrix4f;  // Orthographe alternative
            using NkMat2 = NkMatrix2f;
            using NkMat3 = NkMatrix3f;
            using NkMat4 = NkMatrix4f;

        }  // namespace math

        // =====================================================================
        // Spécialisations de NkToString pour intégration au système de formatage
        // =====================================================================
        // Permet d'utiliser NkMat2T/3T/4T avec NkFormat() et les utilitaires d'E/S
        // du projet, avec support des options de formatage via NkFormatProps.
        // =====================================================================
        template <typename T>
        inline NkString NkToString(
            const math::NkMat2T<T>& matrix,
            const NkFormatProps& formatProperties = {}
        ) {
            return NkApplyFormatProps(matrix.ToString(), formatProperties);
        }

        template <typename T>
        inline NkString NkToString(
            const math::NkMat3T<T>& matrix,
            const NkFormatProps& formatProperties = {}
        ) {
            return NkApplyFormatProps(matrix.ToString(), formatProperties);
        }

        template <typename T>
        inline NkString NkToString(
            const math::NkMat4T<T>& matrix,
            const NkFormatProps& formatProperties = {}
        ) {
            return NkApplyFormatProps(matrix.ToString(), formatProperties);
        }

    }  // namespace nkentseu

#endif  // __NKENTSEU_MATRIX_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création de matrices avec différents constructeurs
    // ---------------------------------------------------------------------
    #include "NKMath/NkMatrix.h"
    using namespace nkentseu::math;

    // Matrice identité 4×4 float
    NkMat4f identity = NkMat4f::Identity();

    // Matrice de translation pure
    NkMat4f translation = NkMat4f::Translation(NkVec3f(10.0f, 5.0f, -3.0f));

    // Matrice de rotation autour de l'axe Y (yaw = 45°)
    NkMat4f rotation = NkMat4f::RotationY(NkAngle::FromDeg(45.0f));

    // Matrice de scale non-uniforme
    NkMat4f scaling = NkMat4f::Scaling(NkVec3f(2.0f, 1.0f, 0.5f));

    // Construction manuelle élément par élément (column-major)
    NkMat2f manual2x2(
        1.0f, 0.0f,   // Colonne 0 : [1, 0]
        0.0f, 1.0f    // Colonne 1 : [0, 1]
    );  // Résultat : matrice identité 2×2

    // ---------------------------------------------------------------------
    // Exemple 2 : Composition de transformations (ordre important !)
    // ---------------------------------------------------------------------
    // Ordre d'application typique : Scale → Rotate → Translate
    // En column-major avec vecteur colonne à droite : M = T * R * S
    NkMat4f modelMatrix = translation * rotation * scaling;

    // Transformation d'un point par la matrice composée
    NkVec3f localPoint(1.0f, 0.0f, 0.0f);
    NkVec3f worldPoint = modelMatrix * localPoint;

    // ---------------------------------------------------------------------
    // Exemple 3 : Matrices de vue et projection pour rendu 3D
    // ---------------------------------------------------------------------
    // Matrice de vue : caméra regardant vers (0,0,0) depuis (0,0,10)
    NkMat4f viewMatrix = NkMat4f::LookAt(
        NkVec3f(0.0f, 0.0f, 10.0f),  // Position caméra
        NkVec3f(0.0f, 0.0f, 0.0f),   // Point regardé
        NkVec3f(0.0f, 1.0f, 0.0f)    // Vecteur up mondial
    );

    // Matrice de projection perspective (FOV 60°, aspect 16:9, near=0.1, far=1000)
    NkMat4f projectionMatrix = NkMat4f::Perspective(
        NkAngle::FromDeg(60.0f),
        16.0f / 9.0f,
        0.1f,
        1000.0f,
        false  // OpenGL depth range [-1, 1]
    );

    // Matrice MVP complète pour vertex shader
    NkMat4f mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;

    // ---------------------------------------------------------------------
    // Exemple 4 : Upload vers GPU avec conversion row-major si nécessaire
    // ---------------------------------------------------------------------
    // Pour OpenGL/Vulkan (column-major natif) :
    // glUniformMatrix4fv(location, 1, GL_FALSE, mvpMatrix.data);

    // Pour DirectX/HLSL (row-major attendu) :
    float32 rowMajorData[16];
    mvpMatrix.ToRowMajor(rowMajorData);
    // Puis upload vers cbuffer : context->UpdateSubresource(..., rowMajorData, ...);

    // Alternative : utiliser AsRowMajor() pour obtenir la transposée
    NkMat4f rowMajorMatrix = mvpMatrix.AsRowMajor();
    // Puis upload : context->UpdateSubresource(..., rowMajorMatrix.data, ...);

    // ---------------------------------------------------------------------
    // Exemple 5 : Extraction TRS depuis une matrice composée
    // ---------------------------------------------------------------------
    NkMat4f complexTransform = /\* matrice TRS quelconque *\/;

    NkVec3f extractedPosition;
    NkEulerAngle extractedEuler;
    NkVec3f extractedScale;

    complexTransform.Extract(extractedPosition, extractedEuler, extractedScale);

    // Utilisation : reconstruction ou interpolation de transformations
    NkMat4f rebuilt = NkMat4f::Translation(extractedPosition)
                    * NkMat4f::Rotation(extractedEuler)
                    * NkMat4f::Scaling(extractedScale);

    // ---------------------------------------------------------------------
    // Exemple 6 : Transformations de points, vecteurs et normales
    // ---------------------------------------------------------------------
    NkMat4f transform = /\* matrice avec rotation non-uniforme *\/;

    // Point : translation + rotation + scale + division perspective si w!=1
    NkVec3f point(1.0f, 2.0f, 3.0f);
    NkVec3f transformedPoint = transform.TransformPoint(point);

    // Vecteur directionnel : ignore la translation (w=0)
    NkVec3f direction(0.0f, 1.0f, 0.0f);
    NkVec3f transformedDirection = transform.TransformVector(direction);

    // Normale : utilise inverse-transposée pour préserver la perpendicularité
    NkVec3f normal(0.0f, 0.0f, 1.0f);
    NkVec3f transformedNormal = transform.TransformNormal(normal);

    // ---------------------------------------------------------------------
    // Exemple 7 : Orthogonalisation pour corriger la dérive numérique
    // ---------------------------------------------------------------------
    // Après de nombreuses multiplications, les axes peuvent perdre l'orthogonalité
    NkMat4f accumulatedTransform = /* résultat de nombreuses opérations */;

    // Réorthonormalisation sans préserver le scale
    accumulatedTransform.OrthoNormalize();

    // Ou avec préservation des facteurs d'échelle
    accumulatedTransform.OrthoNormalizeScaled();

    // Utilisation typique : dans une boucle de mise à jour de caméra
    void UpdateCameraTransform(NkMat4f& cameraMatrix)
    {
        // Application de petites rotations incrémentales
        cameraMatrix *= NkMat4f::RotationY(NkAngle::FromDeg(0.1f));

        // Correction périodique pour éviter la dérive des axes
        static int frameCount = 0;
        if (++frameCount % 100 == 0) {
            cameraMatrix.OrthoNormalize();
        }
    }

    // ---------------------------------------------------------------------
    // Exemple 8 : Matrices 2×2 et 3×3 pour cas spécialisés
    // ---------------------------------------------------------------------
    // Matrice 2×2 pour transformations 2D (rotation, scale, shearing)
    NkMat2f rotation2D = NkMat2f(
        NkCos(NkAngle::FromDeg(30.0f).Rad()), -NkSin(NkAngle::FromDeg(30.0f).Rad()),
        NkSin(NkAngle::FromDeg(30.0f).Rad()),  NkCos(NkAngle::FromDeg(30.0f).Rad())
    );

    NkVec2f point2D(1.0f, 0.0f);
    NkVec2f rotated2D = rotation2D * point2D;

    // Matrice 3×3 pour projections 2D ou transformations affines 3D sans translation
    NkMat3f projection2D = NkMat3f::Identity();
    projection2D.mat[0][0] = 2.0f / 800.0f;   // Scale X pour écran 800px
    projection2D.mat[1][1] = 2.0f / 600.0f;   // Scale Y pour écran 600px
    projection2D.mat[2][0] = -1.0f;           // Translation X vers [-1, 1]
    projection2D.mat[2][1] = -1.0f;           // Translation Y vers [-1, 1]

    // ---------------------------------------------------------------------
    // Exemple 9 : Comparaisons et tolérance flottante
    // ---------------------------------------------------------------------
    NkMat4f matrixA = NkMat4f::Identity();
    NkMat4f matrixB = NkMat4f::Identity();

    // Égalité avec tolérance via NkMatrixEpsilon
    bool areEqual = (matrixA == matrixB);  // true

    // Modification infime dans la tolérance
    matrixB.mat[0][0] = 1.0f + NkMatrixEpsilon * 0.5f;
    bool stillEqual = (matrixA == matrixB);  // true (dans la tolérance)

    // Modification au-delà de la tolérance
    matrixB.mat[0][0] = 1.0f + NkMatrixEpsilon * 2.0f;
    bool different = (matrixA != matrixB);  // true

    // ---------------------------------------------------------------------
    // Exemple 10 : Affichage et débogage
    // ---------------------------------------------------------------------
    NkMat4f debugMatrix = NkMat4f::Perspective(
        NkAngle::FromDeg(90.0f), 1.0f, 1.0f, 100.0f
    );

    // Via méthode membre ToString()
    NkString matrixStr = debugMatrix.ToString();
    // Résultat : "NkMat4T[\n  [1.0000, 0.0000, ...]\n  ..."

    // Via flux de sortie standard
    std::cout << "Projection matrix:\n" << debugMatrix << std::endl;

    // Via fonction globale NkToString avec formatage personnalisé
    NkString formatted = NkToString(
        debugMatrix,
        NkFormatProps().SetPrecision(6)
    );  // Affiche avec 6 décimales au lieu de 4

    // ---------------------------------------------------------------------
    // Bonnes pratiques d'utilisation
    // ---------------------------------------------------------------------
    // ✓ Toujours vérifier l'ordre de multiplication : T * R * S ≠ S * R * T
    // ✓ Utiliser TransformPoint/Vector/Normal selon le type de donnée
    // ✓ Appeler OrthoNormalize() périodiquement pour les matrices accumulées
    // ✓ Utiliser Extract() pour interpolation ou reconstruction de TRS
    // ✓ ToRowMajor() avant upload DirectX, pas de conversion pour OpenGL/Vulkan
    // ✗ Éviter l'inversion répétée : pré-calculer et réutiliser Inverse() si possible
    // ✗ Attention au gimbal lock : préférer les quaternions pour les rotations complexes
*/