#pragma once
// =============================================================================
// NkMetalContext.h + NkMetalContext.mm
// Metal (macOS/iOS) — production ready
// Compiler NkMetalContext.mm avec -x objective-c++
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "NKRenderer/Context/Core/NkIGraphicsContext.h"

namespace nkentseu {

    struct NkMetalContextData {
        void* device          = nullptr;  // id<MTLDevice>
        void* commandQueue    = nullptr;  // id<MTLCommandQueue>
        void* layer           = nullptr;  // CAMetalLayer*
        void* currentDrawable = nullptr;  // id<CAMetalDrawable>
        void* commandBuffer   = nullptr;  // id<MTLCommandBuffer>
        void* currentEncoder  = nullptr;  // id<MTLRenderCommandEncoder>
        void* depthTexture    = nullptr;  // id<MTLTexture>
        void* library         = nullptr;  // id<MTLLibrary> (shaders compilés)

        uint32      width     = 0;
        uint32      height    = 0;
        uint32      sampleCount = 1;
        const char* renderer  = "Metal";
        const char* vendor    = "Apple";
        const char* version   = "Metal";
        uint32      vramMB    = 0;
    };

    class NkMetalContext final : public NkIGraphicsContext {
    public:
        NkMetalContext()  = default;
        ~NkMetalContext() override;

        bool          Initialize(const NkWindow& w, const NkContextDesc& d) override;
        void          Shutdown()                                             override;
        bool          IsValid()   const                                      override;
        bool          BeginFrame();
        void          EndFrame();
        void          Present();
        bool          OnResize(uint32 w, uint32 h)                           override;
        void          SetVSync(bool enabled)                                 override;
        bool          GetVSync() const                                       override;
        NkGraphicsApi GetApi()   const                                       override;
        NkContextInfo GetInfo()  const                                       override;
        NkContextDesc GetDesc()  const                                       override;
        void*         GetNativeContextData()                                 override;
        bool          SupportsCompute() const                                override;

    private:
        bool CreateDepthTexture(uint32 w, uint32 h);

        NkMetalContextData mData;
        NkContextDesc      mDesc;
        bool               mIsValid = false;
        bool               mVSync   = true;
    };

} // namespace nkentseu

#endif // MACOS || IOS

// =============================================================================
// NkMetalContext.mm — Implémentation Objective-C++
// =============================================================================
/*
INSTRUCTIONS DE COMPILATION :
  CMake :
    set_source_files_properties(NkMetalContext.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(NkEngine PRIVATE
        "-framework Metal" "-framework QuartzCore" "-framework AppKit")

  Xcode : ajouter NkMetalContext.mm, Metal.framework est lié automatiquement.
*/

/*
CONTENU DE NkMetalContext.mm :

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "NkMetalContext.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if defined(NKENTSEU_PLATFORM_MACOS)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#define NK_MTL_LOG(...) logger.Infof("[NkMetal] " __VA_ARGS__)
#define NK_MTL_ERR(...) logger.Errorf("[NkMetal][ERROR] " __VA_ARGS__)

namespace nkentseu {

NkMetalContext::~NkMetalContext() { if (mIsValid) Shutdown(); }

bool NkMetalContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) return false;
    mDesc = desc;
    const NkMetalDesc& m = desc.metal;
    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    if (!surf.IsValid()) { NK_MTL_ERR("Invalid surface\n"); return false; }

    mData.width  = surf.width;
    mData.height = surf.height;
    mData.sampleCount = m.sampleCount;
    mVSync = m.vsync;

    // Device
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) { NK_MTL_ERR("MTLCreateSystemDefaultDevice failed\n"); return false; }
    mData.device = (void*)CFBridgingRetain(device);

    // Command queue
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) { NK_MTL_ERR("newCommandQueue failed\n"); return false; }
    mData.commandQueue = (void*)CFBridgingRetain(queue);

    // CAMetalLayer — récupérer ou créer depuis NSView/UIView
    CAMetalLayer* layer = nil;
    if (surf.metalLayer) {
        layer = (__bridge CAMetalLayer*)surf.metalLayer;
    } else {
#if defined(NKENTSEU_PLATFORM_MACOS)
        NSView* view = (__bridge NSView*)surf.nsView;
        layer = [CAMetalLayer layer];
        layer.device = device;
        view.wantsLayer = YES;
        view.layer = layer;
#else
        UIView* view = (__bridge UIView*)surf.uiView;
        layer = (CAMetalLayer*)view.layer;
        layer.device = device;
#endif
    }
    layer.pixelFormat        = m.srgb ? MTLPixelFormatBGRA8Unorm_sRGB
                                      : MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly    = YES;
    layer.drawableSize       = CGSizeMake(surf.width, surf.height);
    layer.displaySyncEnabled = m.vsync;
    mData.layer = (void*)CFBridgingRetain(layer);

    // Depth texture
    if (!CreateDepthTexture(surf.width, surf.height)) return false;

    // Library par défaut (shaders compilés dans le bundle)
    NSError* err = nil;
    id<MTLLibrary> lib = [device newDefaultLibrary];
    if (!lib) {
        NK_MTL_LOG("Warning: no default library (no .metal shaders compiled)\n");
    } else {
        mData.library = (void*)CFBridgingRetain(lib);
    }

    // Infos renderer
    mData.renderer = [device.name UTF8String];
    mData.vramMB   = (uint32)(device.recommendedMaxWorkingSetSize / (1024*1024));

    mIsValid = true;
    NK_MTL_LOG("Ready — %s | VRAM %u MB\n", mData.renderer, mData.vramMB);
    return true;
}

bool NkMetalContext::CreateDepthTexture(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return true;
    id<MTLDevice> device = (__bridge id<MTLDevice>)mData.device;

    MTLTextureDescriptor* desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat: MTLPixelFormatDepth32Float
        width:w height:h mipmapped:NO];
    desc.storageMode  = MTLStorageModePrivate;
    desc.usage        = MTLTextureUsageRenderTarget;

    id<MTLTexture> depth = [device newTextureWithDescriptor:desc];
    if (!depth) { NK_MTL_ERR("CreateDepthTexture failed\n"); return false; }

    if (mData.depthTexture) CFBridgingRelease(mData.depthTexture);
    mData.depthTexture = (void*)CFBridgingRetain(depth);
    return true;
}

void NkMetalContext::Shutdown() {
    if (!mIsValid) return;
    // Attendre que le GPU termine
    id<MTLCommandQueue> q = (__bridge id<MTLCommandQueue>)mData.commandQueue;
    id<MTLCommandBuffer> flush = [q commandBuffer];
    [flush commit]; [flush waitUntilCompleted];

    if (mData.library)      CFBridgingRelease(mData.library);
    if (mData.depthTexture) CFBridgingRelease(mData.depthTexture);
    if (mData.layer)        CFBridgingRelease(mData.layer);
    if (mData.commandQueue) CFBridgingRelease(mData.commandQueue);
    if (mData.device)       CFBridgingRelease(mData.device);
    mData = NkMetalContextData{};
    mIsValid = false;
    NK_MTL_LOG("Shutdown OK\n");
}

bool NkMetalContext::BeginFrame() {
    if (!mIsValid) return false;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)mData.layer;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) { NK_MTL_ERR("nextDrawable failed\n"); return false; }
    mData.currentDrawable = (void*)CFBridgingRetain(drawable);

    id<MTLCommandQueue>  queue = (__bridge id<MTLCommandQueue>)mData.commandQueue;
    id<MTLCommandBuffer> cmdb  = [queue commandBuffer];
    mData.commandBuffer = (void*)CFBridgingRetain(cmdb);

    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture     = drawable.texture;
    rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpd.colorAttachments[0].clearColor  = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);

    if (mData.depthTexture) {
        rpd.depthAttachment.texture     = (__bridge id<MTLTexture>)mData.depthTexture;
        rpd.depthAttachment.loadAction  = MTLLoadActionClear;
        rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
        rpd.depthAttachment.clearDepth  = 1.0;
    }

    id<MTLRenderCommandEncoder> enc = [cmdb renderCommandEncoderWithDescriptor:rpd];
    mData.currentEncoder = (void*)CFBridgingRetain(enc);
    return true;
}

void NkMetalContext::EndFrame() {
    if (!mIsValid) return;
    if (mData.currentEncoder) {
        id<MTLRenderCommandEncoder> enc = (__bridge_transfer id<MTLRenderCommandEncoder>)mData.currentEncoder;
        [enc endEncoding];
        mData.currentEncoder = nullptr;
    }
}

void NkMetalContext::Present() {
    if (!mIsValid) return;
    id<MTLCommandBuffer> cmdb = (__bridge_transfer id<MTLCommandBuffer>)mData.commandBuffer;
    id<CAMetalDrawable>  drw  = (__bridge_transfer id<CAMetalDrawable>)mData.currentDrawable;
    [cmdb presentDrawable:drw];
    [cmdb commit];
    mData.commandBuffer   = nullptr;
    mData.currentDrawable = nullptr;
}

bool NkMetalContext::OnResize(uint32 w, uint32 h) {
    if (!mIsValid) return false;
    if (w == 0 || h == 0) return true;
    mData.width  = w;
    mData.height = h;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)mData.layer;
    layer.drawableSize = CGSizeMake(w, h);
    return CreateDepthTexture(w, h);
}

void NkMetalContext::SetVSync(bool e) {
    mVSync = e;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)mData.layer;
    layer.displaySyncEnabled = e;
}
bool NkMetalContext::GetVSync()  const { return mVSync; }
bool NkMetalContext::IsValid()   const { return mIsValid; }
NkGraphicsApi NkMetalContext::GetApi()  const { return NkGraphicsApi::NK_API_METAL; }
NkContextDesc NkMetalContext::GetDesc() const { return mDesc; }
void* NkMetalContext::GetNativeContextData() { return &mData; }
bool NkMetalContext::SupportsCompute() const { return true; } // Metal Compute toujours dispo

NkContextInfo NkMetalContext::GetInfo() const {
    NkContextInfo i;
    i.api              = NkGraphicsApi::NK_API_METAL;
    i.renderer         = mData.renderer;
    i.vendor           = "Apple";
    i.version          = "Metal";
    i.vramMB           = mData.vramMB;
    i.computeSupported = true;
    return i;
}

} // namespace nkentseu

#endif // MACOS || IOS
*/
