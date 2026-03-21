//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/10/2024 at 2:35:46 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_INTERNALRENDERER_H__
#define __NKENTSEU_INTERNALRENDERER_H__

#pragma once

#include "NTSCore/System.h"

#ifdef NKENTSEU_GRAPHICS_API_SOFTWARE

#include <Nkentseu/Graphics/Color.h>
#include <Ntsm/Vector/Vector2.h>

namespace nkentseu {
    class Context;

    class NKENTSEU_API InternalRenderer
    {
        public:
            InternalRenderer();
            ~InternalRenderer();

            bool Initialize(class Context* context);
            bool Deinitialize();

            bool Clear(const Color& color);
            bool Clear(uint8 r, uint8 g, uint8 b, uint8 a = 255);

            bool Present();
            bool Swapbuffer();

            bool Resize(const Vector2u& size);
        private:
            class Context* m_Context = nullptr;
    };
} // namespace nkentseu

#endif    // __NKENTSEU_INTERNALRENDERER_H__

#endif    // __NKENTSEU_INTERNALRENDERER_H__