//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-29 at 10:13:50 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenglShaderInputLayout.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
    
    OpenglShaderInputLayout::OpenglShaderInputLayout(Memory::Shared<Context> context) : m_Context(Memory::Cast<OpenglContext>(context)) {
    }

    OpenglShaderInputLayout::~OpenglShaderInputLayout() {
    }

    bool OpenglShaderInputLayout::Initialize() {
        if (m_Context == nullptr || !ShaderInputLayout::Initialize()) {
            return false;
        }

        usize maxBindingPoint = uniformInput.maxBindingPoint + 1;

        // Initialisation des buffers uniform pour les push constants
        for (const auto& attribute : pushConstantInput.attributes) {
            m_PushConstants[attribute.name] = {};
            if (!m_PushConstants[attribute.name].Create(attribute.name, GL_UNIFORM_BUFFER, GL_STATIC_DRAW, nullptr, attribute.size, maxBindingPoint, 0, 1, false)) {
                return false;
            }
            m_PushConstants[attribute.name].uType = BufferUsageType::Enum::StaticDraw;
            maxBindingPoint++;
        }

        return true;
    }

    bool OpenglShaderInputLayout::Release() {
        // Release OpenGL resources

        return ShaderInputLayout::Release();
    }

    bool OpenglShaderInputLayout::UpdatePushConstant(const std::string& name, void* data, usize size, Memory::Shared<Shader> shader) {
        if (!m_Context) {
            return false;
        }

        auto it = m_PushConstants.find(name);
        if (it == m_PushConstants.end()) {
            return false;
        }

        if (size != it->second.size) {
            return false;
        }

        //ResetConstantPush(Memory::SharedCast<OpenglShader>(shader), it->second);

        if (!it->second.WriteToBuffer(data, size, 0)) {
            return false;
        }

        return true;
    }

    void OpenglShaderInputLayout::ResetConstantPush(OpenglShader* shader)
    {
        if (shader == nullptr) return;

        shader->Bind();

        for (auto& [name, buffer] : m_PushConstants) {
            OpenGLResult result;
            bool first = true;

            glCheckError(first, result, GLuint blockIndex = glGetUniformBlockIndex(shader->GetProgramme(), buffer.name.c_str()), "");
            if (blockIndex == GL_INVALID_INDEX) {
                gLog.Error("Error: Uniform block index is invalid : {0}-{1}", buffer.name, shader->GetProgramme());
            }
            else {
                glCheckError(first, result, glUniformBlockBinding(shader->GetProgramme(), blockIndex, buffer.binding), "");
            }
        }

        shader->Unbind();
    }

}  //  nkentseu