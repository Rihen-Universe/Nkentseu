//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-16 at 11:23:31 AM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __OPENGL_TEXTURE_H__
#define __OPENGL_TEXTURE_H__

#pragma once

#include <NTSCore/System.h>

#include "NTSGraphics/Core/Context.h"
#include "NTSGraphics/Core/Texture.h"
#include "OpenglContext.h"

#include <vector>
#include <functional>
#include <unordered_map>

namespace nkentseu {
	class OpenglTexture2D;

	using T2DBindingCreateCallBackFn2 = std::function<bool(bool, OpenglTexture2D*)>;
	using T2DBindingDestroyCallBackFn2 = T2DBindingCreateCallBackFn2;
	#define T2D_BINDING_CALLBACK_FN2(method) std::bind(&method, this, STDPH(1), STDPH(2))
    
    class NKENTSEU_API OpenglTexture2D : public Texture2D {
    public:
		OpenglTexture2D(Memory::Shared<Context> context);
        ~OpenglTexture2D();

		bool Create(TextureFormat textureFormat, const maths::Vector2u& size, uint32 channels = 4) override;
		bool Create(TextureFormat textureFormat, const std::filesystem::path& filename) override;
		bool Create(TextureFormat textureFormat, const Image& image) override;
		bool Create(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels = 4) override;

		bool Destroy() override;

		bool                Update(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size) override;
		bool                Update(const Image& image, const maths::Vector2i& offset = maths::Vector2i()) override;
		bool                Update(const Texture2D& texture, const maths::Vector2i& offset = maths::Vector2i()) override;
		maths::Vector2u     GetSize() const override;
		uint8* ExtractDatas(maths::Vector2u* size) override;
		Image& GetImage() override;
		const Image& GetImage() const override;
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
		friend class OpenglTexture2DBinding;

        Memory::Shared<OpenglContext> m_Context;
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
		bool GetValidSize(maths::Vector2i* offset, maths::Vector2u* size);


		uint32 m_Handle = 0;
		bool m_HasMipmap = false;

		NkUnorderedMap<std::string, T2DBindingCreateCallBackFn2> bindingCreate = {};
		NkUnorderedMap<std::string, T2DBindingDestroyCallBackFn2> bindingDestroy = {};

		const std::string& AddBinding(T2DBindingCreateCallBackFn2 creat, T2DBindingCreateCallBackFn2 destroy);
		void RemoveBinding(const std::string& id);
		bool CreateAllBinding(bool value);
		bool DestroyAllBinding(bool value);

		friend class VulkanTexture2DBinding;
    };

	class NKENTSEU_API OpenglTexture2DBinding : public Texture2DBinding {
	public:
		OpenglTexture2DBinding(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil);
		bool Initialize(Memory::Shared<Texture2D> texture) override;
		bool Destroy() override;
		bool Bind(const std::string& name) override;
		bool Bind(uint32 binding) override;
		bool Equal(Memory::Shared<Texture2DBinding> binding) override;
		bool IsDefined(Memory::Shared<Texture2D> binding) override;
		bool IsValide() override;

	private:
		uint32 m_Handle = 0;
		Memory::Shared<OpenglContext> m_Context;
		SamplerInputLayout m_InputLayout;
	};

}  //  nkentseu

#endif  // __OPENGL_TEXTURE_H__!