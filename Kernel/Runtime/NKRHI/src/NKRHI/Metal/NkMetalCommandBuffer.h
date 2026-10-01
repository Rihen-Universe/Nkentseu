#pragma once
// =============================================================================
// NkMetalCommandBuffer.h — MTLCommandBuffer + encoders Metal
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Metal/NkMetalDevice.h" // NkMetalPassFormats
#ifdef NK_RHI_METAL_ENABLED
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace nkentseu {

	class NkMetalDevice;

	class NkMetalCommandBuffer final : public NkICommandBuffer {
		public:
			NkMetalCommandBuffer(NkMetalDevice *dev, NkCommandBufferType type);
			~NkMetalCommandBuffer() override;

			bool Begin() override;
			void End() override;
			void Reset() override;

			// Le MTLCommandBuffer est cree a Begin (ou a la premiere commande) et
			// rendu des qu'il est soumis : voir NkMetalCommandBuffer.mm.
			bool IsValid() const override {
				return mDev != nullptr;
			}

			NkCommandBufferType GetType() const override {
				return mType;
			}

			void CommitAndWait();
			void CommitAndPresent(void *drawable);

			bool BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb, const NkRect2D &area) override;
			void EndRenderPass() override;
			void SetViewport(const NkViewport &vp) override;
			void SetViewports(const NkViewport *vps, uint32 n) override;
			void SetScissor(const NkRect2D &r) override;
			void SetScissors(const NkRect2D *r, uint32 n) override;
			void BindGraphicsPipeline(NkPipelineHandle p) override;
			void BindComputePipeline(NkPipelineHandle p) override;
			void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32 *off, uint32 cnt) override;
			void PushConstants(NkShaderStage stages, uint32 offset, uint32 size, const void *data) override;

			void UpdateBuffer(NkBufferHandle buf, uint64 off, uint64 size, const void *data) override;

			// Clear dynamique (comme Vulkan) : la couleur/profondeur de la PROCHAINE
			// passe dont l'attachement est en NK_CLEAR.
			void SetClearColor(float32 r, float32 g, float32 b, float32 a = 1.f) override {
				mClearColor[0] = r;
				mClearColor[1] = g;
				mClearColor[2] = b;
				mClearColor[3] = a;
			}

			void SetClearDepth(float32 depth = 1.f, uint32 stencil = 0) override {
				mClearDepth = depth;
				mClearStencil = stencil;
			}

			void BindVertexBuffer(uint32 b, NkBufferHandle buf, uint64 off) override;
			void BindVertexBuffers(uint32 first, const NkBufferHandle *bufs, const uint64 *offs, uint32 n) override;
			void BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) override;
			// *Impl : les publiques comptent dans NkICommandBuffer (patron NVI).
			void DrawImpl(uint32 v, uint32 i, uint32 fv, uint32 fi) override;
			void DrawIndexedImpl(uint32 idx, uint32 inst, uint32 fi, int32 vo, uint32 fInst) override;
			void DrawIndirectImpl(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;
			void DrawIndexedIndirectImpl(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;
			void Dispatch(uint32 gx, uint32 gy, uint32 gz) override;
			void DispatchIndirect(NkBufferHandle buf, uint64 off) override;
			void CopyBuffer(NkBufferHandle s, NkBufferHandle d, const NkBufferCopyRegion &r) override;
			void CopyBufferToTexture(NkBufferHandle, NkTextureHandle, const NkBufferTextureCopyRegion &) override;
			void CopyTextureToBuffer(NkTextureHandle, NkBufferHandle, const NkBufferTextureCopyRegion &) override;
			void CopyTexture(NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion &r) override;
			void BlitTexture(NkTextureHandle, NkTextureHandle, const NkTextureCopyRegion &, NkFilter) override;

			void Barrier(const NkBufferBarrier *, uint32, const NkTextureBarrier *, uint32) override {
			} // Metal gère implicitement

			void GenerateMipmaps(NkTextureHandle tex, NkFilter f) override;
			void BeginDebugGroup(const char *name, float r, float g, float b) override;
			void EndDebugGroup() override;
			void InsertDebugLabel(const char *name) override;

		private:
			void EndCurrentEncoder();
			bool AssurerCmdBuf();
			void RendreCmdBuf();
			void AppliquerDescripteurs();

			NkMetalDevice *mDev = nullptr;
			NkCommandBufferType mType;
			void *mCmdBuf = nullptr;		 // id<MTLCommandBuffer>
			void *mRenderEncoder = nullptr;	 // id<MTLRenderCommandEncoder>
			void *mComputeEncoder = nullptr; // id<MTLComputeCommandEncoder>
			void *mBlitEncoder = nullptr;	 // id<MTLBlitCommandEncoder>

			// État courant pour les draw calls
			void *mCurrentIndexBuffer = nullptr; // id<MTLBuffer>
			uint64 mIndexOffset = 0;
			bool mIndexUint32 = true;
			MTLPrimitiveType mPrimitive = MTLPrimitiveTypeTriangle;

			float32 mClearColor[4] = {0.f, 0.f, 0.f, 1.f};
			float32 mClearDepth = 1.f;
			uint32 mClearStencil = 0;
			// Taille de la passe en cours : Metal refuse un scissor qui deborde.
			uint32 mPassW = 0, mPassH = 0;
			// Taille de groupe du pipeline compute lie (Dispatch).
			uint32 mTgX = 1, mTgY = 1, mTgZ = 1;
			// Formats de la passe en cours : choisissent la variante du pipeline.
			NkMetalPassFormats mFormats;
			bool mPipelineValide = false;
			// Ensembles de descripteurs lies, par index d'ensemble ; appliques au
			// prochain dessin / dispatch, filtres par ce que le pipeline lit.
			static constexpr uint32 kEnsembles = 8;
			uint64 mEnsembles[kEnsembles] = {};
			uint64 mPipelineCourant = 0;
			bool mDescripteursSales = false;
	};

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
