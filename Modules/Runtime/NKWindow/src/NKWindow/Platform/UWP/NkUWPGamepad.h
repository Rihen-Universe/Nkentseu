#pragma once
// =============================================================================
// NkUWPGamepad.h - UWP gamepad backend (safe fallback implementation)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_UWP)

#include "NKEvent/NkGamepadSystem.h"


namespace nkentseu {

    class NkUWPGamepad final : public NkIGamepad {
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
                // Placeholder backend: keeps slots disconnected until native
                // UWP/Xbox runtime polling is wired.
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
                return "UWPGamepad";
            }

        private:
            NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_UWP
