#pragma once
// =============================================================================
// NkVulkanRenderer2D.h — Vulkan 2D renderer backend
// Uses push constants for the projection matrix (no UBO needed for 2D).
// One descriptor set layout: binding 0 = combined image sampler.
// One pipeline per blend mode (4 pipelines max).
// Dynamic VBO/IBO via host-visible buffer (re-mapped each frame).
// =============================================================================
#include "NKContext/Renderer/Batch/NkBatchRenderer2D.h"
#include "NKContext/Graphics/Vulkan/NkVulkanContextData.h"
#include "NKLogger/NkLog.h"

#if NKENTSEU_HAS_VULKAN_HEADERS
#include <vulkan/vulkan.h>
#endif

namespace nkentseu {
    namespace renderer {

        #if NKENTSEU_HAS_VULKAN_HEADERS

            class NkVulkanRenderer2D final : public NkBatchRenderer2D {
            public:
                NkVulkanRenderer2D()  = default;
                ~NkVulkanRenderer2D() override { if (IsValid()) Shutdown(); }

                bool Initialize(NkIGraphicsContext* ctx) override;
                void Shutdown()                          override;
                bool IsValid()                   const   override { return mIsValid; }
                void Clear(const NkColor2D& col)         override;

                // Texture ops (called by NkTexture integration)
                static VkSampler GetDefaultSampler() { return sSampler; }

            protected:
                void BeginBackend()  override;
                void EndBackend()    override;
                void SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                const NkVertex2D* verts, uint32 vCount,
                                const uint32*     idx,   uint32 iCount) override;
                void UploadProjection(const float32 proj[16]) override;

            private:
                // Pipeline creation
                bool CreateDescriptorPool();
                bool CreateDescriptorSetLayout();
                bool CreatePipelineLayout();
                bool CreatePipelines();
                bool CreateBuffers();
                bool CreateWhiteTexture();
                bool CreateSampler();

                VkPipeline GetPipelineForBlend(NkBlendMode mode);

                void DestroyBuffers();

                // Helpers
                bool FindMemoryType(uint32 filter, VkMemoryPropertyFlags props, uint32& out);
                bool CreateBuffer_Internal(VkDeviceSize size, VkBufferUsageFlags usage,
                                        VkMemoryPropertyFlags props,
                                        VkBuffer& buf, VkDeviceMemory& mem);

                NkIGraphicsContext* mCtx     = nullptr;
                NkVulkanContextData* mVkData = nullptr;
                bool mIsValid = false;

                // Descriptor resources
                VkDescriptorPool      mDescPool   = VK_NULL_HANDLE;
                VkDescriptorSetLayout mSetLayout  = VK_NULL_HANDLE;
                VkPipelineLayout      mPipeLayout = VK_NULL_HANDLE;

                // One pipeline per blend mode
                VkPipeline mPipeAlpha    = VK_NULL_HANDLE;
                VkPipeline mPipeAdd      = VK_NULL_HANDLE;
                VkPipeline mPipeMul      = VK_NULL_HANDLE;
                VkPipeline mPipeNone     = VK_NULL_HANDLE;

                // Vertex / index buffers (host-visible, persistent map)
                VkBuffer       mVB     = VK_NULL_HANDLE;
                VkDeviceMemory mVBMem  = VK_NULL_HANDLE;
                VkBuffer       mIB     = VK_NULL_HANDLE;
                VkDeviceMemory mIBMem  = VK_NULL_HANDLE;
                void*          mVBMap  = nullptr;
                void*          mIBMap  = nullptr;

                // White 1x1 texture
                VkImage        mWhiteImage  = VK_NULL_HANDLE;
                VkDeviceMemory mWhiteMem    = VK_NULL_HANDLE;
                VkImageView    mWhiteView   = VK_NULL_HANDLE;
                VkDescriptorSet mWhiteSet   = VK_NULL_HANDLE;

                static VkSampler sSampler;   // shared sampler for all textures

                // Per-texture descriptor set cache
                struct TexDescEntry {
                    const NkTexture* texture = nullptr;
                    VkDescriptorSet  set     = VK_NULL_HANDLE;
                };
                NkVector<TexDescEntry> mTexDescCache;

                VkDescriptorSet GetOrCreateDescSet(const NkTexture* tex);

                // Push constants layout (projection = 64 bytes at offset 0)
                float32 mProjection[16] = {};
            };

        #else

            // Stub when Vulkan headers are not available
            class NkVulkanRenderer2D final : public NkBatchRenderer2D {
            public:
                bool Initialize(NkIGraphicsContext*) override {
                    logger.Errorf("[NkVkRenderer2D] Vulkan headers not available at build time");
                    return false;
                }
                void Shutdown()  override {}
                bool IsValid() const override { return false; }
                void Clear(const NkColor2D&) override {}
            protected:
                void SubmitBatches(const NkBatchGroup*, uint32, const NkVertex2D*, uint32, const uint32*, uint32) override {}
                void UploadProjection(const float32*) override {}
            };

        #endif // NKENTSEU_HAS_VULKAN_HEADERS

    } // namespace renderer
} // namespace nkentseu