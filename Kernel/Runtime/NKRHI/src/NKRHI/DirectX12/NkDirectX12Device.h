#pragma once
// =============================================================================
// NkRHI_Device_DX12.h — Backend DirectX 12
// D3D12 avec descriptor heaps, command allocators par frame,
// resource barriers explicites et pipeline state objects.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkRecursiveMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"
#include <queue>
#include "NKContainers/Functional/NkFunction.h"

#ifdef NK_RHI_DX12_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

namespace nkentseu {

class NkDirectX12CommandBuffer;

// =============================================================================
// Descriptor Heap Manager (CPU + GPU ranges)
// =============================================================================
struct NkDX12DescHeap {
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  cpuBase{};
    D3D12_GPU_DESCRIPTOR_HANDLE  gpuBase{};
    UINT                         incrementSize = 0;
    UINT                         capacity      = 0;
    UINT                         allocated     = 0;

    bool overflowed = false;  ///< passé à true dès qu'AllocCPU dépasse capacity.

    D3D12_CPU_DESCRIPTOR_HANDLE AllocCPU() {
        // Garde-fou anti-OOB : un handle hors-limites passé à Create*View provoque
        // un "Removing Device" silencieux. Si le heap est plein, on clamp sur le
        // dernier slot (rendu faux par aliasing mais pas de device-removal) et on
        // marque overflowed pour diagnostic / agrandissement futur du heap.
        UINT slot = (capacity > 0 && allocated >= capacity) ? (capacity - 1) : allocated;
        if (capacity > 0 && allocated >= capacity) overflowed = true;
        D3D12_CPU_DESCRIPTOR_HANDLE h = cpuBase;
        h.ptr += (SIZE_T)slot * incrementSize;
        if (allocated < capacity) allocated++;
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GPUFrom(UINT idx) const {
        D3D12_GPU_DESCRIPTOR_HANDLE h = gpuBase;
        h.ptr += (UINT64)idx * incrementSize;
        return h;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE CPUFrom(UINT idx) const {
        D3D12_CPU_DESCRIPTOR_HANDLE h = cpuBase;
        h.ptr += (SIZE_T)idx * incrementSize;
        return h;
    }
    UINT IndexOf(D3D12_CPU_DESCRIPTOR_HANDLE h) const {
        return (UINT)((h.ptr - cpuBase.ptr) / incrementSize);
    }
};

// =============================================================================
// Layout de la root signature : UNE table de descripteurs PAR registre.
// Une table d'1 seul descripteur peut pointer sur n'importe quel slot du heap
// (meme non contigu) -> resout le bug ou un shader lisant t1/t2 ne tombait pas
// sur les bons SRV (la table t0..t15 offsetait par numero de registre depuis
// une base individuelle ecrasee par la derniere texture liee).
// Budget DWORD (max 64) : 16 (constants) + 8*2 (CBV root descriptors) +
//                         16+8+4 tables = 16+16+28 = 60. OK.
// Les shaders NKRenderer utilisent PLUSIEURS cbuffers (b0,b1,…) → b0..b7 en root CBV
// indexés par le numéro de registre (b.slot). SRV/Sampler/UAV = une table d'1 descripteur
// par registre (slot heap potentiellement non contigu), comme avant mais étendu.
// =============================================================================
// REFONTE TABLES LARGES (2026-06-11) : le générateur NkSL assigne via un COMPTEUR PARTAGÉ
// → registres hauts/épars (jusqu'à ~50 pour le PBR), impossible « 1 param par registre »
// dans 64 DWORDs. → 2 tables de descripteurs à PLAGE LARGE + root constants. Par draw, on
// COPIE les descripteurs des ressources bindées depuis les heaps STAGING (CPU) vers une
// région contiguë d'un RING shader-visible, puis on binde la table. Root sig 1.1 + flag
// DESCRIPTORS_VOLATILE : seuls les descripteurs réellement accédés doivent être valides.
namespace NkDX12RootLayout {
    constexpr uint32 ROOT_CONSTANTS    = 0;  // b0 space1, 16 valeurs (PushConstants, 64 o)
    constexpr uint32 TABLE_CBV_SRV_UAV = 1;  // table : CBV b0-31, SRV t0-31, UAV u0-7 (space0)
    constexpr uint32 TABLE_SAMPLER     = 2;  // table : SAMPLER s0-31 (space0)
    constexpr uint32 NUM_PARAMS        = 3;

    // Plages couvertes + disposition dans le bloc contigu du ring CBV/SRV/UAV.
    // NUM_CBV élargi 16→32 (2026-06-23) : le générateur NkSL émet le Material Parameter
    // Collection (MPC_UBO) en register(b25) — au-delà de b15. Mapping DIRECT par numéro de
    // binding (cohérent avec SRV t16..t27 qui marche déjà). Tables de descripteurs → la
    // limite Tier 1 des 14 CBV en root descriptor NE s'applique PAS (OK tout GPU Tier 2+).
    constexpr uint32 NUM_CBV  = 32;  constexpr uint32 OFF_CBV = 0;               // b0..b31 → bloc[0..31]
    constexpr uint32 NUM_SRV  = 32;  constexpr uint32 OFF_SRV = OFF_CBV + NUM_CBV; // t0..t31 → bloc[32..63]
    constexpr uint32 NUM_UAV  = 8;   constexpr uint32 OFF_UAV = OFF_SRV + NUM_SRV;  // u0..u7  → bloc[64..71]
    constexpr uint32 BLOCK_CBV_SRV_UAV = OFF_UAV + NUM_UAV;  // 72 descripteurs / draw
    // Les samplers combinés suivent le numéro de leur texture (tN/sN) → couvrir s0..s31.
    constexpr uint32 NUM_SAMP = 32;  constexpr uint32 BLOCK_SAMPLER = NUM_SAMP;    // 32 samplers / draw
}

// =============================================================================
// Internal resource structs
// =============================================================================
struct NkDX12Buffer {
    ComPtr<ID3D12Resource> resource;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
    void*    mapped = nullptr;  // persistently mapped si upload/readback
    NkBufferDesc desc;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    UINT cbvIdx = UINT_MAX;  // index dans le descriptor heap CBV/SRV/UAV
    UINT srvIdx = UINT_MAX;
    UINT uavIdx = UINT_MAX;
};

struct NkDX12Texture {
    ComPtr<ID3D12Resource> resource;
    NkTextureDesc          desc;
    UINT rtvIdx   = UINT_MAX;
    UINT dsvIdx   = UINT_MAX;
    UINT srvIdx   = UINT_MAX;
    UINT uavIdx   = UINT_MAX;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    bool isSwapchain = false;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
};

struct NkDX12Sampler {
    UINT heapIdx = UINT_MAX;
};

struct NkDX12ShaderStage {
    NkVector<uint8> bytecode;  // DXBC ou DXIL
    D3D12_SHADER_BYTECODE bc() const {
        return { bytecode.Data(), static_cast<SIZE_T>(bytecode.Size()) };
    }
};

struct NkDX12Shader {
    NkDX12ShaderStage vs, ps, cs, gs, hs, ds;
    ComPtr<ID3DBlob>  vsBlob; // pour InputLayout
};

struct NkDX12Pipeline {
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12RootSignature> rootSig;
    bool isCompute = false;
    D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint32 vertexStrides[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};

    // ── Fix #613 (RENDER_TARGET_FORMAT_MISMATCH) : variantes PSO par formats de RP ──
    // En D3D12 le format RTV/DSV est figé dans le PSO. Un même pipeline NKRenderer
    // (ex. 2D_Alpha, PP_Tonemap, Skybox…) est dessiné dans des passes de formats
    // DIFFÉRENTS selon la config (HDR R16F vs swapchain BGRA vs LDR RGBA8). On garde
    // donc le desc d'origine + une table {signature formats → PSO} pour reconstruire/
    // cacher la bonne variante au 1er bind dans une passe (cf. ResolvePipelineForRenderPass).
    NkGraphicsPipelineDesc desc;                 // desc d'origine (graphics only)
    uint64                 baseFmtSig = 0;        // signature du PSO de base (.pso)
    // Variantes additionnelles : signature formats -> PSO. Le PSO de base (baseFmtSig)
    // reste dans .pso ; on ne stocke ici que les formats DIFFÉRENTS rencontrés au bind.
    NkUnorderedMap<uint64, ComPtr<ID3D12PipelineState>> variants;
};

struct NkDX12RenderPass  { NkRenderPassDesc desc; };
struct NkDX12Framebuffer {
    UINT rtvIdxs[8]  = {};
    uint64 colorTexIds[8] = {};
    UINT rtvCount    = 0;
    UINT dsvIdx      = UINT_MAX;
    uint64 depthTexId = 0;
    uint32 w = 0, h  = 0;
    // RP synthetise depuis les formats des attachments (cause A ecran noir DX12) :
    // permet a GetFramebufferRenderPass() de renvoyer un RP dont les formats RTV/DSV
    // correspondent au FB. Sinon le renderer cree ses PSO sans format => fallback
    // swapchain B8G8R8A8 => RENDER_TARGET_FORMAT_MISMATCH_PIPELINE_STATE (#613).
    NkRenderPassHandle renderPassHandle{};
};

struct NkDX12DescSetLayout { NkDescriptorSetLayoutDesc desc; };
struct NkDX12DescSet {
    struct Binding {
        uint32 slot  = 0;
        NkDescriptorType type{};
        uint64 bufId = 0;
        uint64 texId = 0;
        uint64 sampId= 0;
    };
    NkVector<Binding> bindings;
    uint64 layoutId = 0;
};

// =============================================================================
// Frame data (triple buffering avec command allocators séparés)
// =============================================================================
struct NkDX12FrameData {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Fence>            fence;
    UINT64                         fenceValue = 0;
    HANDLE                         fenceEvent = nullptr;

    void WaitAndReset(ID3D12CommandQueue* queue) {
        if (fence->GetCompletedValue() < fenceValue) {
            fence->SetEventOnCompletion(fenceValue, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        allocator->Reset();
    }
    void Signal(ID3D12CommandQueue* queue, UINT64 val) {
        fenceValue = val;
        queue->Signal(fence.Get(), val);
    }
};

// =============================================================================
// NkDirectX12Device
// =============================================================================
class NkDirectX12Device final : public NkIDevice {
public:
    NkDirectX12Device()  = default;
    ~NkDirectX12Device() override;

    bool          Initialize(const NkDeviceInitInfo& init) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_GFX_API_DX12; }
    const NkDeviceCaps& GetCaps()               const override { return mCaps; }

    NkBufferHandle  CreateBuffer (const NkBufferDesc& d)                      override;
    void            DestroyBuffer(NkBufferHandle& h)                          override;
    bool WriteBuffer(NkBufferHandle,const void*,uint64,uint64)                override;
    bool WriteBufferAsync(NkBufferHandle,const void*,uint64,uint64)           override;
    bool ReadBuffer(NkBufferHandle,void*,uint64,uint64)                       override;
    NkMappedMemory MapBuffer(NkBufferHandle,uint64,uint64)                    override;
    void           UnmapBuffer(NkBufferHandle)                                override;

    NkTextureHandle  CreateTexture (const NkTextureDesc& d)                   override;
    void             DestroyTexture(NkTextureHandle& h)                        override;
    bool WriteTexture(NkTextureHandle,const void*,uint32)                     override;
    bool WriteTextureRegion(NkTextureHandle,const void*,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32) override;
    bool GenerateMipmaps(NkTextureHandle, NkFilter)                           override;

    NkSamplerHandle  CreateSampler (const NkSamplerDesc& d)                   override;
    void             DestroySampler(NkSamplerHandle& h)                        override;

    NkShaderHandle   CreateShader (const NkShaderDesc& d)                     override;
    void             DestroyShader(NkShaderHandle& h)                          override;

    NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d)  override;
    NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc& d)   override;
    void             DestroyPipeline(NkPipelineHandle& h)                     override;

    NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc& d)         override;
    void                DestroyRenderPass (NkRenderPassHandle& h)              override;
    NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc& d)        override;
    void                DestroyFramebuffer(NkFramebufferHandle& h)             override;
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFBs[mBackBufferIdx]; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
    // Cause A : renvoie le RP synthetise au CreateFramebuffer (formats RTV/DSV reels
    // du FB) pour que les PSO offscreen ne tombent pas sur le fallback swapchain.
    NkRenderPassHandle  GetFramebufferRenderPass(NkFramebufferHandle fb) const override;
    // Format REEL du backbuffer (cf. CreateSwapchain : B8G8R8A8_UNORM + RTV sans desc).
    // Doit matcher, sinon les PSO des passes qui ecrivent le swapchain sont bakes en
    // SRGB et D3D12 rejette le draw (#613 format mismatch).
    NkGPUFormat GetSwapchainFormat()      const override { return NkGPUFormat::NK_BGRA8_UNORM; }
    NkGPUFormat GetSwapchainDepthFormat() const override { return NkGPUFormat::NK_D32_FLOAT; }
    uint32   GetSwapchainWidth()       const override { return mWidth; }
    uint32   GetSwapchainHeight()      const override { return mHeight; }

    NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) override;
    void            DestroyDescriptorSetLayout(NkDescSetHandle& h)                override;
    NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)           override;
    void            FreeDescriptorSet    (NkDescSetHandle& h)                     override;
    void            UpdateDescriptorSets(const NkDescriptorWrite* w, uint32 n)   override;

    NkICommandBuffer* CreateCommandBuffer(NkCommandBufferType t)                  override;
    void              DestroyCommandBuffer(NkICommandBuffer*& cb)                 override;

    void Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence)     override;
    void SubmitAndPresent(NkICommandBuffer* cb)                                   override;
    NkFenceHandle CreateFence(bool signaled)  override;
    void DestroyFence(NkFenceHandle& h)       override;
    bool WaitFence(NkFenceHandle f,uint64 to) override;
    bool IsFenceSignaled(NkFenceHandle f)     override;
    void ResetFence(NkFenceHandle f)          override;
    void WaitIdle()                           override;

    bool   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return MAX_FRAMES; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }
    void   OnResize(uint32 w, uint32 h) override;

    void* GetNativeDevice()       const override { return mDevice.Get(); }
    void* GetNativeCommandQueue() const override { return mGraphicsQueue.Get(); }

    // ── Accès interne pour NkDirectX12CommandBuffer ──────────────────────────────
    ID3D12Device*           Dev()   const { return mDevice.Get(); }
    ID3D12CommandQueue*     Queue() const { return mGraphicsQueue.Get(); }
    ID3D12CommandAllocator* FrameAlloc() const { return mFrameData[mFrameIndex].allocator.Get(); }

    ID3D12Resource*     GetDX12Buffer (uint64 id) const;
    ID3D12Resource*     GetDX12Texture(uint64 id) const;
    DXGI_FORMAT         GetTextureDXGIFormat(uint64 id) const;
    D3D12_GPU_VIRTUAL_ADDRESS GetBufferGPUAddr(uint64 id) const;
    UINT GetBufferCbvIndex(uint64 id) const;
    UINT GetBufferSrvIndex(uint64 id) const;
    UINT GetBufferUavIndex(uint64 id) const;
    UINT GetTextureSrvIndex(uint64 id) const;
    UINT GetTextureUavIndex(uint64 id) const;
    UINT GetSamplerHeapIndex(uint64 id) const;
    bool IsSwapchainTexture(uint64 id) const;

    const NkDX12Pipeline*   GetPipeline(uint64 id) const;
    const NkDX12DescSet*    GetDescSet (uint64 id) const;
    const NkDX12Framebuffer* GetFBO    (uint64 id) const;

    // ── Fix #613 : résolution PSO ↔ render pass actif (par formats RTV/DSV) ──────
    // Appelé par NkDirectX12CommandBuffer::BindGraphicsPipeline avec le RP de la passe
    // courante (le renderPassHandle du framebuffer lié). Retourne le PSO de CE pipeline
    // dont les formats RTV/DSV matchent ce RP — en réutilisant le PSO de base s'il
    // correspond déjà, sinon en construisant/cachant une variante. Ne touche PAS la
    // root sig / topology / strides (identiques pour toutes les variantes).
    // Retourne nullptr si le pipeline est invalide ou compute.
    ID3D12PipelineState* ResolvePipelineForRenderPass(uint64 pipelineId,
                                                      NkRenderPassHandle rp);

    NkDX12DescHeap& RtvHeap()     { return mRtvHeap; }
    NkDX12DescHeap& DsvHeap()     { return mDsvHeap; }
    NkDX12DescHeap& CbvSrvUavHeap() { return mCbvSrvUavHeap; } // STAGING (CPU) — source des copies
    NkDX12DescHeap& SamplerHeap() { return mSamplerHeap; }     // STAGING (CPU)
    NkDX12DescHeap& CbvSrvUavRing() { return mCbvSrvUavRing; } // shader-visible — destination/draw
    NkDX12DescHeap& SamplerRing()   { return mSamplerRing; }   // shader-visible

    // Ring de descripteurs shader-visible (1 région par frame, reset à BeginFrame).
    // AllocCbvSrvUavRing/AllocSamplerRing : réservent un bloc CONTIGU dans la région
    // courante et renvoient l'index de base (UINT_MAX si plus de place).
    void ResetDescriptorRingForFrame(uint32 frameIdx);
    UINT AllocCbvSrvUavRing(UINT count);
    UINT AllocSamplerRing(UINT count);
    // Copie 1 descripteur du STAGING (CPU) vers le RING shader-visible (no-op si idx invalide).
    void CopyCbvSrvUavToRing(UINT destRingIdx, UINT srcStagingIdx);
    void CopySamplerToRing(UINT destRingIdx, UINT srcStagingIdx);
    void FillRingBlockDefaults(UINT csuBase, UINT sampBase); // remplit un bloc avec les défauts valides

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(UINT idx) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(UINT idx) const;
    void TransitionResource(ID3D12GraphicsCommandList* cmd,
                             ID3D12Resource* res,
                             D3D12_RESOURCE_STATES from,
                             D3D12_RESOURCE_STATES to);
    void TransitionTextureState(ID3D12GraphicsCommandList* cmd,
                                uint64 textureId,
                                D3D12_RESOURCE_STATES to);

    // Execute one-shot (pour uploads)
    void ExecuteOneShot(NkFunction<void(ID3D12GraphicsCommandList*)> fn);

    // DIAGNOSTIC (gate env NK_DX12_READBACK) : copie le pixel central du backbuffer
    // courant dans un staging READBACK et logue sa valeur BGRA. No-op si la variable
    // d'environnement NK_DX12_READBACK n'est pas définie. Appelé juste avant Present.
    void ReadbackBackbufferCenterDiag();
    // DIAGNOSTIC (NK_DX12_READBACK>=2) : grille 16x16 sur le HDR RT plein écran.
    void ReadbackHDRGridDiag(ID3D12Resource* res, D3D12_RESOURCE_STATES prev,
                             uint32 tw, uint32 th);
    // DIAGNOSTIC (NK_DX12_READBACK>=2) : grille de profondeur sur l'atlas d'ombre.
    void ReadbackDepthGridDiag(ID3D12Resource* res, D3D12_RESOURCE_STATES prev,
                               uint32 tw, uint32 th, uint64 tid);
    // DIAGNOSTIC : lit n floats depuis la donnée mappée d'un buffer UPLOAD (UBO). false si
    // buffer introuvable / non mappé. Utilisé pour vérifier que la matrice caméra est non-nulle.
    bool PeekBufferFloats(uint64 bufId, float* out, uint32 n, uint32 floatOffset = 0) const;

private:
    // ── Fix #613 : helpers PSO graphics paramétrés par formats RTV/DSV ──────────
    // BuildGraphicsPSO construit un ID3D12PipelineState à partir du desc + d'un set
    // explicite de formats (numRT, RTVFormats[8], dsvFormat). Utilisé à la fois par
    // CreateGraphicsPipeline (PSO de base) et ResolvePipelineForRenderPass (variantes).
    ComPtr<ID3D12PipelineState> BuildGraphicsPSO(const NkGraphicsPipelineDesc& d,
                                                 ID3D12RootSignature* rootSig,
                                                 UINT numRT,
                                                 const DXGI_FORMAT* rtvFormats,
                                                 DXGI_FORMAT dsvFormat);
    // RenderPassFormats : extrait (numRT, RTVFormats[8], dsvFormat) d'un RP enregistré.
    // rp invalide -> formats swapchain par défaut (1×B8G8R8A8_UNORM + D32_FLOAT).
    void RenderPassFormats(NkRenderPassHandle rp, UINT& numRT,
                           DXGI_FORMAT rtvFormats[8], DXGI_FORMAT& dsvFormat) const;
    // FmtSignature : hash compact des formats (pour clé de cache des variantes).
    static uint64 FmtSignature(UINT numRT, const DXGI_FORMAT* rtvFormats,
                               DXGI_FORMAT dsvFormat);

    void CreateSwapchain(uint32 w, uint32 h);
    void DestroySwapchain();
    void ResizeSwapchain(uint32 w, uint32 h);
    void QueryCaps();
    void InitDescriptorHeaps();
    ComPtr<ID3D12RootSignature> CreateDefaultRootSignature(bool compute);

    static DXGI_FORMAT     ToDXGIFormat(NkGPUFormat f);
    static DXGI_FORMAT     ToDXGIVertexFormat(NkVertexFormat f);
    static D3D12_COMPARISON_FUNC     ToDX12Compare(NkCompareOp op);
    static D3D12_BLEND               ToDX12Blend(NkBlendFactor f);
    static D3D12_BLEND_OP            ToDX12BlendOp(NkBlendOp op);
    static D3D12_STENCIL_OP          ToDX12StencilOp(NkStencilOp op);
    static D3D12_PRIMITIVE_TOPOLOGY  ToDX12Topology(NkPrimitiveTopology t);
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12TopologyType(NkPrimitiveTopology t);
    static D3D12_FILTER              ToDX12Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp);
    static D3D12_TEXTURE_ADDRESS_MODE ToDX12Address(NkAddressMode a);
    static D3D12_RESOURCE_STATES     ToDX12State(NkResourceState s);

    uint64 NextId() { return ++mNextId; }
    NkAtomic<uint64> mNextId{0};

    ComPtr<ID3D12Device>       mDevice;
    ComPtr<ID3D12CommandQueue> mGraphicsQueue;
    ComPtr<ID3D12CommandQueue> mComputeQueue;
    ComPtr<IDXGISwapChain4>    mSwapchain;
    ComPtr<IDXGIFactory6>      mFactory;

    // Debug InfoQueue : routage validation -> NkLog.
    // mInfoQueue1 = callback live (Win10 2004+ / SDK Agility). mInfoQueue = polling fallback.
    ComPtr<struct ID3D12InfoQueue>  mInfoQueue;
    ComPtr<struct ID3D12InfoQueue1> mInfoQueue1;
    DWORD                            mInfoQueueCookie = 0;

    // Descriptor heaps. mCbvSrvUavHeap/mSamplerHeap = STAGING (CPU, non shader-visible) :
    // les descripteurs persistants (SRV/CBV/UAV/sampler) y sont créés. mCbvSrvUavRing/
    // mSamplerRing = shader-visible, divisés en MAX_FRAMES régions ; par draw on copie les
    // descripteurs bindés depuis le staging vers une région contiguë du ring.
    NkDX12DescHeap mRtvHeap;
    NkDX12DescHeap mDsvHeap;
    NkDX12DescHeap mCbvSrvUavHeap;   // staging
    NkDX12DescHeap mSamplerHeap;     // staging
    NkDX12DescHeap mCbvSrvUavRing;   // shader-visible (ring linéaire, reset/frame)
    NkDX12DescHeap mSamplerRing;     // shader-visible (ring linéaire, reset/frame)
    // Tête linéaire remise à 0 à chaque frame. Sûr car le présent est SÉRIALISÉ
    // (SubmitAndPresent attend la fence → GPU idle entre frames). Si on pipeline les
    // frames plus tard, repasser à des régions par frame.
    UINT mCbvSrvUavRingHead = 0;
    UINT mSamplerRingHead   = 0;
    // Descripteurs DÉFAUT (staging) pour remplir les slots NON bindés d'un bloc de ring :
    // sans ça, un shader accédant à un registre déclaré-mais-non-bindé lit un descripteur
    // GARBAGE → le GPU déréférence de la mémoire invalide → HANG (DXGI_ERROR_DEVICE_HUNG).
    // SRV/UAV = descripteurs NULL (lecture = 0), CBV = petit buffer dummy, sampler = défaut.
    UINT mNullSrvIdx = UINT_MAX, mNullUavIdx = UINT_MAX, mDefaultCbvIdx = UINT_MAX, mDefaultSamplerIdx = UINT_MAX;
    ComPtr<ID3D12Resource> mDummyCbvBuffer;
    void InitNullDescriptors();
    void LogDREDOutput();        // dump les auto-breadcrumbs + page fault sur device removed
    bool mDredLogged = false;

    // Swapchain
    NkVector<NkFramebufferHandle> mSwapchainFBs;
    NkRenderPassHandle mSwapchainRP;
    UINT               mBackBufferIdx = 0;
    uint64             mDepthTexId    = 0;

    // Frame data
    static constexpr uint32 MAX_FRAMES = 3;
    NkDX12FrameData mFrameData[MAX_FRAMES];
    uint32          mFrameIndex  = 0;
    uint64          mFrameNumber = 0;
    UINT64          mFenceValue  = 0;

    // Resources
    NkUnorderedMap<uint64, NkDX12Buffer>       mBuffers;
    NkUnorderedMap<uint64, NkDX12Texture>      mTextures;
    NkUnorderedMap<uint64, NkDX12Sampler>      mSamplers;
    NkUnorderedMap<uint64, NkDX12Shader>       mShaders;
    NkUnorderedMap<uint64, NkDX12Pipeline>     mPipelines;
    NkUnorderedMap<uint64, NkDX12RenderPass>   mRenderPasses;
    NkUnorderedMap<uint64, NkDX12Framebuffer>  mFramebuffers;
    NkUnorderedMap<uint64, NkDX12DescSetLayout>mDescLayouts;
    NkUnorderedMap<uint64, NkDX12DescSet>      mDescSets;
    struct DX12Fence { ComPtr<ID3D12Fence> fence; UINT64 value=0; HANDLE event=nullptr; };
    NkUnorderedMap<uint64, DX12Fence>          mFences;

    // Mutex RÉCURSIF : certaines méthodes verrouillées appellent d'autres méthodes
    // verrouillées sur le même thread (ex. CreateTexture → CreateBuffer pour le
    // staging) — un mutex non-récursif provoquerait un auto-deadlock.
    mutable threading::NkRecursiveMutex  mMutex;
    NkDeviceInitInfo    mInit   {};
    NkDeviceCaps        mCaps   {};
    bool                mIsValid= false;
    uint32              mWidth=0, mHeight=0;
    HWND                mHwnd   = nullptr;
    bool                mVsync = true;
    bool                mAllowTearing = true;
    bool                mTearingSupported = false;
    bool                mEnableComputeQueue = true;
    uint32              mSwapchainBufferCount = MAX_FRAMES;
    uint32              mRtvHeapCapacity = 256;
    uint32              mDsvHeapCapacity = 64;
    uint32              mSrvHeapCapacity = 1024;
    uint32              mSamplerHeapCapacity = 64;
};

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
