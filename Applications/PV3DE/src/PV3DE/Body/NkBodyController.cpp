#include "NkBodyController.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace pv3de {

        void NkBodyController::Init() {
            mBreath.rate      = 16.f;
            mBreath.amplitude = 0.30f;
            mBreathTarget     = mBreath;
            logger.Infof("[NkBodyController] Init\n");
        }

        void NkBodyController::ApplyEmotionOutput(const NkEmotionOutput& em) {
            // Respiration
            mBreathTarget.rate      = em.breathRate;
            mBreathTarget.amplitude = em.breathAmplitude;

            // Tremblements
            mTremorTarget = em.tremorAmplitude;

            // Pose
            NkBodyPose target = NkBodyPose::Neutral;
            switch (em.state) {
                case EmotionState::PainSevere: target = NkBodyPose::PainCurl;    break;
                case EmotionState::Anxious:
                case EmotionState::Panic:      target = NkBodyPose::Tense;       break;
                case EmotionState::Exhausted:  target = NkBodyPose::Slumped;     break;
                case EmotionState::PainMild:   target = NkBodyPose::RigidBreath; break;
                default:                       target = NkBodyPose::Neutral;     break;
            }
            if (target != mTargetPose) SetTargetPose(target, 0.6f);

            // Balancement de détresse
            mSwayTarget = (em.state == EmotionState::Panic)   ? 1.f :
                          (em.state == EmotionState::Anxious)  ? 0.4f :
                          (em.state == EmotionState::PainSevere)? 0.3f : 0.f;
        }

        void NkBodyController::SetTargetPose(NkBodyPose pose, nk_float32 blendTime) {
            if (mCurrentPose == pose) return;
            mTargetPose   = pose;
            mPoseBlend    = 0.f;
            mPoseBlendSpd = (blendTime > 0.f) ? 1.f / blendTime : 10.f;
        }

        void NkBodyController::SetTremorAmplitude(nk_float32 amp) {
            mTremorTarget = NkClamp(amp, 0.f, 1.f);
        }

        void NkBodyController::Update(nk_float32 dt) {
            UpdateBreath(dt);
            UpdateTremor(dt);
            UpdatePoseBlend(dt);
            UpdateSway(dt);
        }

        void NkBodyController::UpdateBreath(nk_float32 dt) {
            // Interpolation douce vers les paramètres cibles
            float s = NkMin(dt * 1.5f, 1.f);
            mBreath.rate      = NkLerp(mBreath.rate,      mBreathTarget.rate,      s);
            mBreath.amplitude = NkLerp(mBreath.amplitude, mBreathTarget.amplitude, s);

            // Avancement de la phase (cycle sinusoïdal)
            nk_float32 freq = mBreath.rate / 60.f; // cycles/s
            mBreath.phase  += NkTwoPi * freq * dt;
            if (mBreath.phase > NkTwoPi) mBreath.phase -= NkTwoPi;

            // Déplacement thoracique : sin déphasé pour effet organique
            mChestDisp = mBreath.amplitude *
                (0.6f * NkSin(mBreath.phase) +
                 0.3f * NkSin(mBreath.phase * 2.f + 0.4f) +
                 0.1f * NkSin(mBreath.phase * 3.f));
            mChestDisp = NkClamp(mChestDisp, -1.f, 1.f);
        }

        void NkBodyController::UpdateTremor(nk_float32 dt) {
            mTremorAmp = NkLerp(mTremorAmp, mTremorTarget, NkMin(dt * 3.f, 1.f));

            if (mTremorAmp < 0.005f) {
                mTremorOffset = {0.f, 0.f, 0.f};
                return;
            }

            mTremorTime += dt;
            // Bruit pseudo-aléatoire multi-fréquences
            nk_float32 nx = NkSin(mTremorTime * 17.3f) * 0.5f +
                            NkSin(mTremorTime * 31.7f) * 0.3f +
                            NkSin(mTremorTime * 47.1f) * 0.2f;
            nk_float32 ny = NkSin(mTremorTime * 13.1f + 1.2f) * 0.5f +
                            NkSin(mTremorTime * 27.9f + 0.8f) * 0.5f;
            nk_float32 nz = NkSin(mTremorTime * 11.7f + 2.1f) * 0.6f +
                            NkSin(mTremorTime * 23.3f + 1.5f) * 0.4f;

            nk_float32 scale = mTremorAmp * 0.004f; // amplitude en mètres
            mTremorOffset = {nx * scale, ny * scale, nz * scale};
        }

        void NkBodyController::UpdatePoseBlend(nk_float32 dt) {
            if (mCurrentPose == mTargetPose) return;
            mPoseBlend += mPoseBlendSpd * dt;
            if (mPoseBlend >= 1.f) {
                mPoseBlend   = 1.f;
                mCurrentPose = mTargetPose;
            }
        }

        void NkBodyController::UpdateSway(nk_float32 dt) {
            nk_float32 cur = NkMax(NkAbs(mHeadSwayX), NkAbs(mHeadSwayZ));
            nk_float32 amp = NkLerp(cur, mSwayTarget * 3.f, NkMin(dt * 2.f, 1.f));

            mSwayTime += dt;
            mHeadSwayX = amp * NkSin(mSwayTime * 0.7f);
            mHeadSwayZ = amp * NkSin(mSwayTime * 0.5f + 1.3f) * 0.6f;
        }

    } // namespace pv3de
} // namespace nkentseu
