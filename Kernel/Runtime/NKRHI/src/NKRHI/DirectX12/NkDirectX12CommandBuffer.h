#pragma once
// =============================================================================
// NkDirectX12CommandBuffer.h — ID3D12GraphicsCommandList wrapper
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#ifdef NK_RHI_DX12_ENABLED
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace nkentseu {

class NkDirectX12Device;

class NkDirectX12CommandBuffer final : public NkICommandBuffer {
public:
    NkDirectX12CommandBuffer(NkDirectX12Device* dev, NkCommandBufferType type);
    ~NkDirectX12CommandBuffer() override;

    bool Begin()  override;
    void End()    override;
    void Reset()  override;
    bool IsValid()              const override { return mCmdList != nullptr; }
    NkCommandBufferType GetType() const override { return mType; }
    ID3D12GraphicsCommandList* GetCmdList() const { return mCmdList.Get(); }

    bool BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb, const NkRect2D& area) override;
    void EndRenderPass() override;

    void SetViewport (const NkViewport& vp) override;
    void SetViewports(const NkViewport* vps, uint32 n) override;
    void SetScissor  (const NkRect2D& r) override;
    void SetScissors (const NkRect2D* r, uint32 n) override;
    // SetClearColor/SetClearDepth = SIGNAL « cette passe doit clear » (loadOp CLEAR).
    // Le render graph ne les appelle QUE pour les passes loadOp=NK_CLEAR (cf.
    // NkRenderGraph.cpp). BeginRenderPass clear UNIQUEMENT si le flag pending est armé,
    // puis le désarme — exactement comme le backend DX11. Sinon une passe LOAD
    // (Overlay2D sur le swapchain) efface l'image déjà composée → écran noir 3D.
    void SetClearColor(float r, float g, float b, float a = 1.f) override {
        mClearColor[0]=r; mClearColor[1]=g; mClearColor[2]=b; mClearColor[3]=a;
        mPendingClearColor = true;
    }
    void SetClearDepth(float depth = 1.f, uint32 stencil = 0) override {
        mClearDepth=depth; mClearStencil=stencil;
        mPendingClearDepth = true;
    }

    void BindGraphicsPipeline(NkPipelineHandle p) override;
    void BindComputePipeline (NkPipelineHandle p) override;
    void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
    void PushConstants(NkShaderStage stages, uint32 offset, uint32 size, const void* data) override;
    void UpdateBuffer(NkBufferHandle, uint64, uint64, const void*) override {} // TODO: staging + CopyBufferRegion

    void BindVertexBuffer (uint32 binding, NkBufferHandle buf, uint64 offset) override;
    void BindVertexBuffers(uint32 first, const NkBufferHandle* bufs, const uint64* offs, uint32 n) override;
    void BindIndexBuffer  (NkBufferHandle buf, NkIndexFormat fmt, uint64 offset) override;

    void Draw             (uint32 v, uint32 i, uint32 fv, uint32 fi) override;
    void DrawIndexed      (uint32 idx, uint32 inst, uint32 fi, int32 vo, uint32 fInst) override;
    void DrawIndirect     (NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;
    void DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;

    void Dispatch         (uint32 gx, uint32 gy, uint32 gz) override;
    void DispatchIndirect (NkBufferHandle buf, uint64 off) override;

    void CopyBuffer         (NkBufferHandle s, NkBufferHandle d, const NkBufferCopyRegion& r) override;
    void CopyBufferToTexture(NkBufferHandle s, NkTextureHandle d, const NkBufferTextureCopyRegion& r) override;
    void CopyTextureToBuffer(NkTextureHandle s, NkBufferHandle d, const NkBufferTextureCopyRegion& r) override;
    void CopyTexture        (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r) override;
    void BlitTexture        (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r, NkFilter f) override;

    void Barrier(const NkBufferBarrier* bb, uint32 bc, const NkTextureBarrier* tb, uint32 tc) override;
    void GenerateMipmaps(NkTextureHandle, NkFilter) override {}

    void BeginDebugGroup (const char* name, float r, float g, float b) override;
    void EndDebugGroup   () override;
    void InsertDebugLabel(const char* name) override;

private:
    NkDirectX12Device*                     mDev = nullptr;
    ComPtr<ID3D12GraphicsCommandList>   mCmdList;
    ComPtr<ID3D12CommandAllocator>      mAllocator; // propre (non partagé avec frames)
    NkCommandBufferType                 mType;
    bool                               mIsCompute = false;
    bool                               mRecording = false;
    // Vrai uniquement quand un pipeline VALIDE (avec root signature) est bindé. Si la
    // création du pipeline a échoué, on saute PushConstants/BindDescriptorSet/Draw pour
    // éviter SetRoot*/Draw sur une command list sans root sig (SIGSEGV driver).
    bool                               mRootSigBound = false;
    uint64                              mActiveColorTexIds[8]{};
    uint32                              mActiveColorCount = 0;
    uint64                              mActiveDepthTexId = 0;
    // Fix #613 : RP de la passe courante (formats RTV/DSV réels du framebuffer lié).
    // Utilisé par BindGraphicsPipeline pour résoudre le PSO format-compatible.
    NkRenderPassHandle                  mActiveRP{};
    uint32                              mVertexStrides[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
    float                              mClearColor[4] = {0.f, 0.f, 0.f, 1.f};
    float                              mClearDepth    = 1.f;
    uint32                             mClearStencil  = 0;
    // ── État de binding FUSIONNÉ (fix multi-set DX12) ───────────────────────────
    // Le renderer lie SET 0 (caméra b0, lights, shadow…) UNE FOIS par passe, puis SET 1
    // (transform/matériau par objet) à chaque objet, en s'appuyant sur la persistance
    // multi-set (DX11/GL/VK gardent le set 0 lié pendant que le set 1 change). Le modèle
    // « table large » DX12 alloue un bloc de ring par BindDescriptorSet et n'y copie QUE
    // les descripteurs de CE set → le set 0 (caméra) était perdu pour les draws d'objets
    // → matrice caméra = garbage → géométrie dégénérée → écran noir 3D. FIX : on conserve
    // un état de binding fusionné (slot → index staging) cumulé sur tous les BindDescriptorSet
    // depuis le dernier BeginRenderPass ; à chaque bind on ré-émet l'INTÉGRALITÉ de l'état
    // fusionné dans un bloc de ring frais → le set 0 survit aux re-binds du set 1.
    static constexpr uint32 kMergedCbv  = 32;  // NUM_CBV (b0..b31, inclut MPC_UBO @ b25)
    static constexpr uint32 kMergedSrv  = 32;  // NUM_SRV
    static constexpr uint32 kMergedUav  = 8;   // NUM_UAV
    static constexpr uint32 kMergedSamp = 32;  // NUM_SAMP
    uint32 mMergedCbv [kMergedCbv]  {};
    uint32 mMergedSrv [kMergedSrv]  {};
    uint32 mMergedUav [kMergedUav]  {};
    uint32 mMergedSamp[kMergedSamp] {};
    // Déduplication du bloc sampler (le heap sampler shader-visible est plafonné à
    // 2048 par D3D12 ; réallouer 32 slots/draw débordait après 64 draws). On réutilise
    // le dernier bloc ring si l'état sampler est inchangé. Réinit par ResetMergedBindings.
    uint32 mLastSampBase       = 0xFFFFFFFFu;
    uint32 mLastSampCount      = 0;
    uint32 mLastSampCopiedBase = 0xFFFFFFFFu;
    uint32 mLastSampVals[kMergedSamp] {};
    void ResetMergedBindings();
    // Clear conditionnel (parité DX11) : armé par SetClearColor/SetClearDepth, consommé
    // et désarmé par BeginRenderPass. Une passe sans clear (loadOp LOAD) préserve la cible.
    bool                               mPendingClearColor = false;
    bool                               mPendingClearDepth = false;
};

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
