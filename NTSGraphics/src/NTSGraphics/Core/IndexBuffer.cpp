//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-05-19 at 10:46:54 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "IndexBuffer.h"

#include "NTSGraphics/Api/Opengl/OpenglIndexBuffer.h"
#include "NTSGraphics/Api/Vulkan/VulkanIndexBuffer.h"

#include "Context.h"

namespace nkentseu {

	Memory::Shared<IndexBuffer> IndexBuffer::Create(Memory::Shared<Context> context)
	{
		if (context == nullptr) return nullptr;

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			return Memory::AllocateShared<VulkanIndexBuffer>(context);
		}

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			return Memory::AllocateShared<OpenglIndexBuffer>(context);
		}

		return nullptr;
	}

	Memory::Shared<IndexBuffer> IndexBuffer::Create(Memory::Shared<Context> context, BufferUsageType bufferUsage, const std::vector<uint32>& indices)
    {
		auto index = Create(context);
		if (index == nullptr || !index->Create(bufferUsage, indices)) {
			Memory::Reset(index);
		}
		return index;
    }

    Memory::Shared<IndexBuffer> IndexBuffer::Create(Memory::Shared<Context> context, BufferUsageType bufferUsage, IndexBufferType indexType, const void* indices, uint32 leng)
    {
		auto index = Create(context);
		if (index == nullptr || !index->Create(bufferUsage, indexType, indices, leng)) {
			Memory::Reset(index);
		}
		return index;
    }

}  //  nkentseu