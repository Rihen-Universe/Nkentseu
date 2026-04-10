#pragma once
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKMath/NkColor.h"
#include "NKMath/NKMath.h"

namespace nkentseu {

    struct NkGrid3DConfig {
        float baseUnit        = 1.0f;
        int   subDivisions    = 4;
        float extent          = 50.0f;
        bool  infinite        = true;
        float fadeStart       = 30.0f;
        float fadeEnd         = 80.0f;
        float lineWidthMajor  = 0.02f;
        float lineWidthMinor  = 0.008f;
        bool  showAxes        = true;
        bool  showSolidFloor  = false;

        math::NkColor majorColor   = { 55, 60, 75, 200 };
        math::NkColor minorColor   = { 40, 45, 55, 120 };
        math::NkColor axisXColor   = { 220, 60, 60, 220 };
        math::NkColor axisZColor   = { 60, 120, 220, 220 };
        math::NkColor solidColor   = { 25, 28, 35, 100 };
    };

    class NkGrid3D {
        public:
            NkGrid3D();
            ~NkGrid3D();

            bool Init(NkIDevice* device, NkRenderPassHandle renderPass);
            void Shutdown();
            bool IsValid() const { return mInitialized; }

            void SetConfig(const NkGrid3DConfig& cfg) { mConfig = cfg; }
            const NkGrid3DConfig& GetConfig() const { return mConfig; }

            void Draw(NkICommandBuffer* cmd,
                    const math::NkMat4& viewMatrix,
                    const math::NkMat4& projMatrix,
                    uint32 width, uint32 height);

        private:
            bool CreatePipeline(NkRenderPassHandle renderPass);
            void UpdatePushConstants(NkICommandBuffer* cmd,
                                    const math::NkMat4& invView,
                                    const math::NkMat4& invProj);

            NkIDevice*          mDevice = nullptr;
            bool                mInitialized = false;
            NkGrid3DConfig      mConfig;

            NkPipelineHandle    mPipeline;
            NkShaderHandle      mShader;            // shader combiné (vert+frag)
            NkDescSetHandle     mDescriptorSetLayout;
            NkDescSetHandle     mDescriptorSet;      // set réutilisable (sans texture)
            NkSamplerHandle     mDummySampler;       // inutilisé mais requis
    };
} // namespace nkentseu