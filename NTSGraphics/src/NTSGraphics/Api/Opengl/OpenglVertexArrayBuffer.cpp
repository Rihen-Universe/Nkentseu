//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-16 at 10:11:05 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenglVertexArrayBuffer.h"

#include "NTSGraphics/Core/VertexBuffer.h"
#include "NTSGraphics/Core/IndexBuffer.h"
#include "OpenglVertexBuffer.h"
#include "OpenglIndexBuffer.h"

#include "NTSGraphics/Core/Context.h"
#include "OpenglContext.h"

#include "OpenGLUtils.h"

namespace nkentseu {

    OpenglVertexArrayBuffer::OpenglVertexArrayBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<OpenglContext>(context)) {
        m_OglSil = Memory::Cast<OpenglShaderInputLayout>(sil);
    }

    OpenglVertexArrayBuffer::~OpenglVertexArrayBuffer() {
    }

    Memory::Shared<Context> nkentseu::OpenglVertexArrayBuffer::GetContext()
    {
        return m_Context;
    }

    bool OpenglVertexArrayBuffer::Create()
    {
        if (m_VertexArrayObject != 0 || m_Context == nullptr) {
            return false;
        }

        OpenGLResult result;
        bool first = true;

        if (m_UseDsa) {
            glCheckError(first, result, glCreateVertexArrays(1, &m_VertexArrayObject), "cannot gen vertex array");
        }
        else {
            glCheckError(first, result, glGenVertexArrays(1, &m_VertexArrayObject), "cannot gen vertex array");
        }

        return true;
    }

    bool OpenglVertexArrayBuffer::Create(uint32 vertexNumber)
    {
        if (m_VertexArrayObject != 0 || m_Context == nullptr) return false;

        OpenGLResult result;
        bool first = true;

        if (m_UseDsa) {
            glCheckError(first, result, glCreateVertexArrays(1, &m_VertexArrayObject), "cannot gen vertex array");
        }
        else {
            glCheckError(first, result, glGenVertexArrays(1, &m_VertexArrayObject), "cannot gen vertex array");
        }

        if (!result.success || m_VertexArrayObject == 0) {
            return false;
        }

        if (!Bind()) {
            return false;
        }

        m_VertexNumber = vertexNumber;
        return Unbind();
    }

    uint32 OpenglVertexArrayBuffer::GetVertexLeng() {
        if (m_VertexBuffer == nullptr) return 0;
        return m_VertexBuffer->Leng();
    }

    bool OpenglVertexArrayBuffer::SetVertexBuffer(Memory::Shared<VertexBuffer> vertexBuffer)
    {
        if (m_OglSil == nullptr) return false;
        m_VertexBuffer = Memory::Cast<OpenglVertexBuffer>(vertexBuffer);
        return ActualizeVertexBuffer();
    }

    Memory::Shared<VertexBuffer> OpenglVertexArrayBuffer::GetVertexBuffer()
    {
        return m_VertexBuffer;
    }

    bool OpenglVertexArrayBuffer::SetIndexBuffer(Memory::Shared<IndexBuffer> indexBuffer)
    {
        if (m_OglSil == nullptr) return false;

        m_IndexBuffer = Memory::Cast<OpenglIndexBuffer>(indexBuffer);

        if (m_UseDsa && m_IndexBuffer != nullptr) {
            glVertexArrayElementBuffer(m_VertexArrayObject, m_IndexBuffer->GetBuffer());
        }
        return m_IndexBuffer == nullptr;
    }

    Memory::Shared<IndexBuffer> OpenglVertexArrayBuffer::GetIndexBuffer()
    {
        return m_IndexBuffer;
    }

    uint32 OpenglVertexArrayBuffer::GetIndexLeng()
    {
        if (m_IndexBuffer == nullptr) return 0;
        return m_IndexBuffer->Leng();
    }

    uint32 OpenglVertexArrayBuffer::Leng()
    {
        if (m_Context == nullptr || m_VertexBuffer == nullptr) return m_VertexNumber;
        return m_VertexBuffer->Leng() == 0 ? m_VertexNumber : m_VertexBuffer->Leng();
    }

    bool OpenglVertexArrayBuffer::Destroy()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0) return false;

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glDeleteVertexArrays(1, &m_VertexArrayObject), "cannot delete vertex array");

        if (!result.success) {
            return false;
        }

        m_VertexArrayObject = 0;
        return true;
    }

    bool OpenglVertexArrayBuffer::Bind()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0) {
            return false;
        }

        if (binding && m_VertexBuffer == nullptr && m_VertexBuffer->binding) return false;

        if (!binding && !BindVA()) {
            binding = false;
            if (m_VertexBuffer == nullptr) m_VertexBuffer->binding = false;
            return false;
        }
        binding = true;

        if (m_VertexBuffer == nullptr || m_VertexBuffer->binding) return false;
        if (!m_VertexBuffer->Bind()) {
            m_VertexBuffer->binding = false;
            return false;
        }
        m_VertexBuffer->binding = true;
        return true;
    }

    bool OpenglVertexArrayBuffer::Unbind()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0) {
            return false;
        }

        if (!binding && m_VertexBuffer != nullptr && !m_VertexBuffer->binding) return false;

        if (m_VertexBuffer != nullptr && m_VertexBuffer->binding){
            m_VertexBuffer->Unbind();
            m_VertexBuffer->binding = false;
        }

        if (binding) {
            UnbindVA();
            binding = false;
            return true;
        }
        return false;
    }

    bool OpenglVertexArrayBuffer::Draw(RenderPrimitive::Enum primitive)
    {
        if (m_VertexBuffer != nullptr && m_VertexBuffer->Leng() != 0 && m_VertexBuffer->binding) {
            return Draw(primitive, 0, m_VertexBuffer->Leng());
        }

        if (Leng() != 0 && binding) {
            return Draw(primitive, 0, Leng());
        }

        return false;
    }

    bool OpenglVertexArrayBuffer::Draw(RenderPrimitive::Enum primitive, uint32 firstVertex, uint32 vertexCount)
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0 || vertexCount == 0 || !binding) return false;
        uint32 firstElement = Leng() < firstVertex ? 0 : firstVertex;
        uint32 count = firstElement + vertexCount > Leng() ? Leng() : vertexCount;

        OpenGLResult result;
        bool first = true;
        glCheckError(first, result, glDrawArrays(GLConvert::GetPrimitiveType(primitive), firstElement, count), "cannot draw arrays");
        return result.success;
    }

    bool OpenglVertexArrayBuffer::BindIndex()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0 || !binding || m_IndexBuffer == nullptr) {
            return false;
        }

        if (m_IndexBuffer->Bind()) {
            m_IndexBuffer->binding = true;
            return true;
        }
        m_IndexBuffer->binding = false;
        return false;
    }

    bool OpenglVertexArrayBuffer::UnbindIndex()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0 || m_IndexBuffer == nullptr || !m_IndexBuffer->binding) return false;
        if (!m_IndexBuffer->Unbind()) {
            m_IndexBuffer->binding = false;
            return false;
        }
        m_IndexBuffer->binding = true;
        return true;
    }

    bool OpenglVertexArrayBuffer::DrawIndex(RenderPrimitive::Enum primitive)
    {
        if (m_IndexBuffer == nullptr) {
            return false;
        }
        return DrawIndex(primitive, 0, m_IndexBuffer->Leng());
    }

    bool OpenglVertexArrayBuffer::DrawIndex(RenderPrimitive::Enum primitive, uint32 firstIndex, uint32 indexCount)
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0 || m_IndexBuffer == nullptr || !m_IndexBuffer->binding) return false;

        OpenGLResult result;
        bool first = true;

        uint32 firstElement = m_IndexBuffer->Leng() < firstIndex ? 0 : firstIndex;
        uint32 count = firstElement + indexCount > m_IndexBuffer->Leng() ? m_IndexBuffer->Leng() : indexCount;

        uint32 indexType = GLConvert::IndexType(m_IndexBuffer->GetIndexType());
        uint32 offset = (firstElement) * sizeof(uint32);
        uint32 primitiveType = GLConvert::GetPrimitiveType(primitive);
        
        //glCheckError(first, result, glDrawElements(primitiveType, count, indexType, 0), "cannot draw elements");
        glCheckError(first, result, glDrawElements(primitiveType, count, indexType, (void*)(offset)), "cannot draw elements");

        return result.success;
    }

    bool OpenglVertexArrayBuffer::BindVA()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0) return false;

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glBindVertexArray(m_VertexArrayObject), "cannot bin vertes array");
        return result.success;
    }

    bool OpenglVertexArrayBuffer::UnbindVA()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0) return false;

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glBindVertexArray(0), "cannot unbin vertex array");
        return result.success;
    }

    GLuint OpenglVertexArrayBuffer::GetBuffer()
    {
        return m_VertexArrayObject;
    }

    bool OpenglVertexArrayBuffer::ActualizeVertexBuffer()
    {
        if (m_Context == nullptr || m_VertexArrayObject == 0) {
            return false;
        }

        if (!Bind()) {
            return false;
        }

        bool success = true;

        if (m_VertexBuffer != nullptr) {
            success = m_VertexBuffer->AttachToVAO(m_VertexArrayObject, m_UseDsa);
        }

        return Unbind() && success;
    }
}  //  nkentseu