//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/3/2024 at 12:48:26 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_CONTEXT_H__
#define __NKENTSEU_CONTEXT_H__

#pragma once

#include "NTSCore/System.h"
#include "GraphicsProperties.h"

namespace nkentseu {
    struct WindowInfo;

    class NKENTSEU_API Context
    {
        public:
            virtual bool Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize) = 0;
            virtual bool Deinitialize() = 0;
            virtual bool IsInitialize() = 0;

            virtual bool EnableVSync() = 0;
            virtual bool DisableVSync() = 0;

            virtual const maths::Vector2f& GetSize() = 0;
            virtual const maths::Vector2f& GetDpiSize() = 0;

            virtual bool Prepare() { return true; };
            virtual bool Present() { return true; };

            virtual const GraphicsInfos& GetGraphicsInfo() = 0;
            virtual const ContextProperties& GetProperties() = 0;

            virtual WindowInfo* GetWindowInfo() = 0;

            virtual bool OnWindowResized(const maths::Vector2f& size, const maths::Vector2f& dpiSize) = 0;

            static Memory::Shared<Context> Create(WindowInfo* window, const ContextProperties& contextProperties);
            static Memory::Shared<Context> CreateInitialized(WindowInfo* window, const ContextProperties& contextProperties, const maths::Vector2f& size, const maths::Vector2f& dpiSize);
    };
} // namespace nkentseu

#endif    // __NKENTSEU_CONTEXT_H__