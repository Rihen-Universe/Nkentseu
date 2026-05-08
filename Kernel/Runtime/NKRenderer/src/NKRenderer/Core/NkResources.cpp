// =============================================================================
// NkResources.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkResources.h"
#include "NKRHI/Core/NkDescs.h"

namespace nkentseu {
    namespace renderer {

        NkResources::~NkResources() { Shutdown(); }

        // =====================================================================
        // Init / Shutdown
        // =====================================================================
        NkRResult NkResources::Init(NkIDevice* device) {
            if (mReady) return NkRResult::NK_OK;
            if (!device) {
                NkRSetLastError(NkRResult::NK_ERR_INVALID_DEVICE,
                                "NkResources::Init device==nullptr");
                return NkRResult::NK_ERR_INVALID_DEVICE;
            }
            mDevice = device;

            if (!CreateDefaultTextures()) {
                NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY,
                                "NkResources : default textures creation failed");
                return NkRResult::NK_ERR_OUT_OF_MEMORY;
            }
            if (!CreateDefaultSamplers()) {
                NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY,
                                "NkResources : default samplers creation failed");
                return NkRResult::NK_ERR_OUT_OF_MEMORY;
            }
            if (!CreateStandardLayouts()) {
                NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY,
                                "NkResources : standard descriptor-set layouts failed");
                return NkRResult::NK_ERR_OUT_OF_MEMORY;
            }

            mReady = true;
            return NkRResult::NK_OK;
        }

        void NkResources::Shutdown() {
            if (!mDevice) return;

            // Layouts
            if (mFrameLayout.IsValid())       mDevice->DestroyDescriptorSetLayout(mFrameLayout);
            if (mObjectLayout.IsValid())      mDevice->DestroyDescriptorSetLayout(mObjectLayout);
            if (mMaterialLayout.IsValid())    mDevice->DestroyDescriptorSetLayout(mMaterialLayout);
            if (mPostProcessLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mPostProcessLayout);

            // Samplers
            if (mSamLinearRepeat.IsValid())   mDevice->DestroySampler(mSamLinearRepeat);
            if (mSamLinearClamp.IsValid())    mDevice->DestroySampler(mSamLinearClamp);
            if (mSamLinearBorder.IsValid())   mDevice->DestroySampler(mSamLinearBorder);
            if (mSamNearestRepeat.IsValid())  mDevice->DestroySampler(mSamNearestRepeat);
            if (mSamNearestClamp.IsValid())   mDevice->DestroySampler(mSamNearestClamp);
            if (mSamAniso16.IsValid())        mDevice->DestroySampler(mSamAniso16);
            if (mSamShadow.IsValid())         mDevice->DestroySampler(mSamShadow);
            if (mSamCubemap.IsValid())        mDevice->DestroySampler(mSamCubemap);

            // Textures
            if (mTexWhite.IsValid())          mDevice->DestroyTexture(mTexWhite);
            if (mTexBlack.IsValid())          mDevice->DestroyTexture(mTexBlack);
            if (mTexNormal.IsValid())         mDevice->DestroyTexture(mTexNormal);
            if (mTexMagenta.IsValid())        mDevice->DestroyTexture(mTexMagenta);
            if (mTexGray.IsValid())           mDevice->DestroyTexture(mTexGray);

            mTexWhite = mTexBlack = mTexNormal = mTexMagenta = mTexGray = {};
            mSamLinearRepeat = mSamLinearClamp = mSamLinearBorder = {};
            mSamNearestRepeat = mSamNearestClamp = {};
            mSamAniso16 = mSamShadow = mSamCubemap = {};
            mFrameLayout = mObjectLayout = mMaterialLayout = mPostProcessLayout = {};

            mDevice = nullptr;
            mReady  = false;
        }

        // =====================================================================
        // Default textures (1x1)
        // =====================================================================
        static NkTextureHandle Make1x1(NkIDevice* dev, const uint8 rgba[4], const char* name) {
            NkTextureDesc d = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RGBA8_UNORM, 1);
            d.bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
            d.usage       = NkResourceUsage::NK_DEFAULT;
            d.initialData = rgba;
            d.debugName   = name;
            return dev->CreateTexture(d);
        }

        bool NkResources::CreateDefaultTextures() {
            const uint8 white[4]   = {255, 255, 255, 255};
            const uint8 black[4]   = {  0,   0,   0, 255};
            const uint8 normal[4]  = {128, 128, 255, 255};   // (0.5, 0.5, 1, 1) en UNORM
            const uint8 magenta[4] = {255,   0, 255, 255};
            const uint8 gray[4]    = {128, 128, 128, 255};

            mTexWhite   = Make1x1(mDevice, white,   "NkResources_White1x1");
            mTexBlack   = Make1x1(mDevice, black,   "NkResources_Black1x1");
            mTexNormal  = Make1x1(mDevice, normal,  "NkResources_Normal1x1");
            mTexMagenta = Make1x1(mDevice, magenta, "NkResources_Magenta1x1");
            mTexGray    = Make1x1(mDevice, gray,    "NkResources_Gray1x1");

            return mTexWhite.IsValid() && mTexBlack.IsValid()
                && mTexNormal.IsValid() && mTexMagenta.IsValid() && mTexGray.IsValid();
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
                d.maxLod = 1000.f;     // sample tous les mips (utile pour GGX prefiltere)
                mSamCubemap = mDevice->CreateSampler(d);
            }
            return mSamLinearRepeat.IsValid()  && mSamLinearClamp.IsValid()
                && mSamLinearBorder.IsValid()  && mSamNearestRepeat.IsValid()
                && mSamNearestClamp.IsValid()  && mSamAniso16.IsValid()
                && mSamShadow.IsValid()        && mSamCubemap.IsValid();
        }

        // =====================================================================
        // Standard descriptor-set layouts
        // =====================================================================
        bool NkResources::CreateStandardLayouts() {
            // Frame (set=0)
            //   binding 0 : camera UBO
            //   binding 1 : lights UBO (forward)
            //   binding 2 : lights SSBO (forward+ light list)
            //   binding 3 : clusters SSBO
            //   binding 4 : shadow atlas (sampled image)
            //   binding 5 : IBL irradiance (cubemap)
            //   binding 6 : IBL specular prefiltered (cubemap)
            //   binding 7 : BRDF LUT (2D)
            {
                NkDescriptorSetLayoutDesc d;
                d.Add(NK_BIND_CAMERA_UBO,    NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_ALL_GRAPHICS)
                 .Add(NK_BIND_LIGHTS_UBO,    NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_LIGHTS_SSBO,   NkDescriptorType::NK_STORAGE_BUFFER,         NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_CLUSTERS_SSBO, NkDescriptorType::NK_STORAGE_BUFFER,         NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_SHADOW_ATLAS,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_IBL_IRRADIANCE,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_IBL_SPECULAR,  NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_BRDF_LUT,      NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
                mFrameLayout = mDevice->CreateDescriptorSetLayout(d);
            }
            // Object (set=1)
            //   binding 0 : object UBO (model + normal matrix + tint + flags)
            //   binding 1 : bones SSBO (skinning)
            //   binding 2 : instance SSBO (instanced draws)
            {
                NkDescriptorSetLayoutDesc d;
                d.Add(NK_BIND_OBJECT_UBO,    NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS)
                 .Add(NK_BIND_BONES_SSBO,    NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_VERTEX)
                 .Add(NK_BIND_INSTANCE_SSBO, NkDescriptorType::NK_STORAGE_BUFFER, NkShaderStage::NK_VERTEX);
                mObjectLayout = mDevice->CreateDescriptorSetLayout(d);
            }
            // Material (set=2)
            //   binding 0 : PBR params UBO
            //   binding 1..5 : albedo / normal / ORM / emissive / AO
            {
                NkDescriptorSetLayoutDesc d;
                d.Add(NK_BIND_PBR_PARAMS,   NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_TEX_ALBEDO,   NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_TEX_NORMAL,   NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_TEX_ORM,      NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_TEX_EMISSIVE, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT)
                 .Add(NK_BIND_TEX_AO,       NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
                mMaterialLayout = mDevice->CreateDescriptorSetLayout(d);
            }
            // PostProcess (set=3)
            //   binding 0 : input image (full-screen pass)
            {
                NkDescriptorSetLayoutDesc d;
                d.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
                mPostProcessLayout = mDevice->CreateDescriptorSetLayout(d);
            }
            return mFrameLayout.IsValid() && mObjectLayout.IsValid()
                && mMaterialLayout.IsValid() && mPostProcessLayout.IsValid();
        }

        // =====================================================================
        // Buffer factories
        // =====================================================================
        NkBufferHandle NkResources::CreateUBO(uint64 sizeBytes, const char* debugName) {
            NkBufferDesc d = NkBufferDesc::Uniform(sizeBytes);
            d.debugName = debugName;
            return mDevice->CreateBuffer(d);
        }

        NkBufferHandle NkResources::CreateSSBO(uint64 sizeBytes, const char* debugName) {
            NkBufferDesc d = NkBufferDesc::Storage(sizeBytes);
            d.debugName = debugName;
            return mDevice->CreateBuffer(d);
        }

        NkBufferHandle NkResources::CreateVertexDynamic(uint64 sizeBytes, const char* debugName) {
            NkBufferDesc d = NkBufferDesc::VertexDynamic(sizeBytes);
            d.debugName = debugName;
            return mDevice->CreateBuffer(d);
        }

        NkBufferHandle NkResources::CreateIndexBuffer(const uint32* data, uint32 count, const char* debugName) {
            NkBufferDesc d;
            d.sizeBytes   = (uint64)count * sizeof(uint32);
            d.type        = NkBufferType::NK_INDEX;
            d.usage       = NkResourceUsage::NK_IMMUTABLE;
            d.initialData = data;
            d.debugName   = debugName;
            return mDevice->CreateBuffer(d);
        }

        NkBufferHandle NkResources::CreateStagingBuffer(uint64 sizeBytes, const char* debugName) {
            NkBufferDesc d;
            d.sizeBytes = sizeBytes;
            d.type      = NkBufferType::NK_STAGING;
            d.usage     = NkResourceUsage::NK_UPLOAD;
            d.debugName = debugName;
            return mDevice->CreateBuffer(d);
        }

    } // namespace renderer
} // namespace nkentseu
