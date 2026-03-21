//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-15 at 04:56:08 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_INTERNAL_SHADER_H__
#define __OPENGL_INTERNAL_SHADER_H__

#pragma once

#include <NTSCore/System.h>
#include <unordered_map>

#include <NTSGraphics/Core/Shader.h>
#include <NTSGraphics/Core/Context.h>

#include "OpenglContext.h"
#include "OpenGLUtils.h"

namespace nkentseu {
    class OpenglShaderInputLayout;

    class NKENTSEU_API OpenglShader : public Shader {
        public:
            OpenglShader(Memory::Shared<Context> context);
            ~OpenglShader();

            Memory::Shared<Context> GetContext() override;
            bool Destroy() override;

            bool LoadFromFile(const ShaderFilePathLayout& shaderFiles, Memory::Shared<ShaderInputLayout> shaderInputLayout) override;

            bool Compile();

            bool Bind() override;
            bool Unbind() override;

            uint32 GetProgramme() const {
                return m_Programme;
            }
            uint32 GetProgramme() {
                return m_Programme;
            }

            void GetUniformBufferInfos();
        private:
            uint32 m_Programme = 0;
            Memory::Shared<OpenglContext> m_Context = nullptr;
            Memory::Shared<OpenglShaderInputLayout> oglSIL = nullptr;

            NkUnorderedMap<uint32, uint32> m_Modules;

        private:
            uint32 MakeModule(const std::string& filepath, ShaderStage code);
            uint32 MakeShader();

            uint32 MakeShader(Memory::Shared<OpenglShaderInputLayout> oglsil);
            bool Compile(Memory::Shared<OpenglShaderInputLayout> oglsil);

            std::string LoadShader(const std::string& shaderFile);

            bool m_UseDsa = false;
    };

    //NkUnorderedMap<std::string, OpenglBuffer> m_UniformBuffers;
}  //  nkentseu

#endif  // __INTERNAL_SHADER_H__!