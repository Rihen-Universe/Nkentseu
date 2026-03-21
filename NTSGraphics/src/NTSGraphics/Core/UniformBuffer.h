//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-03 at 08:32:23 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __UNIFORM_BUFFER_H__
#define __UNIFORM_BUFFER_H__

#pragma once

#include <NTSCore/System.h>
#include "Context.h"
#include "Shader.h"

namespace nkentseu {
    
    class NKENTSEU_API UniformBuffer {
        public:
            virtual Memory::Shared<Context> GetContext() = 0;

            virtual bool SetData(const std::string& name, void* data, usize size) = 0;
            virtual bool SetData(const std::string& name, void* data, usize size, uint32 index) = 0;
            virtual bool Bind(const std::string& name) = 0;

            virtual bool Destroy() = 0;
            virtual bool Create(Memory::Shared<Shader> shader) = 0;

            static Memory::Shared<UniformBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
            static Memory::Shared<UniformBuffer> Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, Memory::Shared<Shader> shader);
    };

}  //  nkentseu

#endif  // __UNIFORM_BUFFER_H__!