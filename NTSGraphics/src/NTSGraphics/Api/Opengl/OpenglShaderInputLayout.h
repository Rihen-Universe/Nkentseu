//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-29 at 10:13:50 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_SHADER_INPUT_LAYOUT_H__
#define __OPENGL_SHADER_INPUT_LAYOUT_H__

#pragma once

#include <NTSCore/System.h>

#include "NTSGraphics/Core/ShaderInputLayout.h"
#include "NTSGraphics/Core/Context.h"
#include "OpenGLUtils.h"
#include "OpenglContext.h"
#include "OpenglShader.h"

namespace nkentseu {
    
    class NKENTSEU_API OpenglShaderInputLayout : public ShaderInputLayout {
    public:
        OpenglShaderInputLayout(Memory::Shared<Context> context);
        ~OpenglShaderInputLayout();


        virtual bool Initialize() override;
        virtual bool Release() override;

        virtual bool UpdatePushConstant(const std::string& name, void* data, usize size, Memory::Shared<Shader> shader = nullptr) override;

    private:
        friend class OpenglShader;

        void ResetConstantPush(OpenglShader* shader);

        Memory::Shared<OpenglContext> m_Context;

        NkUnorderedMap<std::string, OpenglBuffer> m_PushConstants;
    };

}  //  nkentseu

#endif  // __OPENGL_SHADER_INPUT_LAYOUT_H__!