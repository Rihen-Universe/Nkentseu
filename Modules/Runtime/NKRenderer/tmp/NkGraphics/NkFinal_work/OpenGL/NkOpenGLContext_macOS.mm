// =============================================================================
// NkOpenGLContext_macOS.mm — Implémentation macOS (Objective-C++)
// Compiler avec : -x objective-c++
// CMake : set_source_files_properties(NkOpenGLContext_macOS.mm
//             PROPERTIES COMPILE_FLAGS "-x objective-c++")
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS)

#include "NkOpenGLContext.h"
#include "../Core/NkSurfaceDesc.h"
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>

#define NK_GL_LOG(...) printf("[NkOpenGL/NSGL] " __VA_ARGS__)
#define NK_GL_ERR(...) printf("[NkOpenGL/NSGL][ERROR] " __VA_ARGS__)

namespace nkentseu {

bool NkOpenGLContext::InitNSGL(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
    NSView* view = (__bridge NSView*)surf.nsView;
    if (!view) { NK_GL_ERR("NSView is null\n"); return false; }

    // Construire les attributs pixel format depuis NkOpenGLDesc
    std::vector<NSOpenGLPixelFormatAttribute> attribs;

    // Profil Core
    if (gl.profile == NkGLProfile::Core) {
        attribs.push_back(NSOpenGLPFAOpenGLProfile);
        if (gl.majorVersion >= 4)
            attribs.push_back(NSOpenGLProfileVersion4_1Core);
        else
            attribs.push_back(NSOpenGLProfileVersion3_2Core);
    }

    attribs.push_back(NSOpenGLPFAAccelerated);
    attribs.push_back(NSOpenGLPFADoubleBuffer); // doubleBuffer toujours sur macOS

    if (gl.colorBits >= 32) {
        attribs.push_back(NSOpenGLPFAColorSize); attribs.push_back(32);
    } else {
        attribs.push_back(NSOpenGLPFAColorSize); attribs.push_back(24);
    }
    if (gl.alphaBits > 0) {
        attribs.push_back(NSOpenGLPFAAlphaSize); attribs.push_back(gl.alphaBits);
    }
    if (gl.depthBits > 0) {
        attribs.push_back(NSOpenGLPFADepthSize); attribs.push_back(gl.depthBits);
    }
    if (gl.stencilBits > 0) {
        attribs.push_back(NSOpenGLPFAStencilSize); attribs.push_back(gl.stencilBits);
    }
    if (gl.msaaSamples > 1) {
        attribs.push_back(NSOpenGLPFAMultisample);
        attribs.push_back(NSOpenGLPFASampleBuffers); attribs.push_back(1);
        attribs.push_back(NSOpenGLPFASamples); attribs.push_back(gl.msaaSamples);
    }
    attribs.push_back(0);

    NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc]
        initWithAttributes:attribs.data()];
    if (!pf) { NK_GL_ERR("NSOpenGLPixelFormat creation failed\n"); return false; }

    // Partage de ressources avec contexte parent
    NSOpenGLContext* shareNSCtx = nil;
    if (mSharedParent && mSharedParent->mData.context)
        shareNSCtx = (__bridge NSOpenGLContext*)mSharedParent->mData.context;

    NSOpenGLContext* ctx = [[NSOpenGLContext alloc]
        initWithFormat:pf shareContext:shareNSCtx];
    if (!ctx) { NK_GL_ERR("NSOpenGLContext creation failed\n"); return false; }

    [ctx setView:view];
    [ctx makeCurrentContext];

    // VSync via CGLSetParameter
    GLint swapInterval = (gl.swapInterval == NkGLSwapInterval::Immediate) ? 0 : 1;
    [ctx setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];

    // Stocker dans mData
    mData.context     = (void*)CFBridgingRetain(ctx);
    mData.pixelFormat = (void*)CFBridgingRetain(pf);
    mData.view        = (void*)CFBridgingRetain(view);

    NK_GL_LOG("NSGL OK (OpenGL %d.%d)\n", gl.majorVersion, gl.minorVersion);
    return true;
}

void NkOpenGLContext::ShutdownNSGL() {
    if (mData.context) {
        NSOpenGLContext* ctx = (__bridge_transfer NSOpenGLContext*)mData.context;
        [NSOpenGLContext clearCurrentContext];
        [ctx clearDrawable];
        mData.context = nullptr;
    }
    if (mData.pixelFormat) {
        CFBridgingRelease(mData.pixelFormat); mData.pixelFormat = nullptr;
    }
    if (mData.view) {
        CFBridgingRelease(mData.view); mData.view = nullptr;
    }
}

void NkOpenGLContext::SwapNSGL() {
    if (mData.context) {
        NSOpenGLContext* ctx = (__bridge NSOpenGLContext*)mData.context;
        [ctx flushBuffer];
    }
}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
