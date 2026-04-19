#pragma once
// =============================================================================
// Humanoid/Face/NkFaceControllerV2.h
// =============================================================================
// Version améliorée du NkFaceController avec :
//   - Asymétrie faciale naturelle (dominant gauche/droite)
//   - Micro-expressions flash (durée < 300ms, involontaires)
//   - Gestion de l'état de repos naturel (idle movements)
//   - Connexion directe à NkHumanoidBehavior::GetOutput()
//
// Compatible avec l'API de NkFaceController existant —
// se substitue directement dans PatientLayer.
// =============================================================================

#include "../../PV3DE/src/PV3DE/Face/NkFaceController.h"
#include "../Behavior/NkHumanoidBehavior.h"

namespace nkentseu {
    namespace humanoid {

        class NkFaceControllerV2 : public pv3de::NkFaceController {
        public:
            NkFaceControllerV2() = default;

            // ── Extension : appliquer la sortie du BehaviorEngine ────────────
            void ApplyBehaviorOutput(const NkBehaviorOutput& output,
                                     nk_float32 dt) noexcept;

            // ── Update étendu (appelle Update() + gère les flash) ─────────────
            void UpdateV2(nk_float32 dt) noexcept;

            // ── Asymétrie ────────────────────────────────────────────────────
            // Facteur d'asymétrie [-1..1] : -1=gauche dominant, +1=droite dominant
            void SetAsymmetry(nk_float32 asym) noexcept { mAsymmetry = asym; }

        private:
            // Flash actifs (micro-expressions temporaires)
            struct FlashEntry {
                nk_uint8   auId       = 0;
                nk_float32 intensity  = 0.f;
                nk_float32 timer      = 0.f;   // temps restant
                nk_float32 duration   = 0.15f;
            };
            static constexpr nk_uint32 kMaxFlashes = 8;
            FlashEntry mFlashes[kMaxFlashes] = {};
            nk_uint32  mFlashCount = 0;

            nk_float32 mAsymmetry = 0.f;

            void AddFlash(nk_uint8 auId, nk_float32 intensity, nk_float32 dur) noexcept;
            void UpdateFlashes(nk_float32 dt) noexcept;
            void ApplyAsymmetry() noexcept;
        };

    } // namespace humanoid
} // namespace nkentseu
