//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:46:43 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VertexBuffer.h"

#include "NTSGraphics/Api/Vulkan/VulkanVertexBuffer.h"
#include "NTSGraphics/Api/Opengl/OpenglVertexBuffer.h"

namespace nkentseu {

	Memory::Shared<VertexBuffer> VertexBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) {
		if (context == nullptr) {
			return nullptr;
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			return Memory::AllocateShared<VulkanVertexBuffer>(context, sil);
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			return Memory::AllocateShared<OpenglVertexBuffer>(context, sil);
		}

		return nullptr;
	}

	Memory::Shared<VertexBuffer> VertexBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, BufferUsageType bufferUsage, const void* vertices, uint32 leng)
    {
		auto vertexbuffer = Create(context, sil);

		if (vertexbuffer == nullptr || !vertexbuffer->Create(bufferUsage, vertices, leng)) {
			Memory::Reset(vertexbuffer);
			return nullptr;
		}

		return vertexbuffer;
    }

	Memory::Shared<VertexBuffer> VertexBuffer::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, BufferUsageType bufferUsage, const std::vector<float32>& vertices)
    {
		auto vertexbuffer = Create(context, sil);

		if (vertexbuffer == nullptr || !vertexbuffer->Create(bufferUsage, vertices)) {
			Memory::Reset(vertexbuffer);
			return nullptr;
		}

		return vertexbuffer;
    }
}  //  nkentseu