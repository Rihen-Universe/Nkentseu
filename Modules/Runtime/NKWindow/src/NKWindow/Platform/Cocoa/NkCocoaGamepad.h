#pragma once
// =============================================================================
// NkCocoaGamepad.h - Cocoa gamepad backend
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#include "NKWindow/Events/NkGamepadSystem.h"

#include <array>

namespace nkentseu {

    class NkCocoaGamepad final : public NkIGamepad {
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
                // Native GCController polling can be plugged here for macOS.
                for (auto& snapshot : mSnapshots) {
                    snapshot.connected = false;
                }
            }

            NkU32 GetConnectedCount() const override {
                NkU32 count = 0;
                for (const auto& snapshot : mSnapshots) {
                    if (snapshot.connected) {
                        ++count;
                    }
                }
                return count;
            }

            const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const override {
                static NkGamepadSnapshot dummy{};
                return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : dummy;
            }

            void Rumble(NkU32, NkF32, NkF32, NkF32, NkF32, NkU32) override {}

            const char* GetName() const noexcept override {
                return "CocoaGamepad";
            }

        private:
            std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS

