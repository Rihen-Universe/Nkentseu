#pragma once
// =============================================================================
// PV3DE/Body/NkBreathController.h
// =============================================================================
// Contrôleur de respiration dédié — séparé de NkBodyController pour la clarté.
// Modélise les patterns respiratoires normaux et pathologiques.
//
// Patterns supportés :
//   Normal        — respiration eupnéique 12-20 /min
//   Dyspnée       — rapide et superficielle (> 20 /min)
//   Bradypnée     — lente (< 12 /min), opioïdes, coma
//   Hyperpnée     — ample et profonde, acidose métabolique
//   CheyneStokes  — apnée cyclique avec crescendo/décrescendo
//   Kussmaul      — profonde et régulière, acidocétose
//   Biot          — irrégulière avec apnées, lésion bulbaire
//   Paradoxale    — tirage, détresse sévère
//
// Sorties utilisées par NkBodyController et NkPatientRenderer :
//   GetChestDisplacement()   — déplacement cage thoracique [-1, 1]
//   GetAbdomenDisplacement() — déplacement abdominal [-1, 1]
//   GetNasalFlareFactor()    — battement des ailes du nez [0, 1]
//   GetAccessoryUsage()      — utilisation muscles accessoires [0, 1]
//   IsInApnea()              — pause respiratoire en cours
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
    namespace pv3de {

        enum class NkBreathPattern : nk_uint8 {
            Normal      = 0,
            Dyspnea,        // dyspnée — rapide et superficielle
            Bradypnea,      // bradypnée — lente
            Hyperpnea,      // hyperpnée — ample et profonde
            CheyneStokes,   // apnée cyclique
            Kussmaul,       // profonde et régulière
            Biot,           // irrégulière avec apnées
            Paradoxical,    // tirage paradoxal
        };

        struct NkBreathParams {
            NkBreathPattern pattern   = NkBreathPattern::Normal;
            nk_float32      rate      = 16.f;   // resp/min (12–20 normal)
            nk_float32      amplitude = 0.30f;  // profondeur [0, 1]
            nk_float32      ratio     = 0.4f;   // ratio I/E (inspiration/expiration)
        };

        class NkBreathController {
        public:
            NkBreathController() noexcept { _Reset(); }

            // ── Contrôle ──────────────────────────────────────────────────────
            void SetPattern(NkBreathPattern pattern) noexcept;
            void SetRate   (nk_float32 ratePerMin)   noexcept;
            void SetAmplitude(nk_float32 amp)         noexcept;

            // Raccourcis cliniques
            void SetFromBreathingDifficulty(nk_float32 difficulty01) noexcept;

            // ── Update ────────────────────────────────────────────────────────
            void Update(nk_float32 dt) noexcept;

            // ── Sorties ───────────────────────────────────────────────────────
            nk_float32 GetChestDisplacement()   const noexcept { return mChest;     }
            nk_float32 GetAbdomenDisplacement()  const noexcept { return mAbdomen;   }
            nk_float32 GetNasalFlareFactor()     const noexcept { return mNasalFlare;}
            nk_float32 GetAccessoryUsage()       const noexcept { return mAccessory; }
            bool       IsInApnea()               const noexcept { return mInApnea;   }

            const NkBreathParams& GetParams()    const noexcept { return mCurrent;   }

        private:
            void _Reset() noexcept;
            void _UpdateNormal       (nk_float32 dt) noexcept;
            void _UpdateCheyneStokes (nk_float32 dt) noexcept;
            void _UpdateBiot         (nk_float32 dt) noexcept;
            void _UpdateParadoxical  (nk_float32 dt) noexcept;
            void _Solve              () noexcept;

            NkBreathParams mCurrent;
            NkBreathParams mTarget;

            // Phase interne
            nk_float32 mPhase        = 0.f;  // [0, 2π]
            nk_float32 mCycleTime    = 0.f;  // temps dans le cycle courant

            // CheyneStokes — enveloppe cyclique
            nk_float32 mCSEnvelope   = 0.f;  // amplitude enveloppe [0,1]
            nk_float32 mCSEnvPhase   = 0.f;  // phase de l'enveloppe
            nk_float32 mCSApneaTimer = 0.f;

            // Biot — timer d'irrégularité
            nk_float32 mBiotTimer    = 0.f;
            nk_float32 mBiotInterval = 3.f;  // prochaine apnée

            // Sorties
            nk_float32 mChest      = 0.f;
            nk_float32 mAbdomen    = 0.f;
            nk_float32 mNasalFlare = 0.f;
            nk_float32 mAccessory  = 0.f;
            bool       mInApnea    = false;
        };

    } // namespace pv3de
} // namespace nkentseu
