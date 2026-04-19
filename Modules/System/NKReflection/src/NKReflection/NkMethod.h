// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkMethod.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime d'une methode (nom, type de retour, flags, liste des
//   parametres) et point d'invocation indirecte.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKMETHOD_H_INCLUDED
#define NK_REFLECTION_NKMETHOD_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKReflection/NkType.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    namespace reflection {

        // =====================================================================
        // Enum : NkMethodFlags
        // =====================================================================

        enum class NkMethodFlags : uint32 {
            NK_NONE = 0,
            NK_STATIC = 1 << 0,
            NK_MCONST = 1 << 1,
            NK_VIRTUAL = 1 << 2,
            NK_ABSTRACT = 1 << 3,
            NK_MFINAL = 1 << 4,
            NK_MDEPRECATED = 1 << 5
        };

        // =====================================================================
        // Classe : NkMethod
        // =====================================================================

        class NKENTSEU_CORE_API NkMethod {
            public:
                using InvokeFn = void*(*)(void*, void**);

                NkMethod(
                    const char* name,
                    const NkType& returnType,
                    uint32 flags = 0
                )
                    : mName(name)
                    , mReturnType(&returnType)
                    , mFlags(flags)
                    , mInvoke(nullptr)
                    , mParameterTypes(nullptr)
                    , mParameterCount(0) {
                }

                // -- Accesseurs ------------------------------------------------
                const char* GetName() const {
                    return mName;
                }

                const NkType& GetReturnType() const {
                    return *mReturnType;
                }

                uint32 GetFlags() const {
                    return mFlags;
                }

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

                // -- Parametres ------------------------------------------------
                void SetParameters(const NkType** types, usize count) {
                    mParameterTypes = types;
                    mParameterCount = count;
                }

                usize GetParameterCount() const {
                    return mParameterCount;
                }

                const NkType& GetParameterType(usize index) const {
                    NKENTSEU_ASSERT(index < mParameterCount);
                    return *mParameterTypes[index];
                }

                // -- Invocation ------------------------------------------------
                void SetInvoke(InvokeFn invoke) {
                    mInvoke = invoke;
                }

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

#endif // NK_REFLECTION_NKMETHOD_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkMethod
// =============================================================================
//
//   const nkentseu::reflection::NkType& voidType =
//       nkentseu::reflection::NkTypeOf<void>();
//
//   nkentseu::reflection::NkMethod m(
//       "Tick",
//       voidType,
//       static_cast<nkentseu::uint32>(nkentseu::reflection::NkMethodFlags::NK_NONE)
//   );
//
//   // Invocation indirecte : m.SetInvoke(...); puis m.Invoke(instance, args);
//
// =============================================================================
