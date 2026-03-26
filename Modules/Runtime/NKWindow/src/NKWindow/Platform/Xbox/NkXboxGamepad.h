#pragma once
// =============================================================================
// NkXboxGamepad.h - Xbox gamepad backend (safe fallback implementation)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_XBOX)

#include "NKEvent/NkGamepadSystem.h"
#include "NKMemory/NkUtils.h"

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <Xinput.h>
#endif


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
#if defined(_WIN32)
                for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
                    XINPUT_STATE state {};
                    const DWORD err = ::XInputGetState(i, &state);
                    NkGamepadSnapshot& snapshot = mSnapshots[i];
                    snapshot.Clear();
                    if (err != ERROR_SUCCESS) {
                        continue;
                    }

                    snapshot.connected = true;
                    snapshot.info.index = i;
                    snapshot.info.type = NkGamepadType::NK_GP_TYPE_XBOX;
                    snapshot.info.numButtons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
                    snapshot.info.numAxes = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
                    snapshot.info.hasRumble = true;
                    CopyText(snapshot.info.id, sizeof(snapshot.info.id), "Xbox_XInput");
                    CopyText(snapshot.info.name, sizeof(snapshot.info.name), "Xbox Controller");

                    const WORD buttons = state.Gamepad.wButtons;
                    SetButton(snapshot, NkGamepadButton::NK_GP_SOUTH,      (buttons & XINPUT_GAMEPAD_A) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_EAST,       (buttons & XINPUT_GAMEPAD_B) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_WEST,       (buttons & XINPUT_GAMEPAD_X) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_NORTH,      (buttons & XINPUT_GAMEPAD_Y) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_LB,         (buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_RB,         (buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_START,      (buttons & XINPUT_GAMEPAD_START) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_BACK,       (buttons & XINPUT_GAMEPAD_BACK) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_LSTICK,     (buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_RSTICK,     (buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_DPAD_UP,    (buttons & XINPUT_GAMEPAD_DPAD_UP) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_DPAD_DOWN,  (buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_DPAD_LEFT,  (buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
                    SetButton(snapshot, NkGamepadButton::NK_GP_DPAD_RIGHT, (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);

                    const float32 lt = static_cast<float32>(state.Gamepad.bLeftTrigger) / 255.0f;
                    const float32 rt = static_cast<float32>(state.Gamepad.bRightTrigger) / 255.0f;
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_LT, lt);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_RT, rt);
                    SetButton(snapshot, NkGamepadButton::NK_GP_LT_DIGITAL, lt > 0.5f);
                    SetButton(snapshot, NkGamepadButton::NK_GP_RT_DIGITAL, rt > 0.5f);

                    const float32 lx = NormalizeStick(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                    const float32 ly = NormalizeStick(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                    const float32 rx = NormalizeStick(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                    const float32 ry = NormalizeStick(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_LX, lx);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_LY, ly);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_RX, rx);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_RY, ry);

                    const float32 dpadX =
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] ? 1.0f : 0.0f) -
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)] ? 1.0f : 0.0f);
                    const float32 dpadY =
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)] ? 1.0f : 0.0f) -
                        (snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)] ? 1.0f : 0.0f);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_DPAD_X, dpadX);
                    SetAxis(snapshot, NkGamepadAxis::NK_GP_AXIS_DPAD_Y, dpadY);
                }
#else
                // Fallback if no native Xbox gamepad API is available in this build.
                for (auto& snapshot : mSnapshots) {
                    snapshot.connected = false;
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

            void Rumble(uint32 idx, float32 lowFreq, float32 highFreq, float32, float32, uint32) override {
#if defined(_WIN32)
                if (idx >= NK_MAX_GAMEPADS) {
                    return;
                }
                XINPUT_VIBRATION vibration {};
                vibration.wLeftMotorSpeed  = ToMotorSpeed(lowFreq);
                vibration.wRightMotorSpeed = ToMotorSpeed(highFreq);
                (void)::XInputSetState(idx, &vibration);
#else
                (void)idx;
                (void)lowFreq;
                (void)highFreq;
#endif
            }

            const char* GetName() const noexcept override {
                return "XboxGamepad";
            }

        private:
            static void CopyText(char* dst, uint32 dstSize, const char* text) {
                if (!dst || dstSize == 0u) {
                    return;
                }
                memory::NkMemSet(dst, 0, dstSize);
                if (!text) {
                    return;
                }
                uint32 i = 0u;
                while (i + 1u < dstSize && text[i] != '\0') {
                    dst[i] = text[i];
                    ++i;
                }
                dst[i] = '\0';
            }

            static void SetButton(NkGamepadSnapshot& snapshot, NkGamepadButton button, bool pressed) {
                snapshot.buttons[static_cast<uint32>(button)] = pressed;
            }

            static void SetAxis(NkGamepadSnapshot& snapshot, NkGamepadAxis axis, float32 value) {
                snapshot.axes[static_cast<uint32>(axis)] = value;
            }

#if defined(_WIN32)
            static float32 NormalizeStick(SHORT value, SHORT deadzone) {
                const float32 absValue = (value >= 0) ? static_cast<float32>(value) : static_cast<float32>(-value);
                if (absValue <= static_cast<float32>(deadzone)) {
                    return 0.0f;
                }

                float32 normalized = 0.0f;
                if (value >= 0) {
                    normalized = (static_cast<float32>(value) - static_cast<float32>(deadzone)) /
                                 (32767.0f - static_cast<float32>(deadzone));
                } else {
                    normalized = (static_cast<float32>(value) + static_cast<float32>(deadzone)) /
                                 (32768.0f - static_cast<float32>(deadzone));
                }

                if (normalized > 1.0f) normalized = 1.0f;
                if (normalized < -1.0f) normalized = -1.0f;
                return normalized;
            }

            static WORD ToMotorSpeed(float32 intensity) {
                if (intensity < 0.0f) intensity = 0.0f;
                if (intensity > 1.0f) intensity = 1.0f;
                return static_cast<WORD>(intensity * 65535.0f);
            }
#endif

            NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_XBOX
