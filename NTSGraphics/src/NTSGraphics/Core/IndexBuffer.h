//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:46:54 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __INDEX_BUFFER_H__
#define __INDEX_BUFFER_H__

#pragma once

#include <NTSCore/System.h>

#include "ShaderInputLayout.h"
#include "Context.h"

namespace nkentseu {
    class InternalIndexBuffer;
    
    class NKENTSEU_API IndexBuffer {
        public:
            virtual Memory::Shared<Context> GetContext() = 0;
            virtual bool Destroy() = 0;
            virtual uint32 Leng() const = 0;
            virtual bool SetData(const void* data, usize size) = 0;

            virtual bool Create(BufferUsageType bufferUsage, const std::vector<uint32>& indices) = 0;
            virtual bool Create(BufferUsageType bufferUsage, IndexBufferType indexType, const void* vertices, uint32 leng) = 0;

            static Memory::Shared<IndexBuffer> Create(Memory::Shared<Context> context);
            static Memory::Shared<IndexBuffer> Create(Memory::Shared<Context> context, BufferUsageType bufferUsage, const std::vector<uint32>& indices);
            static Memory::Shared<IndexBuffer> Create(Memory::Shared<Context> context, BufferUsageType bufferUsage, IndexBufferType indexType, const void* indices, uint32 leng);

        protected:
            bool binding = false;

            friend class VulkanVertexArrayBuffer;
            friend class OpenglVertexArrayBuffer;
    };

}  //  nkentseu

#endif  // __INDEX_BUFFER_H__!