#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKReflection/NkType.h"
#include "NKReflection/NkRegistry.h"

using namespace nkentseu::reflection;

TEST_CASE(NKReflectionSmoke, TypeCategoryBasics) {
    const NkType& ti = NkTypeOf<nkentseu::nk_int32>();
    const NkType& tf = NkTypeOf<nkentseu::nk_float32>();

    ASSERT_EQUAL(NkTypeCategory::NK_INT32, ti.GetCategory());
    ASSERT_EQUAL(NkTypeCategory::NK_FLOAT32, tf.GetCategory());
    ASSERT_TRUE(ti.GetSize() == sizeof(nkentseu::nk_int32));
}

TEST_CASE(NKReflectionSmoke, RegistryTypeAccessor) {
    const NkType* t = NkRegistry::Get().GetType<nkentseu::nk_uint64>();
    ASSERT_NOT_NULL(t);
    ASSERT_TRUE(t->GetSize() == sizeof(nkentseu::nk_uint64));
}
