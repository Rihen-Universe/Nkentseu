#pragma once
// =============================================================================
// NkRHI_Device_SW.h — Backend Software Rasterizer (CPU)
// Rasterisation CPU complète : triangle setup, interpolation barycentrique,
// z-buffer, pixel shaders émulés via std::function, MSAA optionnel.
// =============================================================================
#include "../Core/NkIDevice.h"
#include "../Commands/NkICommandBuffer.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <cmath>
#include <thread>
#include <algorithm>

namespace nkentseu {

class NkCommandBuffer_SW;

// =============================================================================
// Types internes
// =============================================================================
struct NkSWVec4  { float x=0,y=0,z=0,w=1; };
struct NkSWVec3  { float x=0,y=0,z=0; };
struct NkSWVec2  { float x=0,y=0; };
struct NkSWColor { float r=0,g=0,b=0,a=1; };

struct NkSWVertex {
    NkSWVec4 position;   // clip space
    NkSWVec3 normal;
    NkSWVec2 uv;
    NkSWColor color;
    float attrs[16];     // attributs supplémentaires pour interpolation
};

// Signature des shaders logiciels
using NkSWVertexShader = std::function<NkSWVertex(
    const void* vertexData,
    uint32 vertexIndex,
    const void* uniformData)>;

using NkSWPixelShader = std::function<NkSWColor(
    const NkSWVertex& interpolated,
    const void* uniformData,
    const void* texSampler)>;

// =============================================================================
struct NkSWBuffer {
    std::vector<uint8_t> data;
    NkBufferDesc desc;
    bool mapped = false;
};

struct NkSWSampler {
    NkSamplerDesc desc;
    NkSWColor Sample(const std::vector<uint8_t>& pixels,
                     uint32 width, uint32 height, uint32 bpp,
                     float u, float v) const;
};

struct NkSWTexture {
    std::vector<std::vector<uint8_t>> mips; // mip[0] = niveau 0
    NkTextureDesc desc;
    bool isRenderTarget = false;
    NkSWSampler defaultSampler;
    uint32 Width (uint32 mip=0) const { return std::max(1u, desc.width  >> mip); }
    uint32 Height(uint32 mip=0) const { return std::max(1u, desc.height >> mip); }
    uint32 Bpp()                const { return NkFormatBytesPerPixel(desc.format); }
    NkSWColor Sample(float u, float v, uint32 mip=0) const;
    NkSWColor Read(uint32 x, uint32 y, uint32 mip=0) const;
    void Write(uint32 x, uint32 y, const NkSWColor& c, uint32 mip=0);
};

struct NkSWShader {
    NkSWVertexShader  vertFn;
    NkSWPixelShader   fragFn;
    bool isCompute = false;
    std::function<void(uint32,uint32,uint32,const void*)> computeFn;
};

struct NkSWPipeline {
    uint64 shaderId   = 0;
    bool isCompute    = false;
    bool depthTest    = true;
    bool depthWrite   = true;
    bool blendEnable  = false;
    NkCompareOp  depthOp   = NkCompareOp::Less;
    NkCullMode   cullMode  = NkCullMode::Back;
    NkFrontFace  frontFace = NkFrontFace::CCW;
    NkBlendFactor srcColor = NkBlendFactor::SrcAlpha;
    NkBlendFactor dstColor = NkBlendFactor::OneMinusSrcAlpha;
    NkPrimitiveTopology topology = NkPrimitiveTopology::TriangleList;
    uint32 vertexStride = 0;
};

struct NkSWRenderPass   { NkRenderPassDesc desc; };
struct NkSWFramebuffer  {
    uint64 colorId   = 0;
    uint64 depthId   = 0;
    uint32 w=0, h=0;
};
struct NkSWDescSetLayout { NkDescriptorSetLayoutDesc desc; };
struct NkSWDescSet {
    struct Binding { uint32 slot=0; NkDescriptorType type{}; uint64 bufId=0; uint64 texId=0; uint64 sampId=0; };
    std::vector<Binding> bindings;
};
struct NkSWFence { bool signaled=false; };

// =============================================================================
// Rastériseur interne
// =============================================================================
struct NkSWRasterState {
    NkSWTexture*   colorTarget = nullptr;
    NkSWTexture*   depthTarget = nullptr;
    NkSWPipeline*  pipeline    = nullptr;
    NkSWShader*    shader      = nullptr;
    const void*    uniformData = nullptr;
    const void*    vertexData  = nullptr;
    uint32         vertexStride= 0;
    bool           wireframe   = false;
};

class NkSWRasterizer {
public:
    void SetState(const NkSWRasterState& s) { mState = s; }

    void DrawTriangles(const NkSWVertex* verts, uint32 count);
    void DrawTriangle (const NkSWVertex& v0, const NkSWVertex& v1, const NkSWVertex& v2);
    void DrawLine     (const NkSWVertex& v0, const NkSWVertex& v1);
    void DrawPoint    (const NkSWVertex& v0);

private:
    NkSWVertex ClipToNDC(const NkSWVertex& v) const;
    NkSWVertex NDCToScreen(const NkSWVertex& v, float w, float h) const;
    NkSWVertex Interpolate(const NkSWVertex& a, const NkSWVertex& b, float t) const;
    NkSWVertex BaryInterp(const NkSWVertex& v0, const NkSWVertex& v1, const NkSWVertex& v2,
                           float l0, float l1, float l2) const;
    bool DepthTest(uint32 x, uint32 y, float z);
    NkSWColor BlendColor(const NkSWColor& src, const NkSWColor& dst) const;
    float ApplyBlendFactor(NkBlendFactor f, float src, float dst, float srcA, float dstA) const;

    NkSWRasterState mState;
};

// =============================================================================
// NkDevice_SW
// =============================================================================
class NkDevice_SW final : public NkIDevice {
public:
    NkDevice_SW()  = default;
    ~NkDevice_SW() override;

    bool          Initialize(NkIGraphicsContext* ctx) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_SOFTWARE; }
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
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFB; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
    NkFormat GetSwapchainFormat()      const override { return NkFormat::RGBA8_Unorm; }
    NkFormat GetSwapchainDepthFormat() const override { return NkFormat::D32_Float;   }
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
    void WaitIdle()                           override {}

    void   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return 1; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }
    void   OnResize(uint32 w, uint32 h) override;

    void* GetNativeDevice()       const override { return nullptr; }
    void* GetNativeCommandQueue() const override { return nullptr; }

    // Accès interne
    NkSWBuffer*      GetBuf  (uint64 id);
    NkSWTexture*     GetTex  (uint64 id);
    NkSWSampler*     GetSamp (uint64 id);
    NkSWShader*      GetShader(uint64 id);
    NkSWPipeline*    GetPipe (uint64 id);
    NkSWDescSet*     GetDescSet(uint64 id);
    NkSWFramebuffer* GetFBO  (uint64 id);

    // Présentation — copie le color buffer vers la surface native
    void Present();
    // Accès direct au backbuffer pour présentation (lecture seule)
    const uint8_t* BackbufferPixels() const;
    uint32 BackbufferWidth()  const { return mWidth; }
    uint32 BackbufferHeight() const { return mHeight; }

private:
    void CreateSwapchainObjects();
    uint64 NextId() { return ++mNextId; }
    std::atomic<uint64> mNextId{0};

    std::unordered_map<uint64, NkSWBuffer>       mBuffers;
    std::unordered_map<uint64, NkSWTexture>       mTextures;
    std::unordered_map<uint64, NkSWSampler>       mSamplers;
    std::unordered_map<uint64, NkSWShader>        mShaders;
    std::unordered_map<uint64, NkSWPipeline>      mPipelines;
    std::unordered_map<uint64, NkSWRenderPass>    mRenderPasses;
    std::unordered_map<uint64, NkSWFramebuffer>   mFramebuffers;
    std::unordered_map<uint64, NkSWDescSetLayout> mDescLayouts;
    std::unordered_map<uint64, NkSWDescSet>       mDescSets;
    std::unordered_map<uint64, NkSWFence>         mFences;

    NkFramebufferHandle mSwapchainFB;
    NkRenderPassHandle  mSwapchainRP;

    mutable std::mutex  mMutex;
    NkIGraphicsContext* mCtx    = nullptr;
    NkDeviceCaps        mCaps   {};
    bool                mIsValid= false;
    uint32              mWidth=0, mHeight=0;
    uint32              mFrameIndex  = 0;
    uint64              mFrameNumber = 0;
};

} // namespace nkentseu
