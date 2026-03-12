#pragma once
// =============================================================================
// NkEmscriptenGamepad.h - WASM gamepad backend via navigator.getGamepads()
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include "NKWindow/Events/NkGamepadSystem.h"

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
#ifdef __EMSCRIPTEN__
                if (!mCallbacksRegistered) {
                    const EMSCRIPTEN_RESULT c =
                        emscripten_set_gamepadconnected_callback(this, EM_FALSE, &NkEmscriptenGamepad::OnConnected);
                    const EMSCRIPTEN_RESULT d =
                        emscripten_set_gamepaddisconnected_callback(this, EM_FALSE, &NkEmscriptenGamepad::OnDisconnected);
                    mCallbacksRegistered = (c == EMSCRIPTEN_RESULT_SUCCESS && d == EMSCRIPTEN_RESULT_SUCCESS);
                }
#endif
                return true;
            }

            void Shutdown() override {
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
                for (auto& info : mInfos) {
                    info = {};
                }
#ifdef __EMSCRIPTEN__
                if (mCallbacksRegistered) {
                    emscripten_set_gamepadconnected_callback(nullptr, EM_FALSE, nullptr);
                    emscripten_set_gamepaddisconnected_callback(nullptr, EM_FALSE, nullptr);
                    mCallbacksRegistered = false;
                }
#endif
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
                int maxProbe = count > 0 ? count : static_cast<int>(NK_MAX_GAMEPADS);
                if (maxProbe > static_cast<int>(NK_MAX_GAMEPADS)) {
                    maxProbe = static_cast<int>(NK_MAX_GAMEPADS);
                }

                for (int i = 0; i < maxProbe && static_cast<uint32>(i) < NK_MAX_GAMEPADS; ++i) {
                    EmscriptenGamepadEvent ev{};
                    if (emscripten_get_gamepad_status(i, &ev) != EMSCRIPTEN_RESULT_SUCCESS) {
                        continue;
                    }

                    NkGamepadSnapshot& snapshot = mSnapshots[static_cast<uint32>(i)];
                    NkGamepadInfo& info = mInfos[static_cast<uint32>(i)];

                    snapshot.connected = ev.connected;
                    if (!ev.connected) {
                        continue;
                    }

                    auto mapButton = [&](NkGamepadButton button, int idx) {
                        if (idx < 0 || idx >= ev.numButtons) {
                            return;
                        }
                        snapshot.buttons[static_cast<uint32>(button)] =
                            ev.digitalButton[idx] || ev.analogButton[idx] > 0.5;
                    };

                    auto mapAxis = [&](NkGamepadAxis axis, int idx) {
                        if (idx < 0 || idx >= ev.numAxes) {
                            return;
                        }
                        snapshot.axes[static_cast<uint32>(axis)] = ev.axis[idx];
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
                    auto clamp01 = [](double v) -> float32 {
                        if (v < 0.0) return 0.f;
                        if (v > 1.0) return 1.f;
                        return static_cast<float32>(v);
                    };
                    float32 lt = 0.f;
                    float32 rt = 0.f;
                    if (ev.numButtons > 6) {
                        lt = clamp01(ev.analogButton[6]);
                    } else if (ev.numAxes > 4) {
                        lt = clamp01((ev.axis[4] + 1.0) * 0.5);
                    }
                    if (ev.numButtons > 7) {
                        rt = clamp01(ev.analogButton[7]);
                    } else if (ev.numAxes > 5) {
                        rt = clamp01((ev.axis[5] + 1.0) * 0.5);
                    }
                    snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT)] = lt;
                    snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT)] = rt;
                    snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_LT_DIGITAL)] =
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_LT_DIGITAL)] || (lt > 0.5f);
                    snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_RT_DIGITAL)] =
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_RT_DIGITAL)] || (rt > 0.5f);

                    snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X)] =
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] ? 1.0f : 0.0f) -
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)] ? 1.0f : 0.0f);
                    snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y)] =
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)] ? 1.0f : 0.0f) -
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)] ? 1.0f : 0.0f);

                    info = {};
                    info.index = static_cast<uint32>(i);
                    info.type = NkGamepadType::NK_GP_TYPE_GENERIC;
                    info.numButtons = static_cast<uint32>(ev.numButtons);
                    info.numAxes = static_cast<uint32>(ev.numAxes);
                    const char* id = (ev.id[0] != '\0') ? ev.id : "Web Gamepad";
                    snprintf(info.id, sizeof(info.id), "%s", id);
                    snprintf(info.name, sizeof(info.name), "%s", id);
                    snapshot.info = info;
                }
#else
                for (auto& snapshot : mSnapshots) {
                    snapshot.Clear();
                }
#endif
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

            void Rumble(uint32, float32, float32, float32, float32, uint32) override {
                // Browser rumble/haptics is not universally available in Emscripten C++ API.
            }

            const char* GetName() const noexcept override {
                return "WebGamepad";
            }

        private:
#ifdef __EMSCRIPTEN__
            static EM_BOOL OnConnected(int, const EmscriptenGamepadEvent* ev, void* userData) {
                if (!ev || !userData) {
                    return EM_FALSE;
                }
                auto* self = static_cast<NkEmscriptenGamepad*>(userData);
                const uint32 idx = static_cast<uint32>(ev->index);
                if (idx >= NK_MAX_GAMEPADS) {
                    return EM_FALSE;
                }
                self->mSnapshots[idx].connected = true;
                self->mInfos[idx].index = idx;
                self->mInfos[idx].type = NkGamepadType::NK_GP_TYPE_GENERIC;
                self->mInfos[idx].numButtons = static_cast<uint32>(ev->numButtons);
                self->mInfos[idx].numAxes = static_cast<uint32>(ev->numAxes);
                const char* id = (ev->id[0] != '\0') ? ev->id : "Web Gamepad";
                snprintf(self->mInfos[idx].id, sizeof(self->mInfos[idx].id), "%s", id);
                snprintf(self->mInfos[idx].name, sizeof(self->mInfos[idx].name), "%s", id);
                return EM_TRUE;
            }

            static EM_BOOL OnDisconnected(int, const EmscriptenGamepadEvent* ev, void* userData) {
                if (!ev || !userData) {
                    return EM_FALSE;
                }
                auto* self = static_cast<NkEmscriptenGamepad*>(userData);
                const uint32 idx = static_cast<uint32>(ev->index);
                if (idx >= NK_MAX_GAMEPADS) {
                    return EM_FALSE;
                }
                self->mSnapshots[idx].Clear();
                self->mInfos[idx] = {};
                return EM_TRUE;
            }
#endif

            NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
            NkArray<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
            bool mCallbacksRegistered = false;
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
