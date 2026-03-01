#pragma once
// NkAndroidGamepadBackend.h — AInputEvent AINPUT_SOURCE_JOYSTICK
#include "../../Core/NkGamepadSystem.h"
#include <array>
#include <cstring>
#include <cstdint>
#ifdef __ANDROID__
#include <android/input.h>
#include <android/keycodes.h>
#endif
/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	/**
	 * @brief Class NkAndroidGamepadBackend.
	 */
	class NkAndroidGamepadBackend : public INkGamepadBackend {
		public:
			bool Init() override {
				for (auto &s : mStates)
					s = {};
				std::fill(mDevIds, mDevIds + NK_MAX_GAMEPADS, -1);
				return true;
			}
			void Shutdown() override {
			}
			void Poll() override {
			} // alimenté par OnInputEvent
			NkU32 GetConnectedCount() const override {
				NkU32 n = 0;
				for (auto &s : mStates)
					if (s.connected)
						++n;
				return n;
			}
			const NkGamepadInfo &GetInfo(NkU32 i) const override {
				static NkGamepadInfo d;
				return i < NK_MAX_GAMEPADS ? mInfos[i] : d;
			}
			const NkGamepadStateData &GetState(NkU32 i) const override {
				static NkGamepadStateData d;
				return i < NK_MAX_GAMEPADS ? mStates[i] : d;
			}
			void Rumble(NkU32, float, float, float, float, NkU32) override {
			}

			void OnInputEvent(void *rawEvent) {
		#ifdef __ANDROID__
				AInputEvent *ev = static_cast<AInputEvent *>(rawEvent);
				if (!ev)
					return;
				int32_t src = AInputEvent_getSource(ev);
				if (!(src & AINPUT_SOURCE_JOYSTICK) && !(src & AINPUT_SOURCE_GAMEPAD))
					return;
				NkU32 slot = Slot(AInputEvent_getDeviceId(ev));
				if (slot >= NK_MAX_GAMEPADS)
					return;
				mStates[slot].connected = true;
				mStates[slot].gamepadIndex = slot;
				using B = NkGamepadButton;
				using A = NkGamepadAxis;
				auto sb = [&](B b, bool v) { mStates[slot].buttons[static_cast<NkU32>(b)] = v; };
				auto sa = [&](A a, float v) { mStates[slot].axes[static_cast<NkU32>(a)] = v; };

				if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
					sa(A::NK_GP_AXIS_LX, AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_X, 0));
					sa(A::NK_GP_AXIS_LY, AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_Y, 0));
					sa(A::NK_GP_AXIS_RX, AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_Z, 0));
					sa(A::NK_GP_AXIS_RY, AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RZ, 0));
					sa(A::NK_GP_AXIS_LT, AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_LTRIGGER, 0));
					sa(A::NK_GP_AXIS_RT, AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_RTRIGGER, 0));
					float hx = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_X, 0);
					float hy = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HAT_Y, 0);
					sa(A::NK_GP_AXIS_DPAD_X, hx);
					sa(A::NK_GP_AXIS_DPAD_Y, hy);
					sb(B::NK_GP_DPAD_LEFT, hx < -0.5f);
					sb(B::NK_GP_DPAD_RIGHT, hx > 0.5f);
					sb(B::NK_GP_DPAD_UP, hy < -0.5f);
					sb(B::NK_GP_DPAD_DOWN, hy > 0.5f);
				} else if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
					bool pressed = AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_DOWN;
					switch (AKeyEvent_getKeyCode(ev)) {
						case AKEYCODE_BUTTON_A:
							sb(B::NK_GP_SOUTH, pressed);
							break;
						case AKEYCODE_BUTTON_B:
							sb(B::NK_GP_EAST, pressed);
							break;
						case AKEYCODE_BUTTON_X:
							sb(B::NK_GP_WEST, pressed);
							break;
						case AKEYCODE_BUTTON_Y:
							sb(B::NK_GP_NORTH, pressed);
							break;
						case AKEYCODE_BUTTON_L1:
							sb(B::NK_GP_LB, pressed);
							break;
						case AKEYCODE_BUTTON_R1:
							sb(B::NK_GP_RB, pressed);
							break;
						case AKEYCODE_BUTTON_L2:
							sb(B::NK_GP_LT_DIGITAL, pressed);
							break;
						case AKEYCODE_BUTTON_R2:
							sb(B::NK_GP_RT_DIGITAL, pressed);
							break;
						case AKEYCODE_BUTTON_THUMBL:
							sb(B::NK_GP_LSTICK, pressed);
							break;
						case AKEYCODE_BUTTON_THUMBR:
							sb(B::NK_GP_RSTICK, pressed);
							break;
						case AKEYCODE_BUTTON_START:
							sb(B::NK_GP_START, pressed);
							break;
						case AKEYCODE_BUTTON_SELECT:
							sb(B::NK_GP_BACK, pressed);
							break;
						case AKEYCODE_DPAD_UP:
							sb(B::NK_GP_DPAD_UP, pressed);
							break;
						case AKEYCODE_DPAD_DOWN:
							sb(B::NK_GP_DPAD_DOWN, pressed);
							break;
						case AKEYCODE_DPAD_LEFT:
							sb(B::NK_GP_DPAD_LEFT, pressed);
							break;
						case AKEYCODE_DPAD_RIGHT:
							sb(B::NK_GP_DPAD_RIGHT, pressed);
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
			std::array<NkGamepadStateData, NK_MAX_GAMEPADS> mStates{};
			std::array<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
			int32_t mDevIds[NK_MAX_GAMEPADS];

			NkU32 Slot(int32_t id) {
				for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i)
					if (mDevIds[i] == id)
						return i;
				for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i)
					if (mDevIds[i] == -1) {
						mDevIds[i] = id;
						return i;
					}
				return NK_MAX_GAMEPADS;
			}
	};
} // namespace nkentseu
