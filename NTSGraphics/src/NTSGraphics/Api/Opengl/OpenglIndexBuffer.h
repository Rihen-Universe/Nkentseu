//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:47:38 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_INTERNAL_INDEX_BUFFER_H__
#define __OPENGL_INTERNAL_INDEX_BUFFER_H__

#pragma once

#include <NTSCore/System.h>

#include "NTSGraphics/Core/IndexBuffer.h"
#include "OpenglContext.h"
#include "OpenGLUtils.h"

namespace nkentseu {
    class Context;

    class NKENTSEU_API OpenglIndexBuffer : public IndexBuffer {
    public:
        OpenglIndexBuffer(Memory::Shared<Context> context);
        ~OpenglIndexBuffer();

        Memory::Shared<Context> GetContext() override;
        bool Destroy() override;
        uint32 Leng() const override;

        bool Create(BufferUsageType bufferUsage, const std::vector<uint32>& indices) override;
        bool Create(BufferUsageType bufferUsage, IndexBufferType indexType, const void* vertices, uint32 leng) override;

        bool Bind() const;
        bool Unbind() const;

        virtual bool SetData(const void* data, usize size) override;

        IndexBufferType GetIndexType() const;
        uint32 GetBuffer() const;
    private:
        Memory::Shared<OpenglContext> m_Context = nullptr;
        BufferUsageType m_BufferUsage = BufferUsageType::Enum::StaticDraw;
        IndexBufferType m_IndexType = IndexBufferType::Enum::UInt32;

        uint32 m_Size = 0;
        uint32 m_Leng = 0;
        OpenglBuffer m_Buffer;
    };

}  //  nkentseu

#endif  // __INTERNAL_INDEX_BUFFER_H__!