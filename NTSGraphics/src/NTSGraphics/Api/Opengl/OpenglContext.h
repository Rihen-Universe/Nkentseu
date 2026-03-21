//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/3/2024 at 12:51:22 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_INTERNAL_CONTEXT_H__
#define __OPENGL_INTERNAL_CONTEXT_H__

#pragma once

#include "NTSCore/System.h"
#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/GraphicsProperties.h"

namespace nkentseu {
    class NativeContext;
    class Window;

    class NKENTSEU_API OpenglContext : public Context
    {
        public:
            OpenglContext(WindowInfo* window, const ContextProperties& contextProperties);
            ~OpenglContext();

            bool Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize) override;
            bool Deinitialize() override;
            bool IsInitialize() override;

            bool EnableVSync() override;
            bool DisableVSync() override;
            
            const maths::Vector2f& GetSize() override;
            const maths::Vector2f& GetDpiSize() override;

            const GraphicsInfos& GetGraphicsInfo() override;
            const ContextProperties& GetProperties() override;
            WindowInfo* GetWindowInfo() override;
            bool OnWindowResized(const maths::Vector2f& size, const maths::Vector2f& dpiSize) override;
            Memory::Shared<NativeContext> GetNative();

            bool MakeCurrent();
            bool UnmakeCurrent();
            bool IsCurrent();

            bool Swapchaine();
            bool IsValidContext();

            bool Present() override;
        private:
            friend class OpenglRenderWindow;

            WindowInfo* m_Window = nullptr;
            Memory::Shared<NativeContext> m_NativeContext;
            maths::Vector2f size;
            maths::Vector2f dpiSize;

            ContextProperties m_Properties;

    };
} // namespace nkentseu

#endif    // __NKENTSEU_INTERNAL_CONTEXT_H__