// =============================================================================
// NkShadowSystem.cpp  — NKRenderer v5.0
//
// Etat actuel : D.3a stub fonctionnel.
//   - L'atlas est cree (1x1 depth) pour avoir un handle bindable.
//   - Le ShadowUBO est cree avec cascadeCount=0 -> le shader PBR prend la
//     branche "pas d'ombre" (GetShadow() retourne 1.0 quand cascadeCount=0).
//   - Le pipeline shadow et le rendu des cascades arrivent en D.3b.
// =============================================================================
#include "NkShadowSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include <cmath>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        // Layout std140 du ShadowUBO (binding=3 dans le shader PBR).
        // Doit matcher pbr.frag.gl.glsl exactement (alignement std140).
        struct ShadowUBOBlock {
            NkMat4f cascadeMats[4];
            // std140 : un float dans un array prend un vec4 -> on laisse 4 float
            // mais l'array est indexé .x pour matcher le shader (glsl autorise
            // float cascadeSplits[4] qui prend 4 vec4 en std140).
            float32 cascadeSplits[4 * 4];  // 4 entrees, chaque dans un vec4
            int32   cascadeCount;
            float32 shadowBias;
            float32 normalBias;
            int32   softShadows;
        };

        bool NkShadowSystem::Init(NkIDevice* d, NkMeshSystem* m,
                                NkMaterialSystem* mat, const NkShadowSystemConfig& cfg) {
            mDevice = d; mMesh = m; mMat = mat; mCfg = cfg;

            // ── Atlas depth texture (D.3a : 1x1 stub, D.3b : resolution * numCascades) ──
            auto td = NkTextureDesc::DepthStencil(1, 1, NkGPUFormat::NK_D32_FLOAT);
            td.debugName = "ShadowAtlas_Stub";
            mAtlasRhi = mDevice->CreateTexture(td);

            // ── Sampler avec compare-mode (sampler2DShadow dans le shader) ──
            mShadowSampler = mDevice->CreateSampler(NkSamplerDesc::Shadow());

            // ── ShadowUBO ──
            mUBOShadow = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(ShadowUBOBlock)));
            mActiveCascades = 0;  // stub : pas de cascades actives -> pas d'ombre
            for (int i = 0; i < 4; i++) {
                mCascadeMats[i]    = NkMat4f::Identity();
                mCascadeSplits[i]  = 0.f;
            }
            UploadShadowUBO();

            return mAtlasRhi.IsValid() && mUBOShadow.IsValid();
        }

        void NkShadowSystem::Shutdown() {
            if (mUBOShadow.IsValid())     { mDevice->DestroyBuffer(mUBOShadow);     mUBOShadow={}; }
            if (mShadowSampler.IsValid()) { mDevice->DestroySampler(mShadowSampler); mShadowSampler={}; }
            if (mAtlasRhi.IsValid())      { mDevice->DestroyTexture(mAtlasRhi);     mAtlasRhi={}; }
        }

        void NkShadowSystem::UploadShadowUBO() {
            ShadowUBOBlock b{};
            for (int c = 0; c < 4; c++) {
                b.cascadeMats[c] = mCascadeMats[c];
                b.cascadeSplits[c * 4] = mCascadeSplits[c];  // .x dans le vec4
            }
            b.cascadeCount = (int32)mActiveCascades;
            b.shadowBias   = mCfg.depthBias * 0.001f;
            b.normalBias   = mCfg.normalBias;
            b.softShadows  = (mCfg.pcfMode == NkPCFMode::PCF5x5) ? 1 : 0;
            mDevice->WriteBuffer(mUBOShadow, &b, sizeof(b));
        }

        void NkShadowSystem::BeginShadowPass(NkICommandBuffer* cmd,
                                            NkVec3f lightDir, const NkCamera3D& cam) {
            (void)cmd; (void)lightDir; (void)cam;
            // D.3b : compute cascade splits + matrices ici.
            mInPass = true;
        }

        void NkShadowSystem::EndShadowPass(NkICommandBuffer* cmd) {
            (void)cmd;
            mInPass = false;
        }

        void NkShadowSystem::RenderShadowPasses(NkICommandBuffer* cmd) {
            (void)cmd;
            // D.3a : pas de rendu shadow (atlas 1x1, cascadeCount=0 -> pas d'ombre).
            // L'upload du UBO est fait au Init et ne change pas tant que la config
            // ou la camera ne change pas. En D.3b on appellera ici :
            //   1. Compute cascade matrices a partir du frustum camera
            //   2. UploadShadowUBO() avec mActiveCascades = mCfg.numCascades
            //   3. Pour chaque cascade : bind atlas slice + lightVP + Render3D->RenderShadowPass
        }

    } // namespace renderer
} // namespace nkentseu
