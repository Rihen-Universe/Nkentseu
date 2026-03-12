// =============================================================================
// NkMetalContext.mm — Objective-C++ — compiler avec -x objective-c++
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "NkMetalContext.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if defined(NKENTSEU_PLATFORM_MACOS)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif
#include <vector>

#define NK_MTL_LOG(...) printf("[NkMetal] " __VA_ARGS__)
#define NK_MTL_ERR(...) printf("[NkMetal][ERROR] " __VA_ARGS__)

namespace nkentseu {

NkMetalContext::~NkMetalContext() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkMetalContext::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) return false;
    mDesc = desc;
    const NkMetalDesc& m     = desc.metal;
    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    if (!surf.IsValid()) { NK_MTL_ERR("Invalid surface\n"); return false; }

    mData.width       = surf.width;
    mData.height      = surf.height;
    mData.sampleCount = m.sampleCount;
    mVSync            = m.vsync;

    // Créer le MTLDevice
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) { NK_MTL_ERR("MTLCreateSystemDefaultDevice failed\n"); return false; }
    mData.device = (void*)CFBridgingRetain(device);

    // Command queue
    id<MTLCommandQueue> queue = [device newCommandQueue];
    if (!queue) { NK_MTL_ERR("newCommandQueue failed\n"); return false; }
    mData.commandQueue = (void*)CFBridgingRetain(queue);

    // CAMetalLayer
    CAMetalLayer* layer = nil;
    if (surf.metalLayer) {
        layer = (__bridge CAMetalLayer*)surf.metalLayer;
    } else {
#if defined(NKENTSEU_PLATFORM_MACOS)
        NSView* view = (__bridge NSView*)surf.nsView;
        if (!view) { NK_MTL_ERR("nsView is null\n"); return false; }
        layer = [CAMetalLayer layer];
        layer.device = device;
        view.wantsLayer = YES;
        view.layer = layer;
#else
        UIView* view = (__bridge UIView*)surf.uiView;
        if (!view) { NK_MTL_ERR("uiView is null\n"); return false; }
        layer = (CAMetalLayer*)view.layer;
        layer.device = device;
#endif
    }

    layer.pixelFormat        = m.srgb ? MTLPixelFormatBGRA8Unorm_sRGB
                                      : MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly    = YES;
    layer.drawableSize       = CGSizeMake((CGFloat)surf.width, (CGFloat)surf.height);
    layer.displaySyncEnabled = m.vsync;

    mData.layer = (void*)CFBridgingRetain(layer);

    // Depth texture
    if (!CreateDepthTexture(surf.width, surf.height)) return false;

    // Library par défaut (shaders .metal compilés dans le bundle)
    NSError* err = nil;
    id<MTLLibrary> lib = [device newDefaultLibrary];
    if (lib) {
        mData.library = (void*)CFBridgingRetain(lib);
    } else {
        NK_MTL_LOG("No default MTLLibrary (.metal shaders not compiled)\n");
    }

    // Infos
    mData.renderer = [device.name UTF8String];
    mData.vramMB   = (uint32)(device.recommendedMaxWorkingSetSize / (1024*1024));

    mIsValid = true;
    NK_MTL_LOG("Ready — %s | VRAM %u MB | vsync=%s\n",
               mData.renderer, mData.vramMB, m.vsync ? "on" : "off");
    return true;
}

// =============================================================================
bool NkMetalContext::CreateDepthTexture(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return true;
    id<MTLDevice> device = (__bridge id<MTLDevice>)mData.device;

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                           width:w height:h mipmapped:NO];
    desc.storageMode = MTLStorageModePrivate;
    desc.usage       = MTLTextureUsageRenderTarget;

    id<MTLTexture> depth = [device newTextureWithDescriptor:desc];
    if (!depth) { NK_MTL_ERR("Depth texture creation failed (%ux%u)\n", w, h); return false; }

    if (mData.depthTexture) {
        CFBridgingRelease(mData.depthTexture);
        mData.depthTexture = nullptr;
    }
    mData.depthTexture = (void*)CFBridgingRetain(depth);
    return true;
}

// =============================================================================
void NkMetalContext::Shutdown() {
    if (!mIsValid) return;

    // Flush GPU : soumettre un command buffer vide et attendre
    id<MTLCommandQueue> q    = (__bridge id<MTLCommandQueue>)mData.commandQueue;
    id<MTLCommandBuffer> flush = [q commandBuffer];
    [flush commit];
    [flush waitUntilCompleted];

    if (mData.currentEncoder) { CFBridgingRelease(mData.currentEncoder); mData.currentEncoder = nullptr; }
    if (mData.commandBuffer)  { CFBridgingRelease(mData.commandBuffer);  mData.commandBuffer  = nullptr; }
    if (mData.currentDrawable){ CFBridgingRelease(mData.currentDrawable);mData.currentDrawable= nullptr; }
    if (mData.library)        { CFBridgingRelease(mData.library);        mData.library        = nullptr; }
    if (mData.depthTexture)   { CFBridgingRelease(mData.depthTexture);   mData.depthTexture   = nullptr; }
    if (mData.layer)          { CFBridgingRelease(mData.layer);          mData.layer          = nullptr; }
    if (mData.commandQueue)   { CFBridgingRelease(mData.commandQueue);   mData.commandQueue   = nullptr; }
    if (mData.device)         { CFBridgingRelease(mData.device);         mData.device         = nullptr; }

    mIsValid = false;
    NK_MTL_LOG("Shutdown OK\n");
}

// =============================================================================
bool NkMetalContext::BeginFrame() {
    if (!mIsValid) return false;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)mData.layer;

    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) {
        NK_MTL_ERR("nextDrawable returned nil (likely minimized)\n");
        return false;
    }
    mData.currentDrawable = (void*)CFBridgingRetain(drawable);

    id<MTLCommandQueue>  queue = (__bridge id<MTLCommandQueue>)mData.commandQueue;
    id<MTLCommandBuffer> cmdb  = [queue commandBuffer];
    mData.commandBuffer = (void*)CFBridgingRetain(cmdb);

    // Render pass descriptor
    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture     = drawable.texture;
    rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpd.colorAttachments[0].clearColor  = MTLClearColorMake(0.1, 0.1, 0.1, 1.0);

    if (mData.depthTexture) {
        id<MTLTexture> depth = (__bridge id<MTLTexture>)mData.depthTexture;
        rpd.depthAttachment.texture     = depth;
        rpd.depthAttachment.loadAction  = MTLLoadActionClear;
        rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
        rpd.depthAttachment.clearDepth  = 1.0;
    }

    id<MTLRenderCommandEncoder> enc = [cmdb renderCommandEncoderWithDescriptor:rpd];
    if (!enc) { NK_MTL_ERR("renderCommandEncoderWithDescriptor failed\n"); return false; }
    mData.currentEncoder = (void*)CFBridgingRetain(enc);
    return true;
}

// =============================================================================
void NkMetalContext::EndFrame() {
    if (!mIsValid || !mData.currentEncoder) return;
    id<MTLRenderCommandEncoder> enc =
        (__bridge_transfer id<MTLRenderCommandEncoder>)mData.currentEncoder;
    [enc endEncoding];
    mData.currentEncoder = nullptr;
}

void NkMetalContext::Present() {
    if (!mIsValid) return;
    id<MTLCommandBuffer> cmdb =
        (__bridge_transfer id<MTLCommandBuffer>)mData.commandBuffer;
    id<CAMetalDrawable> drw =
        (__bridge_transfer id<CAMetalDrawable>)mData.currentDrawable;
    [cmdb presentDrawable:drw];
    [cmdb commit];
    mData.commandBuffer   = nullptr;
    mData.currentDrawable = nullptr;
}

// =============================================================================
bool NkMetalContext::OnResize(uint32 w, uint32 h) {
    if (!mIsValid) return false;
    if (w == 0 || h == 0) return true; // Minimisée — skip
    mData.width  = w;
    mData.height = h;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)mData.layer;
    layer.drawableSize = CGSizeMake((CGFloat)w, (CGFloat)h);
    return CreateDepthTexture(w, h);
}

void NkMetalContext::SetVSync(bool e) {
    mVSync = e;
    CAMetalLayer* layer = (__bridge CAMetalLayer*)mData.layer;
    layer.displaySyncEnabled = e;
}

bool          NkMetalContext::GetVSync()  const { return mVSync; }
bool          NkMetalContext::IsValid()   const { return mIsValid; }
NkGraphicsApi NkMetalContext::GetApi()    const { return NkGraphicsApi::NK_API_METAL; }
NkContextDesc NkMetalContext::GetDesc()   const { return mDesc; }
void*         NkMetalContext::GetNativeContextData() { return &mData; }
bool          NkMetalContext::SupportsCompute() const { return true; }

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

#endif // NKENTSEU_PLATFORM_MACOS || IOS
