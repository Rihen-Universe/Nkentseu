//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-06-16 at 11:22:57 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "Texture.h"
#include <NTSLogger/Formatter.h>

#include "NTSGraphics/Api/Opengl/OpenglTexture.h"
#include "NTSGraphics/Api/Vulkan/VulkanTexture.h"

#include <glad/gl.h>

namespace nkentseu {
	using namespace maths;

	uint32 Texture2D::GetMaximumSize(Memory::Shared<Context> context)
	{
		if (context == nullptr) return 0;

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			return VulkanTexture2D::GetMaximumSize(context);
		}
		else if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			return OpenglTexture2D::GetMaximumSize(context);
		}
		return uint32();
	}

	uint32 Texture2D::GetValidSize(Memory::Shared<Context> context, uint32 size)
	{
		if (context == nullptr) return 0;

		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			return VulkanTexture2D::GetValidSize(context, size);
		}
		else if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			return OpenglTexture2D::GetValidSize(context, size);
		}
		return uint32();
	}

	Memory::Shared<Texture2D> Texture2D::Create(Memory::Shared<Context> context)
	{
		if (context == nullptr) return nullptr;

		Memory::Shared<Texture2D> texture = nullptr;
		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			texture = Memory::AllocateShared<VulkanTexture2D>(context);
		}
		else if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			texture = Memory::AllocateShared<OpenglTexture2D>(context);
		}
		return texture;
	}

	Memory::Shared<Texture2D> Texture2D::Create(Memory::Shared<Context> context, TextureFormat textureFormat, const maths::Vector2u& size)
	{
		Memory::Shared<Texture2D> texture = Create(context);

		if (texture == nullptr || !texture->Create(textureFormat, size)) {
			Memory::Reset(texture);
		}
		return texture;
	}

	Memory::Shared<Texture2D> Texture2D::Create(Memory::Shared<Context> context, TextureFormat textureFormat, const std::filesystem::path& filename)
	{
		Memory::Shared<Texture2D> texture = Create(context);

		if (texture == nullptr || !texture->Create(textureFormat, filename)) {
			Memory::Reset(texture);
		}
		return texture;
	}

	Memory::Shared<Texture2D> Texture2D::Create(Memory::Shared<Context> context, TextureFormat textureFormat, const Image& image)
	{
		Memory::Shared<Texture2D> texture = Create(context);

		if (texture == nullptr || !texture->Create(textureFormat, image)) {
			Memory::Reset(texture);
		}
		return texture;
	}

	Memory::Shared<Texture2D> Texture2D::Create(Memory::Shared<Context> context, TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels)
	{
		Memory::Shared<Texture2D> texture = Create(context);

		if (texture == nullptr || !texture->Create(textureFormat, data, size, channels)) {
			Memory::Reset(texture);
		}
		return texture;
	}

	Memory::Shared<Texture2DBinding> Texture2DBinding::Create(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil)
	{
		Memory::Shared<Texture2DBinding> textureBinding = nullptr;
		if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::VulkanApi) {
			textureBinding = Memory::AllocateShared<VulkanTexture2DBinding>(context, sil);
		}
		else if (context->GetProperties().graphicsApi == GraphicsApiType::Enum::OpenglApi) {
			textureBinding = Memory::AllocateShared<OpenglTexture2DBinding>(context, sil);
		}
		return textureBinding;
	}

	Memory::Shared<Texture2DBinding> Texture2DBinding::CreateInitialize(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil, Memory::Shared<Texture2D> texture)
	{
		Memory::Shared<Texture2DBinding> textureBinding = Create(context, sil);

		if (textureBinding == nullptr || !textureBinding->Initialize(texture)) {
			Memory::Reset(textureBinding);
		}
		return textureBinding;
	}

}  //  nkentseu