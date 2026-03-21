//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-03 at 08:32:23 PM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "UniformBuffer.h"
#include <NTSLogger/Formatter.h>

#include "NTSGraphics/Api/Vulkan/VulkanUniformBuffer.h"
#include "NTSGraphics/Api/Opengl/OpenglUniformBuffer.h"
#include "Log.h"

namespace nkentseu {

	Memory::Shared<UniformBuffer> UniformBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil)
	{
		if (context == nullptr) {
			return nullptr;
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			return Memory::AllocateShared<VulkanUniformBuffer>(context, sil);
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			return Memory::AllocateShared<OpenglUniformBuffer>(context, sil);
		}

		return nullptr;
	}

	Memory::Shared<UniformBuffer> UniformBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, Memory::Shared<Shader> shader)
	{
		auto uniform = Create(context, sil);

		if (shader == nullptr) {
			gLog.Fatal();
		}

		if (uniform == nullptr || !uniform->Create(shader)) {

			if (uniform != nullptr) {
				uniform->Destroy();
			}

			Memory::Reset(uniform);
			return nullptr;
		}

		return uniform;
	}

}  //  nkentseu