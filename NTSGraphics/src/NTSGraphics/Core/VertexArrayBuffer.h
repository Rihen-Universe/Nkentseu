//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-16 at 10:10:05 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VERTEX_ARRAY_BUFFER_H__
#define __VERTEX_ARRAY_BUFFER_H__

#pragma once

#include <NTSCore/System.h>

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Context.h"
#include "ShaderInputLayout.h"

namespace nkentseu {
    class NKENTSEU_API VertexArrayBuffer {
        public:
            virtual bool Create() = 0;
            virtual bool Create(uint32 vertexNumber) = 0;

            virtual Memory::Shared<Context> GetContext() = 0;
            virtual bool SetVertexBuffer(Memory::Shared<VertexBuffer> vertexBuffer) = 0;
            virtual Memory::Shared<VertexBuffer> GetVertexBuffer() = 0;
            virtual uint32 GetVertexLeng() = 0;
            virtual bool SetIndexBuffer(Memory::Shared<IndexBuffer> indexBuffer) = 0;
            virtual Memory::Shared<IndexBuffer> GetIndexBuffer() = 0;
            virtual uint32 GetIndexLeng() = 0;
            virtual uint32 Leng() = 0;
            virtual bool Destroy() = 0;

            virtual bool Bind() = 0;
            virtual bool Unbind() = 0;
            virtual bool BindIndex() = 0;
            virtual bool UnbindIndex() = 0;

            virtual bool Draw(RenderPrimitive::Enum primitive) = 0;
            virtual bool Draw(RenderPrimitive::Enum primitive, uint32 firstVertex, uint32 vertexCount) = 0;

            virtual bool DrawIndex(RenderPrimitive::Enum primitive) = 0;
            virtual bool DrawIndex(RenderPrimitive::Enum primitive, uint32 firstIndex, uint32 indexCount) = 0;

            static Memory::Shared<VertexArrayBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
            static Memory::Shared<VertexArrayBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, uint32 vertexNumber);

        protected:
            static Memory::Shared<VertexArrayBuffer> CreatePrivate(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);

            enum BindInfo {
                NO_BIND,
                INDEX_BIND,
                VERTEX_BIND
            };

            bool binding = false;
    };

}  //  nkentseu

#endif  // __VERTEX_ARRAY_BUFFER_H__!