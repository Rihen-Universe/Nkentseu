//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-03 at 08:33:39 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_UNIFORM_BUFFER_H__
#define __OPENGL_UNIFORM_BUFFER_H__

#pragma once

#include <NTSCore/System.h>

#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/Shader.h"
#include "NTSGraphics/Core/UniformBuffer.h"
#include "OpenglContext.h"
#include "OpenglShader.h"
#include "OpenGLUtils.h"
#include "OpenglShaderInputLayout.h"

namespace nkentseu {
    
    class NKENTSEU_API OpenglUniformBuffer : public UniformBuffer {
        public:
            OpenglUniformBuffer(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
            ~OpenglUniformBuffer();

            bool Create(Memory::Shared<Shader> shader) override;
            bool Destroy() override;

            bool SetData(const std::string& name, void* data, usize size) override;
            bool SetData(const std::string& name, void* data, usize size, uint32 index) override;
            bool Bind(const std::string& name) override;

            Memory::Shared<Context> GetContext() override;
        private:
            Memory::Shared<OpenglContext> m_Context;
            Memory::Shared<OpenglShader> m_Shader;
            Memory::Shared<OpenglShaderInputLayout> m_Oglsil = nullptr;
            //UniformBufferLayout m_BufferLayout;

            NkUnorderedMap<std::string, OpenglBuffer> m_Buffers;
    };

}  //  nkentseu

#endif  // __OPENGL_UNIFORM_BUFFER_H__!