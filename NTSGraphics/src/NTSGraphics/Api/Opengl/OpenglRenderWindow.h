//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-08 at 05:29:07 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_RENDER_WINDOW_H__
#define __OPENGL_RENDER_WINDOW_H__

#pragma once

#include <NTSCore/System.h>
#include "NTSGraphics/Core/RenderWindow.h"
#include "OpenglContext.h"

namespace nkentseu {
    
    class NKENTSEU_API OpenglRenderWindow : public RenderWindow {
        public:
            OpenglRenderWindow(Memory::Shared<Context> context);
            ~OpenglRenderWindow();

            Memory::Shared<Context> GetContext() override;

            bool Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize) override;
            bool Deinitialize() override;

            bool Begin(const maths::Color& color) override;
            bool Begin(uint8 r, uint8 g, uint8 b, uint8 a = 255) override;
            bool End() override;

            bool SetViewport(const maths::Vector4f& viewport) override;
            bool SetViewport(float32 x, float32 y, float32 width, float32 height) override;
            bool ResetViewport() override;

            bool EnableBlending(bool enabled) override;

            bool EnableDepthTest(bool enabled) override;
            bool EnableScissorTest(bool enabled) override;

            bool SetScissor(const maths::Vector4f& scissor) override;
            bool SetScissor(float32 x, float32 y, float32 width, float32 height) override;
            bool ResetScissor() override;

            bool SetPolygonMode(PolygonModeType mode) override;
            bool SetCullMode(CullModeType mode) override;
            bool SetFrontFaceMode(FrontFaceType mode) override;
            bool SetRenderPrimitive(RenderPrimitive mode) override;

            //bool OnWindowResized(const maths::Vector2f& size, const maths::Vector2f& dpiSize) override;
        private:
            Memory::Shared<OpenglContext> m_Context = nullptr;
            bool m_IsPrepare = false;
            maths::Vector2f size;
            maths::Vector2f dpiSize;
    };

}  //  nkentseu

#endif  // __OPENGL_RENDER_WINDOW_H__!