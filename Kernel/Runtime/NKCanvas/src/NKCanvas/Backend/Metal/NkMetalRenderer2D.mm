// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkMetalRenderer2D.mm — rendu 2D de NKCanvas sur Metal (Objective-C++, ARC)
//
// Un seul couple de nuanceurs MSL (compile au lancement : pas de .metallib a
// livrer), huit etats de pipeline (un par NkBlendMode), une texture blanche
// 1x1 pour la geometrie sans texture, et un anneau de tampons protege par un
// semaphore : l'image N+3 attend que le GPU ait fini l'image N avant de
// reecrire son tampon.
// =============================================================================
#include "NkMetalRenderer2D.h"
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "NKCanvas/Backend/Metal/NkMetalContext.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkTextureBackend.h"
#include "NKCanvas/Renderer/Resources/NkShaderBackend.h"
#include "NKCanvas/Renderer/Targets/NkRenderTextureBackend.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKLogger/NkLog.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <dispatch/dispatch.h>
#include <cstring>

#define NK_MTL2D_LOG(...) logger.Infof("[NkMetal-2D] " __VA_ARGS__)
#define NK_MTL2D_ERR(...) logger.Errorf("[NkMetal-2D] " __VA_ARGS__)

namespace nkentseu {
	namespace renderer {

		// ── MSL ──────────────────────────────────────────────────────────────────
		// Meme calcul que le HLSL de DX11 : la projection (colonne-major, comme
		// NkView2D::ToProjectionMatrix) fois la position en pixels ; Y vers le
		// haut en NDC, viewport en haut-gauche, comme Metal nativement.
		static const char *kNkMetal2DMsl = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Nk2DIn  { float2 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; float4 col [[attribute(2)]]; };
struct Nk2DOut { float4 pos [[position]]; float2 uv; float4 col; };
vertex Nk2DOut nk2d_vs(Nk2DIn v [[stage_in]], constant float4x4 &proj [[buffer(1)]]) {
    Nk2DOut o;
    o.pos = proj * float4(v.pos, 0.0, 1.0);
    o.uv  = v.uv;
    o.col = v.col;
    return o;
}
fragment float4 nk2d_fs(Nk2DOut i [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {
    return tex.sample(smp, i.uv) * i.col;
}
)MSL";

		// ── Registre des textures (appels statiques de NkTextureBackend) ─────────
		// Identifiant 1-based (0 = invalide), comme DX11. Les objets Metal sont
		// retenus par CFBridgingRetain et rendus a la destruction.
		struct NkMetal2DTexture {
				void *tex = nullptr;	 // id<MTLTexture>
				void *sampler = nullptr; // id<MTLSamplerState>
				NkTextureFilter filter = NkTextureFilter::NK_LINEAR;
				NkTextureWrap wrap = NkTextureWrap::NK_CLAMP;
				uint32 width = 0, height = 0;
				bool vivante = false;
		};

		static struct {
				id<MTLDevice> device = nil;
				NkVector<NkMetal2DTexture> entrees; // index = id - 1
		} gMtl2D;

		static NkMetal2DTexture *Mtl2DEntree(uint32 ident) {
			if (ident == 0 || ident > gMtl2D.entrees.Size())
				return nullptr;
			NkMetal2DTexture *e = &gMtl2D.entrees[ident - 1];
			return e->vivante ? e : nullptr;
		}

		static id<MTLSamplerState> Mtl2DSampler(NkTextureFilter f, NkTextureWrap w) {
			if (!gMtl2D.device)
				return nil;
			MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
			const MTLSamplerMinMagFilter mm =
				(f == NkTextureFilter::NK_NEAREST) ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
			sd.minFilter = mm;
			sd.magFilter = mm;
			sd.mipFilter = MTLSamplerMipFilterNotMipmapped;
			MTLSamplerAddressMode a = MTLSamplerAddressModeClampToEdge;
			if (w == NkTextureWrap::NK_REPEAT)
				a = MTLSamplerAddressModeRepeat;
			else if (w == NkTextureWrap::NK_MIRROR_REPEAT)
				a = MTLSamplerAddressModeMirrorRepeat;
			sd.sAddressMode = a;
			sd.tAddressMode = a;
			sd.rAddressMode = a;
			return [gMtl2D.device newSamplerStateWithDescriptor:sd];
		}

		static void Mtl2DRefaireSampler(NkMetal2DTexture *e) {
			if (!e)
				return;
			if (e->sampler)
				CFBridgingRelease(e->sampler);
			id<MTLSamplerState> s = Mtl2DSampler(e->filter, e->wrap);
			e->sampler = s ? (void *)CFBridgingRetain(s) : nullptr;
		}

		static void Mtl2DEcrire(id<MTLTexture> tex, uint32 x, uint32 y, uint32 w, uint32 h, const uint8 *rgba) {
			if (!tex || !rgba || w == 0 || h == 0)
				return;
			[tex replaceRegion:MTLRegionMake2D(x, y, w, h) mipmapLevel:0 withBytes:rgba bytesPerRow:(NSUInteger)w * 4u];
		}

		static uint32 Mtl2DCreer(uint32 w, uint32 h, const uint8 *rgba) {
			if (!gMtl2D.device || w == 0 || h == 0)
				return 0;
			MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																						   width:w
																						  height:h
																					   mipmapped:NO];
			td.usage = MTLTextureUsageShaderRead;
			// Stockage par defaut (gere sur macOS, partage sur iOS) : replaceRegion
			// y ecrit directement, sans tampon intermediaire ni attente.
			id<MTLTexture> tex = [gMtl2D.device newTextureWithDescriptor:td];
			if (!tex)
				return 0;
			Mtl2DEcrire(tex, 0, 0, w, h, rgba);

			NkMetal2DTexture e;
			e.tex = (void *)CFBridgingRetain(tex);
			e.width = w;
			e.height = h;
			e.vivante = true;
			Mtl2DRefaireSampler(&e);
			gMtl2D.entrees.PushBack(e);
			return (uint32)gMtl2D.entrees.Size();
		}

		static void Mtl2DMettreAJour(uint32 ident, uint32 x, uint32 y, uint32 w, uint32 h, const uint8 *rgba) {
			NkMetal2DTexture *e = Mtl2DEntree(ident);
			if (!e || !e->tex)
				return;
			// La police de NKGui passe par Create PUIS Update : sans Update, l'atlas
			// restait vide et le texte invisible.
			Mtl2DEcrire((__bridge id<MTLTexture>)e->tex, x, y, w, h, rgba);
		}

		static void Mtl2DDetruire(uint32 ident) {
			NkMetal2DTexture *e = Mtl2DEntree(ident);
			if (!e)
				return;
			if (e->sampler)
				CFBridgingRelease(e->sampler);
			if (e->tex)
				CFBridgingRelease(e->tex);
			*e = NkMetal2DTexture{};
		}

		static void Mtl2DFiltre(uint32 ident, NkTextureFilter f) {
			if (NkMetal2DTexture *e = Mtl2DEntree(ident)) {
				e->filter = f;
				Mtl2DRefaireSampler(e);
			}
		}

		static void Mtl2DRepetition(uint32 ident, NkTextureWrap w) {
			if (NkMetal2DTexture *e = Mtl2DEntree(ident)) {
				e->wrap = w;
				Mtl2DRefaireSampler(e);
			}
		}

		// =============================================================================
		NkMetalRenderer2D::~NkMetalRenderer2D() {
			if (mIsValid)
				Shutdown();
		}

		bool NkMetalRenderer2D::Initialize(NkIGraphicsContext *ctx) {
			if (mIsValid)
				return false;
			if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_METAL) {
				NK_MTL2D_ERR("contexte Metal requis\n");
				return false;
			}
			NkMetalContextData *d = NkNativeContext::Metal(ctx);
			if (!d || !d->device || !d->layer) {
				NK_MTL2D_ERR("donnees du contexte Metal invalides\n");
				return false;
			}
			mCtx = ctx;
			gMtl2D.device = (__bridge id<MTLDevice>)d->device;

			if (!CreerPipelines())
				return false;

			// Texture blanche 1x1 : la geometrie sans texture (NkTexture::GetWhiteTexture
			// rend nullptr) l'echantillonne.
			{
				MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																							   width:1
																							  height:1
																						   mipmapped:NO];
				td.usage = MTLTextureUsageShaderRead;
				id<MTLTexture> blanche = [gMtl2D.device newTextureWithDescriptor:td];
				const uint32 blanc = 0xFFFFFFFFu;
				Mtl2DEcrire(blanche, 0, 0, 1, 1, (const uint8 *)&blanc);
				mTextureBlanche = blanche ? (void *)CFBridgingRetain(blanche) : nullptr;
				id<MTLSamplerState> s = Mtl2DSampler(NkTextureFilter::NK_LINEAR, NkTextureWrap::NK_CLAMP);
				mSamplerBlanc = s ? (void *)CFBridgingRetain(s) : nullptr;
			}

			{
				MTLDepthStencilDescriptor *dsd = [[MTLDepthStencilDescriptor alloc] init];
				dsd.depthCompareFunction = MTLCompareFunctionAlways;
				dsd.depthWriteEnabled = NO;
				id<MTLDepthStencilState> dss = [gMtl2D.device newDepthStencilStateWithDescriptor:dsd];
				mProfondeurCoupee = dss ? (void *)CFBridgingRetain(dss) : nullptr;
			}

			dispatch_semaphore_t sem = dispatch_semaphore_create(kImagesEnVol);
			mSemaphore = (void *)CFBridgingRetain(sem);

			// Vue et viewport initiaux : la taille de la surface en PIXELS.
			const NkContextInfo info = ctx->GetInfo();
			const uint32 W = info.windowWidth > 0 ? info.windowWidth : (d->width > 0 ? d->width : 800);
			const uint32 H = info.windowHeight > 0 ? info.windowHeight : (d->height > 0 ? d->height : 600);
			mDefaultView.center = {W * 0.5f, H * 0.5f};
			mDefaultView.size = {(float32)W, (float32)H};
			mCurrentView = mDefaultView;
			mViewport = {0, 0, (int32)W, (int32)H};
			float32 proj[16];
			mCurrentView.ToProjectionMatrix(proj);
			UploadProjection(proj);

			{
				NkTextureBackend backend{};
				backend.Create = &Mtl2DCreer;
				backend.Update = &Mtl2DMettreAJour;
				backend.Destroy = &Mtl2DDetruire;
				backend.SetFilter = &Mtl2DFiltre;
				backend.SetWrap = &Mtl2DRepetition;
				NkTextureSetBackend(backend);
			}
			// Pas encore de nuanceurs utilisateur ni de textures de rendu en Metal :
			// les deux le DISENT au lieu de ne rien dessiner en silence.
			NkShaderInstallUnsupportedBackend("Metal");
			NkRenderTextureInstallUnsupportedBackend("Metal");

			mIsValid = true;
			NK_MTL2D_LOG("pret (%ux%u pixels, %s)\n", W, H, [gMtl2D.device.name UTF8String]);
			return true;
		}

		bool NkMetalRenderer2D::CreerPipelines() {
			NSError *err = nil;
			id<MTLLibrary> lib = [gMtl2D.device newLibraryWithSource:[NSString stringWithUTF8String:kNkMetal2DMsl]
															  options:nil
																error:&err];
			if (!lib) {
				NK_MTL2D_ERR("MSL : %s\n", err ? [err.localizedDescription UTF8String] : "?");
				return false;
			}
			id<MTLFunction> vs = [lib newFunctionWithName:@"nk2d_vs"];
			id<MTLFunction> fs = [lib newFunctionWithName:@"nk2d_fs"];
			if (!vs || !fs) {
				NK_MTL2D_ERR("nk2d_vs / nk2d_fs introuvables\n");
				return false;
			}

			// NkVertex2D : x, y, u, v (float), r, g, b, a (octets) -- 20 octets.
			MTLVertexDescriptor *vd = [[MTLVertexDescriptor alloc] init];
			vd.attributes[0].format = MTLVertexFormatFloat2;
			vd.attributes[0].offset = 0;
			vd.attributes[0].bufferIndex = 0;
			vd.attributes[1].format = MTLVertexFormatFloat2;
			vd.attributes[1].offset = 8;
			vd.attributes[1].bufferIndex = 0;
			vd.attributes[2].format = MTLVertexFormatUChar4Normalized;
			vd.attributes[2].offset = 16;
			vd.attributes[2].bufferIndex = 0;
			vd.layouts[0].stride = sizeof(NkVertex2D);
			vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

			NkMetalContextData *d = NkNativeContext::Metal(mCtx);
			CAMetalLayer *layer = (__bridge CAMetalLayer *)d->layer;

			// Un pipeline par mode de fusion, dans l'ordre de NkBlendMode. Memes
			// facteurs que DX11 (NkDX11Renderer2D::CreateStates).
			struct Fusion {
					bool actif;
					MTLBlendFactor src, dst;
					MTLBlendOperation op;
					MTLBlendFactor srcA, dstA;
			};
			const Fusion fusions[kModesDeFusion] = {
				/* NK_ALPHA */ {true, MTLBlendFactorSourceAlpha, MTLBlendFactorOneMinusSourceAlpha, MTLBlendOperationAdd,
								MTLBlendFactorOne, MTLBlendFactorOneMinusSourceAlpha},
				/* NK_ADD */ {true, MTLBlendFactorSourceAlpha, MTLBlendFactorOne, MTLBlendOperationAdd, MTLBlendFactorOne,
							  MTLBlendFactorOne},
				/* NK_MULTIPLY */ {true, MTLBlendFactorDestinationColor, MTLBlendFactorZero, MTLBlendOperationAdd,
								   MTLBlendFactorOne, MTLBlendFactorZero},
				/* NK_NONE */ {false, MTLBlendFactorOne, MTLBlendFactorZero, MTLBlendOperationAdd, MTLBlendFactorOne,
							   MTLBlendFactorZero},
				/* NK_SCREEN */ {true, MTLBlendFactorOne, MTLBlendFactorOneMinusSourceColor, MTLBlendOperationAdd,
								 MTLBlendFactorOne, MTLBlendFactorOneMinusSourceAlpha},
				/* NK_DARKEN */ {true, MTLBlendFactorOne, MTLBlendFactorOne, MTLBlendOperationMin, MTLBlendFactorOne,
								 MTLBlendFactorOneMinusSourceAlpha},
				/* NK_LIGHTEN */ {true, MTLBlendFactorOne, MTLBlendFactorOne, MTLBlendOperationMax, MTLBlendFactorOne,
								  MTLBlendFactorOneMinusSourceAlpha},
				/* NK_PLUS_LIGHTER */ {true, MTLBlendFactorOne, MTLBlendFactorOne, MTLBlendOperationAdd,
									   MTLBlendFactorOne, MTLBlendFactorOneMinusSourceAlpha},
			};

			for (uint32 i = 0; i < kModesDeFusion; ++i) {
				MTLRenderPipelineDescriptor *pd = [[MTLRenderPipelineDescriptor alloc] init];
				pd.vertexFunction = vs;
				pd.fragmentFunction = fs;
				pd.vertexDescriptor = vd;
				pd.sampleCount = d->sampleCount > 0 ? d->sampleCount : 1;
				// La passe du contexte : couleur au format de la CAMetalLayer, et
				// une profondeur Depth32Float (toujours creee par NkMetalContext).
				pd.colorAttachments[0].pixelFormat = layer.pixelFormat;
				pd.depthAttachmentPixelFormat = d->depthTexture ? MTLPixelFormatDepth32Float : MTLPixelFormatInvalid;
				const Fusion &f = fusions[i];
				pd.colorAttachments[0].blendingEnabled = f.actif ? YES : NO;
				pd.colorAttachments[0].sourceRGBBlendFactor = f.src;
				pd.colorAttachments[0].destinationRGBBlendFactor = f.dst;
				pd.colorAttachments[0].rgbBlendOperation = f.op;
				pd.colorAttachments[0].sourceAlphaBlendFactor = f.srcA;
				pd.colorAttachments[0].destinationAlphaBlendFactor = f.dstA;
				pd.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
				id<MTLRenderPipelineState> ps = [gMtl2D.device newRenderPipelineStateWithDescriptor:pd error:&err];
				if (!ps) {
					NK_MTL2D_ERR("pipeline %u : %s\n", i, err ? [err.localizedDescription UTF8String] : "?");
					return false;
				}
				mPipelines[i] = (void *)CFBridgingRetain(ps);
			}
			return true;
		}

		void NkMetalRenderer2D::Shutdown() {
			if (!mIsValid)
				return;
			// Attendre que le GPU ait rendu tous les tampons de l'anneau.
			if (mSemaphore) {
				dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)mSemaphore;
				for (uint32 i = 0; i < kImagesEnVol; ++i)
					dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)NSEC_PER_SEC));
				for (uint32 i = 0; i < kImagesEnVol; ++i)
					dispatch_semaphore_signal(sem);
			}
			{
				NkTextureBackend vide{};
				NkTextureSetBackend(vide);
			}
			for (usize i = 0; i < gMtl2D.entrees.Size(); ++i)
				Mtl2DDetruire((uint32)(i + 1));
			gMtl2D.entrees.Clear();
			gMtl2D.device = nil;

			for (uint32 i = 0; i < kImagesEnVol; ++i) {
				if (mAnneau[i])
					CFBridgingRelease(mAnneau[i]);
				mAnneau[i] = nullptr;
			}
			for (uint32 i = 0; i < kModesDeFusion; ++i) {
				if (mPipelines[i])
					CFBridgingRelease(mPipelines[i]);
				mPipelines[i] = nullptr;
			}
			if (mProfondeurCoupee)
				CFBridgingRelease(mProfondeurCoupee);
			if (mTextureBlanche)
				CFBridgingRelease(mTextureBlanche);
			if (mSamplerBlanc)
				CFBridgingRelease(mSamplerBlanc);
			if (mSemaphore)
				CFBridgingRelease(mSemaphore);
			mProfondeurCoupee = mTextureBlanche = mSamplerBlanc = mSemaphore = nullptr;
			mIsValid = false;
			NK_MTL2D_LOG("arret\n");
		}

		void NkMetalRenderer2D::Clear(const NkColor2D &col) {
			// La couleur arrive au contexte par NkRenderWindow::Clear (SetClearColor)
			// avant Begin ; on la repose pour qui appelle Clear seul.
			if (mCtx) {
				const math::NkColorF c = col.ToColorF();
				mCtx->SetClearColor(c.r, c.g, c.b, c.a);
			}
		}

		void NkMetalRenderer2D::BeginBackend() {
			mImageOuverte = false;
			if (!mIsValid || !mCtx)
				return;
			// Au plus kImagesEnVol images en vol : l'anneau de l'image N+3 n'est
			// reecrit qu'une fois l'image N rendue par le GPU.
			// Borne a une seconde : une image ouverte mais jamais presentee (Display
			// oublie) ne doit pas bloquer l'application pour toujours.
			dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)mSemaphore;
			dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)NSEC_PER_SEC));
			if (!mCtx->BeginFrame()) {
				dispatch_semaphore_signal(sem); // pas d'image : rien a attendre
				return;
			}
			mImageOuverte = true;
			mCase = (mCase + 1) % kImagesEnVol;
			mDecalage = 0;

			NkMetalContextData *d = NkNativeContext::Metal(mCtx);
			id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)d->currentEncoder;
			[enc setDepthStencilState:(__bridge id<MTLDepthStencilState>)mProfondeurCoupee];
			[enc setCullMode:MTLCullModeNone];
		}

		void NkMetalRenderer2D::EndBackend() {
			if (!mImageOuverte)
				return;
			NkMetalContextData *d = NkNativeContext::Metal(mCtx);
			id<MTLCommandBuffer> cmdb = (__bridge id<MTLCommandBuffer>)d->commandBuffer;
			dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)mSemaphore;
			[cmdb addCompletedHandler:^(id<MTLCommandBuffer>) {
			  dispatch_semaphore_signal(sem);
			}];
			mCtx->EndFrame();
			mImageOuverte = false;
		}

		bool NkMetalRenderer2D::AssurerPlace(uint64 octets) {
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)mAnneau[mCase];
			const uint64 besoin = mDecalage + octets;
			if (buf && buf.length >= besoin)
				return true;
			// Trop petit : un tampon plus grand POUR LA SUITE de cette image. Les
			// commandes deja encodees retiennent l'ancien (Metal retient les
			// ressources d'un command buffer) : le relacher ici est sans danger.
			uint64 taille = buf ? (uint64)buf.length * 2u : (uint64)(8u << 20);
			while (taille < octets)
				taille *= 2u;
			id<MTLBuffer> neuf = [gMtl2D.device newBufferWithLength:(NSUInteger)taille
															 options:MTLResourceStorageModeShared |
																	 MTLResourceCPUCacheModeWriteCombined];
			if (!neuf)
				return false;
			if (mAnneau[mCase])
				CFBridgingRelease(mAnneau[mCase]);
			mAnneau[mCase] = (void *)CFBridgingRetain(neuf);
			mDecalage = 0;
			return true;
		}

		void NkMetalRenderer2D::SubmitBatches(const NkBatchGroup *groups, uint32 groupCount, const NkVertex2D *verts,
											  uint32 vCount, const uint32 *idx, uint32 iCount) {
			if (!mIsValid || !mImageOuverte || !vCount || !iCount)
				return;
			NkMetalContextData *d = NkNativeContext::Metal(mCtx);
			id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)d->currentEncoder;
			if (!enc)
				return;

			// Sommets puis indices, chacun aligne sur 256 octets.
			auto aligner = [](uint64 v) { return (v + 255u) & ~(uint64)255u; };
			const uint64 octetsV = (uint64)vCount * sizeof(NkVertex2D);
			const uint64 octetsI = (uint64)iCount * sizeof(uint32);
			mDecalage = aligner(mDecalage);
			if (!AssurerPlace(aligner(octetsV) + octetsI + 256u))
				return;
			id<MTLBuffer> buf = (__bridge id<MTLBuffer>)mAnneau[mCase];
			const uint64 decV = aligner(mDecalage);
			const uint64 decI = aligner(decV + octetsV);
			memcpy((uint8 *)buf.contents + decV, verts, (size_t)octetsV);
			memcpy((uint8 *)buf.contents + decI, idx, (size_t)octetsI);
			mDecalage = decI + octetsI;

			[enc setVertexBuffer:buf offset:(NSUInteger)decV atIndex:0];
			[enc setVertexBytes:mProj length:sizeof(mProj) atIndex:1];

			// Viewport (pixels, origine haut-gauche) et scissor : Metal ARRETE le
			// processus sur un scissor qui deborde de la cible -- on rogne a la
			// taille de la surface.
			const int32 cibleW = (int32)(d->width > 0 ? d->width : (uint32)mViewport.width);
			const int32 cibleH = (int32)(d->height > 0 ? d->height : (uint32)mViewport.height);
			MTLViewport vp{(double)mViewport.left, (double)mViewport.top, (double)mViewport.width,
						   (double)mViewport.height, 0.0, 1.0};
			[enc setViewport:vp];
			NkRect2i sc = mScissorActif ? mScissor : mViewport;
			int32 x0 = sc.x < 0 ? 0 : sc.x, y0 = sc.y < 0 ? 0 : sc.y;
			int32 x1 = sc.x + sc.width, y1 = sc.y + sc.height;
			if (x1 > cibleW)
				x1 = cibleW;
			if (y1 > cibleH)
				y1 = cibleH;
			if (x1 <= x0 || y1 <= y0)
				return; // tout est rogne : rien a dessiner
			MTLScissorRect rs{(NSUInteger)x0, (NSUInteger)y0, (NSUInteger)(x1 - x0), (NSUInteger)(y1 - y0)};
			[enc setScissorRect:rs];

			int32 dernierPipeline = -1;
			for (uint32 g = 0; g < groupCount; ++g) {
				const NkBatchGroup &grp = groups[g];
				if (grp.indexCount == 0)
					continue;
				int32 p = (int32)grp.blendMode;
				if (p < 0 || p >= (int32)kModesDeFusion)
					p = 0; // mode inconnu : alpha, comme les autres dorsaux
				if (p != dernierPipeline) {
					[enc setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)mPipelines[p]];
					dernierPipeline = p;
				}
				id<MTLTexture> tex = (__bridge id<MTLTexture>)mTextureBlanche;
				id<MTLSamplerState> smp = (__bridge id<MTLSamplerState>)mSamplerBlanc;
				if (grp.texture) {
					if (NkMetal2DTexture *e = Mtl2DEntree(grp.texture->GetGPUId())) {
						if (e->tex)
							tex = (__bridge id<MTLTexture>)e->tex;
						if (e->sampler)
							smp = (__bridge id<MTLSamplerState>)e->sampler;
					}
				}
				[enc setFragmentTexture:tex atIndex:0];
				[enc setFragmentSamplerState:smp atIndex:0];
				[enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								indexCount:grp.indexCount
								 indexType:MTLIndexTypeUInt32
							   indexBuffer:buf
						 indexBufferOffset:(NSUInteger)(decI + (uint64)grp.indexStart * sizeof(uint32))];
			}
		}

		void NkMetalRenderer2D::UploadProjection(const float32 proj[16]) {
			memcpy(mProj, proj, sizeof(mProj));
		}

		void NkMetalRenderer2D::ApplyScissor(bool enabled, const NkRect2i &rect) {
			mScissorActif = enabled;
			mScissor = rect;
		}

	} // namespace renderer
} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS || NKENTSEU_PLATFORM_IOS
