#pragma once

// =============================================================================
// NkWin32GamepadBackend.h
// Backend gamepad Win32 via XInput (Xbox 360/One/Series).
// Une instance unique par NkSystem (singleton via NkGamepadSystem).
// Plusieurs fenêtres = même backend partagé.
//
// Dépendances : xinput.lib (automatiquement lié via #pragma comment)
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")

#include "../../Events/NkGamepadEvent.h"
#include "../../Events/NkGamepadSystem.h"
#include <array>
#include <cstring>
#include <cstdio>
#include <cmath>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------

	static inline float NkXI_NormAxis(SHORT raw) {
		return raw >= 0 ? static_cast<float>(raw) / 32767.f : static_cast<float>(raw) / 32768.f;
	}

	static inline float NkXI_ApplyDeadzone(SHORT raw, SHORT dz) {
		return (std::abs(raw) < dz) ? 0.f : NkXI_NormAxis(raw);
	}

	// ---------------------------------------------------------------------------

	class NkWin32GamepadBackend : public INkGamepadBackend {
	public:
		NkWin32GamepadBackend() = default;
		~NkWin32GamepadBackend() override {
			Shutdown();
		}

		bool Init() override {
			for (auto &s : mSnapshots)
				s.Clear();
			return true;
		}

		void Shutdown() override {
			// Couper toutes les vibrations
			XINPUT_VIBRATION v{};
			for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
				XInputSetState(i, &v);
		}

		void Poll() override {
			for (DWORD i = 0; i < XUSER_MAX_COUNT && i < NK_MAX_GAMEPADS; ++i) {
				XINPUT_STATE xs{};
				bool wasConnected = mSnapshots[i].connected;
				bool isConnected  = (XInputGetState(i, &xs) == ERROR_SUCCESS);

				mSnapshots[i].connected = isConnected;

				if (isConnected) {
					FillSnapshot(xs.Gamepad, static_cast<NkU32>(i), mSnapshots[i]);
					FillBattery(i, mSnapshots[i]);
					if (!wasConnected)
						FillInfo(i, mSnapshots[i].info);
				} else {
					// Conserver les infos mais effacer les entrées
					NkGamepadInfo savedInfo = mSnapshots[i].info;
					mSnapshots[i].Clear();
					mSnapshots[i].info = savedInfo;
				}
			}
		}

		NkU32 GetConnectedCount() const override {
			NkU32 n = 0;
			for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i)
				if (mSnapshots[i].connected)
					++n;
			return n;
		}

		// -----------------------------------------------------------------------
		// INkGamepadBackend — GetSnapshot (interface principale, fichier 7)
		// -----------------------------------------------------------------------

		const NkGamepadSnapshot& GetSnapshot(NkU32 idx) const override {
			static NkGamepadSnapshot sDummy;
			return idx < NK_MAX_GAMEPADS ? mSnapshots[idx] : sDummy;
		}

		// -----------------------------------------------------------------------
		// INkGamepadBackend — commandes de sortie
		// -----------------------------------------------------------------------

		void Rumble(NkU32 idx, float motorLow, float motorHigh,
					float /*triggerLeft*/, float /*triggerRight*/,
					NkU32 /*durationMs*/) override
		{
			if (idx >= XUSER_MAX_COUNT)
				return;
			XINPUT_VIBRATION v{};
			v.wLeftMotorSpeed  = static_cast<WORD>(std::min(motorLow,  1.f) * 65535.f);
			v.wRightMotorSpeed = static_cast<WORD>(std::min(motorHigh, 1.f) * 65535.f);
			XInputSetState(static_cast<DWORD>(idx), &v);
		}

		const char* GetName() const noexcept override {
			return "XInput";
		}

	private:
		/// Snapshots complets par slot (indexés 0..NK_MAX_GAMEPADS-1)
		std::array<NkGamepadSnapshot, NK_MAX_GAMEPADS> mSnapshots{};

		// -----------------------------------------------------------------------
		// Helpers privés
		// -----------------------------------------------------------------------

		static void FillSnapshot(const XINPUT_GAMEPAD& xp, NkU32 index, NkGamepadSnapshot& s) {
			using B = NkGamepadButton;
			using A = NkGamepadAxis;

			s.connected      = true;
			s.info.index     = index;

			// Macro bouton
			auto btn = [&](B b, WORD mask) {
				s.buttons[static_cast<NkU32>(b)] = (xp.wButtons & mask) != 0;
			};

			btn(B::NK_GP_SOUTH,      XINPUT_GAMEPAD_A);
			btn(B::NK_GP_EAST,       XINPUT_GAMEPAD_B);
			btn(B::NK_GP_WEST,       XINPUT_GAMEPAD_X);
			btn(B::NK_GP_NORTH,      XINPUT_GAMEPAD_Y);
			btn(B::NK_GP_LB,         XINPUT_GAMEPAD_LEFT_SHOULDER);
			btn(B::NK_GP_RB,         XINPUT_GAMEPAD_RIGHT_SHOULDER);
			btn(B::NK_GP_LSTICK,     XINPUT_GAMEPAD_LEFT_THUMB);
			btn(B::NK_GP_RSTICK,     XINPUT_GAMEPAD_RIGHT_THUMB);
			btn(B::NK_GP_BACK,       XINPUT_GAMEPAD_BACK);
			btn(B::NK_GP_START,      XINPUT_GAMEPAD_START);
			btn(B::NK_GP_DPAD_UP,    XINPUT_GAMEPAD_DPAD_UP);
			btn(B::NK_GP_DPAD_DOWN,  XINPUT_GAMEPAD_DPAD_DOWN);
			btn(B::NK_GP_DPAD_LEFT,  XINPUT_GAMEPAD_DPAD_LEFT);
			btn(B::NK_GP_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT);

			// Gâchettes analogiques → boutons digitaux
			float lt = xp.bLeftTrigger  / 255.f;
			float rt = xp.bRightTrigger / 255.f;
			s.buttons[static_cast<NkU32>(B::NK_GP_LT_DIGITAL)] = (lt > 0.5f);
			s.buttons[static_cast<NkU32>(B::NK_GP_RT_DIGITAL)] = (rt > 0.5f);

			// Axes
			auto ax = [&](A a, float v) { s.axes[static_cast<NkU32>(a)] = v; };

			ax(A::NK_GP_AXIS_LX, NkXI_ApplyDeadzone(xp.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE));
			ax(A::NK_GP_AXIS_LY, NkXI_ApplyDeadzone(xp.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE));
			ax(A::NK_GP_AXIS_RX, NkXI_ApplyDeadzone(xp.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE));
			ax(A::NK_GP_AXIS_RY, NkXI_ApplyDeadzone(xp.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE));
			ax(A::NK_GP_AXIS_LT, lt);
			ax(A::NK_GP_AXIS_RT, rt);
		}

		static void FillBattery(DWORD idx, NkGamepadSnapshot& s) {
			XINPUT_BATTERY_INFORMATION bi{};
			if (XInputGetBatteryInformation(idx, BATTERY_DEVTYPE_GAMEPAD, &bi) != ERROR_SUCCESS)
				return;
			s.isCharging = false; // XInput ne rapporte pas la charge
			switch (bi.BatteryLevel) {
				case BATTERY_LEVEL_EMPTY:  s.batteryLevel = 0.00f; break;
				case BATTERY_LEVEL_LOW:    s.batteryLevel = 0.25f; break;
				case BATTERY_LEVEL_MEDIUM: s.batteryLevel = 0.60f; break;
				case BATTERY_LEVEL_FULL:   s.batteryLevel = 1.00f; break;
				default:                   s.batteryLevel = -1.f;  break;
			}
		}

		static void FillInfo(DWORD idx, NkGamepadInfo& info) {
			info.index      = static_cast<NkU32>(idx);
			info.type       = NkGamepadType::NK_GP_TYPE_XBOX;
			info.hasRumble  = true;
			info.numButtons = static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
			info.numAxes    = static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);
			std::snprintf(info.id,   sizeof(info.id),   "XInput#%u", static_cast<unsigned>(idx));
			std::snprintf(info.name, sizeof(info.name), "Xbox Controller %u", static_cast<unsigned>(idx));
		}
	};

} // namespace nkentseu