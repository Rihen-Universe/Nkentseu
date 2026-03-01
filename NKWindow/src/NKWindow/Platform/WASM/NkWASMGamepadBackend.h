#pragma once
// NkWASMGamepadBackend.h — Web Gamepad API (navigator.getGamepads)
#include "../../Core/NkGamepadSystem.h"
#include <array>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif
/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Class NkWASMGamepadBackend.
 */
class NkWASMGamepadBackend : public INkGamepadBackend {
public:
	bool Init() override {
		for (auto &s : mStates)
			s = {};
		return true;
	}
	void Shutdown() override {
	}
	void Poll() override {
#ifdef __EMSCRIPTEN__
		// Emscripten requires a successful sampling call before any read.
		// If this fails (unsupported API / browser policy), keep gamepads disabled.
		const EMSCRIPTEN_RESULT sampleRes = emscripten_sample_gamepad_data();
		if (sampleRes != EMSCRIPTEN_RESULT_SUCCESS) {
			for (auto &s : mStates)
				s = {};
			return;
		}

		int n = emscripten_get_num_gamepads();
		if (n < 0)
			n = 0;
		for (int i = 0; i < n && (NkU32)i < NK_MAX_GAMEPADS; ++i) {
			mStates[i] = {};
			mStates[i].gamepadIndex = static_cast<NkU32>(i);

			EmscriptenGamepadEvent ev{};
			if (emscripten_get_gamepad_status(i, &ev) != EMSCRIPTEN_RESULT_SUCCESS) {
				continue;
			}
			mStates[i].connected = ev.connected;
			if (!ev.connected)
				continue;
			using B = NkGamepadButton;
			using A = NkGamepadAxis;
			auto sb = [&](B b, int j) {
				if (j < ev.numButtons)
					mStates[i].buttons[static_cast<NkU32>(b)] = ev.digitalButton[j] || ev.analogButton[j] > 0.5;
			};
			auto sa = [&](A a, int j) {
				if (j < ev.numAxes)
					mStates[i].axes[static_cast<NkU32>(a)] = ev.axis[j];
			};
			// Standard mapping W3C
			sb(B::NK_GP_SOUTH, 0);
			sb(B::NK_GP_EAST, 1);
			sb(B::NK_GP_WEST, 2);
			sb(B::NK_GP_NORTH, 3);
			sb(B::NK_GP_LB, 4);
			sb(B::NK_GP_RB, 5);
			sb(B::NK_GP_LT_DIGITAL, 6);
			sb(B::NK_GP_RT_DIGITAL, 7);
			sb(B::NK_GP_BACK, 8);
			sb(B::NK_GP_START, 9);
			sb(B::NK_GP_LSTICK, 10);
			sb(B::NK_GP_RSTICK, 11);
			sb(B::NK_GP_DPAD_UP, 12);
			sb(B::NK_GP_DPAD_DOWN, 13);
			sb(B::NK_GP_DPAD_LEFT, 14);
			sb(B::NK_GP_DPAD_RIGHT, 15);
			sb(B::NK_GP_GUIDE, 16);
			sa(A::NK_GP_AXIS_LX, 0);
			sa(A::NK_GP_AXIS_LY, 1);
			sa(A::NK_GP_AXIS_RX, 2);
			sa(A::NK_GP_AXIS_RY, 3);
			mInfos[i].index = (NkU32)i;
			mInfos[i].numButtons = (NkU32)ev.numButtons;
			mInfos[i].numAxes = (NkU32)ev.numAxes;
			std::snprintf(mInfos[i].id, sizeof(mInfos[i].id), "%s", ev.id);
		}
		for (int i = n; (NkU32)i < NK_MAX_GAMEPADS; ++i)
			mStates[i] = {};
#endif
	}
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

private:
	std::array<NkGamepadStateData, NK_MAX_GAMEPADS> mStates{};
	std::array<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
};
} // namespace nkentseu
