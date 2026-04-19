// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkRegistry.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Registre global des types/classes runtime + macros de declaration et
//   d'enregistrement de reflection.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKREGISTRY_H_INCLUDED
#define NK_REFLECTION_NKREGISTRY_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKReflection/NkType.h"
#include "NKReflection/NkClass.h"
#include "NKReflection/NkProperty.h"
#include "NKReflection/NkMethod.h"
#include "NKCore/NkTraits.h"

// -- En-tetes standard minimaux ----------------------------------------------
#include <cstddef>
#include <cstring>
#include <typeinfo>

namespace nkentseu {
    namespace reflection {

        // =====================================================================
        // Classe : NkRegistry
        // =====================================================================

        class NKENTSEU_CORE_API NkRegistry {
            public:
                static NkRegistry& Get() {
                    static NkRegistry instance;
                    return instance;
                }

                // -- Enregistrement types --------------------------------------
                void RegisterType(const NkType* type) {
                    if (!type || !type->GetName()) {
                        return;
                    }

                    for (usize i = 0; i < mTypeCount; ++i) {
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

                const NkType* FindType(const nk_char* name) const {
                    if (!name || name[0] == '\0') {
                        return nullptr;
                    }

                    for (usize i = 0; i < mTypeCount; ++i) {
                        if (mTypes[i] && mTypes[i]->GetName() && ::strcmp(mTypes[i]->GetName(), name) == 0) {
                            return mTypes[i];
                        }
                    }

                    return nullptr;
                }

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

                // -- Enregistrement classes ------------------------------------
                void RegisterClass(const NkClass* classInfo) {
                    if (!classInfo || !classInfo->GetName()) {
                        return;
                    }

                    for (usize i = 0; i < mClassCount; ++i) {
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

                const NkClass* FindClass(const nk_char* name) const {
                    if (!name || name[0] == '\0') {
                        return nullptr;
                    }

                    for (usize i = 0; i < mClassCount; ++i) {
                        if (mClasses[i] && mClasses[i]->GetName() && ::strcmp(mClasses[i]->GetName(), name) == 0) {
                            return mClasses[i];
                        }
                    }

                    return nullptr;
                }

                template<typename T>
                const NkClass* GetClass() const {
                    return FindClass(typeid(T).name());
                }

            private:
                static constexpr usize NK_MAX_TYPES = 512;
                static constexpr usize NK_MAX_CLASSES = 512;

                NkRegistry()
                    : mTypeCount(0)
                    , mClassCount(0) {
                    for (usize i = 0; i < NK_MAX_TYPES; ++i) {
                        mTypes[i] = nullptr;
                    }

                    for (usize i = 0; i < NK_MAX_CLASSES; ++i) {
                        mClasses[i] = nullptr;
                    }
                }

                NkRegistry(const NkRegistry&) = delete;
                NkRegistry& operator=(const NkRegistry&) = delete;

                const NkType* mTypes[NK_MAX_TYPES];
                usize mTypeCount;
                const NkClass* mClasses[NK_MAX_CLASSES];
                usize mClassCount;
        };

    } // namespace reflection
} // namespace nkentseu

// =============================================================================
// Macros de reflection
// =============================================================================

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

#define NKENTSEU_PROPERTY(Type, Name) \
    Type Name; \
    NKENTSEU_REFLECT_PROPERTY(Name)

#define NKENTSEU_REFLECT [[nkentseu::reflect]]

#define NKENTSEU_REGISTER_CLASS(ClassName) \
    namespace { \
        struct ClassName##_Registrar { \
            ClassName##_Registrar() { \
                ::nkentseu::reflection::NkRegistry::Get().RegisterClass( \
                    &ClassName::GetStaticClass() \
                ); \
            } \
        }; \
        static ClassName##_Registrar g_##ClassName##_registrar; \
    }

#endif // NK_REFLECTION_NKREGISTRY_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkRegistry
// =============================================================================
//
//   class Player {
//       NKENTSEU_REFLECT_CLASS(Player)
//
//       public:
//           nkentseu::nk_int32 health;
//           NKENTSEU_REFLECT_PROPERTY(health)
//   };
//
//   NKENTSEU_REGISTER_CLASS(Player)
//
//   const nkentseu::reflection::NkClass* cls =
//       nkentseu::reflection::NkRegistry::Get().FindClass("Player");
//   (void)cls;
//
// =============================================================================
