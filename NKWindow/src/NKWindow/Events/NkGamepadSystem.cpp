// =============================================================================
// NkGamepadSystem.cpp — implémentation façade + sélection backend
// =============================================================================

#include "NkGamepadSystem.h"
#include "NkGamepadEvent.h"
#include "NkEventSystem.h"
#include "NkPlatformDetect.h"
#include <algorithm>
#include <cmath>

// Sélection du backend gamepad par plateforme
#if defined(NKENTSEU_PLATFORM_WIN32)
#include "../Platform/Win32/NkWin32GamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkWin32GamepadBackend;

#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
#include "../Platform/UWP/NkUWPGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkUWPGamepadBackend;

#elif defined(NKENTSEU_PLATFORM_COCOA)
#include "../Platform/Cocoa/NkCocoaGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkCocoaGamepadBackend;

#elif defined(NKENTSEU_PLATFORM_UIKIT)
#include "../Platform/UIKit/NkUIKitGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkUIKitGamepadBackend;

#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include "../Platform/Android/NkAndroidGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkAndroidGamepadBackend;

#elif defined(NKENTSEU_PLATFORM_XCB) || defined(NKENTSEU_PLATFORM_XLIB)
#include "../Platform/Linux/NkLinuxGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkLinuxGamepadBackend;

#elif defined(NKENTSEU_PLATFORM_WASM)
#include "../Platform/WASM/NkWASMGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkWASMGamepadBackend;

#else
#include "../Platform/Noop/NkNoopGamepadBackend.h"
using PlatformGamepadBackend = nkentseu::NkNoopGamepadBackend;
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	NkGamepadSnapshot NkGamepadSystem::sDummySnapshot;
	NkGamepadInfo     NkGamepadSystem::sDummyInfo;

	// ---------------------------------------------------------------------------

	NkGamepadSystem &NkGamepadSystem::Instance() {
		static NkGamepadSystem sInstance;
		return sInstance;
	}

	// ---------------------------------------------------------------------------

	bool NkGamepadSystem::Init(std::unique_ptr<INkGamepadBackend> backend) {
		if (mReady)
			return true;
		
		if (!backend) {
			backend = std::make_unique<PlatformGamepadBackend>();
		}
		
		mBackend = std::move(backend);
		mReady = mBackend->Init();
		
		for (auto &s : mPrevSnapshot) {
			s.Clear();
		}
		
		return mReady;
	}

	void NkGamepadSystem::Shutdown() {
		if (!mReady)
			return;
		if (mBackend)
			mBackend->Shutdown();
		mBackend.reset();
		mReady = false;
	}

	// ---------------------------------------------------------------------------
	// PollGamepads — détection deltas boutons/axes + fire callbacks
	// ---------------------------------------------------------------------------

	void NkGamepadSystem::PollGamepads() {
		if (!mReady || !mBackend)
			return;
		
		mBackend->Poll();

		for (NkU32 i = 0; i < NK_MAX_GAMEPADS; ++i) {
			const NkGamepadSnapshot &cur = mBackend->GetSnapshot(i);
			const NkGamepadSnapshot &prev = mPrevSnapshot[i];

			// Connexion / déconnexion
			if (cur.connected != prev.connected) {
				FireConnect(cur.info, cur.connected);
			}

			if (!cur.connected) {
				mPrevSnapshot[i] = cur;
				continue;
			}

			// Boutons
			for (NkU32 b = 0; b < static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX); ++b) {
				if (cur.buttons[b] != prev.buttons[b]) {
					FireButton(i, static_cast<NkGamepadButton>(b),
							cur.buttons[b] ? NkButtonState::NK_PRESSED : NkButtonState::NK_RELEASED);
				}
			}

			// Axes avec deadzone et epsilon
			for (NkU32 a = 0; a < static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX); ++a) {
				NkF32 v = ApplyDeadzone(cur.axes[a]);
				NkF32 pv = ApplyDeadzone(prev.axes[a]);
				if (std::abs(v - pv) > mAxisEpsilon) {
					FireAxis(i, static_cast<NkGamepadAxis>(a), v, pv);
				}
			}

			mPrevSnapshot[i] = cur;
		}
	}

	// ---------------------------------------------------------------------------

	NkU32 NkGamepadSystem::GetConnectedCount() const noexcept {
		return mReady && mBackend ? mBackend->GetConnectedCount() : 0;
	}

	bool NkGamepadSystem::IsConnected(NkU32 idx) const noexcept {
		return mReady && mBackend && idx < NK_MAX_GAMEPADS && mBackend->GetSnapshot(idx).connected;
	}

	const NkGamepadInfo &NkGamepadSystem::GetInfo(NkU32 idx) const noexcept {
		if (!mReady || !mBackend || idx >= NK_MAX_GAMEPADS) {
			return sDummyInfo;
		}
		return mBackend->GetSnapshot(idx).info;
	}

	const NkGamepadSnapshot &NkGamepadSystem::GetSnapshot(NkU32 idx) const noexcept {
		if (!mReady || !mBackend || idx >= NK_MAX_GAMEPADS) {
			return sDummySnapshot;
		}
		return mBackend->GetSnapshot(idx);
	}

	bool NkGamepadSystem::IsButtonDown(NkU32 idx, NkGamepadButton btn) const noexcept {
		return GetSnapshot(idx).IsButtonDown(btn);
	}

	NkF32 NkGamepadSystem::GetAxis(NkU32 idx, NkGamepadAxis ax) const noexcept {
		return GetSnapshot(idx).GetAxis(ax);
	}

	void NkGamepadSystem::Rumble(NkU32 idx,
				NkF32 motorLow, NkF32 motorHigh,
				NkF32 triggerLeft, NkF32 triggerRight,
				NkU32 durationMs) {
		if (!mReady || !mBackend)
			return;
		
		mBackend->Rumble(idx, motorLow, motorHigh, triggerLeft, triggerRight, durationMs);

		// Dispatch rumble event
		NkGamepadRumbleEvent event(idx, motorLow, motorHigh, triggerLeft, triggerRight, durationMs);
		NkEventSystem::Instance().DispatchEvent(event);
	}

	void NkGamepadSystem::SetLEDColor(NkU32 idx, NkU32 rgba) {
		if (!mReady || !mBackend)
			return;
		
		mBackend->SetLEDColor(idx, rgba);
	}

	// ---------------------------------------------------------------------------

	void NkGamepadSystem::FireConnect(const NkGamepadInfo &info, bool connected) {
		if (connected) {
			NkGamepadConnectEvent event(info);
			NkEventSystem::Instance().DispatchEvent(event);
		} else {
			NkGamepadDisconnectEvent event(info.index);
			NkEventSystem::Instance().DispatchEvent(event);
		}

		if (mConnectCb)
			mConnectCb(info, connected);
	}

	void NkGamepadSystem::FireButton(NkU32 idx, NkGamepadButton btn, NkButtonState st) {
		if (st == NkButtonState::NK_PRESSED) {
			NkGamepadButtonPressEvent event(idx, btn);
			NkEventSystem::Instance().DispatchEvent(event);
		} else {
			NkGamepadButtonReleaseEvent event(idx, btn);
			NkEventSystem::Instance().DispatchEvent(event);
		}

		if (mButtonCb)
			mButtonCb(idx, btn, st);
	}

	void NkGamepadSystem::FireAxis(NkU32 idx, NkGamepadAxis ax, NkF32 value, NkF32 prevValue) {
		NkGamepadAxisEvent event(idx, ax, value, prevValue);
		NkEventSystem::Instance().DispatchEvent(event);

		if (mAxisCb)
			mAxisCb(idx, ax, value);
	}

} // namespace nkentseu
