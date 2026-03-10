#pragma once
// =============================================================================
// NkEmscriptenGamepad.h - WASM gamepad backend via navigator.getGamepads()
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include "NKWindow/Events/NkGamepadSystem.h"

#include <array>
#include <cstdio>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace nkentseu {

    class NkEmscriptenGamepad final : public NkIGamepad {
        public:
            bool Init() override {
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
                for (auto& info : mInfos) {
                    info = {};
                }
                return true;
            }

            void Shutdown() override {
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
                for (auto& info : mInfos) {
                    info = {};
                }
            }

            void Poll() override {
#ifdef __EMSCRIPTEN__
                const EMSCRIPTEN_RESULT sampleResult = emscripten_sample_gamepad_data();
                if (sampleResult != EMSCRIPTEN_RESULT_SUCCESS) {
                    for (auto& snapshot : mSnapshots) {
                        snapshot.Clear();
                    }
                    return;
                }

                for (auto& snapshot : mSnapshots) {
                    snapshot.connected = false;
                }

                int count = emscripten_get_num_gamepads();
                if (count < 0) {
                    count = 0;
                }

                for (int i = 0; i < count && static_cast<NkU32>(i) < NK_MAX_GAMEPADS; ++i) {
                    EmscriptenGamepadEvent ev{};
                    if (emscripten_get_gamepad_status(i, &ev) != EMSCRIPTEN_RESULT_SUCCESS) {
                        continue;
                    }

                    NkGamepadSnapshot& snapshot = mSnapshots[static_cast<NkU32>(i)];
                    NkGamepadInfo& info = mInfos[static_cast<NkU32>(i)];

                    snapshot.connected = ev.connected;
                    if (!ev.connected) {
                        continue;
                    }

                    auto mapButton = [&](NkGamepadButton button, int idx) {
                        if (idx < 0 || idx >= ev.numButtons) {
                            return;
                        }
                        snapshot.buttons[static_cast<NkU32>(button)] =
                            ev.digitalButton[idx] || ev.analogButton[idx] > 0.5;
                    };

                    auto mapAxis = [&](NkGamepadAxis axis, int idx) {
                        if (idx < 0 || idx >= ev.numAxes) {
                            return;
                        }
                        snapshot.axes[static_cast<NkU32>(axis)] = ev.axis[idx];
                    };

                    mapButton(NkGamepadButton::NK_GP_SOUTH, 0);
                    mapButton(NkGamepadButton::NK_GP_EAST, 1);
                    mapButton(NkGamepadButton::NK_GP_WEST, 2);
                    mapButton(NkGamepadButton::NK_GP_NORTH, 3);
                    mapButton(NkGamepadButton::NK_GP_LB, 4);
                    mapButton(NkGamepadButton::NK_GP_RB, 5);
                    mapButton(NkGamepadButton::NK_GP_LT_DIGITAL, 6);
                    mapButton(NkGamepadButton::NK_GP_RT_DIGITAL, 7);
                    mapButton(NkGamepadButton::NK_GP_BACK, 8);
                    mapButton(NkGamepadButton::NK_GP_START, 9);
                    mapButton(NkGamepadButton::NK_GP_LSTICK, 10);
                    mapButton(NkGamepadButton::NK_GP_RSTICK, 11);
                    mapButton(NkGamepadButton::NK_GP_DPAD_UP, 12);
                    mapButton(NkGamepadButton::NK_GP_DPAD_DOWN, 13);
                    mapButton(NkGamepadButton::NK_GP_DPAD_LEFT, 14);
                    mapButton(NkGamepadButton::NK_GP_DPAD_RIGHT, 15);
                    mapButton(NkGamepadButton::NK_GP_GUIDE, 16);

                    mapAxis(NkGamepadAxis::NK_GP_AXIS_LX, 0);
                    mapAxis(NkGamepadAxis::NK_GP_AXIS_LY, 1);
                    mapAxis(NkGamepadAxis::NK_GP_AXIS_RX, 2);
                    mapAxis(NkGamepadAxis::NK_GP_AXIS_RY, 3);

                    snapshot.axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X)] =
                        (snapshot.buttons[static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] ? 1.0f : 0.0f) -
                        (snapshot.buttons[static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_LEFT)] ? 1.0f : 0.0f);
                    snapshot.axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y)] =
                        (snapshot.buttons[static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_DOWN)] ? 1.0f : 0.0f) -
                        (snapshot.buttons[static_cast<NkU32>(NkGamepadButton::NK_GP_DPAD_UP)] ? 1.0f : 0.0f);

                    info = {};
                    info.index = static_cast<NkU32>(i);
                    info.type = NkGamepadType::NK_GP_TYPE_GENERIC;
                    info.numButtons = static_cast<NkU32>(ev.numButtons);
                    info.numAxes = static_cast<NkU32>(ev.numAxes);
                    std::snprintf(info.id, sizeof(info.id), "%s", ev.id);
                    std::snprintf(info.name, sizeof(info.name), "%s", ev.id);
                    snapshot.info = info;
                }
#else
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
#endif
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

            void Rumble(NkU32, NkF32, NkF32, NkF32, NkF32, NkU32) override {
                // Browser rumble/haptics is not universally available in Emscripten C++ API.
            }

            const char* GetName() const noexcept override {
                return "WebGamepad";
            }

        private:
            std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
            std::array<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
