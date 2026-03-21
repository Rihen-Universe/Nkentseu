#pragma once
// =============================================================================
// NkVulkanComputeContext.h - Vulkan compute queue wrapper
// =============================================================================
#include "NkIComputeContext.h"
#include "NKRenderer/Context/Core/NkIGraphicsContext.h"
#include "NKRenderer/Context/Graphics/Vulkan/NkVulkanContextData.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(MemoryBarrier)
#undef MemoryBarrier
#endif

namespace nkentseu {

#if NKENTSEU_HAS_VULKAN_HEADERS
struct NkVulkanComputeData {
    VkDevice         device         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue          computeQueue   = VK_NULL_HANDLE;
    VkCommandPool    commandPool    = VK_NULL_HANDLE;
    VkCommandBuffer  commandBuffer  = VK_NULL_HANDLE;
    VkFence          fence          = VK_NULL_HANDLE;
    uint32           computeFamily  = UINT32_MAX;
    bool             ownsDevice     = false; // true for standalone context
};
#endif

class NkVulkanComputeContext final : public NkIComputeContext {
public:
    NkVulkanComputeContext()  = default;
    ~NkVulkanComputeContext() override;

    bool Init(const NkContextDesc& desc);
    void InitFromGraphicsContext(NkIGraphicsContext* gfx);

    bool          IsValid()  const override;
    void          Shutdown()       override;

    NkComputeBuffer  CreateBuffer (const NkComputeBufferDesc& d)          override;
    void             DestroyBuffer(NkComputeBuffer& buf)                   override;
    bool             WriteBuffer  (NkComputeBuffer& buf, const void* data,
                                   uint64 bytes, uint64 offset=0)          override;
    bool             ReadBuffer   (const NkComputeBuffer& buf, void* out,
                                   uint64 bytes, uint64 offset=0)          override;

    NkComputeShader   CreateShaderFromSource(const char* src,
                                             const char* entry="main")    override;
    NkComputeShader   CreateShaderFromFile  (const char* path,
                                             const char* entry="main")    override;
    void              DestroyShader(NkComputeShader& s)                    override;

    NkComputePipeline CreatePipeline (const NkComputeShader& s)            override;
    void              DestroyPipeline(NkComputePipeline& p)                override;

    void BindBuffer  (uint32 slot, NkComputeBuffer& buf)                   override;
    void BindPipeline(const NkComputePipeline& p)                          override;
    void Dispatch    (uint32 gx, uint32 gy=1, uint32 gz=1)                 override;

    void WaitIdle()      override;
    void MemoryBarrier() override;

    NkGraphicsApi GetApi()               const override;
    uint32        GetMaxGroupSizeX()     const override;
    uint32        GetMaxGroupSizeY()     const override;
    uint32        GetMaxGroupSizeZ()     const override;
    uint64        GetSharedMemoryBytes() const override;
    bool          SupportsAtomics()      const override;
    bool          SupportsFloat64()      const override;

private:
#if NKENTSEU_HAS_VULKAN_HEADERS
    bool FindMemoryType(uint32 filter, VkMemoryPropertyFlags props, uint32& out);
    bool CreateBuffer_Internal(uint64 size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags memProps,
                               VkBuffer& buf, VkDeviceMemory& mem);

    NkVulkanComputeData mData;
    VkPipeline          mCurrentPipeline = VK_NULL_HANDLE;

    // Simplified descriptor resources (single set, storage buffer bindings).
    VkDescriptorPool       mDescPool   = VK_NULL_HANDLE;
    VkDescriptorSetLayout  mSetLayout  = VK_NULL_HANDLE;
    VkDescriptorSet        mDescSet    = VK_NULL_HANDLE;
    VkPipelineLayout       mPipeLayout = VK_NULL_HANDLE;
#endif

    bool mIsValid = false;
};

} // namespace nkentseu