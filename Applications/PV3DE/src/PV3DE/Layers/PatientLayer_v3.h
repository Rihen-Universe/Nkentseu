#pragma once
// =============================================================================
// PV3DE/Layers/PatientLayer_v3.h
// =============================================================================
// PatientLayer v3 : intègre NkAIDriver (NkHumanoidBehavior + NkConversationEngine)
//
// CHANGEMENTS vs v2 :
//   - Le switch EmotionState → ApplyPreset est remplacé par mAIDriver.Update()
//   - AskQuestion() déclenche une réponse LLM asynchrone
//   - NkFaceControllerV2 remplace NkFaceController (micro-expressions + asymétrie)
// =============================================================================

#include "Nkentseu/Core/Layer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "PV3DE/Core/NkClinicalState.h"
#include "PV3DE/Diagnostic/NkDiagnosticEngine.h"
#include "PV3DE/Diagnostic/NkCaseLoader.h"
#include "PV3DE/Emotion/NkEmotionFSM.h"
#include "PV3DE/Body/NkBodyController.h"
#include "PV3DE/Body/NkBreathController.h"
#include "PV3DE/Speech/NkSpeechEngine.h"
#include "PV3DE/Renderer/NkPatientRenderer.h"
// Humanoid AI
#include "PV3DE/AI/NkAIDriver.h"
#include "PV3DE/Face/NkFaceControllerV2.h"

using namespace nkentseu::math;
using namespace nkentseu::humanoid;

namespace nkentseu {
    namespace pv3de {

        class PatientLayerV3 : public nkentseu::Layer {
        public:
            PatientLayerV3(const NkString& name,
                           NkIDevice* device,
                           NkICommandBuffer* cmd) noexcept;
            ~PatientLayerV3() override;

            void OnAttach() override;
            void OnDetach() override;
            void OnUpdate(nk_float32 dt) override;
            void OnRender() override;
            bool OnEvent(nkentseu::NkEvent* e) override;

            // ── API clinique ──────────────────────────────────────────────────
            void AddSymptom   (NkSymptomId id)   { mDiagnostic.AddSymptom(id);    }
            void RemoveSymptom(NkSymptomId id)    { mDiagnostic.RemoveSymptom(id); }
            void ClearSymptoms()                  { mDiagnostic.ClearSymptoms();   }
            void SetVitalSigns(nk_float32 hr,
                               nk_float32 temp,
                               nk_float32 spo2)  { mDiagnostic.SetVitalSigns(hr,temp,spo2); }
            void ForceEmotion (EmotionState state, nk_float32 intensity = 1.f) {
                mEmotionFSM.ForceState(state, intensity);
            }

            // ── Question médecin → réponse LLM (async) ───────────────────────
            // Le callback est appelé quand la réponse LLM est prête (thread principal)
            void AskQuestion(const char* question,
                             NkAIResponseCallback callback = nullptr) noexcept;

            // ── Cas clinique ──────────────────────────────────────────────────
            bool LoadCase(const char* path) noexcept;
            void StartCase() noexcept;
            void StopCase()  noexcept;

            // ── Personnalité ──────────────────────────────────────────────────
            void SetPersonality(const NkPersonality& p) noexcept;

            // ── Accès ─────────────────────────────────────────────────────────
            const NkClinicalState&  GetClinicalState()     const { return mClinicalState; }
            const NkEmotionOutput&  GetEmotionOutput()     const { return mEmotionFSM.GetOutput(); }
            const NkDiagnosticEngine& GetDiagnosticEngine() const { return mDiagnostic; }
            NkAIDriver&             GetAIDriver()                 { return mAIDriver; }
            bool                    IsThinking()           const { return mAIDriver.IsThinking(); }

        private:
            NkIDevice*        mDevice = nullptr;
            NkICommandBuffer* mCmd    = nullptr;

            // ── Sous-systèmes ─────────────────────────────────────────────────
            NkDiagnosticEngine   mDiagnostic;
            NkClinicalState      mClinicalState;
            NkEmotionFSM         mEmotionFSM;
            NkFaceControllerV2   mFaceController;  // ← v2 avec micro-expressions
            NkBodyController     mBodyController;
            NkBreathController   mBreathController;
            NkSpeechEngine       mSpeechEngine;
            NkPatientRenderer    mRenderer;

            // ── IA ────────────────────────────────────────────────────────────
            NkAIDriver           mAIDriver;         // ← NOUVEAU
            NkPersonality        mPersonality;

            // ── Cas clinique ──────────────────────────────────────────────────
            NkCaseLoader  mCaseLoader;
            NkCaseData    mCurrentCase;
            NkCaseRunner  mCaseRunner;
            bool          mCaseLoaded = false;

            // Caméra
            NkMat4f mViewProj = NkMat4f::Identity();
            NkMat4f mModel    = NkMat4f::Identity();
            NkVec3f mCamPos   = {0.f, 1.f, 3.f};
        };

    } // namespace pv3de
} // namespace nkentseu
