//
// Created by TEUGUIA TADJUIDJE Rodolf S�deris on 2024-06-16 at 11:23:53 AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#include "pch/ntspch.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include <NTSGraphics/Core/Log.h>

namespace nkentseu {
	using namespace maths;

	// Compteur atomique pour générer des identifiants uniques
	static std::atomic<usize> textureUniqueIdCounter(0);

	// Génère un identifiant unique
	static std::string GenerateTextureUniqueId() {
		usize id = textureUniqueIdCounter++;
		std::ostringstream oss;
		oss << id;
		return oss.str();
	}

	VulkanTexture2D::VulkanTexture2D(Memory::Shared<Context> context) : m_Context(Memory::Cast<VulkanContext>(context))
	{
		bindingDestroy.clear();
		bindingCreate.clear();
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
	}

	bool VulkanTexture2D::Create(TextureFormat textureFormat, const maths::Vector2u& size, uint32 channels) {
		if (SizeIsValid(maths::Vector2i(), size) && m_Image.Create(size, channels)) {
			return CreateInternal(textureFormat, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		return false;
	}

	bool VulkanTexture2D::Create(TextureFormat textureFormat, const std::filesystem::path& filename) {
		Image image;
		if (image.LoadFromFile(filename)) {
			m_Path = filename;
			return Create(textureFormat, image);
		}
		return false;
	}

	bool VulkanTexture2D::Create(TextureFormat textureFormat, const Image& image) {
		if (SizeIsValid(maths::Vector2i(), image.GetSize()) && m_Image.Create(image.GetSize(), image.GetPixels())) {
			return CreateInternal(textureFormat, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		return false;
	}

	bool VulkanTexture2D::Create(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels) {
		if (SizeIsValid(maths::Vector2i(), size) && m_Image.Create(size, data, channels)) {
			return CreateInternal(textureFormat, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		return false;
	}

	bool VulkanTexture2D::Destroy() {
		bool success = DestroyInternal();
		bindingDestroy.clear();
		bindingCreate.clear();
		return success;
	}

	bool VulkanTexture2D::Update(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size) {
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(size);
		if (GetValidSize(offset__, size__) && m_Image.Update(datas, offset, size)) {
			return UpdateInternal(datas, offset, size, size__);
		}
		return false;
	}

	bool VulkanTexture2D::Update(const Image& image, const maths::Vector2i& offset) {
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(image.GetSize());
		if (GetValidSize(offset__, size__) && m_Image.Update(image.GetPixels(), offset, image.GetSize())) {
			return UpdateInternal(image.GetPixels(), offset, image.GetSize(), size__);
		}
		return false;
	}

	bool VulkanTexture2D::Update(const Texture2D& texture, const maths::Vector2i& offset) {
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(texture.GetSize());
		if (GetValidSize(offset__, size__) && m_Image.Update(texture.GetImage().GetPixels(), offset, texture.GetSize())) {
			return UpdateInternal(texture.GetImage().GetPixels(), offset, texture.GetSize(), size__);
		}
		return false;
	}

	maths::Vector2u VulkanTexture2D::GetSize() const {
		return m_Size;
	}

	uint8* VulkanTexture2D::ExtractDatas(maths::Vector2u* size) {
		// create le stanging buffer
		// transfer l'image vers le stanging
		// extraire les donne du stanging

		//void* data;
		//vk::DeviceMemory stagingMemory = GetBufferMemory(stagingBuffer); // Mémoire associée
		//device.mapMemory(stagingMemory, 0, VK_WHOLE_SIZE, {}, &data);
		return nullptr;
	}

	Image& VulkanTexture2D::GetImage() {
		return m_Image;
	}

	const Image& VulkanTexture2D::GetImage() const
	{
		return m_Image;
	}

	void VulkanTexture2D::SetSmooth(bool smooth) {
		if (m_IsSmooth == smooth) return;

		DestroyAllBinding(false);

		DestroySampler();
		m_IsSmooth = smooth;
		CreateSampler();

		CreateAllBinding(false);
	}

	bool VulkanTexture2D::IsSmooth() const { return m_IsSmooth; }

	TextureFormat VulkanTexture2D::GetTextureFormat() const { return m_Format; }

	void VulkanTexture2D::SetRepeated(bool repeated) {
		if (m_IsRepeated == repeated) return;

		DestroyAllBinding(false);

		DestroySampler();
		m_IsRepeated = repeated;
		CreateSampler();

		CreateAllBinding(false);
	}

	bool VulkanTexture2D::IsRepeated() const { return m_IsRepeated; }

	bool VulkanTexture2D::GenerateMipmap() {
		return false;
	}

	void VulkanTexture2D::Swap(Texture& right) noexcept {}

	void VulkanTexture2D::InvalidateMipmap() {}

	const std::filesystem::path& VulkanTexture2D::GetPath() const { return m_Path; }

	bool VulkanTexture2D::CreateInternal(TextureFormat textureFormat, const uint8* data, const maths::Vector2u& size, uint32 channels)
	{
		if ((size.x == 0) || (size.y == 0)) {
			gLog.Error("Failed to create texture, invalid size ({0}x{1})", size.x, size.y);
			return false;
		}

		// Compute the internal texture dimensions depending on NPOT textures support
		const maths::Vector2u actualSize(GetValidSize(m_Context, size.x), GetValidSize(m_Context, size.y));

		// Check the maximum texture size
		const uint32 maxSize = GetMaximumSize(m_Context);
		if ((actualSize.x > maxSize) || (actualSize.y > maxSize)) {
			gLog.Error("Failed to create texture, its internal size is too high ({0}x{1}, maximum is {2}x{2})", actualSize.x, actualSize.y, maxSize);
			return false;
		}

		if (m_IsCreated) {
			DestroyInternal();
		}

		m_Format = textureFormat;
		m_Size = actualSize;
		m_Channels = channels;

		if (!CreateImage(VulkanConvert::ToTextureFormat(m_Format), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal)) {
			gLog.Error("Cannot create Image texture");
			return false;
		}

		vk::DeviceSize deviceSize = m_Size.x * m_Size.y * m_Channels;
		if (deviceSize == 0) return false;

		VkBufferInternal stanging;

		vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eTransferSrc;
		vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
		vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		if (!VkBufferInternal::CreateBuffer(m_Context.get(), deviceSize, usage, sharingMode, propertyFlags, stanging.buffer, stanging.bufferMemory)) {

			return false;
		}

		if (!stanging.Mapped(m_Context.get(), deviceSize, 0, {})) {

			return false;
		}

		if (!stanging.WriteToBuffer(data, deviceSize, 0)) {

			return false;
		}

		// VK_IMAGE_LAYOUT_GENERAL a utiliser pour l'access en lecture et en ecriture
		TransitionImageLayout(image, VulkanConvert::ToTextureFormat(m_Format), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);

		CopyBufferToImage(stanging.buffer, image, m_Size.width, m_Size.height, maths::Vector2f());

		TransitionImageLayout(image, VulkanConvert::ToTextureFormat(m_Format), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels);

		if (!stanging.UnMapped(m_Context.get())) {

			return false;
		}

		if (!stanging.Destroy(m_Context.get())) {

			return false;
		}

		DestroyAllBinding(false);

		CreateImageView();
		CreateSampler();

		CreateAllBinding(false);
		m_IsCreated = true;
		return true;
	}

	bool VulkanTexture2D::UpdateInternal(const uint8* datas, const maths::Vector2i& offset, const maths::Vector2u& size, const maths::Vector2u& nSize)
	{
		if (nSize.width != m_Size.width || nSize.height != m_Size.height) {
			// on dois redimensionner en prenant en compte nOffset et nSize sinon mettre juste a jour
			return CreateInternal(m_Format, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}
		// Vérifications de validité des paramètres pour glTexSubImage2D
		if (offset.x < 0 || offset.y < 0 ||
			offset.x + size.width > m_Size.x || offset.y + size.height > m_Size.y) {
			//gLog.Error("Invalid offset or size for glTexSubImage2D. Offset: ({0}, {1}), Size: ({2}, {3}), Texture Size: ({4}, {5})", offset.x, offset.y, size.width, size.height, nSize.x, nSize.y);
			return CreateInternal(m_Format, m_Image.GetPixels(), m_Image.GetSize(), m_Image.GetChannels());
		}

		vk::DeviceSize deviceSize = m_Size.x * m_Size.y * m_Channels;
		if (deviceSize == 0) return false;

		VkBufferInternal stanging;

		vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eTransferSrc;
		vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
		vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

		if (!VkBufferInternal::CreateBuffer(m_Context.get(), deviceSize, usage, sharingMode, propertyFlags, stanging.buffer, stanging.bufferMemory)) {

			return false;
		}

		if (!stanging.Mapped(m_Context.get(), deviceSize, 0, {})) {

			return false;
		}

		if (!stanging.WriteToBuffer(datas, deviceSize, 0)) {

			return false;
		}

		// VK_IMAGE_LAYOUT_GENERAL a utiliser pour l'access en lecture et en ecriture
		TransitionImageLayout(image, VulkanConvert::ToTextureFormat(m_Format), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);

		CopyBufferToImage(stanging.buffer, image, m_Size.width, m_Size.height, offset);

		TransitionImageLayout(image, VulkanConvert::ToTextureFormat(m_Format), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels);

		if (!stanging.UnMapped(m_Context.get())) {

			return false;
		}

		if (!stanging.Destroy(m_Context.get())) {

			return false;
		}

		DestroySampler();
		DestroyImageView();
		CreateImageView();
		CreateSampler();

		CreateAllBinding(false);
		m_IsCreated = true;
		return true;
	}

	bool VulkanTexture2D::DestroyInternal()
	{
		DestroyAllBinding(false);

		DestroySampler();
		DestroyImageView();
		DestroyImage();

		m_IsCreated = false;
		return false;
	}

	Image VulkanTexture2D::ExtractDatasInternal()
	{
		return Image();
	}

	bool VulkanTexture2D::SizeIsValid(const maths::Vector2i& offset, const maths::Vector2u& size)
	{
		maths::Vector2i offset__(offset);
		maths::Vector2u size__(size);
		return GetValidSize(offset__, size__);
	}

	bool VulkanTexture2D::GetValidSize(maths::Vector2i& offset, maths::Vector2u& size)
	{
		bool resize_image = false;
		bool recreate = false;

		if (offset.x < 0 || (offset.x >= 0 && offset.x + size.width > m_Size.width) ||
			offset.y < 0 || (offset.y >= 0 && offset.y + size.height > m_Size.height)) resize_image = true;

		maths::Vector2i new_offset(offset);
		maths::Vector2i new_size(m_Size);

		if (resize_image) {
			uint32 offset_dep = ((new_offset.y < 0) ? -new_offset.y : 0) * m_Size.width + ((new_offset.x < 0) ? -new_offset.x : 0);

			new_offset = maths::Vector2i(offset.x < 0 ? 0 : offset.x, offset.y < 0 ? 0 : offset.y);

			if (offset.x < 0) new_size.width = m_Size.width - offset.x;
			if (size.width > new_size.width) new_size.width = size.width;

			if (offset.y < 0) new_size.height = m_Size.height - offset.y;
			if (size.height > new_size.height) new_size.height = size.height;

			uint32 maxSize = GetMaximumSize(m_Context);

			if (maxSize <= new_size.width || maxSize <= new_size.height) return false;
		}
		offset.x = new_offset.x;
		offset.y = new_offset.y;
		size.x = new_size.x;
		size.y = new_size.y;
		return true;
	}

	uint32 VulkanTexture2D::GetMaximumSize(Memory::Shared<Context> context)
	{
		Memory::Shared<VulkanContext> vkcontext = Memory::Cast<VulkanContext>(context);
		if (vkcontext != nullptr) {
			return vkcontext->properties.limits.maxImageDimension2D;
		}
		return 0;
	}

	uint32 VulkanTexture2D::GetValidSize(Memory::Shared<Context> context, uint32 size)
	{
		if (size <= GetMaximumSize(context)) {
			// If hardware supports the desired size, return it
			return size;
		}
		else {
			// If hardware doesn't support the desired size, calculate the nearest power of two
			uint32 powerOfTwo = 1;
			while (powerOfTwo < size)
				powerOfTwo *= 2;

			return powerOfTwo;
		}
	}

	bool VulkanTexture2D::CreateImage(vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags prop)
	{
		if (m_Context == nullptr) return false;

		VulkanResult result;
		bool first = true;

		vk::ImageCreateInfo imageInfo{};
		imageInfo.imageType = vk::ImageType::e2D;
		imageInfo.extent.width = m_Size.width;
		imageInfo.extent.height = m_Size.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		//imageInfo.format = ConvertTextureFormat(m_Format);
		imageInfo.format = format;
		//imageInfo.tiling = vk::ImageTiling::eOptimal;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = vk::ImageLayout::eUndefined;
		//imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
		imageInfo.usage = usage;
		imageInfo.sharingMode = vk::SharingMode::eExclusive;
		imageInfo.samples = vk::SampleCountFlagBits::e1;

		vkCheckError(first, result, m_Context->device.createImage(&imageInfo, m_Context->allocator, &image), "cannot create vulkan img");

		vk::MemoryRequirements memRequirements;
		vkCheckErrorVoid(m_Context->device.getImageMemoryRequirements(image, &memRequirements));

		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot get image memory requirement");
		}

		vk::MemoryAllocateInfo allocInfo{};
		allocInfo.allocationSize = memRequirements.size;
		//allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(m_Context->physicalDevice, memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(m_Context->physicalDevice, memRequirements.memoryTypeBits, prop);

		vkCheckError(first, result, m_Context->device.allocateMemory(&allocInfo, m_Context->allocator, &imageMemory), "echec lors de lallocation de la memoire pour l'image !");

		vkCheckErrorVoid(m_Context->device.bindImageMemory(image, imageMemory, 0));

		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot bind image memory");
		}

		return true;
	}

	bool VulkanTexture2D::DestroyImage()
	{
		if (m_Context == nullptr) return false;

		bool success = true;

		vkCheckErrorVoid(m_Context->device.destroyImage(image, m_Context->allocator));
		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot destroy image texture");
			success = false;
		}

		image = nullptr;

		vkCheckErrorVoid(m_Context->device.freeMemory(imageMemory, m_Context->allocator));
		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot free image texture memory");
			success = false;
		}

		imageMemory = nullptr;
		return success;
	}

	bool VulkanTexture2D::CreateImageView()
	{

		if (m_Context == nullptr) return false;
		VulkanResult result;
		bool first = true;

		vk::ImageViewCreateInfo viewInfo{};
		viewInfo.image = image;
		viewInfo.viewType = vk::ImageViewType::e2D;
		viewInfo.format = VulkanConvert::ToTextureFormat(m_Format);
		viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.components = vk::ComponentSwizzle::eIdentity;

		vkCheckError(first, result, m_Context->device.createImageView(&viewInfo, m_Context->allocator, &imageView), "Echec de création de la vue pour la texture !");

		return result.success;
	}

	bool VulkanTexture2D::DestroyImageView()
	{
		if (m_Context == nullptr) return false;
		vkCheckErrorVoid(m_Context->device.destroyImageView(imageView, m_Context->allocator));
		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot destroy image view");
			return false;
		}
		imageView = nullptr;
		return true;
	}

	bool VulkanTexture2D::CreateSampler()
	{
		if (m_Context == nullptr) return false;
		VulkanResult result;
		bool first = true;

		vk::SamplerCreateInfo samplerInfo{};
		samplerInfo.magFilter = m_IsSmooth ? vk::Filter::eLinear : vk::Filter::eNearest;
		samplerInfo.minFilter = m_IsSmooth ? vk::Filter::eLinear : vk::Filter::eNearest;
		samplerInfo.addressModeU = m_IsRepeated ? vk::SamplerAddressMode::eRepeat : vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeV = m_IsRepeated ? vk::SamplerAddressMode::eRepeat : vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.addressModeW = m_IsRepeated ? vk::SamplerAddressMode::eRepeat : vk::SamplerAddressMode::eClampToEdge;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = vk::CompareOp::eAlways;
		samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		//samplerInfo.maxLod = 0.0f;
		samplerInfo.maxLod = static_cast<float32>(mipLevels);

		vkCheckError(first, result, m_Context->device.createSampler(&samplerInfo, nullptr, &sampler), "Failed to create texture sampler!");

		return result.success;
	}

	bool VulkanTexture2D::DestroySampler()
	{
		if (m_Context == nullptr) return false;
		vkCheckErrorVoid(m_Context->device.destroySampler(sampler, m_Context->allocator));
		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot destroy texture image sampler");
			return false;
		}
		sampler = nullptr;
		return true;
	}

	void VulkanTexture2D::TransitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32 mipL)
	{
		vk::CommandBuffer commandBuffer = VulkanContext::BeginSingleTimeCommands(m_Context.get());

		vk::ImageMemoryBarrier barrier{};
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
		barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;

		barrier.image = image;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipL;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eNone;
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else {
			gLog.Error("Transition de l'agencement non supportée !");
			return;
		}

		vkCheckErrorVoid(commandBuffer.pipelineBarrier(
			sourceStage, destinationStage,
			vk::DependencyFlags{},
			0, nullptr,
			0, nullptr,
			1, &barrier
		));

		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot use pipeline barrier");
		}

		VulkanContext::EndSingleTimeCommands(m_Context.get(), commandBuffer);
	}

	void VulkanTexture2D::CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32 width, uint32 height, const maths::Vector2i& offset)
	{
		vk::CommandBuffer commandBuffer = VulkanContext::BeginSingleTimeCommands(m_Context.get());

		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D{ offset.x, offset.y, 0 };
		region.imageExtent = vk::Extent3D{ width, height, 1 };

		vkCheckErrorVoid(commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region));
		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot copy buffer to image");
		}

		VulkanContext::EndSingleTimeCommands(m_Context.get(), commandBuffer);
	}

	void VulkanTexture2D::CopyImageToBuffer(vk::Image image, vk::Buffer buffer, uint32 width, uint32 height) {
		vk::CommandBuffer commandBuffer = VulkanContext::BeginSingleTimeCommands(m_Context.get());

		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D{ 0, 0, 0 };
		region.imageExtent = vk::Extent3D{ width, height, 1 };

		vkCheckErrorVoid(commandBuffer.copyImageToBuffer(image, vk::ImageLayout::eTransferSrcOptimal, buffer, 1, &region));
		if (!VulkanStaticDebugInfo::Result()) {
			gLog.Error("Cannot copy buffer to image");
		}

		VulkanContext::EndSingleTimeCommands(m_Context.get(), commandBuffer);
	}

	const std::string& VulkanTexture2D::AddBinding(T2DBindingCreateCallBackFn creat, T2DBindingCreateCallBackFn destroy)
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

			std::string id = GenerateTextureUniqueId();
			bindingCreate[id] = creat;
			bindingDestroy[id] = destroy;
			return id;
		}
		return "";
	}

	void VulkanTexture2D::RemoveBinding(const std::string& id)
	{
		if (id == "") return;

		if (bindingCreate.find(id) == bindingCreate.end() || bindingDestroy.find(id) == bindingDestroy.end()) {
			return;
		}
		bindingCreate.erase(id);
		bindingDestroy.erase(id);
	}

	bool VulkanTexture2D::CreateAllBinding(bool value)
	{
		bool success = true;
		for (auto [id, icreate] : bindingCreate) {
			if (icreate != nullptr) {
				success = icreate(value, this) && success;
			}
		}
		return success;
	}

	bool VulkanTexture2D::DestroyAllBinding(bool value)
	{
		bool success = true;
		for (auto [id, idestroy] : bindingDestroy) {
			if (idestroy != nullptr) {
				success = idestroy(value, this) && success;
			}
		}
		return success;
	}

	VulkanTexture2DBinding::VulkanTexture2DBinding(Memory::Shared<Context> context, Memory::Shared<ShaderInputLayout> sil) : m_Context(Memory::Cast<VulkanContext>(context)), m_Vksil(Memory::Cast<VulkanShaderInputLayout>(sil))
	{
	}

	bool VulkanTexture2DBinding::Initialize(Memory::Shared<Texture2D> texture)
	{
		Memory::Shared<VulkanTexture2D> vkTexture = Memory::Cast<VulkanTexture2D>(texture);
		if (vkTexture == nullptr) return false;

		id = vkTexture->AddBinding(T2D_BINDING_CALLBACK_FN(VulkanTexture2DBinding::icreate), T2D_BINDING_CALLBACK_FN(VulkanTexture2DBinding::idestroy));
		return icreate(true, vkTexture.get());
	}

	bool VulkanTexture2DBinding::Initialize(VulkanTexture2D* texture)
	{
		id = texture->AddBinding(T2D_BINDING_CALLBACK_FN(VulkanTexture2DBinding::icreate), T2D_BINDING_CALLBACK_FN(VulkanTexture2DBinding::idestroy));
		return icreate(true, texture);
	}

	bool VulkanTexture2DBinding::Destroy()
	{
		if (idestroy(false, texture)) {
			texture->RemoveBinding(id);
			return true;
		}
		return false;
	}

	bool VulkanTexture2DBinding::Bind(const std::string& name)
	{
		for (auto& attribut : m_Vksil->samplerInput) {
			if (attribut.name == name) {
				if (m_DescriptorSets.Bind(m_Context, m_Vksil->pipelineLayout, attribut.name, attribut.binding, attribut.set)) {
					return true;
				}
				break;
			}
		}
		gLog.Error("cannot bind {0}", name);
		return false;
	}

	bool VulkanTexture2DBinding::Bind(uint32 binding)
	{
		for (auto& attribut : m_Vksil->samplerInput) {
			if (attribut.binding == binding) {
				if (m_DescriptorSets.Bind(m_Context, m_Vksil->pipelineLayout, attribut.name, attribut.binding, attribut.set)) {
					return true;
				}
				break;
			}
		}
		gLog.Error("cannot bind {0}", binding);
		return false;
	}

	bool VulkanTexture2DBinding::Equal(Memory::Shared<Texture2DBinding> binding)
	{
		Memory::Shared<VulkanTexture2DBinding> vkBinding = Memory::Cast<VulkanTexture2DBinding>(binding);
		if (m_Context == nullptr || vkBinding == nullptr || vkBinding->texture == nullptr || texture == nullptr) return false;

		return texture->sampler == vkBinding->texture->sampler && texture->imageView == vkBinding->texture->imageView;
	}

	bool VulkanTexture2DBinding::IsDefined(Memory::Shared<Texture2D> binding)
	{
		Memory::Shared<VulkanTexture2D> vkBinding = Memory::Cast<VulkanTexture2D>(binding);
		if (m_Context == nullptr || vkBinding == nullptr || texture == nullptr) return false;

		if (vkBinding->sampler == vkBinding->sampler && vkBinding->imageView == vkBinding->imageView) {
			return true;
		}

		return vkBinding->sampler == nullptr && vkBinding->imageView == nullptr;
	}

	bool VulkanTexture2DBinding::IsValide()
	{
		if (m_Context == nullptr || texture == nullptr) return false;
		return texture->sampler == nullptr && texture->imageView == nullptr;
	}

	bool VulkanTexture2DBinding::icreate(bool value, VulkanTexture2D* texture)
	{
		if (texture == nullptr) return false;
		if (texture != this->texture && texture != nullptr) this->texture = texture;

		if (m_Context == nullptr || m_Vksil == nullptr || texture == nullptr || texture->sampler == nullptr || texture->image == nullptr || texture->imageMemory == nullptr || texture->imageView == nullptr) {
			return false;
		}

		if (isCreated) {
			if (!idestroy(false, texture)) return false;
		}

		// Recupere le layout du descriptor set pour les uniform buffers
		auto it = m_Vksil->descriptorSetLayouts.find(BufferSpecificationType::Enum::Texture);
		if (it == m_Vksil->descriptorSetLayouts.end() || it->second.empty()) {
			gLog.Error("Texture buffer descriptor set layout not found.");
			return false;
		}

		std::vector<uint32> bindings;
		std::vector<vk::DescriptorPoolSize> poolSizes;
		for (auto& attribut : m_Vksil->samplerInput) {
			bindings.push_back(attribut.binding);
			vk::DescriptorPoolSize poolSize{};
			poolSize.type = VulkanConvert::SamplerInputAttributType(attribut.type);
			poolSize.descriptorCount = m_Context->swapchainImages.size();
			poolSizes.push_back(poolSize);
		}

		m_DescriptorSets.SetDataBindings(texture->sampler, texture->imageView);
		if (!m_DescriptorSets.Create(m_Context, bindings, it->second, poolSizes)) {
			gLog.Error("Cannot create descriptor set for texture");
			return false;
		}
		return true;
	}

	bool VulkanTexture2DBinding::idestroy(bool value, VulkanTexture2D* texture)
	{
		if (texture == nullptr) return false;
		if (this->texture != nullptr) this->texture = nullptr;

		bool success = m_DescriptorSets.Destroy(m_Context);
		isCreated = false;
		return success;
	}

}  //  nkentseu

