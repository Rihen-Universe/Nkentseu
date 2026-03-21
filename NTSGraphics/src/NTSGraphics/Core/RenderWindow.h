//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-08 at 05:28:16 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __RENDER_WINDOW_H__
#define __RENDER_WINDOW_H__

#pragma once

#include <NTSCore/System.h>
#include "Renderer.h"

namespace nkentseu {
    
    class NKENTSEU_API RenderWindow : public Renderer {
        public:
            static Memory::Shared<RenderWindow> Create(Memory::Shared<Context> context);
            static Memory::Shared<RenderWindow> CreateInitialized(Memory::Shared<Context> context, const maths::Vector2f& size, const maths::Vector2f& dpiSize);
    };

}  //  nkentseu

#endif  // __RENDER_WINDOW_H__!