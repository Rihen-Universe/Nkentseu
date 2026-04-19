// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkType.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime sur les types (nom, taille, categorie) et registre
//   de types. API sans dependance STL sur les conteneurs runtime du module.
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKTYPE_H_INCLUDED
#define NK_REFLECTION_NKTYPE_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKCore/NkTypes.h"
#include "NKCore/NkExport.h"
#include "NKCore/NkTraits.h"

// -- En-tetes standard minimaux ----------------------------------------------
#include <typeinfo>

namespace nkentseu {
    namespace reflection {

        // =====================================================================
        // Declarations anticipees
        // =====================================================================

        class NkClass;
        class NkProperty;
        class NkMethod;
        enum class NkTypeCategory;

        template<typename T>
        constexpr NkTypeCategory DetermineCategory();

        // =====================================================================
        // Enum : NkTypeCategory
        // =====================================================================

        enum class NkTypeCategory {
            NK_BOOL,
            NK_INT8,
            NK_INT16,
            NK_INT32,
            NK_INT64,
            NK_UINT8,
            NK_UINT16,
            NK_UINT32,
            NK_UINT64,
            NK_FLOAT32,
            NK_FLOAT64,
            NK_nk_char,
            NK_STRING,
            NK_POINTER,
            NK_REFERENCE,
            NK_ARRAY,
            NK_VECTOR,
            NK_CLASS,
            NK_STRUCT,
            NK_ENUM,
            NK_UNION,
            NK_FUNCTION,
            NK_VOID,
            NK_UNKNOWN
        };

        // =====================================================================
        // Classe : NkType
        // =====================================================================

        class NKENTSEU_CORE_API NkType {
            public:
                NkType(
                    const nk_char* name,
                    nk_usize size,
                    nk_usize alignment,
                    NkTypeCategory category
                )
                    : mName(name)
                    , mSize(size)
                    , mAlignment(alignment)
                    , mCategory(category)
                    , mClass(nullptr) {
                }

                virtual ~NkType() = default;

                // -- Accesseurs ------------------------------------------------
                const nk_char* GetName() const {
                    return mName;
                }

                nk_usize GetSize() const {
                    return mSize;
                }

                nk_usize GetAlignment() const {
                    return mAlignment;
                }

                NkTypeCategory GetCategory() const {
                    return mCategory;
                }

                nk_bool IsClass() const {
                    return mCategory == NkTypeCategory::NK_CLASS;
                }

                nk_bool IsPointer() const {
                    return mCategory == NkTypeCategory::NK_POINTER;
                }

                nk_bool IsReference() const {
                    return mCategory == NkTypeCategory::NK_REFERENCE;
                }

                nk_bool IsArray() const {
                    return mCategory == NkTypeCategory::NK_ARRAY;
                }

                nk_bool IsEnum() const {
                    return mCategory == NkTypeCategory::NK_ENUM;
                }

                nk_bool IsPrimitive() const {
                    return mCategory >= NkTypeCategory::NK_BOOL && mCategory <= NkTypeCategory::NK_FLOAT64;
                }

                // -- Association de classe -------------------------------------
                const NkClass* GetClass() const {
                    return mClass;
                }

                void SetClass(const NkClass* classInfo) {
                    mClass = classInfo;
                }

                // -- Comparaison ------------------------------------------------
                nk_bool operator==(const NkType& other) const {
                    return this == &other || (mName == other.mName && mSize == other.mSize);
                }

                nk_bool operator!=(const NkType& other) const {
                    return !(*this == other);
                }

            private:
                const nk_char* mName;
                nk_usize mSize;
                nk_usize mAlignment;
                NkTypeCategory mCategory;
                const NkClass* mClass;
        };

        // =====================================================================
        // Classe : NkTypeRegistry
        // =====================================================================

        class NKENTSEU_CORE_API NkTypeRegistry {
            public:
                static NkTypeRegistry& Get();

                void RegisterType(const NkType* type);
                const NkType* FindType(const nk_char* name) const;

                template<typename T>
                const NkType* GetType() const {
                    return FindType(GetTypeName<T>());
                }

            private:
                static constexpr nk_usize NK_MAX_TYPES = 512;

                NkTypeRegistry()
                    : mTypes(mTypeStorage)
                    , mTypeCount(0)
                    , mTypeCapacity(NK_MAX_TYPES) {
                    for (nk_usize i = 0; i < NK_MAX_TYPES; ++i) {
                        mTypeStorage[i] = nullptr;
                    }
                }

                template<typename T>
                static const nk_char* GetTypeName() {
                    return typeid(T).name();
                }

                const NkType* mTypeStorage[NK_MAX_TYPES];
                const NkType** mTypes;
                nk_usize mTypeCount;
                nk_usize mTypeCapacity;
        };

        // =====================================================================
        // Helpers : NkTypeOf
        // =====================================================================

        template<typename T>
        const NkType& NkTypeOf() {
            static NkType type(
                typeid(T).name(),
                sizeof(T),
                alignof(T),
                DetermineCategory<T>()
            );
            return type;
        }

        template<typename T>
        const NkType& NkTypeOf(const T&) {
            return NkTypeOf<T>();
        }

        // =====================================================================
        // Detection de categories
        // =====================================================================

        template<typename T>
        constexpr NkTypeCategory DetermineCategory() {
            if (traits::NkIsSame<T, void>::value) {
                return NkTypeCategory::NK_VOID;
            }

            if (traits::NkIsSame<T, nk_bool>::value) {
                return NkTypeCategory::NK_BOOL;
            }

            if (traits::NkIsSame<T, nk_int8>::value) {
                return NkTypeCategory::NK_INT8;
            }
            if (traits::NkIsSame<T, nk_int16>::value) {
                return NkTypeCategory::NK_INT16;
            }
            if (traits::NkIsSame<T, nk_int32>::value) {
                return NkTypeCategory::NK_INT32;
            }
            if (traits::NkIsSame<T, nk_int64>::value) {
                return NkTypeCategory::NK_INT64;
            }

            if (traits::NkIsSame<T, nk_uint8>::value) {
                return NkTypeCategory::NK_UINT8;
            }
            if (traits::NkIsSame<T, nk_uint16>::value) {
                return NkTypeCategory::NK_UINT16;
            }
            if (traits::NkIsSame<T, nk_uint32>::value) {
                return NkTypeCategory::NK_UINT32;
            }
            if (traits::NkIsSame<T, nk_uint64>::value) {
                return NkTypeCategory::NK_UINT64;
            }

            if (traits::NkIsSame<T, nk_float32>::value) {
                return NkTypeCategory::NK_FLOAT32;
            }
            if (traits::NkIsSame<T, nk_float64>::value) {
                return NkTypeCategory::NK_FLOAT64;
            }

            if (traits::NkIsPointer<T>::value) {
                return NkTypeCategory::NK_POINTER;
            }
            if (traits::NkIsReference<T>::value) {
                return NkTypeCategory::NK_REFERENCE;
            }
            if (traits::NkIsArray<T>::value) {
                return NkTypeCategory::NK_ARRAY;
            }

            return NkTypeCategory::NK_CLASS;
        }

    } // namespace reflection
} // namespace nkentseu

#endif // NK_REFLECTION_NKTYPE_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkType
// =============================================================================
//
//   const NkType& ti = nkentseu::reflection::NkTypeOf<nkentseu::nk_int32>();
//   const NkType& tf = nkentseu::reflection::NkTypeOf<nkentseu::nk_float32>();
//
//   nkentseu::nk_bool intIsPrimitive = ti.IsPrimitive();
//   nkentseu::nk_bool floatIsPrimitive = tf.IsPrimitive();
//   (void)intIsPrimitive;
//   (void)floatIsPrimitive;
//
//   nkentseu::reflection::NkTypeRegistry::Get().RegisterType(&ti);
//   const nkentseu::reflection::NkType* found =
//       nkentseu::reflection::NkTypeRegistry::Get().FindType(ti.GetName());
//   (void)found;
//
// =============================================================================
