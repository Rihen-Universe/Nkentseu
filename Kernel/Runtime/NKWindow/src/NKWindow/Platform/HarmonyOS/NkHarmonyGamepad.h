#pragma once
// =============================================================================
// NkHarmonyGamepad.h — HarmonyOS gamepad backend (STUB)
//
// Implémentation STUB alignée sur l'interface NkIGamepad (NkGamepadSystem.h).
// Permet de COMPILER NKWindow pour HarmonyOS sans tirer OH_Input ; toutes les
// méthodes retournent des valeurs par défaut (aucune manette détectée).
//
// L'intégration réelle utilisera OH_Input_GetGamepadAxisValue (API 13+) et
// OH_Input_SetGamepadVibration (API 14+) — gardée pour plus tard quand un
// dispositif HarmonyOS pourra être testé.
//
// Interface NkIGamepad implémentée (signatures exactes) :
//   - bool Init() / void Shutdown()
//   - void Poll()
//   - uint32 GetConnectedCount() const
//   - const NkGamepadSnapshot& GetSnapshot(uint32 idx) const
//   - void Rumble(uint32, float, float, float, float, uint32)
//   - const char* GetName() const noexcept
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_HARMONYOS)

#include "NKEvent/NkGamepadSystem.h"

namespace nkentseu {

    // -------------------------------------------------------------------------
    // NkHarmonyGamepad — backend stub conforme NkIGamepad
    // -------------------------------------------------------------------------

    class NkHarmonyGamepad final : public NkIGamepad {
        public:
            NkHarmonyGamepad()           = default;
            ~NkHarmonyGamepad() override = default;

            // --- Cycle de vie ---
            bool Init()     override { return true; }
            void Shutdown() override {}

            // --- Polling (no-op stub) ---
            void Poll()     override {}

            // --- État courant ---
            uint32 GetConnectedCount() const override { return 0; }

            /// Retourne TOUJOURS le snapshot vide (connected=false) — aucun
            /// gamepad n'est détecté tant que OH_Input n'est pas câblé.
            const NkGamepadSnapshot& GetSnapshot(uint32 /*idx*/) const override {
                return mEmptySnapshot;
            }

            // --- Vibration (no-op) ---
            void Rumble(uint32 /*idx*/,
                        float32 /*motorLow*/,  float32 /*motorHigh*/,
                        float32 /*trigLeft*/,  float32 /*trigRight*/,
                        uint32  /*durationMs*/) override {}

            // --- Diagnostic ---
            const char* GetName() const noexcept override {
                return "HarmonyOS-Stub";
            }

        private:
            // Snapshot vide partagé (Clear() au construction par les valeurs
            // par défaut du POD — connected=false, axes=0, buttons=0).
            NkGamepadSnapshot mEmptySnapshot{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_HARMONYOS
