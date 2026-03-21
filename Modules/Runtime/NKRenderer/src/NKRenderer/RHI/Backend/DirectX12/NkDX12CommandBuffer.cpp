// =============================================================================
// NkCommandBuffer_DX12.cpp
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NkDX12CommandBuffer.h"
#include "NkDX12Device.h"
#include "NKCore/NkMacros.h"
#include <cstring>

namespace nkentseu {

NkCommandBuffer_DX12::NkCommandBuffer_DX12(NkDeviceDX12* dev, NkCommandBufferType type)
    : mDev(dev), mType(type) {
    auto listType = (type == NkCommandBufferType::NK_COMPUTE)
                  ? D3D12_COMMAND_LIST_TYPE_COMPUTE
                  : D3D12_COMMAND_LIST_TYPE_DIRECT;

    dev->Dev()->CreateCommandAllocator(listType, IID_PPV_ARGS(&mAllocator));
    dev->Dev()->CreateCommandList(0, listType, mAllocator.Get(), nullptr, IID_PPV_ARGS(&mCmdList));
    mCmdList->Close(); // démarre fermé, Begin() rouvre
}

NkCommandBuffer_DX12::~NkCommandBuffer_DX12() {}

void NkCommandBuffer_DX12::Begin() {
    mAllocator->Reset();
    mCmdList->Reset(mAllocator.Get(), nullptr);

    // Binder les descriptor heaps shader-visible
    ID3D12DescriptorHeap* heaps[] = {
        mDev->CbvSrvUavHeap().heap.Get(),
        mDev->SamplerHeap().heap.Get()
    };
    mCmdList->SetDescriptorHeaps(2, heaps);
}

void NkCommandBuffer_DX12::End()   { mCmdList->Close(); }
void NkCommandBuffer_DX12::Reset() { mAllocator->Reset(); mCmdList->Reset(mAllocator.Get(), nullptr); }

// =============================================================================
// Render Pass (en DX12 = OMSetRenderTargets + Clear + transitions)
// =============================================================================
void NkCommandBuffer_DX12::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                            NkFramebufferHandle fb,
                                            const NkRect2D& area) {
    auto* fbo = mDev->GetFBO(fb.id);
    if (!fbo) return;

    // Transitions de toutes les RTV vers RENDER_TARGET
    NkVector<D3D12_RESOURCE_BARRIER> barriers;
    for (uint32 i = 0; i < fbo->rtvCount; i++) {
        // On suppose que les textures swapchain sont en PRESENT → RENDER_TARGET
        // Pour simplifier on émet la barrière sans tracking d'état ici
        // (le tracking complet est dans Barrier())
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

void NkCommandBuffer_DX12::EndRenderPass() {
    // Les transitions RENDER_TARGET → PRESENT sont dans Barrier() ou SubmitAndPresent
}

// =============================================================================
// Viewport & Scissor
// =============================================================================
void NkCommandBuffer_DX12::SetViewport(const NkViewport& vp) {
    D3D12_VIEWPORT v{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
    mCmdList->RSSetViewports(1, &v);
}

void NkCommandBuffer_DX12::SetViewports(const NkViewport* vps, uint32 n) {
    D3D12_VIEWPORT v[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    n = NK_MIN(n, (uint32)D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for (uint32 i = 0; i < n; i++)
        v[i] = { vps[i].x, vps[i].y, vps[i].width, vps[i].height, vps[i].minDepth, vps[i].maxDepth };
    mCmdList->RSSetViewports(n, v);
}

void NkCommandBuffer_DX12::SetScissor(const NkRect2D& r) {
    D3D12_RECT sc{ r.x, r.y, (LONG)(r.x + r.width), (LONG)(r.y + r.height) };
    mCmdList->RSSetScissorRects(1, &sc);
}

void NkCommandBuffer_DX12::SetScissors(const NkRect2D* rects, uint32 n) {
    D3D12_RECT sc[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    n = NK_MIN(n, (uint32)D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for (uint32 i = 0; i < n; i++)
        sc[i] = { rects[i].x, rects[i].y,
                  (LONG)(rects[i].x + rects[i].width),
                  (LONG)(rects[i].y + rects[i].height) };
    mCmdList->RSSetScissorRects(n, sc);
}

// =============================================================================
// Pipeline
// =============================================================================
void NkCommandBuffer_DX12::BindGraphicsPipeline(NkPipelineHandle p) {
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe) return;
    mIsCompute = false;
    mCmdList->SetPipelineState(pipe->pso.Get());
    mCmdList->SetGraphicsRootSignature(pipe->rootSig.Get());
    mCmdList->IASetPrimitiveTopology(pipe->topology);
}

void NkCommandBuffer_DX12::BindComputePipeline(NkPipelineHandle p) {
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe) return;
    mIsCompute = true;
    mCmdList->SetPipelineState(pipe->pso.Get());
    mCmdList->SetComputeRootSignature(pipe->rootSig.Get());
}

// =============================================================================
// Descriptor Set
// =============================================================================
void NkCommandBuffer_DX12::BindDescriptorSet(NkDescSetHandle set,
                                              uint32 /*rootParamIdx*/,
                                              uint32* /*dynamicOffsets*/,
                                              uint32 /*dynOffsetCount*/) {
    auto* ds = mDev->GetDescSet(set.id);
    if (!ds) return;

    for (auto& b : ds->bindings) {
        // Résoudre le handle GPU selon le type
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
        bool isSampler = false;

        if (b.texId) {
            // Texture SRV
            auto& heap = mDev->CbvSrvUavHeap();
            // L'index dans le heap est stocké dans la texture
            // Pour simplifier on utilise l'adresse GPU directement
            // (une vraie impl utiliserait un index stable)
            gpuHandle = heap.gpuBase;
            gpuHandle.ptr += (UINT64)b.slot * heap.incrementSize;
        } else if (b.bufId) {
            auto& heap = mDev->CbvSrvUavHeap();
            gpuHandle = heap.gpuBase;
            gpuHandle.ptr += (UINT64)b.slot * heap.incrementSize;
        } else if (b.sampId) {
            auto& heap = mDev->SamplerHeap();
            gpuHandle = heap.gpuBase;
            gpuHandle.ptr += (UINT64)b.slot * heap.incrementSize;
            isSampler = true;
        }

        // Root parameter 1 = CBV/SRV/UAV table, 2 = sampler table
        if (mIsCompute) {
            if (isSampler) mCmdList->SetComputeRootDescriptorTable(2, gpuHandle);
            else           mCmdList->SetComputeRootDescriptorTable(1, gpuHandle);
        } else {
            if (isSampler) mCmdList->SetGraphicsRootDescriptorTable(2, gpuHandle);
            else           mCmdList->SetGraphicsRootDescriptorTable(1, gpuHandle);
        }
    }
}

// =============================================================================
// Push Constants (root constants, paramètre 0 de la root signature)
// =============================================================================
void NkCommandBuffer_DX12::PushConstants(NkShaderStage /*stages*/,
                                          uint32 offset, uint32 size,
                                          const void* data) {
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
void NkCommandBuffer_DX12::BindVertexBuffer(uint32 binding,
                                             NkBufferHandle buf, uint64 offset) {
    ID3D12Resource* res = mDev->GetDX12Buffer(buf.id);
    if (!res) return;
    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = mDev->GetBufferGPUAddr(buf.id) + offset;
    vbv.SizeInBytes    = (UINT)(res->GetDesc().Width - offset);
    vbv.StrideInBytes  = 0; // stride défini dans le pipeline (InputLayout)
    mCmdList->IASetVertexBuffers(binding, 1, &vbv);
}

void NkCommandBuffer_DX12::BindVertexBuffers(uint32 first,
                                              const NkBufferHandle* bufs,
                                              const uint64* offsets, uint32 n) {
    D3D12_VERTEX_BUFFER_VIEW vbvs[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
    n = NK_MIN(n, (uint32)D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
    for (uint32 i = 0; i < n; i++) {
        ID3D12Resource* res = mDev->GetDX12Buffer(bufs[i].id);
        vbvs[i].BufferLocation = mDev->GetBufferGPUAddr(bufs[i].id) + offsets[i];
        vbvs[i].SizeInBytes    = res ? (UINT)(res->GetDesc().Width - offsets[i]) : 0;
        vbvs[i].StrideInBytes  = 0;
    }
    mCmdList->IASetVertexBuffers(first, n, vbvs);
}

void NkCommandBuffer_DX12::BindIndexBuffer(NkBufferHandle buf,
                                            NkIndexFormat fmt, uint64 offset) {
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
void NkCommandBuffer_DX12::Draw(uint32 vtx, uint32 inst,
                                 uint32 firstVtx, uint32 firstInst) {
    mCmdList->DrawInstanced(vtx, inst, firstVtx, firstInst);
}

void NkCommandBuffer_DX12::DrawIndexed(uint32 idx, uint32 inst,
                                        uint32 firstIdx, int32 vtxOff,
                                        uint32 firstInst) {
    mCmdList->DrawIndexedInstanced(idx, inst, firstIdx, vtxOff, firstInst);
}

void NkCommandBuffer_DX12::DrawIndirect(NkBufferHandle buf, uint64 off,
                                         uint32 cnt, uint32 /*stride*/) {
    // Nécessite un ID3D12CommandSignature pré-créé
    // Simplification : on appelle ExecuteIndirect avec une signature draw standard
    (void)buf; (void)off; (void)cnt;
    // TODO: créer et cacher une DrawIndirectSignature dans le device
}

void NkCommandBuffer_DX12::DrawIndexedIndirect(NkBufferHandle buf, uint64 off,
                                                uint32 cnt, uint32 /*stride*/) {
    (void)buf; (void)off; (void)cnt;
    // TODO: signature DrawIndexedIndirect
}

// =============================================================================
// Compute
// =============================================================================
void NkCommandBuffer_DX12::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    mCmdList->Dispatch(gx, gy, gz);
}

void NkCommandBuffer_DX12::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    (void)buf; (void)off;
    // TODO: signature DispatchIndirect
}

// =============================================================================
// Copies
// =============================================================================
void NkCommandBuffer_DX12::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                       const NkBufferCopyRegion& r) {
    mCmdList->CopyBufferRegion(
        mDev->GetDX12Buffer(dst.id), r.dstOffset,
        mDev->GetDX12Buffer(src.id), r.srcOffset,
        r.size);
}

void NkCommandBuffer_DX12::CopyBufferToTexture(NkBufferHandle src,
                                                NkTextureHandle dst,
                                                const NkBufferTextureCopyRegion& r) {
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

void NkCommandBuffer_DX12::CopyTextureToBuffer(NkTextureHandle src,
                                                NkBufferHandle dst,
                                                const NkBufferTextureCopyRegion& r) {
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

void NkCommandBuffer_DX12::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                        const NkTextureCopyRegion& r) {
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

void NkCommandBuffer_DX12::BlitTexture(NkTextureHandle /*src*/,
                                        NkTextureHandle /*dst*/,
                                        const NkTextureCopyRegion& /*r*/,
                                        NkFilter /*f*/) {
    // DX12 n'a pas de blit natif — nécessite un compute ou graphics pass de fullscreen
    // Laissé pour implémentation future dans un BlitHelper
}

// =============================================================================
// Barriers
// =============================================================================
void NkCommandBuffer_DX12::Barrier(const NkBufferBarrier* bb, uint32 bc,
                                    const NkTextureBarrier* tb, uint32 tc) {
    NkVector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.Resize(bc + tc);

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

    if (!barriers.IsEmpty())
        mCmdList->ResourceBarrier((UINT)barriers.Size(), barriers.Data());
}

// =============================================================================
// Debug
// =============================================================================
void NkCommandBuffer_DX12::BeginDebugGroup(const char* name, float r, float g, float b) {
    // PIX events (nécessite WinPixEventRuntime)
    // PIXBeginEvent(mCmdList.Get(), PIX_COLOR_DEFAULT, name);
    (void)name; (void)r; (void)g; (void)b;
}

void NkCommandBuffer_DX12::EndDebugGroup() {
    // PIXEndEvent(mCmdList.Get());
}

void NkCommandBuffer_DX12::InsertDebugLabel(const char* name) {
    // PIXSetMarker(mCmdList.Get(), PIX_COLOR_DEFAULT, name);
    (void)name;
}

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
