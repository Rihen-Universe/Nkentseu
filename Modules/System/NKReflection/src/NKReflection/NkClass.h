// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkClass.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime d'une classe : nom, type associe, heritage,
//   proprietes/methodes et callbacks de construction/destruction.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKCLASS_H_INCLUDED
#define NK_REFLECTION_NKCLASS_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKReflection/NkType.h"
#include "NKReflection/NkProperty.h"
#include "NKReflection/NkMethod.h"

// -- En-tetes standard minimaux ----------------------------------------------
#include <cstring>

namespace nkentseu {
    namespace reflection {

        // =====================================================================
        // Classe : NkClass
        // =====================================================================

        class NKENTSEU_CORE_API NkClass {
            public:
                using ConstructorFn = void*(*)(void);
                using DestructorFn = void(*)(void*);

                NkClass(const char* name, usize size, const NkType& type)
                    : mName(name)
                    , mSize(size)
                    , mType(&type)
                    , mBaseClass(nullptr)
                    , mProperties(mPropertyStorage)
                    , mPropertyCount(0)
                    , mMethods(mMethodStorage)
                    , mMethodCount(0)
                    , mConstructor(nullptr)
                    , mDestructor(nullptr) {
                }

                // -- Infos generales -------------------------------------------
                const char* GetName() const {
                    return mName;
                }

                usize GetSize() const {
                    return mSize;
                }

                const NkType& GetType() const {
                    return *mType;
                }

                // -- Heritage ---------------------------------------------------
                const NkClass* GetBaseClass() const {
                    return mBaseClass;
                }

                void SetBaseClass(const NkClass* base) {
                    mBaseClass = base;
                }

                bool IsSubclassOf(const NkClass* other) const {
                    const NkClass* current = this;
                    while (current) {
                        if (current == other) {
                            return true;
                        }
                        current = current->mBaseClass;
                    }
                    return false;
                }

                bool IsSuperclassOf(const NkClass* other) const {
                    return other && other->IsSubclassOf(this);
                }

                // -- Proprietes -------------------------------------------------
                void AddProperty(const NkProperty* property) {
                    if (!property) {
                        return;
                    }

                    if (mPropertyCount < 64) {
                        mProperties[mPropertyCount++] = property;
                    }
                }

                const NkProperty* GetProperty(const char* name) const {
                    if (!name) {
                        return nullptr;
                    }

                    for (usize i = 0; i < mPropertyCount; ++i) {
                        if (std::strcmp(mProperties[i]->GetName(), name) == 0) {
                            return mProperties[i];
                        }
                    }

                    if (mBaseClass) {
                        return mBaseClass->GetProperty(name);
                    }

                    return nullptr;
                }

                const NkProperty* GetPropertyAt(usize index) const {
                    return index < mPropertyCount ? mProperties[index] : nullptr;
                }

                usize GetPropertyCount() const {
                    return mPropertyCount;
                }

                // -- Methodes ---------------------------------------------------
                void AddMethod(const NkMethod* method) {
                    if (!method) {
                        return;
                    }

                    if (mMethodCount < 64) {
                        mMethods[mMethodCount++] = method;
                    }
                }

                const NkMethod* GetMethod(const char* name) const {
                    if (!name) {
                        return nullptr;
                    }

                    for (usize i = 0; i < mMethodCount; ++i) {
                        if (std::strcmp(mMethods[i]->GetName(), name) == 0) {
                            return mMethods[i];
                        }
                    }

                    if (mBaseClass) {
                        return mBaseClass->GetMethod(name);
                    }

                    return nullptr;
                }

                const NkMethod* GetMethodAt(usize index) const {
                    return index < mMethodCount ? mMethods[index] : nullptr;
                }

                usize GetMethodCount() const {
                    return mMethodCount;
                }

                // -- Instances (optionnel) -------------------------------------
                void SetConstructor(ConstructorFn ctor) {
                    mConstructor = ctor;
                }

                void SetDestructor(DestructorFn dtor) {
                    mDestructor = dtor;
                }

                void* CreateInstance() const {
                    return mConstructor ? mConstructor() : nullptr;
                }

                void DestroyInstance(void* instance) const {
                    if (mDestructor && instance) {
                        mDestructor(instance);
                    }
                }

            private:
                const char* mName;
                usize mSize;
                const NkType* mType;
                const NkClass* mBaseClass;

                const NkProperty* mPropertyStorage[64];
                const NkProperty** mProperties;
                usize mPropertyCount;

                const NkMethod* mMethodStorage[64];
                const NkMethod** mMethods;
                usize mMethodCount;

                ConstructorFn mConstructor;
                DestructorFn mDestructor;
        };

    } // namespace reflection
} // namespace nkentseu

#endif // NK_REFLECTION_NKCLASS_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkClass
// =============================================================================
//
//   const nkentseu::reflection::NkType& t =
//       nkentseu::reflection::NkTypeOf<nkentseu::nk_int32>();
//
//   nkentseu::reflection::NkClass c("Dummy", sizeof(nkentseu::nk_int32), t);
//   c.GetName();
//   c.GetPropertyCount();
//   c.GetMethodCount();
//
// =============================================================================
