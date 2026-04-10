#pragma once
// =============================================================================
// NkShader.h — Shader compilé + pipeline graphics/compute haut niveau.
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {

    // =========================================================================
    // NkShaderSource — sources multi-API pour un shader program
    // =========================================================================
    struct NkShaderSource {
        // GLSL (OpenGL)
        const char* vertexGLSL   = nullptr;
        const char* fragmentGLSL = nullptr;
        const char* geometryGLSL = nullptr;
        const char* computeGLSL  = nullptr;

        // GLSL (Vulkan + glslang)
        const char* vertexVkGLSL   = nullptr;
        const char* fragmentVkGLSL = nullptr;
        const char* geometryVkGLSL = nullptr;
        const char* computeVkGLSL  = nullptr;

        // HLSL (DX11)
        const char* vertexHLSLdx11   = nullptr;
        const char* fragmentHLSLdx11 = nullptr; // "PSMain"
        const char* geometryHLSLdx11 = nullptr;
        const char* computeHLSLdx11  = nullptr;

        // HLSL (DX12)
        const char* vertexHLSLdx12   = nullptr;
        const char* fragmentHLSLdx12 = nullptr; // "PSMain"
        const char* geometryHLSLdx12 = nullptr;
        const char* computeHLSLdx12  = nullptr;

        // MSL (Metal)
        const char* vertexMSL    = nullptr;
        const char* fragmentMSL  = nullptr;
        const char* computeMSL   = nullptr;

        // SPIR-V précompilé (Vulkan)
        const void* spirvVert    = nullptr; uint64 spirvVertSize   = 0;
        const void* spirvFrag    = nullptr; uint64 spirvFragSize   = 0;
        const void* spirvComp    = nullptr; uint64 spirvCompSize   = 0;

        // CPU software rasterizer
        const void* cpuVertFn    = nullptr;
        const void* cpuFragFn    = nullptr;
        const void* cpuCompFn    = nullptr;

        const char* debugName    = nullptr;

        // ── Helpers ──────────────────────────────────────────────────────────

        static NkShaderSource FromGLSL(const char* vert, const char* frag,
                                        const char* name = nullptr) noexcept {
            NkShaderSource s; s.vertexGLSL=vert; s.fragmentGLSL=frag; s.debugName=name; return s;
        }

        static NkShaderSource FromVkGLSL(const char* vert, const char* frag,
                                        const char* name = nullptr) noexcept {
            NkShaderSource s; s.vertexVkGLSL=vert; s.fragmentVkGLSL=frag; s.debugName=name; return s;
        }

        static NkShaderSource FromHLSL11(const char* vert, const char* frag,
                                          const char* name = nullptr) noexcept {
            NkShaderSource s; s.vertexHLSLdx11=vert; s.fragmentHLSLdx11=frag; s.debugName=name; return s;
        }

        static NkShaderSource FromHLSL12(const char* vert, const char* frag,
                                          const char* name = nullptr) noexcept {
            NkShaderSource s; s.vertexHLSLdx12=vert; s.fragmentHLSLdx12=frag; s.debugName=name; return s;
        }

        // Fixe les mêmes sources pour DX11 et DX12 (si le HLSL est compatible)
        static NkShaderSource FromHLSL(const char* vert, const char* frag,
                                        const char* name = nullptr) noexcept {
            NkShaderSource s;
            s.vertexHLSLdx11=vert; s.fragmentHLSLdx11=frag;
            s.vertexHLSLdx12=vert; s.fragmentHLSLdx12=frag;
            s.debugName=name; return s;
        }

        static NkShaderSource FromSPIRV(const void* vert, uint64 vSz,
                                         const void* frag, uint64 fSz,
                                         const char* name = nullptr) noexcept {
            NkShaderSource s; s.spirvVert=vert; s.spirvVertSize=vSz;
            s.spirvFrag=frag; s.spirvFragSize=fSz; s.debugName=name; return s;
        }
    };

    // =========================================================================
    // NkShader — encapsule un NkShaderHandle compilé (prêt pour le pipeline)
    // =========================================================================
    class NkShader {
        public:
            NkShader()  = default;
            ~NkShader() { Destroy(); }

            NkShader(const NkShader&)            = delete;
            NkShader& operator=(const NkShader&) = delete;

            bool Compile(NkIDevice* device, const NkShaderSource& src) noexcept;
            void Destroy() noexcept;

            bool           IsValid()    const noexcept { return mHandle.IsValid(); }
            NkShaderHandle GetHandle()  const noexcept { return mHandle; }
            const char*    GetName()    const noexcept { return mName; }
            NkIDevice*     GetDevice()  const noexcept { return mDevice; }

        private:
            NkShaderDesc BuildDesc(const NkShaderSource& src, NkGraphicsApi api) const noexcept;

            NkIDevice*     mDevice  = nullptr;
            NkShaderHandle mHandle;
            const char*    mName    = nullptr;
    };

    // =========================================================================
    // NkPipeline — pipeline graphique compilé (shader + état)
    // =========================================================================
    class NkPipeline {
        public:
            NkPipeline()  = default;
            ~NkPipeline() { Destroy(); }

            NkPipeline(const NkPipeline&)            = delete;
            NkPipeline& operator=(const NkPipeline&) = delete;

            bool Create(NkIDevice* device, const NkGraphicsPipelineDesc& desc) noexcept;
            bool CreateCompute(NkIDevice* device, const NkComputePipelineDesc& desc) noexcept;
            void Destroy() noexcept;

            bool            IsValid()   const noexcept { return mHandle.IsValid(); }
            NkPipelineHandle GetHandle()const noexcept { return mHandle; }

        private:
            NkIDevice*       mDevice = nullptr;
            NkPipelineHandle mHandle;
    };

    using NkShaderRef   = memory::NkSharedPtr<NkShader>;
    using NkPipelineRef = memory::NkSharedPtr<NkPipeline>;

} // namespace nkentseu
