#include "NkFaceController.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include <string.h>

namespace nkentseu {
    namespace pv3de {

        NkFaceController::NkFaceController() {
            for (nk_uint32 i = 0; i < kAUCount; ++i) {
                mAUs[i].id        = static_cast<NkActionUnitId>(i + 1);
                mAUs[i].intensity = 0.f;
                mAUs[i].target    = 0.f;
                mAUs[i].speed     = 3.5f;
            }
        }

        void NkFaceController::Init(nk_uint32 blendshapeCount) {
            mBlendshapeCount = blendshapeCount;
            mBlendshapeWeights.Resize(blendshapeCount, 0.f);
            logger.Infof("[NkFaceController] Init — {} blendshapes\n", blendshapeCount);
        }

        void NkFaceController::BindAU(NkActionUnitId au, nk_uint32 bsIndex,
                                      nk_float32 weight) {
            NkBlendshapeBinding b;
            b.au             = au;
            b.blendshapeIndex = bsIndex;
            b.weight          = weight;
            mBindings.PushBack(b);
        }

        void NkFaceController::ApplyNeutral() {
            for (nk_uint32 i = 0; i < kAUCount; ++i) {
                mAUs[i].target = 0.f;
                mAUs[i].speed  = 2.f; // retour rapide au neutre
            }
        }

        void NkFaceController::SetAUTarget(NkActionUnitId au, nk_float32 target) {
            nk_uint32 idx = static_cast<nk_uint32>(au);
            if (idx == 0 || idx > kAUCount) return;
            mAUs[idx - 1].target = NkClamp(target, 0.f, 1.f);
            mAUs[idx - 1].speed  = 3.5f;
        }

        void NkFaceController::SetAUImmediate(NkActionUnitId au, nk_float32 v) {
            nk_uint32 idx = static_cast<nk_uint32>(au);
            if (idx == 0 || idx > kAUCount) return;
            mAUs[idx - 1].intensity = NkClamp(v, 0.f, 1.f);
            mAUs[idx - 1].target    = mAUs[idx - 1].intensity;
        }

        nk_float32 NkFaceController::GetAUIntensity(NkActionUnitId au) const {
            nk_uint32 idx = static_cast<nk_uint32>(au);
            if (idx == 0 || idx > kAUCount) return 0.f;
            return mAUs[idx - 1].intensity;
        }

        void NkFaceController::ForceBlink() {
            mBlinking   = true;
            mBlinkPhase = 0.f;
        }

        void NkFaceController::SetGazeDirection(nk_float32 yawDeg, nk_float32 pitchDeg) {
            mGazeYaw   = NkClamp(yawDeg,   -30.f, 30.f);
            mGazePitch = NkClamp(pitchDeg, -20.f, 20.f);
        }

        // =====================================================================
        void NkFaceController::Update(nk_float32 dt) {
            // Mise à jour de tous les AUs (interpolation)
            for (nk_uint32 i = 0; i < kAUCount; ++i) {
                mAUs[i].Update(dt);
            }

            // Clignement automatique
            UpdateBlink(dt);

            // Résolution blendshapes
            SolveBlendshapes();
        }

        void NkFaceController::UpdateBlink(nk_float32 dt) {
            if (!mBlinkEnabled) return;

            mBlinkTimer += dt;

            if (!mBlinking && mBlinkTimer >= mBlinkInterval) {
                mBlinking   = true;
                mBlinkPhase = 0.f;
                mBlinkTimer = 0.f;
                // Prochain intervalle : 3–7 secondes (pseudo-aléatoire simple)
                mBlinkInterval = 3.f + NkFmod(mBlinkTimer * 1234.5f, 4.f);
            }

            if (mBlinking) {
                mBlinkPhase += dt * 8.f; // vitesse clignement (~125ms total)
                // Phase 0–0.5 : fermeture, 0.5–1.0 : réouverture
                nk_float32 closedness = (mBlinkPhase < 0.5f)
                    ? mBlinkPhase * 2.f
                    : (1.f - mBlinkPhase) * 2.f;
                closedness = NkClamp(closedness, 0.f, 1.f);
                SetAUImmediate(NkActionUnitId::AU43, closedness);
                if (mBlinkPhase >= 1.f) {
                    SetAUImmediate(NkActionUnitId::AU43, 0.f);
                    mBlinking = false;
                }
            }
        }

        void NkFaceController::SolveBlendshapes() {
            if (mBlendshapeWeights.IsEmpty()) return;

            // Reset
            for (nk_uint32 i = 0; i < mBlendshapeCount; ++i) {
                mBlendshapeWeights[i] = 0.f;
            }

            // Accumulation AU → blendshapes
            for (const auto& b : mBindings) {
                nk_uint32 auIdx = static_cast<nk_uint32>(b.au);
                if (auIdx == 0 || auIdx > kAUCount) continue;
                nk_float32 v = mAUs[auIdx - 1].intensity * b.weight;
                if (b.blendshapeIndex < mBlendshapeCount) {
                    mBlendshapeWeights[b.blendshapeIndex] =
                        NkClamp(mBlendshapeWeights[b.blendshapeIndex] + v, 0.f, 1.f);
                }
            }
        }

    } // namespace pv3de
} // namespace nkentseu
