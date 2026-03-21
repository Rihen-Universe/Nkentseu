#pragma once
// =============================================================================
// NkIComputeContext.h — Interface abstraite Compute (GPGPU)
//
// Deux modes d'utilisation :
//   A) Via le contexte graphique existant (partagé device/queue) :
//         auto compute = NkComputeFromGraphics(graphicsCtx);
//      → Vulkan  : compute queue dédiée si dispo, sinon graphics queue
//      → DX11    : même device, ID3D11ComputeShader
//      → DX12    : même device, D3D12_COMMAND_LIST_TYPE_COMPUTE
//      → OpenGL  : même contexte, glDispatchCompute (GL 4.3+)
//      → Metal   : même device, MTLComputeCommandEncoder
//
//   B) Standalone (sans rendu) :
//         auto compute = NkContextFactory::CreateCompute(desc);
//      → Utile pour GPGPU pur : ML inference, simulation, physique, etc.
//      → Pas de swapchain/surface nécessaire
//
// Backends :
//   Vulkan  ✓ queue COMPUTE_BIT dédiée
//   DX11    ✓ CS_5_0 compute shaders
//   DX12    ✓ CommandQueue COMPUTE
//   OpenGL  ✓ glDispatchCompute (GL 4.3+)
//   Metal   ✓ MTLComputeCommandEncoder
//   Software ✓ fallback CPU (pipeline sans GPU, activable par backend software)
// =============================================================================
#include "NKContext/Core/NkGraphicsApi.h"
#include "NKMemory/NkUniquePtr.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS) && defined(MemoryBarrier)
#undef MemoryBarrier
#endif

namespace nkentseu {

    // -----------------------------------------------------------------------
    // Descripteurs
    // -----------------------------------------------------------------------
    struct NkComputeBufferDesc {
        uint64      sizeBytes    = 0;
        bool        cpuReadable  = false;   // Readback GPU→CPU
        bool        cpuWritable  = true;    // Upload CPU→GPU
        bool        atomics      = false;   // Accès atomique (UAV / SSBO)
        const void* initialData  = nullptr;
    };

    // Handles opaques (pas de templates pour rester compatible C linkage)
    struct NkComputeBuffer   { void* handle = nullptr; uint64 sizeBytes = 0; bool valid = false; };
    struct NkComputeShader   { void* handle = nullptr; bool valid = false; };
    struct NkComputePipeline { void* handle = nullptr; bool valid = false; };

    // -----------------------------------------------------------------------
    // NkIComputeContext
    // -----------------------------------------------------------------------
    class NkIComputeContext {
    public:
        virtual ~NkIComputeContext() = default;

        // Cycle de vie
        virtual bool IsValid()  const = 0;
        virtual void Shutdown()       = 0;

        // --- Buffers ---
        virtual NkComputeBuffer  CreateBuffer (const NkComputeBufferDesc& desc)   = 0;
        virtual void             DestroyBuffer(NkComputeBuffer& buf)              = 0;
        virtual bool             WriteBuffer  (NkComputeBuffer& buf,
                                               const void* data, uint64 bytes,
                                               uint64 offset = 0)                 = 0;
        virtual bool             ReadBuffer   (const NkComputeBuffer& buf,
                                               void* outData, uint64 bytes,
                                               uint64 offset = 0)                 = 0;

        // --- Shaders ---
        // source : GLSL (OpenGL), HLSL (DX), MSL (Metal), chemin vers .spv (Vulkan)
        // entry  : nom du kernel (ex: "CSMain" pour HLSL, "main" pour GLSL)
        virtual NkComputeShader  CreateShaderFromSource(const char* source,
                                                        const char* entry = "main") = 0;
        virtual NkComputeShader  CreateShaderFromFile  (const char* path,
                                                        const char* entry = "main") = 0;
        virtual void             DestroyShader(NkComputeShader& shader)             = 0;

        // --- Pipelines ---
        virtual NkComputePipeline CreatePipeline (const NkComputeShader& shader)   = 0;
        virtual void              DestroyPipeline(NkComputePipeline& pipeline)     = 0;

        // --- Dispatch ---
        virtual void BindBuffer  (uint32 slot, NkComputeBuffer& buf)               = 0;
        virtual void BindPipeline(const NkComputePipeline& pipeline)               = 0;
        virtual void Dispatch    (uint32 groupX, uint32 groupY = 1,
                                  uint32 groupZ = 1)                               = 0;

        // --- Synchronisation ---
        virtual void WaitIdle()      = 0;   // GPU idle — bloquant
        virtual void MemoryBarrier() = 0;   // Barrière entre dispatches GPU→GPU

        // --- Capacités ---
        virtual NkGraphicsApi GetApi()              const = 0;
        virtual uint32        GetMaxGroupSizeX()    const = 0;
        virtual uint32        GetMaxGroupSizeY()    const = 0;
        virtual uint32        GetMaxGroupSizeZ()    const = 0;
        virtual uint64        GetSharedMemoryBytes()const = 0;
        virtual bool          SupportsAtomics()     const = 0;
        virtual bool          SupportsFloat64()     const = 0;
    };

    using NkComputeContextPtr = memory::NkUniquePtr<NkIComputeContext>;

} // namespace nkentseu
