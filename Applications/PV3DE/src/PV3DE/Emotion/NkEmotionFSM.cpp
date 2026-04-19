#include "NkEmotionFSM.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace pv3de {

        NkEmotionFSM::NkEmotionFSM() = default;

        void NkEmotionFSM::Init() {
            // Enregistrement des transitions principales
            auto AddT = [&](EmotionState f, EmotionState t, nk_float32 blend, nk_float32 minI) {
                NkEmotionTransition tr;
                tr.from = f; tr.to = t;
                tr.blendTime = blend; tr.minIntensity = minI;
                mTransitions.PushBack(tr);
            };

            // Neutre → états activés
            AddT(EmotionState::Neutral,    EmotionState::Discomfort,  0.4f, 0.1f);
            AddT(EmotionState::Neutral,    EmotionState::Anxious,     0.5f, 0.3f);
            AddT(EmotionState::Neutral,    EmotionState::Nauseous,    0.6f, 0.3f);
            AddT(EmotionState::Neutral,    EmotionState::Exhausted,   0.8f, 0.4f);

            // Progression de la douleur
            AddT(EmotionState::Discomfort, EmotionState::PainMild,   0.4f, 0.4f);
            AddT(EmotionState::PainMild,   EmotionState::PainSevere, 0.3f, 0.7f);

            // Anxiété → panique
            AddT(EmotionState::Anxious,    EmotionState::Panic,       0.3f, 0.8f);

            // Retours vers neutre (toujours possibles)
            for (nk_uint8 s = 1; s < static_cast<nk_uint8>(EmotionState::COUNT); ++s) {
                AddT(static_cast<EmotionState>(s), EmotionState::Neutral, 1.0f, 0.0f);
            }

            logger.Infof("[NkEmotionFSM] Init — {} transitions\n", mTransitions.Size());
        }

        // =====================================================================
        void NkEmotionFSM::Update(const NkClinicalState& clinical, nk_float32 dt) {
            EmotionState targetState    = clinical.emotionState;
            nk_float32   targetIntensity = clinical.emotionIntensity;

            // Chercher si une transition s'applique
            if (targetState != mCurrentState) {
                nk_float32 blendDur = 0.5f;
                for (const auto& t : mTransitions) {
                    if (t.from == mCurrentState && t.to == targetState
                        && targetIntensity >= t.minIntensity) {
                        blendDur = t.blendTime;
                        break;
                    }
                }
                mPreviousState  = mCurrentState;
                mCurrentState   = targetState;
                mBlendDuration  = blendDur;
                mBlendTimer     = 0.f;
                mTransitioning  = true;
            }

            // Avancement de la transition
            if (mTransitioning) {
                mBlendTimer += dt;
                if (mBlendTimer >= mBlendDuration) {
                    mBlendTimer    = mBlendDuration;
                    mTransitioning = false;
                }
            }

            // Interpolation intensité
            mIntensity = NkLerp(mIntensity, targetIntensity, NkMin(dt * 3.f, 1.f));

            ComputeOutput();
        }

        void NkEmotionFSM::ForceState(EmotionState state, nk_float32 intensity) {
            mPreviousState = mCurrentState;
            mCurrentState  = state;
            mIntensity     = NkClamp(intensity, 0.f, 1.f);
            mBlendTimer    = 0.f;
            mTransitioning = false;
            ComputeOutput();
        }

        // =====================================================================
        void NkEmotionFSM::ComputeOutput() {
            nk_float32 blendT = (mBlendDuration > 0.f)
                ? NkClamp(mBlendTimer / mBlendDuration, 0.f, 1.f)
                : 1.f;

            mOutput.state     = mCurrentState;
            mOutput.intensity = mIntensity;
            mOutput.blendT    = blendT;

            ApplyBodyParams(mCurrentState, mIntensity);
            ApplyVoiceParams(mCurrentState, mIntensity);
        }

        void NkEmotionFSM::ApplyBodyParams(EmotionState state, nk_float32 intensity) {
            // Valeurs de base
            mOutput.breathRate      = 16.f;
            mOutput.breathAmplitude = 0.3f;
            mOutput.tremorAmplitude = 0.f;

            switch (state) {
                case EmotionState::PainMild:
                    mOutput.breathRate      = 16.f + 4.f * intensity;
                    mOutput.breathAmplitude = 0.3f + 0.1f * intensity;
                    break;
                case EmotionState::PainSevere:
                    mOutput.breathRate      = 22.f + 8.f * intensity;
                    mOutput.breathAmplitude = 0.5f + 0.2f * intensity;
                    mOutput.tremorAmplitude = 0.2f * intensity;
                    break;
                case EmotionState::Anxious:
                    mOutput.breathRate      = 18.f + 6.f * intensity;
                    mOutput.breathAmplitude = 0.4f;
                    mOutput.tremorAmplitude = 0.1f * intensity;
                    break;
                case EmotionState::Panic:
                    mOutput.breathRate      = 28.f;
                    mOutput.breathAmplitude = 0.7f;
                    mOutput.tremorAmplitude = 0.4f;
                    break;
                case EmotionState::Exhausted:
                    mOutput.breathRate      = 12.f;
                    mOutput.breathAmplitude = 0.2f;
                    break;
                case EmotionState::Nauseous:
                    mOutput.breathRate      = 14.f;
                    mOutput.breathAmplitude = 0.25f;
                    break;
                default: break;
            }
        }

        void NkEmotionFSM::ApplyVoiceParams(EmotionState state, nk_float32 intensity) {
            mOutput.voicePitch  = 1.f;
            mOutput.voiceBreath = 0.f;
            mOutput.voiceTremor = 0.f;
            mOutput.speechRate  = 1.f;

            switch (state) {
                case EmotionState::PainMild:
                    mOutput.voiceBreath = 0.2f * intensity;
                    mOutput.speechRate  = 0.95f;
                    break;
                case EmotionState::PainSevere:
                    mOutput.voicePitch  = 1.1f + 0.1f * intensity;
                    mOutput.voiceBreath = 0.5f * intensity;
                    mOutput.voiceTremor = 0.3f * intensity;
                    mOutput.speechRate  = 0.8f;
                    break;
                case EmotionState::Anxious:
                    mOutput.voicePitch  = 1.05f;
                    mOutput.voiceBreath = 0.1f;
                    mOutput.speechRate  = 1.2f;
                    break;
                case EmotionState::Panic:
                    mOutput.voicePitch  = 1.2f;
                    mOutput.voiceBreath = 0.4f;
                    mOutput.voiceTremor = 0.5f;
                    mOutput.speechRate  = 1.5f;
                    break;
                case EmotionState::Exhausted:
                    mOutput.voicePitch  = 0.95f;
                    mOutput.speechRate  = 0.7f;
                    mOutput.voiceBreath = 0.3f;
                    break;
                default: break;
            }
        }

    } // namespace pv3de
} // namespace nkentseu
