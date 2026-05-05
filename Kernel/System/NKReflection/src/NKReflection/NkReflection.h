// =============================================================================
// FICHIER  : Modules/System/NKReflection/include/NKReflection/NkReflection.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Point d'entree unique pour le module NKReflection.
//   Ce fichier inclut tous les en-tetes publics du module et fournit
//   des fonctions utilitaires de haut niveau pour l'initialisation,
//   l'enregistrement et l'interrogation des meta-donnees de reflexion.
//
// USAGE RECOMMANDE :
//   #include <NKReflection/NkReflection.h>
//
//   // Toutes les fonctionnalites du module sont alors disponibles :
//   // - NkType, NkProperty, NkMethod, NkClass, NkRegistry
//   // - Macros NKENTSEU_REFLECT_CLASS, NKENTSEU_PROPERTY, etc.
//   // - Fonctions d'initialisation et d'inspection runtime
//
// DEPENDANCES :
//   - NKCore (NkTypes, NkTraits, NkAssert)
//   - NKPlatform (macros d'export, compatibilite multiplateforme)
//   - NKContainers/Functional (NkFunction, NkBind pour les callbacks)
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKREFLECTION_H
#define NK_REFLECTION_NKREFLECTION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : CONFIGURATION DU MODULE
    // -------------------------------------------------------------------------
    // Inclusion de l'en-tete d'API pour les macros d'export/import.
    // Doit etre inclus en premier pour garantir la bonne visibilite des symboles.

    #include "NKReflection/NkReflectionApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : COMPOSANTS DE BASE DE LA REFLEXION
    // -------------------------------------------------------------------------
    // Inclusion des meta-donnees fondamentales : types, proprietes, methodes.
    // Ces composants forment le socle du systeme d'introspection runtime.

    #include "NKReflection/NkType.h"
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkMethod.h"

    // -------------------------------------------------------------------------
    // SECTION 3 : META-DONNEES DE CLASSES ET REGISTRE GLOBAL
    // -------------------------------------------------------------------------
    // Inclusion des meta-donnees de classes et du registre central.
    // Le registre permet l'enregistrement et la recherche dynamique des types.

    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkRegistry.h"

    // -------------------------------------------------------------------------
    // SECTION 4 : ESPACE DE NOMS DE CONVENANCE
    // -------------------------------------------------------------------------
    // Alias de namespace pour simplifier l'usage du module dans le code client.
    // Ces alias sont optionnels et peuvent etre ignores si non desires.

    namespace nkentseu {

        /**
         * @namespace refl
         * @brief Alias de convenance pour nkentseu::reflection
         *
         * Permet d'abbrevier les references au namespace de reflexion
         * dans le code client pour plus de concision.
         *
         * @example
         * @code
         * using namespace nkentseu::refl;
         * auto* cls = NkRegistry::Get().FindClass("Player");
         * @endcode
         */
        namespace refl = reflection;

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 5 : FONCTIONS UTILITAIRES DE HAUT NIVEAU
    // -------------------------------------------------------------------------
    // Fonctions de convenance pour les operations courantes de reflexion.
    // Ces fonctions encapsulent les patterns repetitifs d'usage du registre.

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // INITIALISATION ET FINALISATION DU MODULE
            // =================================================================

            /**
             * @brief Initialise le systeme de reflexion NKReflection
             * @return nk_bool vrai si l'initialisation a reussi, faux sinon
             *
             * @note Cette fonction est optionnelle : le registre s'initialise
             *       automatiquement lors du premier acces via Get().
             *       Utilisez Initialize() si vous avez besoin de :
             *       - Pre-enregistrer des types systemes
             *       - Configurer des callbacks globaux
             *       - Valider l'integrite du registre au startup
             *
             * @note Thread-safety : Doit etre appelee avant tout autre thread
             *       utilisant le registre, ou protegee par un mutex.
             *
             * @example
             * @code
             * if (!nkentseu::reflection::Initialize()) {
             *     NK_FOUNDATION_LOG_ERROR("Failed to initialize reflection system");
             *     return false;
             * }
             * @endcode
             */
            NKENTSEU_REFLECTION_API nk_bool Initialize();

            /**
             * @brief Finalise le systeme de reflexion NKReflection
             * @note Libere les ressources internes et reinitialise le registre
             * @note Optionnel : la destruction automatique a la fin du programme
             *       suffit dans la plupart des cas d'usage
             * @note Thread-safety : Doit etre appelee apres l'arret de tous
             *       les threads utilisant le registre
             */
            NKENTSEU_REFLECTION_API void Shutdown();

            // =================================================================
            // ACCES RAPIDE AUX META-DONNEES
            // =================================================================

            /**
             * @brief Recherche une classe par son nom dans le registre global
             * @param className Nom de la classe a rechercher
             * @return Pointeur constant vers le NkClass trouve ou nullptr
             * @note Equivalent a NkRegistry::Get().FindClass(className)
             * @note Thread-safe en lecture seule
             */
            NKENTSEU_REFLECTION_API const NkClass* FindClass(const nk_char* className);

            /**
             * @brief Recherche un type par son nom dans le registre global
             * @param typeName Nom du type a rechercher
             * @return Pointeur constant vers le NkType trouve ou nullptr
             * @note Equivalent a NkRegistry::Get().FindType(typeName)
             * @note Thread-safe en lecture seule
             */
            NKENTSEU_REFLECTION_API const NkType* FindType(const nk_char* typeName);

            /**
             * @brief Template pour recuperer les meta-donnees d'un type C++
             * @tparam T Type C++ dont on souhaite obtenir les meta-donnees
             * @return Pointeur constant vers le NkType correspondant
             * @note Equivalent a NkRegistry::Get().GetType<T>()
             * @note Cree le type automatiquement s'il n'existe pas encore
             */
            template<typename T>
            NKENTSEU_REFLECTION_API_INLINE const NkType* GetType() {
                return NkRegistry::Get().GetType<T>();
            }

            /**
             * @brief Template pour recuperer les meta-donnees d'une classe C++
             * @tparam T Type C++ de la classe a rechercher
             * @return Pointeur constant vers le NkClass correspondant ou nullptr
             * @note Equivalent a NkRegistry::Get().GetClass<T>()
             * @note Ne cree pas la classe automatiquement : retourne nullptr si absente
             */
            template<typename T>
            NKENTSEU_REFLECTION_API_INLINE const NkClass* GetClass() {
                return NkRegistry::Get().GetClass<T>();
            }

            // =================================================================
            // CREATION D'INSTANCES VIA REFLEXION
            // =================================================================

            /**
             * @brief Cree une instance d'une classe enregistree par son nom
             * @param className Nom de la classe a instancier
             * @return Pointeur void* vers l'instance creee ou nullptr en cas d'echec
             * @note La classe doit avoir un constructeur enregistre via SetConstructor()
             * @note La memoire de l'instance doit etre liberee via DestroyInstanceByName()
             *       ou par l'appelant selon la politique memoire utilisee
             *
             * @example
             * @code
             * void* obj = nkentseu::reflection::CreateInstanceByName("Player");
             * if (obj) {
             *     // Utilisation de l'instance...
             *     nkentseu::reflection::DestroyInstanceByName("Player", obj);
             * }
             * @endcode
             */
            NKENTSEU_REFLECTION_API void* CreateInstanceByName(const nk_char* className);

            /**
             * @brief Template pour creer une instance d'une classe C++ connue
             * @tparam T Type C++ de la classe a instancier
             * @return Pointeur vers une nouvelle instance de T ou nullptr
             * @note La classe doit avoir un constructeur enregistre
             * @note Plus type-safe que CreateInstanceByName() car le type de retour est connu
             *
             * @example
             * @code
             * Player* player = nkentseu::reflection::CreateInstance<Player>();
             * if (player) {
             *     player->Start();
             *     delete player; // Ou DestroyInstance si destructeur reflexif configure
             * }
             * @endcode
             */
            template<typename T>
            NKENTSEU_REFLECTION_API_INLINE T* CreateInstance() {
                const NkClass* classMeta = NkRegistry::Get().GetClass<T>();
                if (!classMeta || !classMeta->HasConstructor()) {
                    return nullptr;
                }
                return static_cast<T*>(classMeta->CreateInstance());
            }

            /**
             * @brief Detruit une instance creee par reflexion via son nom de classe
             * @param className Nom de la classe de l'instance a detruire
             * @param instance Pointeur vers l'instance a detruire
             * @note La classe doit avoir un destructeur enregistre via SetDestructor()
             * @note Ne dealloque pas la memoire de l'objet : c'est la responsabilite
             *       de l'appelant ou d'un allocateur externe si le destructeur
             *       reflexif ne le fait pas
             */
            NKENTSEU_REFLECTION_API void DestroyInstanceByName(
                const nk_char* className,
                void* instance
            );

            /**
             * @brief Template pour detruire une instance de type connu
             * @tparam T Type C++ de l'instance a detruire
             * @param instance Pointeur vers l'instance a detruire
             * @note Utilise le destructeur enregistre si disponible, sinon delete
             * @note Plus type-safe que DestroyInstanceByName() car le type est connu
             */
            template<typename T>
            NKENTSEU_REFLECTION_API_INLINE void DestroyInstance(T* instance) {
                if (!instance) {
                    return;
                }
                const NkClass* classMeta = NkRegistry::Get().GetClass<T>();
                if (classMeta && classMeta->HasDestructor()) {
                    classMeta->DestroyInstance(static_cast<void*>(instance));
                } else {
                    delete instance;
                }
            }

            // =================================================================
            // INSPECTION ET DEBUGGING
            // =================================================================

            /**
             * @brief Retourne le nombre total de classes enregistrees
             * @return Valeur nk_usize representant le nombre de classes
             * @note Thread-safe en lecture seule
             */
            NKENTSEU_REFLECTION_API nk_usize GetRegisteredClassCount();

            /**
             * @brief Retourne le nombre total de types enregistres
             * @return Valeur nk_usize representant le nombre de types
             * @note Thread-safe en lecture seule
             */
            NKENTSEU_REFLECTION_API nk_usize GetRegisteredTypeCount();

            /**
             * @brief Dump le contenu du registre pour le debugging
             * @note Affiche via NK_FOUNDATION_LOG_INFO toutes les classes/types enregistres
             * @note Utile pour le debugging et la validation de l'enregistrement
             * @note Thread-safe en lecture seule
             */
            NKENTSEU_REFLECTION_API void DumpRegistry();

            /**
             * @brief Valide l'integrite du registre de reflexion
             * @return nk_bool vrai si toutes les entites sont valides, faux sinon
             * @note Verifie : noms non-vides, types coherents, pas de cycles d'heritage
             * @note Thread-safe en lecture seule
             */
            NKENTSEU_REFLECTION_API nk_bool ValidateRegistry();

        } // namespace reflection

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 6 : MACROS DE CONVENANCE GLOBALES
    // -------------------------------------------------------------------------
    // Re-export des macros de reflexion pour un acces direct sans namespace.
    // Ces macros sont definies dans NkRegistry.h et reprises ici pour commodite.

    /**
     * @brief Macro pour declarer les meta-donnees de reflexion d'une classe
     * @def NKENTSEU_REFLECT_CLASS
     * @ingroup ReflectionMacros
     * @see NKReflection/NkRegistry.h pour la definition complete
     */
    // [Re-export via inclusion de NkRegistry.h]

    /**
     * @brief Macro pour declarer une propriete reflechie
     * @def NKENTSEU_REFLECT_PROPERTY
     * @ingroup ReflectionMacros
     * @see NKReflection/NkRegistry.h pour la definition complete
     */
    // [Re-export via inclusion de NkRegistry.h]

    /**
     * @brief Macro combinant declaration et reflexion d'une propriete
     * @def NKENTSEU_PROPERTY
     * @ingroup ReflectionMacros
     * @see NKReflection/NkRegistry.h pour la definition complete
     */
    // [Re-export via inclusion de NkRegistry.h]

    /**
     * @brief Macro pour l'enregistrement automatique d'une classe au startup
     * @def NKENTSEU_REGISTER_CLASS
     * @ingroup ReflectionMacros
     * @see NKReflection/NkRegistry.h pour la definition complete et les avertissements
     */
    // [Re-export via inclusion de NkRegistry.h]

#endif // NK_REFLECTION_NKREFLECTION_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkReflection.h (Point d'entree unique)
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Inclusion unique et usage basique
// -----------------------------------------------------------------------------
/*
    // Dans votre fichier principal ou unite de compilation :
    #include <NKReflection/NkReflection.h>

    // Toutes les fonctionnalites sont maintenant disponibles :
    // - Types, proprietes, methodes, classes, registre
    // - Macros de declaration et d'enregistrement

    namespace game {

        class Entity {
            NKENTSEU_REFLECT_CLASS(Entity)
        public:
            NKENTSEU_PROPERTY(nk_int32, id)
            NKENTSEU_PROPERTY(nk_bool, active)

            virtual void Update(nk_float32 deltaTime) {
                // Logique de mise a jour...
            }
        };

    } // namespace game

    // Enregistrement automatique (dans un fichier .cpp uniquement) :
    // NKENTSEU_REGISTER_CLASS(game::Entity)
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Initialisation et inspection du registre
// -----------------------------------------------------------------------------
/*
    #include <NKReflection/NkReflection.h>
    #include <NKCore/Log/NkLog.h>

    bool InitializeGameReflection() {
        // Initialisation optionnelle du module
        if (!nkentseu::reflection::Initialize()) {
            NK_FOUNDATION_LOG_ERROR("Failed to initialize reflection system");
            return false;
        }

        // Verification de l'enregistrement d'une classe attendue
        const auto* entityClass = nkentseu::reflection::FindClass("Entity");
        if (!entityClass) {
            NK_FOUNDATION_LOG_WARNING("Class 'Entity' not found in registry");
            // Peut etre normal si l'enregistrement est differe...
        } else {
            NK_FOUNDATION_LOG_INFO("Found class '%s' with %zu properties",
                entityClass->GetName(),
                entityClass->GetPropertyCount());
        }

        return true;
    }

    void ShutdownGameReflection() {
        // Finalisation optionnelle du module
        nkentseu::reflection::Shutdown();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Creation et destruction d'instances via reflexion
// -----------------------------------------------------------------------------
/*
    #include <NKReflection/NkReflection.h>

    void* CreateGameObject(const char* typeName) {
        // Creation via nom de classe (factory pattern reflexif)
        return nkentseu::reflection::CreateInstanceByName(typeName);
    }

    void DestroyGameObject(const char* typeName, void* instance) {
        // Destruction via nom de classe
        nkentseu::reflection::DestroyInstanceByName(typeName, instance);
    }

    // Usage type-safe quand le type est connu a la compilation :
    template<typename T>
    T* CreateTypedObject() {
        return nkentseu::reflection::CreateInstance<T>();
    }

    template<typename T>
    void DestroyTypedObject(T* obj) {
        nkentseu::reflection::DestroyInstance<T>(obj);
    }

    // Exemple d'utilisation :
    void SpawnPlayer() {
        // Methode dynamique (nom de classe connu a l'execution)
        void* player = CreateGameObject("Player");
        if (player) {
            // Utilisation via reflexion ou cast...
            DestroyGameObject("Player", player);
        }

        // Methode type-safe (type connu a la compilation)
        auto* typedPlayer = CreateTypedObject<game::Player>();
        if (typedPlayer) {
            typedPlayer->Start();
            DestroyTypedObject(typedPlayer);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Inspection et iteration sur les meta-donnees
// -----------------------------------------------------------------------------
/*
    #include <NKReflection/NkReflection.h>
    #include <NKCore/Log/NkLog.h>

    void InspectClass(const nk_char* className) {
        const auto* classMeta = nkentseu::reflection::FindClass(className);
        if (!classMeta) {
            NK_FOUNDATION_LOG_WARNING("Class '%s' not found", className);
            return;
        }

        NK_FOUNDATION_LOG_INFO("=== Class: %s ===", classMeta->GetName());
        NK_FOUNDATION_LOG_INFO("Size: %zu bytes", classMeta->GetSize());

        // Heritage
        if (const auto* base = classMeta->GetBaseClass()) {
            NK_FOUNDATION_LOG_INFO("Base class: %s", base->GetName());
        }

        // Proprietes
        NK_FOUNDATION_LOG_INFO("Properties (%zu):", classMeta->GetPropertyCount());
        for (nk_usize i = 0; i < classMeta->GetPropertyCount(); ++i) {
            const auto* prop = classMeta->GetPropertyAt(i);
            if (prop) {
                NK_FOUNDATION_LOG_INFO("  - %s : %s%s%s",
                    prop->GetName(),
                    prop->GetType().GetName(),
                    prop->IsReadOnly() ? " [readonly]" : "",
                    prop->IsStatic() ? " [static]" : "");
            }
        }

        // Methodes
        NK_FOUNDATION_LOG_INFO("Methods (%zu):", classMeta->GetMethodCount());
        for (nk_usize i = 0; i < classMeta->GetMethodCount(); ++i) {
            const auto* method = classMeta->GetMethodAt(i);
            if (method) {
                NK_FOUNDATION_LOG_INFO("  - %s() -> %s%s%s%s",
                    method->GetName(),
                    method->GetReturnType().GetName(),
                    method->IsStatic() ? " [static]" : "",
                    method->IsConst() ? " [const]" : "",
                    method->IsVirtual() ? " [virtual]" : "");
            }
        }
        NK_FOUNDATION_LOG_INFO("=== End Class ===");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Callback d'enregistrement pour logging automatique
// -----------------------------------------------------------------------------
/*
    #include <NKReflection/NkReflection.h>
    #include <NKCore/Log/NkLog.h>

    void SetupReflectionLogging() {
        auto& registry = nkentseu::reflection::NkRegistry::Get();

        // Callback pour logger chaque enregistrement
        registry.SetOnRegisterCallback(
            nkentseu::reflection::NkRegistry::OnRegisterCallback(
                [](const nk_char* name, nk_bool isClass) -> void {
                    NK_FOUNDATION_LOG_DEBUG("Registered %s: %s",
                        isClass ? "class" : "type",
                        name ? name : "<unknown>");
                }
            )
        );
    }

    // Note : Les classes enregistrees via NKENTSEU_REGISTER_CLASS
    // avant cet appel ne declencheront pas le callback.
    // Appelez SetupReflectionLogging() tot dans l'initialisation.
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Validation et dump du registre pour debugging
// -----------------------------------------------------------------------------
/*
    #include <NKReflection/NkReflection.h>
    #include <NKCore/Log/NkLog.h>

    void ValidateAndDumpReflection() {
        // Validation de l'integrite du registre
        if (!nkentseu::reflection::ValidateRegistry()) {
            NK_FOUNDATION_LOG_ERROR("Registry validation failed!");
            return;
        }

        NK_FOUNDATION_LOG_INFO("Registry validation passed.");

        // Dump complet pour inspection
        #ifdef NKENTSEU_DEBUG
        nkentseu::reflection::DumpRegistry();
        #endif

        // Statistiques
        NK_FOUNDATION_LOG_INFO("Registered: %zu types, %zu classes",
            nkentseu::reflection::GetRegisteredTypeCount(),
            nkentseu::reflection::GetRegisteredClassCount());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Usage avec l'alias de namespace de convenance
// -----------------------------------------------------------------------------
/*
    #include <NKReflection/NkReflection.h>

    // Utilisation de l'alias nkentseu::refl pour plus de concision
    namespace refl = nkentseu::refl;  // ou using namespace nkentseu::refl;

    void QuickAccessExample() {
        // Acces raccourci aux fonctionnalites du registre
        auto* playerClass = refl::NkRegistry::Get().FindClass("Player");
        if (playerClass) {
            auto* healthProp = playerClass->GetProperty("health");
            if (healthProp) {
                // Lecture de la propriete via reflexion...
            }
        }

        // Ou via les fonctions utilitaires de haut niveau :
        auto* typeMeta = refl::FindType("nk_int32");
        auto* classMeta = refl::FindClass("Entity");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Pattern complet de classe reflechie avec heritage
// -----------------------------------------------------------------------------
/*
    // === Player.h ===
    #pragma once
    #include <NKReflection/NkReflection.h>

    namespace game {

        class Character {
            NKENTSEU_REFLECT_CLASS(Character)
        public:
            NKENTSEU_PROPERTY(nk_int32, level)
            NKENTSEU_PROPERTY(nk_int32, experience)

            virtual void GainExperience(nk_int32 amount);
        };

    } // namespace game

    // === Player.cpp ===
    #include "Game/Player.h"
    #include <NKReflection/NkReflection.h>

    namespace game {

        // Definition des methodes
        void Character::GainExperience(nk_int32 amount) {
            experience += amount;
            // Logique de level up...
        }

        // Enregistrement automatique de la classe
        NKENTSEU_REGISTER_CLASS(Character)

    } // namespace game

    // === Usage dans le code jeu ===
    #include <NKReflection/NkReflection.h>

    void OnCharacterLevelUp(game::Character* character) {
        // Acces aux meta-donnees via reflexion
        const auto* classMeta = character->GetClass();
        const auto* expProp = classMeta->GetProperty("experience");

        if (expProp) {
            nk_int32 currentExp = expProp->GetValue<nk_int32>(character);
            printf("Character now has %d experience points\n", currentExp);
        }

        // Invocation d'une methode via reflexion (si configuree)
        const auto* levelUpMethod = classMeta->GetMethod("OnLevelUp");
        if (levelUpMethod && levelUpMethod->HasInvoke()) {
            // Preparation des arguments...
            // levelUpMethod->Invoke(character, args);
        }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================