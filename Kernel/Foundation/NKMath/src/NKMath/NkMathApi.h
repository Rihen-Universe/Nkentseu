// =============================================================================
// NKMath/NkMathApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKMath.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKMath uniquement pour la configuration de build NKMath
//  - Indépendance totale des modes de build : NKMath et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MATH_NKMATHAPI_H
#define NKENTSEU_MATH_NKMATHAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKMath dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKMath
    // -------------------------------------------------------------------------
    /**
     * @defgroup MathBuildConfig Configuration du Build NKMath
     * @brief Macros pour contrôler le mode de compilation de NKMath
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_MATH_BUILD_SHARED_LIB : Compiler NKMath en bibliothèque partagée
     *  - NKENTSEU_MATH_STATIC_LIB : Utiliser NKMath en mode bibliothèque statique
     *  - NKENTSEU_MATH_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKMath et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform en DLL + NKMath en static.
     *
     * @example CMakeLists.txt
     * @code
     * # NKMath en DLL, NKPlatform en static
     * target_compile_definitions(nkmath PRIVATE NKENTSEU_MATH_BUILD_SHARED_LIB)
     * target_compile_definitions(nkmath PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform
     *
     * # NKMath en static, NKPlatform en DLL
     * target_compile_definitions(monapp PRIVATE NKENTSEU_MATH_STATIC_LIB)
     * # (NKPlatform en DLL par défaut, pas de define nécessaire)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_MATH_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKMath
     * @def NKENTSEU_MATH_API
     * @ingroup MathApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKMath.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_MATH_BUILD_SHARED_LIB : export (compilation de NKMath en DLL)
     *  - NKENTSEU_MATH_STATIC_LIB ou NKENTSEU_MATH_HEADER_ONLY : vide (pas d'export)
     *  - Sinon : import (utilisation de NKMath en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication.
     *
     * @example
     * @code
     * // Dans un header public de NKMath :
     * NKENTSEU_MATH_API void Math_Initialize();
     *
     * // Pour compiler NKMath en DLL : -DNKENTSEU_MATH_BUILD_SHARED_LIB
     * // Pour utiliser NKMath en DLL : (aucun define, import par défaut)
     * // Pour utiliser NKMath en static : -DNKENTSEU_MATH_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_MATH_BUILD_SHARED_LIB)
        // Compilation de NKMath en bibliothèque partagée : exporter
        #define NKENTSEU_MATH_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_MATH_STATIC_LIB) || defined(NKENTSEU_MATH_HEADER_ONLY)
        // Build statique ou header-only : pas de décoration
        #define NKENTSEU_MATH_API
    #else
        // Utilisation de NKMath en mode DLL : importer
        #define NKENTSEU_MATH_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKMath
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKMath sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKMath
     * @def NKENTSEU_MATH_CLASS_EXPORT
     * @ingroup MathApiMacros
     *
     * Alias vers NKENTSEU_MATH_API pour les déclarations de classe.
     *
     * @example
     * @code
     * class NKENTSEU_MATH_CLASS_EXPORT Matrix4x4 {
     * public:
     *     void SetIdentity();
     * };
     * @endcode
     */
    #define NKENTSEU_MATH_CLASS_EXPORT NKENTSEU_MATH_API

    /**
     * @brief Fonction inline exportée pour NKMath
     * @def NKENTSEU_MATH_API_INLINE
     * @ingroup MathApiMacros
     *
     * Combinaison de NKENTSEU_MATH_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_MATH_HEADER_ONLY)
        #define NKENTSEU_MATH_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_MATH_API_INLINE NKENTSEU_MATH_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKMath
     * @def NKENTSEU_MATH_API_FORCE_INLINE
     * @ingroup MathApiMacros
     *
     * Combinaison de NKENTSEU_MATH_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     * Particulièrement utile pour les opérations mathématiques fréquentes :
     * - Multiplications de matrices/vecteurs
     * - Normalisations, dot/cross products
     * - Conversions de coordonnées
     */
    #if defined(NKENTSEU_MATH_HEADER_ONLY)
        #define NKENTSEU_MATH_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_MATH_API_FORCE_INLINE NKENTSEU_MATH_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKMath
     * @def NKENTSEU_MATH_API_NO_INLINE
     * @ingroup MathApiMacros
     *
     * Combinaison de NKENTSEU_MATH_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     * Utile pour les fonctions complexes rarement appelées :
     * - Décomposition de matrices
     * - Résolution de systèmes linéaires
     * - Calculs de transformations inverses
     */
    #define NKENTSEU_MATH_API_NO_INLINE NKENTSEU_MATH_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup MathApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKMath
     *
     * Pour éviter la duplication, NKMath réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKMath | Macro à utiliser | Source |
     * |-------------------|-----------------|--------|
     * | Dépréciation | `NKENTSEU_DEPRECATED` | NKPlatform |
     * | Dépréciation avec message | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement cache | `NKENTSEU_ALIGN_CACHE` | NKPlatform |
     * | Alignement SIMD 16/32/64 | `NKENTSEU_ALIGN_16`, etc. | NKPlatform |
     * | Liaison C | `NKENTSEU_EXTERN_C_BEGIN/END` | NKPlatform |
     * | Pragmas | `NKENTSEU_PRAGMA(x)` | NKPlatform |
     * | Gestion warnings | `NKENTSEU_DISABLE_WARNING_*` | NKPlatform |
     * | Symbole local | `NKENTSEU_API_LOCAL` | NKPlatform |
     * | Vectorisation | `NKENTSEU_SIMD_*` | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKMath (pas de NKENTSEU_MATH_DEPRECATED) :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser Matrix4x4::CreatePerspective()")
     * NKENTSEU_MATH_API void Legacy_CreateProjection(float fov, float aspect);
     *
     * // Alignement SIMD dans NKMath (pas de NKENTSEU_MATH_ALIGN_16) :
     * struct NKENTSEU_ALIGN_16 NKENTSEU_MATH_CLASS_EXPORT Vector4 {
     *     float x, y, z, w;  // 16 bytes, aligné pour SSE/NEON
     * };
     *
     * // Fonction vectorisée critique :
     * NKENTSEU_MATH_API_FORCE_INLINE
     * NKENTSEU_SIMD_CALL
     * Vector4 Vector4_Multiply(const Vector4& a, const Vector4& b);
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKMath
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKMath.

    #if defined(NKENTSEU_MATH_BUILD_SHARED_LIB) && defined(NKENTSEU_MATH_STATIC_LIB)
        #warning "NKMath: NKENTSEU_MATH_BUILD_SHARED_LIB et NKENTSEU_MATH_STATIC_LIB définis - NKENTSEU_MATH_STATIC_LIB ignoré"
        #undef NKENTSEU_MATH_STATIC_LIB
    #endif

    #if defined(NKENTSEU_MATH_BUILD_SHARED_LIB) && defined(NKENTSEU_MATH_HEADER_ONLY)
        #warning "NKMath: NKENTSEU_MATH_BUILD_SHARED_LIB et NKENTSEU_MATH_HEADER_ONLY définis - NKENTSEU_MATH_HEADER_ONLY ignoré"
        #undef NKENTSEU_MATH_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_MATH_STATIC_LIB) && defined(NKENTSEU_MATH_HEADER_ONLY)
        #warning "NKMath: NKENTSEU_MATH_STATIC_LIB et NKENTSEU_MATH_HEADER_ONLY définis - NKENTSEU_MATH_HEADER_ONLY ignoré"
        #undef NKENTSEU_MATH_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_MATH_DEBUG
        #pragma message("NKMath Export Config:")
        #if defined(NKENTSEU_MATH_BUILD_SHARED_LIB)
            #pragma message("  NKMath mode: Shared (export)")
        #elif defined(NKENTSEU_MATH_STATIC_LIB)
            #pragma message("  NKMath mode: Static")
        #elif defined(NKENTSEU_MATH_HEADER_ONLY)
            #pragma message("  NKMath mode: Header-only")
        #else
            #pragma message("  NKMath mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_MATH_API = " NKENTSEU_STRINGIZE(NKENTSEU_MATH_API))
    #endif

#endif // NKENTSEU_MATH_NKMATHAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Déclaration d'une fonction publique NKMath
    #include "NKMath/NkMathApi.h"

    NKENTSEU_MATH_EXTERN_C_BEGIN
    NKENTSEU_MATH_API void Math_Initialize(void);
    NKENTSEU_MATH_API void Math_Shutdown(void);
    NKENTSEU_MATH_API float Math_DegreesToRadians(float degrees);
    NKENTSEU_MATH_EXTERN_C_END

    // Exemple 2 : Déclaration d'une classe mathématique avec dépréciation
    #include "NKMath/NkMathApi.h"

    class NKENTSEU_MATH_CLASS_EXPORT Matrix4x4 {
    public:
        NKENTSEU_MATH_API Matrix4x4();
        NKENTSEU_MATH_API ~Matrix4x4();

        // Dépréciation via macro NKPlatform (pas de duplication)
        NKENTSEU_DEPRECATED_MESSAGE("Utiliser CreatePerspectiveRH()")
        NKENTSEU_MATH_API void SetPerspective(float fov, float aspect, float zn, float zf);

        NKENTSEU_MATH_API void SetIdentity();
        NKENTSEU_MATH_API Matrix4x4 Transposed() const;

        // Fonction inline critique pour performance
        NKENTSEU_MATH_API_FORCE_INLINE const float* Data() const {
            return m_data;
        }

    private:
        float m_data[16];  // Row-major order
    };

    // Exemple 3 : Alignement SIMD via macro NKPlatform
    struct NKENTSEU_ALIGN_16 NKENTSEU_MATH_CLASS_EXPORT Vector3 {
        float x, y, z;
        // Padding implicite pour alignement 16 bytes (SSE/NEON)
    };

    // Exemple 4 : Modes de build indépendants (CMake)
    // NKPlatform en DLL, NKMath en static :
    //   target_compile_definitions(nkmath PRIVATE NKENTSEU_MATH_STATIC_LIB)
    //   target_compile_definitions(nkmath PRIVATE NKENTSEU_BUILD_SHARED_LIB)  # Pour NKPlatform
    //
    // NKPlatform en static, NKMath en DLL :
    //   target_compile_definitions(nkmath PRIVATE NKENTSEU_MATH_BUILD_SHARED_LIB)
    //   target_compile_definitions(nkmath PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform

    // Exemple 5 : Utilisation dans le code client (moteur de jeu)
    #include <NKMath/Matrix4x4.h>
    #include <NKMath/Vector3.h>

    void UpdateCameraTransform() {
        nkentseu::math::Matrix4x4 view, projection;
        view.SetLookAt(position, target, up);
        projection.SetPerspectiveRH(nkentseu::math::PI/4, 16.0f/9.0f, 0.1f, 1000.0f);
        
        nkentseu::math::Matrix4x4 vp = projection * view;
        // ... upload to GPU ...
    }
*/

// =============================================================================
// EXEMPLES DÉTAILLÉS D'UTILISATION DE NKMATHAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKMath, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : API C publique pour les bindings et l'interopérabilité
// -----------------------------------------------------------------------------
/*
    // Dans nkmath_public.h :
    #include "NKMath/NkMathApi.h"

    NKENTSEU_MATH_EXTERN_C_BEGIN

    // Fonctions mathématiques de base exportées en C
    NKENTSEU_MATH_API float nkmath_clamp(float value, float min, float max);
    NKENTSEU_MATH_API float nkmath_lerp(float a, float b, float t);
    NKENTSEU_MATH_API float nkmath_smoothstep(float edge0, float edge1, float x);

    // Fonctions vectorielles
    NKENTSEU_MATH_API void nkmath_vec3_normalize(float* out, const float* v);
    NKENTSEU_MATH_API float nkmath_vec3_dot(const float* a, const float* b);
    NKENTSEU_MATH_API void nkmath_vec3_cross(float* out, const float* a, const float* b);

    // Fonctions matricielles
    NKENTSEU_MATH_API void nkmath_mat4_identity(float* out);
    NKENTSEU_MATH_API void nkmath_mat4_multiply(float* out, const float* a, const float* b);
    NKENTSEU_MATH_API void nkmath_mat4_translate(float* out, float x, float y, float z);

    NKENTSEU_MATH_EXTERN_C_END

    // Dans nkmath_impl.cpp :
    #include "nkmath_public.h"
    #include "NKMath/Internal/MathImpl.h"

    float nkmath_clamp(float value, float min, float max) {
        return nkentseu::math::internal::Clamp(value, min, max);
    }

    void nkmath_vec3_normalize(float* out, const float* v) {
        nkentseu::math::Vector3 result = nkentseu::math::Normalize(
            nkentseu::math::Vector3{v[0], v[1], v[2]});
        out[0] = result.x; out[1] = result.y; out[2] = result.z;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Classes C++ avec optimisations inline critiques
// -----------------------------------------------------------------------------
/*
    // Dans Vector3.h :
    #include "NKMath/NkMathApi.h"

    struct NKENTSEU_ALIGN_16 NKENTSEU_MATH_CLASS_EXPORT Vector3 {
        float x, y, z;

        // Constructeurs
        NKENTSEU_MATH_API_INLINE Vector3() : x(0), y(0), z(0) {}
        NKENTSEU_MATH_API_INLINE Vector3(float vx, float vy, float vz) 
            : x(vx), y(vy), z(vz) {}

        // Opérateurs arithmétiques inline pour performance
        NKENTSEU_MATH_API_FORCE_INLINE Vector3 operator+(const Vector3& other) const {
            return {x + other.x, y + other.y, z + other.z};
        }

        NKENTSEU_MATH_API_FORCE_INLINE Vector3 operator-(const Vector3& other) const {
            return {x - other.x, y - other.y, z - other.z};
        }

        NKENTSEU_MATH_API_FORCE_INLINE Vector3 operator*(float scalar) const {
            return {x * scalar, y * scalar, z * scalar};
        }

        // Méthodes utilitaires
        NKENTSEU_MATH_API_INLINE float LengthSquared() const {
            return x*x + y*y + z*z;
        }

        NKENTSEU_MATH_API float Length() const;  // Implémentation non-inline (sqrt)

        NKENTSEU_MATH_API Vector3 Normalized() const;  // Non-inline pour code size

        // Produit scalaire et vectoriel
        NKENTSEU_MATH_API_FORCE_INLINE float Dot(const Vector3& other) const {
            return x*other.x + y*other.y + z*other.z;
        }

        NKENTSEU_MATH_API_INLINE Vector3 Cross(const Vector3& other) const {
            return {
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            };
        }

        // Constantes statiques
        NKENTSEU_MATH_API static const Vector3 Zero;
        NKENTSEU_MATH_API static const Vector3 One;
        NKENTSEU_MATH_API static const Vector3 UnitX;
        NKENTSEU_MATH_API static const Vector3 UnitY;
        NKENTSEU_MATH_API static const Vector3 UnitZ;
    };

    // Définition des constantes statiques dans Vector3.cpp
    const Vector3 Vector3::Zero{0, 0, 0};
    const Vector3 Vector3::One{1, 1, 1};
    const Vector3 Vector3::UnitX{1, 0, 0};
    const Vector3 Vector3::UnitY{0, 1, 0};
    const Vector3 Vector3::UnitZ{0, 0, 1};
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des modes de build via CMake pour NKMath
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKMath

    cmake_minimum_required(VERSION 3.15)
    project(NKMath VERSION 1.0.0 LANGUAGES C CXX)

    # Options de build
    option(NKMATH_BUILD_SHARED "Build NKMath as shared library" ON)
    option(NKMATH_HEADER_ONLY "Use NKMath in header-only mode" OFF)
    option(NKMATH_ENABLE_SIMD "Enable SIMD optimizations (SSE2/NEON)" ON)

    # Configuration des defines
    if(NKMATH_HEADER_ONLY)
        add_definitions(-DNKENTSEU_MATH_HEADER_ONLY)
        set(NKMATH_LIBRARY_TYPE INTERFACE)
    elseif(NKMATH_BUILD_SHARED)
        add_definitions(-DNKENTSEU_MATH_BUILD_SHARED_LIB)
        set(NKMATH_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_MATH_STATIC_LIB)
        set(NKMATH_LIBRARY_TYPE STATIC)
    endif()

    # Configuration SIMD
    if(NKMATH_ENABLE_SIMD)
        if(MSVC)
            target_compile_options(nkmath PRIVATE /arch:AVX2)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(nkmath PRIVATE -march=native)
        endif()
    endif()

    # Création de la bibliothèque
    add_library(nkmath ${NKMATH_LIBRARY_TYPE}
        src/Vector3.cpp
        src/Matrix4x4.cpp
        src/Quaternion.cpp
        src/Transform.cpp
        src/MathUtils.cpp
        # ... autres fichiers sources
    )

    # Dépendances
    target_link_libraries(nkmath PUBLIC NKCore::NKCore)
    target_link_libraries(nkmath PRIVATE NKPlatform::NKPlatform)

    # Installation des en-têtes publics
    install(DIRECTORY include/NKMath DESTINATION include)

    # Export pour les consommateurs
    target_include_directories(nkmath PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Configuration du package pour find_package()
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/NKMathConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/NKMathConfigVersion.cmake"
            DESTINATION lib/cmake/NKMath)
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation dans un moteur de rendu 3D
// -----------------------------------------------------------------------------
/*
    // Dans Renderer.cpp :
    #include <NKMath/Matrix4x4.h>
    #include <NKMath/Vector3.h>
    #include <NKMath/Quaternion.h>

    class Camera {
    public:
        void UpdateViewProjection(float aspectRatio) {
            // Matrice de vue : position + orientation
            nkentseu::math::Matrix4x4 view;
            view.SetLookAt(m_position, m_target, m_up);

            // Matrice de projection : perspective droite
            nkentseu::math::Matrix4x4 projection;
            projection.SetPerspectiveRH(
                m_fovRadians, 
                aspectRatio, 
                m_nearPlane, 
                m_farPlane
            );

            // Combinaison pour le pipeline graphique
            m_viewProjection = projection * view;
        }

        // Transformation d'un point monde vers écran
        nkentseu::math::Vector4 WorldToScreen(const nkentseu::math::Vector3& worldPos) const {
            auto homogeneous = nkentseu::math::Vector4{worldPos.x, worldPos.y, worldPos.z, 1.0f};
            auto clip = m_viewProjection * homogeneous;
            
            // Perspective divide
            if (clip.w != 0.0f) {
                clip.x /= clip.w;
                clip.y /= clip.w;
                clip.z /= clip.w;
            }
            
            // Conversion NDC -> viewport
            return {
                (clip.x * 0.5f + 0.5f) * m_viewportWidth,
                (1.0f - (clip.y * 0.5f + 0.5f)) * m_viewportHeight,
                clip.z,
                clip.w
            };
        }

    private:
        nkentseu::math::Vector3 m_position;
        nkentseu::math::Vector3 m_target;
        nkentseu::math::Vector3 m_up;
        float m_fovRadians;
        float m_nearPlane;
        float m_farPlane;
        
        nkentseu::math::Matrix4x4 m_viewProjection;
        float m_viewportWidth;
        float m_viewportHeight;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Mode header-only pour intégration rapide
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKMath en mode header-only :

    // 1. Définir la macro avant toute inclusion
    #define NKENTSEU_MATH_HEADER_ONLY
    #include <NKMath/NkMathApi.h>
    #include <NKMath/Vector3.h>
    #include <NKMath/Matrix4x4.h>

    // 2. Toutes les fonctions marquées NKENTSEU_MATH_API_INLINE sont inline
    // 3. Pas de linkage nécessaire, idéal pour les petits projets ou prototypes

    void QuickMathDemo() {
        using namespace nkentseu::math;
        
        Vector3 a{1, 2, 3};
        Vector3 b{4, 5, 6};
        
        // Opérations inline : pas d'appel de fonction, code généré in-place
        Vector3 sum = a + b;              // {5, 7, 9}
        float dot = a.Dot(b);             // 1*4 + 2*5 + 3*6 = 32
        Vector3 cross = a.Cross(b);       // {-3, 6, -3}
        
        // Matrices : certaines opérations restent non-inline pour code size
        Matrix4x4 rotation;
        rotation.SetRotationY(PI / 4);    // 45 degrés
        
        Vector3 rotated = rotation * a;   // Multiplication matrice-vecteur
    }

    // Note : Le mode header-only peut augmenter la taille du binaire
    // car le code inline est dupliqué dans chaque unité de traduction.
    // Pour les projets de production, privilégier static/shared library.
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Dépréciation et migration d'API mathématique
// -----------------------------------------------------------------------------
/*
    #include <NKMath/NkMathApi.h>

    // Ancienne API dépréciée (angles en degrés)
    NKENTSEU_DEPRECATED_MESSAGE("Utiliser SetRotationRadians()")
    NKENTSEU_MATH_API void Matrix4x4::SetRotationDegrees(float degrees);

    // Nouvelle API recommandée (angles en radians, standard graphique)
    NKENTSEU_MATH_API void Matrix4x4::SetRotationRadians(float radians);

    // Dans le code client :
    void MigrateTransformCode() {
        nkentseu::math::Matrix4x4 transform;
        
        // Ceci génère un warning de dépréciation :
        // transform.SetRotationDegrees(90.0f);

        // Utiliser la nouvelle API avec conversion si nécessaire :
        transform.SetRotationRadians(nkentseu::math::DegreesToRadians(90.0f));
        
        // Ou mieux : travailler directement en radians dans tout le codebase
        constexpr float QUARTER_TURN = nkentseu::math::PI / 2;
        transform.SetRotationRadians(QUARTER_TURN);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison NKMath + NKPlatform pour code multi-module
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>   // NKENTSEU_PLATFORM_*
    #include <NKPlatform/NkPlatformExport.h>   // NKENTSEU_PLATFORM_API
    #include <NKMath/NkMathApi.h>              // NKENTSEU_MATH_API

    // Fonction qui utilise les deux modules pour init math SIMD
    NKENTSEU_MATH_API void Math_InitializeSIMD() {
        // Logging via NKPlatform
        NK_FOUNDATION_LOG_INFO("Initializing NKMath SIMD extensions...");

        // Détection des capacités CPU via NKPlatform
        #if NKENTSEU_PLATFORM_X86_64
            if (NKENTSEU_CPU_HAS_AVX2) {
                NK_FOUNDATION_LOG_DEBUG("AVX2 detected: enabling vectorized math");
                // Activer les kernels AVX2...
            } else if (NKENTSEU_CPU_HAS_SSE4_1) {
                NK_FOUNDATION_LOG_DEBUG("SSE4.1 detected: enabling SSE math");
                // Activer les kernels SSE...
            }
        #elif NKENTSEU_PLATFORM_ARM64
            if (NKENTSEU_CPU_HAS_NEON) {
                NK_FOUNDATION_LOG_DEBUG("NEON detected: enabling ARM SIMD math");
                // Activer les kernels NEON...
            }
        #endif

        // Configuration via NKMath build mode
        #if defined(NKENTSEU_MATH_BUILD_SHARED_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKMath mode: Shared DLL");
        #elif defined(NKENTSEU_MATH_STATIC_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKMath mode: Static library");
        #elif defined(NKENTSEU_MATH_HEADER_ONLY)
            NK_FOUNDATION_LOG_DEBUG("NKMath mode: Header-only");
        #endif
    }

    // Fonction mathématique critique avec annotations de performance
    NKENTSEU_MATH_API_FORCE_INLINE
    NKENTSEU_SIMD_CALL
    void Vector3_BatchNormalize(Vector3* NKENTSEU_RESTRICT out, 
                                const Vector3* NKENTSEU_RESTRICT in,
                                usize count) {
        // Implémentation vectorisée avec boucle déroulée
        // Annotations NKPlatform pour optimisations compilateur :
        // - NKENTSEU_SIMD_CALL : hint pour vectorisation
        // - NKENTSEU_RESTRICT : aliasing mémoire optimisé
        // - NKENTSEU_MATH_API_FORCE_INLINE : inlining agressif
        for (usize i = 0; i < count; ++i) {
            out[i] = in[i].Normalized();
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Tests unitaires avec gestion des modes de build
// -----------------------------------------------------------------------------
/*
    // Dans test_math.cpp :
    #include <NKMath/NkMathApi.h>
    #include <NKTest/NkTest.h>  // Framework de test interne

    NK_TEST_SUITE(MathModule) {

        NK_TEST_CASE(Vector3_BasicOps) {
            using nkentseu::math::Vector3;
            
            Vector3 a{1, 2, 3};
            Vector3 b{4, 5, 6};
            
            NK_CHECK_EQ((a + b).x, 5);
            NK_CHECK_EQ((a - b).y, -3);
            NK_CHECK_EQ((a * 2).z, 6);
            
            NK_CHECK_NEAR(a.Dot(b), 32.0f, 1e-5f);
            
            auto cross = a.Cross(b);
            NK_CHECK_NEAR(cross.x, -3.0f, 1e-5f);
            NK_CHECK_NEAR(cross.y,  6.0f, 1e-5f);
            NK_CHECK_NEAR(cross.z, -3.0f, 1e-5f);
        }

        NK_TEST_CASE(Matrix4x4_Identity) {
            using nkentseu::math::Matrix4x4;
            
            Matrix4x4 identity;
            identity.SetIdentity();
            
            // Vérifier que la multiplication par l'identité est neutre
            nkentseu::math::Vector4 v{1, 2, 3, 1};
            auto result = identity * v;
            
            NK_CHECK_NEAR(result.x, v.x, 1e-5f);
            NK_CHECK_NEAR(result.y, v.y, 1e-5f);
            NK_CHECK_NEAR(result.z, v.z, 1e-5f);
            NK_CHECK_NEAR(result.w, v.w, 1e-5f);
        }

        // Test conditionnel selon le mode de build
        #if !defined(NKENTSEU_MATH_HEADER_ONLY)
        NK_TEST_CASE(SymbolVisibility_Check) {
            // Vérifier que les symboles sont correctement exportés/importés
            // Ce test n'a de sens qu'en mode static/shared, pas header-only
            auto* funcPtr = &nkentseu::math::internal::GetMathVersion;
            NK_CHECK_NE(funcPtr, nullptr);
            NK_CHECK_GT(funcPtr(), 0);  // Version > 0
        }
        #endif
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================