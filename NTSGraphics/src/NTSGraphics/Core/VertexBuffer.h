//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-19 at 10:46:43 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VERTEX_BUFFER_H__
#define __VERTEX_BUFFER_H__

#pragma once

#include <NTSCore/System.h>
#include "Context.h"
#include "ShaderInputLayout.h"
#include "GraphicsEnum.h"

namespace nkentseu {
    
    class NKENTSEU_API VertexBuffer {
        public:
            virtual Memory::Shared<Context> GetContext() = 0;
            virtual bool Destroy() = 0;
            virtual uint32 Leng() const = 0;
            virtual bool SetData(const void* data, usize size) = 0;

            virtual bool Create(BufferUsageType bufferUsage, const std::vector<float32>& vertices) = 0;
            virtual bool Create(BufferUsageType bufferUsage, const void* vertices, uint32 leng) = 0;

            static Memory::Shared<VertexBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);

            static Memory::Shared<VertexBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, BufferUsageType bufferUsage, const std::vector<float32>& vertices);

            static Memory::Shared<VertexBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, BufferUsageType bufferUsage, const void* vertices, uint32 leng);

            template <typename T>
            static Memory::Shared<VertexBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, BufferUsageType bufferUsage, const std::vector<T>& vertices) {
                return Create(context, sil, bufferUsage, vertices.data(), vertices.size());
            }

        protected:
            friend class VulkanVertexArrayBuffer;
            friend class OpenglVertexArrayBuffer;

            bool binding = false;
    };

}  //  nkentseu

#endif  // __VERTEX_BUFFER_H__!