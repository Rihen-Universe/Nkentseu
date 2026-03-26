// ============================================================================
// Sandbox/src/main10.cpp
// OpenGL-only example with explicit NkContext hints + screen colorization.
// ============================================================================

#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKLogger/NkLog.h"

namespace {
    using namespace nkentseu;

    using NkGLClearColorFn = void (*)(float, float, float, float);
    using NkGLClearFn = void (*)(unsigned int);
    static constexpr unsigned int NK_GL_COLOR_BUFFER_BIT = 0x00004000u;

    static uint8 ChannelFromFrame(uint32 frame, uint32 speed, uint32 phase) {
        return static_cast<uint8>(((frame * speed) + phase) & 0xFFu);
    }
} // namespace

NKENTSEU_DEFINE_APP_DATA(([]() {
    nkentseu::NkAppData d{};
    d.appName = "SandboxOpenGLOnly";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    return d;
})());

int nkmain(const nkentseu::NkEntryState&) {
    using namespace nkentseu;

    NkContextInit();
    NkContextResetHints();
    NkContextSetApi(NkRendererApi::NK_OPENGL);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_VERSION_MAJOR, 4);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_VERSION_MINOR, 5);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_PROFILE,
                        static_cast<int32>(NkContextProfile::NK_CONTEXT_PROFILE_CORE));
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_DEBUG, 0);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_DOUBLEBUFFER, 1);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_MSAA_SAMPLES, 4);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_VSYNC, 1);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_STEREO, 0);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_RED_BITS, 8);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_GREEN_BITS, 8);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_BLUE_BITS, 8);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_ALPHA_BITS, 8);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_DEPTH_BITS, 24);
    NkContextWindowHint(NkContextHint::NK_CONTEXT_HINT_STENCIL_BITS, 8);

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    NkWin32PixelFormatConfig pfd{};
    pfd.useCustomDescriptor = true;
    pfd.drawToWindow = true;
    pfd.supportOpenGL = true;
    pfd.doubleBuffer = true;
    pfd.pixelType = 0; // RGBA
    pfd.colorBits = 32;
    pfd.redBits = 8;
    pfd.greenBits = 8;
    pfd.blueBits = 8;
    pfd.alphaBits = 8;
    pfd.depthBits = 24;
    pfd.stencilBits = 8;
    NkContextSetWin32PixelFormat(pfd);
#endif

    NkContextConfig contextConfig = NkContextGetHints();

    NkWindowConfig windowConfig{};
    windowConfig.title = "Sandbox OpenGL Only";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.resizable = true;
    NkContextApplyWindowHints(windowConfig);

    NkWindow window;
    if (!window.Create(windowConfig)) {
        logger.Error("[main10] Window creation failed: {0}", window.GetLastError().ToString());
        NkContextShutdown();
        return -1;
    }

    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        logger.Error("[main10] Context creation failed: {0}", context.lastError.ToString());
        window.Close();
        NkContextShutdown();
        return -2;
    }

    if (!NkContextMakeCurrent(context)) {
        logger.Error("[main10] MakeCurrent failed: {0}", context.lastError.ToString());
        NkContextDestroy(context);
        window.Close();
        NkContextShutdown();
        return -3;
    }

    NkGLClearColorFn glClearColorFn = reinterpret_cast<NkGLClearColorFn>(NkContextGetProcAddress(context, "glClearColor"));
    NkGLClearFn glClearFn = reinterpret_cast<NkGLClearFn>(NkContextGetProcAddress(context, "glClear"));

    if (!glClearColorFn || !glClearFn) {
        logger.Error("[main10] Missing OpenGL symbols (glClearColor/glClear).");
        NkContextDestroy(context);
        window.Close();
        NkContextShutdown();
        return -4;
    }

    logger.Info("[main10] OpenGL context ready - colorizing screen.");

    auto& events = NkEvents();
    bool running = true;
    uint32 frame = 0;
    while (running && window.IsOpen()) {
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
        }

        const uint8 r8 = ChannelFromFrame(frame, 1, 0);
        const uint8 g8 = ChannelFromFrame(frame, 2, 96);
        const uint8 b8 = ChannelFromFrame(frame, 3, 192);

        const float rf = static_cast<float>(r8) / 255.0f;
        const float gf = static_cast<float>(g8) / 255.0f;
        const float bf = static_cast<float>(b8) / 255.0f;

        glClearColorFn(rf, gf, bf, 1.0f);
        glClearFn(NK_GL_COLOR_BUFFER_BIT);
        NkContextSwapBuffers(context);
        ++frame;
    }

    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}

