// =============================================================================
// NkDirectX12CommandBuffer.cpp
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NkDirectX12CommandBuffer.h"
#include "NkDirectX12Device.h"
#include <algorithm>
#include <cstring>

namespace nkentseu {

NkDirectX12CommandBuffer::NkDirectX12CommandBuffer(NkDirectX12Device* dev, NkCommandBufferType type)
    : mDev(dev), mType(type) {
    if (!dev || !dev->Dev()) return;

    auto listType = (type == NkCommandBufferType::NK_COMPUTE)
                  ? D3D12_COMMAND_LIST_TYPE_COMPUTE
                  : D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT hr = dev->Dev()->CreateCommandAllocator(listType, IID_PPV_ARGS(&mAllocator));
    if (FAILED(hr) || !mAllocator) return;

    hr = dev->Dev()->CreateCommandList(0, listType, mAllocator.Get(), nullptr, IID_PPV_ARGS(&mCmdList));
    if (FAILED(hr) || !mCmdList) {
        mAllocator.Reset();
        return;
    }

    mCmdList->Close(); // démarre fermé, Begin() rouvre
}

NkDirectX12CommandBuffer::~NkDirectX12CommandBuffer() {}

void NkDirectX12CommandBuffer::Begin() {
    if (!mAllocator || !mCmdList) return;
    if (FAILED(mAllocator->Reset())) return;
    if (FAILED(mCmdList->Reset(mAllocator.Get(), nullptr))) return;

    // Binder les descriptor heaps shader-visible
    ID3D12DescriptorHeap* heaps[] = {
        mDev->CbvSrvUavHeap().heap.Get(),
        mDev->SamplerHeap().heap.Get()
    };
    mCmdList->SetDescriptorHeaps(2, heaps);
}

void NkDirectX12CommandBuffer::End()   { if (mCmdList) mCmdList->Close(); }
void NkDirectX12CommandBuffer::Reset() {
    if (!mAllocator || !mCmdList) return;
    if (FAILED(mAllocator->Reset())) return;
    mCmdList->Reset(mAllocator.Get(), nullptr);
}

// =============================================================================
// Render Pass (en DX12 = OMSetRenderTargets + Clear + transitions)
// =============================================================================
void NkDirectX12CommandBuffer::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                            NkFramebufferHandle fb,
                                            const NkRect2D& area) {
    if (!mCmdList) return;
    auto* fbo = mDev->GetFBO(fb.id);
    if (!fbo) return;

    mActiveColorCount = fbo->rtvCount;
    for (uint32 i = 0; i < mActiveColorCount; i++) {
        mActiveColorTexIds[i] = fbo->colorTexIds[i];
        mDev->TransitionTextureState(mCmdList.Get(), mActiveColorTexIds[i],
                                     D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    mActiveDepthTexId = fbo->depthTexId;
    if (mActiveDepthTexId != 0) {
        mDev->TransitionTextureState(mCmdList.Get(), mActiveDepthTexId,
                                     D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }

    // Bind render targets
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8];
    for (uint32 i = 0; i < fbo->rtvCount; i++)
        rtvHandles[i] = mDev->GetRTV(fbo->rtvIdxs[i]);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
    D3D12_CPU_DESCRIPTOR_HANDLE* pDsv = nullptr;
    if (fbo->dsvIdx != UINT_MAX) {
        dsvHandle = mDev->GetDSV(fbo->dsvIdx);
        pDsv = &dsvHandle;
    }

    mCmdList->OMSetRenderTargets(fbo->rtvCount, rtvHandles, FALSE, pDsv);

    // Clear
    float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
    for (uint32 i = 0; i < fbo->rtvCount; i++)
        mCmdList->ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
    if (pDsv)
        mCmdList->ClearDepthStencilView(dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0, 0, nullptr);

    // Viewport & scissor par défaut
    D3D12_VIEWPORT vp{ (float)area.x, (float)area.y,
                        (float)area.width, (float)area.height, 0.f, 1.f };
    D3D12_RECT    sc{ area.x, area.y,
                       (LONG)(area.x + area.width),
                       (LONG)(area.y + area.height) };
    mCmdList->RSSetViewports(1, &vp);
    mCmdList->RSSetScissorRects(1, &sc);
}

void NkDirectX12CommandBuffer::EndRenderPass() {
    if (!mCmdList) return;
    for (uint32 i = 0; i < mActiveColorCount; i++) {
        if (mDev->IsSwapchainTexture(mActiveColorTexIds[i])) {
            mDev->TransitionTextureState(mCmdList.Get(), mActiveColorTexIds[i],
                                         D3D12_RESOURCE_STATE_PRESENT);
        }
    }
    mActiveColorCount = 0;
    mActiveDepthTexId = 0;
}

// =============================================================================
// Viewport & Scissor
// =============================================================================
void NkDirectX12CommandBuffer::SetViewport(const NkViewport& vp) {
    if (!mCmdList) return;
    D3D12_VIEWPORT v{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
    mCmdList->RSSetViewports(1, &v);
}

void NkDirectX12CommandBuffer::SetViewports(const NkViewport* vps, uint32 n) {
    if (!mCmdList) return;
    D3D12_VIEWPORT v[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    n = math::NkMin(n, (uint32)D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for (uint32 i = 0; i < n; i++)
        v[i] = { vps[i].x, vps[i].y, vps[i].width, vps[i].height, vps[i].minDepth, vps[i].maxDepth };
    mCmdList->RSSetViewports(n, v);
}

void NkDirectX12CommandBuffer::SetScissor(const NkRect2D& r) {
    if (!mCmdList) return;
    D3D12_RECT sc{ r.x, r.y, (LONG)(r.x + r.width), (LONG)(r.y + r.height) };
    mCmdList->RSSetScissorRects(1, &sc);
}

void NkDirectX12CommandBuffer::SetScissors(const NkRect2D* rects, uint32 n) {
    if (!mCmdList) return;
    D3D12_RECT sc[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    n = math::NkMin(n, (uint32)D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for (uint32 i = 0; i < n; i++)
        sc[i] = { rects[i].x, rects[i].y,
                  (LONG)(rects[i].x + rects[i].width),
                  (LONG)(rects[i].y + rects[i].height) };
    mCmdList->RSSetScissorRects(n, sc);
}

// =============================================================================
// Pipeline
// =============================================================================
void NkDirectX12CommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
    if (!mCmdList) return;
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe || !pipe->pso || !pipe->rootSig) return;
    mIsCompute = false;
    mCmdList->SetPipelineState(pipe->pso.Get());
    mCmdList->SetGraphicsRootSignature(pipe->rootSig.Get());
    mCmdList->IASetPrimitiveTopology(pipe->topology);
    for (uint32 i = 0; i < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
        mVertexStrides[i] = pipe->vertexStrides[i];
}

void NkDirectX12CommandBuffer::BindComputePipeline(NkPipelineHandle p) {
    if (!mCmdList) return;
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe || !pipe->pso || !pipe->rootSig) return;
    mIsCompute = true;
    mCmdList->SetPipelineState(pipe->pso.Get());
    mCmdList->SetComputeRootSignature(pipe->rootSig.Get());
}

// =============================================================================
// Descriptor Set
// =============================================================================
void NkDirectX12CommandBuffer::BindDescriptorSet(NkDescSetHandle set,
                                              uint32 /*rootParamIdx*/,
                                              uint32* /*dynamicOffsets*/,
                                              uint32 /*dynOffsetCount*/) {
    if (!mCmdList) return;
    auto* ds = mDev->GetDescSet(set.id);
    if (!ds) return;

    for (auto& b : ds->bindings) {
        switch (b.type) {
            case NkDescriptorType::NK_UNIFORM_BUFFER:
            case NkDescriptorType::NK_UNIFORM_BUFFER_DYNAMIC:
                if (b.bufId != 0) {
                    D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = mDev->GetBufferGPUAddr(b.bufId);
                    if (gpuAddr != 0) {
                        if (mIsCompute) mCmdList->SetComputeRootConstantBufferView(1, gpuAddr);
                        else            mCmdList->SetGraphicsRootConstantBufferView(1, gpuAddr);
                    }
                }
                break;

            case NkDescriptorType::NK_SAMPLED_TEXTURE:
            case NkDescriptorType::NK_INPUT_ATTACHMENT:
                if (b.texId != 0) {
                    UINT idx = mDev->GetTextureSrvIndex(b.texId);
                    if (idx != UINT_MAX) {
                        D3D12_GPU_DESCRIPTOR_HANDLE h = mDev->CbvSrvUavHeap().GPUFrom(idx);
                        if (mIsCompute) mCmdList->SetComputeRootDescriptorTable(2, h);
                        else            mCmdList->SetGraphicsRootDescriptorTable(2, h);
                    }
                }
                break;

            case NkDescriptorType::NK_STORAGE_BUFFER:
            case NkDescriptorType::NK_STORAGE_BUFFER_DYNAMIC:
                if (b.bufId != 0) {
                    UINT idx = mDev->GetBufferUavIndex(b.bufId);
                    if (idx != UINT_MAX) {
                        D3D12_GPU_DESCRIPTOR_HANDLE h = mDev->CbvSrvUavHeap().GPUFrom(idx);
                        if (mIsCompute) mCmdList->SetComputeRootDescriptorTable(3, h);
                        else            mCmdList->SetGraphicsRootDescriptorTable(3, h);
                    }
                }
                break;

            case NkDescriptorType::NK_STORAGE_TEXTURE:
                if (b.texId != 0) {
                    UINT idx = mDev->GetTextureUavIndex(b.texId);
                    if (idx != UINT_MAX) {
                        D3D12_GPU_DESCRIPTOR_HANDLE h = mDev->CbvSrvUavHeap().GPUFrom(idx);
                        if (mIsCompute) mCmdList->SetComputeRootDescriptorTable(3, h);
                        else            mCmdList->SetGraphicsRootDescriptorTable(3, h);
                    }
                }
                break;

            case NkDescriptorType::NK_SAMPLER:
                if (b.sampId != 0) {
                    UINT idx = mDev->GetSamplerHeapIndex(b.sampId);
                    if (idx != UINT_MAX) {
                        D3D12_GPU_DESCRIPTOR_HANDLE h = mDev->SamplerHeap().GPUFrom(idx);
                        if (mIsCompute) mCmdList->SetComputeRootDescriptorTable(4, h);
                        else            mCmdList->SetGraphicsRootDescriptorTable(4, h);
                    }
                }
                break;

            case NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER:
                if (b.texId != 0) {
                    UINT srvIdx = mDev->GetTextureSrvIndex(b.texId);
                    if (srvIdx != UINT_MAX) {
                        D3D12_GPU_DESCRIPTOR_HANDLE h = mDev->CbvSrvUavHeap().GPUFrom(srvIdx);
                        if (mIsCompute) mCmdList->SetComputeRootDescriptorTable(2, h);
                        else            mCmdList->SetGraphicsRootDescriptorTable(2, h);
                    }
                }
                if (b.sampId != 0) {
                    UINT sampIdx = mDev->GetSamplerHeapIndex(b.sampId);
                    if (sampIdx != UINT_MAX) {
                        D3D12_GPU_DESCRIPTOR_HANDLE h = mDev->SamplerHeap().GPUFrom(sampIdx);
                        if (mIsCompute) mCmdList->SetComputeRootDescriptorTable(4, h);
                        else            mCmdList->SetGraphicsRootDescriptorTable(4, h);
                    }
                }
                break;
        }
    }
}

// =============================================================================
// Push Constants (root constants, paramètre 0 de la root signature)
// =============================================================================
void NkDirectX12CommandBuffer::PushConstants(NkShaderStage /*stages*/,
                                          uint32 offset, uint32 size,
                                          const void* data) {
    if (!mCmdList) return;
    uint32 numConstants = size / 4;
    uint32 destOffset   = offset / 4;
    if (mIsCompute)
        mCmdList->SetComputeRoot32BitConstants(0, numConstants, data, destOffset);
    else
        mCmdList->SetGraphicsRoot32BitConstants(0, numConstants, data, destOffset);
}

// =============================================================================
// Vertex / Index
// =============================================================================
void NkDirectX12CommandBuffer::BindVertexBuffer(uint32 binding,
                                             NkBufferHandle buf, uint64 offset) {
    if (!mCmdList) return;
    ID3D12Resource* res = mDev->GetDX12Buffer(buf.id);
    if (!res) return;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = mDev->GetBufferGPUAddr(buf.id) + offset;
    vbv.SizeInBytes    = (UINT)(res->GetDesc().Width - offset);
    vbv.StrideInBytes  = (binding < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
        ? mVertexStrides[binding]
        : 0;
    if (vbv.StrideInBytes == 0) vbv.StrideInBytes = 1;
    mCmdList->IASetVertexBuffers(binding, 1, &vbv);
}

void NkDirectX12CommandBuffer::BindVertexBuffers(uint32 first,
                                              const NkBufferHandle* bufs,
                                              const uint64* offsets, uint32 n) {
    if (!mCmdList) return;
    D3D12_VERTEX_BUFFER_VIEW vbvs[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    n = math::NkMin(n, (uint32)D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
    for (uint32 i = 0; i < n; i++) {
        ID3D12Resource* res = mDev->GetDX12Buffer(bufs[i].id);
        vbvs[i].BufferLocation = mDev->GetBufferGPUAddr(bufs[i].id) + offsets[i];
        vbvs[i].SizeInBytes    = res ? (UINT)(res->GetDesc().Width - offsets[i]) : 0;
        uint32 slot = first + i;
        vbvs[i].StrideInBytes = (slot < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            ? mVertexStrides[slot]
            : 0;
        if (vbvs[i].StrideInBytes == 0) vbvs[i].StrideInBytes = 1;
    }
    mCmdList->IASetVertexBuffers(first, n, vbvs);
}

void NkDirectX12CommandBuffer::BindIndexBuffer(NkBufferHandle buf,
                                            NkIndexFormat fmt, uint64 offset) {
    if (!mCmdList) return;
    D3D12_INDEX_BUFFER_VIEW ibv{};
    ID3D12Resource* res = mDev->GetDX12Buffer(buf.id);
    ibv.BufferLocation = mDev->GetBufferGPUAddr(buf.id) + offset;
    ibv.SizeInBytes    = res ? (UINT)(res->GetDesc().Width - offset) : 0;
    ibv.Format         = fmt == NkIndexFormat::NK_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    mCmdList->IASetIndexBuffer(&ibv);
}

// =============================================================================
// Draw
// =============================================================================
void NkDirectX12CommandBuffer::Draw(uint32 vtx, uint32 inst,
                                 uint32 firstVtx, uint32 firstInst) {
    if (!mCmdList) return;
    mCmdList->DrawInstanced(vtx, inst, firstVtx, firstInst);
}

void NkDirectX12CommandBuffer::DrawIndexed(uint32 idx, uint32 inst,
                                        uint32 firstIdx, int32 vtxOff,
                                        uint32 firstInst) {
    if (!mCmdList) return;
    mCmdList->DrawIndexedInstanced(idx, inst, firstIdx, vtxOff, firstInst);
}

void NkDirectX12CommandBuffer::DrawIndirect(NkBufferHandle buf, uint64 off,
                                         uint32 cnt, uint32 /*stride*/) {
    // Nécessite un ID3D12CommandSignature pré-créé
    // Simplification : on appelle ExecuteIndirect avec une signature draw standard
    (void)buf; (void)off; (void)cnt;
    // TODO: créer et cacher une DrawIndirectSignature dans le device
}

void NkDirectX12CommandBuffer::DrawIndexedIndirect(NkBufferHandle buf, uint64 off,
                                                uint32 cnt, uint32 /*stride*/) {
    (void)buf; (void)off; (void)cnt;
    // TODO: signature DrawIndexedIndirect
}

// =============================================================================
// Compute
// =============================================================================
void NkDirectX12CommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    if (!mCmdList) return;
    mCmdList->Dispatch(gx, gy, gz);
}

void NkDirectX12CommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    (void)buf; (void)off;
    // TODO: signature DispatchIndirect
}

// =============================================================================
// Copies
// =============================================================================
void NkDirectX12CommandBuffer::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                       const NkBufferCopyRegion& r) {
    if (!mCmdList) return;
    mCmdList->CopyBufferRegion(
        mDev->GetDX12Buffer(dst.id), r.dstOffset,
        mDev->GetDX12Buffer(src.id), r.srcOffset,
        r.size);
}

void NkDirectX12CommandBuffer::CopyBufferToTexture(NkBufferHandle src,
                                                NkTextureHandle dst,
                                                const NkBufferTextureCopyRegion& r) {
    if (!mCmdList) return;
    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource        = mDev->GetDX12Texture(dst.id);
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = r.mipLevel;

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = mDev->GetDX12Buffer(src.id);
    srcLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset = r.bufferOffset;
    srcLoc.PlacedFootprint.Footprint = {
        DXGI_FORMAT_R8G8B8A8_UNORM, r.width, r.height,
        r.depth > 0 ? r.depth : 1,
        r.bufferRowPitch
    };

    mCmdList->CopyTextureRegion(&dstLoc, r.x, r.y, r.z, &srcLoc, nullptr);
}

void NkDirectX12CommandBuffer::CopyTextureToBuffer(NkTextureHandle src,
                                                NkBufferHandle dst,
                                                const NkBufferTextureCopyRegion& r) {
    if (!mCmdList) return;
    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource        = mDev->GetDX12Texture(src.id);
    srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = r.mipLevel;

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = mDev->GetDX12Buffer(dst.id);
    dstLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint.Offset = r.bufferOffset;
    dstLoc.PlacedFootprint.Footprint = {
        DXGI_FORMAT_R8G8B8A8_UNORM, r.width, r.height,
        r.depth > 0 ? r.depth : 1,
        r.bufferRowPitch
    };

    D3D12_BOX box{ r.x, r.y, r.z, r.x + r.width, r.y + r.height, r.z + (r.depth > 0 ? r.depth : 1) };
    mCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &box);
}

void NkDirectX12CommandBuffer::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                        const NkTextureCopyRegion& r) {
    if (!mCmdList) return;
    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource        = mDev->GetDX12Texture(src.id);
    srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = r.srcMip;

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource        = mDev->GetDX12Texture(dst.id);
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = r.dstMip;

    D3D12_BOX box{ r.srcX, r.srcY, r.srcZ,
                   r.srcX + r.width, r.srcY + r.height,
                   r.srcZ + (r.depth > 0 ? r.depth : 1) };
    mCmdList->CopyTextureRegion(&dstLoc, r.dstX, r.dstY, r.dstZ, &srcLoc, &box);
}

void NkDirectX12CommandBuffer::BlitTexture(NkTextureHandle /*src*/,
                                        NkTextureHandle /*dst*/,
                                        const NkTextureCopyRegion& /*r*/,
                                        NkFilter /*f*/) {
    // DX12 n'a pas de blit natif — nécessite un compute ou graphics pass de fullscreen
    // Laissé pour implémentation future dans un BlitHelper
}

// =============================================================================
// Barriers
// =============================================================================
void NkDirectX12CommandBuffer::Barrier(const NkBufferBarrier* bb, uint32 bc,
                                    const NkTextureBarrier* tb, uint32 tc) {
    if (!mCmdList) return;
    NkVector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.Reserve(bc + tc);

    auto toDX12State = [](NkResourceState s) -> D3D12_RESOURCE_STATES {
        switch (s) {
            case NkResourceState::NK_COMMON:          return D3D12_RESOURCE_STATE_COMMON;
            case NkResourceState::NK_VERTEX_BUFFER:    return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            case NkResourceState::NK_INDEX_BUFFER:     return D3D12_RESOURCE_STATE_INDEX_BUFFER;
            case NkResourceState::NK_UNIFORM_BUFFER:   return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            case NkResourceState::NK_UNORDERED_ACCESS: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            case NkResourceState::NK_SHADER_READ:      return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                                          D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            case NkResourceState::NK_RENDER_TARGET:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case NkResourceState::NK_DEPTH_WRITE:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case NkResourceState::NK_DEPTH_READ:       return D3D12_RESOURCE_STATE_DEPTH_READ;
            case NkResourceState::NK_TRANSFER_SRC:     return D3D12_RESOURCE_STATE_COPY_SOURCE;
            case NkResourceState::NK_TRANSFER_DST:     return D3D12_RESOURCE_STATE_COPY_DEST;
            case NkResourceState::NK_INDIRECT_ARG:     return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
            case NkResourceState::NK_PRESENT:         return D3D12_RESOURCE_STATE_PRESENT;
            default:                               return D3D12_RESOURCE_STATE_COMMON;
        }
    };

    for (uint32 i = 0; i < bc; i++) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource   = mDev->GetDX12Buffer(bb[i].buffer.id);
        b.Transition.StateBefore = toDX12State(bb[i].stateBefore);
        b.Transition.StateAfter  = toDX12State(bb[i].stateAfter);
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        if (b.Transition.pResource && b.Transition.StateBefore != b.Transition.StateAfter)
            barriers.PushBack(b);
    }

    for (uint32 i = 0; i < tc; i++) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource   = mDev->GetDX12Texture(tb[i].texture.id);
        b.Transition.StateBefore = toDX12State(tb[i].stateBefore);
        b.Transition.StateAfter  = toDX12State(tb[i].stateAfter);
        b.Transition.Subresource = (tb[i].mipCount == UINT32_MAX && tb[i].layerCount == UINT32_MAX)
            ? D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
            : tb[i].baseMip; // simplification
        if (b.Transition.pResource && b.Transition.StateBefore != b.Transition.StateAfter)
            barriers.PushBack(b);
    }

    if (!barriers.empty())
        mCmdList->ResourceBarrier((UINT)barriers.Size(), barriers.Data());
}

// =============================================================================
// Debug
// =============================================================================
void NkDirectX12CommandBuffer::BeginDebugGroup(const char* name, float r, float g, float b) {
    // PIX events (nécessite WinPixEventRuntime)
    // PIXBeginEvent(mCmdList.Get(), PIX_COLOR_DEFAULT, name);
    (void)name; (void)r; (void)g; (void)b;
}

void NkDirectX12CommandBuffer::EndDebugGroup() {
    // PIXEndEvent(mCmdList.Get());
}

void NkDirectX12CommandBuffer::InsertDebugLabel(const char* name) {
    // PIXSetMarker(mCmdList.Get(), PIX_COLOR_DEFAULT, name);
    (void)name;
}

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
