// -----------------------------------------------------------------------------
// FICHIER: NKReflection/NkType.cpp
// DESCRIPTION: Implémentation du registre de types runtime.
// -----------------------------------------------------------------------------

#include "NKReflection/NkType.h"

#include <cstring>

namespace nkentseu {
namespace reflection {

NkTypeRegistry& NkTypeRegistry::Get() {
    static NkTypeRegistry registry;
    return registry;
}

void NkTypeRegistry::RegisterType(const NkType* type) {
    if (type == nullptr || type->GetName() == nullptr) {
        return;
    }

    for (nk_usize i = 0; i < mTypeCount; ++i) {
        if (mTypes[i] == type) {
            return;
        }
        if (mTypes[i] != nullptr &&
            mTypes[i]->GetName() != nullptr &&
            ::strcmp(mTypes[i]->GetName(), type->GetName()) == 0) {
            return;
        }
    }

    if (mTypeCount >= mTypeCapacity) {
        return;
    }

    mTypes[mTypeCount++] = type;
}

const NkType* NkTypeRegistry::FindType(const nk_char* name) const {
    if (name == nullptr || name[0] == '\0') {
        return nullptr;
    }

    for (nk_usize i = 0; i < mTypeCount; ++i) {
        const NkType* type = mTypes[i];
        if (type == nullptr || type->GetName() == nullptr) {
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

