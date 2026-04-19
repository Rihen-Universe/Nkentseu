#include "PatientLayer.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace pv3de {

        PatientLayer::PatientLayer(const NkString& name,
                                   NkIDevice* device,
                                   NkICommandBuffer* cmd) noexcept
            : Layer(name), mDevice(device), mCmd(cmd) {}

        PatientLayer::~PatientLayer() = default;

        // =====================================================================
        void PatientLayer::OnAttach() {
            mDiagnostic.Init();
            mEmotionFSM.Init();
            mFaceController.Init(64);
            mBodyController.Init();
            mSpeechEngine.Init();

            // Liaison AU → blendshapes standard (par convention mesh PV3DE)
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
            mFaceController.BindAU(NkActionUnitId::AU45, 14);

            // Charger les BDD JSON si disponibles
            mDiagnostic.LoadSymptomDatabase("Data/symptoms.json");
            mDiagnostic.LoadDiseaseDatabase("Data/diseases.json");

            // Initialiser le renderer
            SetupRenderer();

            logger.Infof("[PatientLayer] v2 initialisé\n");
        }

        void PatientLayer::OnDetach() {
            if (mDevice) mDevice->WaitIdle();
            logger.Infof("[PatientLayer] Détaché\n");
        }

        // =====================================================================
        void PatientLayer::OnUpdate(nk_float32 dt) {
            // ── 1. Diagnostic → ClinicalState ─────────────────────────────────
            mDiagnostic.Update(mClinicalState);
            mClinicalState.DeduceEmotion();

            // ── 2. ClinicalState → EmotionFSM ────────────────────────────────
            mEmotionFSM.Update(mClinicalState, dt);
            const NkEmotionOutput& em = mEmotionFSM.GetOutput();

            // ── 3. EmotionFSM → FaceController ───────────────────────────────
            switch (em.state) {
                case EmotionState::PainSevere:
                    mFaceController.ApplyPreset(kAU_PainSevere, em.intensity); break;
                case EmotionState::PainMild:
                case EmotionState::Discomfort:
                    mFaceController.ApplyPreset(kAU_PainMild, em.intensity); break;
                case EmotionState::Anxious:
                case EmotionState::Panic:
                    mFaceController.ApplyPreset(kAU_Anxious, em.intensity); break;
                case EmotionState::Nauseous:
                    mFaceController.ApplyPreset(kAU_Nauseous, em.intensity); break;
                case EmotionState::Exhausted:
                    mFaceController.ApplyPreset(kAU_Exhausted, em.intensity); break;
                default:
                    mFaceController.ApplyNeutral(); break;
            }
            mFaceController.Update(dt);

            // ── 4. EmotionFSM → BodyController ───────────────────────────────
            mBodyController.ApplyEmotionOutput(em);
            mBodyController.Update(dt);

            // ── 5. ClinicalState → BreathController ──────────────────────────
            mBreathController.SetFromBreathingDifficulty(
                mClinicalState.breathingDifficulty);
            mBreathController.Update(dt);

            // ── 6. SpeechEngine → LipSync ────────────────────────────────────
            mSpeechEngine.Update(dt, mFaceController);

            // ── 7. Cas clinique scriptés ──────────────────────────────────────
            if (mCaseLoaded && mCaseRunner.IsRunning()) {
                const auto& events = mCaseRunner.Update(dt, mCaseLoader);
                for (nk_usize i = 0; i < events.Size(); ++i)
                    DispatchCaseEvent(*events[i]);
            }
        }

        // =====================================================================
        void PatientLayer::OnRender() {
            if (!mRenderer.IsReady()) return;

            mRenderer.UpdateFromSystems(mFaceController,
                                         mBodyController,
                                         mClinicalState);
            mRenderer.Draw(mCmd, mViewProj, mModel, mCamPos);
        }

        bool PatientLayer::OnEvent(nkentseu::NkEvent* /*e*/) { return false; }

        // =====================================================================
        void PatientLayer::Speak(const char* text) noexcept {
            NkSpeechRequest req;
            req.text      = NkString(text);
            req.pitchMult = 1.f + mEmotionFSM.GetOutput().voicePitch - 1.f;
            req.rateMult  = mEmotionFSM.GetOutput().speechRate;
            req.breathAmt = mEmotionFSM.GetOutput().voiceBreath;
            req.tremorAmt = mEmotionFSM.GetOutput().voiceTremor;
            mSpeechEngine.Speak(req, mFaceController);
        }

        void PatientLayer::AnswerQuestion(const char* question) noexcept {
            if (!mCaseLoaded) {
                // Réponse générique basée sur l'état clinique
                NkString answer = NkSpeechEngine::GetResponse(
                    NkString(question),
                    mClinicalState.painLevel,
                    mClinicalState.breathingDifficulty);
                Speak(answer.CStr());
                return;
            }
            // Chercher dans la BDD Q/A du cas
            const NkQAPair* qa = mCaseLoader.FindAnswer(mCurrentCase, question);
            if (qa) Speak(qa->answer.CStr());
            else    Speak("Je... je ne sais pas.");
        }

        // =====================================================================
        bool PatientLayer::LoadCase(const char* path) noexcept {
            if (!mCaseLoader.Load(path, mCurrentCase)) return false;
            mCaseLoaded = true;
            // Appliquer l'état initial
            const auto& init = mCurrentCase.initialState;
            mDiagnostic.ClearSymptoms();
            for (nk_usize i = 0; i < init.symptoms.Size(); ++i)
                mDiagnostic.AddSymptom(init.symptoms[i]);
            mDiagnostic.SetVitalSigns(init.heartRate, init.temperature, init.spo2);
            return true;
        }

        void PatientLayer::StartCase() noexcept {
            if (!mCaseLoaded) return;
            mCaseRunner.Load(&mCurrentCase);
        }

        void PatientLayer::StopCase() noexcept {
            mCaseRunner.Reset();
        }

        void PatientLayer::SetCamera(const NkMat4f& vp,
                                      const NkMat4f& m,
                                      const NkVec3f& pos) noexcept {
            mViewProj = vp;
            mModel    = m;
            mCamPos   = pos;
        }

        // =====================================================================
        void PatientLayer::SetupRenderer() noexcept {
            if (!mDevice) return;
            NkPatientRenderConfig cfg;
            if (!mRenderer.Init(mDevice, mCmd, cfg)) {
                logger.Warnf("[PatientLayer] Renderer non disponible "
                             "(assets manquants — mode logique seul)\n");
            }
        }

        void PatientLayer::DispatchCaseEvent(const NkCaseEvent& ev) noexcept {
            switch (ev.type) {
                case NkCaseEventType::AddSymptom:
                    mDiagnostic.AddSymptom(ev.symptomId);
                    logger.Infof("[Case] +symptôme {}\n", ev.symptomId);
                    break;
                case NkCaseEventType::RemoveSymptom:
                    mDiagnostic.RemoveSymptom(ev.symptomId);
                    break;
                case NkCaseEventType::SetPain: {
                    // Injection directe de la douleur via symptôme virtuel
                    mClinicalState.painLevel = ev.floatValue;
                    break;
                }
                case NkCaseEventType::SetVitals:
                    mDiagnostic.SetVitalSigns(ev.heartRate, ev.temperature, ev.spo2);
                    break;
                case NkCaseEventType::ForceEmotion: {
                    // Parser le nom de l'émotion
                    const NkString& name = ev.stringValue;
                    EmotionState state = EmotionState::Neutral;
                    if (name == "PainMild")   state = EmotionState::PainMild;
                    if (name == "PainSevere") state = EmotionState::PainSevere;
                    if (name == "Anxious")    state = EmotionState::Anxious;
                    if (name == "Panic")      state = EmotionState::Panic;
                    if (name == "Nauseous")   state = EmotionState::Nauseous;
                    if (name == "Exhausted")  state = EmotionState::Exhausted;
                    mEmotionFSM.ForceState(state, ev.floatValue > 0.f ? ev.floatValue : 1.f);
                    break;
                }
                case NkCaseEventType::PlaySpeech:
                    Speak(ev.stringValue.CStr());
                    break;
                case NkCaseEventType::SetBreathPattern: {
                    NkBreathPattern pat = NkBreathPattern::Normal;
                    if (ev.stringValue == "Dyspnea")      pat = NkBreathPattern::Dyspnea;
                    if (ev.stringValue == "CheyneStokes") pat = NkBreathPattern::CheyneStokes;
                    if (ev.stringValue == "Kussmaul")     pat = NkBreathPattern::Kussmaul;
                    mBreathController.SetPattern(pat);
                    break;
                }
                default: break;
            }
        }

    } // namespace pv3de
} // namespace nkentseu
