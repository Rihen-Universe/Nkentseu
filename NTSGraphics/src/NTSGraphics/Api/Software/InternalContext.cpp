//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/3/2024 at 12:51:35 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "InternalContext.h"


#ifdef NKENTSEU_GRAPHICS_API_SOFTWARE

namespace nkentseu {

    InternalContext::InternalContext() : m_Window(nullptr){
    }

    InternalContext::~InternalContext(){
    }

    bool nkentseu::InternalContext::SetWindow(Window* window)
    {
        return false;
    }

    bool InternalContext::SetProperties(const ContextProperties& properties)
    {
        return false;
    }

    bool InternalContext::Initialize()
    {
        return false;
    }

    bool InternalContext::Initialize(Window* window, const ContextProperties& contextProperties)
    {
        return false;
    }

    bool InternalContext::Deinitialize()
    {
        return false;
    }

    bool InternalContext::IsInitialize()
    {
        return false;
    }

    bool InternalContext::MakeCurrent()
    {
        return false;
    }

    bool InternalContext::UnmakeCurrent()
    {
        return false;
    }

    bool InternalContext::IsCurrent()
    {
        return false;
    }

    bool InternalContext::EnableVSync()
    {
        return false;
    }

    bool InternalContext::DisableVSync()
    {
        return false;
    }

    bool InternalContext::Present()
    {
        return false;
    }

    bool InternalContext::Swapchaine()
    {
        return false;
    }

    const GraphicsInfos& InternalContext::GetGraphicsInfo()
    {
        static const GraphicsInfos graphicsInfos = {}; 
        return graphicsInfos;
    }

    Memory::Shared<NativeContext> InternalContext::GetNativeContext() {
        return m_NativeContext;
    }

    Window* InternalContext::GetWindow() {
        return nullptr;
    }

    const ContextProperties& InternalContext::GetProperties()
    {
        static const ContextProperties contextProperties = {}; 
        return contextProperties;
    }

}    // namespace nkentseu

#endif