#pragma once
// =============================================================================
// PV3DE/Layers/PatientLayer.h  —  v2
// =============================================================================
// Pipeline complet :
//   DiagnosticEngine → ClinicalState → EmotionFSM
//        → FaceController (AU → blendshapes)
//        → BodyController  (postures, tremblements)
//        → BreathController (rythme + pattern respiratoire)
//        → SpeechEngine    (TTS + LipSync)
//        → PatientRenderer (GPU)
//        → CaseRunner      (cas cliniques scriptés)
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKMath/NKMath.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Diagnostic/NkDiagnosticEngine.h"
#include "PV3DE/Diagnostic/NkCaseLoader.h"
#include "PV3DE/Emotion/NkEmotionFSM.h"
#include "PV3DE/Face/NkFaceController.h"
#include "PV3DE/Body/NkBodyController.h"
#include "PV3DE/Body/NkBreathController.h"
#include "PV3DE/Speech/NkSpeechEngine.h"
#include "PV3DE/Renderer/NkPatientRenderer.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace pv3de {

        class PatientLayer : public nkentseu::Layer {
        public:
            PatientLayer(const NkString& name,
                         NkIDevice* device,
                         NkICommandBuffer* cmd) noexcept;
            ~PatientLayer() override;

            void OnAttach()                       override;
            void OnDetach()                       override;
            void OnUpdate(nk_float32 dt)          override;
            void OnRender()                       override;
            bool OnEvent(nkentseu::NkEvent* e)    override;

            // ── API clinique ──────────────────────────────────────────────────
            void AddSymptom   (NkSymptomId id)   { mDiagnostic.AddSymptom(id);    }
            void RemoveSymptom(NkSymptomId id)    { mDiagnostic.RemoveSymptom(id); }
            void ClearSymptoms()                  { mDiagnostic.ClearSymptoms();   }
            void SetVitalSigns(nk_float32 hr,
                               nk_float32 temp,
                               nk_float32 spo2)  { mDiagnostic.SetVitalSigns(hr, temp, spo2); }
            void ForceEmotion(EmotionState state,
                              nk_float32 intensity = 1.f) {
                mEmotionFSM.ForceState(state, intensity);
            }

            // ── Parole ────────────────────────────────────────────────────────
            void Speak(const char* text) noexcept;
            void AnswerQuestion(const char* question) noexcept;

            // ── Cas cliniques ─────────────────────────────────────────────────
            bool LoadCase(const char* nkcasePath) noexcept;
            void StartCase() noexcept;
            void StopCase()  noexcept;

            // ── Renderer ──────────────────────────────────────────────────────
            void SetCamera(const NkMat4f& viewProj,
                           const NkMat4f& model,
                           const NkVec3f& camPos) noexcept;

            // Texture FBO du patient (pour MedicalUILayer)
            NkTextureHandle GetPatientFBO() const noexcept { return mPatientFBO; }

            // ── Accès état courant ────────────────────────────────────────────
            const NkClinicalState&  GetClinicalState()    const { return mClinicalState; }
            const NkEmotionOutput&  GetEmotionOutput()    const { return mEmotionFSM.GetOutput(); }
            const NkFaceController& GetFaceController()   const { return mFaceController; }
            const NkDiagnosticEngine& GetDiagnosticEngine() const { return mDiagnostic; }

        private:
            void SetupRenderer() noexcept;
            void DispatchCaseEvent(const NkCaseEvent& ev) noexcept;

            NkIDevice*        mDevice = nullptr;
            NkICommandBuffer* mCmd    = nullptr;

            // ── Sous-systèmes ─────────────────────────────────────────────────
            NkDiagnosticEngine  mDiagnostic;
            NkClinicalState     mClinicalState;
            NkEmotionFSM        mEmotionFSM;
            NkFaceController    mFaceController;
            NkBodyController    mBodyController;
            NkBreathController  mBreathController;
            NkSpeechEngine      mSpeechEngine;
            NkPatientRenderer   mRenderer;

            // ── Cas clinique ──────────────────────────────────────────────────
            NkCaseLoader  mCaseLoader;
            NkCaseData    mCurrentCase;
            NkCaseRunner  mCaseRunner;
            bool          mCaseLoaded = false;

            // ── Camera ────────────────────────────────────────────────────────
            NkMat4f mViewProj = NkMat4f::Identity();
            NkMat4f mModel    = NkMat4f::Identity();
            NkVec3f mCamPos   = {0.f, 1.f, 3.f};

            // FBO viewport patient (injecté dans MedicalUILayer)
            NkTextureHandle mPatientFBO;
        };

    } // namespace pv3de
} // namespace nkentseu
