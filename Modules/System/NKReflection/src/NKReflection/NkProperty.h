// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkProperty.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime d'une propriete (nom, type, offset, flags), avec
//   acces direct ou par getter/setter.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKPROPERTY_H_INCLUDED
#define NK_REFLECTION_NKPROPERTY_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKReflection/NkType.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    namespace reflection {

        // =====================================================================
        // Enum : NkPropertyFlags
        // =====================================================================

        enum class NkPropertyFlags : uint32 {
            NK_NONE = 0,
            NK_READ_ONLY = 1 << 0,
            NK_WRITE_ONLY = 1 << 1,
            NK_STATIC = 1 << 2,
            NK_PCONST = 1 << 3,
            NK_TRANSIENT = 1 << 4,
            NK_PDEPRECATED = 1 << 5
        };

        // =====================================================================
        // Classe : NkProperty
        // =====================================================================

        class NKENTSEU_CORE_API NkProperty {
            public:
                using GetterFn = void*(*)(void*);
                using SetterFn = void(*)(void*, void*);

                NkProperty(
                    const char* name,
                    const NkType& type,
                    usize offset,
                    uint32 flags = 0
                )
                    : mName(name)
                    , mType(&type)
                    , mOffset(offset)
                    , mFlags(flags)
                    , mGetter(nullptr)
                    , mSetter(nullptr) {
                }

                // -- Accesseurs ------------------------------------------------
                const char* GetName() const {
                    return mName;
                }

                const NkType& GetType() const {
                    return *mType;
                }

                usize GetOffset() const {
                    return mOffset;
                }

                uint32 GetFlags() const {
                    return mFlags;
                }

                bool IsReadOnly() const {
                    return (mFlags & static_cast<uint32>(NkPropertyFlags::NK_READ_ONLY)) != 0;
                }

                bool IsStatic() const {
                    return (mFlags & static_cast<uint32>(NkPropertyFlags::NK_STATIC)) != 0;
                }

                bool IsConst() const {
                    return (mFlags & static_cast<uint32>(NkPropertyFlags::NK_PCONST)) != 0;
                }

                // -- Accessors runtime ----------------------------------------
                void SetGetter(GetterFn getter) {
                    mGetter = getter;
                }

                void SetSetter(SetterFn setter) {
                    mSetter = setter;
                }

                template<typename T>
                const T& GetValue(void* instance) const {
                    if (mGetter) {
                        return *static_cast<T*>(mGetter(instance));
                    }

                    return *reinterpret_cast<T*>(static_cast<char*>(instance) + mOffset);
                }

                template<typename T>
                void SetValue(void* instance, const T& value) const {
                    NKENTSEU_ASSERT(!IsReadOnly());

                    if (mSetter) {
                        mSetter(instance, const_cast<T*>(&value));
                    } else {
                        *reinterpret_cast<T*>(static_cast<char*>(instance) + mOffset) = value;
                    }
                }

                void* GetValuePtr(void* instance) const {
                    return static_cast<char*>(instance) + mOffset;
                }

            private:
                const char* mName;
                const NkType* mType;
                usize mOffset;
                uint32 mFlags;
                GetterFn mGetter;
                SetterFn mSetter;
        };

    } // namespace reflection
} // namespace nkentseu

#endif // NK_REFLECTION_NKPROPERTY_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkProperty
// =============================================================================
//
//   struct PlayerData {
//       nkentseu::nk_int32 health;
//   };
//
//   PlayerData p { 100 };
//   const nkentseu::reflection::NkType& i32Type =
//       nkentseu::reflection::NkTypeOf<nkentseu::nk_int32>();
//
//   nkentseu::reflection::NkProperty healthProp(
//       "health",
//       i32Type,
//       offsetof(PlayerData, health)
//   );
//
//   nkentseu::nk_int32 v = healthProp.GetValue<nkentseu::nk_int32>(&p);
//   healthProp.SetValue<nkentseu::nk_int32>(&p, v + 10);
//
// =============================================================================
