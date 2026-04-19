// =============================================================================
// FICHIER  : Sandbox/System/NKReflection/src/main.cpp
// PROJET   : SandboxNKReflection
// OBJET    : Validation compile + execution du module NKReflection
// =============================================================================

#include "NKReflection/NkType.h"
#include "NKReflection/NkRegistry.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::reflection;

namespace {

class DemoObject {
    NKENTSEU_REFLECT_CLASS(DemoObject)

    public:
        nk_int32 value;
        NKENTSEU_REFLECT_PROPERTY(value)
};

NKENTSEU_REGISTER_CLASS(DemoObject)

} // namespace

int main() {
    const NkType& tInt = NkTypeOf<nk_int32>();
    const NkType& tFloat = NkTypeOf<nk_float32>();

    NkRegistry::Get().RegisterType(&tInt);
    NkRegistry::Get().RegisterType(&tFloat);

    const NkType* intFound = NkRegistry::Get().FindType(tInt.GetName());
    const NkClass* cls = NkRegistry::Get().FindClass("DemoObject");

    std::printf(
        "[SandboxNKReflection] intSize=%zu floatSize=%zu intFound=%d classFound=%d\n",
        static_cast<size_t>(tInt.GetSize()),
        static_cast<size_t>(tFloat.GetSize()),
        intFound ? 1 : 0,
        cls ? 1 : 0
    );

    return (intFound && cls) ? 0 : 1;
}
