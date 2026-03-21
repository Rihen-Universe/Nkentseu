//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-19 at 10:47:51 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenglVertexBuffer.h"

#include <glad/gl.h>

#include "NTSGraphics/Core/Context.h"
#include "OpenglContext.h"
#include "OpenGLUtils.h"

namespace nkentseu {
    GLfloat vertices_[] =
    {
        // Positions         // Colors
        0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  // Bottom Right
        -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,  // Bottom Left
        0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f   // Top
    };

    OpenglVertexBuffer::OpenglVertexBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<OpenglContext>(context)) {
        m_OglSil = Memory::Cast<OpenglShaderInputLayout>(sil);
    }

    OpenglVertexBuffer::~OpenglVertexBuffer() {
    }

    bool OpenglVertexBuffer::Create(BufferUsageType bufferUsage, const std::vector<float32>& vertices)
    {
        if (m_OglSil == nullptr) return false;
        return Create(bufferUsage, vertices.data(), (vertices.size() / m_OglSil->vertexInput.componentCount));
    }

    bool OpenglVertexBuffer::Create(BufferUsageType bufferUsage, const void* vertices, uint32 leng)
    {
        if (m_Buffer.buffer != 0 || m_Context == nullptr || m_OglSil == nullptr) {
            return false;
        }

        m_BufferUsage = bufferUsage;
        m_Leng = leng;

        usize size = m_Leng * m_OglSil->vertexInput.stride;
        m_Size = size;

        //gLog.Debug("{0}-{1}", m_Size, m_BufferLayout.stride);

        return m_Buffer.Create("vertex buffer", GL_ARRAY_BUFFER, GLConvert::UsageType(m_BufferUsage), vertices, size, 0, 0, 1);
    }

    Memory::Shared<Context> OpenglVertexBuffer::GetContext()
    {
        return m_Context;
    }

    bool OpenglVertexBuffer::Destroy()
    {
        return m_Buffer.Destroy();
    }

    bool OpenglVertexBuffer::Bind() const
    {
        return m_Buffer.Bind();
    }

    bool OpenglVertexBuffer::Unbind() const
    {
        return m_Buffer.Unbind();
    }

    bool OpenglVertexBuffer::SetData(const void* data, usize size)
    {
        return m_Buffer.WriteToBuffer(data, size);
    }

    uint32 OpenglVertexBuffer::Leng() const
    {
        return m_Leng;
    }

    uint32 OpenglVertexBuffer::GetBuffer() const
    {
        return m_Buffer.buffer;
    }

    bool OpenglVertexBuffer::AttachToVAO(uint32 vao, bool useDsa)
    {
        if (m_Buffer.buffer == 0 || m_OglSil == nullptr) {
            return false;
        }
        OpenGLResult result;
        bool first = true;

        if (!Bind()) {
            return false;
        }

        if (m_Buffer.useDAS) {
            glCheckError(first, result, glVertexArrayVertexBuffer(vao, 0, m_Buffer.buffer, 0, m_OglSil->vertexInput.stride), "cannot enable vertex array vertex buffer");
        }

        uint32 stride = m_OglSil->vertexInput.stride;

        for (auto& attribut : m_OglSil->vertexInput) {
            uint32 type = GLConvert::ShaderType(attribut.type);
            uint32 normalized = attribut.normalized ? GL_TRUE : GL_FALSE;
            uint32 count = ShaderInternalType::ComponentCount(attribut.type);
            uint32 offset = attribut.offset;
            uint32 location = attribut.location;

            if (useDsa) {
                glCheckError(first, result, glEnableVertexArrayAttrib(vao, location), "cannot enable vertex atribut array");
            }
            else {
                glCheckError(first, result, glEnableVertexAttribArray(location), "cannot enable vertex atribut array");
            }

            if (!result.success) {
                gLog.Debug();
                return false;
            }

            if (attribut.type == ShaderInternalType::Enum::Float || attribut.type == ShaderInternalType::Enum::Float2 || attribut.type == ShaderInternalType::Enum::Float3 || attribut.type == ShaderInternalType::Enum::Float4) {
                if (useDsa) {
                    glCheckError(first, result, glVertexArrayAttribFormat(vao, location, count, type, normalized, offset), "cannot set vertex attribut pointer");
                }
                else {
                    //gLog.Debug("name = {0}, binding = {1}", attribut.name, attribut.location);
                    glCheckError(first, result, glVertexAttribPointer(location, count, type, normalized, stride, (const void*)offset), "cannot set vertex attribut pointer");
                }
            }
            else if (attribut.type == ShaderInternalType::Enum::Mat3 || attribut.type == ShaderInternalType::Enum::Mat4) {
                for (uint8 i = 0; i < count; i++) {

                    offset = offset + sizeof(float32) * count * i;

                    if (useDsa) {
                        glCheckError(first, result, glVertexArrayAttribFormat(vao, location, count, type, normalized, offset), "cannot set vertex attribut pointer");
                    }
                    else {
                        glCheckError(first, result, glVertexAttribPointer(location, count, type, normalized, stride, (const void*)(offset)), "cannot set vertex attribut pointer");
                        glCheckError(first, result, glVertexAttribDivisor(location, 1), "cannot set vertex attribut pointer");
                    }
                }
            }
            else if (attribut.type == ShaderInternalType::Enum::NotDefine) {
                continue;
            }
            else {
                if (useDsa) {
                    glCheckError(first, result, glVertexArrayAttribFormat(vao, location, count, type, normalized, offset), "cannot set vertex attribut pointer");
                }
                else {
                    glCheckError(first, result, glVertexAttribIPointer(location, count, type, stride, (const void*)offset), "cannot set vertex attribut pointer");
                }
            }

            if (!result.success) {
                gLog.Debug();
                return false;
            }
        }
        return Unbind();
    }

}  //  nkentseu