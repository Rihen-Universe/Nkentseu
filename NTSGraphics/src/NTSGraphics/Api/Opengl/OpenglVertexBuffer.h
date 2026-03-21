//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:47:51 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_INTERNAL_VERTEX_BUFFER_H__
#define __OPENGL_INTERNAL_VERTEX_BUFFER_H__

#pragma once

#include <NTSCore/System.h>
#include "NTSGraphics/Core/VertexBuffer.h"
#include "NTSGraphics/Core/ShaderInputLayout.h"

#include "OpenGLUtils.h"
#include "OpenglShaderInputLayout.h"

namespace nkentseu {
    class Context;

    class NKENTSEU_API OpenglVertexBuffer : public VertexBuffer {
    public:
        OpenglVertexBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
        ~OpenglVertexBuffer();

        Memory::Shared<Context> GetContext() override;
        bool Destroy() override;
        uint32 Leng() const override;

        bool Create(BufferUsageType bufferUsage, const std::vector<float32>& vertices) override;
        bool Create(BufferUsageType bufferUsage, const void* vertices, uint32 leng) override;

        template <typename T>
        bool Create(BufferUsageType bufferUsage, const std::vector<T>& vertices) {
            return Create(bufferUsage, vertices.data(), vertices.size());
        }

        bool Bind() const;
        bool Unbind() const;

        virtual bool SetData(const void* data, usize size) override;

        uint32 GetBuffer() const;
        bool AttachToVAO(uint32 vao, bool useDsa);
    private:
        Memory::Shared<OpenglShaderInputLayout> m_OglSil;
        BufferUsageType m_BufferUsage;
        Memory::Shared<Context> m_Context;

        uint32 m_Size = 0;
        uint32 m_Leng = 0;
        OpenglBuffer m_Buffer;
    };

}  //  nkentseu

#endif  // __INTERNAL_VERTEX_BUFFER_H__!