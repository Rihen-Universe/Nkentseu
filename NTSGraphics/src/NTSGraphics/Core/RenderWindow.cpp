//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-08 at 05:28:16 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "RenderWindow.h"
#include <NTSLogger/Formatter.h>
#include "NTSGraphics/Api/Vulkan/VulkanRenderWindow.h"
#include "NTSGraphics/Api/Opengl/OpenglRenderWindow.h"

namespace nkentseu {

    Memory::Shared<RenderWindow> RenderWindow::Create(Memory::Shared<Context> context)
    {
        if (context == nullptr) {
            return nullptr;
        }

        if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
            return Memory::AllocateShared<VulkanRenderWindow>(context);
        }

        if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
            return Memory::AllocateShared<OpenglRenderWindow>(context);
        }

        return nullptr;
    }

    Memory::Shared<RenderWindow> RenderWindow::CreateInitialized(Memory::Shared<Context> context, const maths::Vector2f& size, const maths::Vector2f& dpiSize)
    {
        auto renderWindow = Create(context);

        if (renderWindow == nullptr || !renderWindow->Initialize(size, dpiSize)) {
            Memory::Reset(renderWindow);
        }

        return renderWindow;
    }

}  //  nkentseu