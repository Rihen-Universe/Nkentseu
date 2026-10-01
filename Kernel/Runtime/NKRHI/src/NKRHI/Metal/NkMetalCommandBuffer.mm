// =============================================================================
// NkMetalCommandBuffer.mm
// =============================================================================
#ifdef NK_RHI_METAL_ENABLED
#import "NkMetalCommandBuffer.h"
#import "NkMetalDevice.h"
#import <Metal/Metal.h>
#include <cstring>

#define RENDER_ENC ((__bridge id<MTLRenderCommandEncoder>)mRenderEncoder)
#define COMPUTE_ENC ((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder)
#define BLIT_ENC ((__bridge id<MTLBlitCommandEncoder>)mBlitEncoder)
#define CMD_BUF ((__bridge id<MTLCommandBuffer>)mCmdBuf)
#define DEV_MTL (mDev->MtlDevice())
#define QUEUE_MTL (mDev->MtlQueue())

namespace nkentseu {

	// (2026-09-30) UN MTLCommandBuffer N'EXISTE QU'ENTRE Begin ET SA SOUMISSION.
	// Avant, chaque NkMetalCommandBuffer en gardait un des sa construction, et
	// en recreait un a Reset : une file Metal n'en accorde qu'un nombre fini
	// (64 par defaut) avant de BLOQUER la creation suivante. Les tutoriels se
	// figeaient ainsi dans [queue commandBuffer] (pile `sample` de la CI) et ne
	// repondaient plus a « Quitter ».
	NkMetalCommandBuffer::NkMetalCommandBuffer(NkMetalDevice *dev, NkCommandBufferType type) : mDev(dev), mType(type) {
	}

	NkMetalCommandBuffer::~NkMetalCommandBuffer() {
		EndCurrentEncoder();
		RendreCmdBuf();
	}

	bool NkMetalCommandBuffer::AssurerCmdBuf() {
		if (mCmdBuf)
			return true;
		id<MTLCommandBuffer> cmd = [mDev->MtlQueue() commandBuffer];
		if (!cmd)
			return false;
		mCmdBuf = (__bridge_retained void *)cmd;
		return true;
	}

	void NkMetalCommandBuffer::RendreCmdBuf() {
		if (mCmdBuf) {
			CFRelease(mCmdBuf);
			mCmdBuf = nullptr;
		}
	}

	bool NkMetalCommandBuffer::Begin() {
		EndCurrentEncoder();
		RendreCmdBuf();
		mPipelineValide = false;
		return AssurerCmdBuf();
	}

	void NkMetalCommandBuffer::End() {
		EndCurrentEncoder();
	}

	void NkMetalCommandBuffer::Reset() {
		EndCurrentEncoder();
		RendreCmdBuf();
	}

	// Une erreur GPU (faute d'adresse, delai depasse) ne se voyait nulle part :
	// les premieres sont journalisees, avec le nom de l'erreur Metal.
	static void NkMtlSurveiller(id<MTLCommandBuffer> cb) {
		static int sJournalisees = 0;
		[cb addCompletedHandler:^(id<MTLCommandBuffer> fini) {
		  if (fini.error && sJournalisees < 8) {
			  ++sJournalisees;
			  printf("[NkRHI_Metal][ERR] command buffer en erreur : %s\n", [fini.error.localizedDescription UTF8String]);
		  }
		}];
	}

	void NkMetalCommandBuffer::EndCurrentEncoder() {
		if (mRenderEncoder) {
			[RENDER_ENC endEncoding];
			CFRelease(mRenderEncoder);
			mRenderEncoder = nullptr;
		}
		if (mComputeEncoder) {
			[((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) endEncoding];
			CFRelease(mComputeEncoder);
			mComputeEncoder = nullptr;
		}
		if (mBlitEncoder) {
			[BLIT_ENC endEncoding];
			CFRelease(mBlitEncoder);
			mBlitEncoder = nullptr;
		}
	}

	void NkMetalCommandBuffer::CommitAndWait() {
		EndCurrentEncoder();
		if (!mCmdBuf)
			return; // rien d'enregistre
		NkMtlSurveiller(CMD_BUF);
		[CMD_BUF commit];
		[CMD_BUF waitUntilCompleted];
		RendreCmdBuf();
	}

	void NkMetalCommandBuffer::CommitAndPresent(void *drawable) {
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		if (drawable) {
			id<CAMetalDrawable> d = (__bridge id<CAMetalDrawable>)drawable;
			[CMD_BUF presentDrawable:d];
		}
		NkMtlSurveiller(CMD_BUF);
		[CMD_BUF commit];
		RendreCmdBuf(); // Metal garde le command buffer soumis jusqu'a la fin
	}

	// =============================================================================
	// Render Pass
	// =============================================================================
	static MTLLoadAction NkMtlLoad(NkLoadOp op) {
		switch (op) {
			case NkLoadOp::NK_LOAD:
				return MTLLoadActionLoad;
			case NkLoadOp::NK_DONT_CARE:
				return MTLLoadActionDontCare;
			default:
				return MTLLoadActionClear;
		}
	}

	static MTLStoreAction NkMtlStore(NkStoreOp op) {
		return op == NkStoreOp::NK_DONT_CARE ? MTLStoreActionDontCare : MTLStoreActionStore;
	}

	// (2026-09-30) La passe suit enfin son NkRenderPassDesc : avant, TOUT
	// attachement etait efface (une passe NK_LOAD -- l'overlay 2D sur la scene --
	// effacait ce qui la precedait) et la profondeur jetee a la fin (l'atlas
	// d'ombre ne survivait pas a sa propre passe). La passe vient de `rpH`, sinon
	// de celle donnee a la creation du framebuffer (convention Vulkan de NKRHI) ;
	// les couleurs d'effacement, de SetClearColor/SetClearDepth (comme Vulkan).
	bool NkMetalCommandBuffer::BeginRenderPass(NkRenderPassHandle rpH, NkFramebufferHandle fbH,
											   const NkRect2D & /*area*/) {
		if (!fbH.IsValid() || !AssurerCmdBuf())
			return false;
		EndCurrentEncoder();

		auto *fb = mDev->GetFBO(fbH.id);
		if (!fb)
			return false;
		const NkMetalRenderPass *rp = mDev->GetRenderPass(rpH.IsValid() ? rpH.id : fb->renderPassId);
		const NkRenderPassDesc *rpd0 = rp ? &rp->desc : nullptr;

		MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
		mPassW = 0;
		mPassH = 0;
		mFormats = NkMetalPassFormats{};
		mPipelineValide = false;

		for (uint32 i = 0; i < fb->colorCount; i++) {
			id<MTLTexture> tex = (__bridge id<MTLTexture>)mDev->GetMTLTexture(fb->colorAttachments[i].id);
			rpd.colorAttachments[i].texture = tex;
			const bool aDesc = rpd0 && i < rpd0->colorAttachments.Size();
			rpd.colorAttachments[i].loadAction = aDesc ? NkMtlLoad(rpd0->colorAttachments[i].loadOp) : MTLLoadActionClear;
			rpd.colorAttachments[i].storeAction =
				aDesc ? NkMtlStore(rpd0->colorAttachments[i].storeOp) : MTLStoreActionStore;
			rpd.colorAttachments[i].clearColor =
				MTLClearColorMake(mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]);
			if (tex && mPassW == 0) {
				mPassW = (uint32)tex.width;
				mPassH = (uint32)tex.height;
			}
			if (tex && i < 8) {
				mFormats.color[i] = (uint32)tex.pixelFormat;
				mFormats.colorCount = i + 1;
				mFormats.samples = (uint32)tex.sampleCount;
			}
		}
		if (fb->depthAttachment.IsValid()) {
			id<MTLTexture> dtex = (__bridge id<MTLTexture>)mDev->GetMTLTexture(fb->depthAttachment.id);
			const bool aDesc = rpd0 && rpd0->hasDepth;
			rpd.depthAttachment.texture = dtex;
			rpd.depthAttachment.loadAction = aDesc ? NkMtlLoad(rpd0->depthAttachment.loadOp) : MTLLoadActionClear;
			rpd.depthAttachment.storeAction = aDesc ? NkMtlStore(rpd0->depthAttachment.storeOp) : MTLStoreActionStore;
			rpd.depthAttachment.clearDepth = mClearDepth;
			if (dtex) {
				mFormats.depth = (uint32)dtex.pixelFormat;
				if (mFormats.colorCount == 0)
					mFormats.samples = (uint32)dtex.sampleCount;
			}
			// Profondeur + stencil dans la MEME texture : le stencil doit etre
			// attache aussi, sinon la passe ne correspond pas au pipeline.
			if (dtex && (dtex.pixelFormat == MTLPixelFormatDepth32Float_Stencil8
#if TARGET_OS_OSX
						 || dtex.pixelFormat == MTLPixelFormatDepth24Unorm_Stencil8
#endif
						 )) {
				rpd.stencilAttachment.texture = dtex;
				rpd.stencilAttachment.loadAction = aDesc ? NkMtlLoad(rpd0->depthAttachment.stencilLoad) : MTLLoadActionClear;
				rpd.stencilAttachment.storeAction =
					aDesc ? NkMtlStore(rpd0->depthAttachment.stencilStore) : MTLStoreActionDontCare;
				rpd.stencilAttachment.clearStencil = mClearStencil;
				mFormats.stencil = (uint32)dtex.pixelFormat;
			}
			if (dtex && mPassW == 0) {
				mPassW = (uint32)dtex.width;
				mPassH = (uint32)dtex.height;
			}
		}

		id<MTLRenderCommandEncoder> enc = [CMD_BUF renderCommandEncoderWithDescriptor:rpd];
		if (!enc)
			return false;
		mRenderEncoder = (__bridge_retained void *)enc;
		mDescripteursSales = true; // un encodeur neuf n'a aucune liaison
		return true;
	}

	void NkMetalCommandBuffer::EndRenderPass() {
		if (mRenderEncoder) {
			[RENDER_ENC endEncoding];
			CFRelease(mRenderEncoder);
			mRenderEncoder = nullptr;
		}
	}

	// =============================================================================
	// Viewport & Scissor
	// =============================================================================
	// (2026-09-30) Les shaders Metal sortent du MEME SPIR-V que Vulkan, et le
	// renderer regle ses conventions pour Vulkan : il faut donc le viewport de
	// Vulkan. Vulkan retourne l'axe Y quand flipY (defaut : NDC Y vers le haut,
	// comme Metal nativement) et le laisse sinon (passes d'ombre : NDC Y vers le
	// bas). Metal fait donc l'inverse : hauteur positive si flipY, negative sinon.
	static MTLViewport NkMtlViewport(const NkViewport &vp) {
		if (vp.flipY)
			return MTLViewport{vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth};
		return MTLViewport{vp.x, vp.y + vp.height, vp.width, -vp.height, vp.minDepth, vp.maxDepth};
	}

	void NkMetalCommandBuffer::SetViewport(const NkViewport &vp) {
		if (!mRenderEncoder)
			return;
		[RENDER_ENC setViewport:NkMtlViewport(vp)];
	}

	void NkMetalCommandBuffer::SetViewports(const NkViewport *vps, uint32 n) {
		if (!mRenderEncoder || n == 0)
			return;
		// Metal 3+: setViewports:count:
		[RENDER_ENC setViewport:NkMtlViewport(vps[0])];
	}

	void NkMetalCommandBuffer::SetScissor(const NkRect2D &r) {
		if (!mRenderEncoder)
			return;
		// Metal ARRETE le processus sur un scissor qui deborde de la cible (Vulkan
		// et DX le rognent) : on le rogne a la taille de la passe.
		int64 x = r.x < 0 ? 0 : r.x, y = r.y < 0 ? 0 : r.y;
		int64 w = (int64)r.width - (x - r.x), h = (int64)r.height - (y - r.y);
		if (mPassW > 0) {
			if (x > (int64)mPassW)
				x = mPassW;
			if (y > (int64)mPassH)
				y = mPassH;
			if (x + w > (int64)mPassW)
				w = (int64)mPassW - x;
			if (y + h > (int64)mPassH)
				h = (int64)mPassH - y;
		}
		if (w < 0)
			w = 0;
		if (h < 0)
			h = 0;
		MTLScissorRect sc{(NSUInteger)x, (NSUInteger)y, (NSUInteger)w, (NSUInteger)h};
		[RENDER_ENC setScissorRect:sc];
	}

	void NkMetalCommandBuffer::SetScissors(const NkRect2D *r, uint32 /*n*/) {
		SetScissor(r[0]);
	}

	// =============================================================================
	// Pipeline
	// =============================================================================
	void NkMetalCommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
		if (!mRenderEncoder)
			return;
		auto *pipe = mDev->GetPipeline(p.id);
		mPipelineValide = false;
		mPipelineCourant = p.id;
		mDescripteursSales = true;
		if (!pipe)
			return;
		// La variante faite pour les formats de CETTE passe (cf. NkMetalPassFormats).
		void *rpso = mDev->ResolveRenderPipeline(p.id, mFormats);
		if (!rpso)
			return; // les dessins suivants sont sautes plutot que refuses par Metal
		mPipelineValide = true;
		[RENDER_ENC setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)rpso];
		if (pipe->dss)
			[RENDER_ENC setDepthStencilState:(__bridge id<MTLDepthStencilState>)pipe->dss];
		[RENDER_ENC setFrontFacingWinding:pipe->frontFaceCCW ? MTLWindingCounterClockwise : MTLWindingClockwise];
		MTLCullMode cull = pipe->cullMode == 0	 ? MTLCullModeNone
						   : pipe->cullMode == 1 ? MTLCullModeFront
												 : MTLCullModeBack;
		[RENDER_ENC setCullMode:cull];
		if (pipe->depthBiasConst != 0 || pipe->depthBiasSlope != 0)
			[RENDER_ENC setDepthBias:pipe->depthBiasConst slopeScale:pipe->depthBiasSlope clamp:pipe->depthBiasClamp];
		mPrimitive = (MTLPrimitiveType)pipe->primitive; // topologie du pipeline
	}

	void NkMetalCommandBuffer::BindComputePipeline(NkPipelineHandle p) {
		EndCurrentEncoder();
		auto *pipe = mDev->GetPipeline(p.id);
		mPipelineCourant = p.id;
		mDescripteursSales = true;
		if (!pipe || !pipe->cpso || !AssurerCmdBuf())
			return;
		id<MTLComputeCommandEncoder> enc = [CMD_BUF computeCommandEncoder];
		[enc setComputePipelineState:(__bridge id<MTLComputePipelineState>)pipe->cpso];
		mComputeEncoder = (__bridge_retained void *)enc;
		mTgX = pipe->tgX;
		mTgY = pipe->tgY;
		mTgZ = pipe->tgZ;
	}

	// =============================================================================
	// Descriptor Set
	// =============================================================================
	// (2026-10-01) Metal n'a qu'UNE table de ressources par etage : les ensembles
	// de Vulkan s'y superposent. Lier un ensemble des son BindDescriptorSet
	// ecrasait les slots d'un autre (binding 9 2D de l'ensemble materiau sur la
	// cubemap binding 9 de l'ensemble global : « incorrect type of texture »,
	// eclairage PBR faux). Les ensembles sont donc retenus, et appliques au
	// prochain dessin -- seulement les (ensemble, binding) que le nuanceur du
	// pipeline lit (« // nk_rsrc » du MSL). Pipeline sans liste (MSL a la main) :
	// tout est lie, comme avant.
	void NkMetalCommandBuffer::BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32 * /*off*/, uint32 /*cnt*/) {
		if (idx >= kEnsembles)
			return;
		mEnsembles[idx] = set.id;
		mDescripteursSales = true;
	}

	void NkMetalCommandBuffer::AppliquerDescripteurs() {
		if (!mDescripteursSales)
			return;
		mDescripteursSales = false;
		const NkMetalPipeline *pipe = mPipelineCourant ? mDev->GetPipeline(mPipelineCourant) : nullptr;
		const uint32 nbLus = pipe ? pipe->ressources.Size() : 0;
		for (uint32 e = 0; e < kEnsembles; ++e) {
			if (!mEnsembles[e])
				continue;
			const NkMetalDescSet *ds = mDev->GetDescSet(mEnsembles[e]);
			if (!ds)
				continue;
			for (auto &b : ds->bindings) {
				if (nbLus > 0) {
					const uint32 cle = (e << 16) | (b.slot & 0xFFFFu);
					bool lu = false;
					for (uint32 k = 0; k < nbLus && !lu; ++k)
						lu = pipe->ressources[k] == cle;
					if (!lu)
						continue;
				}
				// Une entree de binding N va en buffer(N) / texture(N) / sampler(N)
				// (NkMslConventions.h). Au-dela des tables de Metal, le shader a recu
				// un sampler constexpr (SpirvToMsl) : on ne lie pas -- Metal
				// arreterait le processus sur un index hors table.
				uint32 slot = b.slot; // UINT est un type Windows, indisponible sur Apple/clang
				if (b.bufId && slot < kNkMslVertexBufferBase) {
				id<MTLBuffer> buf = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(b.bufId);
				const NSUInteger dec = (NSUInteger)b.offset;
				if (mRenderEncoder) {
					[RENDER_ENC setVertexBuffer:buf offset:dec atIndex:slot];
					[RENDER_ENC setFragmentBuffer:buf offset:dec atIndex:slot];
				}
				if (mComputeEncoder)
					[((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setBuffer:buf offset:dec atIndex:slot];
			}
			if (b.texId && slot < kNkMslMaxTextureSlots) {
				id<MTLTexture> tex = (__bridge id<MTLTexture>)mDev->GetMTLTexture(b.texId);
				if (mRenderEncoder) {
					[RENDER_ENC setVertexTexture:tex atIndex:slot];
					[RENDER_ENC setFragmentTexture:tex atIndex:slot];
				}
				if (mComputeEncoder)
					[((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setTexture:tex atIndex:slot];
			}
			if (b.sampId && slot < kNkMslMaxSamplerSlots) {
				id<MTLSamplerState> ss = (__bridge id<MTLSamplerState>)mDev->GetMTLSampler(b.sampId);
				if (mRenderEncoder) {
					[RENDER_ENC setVertexSamplerState:ss atIndex:slot];
					[RENDER_ENC setFragmentSamplerState:ss atIndex:slot];
				}
				if (mComputeEncoder)
					[((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setSamplerState:ss atIndex:slot];
			}
			}
		}
	}

	// =============================================================================
	// Push Constants — via setVertexBytes / setFragmentBytes
	// =============================================================================
	void NkMetalCommandBuffer::PushConstants(NkShaderStage stages, uint32 /*offset*/, uint32 size, const void *data) {
		uint32 slot = kNkMslPushConstantBuffer; // buffer(30), cf. NkMslConventions.h (UINT = type Windows)
		if (!mRenderEncoder && !mComputeEncoder)
			return;
		bool doVert = (uint32)stages & (uint32)NkShaderStage::NK_VERTEX;
		bool doFrag = (uint32)stages & (uint32)NkShaderStage::NK_FRAGMENT;
		bool doComp = (uint32)stages & (uint32)NkShaderStage::NK_COMPUTE;
		if (mRenderEncoder) {
			if (doVert || !doFrag)
				[RENDER_ENC setVertexBytes:data length:size atIndex:slot];
			if (doFrag || !doVert)
				[RENDER_ENC setFragmentBytes:data length:size atIndex:slot];
		}
		if (mComputeEncoder && doComp)
			[((__bridge id<MTLComputeCommandEncoder>)mComputeEncoder) setBytes:data length:size atIndex:slot];
	}

	// =============================================================================
	// Vertex / Index
	// =============================================================================
	void NkMetalCommandBuffer::BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 off) {
		if (!mRenderEncoder)
			return;
		id<MTLBuffer> b = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
		// buffer(26 + liaison) : NkMslConventions.h, et le vertex descriptor du pipeline.
		[RENDER_ENC setVertexBuffer:b offset:(NSUInteger)off atIndex:kNkMslVertexBufferBase + binding];
	}

	void NkMetalCommandBuffer::BindVertexBuffers(uint32 first, const NkBufferHandle *bufs, const uint64 *offs,
												 uint32 n) {
		for (uint32 i = 0; i < n; i++)
			BindVertexBuffer(first + i, bufs[i], offs[i]);
	}

	void NkMetalCommandBuffer::BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) {
		mCurrentIndexBuffer = mDev->GetMTLBuffer(buf.id);
		mIndexOffset = off;
		mIndexUint32 = (fmt == NkIndexFormat::NK_UINT32);
	}

	// =============================================================================
	// Draw
	// =============================================================================
	void NkMetalCommandBuffer::DrawImpl(uint32 vtx, uint32 inst, uint32 firstVtx, uint32 firstInst) {
		if (!mRenderEncoder || !mPipelineValide)
			return;
		AppliquerDescripteurs();
		[RENDER_ENC drawPrimitives:mPrimitive
					   vertexStart:firstVtx
					   vertexCount:vtx
					 instanceCount:inst
					  baseInstance:firstInst];
	}

	void NkMetalCommandBuffer::DrawIndexedImpl(uint32 idx, uint32 inst, uint32 firstIdx, int32 vtxOff,
											   uint32 firstInst) {
		if (!mRenderEncoder || !mCurrentIndexBuffer || !mPipelineValide)
			return;
		AppliquerDescripteurs();
		id<MTLBuffer> ib = (__bridge id<MTLBuffer>)mCurrentIndexBuffer;
		MTLIndexType it = mIndexUint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
		NSUInteger stride = mIndexUint32 ? 4 : 2;
		NSUInteger offset = mIndexOffset + firstIdx * stride;

		[RENDER_ENC drawIndexedPrimitives:mPrimitive
							   indexCount:idx
								indexType:it
							  indexBuffer:ib
						indexBufferOffset:offset
							instanceCount:inst
							   baseVertex:vtxOff
							 baseInstance:firstInst];
	}

	void NkMetalCommandBuffer::DrawIndirectImpl(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) {
		if (!mRenderEncoder)
			return;
		AppliquerDescripteurs();
		id<MTLBuffer> b = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
		for (uint32 i = 0; i < cnt; i++)
			[RENDER_ENC drawPrimitives:mPrimitive indirectBuffer:b indirectBufferOffset:off + i * stride];
	}

	void NkMetalCommandBuffer::DrawIndexedIndirectImpl(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) {
		if (!mRenderEncoder || !mCurrentIndexBuffer)
			return;
		AppliquerDescripteurs();
		id<MTLBuffer> ib = (__bridge id<MTLBuffer>)mCurrentIndexBuffer;
		id<MTLBuffer> ab = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
		MTLIndexType it = mIndexUint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
		for (uint32 i = 0; i < cnt; i++)
			[RENDER_ENC drawIndexedPrimitives:mPrimitive
									indexType:it
								  indexBuffer:ib
							indexBufferOffset:mIndexOffset
							   indirectBuffer:ab
						 indirectBufferOffset:off + i * stride];
	}

	// =============================================================================
	// Compute
	// =============================================================================
	void NkMetalCommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
		if (!mComputeEncoder)
			return;
		AppliquerDescripteurs();
		auto *enc = (__bridge id<MTLComputeCommandEncoder>)mComputeEncoder;
		[enc dispatchThreadgroups:MTLSizeMake(gx, gy, gz) threadsPerThreadgroup:MTLSizeMake(mTgX, mTgY, mTgZ)];
	}

	void NkMetalCommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
		if (!mComputeEncoder)
			return;
		AppliquerDescripteurs();
		auto *enc = (__bridge id<MTLComputeCommandEncoder>)mComputeEncoder;
		id<MTLBuffer> b = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
		[enc dispatchThreadgroupsWithIndirectBuffer:b
							   indirectBufferOffset:off
							  threadsPerThreadgroup:MTLSizeMake(mTgX, mTgY, mTgZ)];
	}

	// =============================================================================
	// UpdateBuffer (vkCmdUpdateBuffer) : c'etait un no-op
	// =============================================================================
	void NkMetalCommandBuffer::UpdateBuffer(NkBufferHandle buf, uint64 off, uint64 size, const void *data) {
		if (!data || size == 0)
			return;
		id<MTLBuffer> dst = (__bridge id<MTLBuffer>)mDev->GetMTLBuffer(buf.id);
		if (!dst || off + size > dst.length)
			return;
		if (dst.storageMode == MTLStorageModeShared) {
			memcpy((uint8 *)dst.contents + off, data, (size_t)size);
			return;
		}
		// Tampon prive : copie dans le flux de commandes, a sa place.
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		id<MTLBuffer> stage = [DEV_MTL newBufferWithBytes:data length:(NSUInteger)size
												  options:MTLResourceStorageModeShared];
		id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
		[blit copyFromBuffer:stage sourceOffset:0 toBuffer:dst destinationOffset:(NSUInteger)off size:(NSUInteger)size];
		[blit endEncoding];
	}

	// =============================================================================
	// Copies
	// =============================================================================
	void NkMetalCommandBuffer::CopyBuffer(NkBufferHandle src, NkBufferHandle dst, const NkBufferCopyRegion &r) {
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
		[blit copyFromBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(src.id)
				 sourceOffset:r.srcOffset
					 toBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(dst.id)
			destinationOffset:r.dstOffset
						 size:r.size];
		[blit endEncoding];
	}

	// bufferRowPitch = 0 veut dire « lignes jointives » (Vulkan, DX, NkOffscreenTarget) ;
	// Metal exige la vraie valeur : 0 donnait une copie invalide (la capture hors
	// ecran lisait du vide).
	static uint64 NkPasDeLigne(uint64 pas, uint32 largeur, uint32 octetsParPixel) {
		return pas > 0 ? pas : (uint64)largeur * octetsParPixel;
	}

	void NkMetalCommandBuffer::CopyBufferToTexture(NkBufferHandle src, NkTextureHandle dst,
												   const NkBufferTextureCopyRegion &r) {
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		const uint64 pas = NkPasDeLigne(r.bufferRowPitch, r.width, mDev->GetTextureBytesPerPixel(dst.id));
		id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
		[blit copyFromBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(src.id)
				   sourceOffset:r.bufferOffset
			  sourceBytesPerRow:pas
			sourceBytesPerImage:pas * r.height
					 sourceSize:MTLSizeMake(r.width, r.height, r.depth > 0 ? r.depth : 1)
					  toTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(dst.id)
			   destinationSlice:r.arrayLayer
			   destinationLevel:r.mipLevel
			  destinationOrigin:MTLOriginMake(r.x, r.y, r.z)];
		[blit endEncoding];
	}

	void NkMetalCommandBuffer::CopyTextureToBuffer(NkTextureHandle src, NkBufferHandle dst,
												   const NkBufferTextureCopyRegion &r) {
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		const uint64 pas = NkPasDeLigne(r.bufferRowPitch, r.width, mDev->GetTextureBytesPerPixel(src.id));
		id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
		[blit copyFromTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(src.id)
						 sourceSlice:r.arrayLayer
						 sourceLevel:r.mipLevel
						sourceOrigin:MTLOriginMake(r.x, r.y, r.z)
						  sourceSize:MTLSizeMake(r.width, r.height, r.depth > 0 ? r.depth : 1)
							toBuffer:(__bridge id<MTLBuffer>)mDev->GetMTLBuffer(dst.id)
				   destinationOffset:r.bufferOffset
			  destinationBytesPerRow:pas
			destinationBytesPerImage:pas * r.height];
		[blit endEncoding];
	}

	void NkMetalCommandBuffer::CopyTexture(NkTextureHandle src, NkTextureHandle dst, const NkTextureCopyRegion &r) {
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
		[blit copyFromTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(src.id)
				  sourceSlice:r.srcLayer
				  sourceLevel:r.srcMip
				 sourceOrigin:MTLOriginMake(r.srcX, r.srcY, r.srcZ)
				   sourceSize:MTLSizeMake(r.width, r.height, r.depth > 0 ? r.depth : 1)
					toTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(dst.id)
			 destinationSlice:r.dstLayer
			 destinationLevel:r.dstMip
			destinationOrigin:MTLOriginMake(r.dstX, r.dstY, r.dstZ)];
		[blit endEncoding];
	}

	void NkMetalCommandBuffer::BlitTexture(NkTextureHandle src, NkTextureHandle dst, const NkTextureCopyRegion &r,
										   NkFilter /*f*/) {
		CopyTexture(src, dst, r); // Metal n'a pas de blit avec filtre → copie directe
	}

	// =============================================================================
	// Mip generation
	// =============================================================================
	void NkMetalCommandBuffer::GenerateMipmaps(NkTextureHandle tex, NkFilter /*f*/) {
		EndCurrentEncoder();
		if (!AssurerCmdBuf())
			return;
		id<MTLBlitCommandEncoder> blit = [CMD_BUF blitCommandEncoder];
		[blit generateMipmapsForTexture:(__bridge id<MTLTexture>)mDev->GetMTLTexture(tex.id)];
		[blit endEncoding];
	}

	// =============================================================================
	// Debug
	// =============================================================================
	void NkMetalCommandBuffer::BeginDebugGroup(const char *name, float r, float g, float b) {
		NSString *s = [NSString stringWithUTF8String:name];
		if (mRenderEncoder)
			[RENDER_ENC pushDebugGroup:s];
		else
			[CMD_BUF pushDebugGroup:s];
		(void)r;
		(void)g;
		(void)b;
	}

	void NkMetalCommandBuffer::EndDebugGroup() {
		if (mRenderEncoder)
			[RENDER_ENC popDebugGroup];
		else
			[CMD_BUF popDebugGroup];
	}

	void NkMetalCommandBuffer::InsertDebugLabel(const char *name) {
		NSString *s = [NSString stringWithUTF8String:name];
		if (mRenderEncoder)
			[RENDER_ENC insertDebugSignpost:s];
		else
			[CMD_BUF addCompletedHandler:^(id<MTLCommandBuffer>) {
			  (void)s;
			}];
	}

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
