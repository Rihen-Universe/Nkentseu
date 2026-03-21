//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/9/2024 at 10:22:38 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_XGLCONTEXT_H__
#define __NKENTSEU_XGLCONTEXT_H__

#pragma once

#include "NTSCore/System.h"

#if defined(NKENTSEU_PLATFORM_LINUX_XLIB) and defined(NKENTSEU_GRAPHICS_API_OPENGL)

#include "Nkentseu/Graphics/GraphicsProperties.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <glad/gl.h>
#include <glad/glx.h>

namespace nkentseu {
    using GlFunctionPointer = void (*)();
    class Window;

    class NKENTSEU_API NativeContext
    {
    public:
        NativeContext();
        ~NativeContext();

        bool SetWindow(class Window* window);
        class Window* GetWindow();
        bool SetProperties(const ContextProperties& properties);
        const ContextProperties& GetProperties();

        bool Initialize();
        bool Deinitialize();
        bool IsInitialize();

        bool MakeCurrent();
        bool UnmakeCurrent();
        bool IsCurrent();

        bool EnableVSync();
        bool DisableVSync();

        bool Present();
        bool Swapchaine();

        const GraphicsInfos& GetGraphicsInfo();
    private:
        class Window* m_Window;
        ContextProperties   m_ContextInfo;
        GraphicsInfos m_GraphicsInfo = {};

        int32 m_NumberFramebufferContext = 0;
        GLXFBConfig *m_FramebufferContext = nullptr;
        GLXContext m_Context = nullptr;
        unsigned long m_WindowHandle = 0;

        bool m_IsInitialize;
        bool m_IsCurrent;
    private:
        bool IsValide(class Window* window);
        static std::string GetErrorString(uint32 errorCode);

        void InitFrameBuffer();
        int32 GetPixelType(uint32 pixelTypes);
        int32 GetFlag(uint32 flags);

        bool CreateGLContext();
        bool ConfigureFramebufferContext();
        bool LoadGLX();
        bool CreateGLContextAttributes(GLXContext& context);
        void UpdateContextVersion();
        bool CheckGLXMakeCurrent();
        void PrintGLXDirectStatus();
        bool SetGLXMakeCurrent();
        bool LoadGL();
    };
} // namespace nkentseu

#endif

#endif    // __NKENTSEU_XGLCONTEXT_H__