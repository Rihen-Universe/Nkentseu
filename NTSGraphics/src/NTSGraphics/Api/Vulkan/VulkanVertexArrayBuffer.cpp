//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-20 at 09:22:16 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanVertexArrayBuffer.h"

#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/VertexBuffer.h"
#include "NTSGraphics/Core/IndexBuffer.h"

#include "VulkanContext.h"
#include "VulkanVertexBuffer.h"
#include "VulkanIndexBuffer.h"
#include "VulkanUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {

    VulkanVertexArrayBuffer::VulkanVertexArrayBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<VulkanContext>(context)) {
        m_Vksil = Memory::Cast<VulkanShaderInputLayout>(sil);
    }

    // Destructor
    VulkanVertexArrayBuffer::~VulkanVertexArrayBuffer() {
        // Ajoutez votre code de destructeur ici
    }

    Memory::Shared<Context> VulkanVertexArrayBuffer::GetContext() {
        return m_Context;
    }

    bool VulkanVertexArrayBuffer::Create() {
        if (m_Context == nullptr) return false;
        return true;
    }

    bool VulkanVertexArrayBuffer::Create(uint32 vertexNumber)
    {
        if (m_Context == nullptr) return false;
        m_VertexNumber = vertexNumber;
        return true;
    }

    uint32 VulkanVertexArrayBuffer::GetVertexLeng() {
        if (m_Context == nullptr || m_VertexBuffer == nullptr) return 0;
        return m_VertexBuffer->Leng();
    }

    bool VulkanVertexArrayBuffer::Destroy() {
        if (m_Context == nullptr) return false;
        return true;
    }

    bool VulkanVertexArrayBuffer::SetVertexBuffer(Memory::Shared<VertexBuffer> vertexBuffer) {
        if (m_Context == nullptr || m_Vksil == nullptr) return false;

        m_VertexBuffer = Memory::Cast<VulkanVertexBuffer>(vertexBuffer);

        return m_VertexBuffer != nullptr;
    }

    Memory::Shared<VertexBuffer> VulkanVertexArrayBuffer::GetVertexBuffer() {
        if (m_Context == nullptr || m_Vksil == nullptr) return nullptr;
        return m_VertexBuffer;
    }

    bool VulkanVertexArrayBuffer::SetIndexBuffer(Memory::Shared<IndexBuffer> indexBuffer) {
        if (m_Context == nullptr || m_Vksil == nullptr) return false;

        m_IndexBuffer = Memory::Cast<VulkanIndexBuffer>(indexBuffer);

        return m_IndexBuffer == nullptr;
    }

    Memory::Shared<IndexBuffer> VulkanVertexArrayBuffer::GetIndexBuffer() {
        if (m_Context == nullptr || m_Vksil == nullptr) return nullptr;
        return m_IndexBuffer;
    }

    uint32 VulkanVertexArrayBuffer::GetIndexLeng()
    {
        if (m_Context == nullptr) return 0;
        return m_IndexBuffer->Leng();
    }

    uint32 VulkanVertexArrayBuffer::Leng()
    {
        if (m_Context == nullptr || m_VertexBuffer == nullptr) return m_VertexNumber;
        return m_VertexBuffer->Leng() == 0 ? m_VertexNumber : m_VertexBuffer->Leng();
    }

    bool VulkanVertexArrayBuffer::Bind()
    {
        if (m_Context == nullptr) return false;

        if (m_VertexBuffer == nullptr || m_VertexBuffer->GetBuffer() == nullptr || m_VertexBuffer->GetBuffer()->buffer == nullptr) {
            if (Leng() == 0) {
                binding = false;
                return false;
            }
            binding = true;
            return true;
        }

        if (m_VertexBuffer->binding || binding) return false;

        vk::CommandBuffer commandBuffer = m_Context->GetCommandBuffer();

        vk::Buffer vertexBuffers[] = { m_VertexBuffer->GetBuffer()->buffer };
        vk::DeviceSize offsets[] = { 0 };
        vkCheckErrorVoid(commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot bind vertex buffer");
            binding = false;
            m_VertexBuffer->binding = false;
        }
        else {
            binding = true;
            m_VertexBuffer->binding = true;
        }
        return true;
    }

    bool VulkanVertexArrayBuffer::Unbind()
    {
        if (m_Context == nullptr || (!binding && !m_VertexBuffer->binding)) return false;
        binding = false;
        m_VertexBuffer->binding = false;
        return true;
    }

    bool VulkanVertexArrayBuffer::Draw(RenderPrimitive::Enum primitive)
    {
        if (m_VertexBuffer != nullptr && m_VertexBuffer->binding && m_VertexBuffer->Leng() != 0) {
            return Draw(primitive, 0, m_VertexBuffer->Leng());
        }

        if (binding && Leng() != 0) {
            return Draw(primitive, 0, Leng());
        }

        return false;
    }

    bool VulkanVertexArrayBuffer::Draw(RenderPrimitive::Enum primitive, uint32 firstVertex, uint32 vertexCount)
    {
        if (m_Context == nullptr || vertexCount == 0 || !binding) {
            return false;
        }
        vk::CommandBuffer commandBuffer = m_Context->GetCommandBuffer();

        uint32 first = Leng() < firstVertex ? 0 : firstVertex;
        uint32 count = first + vertexCount > Leng() ? Leng() : vertexCount;

        vkCheckErrorVoid(commandBuffer.setPrimitiveTopology(VulkanConvert::GetPrimitiveType(primitive)));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set primitive topology");
        }
        vkCheckErrorVoid(commandBuffer.draw(Leng(), 1, first, 0));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot draw vertex buffer");
        }

        return true;
    }

    bool VulkanVertexArrayBuffer::BindIndex()
    {
        if (m_Context == nullptr || !binding) return false;

        if (m_IndexBuffer == nullptr || m_IndexBuffer->binding || m_IndexBuffer->GetBuffer() == nullptr ||
            m_IndexBuffer->GetBuffer()->buffer == nullptr) {
            return false;
        }

        vk::CommandBuffer commandBuffer = m_Context->GetCommandBuffer();

        vkCheckErrorVoid(commandBuffer.bindIndexBuffer(m_IndexBuffer->GetBuffer()->buffer, 0, vk::IndexType::eUint32));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot bind index buffer");
            m_IndexBuffer->binding = false;
        } else{
            m_IndexBuffer->binding = true;
        }

        return true;
    }

    bool VulkanVertexArrayBuffer::UnbindIndex()
    {
        if (m_Context == nullptr || !binding || m_IndexBuffer == nullptr || !m_IndexBuffer->binding) return false;
        m_IndexBuffer->binding = false;
        return true;
    }

    bool VulkanVertexArrayBuffer::DrawIndex(RenderPrimitive::Enum primitive)
    {
        if (m_IndexBuffer == nullptr) {
            return false;
        }
        return DrawIndex(primitive, 0, m_IndexBuffer->Leng());
    }

    bool VulkanVertexArrayBuffer::DrawIndex(RenderPrimitive::Enum primitive, uint32 firstIndex, uint32 indexCount)
    {
        if (m_Context == nullptr || !binding || m_IndexBuffer == nullptr || !m_IndexBuffer->binding) {
            return false;
        }
        vk::CommandBuffer commandBuffer = m_Context->GetCommandBuffer();

        uint32 first = m_IndexBuffer->Leng() < firstIndex ? 0 : firstIndex;
        uint32 count = first + indexCount > m_IndexBuffer->Leng() ? m_IndexBuffer->Leng() : indexCount;

        vkCheckErrorVoid(commandBuffer.setPrimitiveTopology(VulkanConvert::GetPrimitiveType(primitive))); 
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot set primitive topology");
        }
        vkCheckErrorVoid(commandBuffer.drawIndexed(count, 1, first, 0, 0));
        if (!VulkanStaticDebugInfo::Result()) {
            gLog.Error("cannot draw index buffer");
            return false;
        }
        return true;
    }

}  //  nkentseu