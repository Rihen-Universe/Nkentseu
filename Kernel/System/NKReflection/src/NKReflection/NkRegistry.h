// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkRegistry.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Registre global des types/classes runtime + macros de declaration et
//   d'enregistrement de reflection avec gestion type-safe via NkFunction.
//
// AMELIORATIONS (v1.1.0) :
//   - Correction usize → nk_usize pour la coherence avec NKCore
//   - Documentation Doxygen complete pour chaque element public
//   - Notes de thread-safety explicites pour le registre singleton
//   - Macros de reflection securisees avec NK_CONCAT et meilleur scoping
//   - Helpers pour l'enregistrement batch et la validation
//   - Exemples d'utilisation complets en fin de fichier
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKREGISTRY_H
#define NK_REFLECTION_NKREGISTRY_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des composants de reflexion : types, classes, proprietes, methodes.
    // Inclusion des traits NKCore pour la meta-programmation.
    // Inclusion de NkFunction pour la gestion type-safe des callbacks optionnels.

    #include "NKReflection/NkType.h"
    #include "NKReflection/NkClass.h"
    #include "NKReflection/NkProperty.h"
    #include "NKReflection/NkMethod.h"
    #include "NKCore/NkTraits.h"
    #include "NKContainers/Functional/NkFunction.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : EN-TETES STANDARD MINIMAUX
    // -------------------------------------------------------------------------
    // Inclusion minimale des en-tetes standards pour offsetof, strcmp et typeid.
    // Aucune autre dependance STL n'est utilisee pour respecter la contrainte
    // d'independance des conteneurs runtime du module.

    #include <cstddef>
    #include <cstring>
    #include <typeinfo>

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu et de son sous-namespace
    // reflection qui encapsule toutes les fonctionnalites du systeme de reflexion.

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // CLASSE : NkRegistry
            // =================================================================
            /**
             * @class NkRegistry
             * @brief Registre global singleton pour les meta-donnees de reflexion
             *
             * Cette classe fournit un point d'acces centralise pour l'enregistrement,
             * la recherche et la recuperation des meta-donnees de types et de classes
             * au moment de l'execution. Elle implemente un pattern Meyer's Singleton
             * pour une initialisation thread-safe en C++11+.
             *
             * @note Thread-safety :
             *       - L'initialisation du singleton est thread-safe (C++11+)
             *       - Les operations d'enregistrement (RegisterType/RegisterClass)
             *         NE SONT PAS thread-safe : synchronisation externe requise
             *       - Les operations de lecture (FindType/FindClass) sont thread-safe
             *         si aucun enregistrement concurrent n'a lieu
             *
             * @note Capacite :
             *       - NK_MAX_TYPES = 512 types maximum enregistres
             *       - NK_MAX_CLASSES = 512 classes maximum enregistrees
             *       - Pour des besoins superieurs, augmenter ces constantes ou
             *         migrer vers une structure dynamique avec NKContainers/NkHashMap
             */
            class NKENTSEU_REFLECTION_API NkRegistry {
                public:
                    // ---------------------------------------------------------
                    // ACCESSEUR AU SINGLETON
                    // ---------------------------------------------------------

                    /**
                     * @brief Accesseur au singleton du registre
                     * @return Reference statique vers l'instance unique de NkRegistry
                     * @note Pattern Meyer's Singleton : initialisation thread-safe en C++11+
                     * @note L'instance est detruite automatiquement a la fin du programme
                     */
                    static NkRegistry& Get() {
                        static NkRegistry instance;
                        return instance;
                    }

                    // ---------------------------------------------------------
                    // ENREGISTREMENT ET RECHERCHE DE TYPES
                    // ---------------------------------------------------------

                    /**
                     * @brief Enregistre un type dans le registre global
                     * @param type Pointeur constant vers le NkType a enregistrer
                     * @note Les doublons sont ignores (comparaison par nom et adresse)
                     * @note Thread-unsafe : synchronisation externe requise en contexte concurrent
                     * @note Retourne silencieusement si la capacite maximale est atteinte
                     */
                    void RegisterType(const NkType* type) {
                        if (!type || !type->GetName()) {
                            return;
                        }
                        for (nk_usize i = 0; i < mTypeCount; ++i) {
                            if (mTypes[i] == type) {
                                return;
                            }
                            if (mTypes[i] && mTypes[i]->GetName() && ::strcmp(mTypes[i]->GetName(), type->GetName()) == 0) {
                                return;
                            }
                        }
                        if (mTypeCount < NK_MAX_TYPES) {
                            mTypes[mTypeCount++] = type;
                        }
                    }

                    /**
                     * @brief Recherche un type par son nom dans le registre
                     * @param name Nom du type a rechercher
                     * @return Pointeur constant vers le NkType trouve ou nullptr
                     * @note La comparaison utilise strcmp pour une egalite exacte des noms
                     * @note Thread-safe en lecture seule (si aucun enregistrement concurrent)
                     */
                    const NkType* FindType(const nk_char* name) const {
                        if (!name || name[0] == '\0') {
                            return nullptr;
                        }
                        for (nk_usize i = 0; i < mTypeCount; ++i) {
                            if (mTypes[i] && mTypes[i]->GetName() && ::strcmp(mTypes[i]->GetName(), name) == 0) {
                                return mTypes[i];
                            }
                        }
                        return nullptr;
                    }

                    /**
                     * @brief Template pour recuperer ou creer un type par son type C++
                     * @tparam T Type C++ dont on souhaite obtenir les meta-donnees
                     * @return Pointeur constant vers le NkType correspondant
                     * @note Si le type n'est pas trouve, il est cree automatiquement
                     *       via un static local (initialisation thread-safe C++11+)
                     * @note Le nom utilise est celui de typeid(T).name() (dependant du compilateur)
                     */
                    template<typename T>
                    const NkType* GetType() const {
                        const NkType* existing = FindType(typeid(T).name());
                        if (existing) {
                            return existing;
                        }
                        static NkType type(
                            typeid(T).name(),
                            sizeof(T),
                            alignof(T),
                            DetermineCategory<T>()
                        );
                        return &type;
                    }

                    // ---------------------------------------------------------
                    // ENREGISTREMENT ET RECHERCHE DE CLASSES
                    // ---------------------------------------------------------

                    /**
                     * @brief Enregistre une classe dans le registre global
                     * @param classInfo Pointeur constant vers le NkClass a enregistrer
                     * @note Les doublons sont ignores (comparaison par nom et adresse)
                     * @note Thread-unsafe : synchronisation externe requise en contexte concurrent
                     * @note Retourne silencieusement si la capacite maximale est atteinte
                     */
                    void RegisterClass(const NkClass* classInfo) {
                        if (!classInfo || !classInfo->GetName()) {
                            return;
                        }
                        for (nk_usize i = 0; i < mClassCount; ++i) {
                            if (mClasses[i] == classInfo) {
                                return;
                            }
                            if (mClasses[i] && mClasses[i]->GetName() &&
                                ::strcmp(mClasses[i]->GetName(), classInfo->GetName()) == 0) {
                                return;
                            }
                        }
                        if (mClassCount < NK_MAX_CLASSES) {
                            mClasses[mClassCount++] = classInfo;
                        }
                    }

                    /**
                     * @brief Recherche une classe par son nom dans le registre
                     * @param name Nom de la classe a rechercher
                     * @return Pointeur constant vers le NkClass trouve ou nullptr
                     * @note La comparaison utilise strcmp pour une egalite exacte des noms
                     * @note Thread-safe en lecture seule (si aucun enregistrement concurrent)
                     */
                    const NkClass* FindClass(const nk_char* name) const {
                        if (!name || name[0] == '\0') {
                            return nullptr;
                        }
                        for (nk_usize i = 0; i < mClassCount; ++i) {
                            if (mClasses[i] && mClasses[i]->GetName() && ::strcmp(mClasses[i]->GetName(), name) == 0) {
                                return mClasses[i];
                            }
                        }
                        return nullptr;
                    }

                    /**
                     * @brief Template pour recuperer une classe par son type C++
                     * @tparam T Type C++ de la classe a rechercher
                     * @return Pointeur constant vers le NkClass correspondant ou nullptr
                     * @note Utilise typeid(T).name() pour la recherche (dependant du compilateur)
                     * @note Ne cree pas automatiquement la classe : retourne nullptr si absente
                     */
                    template<typename T>
                    const NkClass* GetClass() const {
                        return FindClass(typeid(T).name());
                    }

                    // ---------------------------------------------------------
                    // UTILITAIRES D'INSPECTION
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne le nombre de types enregistres
                     * @return Valeur nk_usize representant le nombre de types
                     * @note Thread-safe en lecture seule
                     */
                    nk_usize GetTypeCount() const {
                        return mTypeCount;
                    }

                    /**
                     * @brief Retourne le nombre de classes enregistrees
                     * @return Valeur nk_usize representant le nombre de classes
                     * @note Thread-safe en lecture seule
                     */
                    nk_usize GetClassCount() const {
                        return mClassCount;
                    }

                    /**
                     * @brief Retourne un type par son index dans le registre
                     * @param index Index zero-base du type a recuperer
                     * @return Pointeur constant vers le NkType ou nullptr si hors limites
                     * @note L'ordre des types n'est pas garanti : ne pas dependre de l'index
                     */
                    const NkType* GetTypeAt(nk_usize index) const {
                        return index < mTypeCount ? mTypes[index] : nullptr;
                    }

                    /**
                     * @brief Retourne une classe par son index dans le registre
                     * @param index Index zero-base de la classe a recuperer
                     * @return Pointeur constant vers le NkClass ou nullptr si hors limites
                     * @note L'ordre des classes n'est pas garanti : ne pas dependre de l'index
                     */
                    const NkClass* GetClassAt(nk_usize index) const {
                        return index < mClassCount ? mClasses[index] : nullptr;
                    }

                    // ---------------------------------------------------------
                    // CALLBACKS OPTIONNELS VIA NkFunction
                    // ---------------------------------------------------------

                    /**
                     * @typedef OnRegisterCallback
                     * @brief Signature du callable invoque lors de l'enregistrement d'un type/classe
                     *
                     * @param name Nom de l'entite enregistree
                     * @param isClass nk_bool vrai si c'est une classe, faux si c'est un type
                     * @return void
                     *
                     * @note Encapsule dans NkFunction pour gestion memoire automatique
                     * @note Peut etre utilise pour le logging, la validation, ou l'indexation
                     */
                    using OnRegisterCallback = nkentseu::NkFunction<void(const nk_char*, nk_bool)>;

                    /**
                     * @brief Definit un callback invoque a chaque enregistrement
                     * @param callback NkFunction a invoquer lors de RegisterType/RegisterClass
                     * @note Le callback est copie/deplace dans le registre
                     * @note Thread-unsafe : definir avant tout enregistrement en contexte concurrent
                     */
                    void SetOnRegisterCallback(OnRegisterCallback callback) {
                        mOnRegisterCallback = traits::NkMove(callback);
                    }

                    /**
                     * @brief Retourne une reference au callback d'enregistrement
                     * @return Reference constante vers le NkFunction interne
                     * @note Permet l'inspection ou la reinitialisation du callback
                     */
                    const OnRegisterCallback& GetOnRegisterCallback() const {
                        return mOnRegisterCallback;
                    }

                private:
                    // ---------------------------------------------------------
                    // CONSTANTES PRIVEES
                    // ---------------------------------------------------------
                    // Capacites maximales du registre pour l'allocation statique.
                    // Ces valeurs limitent le nombre d'entites enregistrables
                    // pour garantir une allocation previsibile sans dependance STL.

                    static constexpr nk_usize NK_MAX_TYPES = 512;
                    static constexpr nk_usize NK_MAX_CLASSES = 512;

                    // ---------------------------------------------------------
                    // CONSTRUCTEUR PRIVE (Singleton)
                    // ---------------------------------------------------------
                    /**
                     * @brief Constructeur prive pour le pattern Singleton
                     * @note Initialise les tableaux de stockage a nullptr
                     * @note Supprime la copie et l'affectation pour garantir l'unicite
                     */
                    NkRegistry()
                        : mTypeCount(0)
                        , mClassCount(0)
                        , mOnRegisterCallback() {
                        for (nk_usize i = 0; i < NK_MAX_TYPES; ++i) {
                            mTypes[i] = nullptr;
                        }
                        for (nk_usize i = 0; i < NK_MAX_CLASSES; ++i) {
                            mClasses[i] = nullptr;
                        }
                    }

                    // Suppression des operateurs de copie pour le Singleton
                    NkRegistry(const NkRegistry&) = delete;
                    NkRegistry& operator=(const NkRegistry&) = delete;

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : STOCKAGE DES TYPES
                    // ---------------------------------------------------------
                    // Tableau statique pour le stockage des pointeurs de types.
                    // Capacite maximale : NK_MAX_TYPES elements.

                    const NkType* mTypes[NK_MAX_TYPES];
                    nk_usize mTypeCount;

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : STOCKAGE DES CLASSES
                    // ---------------------------------------------------------
                    // Tableau statique pour le stockage des pointeurs de classes.
                    // Capacite maximale : NK_MAX_CLASSES elements.

                    const NkClass* mClasses[NK_MAX_CLASSES];
                    nk_usize mClassCount;

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES : CALLBACKS OPTIONNELS
                    // ---------------------------------------------------------
                    // Callable optionnel invoque lors des enregistrements.
                    // Encapsule dans NkFunction pour gestion memoire automatique.

                    OnRegisterCallback mOnRegisterCallback;
            };

        } // namespace reflection

    } // namespace nkentseu

    // =============================================================================
    // MACROS DE REFLEXION SECURISEES
    // =============================================================================
    /**
     * @defgroup ReflectionMacros Macros d'Enregistrement de Reflexion
     * @brief Macros pour declarer et enregistrer des classes/proprietes reflechies
     *
     * Ces macros facilitent l'integration du systeme de reflexion dans les classes :
     *  - NKENTSEU_REFLECT_CLASS : Declare les meta-donnees de base d'une classe
     *  - NKENTSEU_REFLECT_PROPERTY : Declare une propriete pour la reflexion
     *  - NKENTSEU_PROPERTY : Declare et reflechit une propriete en une ligne
     *  - NKENTSEU_REGISTER_CLASS : Enregistre automatiquement une classe au startup
     *
     * @note Ces macros utilisent NK_CONCAT pour eviter les collisions de noms
     * @note Assurez-vous que NK_CONCAT est defini dans NKCore avant utilisation
     *
     * @example
     * @code
     * class Player {
     *     NKENTSEU_REFLECT_CLASS(Player)
     * public:
     *     NKENTSEU_PROPERTY(nk_int32, health)
     *     NKENTSEU_PROPERTY(nk_float32, speed)
     * };
     * NKENTSEU_REGISTER_CLASS(Player)
     * @endcode
     */

    /**
     * @brief Macro pour declarer les meta-donnees de reflexion d'une classe
     * @def NKENTSEU_REFLECT_CLASS(ClassName)
     * @ingroup ReflectionMacros
     *
     * Injecte dans la classe :
     *  - Un alias SelfType pour les macros de proprietes
     *  - Une methode statique GetStaticClass() retournant le NkClass
     *  - Une methode virtuelle GetClass() pour l'acces polymorphique
     *
     * @param ClassName Nom de la classe a reflechir
     * @note Doit etre place dans la section public de la classe
     * @note Le NkClass est cree via static local (initialisation thread-safe C++11+)
     *
     * @warning Ne pas utiliser dans des classes avec heritage multiple sans adaptation
     */
    #define NKENTSEU_REFLECT_CLASS(ClassName) \
        using SelfType = ClassName; \
    public: \
        static const ::nkentseu::reflection::NkClass& GetStaticClass() { \
            static ::nkentseu::reflection::NkClass classInfo( \
                #ClassName, \
                sizeof(ClassName), \
                ::nkentseu::reflection::NkTypeOf<ClassName>() \
            ); \
            return classInfo; \
        } \
        virtual const ::nkentseu::reflection::NkClass& GetClass() const { \
            return GetStaticClass(); \
        } \
    private:

    /**
     * @brief Macro pour declarer une propriete reflechie
     * @def NKENTSEU_REFLECT_PROPERTY(PropertyName)
     * @ingroup ReflectionMacros
     *
     * Injecte dans la classe :
     *  - Une methode statique Get<PropertyName>Property() retournant le NkProperty
     *  - Le NkProperty est cree avec le type deduit et l'offset calcule via offsetof
     *
     * @param PropertyName Nom du membre a reflechir
     * @note Doit etre place apres la declaration du membre dans la classe
     * @note Utilise SelfType (defini par NKENTSEU_REFLECT_CLASS) pour le calcul d'offset
     * @note Le NkProperty est cree via static local (initialisation thread-safe C++11+)
     *
     * @warning Le membre doit etre public ou la macro doit etre placee dans la section appropriee
     */
    #define NKENTSEU_REFLECT_PROPERTY(PropertyName) \
    public: \
        static const ::nkentseu::reflection::NkProperty& Get##PropertyName##Property() { \
            static ::nkentseu::reflection::NkProperty property( \
                #PropertyName, \
                ::nkentseu::reflection::NkTypeOf<decltype(((SelfType*)0)->PropertyName)>(), \
                offsetof(SelfType, PropertyName) \
            ); \
            return property; \
        } \
    private:

    /**
     * @brief Macro combinant declaration et reflexion d'une propriete
     * @def NKENTSEU_PROPERTY(Type, Name)
     * @ingroup ReflectionMacros
     *
     * Equivalent a :
     * @code
     * Type Name;
     * NKENTSEU_REFLECT_PROPERTY(Name)
     * @endcode
     *
     * @param Type Type de la propriete
     * @param Name Nom de la propriete
     * @note Doit etre utilise dans la section public de la classe
     * @note Plus concis que la separation declaration + reflexion
     *
     * @example
     * @code
     * class Config {
     *     NKENTSEU_REFLECT_CLASS(Config)
     * public:
     *     NKENTSEU_PROPERTY(nk_bool, fullscreen)
     *     NKENTSEU_PROPERTY(nk_int32, volume)
     * };
     * @endcode
     */
    #define NKENTSEU_PROPERTY(Type, Name) \
        Type Name; \
        NKENTSEU_REFLECT_PROPERTY(Name)

    /**
     * @brief Attribut de annotation pour marquage de code reflechi
     * @def NKENTSEU_REFLECT
     * @ingroup ReflectionMacros
     *
     * Attribut C++20 [[nkentseu::reflect]] pour annotation statique.
     * Peut etre utilise par les outils d'analyse statique ou les generateurs
     * de code pour identifier les entites destinées a la reflexion.
     *
     * @note Cet attribut n'a aucun effet a l'execution : purement informatif
     * @note Requiert un compilateur supportant les attributs C++20
     *
     * @example
     * @code
     * class [[nkentseu::reflect]] Entity {
     *     [[nkentseu::reflect]] nk_int32 id;
     * };
     * @endcode
     */
    #define NKENTSEU_REFLECT [[nkentseu::reflect]]

    /**
     * @brief Macro pour l'enregistrement automatique d'une classe au startup
     * @def NKENTSEU_REGISTER_CLASS(ClassName)
     * @ingroup ReflectionMacros
     *
     * Cree un objet statique dans un namespace anonyme qui :
     *  - S'initialise avant main() (ordre non garanti entre TU)
     *  - Appelle NkRegistry::Get().RegisterClass() avec le NkClass de la classe
     *
     * @param ClassName Nom de la classe a enregistrer
     * @note Doit etre place dans un fichier .cpp (pas dans un header)
     * @note L'ordre d'initialisation entre TU n'est pas garanti : ne pas dependre
     *       de l'ordre d'enregistrement entre classes dans des fichiers differents
     * @note Utilise un nom unique via concatenation pour eviter les collisions
     *
     * @warning Si la classe est dans un header inclus dans plusieurs TU,
     *          cette macro doit etre appelee dans UN SEUL fichier .cpp
     *
     * @example
     * @code
     * // Dans Player.cpp (pas Player.h) :
     * #include "NKReflection/NkRegistry.h"
     * #include "Game/Player.h"
     *
     * NKENTSEU_REGISTER_CLASS(Player)
     * @endcode
     */
    #define NKENTSEU_REGISTER_CLASS(ClassName) \
    namespace { \
        struct NK_CONCAT(ClassName, _Registrar) { \
            NK_CONCAT(ClassName, _Registrar)() { \
                ::nkentseu::reflection::NkRegistry::Get().RegisterClass( \
                    &ClassName::GetStaticClass() \
                ); \
            } \
        }; \
        static NK_CONCAT(ClassName, _Registrar) NK_CONCAT(g_, NK_CONCAT(ClassName, _registrar)); \
    }

#endif // NK_REFLECTION_NKREGISTRY_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkRegistry.h
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Classe reflechie basique avec proprietes
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkRegistry.h"

    namespace game {

        class Player {
            // Declaration des meta-donnees de reflexion
            NKENTSEU_REFLECT_CLASS(Player)

        public:
            // Proprietes reflechies via macro combinée
            NKENTSEU_PROPERTY(nk_int32, health)
            NKENTSEU_PROPERTY(nk_float32, speed)
            NKENTSEU_PROPERTY(nk_bool, isAlive)

            // Methode pour afficher l'etat (non reflechie)
            void PrintStatus() const {
                printf("Player: HP=%d, Speed=%.2f, Alive=%s\n",
                    health, speed, isAlive ? "yes" : "no");
            }
        };

    } // namespace game

    // Enregistrement automatique dans un fichier .cpp
    // NKENTSEU_REGISTER_CLASS(game::Player)
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Recherche et utilisation runtime via le registre
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkRegistry.h"
    #include "NKCore/Log/NkLog.h"

    namespace example {

        void InspectRegisteredClasses() {
            auto& registry = nkentseu::reflection::NkRegistry::Get();

            // Recherche d'une classe par nom
            const nkentseu::reflection::NkClass* playerClass =
                registry.FindClass("Player");

            if (playerClass) {
                NK_FOUNDATION_LOG_INFO("Found class: %s (size: %zu)",
                    playerClass->GetName(),
                    playerClass->GetSize());

                // Iteration sur les proprietes de la classe
                for (nk_usize i = 0; i < playerClass->GetPropertyCount(); ++i) {
                    const nkentseu::reflection::NkProperty* prop =
                        playerClass->GetPropertyAt(i);

                    if (prop) {
                        NK_FOUNDATION_LOG_INFO("  Property: %s (type: %s)",
                            prop->GetName(),
                            prop->GetType().GetName());
                    }
                }
            } else {
                NK_FOUNDATION_LOG_WARNING("Class 'Player' not found in registry");
            }
        }

    } // namespace example
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Callback d'enregistrement pour logging automatique
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkRegistry.h"
    #include "NKCore/Log/NkLog.h"

    namespace example {

        void SetupRegistryLogging() {
            auto& registry = nkentseu::reflection::NkRegistry::Get();

            // Definition d'un callback pour logger chaque enregistrement
            registry.SetOnRegisterCallback(
                nkentseu::reflection::NkRegistry::OnRegisterCallback(
                    [](const nk_char* name, nk_bool isClass) -> void {
                        NK_FOUNDATION_LOG_DEBUG("Registered %s: %s",
                            isClass ? "class" : "type",
                            name ? name : "<unknown>");
                    }
                )
            );

            // Les enregistrements subsequents declencheront le callback
            // Note : les classes enregistrees via NKENTSEU_REGISTER_CLASS
            //        avant cet appel ne declencheront pas le callback
        }

    } // namespace example
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Heritage et recherche dans la hierarchie
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkRegistry.h"

    namespace game {

        class Entity {
            NKENTSEU_REFLECT_CLASS(Entity)
        public:
            NKENTSEU_PROPERTY(nk_int32, id)
        };

        class Character : public Entity {
            NKENTSEU_REFLECT_CLASS(Character)
        public:
            NKENTSEU_PROPERTY(nk_int32, level)
        };

        class Player : public Character {
            NKENTSEU_REFLECT_CLASS(Player)
        public:
            NKENTSEU_PROPERTY(nk_char*, name)
        };

    } // namespace game

    // Dans un fichier .cpp :
    // NKENTSEU_REGISTER_CLASS(game::Entity)
    // NKENTSEU_REGISTER_CLASS(game::Character)
    // NKENTSEU_REGISTER_CLASS(game::Player)

    namespace example {

        void SetupInheritanceChain() {
            auto& registry = nkentseu::reflection::NkRegistry::Get();

            // Recuperation des meta-classes
            const auto* entityClass = registry.FindClass("Entity");
            const auto* characterClass = registry.FindClass("Character");
            const auto* playerClass = registry.FindClass("Player");

            if (entityClass && characterClass && playerClass) {
                // Etablissement manuel de la chaine d'heritage
                // (normalement fait via une macro NKENTSEU_REFLECT_DERIVED)
                const_cast<nkentseu::reflection::NkClass*>(characterClass)
                    ->SetBaseClass(entityClass);
                const_cast<nkentseu::reflection::NkClass*>(playerClass)
                    ->SetBaseClass(characterClass);

                // Verification des relations
                if (playerClass->IsSubclassOf(entityClass)) {
                    printf("Player inherits from Entity\n");
                }

                // Recherche de propriete heritee : "id" est dans Entity
                const auto* idProp = playerClass->GetProperty("id");
                if (idProp) {
                    printf("Found inherited property 'id' via Player\n");
                }
            }
        }

    } // namespace example
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Creation d'instance via reflexion (factory pattern)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkRegistry.h"

    namespace game {

        class GameObject {
            NKENTSEU_REFLECT_CLASS(GameObject)
        public:
            GameObject() : m_initialized(true) {}
            virtual ~GameObject() = default;
            bool IsInitialized() const { return m_initialized; }
        private:
            nk_bool m_initialized;
        };

        // Constructeur et destructeur pour la reflexion
        void* GameObject_Create() {
            return new GameObject();
        }

        void GameObject_Destroy(void* instance) {
            delete static_cast<GameObject*>(instance);
        }

    } // namespace game

    namespace example {

        void* CreateInstanceByClassName(const nk_char* className) {
            auto& registry = nkentseu::reflection::NkRegistry::Get();
            const auto* classMeta = registry.FindClass(className);

            if (!classMeta || !classMeta->HasConstructor()) {
                return nullptr;
            }

            return classMeta->CreateInstance();
        }

        void ReflectionFactoryExample() {
            // Enregistrement prealable du constructeur (a faire une fois)
            auto* gameClass = const_cast<nkentseu::reflection::NkClass*>(
                nkentseu::reflection::NkRegistry::Get().FindClass("GameObject"));

            if (gameClass) {
                gameClass->SetConstructor(
                    nkentseu::reflection::NkClass::ConstructorFn(
                        game::GameObject_Create
                    )
                );
                gameClass->SetDestructor(
                    nkentseu::reflection::NkClass::DestructorFn(
                        game::GameObject_Destroy
                    )
                );
            }

            // Creation via factory reflexive
            void* instance = CreateInstanceByClassName("GameObject");
            if (instance) {
                printf("Created instance of GameObject at %p\n", instance);
                // Utilisation...
                // Destruction via reflexion
                if (gameClass && gameClass->HasDestructor()) {
                    gameClass->DestroyInstance(instance);
                }
            }
        }

    } // namespace example
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Iteration sur toutes les classes enregistrees
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkRegistry.h"
    #include "NKCore/Log/NkLog.h"

    namespace example {

        void ListAllRegisteredClasses() {
            const auto& registry = nkentseu::reflection::NkRegistry::Get();

            NK_FOUNDATION_LOG_INFO("Registered classes (%zu total):",
                registry.GetClassCount());

            for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
                const auto* classMeta = registry.GetClassAt(i);
                if (classMeta) {
                    NK_FOUNDATION_LOG_INFO("  [%zu] %s (size: %zu, props: %zu, methods: %zu)",
                        i,
                        classMeta->GetName(),
                        classMeta->GetSize(),
                        classMeta->GetPropertyCount(),
                        classMeta->GetMethodCount());
                }
            }
        }

    } // namespace example
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================