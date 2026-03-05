// -----------------------------------------------------------------------------
// FICHIER: Examples\ModularExports\NkPhysicsExport.h
// DESCRIPTION: Exemple d'export modulaire pour le module Physics
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_EXAMPLES_MODULAREXPORTS_NKPHYSICSEXPORT_H_INCLUDED
#define NKENTSEU_EXAMPLES_MODULAREXPORTS_NKPHYSICSEXPORT_H_INCLUDED

#include "NkExport.h"

// ============================================================
// CONFIGURATION DU MODULE PHYSICS
// ============================================================

#ifndef NKENTSEU_BUILDING_PHYSICS
#if defined(NKENTSEU_PHYSICS_EXPORTS) || defined(PHYSICS_EXPORTS) || defined(NKENTSEU_BUILDING_PHY)
#define NKENTSEU_BUILDING_PHYSICS 1
#else
#define NKENTSEU_BUILDING_PHYSICS 0
#endif
#endif

// ============================================================
// MACROS D'EXPORT POUR LE MODULE PHYSICS
// ============================================================

#if NKENTSEU_BUILDING_PHYSICS
#define NKENTSEU_PHYSICS_API NKENTSEU_SYMBOL_EXPORT
#elif NKENTSEU_SHARED_BUILD
#define NKENTSEU_PHYSICS_API NKENTSEU_SYMBOL_IMPORT
#else
#define NKENTSEU_PHYSICS_API
#endif

#define NKENTSEU_PHYSICS_C_API NKENTSEU_EXTERN_C NKENTSEU_PHYSICS_API NKENTSEU_CALL
#define NKENTSEU_PHYSICS_PUBLIC NKENTSEU_PHYSICS_API
#define NKENTSEU_PHYSICS_PRIVATE NKENTSEU_SYMBOL_HIDDEN

#define NK_PHYSICS_API NKENTSEU_PHYSICS_API
#define NK_PHY_API NKENTSEU_PHYSICS_API

// ============================================================
// EXEMPLE D'UTILISATION
// ============================================================

/*
 * #include "NkPhysicsExport.h"
 *
 * namespace nkentseu {
 * namespace physics {
 *
 * class NKENTSEU_PHYSICS_API PhysicsWorld {
 * public:
 *     NKENTSEU_PHYSICS_API void step(float dt);
 *     NKENTSEU_PHYSICS_API void addRigidBody(void* body);
 * };
 *
 * class NKENTSEU_PHYSICS_API RigidBody {
 * public:
 *     NKENTSEU_PHYSICS_API void applyForce(float x, float y, float z);
 * };
 *
 * } // namespace physics
 * } // namespace nkentseu
 *
 * // API C
 * NKENTSEU_EXTERN_C_BEGIN
 * NKENTSEU_PHYSICS_C_API void* nkPhysicsCreateWorld(void);
 * NKENTSEU_PHYSICS_C_API void nkPhysicsStep(void* world, float dt);
 * NKENTSEU_EXTERN_C_END
 */

#endif // NKENTSEU_EXAMPLES_MODULAREXPORTS_NKPHYSICSEXPORT_H_INCLUDED