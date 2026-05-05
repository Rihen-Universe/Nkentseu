#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCamera/NkCamera2D.h"
#include "NKCamera/NkCameraTypes.h"

using namespace nkentseu;

TEST_CASE(NKCameraSmoke, Camera2DBasics) {
    NkCamera2D cam;
    cam.SetPosition(10.0f, 20.0f);
    cam.SetRotation(33.0f);

    ASSERT_NEAR(10.0f, cam.GetX(), 0.0001f);
    ASSERT_NEAR(20.0f, cam.GetY(), 0.0001f);
    ASSERT_NEAR(33.0f, cam.GetRotation(), 0.0001f);
}

TEST_CASE(NKCameraSmoke, ConfigResolveAndFrameStride) {
    NkCameraConfig cfg;
    cfg.preset = NkCameraResolution::NK_CAM_RES_VGA;
    cfg.Resolve();

    ASSERT_EQUAL(640, static_cast<int>(cfg.width));
    ASSERT_EQUAL(480, static_cast<int>(cfg.height));

    ASSERT_EQUAL(2560, static_cast<int>(NkCameraFrame::DefaultStride(640, NkPixelFormat::NK_PIXEL_RGBA8)));
}
