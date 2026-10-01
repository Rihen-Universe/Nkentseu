// =============================================================================
// NkMetalContext.mm â€” Objective-C++ â€” compiler avec -x objective-c++
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "NkMetalContext.h"
#include "NKWindow/Core/NkWindow.h" // NkWindow complet + NkSurfaceDesc (GetSurfaceDesc)
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if defined(NKENTSEU_PLATFORM_MACOS)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#define NK_MTL_LOG(...) logger.Infof("[NkMetal] " __VA_ARGS__)
#define NK_MTL_ERR(...) logger.Errorf("[NkMetal] " __VA_ARGS__)

namespace nkentseu {

	NkMetalContext::~NkMetalContext() {
		if (mIsValid)
			Shutdown();
	}

	// =============================================================================
	bool NkMetalContext::Initialize(const NkWindow &window, const NkContextDesc &desc) {
		if (mIsValid)
			return false;
		mDesc = desc;
		const NkMetalDesc &m = desc.metal;
		const NkSurfaceDesc surf = window.GetSurfaceDesc();
		if (!surf.IsValid()) {
			NK_MTL_ERR("Invalid surface\n");
			return false;
		}

		mData.width = surf.width;
		mData.height = surf.height;
		mData.sampleCount = m.sampleCount;
		mVSync = m.vsync;

		// CrÃ©er le MTLDevice
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		if (!device) {
			NK_MTL_ERR("MTLCreateSystemDefaultDevice failed\n");
			return false;
		}
		mData.device = (void *)CFBridgingRetain(device);

		// Command queue
		id<MTLCommandQueue> queue = [device newCommandQueue];
		if (!queue) {
			NK_MTL_ERR("newCommandQueue failed\n");
			return false;
		}
		mData.commandQueue = (void *)CFBridgingRetain(queue);

		// CAMetalLayer
		CAMetalLayer *layer = nil;
		// surf.view / surf.metalLayer sont DEJA des pointeurs Objective-C en .mm
		// (NkSurface.h) : un __bridge entre deux types ObjC est refuse sous ARC.
		if (surf.metalLayer) {
			layer = surf.metalLayer;
		} else {
#if defined(NKENTSEU_PLATFORM_MACOS)
			NSView *view = surf.view;
			if (!view) {
				NK_MTL_ERR("nsView is null\n");
				return false;
			}
			layer = [CAMetalLayer layer];
			layer.device = device;
			view.wantsLayer = YES;
			view.layer = layer;
#else
			UIView *view = surf.view;
			if (!view) {
				NK_MTL_ERR("uiView is null\n");
				return false;
			}
			layer = (CAMetalLayer *)view.layer;
			layer.device = device;
#endif
		}

		layer.pixelFormat = m.srgb ? MTLPixelFormatBGRA8Unorm_sRGB : MTLPixelFormatBGRA8Unorm;
		layer.framebufferOnly = YES;
		layer.drawableSize = CGSizeMake((CGFloat)surf.width, (CGFloat)surf.height);
#if defined(NKENTSEU_PLATFORM_MACOS)
		layer.displaySyncEnabled = m.vsync; // propriété CAMetalLayer macOS uniquement
#endif

		mData.layer = (void *)CFBridgingRetain(layer);

		// Depth texture
		if (!CreateDepthTexture(surf.width, surf.height))
			return false;

		// Library par dÃ©faut (shaders .metal compilÃ©s dans le bundle)
		NSError *err = nil;
		id<MTLLibrary> lib = [device newDefaultLibrary];
		if (lib) {
			mData.library = (void *)CFBridgingRetain(lib);
		} else {
			NK_MTL_LOG("No default MTLLibrary (.metal shaders not compiled)\n");
		}

		// Infos
		mData.renderer = [device.name UTF8String];
#if defined(NKENTSEU_PLATFORM_MACOS)
		mData.vramMB = (uint32)(device.recommendedMaxWorkingSetSize / (1024 * 1024));
#else
		mData.vramMB = 0; // recommendedMaxWorkingSetSize indisponible sur iOS
#endif

		mIsValid = true;
		NK_MTL_LOG("Ready â€” %s | VRAM %u MB | vsync=%s\n", mData.renderer, mData.vramMB, m.vsync ? "on" : "off");
		return true;
	}

	// =============================================================================
	bool NkMetalContext::CreateDepthTexture(uint32 w, uint32 h) {
		if (w == 0 || h == 0)
			return true;
		id<MTLDevice> device = (__bridge id<MTLDevice>)mData.device;

		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
																						width:w
																					   height:h
																					mipmapped:NO];
		desc.storageMode = MTLStorageModePrivate;
		desc.usage = MTLTextureUsageRenderTarget;

		id<MTLTexture> depth = [device newTextureWithDescriptor:desc];
		if (!depth) {
			NK_MTL_ERR("Depth texture creation failed (%ux%u)\n", w, h);
			return false;
		}

		if (mData.depthTexture) {
			CFBridgingRelease(mData.depthTexture);
			mData.depthTexture = nullptr;
		}
		mData.depthTexture = (void *)CFBridgingRetain(depth);
		return true;
	}

	// =============================================================================
	void NkMetalContext::Shutdown() {
		if (!mIsValid)
			return;

		// Flush GPU : soumettre un command buffer vide et attendre
		id<MTLCommandQueue> q = (__bridge id<MTLCommandQueue>)mData.commandQueue;
		id<MTLCommandBuffer> flush = [q commandBuffer];
		[flush commit];
		[flush waitUntilCompleted];

		if (mData.currentEncoder) {
			CFBridgingRelease(mData.currentEncoder);
			mData.currentEncoder = nullptr;
		}
		if (mData.commandBuffer) {
			CFBridgingRelease(mData.commandBuffer);
			mData.commandBuffer = nullptr;
		}
		if (mData.currentDrawable) {
			CFBridgingRelease(mData.currentDrawable);
			mData.currentDrawable = nullptr;
		}
		if (mData.library) {
			CFBridgingRelease(mData.library);
			mData.library = nullptr;
		}
		if (mData.depthTexture) {
			CFBridgingRelease(mData.depthTexture);
			mData.depthTexture = nullptr;
		}
		if (mData.layer) {
			CFBridgingRelease(mData.layer);
			mData.layer = nullptr;
		}
		if (mData.commandQueue) {
			CFBridgingRelease(mData.commandQueue);
			mData.commandQueue = nullptr;
		}
		if (mReadback) {
			CFBridgingRelease(mReadback);
			mReadback = nullptr;
		}
		if (mHorsEcran) {
			CFBridgingRelease(mHorsEcran);
			mHorsEcran = nullptr;
		}
		if (mData.device) {
			CFBridgingRelease(mData.device);
			mData.device = nullptr;
		}

		mIsValid = false;
		NK_MTL_LOG("Shutdown OK\n");
	}

	// =============================================================================
	bool NkMetalContext::BeginFrame() {
		if (!mIsValid)
			return false;
		CAMetalLayer *layer = (__bridge CAMetalLayer *)mData.layer;

		id<MTLTexture> cible = nil;
		if (mKeepLast) {
			// Mode capture : texture hors ecran, au format et a la taille de la surface.
			id<MTLTexture> he = (__bridge id<MTLTexture>)mHorsEcran;
			if (!he || he.width != mData.width || he.height != mData.height) {
				if (mHorsEcran)
					CFBridgingRelease(mHorsEcran);
				id<MTLDevice> device = (__bridge id<MTLDevice>)mData.device;
				MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:layer.pixelFormat
																							   width:mData.width
																							  height:mData.height
																						   mipmapped:NO];
				td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
				td.storageMode = MTLStorageModePrivate;
				he = [device newTextureWithDescriptor:td];
				mHorsEcran = he ? (void *)CFBridgingRetain(he) : nullptr;
			}
			cible = he;
			if (!cible) {
				NK_MTL_ERR("texture hors ecran %ux%u impossible\n", mData.width, mData.height);
				return false;
			}
		} else {
			id<CAMetalDrawable> drawable = [layer nextDrawable];
			if (!drawable) {
				NK_MTL_ERR("nextDrawable returned nil (likely minimized)\n");
				return false;
			}
			mData.currentDrawable = (void *)CFBridgingRetain(drawable);
			cible = drawable.texture;
		}

		id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)mData.commandQueue;
		id<MTLCommandBuffer> cmdb = [queue commandBuffer];
		mData.commandBuffer = (void *)CFBridgingRetain(cmdb);

		// Render pass descriptor
		MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
		rpd.colorAttachments[0].texture = cible;
		rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
		rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
		rpd.colorAttachments[0].clearColor = MTLClearColorMake(mClear[0], mClear[1], mClear[2], mClear[3]);

		if (mData.depthTexture) {
			id<MTLTexture> depth = (__bridge id<MTLTexture>)mData.depthTexture;
			rpd.depthAttachment.texture = depth;
			rpd.depthAttachment.loadAction = MTLLoadActionClear;
			rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
			rpd.depthAttachment.clearDepth = 1.0;
		}

		id<MTLRenderCommandEncoder> enc = [cmdb renderCommandEncoderWithDescriptor:rpd];
		if (!enc) {
			NK_MTL_ERR("renderCommandEncoderWithDescriptor failed\n");
			return false;
		}
		mData.currentEncoder = (void *)CFBridgingRetain(enc);
		return true;
	}

	// =============================================================================
	void NkMetalContext::EndFrame() {
		if (!mIsValid || !mData.currentEncoder)
			return;
		id<MTLRenderCommandEncoder> enc = (__bridge_transfer id<MTLRenderCommandEncoder>)mData.currentEncoder;
		[enc endEncoding];
		mData.currentEncoder = nullptr;
	}

	void NkMetalContext::Present() {
		if (!mIsValid)
			return;
		id<MTLCommandBuffer> cmdb = (__bridge_transfer id<MTLCommandBuffer>)mData.commandBuffer;
		id<CAMetalDrawable> drw = (__bridge_transfer id<CAMetalDrawable>)mData.currentDrawable;
		mData.commandBuffer = nullptr;
		mData.currentDrawable = nullptr;
		if (!cmdb)
			return; // BeginFrame avait echoue (fenetre minimisee) : rien a presenter

		// Mode capture : copie de la texture hors ecran dans un tampon partage.
		id<MTLBuffer> lecture = nil;
		uint32 w = 0, h = 0;
		if (mKeepLast && mHorsEcran) {
			id<MTLTexture> tex = (__bridge id<MTLTexture>)mHorsEcran;
			w = (uint32)tex.width;
			h = (uint32)tex.height;
			const NSUInteger taille = (NSUInteger)w * h * 4u;
			lecture = (__bridge id<MTLBuffer>)mReadback;
			if (!lecture || lecture.length < taille) {
				if (mReadback)
					CFBridgingRelease(mReadback);
				id<MTLDevice> device = (__bridge id<MTLDevice>)mData.device;
				lecture = [device newBufferWithLength:taille options:MTLResourceStorageModeShared];
				mReadback = lecture ? (void *)CFBridgingRetain(lecture) : nullptr;
			}
			if (lecture) {
				id<MTLBlitCommandEncoder> blit = [cmdb blitCommandEncoder];
				[blit copyFromTexture:tex
							 sourceSlice:0
							 sourceLevel:0
							sourceOrigin:MTLOriginMake(0, 0, 0)
							  sourceSize:MTLSizeMake(w, h, 1)
								toBuffer:lecture
					   destinationOffset:0
				  destinationBytesPerRow:(NSUInteger)w * 4u
				destinationBytesPerImage:(NSUInteger)w * h * 4u];
				[blit endEncoding];
			}
		}

		if (drw)
			[cmdb presentDrawable:drw];
		[cmdb commit];

		if (lecture) {
			[cmdb waitUntilCompleted];
			// BGRA (format de la CAMetalLayer) -> RGBA.
			mLast.Resize((usize)w * h * 4u);
			const uint8 *src = (const uint8 *)lecture.contents;
			uint8 *dst = mLast.Data();
			for (usize i = 0; i < (usize)w * h; ++i) {
				dst[i * 4 + 0] = src[i * 4 + 2];
				dst[i * 4 + 1] = src[i * 4 + 1];
				dst[i * 4 + 2] = src[i * 4 + 0];
				dst[i * 4 + 3] = src[i * 4 + 3];
			}
			mLastW = w;
			mLastH = h;
		}
	}

	void NkMetalContext::SetClearColor(float r, float g, float b, float a) {
		mClear[0] = r;
		mClear[1] = g;
		mClear[2] = b;
		mClear[3] = a;
	}

	void NkMetalContext::KeepLastFrame(bool keep) {
		mKeepLast = keep;
	}

	bool NkMetalContext::ReadLastFrame(NkVector<uint8> &rgba, uint32 &width, uint32 &height) const {
		if (mLastW == 0 || mLastH == 0 || mLast.Size() < (usize)mLastW * mLastH * 4u)
			return false;
		rgba = mLast;
		width = mLastW;
		height = mLastH;
		return true;
	}

	// =============================================================================
	bool NkMetalContext::OnResize(uint32 w, uint32 h) {
		if (!mIsValid)
			return false;
		if (w == 0 || h == 0)
			return true; // MinimisÃ©e â€” skip
		mData.width = w;
		mData.height = h;
		CAMetalLayer *layer = (__bridge CAMetalLayer *)mData.layer;
		layer.drawableSize = CGSizeMake((CGFloat)w, (CGFloat)h);
		return CreateDepthTexture(w, h);
	}

	void NkMetalContext::SetVSync(bool e) {
		mVSync = e;
		CAMetalLayer *layer = (__bridge CAMetalLayer *)mData.layer;
#if defined(NKENTSEU_PLATFORM_MACOS)
		layer.displaySyncEnabled = e; // macOS uniquement ; iOS : VSync implicite
#else
		(void)layer;
#endif
	}

	bool NkMetalContext::GetVSync() const {
		return mVSync;
	}

	bool NkMetalContext::IsValid() const {
		return mIsValid;
	}

	NkGraphicsApi NkMetalContext::GetApi() const {
		return NkGraphicsApi::NK_GFX_API_METAL;
	}

	NkContextDesc NkMetalContext::GetDesc() const {
		return mDesc;
	}

	void *NkMetalContext::GetNativeContextData() {
		return &mData;
	}

	bool NkMetalContext::SupportsCompute() const {
		return true;
	}

	NkContextInfo NkMetalContext::GetInfo() const {
		NkContextInfo i;
		i.api = NkGraphicsApi::NK_GFX_API_METAL;
		i.renderer = mData.renderer;
		i.vendor = "Apple";
		i.version = "Metal";
		i.vramMB = mData.vramMB;
		i.computeSupported = true;
		// Taille de la surface en PIXELS (drawableSize) : le renderer 2D en tire
		// sa vue et son viewport initiaux. Laissee a 0, il partait en 800x600.
		i.windowWidth = mData.width;
		i.windowHeight = mData.height;
		return i;
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS || IOS
