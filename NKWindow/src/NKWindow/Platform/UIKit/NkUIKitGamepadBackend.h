#pragma once
// NkUIKitGamepadBackend.h — GCController iOS/tvOS (MFi, Xbox, DualShock)
#include "../../Core/NkGamepadSystem.h"
#include <array>
#include <cstring>
/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Class NkUIKitGamepadBackend.
 */
class NkUIKitGamepadBackend : public INkGamepadBackend {
public:
	bool Init() override {
		for (auto &s : mStates)
			s = {};
		return true;
	}
	void Shutdown() override {
		for (auto &s : mStates)
			s.connected = false;
	}
	void Poll() override;
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
	bool HasMotion(NkU32 i) const override {
		return i < NK_MAX_GAMEPADS && mInfos[i].hasGyro;
	}
	std::array<NkGamepadStateData, NK_MAX_GAMEPADS> mStates{};
	std::array<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
};
} // namespace nkentseu
#ifdef __OBJC__
#import <GameController/GameController.h>
/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
void NkUIKitGamepadBackend::Poll() {
	@autoreleasepool {
		NSArray<GCController *> *ctrls = [GCController controllers];
		NkU32 idx = 0;
		for (GCController *ctrl in ctrls) {
			if (idx >= NK_MAX_GAMEPADS)
				break;
			auto &s = mStates[idx];
			auto &info = mInfos[idx];
			s.connected = true;
			s.gamepadIndex = idx;
			info.index = idx;
			using B = NkGamepadButton;
			using A = NkGamepadAxis;
			auto sb = [&](B b, bool v) { s.buttons[static_cast<NkU32>(b)] = v; };
			auto sa = [&](A a, float v) { s.axes[static_cast<NkU32>(a)] = v; };
			if (GCExtendedGamepad *pad = ctrl.extendedGamepad) {
				sb(B::NK_GP_SOUTH, pad.buttonA.isPressed);
				sb(B::NK_GP_EAST, pad.buttonB.isPressed);
				sb(B::NK_GP_WEST, pad.buttonX.isPressed);
				sb(B::NK_GP_NORTH, pad.buttonY.isPressed);
				sb(B::NK_GP_LB, pad.leftShoulder.isPressed);
				sb(B::NK_GP_RB, pad.rightShoulder.isPressed);
				sb(B::NK_GP_DPAD_UP, pad.dpad.up.isPressed);
				sb(B::NK_GP_DPAD_DOWN, pad.dpad.down.isPressed);
				sb(B::NK_GP_DPAD_LEFT, pad.dpad.left.isPressed);
				sb(B::NK_GP_DPAD_RIGHT, pad.dpad.right.isPressed);
				sa(A::NK_GP_AXIS_LX, pad.leftThumbstick.xAxis.value);
				sa(A::NK_GP_AXIS_LY, pad.leftThumbstick.yAxis.value);
				sa(A::NK_GP_AXIS_RX, pad.rightThumbstick.xAxis.value);
				sa(A::NK_GP_AXIS_RY, pad.rightThumbstick.yAxis.value);
				sa(A::NK_GP_AXIS_LT, pad.leftTrigger.value);
				sa(A::NK_GP_AXIS_RT, pad.rightTrigger.value);
			}
			if (@available(iOS 14, *)) {
				info.hasGyro = ctrl.motion != nil && ctrl.motion.sensorsActive;
				if (info.hasGyro) {
					s.gyroX = ctrl.motion.rotationRate.x;
					s.gyroY = ctrl.motion.rotationRate.y;
					s.gyroZ = ctrl.motion.rotationRate.z;
				}
			}
			++idx;
		}
		for (; idx < NK_MAX_GAMEPADS; ++idx)
			mStates[idx].connected = false;
	}
}
} // namespace nkentseu
#endif
