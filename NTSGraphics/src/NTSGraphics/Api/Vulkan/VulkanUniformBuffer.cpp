//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-03 at 08:33:11 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanUniformBuffer.h"
#include "VulkanUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
    
    VulkanUniformBuffer::VulkanUniformBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<VulkanContext>(context)){
        m_Vksil = Memory::Cast<VulkanShaderInputLayout>(sil);
        if (m_Context != nullptr && m_Vksil != nullptr) {
            m_Context->AddRecreateCallback(SWAPCHAIN_CALLBACK_FN(VulkanUniformBuffer::Recreate));
            m_Context->AddCleanUpCallback(SWAPCHAIN_CALLBACK_FN(VulkanUniformBuffer::CleanUp));
        }
    }

    VulkanUniformBuffer::~VulkanUniformBuffer() {
    }

    bool VulkanUniformBuffer::Create(Memory::Shared<Shader> shader)
    {
        m_Shader = Memory::Cast<VulkanShader>(shader);
        return Create();
    }

    Memory::Shared<Context> VulkanUniformBuffer::GetContext()
    {
        return m_Context;
    }

    bool VulkanUniformBuffer::SetData(const std::string& name, void* data, usize size)
    {
        if (m_Context == nullptr || m_Context->GetCommandBuffer() == nullptr || data == nullptr || size == 0) return false;

        auto it = m_UniformBuffers.find(name);
        if (it == m_UniformBuffers.end()) {
            return false;
        }

        auto& uniforms = it->second;

        uniforms.Binds(m_Context.get(), data, size, 0);
        return false;
    }

    bool VulkanUniformBuffer::SetData(const std::string& name, void* data, usize size, uint32 index)
    {
        if (m_Context == nullptr || m_Context->GetCommandBuffer() == nullptr || data == nullptr || size == 0) return false;

        auto it = m_UniformBuffers.find(name);
        if (it == m_UniformBuffers.end()) {
            return false;
        }

        bool success = true;
        auto& uniforms = it->second;
        success = uniforms.Binds(m_Context.get(), data, size, index);
        //success = uniforms.Bind(m_Context->GetDevice(), data, size, m_Context->currentImageIndex, index);
        return success;
    }

    bool VulkanUniformBuffer::Bind(const std::string& name)
    {
        if (m_Context == nullptr || m_Context->GetCommandBuffer() == nullptr || m_Vksil == nullptr) return false;

        auto it = m_DescriptorSets.find(name);
        if (it == m_DescriptorSets.end()) {
            return false;
        }

        auto it2 = m_UniformBuffers.find(name);
        if (it2 == m_UniformBuffers.end()) {
            return false;
        }

        std::vector<uint32> dynamicOffsets = {};

        for (auto& [nom, uniform] : m_UniformBuffers) {
            if (uniform.dynamicAlignment != 0) {
                dynamicOffsets.push_back(uniform.currentOffset);
            }
        }

        uint32 slot = it2->second.uniformInput.binding;
        uint32 set = it2->second.uniformInput.set;

        return it->second.Bind(m_Context, m_Vksil->pipelineLayout, name, slot, set, m_Context->currentImageIndex, dynamicOffsets);
    }

    bool VulkanUniformBuffer::Destroy()
    {
        if (FreeData()) {
            m_Context->RemoveRecreateCallback(SWAPCHAIN_CALLBACK_FN(VulkanUniformBuffer::Recreate));
            m_Context->RemoveCleanUpCallback(SWAPCHAIN_CALLBACK_FN(VulkanUniformBuffer::CleanUp));
            return true;
        }
        return false;
    }

    bool VulkanUniformBuffer::Recreate(bool force)
    {
        return Create();
    }

    bool VulkanUniformBuffer::CleanUp(bool force)
    {
        return FreeData();
    }

    bool VulkanUniformBuffer::FreeData()
    {
        if (m_Context == nullptr || m_Vksil == nullptr) return false;
        if (m_Vksil->uniformInput.attributes.size() > 0) {
            for (auto& attribut : m_Vksil->uniformInput.attributes) {
                if (!m_UniformBuffers[attribut.name].Destroy(m_Context.get())) {
                  gLog.Error("Cannot destroy uniforme buffer : name = {0}", attribut.name);
                }
            }
            m_UniformBuffers.clear();
        }

        bool success = true;
        for (auto [name, descriptor] : m_DescriptorSets) {
            success = descriptor.Destroy(m_Context) && success;
        }

        return success;
    }

    bool VulkanUniformBuffer::Create()
    {
        if (m_Context == nullptr || m_Vksil == nullptr) return false;
        auto it = m_Vksil->descriptorSetLayouts.find(BufferSpecificationType::Enum::Uniform);
        if (it == m_Vksil->descriptorSetLayouts.end()) {
            gLog.Error("Uniform buffer descriptor set layout not found.");
            return false;
        }

        if (m_Vksil->uniformInput.attributes.size() > 0) {
            vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eUniformBuffer;
            uint32 imageCount = m_Context->swapchainImages.size();
            std::vector<vk::WriteDescriptorSet> descriptorWrites;
            bool success = true;

            for (auto& attribut : m_Vksil->uniformInput.attributes) {

                m_UniformBuffers[attribut.name] = {};
                vk::DescriptorType descriptorType = vk::DescriptorType::eUniformBuffer;
                if (attribut.usage == BufferUsageType::Enum::DynamicDraw) {
                    descriptorType = vk::DescriptorType::eUniformBufferDynamic;
                }

                if (!m_UniformBuffers[attribut.name].Create(m_Context.get(), attribut, usage, m_Context->swapchainImages.size(), descriptorType)) {
                    gLog.Error("Cannot create uniforme buffer : name = {0}", attribut.name);
                    success = false;
                }
                else {
                    m_DescriptorSets[attribut.name].SetDataBindings(&m_UniformBuffers[attribut.name]);

                    std::vector<vk::DescriptorPoolSize> poolSizes;
                    vk::DescriptorPoolSize poolSize{};
                    poolSize.type = attribut.usage == BufferUsageType::Enum::DynamicDraw ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eUniformBuffer;
                    poolSize.descriptorCount = m_Context->swapchainImages.size();
                    poolSizes.push_back(poolSize);

                    gLog.Debug();
                    if (!m_DescriptorSets[attribut.name].Create(m_Context, { attribut.binding }, it->second, poolSizes)) {
                        gLog.Error("Cannot create descriptor set for uniform buffer at set {0}, binding {1}, name {2}", attribut.set, attribut.binding, attribut.name);
                    }
                }
            }
            return success;
        }
        return false;
    }

}  //  nkentseu