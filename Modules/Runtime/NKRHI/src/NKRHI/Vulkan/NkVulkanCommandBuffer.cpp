// =============================================================================
// NkVulkanCommandBuffer.cpp
// =============================================================================
#ifdef NK_RHI_VK_ENABLED
#include "NkVulkanCommandBuffer.h"
#include "NkVulkanDevice.h"
#include <cstring>
#include "NKContainers/Sequential/NkVector.h"

#include "NKPlatform/NkPlatformDetect.h"

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    #ifdef NKENTSEU_PLATFORM_ANDROID
        #include <vulkan/vulkan_android.h>
        #include <android/native_window.h>
    #endif
#endif

namespace nkentseu {

    NkVulkanCommandBuffer::NkVulkanCommandBuffer(NkVulkanDevice* dev, NkCommandBufferType type)
        : mDev(dev), mType(type) {
        VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpci.queueFamilyIndex = dev->GetGraphicsQueueFamilyIndex();
        vkCreateCommandPool(dev->GetVkDevice(), &cpci, nullptr, &mPool);

        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = mPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        vkAllocateCommandBuffers(dev->GetVkDevice(), &ai, &mCmdBuf);
    }

    NkVulkanCommandBuffer::~NkVulkanCommandBuffer() {
        if (mCmdBuf) vkFreeCommandBuffers(mDev->GetVkDevice(), mPool, 1, &mCmdBuf);
        if (mPool)   vkDestroyCommandPool(mDev->GetVkDevice(), mPool, nullptr);
    }

    bool NkVulkanCommandBuffer::Begin() {
        if (mCmdBuf == VK_NULL_HANDLE) return false;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(mCmdBuf, &bi) != VK_SUCCESS) {
            mRecording = false;
            return false;
        }
        mRecording = true;
        return true;
    }
    void NkVulkanCommandBuffer::End()   { vkEndCommandBuffer(mCmdBuf); mRecording = false; }
    void NkVulkanCommandBuffer::Reset() { vkResetCommandBuffer(mCmdBuf, 0); mRecording = false; }

    // ── Render Pass ───────────────────────────────────────────────────────────────
    bool NkVulkanCommandBuffer::BeginRenderPass(NkRenderPassHandle rp,
                                            NkFramebufferHandle fb,
                                            const NkRect2D& area) {
        if (!mRecording || !rp.IsValid() || !fb.IsValid()) return false;
        const VkRenderPass vkRp = mDev->GetVkRP(rp.id);
        const VkFramebuffer vkFb = mDev->GetVkFB(fb.id);
        if (vkRp == VK_NULL_HANDLE || vkFb == VK_NULL_HANDLE) return false;

        VkClearValue clears[9];
        const uint32 colorCount = mDev->GetVkRenderPassColorCount(rp.id);
        const bool hasDepth = mDev->GetVkRenderPassHasDepth(rp.id);
        uint32 clearCount = colorCount + (hasDepth ? 1u : 0u);
        if (clearCount > 9u) clearCount = 9u;
        for (uint32 i = 0; i < clearCount; ++i) {
            clears[i].color = {{mClearColor[0], mClearColor[1], mClearColor[2], mClearColor[3]}};
        }
        if (hasDepth && clearCount > 0) {
            clears[colorCount].depthStencil = {mClearDepth, mClearStencil};
        }

        VkRenderPassBeginInfo rpbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpbi.renderPass  = vkRp;
        rpbi.framebuffer = vkFb;
        rpbi.renderArea.offset.x = area.x;
        rpbi.renderArea.offset.y = area.y;
        rpbi.renderArea.extent.width  = area.width  > 0 ? static_cast<uint32>(area.width)  : 0u;
        rpbi.renderArea.extent.height = area.height > 0 ? static_cast<uint32>(area.height) : 0u;
        if (rpbi.renderArea.extent.width == 0 || rpbi.renderArea.extent.height == 0) return false;
        rpbi.clearValueCount = clearCount;
        rpbi.pClearValues    = clearCount > 0 ? clears : nullptr;
        vkCmdBeginRenderPass(mCmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        return true;
    }
    void NkVulkanCommandBuffer::EndRenderPass() { vkCmdEndRenderPass(mCmdBuf); }

    // ── Viewport & Scissor ────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::SetViewport(const NkViewport& vp) {
        VkViewport v = vp.flipY
            ? VkViewport{vp.x, vp.y+vp.height, vp.width, -vp.height, vp.minDepth, vp.maxDepth}
            : VkViewport{vp.x, vp.y,            vp.width,  vp.height, vp.minDepth, vp.maxDepth};
        vkCmdSetViewport(mCmdBuf, 0, 1, &v);
    }
    void NkVulkanCommandBuffer::SetViewports(const NkViewport* vps, uint32 n) {
        NkVector<VkViewport> v(n);
        for (uint32 i=0;i<n;i++) {
            v[i] = vps[i].flipY
                ? VkViewport{vps[i].x, vps[i].y+vps[i].height, vps[i].width, -vps[i].height, vps[i].minDepth, vps[i].maxDepth}
                : VkViewport{vps[i].x, vps[i].y,                vps[i].width,  vps[i].height, vps[i].minDepth, vps[i].maxDepth};
        }
        vkCmdSetViewport(mCmdBuf,0,n,v.Data());
    }
    void NkVulkanCommandBuffer::SetScissor(const NkRect2D& r) {
        VkRect2D sc{};
        sc.offset.x = r.x;
        sc.offset.y = r.y;
        sc.extent.width  = r.width  > 0 ? static_cast<uint32>(r.width)  : 0u;
        sc.extent.height = r.height > 0 ? static_cast<uint32>(r.height) : 0u;
        vkCmdSetScissor(mCmdBuf,0,1,&sc);
    }
    void NkVulkanCommandBuffer::SetScissors(const NkRect2D* r, uint32 n) {
        NkVector<VkRect2D> v(n);
        for (uint32 i=0;i<n;i++) {
            v[i].offset.x = r[i].x;
            v[i].offset.y = r[i].y;
            v[i].extent.width  = r[i].width  > 0 ? static_cast<uint32>(r[i].width)  : 0u;
            v[i].extent.height = r[i].height > 0 ? static_cast<uint32>(r[i].height) : 0u;
        }
        vkCmdSetScissor(mCmdBuf,0,n,v.Data());
    }

    // ── Pipeline ──────────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
        mBoundLayout = mDev->GetVkPipelineLayout(p.id);
        mIsCompute   = false;
        vkCmdBindPipeline(mCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mDev->GetVkPipeline(p.id));
    }
    void NkVulkanCommandBuffer::BindComputePipeline(NkPipelineHandle p) {
        mBoundLayout = mDev->GetVkPipelineLayout(p.id);
        mIsCompute   = true;
        vkCmdBindPipeline(mCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, mDev->GetVkPipeline(p.id));
    }
    void NkVulkanCommandBuffer::BindDescriptorSet(NkDescSetHandle set, uint32 idx,
                                                uint32* off, uint32 cnt) {
        VkDescriptorSet ds = mDev->GetVkDescSet(set.id);
        auto bp = mIsCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindDescriptorSets(mCmdBuf, bp, mBoundLayout, idx, 1, &ds, cnt, off);
    }
    void NkVulkanCommandBuffer::PushConstants(NkShaderStage stages, uint32 offset,
                                            uint32 size, const void* data) {
        VkShaderStageFlags vkStages = 0;
        if ((uint32)stages & (uint32)NkShaderStage::NK_VERTEX)   vkStages |= VK_SHADER_STAGE_VERTEX_BIT;
        if ((uint32)stages & (uint32)NkShaderStage::NK_FRAGMENT) vkStages |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if ((uint32)stages & (uint32)NkShaderStage::NK_COMPUTE)  vkStages |= VK_SHADER_STAGE_COMPUTE_BIT;
        if (vkStages == 0) vkStages = VK_SHADER_STAGE_ALL;
        vkCmdPushConstants(mCmdBuf, mBoundLayout, vkStages, offset, size, data);
    }

    // ── Vertex / Index ────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::BindVertexBuffer(uint32 binding, NkBufferHandle buf, uint64 off) {
        VkBuffer b = mDev->GetVkBuffer(buf.id);
        VkDeviceSize vkOff = static_cast<VkDeviceSize>(off);
        vkCmdBindVertexBuffers(mCmdBuf, binding, 1, &b, &vkOff);
    }
    void NkVulkanCommandBuffer::BindVertexBuffers(uint32 first, const NkBufferHandle* bufs,
                                                const uint64* offs, uint32 n) {
        NkVector<VkBuffer> vb;
        vb.Resize(static_cast<NkVector<VkBuffer>::SizeType>(n));
        NkVector<VkDeviceSize> vo;
        vo.Resize(static_cast<NkVector<VkDeviceSize>::SizeType>(n), VkDeviceSize(0));
        for (uint32 i=0;i<n;i++) { vb[i]=mDev->GetVkBuffer(bufs[i].id); vo[i]=offs[i]; }
        vkCmdBindVertexBuffers(mCmdBuf, first, n, vb.Data(), vo.Data());
    }
    void NkVulkanCommandBuffer::BindIndexBuffer(NkBufferHandle buf, NkIndexFormat fmt, uint64 off) {
        vkCmdBindIndexBuffer(mCmdBuf, mDev->GetVkBuffer(buf.id), off,
            fmt==NkIndexFormat::NK_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    // ── Draw ──────────────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::Draw(uint32 v, uint32 i, uint32 fv, uint32 fi) {
        vkCmdDraw(mCmdBuf, v, i, fv, fi);
    }
    void NkVulkanCommandBuffer::DrawIndexed(uint32 idx, uint32 inst, uint32 fi, int32 vo, uint32 fInst) {
        vkCmdDrawIndexed(mCmdBuf, idx, inst, fi, vo, fInst);
    }
    void NkVulkanCommandBuffer::DrawIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) {
        vkCmdDrawIndirect(mCmdBuf, mDev->GetVkBuffer(buf.id), off, cnt, stride);
    }
    void NkVulkanCommandBuffer::DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) {
        vkCmdDrawIndexedIndirect(mCmdBuf, mDev->GetVkBuffer(buf.id), off, cnt, stride);
    }

    // ── Compute ───────────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
        vkCmdDispatch(mCmdBuf, gx, gy, gz);
    }
    void NkVulkanCommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
        vkCmdDispatchIndirect(mCmdBuf, mDev->GetVkBuffer(buf.id), off);
    }

    // ── Copies ────────────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::CopyBuffer(NkBufferHandle s, NkBufferHandle d,
                                        const NkBufferCopyRegion& r) {
        VkBufferCopy cp{r.srcOffset, r.dstOffset, r.size};
        vkCmdCopyBuffer(mCmdBuf, mDev->GetVkBuffer(s.id), mDev->GetVkBuffer(d.id), 1, &cp);
    }
    void NkVulkanCommandBuffer::CopyBufferToTexture(NkBufferHandle s, NkTextureHandle d,
                                                const NkBufferTextureCopyRegion& r) {
        VkBufferImageCopy cp{};
        cp.bufferOffset=r.bufferOffset; cp.bufferRowLength=r.bufferRowPitch;
        cp.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,r.mipLevel,r.arrayLayer,1};
        cp.imageOffset={(int32)r.x,(int32)r.y,(int32)r.z};
        cp.imageExtent={r.width,r.height,r.depth>0?r.depth:1};
        vkCmdCopyBufferToImage(mCmdBuf, mDev->GetVkBuffer(s.id), mDev->GetVkImage(d.id),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    }
    void NkVulkanCommandBuffer::CopyTextureToBuffer(NkTextureHandle s, NkBufferHandle d,
                                                const NkBufferTextureCopyRegion& r) {
        VkBufferImageCopy cp{};
        cp.bufferOffset=r.bufferOffset;
        cp.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,r.mipLevel,r.arrayLayer,1};
        cp.imageOffset={(int32)r.x,(int32)r.y,(int32)r.z};
        cp.imageExtent={r.width,r.height,r.depth>0?r.depth:1};
        vkCmdCopyImageToBuffer(mCmdBuf, mDev->GetVkImage(s.id),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mDev->GetVkBuffer(d.id), 1, &cp);
    }
    void NkVulkanCommandBuffer::CopyTexture(NkTextureHandle s, NkTextureHandle d,
                                        const NkTextureCopyRegion& r) {
        VkImageCopy cp{};
        cp.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,r.srcMip,r.srcLayer,1};
        cp.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,r.dstMip,r.dstLayer,1};
        cp.srcOffset={(int32)r.srcX,(int32)r.srcY,(int32)r.srcZ};
        cp.dstOffset={(int32)r.dstX,(int32)r.dstY,(int32)r.dstZ};
        cp.extent={r.width,r.height,r.depth>0?r.depth:1};
        vkCmdCopyImage(mCmdBuf,
            mDev->GetVkImage(s.id), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            mDev->GetVkImage(d.id), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    }
    void NkVulkanCommandBuffer::BlitTexture(NkTextureHandle s, NkTextureHandle d,
                                        const NkTextureCopyRegion& r, NkFilter filter) {
        VkImageBlit blit{};
        blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,r.srcMip,r.srcLayer,1};
        blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,r.dstMip,r.dstLayer,1};
        blit.srcOffsets[0]={(int32)r.srcX,(int32)r.srcY,(int32)r.srcZ};
        blit.srcOffsets[1]={(int32)(r.srcX+r.width),(int32)(r.srcY+r.height),(int32)(r.srcZ+(r.depth>0?r.depth:1))};
        blit.dstOffsets[0]={(int32)r.dstX,(int32)r.dstY,(int32)r.dstZ};
        blit.dstOffsets[1]={(int32)(r.dstX+r.width),(int32)(r.dstY+r.height),(int32)(r.dstZ+(r.depth>0?r.depth:1))};
        vkCmdBlitImage(mCmdBuf,
            mDev->GetVkImage(s.id), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            mDev->GetVkImage(d.id), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, filter==NkFilter::NK_NEAREST?VK_FILTER_NEAREST:VK_FILTER_LINEAR);
    }

    // ── Barriers ──────────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::Barrier(const NkBufferBarrier* bb, uint32 bc,
                                    const NkTextureBarrier* tb, uint32 tc) {
        NkVector<VkBufferMemoryBarrier>  bufBarriers;
        NkVector<VkImageMemoryBarrier>   imgBarriers;
        VkPipelineStageFlags srcStage=0, dstStage=0;

        auto toAccess=[](NkResourceState s, bool write) -> VkAccessFlags {
            switch(s){
                case NkResourceState::NK_UNIFORM_BUFFER:    return VK_ACCESS_UNIFORM_READ_BIT;
                case NkResourceState::NK_UNORDERED_ACCESS:  return write?VK_ACCESS_SHADER_WRITE_BIT:VK_ACCESS_SHADER_READ_BIT;
                case NkResourceState::NK_SHADER_READ:       return VK_ACCESS_SHADER_READ_BIT;
                case NkResourceState::NK_RENDER_TARGET:     return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                case NkResourceState::NK_DEPTH_WRITE:       return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                case NkResourceState::NK_TRANSFER_SRC:      return VK_ACCESS_TRANSFER_READ_BIT;
                case NkResourceState::NK_TRANSFER_DST:      return VK_ACCESS_TRANSFER_WRITE_BIT;
                default:                                return VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
            }
        };
        auto toStage=[](NkPipelineStage s) -> VkPipelineStageFlags {
            if ((uint32)s & (uint32)NkPipelineStage::NK_COMPUTE_SHADER) return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            if ((uint32)s & (uint32)NkPipelineStage::NK_TRANSFER)      return VK_PIPELINE_STAGE_TRANSFER_BIT;
            if ((uint32)s & (uint32)NkPipelineStage::NK_FRAGMENT_SHADER)return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            if ((uint32)s & (uint32)NkPipelineStage::NK_VERTEX_SHADER)  return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        };

        for (uint32 i=0;i<bc;i++) {
            VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            b.srcAccessMask=toAccess(bb[i].stateBefore,true);
            b.dstAccessMask=toAccess(bb[i].stateAfter,false);
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.buffer=mDev->GetVkBuffer(bb[i].buffer.id);
            b.offset=0; b.size=VK_WHOLE_SIZE;
            bufBarriers.PushBack(b);
            srcStage|=toStage(bb[i].srcStage); dstStage|=toStage(bb[i].dstStage);
        }

        auto toLayout=[](NkResourceState s) -> VkImageLayout {
            switch(s){
                case NkResourceState::NK_RENDER_TARGET:    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                case NkResourceState::NK_DEPTH_WRITE:      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                case NkResourceState::NK_DEPTH_READ:       return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                case NkResourceState::NK_SHADER_READ:      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                case NkResourceState::NK_UNORDERED_ACCESS: return VK_IMAGE_LAYOUT_GENERAL;
                case NkResourceState::NK_TRANSFER_SRC:     return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                case NkResourceState::NK_TRANSFER_DST:     return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                case NkResourceState::NK_PRESENT:         return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                default:                               return VK_IMAGE_LAYOUT_GENERAL;
            }
        };

        for (uint32 i=0;i<tc;i++) {
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.oldLayout=toLayout(tb[i].stateBefore);
            b.newLayout=toLayout(tb[i].stateAfter);
            b.srcAccessMask=toAccess(tb[i].stateBefore,true);
            b.dstAccessMask=toAccess(tb[i].stateAfter,false);
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            b.image=mDev->GetVkImage(tb[i].texture.id);
            VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT;
            if (tb[i].stateAfter==NkResourceState::NK_DEPTH_WRITE||tb[i].stateAfter==NkResourceState::NK_DEPTH_READ)
                aspect=VK_IMAGE_ASPECT_DEPTH_BIT;
            b.subresourceRange={aspect,tb[i].baseMip,tb[i].mipCount==UINT32_MAX?VK_REMAINING_MIP_LEVELS:tb[i].mipCount,
                                tb[i].baseLayer,tb[i].layerCount==UINT32_MAX?VK_REMAINING_ARRAY_LAYERS:tb[i].layerCount};
            imgBarriers.PushBack(b);
            srcStage|=toStage(tb[i].srcStage); dstStage|=toStage(tb[i].dstStage);
        }

        if (srcStage==0) srcStage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        if (dstStage==0) dstStage=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        vkCmdPipelineBarrier(mCmdBuf, srcStage, dstStage, 0,
            0, nullptr,
            (uint32)bufBarriers.Size(), bufBarriers.Empty()?nullptr:bufBarriers.Data(),
            (uint32)imgBarriers.Size(), imgBarriers.Empty()?nullptr:imgBarriers.Data());
    }

    // ── Mip generation ────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::GenerateMipmaps(NkTextureHandle tex, NkFilter f) {
        // Déléguer au device (utilise EndOneShot en interne)
        mDev->GenerateMipmaps(tex, f);
    }

    // ── Debug ─────────────────────────────────────────────────────────────────────
    void NkVulkanCommandBuffer::BeginDebugGroup(const char* name, float r, float g, float b) {
        // Nécessite VK_EXT_debug_utils
        // VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
        // label.pLabelName=name; label.color[0]=r;label.color[1]=g;label.color[2]=b;label.color[3]=1.f;
        // vkCmdBeginDebugUtilsLabelEXT(mCmdBuf,&label);
        (void)name; (void)r; (void)g; (void)b;
    }
    void NkVulkanCommandBuffer::EndDebugGroup() {
        // vkCmdEndDebugUtilsLabelEXT(mCmdBuf);
    }
    void NkVulkanCommandBuffer::InsertDebugLabel(const char* name) {
        (void)name;
    }

} // namespace nkentseu
#endif // NK_RHI_VK_ENABLED
