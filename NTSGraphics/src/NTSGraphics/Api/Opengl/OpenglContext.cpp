//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/3/2024 at 12:51:22 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//
#include "pch/ntspch.h"
#include "OpenglContext.h"
#include <glad/gl.h>

#ifdef NKENTSEU_PLATFORM_WINDOWS
#include <NTSGraphics/Platform/Windows/WGLContext.h>
#elif defined NKENTSEU_PLATFORM_LINUX
    #include <GL/glext.h>
    #if defined(NKENTSEU_PLATFORM_LINUX_XCB)
    #include <Nkentseu/Platform/Window/Linux/XCB/XGLContext.h>
    #elif defined(NKENTSEU_PLATFORM_LINUX_XLIB)
    #include <Nkentseu/Platform/Window/Linux/XLIB/XGLContext.h>
    #endif
#endif

#include "OpenGLUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
    void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length, const char* message, const void* userParam)
    {
        // ignore non-significant error/warning codes
        if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;
        std::string source_str;
        std::string type_str;

        switch (source)
        {
        case GL_DEBUG_SOURCE_API:
            source_str = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            source_str = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            source_str = "Shader Compile"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            source_str = "Third party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:
            source_str = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER:
            source_str = "Other"; break;
        }

        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR:
            type_str = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type_str = "Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type_str = "Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:
            type_str = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            type_str = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:
            type_str = "Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:
            type_str = "Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:
            type_str = "Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:
            type_str = "Other"; break;
        }

        std::string file = OpenGLStaticDebugInfo::file_call;
        std::string methode = OpenGLStaticDebugInfo::methode_call;
        uint32 line = OpenGLStaticDebugInfo::line_call;

        OpenGLStaticDebugInfo::success = false;

        if (severity == GL_DEBUG_SEVERITY_HIGH) {
            std::string message = FORMATTER.Format("source : {0}({1}), type : {2}({3})", source_str, source, type_str, type);
            LogBaseReset(GraphicsNameLogger, file.c_str(), line, methode.c_str()).Fatal("{0}", message);
            return;
        }
        else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
            std::string message = FORMATTER.Format("source : {0}({1}), type : {2}({3})", source_str, source, type_str, type);
            LogBaseReset(GraphicsNameLogger, file.c_str(), line, methode.c_str()).Error("{0}", message);
            return;
        }
        else if (severity == GL_DEBUG_SEVERITY_LOW) {
            std::string message = FORMATTER.Format("source : {0}({1}), type : {2}({3})", source_str, source, type_str, type);
            LogBaseReset(GraphicsNameLogger, file.c_str(), line, methode.c_str()).Warning("{0}", message);
            return;
        }
        else if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
            //NkentseuTrace::Instance().GetLog()->Details(file.c_str(), line, methode.c_str(), nkentseu::Date::GetCurrent(), nkentseu::Time::GetCurrent()).Warning("{0}", message);
            //return;
        }

        OpenGLStaticDebugInfo::success = true;
    }

    OpenglContext::OpenglContext(WindowInfo* window, const ContextProperties& contextProperties) : m_Window(window), m_Properties(contextProperties){
        m_NativeContext = Memory::AllocateShared<NativeContext>();

        if (m_NativeContext == nullptr) return;

        if (!m_NativeContext->SetWindowInfo(m_Window)) {
            return;
        }

        if (!m_NativeContext->SetProperties(m_Properties)) {
            return;
        }
    }

    OpenglContext::~OpenglContext(){
    }

    bool OpenglContext::Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize)
    {
        if (m_NativeContext == nullptr) return false;

        this->size = size;
        this->dpiSize = dpiSize;

        if (m_NativeContext->Initialize()) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            return true;
        }
        return false;
    }

    bool OpenglContext::Deinitialize()
    {
        if (m_NativeContext == nullptr) return false;
        bool deinit = m_NativeContext->Deinitialize();
        if (deinit) {
            Memory::Reset(m_NativeContext);
        }
        return deinit;
    }

    bool OpenglContext::IsInitialize()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->IsInitialize();
    }

    bool OpenglContext::MakeCurrent()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->MakeCurrent();
    }

    bool OpenglContext::UnmakeCurrent()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->UnmakeCurrent();
    }

    bool OpenglContext::IsCurrent()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->IsCurrent();
    }

    bool OpenglContext::EnableVSync()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->EnableVSync();
    }

    bool OpenglContext::DisableVSync()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->DisableVSync();
    }

    const maths::Vector2f& OpenglContext::GetSize()
    {
        return size;
    }

    const maths::Vector2f& OpenglContext::GetDpiSize()
    {
        return dpiSize;
    }

    bool OpenglContext::Swapchaine()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->Swapchaine();
    }

    const GraphicsInfos& OpenglContext::GetGraphicsInfo()
    {
        static const GraphicsInfos graphicsInfos = {}; 
        if (m_NativeContext == nullptr) return graphicsInfos;
        return m_NativeContext->GetGraphicsInfo();
    }

    bool OpenglContext::OnWindowResized(const maths::Vector2f& size, const maths::Vector2f& dpiSize)
    {
        if (m_NativeContext == nullptr) return false;
        this->size = size;
        this->dpiSize = dpiSize;
        return true;
    }

    Memory::Shared<NativeContext> OpenglContext::GetNative() {
        return m_NativeContext;
    }

    WindowInfo* OpenglContext::GetWindowInfo() {
        return m_Window;
    }

    const ContextProperties& OpenglContext::GetProperties()
    {
        return m_Properties;
    }

    bool OpenglContext::IsValidContext()
    {
        if (m_NativeContext == nullptr) return false;
        return m_NativeContext->IsInitialize();
    }

    bool OpenglContext::Present()
    {
        if (m_NativeContext == nullptr) return false;
        bool swap = m_NativeContext->Swapchaine();

        if (swap) {
            OpenGLResult result;
            bool first = true;

            glCheckError(first, result, glFlush(), "cannot flush");
            swap = result.success;
        }

        return swap;
    }

}    // namespace nkentseu