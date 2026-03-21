//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:47:38 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenglIndexBuffer.h"

#include <glad/gl.h>

#include "NTSGraphics/Core/Context.h"
#include "OpenglContext.h"
#include "OpenGLUtils.h"
#include "OpenglIndexBuffer.h"
#include "OpenglIndexBuffer.h"

namespace nkentseu {

    // Constructor  (m_ElementBufferObject)
    OpenglIndexBuffer::OpenglIndexBuffer(Memory::Shared<Context> context) : m_Context(Memory::Cast<OpenglContext>(context)) {
        // Ajoutez votre code de constructeur ici
    }

    // Destructor
    OpenglIndexBuffer::~OpenglIndexBuffer() {
        // Ajoutez votre code de destructeur ici
    }

    bool OpenglIndexBuffer::Create(BufferUsageType bufferUsage, const std::vector<uint32>& indices)
    {
        return Create(bufferUsage, IndexBufferType::Enum::UInt32, indices.data(), indices.size());
    }

    bool OpenglIndexBuffer::Create(BufferUsageType bufferUsage, IndexBufferType indexType, const void* indices, uint32 leng)
    {
        if (m_Buffer.buffer != 0 || m_Context == nullptr) {
            return false;
        }

        OpenGLResult result;
        bool first = true;

        if (m_Buffer.buffer != 0) {
            return false;
        }

        m_Leng = leng;
        m_IndexType = indexType;
        usize size = m_Leng * sizeof(uint32);
        m_Size = size;

        return m_Buffer.Create("index buffer", GL_ELEMENT_ARRAY_BUFFER, GLConvert::UsageType(m_BufferUsage), indices, size, 0, 0, 1);
    }

    Memory::Shared<Context> OpenglIndexBuffer::GetContext()
    {
        return m_Context;
    }

    bool OpenglIndexBuffer::Destroy()
    {
        if (m_Context == nullptr) {
            return false;
        }
        return m_Buffer.Destroy();
    }

    bool OpenglIndexBuffer::Bind() const
    {
        if (m_Context == nullptr) {
            return false;
        }
        return m_Buffer.Bind();
    }

    bool OpenglIndexBuffer::Unbind() const
    {
        if (m_Context == nullptr) {
            return false;
        }
        return m_Buffer.Unbind();
    }

    bool OpenglIndexBuffer::SetData(const void* data, usize size)
    {
        if (m_Context == nullptr) {
            return false;
        }
        return m_Buffer.WriteToBuffer(data, size);
    }

    uint32 OpenglIndexBuffer::Leng() const
    {
        return m_Leng;
    }

    IndexBufferType OpenglIndexBuffer::GetIndexType() const
    {
        return m_IndexType;
    }

    uint32 OpenglIndexBuffer::GetBuffer() const
    {
        return m_Buffer.buffer;
    }

}  //  nkentseu