//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/7/2024 at 8:07:13 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_WGL_CONTEXT_H__
#define __NKENTSEU_WGL_CONTEXT_H__

#pragma once

#include "NTSCore/System.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "NTSGraphics/Core/GraphicsProperties.h"
#include <NTSGraphics/Platform/WindowInfo.h>

#include <glad/wgl.h>
#include <glad/gl.h>

namespace nkentseu {
    using GlFunctionPointer = void (*)();
    using GLFunctionPointer = PROC;

    class NKENTSEU_API NativeContext
    {
    public:
        NativeContext();
        ~NativeContext();

        bool SetWindowInfo(WindowInfo* window);
        WindowInfo* GetWindowInfo();
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

        bool IsSupportDAS();
    private:
        WindowInfo*         m_Window;
        HWND                m_WindowHandle = nullptr;
        ContextProperties   m_ContextInfo;
        HGLRC               m_Context{};
        HDC                 m_DeviceContext{};

        bool m_IsInitialize;
        bool m_IsCurrent;
        bool m_IsSupportDSA = false;

    private:
        GraphicsInfos m_GraphicsInfo = {};
        bool IsValide(WindowInfo* window);

        bool InitializeInternal(HWND windowHandle, const std::string& windowTitle, bool offscreen);
        bool DeinitializeInternal(bool precreated = false);

        HWND CreateWindowHandle();

    public:
        static std::string GetErrorString(DWORD errorCode);
        static DWORD GetFlag(uint32 flags);
        static uint32 GetPixelType(uint32 pixelTypes);
        static uint32 GetPlane(uint32 plane);

        HDC GetDeviceContextPrivate(HWND windowHandle, const std::string& windowTitle);
        bool SetPixelFormatPrivate(HDC deviceContext, const ContextProperties* contextProperties, const std::string& windowTitle);
        HGLRC CreateTemporaryContextPrivate(HDC deviceContext, const std::string& windowTitle);
        bool LoadWGLExtensionsPrivate(HDC deviceContext, const std::string& windowTitle);
        HGLRC CreateOpenGLContextPrivate(HDC deviceContext, ContextProperties* contextProperties, const std::string& windowTitle);
        void CleanupTemporaryContextPrivate(HGLRC tempContext, HDC deviceContext, HWND windowHandle, const std::string& windowTitle);
        bool LoadGladPrivate(const std::string& windowTitle);

        uint32 GetColorBits(const GraphicsPixelFormat& pixelFormat);
    };
} // namespace nkentseu

#endif

#endif    // __NKENTSEU_WGL_CONTEXT_H__