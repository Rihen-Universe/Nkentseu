//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/5/2024 at 3:12:38 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_RENDERER_H__
#define __NKENTSEU_RENDERER_H__

#pragma once

#include "NTSCore/System.h"
#include "Context.h"
#include "Shader.h"

#include "NTSMaths/Color.h"
#include <NTSMaths/Vector/Vector2.h>
#include <NTSMaths/Vector/Vector4.h>

#include "UniformBuffer.h"

namespace nkentseu {
    class VertexArray;

    class NKENTSEU_API Renderer
    {
        public:
            virtual Memory::Shared<Context> GetContext() = 0;

            virtual bool Initialize(const maths::Vector2f& size, const maths::Vector2f& dpiSize) = 0;
            virtual bool Deinitialize() = 0;

            virtual bool Begin(const maths::Color& color) = 0;
            virtual bool Begin(uint8 r, uint8 g, uint8 b, uint8 a = 255) = 0;
            virtual bool End() = 0;

            virtual bool SetViewport(const maths::Vector4f& viewport) = 0;
            virtual bool SetViewport(float32 x, float32 y, float32 width, float32 height) = 0;
            virtual bool ResetViewport() = 0;

            virtual bool EnableBlending(bool enabled) = 0;

            virtual bool EnableDepthTest(bool enabled) = 0;
            virtual bool EnableScissorTest(bool enabled) = 0;

            virtual bool SetScissor(const maths::Vector4f& scissor) = 0;
            virtual bool SetScissor(float32 x, float32 y, float32 width, float32 height) = 0;
            virtual bool ResetScissor() = 0;

            virtual bool SetPolygonMode(PolygonModeType mode) = 0;
            virtual bool SetCullMode(CullModeType mode) = 0;
            virtual bool SetFrontFaceMode(FrontFaceType mode) = 0;
            virtual bool SetRenderPrimitive(RenderPrimitive mode) = 0;
            //virtual bool OnWindowResized(const maths::Vector2f& size, const maths::Vector2f& dpiSize) = 0;
    };
} // namespace nkentseu

#endif    // __NKENTSEU_RENDERER_H__