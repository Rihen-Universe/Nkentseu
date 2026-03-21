//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-16 at 11:23:31 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "OpenglTexture.h"
#include <glad/gl.h>
#include "OpenGLUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
	using namespace maths;

	// Compteur atomique pour générer des identifiants uniques
	static std::atomic<usize> textureUniqueIdCounter2(0);

	// Génère un identifiant unique
	static std::string GenerateTextureUniqueId2() {
		usize id = textureUniqueIdCounter2++;
		std::ostringstream oss;
		oss << id;
		return oss.str();
	}

	OpenglTexture2D::OpenglTexture2D(Memory::Shared<Context> context) : m_Context(Memory::Cast<OpenglContext>(context)), m_Handle(0)
	{
		bindingDestroy.clear();
		bindingCreate.clear();
	}

	OpenglTexture2D::~OpenglTexture2D()
	{
	}bool OpenglTexture2D::Create(TextureFormat textureFormat, const maths::Vector2u& size, uint32 channels) {
		if (SizeIsValid(maths::Vector2i(), size) && m_Image.Create(size, channels)) {
			return CreateInternal(textureFormat, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		return false;
	}

	bool OpenglTexture2D::Create(TextureFormat textureFormat, const std::filesystem::path& filename) {
		Image image;
		if (image.LoadFromFile(filename)) {
			return Create(textureFormat, image);
		}
		return false;
	}

	bool OpenglTexture2D::Create(TextureFormat textureFormat, const Image& image) {
		if (SizeIsValid(maths::Vector2i(), image.GetSize()) && m_Image.Create(image.GetSize(), image.GetPixels())) {
			return CreateInternal(textureFormat, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		return false;
	}

	bool OpenglTexture2D::Create(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels) {
		if (SizeIsValid(maths::Vector2i(), size) && m_Image.Create(size, data, channels)) {
			return CreateInternal(textureFormat, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		return false;
	}

	bool OpenglTexture2D::Destroy() {
		bool success = DestroyInternal();
		bindingDestroy.clear();
		bindingCreate.clear();
		return success;
	}

	bool OpenglTexture2D::Update(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size) {
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(size);
		if (GetValidSize(&offset__, &size__) && m_Image.Update(datas, offset, size)) {
			return UpdateInternal(datas, offset, size, size__);
		}
		return false;
	}

	bool OpenglTexture2D::Update(const Image& image, const maths::Vector2i& offset) {
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(image.GetSize());
		if (GetValidSize(&offset__, &size__) && m_Image.Update(image.GetPixels(), offset, image.GetSize())) {
			return UpdateInternal(image.GetPixels(), offset, image.GetSize(), size__);
		}
		return false;
	}

	bool OpenglTexture2D::Update(const Texture2D& texture, const maths::Vector2i& offset) {
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(texture.GetSize());
		if (GetValidSize(&offset__, &size__) && m_Image.Update(texture.GetImage().GetPixels(), offset, texture.GetSize())) {
			return UpdateInternal(texture.GetImage().GetPixels(), offset, texture.GetSize(), size__);
		}
		return false;
	}

	maths::Vector2u OpenglTexture2D::GetSize() const {
		return m_Size;
	}

	uint8* OpenglTexture2D::ExtractDatas(maths::Vector2u* size) {
		// create le stanging buffer
		// transfer l'image vers le stanging
		// extraire les donne du stanging

		//void* data;
		//vk::DeviceMemory stagingMemory = GetBufferMemory(stagingBuffer); // Mémoire associée
		//device.mapMemory(stagingMemory, 0, VK_WHOLE_SIZE, {}, &data);
		return nullptr;
	}

	Image& OpenglTexture2D::GetImage() {
		return m_Image;
	}

	const Image& OpenglTexture2D::GetImage() const
	{
		return m_Image;
	}

	void OpenglTexture2D::SetSmooth(bool smooth) {
		if (m_IsSmooth == smooth) return;

		m_IsSmooth = smooth;
		if (m_Handle) {

			OpenGLResult result;
			bool first = true;

			glCheckError(first, result, glBindTexture(GL_TEXTURE_2D, m_Handle), "");
			glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_MAG_FILTER, m_IsSmooth ? GL_LINEAR : GL_NEAREST), "");
			if (m_HasMipmap) {
				glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, m_IsSmooth ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST), "");
			}
			else {
				glCheckError(first, result, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_IsSmooth ? GL_LINEAR : GL_NEAREST), "");
			}
			glCheckError(first, result, glBindTexture(GL_TEXTURE_2D, 0), "");
		}
	}

	bool OpenglTexture2D::IsSmooth() const { return m_IsSmooth; }

	TextureFormat OpenglTexture2D::GetTextureFormat() const { return m_Format; }

	void OpenglTexture2D::SetRepeated(bool repeated) {
		if (m_IsRepeated == repeated) return;

		m_IsRepeated = repeated;

		if (m_Handle) {

			OpenGLResult result;
			bool first = true;

			glCheckError(first, result, glBindTexture(GL_TEXTURE_2D, m_Handle), "");
			glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_S, m_IsRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE), "");
			glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_T, m_IsRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE), "");
			glCheckError(first, result, glBindTexture(GL_TEXTURE_2D, 0), "");
		}
	}

	bool OpenglTexture2D::IsRepeated() const { return m_IsRepeated; }

	bool OpenglTexture2D::GenerateMipmap() {
		return false;
	}

	void OpenglTexture2D::Swap(Texture& right) noexcept {}

	void OpenglTexture2D::InvalidateMipmap() {}

	const std::filesystem::path& OpenglTexture2D::GetPath() const { return m_Path; }

	uint32 OpenglTexture2D::GetMaximumSize(Memory::Shared<Context> context)
	{

		OpenGLResult result;
		bool first = true;

		GLint maxTextureSize;
		glCheckError(first, result, glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize), "");
		return maxTextureSize;
	}

	uint32 OpenglTexture2D::GetValidSize(Memory::Shared<Context> context, uint32 size)
	{
		if (size <= GetMaximumSize(context)) {
			return size;
		}
		else {
			uint32 powerOfTwo = 1;
			while (powerOfTwo < GetMaximumSize(context))
				powerOfTwo *= 2;

			return powerOfTwo;
		}
	}

	bool OpenglTexture2D::CreateInternal(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels)
	{
		// Vérification de la validité de la taille
		if ((size.x == 0) || (size.y == 0)) {
			gLog.Error("Failed to create texture, invalid size ({0}x{1})", size.x, size.y);
			return false;
		}

		// Calcul des dimensions internes de la texture en fonction du support des textures NPOT
		const maths::Vector2u actualSize(GetValidSize(m_Context, size.x), GetValidSize(m_Context, size.y));

		// Vérification de la taille maximale de la texture
		const uint32 maxSize = GetMaximumSize(m_Context);
		if ((actualSize.x > maxSize) || (actualSize.y > maxSize)) {
			gLog.Error("Failed to create texture, its internal size is too high ({0}x{1}, maximum is {2}x{2})", actualSize.x, actualSize.y, maxSize);
			return false;
		}

		// Si une texture est déjà créée, on la détruit avant de créer la nouvelle
		bool destorybinding = false;
		if (m_IsCreated) {
			DestroyInternal();
			destorybinding = true;
		}

		// Mise à jour des propriétés de la texture
		m_Format = textureFormat;
		m_Size = actualSize;
		m_Channels = channels;

		OpenGLResult result;
		bool first = true;

		// Création de la texture OpenGL
		if (m_Handle == 0) {
			destorybinding = true;
			DestroyAllBinding(false);
			glCheckError(first, result, glCreateTextures(GL_TEXTURE_2D, 1, &m_Handle), "");
		}
		glCheckError(first, result, glTextureStorage2D(m_Handle, 1, GLConvert::ToTextureFormat(m_Format), m_Size.x, m_Size.y), "");

		// Chargement des données de la texture
		if (data) {
			glCheckError(first, result, glTextureSubImage2D(m_Handle, 0, 0, 0, m_Size.x, m_Size.y, GLConvert::ToTextureDataFormat(m_Format), GL_UNSIGNED_BYTE, data), "");
		}

		// Configuration des paramètres de la texture
		glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, m_IsSmooth ? GL_LINEAR : GL_NEAREST), "");
		glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_MAG_FILTER, m_IsSmooth ? GL_LINEAR : GL_NEAREST), "");
		glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_S, m_IsRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE), "");
		glCheckError(first, result, glTextureParameteri(m_Handle, GL_TEXTURE_WRAP_T, m_IsRepeated ? GL_REPEAT : GL_CLAMP_TO_EDGE), "");
		
		// Force un flush OpenGL pour s'assurer que la texture est immédiatement visible
		glCheckError(first, result, glFlush(), "");

		if(destorybinding) CreateAllBinding(false);

		m_IsCreated = true;
		return true;
	}


	bool OpenglTexture2D::UpdateInternal(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size, const maths::Vector2u& nSize)
	{
		// Vérifier si la taille a changé, et si oui, recréer la texture
		if (nSize.width != m_Size.width || nSize.height != m_Size.height) {
			return CreateInternal(m_Format, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}

		// Vérifications de validité des paramètres pour glTexSubImage2D
		if (offset.x < 0 || offset.y < 0 ||
			offset.x + size.width > m_Size.x || offset.y + size.height > m_Size.y) {
			//gLog.Error("Invalid offset or size for glTexSubImage2D. Offset: ({0}, {1}), Size: ({2}, {3}), Texture Size: ({4}, {5})", offset.x, offset.y, size.width, size.height, nSize.x, nSize.y);
			return CreateInternal(m_Format, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}

		// Calculer la taille totale des données de texture
		uint32 deviceSize = m_Size.x * m_Size.y * 4;
		if (deviceSize == 0 || m_Handle == 0) {
			gLog.Error("Invalid texture size or handle (size: {0}, handle: {1})", deviceSize, m_Handle);
			return false;
		}

		OpenGLResult result;
		bool first = true;

		// Lier la texture
		glCheckError(first, result, glBindTexture(GL_TEXTURE_2D, m_Handle), "");

		// Mettre à jour la texture
		glCheckError(first, result, glTexSubImage2D(GL_TEXTURE_2D, 0, offset.x, offset.y, size.width, size.height,
			GLConvert::ToTextureDataFormat(m_Format), GL_UNSIGNED_BYTE, datas), "");

		// Réinitialiser les paramètres de texture après mise à jour
		glCheckError(first, result, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_IsSmooth ? GL_LINEAR : GL_NEAREST), "");
		glCheckError(first, result, glFlush(), "");

		m_IsCreated = true;
		return true;
	}

	bool OpenglTexture2D::DestroyInternal()
	{
		if (m_IsCreated && m_Handle != 0) {
			glDeleteTextures(1, &m_Handle);
			m_Handle = 0;
			m_IsCreated = false;
		}
		return true;
	}

	Image OpenglTexture2D::ExtractDatasInternal()
	{
		return Image();
	}

	bool OpenglTexture2D::SizeIsValid(const maths::Vector2i& offset, const maths::Vector2u& size)
	{
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(size);
		return GetValidSize(&offset__, &size__);
	}

	bool OpenglTexture2D::GetValidSize(maths::Vector2i* offset, maths::Vector2u* size)
	{
		if (offset == nullptr || size == nullptr) return false;

		bool resize_image = false;
		bool recreate = false;

		if (offset->x < 0 || (offset->x >= 0 && offset->x + size->width > m_Size.width) ||
			offset->y < 0 || (offset->y >= 0 && offset->y + size->height > m_Size.height)) resize_image = true;

		maths::Vector2i new_offset(offset->x, offset->y);
		maths::Vector2i new_size(m_Size);

		if (resize_image) {
			uint32 offset_dep = ((new_offset.y < 0) ? -new_offset.y : 0) * m_Size.width + ((new_offset.x < 0) ? -new_offset.x : 0);

			new_offset = maths::Vector2i(offset->x < 0 ? 0 : offset->x, offset->y < 0 ? 0 : offset->y);

			if (offset->x < 0) new_size.width = m_Size.width - offset->x;
			if (size->width > new_size.width) new_size.width = size->width;

			if (offset->y < 0) new_size.height = m_Size.height - offset->y;
			if (size->height > new_size.height) new_size.height = size->height;

			uint32 maxSize = GetMaximumSize(m_Context);

			if (maxSize <= new_size.width || maxSize <= new_size.height) return false;
		}
		offset->x = new_offset.x;
		offset->y = new_offset.y;
		size->x = new_size.x;
		size->y = new_size.y;
		return true;
	}

	const std::string& OpenglTexture2D::AddBinding(T2DBindingCreateCallBackFn2 creat, T2DBindingCreateCallBackFn2 destroy)
	{
		if (creat != nullptr && destroy != nullptr) {
			for (const auto& pair : bindingCreate) {
				if (pair.second.target_type() == creat.target_type() && pair.second.target<bool(bool)>() == creat.target<bool(bool)>()) {
					return "";
				}
			}
			for (const auto& pair : bindingDestroy) {
				if (pair.second.target_type() == destroy.target_type() && pair.second.target<bool(bool)>() == destroy.target<bool(bool)>()) {
					return "";
				}
			}

			std::string id = GenerateTextureUniqueId2();
			bindingCreate[id] = creat;
			bindingDestroy[id] = destroy;
			return id;
		}
		return "";
	}

	void OpenglTexture2D::RemoveBinding(const std::string& id)
	{
		if (id == "") return;

		if (bindingCreate.find(id) == bindingCreate.end() || bindingDestroy.find(id) == bindingDestroy.end()) {
			return;
		}
		bindingCreate.erase(id);
		bindingDestroy.erase(id);
	}

	bool OpenglTexture2D::CreateAllBinding(bool value)
	{
		bool success = true;
		for (auto [id, icreate] : bindingCreate) {
			if (icreate != nullptr) {
				success = icreate(value, this) && success;
			}
		}
		return success;
	}

	bool OpenglTexture2D::DestroyAllBinding(bool value)
	{
		bool success = true;
		for (auto [id, idestroy] : bindingDestroy) {
			if (idestroy != nullptr) {
				success = idestroy(value, this) && success;
			}
		}
		return success;
	}

	OpenglTexture2DBinding::OpenglTexture2DBinding(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<OpenglContext>(context)), m_Handle(0)
	{
		if (sil != nullptr) {
			m_InputLayout = sil->samplerInput;
		}
	}

	bool OpenglTexture2DBinding::Initialize(Memory::Shared<Texture2D> texture)
	{
		Memory::Shared<OpenglTexture2D> oglTexture = Memory::Cast<OpenglTexture2D>(texture);
		if (m_Context == nullptr || texture == nullptr) return false;
		m_Handle = oglTexture->m_Handle;
		return true;
	}

	bool OpenglTexture2DBinding::Destroy()
	{
		if (m_Context == nullptr) return false;
		return true;
	}

	bool OpenglTexture2DBinding::Bind(const std::string& name)
	{
		if (m_Context == nullptr) return false;

		OpenGLResult result;
		bool first = true;

		if (m_Handle) {
			for (auto attribut : m_InputLayout) {
				if (attribut.name == name) {
					glCheckError(first, result, glBindTextureUnit(attribut.binding, m_Handle), "cannot bind {0}", name);
					if (result.success) return result.success;
					break;
				}
			}
		}
		gLog.Error("cannot bind {0}", name);
		return false;
	}

	bool OpenglTexture2DBinding::Bind(uint32 binding)
	{
		if (m_Context == nullptr) return false;

		OpenGLResult result;
		bool first = true;

		if (m_Handle) {
			for (auto attribut : m_InputLayout) {
				if (attribut.binding == binding) {
					glCheckError(first, result, glBindTextureUnit(attribut.binding, m_Handle), "cannot bind {0}", attribut.name);
					if (result.success) return result.success;
					break;
				}
			}
		}
		gLog.Error("cannot bind {0}", binding);
		return false;
	}

	bool OpenglTexture2DBinding::Equal(Memory::Shared<Texture2DBinding> binding)
	{
		Memory::Shared<OpenglTexture2DBinding> oglBinding = Memory::Cast<OpenglTexture2DBinding>(binding);
		if (m_Context == nullptr || oglBinding == nullptr) return false;

		return m_Handle == oglBinding->m_Handle;
	}

	bool OpenglTexture2DBinding::IsDefined(Memory::Shared<Texture2D> binding)
	{
		Memory::Shared<OpenglTexture2D> oglBinding = Memory::Cast<OpenglTexture2D>(binding);
		if (m_Context == nullptr || oglBinding == nullptr) return false;

		return m_Handle == oglBinding->m_Handle;
	}

	bool OpenglTexture2DBinding::IsValide()
	{
		return m_Handle != 0;
	}

}  //  nkentseu