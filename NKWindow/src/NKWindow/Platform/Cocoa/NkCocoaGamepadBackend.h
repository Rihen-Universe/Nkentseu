#pragma once

// =============================================================================
// NkCocoaGamepadBackend.h — GCController (macOS 10.9+)
// Déclaration C++ + implémentation Objective-C en bas (guard __OBJC__).
// =============================================================================

#include "../../Core/NkGamepadSystem.h"
#include <array>
#include <cstring>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkCocoaGamepadBackend.
 */
class NkCocoaGamepadBackend : public INkGamepadBackend {
public:
	NkCocoaGamepadBackend() = default;
	~NkCocoaGamepadBackend() override {
		Shutdown();
	}

	bool Init() override;
	void Shutdown() override;
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

	void Rumble(NkU32 idx, float motorLow, float motorHigh, float triggerLeft, float triggerRight,
				NkU32 durationMs) override;
	void SetLEDColor(NkU32 idx, NkU32 rgba) override;
	bool HasMotion(NkU32 idx) const override {
		return idx < NK_MAX_GAMEPADS && mInfos[idx].hasGyro;
	}

	// Tableau partagé avec la partie ObjC
	std::array<NkGamepadStateData, NK_MAX_GAMEPADS> mStates{};
	std::array<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
};

} // namespace nkentseu

// ===========================================================================
#ifndef __OBJC__

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

inline bool NkCocoaGamepadBackend::Init() {
	for (auto &s : mStates)
		s = {};
	return true;
}

inline void NkCocoaGamepadBackend::Shutdown() {
	for (auto &s : mStates)
		s.connected = false;
}

inline void NkCocoaGamepadBackend::Poll() {
	for (auto &s : mStates)
		s.connected = false;
}

inline void NkCocoaGamepadBackend::Rumble(NkU32 idx, float motorLow, float motorHigh, float triggerLeft,
										  float triggerRight, NkU32 durationMs) {
	(void)idx;
	(void)motorLow;
	(void)motorHigh;
	(void)triggerLeft;
	(void)triggerRight;
	(void)durationMs;
}

inline void NkCocoaGamepadBackend::SetLEDColor(NkU32 idx, NkU32 rgba) {
	(void)idx;
	(void)rgba;
}

} // namespace nkentseu

#endif // !__OBJC__

#ifdef __OBJC__
#import <GameController/GameController.h>
#import <Foundation/Foundation.h>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

static void NkGC_FillState(GCController *ctrl, NkU32 idx, NkGamepadStateData &s, NkGamepadInfo &info) {
	s.connected = true;
	s.gamepadIndex = idx;
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
		sb(B::NK_GP_LT_DIGITAL, pad.leftTrigger.isPressed);
		sb(B::NK_GP_RT_DIGITAL, pad.rightTrigger.isPressed);
		sb(B::NK_GP_LSTICK, pad.leftThumbstickButton ? pad.leftThumbstickButton.isPressed : false);
		sb(B::NK_GP_RSTICK, pad.rightThumbstickButton ? pad.rightThumbstickButton.isPressed : false);
		sb(B::NK_GP_DPAD_UP, pad.dpad.up.isPressed);
		sb(B::NK_GP_DPAD_DOWN, pad.dpad.down.isPressed);
		sb(B::NK_GP_DPAD_LEFT, pad.dpad.left.isPressed);
		sb(B::NK_GP_DPAD_RIGHT, pad.dpad.right.isPressed);
		if (@available(macOS 10.15, *)) {
			sb(B::NK_GP_BACK, pad.buttonOptions ? pad.buttonOptions.isPressed : false);
			sb(B::NK_GP_START, pad.buttonMenu.isPressed);
		}

		sa(A::NK_GP_AXIS_LX, pad.leftThumbstick.xAxis.value);
		sa(A::NK_GP_AXIS_LY, pad.leftThumbstick.yAxis.value);
		sa(A::NK_GP_AXIS_RX, pad.rightThumbstick.xAxis.value);
		sa(A::NK_GP_AXIS_RY, pad.rightThumbstick.yAxis.value);
		sa(A::NK_GP_AXIS_LT, pad.leftTrigger.value);
		sa(A::NK_GP_AXIS_RT, pad.rightTrigger.value);

		info.numButtons = static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
		info.numAxes = static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
		info.hasRumble = [ctrl respondsToSelector:@selector(haptics)];
	}

	// Gyro
	if (@available(macOS 11, *)) {
		info.hasGyro = (ctrl.motion != nil && ctrl.motion.sensorsActive);
		if (info.hasGyro) {
			s.gyroX = ctrl.motion.rotationRate.x;
			s.gyroY = ctrl.motion.rotationRate.y;
			s.gyroZ = ctrl.motion.rotationRate.z;
		}
	}

	// Type
	NSString *cat = ctrl.productCategory ?: @"";
	if ([cat containsString:@"Xbox"])
		info.type = NkGamepadType::NK_GP_TYPE_XBOX;
	else if ([cat containsString:@"Dual"])
		info.type = NkGamepadType::NK_GP_TYPE_PLAYSTATION;
	else
		info.type = NkGamepadType::NK_GP_TYPE_GENERIC;

	info.index = idx;
	const char *vn = ctrl.vendorName.UTF8String ?: "GCController";
	std::strncpy(info.vendor.name, vn, sizeof(info.vendor.name) - 1);
}

bool NkCocoaGamepadBackend::Init() {
	for (auto &s : mStates)
		s = {};
	return true;
}
void NkCocoaGamepadBackend::Shutdown() {
	for (auto &s : mStates)
		s.connected = false;
}

void NkCocoaGamepadBackend::Poll() {
	@autoreleasepool {
		NSArray<GCController *> *ctrls = [GCController controllers];
		NkU32 idx = 0;
		for (GCController *ctrl in ctrls) {
			if (idx >= NK_MAX_GAMEPADS)
				break;
			NkGC_FillState(ctrl, idx, mStates[idx], mInfos[idx]);
			++idx;
		}
		for (; idx < NK_MAX_GAMEPADS; ++idx)
			mStates[idx].connected = false;
	}
}

void NkCocoaGamepadBackend::Rumble(NkU32 idx, float l, float h, float, float, NkU32) {
	(void)idx;
	(void)l;
	(void)h;
	// GCHapticsLocality API (macOS 11+) — implémentation minimale
}

void NkCocoaGamepadBackend::SetLEDColor(NkU32 idx, NkU32 rgba) {
	@autoreleasepool {
		if (@available(macOS 12, *)) {
			NSArray<GCController *> *ctrls = [GCController controllers];
			if (idx >= (NkU32)ctrls.count)
				return;
			float r = ((rgba >> 24) & 0xFF) / 255.f, g = ((rgba >> 16) & 0xFF) / 255.f,
				  b = ((rgba >> 8) & 0xFF) / 255.f;
			[ctrls[idx].light setColor:[NSColor colorWithRed:r green:g blue:b alpha:1.f]];
		}
	}
}

} // namespace nkentseu
#endif // __OBJC__
