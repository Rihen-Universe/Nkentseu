// =============================================================================
// NkDirectX11CommandBuffer.cpp
// =============================================================================
#ifdef NK_RHI_DX11_ENABLED
#include "NkDirectX11CommandBuffer.h"
#include "NkDirectX11Device.h"
#include <cstring>
#include <algorithm>

namespace nkentseu {

    NkDirectX11CommandBuffer::NkDirectX11CommandBuffer(NkDirectX11Device* dev, NkCommandBufferType type)
        : mDev(dev), mType(type) {
        dev->D3D()->CreateDeferredContext1(0, &mDeferred);
    }

    NkDirectX11CommandBuffer::~NkDirectX11CommandBuffer() {
        if (mCmdList)  { mCmdList->Release();  mCmdList  = nullptr; }
        if (mDeferred) { mDeferred->Release(); mDeferred = nullptr; }
    }

    bool NkDirectX11CommandBuffer::Begin() {
        if (!mDeferred) return false;
        if (mCmdList) { mCmdList->Release(); mCmdList = nullptr; }
        // Le contexte différé est déjà prêt à enregistrer après CreateDeferredContext
        return true;
    }

    void NkDirectX11CommandBuffer::End() {
        if (mDev) mDev->DrainInfoQueue(); // DIAG : log validation AVANT FinishCommandList (qui crashe)
        if (mDeferred) mDeferred->FinishCommandList(FALSE, &mCmdList);
        if (mDev) mDev->DrainInfoQueue(); // DIAG : log validation générée par FinishCommandList
    }

    void NkDirectX11CommandBuffer::Reset() {
        if (mCmdList) { mCmdList->Release(); mCmdList = nullptr; }
        if (mDeferred) mDeferred->ClearState();
    }

    void NkDirectX11CommandBuffer::Execute(NkDirectX11Device* dev) {
        if (mCmdList) dev->Ctx()->ExecuteCommandList(mCmdList, FALSE);
    }

    // =============================================================================
    // Render Pass
    // =============================================================================
    bool NkDirectX11CommandBuffer::BeginRenderPass(NkRenderPassHandle /*rp*/,
                                                NkFramebufferHandle fb,
                                                const NkRect2D& area) {
        if (!mDeferred || !fb.IsValid() || area.width <= 0 || area.height <= 0) return false;
        auto* fbo = mDev->GetFBO(fb.id);
        if (!fbo) return false;

        mDeferred->OMSetRenderTargets(fbo->rtvCount, fbo->rtvs, fbo->dsv);

        // Clear UNIQUEMENT si demandé (loadOp CLEAR → SetClearColor/Depth appelé). Une
        // passe LOAD (Overlay2D sur swapchain) ne doit PAS effacer l'image déjà composée.
        if (mPendingClearColor)
            for (uint32 i = 0; i < fbo->rtvCount; i++)
                if (fbo->rtvs[i]) mDeferred->ClearRenderTargetView(fbo->rtvs[i], mClearColor);
        if (mPendingClearDepth && fbo->dsv)
            mDeferred->ClearDepthStencilView(fbo->dsv,
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, mClearDepth, (UINT8)mClearStencil);
        mPendingClearColor = false;
        mPendingClearDepth = false;

        D3D11_VIEWPORT vp{ (float)area.x, (float)area.y,
                            (float)area.width, (float)area.height, 0.f, 1.f };
        mDeferred->RSSetViewports(1, &vp);
        return true;
    }

    // =============================================================================
    // Viewport & Scissor
    // =============================================================================
    void NkDirectX11CommandBuffer::SetViewport(const NkViewport& vp) {
        D3D11_VIEWPORT d{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
        mDeferred->RSSetViewports(1, &d);
    }

    void NkDirectX11CommandBuffer::SetViewports(const NkViewport* vps, uint32 n) {
        D3D11_VIEWPORT d[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        n = math::NkMin(n, (uint32)D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
        for (uint32 i = 0; i < n; i++)
            d[i] = { vps[i].x, vps[i].y, vps[i].width, vps[i].height, vps[i].minDepth, vps[i].maxDepth };
        mDeferred->RSSetViewports(n, d);
    }

    void NkDirectX11CommandBuffer::SetScissor(const NkRect2D& r) {
        D3D11_RECT rect{ r.x, r.y, (LONG)(r.x + r.width), (LONG)(r.y + r.height) };
        mDeferred->RSSetScissorRects(1, &rect);
    }

    void NkDirectX11CommandBuffer::SetScissors(const NkRect2D* rects, uint32 n) {
        D3D11_RECT d[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        n = math::NkMin(n, (uint32)D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
        for (uint32 i = 0; i < n; i++)
            d[i] = { rects[i].x, rects[i].y,
                    (LONG)(rects[i].x + rects[i].width),
                    (LONG)(rects[i].y + rects[i].height) };
        mDeferred->RSSetScissorRects(n, d);
    }

    // =============================================================================
    // Pipeline Binding
    // =============================================================================
    void NkDirectX11CommandBuffer::BindGraphicsPipeline(NkPipelineHandle p) {
        auto* pipe = mDev->GetPipeline(p.id);
        if (!pipe || pipe->isCompute) return;

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
        for (uint32 i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i) {
            mVertexStrides[i] = pipe->vertexStrides[i];
        }
        mDeferred->IASetPrimitiveTopology(pipe->topology);
    }

    void NkDirectX11CommandBuffer::BindComputePipeline(NkPipelineHandle p) {
        auto* pipe = mDev->GetPipeline(p.id);
        if (!pipe || !pipe->isCompute) return;
        mDeferred->CSSetShader(pipe->cs, nullptr, 0);
    }

    // =============================================================================
    // Descriptor Set
    // =============================================================================
    void NkDirectX11CommandBuffer::BindDescriptorSet(NkDescSetHandle set,
                                                uint32 /*idx*/,
                                                uint32* /*off*/, uint32 /*cnt*/) {
        auto* ds = mDev->GetDescSet(set.id);
        if (!ds) return;
        // Pool de samplers partagés NkSL→HLSL : binder une fois les 4 samplers
        // statiques à s12..s15 (wrap/clamp/point/comparison). Les shaders NkSL
        // référencent ce pool ; les samplers par-descripteur à binding élevé sont
        // ignorés (cf. NkSLCodeGenHLSL kNkSLSamplerPoolBase=12). Inoffensif pour les
        // shaders legacy (ils bindent s0..s11).
        {
            ID3D11SamplerState* const* pool = mDev->PoolSamplers();
            if (pool && pool[0]) {
                mDeferred->VSSetSamplers(12, 4, pool);
                mDeferred->PSSetSamplers(12, 4, pool);
            }
        }
        // D3D11 : 128 slots SRV (t0..t127) mais seulement 16 slots sampler (s0..s15,
        // D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT). NKRenderer utilise des bindings
        // élevés (16, 25, 26, 27…) dans un seul set : un sampler à slot>=16 passé à
        // *SetSamplers est hors-limites -> crash dans d3d11.dll. On garde le SRV (slot
        // valide en t#) et on saute le sampler hors-limites.
        // TODO #4 : remapping propre des registres sampler en SPIRV-Cross (s0..s15)
        // pour que les textures à binding>=16 disposent quand même d'un sampler.
        // s12..s15 sont réservés au pool partagé NkSL : les samplers par-descripteur
        // ne bindent qu'en dessous (s0..s11) pour ne pas l'écraser.
        constexpr UINT kMaxSamplerSlot = 12; // < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT (16), réserve s12..s15
        constexpr UINT kMaxSrvSlot     = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; // 128
        constexpr UINT kMaxCBufSlot    = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; // 14 (b0..b13)
        for (uint32 i = 0; i < ds->count; i++) {
            auto& s = ds->slots[i];
            UINT slot = s.slot;
            // Garde anti-crash : range de slot ET pointeur non-null. Un shader qui a
            // échoué à compiler (ex. PostProcess .vk.glsl→SPIRV-Cross KO) laisse des
            // SRV/sampler/CB null ou pendants -> *Set*(slot, 1, &ptr) crashe dans
            // d3d11.dll. On saute proprement (la scène rend sans ce binding).
            const bool srvOk     = slot < kMaxSrvSlot     && s.srv != nullptr;
            const bool samplerOk = slot < kMaxSamplerSlot && s.ss  != nullptr;
            // DX11 = 14 cbuffers max (b0..b13). NKRenderer binde des UBO à binding élevé
            // (MaterialCollection=25, VoxelAO=27…) -> VSSetConstantBuffers(25) = CORRUPTION
            // out-of-range -> crash. Les shaders NkSL ne déclarent PAS ces b>=14 (sinon
            // X4567), donc cet UBO leur est inutile : on saute proprement (pas de remap).
            const bool cbufOk    = slot < kMaxCBufSlot    && s.buf != nullptr;
            switch (s.kind) {
                case NkDX11DescSet::Slot::Buffer:
                    if (s.type == NkDescriptorType::NK_UNIFORM_BUFFER) {
                        if (cbufOk) {
                            mDeferred->VSSetConstantBuffers(slot, 1, &s.buf);
                            mDeferred->PSSetConstantBuffers(slot, 1, &s.buf);
                            mDeferred->CSSetConstantBuffers(slot, 1, &s.buf);
                        }
                    } else {
                        // UAV (compute)
                        mDeferred->CSSetUnorderedAccessViews(slot, 1, &s.uav, nullptr);
                    }
                    break;
                case NkDX11DescSet::Slot::Texture:
                    if (srvOk) {
                        mDeferred->VSSetShaderResources(slot, 1, &s.srv);
                        mDeferred->PSSetShaderResources(slot, 1, &s.srv);
                        mDeferred->CSSetShaderResources(slot, 1, &s.srv);
                        if (s.uav) mDeferred->CSSetUnorderedAccessViews(slot, 1, &s.uav, nullptr);
                    }
                    break;
                case NkDX11DescSet::Slot::Sampler:
                    // Samplers par-descripteur NON bindés : les shaders NkSL→HLSL
                    // échantillonnent via le POOL partagé (s12..s15, bindé en tête).
                    // Évite aussi un crash sur sampler pendant (PostProcess).
                    (void)samplerOk;
                    break;
                case NkDX11DescSet::Slot::TextureAndSampler:
                    if (srvOk) {
                        mDeferred->VSSetShaderResources(slot, 1, &s.srv);
                        mDeferred->PSSetShaderResources(slot, 1, &s.srv);
                        mDeferred->CSSetShaderResources(slot, 1, &s.srv);
                    }
                    // Sampler par-descripteur ignoré (pool partagé, cf. ci-dessus).
                    break;
                default: break;
            }
        }
    }

    // =============================================================================
    // Push Constants — émulés par un cbuffer dynamique à b13 (DX11 n'en a pas).
    // Le générateur HLSL émet les blocs push_constant en `register(b13)`. On
    // accumule dans un shadow CPU (gère les writes partiels par offset) puis on
    // uploade tout le cbuffer et on le binde à b13 (VS+PS+CS).
    // =============================================================================
    void NkDirectX11CommandBuffer::PushConstants(NkShaderStage /*stages*/, uint32 offset,
                                                 uint32 size, const void* data) {
        if (!data || size == 0) return;
        const uint32 cap = NkDirectX11Device::kPushConstantBytes;
        if (offset >= cap) return;
        if (offset + size > cap) size = cap - offset;
        memcpy(mPushCpu + offset, data, size);

        ID3D11Buffer* cb = mDev->PushConstantCB();
        if (!cb) return;
        // UpdateSubresource enregistre la copie dans le command list différé (le buffer
        // est DEFAULT, pas DYNAMIC → pas de Map sur contexte différé).
        mDeferred->UpdateSubresource(cb, 0, nullptr, mPushCpu, 0, 0);
        const UINT reg = NkDirectX11Device::kPushConstantReg; // b13
        mDeferred->VSSetConstantBuffers(reg, 1, &cb);
        mDeferred->PSSetConstantBuffers(reg, 1, &cb);
        mDeferred->CSSetConstantBuffers(reg, 1, &cb);
    }

    // =============================================================================
    // Vertex / Index Buffers
    // =============================================================================
    void NkDirectX11CommandBuffer::BindVertexBuffer(uint32 binding,
                                                NkBufferHandle buf, uint64 offset) {
        ID3D11Buffer* b = mDev->GetDXBuffer(buf.id);
        UINT stride = (binding < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            ? mVertexStrides[binding]
            : 0;
        UINT off    = (UINT)offset;
        mDeferred->IASetVertexBuffers(binding, 1, &b, &stride, &off);
    }

    void NkDirectX11CommandBuffer::BindVertexBuffers(uint32 first,
                                                const NkBufferHandle* bufs,
                                                const uint64* offsets, uint32 n) {
        ID3D11Buffer* bs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
        UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]     = {};
        UINT offs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]        = {};
        n = math::NkMin(n, (uint32)D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
        for (uint32 i = 0; i < n; i++) {
            bs[i]   = mDev->GetDXBuffer(bufs[i].id);
            const uint32 slot = first + i;
            strides[i] = (slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) ? mVertexStrides[slot] : 0;
            offs[i] = (UINT)offsets[i];
        }
        mDeferred->IASetVertexBuffers(first, n, bs, strides, offs);
    }

    void NkDirectX11CommandBuffer::BindIndexBuffer(NkBufferHandle buf,
                                                NkIndexFormat fmt, uint64 offset) {
        DXGI_FORMAT dxFmt = fmt == NkIndexFormat::NK_UINT16
                        ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        mDeferred->IASetIndexBuffer(mDev->GetDXBuffer(buf.id), dxFmt, (UINT)offset);
    }

    // =============================================================================
    // Draw
    // =============================================================================
    void NkDirectX11CommandBuffer::Draw(uint32 vtx, uint32 inst,
                                    uint32 firstVtx, uint32 firstInst) {
        if (inst > 1)
            mDeferred->DrawInstanced(vtx, inst, firstVtx, firstInst);
        else
            mDeferred->Draw(vtx, firstVtx);
    }

    void NkDirectX11CommandBuffer::DrawIndexed(uint32 idx, uint32 inst,
                                            uint32 firstIdx, int32 vtxOff,
                                            uint32 firstInst) {
        if (inst > 1)
            mDeferred->DrawIndexedInstanced(idx, inst, firstIdx, vtxOff, firstInst);
        else
            mDeferred->DrawIndexed(idx, firstIdx, vtxOff);
    }

    void NkDirectX11CommandBuffer::DrawIndirect(NkBufferHandle buf, uint64 off,
                                            uint32 /*cnt*/, uint32 /*stride*/) {
        mDeferred->DrawInstancedIndirect(mDev->GetDXBuffer(buf.id), (UINT)off);
    }

    void NkDirectX11CommandBuffer::DrawIndexedIndirect(NkBufferHandle buf, uint64 off,
                                                    uint32 /*cnt*/, uint32 /*stride*/) {
        mDeferred->DrawIndexedInstancedIndirect(mDev->GetDXBuffer(buf.id), (UINT)off);
    }

    // =============================================================================
    // Compute
    // =============================================================================
    void NkDirectX11CommandBuffer::Dispatch(uint32 gx, uint32 gy, uint32 gz) {
        mDeferred->Dispatch(gx, gy, gz);
    }

    void NkDirectX11CommandBuffer::DispatchIndirect(NkBufferHandle buf, uint64 off) {
        mDeferred->DispatchIndirect(mDev->GetDXBuffer(buf.id), (UINT)off);
    }

    // =============================================================================
    // Copies
    // =============================================================================
    void NkDirectX11CommandBuffer::CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                                        const NkBufferCopyRegion& r) {
        D3D11_BOX box{ (UINT)r.srcOffset, 0, 0,
                    (UINT)(r.srcOffset + r.size), 1, 1 };
        mDeferred->CopySubresourceRegion(
            mDev->GetDXBuffer(dst.id), 0, (UINT)r.dstOffset, 0, 0,
            mDev->GetDXBuffer(src.id), 0, &box);
    }

    void NkDirectX11CommandBuffer::CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                                            const NkTextureCopyRegion& r) {
        // On accède aux textures via les buffers (les textures DX11 sont dans mTextures)
        // Passage par les internals du device — approche simplifiée
        // Pour une vraie implémentation : enregistrer dans le deferred context via
        // CopySubresourceRegion sur les ID3D11Texture2D
        (void)src; (void)dst; (void)r;
    }

    void NkDirectX11CommandBuffer::GenerateMipmaps(NkTextureHandle tex, NkFilter /*f*/) {
        // Délégué au contexte immédiat (pas de support sur deferred en DX11)
        mDev->GenerateMipmaps(tex, NkFilter::NK_LINEAR);
    }

    // =============================================================================
    // Debug (nécessite ID3DUserDefinedAnnotation)
    // =============================================================================
    void NkDirectX11CommandBuffer::BeginDebugGroup(const char* name, float, float, float) {
        ID3DUserDefinedAnnotation* ann = nullptr;
        if (SUCCEEDED(mDeferred->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                                (void**)&ann))) {
            wchar_t wname[256] = {};
            mbstowcs(wname, name, 255);
            ann->BeginEvent(wname);
            ann->Release();
        }
    }

    void NkDirectX11CommandBuffer::EndDebugGroup() {
        ID3DUserDefinedAnnotation* ann = nullptr;
        if (SUCCEEDED(mDeferred->QueryInterface(__uuidof(ID3DUserDefinedAnnotation),
                                                (void**)&ann))) {
            ann->EndEvent();
            ann->Release();
        }
    }

    void NkDirectX11CommandBuffer::InsertDebugLabel(const char* name) {
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


