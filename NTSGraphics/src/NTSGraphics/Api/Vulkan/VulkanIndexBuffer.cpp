//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-20 at 09:21:33 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanIndexBuffer.h"

#include "NTSGraphics/Core/Context.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {

    VulkanIndexBuffer::VulkanIndexBuffer(Memory::Shared<Context>  context) : m_Context(Memory::Cast<VulkanContext>(context))
    {
    }

    Memory::Shared<Context>  VulkanIndexBuffer::GetContext()
    {
        return m_Context;
    }

    // Destructor
    VulkanIndexBuffer::~VulkanIndexBuffer() {
        // Ajoutez votre code de destructeur ici
    }

    bool VulkanIndexBuffer::Create(BufferUsageType bufferUsage, const std::vector<uint32>& indices) {
        return Create(bufferUsage, IndexBufferType::Enum::UInt32, indices.data(), indices.size());
    }

    bool VulkanIndexBuffer::Destroy() {
        if (m_Context == nullptr) return false;
        bool success = m_IndexBufferObject.Destroy(m_Context.get());
        return success;
    }

    bool VulkanIndexBuffer::SetData(const void* data, usize size)
    {
        if (m_Context == nullptr) return false;

        usize correctSize = VulkanContext::AlignBufferSize(m_IndexBufferObject.size, dynamicAlignment);

        m_IndexBufferObject.Mapped(m_Context.get(), correctSize, 0);
        bool success = m_IndexBufferObject.WriteToBuffer(data, size, 0);
        success = m_IndexBufferObject.Flush(m_Context.get(), VK_WHOLE_SIZE, 0);
        m_IndexBufferObject.UnMapped(m_Context.get());
        return success;
    }

    bool VulkanIndexBuffer::Create(BufferUsageType bufferUsage, IndexBufferType indexType, const void* indices, uint32 leng)
    {
        if (m_Context == nullptr) return false;

        usize size = leng * IndexBufferType::SizeOf(indexType);
        m_IndexBufferObject.size = size;
        m_Leng = leng;

        if (leng == 0) {
            gLog.Error("Size cannot set to zero");
            return false;
        }

        bool success = true;

        if (bufferUsage == BufferUsageType::Enum::StaticDraw && indices != nullptr) {
            success = StaticBufferCreation(indices);
        }
        else {
            success = DynamicBufferCreation();
        }

        if (success) {
            gLog.Info("Create index buffer is good");
        }

        return success;
    }

    VkBufferInternal* VulkanIndexBuffer::GetBuffer()
    {
        if (m_Context == nullptr) return nullptr;
        return &m_IndexBufferObject;
    }

    IndexBufferType VulkanIndexBuffer::GetIndexType() const {
        return m_IndexType;
    }

    uint32 VulkanIndexBuffer::Leng() const
    {
        return m_Leng;
    }

    bool VulkanIndexBuffer::StaticBufferCreation(const void* data)
    {
        usize size = m_IndexBufferObject.size;
        range = size;
        dynamicAlignment = 0;

        vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;
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

        if (!VkBufferInternal::CreateBuffer(m_Context.get(), size, usage, sharingMode, propertyFlags2, m_IndexBufferObject.buffer, m_IndexBufferObject.bufferMemory)) {
            gLog.Error("cannot create buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!VkBufferInternal::CopyBuffer(m_Context.get(), stanging.buffer, m_IndexBufferObject.buffer, size)) {
            gLog.Error("cannot copy stangin buffer into buffer");
            return stanging.Destroy(m_Context.get()) && false;
        }

        if (!stanging.Destroy(m_Context.get())) {
            gLog.Error("cannot destroy stangin buffer");
            return false;
        }
        return true;
    }

    bool VulkanIndexBuffer::DynamicBufferCreation()
    {
        usize size = m_IndexBufferObject.size;
        vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
        vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible;
        vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eIndexBuffer;

        uint32 minUboAlignment = m_Context->properties.limits.minUniformBufferOffsetAlignment;

        if (minUboAlignment > 0) {
            dynamicAlignment = (size + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }

        range = dynamicAlignment;
        size = dynamicAlignment * m_Leng;

        if (!VkBufferInternal::CreateBuffer(m_Context.get(), size, usage, sharingMode, propertyFlags, m_IndexBufferObject.buffer, m_IndexBufferObject.bufferMemory)) {
            gLog.Error("cannot create buffer");
            return false;
        }

        return true;
    }
}  //  nkentseu