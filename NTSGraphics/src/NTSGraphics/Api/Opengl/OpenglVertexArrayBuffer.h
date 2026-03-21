//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-16 at 10:11:05 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_INTERNAL_VERTEX_ARRAY_H__
#define __OPENGL_INTERNAL_VERTEX_ARRAY_H__

#pragma once

#include <NTSCore/System.h>
#include <glad/gl.h>

#include <NTSGraphics/Core/VertexArrayBuffer.h>
#include <NTSGraphics/Core/ShaderInputLayout.h>

#include "OpenglShaderInputLayout.h"

namespace nkentseu {
    class Context;
    class OpenglVertexBuffer;
    class OpenglIndexBuffer;
    
    class NKENTSEU_API OpenglVertexArrayBuffer : public VertexArrayBuffer {
        public:
            OpenglVertexArrayBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
            ~OpenglVertexArrayBuffer();

            Memory::Shared<Context> GetContext() override;

            bool Create() override;
            bool Create(uint32 vertexNumber) override;

            bool Destroy() override;

            bool SetVertexBuffer(Memory::Shared<VertexBuffer> vertexBuffer) override;
            Memory::Shared<VertexBuffer> GetVertexBuffer() override;
            uint32 GetVertexLeng() override;

            bool SetIndexBuffer(Memory::Shared<IndexBuffer> indexBuffer) override;
            Memory::Shared<IndexBuffer> GetIndexBuffer() override;
            uint32 GetIndexLeng() override;

            uint32 Leng() override;

            virtual bool Draw(RenderPrimitive::Enum primitive) override;
            virtual bool Draw(RenderPrimitive::Enum primitive, uint32 firstVertex, uint32 vertexCount) override;
            virtual bool Bind() override;
            virtual bool Unbind() override;

            virtual bool BindIndex() override;
            virtual bool UnbindIndex() override;
            virtual bool DrawIndex(RenderPrimitive::Enum primitive) override;
            virtual bool DrawIndex(RenderPrimitive::Enum primitive, uint32 firstIndex, uint32 indexCount) override;

            bool BindVA();
            bool UnbindVA();

            GLuint GetBuffer();
            //uint32 
        private:
            GLuint m_VertexArrayObject = 0; // VAO
            Memory::Shared<OpenglVertexBuffer> m_VertexBuffer;
            Memory::Shared<OpenglIndexBuffer> m_IndexBuffer;
            //std::vector<Memory::Shared<OpenglVertexBuffer>> m_VertexBuffers;
            uint32 m_VertexNumber = 0;

            Memory::Shared<Context> m_Context;
            Memory::Shared<OpenglShaderInputLayout> m_OglSil;
            bool m_UseDsa = false;
    private:
        bool ActualizeVertexBuffer();
    };

}  //  nkentseu

#endif  // __INTERNAL_VERTEX_ARRAY_H__!