// =============================================================================
// NkShader.cpp
// =============================================================================
#include "pch/pch.h"
#include "NKRenderer/Core/NkShader.h"
#include "NKRHI/Core/NkGraphicsApi.h"

namespace nkentseu {

    // =========================================================================
    // NkShader
    // =========================================================================

    bool NkShader::Compile(NkIDevice* device, const NkShaderSource& src) noexcept {
        if (!device) return false;
        Destroy();
        mDevice = device;
        mName   = src.debugName;

        NkShaderDesc desc = BuildDesc(src, device->GetApi());
        desc.debugName = src.debugName;
        mHandle = device->CreateShader(desc);
        return mHandle.IsValid();
    }

    NkShaderDesc NkShader::BuildDesc(const NkShaderSource& src, NkGraphicsApi api) const noexcept {
        NkShaderDesc desc;
        desc.debugName = src.debugName;

        // Sélectionne les sources HLSL selon l'API cible
        const bool isDX11 = (api == NkGraphicsApi::NK_API_DIRECTX11);
        const bool isDX12 = (api == NkGraphicsApi::NK_API_DIRECTX12);

        const char* vHLSL  = isDX11 ? src.vertexHLSLdx11   : (isDX12 ? src.vertexHLSLdx12   : nullptr);
        const char* fHLSL  = isDX11 ? src.fragmentHLSLdx11 : (isDX12 ? src.fragmentHLSLdx12 : nullptr);
        const char* gHLSL  = isDX11 ? src.geometryHLSLdx11 : (isDX12 ? src.geometryHLSLdx12 : nullptr);
        const char* cHLSL  = isDX11 ? src.computeHLSLdx11  : (isDX12 ? src.computeHLSLdx12  : nullptr);

        auto addStage = [&](NkShaderStage stage,
                            const char* glsl, const char* hlsl, const char* msl,
                            const void* spv, uint64 spvSz,
                            const void* cpuV, const void* cpuF, const void* cpuC) {
            if (!glsl && !hlsl && !msl && (!spv || spvSz==0) && !cpuV && !cpuF && !cpuC) return;
            NkShaderStageDesc sd{};
            sd.stage       = stage;
            sd.glslSource  = glsl;
            sd.hlslSource  = hlsl;
            sd.mslSource   = msl;
            sd.spirvData   = spv;
            sd.spirvSize   = spvSz;
            sd.cpuVertFn   = cpuV;
            sd.cpuFragFn   = cpuF;
            sd.cpuCompFn   = cpuC;
            desc.AddStage(sd);
        };

        // Vertex
        addStage(NkShaderStage::NK_VERTEX,
                 src.vertexGLSL, vHLSL, src.vertexMSL,
                 src.spirvVert, src.spirvVertSize,
                 src.cpuVertFn, nullptr, nullptr);
        // Fragment
        addStage(NkShaderStage::NK_FRAGMENT,
                 src.fragmentGLSL, fHLSL, src.fragmentMSL,
                 src.spirvFrag, src.spirvFragSize,
                 nullptr, src.cpuFragFn, nullptr);
        // Geometry (optionnel)
        addStage(NkShaderStage::NK_GEOMETRY,
                 src.geometryGLSL, gHLSL, nullptr,
                 nullptr, 0, nullptr, nullptr, nullptr);
        // Compute
        addStage(NkShaderStage::NK_COMPUTE,
                 src.computeGLSL, cHLSL, src.computeMSL,
                 src.spirvComp, src.spirvCompSize,
                 nullptr, nullptr, src.cpuCompFn);

        return desc;
    }

    void NkShader::Destroy() noexcept {
        if (mDevice && mHandle.IsValid()) {
            mDevice->DestroyShader(mHandle);
        }
        mDevice = nullptr;
    }

    // =========================================================================
    // NkPipeline
    // =========================================================================

    bool NkPipeline::Create(NkIDevice* device, const NkGraphicsPipelineDesc& desc) noexcept {
        if (!device) return false;
        Destroy();
        mDevice = device;
        mHandle = device->CreateGraphicsPipeline(desc);
        return mHandle.IsValid();
    }

    bool NkPipeline::CreateCompute(NkIDevice* device, const NkComputePipelineDesc& desc) noexcept {
        if (!device) return false;
        Destroy();
        mDevice = device;
        mHandle = device->CreateComputePipeline(desc);
        return mHandle.IsValid();
    }

    void NkPipeline::Destroy() noexcept {
        if (mDevice && mHandle.IsValid()) {
            mDevice->DestroyPipeline(mHandle);
        }
        mDevice = nullptr;
    }

} // namespace nkentseu
