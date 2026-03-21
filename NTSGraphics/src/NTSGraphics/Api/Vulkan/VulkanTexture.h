//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-16 at 11:23:53 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __VULKAN_TEXTURE_H__
#define __VULKAN_TEXTURE_H__

#pragma once

#include <vulkan/vulkan.hpp>

#include <NTSCore/System.h>
#include "NTSGraphics/Core/Texture.h"
#include "NTSGraphics/Core/ShaderInputLayout.h"
#include "VulkanContext.h"
#include "VulkanShaderInputLayout.h"

#include <vector>
#include <unordered_map>

namespace nkentseu {
	class VulkanTexture2D;

	using T2DBindingCreateCallBackFn = std::function<bool(bool, VulkanTexture2D*)>;
	using T2DBindingDestroyCallBackFn = T2DBindingCreateCallBackFn;
	#define T2D_BINDING_CALLBACK_FN(method) std::bind(&method, this, STDPH(1), STDPH(2))
    
	class NKENTSEU_API VulkanTexture2D : public Texture2D {
	public:
		VulkanTexture2D(Memory::Shared<Context> context);
		~VulkanTexture2D();

		bool Create(TextureFormat textureFormat, const maths::Vector2u& size, uint32 channels = 4) override;
		bool Create(TextureFormat textureFormat, const std::filesystem::path& filename) override;
		bool Create(TextureFormat textureFormat, const Image& image) override;
		bool Create(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels = 4) override;

		bool Destroy() override;

		bool                Update(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size) override;
		bool                Update(const Image& image, const maths::Vector2i& offset = maths::Vector2i()) override;
		bool                Update(const Texture2D& texture, const maths::Vector2i& offset = maths::Vector2i()) override;
		maths::Vector2u     GetSize() const override;
		uint8*				ExtractDatas(maths::Vector2u* size) override;
		Image&				GetImage() override;
		const Image&		GetImage() const override;
		void                SetSmooth(bool smooth) override;
		bool                IsSmooth() const override;
		TextureFormat       GetTextureFormat() const override;
		void                SetRepeated(bool repeated) override;
		bool                IsRepeated() const override;
		bool                GenerateMipmap() override;
		void                Swap(Texture& right) noexcept override;
		void                InvalidateMipmap() override;

		const std::filesystem::path& GetPath() const override;

		static uint32 GetMaximumSize(Memory::Shared<Context> context);
		static uint32 GetValidSize(Memory::Shared<Context> context, uint32 size);

	private:
		Memory::Shared<VulkanContext> m_Context;
		TextureFormat m_Format = TextureFormat::Enum::RGBA8;

		std::filesystem::path m_Path;
		Image m_Image;
		maths::Vector2u m_Size;
		uint32 m_Channels;

		bool m_IsRepeated = true;
		bool m_IsSmooth = false;
		bool m_IsCreated = false;

		bool CreateInternal(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels = 4);
		bool UpdateInternal(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size, const maths::Vector2u& nSize);
		bool DestroyInternal();
		Image ExtractDatasInternal();
		bool SizeIsValid(const maths::Vector2i& offset, const maths::Vector2u& size);
		bool GetValidSize(maths::Vector2i& offset, maths::Vector2u& size);

		// vulkan image resource
		vk::Image image = nullptr;
		vk::DeviceMemory imageMemory = nullptr;
		vk::ImageView imageView = nullptr;
		uint32 mipLevels = 1;
		vk::Sampler sampler = nullptr;

		bool CreateImage(vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags prop);
		bool DestroyImage();
		bool CreateImageView();
		bool DestroyImageView();
		bool CreateSampler();
		bool DestroySampler();

		// tools
		void TransitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32 mipL);
		void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32 width, uint32 height, const maths::Vector2i& offset);
		void CopyImageToBuffer(vk::Image image, vk::Buffer buffer, uint32 width, uint32 height);

		NkUnorderedMap<std::string, T2DBindingCreateCallBackFn> bindingCreate;
		NkUnorderedMap<std::string, T2DBindingDestroyCallBackFn> bindingDestroy;

		const std::string& AddBinding(T2DBindingCreateCallBackFn creat, T2DBindingCreateCallBackFn destroy);
		void RemoveBinding(const std::string& id);
		bool CreateAllBinding(bool value);
		bool DestroyAllBinding(bool value);

		friend class VulkanTexture2DBinding;
	};

	class NKENTSEU_API VulkanTexture2DBinding : public Texture2DBinding {
	public:

		VulkanTexture2DBinding(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
		bool Initialize(Memory::Shared<Texture2D> texture) override;
		bool Initialize(VulkanTexture2D* texture);
		bool Destroy() override;
		bool Bind(const std::string& name) override;
		bool Bind(uint32 binding) override;
		bool Equal(Memory::Shared<Texture2DBinding> binding) override;
		bool IsDefined(Memory::Shared<Texture2D> binding) override;
		bool IsValide() override;

	private:
		bool icreate(bool, VulkanTexture2D*);
		bool idestroy(bool, VulkanTexture2D*);
		int32 index = -1;

		friend class VulkanTexture2D;
		VulkanTexture2D* texture = nullptr;
		Memory::Shared<VulkanContext> m_Context;
		Memory::Shared<VulkanShaderInputLayout> m_Vksil = nullptr;

		TextureDescriptorSet m_DescriptorSets;
		bool isCreated = false;
		bool isFirst = true;
		std::string id;
	};

}  //  nkentseu

#endif  // __VULKAN_TEXTURE_H__!