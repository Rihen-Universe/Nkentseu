#pragma once

// =============================================================================
// NkUWPGamepadBackend.h — UWP Windows.Gaming.Input
//
// Utilise Windows::Gaming::Input::Gamepad (WinRT).
// Compatible : UWP, Xbox One/Series, Windows 10+.
// =============================================================================

#include "../../Core/NkGamepadSystem.h"
#include <array>
#include <cstring>
#include <cmath>

#if defined(__cplusplus_winrt) || defined(NKENTSEU_PLATFORM_UWP)
#include <windows.gaming.input.h>
using namespace Windows::Gaming::Input;
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkUWPGamepadBackend.
 */
class NkUWPGamepadBackend : public INkGamepadBackend {
public:
	NkUWPGamepadBackend() = default;
	~NkUWPGamepadBackend() override {
		Shutdown();
	}

	bool Init() override {
		for (auto &s : mStates)
			s = {};
		for (auto &i : mInfos)
			i = {};
		mReady = true;
		return true;
	}

	void Shutdown() override {
		mReady = false;
	}

	void Poll() override {
#if defined(__cplusplus_winrt) || defined(NKENTSEU_PLATFORM_UWP)
		auto gamepads = Gamepad::Gamepads;
		NkU32 count = static_cast<NkU32>(gamepads->Size);
		if (count > NK_MAX_GAMEPADS)
			count = NK_MAX_GAMEPADS;

		// Marquer tout déconnecté, puis remplir
		for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i)
			mStates[i].connected = false;

		for (NkU32 i = 0; i < count; ++i) {
			auto gp = gamepads->GetAt(i);
			if (!gp)
				continue;
			GamepadReading r = gp->GetCurrentReading();
			FillState(r, i);
			mStates[i].connected = true;
			mStates[i].gamepadIndex = i;
			// Infos
			mInfos[i].index = i;
			mInfos[i].type = NkGamepadType::NK_GP_TYPE_XBOX;
			mInfos[i].hasRumble = true;
			snprintf(mInfos[i].id, sizeof(mInfos[i].id), "UWPGamepad#%u", i);
		}
#endif
	}

	NkU32 GetConnectedCount() const override {
		NkU32 n = 0;
		for (const auto &s : mStates)
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

	void Rumble(NkU32 idx, float low, float high, float tl, float tr, NkU32 /*ms*/) override {
#if defined(__cplusplus_winrt) || defined(NKENTSEU_PLATFORM_UWP)
		auto gamepads = Gamepad::Gamepads;
		if (idx >= (NkU32)gamepads->Size)
			return;
		auto gp = gamepads->GetAt(idx);
		if (!gp)
			return;
		GamepadVibration v{};
		v.LeftMotor = static_cast<double>(std::min(low, 1.f));
		v.RightMotor = static_cast<double>(std::min(high, 1.f));
		v.LeftTrigger = static_cast<double>(std::min(tl, 1.f));
		v.RightTrigger = static_cast<double>(std::min(tr, 1.f));
		gp->Vibration = v;
#else
		(void)idx;
		(void)low;
		(void)high;
		(void)tl;
		(void)tr;
#endif
	}

private:
	std::array<NkGamepadStateData, NK_MAX_GAMEPADS> mStates{};
	std::array<NkGamepadInfo, NK_MAX_GAMEPADS> mInfos{};
	bool mReady = false;

#if defined(__cplusplus_winrt) || defined(NKENTSEU_PLATFORM_UWP)
	static void FillState(const GamepadReading &r, NkU32 i) {
		// This would need access to mStates — inline lambda approach
		// Called from Poll() directly — just illustrative layout
		(void)r;
		(void)i;
	}
#endif

	void FillState_Impl(const void *reading, NkU32 slot) {
		(void)reading;
		(void)slot;
		// Real impl in UWP translation unit with WinRT types
	}
};

} // namespace nkentseu
