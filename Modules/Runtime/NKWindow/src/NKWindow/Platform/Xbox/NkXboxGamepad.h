#pragma once
// =============================================================================
// NkXboxGamepad.h - Xbox gamepad backend (safe fallback implementation)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_XBOX)

#include "NKWindow/Events/NkGamepadSystem.h"


namespace nkentseu {

    class NkXboxGamepad final : public NkIGamepad {
        public:
            bool Init() override {
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
                return true;
            }

            void Shutdown() override {
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
            }

            void Poll() override {
                // TODO: brancher runtime Xbox natif (GDK/XInput avancÃ©).
                for (auto& snapshot : mSnapshots) {
                    snapshot.connected = false;
                }
            }

            uint32 GetConnectedCount() const override {
                uint32 count = 0;
                for (const auto& snapshot : mSnapshots) {
                    if (snapshot.connected) {
                        ++count;
                    }
                }
                return count;
            }

            const NkGamepadSnapshot& GetSnapshot(uint32 idx) const override {
                static NkGamepadSnapshot dummy{};
                return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : dummy;
            }

            void Rumble(uint32, float32, float32, float32, float32, uint32) override {}

            const char* GetName() const noexcept override {
                return "XboxGamepad";
            }

        private:
            NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_XBOX
