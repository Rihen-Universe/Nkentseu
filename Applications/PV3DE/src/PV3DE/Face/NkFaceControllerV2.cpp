#include "NkFaceControllerV2.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace humanoid {

        void NkFaceControllerV2::ApplyBehaviorOutput(
            const NkBehaviorOutput& output, nk_float32 dt) noexcept {
            (void)dt;

            // Appliquer les face targets du behavior engine
            for (nk_usize i = 0; i < output.faceTargets.Size(); ++i) {
                const auto& ft = output.faceTargets[i];
                NkActionUnitId au = static_cast<NkActionUnitId>(ft.auId);

                if (ft.isFlash) {
                    // Micro-expression : ajouter en flash temporaire
                    AddFlash(ft.auId, ft.intensity, ft.duration);
                } else {
                    // Expression persistante : définir la cible de l'AU
                    SetAUTarget(au, ft.intensity);
                }
            }

            // Direction du regard
            if (!output.avoidEyeContact) {
                SetGazeDirection(output.gazeYaw, output.gazePitch);
            } else {
                // Regard fuyant : saccade légèrement différente à chaque fois
                SetGazeDirection(output.gazeYaw, output.gazePitch);
            }

            // Pleurs : AU de larmes (AU43 légèrement, AU1 fort, mouvement labial)
            if (output.crying) {
                SetAUTarget(NkActionUnitId::AU1, 0.6f * output.cryIntensity);
                SetAUTarget(NkActionUnitId::AU4, 0.7f * output.cryIntensity);
                // AU17 (menton qui tremble)
                SetAUTarget(NkActionUnitId::AU17, 0.5f * output.cryIntensity);
                // Yeux légèrement fermés
                SetAUTarget(NkActionUnitId::AU43, 0.3f * output.cryIntensity);
            }
        }

        void NkFaceControllerV2::UpdateV2(nk_float32 dt) noexcept {
            // Mise à jour des flash
            UpdateFlashes(dt);

            // Appel de l'Update de base (AU interpolation + blink)
            Update(dt);

            // Appliquer l'asymétrie sur les poids blendshapes finaux
            ApplyAsymmetry();
        }

        void NkFaceControllerV2::AddFlash(nk_uint8 auId,
                                           nk_float32 intensity,
                                           nk_float32 dur) noexcept {
            // Chercher un slot libre
            for (nk_uint32 i = 0; i < mFlashCount; ++i) {
                if (mFlashes[i].auId == auId) {
                    // Mettre à jour le flash existant sur cet AU
                    mFlashes[i].intensity = NkMax(mFlashes[i].intensity, intensity);
                    mFlashes[i].timer     = dur;
                    mFlashes[i].duration  = dur;
                    return;
                }
            }
            if (mFlashCount < kMaxFlashes) {
                mFlashes[mFlashCount].auId      = auId;
                mFlashes[mFlashCount].intensity = intensity;
                mFlashes[mFlashCount].timer     = dur;
                mFlashes[mFlashCount].duration  = dur;
                ++mFlashCount;
            }
        }

        void NkFaceControllerV2::UpdateFlashes(nk_float32 dt) noexcept {
            nk_uint32 write = 0;
            for (nk_uint32 i = 0; i < mFlashCount; ++i) {
                mFlashes[i].timer -= dt;
                if (mFlashes[i].timer > 0.f) {
                    // Appliquer le flash avec courbe d'enveloppe (monte puis descend)
                    float norm = mFlashes[i].timer / mFlashes[i].duration;
                    // Enveloppe triangulaire centrée
                    float env = (norm < 0.5f)
                        ? norm * 2.f
                        : (1.f - norm) * 2.f;

                    NkActionUnitId au = static_cast<NkActionUnitId>(mFlashes[i].auId);
                    // Appliquer immédiatement (écrase la valeur interpolée)
                    nk_float32 flashVal = mFlashes[i].intensity * env;
                    // On utilise SetAUImmediate pour écraser l'interpolation
                    float existing = GetAUIntensity(au);
                    SetAUImmediate(au, NkMax(existing, flashVal));

                    mFlashes[write++] = mFlashes[i];
                }
                // Flash expiré : restaure la cible normale (l'AU retrouvera
                // sa valeur interpolée naturellement au prochain Update)
            }
            mFlashCount = write;
        }

        void NkFaceControllerV2::ApplyAsymmetry() noexcept {
            if (NkAbs(mAsymmetry) < 0.01f) return;

            // mAsymmetry > 0 : côté droit plus expressif
            // mAsymmetry < 0 : côté gauche plus expressif
            // Dans les blendshapes standard, indices pairs = gauche, impairs = droite
            // (convention MetaHuman/ARKit — à adapter selon le mesh)

            auto& weights = const_cast<NkVector<nk_float32>&>(GetBlendshapeWeights());

            float leftScale  = 1.f - NkMax(0.f,  mAsymmetry) * 0.2f;
            float rightScale = 1.f - NkMax(0.f, -mAsymmetry) * 0.2f;

            for (nk_usize i = 0; i < weights.Size(); i += 2) {
                if (i     < weights.Size()) weights[i]   *= leftScale;
                if (i + 1 < weights.Size()) weights[i+1] *= rightScale;
            }
        }

    } // namespace humanoid
} // namespace nkentseu
