//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/9/2024 at 10:22:38 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "XGLContext.h"

#if defined(NKENTSEU_PLATFORM_LINUX_XCB) && defined(NKENTSEU_GRAPHICS_API_OPENGL)

#include "Nkentseu/Core/Window.h"
#include "WindowInternal.h"
#include <Nkentseu/Platform/PlatformState.h>
#include <Nkentseu/Core/NkentseuLogger.h>

namespace nkentseu {
    NativeContext::NativeContext() : m_Window(nullptr), m_IsInitialize(false){}

    NativeContext::~NativeContext() {
        Deinitialize();
    }

    bool NativeContext::SetWindow(Window* window) {
        if (!IsValide(window))
            return false;

        m_Window = window;
        return true;
    }

    Window* NativeContext::GetWindow() {
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
        if (!IsInitialize()) {
            m_VisualID = PlatformState.screen->root_visual;
            InitFrameBuffer();

            /* Select first framebuffer config and query visualID */
            // glXGetFBConfigAttrib(PlatformState.display, m_FbConfig, GLX_VISUAL_ID, &m_VisualID);

            int attribs[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB,
                m_ContextInfo.version.major,
                GLX_CONTEXT_MINOR_VERSION_ARB,
                m_ContextInfo.version.minor,
                0
            };
            XVisualInfo * foo = glXGetVisualFromFBConfig(PlatformState.display, m_FbConfig);
            m_Context = glXCreateContext(PlatformState.display, foo, NULL, True);

            if (!m_Context) {
                Log_nts.Error("glXCreateNewContext failed");
                return false;
            }

            // Create GLX Window
            m_Glxwindow = glXCreateWindow(PlatformState.display, m_FbConfig, m_Window->GetInternal()->GetWindowDisplay()->windowHandle, 0);

            if (!m_Glxwindow){
                Log_nts.Error("glXCreateWindow failed");
                return false;
            }

            m_Drawable = m_Glxwindow;

            // make OpenGL context current
            if (!glXMakeContextCurrent(PlatformState.display, m_Drawable, m_Drawable, m_Context)){
                Log_nts.Error("glXMakeContextCurrent failed");
                return false;
            }
            m_IsCurrent = true;
            m_IsInitialize = true;
        }
        return false;
    }

    bool NativeContext::Deinitialize() {
        if (IsInitialize()) {
            glXDestroyWindow(PlatformState.display, m_Glxwindow);
            glXDestroyContext(PlatformState.display, m_Context);

            m_IsInitialize = false;
            m_IsCurrent = false;

            return true;
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
            // make OpenGL context current
            if (!glXMakeContextCurrent(PlatformState.display, m_Drawable, m_Drawable, m_Context)){
                Log_nts.Error("glXMakeContextCurrent failed");
            }
            m_IsCurrent = true;
        }
        return false;
    }

    bool NativeContext::UnmakeCurrent()
    {
        if (m_IsInitialize && m_IsCurrent) {
            m_IsCurrent = false;
        }
        return false;
    }

    bool NativeContext::IsCurrent()
    {
        return m_IsCurrent;
    }

    bool NativeContext::EnableVSync()
    {
        if (!glXSwapIntervalSGI) {
            Log_nts.Error("VSync is not supported");
        } else {
            glXSwapIntervalSGI(1);
            return true;
        }
        return false;
    }

    bool NativeContext::DisableVSync()
    {
        return false;
    }

    bool NativeContext::Present()
    {
        if (IsInitialize()) {
            glXSwapBuffers(PlatformState.display, m_Drawable);
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

    bool NativeContext::IsValide(class Window* window)
    {
        return !(window == nullptr || window->GetInternal() == nullptr || window->GetInternal()->GetWindowDisplay() == nullptr || window->GetInternal()->GetWindowDisplay()->windowHandle == (xcb_window_t)-1);
    }

    std::string NativeContext::GetErrorString(uint32 errorCode)
    {
        return "";
    }

    void NativeContext::InitFrameBuffer(){
        /* Query framebuffer configurations */
        GLXFBConfig *fbConfigs = 0;
        int numFbConfigs = 0;
        fbConfigs = glXGetFBConfigs(PlatformState.display, PlatformState.defaultScreen, &numFbConfigs);
        if (!fbConfigs || numFbConfigs == 0){
            Log_nts.Error("glXGetFBConfigs failed");
        }
        m_FbConfig = fbConfigs[0];
    }
}    // namespace nkentseu

#endif