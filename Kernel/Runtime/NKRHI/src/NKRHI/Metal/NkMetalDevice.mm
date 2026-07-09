// =============================================================================
// NkRHI_Device_Metal.mm — Backend Apple Metal
// Compilé en Objective-C++ (.mm)
// =============================================================================
#ifdef NK_RHI_METAL_ENABLED
#import "NkMetalDevice.h"
#import "NkMetalCommandBuffer.h"
#import <TargetConditionals.h> // TARGET_OS_OSX / TARGET_OS_IPHONE
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <simd/simd.h>
#include "NKMemory/NkAllocator.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

#define NK_MTL_LOG(...) printf("[NkRHI_Metal] " __VA_ARGS__)
#define NK_MTL_ERR(...) printf("[NkRHI_Metal][ERR] " __VA_ARGS__)

namespace nkentseu {

	// =============================================================================
	static MTLPixelFormat ToMTLFormat(NkGPUFormat f) {
		switch (f) {
			case NkGPUFormat::NK_R8_UNORM:
				return MTLPixelFormatR8Unorm;
			case NkGPUFormat::NK_RG8_UNORM:
				return MTLPixelFormatRG8Unorm;
			case NkGPUFormat::NK_RGBA8_UNORM:
				return MTLPixelFormatRGBA8Unorm;
			case NkGPUFormat::NK_RGBA8_SRGB:
				return MTLPixelFormatRGBA8Unorm_sRGB;
			case NkGPUFormat::NK_BGRA8_UNORM:
				return MTLPixelFormatBGRA8Unorm;
			case NkGPUFormat::NK_BGRA8_SRGB:
				return MTLPixelFormatBGRA8Unorm_sRGB;
			case NkGPUFormat::NK_R16_FLOAT:
				return MTLPixelFormatR16Float;
			case NkGPUFormat::NK_RG16_FLOAT:
				return MTLPixelFormatRG16Float;
			case NkGPUFormat::NK_RGBA16_FLOAT:
				return MTLPixelFormatRGBA16Float;
			case NkGPUFormat::NK_R32_FLOAT:
				return MTLPixelFormatR32Float;
			case NkGPUFormat::NK_RG32_FLOAT:
				return MTLPixelFormatRG32Float;
			case NkGPUFormat::NK_RGBA32_FLOAT:
				return MTLPixelFormatRGBA32Float;
			case NkGPUFormat::NK_R32_UINT:
				return MTLPixelFormatR32Uint;
			// Depth16Unorm = iOS 13+, Depth24Unorm_Stencil8 + BC* = macOS uniquement.
			// Sur iOS on retombe sur des formats universellement disponibles.
			case NkGPUFormat::NK_D16_UNORM:
#if TARGET_OS_OSX
				return MTLPixelFormatDepth16Unorm;
#else
				return MTLPixelFormatDepth32Float;
#endif
			case NkGPUFormat::NK_D32_FLOAT:
				return MTLPixelFormatDepth32Float;
			case NkGPUFormat::NK_D24_UNORM_S8_UINT:
#if TARGET_OS_OSX
				return MTLPixelFormatDepth24Unorm_Stencil8;
#else
				return MTLPixelFormatDepth32Float_Stencil8;
#endif
			case NkGPUFormat::NK_D32_FLOAT_S8_UINT:
				return MTLPixelFormatDepth32Float_Stencil8;
#if TARGET_OS_OSX
			case NkGPUFormat::NK_BC1_RGB_UNORM:
				return MTLPixelFormatBC1_RGBA;
			case NkGPUFormat::NK_BC3_UNORM:
				return MTLPixelFormatBC3_RGBA;
			case NkGPUFormat::NK_BC5_UNORM:
				return MTLPixelFormatBC5_RGUnorm;
			case NkGPUFormat::NK_BC7_UNORM:
				return MTLPixelFormatBC7_RGBAUnorm;
#endif
			case NkGPUFormat::NK_R11G11B10_FLOAT:
				return MTLPixelFormatRG11B10Float;
			case NkGPUFormat::NK_A2B10G10R10_UNORM:
				return MTLPixelFormatBGR10A2Unorm;
			default:
				return MTLPixelFormatRGBA8Unorm;
		}
	}

	static MTLSamplerAddressMode ToMTLAddress(NkAddressMode a) {
		switch (a) {
			case NkAddressMode::NK_REPEAT:
				return MTLSamplerAddressModeRepeat;
			case NkAddressMode::NK_MIRRORED_REPEAT:
				return MTLSamplerAddressModeMirrorRepeat;
			case NkAddressMode::NK_CLAMP_TO_EDGE:
				return MTLSamplerAddressModeClampToEdge;
			case NkAddressMode::NK_CLAMP_TO_BORDER:
#if TARGET_OS_OSX
				return MTLSamplerAddressModeClampToBorderColor;
#else
				return MTLSamplerAddressModeClampToEdge; // ClampToBorderColor = iOS 14+
#endif
			default:
				return MTLSamplerAddressModeRepeat;
		}
	}

	static MTLSamplerMinMagFilter ToMTLFilter(NkFilter f) {
		return f == NkFilter::NK_NEAREST ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
	}

	static MTLSamplerMipFilter ToMTLMipFilter(NkMipFilter f) {
		switch (f) {
			case NkMipFilter::NK_NONE:
				return MTLSamplerMipFilterNotMipmapped;
			case NkMipFilter::NK_NEAREST:
				return MTLSamplerMipFilterNearest;
			default:
				return MTLSamplerMipFilterLinear;
		}
	}

	static MTLCompareFunction ToMTLCompare(NkCompareOp op) {
		switch (op) {
			case NkCompareOp::NK_NEVER:
				return MTLCompareFunctionNever;
			case NkCompareOp::NK_LESS:
				return MTLCompareFunctionLess;
			case NkCompareOp::NK_EQUAL:
				return MTLCompareFunctionEqual;
			case NkCompareOp::NK_LESS_EQUAL:
				return MTLCompareFunctionLessEqual;
			case NkCompareOp::NK_GREATER:
				return MTLCompareFunctionGreater;
			case NkCompareOp::NK_NOT_EQUAL:
				return MTLCompareFunctionNotEqual;
			case NkCompareOp::NK_GREATER_EQUAL:
				return MTLCompareFunctionGreaterEqual;
			default:
				return MTLCompareFunctionAlways;
		}
	}

	static MTLBlendFactor ToMTLBlend(NkBlendFactor f) {
		switch (f) {
			case NkBlendFactor::NK_ZERO:
				return MTLBlendFactorZero;
			case NkBlendFactor::NK_ONE:
				return MTLBlendFactorOne;
			case NkBlendFactor::NK_SRC_COLOR:
				return MTLBlendFactorSourceColor;
			case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:
				return MTLBlendFactorOneMinusSourceColor;
			case NkBlendFactor::NK_SRC_ALPHA:
				return MTLBlendFactorSourceAlpha;
			case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:
				return MTLBlendFactorOneMinusSourceAlpha;
			case NkBlendFactor::NK_DST_ALPHA:
				return MTLBlendFactorDestinationAlpha;
			case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:
				return MTLBlendFactorOneMinusDestinationAlpha;
			case NkBlendFactor::NK_SRC_ALPHA_SATURATE:
				return MTLBlendFactorSourceAlphaSaturated;
			default:
				return MTLBlendFactorOne;
		}
	}

	static MTLBlendOperation ToMTLBlendOp(NkBlendOp op) {
		switch (op) {
			case NkBlendOp::NK_ADD:
				return MTLBlendOperationAdd;
			case NkBlendOp::NK_SUB:
				return MTLBlendOperationSubtract;
			case NkBlendOp::NK_REV_SUB:
				return MTLBlendOperationReverseSubtract;
			case NkBlendOp::NK_MIN:
				return MTLBlendOperationMin;
			case NkBlendOp::NK_MAX:
				return MTLBlendOperationMax;
			default:
				return MTLBlendOperationAdd;
		}
	}

	static MTLStencilOperation ToMTLStencilOp(NkStencilOp op) {
		switch (op) {
			case NkStencilOp::NK_KEEP:
				return MTLStencilOperationKeep;
			case NkStencilOp::NK_ZERO:
				return MTLStencilOperationZero;
			case NkStencilOp::NK_REPLACE:
				return MTLStencilOperationReplace;
			case NkStencilOp::NK_INCR_CLAMP:
				return MTLStencilOperationIncrementClamp;
			case NkStencilOp::NK_DECR_CLAMP:
				return MTLStencilOperationDecrementClamp;
			case NkStencilOp::NK_INVERT:
				return MTLStencilOperationInvert;
			case NkStencilOp::NK_INCR_WRAP:
				return MTLStencilOperationIncrementWrap;
			case NkStencilOp::NK_DECR_WRAP:
				return MTLStencilOperationDecrementWrap;
			default:
				return MTLStencilOperationKeep;
		}
	}

	static MTLPrimitiveType ToMTLTopology(NkPrimitiveTopology t) {
		switch (t) {
			case NkPrimitiveTopology::NK_TRIANGLE_LIST:
				return MTLPrimitiveTypeTriangle;
			case NkPrimitiveTopology::NK_TRIANGLE_STRIP:
				return MTLPrimitiveTypeTriangleStrip;
			case NkPrimitiveTopology::NK_LINE_LIST:
				return MTLPrimitiveTypeLine;
			case NkPrimitiveTopology::NK_LINE_STRIP:
				return MTLPrimitiveTypeLineStrip;
			case NkPrimitiveTopology::NK_POINT_LIST:
				return MTLPrimitiveTypePoint;
			default:
				return MTLPrimitiveTypeTriangle;
		}
	}

	static MTLVertexFormat ToMTLVertexFormat(NkVertexFormat f) {
		switch (f) {
			case NkVertexFormat::NK_R32_FLOAT:
				return MTLVertexFormatFloat;
			case NkVertexFormat::NK_RG32_FLOAT:
				return MTLVertexFormatFloat2;
			case NkVertexFormat::NK_RGB32_FLOAT:
				return MTLVertexFormatFloat3;
			case NkVertexFormat::NK_RGBA32_FLOAT:
				return MTLVertexFormatFloat4;
			case NkVertexFormat::NK_RG16_FLOAT:
				return MTLVertexFormatHalf2;
			case NkVertexFormat::NK_RGBA16_FLOAT:
				return MTLVertexFormatHalf4;
			case NkVertexFormat::NK_RGBA8_UNORM:
			case NkVertexFormat::NK_R8G8B8A8_UNORM_PACKED:
				return MTLVertexFormatUChar4Normalized;
			case NkVertexFormat::NK_RGBA8_SNORM:
				return MTLVertexFormatChar4Normalized;
			case NkVertexFormat::NK_R32_UINT:
				return MTLVertexFormatUInt;
			case NkVertexFormat::NK_RGBA32_UINT:
				return MTLVertexFormatUInt4;
			default:
				return MTLVertexFormatFloat3;
		}
	}

	// =============================================================================
	NkMetalDevice::~NkMetalDevice() {
		if (mIsValid)
			Shutdown();
	}

	bool NkMetalDevice::Initialize(const NkDeviceInitInfo &init) {
		const NkMetalDesc &desc = init.context.metal;

		// Headless / compute-only : pas de CAMetalLayer -> on cree juste le MTLDevice
		// + la queue (Metal n'exige pas de surface pour le compute). Sinon, la layer
		// est FOURNIE par l'appelant (NKRHI ne connait pas la couche fenetrage).
		const bool headless = (desc.metalLayer == nullptr);

		// Device : prefere celui fourni, sinon le device systeme par defaut.
		if (desc.preferredDevice) {
			mDevice = (__bridge id<MTLDevice>)desc.preferredDevice;
		} else {
			mDevice = MTLCreateSystemDefaultDevice();
		}
		if (!mDevice) {
			NK_MTL_ERR("MTLDevice indisponible\n");
			return false;
		}
		mQueue = [mDevice newCommandQueue];

		if (headless) {
			mLayer = nil;
			mWidth = 0;
			mHeight = 0;
			QueryCaps();
			mIsValid = true;
			NK_MTL_LOG("Initialisé HEADLESS (compute, sans layer) sur %s\n", [mDevice.name UTF8String]);
			return true;
		}

		mLayer = (__bridge CAMetalLayer *)desc.metalLayer;
		mLayer.device = mDevice;
		mLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB; // cf CreateSwapchainObjects (NK_BGRA8_SRGB)

		// Dimensions : la layer est la source de verite (drawableSize, sinon bounds*scale).
		CGSize ds = mLayer.drawableSize;
		if (ds.width < 1.0 || ds.height < 1.0) {
			CGFloat scale = mLayer.contentsScale > 0.0 ? mLayer.contentsScale : 1.0;
			ds = CGSizeMake(mLayer.bounds.size.width * scale, mLayer.bounds.size.height * scale);
			mLayer.drawableSize = ds;
		}
		mWidth = (uint32)ds.width;
		mHeight = (uint32)ds.height;

		CreateSwapchainObjects();
		QueryCaps();

		mIsValid = true;
		NK_MTL_LOG("Initialisé (%u×%u) sur %s\n", mWidth, mHeight, [mDevice.name UTF8String]);
		return true;
	}

	void NkMetalDevice::CreateSwapchainObjects() {
		// Depth texture
		NkTextureDesc dd = NkTextureDesc::DepthStencil(mWidth, mHeight);
		mDepthTex = CreateTexture(dd);

		// Render pass metadata
		NkRenderPassDesc rpd;
		rpd.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_BGRA8_SRGB)).SetDepth(NkAttachmentDesc::Depth());
		mSwapchainRP = CreateRenderPass(rpd);

		// Framebuffer swapchain (la color attachment sera remplacée à chaque frame)
		NkFramebufferDesc fbd;
		fbd.renderPass = mSwapchainRP;
		// colorAttachments laisse vide ici (rempli dans BeginFrame avec le drawable).
		fbd.depthAttachment = mDepthTex;
		fbd.width = mWidth;
		fbd.height = mHeight;
		mSwapchainFB = CreateFramebuffer(fbd);
	}

	void NkMetalDevice::Shutdown() {
		WaitIdle();
		mBuffers.ForEach([](const uint64 &, NkMetalBuffer &b) {
			if (b.buf)
				[(__bridge id<MTLBuffer>)b.buf setPurgeableState:MTLPurgeableStateEmpty];
		});
		// (textures : rien a faire ici, ARC libere les id<MTLTexture>)
		mBuffers.Clear();
		mTextures.Clear();
		mSamplers.Clear();
		mShaders.Clear();
		mPipelines.Clear();
		mRenderPasses.Clear();
		mFramebuffers.Clear();
		mDescLayouts.Clear();
		mDescSets.Clear();
		mIsValid = false;
		NK_MTL_LOG("Shutdown\n");
	}

	// =============================================================================
	// Buffers
	// =============================================================================
	NkBufferHandle NkMetalDevice::CreateBuffer(const NkBufferDesc &desc) {
		threading::NkScopedLockMutex lock(mMutex);
		MTLResourceOptions opts = MTLResourceStorageModeShared; // CPU+GPU visible
		switch (desc.usage) {
			case NkResourceUsage::NK_DEFAULT:
				opts = MTLResourceStorageModePrivate;
				break;
			default:
				break;
		}

		id<MTLBuffer> buf = [mDevice newBufferWithLength:(NSUInteger)desc.sizeBytes options:opts];
		if (!buf)
			return {};

		if (desc.initialData && opts != MTLResourceStorageModePrivate)
			memcpy(buf.contents, desc.initialData, (size_t)desc.sizeBytes);

		if (desc.debugName)
			buf.label = [NSString stringWithUTF8String:desc.debugName];

		uint64 hid = NextId();
		mBuffers[hid] = {(__bridge_retained void *)buf, desc};
		NkBufferHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyBuffer(NkBufferHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *it = mBuffers.Find(h.id);
		if (!it)
			return;
		if (it->buf)
			CFRelease(it->buf);
		mBuffers.Erase(h.id);
		h.id = 0;
	}

	bool NkMetalDevice::WriteBuffer(NkBufferHandle buf, const void *data, uint64 sz, uint64 off) {
		auto *it = mBuffers.Find(buf.id);
		if (!it)
			return false;
		auto *b = (__bridge id<MTLBuffer>)it->buf;
		if (b.storageMode == MTLStorageModeShared) {
			memcpy((uint8 *)b.contents + off, data, (size_t)sz);
			return true;
		}
		// GPU-only : upload via staging + blit
		id<MTLBuffer> stage = [mDevice newBufferWithLength:sz options:MTLResourceStorageModeShared];
		memcpy(stage.contents, data, (size_t)sz);
		id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
		[blit copyFromBuffer:stage sourceOffset:0 toBuffer:b destinationOffset:off size:sz];
		[blit endEncoding];
		[cmd commit];
		[cmd waitUntilCompleted];
		return true;
	}

	bool NkMetalDevice::WriteBufferAsync(NkBufferHandle buf, const void *data, uint64 sz, uint64 off) {
		auto *it = mBuffers.Find(buf.id);
		if (!it)
			return false;
		auto *b = (__bridge id<MTLBuffer>)it->buf;
		if (b.storageMode == MTLStorageModeShared) {
			memcpy((uint8 *)b.contents + off, data, (size_t)sz);
			return true;
		}
		return WriteBuffer(buf, data, sz, off);
	}

	bool NkMetalDevice::ReadBuffer(NkBufferHandle buf, void *out, uint64 sz, uint64 off) {
		auto *it = mBuffers.Find(buf.id);
		if (!it)
			return false;
		auto *b = (__bridge id<MTLBuffer>)it->buf;
		if (b.storageMode == MTLStorageModeShared) {
			memcpy(out, (uint8 *)b.contents + off, (size_t)sz);
			return true;
		}
		id<MTLBuffer> stage = [mDevice newBufferWithLength:sz options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
		[blit copyFromBuffer:b sourceOffset:off toBuffer:stage destinationOffset:0 size:sz];
		[blit endEncoding];
		[cmd commit];
		[cmd waitUntilCompleted];
		memcpy(out, stage.contents, (size_t)sz);
		return true;
	}

	NkMappedMemory NkMetalDevice::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
		auto *it = mBuffers.Find(buf.id);
		if (!it)
			return {};
		auto *b = (__bridge id<MTLBuffer>)it->buf;
		if (b.storageMode != MTLStorageModeShared)
			return {};
		uint64 mapSz = sz > 0 ? sz : it->desc.sizeBytes - off;
		return {(uint8 *)b.contents + off, mapSz};
	}

	void NkMetalDevice::UnmapBuffer(NkBufferHandle) {
	} // No-op pour Metal shared

	// =============================================================================
	// Textures
	// =============================================================================
	NkTextureHandle NkMetalDevice::CreateTexture(const NkTextureDesc &desc) {
		threading::NkScopedLockMutex lock(mMutex);
		MTLTextureDescriptor *td = [[MTLTextureDescriptor alloc] init];
		td.pixelFormat = ToMTLFormat(desc.format);
		td.width = desc.width;
		td.height = desc.height;
		td.depth = desc.depth > 0 ? desc.depth : 1;
		td.mipmapLevelCount =
			desc.mipLevels == 0
				? (NSUInteger)(floor(log2((double)(desc.width > desc.height ? desc.width : desc.height))) + 1)
				: desc.mipLevels;
		td.arrayLength = desc.arrayLayers;
		td.sampleCount = (NSUInteger)desc.samples;
		td.storageMode = MTLStorageModePrivate;

		td.usage = MTLTextureUsageUnknown;
		if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_SHADER_RESOURCE))
			td.usage |= MTLTextureUsageShaderRead;
		if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET))
			td.usage |= MTLTextureUsageRenderTarget;
		if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_DEPTH_STENCIL))
			td.usage |= MTLTextureUsageRenderTarget;
		if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNORDERED_ACCESS))
			td.usage |= MTLTextureUsageShaderWrite;
		if (td.usage == MTLTextureUsageUnknown)
			td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

		switch (desc.type) {
			case NkTextureType::NK_CUBE:
				td.textureType = MTLTextureTypeCube;
				break;
			case NkTextureType::NK_TEX3D:
				td.textureType = MTLTextureType3D;
				break;
			case NkTextureType::NK_TEX2D_ARRAY:
				td.textureType = MTLTextureType2DArray;
				break;
			default:
				td.textureType = MTLTextureType2D;
				break;
		}

		id<MTLTexture> tex = [mDevice newTextureWithDescriptor:td];
		if (!tex)
			return {};

		if (desc.initialData) {
			uint32 bpp = NkFormatBytesPerPixel(desc.format);
			uint32 rp = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
			MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
			// Pour un stockage private, on doit passer par un buffer staging + blit
			id<MTLBuffer> stage = [mDevice newBufferWithBytes:desc.initialData
													   length:rp * desc.height
													  options:MTLResourceStorageModeShared];
			id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
			id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
			[blit copyFromBuffer:stage
					   sourceOffset:0
				  sourceBytesPerRow:rp
				sourceBytesPerImage:rp * desc.height
						 sourceSize:MTLSizeMake(desc.width, desc.height, 1)
						  toTexture:tex
				   destinationSlice:0
				   destinationLevel:0
				  destinationOrigin:MTLOriginMake(0, 0, 0)];
			if (desc.mipLevels != 1)
				[blit generateMipmapsForTexture:tex];
			[blit endEncoding];
			[cmd commit];
			[cmd waitUntilCompleted];
		}

		if (desc.debugName)
			tex.label = [NSString stringWithUTF8String:desc.debugName];

		uint64 hid = NextId();
		mTextures[hid] = {(__bridge_retained void *)tex, desc, false};
		NkTextureHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyTexture(NkTextureHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *it = mTextures.Find(h.id);
		if (!it)
			return;
		if (!it->isSwapchain && it->tex)
			CFRelease(it->tex);
		mTextures.Erase(h.id);
		h.id = 0;
	}

	bool NkMetalDevice::WriteTexture(NkTextureHandle t, const void *p, uint32 rp) {
		auto *it = mTextures.Find(t.id);
		if (!it)
			return false;
		auto &desc = it->desc;
		return WriteTextureRegion(t, p, 0, 0, 0, desc.width, desc.height, 1, 0, 0, rp);
	}

	bool NkMetalDevice::WriteTextureRegion(NkTextureHandle t, const void *pixels, uint32 x, uint32 y, uint32 /*z*/,
										   uint32 w, uint32 h, uint32 /*d2*/, uint32 mip, uint32 slice,
										   uint32 rowPitch) {
		auto *it = mTextures.Find(t.id);
		if (!it)
			return false;
		auto *tex = (__bridge id<MTLTexture>)it->tex;
		uint32 bpp = NkFormatBytesPerPixel(it->desc.format);
		uint32 rp = rowPitch > 0 ? rowPitch : w * bpp;
		id<MTLBuffer> stage = [mDevice newBufferWithBytes:pixels length:rp * h options:MTLResourceStorageModeShared];
		id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
		[blit copyFromBuffer:stage
				   sourceOffset:0
			  sourceBytesPerRow:rp
			sourceBytesPerImage:rp * h
					 sourceSize:MTLSizeMake(w, h, 1)
					  toTexture:tex
			   destinationSlice:slice
			   destinationLevel:mip
			  destinationOrigin:MTLOriginMake(x, y, 0)];
		[blit endEncoding];
		[cmd commit];
		[cmd waitUntilCompleted];
		return true;
	}

	bool NkMetalDevice::GenerateMipmaps(NkTextureHandle t, NkFilter) {
		auto *it = mTextures.Find(t.id);
		if (!it)
			return false;
		auto *tex = (__bridge id<MTLTexture>)it->tex;
		id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
		[blit generateMipmapsForTexture:tex];
		[blit endEncoding];
		[cmd commit];
		[cmd waitUntilCompleted];
		return true;
	}

	// =============================================================================
	// Samplers
	// =============================================================================
	NkSamplerHandle NkMetalDevice::CreateSampler(const NkSamplerDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
		sd.magFilter = ToMTLFilter(d.magFilter);
		sd.minFilter = ToMTLFilter(d.minFilter);
		sd.mipFilter = ToMTLMipFilter(d.mipFilter);
		sd.sAddressMode = ToMTLAddress(d.addressU);
		sd.tAddressMode = ToMTLAddress(d.addressV);
		sd.rAddressMode = ToMTLAddress(d.addressW);
		sd.lodMinClamp = d.minLod;
		sd.lodMaxClamp = d.maxLod;
		sd.maxAnisotropy = (NSUInteger)d.maxAnisotropy;
		sd.compareFunction = d.compareEnable ? ToMTLCompare(d.compareOp) : MTLCompareFunctionNever;
		id<MTLSamplerState> ss = [mDevice newSamplerStateWithDescriptor:sd];
		uint64 hid = NextId();
		mSamplers[hid] = {(__bridge_retained void *)ss};
		NkSamplerHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroySampler(NkSamplerHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *it = mSamplers.Find(h.id);
		if (!it)
			return;
		if (it->ss)
			CFRelease(it->ss);
		mSamplers.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Shaders (MSL source ou bibliothèque pré-compilée)
	// =============================================================================
	NkShaderHandle NkMetalDevice::CreateShader(const NkShaderDesc &desc) {
		threading::NkScopedLockMutex lock(mMutex);
		NkMetalShader sh;
		for (uint32 i = 0; i < desc.stages.Size(); i++) {
			auto &s = desc.stages[i];
			id<MTLLibrary> lib = nil;

			if (s.mslSource) {
				NSError *err = nil;
				NSString *src = [NSString stringWithUTF8String:s.mslSource];
				lib = [mDevice newLibraryWithSource:src options:nil error:&err];
				if (err)
					NK_MTL_ERR("Shader MSL: %s\n", [err.localizedDescription UTF8String]);
			} else if (!s.spirvBinary.Empty()) {
				// Metal ne lit pas nativement le SPIR-V : conversion en MSL requise
				// (SPIRV-Cross) en pré-build.
				NK_MTL_ERR("Metal: SPIR-V non supporté directement, utiliser MSL\n");
				continue;
			} else if (s.metalIRData && s.metalIRSize) {
				// Metal IR / metallib précompilé fourni comme blob mémoire.
				dispatch_data_t dd = dispatch_data_create(s.metalIRData, (size_t)s.metalIRSize,
														  dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
				NSError *err = nil;
				lib = [mDevice newLibraryWithData:dd error:&err];
				if (err)
					NK_MTL_ERR("metallib (data): %s\n", [err.localizedDescription UTF8String]);
			}

			if (!lib)
				continue;
			const char *entry = s.entryPoint ? s.entryPoint : "main";
			NSString *fn = [NSString stringWithUTF8String:entry];
			id<MTLFunction> func = [lib newFunctionWithName:fn];
			if (!func) {
				NK_MTL_ERR("Fonction '%s' introuvable dans le shader\n", entry);
				continue;
			}

			void *retained = (__bridge_retained void *)func;
			switch (s.stage) {
				case NkShaderStage::NK_VERTEX:
					sh.vert = retained;
					break;
				case NkShaderStage::NK_FRAGMENT:
					sh.frag = retained;
					break;
				case NkShaderStage::NK_COMPUTE:
					sh.comp = retained;
					break;
				default:
					CFRelease(retained);
					break;
			}
		}
		uint64 hid = NextId();
		mShaders[hid] = sh;
		NkShaderHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyShader(NkShaderHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *it = mShaders.Find(h.id);
		if (!it)
			return;
		if (it->vert)
			CFRelease(it->vert);
		if (it->frag)
			CFRelease(it->frag);
		if (it->comp)
			CFRelease(it->comp);
		mShaders.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Pipelines
	// =============================================================================
	NkPipelineHandle NkMetalDevice::CreateGraphicsPipeline(const NkGraphicsPipelineDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *sit = mShaders.Find(d.shader.id);
		if (!sit)
			return {};
		auto &sh = *sit;

		MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
		if (sh.vert)
			pd.vertexFunction = (__bridge id<MTLFunction>)sh.vert;
		if (sh.frag)
			pd.fragmentFunction = (__bridge id<MTLFunction>)sh.frag;
		pd.sampleCount = (NSUInteger)d.samples;

		// Vertex descriptor
		if (d.vertexLayout.attributes.Size() > 0) {
			MTLVertexDescriptor *vd = [[MTLVertexDescriptor alloc] init];
			for (uint32 i = 0; i < d.vertexLayout.attributes.Size(); i++) {
				auto &a = d.vertexLayout.attributes[i];
				vd.attributes[a.location].format = ToMTLVertexFormat(a.format);
				vd.attributes[a.location].offset = a.offset;
				vd.attributes[a.location].bufferIndex = a.binding;
			}
			for (uint32 i = 0; i < d.vertexLayout.bindings.Size(); i++) {
				auto &b = d.vertexLayout.bindings[i];
				vd.layouts[b.binding].stride = b.stride;
				vd.layouts[b.binding].stepFunction =
					b.perInstance ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
			}
			pd.vertexDescriptor = vd;
		}

		// Render target formats
		auto *rpit = mRenderPasses.Find(d.renderPass.id);
		if (rpit) {
			for (uint32 i = 0; i < rpit->desc.colorAttachments.Size(); i++)
				pd.colorAttachments[i].pixelFormat = ToMTLFormat(rpit->desc.colorAttachments[i].format);
			if (rpit->desc.hasDepth)
				pd.depthAttachmentPixelFormat = ToMTLFormat(rpit->desc.depthAttachment.format);
		} else {
			pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
			pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
		}

		// Blend
		for (uint32 i = 0; i < d.blend.attachments.Size() && i < 8; i++) {
			auto &a = d.blend.attachments[i];
			pd.colorAttachments[i].blendingEnabled = a.blendEnable;
			pd.colorAttachments[i].sourceRGBBlendFactor = ToMTLBlend(a.srcColor);
			pd.colorAttachments[i].destinationRGBBlendFactor = ToMTLBlend(a.dstColor);
			pd.colorAttachments[i].rgbBlendOperation = ToMTLBlendOp(a.colorOp);
			pd.colorAttachments[i].sourceAlphaBlendFactor = ToMTLBlend(a.srcAlpha);
			pd.colorAttachments[i].destinationAlphaBlendFactor = ToMTLBlend(a.dstAlpha);
			pd.colorAttachments[i].alphaBlendOperation = ToMTLBlendOp(a.alphaOp);
			pd.colorAttachments[i].writeMask = a.colorWriteMask & 0xF;
		}

		NSError *err = nil;
		id<MTLRenderPipelineState> rpso = [mDevice newRenderPipelineStateWithDescriptor:pd error:&err];
		if (err) {
			NK_MTL_ERR("Pipeline: %s\n", [err.localizedDescription UTF8String]);
			return {};
		}

		// Depth-stencil state
		MTLDepthStencilDescriptor *dsd = [[MTLDepthStencilDescriptor alloc] init];
		dsd.depthCompareFunction =
			d.depthStencil.depthTestEnable ? ToMTLCompare(d.depthStencil.depthCompareOp) : MTLCompareFunctionAlways;
		dsd.depthWriteEnabled = d.depthStencil.depthWriteEnable;
		if (d.depthStencil.stencilEnable) {
			MTLStencilDescriptor *sf = [[MTLStencilDescriptor alloc] init];
			sf.stencilFailureOperation = ToMTLStencilOp(d.depthStencil.front.failOp);
			sf.depthFailureOperation = ToMTLStencilOp(d.depthStencil.front.depthFailOp);
			sf.depthStencilPassOperation = ToMTLStencilOp(d.depthStencil.front.passOp);
			sf.stencilCompareFunction = ToMTLCompare(d.depthStencil.front.compareOp);
			dsd.frontFaceStencil = sf;
			MTLStencilDescriptor *sb = [[MTLStencilDescriptor alloc] init];
			sb.stencilFailureOperation = ToMTLStencilOp(d.depthStencil.back.failOp);
			sb.depthFailureOperation = ToMTLStencilOp(d.depthStencil.back.depthFailOp);
			sb.depthStencilPassOperation = ToMTLStencilOp(d.depthStencil.back.passOp);
			sb.stencilCompareFunction = ToMTLCompare(d.depthStencil.back.compareOp);
			dsd.backFaceStencil = sb;
		}
		id<MTLDepthStencilState> dss = [mDevice newDepthStencilStateWithDescriptor:dsd];

		NkMetalPipeline p;
		p.rpso = (__bridge_retained void *)rpso;
		p.dss = (__bridge_retained void *)dss;
		p.isCompute = false;
		p.frontFaceCCW = d.rasterizer.frontFace == NkFrontFace::NK_CCW;
		p.cullMode = d.rasterizer.cullMode == NkCullMode::NK_NONE	 ? 0
					 : d.rasterizer.cullMode == NkCullMode::NK_FRONT ? 1
																	 : 2;
		p.depthBiasConst = d.rasterizer.depthBiasConst;
		p.depthBiasSlope = d.rasterizer.depthBiasSlope;
		p.depthBiasClamp = d.rasterizer.depthBiasClamp;

		uint64 hid = NextId();
		mPipelines[hid] = p;
		NkPipelineHandle h;
		h.id = hid;
		return h;
	}

	NkPipelineHandle NkMetalDevice::CreateComputePipeline(const NkComputePipelineDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *sit = mShaders.Find(d.shader.id);
		if (!sit)
			return {};
		id<MTLFunction> comp = (__bridge id<MTLFunction>)sit->comp;
		if (!comp)
			return {};
		NSError *err = nil;
		id<MTLComputePipelineState> cpso = [mDevice newComputePipelineStateWithFunction:comp error:&err];
		if (err) {
			NK_MTL_ERR("ComputePipeline: %s\n", [err.localizedDescription UTF8String]);
			return {};
		}
		NkMetalPipeline p;
		p.cpso = (__bridge_retained void *)cpso;
		p.isCompute = true;
		uint64 hid = NextId();
		mPipelines[hid] = p;
		NkPipelineHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyPipeline(NkPipelineHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		auto *it = mPipelines.Find(h.id);
		if (!it)
			return;
		if (it->rpso)
			CFRelease(it->rpso);
		if (it->cpso)
			CFRelease(it->cpso);
		if (it->dss)
			CFRelease(it->dss);
		mPipelines.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Render Passes & Framebuffers
	// =============================================================================
	NkRenderPassHandle NkMetalDevice::CreateRenderPass(const NkRenderPassDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		uint64 hid = NextId();
		mRenderPasses[hid] = {d};
		NkRenderPassHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyRenderPass(NkRenderPassHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mRenderPasses.Erase(h.id);
		h.id = 0;
	}

	NkFramebufferHandle NkMetalDevice::CreateFramebuffer(const NkFramebufferDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		NkMetalFramebuffer fb;
		fb.colorCount = d.colorAttachments.Size();
		for (uint32 i = 0; i < d.colorAttachments.Size(); i++)
			fb.colorAttachments[i] = d.colorAttachments[i];
		fb.depthAttachment = d.depthAttachment;
		fb.w = d.width;
		fb.h = d.height;
		uint64 hid = NextId();
		mFramebuffers[hid] = fb;
		NkFramebufferHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyFramebuffer(NkFramebufferHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mFramebuffers.Erase(h.id);
		h.id = 0;
	}

	// =============================================================================
	// Descriptor Sets
	// =============================================================================
	NkDescSetHandle NkMetalDevice::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc &d) {
		threading::NkScopedLockMutex lock(mMutex);
		uint64 hid = NextId();
		mDescLayouts[hid] = {d};
		NkDescSetHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyDescriptorSetLayout(NkDescSetHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mDescLayouts.Erase(h.id);
		h.id = 0;
	}

	NkDescSetHandle NkMetalDevice::AllocateDescriptorSet(NkDescSetHandle layout) {
		threading::NkScopedLockMutex lock(mMutex);
		NkMetalDescSet ds;
		ds.layoutId = layout.id;
		uint64 hid = NextId();
		mDescSets[hid] = ds;
		NkDescSetHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::FreeDescriptorSet(NkDescSetHandle &h) {
		threading::NkScopedLockMutex lock(mMutex);
		mDescSets.Erase(h.id);
		h.id = 0;
	}

	void NkMetalDevice::UpdateDescriptorSets(const NkDescriptorWrite *writes, uint32 n) {
		threading::NkScopedLockMutex lock(mMutex);
		for (uint32 i = 0; i < n; i++) {
			auto &w = writes[i];
			auto *sit = mDescSets.Find(w.set.id);
			if (!sit)
				continue;
			NkMetalDescSet::Binding b{w.binding, w.type, w.buffer.id, w.texture.id, w.sampler.id};
			bool found = false;
			for (uint32 j = 0; j < sit->bindings.Size(); j++)
				if (sit->bindings[j].slot == w.binding) {
					sit->bindings[j] = b;
					found = true;
					break;
				}
			if (!found)
				sit->bindings.PushBack(b);
		}
	}

	// =============================================================================
	// Command Buffers
	// =============================================================================
	NkICommandBuffer *NkMetalDevice::CreateCommandBuffer(NkCommandBufferType t) {
		return nkentseu::memory::NkGetDefaultAllocator().New<NkMetalCommandBuffer>(this, t);
	}

	void NkMetalDevice::DestroyCommandBuffer(NkICommandBuffer *&cb) {
		nkentseu::memory::NkGetDefaultAllocator().Delete(cb);
		cb = nullptr;
	}

	// =============================================================================
	// Submit
	// =============================================================================
	void NkMetalDevice::Submit(NkICommandBuffer *const *cbs, uint32 n, NkFenceHandle fence) {
		for (uint32 i = 0; i < n; i++) {
			auto *m = dynamic_cast<NkMetalCommandBuffer *>(cbs[i]);
			if (m)
				m->CommitAndWait();
		}
		if (fence.IsValid()) {
			auto *it = mFences.Find(fence.id);
			if (it)
				it->signaled = true;
		}
	}

	void NkMetalDevice::SubmitAndPresent(NkICommandBuffer *cb) {
		auto *m = dynamic_cast<NkMetalCommandBuffer *>(cb);
		if (m)
			m->CommitAndPresent(mCurrentDrawable);
		mCurrentDrawable = nil;
	}

	// =============================================================================
	// Fence
	// =============================================================================
	NkFenceHandle NkMetalDevice::CreateFence(bool signaled) {
		uint64 hid = NextId();
		mFences[hid] = {signaled};
		NkFenceHandle h;
		h.id = hid;
		return h;
	}

	void NkMetalDevice::DestroyFence(NkFenceHandle &h) {
		mFences.Erase(h.id);
		h.id = 0;
	}

	bool NkMetalDevice::WaitFence(NkFenceHandle f, uint64) {
		auto *it = mFences.Find(f.id);
		return it && it->signaled;
	}

	bool NkMetalDevice::IsFenceSignaled(NkFenceHandle f) {
		auto *it = mFences.Find(f.id);
		return it && it->signaled;
	}

	void NkMetalDevice::ResetFence(NkFenceHandle f) {
		auto *it = mFences.Find(f.id);
		if (it)
			it->signaled = false;
	}

	void NkMetalDevice::WaitIdle() {
		id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
		[cmd commit];
		[cmd waitUntilCompleted];
	}

	// =============================================================================
	// Frame
	// =============================================================================
	bool NkMetalDevice::BeginFrame(NkFrameContext &frame) {
		mCurrentDrawable = [mLayer nextDrawable];
		if (!mCurrentDrawable)
			return false;

		// Mettre à jour le framebuffer swapchain avec le drawable courant
		uint64 colorId = NextId();
		NkMetalTexture swt{};
		swt.tex = (__bridge_retained void *)mCurrentDrawable.texture;
		swt.isSwapchain = true;
		swt.desc = NkTextureDesc::RenderTarget(mWidth, mHeight, NkGPUFormat::NK_BGRA8_SRGB);
		mTextures[colorId] = swt;

		// Mettre à jour le framebuffer
		auto *fbit = mFramebuffers.Find(mSwapchainFB.id);
		if (fbit) {
			NkTextureHandle ch;
			ch.id = colorId;
			fbit->colorAttachments[0] = ch;
			fbit->colorCount = 1;
		}

		frame.frameIndex = mFrameIndex;
		frame.frameNumber = mFrameNumber;
		return true;
	}

	void NkMetalDevice::EndFrame(NkFrameContext &) {
		mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES;
		++mFrameNumber;
	}

	void NkMetalDevice::OnResize(uint32 w, uint32 h) {
		if (w == 0 || h == 0)
			return;
		WaitIdle();
		mWidth = w;
		mHeight = h;
		mLayer.drawableSize = CGSizeMake(w, h);

		// Recréer depth
		if (mDepthTex.IsValid())
			DestroyTexture(mDepthTex);
		NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h);
		mDepthTex = CreateTexture(dd);

		auto *fbit = mFramebuffers.Find(mSwapchainFB.id);
		if (fbit) {
			fbit->depthAttachment = mDepthTex;
			fbit->w = w;
			fbit->h = h;
		}
	}

	// =============================================================================
	// Caps
	// =============================================================================
	void NkMetalDevice::QueryCaps() {
		mCaps.tessellationShaders = true;
		mCaps.computeShaders = true;
		mCaps.drawIndirect = true;
		mCaps.multiViewport = false; // Metal 3+
		mCaps.independentBlend = true;
#if TARGET_OS_OSX
		mCaps.textureCompressionBC = true; // Macs (Intel/AMD/Apple Silicon en rosetta) : BC
#else
		mCaps.textureCompressionBC = false; // iOS/GPU Apple : ASTC/ETC/PVRTC, pas de BC
#endif
		mCaps.textureCompressionASTC = true;
		mCaps.maxTextureDim2D = 16384;
		mCaps.maxTextureDim3D = 2048;
		mCaps.maxColorAttachments = 8;
		mCaps.maxVertexAttributes = 31;
		mCaps.maxPushConstantBytes = 4096; // argument buffers ou vertex bytes
		mCaps.minUniformBufferAlign = 256;
		mCaps.timestampQueries = true;
		mCaps.msaa2x = mCaps.msaa4x = mCaps.msaa8x = true;
		// Mesh shaders = Metal 3 (Apple7+/famille recente) : indispo sur SDK anciens
		// (les enums MTLGPUFamilyApple6/Metal3 n'existent pas avant iOS 13/16).
		// Conservativement false ici ; a affiner avec @available sur SDK recent.
		mCaps.meshShaders = false;
	}

	// =============================================================================
	// Accesseurs internes
	// =============================================================================
	void *NkMetalDevice::GetMTLBuffer(uint64 id) const {
		auto *it = mBuffers.Find(id);
		return it ? it->buf : nullptr;
	}

	void *NkMetalDevice::GetMTLTexture(uint64 id) const {
		auto *it = mTextures.Find(id);
		return it ? it->tex : nullptr;
	}

	void *NkMetalDevice::GetMTLSampler(uint64 id) const {
		auto *it = mSamplers.Find(id);
		return it ? it->ss : nullptr;
	}

	const NkMetalPipeline *NkMetalDevice::GetPipeline(uint64 id) const {
		return mPipelines.Find(id);
	}

	const NkMetalDescSet *NkMetalDevice::GetDescSet(uint64 id) const {
		return mDescSets.Find(id);
	}

	const NkMetalFramebuffer *NkMetalDevice::GetFBO(uint64 id) const {
		return mFramebuffers.Find(id);
	}

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
