// =============================================================================
// NkResources.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkResources.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace renderer {

		NkResources::~NkResources() {
			Shutdown();
		}

		// =====================================================================
		// Init / Shutdown
		// =====================================================================
		NkRResult NkResources::Init(NkIDevice *device) {
			if (mReady)
				return NkRResult::NK_OK;
			if (!device) {
				NkRSetLastError(NkRResult::NK_ERR_INVALID_DEVICE, "NkResources::Init device==nullptr");
				return NkRResult::NK_ERR_INVALID_DEVICE;
			}
			mDevice = device;

			logger.Info("[NkResources]   1.a CreateDefaultTextures...\n");
			if (!CreateDefaultTextures()) {
				NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY, "NkResources : default textures creation failed");
				return NkRResult::NK_ERR_OUT_OF_MEMORY;
			}
			logger.Info("[NkResources]   1.b CreateDefaultSamplers...\n");
			if (!CreateDefaultSamplers()) {
				NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY, "NkResources : default samplers creation failed");
				return NkRResult::NK_ERR_OUT_OF_MEMORY;
			}
			logger.Info("[NkResources]   1.c CreateStandardLayouts...\n");
			if (!CreateStandardLayouts()) {
				NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY,
								"NkResources : standard descriptor-set layouts failed");
				return NkRResult::NK_ERR_OUT_OF_MEMORY;
			}

			logger.Info("[NkResources]   1.d Init done\n");
			mReady = true;
			return NkRResult::NK_OK;
		}

		void NkResources::Shutdown() {
			if (!mDevice)
				return;

			// Layouts
			if (mFrameLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mFrameLayout);
			if (mObjectLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mObjectLayout);
			if (mMaterialLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mMaterialLayout);
			if (mPostProcessLayout.IsValid())
				mDevice->DestroyDescriptorSetLayout(mPostProcessLayout);

			// Samplers
			if (mSamLinearRepeat.IsValid())
				mDevice->DestroySampler(mSamLinearRepeat);
			if (mSamLinearClamp.IsValid())
				mDevice->DestroySampler(mSamLinearClamp);
			if (mSamLinearBorder.IsValid())
				mDevice->DestroySampler(mSamLinearBorder);
			if (mSamNearestRepeat.IsValid())
				mDevice->DestroySampler(mSamNearestRepeat);
			if (mSamNearestClamp.IsValid())
				mDevice->DestroySampler(mSamNearestClamp);
			if (mSamAniso16.IsValid())
				mDevice->DestroySampler(mSamAniso16);
			if (mSamShadow.IsValid())
				mDevice->DestroySampler(mSamShadow);
			if (mSamCubemap.IsValid())
				mDevice->DestroySampler(mSamCubemap);

			// Textures
			if (mTexWhite.IsValid())
				mDevice->DestroyTexture(mTexWhite);
			if (mTexBlack.IsValid())
				mDevice->DestroyTexture(mTexBlack);
			if (mTexNormal.IsValid())
				mDevice->DestroyTexture(mTexNormal);
			if (mTexMagenta.IsValid())
				mDevice->DestroyTexture(mTexMagenta);
			if (mTexGray.IsValid())
				mDevice->DestroyTexture(mTexGray);

			mTexWhite = mTexBlack = mTexNormal = mTexMagenta = mTexGray = {};
			mSamLinearRepeat = mSamLinearClamp = mSamLinearBorder = {};
			mSamNearestRepeat = mSamNearestClamp = {};
			mSamAniso16 = mSamShadow = mSamCubemap = {};
			mFrameLayout = mObjectLayout = mMaterialLayout = mPostProcessLayout = {};

			mDevice = nullptr;
			mReady = false;
		}

		// =====================================================================
		// Default textures (1x1)
		// =====================================================================
		static NkTextureHandle Make1x1(NkIDevice *dev, const uint8 rgba[4], const char *name) {
			NkTextureDesc d = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RGBA8_UNORM, 1);
			d.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
			d.usage = NkResourceUsage::NK_DEFAULT;
			d.initialData = rgba;
			d.debugName = name;
			return dev->CreateTexture(d);
		}

		bool NkResources::CreateDefaultTextures() {
			const uint8 white[4] = {255, 255, 255, 255};
			const uint8 black[4] = {0, 0, 0, 255};
			const uint8 normal[4] = {128, 128, 255, 255}; // (0.5, 0.5, 1, 1) en UNORM
			const uint8 magenta[4] = {255, 0, 255, 255};
			const uint8 gray[4] = {128, 128, 128, 255};

			logger.Info("[NkResources]      White1x1...\n");
			mTexWhite = Make1x1(mDevice, white, "NkResources_White1x1");
			logger.Info("[NkResources]      Black1x1...\n");
			mTexBlack = Make1x1(mDevice, black, "NkResources_Black1x1");
			logger.Info("[NkResources]      Normal1x1...\n");
			mTexNormal = Make1x1(mDevice, normal, "NkResources_Normal1x1");
			logger.Info("[NkResources]      Magenta1x1...\n");
			mTexMagenta = Make1x1(mDevice, magenta, "NkResources_Magenta1x1");
			logger.Info("[NkResources]      Gray1x1...\n");
			mTexGray = Make1x1(mDevice, gray, "NkResources_Gray1x1");
			logger.Info("[NkResources]      defaults done\n");

			return mTexWhite.IsValid() && mTexBlack.IsValid() && mTexNormal.IsValid() && mTexMagenta.IsValid() &&
				   mTexGray.IsValid();
		}

		// =====================================================================
		// Default samplers
		// =====================================================================
		bool NkResources::CreateDefaultSamplers() {
			// Linear / Repeat
			{
				NkSamplerDesc d = NkSamplerDesc::Linear();
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_REPEAT;
				mSamLinearRepeat = mDevice->CreateSampler(d);
			}
			// Linear / Clamp-edge
			{
				NkSamplerDesc d = NkSamplerDesc::Linear();
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
				mSamLinearClamp = mDevice->CreateSampler(d);
			}
			// Linear / Border (transparent black)
			{
				NkSamplerDesc d = NkSamplerDesc::Linear();
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_CLAMP_TO_BORDER;
				d.borderColor = NkBorderColor::NK_TRANSPARENT_BLACK;
				mSamLinearBorder = mDevice->CreateSampler(d);
			}
			// Nearest / Repeat
			{
				NkSamplerDesc d = NkSamplerDesc::Nearest();
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_REPEAT;
				mSamNearestRepeat = mDevice->CreateSampler(d);
			}
			// Nearest / Clamp
			{
				NkSamplerDesc d = NkSamplerDesc::Nearest();
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
				mSamNearestClamp = mDevice->CreateSampler(d);
			}
			// Anisotropic 16x / Repeat
			{
				NkSamplerDesc d = NkSamplerDesc::Anisotropic(16.f);
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_REPEAT;
				mSamAniso16 = mDevice->CreateSampler(d);
			}
			// Shadow comparaison-sampler (PCF)
			{
				NkSamplerDesc d = NkSamplerDesc::Shadow();
				mSamShadow = mDevice->CreateSampler(d);
			}
			// Cubemap : tri-linear, clamp-edge sur les 3 axes
			{
				NkSamplerDesc d = NkSamplerDesc::Linear();
				d.addressU = d.addressV = d.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
				d.maxLod = 1000.f; // sample tous les mips (utile pour GGX prefiltere)
				mSamCubemap = mDevice->CreateSampler(d);
			}
			return mSamLinearRepeat.IsValid() && mSamLinearClamp.IsValid() && mSamLinearBorder.IsValid() &&
				   mSamNearestRepeat.IsValid() && mSamNearestClamp.IsValid() && mSamAniso16.IsValid() &&
				   mSamShadow.IsValid() && mSamCubemap.IsValid();
		}

		// =====================================================================
		// Standard descriptor-set layouts
		// =====================================================================
		bool NkResources::CreateStandardLayouts() {
			// LA TABLE EST LA SOURCE, PAS LE COMMENTAIRE (2026-08-22).
			//
			// Avant : quatre blocs de .Add() enchaines, chacun precede d'un commentaire
			// « set=N ». Le set n'existait donc dans AUCUNE declaration, et personne —
			// ni outil, ni compilateur — ne pouvait confronter ce qu'on lie ici a ce que
			// les shaders echantillonnent. Un commentaire ne peut pas se tromper ; il
			// peut seulement etre faux sans que rien ne le dise.
			//
			// Maintenant : NkResources.h::kStandardBindings porte (set, binding, type,
			// etage), et cette fonction la PARCOURT. La table n'est donc pas une
			// seconde verite a maintenir a cote du code : c'est la seule. Si elle ment,
			// les layouts mentent avec elle.
			//
			// Equivalence avec l'ancienne version : verifiee mecaniquement (meme suite
			// de (set, binding, type, etage), dans le meme ordre) avant de remplacer.
			// Ce n'est PAS une preuve d'execution : aucun GPU n'a tourne ici.
			NkDescriptorSetLayoutDesc descs[4];
			for (uint32 i = 0; i < kStandardBindingCount; ++i) {
				const NkStandardBinding &b = kStandardBindings[i];
				descs[(uint32)b.set].Add(b.binding, b.type, b.stages);
			}

			mFrameLayout = mDevice->CreateDescriptorSetLayout(descs[(uint32)NK_SET_FRAME]);
			mObjectLayout = mDevice->CreateDescriptorSetLayout(descs[(uint32)NK_SET_OBJECT]);
			mMaterialLayout = mDevice->CreateDescriptorSetLayout(descs[(uint32)NK_SET_MATERIAL]);
			mPostProcessLayout = mDevice->CreateDescriptorSetLayout(descs[(uint32)NK_SET_POSTPROCESS]);

			return mFrameLayout.IsValid() && mObjectLayout.IsValid() && mMaterialLayout.IsValid() &&
				   mPostProcessLayout.IsValid();
		}

		// =====================================================================
		// Buffer factories
		// =====================================================================
		NkBufferHandle NkResources::CreateUBO(uint64 sizeBytes, const char *debugName) {
			NkBufferDesc d = NkBufferDesc::Uniform(sizeBytes);
			d.debugName = debugName;
			return mDevice->CreateBuffer(d);
		}

		NkBufferHandle NkResources::CreateSSBO(uint64 sizeBytes, const char *debugName) {
			NkBufferDesc d = NkBufferDesc::Storage(sizeBytes);
			d.debugName = debugName;
			return mDevice->CreateBuffer(d);
		}

		NkBufferHandle NkResources::CreateVertexDynamic(uint64 sizeBytes, const char *debugName) {
			NkBufferDesc d = NkBufferDesc::VertexDynamic(sizeBytes);
			d.debugName = debugName;
			return mDevice->CreateBuffer(d);
		}

		NkBufferHandle NkResources::CreateIndexBuffer(const uint32 *data, uint32 count, const char *debugName) {
			NkBufferDesc d;
			d.sizeBytes = (uint64)count * sizeof(uint32);
			d.type = NkBufferType::NK_INDEX;
			d.usage = NkResourceUsage::NK_IMMUTABLE;
			d.initialData = data;
			d.debugName = debugName;
			return mDevice->CreateBuffer(d);
		}

		NkBufferHandle NkResources::CreateStagingBuffer(uint64 sizeBytes, const char *debugName) {
			NkBufferDesc d;
			d.sizeBytes = sizeBytes;
			d.type = NkBufferType::NK_STAGING;
			d.usage = NkResourceUsage::NK_UPLOAD;
			d.debugName = debugName;
			return mDevice->CreateBuffer(d);
		}

	} // namespace renderer
} // namespace nkentseu
