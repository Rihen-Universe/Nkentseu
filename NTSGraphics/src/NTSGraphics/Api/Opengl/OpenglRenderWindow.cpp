//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-08 at 05:29:07 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenglRenderWindow.h"

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

namespace nkentseu {
    using namespace maths;

    OpenglRenderWindow::OpenglRenderWindow(Memory::Shared<Context> context) : m_Context(Memory::Cast<OpenglContext>(context)) {
    }

    OpenglRenderWindow::~OpenglRenderWindow() {
    }

    Memory::Shared<Context> OpenglRenderWindow::GetContext()
    {
        return m_Context;
    }

    bool OpenglRenderWindow::Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize)
    {
        if (m_Context == nullptr) {
            return false;
        }

        if (!m_Context->IsInitialize()) {
            return m_Context->Initialize(size, dpiSize);
        }

        bool makecurrent = true;

        if (!m_Context->GetNative()->IsCurrent()) {
            makecurrent = m_Context->GetNative()->MakeCurrent();
        }

        this->size = m_Context->size;
        this->dpiSize = m_Context->dpiSize;

        return makecurrent;
    }

    bool OpenglRenderWindow::Deinitialize()
    {
        if (m_Context == nullptr || !m_Context->IsInitialize()) {
            return false;
        }

        bool makecurrent = false;
        if (m_Context->GetNative()->IsCurrent()) {
            makecurrent = m_Context->GetNative()->UnmakeCurrent();
        }
        return false;
    }

    bool OpenglRenderWindow::Begin(const Color& color)
    {
        if (m_Context == nullptr) {
            return false;
        }

        m_IsPrepare = false;
        ResetViewport();

        if (!m_Context->IsCurrent()) {
            m_Context->MakeCurrent();

            if (!m_Context->IsCurrent()) {
                return false;
            }
        }

        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glClearColor(color.r, color.g, color.b, color.a), "cannot clear color");
        if (result.success) {
            static bool enableDepth = false;
            static uint32 clear_infos = 0;

            if (!enableDepth) {
                glEnable(GL_DEPTH_TEST);
                enableDepth = true;
            }

            if (enableDepth) clear_infos |= GL_DEPTH_BUFFER_BIT;
            clear_infos |= GL_COLOR_BUFFER_BIT;
            clear_infos |= GL_STENCIL_BUFFER_BIT;

            glCheckError(first, result, glClear(clear_infos), "cannot clear");
            if (result.success) {
                m_IsPrepare = true;
            }
        }
        return result.success;
    }

    bool OpenglRenderWindow::Begin(uint8 r, uint8 g, uint8 b, uint8 a)
    {
        return Begin(Color(r, g, b, a));
    }

    bool OpenglRenderWindow::End()
    {
        if (m_Context == nullptr) {
            return false;
        }
        return true;
    }

    bool OpenglRenderWindow::SetViewport(const maths::Vector4f& viewport)
    {
        return SetViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    }

    bool OpenglRenderWindow::SetViewport(float32 x, float32 y, float32 width, float32 height)
    {
        if (m_Context == nullptr) return false;
        OpenGLResult result;
        bool first = true;

        glCheckError(first, result, glViewport(x, y, width, height), "cannot change viewport");
        return true;
    }

    bool OpenglRenderWindow::ResetViewport()
    {
        if (m_Context == nullptr) return false;
        OpenGLResult result;
        bool first = true;

        this->size = m_Context->size;
        this->dpiSize = m_Context->dpiSize;

        glCheckError(first, result, glViewport(0, 0, this->dpiSize.width, this->dpiSize.height), "cannot change viewport");
        return false;
    }

    bool OpenglRenderWindow::EnableBlending(bool enabled)
    {
        if (m_Context == nullptr) {
            return false;
        }
        OpenGLResult result;
        bool first = true;

        if (enabled) {
            glCheckError(first, result, glEnable(GL_BLEND), "cannot enable depth test");
            glCheckError(first, result, glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), "cannot enable depth test");
        }
        else {
            glCheckError(first, result, glDisable(GL_BLEND), "cannot disable depth test");
        }
        return result.success;
    }

    bool OpenglRenderWindow::EnableDepthTest(bool enabled)
    {
        if (m_Context == nullptr) {
            return false;
        }
        OpenGLResult result;
        bool first = true;
        if (enabled) {
            glCheckError(first, result, glEnable(GL_DEPTH_TEST), "cannot enable depth test");
        }
        else {
            glCheckError(first, result, glDisable(GL_DEPTH_TEST), "cannot disable depth test");
        }
        return result.success;
    }

    bool OpenglRenderWindow::EnableScissorTest(bool enabled)
    {
        if (m_Context == nullptr) {
            return false;
        }
        OpenGLResult result;
        bool first = true;
        if (enabled) {
            glCheckError(first, result, glEnable(GL_SCISSOR_TEST), "cannot enable scissor test");
        }
        else {
            glCheckError(first, result, glDisable(GL_SCISSOR_TEST), "cannot disable scissor test");
        }
        return result.success;
    }

    bool OpenglRenderWindow::SetScissor(const maths::Vector4f& scissor)
    {
        if (m_Context == nullptr) {
            return false;
        }
        OpenGLResult result;
        bool first = true;

        // Apply scissort
        glCheckError(first, result, glScissor(scissor.x, scissor.y, scissor.width, scissor.height), "cannot set command scissor in canvas fulsh");
        return result.result;
    }

    bool OpenglRenderWindow::SetScissor(float32 x, float32 y, float32 width, float32 height)
    {
        if (m_Context == nullptr) {
            return false;
        }
        OpenGLResult result;
        bool first = true;

        // Apply scissort
        glCheckError(first, result, glScissor(x, y, width, height), "cannot set command scissor in canvas fulsh");
        return result.result;
    }

    bool OpenglRenderWindow::ResetScissor()
    {
        if (m_Context == nullptr) {
            return false;
        }
        maths::Vector2f size = m_Context->dpiSize;
        return SetScissor(0, 0, size.width, size.height);
    }

    bool OpenglRenderWindow::SetPolygonMode(PolygonModeType mode)
    {
        return false;
    }

    bool OpenglRenderWindow::SetCullMode(CullModeType mode)
    {
        return false;
    }

    bool OpenglRenderWindow::SetFrontFaceMode(FrontFaceType mode)
    {
        return false;
    }

    bool OpenglRenderWindow::SetRenderPrimitive(RenderPrimitive mode)
    {
        return false;
    }

}  //  nkentseu