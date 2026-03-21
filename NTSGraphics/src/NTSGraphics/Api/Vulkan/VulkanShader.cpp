//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-15 at 04:56:52 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanShader.h"

#include <fstream>
#include <sstream>

#include "VulkanContext.h"
#include "VulkanUtils.h"

#include <string>
#include <filesystem>

#include <utility>
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
    using namespace maths;
    
    VulkanShader::VulkanShader(Memory::Shared<Context> context) : pipeline(nullptr), m_Context(Memory::Cast<VulkanContext>(context)) {
        if (m_Context != nullptr) {
            m_Context->AddRecreateCallback(SWAPCHAIN_CALLBACK_FN(VulkanShader::Recreate));
            m_Context->AddCleanUpCallback(SWAPCHAIN_CALLBACK_FN(VulkanShader::CleanUp));
        }
    }

    VulkanShader::~VulkanShader() {
    }

    Memory::Shared<Context> VulkanShader::GetContext() {
        return m_Context;
    }

    bool VulkanShader::LoadFromFile(const ShaderFilePathLayout& shaderFiles, Memory::Shared<ShaderInputLayout> shaderInputLayout)
    {
        m_VkSil =  Memory::Cast<VulkanShaderInputLayout>(shaderInputLayout);
        if (m_Context == nullptr || shaderFiles.size() == 0 || m_VkSil == nullptr) return false;
        if (!CreatePipeline(shaderFiles)) {
            gLog.Error("cannot create graphics pipeline");
            return false;
        }
        return true;
    }

    bool VulkanShader::Destroy() {
        if (m_Context == nullptr) return false;

        bindingDescriptions.clear();
        attributeDescriptions.clear();

        vkCheckErrorVoid(m_Context->device.destroyPipeline(pipeline, m_Context->allocator));

        if (m_Context != nullptr) {
            m_Context->RemoveRecreateCallback(SWAPCHAIN_CALLBACK_FN(VulkanShader::Recreate));
            m_Context->RemoveCleanUpCallback(SWAPCHAIN_CALLBACK_FN(VulkanShader::CleanUp));
        }
        return false;
    }

    vk::ShaderModule VulkanShader::MakeModule(const std::string& filepath, ShaderStage type) {
        if (m_Context == nullptr) return nullptr;
        vk::ShaderModule shaderModule = nullptr;
        VulkanResult result;
        bool first = true;

        std::string new_filepath = ReplaceShaderExtension(filepath);

        if (!CheckIfShaderExists(new_filepath)) {
            new_filepath = filepath;
        }

        std::vector<char> code = LoadShader(new_filepath);

        if (code.size() == 0) {
            return nullptr;
        }

        vk::ShaderModuleCreateInfo createInfo{};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32*>(code.data());

        vkCheckError(first, result, m_Context->device.createShaderModule(&createInfo, m_Context->allocator, &shaderModule), "failed to create shader module");

        if (!result.success || shaderModule == nullptr) {
            return nullptr;
        }

        return shaderModule;
    }

    bool VulkanShader::Bind() {
        if (m_Context == nullptr || pipeline == nullptr || !isCreate) return false;

        vk::CommandBuffer commandBuffer = m_Context->GetCommandBuffer();
        if (commandBuffer == nullptr) return false;

        Vector2u size = m_Context->dpiSize;

        vk::Rect2D scissor = {};
        scissor.extent = vk::Extent2D{ size.width, size.height };

        vk::Viewport viewport = {};
        viewport.width = size.width;
        viewport.height = size.height;
        viewport.maxDepth = 1.0f;

        vkCheckErrorVoid(commandBuffer.setScissor(0, 1, &scissor));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set scissor");
        }
        vkCheckErrorVoid(commandBuffer.setViewport(0, 1, &viewport));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set view port");
        }
        vkCheckErrorVoid(commandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleStrip));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set primitive topology");
        }
        vkCheckErrorVoid(commandBuffer.setFrontFace(vk::FrontFace::eCounterClockwise));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set front face");
        }
        vkCheckErrorVoid(commandBuffer.setCullMode(vk::CullModeFlagBits::eNone));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set cull mode");
        }

        vkCheckErrorVoid(commandBuffer.setDepthTestEnable(VK_TRUE));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot enable depth test");
        }

        vkCheckErrorVoid(commandBuffer.setDepthWriteEnable(VK_TRUE));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot enable depth write");
        }

        if (m_Context->cmdSetPolygonModeEXT != nullptr) {
            vkCheckErrorVoid(m_Context->cmdSetPolygonModeEXT((VkCommandBuffer)commandBuffer, VK_POLYGON_MODE_FILL));
            if (!VulkanStaticDebugInfo::Result()) {
                gLog.Error("cannot set polygon mode");
            }
        }

        /*VkBool32 blendEnable = VK_FALSE;

        VkColorBlendEquationEXT colorBlendEquation;
        colorBlendEquation.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendEquation.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendEquation.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendEquation.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendEquation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendEquation.alphaBlendOp = VK_BLEND_OP_ADD;

        float32 blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // Exemples de constantes

        vkCheckErrorVoid(commandBuffer.setBlendConstants(blendConstants));

        if (m_Context->cmdSetColorBlendEnableEXT != nullptr) {
            vkCheckErrorVoid(m_Context->cmdSetColorBlendEnableEXT((VkCommandBuffer)commandBuffer, 0, 1, &blendEnable));
            if (!VulkanStaticDebugInfo::Result()) {
                gLog.Error("cannot set color blend enable");
            }
        }

        if (m_Context->cmdSetColorBlendEquationEXT != nullptr) {
            vkCheckErrorVoid(m_Context->cmdSetColorBlendEquationEXT((VkCommandBuffer)commandBuffer, 0, 1, &colorBlendEquation));
            if (!VulkanStaticDebugInfo::Result()) {
                gLog.Error("cannot set color blend equation");
            }
        }

        VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (m_Context->cmdSetColorBlendEquationEXT != nullptr) {
            vkCheckErrorVoid(m_Context->cmdSetColorWriteMaskEXT((VkCommandBuffer)commandBuffer, 0, 1, &colorWriteMask));
            if (!VulkanStaticDebugInfo::Result()) {
                gLog.Error("cannot set color blend equation");
            }
        }*/

        vkCheckErrorVoid(commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot bind pipeline");
            return false;
        }
        
        return true;
    }

    bool VulkanShader::Unbind() {
        if (m_Context == nullptr) return false;
        return true;
    }

    bool VulkanShader::DefineVertexInput(VertexInputLayout vertexIL)
    {
        if (!vertexIL.attributes.empty()) {
            bindingDescriptions.resize(1);

            bindingDescriptions[0].binding = 0;
            bindingDescriptions[0].stride = vertexIL.stride;
            bindingDescriptions[0].inputRate = vk::VertexInputRate::eVertex;

            attributeDescriptions.resize(vertexIL.attributes.size());

            uint32 index = 0;
            bool set = true;

            for (const auto& attribute : vertexIL.attributes) {
                vk::Format format = VulkanConvert::ShaderInternalToVkFormat2(attribute.type);

                // Vérification si le format est valide
                if (format <= vk::Format::eUndefined || format > vk::Format::eAstc12x12SrgbBlock) {
                    gLog.Error("Invalid format for attribute type: {0}", attribute.type.ToString());
                    set = false;
                    attributeDescriptions.clear();
                    break;
                }

                attributeDescriptions[index].binding = 0;
                attributeDescriptions[index].location = attribute.location;
                attributeDescriptions[index].format = format;
                attributeDescriptions[index].offset = attribute.offset;

                index++;
            }
            return set;
        }
        return false;
    }

    bool VulkanShader::DefinePipelineStage(const ShaderFilePathLayout& shaderFiles)
    {
        for (auto attribut : shaderFiles) {
            shaderModules[(uint32)attribut.stage] = MakeModule(attribut.path, attribut.stage);

            if (shaderModules[(uint32)attribut.stage] == nullptr) {
                shaderModules.clear();
                gLog.Debug("Cannot create shader module;");
                return false;
            }

            vk::PipelineShaderStageCreateInfo shaderStage = {};
            shaderStage.stage = VulkanConvert::GetshaderStageType2(attribut.stage);
            shaderStage.module = shaderModules[(uint32)attribut.stage];
            shaderStage.pName = "main";

            shaderStages.push_back(shaderStage);
        }
        return true;
    }

    std::vector<char> VulkanShader::LoadShader(const std::string& shaderFile) {
        std::ifstream file(shaderFile, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            gLog.Error("failed to open file : {0}", shaderFile);
            return {};
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    bool VulkanShader::Recreate(bool force)
    {
        if (m_Context != nullptr) {
        }
        return true;
    }

    bool VulkanShader::CleanUp(bool force)
    {
        if (m_Context != nullptr) {
        }
        return true;
    }

    std::string VulkanShader::ReplaceShaderExtension(const std::string& shaderPath)
    {
        std::filesystem::path path(shaderPath);

        // Mapping des extensions a leurs equivalents pour l'extension .spv
        NkUnorderedMap<std::string, std::string> extensionMappings = {
            {".glsl", ".spv"},    // Extension .glsl sera remplac�e par .spv
            {".vert", ".vert.spv"},
            {".frag", ".frag.spv"},
            {".comp", ".comp.spv"},
            {".geom", ".geom.spv"},
            {".tesc", ".tesc.spv"},
            {".tese", ".tese.spv"}
        };

        // Obtention de l'extension du fichier
        std::string extension = path.extension().string();

        // V�rification si l'extension est dans la liste des extensions � mapper
        if (extensionMappings.find(extension) != extensionMappings.end()) {
            // Si oui, remplacement de l'extension
            path.replace_extension(extensionMappings[extension]);
        }

        // Retourner le chemin mis � jour
        return path.string();
    }

    bool VulkanShader::CheckIfShaderExists(const std::string& shaderPath)
    {
        return std::filesystem::exists(shaderPath);
    }

    bool VulkanShader::CompileShader(const std::string& filePath, ShaderStage shaderType) {
        // Mapping des type a leurs equivalence pour compilation
        NkUnorderedMap<uint32, std::string> fileTypeMappings = {
            {(uint32)ShaderStage::Enum::Vertex, "--vert"},
            {(uint32)ShaderStage::Enum::Fragment, "--frag"},
            {(uint32)ShaderStage::Enum::Compute, "--comp"},
            {(uint32)ShaderStage::Enum::Geometry, "--geom"},
            {(uint32)ShaderStage::Enum::TesControl, "--tesc"},
            {(uint32)ShaderStage::Enum::TesEvaluation, "--tese"}
        };

        std::filesystem::path path(filePath);
        std::string directory = path.parent_path().string();
        std::string fileName = path.filename().string();

        std::string command = FORMATTER.Format("python Scripts/commands/buildshdr.py --name {0} --directory {1} {2}", fileName, directory, fileTypeMappings[(uint32)shaderType]);

        int32 result = std::system(command.c_str());
        if (result != 0) {
            gLog.Error("Failed to compile shader.");
            return false;
        }
        return true;
    }

    bool VulkanShader::CreatePipeline(const ShaderFilePathLayout& shaderFiles)
    {
        shaderModules.clear();
        VulkanResult result;
        bool first = true;

        DefinePipelineStage(shaderFiles);

        vk::PipelineVertexInputStateCreateInfo vertexInputState = {};

        if (DefineVertexInput(m_VkSil->vertexInput)) {
            vertexInputState.vertexBindingDescriptionCount = bindingDescriptions.size();
            vertexInputState.pVertexBindingDescriptions = bindingDescriptions.data();
            vertexInputState.vertexAttributeDescriptionCount = attributeDescriptions.size();
            vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();
        }

        vk::PipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optionnel
        depthStencil.maxDepthBounds = 1.0f; // Optionnel
        depthStencil.stencilTestEnable = VK_FALSE;

        std::vector<vk::PipelineVertexInputStateCreateInfo> vertexInputStates;
        vertexInputStates.push_back(vertexInputState);

        vk::PipelineColorBlendAttachmentState colorAttachment = {};
        colorAttachment.blendEnable = m_VkSil->activateBlending ? VK_TRUE : VK_FALSE;

        colorAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorAttachment.colorBlendOp = vk::BlendOp::eAdd;
        if (m_VkSil->activateBlending) {
            colorAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
            colorAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
            //colorAttachment.srcAlphaBlendFactor = vk::BlendFactor::eSrcAlpha;
            //colorAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            colorAttachment.alphaBlendOp = vk::BlendOp::eAdd;
        }
        colorAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        std::vector<vk::PipelineColorBlendAttachmentState> colorAttachments;
        colorAttachments.push_back(colorAttachment);

        vk::PipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.pAttachments = colorAttachments.data();
        colorBlendState.attachmentCount = colorAttachments.size();
        colorBlendState.logicOpEnable = VK_FALSE;
        colorBlendState.logicOp = vk::LogicOp::eCopy; // Optional
        //colorBlendState.logicOp = vk::LogicOp::eAnd; // Optional
        colorBlendState.blendConstants[0] = 1.0f; // Optional
        colorBlendState.blendConstants[1] = 1.0f; // Optional
        colorBlendState.blendConstants[2] = 1.0f; // Optional
        colorBlendState.blendConstants[3] = 1.0f; // Optional

        std::vector<vk::PipelineColorBlendStateCreateInfo> colorBlendStates;
        colorBlendStates.push_back(colorBlendState);

        vk::PipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.frontFace = vk::FrontFace::eClockwise;
        rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
        rasterizationState.polygonMode = vk::PolygonMode::eFill;
        rasterizationState.lineWidth = 1.0f;

        std::vector<vk::PipelineRasterizationStateCreateInfo> rasterizationStates;
        rasterizationStates.push_back(rasterizationState);

        vk::PipelineMultisampleStateCreateInfo multiSample = {};
        multiSample.rasterizationSamples = vk::SampleCountFlagBits::e1;

        std::vector<vk::PipelineMultisampleStateCreateInfo> multiSamples;
        multiSamples.push_back(multiSample);

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

        std::vector<vk::PipelineInputAssemblyStateCreateInfo> inputAssemblies;
        inputAssemblies.push_back(inputAssembly);

        std::vector<vk::Rect2D> scissors;
        scissors.push_back(vk::Rect2D{});
        std::vector<vk::Viewport> viewports;
        viewports.push_back(vk::Viewport{});

        vk::PipelineViewportStateCreateInfo viewportState = {};
        viewportState.scissorCount = scissors.size();
        viewportState.pScissors = scissors.data();
        viewportState.viewportCount = viewports.size();
        viewportState.pViewports = viewports.data();

        std::vector<vk::PipelineViewportStateCreateInfo> viewportStates;
        viewportStates.push_back(viewportState);

        std::vector<vk::DynamicState> dynamicStates;
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_VIEWPORT);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_SCISSOR);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_FRONT_FACE);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_CULL_MODE);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        dynamicStates.push_back((vk::DynamicState)VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        //dynamicStates.push_back(vk::DynamicState::eBlendConstants);
        //dynamicStates.push_back(vk::DynamicState::eColorBlendEnableEXT);
        //dynamicStates.push_back(vk::DynamicState::eColorBlendEquationEXT);
        //dynamicStates.push_back(vk::DynamicState::eColorWriteMaskEXT);

        vk::PipelineDynamicStateCreateInfo dynamicStateInfo = {};
        dynamicStateInfo.pDynamicStates = dynamicStates.data();
        dynamicStateInfo.dynamicStateCount = dynamicStates.size();

        std::vector<vk::PipelineDynamicStateCreateInfo> dynamicStateInfos;
        dynamicStateInfos.push_back(dynamicStateInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.layout = m_VkSil->pipelineLayout;
        pipelineInfo.renderPass = m_Context->renderPass;
        pipelineInfo.pVertexInputState = vertexInputStates.data();
        pipelineInfo.pColorBlendState = colorBlendStates.data();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.stageCount = shaderStages.size();
        pipelineInfo.pRasterizationState = rasterizationStates.data();
        pipelineInfo.pViewportState = viewportStates.data();
        pipelineInfo.pDynamicState = dynamicStateInfos.data();
        pipelineInfo.pMultisampleState = multiSamples.data();
        pipelineInfo.pInputAssemblyState = inputAssemblies.data();
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.subpass = 0;

        std::vector<vk::GraphicsPipelineCreateInfo> pipelineInfos;
        pipelineInfos.push_back(pipelineInfo);

        vkCheckError(first, result, m_Context->device.createGraphicsPipelines(nullptr, pipelineInfos.size(), pipelineInfos.data(), m_Context->allocator, &pipeline), "cannot create graphics pipelines");
        //vkCheckError(first, result, m_Context->device.createGraphicsPipelines(nullptr, 1, &pipelineInfo, m_Context->allocator, &pipeline), "cannot create graphics pipelines");

        if (!result.success) {
            return false;
        }

        for (auto [shaderType, module] : shaderModules) {
            if (!VulkanStaticDebugInfo::Result()) {
                gLog.Warning("cannot destroy shader module");
            }
            vkCheckErrorVoid(m_Context->device.destroyShaderModule(module, m_Context->allocator));
            if (!VulkanStaticDebugInfo::Result()) {
                gLog.Error("cannot destroy shader module");
            }
        }

        gLog.Info("Create gaphics pipeline is good");
        isCreate = result.success;
        return result.success;
    }

}  //  nkentseu