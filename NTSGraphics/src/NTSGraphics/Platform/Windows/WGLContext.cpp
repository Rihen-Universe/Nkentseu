//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/7/2024 at 8:07:13 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "WGLContext.h"

#ifdef NKENTSEU_PLATFORM_WINDOWS

#include <NTSMaths/Vector/Vector2.h>

#include <mutex>

#include <tchar.h>
#include <stdbool.h>
#include <glad/egl.h>
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
    using namespace maths;

    static const TCHAR window_classname[] = _T("SampleWndClass");
    static const TCHAR window_title[] = _T("[glad] WGL");
    static const POINT window_location = { CW_USEDEFAULT, 0 };
    static const SIZE window_size = { 1024, 768 };
    static const GLfloat clear_color[] = { 0.0f, 0.0f, 1.0f, 1.0f };

    LRESULT CALLBACK InternalWindowProcedure(HWND, UINT, WPARAM, LPARAM);
    bool internalQuit = false;

    NativeContext::NativeContext() : m_Window(nullptr), m_IsInitialize(false){}

    NativeContext::~NativeContext() {
        Deinitialize();
    }

    bool NativeContext::SetWindowInfo(WindowInfo* window) {
        if (!IsValide(window))
            return false;

        m_Window = window;
        return true;
    }

    WindowInfo* NativeContext::GetWindowInfo() {
        return m_Window;
    }

    bool NativeContext::SetProperties(const ContextProperties& properties) {
        m_ContextInfo = properties;
        return true;
    }

    const ContextProperties& NativeContext::GetProperties() {
        return m_ContextInfo;
    }

    bool NativeContext::Initialize() {
        bool isInitialize = true;
        if (!IsInitialize()) {
            std::string title;
            bool isOffscreen = false;

            if (IsValide(m_Window)) {
                m_WindowHandle = m_Window->windowHandle;
                title = m_Window->title;
                isOffscreen = false;
            }
            else {
                m_Window = nullptr;
                std::wstring wst(window_title);
                title = std::string(wst.begin(), wst.end());
                m_WindowHandle = CreateWindowHandle();
                isOffscreen = true;
            }

            if (m_WindowHandle == nullptr) {
                return false;
            }

            isInitialize = InitializeInternal(m_WindowHandle, title, isOffscreen);
        }

        if (isInitialize) {
            //wglIsDirect();
        }

        return isInitialize;
    }

    bool NativeContext::Deinitialize() {
        if (m_IsInitialize) {
            return DeinitializeInternal(m_Window == nullptr);
        }
        return false;
    }

    bool NativeContext::IsInitialize()
    {
        return m_IsInitialize;
    }

    bool NativeContext::MakeCurrent()
    {
        if (m_IsInitialize && !m_IsCurrent) {
            if (!wglMakeCurrent(m_DeviceContext, m_Context)) {
                return false;
            }
            m_IsCurrent = true;
            return true;
        }
        return false;
    }

    bool NativeContext::UnmakeCurrent()
    {
        if (m_IsInitialize && m_IsCurrent) {
            wglMakeCurrent(nullptr, nullptr);
            m_IsCurrent = false;
            return true;
        }
        return false;
    }

    bool NativeContext::IsCurrent()
    {
        return m_IsCurrent;
    }

    bool NativeContext::EnableVSync()
    {
        // VSync enabled
        return wglSwapIntervalEXT(1);
    }

    bool NativeContext::DisableVSync()
    {
        return wglSwapIntervalEXT(0);
    }

    bool NativeContext::Present()
    {
        if (IsInitialize()) {
            SwapBuffers(m_DeviceContext);
            return true;
        }
        return false;
    }

    bool NativeContext::Swapchaine()
    {
        return Present();
    }

    const GraphicsInfos& NativeContext::GetGraphicsInfo()
    {
        return m_GraphicsInfo;
    }

    bool NativeContext::IsValide(WindowInfo* window)
    {
        return !(window == nullptr || window->windowHandle == nullptr);
    }

    DWORD NativeContext::GetFlag(uint32 flags)
    {
        DWORD flag = 0;

        if (flags & (uint32)GraphicsFlag::Enum::DoubleBuffer) flag |= PFD_DOUBLEBUFFER;
        if (flags & (uint32)GraphicsFlag::Enum::DrawToBitmap) flag |= PFD_DRAW_TO_BITMAP;
        if (flags & (uint32)GraphicsFlag::Enum::DrawToWindow) flag |= PFD_DRAW_TO_WINDOW;
        if (flags & (uint32)GraphicsFlag::Enum::GenericAccelerated) flag |= PFD_GENERIC_ACCELERATED;
        if (flags & (uint32)GraphicsFlag::Enum::GenericFormat) flag |= PFD_GENERIC_FORMAT;
        if (flags & (uint32)GraphicsFlag::Enum::NeedPalette) flag |= PFD_NEED_PALETTE;
        if (flags & (uint32)GraphicsFlag::Enum::NeedSystemPalette) flag |= PFD_NEED_SYSTEM_PALETTE;
        if (flags & (uint32)GraphicsFlag::Enum::Stereo) flag |= PFD_STEREO;
        if (flags & (uint32)GraphicsFlag::Enum::SupportGDI) flag |= PFD_SUPPORT_GDI;
        if (flags & (uint32)GraphicsFlag::Enum::SupportOpenGL) flag |= PFD_SUPPORT_OPENGL;
        if (flags & (uint32)GraphicsFlag::Enum::SwapLayerBuffer) flag |= PFD_SWAP_LAYER_BUFFERS;
        if (flags & (uint32)GraphicsFlag::Enum::TripleBuffer) flag |= PFD_DOUBLEBUFFER; // Est-ce correct ? Ne devrait-ce pas être PFD_SWAP_LAYER_BUFFERS?
        if (flags & (uint32)GraphicsFlag::Enum::DepthDontCare) flag |= PFD_DEPTH_DONTCARE;
        if (flags & (uint32)GraphicsFlag::Enum::DoubleBufferDontCare) flag |= PFD_DOUBLEBUFFER_DONTCARE;
        if (flags & (uint32)GraphicsFlag::Enum::StereoDontCare) flag |= PFD_STEREO_DONTCARE;
        if (flags & (uint32)GraphicsFlag::Enum::SwapExchange) flag |= PFD_SWAP_EXCHANGE;
        if (flags & (uint32)GraphicsFlag::Enum::SwapCopy) flag |= PFD_SWAP_COPY;
        if (flags & (uint32)GraphicsFlag::Enum::SupportDirectDraw) flag |= PFD_SUPPORT_DIRECTDRAW;
        if (flags & (uint32)GraphicsFlag::Enum::Direct3DAccelerated) flag |= PFD_DIRECT3D_ACCELERATED;
        if (flags & (uint32)GraphicsFlag::Enum::SupportComposition) flag |= PFD_SUPPORT_COMPOSITION;
        if (flags & (uint32)GraphicsFlag::Enum::TripeBufferDontCare) flag |= PFD_DOUBLEBUFFER_DONTCARE; // Est-ce correct ? Ne devrait-ce pas être PFD_TRIPLEBUFFER_DONTCARE?

        return flag;
    }

    uint32 NativeContext::GetPixelType(uint32 pixelTypes)
    {
        uint32 pixelType = 0;

        if (pixelTypes & (uint32)GraphicsPixelType::Enum::RgbaArb) pixelType |= PFD_TYPE_RGBA;
        if (pixelTypes & (uint32)GraphicsPixelType::Enum::ColorIndexArb) pixelType |= PFD_TYPE_COLORINDEX;

        return pixelType;
    }

    uint32 NativeContext::GetPlane(uint32 plane)
    {
        uint32 plane_ = 0;

        if (plane & (uint32)GraphicsMainLayer::Enum::MainPlane) plane_ |= PFD_MAIN_PLANE;
        if (plane & (uint32)GraphicsMainLayer::Enum::OverlayPlane) plane_ |= PFD_OVERLAY_PLANE;
        if (plane & (uint32)GraphicsMainLayer::Enum::UnderlayPlane) plane_ |= PFD_UNDERLAY_PLANE;

        return plane_;
    }

    bool NativeContext::InitializeInternal(HWND windowHandle, const std::string& windowTitle, bool offscreen) {
        m_DeviceContext = GetDeviceContextPrivate(windowHandle, windowTitle);
        if (!m_DeviceContext) {
            return false;
        }

        if (!SetPixelFormatPrivate(m_DeviceContext, &m_ContextInfo, windowTitle)) {
            ReleaseDC(windowHandle, m_DeviceContext);
            if (m_Window == nullptr) DestroyWindow(windowHandle);
            return false;
        }

        HGLRC tempContext = CreateTemporaryContextPrivate(m_DeviceContext, windowTitle);
        if (!tempContext) {
            ReleaseDC(windowHandle, m_DeviceContext);
            if (m_Window == nullptr) DestroyWindow(windowHandle);
            return false;
        }

        if (!LoadWGLExtensionsPrivate(m_DeviceContext, windowTitle)) {
            CleanupTemporaryContextPrivate(tempContext, m_DeviceContext, windowHandle, windowTitle);
            if (m_Window == nullptr) DestroyWindow(windowHandle);
            return false;
        }

        m_Context = CreateOpenGLContextPrivate(m_DeviceContext, &m_ContextInfo, windowTitle);
        if (m_Context == nullptr) {
            CleanupTemporaryContextPrivate(tempContext, m_DeviceContext, windowHandle, windowTitle);
            if (m_Window == nullptr) DestroyWindow(windowHandle);
            return false;
        }

        CleanupTemporaryContextPrivate(tempContext, m_DeviceContext, windowHandle, windowTitle);

        wglMakeCurrent(m_DeviceContext, m_Context);

        if (!LoadGladPrivate(windowTitle)) {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(m_Context);
            ReleaseDC(windowHandle, m_DeviceContext);
            if (m_Window == nullptr) DestroyWindow(windowHandle);
            return false;
        }

        if (m_ContextInfo.pixelFormat.flags & GraphicsFlag::Enum::DoubleBuffer) {
            EnableVSync();
        }

        m_IsInitialize = true;
        m_IsCurrent = true;
        return true;
    }

    HDC NativeContext::GetDeviceContextPrivate(HWND windowHandle, const std::string& windowTitle) {
        HDC deviceContext = GetDC(windowHandle);
        if (deviceContext == NULL) {
            gLog.Error("Failed to get Window's device context! {0}", windowTitle);
            return NULL;
        }
        return deviceContext;
    }

    bool NativeContext::SetPixelFormatPrivate(HDC deviceContext, const ContextProperties* contextProperties, const std::string& windowTitle) {
        PIXELFORMATDESCRIPTOR pfd = { };

        pfd.nSize = sizeof(pfd);
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);  // Set the size of the PFD to the size of the class
        pfd.dwFlags = GetFlag(contextProperties->pixelFormat.flags);   // Enable double buffering, opengl support and drawing to a window
        pfd.iPixelType = GetPixelType(m_ContextInfo.pixelFormat.pixelType); // Set our application to use RGBA pixels
        pfd.cColorBits = GetColorBits(contextProperties->pixelFormat); // Give us 32 bits of color information (the higher, the more colors)
        pfd.cAlphaBits = contextProperties->pixelFormat.alphaColorBits;
        pfd.cDepthBits = contextProperties->pixelFormat.depthBits;  // Give us 32 bits of depth information (the higher, the more depth levels)
        pfd.cStencilBits = contextProperties->pixelFormat.stencilBits;
        pfd.cAccumBits = contextProperties->pixelFormat.accumBits;
        pfd.cAuxBuffers = contextProperties->pixelFormat.auxBuffers;
        pfd.iLayerType = GetPlane(contextProperties->pixelFormat.mainLayer);    // Set the layer of the PFD

        int format = ChoosePixelFormat(deviceContext, &pfd);

        if (format == 0 || SetPixelFormat(deviceContext, format, &pfd) == FALSE) {
            gLog.Error("Failed to set a compatible pixel format! {0}", windowTitle);
            return false;
        }
        return true;
    }

    HGLRC NativeContext::CreateTemporaryContextPrivate(HDC deviceContext, const std::string& windowTitle) {
        HGLRC tempContext = wglCreateContext(deviceContext);
        if (tempContext == NULL) {
            gLog.Error("Failed to create a temporary context! {0}", windowTitle);
            return NULL;
        }
        wglMakeCurrent(deviceContext, tempContext);
        return tempContext;
    }

    bool NativeContext::LoadWGLExtensionsPrivate(HDC deviceContext, const std::string& windowTitle) {
        if (!gladLoaderLoadWGL(deviceContext)) {
            gLog.Error("Failed to load wgl extension! {0}", windowTitle);
            return false;
        }
        return true;
    }

    HGLRC NativeContext::CreateOpenGLContextPrivate(HDC deviceContext, ContextProperties* contextProperties, const std::string& windowTitle) {
        HGLRC context = nullptr;
        HGLRC sharedContext = nullptr;

        if (contextProperties->version == Vector2i()) {
            contextProperties->version = ContextProperties::InitVersion(GraphicsApiType::Enum::OpenglApi);
        }

        while (!context && contextProperties->version.major) {
            if (WGL_ARB_create_context) {
                std::vector<int32> attributes;

                if ((contextProperties->version.major > 1) || ((contextProperties->version.major == 1) && (contextProperties->version.minor > 1))) {
                    attributes.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
                    attributes.push_back(static_cast<int32>(contextProperties->version.major));
                    attributes.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
                    attributes.push_back(static_cast<int32>(contextProperties->version.minor));
                }

                if (WGL_ARB_create_context_profile) {
                    const int32 profile = (contextProperties->pixelFormat.attributeFlags & (uint32)GraphicsAttribute::Enum::AttributeCore)
                        ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
                    const int32 debug = (contextProperties->pixelFormat.attributeFlags & (uint32)GraphicsAttribute::Enum::AttributeDebug) ? WGL_CONTEXT_DEBUG_BIT_ARB : 0;

                    attributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
                    attributes.push_back(profile);
                    attributes.push_back(WGL_CONTEXT_FLAGS_ARB);
                    attributes.push_back(debug);
                }
                else {
                    if ((contextProperties->pixelFormat.attributeFlags & (uint32)GraphicsAttribute::Enum::AttributeCore) ||
                        (contextProperties->pixelFormat.attributeFlags & (uint32)GraphicsAttribute::Enum::AttributeDebug)) {
                        gLog.Error("Selecting a profile during context creation is not supported, disabling compatibility and debug");
                    }

                    contextProperties->pixelFormat.attributeFlags = (uint32)GraphicsAttribute::Enum::AttributeDefault;
                }

                attributes.push_back(0);
                attributes.push_back(0);

                context = wglCreateContextAttribsARB(deviceContext, sharedContext, attributes.data());
            }
            else {
                break;
            }
            if (!context) {
                if (contextProperties->pixelFormat.attributeFlags != (uint32)GraphicsAttribute::Enum::AttributeDefault) {
                    contextProperties->pixelFormat.attributeFlags = (uint32)GraphicsAttribute::Enum::AttributeDefault;
                }
                else if (contextProperties->version.minor > 0) {
                    --contextProperties->version.minor;

                    contextProperties->pixelFormat.attributeFlags = contextProperties->pixelFormat.attributeFlags;
                }
                else {
                    --contextProperties->version.major;
                    contextProperties->version.minor = 9;

                    contextProperties->pixelFormat.attributeFlags = contextProperties->pixelFormat.attributeFlags;
                }
            }
        }
        if (context == nullptr){
            gLog.Error("Cannot create opengl context : Version = {0}", contextProperties->version);
        }
        return context;
    }

    void NativeContext::CleanupTemporaryContextPrivate(HGLRC tempContext, HDC deviceContext, HWND windowHandle, const std::string& windowTitle) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(tempContext);
        ReleaseDC(windowHandle, deviceContext);
    }

    bool NativeContext::LoadGladPrivate(const std::string& windowTitle) {
        if (!gladLoaderLoadGL()) {
            gLog.Error("Glad Loader failed! {0}", windowTitle);
            return false;
        }
        return true;
    }

    bool NativeContext::DeinitializeInternal(bool precreated)
    {
        if (m_WindowHandle == nullptr || m_Context == nullptr || m_DeviceContext == nullptr) {
            return false;
        }

        wglMakeCurrent(m_DeviceContext, nullptr);
        wglDeleteContext(m_Context);
        ReleaseDC(m_WindowHandle, m_DeviceContext);

        if (precreated) {
            DestroyWindow(m_WindowHandle);
        }

        return true;
    }

    HWND NativeContext::CreateWindowHandle()
    {
        WNDCLASSEX wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = InternalWindowProcedure;
        wcex.hInstance = m_Window->instance;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszClassName = window_classname;

        ATOM wndclass = RegisterClassEx(&wcex);

        HWND windowHandle = CreateWindow(MAKEINTATOM(wndclass), window_title,
            WS_OVERLAPPEDWINDOW,
            window_location.x, window_location.y,
            window_size.cx, window_size.cy,
            nullptr, nullptr, m_Window->instance, nullptr);
        return windowHandle;
    }

    std::string NativeContext::GetErrorString(DWORD errorCode)
    {
        PTCHAR buffer;
        if (FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            0,
            reinterpret_cast<LPTSTR>(&buffer),
            256,
            nullptr) != 0) {
            std::wstring wErrMsg(buffer);
            std::string errMsg(wErrMsg.begin(), wErrMsg.end());
            LocalFree(buffer);
            return errMsg;
        }
        return FORMATTER.Format("Error : {0}", errorCode);
    }

    uint32 NativeContext::GetColorBits(const GraphicsPixelFormat& pixelFormat){
        return pixelFormat.redColorBits + pixelFormat.greenColorBits + pixelFormat.blueColorBits + pixelFormat.alphaColorBits;
    }
    /*
    * int32 pixel_attributes[] = {
            WGL_SUPPORT_OPENGL_ARB, 1,
            WGL_DRAW_TO_WINDOW_ARB, 1,
            WGL_DRAW_TO_BITMAP_ARB, 1,
            WGL_DOUBLE_BUFFER_ARB, 1,
            WGL_SWAP_LAYER_BUFFERS_ARB, 1,
            WGL_COLOR_BITS_ARB, 32,
            WGL_RED_BITS_ARB, 8,
            WGL_GREEN_BITS_ARB, 8,
            WGL_BLUE_BITS_ARB, 8,
            WGL_ALPHA_BITS_ARB, 8,
            WGL_DEPTH_BITS_ARB, 32,
            WGL_STENCIL_BITS_ARB, 8,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            0
        };

    */
    //----------------------------

    LRESULT CALLBACK InternalWindowProcedure(HWND window_handle, UINT message, WPARAM param_w, LPARAM param_l)
    {
        switch (message) {
        case WM_DESTROY:
            if (!internalQuit) {
                internalQuit = true;
            }
            else {
                PostQuitMessage(0);
            }
            return 0;
        }

        return DefWindowProc(window_handle, message, param_w, param_l);
    }
    
    bool NativeContext::IsSupportDAS()
    {
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        if (extensions == nullptr)
        {
            return false;
        }

        bool hasVertexShader = false;
        bool hasFragmentShader = false;

        while (*extensions != '\0')
        {
            if (strcmp(extensions, "GL_ARB_vertex_shader") == 0)
            {
                hasVertexShader = true;
            }
            else if (strcmp(extensions, "GL_ARB_fragment_shader") == 0)
            {
                hasFragmentShader = true;
            }

            extensions += strlen(extensions) + 1;
        }

        return hasVertexShader && hasFragmentShader;
    }

}    // namespace nkentseu

#endif