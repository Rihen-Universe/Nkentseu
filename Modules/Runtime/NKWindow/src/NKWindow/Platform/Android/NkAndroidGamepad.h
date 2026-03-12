#pragma once
// =============================================================================
// NkAndroidGamepad.h - Android gamepad backend via AInputEvent
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include "NKWindow/Events/NkGamepadSystem.h"

#include <cstdio>
#include <cstring>
#include <cstdint>

#ifdef __ANDROID__
#include <android/input.h>
#include <android/keycodes.h>
#endif

namespace nkentseu {

    class NkAndroidGamepad final : public NkIGamepad {
    public:
        bool Init() override {
            for (auto& snapshot : mSnapshots) {
                snapshot.Clear();
            }
            for (auto& info : mInfos) {
                info = {};
            }
            mDeviceIds.fill(-1);
            return true;
        }

        void Shutdown() override {
            for (auto& snapshot : mSnapshots) {
                snapshot.Clear();
            }
            for (auto& info : mInfos) {
                info = {};
            }
            mDeviceIds.fill(-1);
        }

        void Poll() override {}

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
            // Android NDK does not expose a stable native rumble API for gamepads.
        }

        const char* GetName() const noexcept override {
            return "AInputEvent";
        }

        void OnInputEvent(void* rawEvent) {
#ifdef __ANDROID__
            AInputEvent* ev = static_cast<AInputEvent*>(rawEvent);
            if (!ev) {
                return;
            }

            const int32_t source = AInputEvent_getSource(ev);
            if ((source & AINPUT_SOURCE_GAMEPAD) == 0 &&
                (source & AINPUT_SOURCE_JOYSTICK) == 0) {
                return;
            }

            const uint32 slot = FindOrCreateSlot(AInputEvent_getDeviceId(ev));
            if (slot >= NK_MAX_GAMEPADS) {
                return;
            }

            NkGamepadSnapshot& snapshot = mSnapshots[slot];
            NkGamepadInfo& info = mInfos[slot];

            snapshot.connected = true;
            info.index = slot;
            info.type = NkGamepadType::NK_GP_TYPE_MOBILE;
            info.numButtons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
            info.numAxes = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
            ::snprintf(info.id, sizeof(info.id), "android-device-%d", AInputEvent_getDeviceId(ev));
            ::snprintf(info.name, sizeof(info.name), "Android Gamepad %u", slot);
            snapshot.info = info;

            if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX)] =
                    AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_X, 0);
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LY)] =
                    AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_Y, 0);
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RX)] =
                    AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_Z, 0);
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RY)] =
                    AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RZ, 0);
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT)] =
                    AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_LTRIGGER, 0);
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT)] =
                    AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RTRIGGER, 0);

                const float hatX = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_X, 0);
                const float hatY = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_Y, 0);
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_X)] = hatX;
                snapshot.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_DPAD_Y)] = hatY;

                snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)]  = hatX < -0.5f;
                snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] = hatX >  0.5f;
                snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)]    = hatY < -0.5f;
                snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)]  = hatY >  0.5f;
                return;
            }

            if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
                const bool pressed = AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_DOWN;
                switch (AKeyEvent_getKeyCode(ev)) {
                    case AKEYCODE_BUTTON_A:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_SOUTH)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_B:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_EAST)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_X:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_WEST)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_Y:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_NORTH)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_L1:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_LB)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_R1:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_RB)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_L2:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_LT_DIGITAL)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_R2:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_RT_DIGITAL)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_THUMBL:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_LSTICK)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_THUMBR:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_RSTICK)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_START:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_START)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_SELECT:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_BACK)] = pressed;
                        break;
                    case AKEYCODE_BUTTON_MODE:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_GUIDE)] = pressed;
                        break;
                    case AKEYCODE_DPAD_UP:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_UP)] = pressed;
                        break;
                    case AKEYCODE_DPAD_DOWN:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_DOWN)] = pressed;
                        break;
                    case AKEYCODE_DPAD_LEFT:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_LEFT)] = pressed;
                        break;
                    case AKEYCODE_DPAD_RIGHT:
                        snapshot.buttons[static_cast<uint32>(NkGamepadButton::NK_GP_DPAD_RIGHT)] = pressed;
                        break;
                    default:
                        break;
                }
            }
#else
            (void)rawEvent;
#endif
        }

    private:
        uint32 FindOrCreateSlot(int32_t deviceId) {
            for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
                if (mDeviceIds[i] == deviceId) {
                    return i;
                }
            }
            for (uint32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
                if (mDeviceIds[i] == -1) {
                    mDeviceIds[i] = deviceId;
                    return i;
                }
            }
            return NK_MAX_GAMEPADS;
        }

        NkArray<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};
        NkArray<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
        NkArray<int32_t, NK_MAX_GAMEPADS> mDeviceIds{};
    };

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_ANDROID
