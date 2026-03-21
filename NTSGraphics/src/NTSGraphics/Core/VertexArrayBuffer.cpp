//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-16 at 10:10:05 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VertexArrayBuffer.h"

#include "NTSGraphics/Api/Opengl/OpenglVertexArrayBuffer.h"
#include "NTSGraphics/Api/Vulkan/VulkanVertexArrayBuffer.h"
#include "Context.h"

namespace nkentseu {

	Memory::Shared<VertexArrayBuffer> VertexArrayBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil)
    {
		auto vertexbuffer = CreatePrivate(context, sil);

		if (vertexbuffer == nullptr || !vertexbuffer->Create()) {
			Memory::Reset(vertexbuffer);
			return nullptr;
		}

		return vertexbuffer;
    }

	Memory::Shared<VertexArrayBuffer> VertexArrayBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, uint32 vertexNumber)
    {
		auto vertexbuffer = CreatePrivate(context, sil);

		if (vertexbuffer == nullptr || !vertexbuffer->Create(vertexNumber)) {
			Memory::Reset(vertexbuffer);
			return nullptr;
		}

		return vertexbuffer;
    }

	Memory::Shared<VertexArrayBuffer> VertexArrayBuffer::CreatePrivate(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil)
	{
		if (context == nullptr) {
			return nullptr;
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			return Memory::AllocateShared<VulkanVertexArrayBuffer>(context, sil);
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			return Memory::AllocateShared<OpenglVertexArrayBuffer>(context, sil);
		}

		return nullptr;
	}

}  //  nkentseu