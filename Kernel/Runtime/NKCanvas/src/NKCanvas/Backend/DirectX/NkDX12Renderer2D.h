#pragma once
// =============================================================================
// NkDX12Renderer2D.h — DirectX 12 2D renderer backend
// Uses root constants for projection (no CBV heap needed for a 64-byte matrix).
// Dynamic ring buffer for VB/IB (double-buffered to avoid GPU/CPU stalls).
// =============================================================================
#include "NKCanvas/Renderer/Batch/NkBatchRenderer2D.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkTextureBackend.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
using Microsoft::WRL::ComPtr;

namespace nkentseu {
    namespace renderer {

        class NkDX12Renderer2D final : public NkBatchRenderer2D {
        public:
            NkDX12Renderer2D()  = default;
            ~NkDX12Renderer2D() override { if (IsValid()) Shutdown(); }

            bool Initialize(NkIGraphicsContext* ctx) override;
            void Shutdown()                          override;
            bool IsValid()                   const   override { return mIsValid; }
            void Clear(const NkColor2D& col)         override;

        protected:
            void BeginBackend()  override;
            void EndBackend()    override;
            void SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                            const NkVertex2D* verts, uint32 vCount,
                            const uint32*     idx,   uint32 iCount) override;
            void UploadProjection(const float32 proj[16]) override;

        private:
            bool CreateRootSignature();
            bool CreatePSOs();
            bool CreateBuffers();
            bool CreateDescriptorHeap();
            bool CreateWhiteTexture();
            bool CreateSampler();

            void TransitionResource(ID3D12GraphicsCommandList4* cmd,
                                    ID3D12Resource* res,
                                    D3D12_RESOURCE_STATES from,
                                    D3D12_RESOURCE_STATES to);

            NkIGraphicsContext*  mCtx     = nullptr;
            bool                 mIsValid = false;

            ComPtr<ID3D12Device5>           mDevice;
            ComPtr<ID3D12GraphicsCommandList4> mCmdList;

            ComPtr<ID3D12RootSignature>     mRootSig;
            ComPtr<ID3D12PipelineState>     mPSOAlpha;
            ComPtr<ID3D12PipelineState>     mPSOAdd;
            ComPtr<ID3D12PipelineState>     mPSOMul;
            ComPtr<ID3D12PipelineState>     mPSONone;

            // Dynamic vertex/index buffers (host-visible upload heap)
            ComPtr<ID3D12Resource>          mVB;
            ComPtr<ID3D12Resource>          mIB;
            void*                           mVBMap = nullptr;
            void*                           mIBMap = nullptr;
            D3D12_VERTEX_BUFFER_VIEW        mVBView{};
            D3D12_INDEX_BUFFER_VIEW         mIBView{};

            // SRV descriptor heap (one entry per texture + 1 for white)
            ComPtr<ID3D12DescriptorHeap>    mSRVHeap;
            uint32                          mSRVDescSize = 0;
            uint32                          mNextSRVSlot = 0;
            static constexpr uint32         kMaxSRVSlots = 256;

            // Sampler heap
            ComPtr<ID3D12DescriptorHeap>    mSamplerHeap;

            // White 1x1 texture
            ComPtr<ID3D12Resource>          mWhiteTex;
            uint32                          mWhiteSRVSlot = 0;

            // Per-texture SRV slot cache
            struct TexSRVEntry {
                const NkTexture* texture = nullptr;
                uint32           slot    = 0;
            };
            NkVector<TexSRVEntry> mTexSRVCache;

            float32 mProjection[16] = {};

            // Helper: get GPU handle for a SRV slot
            D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32 slot) const;
            D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32 slot) const;

            uint32 GetOrCreateSRVSlot(const NkTexture* tex);

            // PSO creation helper
            bool MakePSO(D3D12_BLEND_DESC blendDesc, ComPtr<ID3D12PipelineState>& out,
                        ID3DBlob* vsBlob, ID3DBlob* psBlob);

            // ── NkTextureBackend dispatch (registry globale, cf. .cpp) ────────────
            // Les 5 callbacks doivent etre statiques pour matcher la signature de
            // NkTextureBackend (function pointers sans `this`). Une registry
            // globale (gDX12Registry) tient device/cmdQueue/srvHeap captures
            // depuis Initialize() ainsi que la table des entrees.
            // NOTE : SetFilter/SetWrap sont des NO-OP en DX12 car la root sig
            // utilise un sampler de descriptor heap unique (cree dans CreateSampler
            // avec LINEAR + CLAMP). Pour du per-texture filter/wrap il faudrait
            // ajouter un sampler heap multi-slots ; non implemente pour l'instant.
            static uint32 CreateDX12Texture(uint32 w, uint32 h, const uint8* rgba);
            static void   UpdateDX12Texture(uint32 id, uint32 x, uint32 y,
                                            uint32 w, uint32 h, const uint8* rgba);
            static void   DeleteDX12Texture(uint32 id);
            static void   SetDX12TextureFilter(uint32 id, NkTextureFilter f);
            static void   SetDX12TextureWrap  (uint32 id, NkTextureWrap   w);
        };

    } // namespace renderer
} // namespace nkentseu
#endif // WINDOWS