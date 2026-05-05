#pragma once
// =============================================================================
// NkNoopGamepad.h - fallback no-op gamepad backend
// =============================================================================

#include "NKEvent/NkGamepadSystem.h"


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

            uint32 GetConnectedCount() const override {
                return 0;
            }

            const NkGamepadSnapshot& GetSnapshot(uint32 idx) const override {
                static NkGamepadSnapshot dummy{};
                return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : dummy;
            }

            void Rumble(uint32, float32, float32, float32, float32, uint32) override {}

            const char* GetName() const noexcept override {
                return "NoopGamepad";
            }

        private:
            NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    };

} // namespace nkentseu

