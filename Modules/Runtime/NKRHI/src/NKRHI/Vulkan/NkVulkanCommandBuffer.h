#pragma once
// =============================================================================
// NkVulkanCommandBuffer.h — Command Buffer Vulkan
// Enregistre directement dans un VkCommandBuffer natif.
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"

#include "NKPlatform/NkPlatformDetect.h"

#ifdef NK_RHI_VK_ENABLED
#include <vulkan/vulkan.h>

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    #ifdef NKENTSEU_PLATFORM_ANDROID
        #include <vulkan/vulkan_android.h>
        #include <android/native_window.h>
    #endif
#endif

namespace nkentseu {

    class NkVulkanDevice;

    class NkVulkanCommandBuffer final : public NkICommandBuffer {
        public:
            NkVulkanCommandBuffer(NkVulkanDevice* dev, NkCommandBufferType type);
            ~NkVulkanCommandBuffer() override;

            bool Begin()  override;
            void End()    override;
            void Reset()  override;
            bool IsValid()              const override { return mCmdBuf != VK_NULL_HANDLE; }
            NkCommandBufferType GetType() const override { return mType; }
            VkCommandBuffer GetVkCommandBuffer() const { return mCmdBuf; }

            bool BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb, const NkRect2D& area) override;
            void EndRenderPass() override;

            void SetViewport(const NkViewport& vp) override;
            void SetViewports(const NkViewport* vps, uint32 n) override;
            void SetScissor(const NkRect2D& r) override;
            void SetScissors(const NkRect2D* rects, uint32 n) override;
            void SetClearColor(float r, float g, float b, float a = 1.f) override {
                mClearColor[0]=r; mClearColor[1]=g; mClearColor[2]=b; mClearColor[3]=a;
            }
            void SetClearDepth(float depth = 1.f, uint32 stencil = 0) override {
                mClearDepth=depth; mClearStencil=stencil;
            }

            void BindGraphicsPipeline(NkPipelineHandle p) override;
            void BindComputePipeline (NkPipelineHandle p) override;
            void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
            void PushConstants(NkShaderStage stages, uint32 offset, uint32 size, const void* data) override;

            void BindVertexBuffer (uint32 binding, NkBufferHandle buf, uint64 offset) override;
            void BindVertexBuffers(uint32 first, const NkBufferHandle* bufs, const uint64* offs, uint32 n) override;
            void BindIndexBuffer  (NkBufferHandle buf, NkIndexFormat fmt, uint64 offset) override;

            void Draw             (uint32 vtx, uint32 inst, uint32 fv, uint32 fi)                  override;
            void DrawIndexed      (uint32 idx, uint32 inst, uint32 fi, int32 vo, uint32 fInst)     override;
            void DrawIndirect     (NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride)       override;
            void DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride)    override;

            void Dispatch         (uint32 gx, uint32 gy, uint32 gz)                                override;
            void DispatchIndirect (NkBufferHandle buf, uint64 off)                                  override;

            void CopyBuffer         (NkBufferHandle s, NkBufferHandle d, const NkBufferCopyRegion& r) override;
            void CopyBufferToTexture(NkBufferHandle s, NkTextureHandle d, const NkBufferTextureCopyRegion& r) override;
            void CopyTextureToBuffer(NkTextureHandle s, NkBufferHandle d, const NkBufferTextureCopyRegion& r) override;
            void CopyTexture        (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r) override;
            void BlitTexture        (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r, NkFilter f) override;

            void Barrier(const NkBufferBarrier* bb, uint32 bc, const NkTextureBarrier* tb, uint32 tc) override;
            void GenerateMipmaps(NkTextureHandle tex, NkFilter f) override;

            void BeginDebugGroup(const char* name, float r, float g, float b) override;
            void EndDebugGroup() override;
            void InsertDebugLabel(const char* name) override;

        private:
            static VkImageAspectFlags AspectFor(NkGPUFormat fmt);
            VkImageLayout GetCurrentLayout(NkTextureHandle tex) const;

            NkVulkanDevice*       mDev  = nullptr;
            NkCommandBufferType mType;
            VkCommandBuffer    mCmdBuf = VK_NULL_HANDLE;
            VkCommandPool      mPool   = VK_NULL_HANDLE;
            bool               mRecording = false;
            VkPipelineLayout   mBoundLayout = VK_NULL_HANDLE;
            bool               mIsCompute   = false;
            float              mClearColor[4] = {0.f, 0.f, 0.f, 1.f};
            float              mClearDepth    = 1.f;
            uint32             mClearStencil  = 0;
    };

} // namespace nkentseu
#endif // NK_RHI_VK_ENABLED
