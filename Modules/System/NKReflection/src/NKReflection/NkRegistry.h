// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Reflection\NkRegistry.h
// DESCRIPTION: Registre global de réflexion + Macros
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKREGISTRY_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKREGISTRY_H_INCLUDED

#include "NkType.h"
#include "NkClass.h"
#include "NkProperty.h"
#include "NkMethod.h"

namespace nkentseu {
    namespace core {
        namespace reflection {
            
            /**
             * @brief Registre global de réflexion
             */
            class NKENTSEU_CORE_API NkRegistry {
                public:
                    static NkRegistry& Get() {
                        static NkRegistry instance;
                        return instance;
                    }
                    
                    // ========================================
                    // TYPE REGISTRATION
                    // ========================================
                    
                    void RegisterType(const NkType* type) {
                        // Ajouter au registre (implémentation simplifiée)
                    }
                    
                    const NkType* FindType(const Char* name) const {
                        // Chercher dans registre
                        return nullptr;
                    }
                    
                    template<typename T>
                    const NkType* GetType() const {
                        static NkType type(
                            typeid(T).name(),
                            sizeof(T),
                            alignof(T),
                            DetermineCategory<T>()
                        );
                        return &type;
                    }
                    
                    // ========================================
                    // CLASS REGISTRATION
                    // ========================================
                    
                    void RegisterClass(const NkClass* classInfo) {
                        // Ajouter au registre
                    }
                    
                    const NkClass* FindClass(const Char* name) const {
                        // Chercher dans registre
                        return nullptr;
                    }
                    
                    template<typename T>
                    const NkClass* GetClass() const {
                        return FindClass(typeid(T).name());
                    }
                    
                private:
                    NkRegistry() {} // Constructeur par défaut privé
                    NkRegistry(const NkRegistry&) = delete;
                    NkRegistry& operator=(const NkRegistry&) = delete;
            };
            
        } // namespace reflection
    } // namespace core
} // namespace nkentseu

// ============================================================
// MACROS DE RÉFLEXION
// ============================================================

/**
 * @brief Déclare une classe réfléchie
 */
#define NKENTSEU_REFLECT_CLASS(ClassName) \
    friend class ::nkentseu::core::reflection::Class; \
    public: \
    static const ::nkentseu::core::reflection::Class& GetStaticClass() { \
        static ::nkentseu::core::reflection::Class classInfo( \
            #ClassName, \
            sizeof(ClassName), \
            ::nkentseu::core::reflection::TypeOf<ClassName>() \
        ); \
        return classInfo; \
    } \
    virtual const ::nkentseu::core::reflection::Class& GetClass() const { \
        return GetStaticClass(); \
    } \
    private:

/**
 * @brief Déclare une propriété réfléchie
 */
#define NKENTSEU_REFLECT_PROPERTY(PropertyName) \
    public: \
    static const ::nkentseu::core::reflection::Property& Get##PropertyName##Property() { \
        static ::nkentseu::core::reflection::Property property( \
            #PropertyName, \
            ::nkentseu::core::reflection::TypeOf<decltype(PropertyName)>(), \
            offsetof(std::remove_reference<decltype(*this)>::type, PropertyName) \
        ); \
        return property; \
    } \
    private:


#define NKENTSEU_PROPERTY(Type, Name) \
    Type Name; \
    NKENTSEU_REFLECT_PROPERTY(Name)

#define NKENTSEU_REFLECT [[nkentseu::reflect]]

/**
 * @brief Enregistre une classe dans le registre
 */
#define NKENTSEU_REGISTER_CLASS(ClassName) \
    namespace { \
        struct ClassName##_Registrar { \
            ClassName##_Registrar() { \
                ::nkentseu::core::reflection::Registry::Get().RegisterClass( \
                    &ClassName::GetStaticClass() \
                ); \
            } \
        }; \
        static ClassName##_Registrar g_##ClassName##_registrar; \
    }

/**
 * @brief Utilisation simplifiée
 * 
 * @example
 * class Player {
 *     NKENTSEU_REFLECT_CLASS(Player)
 *     
 * public:
 *     int health;
 *     NKENTSEU_REFLECT_PROPERTY(health)
 *     
 *     float speed;
 *     NKENTSEU_REFLECT_PROPERTY(speed)
 * };
 * 
 * NKENTSEU_REGISTER_CLASS(Player)
 */

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKREGISTRY_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================