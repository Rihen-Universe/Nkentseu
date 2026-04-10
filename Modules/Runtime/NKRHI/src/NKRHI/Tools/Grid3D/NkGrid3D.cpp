#include "NkGrid3D.h"
#include "NkGrid3DShaders.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    static uint32 PackFlags(const NkGrid3DConfig& cfg) {
        uint32 flags = 0;
        if (cfg.infinite)       flags |= 1u;
        if (cfg.showAxes)       flags |= 2u;
        if (cfg.showSolidFloor) flags |= 4u;
        return flags;
    }

    NkGrid3D::NkGrid3D() = default;
    NkGrid3D::~NkGrid3D() { Shutdown(); }

    bool NkGrid3D::Init(NkIDevice* device, NkRenderPassHandle renderPass) {
        if (!device || !device->IsValid()) {
            logger.Error("[NkGrid3D] Invalid device\n");
            return false;
        }
        mDevice = device;

        if (!CreatePipeline(renderPass)) {
            logger.Error("[NkGrid3D] Failed to create pipeline\n");
            return false;
        }

        // Créer un sampler dummy (obligatoire pour le binding, mais inutilisé)
        NkSamplerDesc samplerDesc = NkSamplerDesc::Linear();
        mDummySampler = mDevice->CreateSampler(samplerDesc);

        // Créer un descriptor set layout (un seul binding de sampler dummy)
        NkDescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Add(0, NkDescriptorType::NK_SAMPLER, NkShaderStage::NK_FRAGMENT);
        mDescriptorSetLayout = mDevice->CreateDescriptorSetLayout(layoutDesc);
        if (!mDescriptorSetLayout.IsValid()) {
            logger.Error("[NkGrid3D] Failed to create descriptor set layout\n");
            return false;
        }

        // Allouer un descriptor set réutilisable
        mDescriptorSet = mDevice->AllocateDescriptorSet(mDescriptorSetLayout);
        if (!mDescriptorSet.IsValid()) {
            logger.Error("[NkGrid3D] Failed to allocate descriptor set\n");
            return false;
        }

        // Écrire le sampler dummy dans le set (une fois pour toutes)
        NkDescriptorWrite write;
        write.set = mDescriptorSet;
        write.binding = 0;
        write.type = NkDescriptorType::NK_SAMPLER;
        write.sampler = mDummySampler;
        mDevice->UpdateDescriptorSets(&write, 1);

        mInitialized = true;
        return true;
    }

    void NkGrid3D::Shutdown() {
        if (mDevice) {
            if (mPipeline.IsValid())          mDevice->DestroyPipeline(mPipeline);
            if (mShader.IsValid())            mDevice->DestroyShader(mShader);
            if (mDescriptorSet.IsValid())     mDevice->FreeDescriptorSet(mDescriptorSet);
            if (mDescriptorSetLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mDescriptorSetLayout);
            if (mDummySampler.IsValid())      mDevice->DestroySampler(mDummySampler);
        }
        mDevice = nullptr;
        mInitialized = false;
    }

    bool NkGrid3D::CreatePipeline(NkRenderPassHandle renderPass) {
        // Shader combiné vertex + fragment
        NkShaderDesc shaderDesc;
        shaderDesc.AddGLSL(NkShaderStage::NK_VERTEX, grid3dshaders::kVertexShaderGLSL, "main");
        shaderDesc.AddGLSL(NkShaderStage::NK_FRAGMENT, grid3dshaders::kFragmentShaderGLSL, "main");
        shaderDesc.debugName = "Grid3D_Shader";
        mShader = mDevice->CreateShader(shaderDesc);
        if (!mShader.IsValid()) {
            logger.Error("[NkGrid3D] Failed to create shader\n");
            return false;
        }

        // Layout des descripteurs (sampler dummy)
        NkDescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Add(0, NkDescriptorType::NK_SAMPLER, NkShaderStage::NK_FRAGMENT);
        NkDescSetHandle setLayout = mDevice->CreateDescriptorSetLayout(layoutDesc);
        if (!setLayout.IsValid()) return false;

        // Pipeline description
        NkGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.shader = mShader;
        pipelineDesc.vertexLayout = NkVertexLayout{}; // pas de vertex buffer
        pipelineDesc.topology = NkPrimitiveTopology::NK_TRIANGLE_STRIP;
        pipelineDesc.rasterizer = NkRasterizerDesc::Default();
        pipelineDesc.rasterizer.cullMode = NkCullMode::NK_NONE;
        pipelineDesc.rasterizer.depthClip = false;
        pipelineDesc.depthStencil = NkDepthStencilDesc::NoDepth(); // pas de test depth
        pipelineDesc.blend = NkBlendDesc::Alpha();
        pipelineDesc.samples = NkSampleCount::NK_S1;
        pipelineDesc.renderPass = renderPass;
        pipelineDesc.subpass = 0;

        // Push constant range unique (vertex + fragment partagent les mêmes données)
        pipelineDesc.AddPushConstant(NkShaderStage::NK_VERTEX | NkShaderStage::NK_FRAGMENT, 0,
                                    sizeof(float)*4 + sizeof(int)*2 + sizeof(float)*6 + sizeof(uint32) + sizeof(float)*4*5);
        pipelineDesc.descriptorSetLayouts.PushBack(setLayout);

        mPipeline = mDevice->CreateGraphicsPipeline(pipelineDesc);
        mDevice->DestroyDescriptorSetLayout(setLayout); // plus besoin, copie interne faite

        if (!mPipeline.IsValid()) {
            logger.Error("[NkGrid3D] Failed to create graphics pipeline\n");
            return false;
        }
        return true;
    }

    void NkGrid3D::UpdatePushConstants(NkICommandBuffer* cmd,
                                    const math::NkMat4& invView,
                                    const math::NkMat4& invProj) {
        uint32 flags = PackFlags(mConfig);

        struct PushConstants {
            // Vertex stage
            math::NkMat4 invProj;
            math::NkMat4 invView;
            // Fragment stage
            float baseUnit;
            int   subDivisions;
            float extent;
            float fadeStart;
            float fadeEnd;
            float lineWidthMajor;
            float lineWidthMinor;
            uint32 flags;
            float majorColor[4];
            float minorColor[4];
            float axisXColor[4];
            float axisZColor[4];
            float solidColor[4];
        } pc;

        pc.invProj = invProj;
        pc.invView = invView;
        pc.baseUnit = mConfig.baseUnit;
        pc.subDivisions = mConfig.subDivisions;
        pc.extent = mConfig.extent;
        pc.fadeStart = mConfig.fadeStart;
        pc.fadeEnd = mConfig.fadeEnd;
        pc.lineWidthMajor = mConfig.lineWidthMajor;
        pc.lineWidthMinor = mConfig.lineWidthMinor;
        pc.flags = flags;

        auto toFloat4 = [](const math::NkColor& c) {
            return float{ c.r * math::c1_255, c.g * math::c1_255, c.b * math::c1_255, c.a * math::c1_255};
        };
        auto copyColor = [&](float* dst, const math::NkColor& src) {
            dst[0] = src.r * math::c1_255; 
            dst[1] = src.g * math::c1_255; 
            dst[2] = src.b * math::c1_255; 
            dst[3] = src.a * math::c1_255;
        };
        copyColor(pc.majorColor, mConfig.majorColor);
        copyColor(pc.minorColor, mConfig.minorColor);
        copyColor(pc.axisXColor, mConfig.axisXColor);
        copyColor(pc.axisZColor, mConfig.axisZColor);
        copyColor(pc.solidColor, mConfig.solidColor);

        cmd->PushConstants(NkShaderStage::NK_VERTEX | NkShaderStage::NK_FRAGMENT, 0, sizeof(pc), &pc);
    }

    void NkGrid3D::Draw(NkICommandBuffer* cmd,
                        const math::NkMat4& viewMatrix,
                        const math::NkMat4& projMatrix,
                        uint32 width, uint32 height) {
        if (!mInitialized || !cmd) return;

        math::NkMat4 invView = viewMatrix.Inverse();
        math::NkMat4 invProj = projMatrix.Inverse();

        cmd->BindGraphicsPipeline(mPipeline);
        cmd->BindDescriptorSet(mDescriptorSet, 0);
        UpdatePushConstants(cmd, invView, invProj);

        // Quad plein écran (4 sommets, triangle strip)
        cmd->Draw(4, 1, 0, 0);
    }

} // namespace nkentseu