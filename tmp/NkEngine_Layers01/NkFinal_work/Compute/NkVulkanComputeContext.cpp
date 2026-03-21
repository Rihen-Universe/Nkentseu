// =============================================================================
// NkVulkanComputeContext.cpp — Compute Vulkan
// Shaders : SPIR-V (.spv) chargés depuis fichier OU compilés via glslang
// Buffers : VkBuffer avec VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
// =============================================================================
#include "NkVulkanComputeContext.h"
#include "../Vulkan/NkVulkanContextData.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#define NK_VKC_LOG(...) printf("[NkVkCompute] " __VA_ARGS__)
#define NK_VKC_ERR(...) printf("[NkVkCompute][ERROR] " __VA_ARGS__)
#define NK_VKC_CHECK(r) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VKC_ERR(#r " = %d\n",(int)_r); return; } } while(0)
#define NK_VKC_CHECKB(r) do { VkResult _r=(r); if(_r!=VK_SUCCESS){ NK_VKC_ERR(#r " = %d\n",(int)_r); return false; } } while(0)

// Nombre max de SSBO bindables simultanément
static constexpr uint32 kMaxBindings = 16;

namespace nkentseu {

NkVulkanComputeContext::~NkVulkanComputeContext() { if (mIsValid) Shutdown(); }

// =============================================================================
void NkVulkanComputeContext::InitFromGraphicsContext(NkIGraphicsContext* gfx) {
    if (!gfx || !gfx->IsValid()) return;
    auto* vkData = static_cast<NkVulkanContextData*>(gfx->GetNativeContextData());
    if (!vkData) return;

    mData.device         = vkData->device;
    mData.physicalDevice = vkData->physicalDevice;
    mData.computeQueue   = vkData->computeQueue ? vkData->computeQueue
                                                 : vkData->graphicsQueue;
    mData.computeFamily  = vkData->computeFamily != UINT32_MAX
                           ? vkData->computeFamily
                           : vkData->graphicsFamily;
    mData.ownsDevice     = false;

    // Command pool dédié compute
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = mData.computeFamily;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(mData.device, &poolInfo, nullptr, &mData.commandPool) != VK_SUCCESS) {
        NK_VKC_ERR("vkCreateCommandPool failed\n"); return;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = mData.commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(mData.device, &allocInfo, &mData.commandBuffer);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(mData.device, &fenceInfo, nullptr, &mData.fence);

    // Descriptor layout : kMaxBindings storage buffers
    std::vector<VkDescriptorSetLayoutBinding> bindings(kMaxBindings);
    for (uint32 i = 0; i < kMaxBindings; ++i) {
        bindings[i] = {};
        bindings[i].binding         = i;
        bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = kMaxBindings;
    layoutInfo.pBindings    = bindings.data();
    vkCreateDescriptorSetLayout(mData.device, &layoutInfo, nullptr, &mSetLayout);

    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts    = &mSetLayout;
    vkCreatePipelineLayout(mData.device, &plInfo, nullptr, &mPipeLayout);

    // Descriptor pool
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxBindings};
    VkDescriptorPoolCreateInfo dpInfo{};
    dpInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpInfo.maxSets       = 1;
    dpInfo.poolSizeCount = 1;
    dpInfo.pPoolSizes    = &poolSize;
    vkCreateDescriptorPool(mData.device, &dpInfo, nullptr, &mDescPool);

    VkDescriptorSetAllocateInfo dsAlloc{};
    dsAlloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAlloc.descriptorPool     = mDescPool;
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts        = &mSetLayout;
    vkAllocateDescriptorSets(mData.device, &dsAlloc, &mDescSet);

    mIsValid = true;
    NK_VKC_LOG("Initialized from graphics context\n");
}

bool NkVulkanComputeContext::Init(const NkContextDesc&) {
    NK_VKC_ERR("Standalone Vulkan compute not yet implemented\n");
    return false;
}

void NkVulkanComputeContext::Shutdown() {
    if (!mIsValid) return;
    WaitIdle();
    if (mDescPool)    vkDestroyDescriptorPool(mData.device, mDescPool, nullptr);
    if (mPipeLayout)  vkDestroyPipelineLayout(mData.device, mPipeLayout, nullptr);
    if (mSetLayout)   vkDestroyDescriptorSetLayout(mData.device, mSetLayout, nullptr);
    if (mData.fence)  vkDestroyFence(mData.device, mData.fence, nullptr);
    if (mData.commandPool) vkDestroyCommandPool(mData.device, mData.commandPool, nullptr);
    mIsValid = false;
    NK_VKC_LOG("Shutdown\n");
}

bool NkVulkanComputeContext::IsValid() const { return mIsValid; }

// ── Mémoire ───────────────────────────────────────────────────────────────────
bool NkVulkanComputeContext::FindMemoryType(uint32 filter,
    VkMemoryPropertyFlags props, uint32& out) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(mData.physicalDevice, &mp);
    for (uint32 i = 0; i < mp.memoryTypeCount; ++i) {
        if ((filter & (1<<i)) && (mp.memoryTypes[i].propertyFlags & props)==props) {
            out=i; return true;
        }
    }
    return false;
}

bool NkVulkanComputeContext::CreateBuffer_Internal(
    uint64 size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memProps,
    VkBuffer& buf, VkDeviceMemory& mem)
{
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size  = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    NK_VKC_CHECKB(vkCreateBuffer(mData.device, &bi, nullptr, &buf));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(mData.device, buf, &req);
    uint32 memType = 0;
    if (!FindMemoryType(req.memoryTypeBits, memProps, memType)) return false;

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = memType;
    NK_VKC_CHECKB(vkAllocateMemory(mData.device, &ai, nullptr, &mem));
    vkBindBufferMemory(mData.device, buf, mem, 0);
    return true;
}

// ── Buffers ───────────────────────────────────────────────────────────────────
struct VkBufPair { VkBuffer buf; VkDeviceMemory mem; };

NkComputeBuffer NkVulkanComputeContext::CreateBuffer(const NkComputeBufferDesc& desc) {
    NkComputeBuffer out;
    if (!mIsValid || desc.sizeBytes == 0) return out;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT   |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags memProps =
        (desc.cpuReadable || desc.cpuWritable)
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    auto* pair = new VkBufPair();
    if (!CreateBuffer_Internal(desc.sizeBytes, usage, memProps, pair->buf, pair->mem)) {
        delete pair; return out;
    }

    if (desc.initialData && (desc.cpuWritable || memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        void* mapped = nullptr;
        vkMapMemory(mData.device, pair->mem, 0, desc.sizeBytes, 0, &mapped);
        if (mapped) {
            memcpy(mapped, desc.initialData, (size_t)desc.sizeBytes);
            vkUnmapMemory(mData.device, pair->mem);
        }
    }

    out.handle    = pair;
    out.sizeBytes = desc.sizeBytes;
    out.valid     = true;
    return out;
}

void NkVulkanComputeContext::DestroyBuffer(NkComputeBuffer& buf) {
    if (!buf.valid) return;
    auto* pair = static_cast<VkBufPair*>(buf.handle);
    vkDestroyBuffer(mData.device, pair->buf, nullptr);
    vkFreeMemory(mData.device, pair->mem, nullptr);
    delete pair;
    buf = NkComputeBuffer{};
}

bool NkVulkanComputeContext::WriteBuffer(NkComputeBuffer& buf,
    const void* data, uint64 bytes, uint64 offset) {
    if (!buf.valid || !data) return false;
    auto* pair = static_cast<VkBufPair*>(buf.handle);
    void* mapped = nullptr;
    if (vkMapMemory(mData.device, pair->mem, offset, bytes, 0, &mapped) != VK_SUCCESS)
        return false;
    memcpy(mapped, data, (size_t)bytes);
    vkUnmapMemory(mData.device, pair->mem);
    return true;
}

bool NkVulkanComputeContext::ReadBuffer(const NkComputeBuffer& buf,
    void* outData, uint64 bytes, uint64 offset) {
    if (!buf.valid || !outData) return false;
    auto* pair = static_cast<VkBufPair*>(buf.handle);
    void* mapped = nullptr;
    if (vkMapMemory(mData.device, pair->mem, offset, bytes, 0, &mapped) != VK_SUCCESS)
        return false;
    memcpy(outData, mapped, (size_t)bytes);
    vkUnmapMemory(mData.device, pair->mem);
    return true;
}

// ── Shaders (SPIR-V) ──────────────────────────────────────────────────────────
NkComputeShader NkVulkanComputeContext::CreateShaderFromFile(
    const char* path, const char* /*entry*/) {
    NkComputeShader s;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) { NK_VKC_ERR("Cannot open: %s\n", path); return s; }
    size_t size = f.tellg(); f.seekg(0);
    std::vector<char> code(size);
    f.read(code.data(), size);

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size;
    ci.pCode    = (uint32*)code.data();
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(mData.device, &ci, nullptr, &mod) != VK_SUCCESS) {
        NK_VKC_ERR("vkCreateShaderModule failed\n"); return s;
    }
    s.handle = mod;
    s.valid  = true;
    return s;
}

NkComputeShader NkVulkanComputeContext::CreateShaderFromSource(
    const char* /*source*/, const char* /*entry*/) {
    // Pour Vulkan, il faut du SPIR-V — utiliser CreateShaderFromFile(.spv)
    // ou compiler via glslang/shaderc (non inclus ici)
    NK_VKC_ERR("Vulkan requires SPIR-V — use CreateShaderFromFile(.spv)\n");
    return {};
}

void NkVulkanComputeContext::DestroyShader(NkComputeShader& s) {
    if (!s.valid) return;
    vkDestroyShaderModule(mData.device, static_cast<VkShaderModule>(s.handle), nullptr);
    s = NkComputeShader{};
}

// ── Pipeline ──────────────────────────────────────────────────────────────────
NkComputePipeline NkVulkanComputeContext::CreatePipeline(const NkComputeShader& s) {
    NkComputePipeline p;
    if (!s.valid) return p;

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = static_cast<VkShaderModule>(s.handle);
    stageInfo.pName  = "main";

    VkComputePipelineCreateInfo pipeInfo{};
    pipeInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.stage  = stageInfo;
    pipeInfo.layout = mPipeLayout;

    VkPipeline pipe = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(mData.device, VK_NULL_HANDLE, 1,
                                  &pipeInfo, nullptr, &pipe) != VK_SUCCESS) {
        NK_VKC_ERR("vkCreateComputePipelines failed\n"); return p;
    }
    p.handle = pipe;
    p.valid  = true;
    return p;
}

void NkVulkanComputeContext::DestroyPipeline(NkComputePipeline& p) {
    if (!p.valid) return;
    vkDestroyPipeline(mData.device, static_cast<VkPipeline>(p.handle), nullptr);
    p = NkComputePipeline{};
}

// ── Dispatch ──────────────────────────────────────────────────────────────────
void NkVulkanComputeContext::BindPipeline(const NkComputePipeline& p) {
    mCurrentPipeline = static_cast<VkPipeline>(p.handle);
}

void NkVulkanComputeContext::BindBuffer(uint32 slot, NkComputeBuffer& buf) {
    if (!buf.valid || slot >= kMaxBindings) return;
    auto* pair = static_cast<VkBufPair*>(buf.handle);
    VkDescriptorBufferInfo binfo{pair->buf, 0, buf.sizeBytes};
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = mDescSet;
    write.dstBinding      = slot;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo     = &binfo;
    vkUpdateDescriptorSets(mData.device, 1, &write, 0, nullptr);
}

void NkVulkanComputeContext::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    if (!mIsValid || mCurrentPipeline == VK_NULL_HANDLE) return;

    vkResetFences(mData.device, 1, &mData.fence);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(mData.commandBuffer, &beginInfo);

    vkCmdBindPipeline(mData.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mCurrentPipeline);
    vkCmdBindDescriptorSets(mData.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                             mPipeLayout, 0, 1, &mDescSet, 0, nullptr);
    vkCmdDispatch(mData.commandBuffer, gx, gy, gz);
    vkEndCommandBuffer(mData.commandBuffer);

    VkSubmitInfo submit{};
    submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &mData.commandBuffer;
    vkQueueSubmit(mData.computeQueue, 1, &submit, mData.fence);
}

void NkVulkanComputeContext::WaitIdle() {
    if (!mIsValid) return;
    vkWaitForFences(mData.device, 1, &mData.fence, VK_TRUE, UINT64_MAX);
}

void NkVulkanComputeContext::MemoryBarrier() {
    // Barrière via pipeline barrier dans un command buffer secondaire si nécessaire
    // Pour l'usage simple, WaitIdle() entre dispatches est suffisant
}

// ── Capacités ─────────────────────────────────────────────────────────────────
NkGraphicsApi NkVulkanComputeContext::GetApi() const { return NkGraphicsApi::NK_API_VULKAN; }
bool NkVulkanComputeContext::IsValid() const { return mIsValid; }

uint32 NkVulkanComputeContext::GetMaxGroupSizeX() const {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(mData.physicalDevice, &p);
    return p.limits.maxComputeWorkGroupSize[0];
}
uint32 NkVulkanComputeContext::GetMaxGroupSizeY() const {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(mData.physicalDevice, &p);
    return p.limits.maxComputeWorkGroupSize[1];
}
uint32 NkVulkanComputeContext::GetMaxGroupSizeZ() const {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(mData.physicalDevice, &p);
    return p.limits.maxComputeWorkGroupSize[2];
}
uint64 NkVulkanComputeContext::GetSharedMemoryBytes() const {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(mData.physicalDevice, &p);
    return p.limits.maxComputeSharedMemorySize;
}
bool NkVulkanComputeContext::SupportsAtomics() const { return true; }
bool NkVulkanComputeContext::SupportsFloat64() const {
    VkPhysicalDeviceFeatures f;
    vkGetPhysicalDeviceFeatures(mData.physicalDevice, &f);
    return f.shaderFloat64 == VK_TRUE;
}

} // namespace nkentseu
