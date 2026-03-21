//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-20 at 09:11:16 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_INTERNAL_VERTEX_BUFFER_H__
#define __VULKAN_INTERNAL_VERTEX_BUFFER_H__

#pragma once

#include <vulkan/vulkan.hpp>
#include <NTSCore/System.h>

#include "NTSGraphics/Core/VertexBuffer.h"
#include "VulkanContext.h"

#include "VulkanShaderInputLayout.h"

namespace nkentseu {
    class VulkanContext;

    class NKENTSEU_API VulkanVertexBuffer : public VertexBuffer {
    public:
        VulkanVertexBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
        ~VulkanVertexBuffer();

        Memory::Shared<Context> GetContext() override;
        bool Destroy() override;

        bool Create(BufferUsageType bufferUsage, const std::vector<float32>& vertices) override;
        bool Create(BufferUsageType bufferUsage, const void* vertices, uint32 leng) override;

        template <typename T>
        bool Create(BufferUsageType bufferUsage, const std::vector<T>& vertices) {
            return Create(bufferUsage, vertices.data(), vertices.size());
        }

        virtual bool SetData(const void* data, usize size) override;

        VkBufferInternal* GetBuffer();

        uint32 Leng() const override;
    private:
        VkBufferInternal m_VertexBufferObject;
        Memory::Shared<VulkanContext> m_Context = nullptr;
        Memory::Shared<VulkanShaderInputLayout> m_Vksil = nullptr;
        uint32 m_Leng = 0;
        uint32 dynamicAlignment = 0;
        uint32 range = 0;

        bool StaticBufferCreation(const void* data);
        bool DynamicBufferCreation();
    };

}  //  nkentseu

#endif  // __INTERNAL_VERTEX_BUFFER_H__!