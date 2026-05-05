// =============================================================================
// NKECS/NKECS.h
// =============================================================================
// Point d'entrée unique pour le moteur ECS (Entity-Component-System) de Nkentseu.
// Ce fichier inclut tous les composants, systèmes et utilitaires de base de l'ECS.
// Les projets utilisant l'ECS peuvent simplement inclure ce fichier pour accéder à l'ensemble des fonctionnalités de l'ECS.
// =============================================================================
// Note : pour les projets qui souhaitent une compilation plus rapide, il est possible d'inclure uniquement les sous-modules nécessaires
// =============================================================================
#pragma once

// =============================================================================
// 1. Définitions fondamentales et utilitaires
// =============================================================================
#include "NkECSDefines.h"               // Macros, constantes et types de base (kMaxComponentTypes, etc.)

// =============================================================================
// 2. Cœur du moteur ECS (gestion des types, stockage, archétypes)
// =============================================================================
#include "Core/NkTypeRegistry.h"        // Registre global des types de composants
#include "Storage/NkComponentPool.h"    // Stockage dense des données de composants
#include "Storage/NkArchetype.h"        // Définition des archétypes (combinaisons de composants)
#include "Storage/NkArchetypeGraph.h"   // Graphe de transition entre archétypes

// =============================================================================
// 3. Systèmes et exécution
// =============================================================================
#include "System/NkSystem.h"            // Interface de base pour les systèmes ECS
#include "System/NkScheduler.h"         // Ordonnanceur pour l'exécution des systèmes

// =============================================================================
// 4. Monde, Scène et Interrogation
// =============================================================================
#include "World/NkWorld.h"              // Conteneur principal des entités et systèmes
// NkScene/NkSceneGraph are part of Nkentseu layer, not NKECS core
#include "Query/NkQuery.h"              // Mécanisme d'interrogation des entités par composants

// =============================================================================
// 5. Communication et Événements
// =============================================================================
#include "Events/NkGameplayEventBus.h"  // Bus d'événements gameplay

// =============================================================================
// 6. Réflexion et Sérialisation
// =============================================================================
#include "Reflect/NkReflect.h"          // Introspection des composants pour éditeur/sérialisation

// =============================================================================
// Vérifications de compilation statiques
// =============================================================================
// S'assure que l'identifiant d'entité tient bien sur 64 bits (pratique pour le packing)
static_assert(sizeof(nkentseu::ecs::NkEntityId) == 8, "NkEntityId doit occuper 8 octets (64 bits) pour assurer l'unicité et les flags.");

// Vérifie que le masque de composants est assez grand pour représenter tous les types possibles
static_assert(sizeof(nkentseu::ecs::NkComponentMask) == nkentseu::ecs::kMaxComponentTypes / 8, "La taille du masque de composants ne correspond pas à kMaxComponentTypes.");

// =============================================================================
// Alias de commodité dans le namespace nkentseu (si les macros ne sont pas désactivées)
// =============================================================================
#ifndef NK_ECS_NO_ALIASES
namespace nkentseu
{
    // --- Types fondamentaux de l'ECS ---
    using NkEntityId      = ecs::NkEntityId;        // Identifiant unique d'une entité
    using NkComponentId   = ecs::NkComponentId;     // Identifiant unique d'un type de composant
    using NkComponentMask = ecs::NkComponentMask;   // Masque de bits représentant un ensemble de composants

    // --- Conteneurs principaux ---
    using NkWorld         = ecs::NkWorld;           // Le monde ECS global
    // NkScene/NkSceneGraph: Defined in Nkentseu (engine layer), not NKECS

    // --- Gestion des systèmes ---
    using NkSystem        = ecs::NkSystem;          // Système de base
    using NkSystemDesc    = ecs::NkSystemDesc;      // Descripteur pour la configuration d'un système
    using NkSystemGroup   = ecs::NkSystemGroup;     // Groupe de systèmes exécutés séquentiellement
    using NkScheduler     = ecs::NkScheduler;       // Ordonnanceur gérant les dépendances entre systèmes

    // --- Communication et utilitaires ---
    using NkGameplayEventBus = ecs::NkGameplayEventBus; // Bus d'événements gameplay
    using NkTypeRegistry  = ecs::NkTypeRegistry;    // Registre de métadonnées des composants

    // --- Événements UI prédéfinis ---
    using NkOnButtonClicked = ecs::NkOnButtonClicked; // Déclenché lorsqu'un bouton est cliqué
    using NkOnSliderChanged = ecs::NkOnSliderChanged; // Déclenché lorsqu'un slider change de valeur

    // --- Fonction utilitaire pour obtenir l'ID d'un type de composant ---
    template<typename T>
    NkComponentId NkIdOf() noexcept
    {
        // Délégation à la fonction interne de l'ECS
        return ecs::NkIdOf<T>();
    }

} // namespace nkentseu
#endif // NK_ECS_NO_ALIASES

// =============================================================================
// 15. Extensions de Scripting (optionnelles - décommenter si besoin)
// =============================================================================
// Ces inclusions requièrent les dépendances Python ou Mono/.NET.
// Décommentez la ligne correspondante après avoir intégré la bibliothèque.
//
// #include "Scripting/Python/NkScriptPython.h"   // Intégration Python
// #include "Scripting/CSharp/NkScriptCSharp.h"   // Intégration C# (Mono)

// =============================================================================
// EXEMPLES D'UTILISATION DE NKECS.H
// =============================================================================
//
// Ce fichier est le point d'entrée unique de la bibliothèque. Il suffit de
// l'inclure pour avoir accès à l'intégralité de l'ECS.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création d'un monde et d'une entité simple
// -----------------------------------------------------------------------------
/*
    #include "NKECS/NKECS.h"

    int main() {
        // Création d'un monde ECS.
        nkentseu::NkWorld world;

        // Création d'une entité avec un composant Transform.
        nkentseu::NkEntityId entity = world.CreateEntity();
        world.AddComponent<nkentseu::NkTransform>(entity, {.position = {0,0,0}});

        // Ajout d'un tag.
        world.AddComponent<nkentseu::NkTag>(entity, {"Player"});

        // Exécution des systèmes.
        nkentseu::NkScheduler scheduler;
        scheduler.AddSystem([](nkentseu::NkWorld& w) {
            // Itération sur toutes les entités ayant Transform et Tag.
            w.ForEach<nkentseu::NkTransform, nkentseu::NkTag>(
                [](nkentseu::NkEntityId id, nkentseu::NkTransform& t, nkentseu::NkTag& tag) {
                    // Logique de mise à jour...
                });
        });
        scheduler.Run(world);

        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des alias et de la fonction NkIdOf
// -----------------------------------------------------------------------------
/*
    #include "NKECS/NKECS.h"

    struct Health { float value; };
    struct Mana   { float value; };

    // Enregistrement manuel des composants (si on n'utilise pas NK_COMPONENT).
    namespace nkentseu::ecs {
        template<> NkComponentId NkIdOf<Health>() { return NkTypeRegistry::Global().IdOf<Health>(); }
        template<> NkComponentId NkIdOf<Mana>()   { return NkTypeRegistry::Global().IdOf<Mana>(); }
    }

    void ExampleAliases() {
        using namespace nkentseu; // grâce aux alias dans nkentseu::

        NkWorld world;
        NkEntityId e = world.CreateEntity();

        // Ajout de composants avec les types aliasés.
        world.AddComponent<Health>(e, {100.0f});
        world.AddComponent<Mana>(e,   {50.0f});

        // Récupération d'un ID de composant.
        NkComponentId healthId = NkIdOf<Health>();
        NkComponentId manaId   = NkIdOf<Mana>();
    }
*/

// =============================================================================