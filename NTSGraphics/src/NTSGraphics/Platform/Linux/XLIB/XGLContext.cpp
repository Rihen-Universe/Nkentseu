//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/9/2024 at 10:22:38 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "XGLContext.h"

#if defined(NKENTSEU_PLATFORM_LINUX_XLIB) && defined(NKENTSEU_GRAPHICS_API_OPENGL)

#include "Nkentseu/Core/Window.h"
#include "WindowInternal.h"
#include <Nkentseu/Platform/PlatformState.h>
#include <Nkentseu/Core/NkentseuLogger.h>

#ifndef GLX_CONTEXT_DEBUG_BIT_ARB
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001
#endif

namespace nkentseu {
    NativeContext::NativeContext() : m_Window(nullptr), m_IsInitialize(false){}

    NativeContext::~NativeContext() {
        Deinitialize();
    }

    bool NativeContext::SetWindow(Window* window) {
        if (!IsValide(window)){
            Log_nts.Error();
            return false;
        }

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
        if (!IsValide(m_Window)){
            m_WindowHandle = PlatformState.rootWindow;
        } else {
            m_WindowHandle = m_Window->GetInternal()->GetWindowDisplay()->windowHandle;
        }
        
        if (!IsInitialize()) {
            if (!LoadGLX()) {
                return false;
            }

            if (!ConfigureFramebufferContext()) {
                return false;
            }

            if (!CreateGLContext()) {
                return false;
            }

            m_IsCurrent = true;
            m_IsInitialize = true;

            return true;
        }
        return false;
    }

    bool NativeContext::LoadGLX() {
        int32_t glx_version = gladLoaderLoadGLX(PlatformState.display, PlatformState.screen);
        if (!glx_version) {
            Log_nts.Error("Unable to load GLX.");
            return false;
        }
        return true;
    }

    bool NativeContext::ConfigureFramebufferContext() {
        int32_t accum = m_ContextInfo.pixelFormat.accumBits;
        int32_t red_accum = accum % 8; accum /= 8;
        int32_t green_accum = accum % 8; accum /= 8;
        int32_t blue_accum = accum % 8; accum /= 8;
        int32_t alpha_accum = accum % 8; accum /= 8;

        GLint visual_attributes[] =
        {
            GLX_DRAWABLE_TYPE, GetFlag(m_ContextInfo.pixelFormat.flags),
            GLX_RENDER_TYPE, GetPixelType(m_ContextInfo.pixelFormat.pixelType),
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_RED_SIZE, (int32_t)m_ContextInfo.pixelFormat.redColorBits,
            GLX_GREEN_SIZE, (int32_t)m_ContextInfo.pixelFormat.greenColorBits,
            GLX_BLUE_SIZE, (int32_t)m_ContextInfo.pixelFormat.blueColorBits,
            GLX_ALPHA_SIZE, (int32_t)m_ContextInfo.pixelFormat.alphaColorBits,
            GLX_DEPTH_SIZE, (int32_t)m_ContextInfo.pixelFormat.depthBits,
            GLX_STENCIL_SIZE, (int32_t)m_ContextInfo.pixelFormat.stencilBits,
            GLX_ACCUM_RED_SIZE, red_accum,
            GLX_ACCUM_GREEN_SIZE, green_accum,
            GLX_ACCUM_BLUE_SIZE, blue_accum,
            GLX_ACCUM_ALPHA_SIZE, alpha_accum,
            GLX_AUX_BUFFERS, (int32_t)m_ContextInfo.pixelFormat.auxBuffers,
            GLX_DOUBLEBUFFER, (m_ContextInfo.pixelFormat.flags & GraphicsFlag::DoubleBuffer) ? True : False,
            None
        };
        
        m_FramebufferContext = glXChooseFBConfig(PlatformState.display, PlatformState.screen, visual_attributes, &m_NumberFramebufferContext);

        if (m_FramebufferContext == nullptr){
            Log_nts.Error("Unable to create frame buffer context");
            return false;
        }
        return true;
    }

    bool NativeContext::CreateGLContext() {
        if (m_ContextInfo.version == Vector2i()) {
            m_ContextInfo.version = ContextProperties::InitVersion();
        }

        GLXContext context = nullptr;

        while (!context && m_ContextInfo.version.major) {
            if (!CreateGLContextAttributes(context)) {
                return false;
            }

            if (!context) {
                UpdateContextVersion();
            }
        }

        if (!context) {
            Log_nts.Error("Cannot create opengl context : Version = {0}", m_ContextInfo.version);
            return false;
        }

        m_Context = context;
        
        if (!m_Context) {
            Log_nts.Error("Unable to create OpenGL context.");
            return false;
        }

        if (!CheckGLXMakeCurrent()) {
            return false;
        }

        PrintGLXDirectStatus();
        
        if (!SetGLXMakeCurrent()) {
            return false;
        }

        if (!LoadGL()) {
            return false;
        }

        return true;
    }

    bool NativeContext::CreateGLContextAttributes(GLXContext& context) {
        std::vector<int32_t> attributes;

        if ((m_ContextInfo.version.major > 1) || ((m_ContextInfo.version.major == 1) && (m_ContextInfo.version.minor > 1))) {
            attributes.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
            attributes.push_back(static_cast<int32_t>(m_ContextInfo.version.major));
            attributes.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
            attributes.push_back(static_cast<int32_t>(m_ContextInfo.version.minor));
        }

        if (GLX_ARB_create_context_profile) {
            const int32_t profile = (m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeCore) ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
            const int32_t debug = (m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeDebug) ? GLX_CONTEXT_DEBUG_BIT_ARB : 0;

            attributes.push_back(GLX_CONTEXT_PROFILE_MASK_ARB);
            attributes.push_back(profile);
            attributes.push_back(GLX_CONTEXT_FLAGS_ARB);
            attributes.push_back(debug);
        }
        else {
            if ((m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeCore) ||
                (m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeDebug)) {
                Log_nts.Error("Selecting a profile during context creation is not supported, disabling compatibility and debug");
            }

            m_ContextInfo.pixelFormat.attributeFlags = GraphicsAttribute::AttributeDefault;
        }

        attributes.push_back(None);

        context = glXCreateContextAttribsARB(PlatformState.display, m_FramebufferContext[0], NULL, True, attributes.data());

        return context != nullptr;
    }

    void NativeContext::UpdateContextVersion() {
        if (m_ContextInfo.pixelFormat.attributeFlags != GraphicsAttribute::AttributeDefault) {
            m_ContextInfo.pixelFormat.attributeFlags = GraphicsAttribute::AttributeDefault;
        }
        else if (m_ContextInfo.version.minor > 0) {
            --m_ContextInfo.version.minor;
            m_ContextInfo.pixelFormat.attributeFlags = m_ContextInfo.pixelFormat.attributeFlags;
        }
        else {
            --m_ContextInfo.version.major;
            m_ContextInfo.version.minor = 9;
            m_ContextInfo.pixelFormat.attributeFlags = m_ContextInfo.pixelFormat.attributeFlags;
        }
    }

    bool NativeContext::CheckGLXMakeCurrent() {
        if (glXMakeCurrent == nullptr) {
            glXDestroyContext(PlatformState.display, m_Context);
            Log_nts.Error("glXMakeCurrent is null.");
            return false;
        }
        return true;
    }

    void NativeContext::PrintGLXDirectStatus() {
        if (!glXIsDirect(PlatformState.display, m_Context)){
            Log_nts.Debug("Indirect GLX");
        } else {
            Log_nts.Debug("Direct GLX");
        }
    }

    bool NativeContext::SetGLXMakeCurrent() {
        return glXMakeCurrent(PlatformState.display, m_WindowHandle, m_Context);;
    }

    bool NativeContext::LoadGL() {
        int32_t gl_version = gladLoaderLoadGL();
        if (!gl_version) {
            Log_nts.Error("Unable to load GL.");
            return false;
        }
        return true;
    }

    bool NativeContext::Deinitialize() {
        if (IsInitialize()) {
            glXMakeCurrent(PlatformState.display, 0, 0);
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
            glXMakeCurrent(PlatformState.display, m_Window->GetInternal()->GetWindowDisplay()->windowHandle, m_Context);
            m_IsCurrent = true;
            return true;
        }
        return false;
    }

    bool NativeContext::UnmakeCurrent()
    {
        if (m_IsInitialize && m_IsCurrent) {
            glXMakeCurrent(PlatformState.display, 0, 0);
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
        return false;
    }

    bool NativeContext::DisableVSync()
    {
        return false;
    }

    bool NativeContext::Present()
    {
        if (IsInitialize()) {
            glXSwapBuffers(PlatformState.display, m_Window->GetInternal()->GetWindowDisplay()->windowHandle);
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
        if (window == nullptr){
            Log_nts.Error("Window is invalid");
            return false;
        }

        if (window->GetInternal() == nullptr){
            Log_nts.Error("Internal window is invalid");
            return false;
        }

        if (window->GetInternal()->GetWindowDisplay() == nullptr){
            Log_nts.Error("Internal window display is invalid");
            return false;
        }

        if (window->GetInternal()->GetWindowDisplay()->windowHandle == 0){
            Log_nts.Error("Internal window display windowHandle is invalid");
            return false;
        }
        return true;
    }

    std::string NativeContext::GetErrorString(uint32 errorCode)
    {
        return "";
    }

    void NativeContext::InitFrameBuffer(){
    }

    int32 NativeContext::GetPixelType(uint32 pixelTypes)
    {
        int32 pixelType = 0;

        if (pixelTypes & GraphicsPixelType::RgbaArb || pixelTypes & GraphicsPixelType::RgbaExt || 
            pixelTypes & GraphicsPixelType::RgbaFloatArb || pixelTypes & GraphicsPixelType::RgbaFloatAti || pixelTypes & GraphicsPixelType::RgbaUnsignedFloatExt) pixelType |= GLX_RGBA_BIT;
        
        if (pixelTypes & GraphicsPixelType::ColorIndexArb || pixelTypes & GraphicsPixelType::ColorIndexExt) pixelType |= GLX_COLOR_INDEX_BIT;

        return pixelType;
    }

    int32 NativeContext::GetFlag(uint32 flags)
    {
        int32 flag = 0;

        if (flags & GraphicsFlag::DrawToBitmap) flag |= GLX_PIXMAP_BIT;
        if (flags & GraphicsFlag::DrawToWindow) flag |= GLX_WINDOW_BIT;
        if (flags & GraphicsFlag::SwapLayerBuffer) flag |= GLX_PBUFFER_BIT;

        return flag;
    }
/*
    HGLRC NativeContext::CreateOpenGLContextPrivate(HDC deviceContext, ContextProperties* contextProperties, const std::string& windowTitle) {
        HGLRC context = nullptr;
        HGLRC sharedContext = nullptr;

        if (m_ContextInfo.version == Vector2i()) {
            m_ContextInfo.version = ContextProperties::InitVersion();
        }

        while (!context && m_ContextInfo.version.major) {
            if (WGL_ARB_create_context) {
                std::vector<int32> attributes;

                if ((m_ContextInfo.version.major > 1) || ((m_ContextInfo.version.major == 1) && (m_ContextInfo.version.minor > 1))) {
                    attributes.push_back(GLX_CONTEXT_MAJOR_VERSION_ARB);
                    attributes.push_back(static_cast<int32>(m_ContextInfo.version.major));
                    attributes.push_back(GLX_CONTEXT_MINOR_VERSION_ARB);
                    attributes.push_back(static_cast<int32>(m_ContextInfo.version.minor));
                }

                if (WGL_ARB_create_context_profile) {
                    const int32 profile = (m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeCore)
                        ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
                    const int32 debug = (m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeDebug) ? WGL_CONTEXT_DEBUG_BIT_ARB : 0;

                    attributes.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
                    attributes.push_back(profile);
                    attributes.push_back(WGL_CONTEXT_FLAGS_ARB);
                    attributes.push_back(debug);
                }
                else {
                    if ((m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeCore) ||
                        (m_ContextInfo.pixelFormat.attributeFlags & GraphicsAttribute::AttributeDebug)) {
                        Log_nts.Error("Selecting a profile during context creation is not supported, disabling compatibility and debug");
                    }

                    m_ContextInfo.pixelFormat.attributeFlags = GraphicsAttribute::AttributeDefault;
                }

                attributes.push_back(0);
                attributes.push_back(0);

                context = wglCreateContextAttribsARB(deviceContext, sharedContext, attributes.data());
            }
            else {
                break;
            }
            if (!context) {
                if (m_ContextInfo.pixelFormat.attributeFlags != GraphicsAttribute::AttributeDefault) {
                    m_ContextInfo.pixelFormat.attributeFlags = GraphicsAttribute::AttributeDefault;
                }
                else if (m_ContextInfo.version.minor > 0) {
                    --m_ContextInfo.version.minor;

                    m_ContextInfo.pixelFormat.attributeFlags = m_ContextInfo.pixelFormat.attributeFlags;
                }
                else {
                    --m_ContextInfo.version.major;
                    m_ContextInfo.version.minor = 9;

                    m_ContextInfo.pixelFormat.attributeFlags = m_ContextInfo.pixelFormat.attributeFlags;
                }
            }
        }
        if (context == nullptr){
            Log_nts.Error("Cannot create opengl context : Version = {0}", m_ContextInfo.version);
        }
        return context;
    }*/
}    // namespace nkentseu

#endif