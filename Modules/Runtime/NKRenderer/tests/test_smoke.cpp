#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKRenderer/Deprecate/NkRenderer.h"

#if defined(True)
#undef True
#endif
#if defined(False)
#undef False
#endif

using namespace nkentseu;

TEST_CASE(NKRendererSmoke, PackUnpackColor) {
    const uint32 color = NkRenderer::PackColor(10, 20, 30, 40);

    uint8 r = 0, g = 0, b = 0, a = 0;
    NkRenderer::UnpackColor(color, r, g, b, a);

    ASSERT_EQUAL(10, static_cast<int>(r));
    ASSERT_EQUAL(20, static_cast<int>(g));
    ASSERT_EQUAL(30, static_cast<int>(b));
    ASSERT_EQUAL(40, static_cast<int>(a));
}

TEST_CASE(NKRendererSmoke, DefaultRendererState) {
    NkRenderer renderer;
    ASSERT_FALSE(renderer.IsEnabled());
    ASSERT_TRUE(renderer.GetApi() == NkRendererApi::NK_SOFTWARE);
}
