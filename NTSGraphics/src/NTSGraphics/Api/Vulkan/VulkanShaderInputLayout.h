//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-29 at 10:08:47 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_SHADER_INPUT_LAYOUT_H__
#define __VULKAN_SHADER_INPUT_LAYOUT_H__

#pragma once

#include <vulkan/vulkan.hpp>
#include <NTSCore/System.h>

#include "NTSGraphics/Core/ShaderInputLayout.h"
#include "NTSGraphics/Core/Context.h"
#include "VulkanContext.h"

namespace nkentseu {
    
    class NKENTSEU_API VulkanShaderInputLayout : public ShaderInputLayout {
    public:
        VulkanShaderInputLayout(Memory::Shared<Context> context);
        ~VulkanShaderInputLayout();

        virtual bool Initialize() override;
        virtual bool Release() override;

        virtual bool UpdatePushConstant(const std::string& name, void* data, usize size, Memory::Shared<Shader> shader = nullptr) override;
    private:
        friend class VulkanShader;
        friend class VulkanUniformBuffer;
        friend class VulkanTexture2D;
        friend class RenderCache2D;
        friend class VulkanTexture2DBinding;

        Memory::Shared<VulkanContext> m_Context;

        bool CreatePipelineLayout();
        bool DestroyPipelineLayout();

        bool CreateDescriptorSetLayout();

        bool IsValid() const;

        NkUnorderedMap<BufferSpecificationType::Enum, std::vector<vk::DescriptorSetLayoutBinding>> layoutBindings{};
        std::vector<vk::PushConstantRange> pushConstantRanges{};

        vk::PipelineLayout pipelineLayout = nullptr;
        NkUnorderedMap<BufferSpecificationType::Enum, std::vector<vk::DescriptorSetLayout>> descriptorSetLayouts{};
    };

}  //  nkentseu

#endif  // __VULKAN_SHADER_INPUT_LAYOUT_H__!