//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-05-15 at 04:53:53 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __SHADER_H__
#define __SHADER_H__

#pragma once

#include <NTSCore/System.h>
#include <unordered_map>  // Inclusion pour NkUnorderedMap

#include "Context.h"
#include "ShaderInputLayout.h"  // Inclusion de ShaderInputLayout pour accéder à ShaderStage


namespace nkentseu {

    class ShaderInputLayout;
    //class ShaderStage;
    class ShaderFilePathLayout;

    class NKENTSEU_API Shader {
        public:
            virtual Memory::Shared<Context> GetContext() = 0;
            virtual bool Destroy() = 0;
            virtual bool Bind() = 0;
            virtual bool Unbind() = 0;

            virtual bool LoadFromFile(const ShaderFilePathLayout& shaderFiles, Memory::Shared<ShaderInputLayout> shaderInputLayout) = 0;

            static Memory::Shared<Shader> Create(Memory::Shared<Context> context);
            static Memory::Shared<Shader> Create(Memory::Shared<Context> context, const ShaderFilePathLayout& shaderFiles, Memory::Shared<ShaderInputLayout> shaderInputLayout);
    };

}  //  nkentseu

#endif  // __SHADER_H__!