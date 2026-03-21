//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 5/3/2024 at 12:48:26 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Context.h"

#include "NTSGraphics/Api/Vulkan/VulkanContext.h"
#include "NTSGraphics/Api/Opengl/OpenglContext.h"

namespace nkentseu {

	Memory::Shared<Context> Context::Create(WindowInfo* window, const ContextProperties& contextProperties)
	{
		if (window == nullptr) return nullptr;
		
		if (contextProperties.graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			auto context = Memory::AllocateShared<VulkanContext>(window, contextProperties);
			if (context != nullptr) return context;
			Memory::Reset(context);
		} else if (contextProperties.graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			auto context = Memory::AllocateShared<OpenglContext>(window, contextProperties);
			if (context != nullptr) return context;
			Memory::Reset(context);
		}

		return nullptr;
	}

	Memory::Shared<Context> Context::CreateInitialized(WindowInfo* window, const ContextProperties& contextProperties, const maths::Vector2f& size, const maths::Vector2f& dpiSize)
	{
		auto context = Create(window, contextProperties);
		if (context != nullptr && context->Initialize(size, dpiSize)) {
			return context;
		}
		Memory::Reset(context);
		return nullptr;
	}
}    // namespace nkentseu