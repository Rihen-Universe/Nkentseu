//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-03 at 08:33:11 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_UNIFORM_BUFFER_H__
#define __VULKAN_UNIFORM_BUFFER_H__

#pragma once

#include <vulkan/vulkan.hpp>
#include <NTSCore/System.h>
#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/Shader.h"
#include "NTSGraphics/Core/UniformBuffer.h"
#include "VulkanContext.h"
#include "VulkanShader.h"

namespace nkentseu {
    
    class NKENTSEU_API VulkanUniformBuffer : public UniformBuffer {
        public:
            VulkanUniformBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
            ~VulkanUniformBuffer();

            bool Create(Memory::Shared<Shader> shader) override;

            bool SetData(const std::string& name, void* data, usize size) override;
            bool SetData(const std::string& name, void* data, usize size, uint32 index) override;
            bool Bind(const std::string& name) override;

            Memory::Shared<Context> GetContext() override;
            bool Destroy() override;
        private:
            Memory::Shared<VulkanContext> m_Context;
            Memory::Shared<VulkanShader> m_Shader;
            Memory::Shared<VulkanShaderInputLayout> m_Vksil = nullptr;

            NkUnorderedMap<std::string, VkUniformBufferInternal> m_UniformBuffers;
            NkUnorderedMap<std::string, UniformBufferDescriptorSet> m_DescriptorSets;

            bool Recreate(bool force);
            bool CleanUp(bool force);
            bool FreeData();

            bool Create();
    };

}  //  nkentseu

#endif  // __VULKAN_UNIFORM_BUFFER_H__!