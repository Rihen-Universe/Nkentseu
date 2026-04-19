#pragma once
// =============================================================================
// PV3DE/Renderer/NkPatientRenderer.h
// =============================================================================
// Orchestrateur du rendu 3D du patient virtuel.
// Coordonne : NkBSDriver, shaders Skin/Eye, NkBodyController outputs.
//
// Pipeline par frame :
//   1. NkFaceController  → poids blendshapes → NkBSDriver::SetWeights()
//   2. NkBodyController  → déplacements thoraciques → matrices bones
//   3. NkBSDriver::Flush() → upload GPU
//   4. DrawPatient() : mesh corps + shader Skin.vert/frag
//   5. DrawEyes()    : mesh yeux + shader Eye.vert/frag
//
// Paramètres cliniques → shaders :
//   - breathingDifficulty → Eye.frag::u_scleraRedness (yeux injectés)
//   - emotionState        → Eye.frag::u_pupilDiameter (mydriase/myosis)
//   - fatigueLevel        → Eye.frag::u_eyeWetness
//   - painLevel           → Skin.frag::u_skinTint (rougeur cutanée)
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NkBSDriver.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Face/NkFaceController.h"
#include "PV3DE/Body/NkBodyController.h"
#include "NKMath/NKMath.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace pv3de {

        struct NkPatientRenderConfig {
            // Chemins des ressources
            const char* bodyMeshPath    = "Assets/Meshes/patient_body.nkmesh";
            const char* eyeMeshPath     = "Assets/Meshes/patient_eyes.nkmesh";
            const char* albedoPath      = "Assets/Textures/skin_albedo.png";
            const char* normalMapPath   = "Assets/Textures/skin_normal.png";
            const char* ormMapPath      = "Assets/Textures/skin_orm.png";
            const char* sssMapPath      = "Assets/Textures/skin_sss.png";
            const char* emissivePath    = "Assets/Textures/skin_emissive.png";
            const char* scleraPath      = "Assets/Textures/eye_sclera.png";
            const char* irisDetailPath  = "Assets/Textures/eye_iris.png";

            nk_uint32 blendshapeCount = 64;
        };

        class NkPatientRenderer {
        public:
            NkPatientRenderer() = default;
            ~NkPatientRenderer() noexcept { Shutdown(); }

            // ── Init / Shutdown ───────────────────────────────────────────────
            bool Init(NkIDevice* device,
                      NkICommandBuffer* cmd,
                      const NkPatientRenderConfig& config) noexcept;
            void Shutdown() noexcept;

            // ── Update depuis les sous-systèmes PV3DE ─────────────────────────
            // Appelé depuis PatientLayer::OnRender() avant Draw()
            void UpdateFromSystems(const NkFaceController& face,
                                   const NkBodyController& body,
                                   const NkClinicalState&  clinical) noexcept;

            // ── Rendu ─────────────────────────────────────────────────────────
            void Draw(NkICommandBuffer* cmd,
                      const NkMat4f& viewProj,
                      const NkMat4f& model,
                      const NkVec3f& cameraPos) noexcept;

            // ── Accesseurs ────────────────────────────────────────────────────
            bool IsReady() const noexcept { return mReady; }

        private:
            void DrawBody(NkICommandBuffer* cmd,
                          const NkMat4f& viewProj,
                          const NkMat4f& model,
                          const NkVec3f& cameraPos) noexcept;

            void DrawEyes(NkICommandBuffer* cmd,
                          const NkMat4f& viewProj,
                          const NkMat4f& model,
                          const NkVec3f& cameraPos) noexcept;

            void UploadSkinUniforms(NkICommandBuffer* cmd,
                                    const NkMat4f& viewProj,
                                    const NkMat4f& model,
                                    const NkVec3f& cameraPos) noexcept;

            void UploadEyeUniforms (NkICommandBuffer* cmd,
                                    const NkMat4f& viewProj,
                                    const NkMat4f& model,
                                    const NkVec3f& cameraPos) noexcept;

            NkIDevice*        mDevice = nullptr;
            bool              mReady  = false;

            NkBSDriver        mBSDriver;

            // GPU resources
            NkShaderHandle    mSkinShader;
            NkShaderHandle    mEyeShader;
            NkMeshHandle      mBodyMesh;
            NkMeshHandle      mEyeMesh;
            NkTextureHandle   mAlbedo, mNormal, mORM, mSSS, mEmissive;
            NkTextureHandle   mSclera, mIrisDetail;

            // Paramètres shaders courants (mis à jour depuis UpdateFromSystems)
            struct SkinParams {
                NkVec4f skinTint        = {1,1,1,1};
                float   sssStrength     = 0.35f;
                float   emissiveStrength= 1.f;
            } mSkinParams;

            struct EyeParams {
                float   pupilDiameter   = 0.35f;
                float   scleraRedness   = 0.f;
                float   eyeWetness      = 1.f;
                float   gazeOffsetX     = 0.f;
                float   gazeOffsetY     = 0.f;
            } mEyeParams;
        };

    } // namespace pv3de
} // namespace nkentseu
