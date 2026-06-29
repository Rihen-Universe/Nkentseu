#pragma once
// =============================================================================
// NkPhysicsTypes.h — Types de base de la dynamique (ZÉRO STL). [SCAFFOLD]
// =============================================================================
#include "NKMath/NkVec.h"

namespace nkentseu {
    namespace physics {

        using NkVec2f = nkentseu::math::NkVec2f;
        using NkVec3f = nkentseu::math::NkVec3f;
        using NkBodyId = uint32;
        static constexpr NkBodyId NK_INVALID_BODY = 0u;

        // Nature d'un corps (détermine sa masse inverse et sa réponse).
        enum class NkBodyType : uint8 {
            STATIC = 0,   // immobile, masse infinie (sol, murs)
            KINEMATIC,    // piloté à la main, ignore les forces, pousse les dynamiques
            DYNAMIC       // simulé entièrement (forces, contacts)
        };

        // Drapeaux d'un corps (bitmask).
        enum NkBodyFlags : uint32 {
            NK_BODY_NONE        = 0u,
            NK_BODY_FIXED_ROT   = 1u << 0, // bloque la rotation (personnage 2D, etc.)
            NK_BODY_NO_GRAVITY  = 1u << 1, // ignore la gravité du monde
            NK_BODY_TRIGGER     = 1u << 2, // détecte mais ne résout pas (zone)
            NK_BODY_CCD         = 1u << 3, // détection continue (corps rapide)
            NK_BODY_SLEEPING    = 1u << 4, // endormi (hors solveur jusqu'au réveil)
        };

        // Comment combiner deux matériaux en contact.
        enum class NkCombine : uint8 { AVERAGE = 0, MIN, MAX, MULTIPLY };

        // Configuration globale du monde physique.
        struct NkPhysicsConfig {
            NkVec3f gravity        = { 0.f, -9.81f, 0.f };
            int32   velocityIters  = 8;     // itérations du solveur de vitesse
            int32   positionIters  = 3;     // itérations de correction positionnelle
            float32 linearSleepTol = 0.01f; // seuil de mise en sommeil (vitesse lin.)
            float32 angularSleepTol= 0.02f; // seuil (vitesse ang.)
            float32 sleepTime      = 0.5f;  // durée sous seuil avant sommeil (s)
            float32 baumgarte      = 0.2f;  // facteur de correction positionnelle
            float32 slop           = 0.005f;// pénétration tolérée (anti-jitter)
            bool    enable2D       = false; // true => simulation contrainte au plan XY
            int32   subSteps       = 1;     // sous-pas internes par Step (chaînes de joints raides)
            float32 fixedTimeStep  = 1.f/60.f; // pas fixe pour Advance() (déterminisme)
            int32   maxSubSteps    = 8;     // garde-fou Advance (anti spirale de la mort)
        };

    } // namespace physics
} // namespace nkentseu
