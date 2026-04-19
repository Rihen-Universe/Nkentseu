// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkType.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation du registre runtime des types.
// =============================================================================

#include "NKReflection/NkType.h"

// -- En-tetes standard minimaux ----------------------------------------------
#include <cstring>

namespace nkentseu {
    namespace reflection {

        NkTypeRegistry& NkTypeRegistry::Get() {
            static NkTypeRegistry registry;
            return registry;
        }

        void NkTypeRegistry::RegisterType(const NkType* type) {
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

            if (mTypeCount >= mTypeCapacity) {
                return;
            }

            mTypes[mTypeCount++] = type;
        }

        const NkType* NkTypeRegistry::FindType(const nk_char* name) const {
            if (!name || name[0] == '\0') {
                return nullptr;
            }

            for (nk_usize i = 0; i < mTypeCount; ++i) {
                const NkType* type = mTypes[i];
                if (!type || !type->GetName()) {
                    continue;
                }

                if (::strcmp(type->GetName(), name) == 0) {
                    return type;
                }
            }

            return nullptr;
        }

    } // namespace reflection
} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
