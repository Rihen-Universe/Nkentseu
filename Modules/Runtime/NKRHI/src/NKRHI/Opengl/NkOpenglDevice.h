#pragma once
// =============================================================================
// NkRHI_Device_GL.h — Implémentation OpenGL du NkIDevice
// Supporte OpenGL 4.3+ (compute, SSBO, DSA)
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"

#ifndef NK_NO_GLAD2
#   include <glad/gl.h>
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#endif

namespace nkentseu {

// Implémentation interne d'un command buffer GL
class NkOpenGLCommandBuffer;
class NkOpenGLDevice;

GLuint NkOpenglGetBufferID (NkOpenGLDevice* dev, uint64 id);
GLuint NkOpenglGetTextureID(NkOpenGLDevice* dev, uint64 id);
GLuint NkOpenglGetFBOID    (NkOpenGLDevice* dev, uint64 id);
GLuint NkOpenglGetSamplerID(NkOpenGLDevice* dev, uint64 id);
GLuint NkOpenglGetProgramID(NkOpenGLDevice* dev, uint64 id);
GLuint NkOpenglGetVAOID    (NkOpenGLDevice* dev, uint64 id);
GLenum NkOpenglGetPrimitive(NkOpenGLDevice* dev, uint64 id);
bool   NkOpenglIsCompute   (NkOpenGLDevice* dev, uint64 id);
uint32 NkOpenglGetVertexStride(NkOpenGLDevice* dev, uint64 pipelineId, uint32 binding);
void   NkOpenglApplyRenderState(NkOpenGLDevice* dev, uint64 pipelineId);
void   NkOpenglApplyDescSet(NkOpenGLDevice* dev, uint64 setId, const NkVector<uint32>& dynOff);

class NkOpenGLDevice final : public NkIDevice {
public:
    NkOpenGLDevice()  = default;
    ~NkOpenGLDevice() override;

    bool          Initialize(const NkDeviceInitInfo& init) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_OPENGL; }
    const NkDeviceCaps& GetCaps()               const override { return mCaps; }

    // ── Buffers ───────────────────────────────────────────────────────────────
    NkBufferHandle  CreateBuffer (const NkBufferDesc& d)  override;
    void            DestroyBuffer(NkBufferHandle& h)      override;
    bool WriteBuffer(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) override;
    bool WriteBufferAsync(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) override;
    bool ReadBuffer(NkBufferHandle buf,void* out,uint64 sz,uint64 off)         override;
    NkMappedMemory MapBuffer(NkBufferHandle buf,uint64 off,uint64 sz)          override;
    void           UnmapBuffer(NkBufferHandle buf)                             override;

    // ── Textures ──────────────────────────────────────────────────────────────
    NkTextureHandle CreateTexture (const NkTextureDesc& d)    override;
    void            DestroyTexture(NkTextureHandle& h)         override;
    bool WriteTexture(NkTextureHandle t,const void* p,uint32 rp) override;
    bool WriteTextureRegion(NkTextureHandle t,const void* p,
                             uint32 x,uint32 y,uint32 z,
                             uint32 w,uint32 h,uint32 d2,
                             uint32 mip,uint32 layer,uint32 rp) override;
    bool GenerateMipmaps(NkTextureHandle t, NkFilter f)        override;

    // ── Samplers ──────────────────────────────────────────────────────────────
    NkSamplerHandle CreateSampler (const NkSamplerDesc& d) override;
    void            DestroySampler(NkSamplerHandle& h)     override;

    // ── Shaders ───────────────────────────────────────────────────────────────
    NkShaderHandle  CreateShader (const NkShaderDesc& d)  override;
    void            DestroyShader(NkShaderHandle& h)      override;

    // ── Pipelines ─────────────────────────────────────────────────────────────
    NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) override;
    NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc&  d) override;
    void             DestroyPipeline(NkPipelineHandle& h)                    override;

    // ── Render Passes & Framebuffers ──────────────────────────────────────────
    NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc& d)    override;
    void                DestroyRenderPass (NkRenderPassHandle& h)         override;
    NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc& d)   override;
    void                DestroyFramebuffer(NkFramebufferHandle& h)        override;
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFB; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
    NkGPUFormat            GetSwapchainFormat()      const override { return mSwapchainFormat; }
    NkGPUFormat            GetSwapchainDepthFormat() const override { return NkGPUFormat::NK_D32_FLOAT; }
    uint32              GetSwapchainWidth()       const override { return mWidth; }
    uint32              GetSwapchainHeight()      const override { return mHeight; }

    // ── Descriptor Sets ───────────────────────────────────────────────────────
    NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) override;
    void            DestroyDescriptorSetLayout(NkDescSetHandle& h)                override;
    NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)           override;
    void            FreeDescriptorSet    (NkDescSetHandle& h)                     override;
    void            UpdateDescriptorSets(const NkDescriptorWrite* writes,uint32 n) override;

    // ── Command Buffers ───────────────────────────────────────────────────────
    NkICommandBuffer* CreateCommandBuffer(NkCommandBufferType t) override;
    void              DestroyCommandBuffer(NkICommandBuffer*& cb) override;

    // ── Submit & Sync ─────────────────────────────────────────────────────────
    void Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) override;
    void SubmitAndPresent(NkICommandBuffer* cb) override;
    NkFenceHandle CreateFence(bool signaled)  override;
    void          DestroyFence(NkFenceHandle& h) override;
    bool          WaitFence(NkFenceHandle f, uint64 timeoutNs) override;
    bool          IsFenceSignaled(NkFenceHandle f)             override;
    void          ResetFence(NkFenceHandle f)                  override;
    void          WaitIdle()                                   override;

    // ── Frame management ──────────────────────────────────────────────────────
    bool   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return MAX_FRAMES; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }

    void OnResize(uint32 w, uint32 h) override;

    void* GetNativeDevice()       const override { return nullptr; }
    void* GetNativeCommandQueue() const override { return nullptr; }

private:
    friend GLuint NkOpenglGetBufferID (NkOpenGLDevice* dev, uint64 id);
    friend GLuint NkOpenglGetTextureID(NkOpenGLDevice* dev, uint64 id);
    friend GLuint NkOpenglGetFBOID    (NkOpenGLDevice* dev, uint64 id);
    friend GLuint NkOpenglGetSamplerID(NkOpenGLDevice* dev, uint64 id);
    friend GLuint NkOpenglGetProgramID(NkOpenGLDevice* dev, uint64 id);
    friend GLuint NkOpenglGetVAOID    (NkOpenGLDevice* dev, uint64 id);
    friend GLenum NkOpenglGetPrimitive(NkOpenGLDevice* dev, uint64 id);
    friend bool   NkOpenglIsCompute   (NkOpenGLDevice* dev, uint64 id);
    friend uint32 NkOpenglGetVertexStride(NkOpenGLDevice* dev, uint64 pipelineId, uint32 binding);
    friend void   NkOpenglApplyRenderState(NkOpenGLDevice* dev, uint64 pipelineId);
    friend void   NkOpenglApplyDescSet(NkOpenGLDevice* dev, uint64 setId, const NkVector<uint32>& dynOff);

    void QueryCaps();

    // Conversion formats
    static GLenum ToGLInternalFormat(NkGPUFormat f);
    static GLenum ToGLBaseFormat(NkGPUFormat f);
    static GLenum ToGLType(NkGPUFormat f);
    static GLenum ToGLTextureTarget(NkTextureType t, NkSampleCount s);
    static GLenum ToGLFilter(NkFilter f, NkMipFilter mf);
    static GLenum ToGLWrap(NkAddressMode a);
    static GLenum ToGLCompareOp(NkCompareOp op);
    static GLenum ToGLBlendFactor(NkBlendFactor f);
    static GLenum ToGLBlendOp(NkBlendOp op);
    static GLenum ToGLPrimitive(NkPrimitiveTopology t);
    static GLenum ToGLShaderStage(NkShaderStage s);
    static GLenum ToGLBufferUsage(NkResourceUsage u, NkBindFlags b);

    // Pool d'IDs
    uint64 NextId() { return ++mNextId; }
    NkAtomic<uint64> mNextId{0};

    // ── Stockage des ressources (ID → objet GL) ───────────────────────────────
    struct GLBuffer  { GLuint id=0; uint64 size=0; NkResourceUsage usage{}; NkBindFlags bind{}; };
    struct GLTexture { GLuint id=0; GLenum target=0; NkTextureDesc desc{}; };
    struct GLSampler { GLuint id=0; };
    struct GLShader  { GLuint program=0; };
    struct GLPipeline{
        GLuint program=0;
        NkVertexLayout vertexLayout;
        GLuint vao=0; // VAO par pipeline
        NkGraphicsPipelineDesc gfxDesc;
        NkComputePipelineDesc  compDesc;
        bool isCompute=false;
    };
    struct GLFBO { GLuint id=0; uint32 w=0, h=0; };
    // GL n'a pas de descriptor sets natifs — on les émule via un slot binding table
    struct GLDescSetLayout { NkDescriptorSetLayoutDesc desc; };
    struct GLDescSet {
        NkDescSetHandle layoutHandle;
        struct Binding {
            NkDescriptorType type{};
            GLuint   bufferId=0;
            uint64   bufferOffset=0;
            uint64   bufferRange=0;
            GLuint   textureId=0;
            GLuint   samplerId=0;
        } bindings[32];
    };
    // Fence GL → GLsync
    struct GLFenceObj { GLsync sync=nullptr; bool signaled=false; };

    NkUnorderedMap<uint64, GLBuffer>        mBuffers;
    NkUnorderedMap<uint64, GLTexture>       mTextures;
    NkUnorderedMap<uint64, GLSampler>       mSamplers;
    NkUnorderedMap<uint64, GLShader>        mShaders;
    NkUnorderedMap<uint64, GLPipeline>      mPipelines;
    NkUnorderedMap<uint64, GLFBO>           mFramebuffers;
    NkUnorderedMap<uint64, NkRenderPassDesc>mRenderPasses;
    NkUnorderedMap<uint64, GLDescSetLayout> mDescLayouts;
    NkUnorderedMap<uint64, GLDescSet>       mDescSets;
    NkUnorderedMap<uint64, GLFenceObj>      mFences;

    mutable threading::NkMutex mMutex;

    // Swapchain "virtuel" (framebuffer 0 = back buffer GL)
    NkFramebufferHandle mSwapchainFB;
    NkRenderPassHandle  mSwapchainRP;
    NkGPUFormat            mSwapchainFormat = NkGPUFormat::NK_RGBA8_SRGB;

    NkDeviceInitInfo    mInit   {};
    NkDeviceCaps        mCaps   {};
    bool                mIsValid= false;
    uint32              mWidth  = 0, mHeight = 0;
    uint32              mFrameIndex = 0;
    uint64              mFrameNumber= 0;
    static constexpr uint32 MAX_FRAMES = 3;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    HWND  mNativeHwnd  = nullptr;
    HDC   mNativeHdc   = nullptr;
    HGLRC mNativeGlrc  = nullptr;
    BOOL (WINAPI* mWglSwapIntervalExt)(int) = nullptr;
#endif

    // Compile un shader GL stage
    GLuint CompileGLStage(GLenum stage, const char* src);
    // Lie les descriptors avant un draw/dispatch
    void   ApplyDescriptors(const GLDescSet& ds);
};

} // namespace nkentseu
