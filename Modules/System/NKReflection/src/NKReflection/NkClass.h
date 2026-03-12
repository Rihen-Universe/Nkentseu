// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Reflection\NkClass.h
// DESCRIPTION: Système de réflexion - Class metadata
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKCLASS_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKCLASS_H_INCLUDED

#include "NKReflection/NkType.h"
#include "NKReflection/NkProperty.h"
#include "NKReflection/NkMethod.h"

#include <cstring> // Pour strcmp

namespace nkentseu {
    
        namespace reflection {
            
            /**
             * @brief Métadonnées d'une classe
             */
            class NKENTSEU_CORE_API NkClass {
            public:
                /**
                 * @brief Constructeur
                 */
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
                    , mDestructor(nullptr)
                {}
                
                // ========================================
                // PROPERTIES
                // ========================================
                
                const char* GetName() const { return mName; }
                usize GetSize() const { return mSize; }
                const NkType& GetType() const { return *mType; }
                
                // ========================================
                // INHERITANCE
                // ========================================
                
                const NkClass* GetBaseClass() const { return mBaseClass; }
                void SetBaseClass(const NkClass* base) { mBaseClass = base; }
                
                bool IsSubclassOf(const NkClass* other) const {
                    const NkClass* current = this;
                    while (current) {
                        if (current == other) return true;
                        current = current->mBaseClass;
                    }
                    return false;
                }
                
                bool IsSuperclassOf(const NkClass* other) const {
                    return other && other->IsSubclassOf(this);
                }
                
                // ========================================
                // PROPERTIES
                // ========================================
                
                void AddProperty(const NkProperty* property) {
                    if (property == nullptr) {
                        return;
                    }
                    // Simplifié: dans implémentation réelle, utiliser Vector
                    // Pour l'instant, assume tableau statique
                    if (mPropertyCount < 64) {
                        mProperties[mPropertyCount++] = property;
                    }
                }
                
                const NkProperty* GetProperty(const char* name) const {
                    for (usize i = 0; i < mPropertyCount; ++i) {
                        if (strcmp(mProperties[i]->GetName(), name) == 0) {
                            return mProperties[i];
                        }
                    }
                    
                    // Chercher dans classe de base
                    if (mBaseClass) {
                        return mBaseClass->GetProperty(name);
                    }
                    
                    return nullptr;
                }
                
                const NkProperty* GetPropertyAt(usize index) const {
                    return index < mPropertyCount ? mProperties[index] : nullptr;
                }
                
                usize GetPropertyCount() const { return mPropertyCount; }
                
                // ========================================
                // METHODS
                // ========================================
                
                void AddMethod(const NkMethod* method) {
                    if (method == nullptr) {
                        return;
                    }
                    if (mMethodCount < 64) {
                        mMethods[mMethodCount++] = method;
                    }
                }
                
                const NkMethod* GetMethod(const char* name) const {
                    for (usize i = 0; i < mMethodCount; ++i) {
                        if (strcmp(mMethods[i]->GetName(), name) == 0) {
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
                
                usize GetMethodCount() const { return mMethodCount; }
                
                // ========================================
                // INSTANCE CREATION (si supporté)
                // ========================================
                
                using ConstructorFn = void*(*)(void);
                using DestructorFn = void(*)(void*);
                
                void SetConstructor(ConstructorFn ctor) { mConstructor = ctor; }
                void SetDestructor(DestructorFn dtor) { mDestructor = dtor; }
                
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

#endif
