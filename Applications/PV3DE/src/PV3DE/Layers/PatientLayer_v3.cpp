#include "PatientLayer_v3.h"
#include "NKLogger/NkLog.h"
#include "Nkentseu/Components/Rendering/NkRenderComponents.h"

namespace nkentseu {
    namespace pv3de {

        PatientLayerV3::PatientLayerV3(const NkString& name,
                                       NkIDevice* device,
                                       NkICommandBuffer* cmd) noexcept
            : Layer(name), mDevice(device), mCmd(cmd)
            , mPersonality(NkPersonality::CooperativePatient()) {}

        PatientLayerV3::~PatientLayerV3() = default;

        void PatientLayerV3::OnAttach() {
            mDiagnostic.Init();
            mEmotionFSM.Init();
            mFaceController.Init(64);
            mBodyController.Init();
            mSpeechEngine.Init();

            // Bindings AU → blendshapes
            mFaceController.BindAU(NkActionUnitId::AU1,  0);
            mFaceController.BindAU(NkActionUnitId::AU2,  1);
            mFaceController.BindAU(NkActionUnitId::AU4,  2);
            mFaceController.BindAU(NkActionUnitId::AU5,  3);
            mFaceController.BindAU(NkActionUnitId::AU6,  4);
            mFaceController.BindAU(NkActionUnitId::AU7,  5);
            mFaceController.BindAU(NkActionUnitId::AU9,  6);
            mFaceController.BindAU(NkActionUnitId::AU12, 7);
            mFaceController.BindAU(NkActionUnitId::AU15, 8);
            mFaceController.BindAU(NkActionUnitId::AU17, 9);
            mFaceController.BindAU(NkActionUnitId::AU20, 10);
            mFaceController.BindAU(NkActionUnitId::AU25, 11);
            mFaceController.BindAU(NkActionUnitId::AU26, 12);
            mFaceController.BindAU(NkActionUnitId::AU43, 13);
            mFaceController.BindAU(NkActionUnitId::AU43, 14);
            mFaceController.SetBlinkEnabled(true);

            // Asymétrie faciale depuis la personnalité
            mFaceController.SetAsymmetry(mPersonality.traits.facialAsymmetry);

            // Charger BDD
            mDiagnostic.LoadSymptomDatabase("Data/symptoms.json");
            mDiagnostic.LoadDiseaseDatabase("Data/diseases.json");

            // Init NkAIDriver
            NkAIDriverConfig aiConfig;
            aiConfig.backendType       = NkConvBackendType::OllamaLocal;
            aiConfig.modelName         = "mistral:7b-instruct";
            aiConfig.endpoint          = "http://localhost:11434";
            aiConfig.asyncResponse     = true;
            aiConfig.enableBehavior    = true;
            aiConfig.enableConversation= true;
            mAIDriver.Init(aiConfig, mPersonality);

            // Callback par défaut : réponse → SpeechEngine
            mAIDriver.SetResponseCallback(
                [this](const NkString& text, nk_float32 emotionIntensity) {
                    // Parler avec les paramètres vocaux de l'émotion courante
                    NkSpeechRequest req;
                    req.text       = text;
                    req.pitchMult  = mEmotionFSM.GetOutput().voicePitch;
                    req.rateMult   = mEmotionFSM.GetOutput().speechRate;
                    req.breathAmt  = mEmotionFSM.GetOutput().voiceBreath;
                    req.tremorAmt  = mEmotionFSM.GetOutput().voiceTremor;
                    mSpeechEngine.Speak(req, mFaceController);

                    // Ajouter un stimulus si la réponse est émotionnellement forte
                    if (emotionIntensity > 0.5f)
                        mAIDriver.GetBehavior().OnPain(emotionIntensity * 0.8f);
                });

            logger.Infof("[PatientLayer v3] Init — NkAIDriver + NkFaceControllerV2\n");
        }

        void PatientLayerV3::OnDetach() {
            if (mDevice) mDevice->WaitIdle();
        }

        // =====================================================================
        void PatientLayerV3::OnUpdate(nk_float32 dt) {
            // ── 1. Diagnostic → ClinicalState ────────────────────────────────
            mDiagnostic.Update(mClinicalState);
            mClinicalState.DeduceEmotion();

            // ── 2. ClinicalState → EmotionFSM ────────────────────────────────
            mEmotionFSM.Update(mClinicalState, dt);

            // ── 3. NkAIDriver (remplace le switch EmotionState → ApplyPreset)
            //       → NkHumanoidBehavior génère les AUs et le regard
            mAIDriver.Update(dt,
                             mClinicalState.painLevel,
                             mClinicalState.anxietyLevel,
                             mClinicalState.fatigueLevel);

            // ── 4. Appliquer la sortie du BehaviorEngine sur le visage ────────
            const auto& behOutput = mAIDriver.GetBehavior().GetOutput();
            mFaceController.ApplyBehaviorOutput(behOutput, dt);

            // Compléter avec les presets FACS selon l'état émotionnel
            // (le BehaviorEngine gère les réactions, les presets gèrent l'état de fond)
            const auto& em = mEmotionFSM.GetOutput();
            float bgScale = 1.f - em.blendT * 0.3f; // atténuer pendant transition
            switch (em.state) {
                case EmotionState::PainSevere:
                    mFaceController.ApplyPreset(kAU_PainSevere, em.intensity * bgScale); break;
                case EmotionState::PainMild:
                    mFaceController.ApplyPreset(kAU_PainMild,   em.intensity * bgScale); break;
                case EmotionState::Anxious:
                case EmotionState::Panic:
                    mFaceController.ApplyPreset(kAU_Anxious,    em.intensity * bgScale); break;
                case EmotionState::Nauseous:
                    mFaceController.ApplyPreset(kAU_Nauseous,   em.intensity * bgScale); break;
                case EmotionState::Exhausted:
                    mFaceController.ApplyPreset(kAU_Exhausted,  em.intensity * bgScale); break;
                default: break;
            }

            // ── 5. UpdateV2 (interpolation AUs + flash + asymétrie) ──────────
            mFaceController.UpdateV2(dt);

            // ── 6. Corps + Respiration ────────────────────────────────────────
            mBodyController.ApplyEmotionOutput(em);
            mBodyController.Update(dt);

            mBreathController.SetFromBreathingDifficulty(mClinicalState.breathingDifficulty);
            mBreathController.Update(dt);

            // Connecter la respiration au regard (inspiration légère = tête qui se lève)
            float chestDisp = mBreathController.GetChestDisplacement();
            mFaceController.SetGazeDirection(
                behOutput.gazeYaw,
                behOutput.gazePitch + chestDisp * 3.f // effet subtil
            );

            // ── 7. SpeechEngine ───────────────────────────────────────────────
            mSpeechEngine.Update(dt, mFaceController);

            // ── 8. Cas clinique ───────────────────────────────────────────────
            if (mCaseLoaded && mCaseRunner.IsRunning()) {
                const auto& events = mCaseRunner.Update(dt, mCaseLoader);
                for (nk_usize i = 0; i < events.Size(); ++i) {
                    const auto& ev = *events[i];
                    switch (ev.type) {
                        case NkCaseEventType::AddSymptom:
                            mDiagnostic.AddSymptom(ev.symptomId);
                            mAIDriver.GetBehavior().OnPain(ev.floatValue / 10.f);
                            break;
                        case NkCaseEventType::SetPain:
                            mClinicalState.painLevel = ev.floatValue;
                            mAIDriver.GetBehavior().OnPain(ev.floatValue / 10.f);
                            break;
                        case NkCaseEventType::PlaySpeech:
                            AskQuestion(nullptr); // auto-réponse
                            break;
                        default: break;
                    }
                }
            }
        }

        void PatientLayerV3::OnRender() {
            mRenderer.UpdateFromSystems(mFaceController, mBodyController, mClinicalState);
            mRenderer.Draw(mCmd, mViewProj, mModel, mCamPos);
        }

        bool PatientLayerV3::OnEvent(nkentseu::NkEvent* /*e*/) { return false; }

        // =====================================================================
        void PatientLayerV3::AskQuestion(const char* question,
                                          NkAIResponseCallback callback) noexcept {
            if (!question) return;

            const auto& cs = mClinicalState;
            mAIDriver.AskQuestion(
                question,
                cs.painLevel, cs.anxietyLevel, cs.fatigueLevel,
                cs.breathingDifficulty, cs.heartRate, cs.temperature, cs.spo2,
                callback
            );
        }

        void PatientLayerV3::SetPersonality(const NkPersonality& p) noexcept {
            mPersonality = p;
            mFaceController.SetAsymmetry(p.traits.facialAsymmetry);
            // Réinitialiser le behavior avec la nouvelle personnalité
            NkAIDriverConfig cfg;
            cfg.backendType = NkConvBackendType::OllamaLocal;
            cfg.asyncResponse = true;
            cfg.enableBehavior = true;
            cfg.enableConversation = true;
            mAIDriver.Init(cfg, p);
        }

        bool PatientLayerV3::LoadCase(const char* path) noexcept {
            if (!mCaseLoader.Load(path, mCurrentCase)) return false;
            mCaseLoaded = true;
            const auto& init = mCurrentCase.initialState;
            mDiagnostic.ClearSymptoms();
            for (nk_usize i = 0; i < init.symptoms.Size(); ++i)
                mDiagnostic.AddSymptom(init.symptoms[i]);
            mDiagnostic.SetVitalSigns(init.heartRate, init.temperature, init.spo2);
            return true;
        }

        void PatientLayerV3::StartCase() noexcept {
            if (!mCaseLoaded) return;
            mCaseRunner.Load(&mCurrentCase);
        }

        void PatientLayerV3::StopCase() noexcept {
            mCaseRunner.Reset();
        }

    } // namespace pv3de
} // namespace nkentseu
