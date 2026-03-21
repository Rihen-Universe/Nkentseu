// =============================================================================
// NkCommandBuffer_DX11.cpp
// =============================================================================
#ifdef NK_RHI_DX11_ENABLED
#include "NkDX11CommandBuffer.h"
#include "NkDX11Device.h"
#include "NKCore/NkMacros.h"
#include <cstring>

namespace nkentseu {

NkCommandBuffer_DX11::NkCommandBuffer_DX11(NkDeviceDX11* dev, NkCommandBufferType type)
    : mDev(dev), mType(type) {
    dev->D3D()->CreateDeferredContext1(0, &mDeferred);
}

NkCommandBuffer_DX11::~NkCommandBuffer_DX11() {
    if (mCmdList)  { mCmdList->Release();  mCmdList  = nullptr; }
    if (mDeferred) { mDeferred->Release(); mDeferred = nullptr; }
}

void NkCommandBuffer_DX11::Begin() {
    if (mCmdList) { mCmdList->Release(); mCmdList = nullptr; }
    // Le contexte différé est déjà prêt à enregistrer après CreateDeferredContext
}

void NkCommandBuffer_DX11::End() {
    if (mDeferred) mDeferred->FinishCommandList(FALSE, &mCmdList);
}

void NkCommandBuffer_DX11::Reset() {
    if (mCmdList) { mCmdList->Release(); mCmdList = nullptr; }
    if (mDeferred) mDeferred->ClearState();
}

void NkCommandBuffer_DX11::Execute(NkDeviceDX11* dev) {
    if (mCmdList) dev->Ctx()->ExecuteCommandList(mCmdList, FALSE);
}

// =============================================================================
// Render Pass
// =============================================================================
void NkCommandBuffer_DX11::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                            NkFramebufferHandle fb,
                                            const NkRect2D& area) {
    auto* fbo = mDev->GetFBO(fb.id);
    if (!fbo) return;

    mDeferred->OMSetRenderTargets(fbo->rtvCount, fbo->rtvs, fbo->dsv);

    // Clear (on utilise les clear values par défaut; une vraie impl lirait le NkRenderPassDesc)
    float clearColor[4] = {0.f, 0.f, 0.f, 1.f};
    for (uint32 i = 0; i < fbo->rtvCount; i++)
        if (fbo->rtvs[i]) mDeferred->ClearRenderTargetView(fbo->rtvs[i], clearColor);
    if (fbo->dsv)
        mDeferred->ClearDepthStencilView(fbo->dsv,
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

    D3D11_VIEWPORT vp{ (float)area.x, (float)area.y,
                        (float)area.width, (float)area.height, 0.f, 1.f };
    mDeferred->RSSetViewports(1, &vp);
}

// =============================================================================
// Viewport & Scissor
// =============================================================================
void NkCommandBuffer_DX11::SetViewport(const NkViewport& vp) {
    D3D11_VIEWPORT d{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
    mDeferred->RSSetViewports(1, &d);
}

void NkCommandBuffer_DX11::SetViewports(const NkViewport* vps, uint32 n) {
    D3D11_VIEWPORT d[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    n = NK_MIN(n, (uint32)D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for (uint32 i = 0; i < n; i++)
        d[i] = { vps[i].x, vps[i].y, vps[i].width, vps[i].height, vps[i].minDepth, vps[i].maxDepth };
    mDeferred->RSSetViewports(n, d);
}

void NkCommandBuffer_DX11::SetScissor(const NkRect2D& r) {
    D3D11_RECT rect{ r.x, r.y, (LONG)(r.x + r.width), (LONG)(r.y + r.height) };
    mDeferred->RSSetScissorRects(1, &rect);
}

void NkCommandBuffer_DX11::SetScissors(const NkRect2D* rects, uint32 n) {
    D3D11_RECT d[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    n = NK_MIN(n, (uint32)D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for (uint32 i = 0; i < n; i++)
        d[i] = { rects[i].x, rects[i].y,
                 (LONG)(rects[i].x + rects[i].width),
                 (LONG)(rects[i].y + rects[i].height) };
    mDeferred->RSSetScissorRects(n, d);
}

// =============================================================================
// Pipeline Binding
// =============================================================================
void NkCommandBuffer_DX11::BindGraphicsPipeline(NkPipelineHandle p) {
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe || pipe->isCompute) return;
    mCurrentPipeId = p.id;

    mDeferred->VSSetShader(pipe->vs, nullptr, 0);
    mDeferred->PSSetShader(pipe->ps, nullptr, 0);
    mDeferred->GSSetShader(nullptr,  nullptr, 0); // reset GS si non utilisé
    if (pipe->il)  mDeferred->IASetInputLayout(pipe->il);
    if (pipe->rs)  mDeferred->RSSetState(pipe->rs);
    if (pipe->dss) mDeferred->OMSetDepthStencilState(pipe->dss, 0);
    if (pipe->bs) {
        float factors[4] = { 0.f, 0.f, 0.f, 0.f };
        mDeferred->OMSetBlendState(pipe->bs, factors, 0xFFFFFFFF);
    }
    mDeferred->IASetPrimitiveTopology(pipe->topology);
}

void NkCommandBuffer_DX11::BindComputePipeline(NkPipelineHandle p) {
    auto* pipe = mDev->GetPipeline(p.id);
    if (!pipe || !pipe->isCompute) return;
    mDeferred->CSSetShader(pipe->cs, nullptr, 0);
}

// =============================================================================
// Descriptor Set
// =============================================================================
void NkCommandBuffer_DX11::BindDescriptorSet(NkDescSetHandle set,
                                              uint32 /*idx*/,
                                              uint32* /*off*/, uint32 /*cnt*/) {
    auto* ds = mDev->GetDescSet(set.id);
    if (!ds) return;
    for (uint32 i = 0; i < ds->count; i++) {
        auto& s = ds->slots[i];
        UINT slot = s.slot;
        switch (s.kind) {
            case NkDX11DescSet::Slot::Buffer:
                if (s.type == NkDescriptorType::NK_UNIFORM_BUFFER) {
                    mDeferred->VSSetConstantBuffers(slot, 1, &s.buf);
                    mDeferred->PSSetConstantBuffers(slot, 1, &s.buf);
                    mDeferred->CSSetConstantBuffers(slot, 1, &s.buf);
                } else {
                    // UAV (compute)
                    mDeferred->CSSetUnorderedAccessViews(slot, 1, &s.uav, nullptr);
                }
                break;
            case NkDX11DescSet::Slot::Texture:
                mDeferred->VSSetShaderResources(slot, 1, &s.srv);
                mDeferred->PSSetShaderResources(slot, 1, &s.srv);
                mDeferred->CSSetShaderResources(slot, 1, &s.srv);
                if (s.uav) mDeferred->CSSetUnorderedAccessViews(slot, 1, &s.uav, nullptr);
                break;
            case NkDX11DescSet::Slot::Sampler:
                mDeferred->VSSetSamplers(slot, 1, &s.ss);
                mDeferred->PSSetSamplers(slot, 1, &s.ss);
                mDeferred->CSSetSamplers(slot, 1, &s.ss);
                break;
            default: break;
        }
    }
}

// =============================================================================
// Vertex / Index Buffers
// =============================================================================
void NkCommandBuffer_DX11::BindVertexBuffer(uint32 binding,
                                             NkBufferHandle buf, uint64 offset) {
    ID3D11Buffer* b = mDev->GetDXBuffer(buf.id);
    UINT stride = 0;
    auto* pipe = mDev->GetPipeline(mCurrentPipeId);
    if (pipe && binding < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
        stride = pipe->strides[binding];
    UINT off = (UINT)offset;
    mDeferred->IASetVertexBuffers(binding, 1, &b, &stride, &off);
}

void NkCommandBuffer_DX11::BindVertexBuffers(uint32 first,
                                              const NkBufferHandle* bufs,
                                              const uint64* offsets, uint32 n) {
    ID3D11Buffer* bs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]     = {};
    UINT offs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]        = {};
    n = NK_MIN(n, (uint32)D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
    auto* pipe = mDev->GetPipeline(mCurrentPipeId);
    for (uint32 i = 0; i < n; i++) {
        bs[i]   = mDev->GetDXBuffer(bufs[i].id);
        offs[i] = (UINT)offsets[i];
        if (pipe && (first + i) < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            strides[i] = pipe->strides[first + i];
    }
    mDeferred->IASetVertexBuffers(first, n, bs, strides, offs);
}

void NkCommandBuffer_DX11::BindIndexBuffer(NkBufferHandle buf,
                                            NkIndexFormat fmt, uint64 offset) {
    DXGI_FORMAT dxFmt = fmt == NkIndexFormat::NK_UINT16
                      ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    mDeferred->IASetIndexBuffer(mDev->GetDXBuffer(buf.id), dxFmt, (UINT)offset);
}

// =============================================================================
// Draw
// =============================================================================
void NkCommandBuffer_DX11::Draw(uint32 vtx, uint32 inst,
                                 uint32 firstVtx, uint32 firstInst) {
    if (inst > 1)
        mDeferred->DrawInstanced(vtx, inst, firstVtx, firstInst);
    else
        mDeferred->Draw(vtx, firstVtx);
}

void NkCommandBuffer_DX11::DrawIndexed(uint32 idx, uint32 inst,
                                        uint32 firstIdx, int32 vtxOff,
                                        uint32 firstInst) {
    if (inst > 1)
        mDeferred->DrawIndexedInstanced(idx, inst, firstIdx, vtxOff, firstInst);
    else
        mDeferred->DrawIndexed(idx, firstIdx, vtxOff);
}

void NkCommandBuffer_DX11::DrawIndirect(NkBufferHandle buf, uint64 off,
                                         uint32 /*cnt*/, uint32 /*stride*/) {
    mDeferred->DrawInstancedIndirect(mDev->GetDXBuffer(buf.id), (UINT)off);
}

void NkCommandBuffer_DX11::DrawIndexedIndirect(NkBufferHandle buf, uint64 off,
                                                uint32 /*cnt*/, uint32 /*stride*/) {
    mDeferred->DrawIndexedInstancedIndirect(mDev->GetDXBuffer(buf.id), (UINT)off);
}

// =============================================================================
// Compute
// =============================================================================
void NkCommandBuffer_DX11::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
    mDeferred->Dispatch(gx, gy, gz);
}

void NkCommandBuffer_DX11::DispatchIndirect(NkBufferHandle buf, uint64 off) {
    mDeferred->DispatchIndirect(mDev->GetDXBuffer(buf.id), (UINT)off);
}

// =============================================================================
// Copies
// =============================================================================
void NkCommandBuffer_DX11::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                       const NkBufferCopyRegion& r) {
    D3D11_BOX box{ (UINT)r.srcOffset, 0, 0,
                   (UINT)(r.srcOffset + r.size), 1, 1 };
    mDeferred->CopySubresourceRegion(
        mDev->GetDXBuffer(dst.id), 0, (UINT)r.dstOffset, 0, 0,
        mDev->GetDXBuffer(src.id), 0, &box);
}

void NkCommandBuffer_DX11::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                        const NkTextureCopyRegion& r) {
    // On accède aux textures via les buffers (les textures DX11 sont dans mTextures)
    // Passage par les internals du device — approche simplifiée
    // Pour une vraie implémentation : enregistrer dans le deferred context via
    // CopySubresourceRegion sur les ID3D11Texture2D
    (void)src; (void)dst; (void)r;
}

void NkCommandBuffer_DX11::GenerateMipmaps(NkTextureHandle tex, NkFilter /*f*/) {
    // Délégué au contexte immédiat (pas de support sur deferred en DX11)
    mDev->GenerateMipmaps(tex, NkFilter::NK_LINEAR);
}

// =============================================================================
// Debug (nécessite ID3DUserDefinedAnnotation)
// =============================================================================
void NkCommandBuffer_DX11::BeginDebugGroup(const char* name, float, float, float) {
    ID3DUserDefinedAnnotation* ann = nullptr;
    if (SUCCEEDED(mDeferred->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                             (void**)&ann))) {
        wchar_t wname[256] = {};
        mbstowcs(wname, name, 255);
        ann->BeginEvent(wname);
        ann->Release();
    }
}

void NkCommandBuffer_DX11::EndDebugGroup() {
    ID3DUserDefinedAnnotation* ann = nullptr;
    if (SUCCEEDED(mDeferred->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                             (void**)&ann))) {
        ann->EndEvent();
        ann->Release();
    }
}

void NkCommandBuffer_DX11::InsertDebugLabel(const char* name) {
    ID3DUserDefinedAnnotation* ann = nullptr;
    if (SUCCEEDED(mDeferred->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                             (void**)&ann))) {
        wchar_t wname[256] = {};
        mbstowcs(wname, name, 255);
        ann->SetMarker(wname);
        ann->Release();
    }
}

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED
