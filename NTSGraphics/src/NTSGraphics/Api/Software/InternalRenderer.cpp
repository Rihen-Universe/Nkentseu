//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/10/2024 at 2:35:46 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "InternalRenderer.h"

#ifdef NKENTSEU_GRAPHICS_API_SOFTWARE

#include <Nkentseu/Graphics/Context.h>
#include "Nkentseu/Graphics/Color.h"
#include <Nkentseu/Core/NkentseuLogger.h>
#include "InternalRenderer.h"
#include "InternalRenderer.h"
#include "InternalRenderer.h"

namespace nkentseu {

    InternalRenderer::InternalRenderer() : m_Context(nullptr){
    }

    InternalRenderer::~InternalRenderer(){
    }

    bool InternalRenderer::Initialize(Context* context)
    {
        return true;
    }

    bool InternalRenderer::Deinitialize()
    {
        return false;
    }

    bool InternalRenderer::Clear(const Color& color)
    {
        return false;
    }

    bool InternalRenderer::Clear(uint8 r, uint8 g, uint8 b, uint8 a)
    {
        return Clear(Color(r, g, b, a));
    }

    bool InternalRenderer::Present()
    {
        return false;
    }

    bool InternalRenderer::Swapbuffer()
    {
        return Present();
    }

    bool InternalRenderer::Resize(const Vector2u& size)
    {
        return true;
    }
}    // namespace nkentseu

#endif