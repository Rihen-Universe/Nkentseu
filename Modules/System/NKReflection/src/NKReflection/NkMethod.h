// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Reflection\NkMethod.h
// DESCRIPTION: Système de réflexion - Method metadata
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKMETHOD_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKMETHOD_H_INCLUDED

#include "NKReflection/NkType.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    
        namespace reflection {
            
            /**
             * @brief Flags de méthode
             */
            enum class NkMethodFlags : uint32 {
                NK_NONE       = 0,
                NK_STATIC     = 1 << 0,
                NK_MCONST      = 1 << 1,
                NK_VIRTUAL    = 1 << 2,
                NK_ABSTRACT   = 1 << 3,
                NK_MFINAL      = 1 << 4,
                NK_MDEPRECATED = 1 << 5
            };
            
            /**
             * @brief Métadonnées d'une méthode
             */
            class NKENTSEU_CORE_API NkMethod {
            public:
                using InvokeFn = void*(*)(void*, void**);
                
                /**
                 * @brief Constructeur
                 */
                NkMethod(const char* name, const NkType& returnType, uint32 flags = 0)
                    : mName(name)
                    , mReturnType(&returnType)
                    , mFlags(flags)
                    , mInvoke(nullptr)
                    , mParameterTypes(nullptr)
                    , mParameterCount(0)
                {}
                
                // ========================================
                // PROPERTIES
                // ========================================
                
                const char* GetName() const { return mName; }
                const NkType& GetReturnType() const { return *mReturnType; }
                uint32 GetFlags() const { return mFlags; }
                
                bool IsStatic() const {
                    return (mFlags & static_cast<uint32>(NkMethodFlags::NK_STATIC)) != 0;
                }
                
                bool IsConst() const {
                    return (mFlags & static_cast<uint32>(NkMethodFlags::NK_MCONST)) != 0;
                }
                
                bool IsVirtual() const {
                    return (mFlags & static_cast<uint32>(NkMethodFlags::NK_VIRTUAL)) != 0;
                }

                bool IsFinal() const {
                    return (mFlags & static_cast<uint32>(NkMethodFlags::NK_MFINAL)) != 0;
                }

                bool IsAbstract() const {
                    return (mFlags & static_cast<uint32>(NkMethodFlags::NK_ABSTRACT)) != 0;
                }

                bool IsDeprecated() const {
                    return (mFlags & static_cast<uint32>(NkMethodFlags::NK_MDEPRECATED)) != 0;
                }
                
                // ========================================
                // PARAMETERS
                // ========================================
                
                void SetParameters(const NkType** types, usize count) {
                    mParameterTypes = types;
                    mParameterCount = count;
                }
                
                usize GetParameterCount() const { return mParameterCount; }
                
                const NkType& GetParameterType(usize index) const {
                    NKENTSEU_ASSERT(index < mParameterCount);
                    return *mParameterTypes[index];
                }
                
                // ========================================
                // INVOCATION
                // ========================================
                
                void SetInvoke(InvokeFn invoke) { mInvoke = invoke; }
                
                /**
                 * @brief Invoke la méthode
                 * @param instance Pointeur vers instance (nullptr si static)
                 * @param args Tableau de pointeurs vers arguments
                 * @return Pointeur vers résultat
                 */
                void* Invoke(void* instance, void** args) const {
                    NKENTSEU_ASSERT(mInvoke);
                    return mInvoke(instance, args);
                }
                
            private:
                const char* mName;
                const NkType* mReturnType;
                uint32 mFlags;
                InvokeFn mInvoke;
                const NkType** mParameterTypes;
                usize mParameterCount;
            };
            
        } // namespace reflection
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKMETHOD_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
