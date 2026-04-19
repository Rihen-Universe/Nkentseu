#include "NkPatientRenderer.h"
#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace pv3de {

        bool NkPatientRenderer::Init(NkIDevice* device,
                                     NkICommandBuffer* /*cmd*/,
                                     const NkPatientRenderConfig& cfg) noexcept {
            mDevice = device;
            if (!mDevice) return false;

            // ── Shaders ───────────────────────────────────────────────────────
            {
                NkShaderDesc sd;
                sd.vertPath = "Shaders/Skin.vert";
                sd.fragPath = "Shaders/Skin.frag";
                sd.debugName = "SkinShader";
                mSkinShader = mDevice->CreateShader(sd);
                if (!mSkinShader.IsValid()) {
                    logger.Errorf("[NkPatientRenderer] Shader Skin chargement échoué\n");
                    return false;
                }
            }
            {
                NkShaderDesc sd;
                sd.vertPath = "Shaders/Eye.vert";
                sd.fragPath = "Shaders/Eye.frag";
                sd.debugName = "EyeShader";
                mEyeShader = mDevice->CreateShader(sd);
                if (!mEyeShader.IsValid()) {
                    logger.Errorf("[NkPatientRenderer] Shader Eye chargement échoué\n");
                    return false;
                }
            }

            // ── Textures ──────────────────────────────────────────────────────
            auto LoadTex = [&](const char* path) -> NkTextureHandle {
                NkTextureLoadDesc d;
                d.path = path;
                d.generateMips = true;
                return mDevice->LoadTexture(d);
            };
            mAlbedo     = LoadTex(cfg.albedoPath);
            mNormal     = LoadTex(cfg.normalMapPath);
            mORM        = LoadTex(cfg.ormMapPath);
            mSSS        = LoadTex(cfg.sssMapPath);
            mEmissive   = LoadTex(cfg.emissivePath);
            mSclera     = LoadTex(cfg.scleraPath);
            mIrisDetail = LoadTex(cfg.irisDetailPath);

            // ── Meshes ────────────────────────────────────────────────────────
            mBodyMesh = mDevice->LoadMesh(cfg.bodyMeshPath);
            mEyeMesh  = mDevice->LoadMesh(cfg.eyeMeshPath);

            // ── NkBSDriver ────────────────────────────────────────────────────
            if (!mBSDriver.Init(mDevice, cfg.blendshapeCount)) {
                logger.Errorf("[NkPatientRenderer] NkBSDriver init échoué\n");
                return false;
            }

            mReady = true;
            logger.Infof("[NkPatientRenderer] Init OK — {} blendshapes\n",
                         cfg.blendshapeCount);
            return true;
        }

        void NkPatientRenderer::Shutdown() noexcept {
            if (!mDevice) return;
            mBSDriver.Shutdown();
            if (mSkinShader.IsValid()) mDevice->DestroyShader(mSkinShader);
            if (mEyeShader.IsValid())  mDevice->DestroyShader(mEyeShader);
            // Textures + meshes détruits par NkResourceManager
            mReady = false;
        }

        // =====================================================================
        void NkPatientRenderer::UpdateFromSystems(const NkFaceController& face,
                                                   const NkBodyController& body,
                                                   const NkClinicalState&  clinical) noexcept {
            // ── Blendshapes (FACS → GPU) ──────────────────────────────────────
            mBSDriver.SetWeights(face.GetBlendshapeWeights());

            // ── Paramètres skin ───────────────────────────────────────────────
            // Rougeur cutanée proportionnelle à la douleur
            float redness = NkClamp(clinical.painLevel / 10.f, 0.f, 1.f) * 0.3f;
            // Pâleur si SpO2 < 94%
            float pallor  = NkClamp((94.f - clinical.spo2) / 10.f, 0.f, 0.5f);
            // Cyanose si SpO2 < 88%
            float cyanosis = NkClamp((88.f - clinical.spo2) / 10.f, 0.f, 0.8f);

            mSkinParams.skinTint = NkVec4f(
                1.f - pallor * 0.3f - cyanosis * 0.2f + redness * 0.15f,
                1.f - pallor * 0.2f - cyanosis * 0.1f,
                1.f - pallor * 0.1f + cyanosis * 0.3f,
                1.f
            );
            mSkinParams.sssStrength     = 0.3f + clinical.breathingDifficulty * 0.1f;
            mSkinParams.emissiveStrength = 1.f + redness;

            // ── Paramètres yeux ───────────────────────────────────────────────
            // Pupille : mydriase (anxiété/choc) ou myosis (opioïdes/sédation)
            float anxPupil    = 0.35f + clinical.anxietyLevel * 0.25f;  // mydriase anxiété
            float fatiguePupil= 0.35f - clinical.fatigueLevel * 0.15f;  // myosis fatigue
            mEyeParams.pupilDiameter = NkClamp(
                NkLerp(fatiguePupil, anxPupil, clinical.anxietyLevel), 0.1f, 0.9f);

            // Rougeur sclère : fatigue + difficulté respiratoire
            mEyeParams.scleraRedness = NkClamp(
                clinical.fatigueLevel * 0.5f + clinical.breathingDifficulty * 0.4f, 0.f, 1.f);

            // Brillance cornée : réduite si fatigue extrême ou coma
            mEyeParams.eyeWetness = NkClamp(1.f - clinical.fatigueLevel * 0.6f, 0.f, 1.f);

            // Regard : légère déviation selon l'état
            mEyeParams.gazeOffsetX = (face.GetGazeYaw() / 45.f) * 0.5f;
            mEyeParams.gazeOffsetY = (face.GetGazePitch() / 30.f) * 0.3f;
        }

        // =====================================================================
        void NkPatientRenderer::Draw(NkICommandBuffer* cmd,
                                     const NkMat4f& viewProj,
                                     const NkMat4f& model,
                                     const NkVec3f& cameraPos) noexcept {
            if (!mReady || !cmd) return;

            // Upload blendshapes
            mBSDriver.Flush(cmd);

            DrawBody(cmd, viewProj, model, cameraPos);
            DrawEyes(cmd, viewProj, model, cameraPos);
        }

        void NkPatientRenderer::DrawBody(NkICommandBuffer* cmd,
                                          const NkMat4f& viewProj,
                                          const NkMat4f& model,
                                          const NkVec3f& cameraPos) noexcept {
            cmd->BindShader(mSkinShader);

            // Bind textures
            cmd->BindTexture(mAlbedo,   0);
            cmd->BindTexture(mNormal,   1);
            cmd->BindTexture(mORM,      2);
            cmd->BindTexture(mSSS,      3);
            cmd->BindTexture(mEmissive, 4);

            // Bind blendshape buffer
            mBSDriver.Bind(cmd);

            UploadSkinUniforms(cmd, viewProj, model, cameraPos);
            cmd->DrawMesh(mBodyMesh);

            mBSDriver.Unbind(cmd);
        }

        void NkPatientRenderer::DrawEyes(NkICommandBuffer* cmd,
                                          const NkMat4f& viewProj,
                                          const NkMat4f& model,
                                          const NkVec3f& cameraPos) noexcept {
            cmd->BindShader(mEyeShader);
            cmd->BindTexture(mIrisDetail, 0);
            cmd->BindTexture(mSclera,     1);
            UploadEyeUniforms(cmd, viewProj, model, cameraPos);
            cmd->DrawMesh(mEyeMesh);
        }

        void NkPatientRenderer::UploadSkinUniforms(NkICommandBuffer* cmd,
                                                    const NkMat4f& viewProj,
                                                    const NkMat4f& model,
                                                    const NkVec3f& cameraPos) noexcept {
            NkMat4f normal = NkTranspose(NkInverse(model));
            cmd->SetUniformMat4("u_model",        model);
            cmd->SetUniformMat4("u_normalMatrix",  normal);
            cmd->SetUniformVec3("u_cameraPos",    cameraPos);
            cmd->SetUniformVec4("u_skinTint",     mSkinParams.skinTint);
            cmd->SetUniformFloat("u_sssStrength",  mSkinParams.sssStrength);
            cmd->SetUniformFloat("u_emissiveStrength", mSkinParams.emissiveStrength);
            // Note : viewProj est dans le UBO caméra — lié ailleurs
            (void)viewProj;
        }

        void NkPatientRenderer::UploadEyeUniforms(NkICommandBuffer* cmd,
                                                   const NkMat4f& viewProj,
                                                   const NkMat4f& model,
                                                   const NkVec3f& cameraPos) noexcept {
            NkMat4f normal = NkTranspose(NkInverse(model));
            cmd->SetUniformMat4("u_model",       model);
            cmd->SetUniformMat4("u_normalMatrix", normal);
            cmd->SetUniformVec3("u_cameraPos",   cameraPos);
            cmd->SetUniformFloat("u_pupilDiameter",  mEyeParams.pupilDiameter);
            cmd->SetUniformFloat("u_scleraRedness",  mEyeParams.scleraRedness);
            cmd->SetUniformFloat("u_eyeWetness",     mEyeParams.eyeWetness);
            cmd->SetUniformFloat("u_gazeOffsetX",    mEyeParams.gazeOffsetX);
            cmd->SetUniformFloat("u_gazeOffsetY",    mEyeParams.gazeOffsetY);
            (void)viewProj;
        }

    } // namespace pv3de
} // namespace nkentseu
