//
// NkVec.h
// =============================================================================
// Description :
//   Définition des structures de vecteurs génériques NkVec2T, NkVec3T, NkVec4T
//   pour les calculs géométriques 2D, 3D et 4D.
//
// Types disponibles (suffixes de type) :
//   f = float32, d = float64, i = int32, u = uint32
//   Exemple : NkVector3f = NkVec3T<float32>
//
// Alias courts :
//   NkVec2f, NkVec3f, NkVec4f (et variantes d/i/u)
//
// Notes d'implémentation :
//   - Toutes les méthodes sont inline car ce sont des templates
//   - Les algorithmes géométriques (SLerp, Cross, Reflect...) sont implémentés
//     uniquement pour NkVec3T car ils ont un sens physique en 3D
//   - NkVec2T expose les équivalents 2D lorsque cela a du sens
//
// Auteur   : Rihen
// Copyright: (c) 2024 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_VECTOR_H__
#define __NKENTSEU_VECTOR_H__

    // =====================================================================
    // Inclusions des dépendances
    // =====================================================================
    #include "NKMath/NkLegacySystem.h"      // Types fondamentaux : float32, int32, etc.
    #include "NKMath/NkFunctions.h"         // Fonctions mathématiques : NkSqrt, NkClamp, etc.
    #include "NKMath/NkAngle.h"             // Type NkAngle pour les conversions angulaires
    #include "NKContainers/String/NkFormat.h" // Système de formatage de chaînes
    #include "NKCore/NkTraits.h"            // Traits de métaprogrammation : NkIsFloatingPoint_v

    // =====================================================================
    // Namespace principal du projet
    // =====================================================================
    namespace nkentseu {

        // =================================================================
        // Sous-namespace mathématique : types et opérations géométriques
        // =================================================================
        namespace math {

            // =================================================================
            // Déclarations anticipées pour les conversions croisées
            // =================================================================
            template <typename T> struct NkVec2T;
            template <typename T> struct NkVec3T;
            template <typename T> struct NkVec4T;

            // =================================================================
            // Structure : NkVec2T<T>
            // =================================================================
            // Vecteur 2D générique pour coordonnées, UV, tailles, couleurs.
            // Opérations disponibles : arithmétique, produit scalaire,
            // normalisation, interpolation, réflexions 2D.
            // Note : Pas de produit vectoriel 3D (résultat scalaire via Collinear).
            // =================================================================
            template <typename T>
            struct NkVec2T {

                // -----------------------------------------------------------------
                // Union de membres : accès sémantique multiple aux mêmes données
                // -----------------------------------------------------------------
                union {
                    struct { T x, y; };              // Coordonnées cartésiennes
                    struct { T u, v; };              // Coordonnées de texture
                    struct { T width, height; };     // Dimensions rectangulaires
                    struct { T r, g; };              // Composantes couleur (RG)
                    T data[2];                        // Accès tableau brut
                };

                // -----------------------------------------------------------------
                // Section : Constructeurs
                // -----------------------------------------------------------------
                // Constructeur par défaut : initialise à (0, 0)
                constexpr NkVec2T() noexcept
                    : x(T(0))
                    , y(T(0))
                {
                }

                // Constructeur scalaire : remplit les deux composantes avec la même valeur
                constexpr explicit NkVec2T(T scalar) noexcept
                    : x(scalar)
                    , y(scalar)
                {
                }

                // Constructeur paramétré : initialise avec des valeurs explicites
                constexpr NkVec2T(T x, T y) noexcept
                    : x(x)
                    , y(y)
                {
                }

                // Constructeur depuis tableau : copie les deux premières valeurs
                constexpr explicit NkVec2T(const T* pointer) noexcept
                    : x(pointer[0])
                    , y(pointer[1])
                {
                }

                // Constructeur de copie : généré par défaut (optimisation compilateur)
                NkVec2T(const NkVec2T&) noexcept = default;

                // Opérateur d'affectation : généré par défaut (optimisation compilateur)
                NkVec2T& operator=(const NkVec2T&) noexcept = default;

                // Constructeur de conversion cross-type explicite (ex: float→int)
                template <typename U>
                constexpr explicit NkVec2T(const NkVec2T<U>& other) noexcept
                    : x(static_cast<T>(other.x))
                    , y(static_cast<T>(other.y))
                {
                }

                // Opérateur d'affectation cross-type
                template <typename U>
                NkVec2T& operator=(const NkVec2T<U>& other)
                {
                    x = static_cast<T>(other.x);
                    y = static_cast<T>(other.y);
                    return *this;
                }

                // -----------------------------------------------------------------
                // Section : Conversions et accesseurs bruts
                // -----------------------------------------------------------------
                // Conversion implicite vers pointeur mutable
                operator T*() noexcept
                {
                    return data;
                }

                // Conversion implicite vers pointeur constant
                operator const T*() const noexcept
                {
                    return data;
                }

                // Conversion vers vecteur de type différent (template)
                template <typename U>
                operator NkVec2T<U>() noexcept
                {
                    return NkVec2T<U>(x, y);
                }

                // Conversion vers vecteur de type différent constant (template)
                template <typename U>
                operator const NkVec2T<U>() const noexcept
                {
                    return NkVec2T<U>(x, y);
                }

                // Opérateur d'indexation mutable avec masquage de borne (0 ou 1)
                NK_FORCE_INLINE T& operator[](size_t index) noexcept
                {
                    return data[index & 1];
                }

                // Opérateur d'indexation constant avec masquage de borne (0 ou 1)
                NK_FORCE_INLINE const T& operator[](size_t index) const noexcept
                {
                    return data[index & 1];
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs arithmétiques unaires et binaires
                // -----------------------------------------------------------------
                // Négation unaire : retourne le vecteur opposé
                NK_FORCE_INLINE NkVec2T operator-() const noexcept
                {
                    return { -x, -y };
                }

                // Addition composante par composante
                NK_FORCE_INLINE NkVec2T operator+(const NkVec2T& other) const noexcept
                {
                    return { x + other.x, y + other.y };
                }

                // Soustraction composante par composante
                NK_FORCE_INLINE NkVec2T operator-(const NkVec2T& other) const noexcept
                {
                    return { x - other.x, y - other.y };
                }

                // Multiplication composante par composante (produit de Hadamard)
                NK_FORCE_INLINE NkVec2T operator*(const NkVec2T& other) const noexcept
                {
                    return { x * other.x, y * other.y };
                }

                // Division composante par composante
                NK_FORCE_INLINE NkVec2T operator/(const NkVec2T& other) const noexcept
                {
                    return { x / other.x, y / other.y };
                }

                // Multiplication par scalaire
                NK_FORCE_INLINE NkVec2T operator*(T scalar) const noexcept
                {
                    return { x * scalar, y * scalar };
                }

                // Division par scalaire (optimisée par inversion multiplicative)
                NK_FORCE_INLINE NkVec2T operator/(T scalar) const noexcept
                {
                    T inverse = T(1) / scalar;
                    return { x * inverse, y * inverse };
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs d'affectation composée
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec2T& operator+=(const NkVec2T& other) noexcept
                {
                    x += other.x;
                    y += other.y;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T& operator-=(const NkVec2T& other) noexcept
                {
                    x -= other.x;
                    y -= other.y;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T& operator*=(const NkVec2T& other) noexcept
                {
                    x *= other.x;
                    y *= other.y;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T& operator/=(const NkVec2T& other) noexcept
                {
                    x /= other.x;
                    y /= other.y;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T& operator*=(T scalar) noexcept
                {
                    x *= scalar;
                    y *= scalar;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T& operator/=(T scalar) noexcept
                {
                    T inverse = T(1) / scalar;
                    x *= inverse;
                    y *= inverse;
                    return *this;
                }

                // Multiplication scalaire commutative (fonction amie)
                friend NK_FORCE_INLINE NkVec2T operator*(T scalar, const NkVec2T& vector) noexcept
                {
                    return vector * scalar;
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs d'incrémentation/décrémentation
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec2T& operator++() noexcept
                {
                    ++x;
                    ++y;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T& operator--() noexcept
                {
                    --x;
                    --y;
                    return *this;
                }

                NK_FORCE_INLINE NkVec2T operator++(int) noexcept
                {
                    NkVec2T temporary(*this);
                    ++(*this);
                    return temporary;
                }

                NK_FORCE_INLINE NkVec2T operator--(int) noexcept
                {
                    NkVec2T temporary(*this);
                    --(*this);
                    return temporary;
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs de comparaison
                // -----------------------------------------------------------------
                // Égalité avec tolérance flottante si T est un type floating-point
                friend NK_FORCE_INLINE bool operator==(
                    const NkVec2T& left,
                    const NkVec2T& right
                ) noexcept {
                    if constexpr (traits::NkIsFloatingPoint_v<T>) {
                        return NkFabs(left.x - right.x) <= T(NkVectorEpsilon)
                            && NkFabs(left.y - right.y) <= T(NkVectorEpsilon);
                    } else {
                        return (left.x == right.x) && (left.y == right.y);
                    }
                }

                // Inégalité déléguée à l'égalité (principe DRY)
                friend NK_FORCE_INLINE bool operator!=(
                    const NkVec2T& left,
                    const NkVec2T& right
                ) noexcept {
                    return !(left == right);
                }

                // -----------------------------------------------------------------
                // Section : Produit scalaire et métriques de longueur
                // -----------------------------------------------------------------
                // Produit scalaire (dot product) : mesure de l'alignement
                NK_FORCE_INLINE T Dot(const NkVec2T& other) const noexcept
                {
                    return x * other.x + y * other.y;
                }

                // Carré de la longueur : évite un sqrt coûteux pour les comparaisons
                NK_FORCE_INLINE T LenSq() const noexcept
                {
                    return x * x + y * y;
                }

                // Longueur euclidienne : racine carrée de LenSq avec garde epsilon
                NK_FORCE_INLINE T Len() const noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkVectorEpsilon)) {
                        return T(0);
                    }
                    return static_cast<T>(
                        NkSqrt(static_cast<float32>(lengthSquared))
                    );
                }

                // Distance euclidienne vers un autre vecteur
                NK_FORCE_INLINE T Distance(const NkVec2T& other) const noexcept
                {
                    return (*this - other).Len();
                }

                // -----------------------------------------------------------------
                // Section : Normalisation
                // -----------------------------------------------------------------
                // Normalisation en place : modifie le vecteur courant
                void Normalize() noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkVectorEpsilon)) {
                        return;
                    }
                    T inverseLength = T(1)
                        / static_cast<T>(NkSqrt(static_cast<float32>(lengthSquared)));
                    x *= inverseLength;
                    y *= inverseLength;
                }

                // Normalisation immuable : retourne un nouveau vecteur normalisé
                NK_FORCE_INLINE NkVec2T Normalized() const noexcept
                {
                    NkVec2T copy(*this);
                    copy.Normalize();
                    return copy;
                }

                // Test d'unité : vérifie si la longueur est proche de 1
                NK_FORCE_INLINE bool IsUnit() const noexcept
                {
                    return NkFabs(
                        static_cast<float32>(LenSq()) - 1.0f
                    ) < 2.0f * static_cast<float32>(NkEpsilon);
                }

                // -----------------------------------------------------------------
                // Section : Opérations géométriques 2D
                // -----------------------------------------------------------------
                // Normale perpendiculaire (rotation 90° anti-horaire)
                NK_FORCE_INLINE NkVec2T Normal() const noexcept
                {
                    return { -y, x };
                }

                // Rotation de 90° anti-horaire (alias de Normal)
                NK_FORCE_INLINE NkVec2T Rotate90() const noexcept
                {
                    return { -y, x };
                }

                // Rotation de 180° (inversion complète)
                NK_FORCE_INLINE NkVec2T Rotate180() const noexcept
                {
                    return { -x, -y };
                }

                // Rotation de 270° anti-horaire (ou 90° horaire)
                NK_FORCE_INLINE NkVec2T Rotate270() const noexcept
                {
                    return { y, -x };
                }

                // Projection orthogonale de ce vecteur sur 'onto'
                NK_FORCE_INLINE NkVec2T Project(const NkVec2T& onto) const noexcept
                {
                    T denominator = onto.LenSq();
                    if (denominator < T(NkVectorEpsilon)) {
                        return {};
                    }
                    T factor = Dot(onto) / denominator;
                    return onto * factor;
                }

                // Rejet : composante perpendiculaire à 'onto'
                NK_FORCE_INLINE NkVec2T Reject(const NkVec2T& onto) const noexcept
                {
                    return *this - Project(onto);
                }

                // Réflexion par rapport à une normale unitaire 'n'
                NK_FORCE_INLINE NkVec2T Reflect(const NkVec2T& normal) const noexcept
                {
                    return *this - normal * (T(2) * Dot(normal));
                }

                // -----------------------------------------------------------------
                // Section : Interpolation
                // -----------------------------------------------------------------
                // Interpolation linéaire : t ∈ [0, 1] entre *this et 'to'
                NK_FORCE_INLINE NkVec2T Lerp(const NkVec2T& to, float32 t) const noexcept
                {
                    return *this + (to - *this) * static_cast<T>(t);
                }

                // Lerp puis normalisation : utile pour interpoler des directions
                NK_FORCE_INLINE NkVec2T NLerp(const NkVec2T& to, float32 t) const noexcept
                {
                    return Lerp(to, t).Normalized();
                }

                // Spherical Linear Interpolation (SLerp) sur le cercle unitaire
                NkVec2T SLerp(const NkVec2T& to, float32 t) const noexcept
                {
                    if (t < 0.01f) {
                        return Lerp(to, t);
                    }
                    NkVec2T fromNormalized = Normalized();
                    NkVec2T toNormalized = NkVec2T(to).Normalized();
                    float32 cosAngle = NkClamp(
                        static_cast<float32>(fromNormalized.Dot(toNormalized)),
                        -1.0f,
                        1.0f
                    );
                    if (cosAngle > 1.0f - static_cast<float32>(NkEpsilon)) {
                        return NLerp(to, t);
                    }
                    float32 angle = NkAcos(cosAngle);
                    float32 sinAngle = NkSin(angle);
                    float32 coefficientA = NkSin((1.0f - t) * angle) / sinAngle;
                    float32 coefficientB = NkSin(t * angle) / sinAngle;
                    return fromNormalized * static_cast<T>(coefficientA)
                         + toNormalized * static_cast<T>(coefficientB);
                }

                // Test de colinéarité via produit croisé scalaire 2D
                NK_FORCE_INLINE bool Collinear(const NkVec2T& other) const noexcept
                {
                    return NkFabs(
                        static_cast<float32>(x * other.y - y * other.x)
                    ) < static_cast<float32>(NkEpsilon);
                }

                // -----------------------------------------------------------------
                // Section : Constantes statiques prédéfinies
                // -----------------------------------------------------------------
                static constexpr NkVec2T Zero() noexcept
                {
                    return {};
                }

                static constexpr NkVec2T One() noexcept
                {
                    return { T(1), T(1) };
                }

                static constexpr NkVec2T UnitX() noexcept
                {
                    return { T(1), T(0) };
                }

                static constexpr NkVec2T UnitY() noexcept
                {
                    return { T(0), T(1) };
                }

                // -----------------------------------------------------------------
                // Section : Sérialisation et E/S
                // -----------------------------------------------------------------
                // Conversion en chaîne de caractères lisible
                NkString ToString() const
                {
                    return NkFormat("({0}, {1})", x, y);
                }

                // Surcharge globale de ToString pour appel fonctionnel
                friend NkString ToString(const NkVec2T& vector)
                {
                    return vector.ToString();
                }

                // Opérateur de flux pour affichage std::ostream
                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkVec2T& vector
                ) {
                    return outputStream << vector.ToString().CStr();
                }

            }; // struct NkVec2T

            // =================================================================
            // Structure : NkVec3T<T>
            // =================================================================
            // Vecteur 3D générique : pilier de la géométrie 3D.
            // Inclut : produit vectoriel, SLerp 3D, Project/Reject/Reflect.
            // Algorithmes de référence : Shoemake 1985 pour SLerp stable.
            // =================================================================
            template <typename T>
            struct NkVec3T {

                // -----------------------------------------------------------------
                // Union de membres : accès sémantique multiple
                // -----------------------------------------------------------------
                union {
                    struct { T x, y, z; };              // Coordonnées cartésiennes
                    struct { T r, g, b; };              // Composantes couleur RGB
                    struct { T u, v, w; };              // Coordonnées paramétriques
                    struct { T pitch, yaw, roll; };     // Angles d'Euler (attention ordre)
                    T data[3];                           // Accès tableau brut
                };

                // -----------------------------------------------------------------
                // Section : Constructeurs
                // -----------------------------------------------------------------
                constexpr NkVec3T() noexcept
                    : x(T(0))
                    , y(T(0))
                    , z(T(0))
                {
                }

                constexpr explicit NkVec3T(T scalar) noexcept
                    : x(scalar)
                    , y(scalar)
                    , z(scalar)
                {
                }

                constexpr NkVec3T(T x, T y, T z) noexcept
                    : x(x)
                    , y(y)
                    , z(z)
                {
                }

                constexpr NkVec3T(const NkVec2T<T>& xy, T z) noexcept
                    : x(xy.x)
                    , y(xy.y)
                    , z(z)
                {
                }

                constexpr NkVec3T(T x, const NkVec2T<T>& yz) noexcept
                    : x(x)
                    , y(yz.x)
                    , z(yz.y)
                {
                }

                constexpr explicit NkVec3T(const T* pointer) noexcept
                    : x(pointer[0])
                    , y(pointer[1])
                    , z(pointer[2])
                {
                }

                NkVec3T(const NkVec3T&) noexcept = default;
                NkVec3T& operator=(const NkVec3T&) noexcept = default;

                template <typename U>
                constexpr explicit NkVec3T(const NkVec3T<U>& other) noexcept
                    : x(static_cast<T>(other.x))
                    , y(static_cast<T>(other.y))
                    , z(static_cast<T>(other.z))
                {
                }

                template <typename U>
                NkVec3T& operator=(const NkVec3T<U>& other)
                {
                    x = static_cast<T>(other.x);
                    y = static_cast<T>(other.y);
                    z = static_cast<T>(other.z);
                    return *this;
                }

                // -----------------------------------------------------------------
                // Section : Conversions et accesseurs
                // -----------------------------------------------------------------
                operator T*() noexcept { return data; }
                operator const T*() const noexcept { return data; }

                template <typename U>
                operator NkVec3T<U>() noexcept
                {
                    return NkVec3T<U>(x, y, z);
                }

                template <typename U>
                operator const NkVec3T<U>() const noexcept
                {
                    return NkVec3T<U>(x, y, z);
                }

                NK_FORCE_INLINE T& operator[](size_t index) noexcept
                {
                    return data[index < 3 ? index : 2];
                }

                NK_FORCE_INLINE const T& operator[](size_t index) const noexcept
                {
                    return data[index < 3 ? index : 2];
                }

                // -----------------------------------------------------------------
                // Section : Opérateurs arithmétiques
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec3T operator-() const noexcept
                {
                    return { -x, -y, -z };
                }

                NK_FORCE_INLINE NkVec3T operator+(const NkVec3T& other) const noexcept
                {
                    return { x + other.x, y + other.y, z + other.z };
                }

                NK_FORCE_INLINE NkVec3T operator-(const NkVec3T& other) const noexcept
                {
                    return { x - other.x, y - other.y, z - other.z };
                }

                NK_FORCE_INLINE NkVec3T operator*(const NkVec3T& other) const noexcept
                {
                    return { x * other.x, y * other.y, z * other.z };
                }

                NK_FORCE_INLINE NkVec3T operator/(const NkVec3T& other) const noexcept
                {
                    return { x / other.x, y / other.y, z / other.z };
                }

                NK_FORCE_INLINE NkVec3T operator+(T scalar) const noexcept
                {
                    return { x + scalar, y + scalar, z + scalar };
                }

                NK_FORCE_INLINE NkVec3T operator-(T scalar) const noexcept
                {
                    return { x - scalar, y - scalar, z - scalar };
                }

                NK_FORCE_INLINE NkVec3T operator*(T scalar) const noexcept
                {
                    return { x * scalar, y * scalar, z * scalar };
                }

                NK_FORCE_INLINE NkVec3T operator/(T scalar) const noexcept
                {
                    T inverse = T(1) / scalar;
                    return { x * inverse, y * inverse, z * inverse };
                }

                NK_FORCE_INLINE NkVec3T& operator+=(const NkVec3T& other) noexcept
                {
                    x += other.x;
                    y += other.y;
                    z += other.z;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator-=(const NkVec3T& other) noexcept
                {
                    x -= other.x;
                    y -= other.y;
                    z -= other.z;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator*=(const NkVec3T& other) noexcept
                {
                    x *= other.x;
                    y *= other.y;
                    z *= other.z;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator/=(const NkVec3T& other) noexcept
                {
                    x /= other.x;
                    y /= other.y;
                    z /= other.z;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator+=(T scalar) noexcept
                {
                    x += scalar;
                    y += scalar;
                    z += scalar;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator-=(T scalar) noexcept
                {
                    x -= scalar;
                    y -= scalar;
                    z -= scalar;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator*=(T scalar) noexcept
                {
                    x *= scalar;
                    y *= scalar;
                    z *= scalar;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator/=(T scalar) noexcept
                {
                    T inverse = T(1) / scalar;
                    x *= inverse;
                    y *= inverse;
                    z *= inverse;
                    return *this;
                }

                friend NK_FORCE_INLINE NkVec3T operator+(T scalar, const NkVec3T& vector) noexcept
                {
                    return vector + scalar;
                }

                friend NK_FORCE_INLINE NkVec3T operator-(T scalar, const NkVec3T& vector) noexcept
                {
                    return { scalar - vector.x, scalar - vector.y, scalar - vector.z };
                }

                friend NK_FORCE_INLINE NkVec3T operator*(T scalar, const NkVec3T& vector) noexcept
                {
                    return vector * scalar;
                }

                friend NK_FORCE_INLINE NkVec3T operator/(T scalar, const NkVec3T& vector) noexcept
                {
                    return { scalar / vector.x, scalar / vector.y, scalar / vector.z };
                }

                // -----------------------------------------------------------------
                // Section : Incrémentation / Décrémentation
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec3T& operator++() noexcept
                {
                    ++x;
                    ++y;
                    ++z;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T& operator--() noexcept
                {
                    --x;
                    --y;
                    --z;
                    return *this;
                }

                NK_FORCE_INLINE NkVec3T operator++(int) noexcept
                {
                    NkVec3T temporary(*this);
                    ++(*this);
                    return temporary;
                }

                NK_FORCE_INLINE NkVec3T operator--(int) noexcept
                {
                    NkVec3T temporary(*this);
                    --(*this);
                    return temporary;
                }

                // -----------------------------------------------------------------
                // Section : Comparaison
                // -----------------------------------------------------------------
                friend NK_FORCE_INLINE bool operator==(
                    const NkVec3T& left,
                    const NkVec3T& right
                ) noexcept {
                    if constexpr (traits::NkIsFloatingPoint_v<T>) {
                        return NkFabs(left.x - right.x) <= T(NkVectorEpsilon)
                            && NkFabs(left.y - right.y) <= T(NkVectorEpsilon)
                            && NkFabs(left.z - right.z) <= T(NkVectorEpsilon);
                    } else {
                        return (left.x == right.x)
                            && (left.y == right.y)
                            && (left.z == right.z);
                    }
                }

                friend NK_FORCE_INLINE bool operator!=(
                    const NkVec3T& left,
                    const NkVec3T& right
                ) noexcept {
                    return !(left == right);
                }

                // -----------------------------------------------------------------
                // Section : Produit scalaire, longueur et produit vectoriel
                // -----------------------------------------------------------------
                NK_FORCE_INLINE T Dot(const NkVec3T& other) const noexcept
                {
                    return x * other.x + y * other.y + z * other.z;
                }

                NK_FORCE_INLINE T LenSq() const noexcept
                {
                    return x * x + y * y + z * z;
                }

                NK_FORCE_INLINE T Len() const noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkVectorEpsilon)) {
                        return T(0);
                    }
                    return static_cast<T>(
                        NkSqrt(static_cast<float32>(lengthSquared))
                    );
                }

                NK_FORCE_INLINE T Distance(const NkVec3T& other) const noexcept
                {
                    return (*this - other).Len();
                }

                // Produit vectoriel : retourne un vecteur perpendiculaire à a et b
                NK_FORCE_INLINE NkVec3T Cross(const NkVec3T& other) const noexcept
                {
                    return {
                        y * other.z - z * other.y,
                        z * other.x - x * other.z,
                        x * other.y - y * other.x
                    };
                }

                // -----------------------------------------------------------------
                // Section : Normalisation
                // -----------------------------------------------------------------
                void Normalize() noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkVectorEpsilon)) {
                        return;
                    }
                    T inverseLength = T(1)
                        / static_cast<T>(NkSqrt(static_cast<float32>(lengthSquared)));
                    x *= inverseLength;
                    y *= inverseLength;
                    z *= inverseLength;
                }

                NK_FORCE_INLINE NkVec3T Normalized() const noexcept
                {
                    NkVec3T copy(*this);
                    copy.Normalize();
                    return copy;
                }

                NK_FORCE_INLINE bool IsUnit() const noexcept
                {
                    return NkFabs(
                        static_cast<float32>(LenSq()) - 1.0f
                    ) < 2.0f * static_cast<float32>(NkEpsilon);
                }

                // -----------------------------------------------------------------
                // Section : Géométrie 3D
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec3T Project(const NkVec3T& onto) const noexcept
                {
                    T denominator = onto.LenSq();
                    if (denominator < T(NkVectorEpsilon)) {
                        return {};
                    }
                    T factor = Dot(onto) / denominator;
                    return onto * factor;
                }

                NK_FORCE_INLINE NkVec3T Reject(const NkVec3T& onto) const noexcept
                {
                    return *this - Project(onto);
                }

                NK_FORCE_INLINE NkVec3T Reflect(const NkVec3T& normal) const noexcept
                {
                    return *this - normal * (T(2) * Dot(normal));
                }

                // -----------------------------------------------------------------
                // Section : Interpolation 3D
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec3T Lerp(const NkVec3T& to, float32 t) const noexcept
                {
                    return *this + (to - *this) * static_cast<T>(t);
                }

                NK_FORCE_INLINE NkVec3T NLerp(const NkVec3T& to, float32 t) const noexcept
                {
                    return Lerp(to, t).Normalized();
                }

                // SLerp 3D : interpolation sphérique stable (Shoemake 1985)
                NkVec3T SLerp(const NkVec3T& to, float32 t) const noexcept
                {
                    if (t < 0.01f) {
                        return Lerp(to, t);
                    }
                    NkVec3T fromNormalized = Normalized();
                    NkVec3T toNormalized = NkVec3T(to).Normalized();
                    float32 cosAngle = NkClamp(
                        static_cast<float32>(fromNormalized.Dot(toNormalized)),
                        -1.0f,
                        1.0f
                    );
                    if (cosAngle > 1.0f - static_cast<float32>(NkEpsilon)) {
                        return NLerp(to, t);
                    }
                    float32 angle = NkAcos(cosAngle);
                    float32 sinAngle = NkSin(angle);
                    float32 coefficientA = NkSin((1.0f - t) * angle) / sinAngle;
                    float32 coefficientB = NkSin(t * angle) / sinAngle;
                    return fromNormalized * static_cast<T>(coefficientA)
                         + toNormalized * static_cast<T>(coefficientB);
                }

                // -----------------------------------------------------------------
                // Section : Colinéarité
                // -----------------------------------------------------------------
                NK_FORCE_INLINE bool Collinear(const NkVec3T& other) const noexcept
                {
                    return Cross(other).LenSq() < T(NkVectorEpsilon);
                }

                // -----------------------------------------------------------------
                // Section : Constantes directionnelles
                // -----------------------------------------------------------------
                static constexpr NkVec3T Zero() noexcept { return {}; }
                static constexpr NkVec3T One() noexcept { return { T(1), T(1), T(1) }; }
                static constexpr NkVec3T Up() noexcept { return { T(0), T(1), T(0) }; }
                static constexpr NkVec3T Down() noexcept { return { T(0), T(-1), T(0) }; }
                static constexpr NkVec3T Left() noexcept { return { T(-1), T(0), T(0) }; }
                static constexpr NkVec3T Right() noexcept { return { T(1), T(0), T(0) }; }
                static constexpr NkVec3T Forward() noexcept { return { T(0), T(0), T(1) }; }
                static constexpr NkVec3T Backward() noexcept { return { T(0), T(0), T(-1) }; }

                // -----------------------------------------------------------------
                // Section : E/S
                // -----------------------------------------------------------------
                NkString ToString() const
                {
                    return NkFormat("({0}, {1}, {2})", x, y, z);
                }

                friend NkString ToString(const NkVec3T& vector)
                {
                    return vector.ToString();
                }

                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkVec3T& vector
                ) {
                    return outputStream << vector.ToString().CStr();
                }

                // -----------------------------------------------------------------
                // Section : Swizzle getters (lecture)
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec2T<T> xy() const { return NkVec2T<T>(x, y); }
                NK_FORCE_INLINE NkVec2T<T> xz() const { return NkVec2T<T>(x, z); }
                NK_FORCE_INLINE NkVec2T<T> yz() const { return NkVec2T<T>(y, z); }
                NK_FORCE_INLINE NkVec2T<T> yx() const { return NkVec2T<T>(y, x); }
                NK_FORCE_INLINE NkVec2T<T> zx() const { return NkVec2T<T>(z, x); }
                NK_FORCE_INLINE NkVec2T<T> zy() const { return NkVec2T<T>(z, y); }

                // -----------------------------------------------------------------
                // Section : Swizzle setters (écriture)
                // -----------------------------------------------------------------
                NK_FORCE_INLINE void xy(const NkVec2T<T>& vector) { x = vector.x; y = vector.y; }
                NK_FORCE_INLINE void xy(float32 newX, float32 newY) { x = static_cast<T>(newX); y = static_cast<T>(newY); }
                NK_FORCE_INLINE void xz(const NkVec2T<T>& vector) { x = vector.x; z = vector.y; }
                NK_FORCE_INLINE void xz(float32 newX, float32 newZ) { x = static_cast<T>(newX); z = static_cast<T>(newZ); }
                NK_FORCE_INLINE void yz(const NkVec2T<T>& vector) { y = vector.x; z = vector.y; }
                NK_FORCE_INLINE void yz(float32 newY, float32 newZ) { y = static_cast<T>(newY); z = static_cast<T>(newZ); }
                NK_FORCE_INLINE void yx(const NkVec2T<T>& vector) { y = vector.x; x = vector.y; }
                NK_FORCE_INLINE void yx(float32 newY, float32 newX) { y = static_cast<T>(newY); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void zx(const NkVec2T<T>& vector) { z = vector.x; x = vector.y; }
                NK_FORCE_INLINE void zx(float32 newZ, float32 newX) { z = static_cast<T>(newZ); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void zy(const NkVec2T<T>& vector) { z = vector.x; y = vector.y; }
                NK_FORCE_INLINE void zy(float32 newZ, float32 newY) { z = static_cast<T>(newZ); y = static_cast<T>(newY); }

            }; // struct NkVec3T

            // =================================================================
            // Structure : NkVec4T<T>
            // =================================================================
            // Vecteur 4D générique : coordonnées homogènes, couleurs RGBA,
            // stockage interne des quaternions.
            // =================================================================
            template <typename T>
            struct NkVec4T {

                union {
                    struct { T x, y, z, w; };
                    struct { T r, g, b, a; };
                    T data[4];
                };

                // -----------------------------------------------------------------
                // Constructeurs
                // -----------------------------------------------------------------
                constexpr NkVec4T() noexcept
                    : x(T(0))
                    , y(T(0))
                    , z(T(0))
                    , w(T(1))
                {
                }

                constexpr explicit NkVec4T(T scalar) noexcept
                    : x(scalar)
                    , y(scalar)
                    , z(scalar)
                    , w(T(1))
                {
                }

                constexpr NkVec4T(T x, T y, T z, T w) noexcept
                    : x(x)
                    , y(y)
                    , z(z)
                    , w(w)
                {
                }

                constexpr NkVec4T(const NkVec3T<T>& xyz, T w) noexcept
                    : x(xyz.x)
                    , y(xyz.y)
                    , z(xyz.z)
                    , w(w)
                {
                }

                constexpr NkVec4T(T x, const NkVec3T<T>& yzw) noexcept
                    : x(x)
                    , y(yzw.x)
                    , z(yzw.y)
                    , w(yzw.z)
                {
                }

                constexpr explicit NkVec4T(const T* pointer) noexcept
                    : x(pointer[0])
                    , y(pointer[1])
                    , z(pointer[2])
                    , w(pointer[3])
                {
                }

                NkVec4T(const NkVec4T&) noexcept = default;
                NkVec4T& operator=(const NkVec4T&) noexcept = default;

                template <typename U>
                constexpr explicit NkVec4T(const NkVec4T<U>& other) noexcept
                    : x(static_cast<T>(other.x))
                    , y(static_cast<T>(other.y))
                    , z(static_cast<T>(other.z))
                    , w(static_cast<T>(other.w))
                {
                }

                template <typename U>
                NkVec4T& operator=(const NkVec4T<U>& other)
                {
                    x = static_cast<T>(other.x);
                    y = static_cast<T>(other.y);
                    z = static_cast<T>(other.z);
                    w = static_cast<T>(other.w);
                    return *this;
                }

                // -----------------------------------------------------------------
                // Accesseurs
                // -----------------------------------------------------------------
                operator T*() noexcept { return data; }
                operator const T*() const noexcept { return data; }

                template <typename U>
                operator NkVec4T<U>() noexcept
                {
                    return NkVec4T<U>(x, y, z, w);
                }

                template <typename U>
                operator const NkVec4T<U>() const noexcept
                {
                    return NkVec4T<U>(x, y, z, w);
                }

                NK_FORCE_INLINE T& operator[](size_t index) noexcept
                {
                    return data[index < 4 ? index : 3];
                }

                NK_FORCE_INLINE const T& operator[](size_t index) const noexcept
                {
                    return data[index < 4 ? index : 3];
                }

                // -----------------------------------------------------------------
                // Arithmétique
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec4T operator-() const noexcept
                {
                    return { -x, -y, -z, -w };
                }

                NK_FORCE_INLINE NkVec4T operator+(const NkVec4T& other) const noexcept
                {
                    return { x + other.x, y + other.y, z + other.z, w + other.w };
                }

                NK_FORCE_INLINE NkVec4T operator-(const NkVec4T& other) const noexcept
                {
                    return { x - other.x, y - other.y, z - other.z, w - other.w };
                }

                NK_FORCE_INLINE NkVec4T operator*(const NkVec4T& other) const noexcept
                {
                    return { x * other.x, y * other.y, z * other.z, w * other.w };
                }

                NK_FORCE_INLINE NkVec4T operator/(const NkVec4T& other) const noexcept
                {
                    return { x / other.x, y / other.y, z / other.z, w / other.w };
                }

                NK_FORCE_INLINE NkVec4T operator*(T scalar) const noexcept
                {
                    return { x * scalar, y * scalar, z * scalar, w * scalar };
                }

                NK_FORCE_INLINE NkVec4T operator/(T scalar) const noexcept
                {
                    T inverse = T(1) / scalar;
                    return { x * inverse, y * inverse, z * inverse, w * inverse };
                }

                NK_FORCE_INLINE NkVec4T& operator+=(const NkVec4T& other) noexcept
                {
                    x += other.x;
                    y += other.y;
                    z += other.z;
                    w += other.w;
                    return *this;
                }

                NK_FORCE_INLINE NkVec4T& operator-=(const NkVec4T& other) noexcept
                {
                    x -= other.x;
                    y -= other.y;
                    z -= other.z;
                    w -= other.w;
                    return *this;
                }

                NK_FORCE_INLINE NkVec4T& operator*=(const NkVec4T& other) noexcept
                {
                    x *= other.x;
                    y *= other.y;
                    z *= other.z;
                    w *= other.w;
                    return *this;
                }

                NK_FORCE_INLINE NkVec4T& operator/=(const NkVec4T& other) noexcept
                {
                    x /= other.x;
                    y /= other.y;
                    z /= other.z;
                    w /= other.w;
                    return *this;
                }

                NK_FORCE_INLINE NkVec4T& operator*=(T scalar) noexcept
                {
                    x *= scalar;
                    y *= scalar;
                    z *= scalar;
                    w *= scalar;
                    return *this;
                }

                NK_FORCE_INLINE NkVec4T& operator/=(T scalar) noexcept
                {
                    T inverse = T(1) / scalar;
                    x *= inverse;
                    y *= inverse;
                    z *= inverse;
                    w *= inverse;
                    return *this;
                }

                friend NK_FORCE_INLINE NkVec4T operator*(T scalar, const NkVec4T& vector) noexcept
                {
                    return vector * scalar;
                }

                // -----------------------------------------------------------------
                // Comparaison
                // -----------------------------------------------------------------
                friend NK_FORCE_INLINE bool operator==(
                    const NkVec4T& left,
                    const NkVec4T& right
                ) noexcept {
                    if constexpr (traits::NkIsFloatingPoint_v<T>) {
                        return NkFabs(left.x - right.x) <= T(NkVectorEpsilon)
                            && NkFabs(left.y - right.y) <= T(NkVectorEpsilon)
                            && NkFabs(left.z - right.z) <= T(NkVectorEpsilon)
                            && NkFabs(left.w - right.w) <= T(NkVectorEpsilon);
                    } else {
                        return (left.x == right.x)
                            && (left.y == right.y)
                            && (left.z == right.z)
                            && (left.w == right.w);
                    }
                }

                friend NK_FORCE_INLINE bool operator!=(
                    const NkVec4T& left,
                    const NkVec4T& right
                ) noexcept {
                    return !(left == right);
                }

                // -----------------------------------------------------------------
                // Produit scalaire et longueur
                // -----------------------------------------------------------------
                NK_FORCE_INLINE T Dot(const NkVec4T& other) const noexcept
                {
                    return x * other.x + y * other.y + z * other.z + w * other.w;
                }

                NK_FORCE_INLINE T LenSq() const noexcept
                {
                    return x * x + y * y + z * z + w * w;
                }

                NK_FORCE_INLINE T Len() const noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkVectorEpsilon)) {
                        return T(0);
                    }
                    return static_cast<T>(
                        NkSqrt(static_cast<float32>(lengthSquared))
                    );
                }

                void Normalize() noexcept
                {
                    T lengthSquared = LenSq();
                    if (lengthSquared < T(NkVectorEpsilon)) {
                        return;
                    }
                    T inverseLength = T(1)
                        / static_cast<T>(NkSqrt(static_cast<float32>(lengthSquared)));
                    x *= inverseLength;
                    y *= inverseLength;
                    z *= inverseLength;
                    w *= inverseLength;
                }

                NK_FORCE_INLINE NkVec4T Normalized() const noexcept
                {
                    NkVec4T copy(*this);
                    copy.Normalize();
                    return copy;
                }

                // -----------------------------------------------------------------
                // Constantes
                // -----------------------------------------------------------------
                static constexpr NkVec4T Zero() noexcept
                {
                    return { T(0), T(0), T(0), T(0) };
                }

                // -----------------------------------------------------------------
                // E/S
                // -----------------------------------------------------------------
                NkString ToString() const
                {
                    return NkFormat("({0}, {1}, {2}, {3})", x, y, z, w);
                }

                friend NkString ToString(const NkVec4T& vector)
                {
                    return vector.ToString();
                }

                friend std::ostream& operator<<(
                    std::ostream& outputStream,
                    const NkVec4T& vector
                ) {
                    return outputStream << vector.ToString().CStr();
                }

                // -----------------------------------------------------------------
                // Swizzles 3D depuis 4D (getters)
                // -----------------------------------------------------------------
                NK_FORCE_INLINE NkVec3T<T> xyz() const { return NkVec3T<T>(x, y, z); }
                NK_FORCE_INLINE NkVec3T<T> xzy() const { return NkVec3T<T>(x, z, y); }
                NK_FORCE_INLINE NkVec3T<T> xyw() const { return NkVec3T<T>(x, y, w); }
                NK_FORCE_INLINE NkVec3T<T> xwy() const { return NkVec3T<T>(x, w, y); }
                NK_FORCE_INLINE NkVec3T<T> xzw() const { return NkVec3T<T>(x, z, w); }
                NK_FORCE_INLINE NkVec3T<T> xwz() const { return NkVec3T<T>(x, w, z); }

                // Swizzles 3D depuis 4D (setters avec NkVec3T)
                NK_FORCE_INLINE void xyz(const NkVec3T<T>& vector) { x = vector.x; y = vector.y; z = vector.z; }
                NK_FORCE_INLINE void xyz(float32 newX, float32 newY, float32 newZ) { x = static_cast<T>(newX); y = static_cast<T>(newY); z = static_cast<T>(newZ); }
                NK_FORCE_INLINE void xzy(const NkVec3T<T>& vector) { x = vector.x; y = vector.z; z = vector.y; }
                NK_FORCE_INLINE void xzy(float32 newX, float32 newZ, float32 newY) { x = static_cast<T>(newX); z = static_cast<T>(newZ); y = static_cast<T>(newY); }
                NK_FORCE_INLINE void xyw(const NkVec3T<T>& vector) { x = vector.x; y = vector.y; w = vector.z; }
                NK_FORCE_INLINE void xyw(float32 newX, float32 newY, float32 newW) { x = static_cast<T>(newX); y = static_cast<T>(newY); w = static_cast<T>(newW); }
                NK_FORCE_INLINE void xwy(const NkVec3T<T>& vector) { x = vector.x; w = vector.y; y = vector.z; }
                NK_FORCE_INLINE void xwy(float32 newX, float32 newW, float32 newY) { x = static_cast<T>(newX); w = static_cast<T>(newW); y = static_cast<T>(newY); }
                NK_FORCE_INLINE void xzw(const NkVec3T<T>& vector) { x = vector.x; z = vector.y; w = vector.z; }
                NK_FORCE_INLINE void xzw(float32 newX, float32 newZ, float32 newW) { x = static_cast<T>(newX); z = static_cast<T>(newZ); w = static_cast<T>(newW); }
                NK_FORCE_INLINE void xwz(const NkVec3T<T>& vector) { x = vector.x; w = vector.y; z = vector.z; }
                NK_FORCE_INLINE void xwz(float32 newX, float32 newW, float32 newZ) { x = static_cast<T>(newX); w = static_cast<T>(newW); z = static_cast<T>(newZ); }

                // Swizzles commençant par y
                NK_FORCE_INLINE NkVec3T<T> yxz() const { return NkVec3T<T>(y, x, z); }
                NK_FORCE_INLINE NkVec3T<T> yzx() const { return NkVec3T<T>(y, z, x); }
                NK_FORCE_INLINE NkVec3T<T> yxw() const { return NkVec3T<T>(y, x, w); }
                NK_FORCE_INLINE NkVec3T<T> ywx() const { return NkVec3T<T>(y, w, x); }
                NK_FORCE_INLINE NkVec3T<T> yzw() const { return NkVec3T<T>(y, z, w); }
                NK_FORCE_INLINE NkVec3T<T> ywz() const { return NkVec3T<T>(y, w, z); }

                NK_FORCE_INLINE void yxz(const NkVec3T<T>& vector) { y = vector.x; x = vector.y; z = vector.z; }
                NK_FORCE_INLINE void yxz(float32 newY, float32 newX, float32 newZ) { y = static_cast<T>(newY); x = static_cast<T>(newX); z = static_cast<T>(newZ); }
                NK_FORCE_INLINE void yzx(const NkVec3T<T>& vector) { y = vector.x; z = vector.y; x = vector.z; }
                NK_FORCE_INLINE void yzx(float32 newY, float32 newZ, float32 newX) { y = static_cast<T>(newY); z = static_cast<T>(newZ); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void yxw(const NkVec3T<T>& vector) { y = vector.x; x = vector.y; w = vector.z; }
                NK_FORCE_INLINE void yxw(float32 newY, float32 newX, float32 newW) { y = static_cast<T>(newY); x = static_cast<T>(newX); w = static_cast<T>(newW); }
                NK_FORCE_INLINE void ywx(const NkVec3T<T>& vector) { y = vector.x; w = vector.y; x = vector.z; }
                NK_FORCE_INLINE void ywx(float32 newY, float32 newW, float32 newX) { y = static_cast<T>(newY); w = static_cast<T>(newW); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void yzw(const NkVec3T<T>& vector) { y = vector.x; z = vector.y; w = vector.z; }
                NK_FORCE_INLINE void yzw(float32 newY, float32 newZ, float32 newW) { y = static_cast<T>(newY); z = static_cast<T>(newZ); w = static_cast<T>(newW); }
                NK_FORCE_INLINE void ywz(const NkVec3T<T>& vector) { y = vector.x; w = vector.y; z = vector.z; }
                NK_FORCE_INLINE void ywz(float32 newY, float32 newW, float32 newZ) { y = static_cast<T>(newY); w = static_cast<T>(newW); z = static_cast<T>(newZ); }

                // Swizzles commençant par z
                NK_FORCE_INLINE NkVec3T<T> zxy() const { return NkVec3T<T>(z, x, y); }
                NK_FORCE_INLINE NkVec3T<T> zyx() const { return NkVec3T<T>(z, y, x); }
                NK_FORCE_INLINE NkVec3T<T> zxw() const { return NkVec3T<T>(z, x, w); }
                NK_FORCE_INLINE NkVec3T<T> zwx() const { return NkVec3T<T>(z, w, x); }
                NK_FORCE_INLINE NkVec3T<T> zyw() const { return NkVec3T<T>(z, y, w); }
                NK_FORCE_INLINE NkVec3T<T> zwy() const { return NkVec3T<T>(z, w, y); }

                NK_FORCE_INLINE void zxy(const NkVec3T<T>& vector) { z = vector.x; x = vector.y; y = vector.z; }
                NK_FORCE_INLINE void zxy(float32 newZ, float32 newX, float32 newY) { z = static_cast<T>(newZ); x = static_cast<T>(newX); y = static_cast<T>(newY); }
                NK_FORCE_INLINE void zyx(const NkVec3T<T>& vector) { z = vector.x; y = vector.y; x = vector.z; }
                NK_FORCE_INLINE void zyx(float32 newZ, float32 newY, float32 newX) { z = static_cast<T>(newZ); y = static_cast<T>(newY); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void zxw(const NkVec3T<T>& vector) { z = vector.x; x = vector.y; w = vector.z; }
                NK_FORCE_INLINE void zxw(float32 newZ, float32 newX, float32 newW) { z = static_cast<T>(newZ); x = static_cast<T>(newX); w = static_cast<T>(newW); }
                NK_FORCE_INLINE void zwx(const NkVec3T<T>& vector) { z = vector.x; w = vector.y; x = vector.z; }
                NK_FORCE_INLINE void zwx(float32 newZ, float32 newW, float32 newX) { z = static_cast<T>(newZ); w = static_cast<T>(newW); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void zyw(const NkVec3T<T>& vector) { z = vector.x; y = vector.y; w = vector.z; }
                NK_FORCE_INLINE void zyw(float32 newZ, float32 newY, float32 newW) { z = static_cast<T>(newZ); y = static_cast<T>(newY); w = static_cast<T>(newW); }
                NK_FORCE_INLINE void zwy(const NkVec3T<T>& vector) { z = vector.x; w = vector.y; y = vector.z; }
                NK_FORCE_INLINE void zwy(float32 newZ, float32 newW, float32 newY) { z = static_cast<T>(newZ); w = static_cast<T>(newW); y = static_cast<T>(newY); }

                // Swizzles commençant par w
                NK_FORCE_INLINE NkVec3T<T> wxy() const { return NkVec3T<T>(w, x, y); }
                NK_FORCE_INLINE NkVec3T<T> wyx() const { return NkVec3T<T>(w, y, x); }
                NK_FORCE_INLINE NkVec3T<T> wxz() const { return NkVec3T<T>(w, x, z); }
                NK_FORCE_INLINE NkVec3T<T> wzx() const { return NkVec3T<T>(w, z, x); }
                NK_FORCE_INLINE NkVec3T<T> wyz() const { return NkVec3T<T>(w, y, z); }
                NK_FORCE_INLINE NkVec3T<T> wzy() const { return NkVec3T<T>(w, z, y); }

                NK_FORCE_INLINE void wxy(const NkVec3T<T>& vector) { w = vector.x; x = vector.y; y = vector.z; }
                NK_FORCE_INLINE void wxy(float32 newW, float32 newX, float32 newY) { w = static_cast<T>(newW); x = static_cast<T>(newX); y = static_cast<T>(newY); }
                NK_FORCE_INLINE void wyx(const NkVec3T<T>& vector) { w = vector.x; y = vector.y; x = vector.z; }
                NK_FORCE_INLINE void wyx(float32 newW, float32 newY, float32 newX) { w = static_cast<T>(newW); y = static_cast<T>(newY); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void wxz(const NkVec3T<T>& vector) { w = vector.x; x = vector.y; z = vector.z; }
                NK_FORCE_INLINE void wxz(float32 newW, float32 newX, float32 newZ) { w = static_cast<T>(newW); x = static_cast<T>(newX); z = static_cast<T>(newZ); }
                NK_FORCE_INLINE void wzx(const NkVec3T<T>& vector) { w = vector.x; z = vector.y; x = vector.z; }
                NK_FORCE_INLINE void wzx(float32 newW, float32 newZ, float32 newX) { w = static_cast<T>(newW); z = static_cast<T>(newZ); x = static_cast<T>(newX); }
                NK_FORCE_INLINE void wyz(const NkVec3T<T>& vector) { w = vector.x; y = vector.y; z = vector.z; }
                NK_FORCE_INLINE void wyz(float32 newW, float32 newY, float32 newZ) { w = static_cast<T>(newW); y = static_cast<T>(newY); z = static_cast<T>(newZ); }
                NK_FORCE_INLINE void wzy(const NkVec3T<T>& vector) { w = vector.x; z = vector.y; y = vector.z; }
                NK_FORCE_INLINE void wzy(float32 newW, float32 newZ, float32 newY) { w = static_cast<T>(newW); z = static_cast<T>(newZ); y = static_cast<T>(newY); }

            }; // struct NkVec4T

            // =================================================================
            // Aliases de types : variantes f/d/i/u
            // =================================================================
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

            // Alias courts pour usage courant
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

            // Alias legacy pour compatibilité
            using NkVector2 = NkVector2f;
            using NkVector3 = NkVector3f;
            using NkVector4 = NkVector4f;
            using NkVec2 = NkVector2f;
            using NkVec3 = NkVector3f;
            using NkVec4 = NkVector4f;

        } // namespace math

        // =====================================================================
        // Spécialisations de NkToString pour intégration au système de formatage
        // =====================================================================
        template <typename T>
        inline NkString NkToString(
            const math::NkVec2T<T>& vector,
            const NkFormatProps& properties = {}
        ) {
            return NkApplyFormatProps(vector.ToString(), properties);
        }

        template <typename T>
        inline NkString NkToString(
            const math::NkVec3T<T>& vector,
            const NkFormatProps& properties = {}
        ) {
            return NkApplyFormatProps(vector.ToString(), properties);
        }

        template <typename T>
        inline NkString NkToString(
            const math::NkVec4T<T>& vector,
            const NkFormatProps& properties = {}
        ) {
            return NkApplyFormatProps(vector.ToString(), properties);
        }

    } // namespace nkentseu

#endif // __NKENTSEU_VECTOR_H__

// =============================================================================
// EXEMPLES D'UTILISATION (documentation - ne pas compiler)
// =============================================================================
/*
    // ---------------------------------------------------------------------
    // Exemple 1 : Création de vecteurs avec différents types
    // ---------------------------------------------------------------------
    #include "NKMath/NkVec.h"
    using namespace nkentseu::math;

    // Vecteur 3D float (le plus courant)
    NkVec3f position(1.0f, 2.5f, -3.0f);

    // Vecteur 2D int pour des coordonnées de grille
    NkVec2i gridCell(10, 20);

    // Vecteur 4D pour couleur RGBA normalisée
    NkVec4f color(1.0f, 0.5f, 0.0f, 1.0f);  // Orange opaque

    // ---------------------------------------------------------------------
    // Exemple 2 : Opérations arithmétiques de base
    // ---------------------------------------------------------------------
    NkVec3f velocity(1.0f, 0.0f, 0.0f);
    NkVec3f acceleration(0.0f, -9.81f, 0.0f);

    // Addition : nouvelle vitesse après une frame
    NkVec3f newVelocity = velocity + acceleration * 0.016f;  // dt = 16ms

    // Multiplication composante par composante (masking)
    NkVec3f masked = position * NkVec3f(1.0f, 0.0f, 1.0f);  // Zero l'axe Y

    // ---------------------------------------------------------------------
    // Exemple 3 : Produit scalaire et vectoriel
    // ---------------------------------------------------------------------
    NkVec3f forward(0.0f, 0.0f, 1.0f);
    NkVec3f targetDir(1.0f, 0.0f, 1.0f);
    targetDir.Normalize();

    // Angle entre deux directions via dot product
    float32 cosAngle = forward.Dot(targetDir);
    float32 angleRad = NkAcos(cosAngle);

    // Produit vectoriel pour obtenir une normale de surface
    NkVec3f edgeA(1.0f, 0.0f, 0.0f);
    NkVec3f edgeB(0.0f, 1.0f, 0.0f);
    NkVec3f surfaceNormal = edgeA.Cross(edgeB);  // Résulte en (0, 0, 1)

    // ---------------------------------------------------------------------
    // Exemple 4 : Interpolation de mouvements
    // ---------------------------------------------------------------------
    NkVec3f startPos(0.0f, 0.0f, 0.0f);
    NkVec3f endPos(10.0f, 5.0f, 0.0f);

    // Lerp linéaire simple
    NkVec3f midPoint = startPos.Lerp(endPos, 0.5f);  // (5, 2.5, 0)

    // SLerp pour interpolation sur sphère (directions unitaires)
    NkVec3f dirA = NkVec3f::Forward();
    NkVec3f dirB = NkVec3f::Up();
    NkVec3f interpolatedDir = dirA.SLerp(dirB, 0.25f);  // 25% du chemin arc

    // ---------------------------------------------------------------------
    // Exemple 5 : Projections et réflexions (physique simple)
    // ---------------------------------------------------------------------
    NkVec3f velocity(3.0f, -4.0f, 0.0f);
    NkVec3f floorNormal(0.0f, 1.0f, 0.0f);  // Sol horizontal

    // Rebond : réflexion par rapport à la normale du sol
    NkVec3f bouncedVelocity = velocity.Reflect(floorNormal);
    // Résultat : (3, 4, 0) - la composante Y s'inverse

    // Projection d'une force sur une direction
    NkVec3f force(10.0f, 10.0f, 0.0f);
    NkVec3f movementDir(1.0f, 0.0f, 0.0f);
    NkVec3f effectiveForce = force.Project(movementDir);  // (10, 0, 0)

    // ---------------------------------------------------------------------
    // Exemple 6 : Swizzling pour manipulation flexible
    // ---------------------------------------------------------------------
    NkVec3f position(1.0f, 2.0f, 3.0f);

    // Extraction de composantes 2D
    NkVec2f horizontal = position.xy();  // (1, 2)
    NkVec2f vertical = position.yz();    // (2, 3)

    // Réassignation via swizzle setter
    position.xy(NkVec2f(10.0f, 20.0f));  // position devient (10, 20, 3)
    position.zx(5.0f, 15.0f);            // position devient (15, 20, 5)

    // ---------------------------------------------------------------------
    // Exemple 7 : Intégration dans une boucle de jeu
    // ---------------------------------------------------------------------
    void UpdatePlayerMovement(
        NkVec3f& position,
        NkVec3f& velocity,
        const NkVec3f& inputDirection,
        float32 deltaTime
    ) {
        constexpr float32 moveSpeed = 5.0f;
        constexpr float32 friction = 0.9f;

        // Application de l'entrée utilisateur
        velocity += inputDirection * moveSpeed * deltaTime;

        // Application de la friction
        velocity *= friction;

        // Mise à jour de la position
        position += velocity * deltaTime;

        // Clamp de la position dans les limites du monde
        position.x = NkClamp(position.x, -100.0f, 100.0f);
        position.z = NkClamp(position.z, -100.0f, 100.0f);
    }

    // ---------------------------------------------------------------------
    // Bonnes pratiques
    // ---------------------------------------------------------------------
    // ✓ Utiliser Normalized() pour les directions avant Dot/Cross
    // ✓ Préférer LenSq() à Len() pour les comparaisons de distance (plus rapide)
    // ✓ Utiliser les alias courts (NkVec3f) pour la lisibilité du code
    // ✓ Profiter des swizzles pour éviter les constructions temporaires
    // ✗ Éviter les divisions par zéro : vérifier LenSq() avant Normalize()
    // ✗ Ne pas comparer directement des vecteurs flottants avec == sans epsilon
*/