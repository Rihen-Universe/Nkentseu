#pragma once
// =============================================================================
// NkOpenGLComputeContext.h — Compute via OpenGL Compute Shaders (GL 4.3+)
// =============================================================================
#include "NkIComputeContext.h"
#include "NKRenderer/Context/Core/NkIGraphicsContext.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(MemoryBarrier)
#undef MemoryBarrier
#endif

namespace nkentseu {

    class NkOpenGLComputeContext final : public NkIComputeContext {
    public:
        using ProcAddressLoader = void*(*)(const char*);

        NkOpenGLComputeContext()  = default;
        ~NkOpenGLComputeContext() override;

        // Standalone (crée un contexte GL headless sans fenêtre — optionnel)
        bool Init(const NkContextDesc& desc);

        // Depuis contexte graphique existant (partage le contexte GL courant)
        void InitFromGraphicsContext(NkIGraphicsContext* gfx);
        bool SetProcAddressLoader(ProcAddressLoader loader);

        bool          IsValid()  const override;
        void          Shutdown()       override;

        NkComputeBuffer  CreateBuffer (const NkComputeBufferDesc& d)           override;
        void             DestroyBuffer(NkComputeBuffer& buf)                   override;
        bool             WriteBuffer  (NkComputeBuffer& buf, const void* data,
                                       uint64 bytes, uint64 offset=0)          override;
        bool             ReadBuffer   (const NkComputeBuffer& buf, void* out,
                                       uint64 bytes, uint64 offset=0)          override;

        NkComputeShader   CreateShaderFromSource(const char* src,
                                                  const char* entry="main")    override;
        NkComputeShader   CreateShaderFromFile  (const char* path,
                                                  const char* entry="main")    override;
        void              DestroyShader(NkComputeShader& s)                    override;

        NkComputePipeline CreatePipeline (const NkComputeShader& s)            override;
        void              DestroyPipeline(NkComputePipeline& p)                override;

        void BindBuffer  (uint32 slot, NkComputeBuffer& buf)                   override;
        void BindPipeline(const NkComputePipeline& p)                          override;
        void Dispatch    (uint32 gx, uint32 gy=1, uint32 gz=1)                 override;

        void WaitIdle()      override;
        void MemoryBarrier() override;

        NkGraphicsApi GetApi()              const override;
        uint32        GetMaxGroupSizeX()    const override;
        uint32        GetMaxGroupSizeY()    const override;
        uint32        GetMaxGroupSizeZ()    const override;
        uint64        GetSharedMemoryBytes()const override;
        bool          SupportsAtomics()     const override;
        bool          SupportsFloat64()     const override;

    private:
        struct Functions;
        Functions*  mFns           = nullptr;
        bool        mIsValid       = false;
        bool        mOwnsContext   = false; // true si contexte standalone
        uint32      mCurrentProgram= 0;
        NkGraphicsApi mApi         = NkGraphicsApi::NK_API_OPENGL;
    };

} // namespace nkentseu
