#pragma once
// =============================================================================
// NkNoopGamepad.h - fallback no-op gamepad backend
// =============================================================================

#include "NKWindow/Events/NkGamepadSystem.h"

#include <array>

namespace nkentseu {

    class NkNoopGamepad final : public NkIGamepad {
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

            void Poll() override {}

            NkU32 GetConnectedCount() const override {
                return 0;
            }

            const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const override {
                static NkGamepadSnapshot dummy{};
                return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : dummy;
            }

            void Rumble(NkU32, NkF32, NkF32, NkF32, NkF32, NkU32) override {}

            const char* GetName() const noexcept override {
                return "NoopGamepad";
            }

        private:
            std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    };

} // namespace nkentseu

