//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-20 at 09:21:33 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_INTERNAL_INDEX_BUFFER_H__
#define __VULKAN_INTERNAL_INDEX_BUFFER_H__

#pragma once

#include <NTSCore/System.h>

#include "NTSGraphics/Core/IndexBuffer.h"
#include "VulkanContext.h"

namespace nkentseu {
    class VulkanContext;

    class NKENTSEU_API VulkanIndexBuffer : public IndexBuffer {
    public:
        VulkanIndexBuffer(Memory::Shared<Context> context);
        ~VulkanIndexBuffer();

        Memory::Shared<Context> GetContext() override;

        bool Create(BufferUsageType bufferUsage, const std::vector<uint32>& indices) override;
        bool Create(BufferUsageType bufferUsage, IndexBufferType indexType, const void* vertices, uint32 leng) override;
        bool Destroy() override;

        virtual bool SetData(const void* data, usize size) override;

        VkBufferInternal* GetBuffer();
        IndexBufferType GetIndexType() const;
        uint32 Leng() const override;
    private:
        Memory::Shared<VulkanContext> m_Context = nullptr;
        VkBufferInternal m_IndexBufferObject;
        IndexBufferType m_IndexType = IndexBufferType::Enum::UInt32;

        uint32 m_Leng = 0;
        uint32 dynamicAlignment = 0;
        uint32 range = 0;

        bool StaticBufferCreation(const void* data);
        bool DynamicBufferCreation();
    };

}  //  nkentseu

#endif  // __INTERNAL_INDEX_BUFFER_H__!