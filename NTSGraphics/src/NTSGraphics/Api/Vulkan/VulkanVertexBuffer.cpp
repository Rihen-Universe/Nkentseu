
//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-20 at 09:11:16 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanVertexBuffer.h"

#include "NTSGraphics/Core/Context.h"
#include "VulkanContext.h"

#include <vulkan/vulkan.hpp>
#include "VulkanUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {

    VulkanVertexBuffer::VulkanVertexBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<VulkanContext>(context)) {
        m_Vksil = Memory::Cast<VulkanShaderInputLayout>(sil);
    }

    Memory::Shared<Context> VulkanVertexBuffer::GetContext()
    {
        return m_Context;
    }

    VulkanVertexBuffer::~VulkanVertexBuffer() {
    }

    bool VulkanVertexBuffer::Create(BufferUsageType bufferUsage, const std::vector<float32>& vertices) {
        if (m_Vksil == nullptr) return false;

        return Create(bufferUsage, vertices.data(), vertices.size() / m_Vksil->vertexInput.componentCount);
    }

    bool VulkanVertexBuffer::Create(BufferUsageType bufferUsage, const void* vertices, uint32 leng) {
        if (m_Context == nullptr || m_Vksil == nullptr) return false;

        usize size = leng * m_Vksil->vertexInput.stride;
        m_VertexBufferObject.size = size;
        m_Leng = leng;

        if (leng == 0) {
            gLog.Error("Size cannot set to zero");
            return false;
        }

        bool success = true;

        if (bufferUsage == BufferUsageType::Enum::StaticDraw && vertices != nullptr) {
            success = StaticBufferCreation(vertices);
        }
        else {
            success = DynamicBufferCreation();

            if (vertices != nullptr) {
                SetData(vertices, size);
            }
        }

        if (success) {
            gLog.Info("Create vertex buffer is good");
        }

        return success;
    }

    bool VulkanVertexBuffer::SetData(const void* data, usize size)
    {
        if (m_Context == nullptr) return false;

        usize correctSize = VulkanContext::AlignBufferSize(m_VertexBufferObject.size, dynamicAlignment);

        m_VertexBufferObject.Mapped(m_Context.get(), correctSize, 0);
        bool success = m_VertexBufferObject.WriteToBuffer(data, size, 0);
        success = m_VertexBufferObject.Flush(m_Context.get(), VK_WHOLE_SIZE, 0);
        m_VertexBufferObject.UnMapped(m_Context.get());

        return success;
    }

    VkBufferInternal* VulkanVertexBuffer::GetBuffer()
    {
        if (m_Context == nullptr) return nullptr;
        return &m_VertexBufferObject;
    }

    uint32 VulkanVertexBuffer::Leng() const {
        return m_Leng;
    }

    bool VulkanVertexBuffer::StaticBufferCreation(const void* data)
    {
        usize size = m_VertexBufferObject.size;
        range = size;
        dynamicAlignment = 0;

        vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::SharingMode sharingMode = vk::SharingMode::eExclusive;

        vk::BufferUsageFlags usage_t = vk::BufferUsageFlagBits::eTransferSrc;
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        vk::MemoryPropertyFlags propertyFlags2 = vk::MemoryPropertyFlagBits::eDeviceLocal;

        VkBufferInternal stanging;

        if (!VkBufferInternal::CreateBuffer(m_Context.get(), size, usage_t, sharingMode, propertyFlags, stanging.buffer, stanging.bufferMemory)) {
            gLog.Error("cannot create stangin buffer");
            return false;
        }

        if (!stanging.Mapped(m_Context.get(), size, 0, {})) {
            gLog.Error("cannot mapped stangin buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!stanging.WriteToBuffer(data, size, 0)) {
            gLog.Error("cannot write in stangin buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!stanging.UnMapped(m_Context.get())) {
            gLog.Error("cannot unmap stangin buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!VkBufferInternal::CreateBuffer(m_Context.get(), size, usage, sharingMode, propertyFlags2, m_VertexBufferObject.buffer, m_VertexBufferObject.bufferMemory)) {
            gLog.Error("cannot create buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!VkBufferInternal::CopyBuffer(m_Context.get(), stanging.buffer, m_VertexBufferObject.buffer, size)) {
            gLog.Error("cannot copy stangin buffer into buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!stanging.Destroy(m_Context.get())) {
            gLog.Error("cannot destroy stangin buffer");
            return false;
        }
        return true;
    }

    bool VulkanVertexBuffer::DynamicBufferCreation()
    {
        usize size = m_VertexBufferObject.size;
        vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;
        vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eVertexBuffer;

        uint32 minUboAlignment = m_Context->properties.limits.minUniformBufferOffsetAlignment;

        if (minUboAlignment > 0) {
            dynamicAlignment = (size + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }

        range = dynamicAlignment;
        size = dynamicAlignment * m_Leng;

        if (!VkBufferInternal::CreateBuffer(m_Context.get(), size, usage, sharingMode, propertyFlags, m_VertexBufferObject.buffer, m_VertexBufferObject.bufferMemory)) {
            gLog.Error("cannot create buffer");
            return false;
        }

        return true;
    }

    bool VulkanVertexBuffer::Destroy() {
        if (m_Context == nullptr) return false;
        bool success = m_VertexBufferObject.Destroy(m_Context.get());
        return success;
    }

}  //  nkentseu
