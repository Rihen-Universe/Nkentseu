// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Reflection\NkProperty.h
// DESCRIPTION: Système de réflexion - Property metadata
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKPROPERTY_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_REFLECTION_NKPROPERTY_H_INCLUDED

#include "NKReflection/NkType.h"
#include "NKCore/Assert/NkAssert.h"

namespace nkentseu {
    
        namespace reflection {
            
            /**
             * @brief Flags de propriété
             */
            enum class NkPropertyFlags : uint32 {
                NK_NONE       = 0,
                NK_READ_ONLY   = 1 << 0,
                NK_WRITE_ONLY  = 1 << 1,
                NK_STATIC     = 1 << 2,
                NK_PCONST      = 1 << 3,
                NK_TRANSIENT  = 1 << 4,  // Ne pas sérialiser
                NK_PDEPRECATED = 1 << 5
            };
            
            /**
             * @brief Métadonnées d'une propriété (membre de classe)
             */
            class NKENTSEU_CORE_API NkProperty {
            public:
                using GetterFn = void*(*)(void*);
                using SetterFn = void(*)(void*, void*);
                
                /**
                 * @brief Constructeur
                 */
                NkProperty(const char* name, const NkType& type, usize offset, uint32 flags = 0)
                    : mName(name)
                    , mType(&type)
                    , mOffset(offset)
                    , mFlags(flags)
                    , mGetter(nullptr)
                    , mSetter(nullptr)
                {}
                
                // ========================================
                // PROPERTIES
                // ========================================
                
                const char* GetName() const { return mName; }
                const NkType& GetType() const { return *mType; }
                usize GetOffset() const { return mOffset; }
                uint32 GetFlags() const { return mFlags; }
                
                bool IsReadOnly() const {
                    return (mFlags & static_cast<uint32>(NkPropertyFlags::NK_READ_ONLY)) != 0;
                }
                
                bool IsStatic() const {
                    return (mFlags & static_cast<uint32>(NkPropertyFlags::NK_STATIC)) != 0;
                }
                
                bool IsConst() const {
                    return (mFlags & static_cast<uint32>(NkPropertyFlags::NK_PCONST)) != 0;
                }
                
                // ========================================
                // ACCESSORS
                // ========================================
                
                void SetGetter(GetterFn getter) { mGetter = getter; }
                void SetSetter(SetterFn setter) { mSetter = setter; }
                
                /**
                 * @brief Récupère la valeur (via offset ou getter)
                 */
                template<typename T>
                const T& GetValue(void* instance) const {
                    if (mGetter) {
                        return *static_cast<T*>(mGetter(instance));
                    }
                    return *reinterpret_cast<T*>(static_cast<char*>(instance) + mOffset);
                }
                
                /**
                 * @brief Définit la valeur (via offset ou setter)
                 */
                template<typename T>
                void SetValue(void* instance, const T& value) const {
                    NKENTSEU_ASSERT(!IsReadOnly());
                    
                    if (mSetter) {
                        mSetter(instance, const_cast<T*>(&value));
                    } else {
                        *reinterpret_cast<T*>(static_cast<char*>(instance) + mOffset) = value;
                    }
                }
                
                /**
                 * @brief Récupère pointeur vers valeur
                 */
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

#endif
