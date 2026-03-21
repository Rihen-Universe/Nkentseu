// =============================================================================
// NkOpenGLContext_macOS.mm â€” ImplÃ©mentation macOS (Objective-C++)
// Compiler avec : -x objective-c++
// CMake : set_source_files_properties(NkOpenGLContext_macOS.mm
//             PROPERTIES COMPILE_FLAGS "-x objective-c++")
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#include "NkOpenGLContext.h"
#include "NKRenderer/Context/Core/NkSurfaceDesc.h"
#include "NKContainers/NkContainers.h"
#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>

#define NK_GL_LOG(...) logger.Infof("[NkOpenGL/NSGL] " __VA_ARGS__)
#define NK_GL_ERR(...) logger.Errorf("[NkOpenGL/NSGL] " __VA_ARGS__)

namespace nkentseu {

bool NkOpenGLContext::InitNSGL(const NkSurfaceDesc& surf, const NkOpenGLDesc& gl) {
    NSView* view = (__bridge NSView*)surf.view;
    if (!view) { NK_GL_ERR("NSView is null\n"); return false; }

    // Construire les attributs pixel format depuis NkOpenGLDesc
    NkVector<NSOpenGLPixelFormatAttribute> attribs;

    // Profil Core
    if (gl.profile == NkGLProfile::Core) {
        attribs.PushBack(NSOpenGLPFAOpenGLProfile);
        if (gl.majorVersion >= 4)
            attribs.PushBack(NSOpenGLProfileVersion4_1Core);
        else
            attribs.PushBack(NSOpenGLProfileVersion3_2Core);
    }

    attribs.PushBack(NSOpenGLPFAAccelerated);
    attribs.PushBack(NSOpenGLPFADoubleBuffer); // doubleBuffer toujours sur macOS

    if (gl.colorBits >= 32) {
        attribs.PushBack(NSOpenGLPFAColorSize); attribs.PushBack(32);
    } else {
        attribs.PushBack(NSOpenGLPFAColorSize); attribs.PushBack(24);
    }
    if (gl.alphaBits > 0) {
        attribs.PushBack(NSOpenGLPFAAlphaSize); attribs.PushBack(gl.alphaBits);
    }
    if (gl.depthBits > 0) {
        attribs.PushBack(NSOpenGLPFADepthSize); attribs.PushBack(gl.depthBits);
    }
    if (gl.stencilBits > 0) {
        attribs.PushBack(NSOpenGLPFAStencilSize); attribs.PushBack(gl.stencilBits);
    }
    if (gl.msaaSamples > 1) {
        attribs.PushBack(NSOpenGLPFAMultisample);
        attribs.PushBack(NSOpenGLPFASampleBuffers); attribs.PushBack(1);
        attribs.PushBack(NSOpenGLPFASamples); attribs.PushBack(gl.msaaSamples);
    }
    attribs.PushBack(0);

    NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc]
        initWithAttributes:attribs.Data()];
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

