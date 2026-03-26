#pragma once

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NkUI/NkUI.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
    namespace nkui {

        class NkUINKRHIBackend {
            public:
                bool Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api);
                void Destroy();

                void Submit(NkICommandBuffer* cmd, const NkUIContext& ctx, uint32 fbW, uint32 fbH);

                bool UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height);
                bool UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height);
                bool HasTexture(uint32 texId) const noexcept;

            private:
                static constexpr uint64 kMinVBOCap = 1ull * 1024ull * 1024ull;
                static constexpr uint64 kMinIBOCap = 512ull * 1024ull;
                static constexpr uint32 kShrinkDelayFrames = 240u;

                NkIDevice*         mDevice = nullptr;
                NkGraphicsApi      mApi = NkGraphicsApi::NK_API_OPENGL;
                NkRenderPassHandle mRenderPass;
                NkShaderHandle     mShader;
                NkPipelineHandle   mPipeline;
                NkBufferHandle     mVBO;
                NkBufferHandle     mIBO;
                NkBufferHandle     mUBO;
                uint64             mVBOCap = 0;
                uint64             mIBOCap = 0;
                uint32             mLowUsageFrames = 0;
                NkTextureHandle    mWhiteTex;
                NkSamplerHandle    mSampler;
                NkDescSetHandle    mLayout;
                NkDescSetHandle    mWhiteDescSet;

                struct TextureEntry {
                    NkTextureHandle texture;
                    NkDescSetHandle descSet;
                    int32 width = 0;
                    int32 height = 0;
                };

                NkHashMap<uint32, TextureEntry> mTextures;
                uint32 mBoundTexId = 0xFFFFFFFFu;

                bool CreatePipeline();
                bool CreateResources();
                bool UploadTextureInternal(uint32 texId, const uint8* data, int32 width, int32 height, bool rgba8);
                bool EnsureGeometryBuffers(uint64 requiredVtxBytes, uint64 requiredIdxBytes, bool allowShrink);
                bool CreateDescriptorSetForTexture(NkTextureHandle texture, NkDescSetHandle& outSet);

                void BindTexture(NkICommandBuffer* cmd, uint32 texId);
        };

    }
}
