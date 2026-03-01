// =============================================================================
// NkSystem.cpp - global lifecycle: EventImpl + GamepadSystem
// =============================================================================

#include "NkSystem.h"
#include "NKWindow/Events/IEventImpl.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WIN32)
#include "../Platform/Win32/NkWin32EventImpl.h"
using PlatformEventImpl = nkentseu::NkWin32EventImpl;
#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
#include "../Platform/UWP/NkUWPEventImpl.h"
using PlatformEventImpl = nkentseu::NkUWPEventImpl;
#elif defined(NKENTSEU_PLATFORM_COCOA)
#include "../Platform/Cocoa/NkCocoaEventImpl.h"
using PlatformEventImpl = nkentseu::NkCocoaEventImpl;
#elif defined(NKENTSEU_PLATFORM_UIKIT)
#include "../Platform/UIKit/NkUIKitEventImpl.h"
using PlatformEventImpl = nkentseu::NkUIKitEventImpl;
#elif defined(NKENTSEU_PLATFORM_XCB)
#include "../Platform/XCB/NkXCBEventImpl.h"
using PlatformEventImpl = nkentseu::NkXCBEventImpl;
#elif defined(NKENTSEU_PLATFORM_XLIB)
#include "../Platform/XLib/NkXLibEventImpl.h"
using PlatformEventImpl = nkentseu::NkXLibEventImpl;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include "../Platform/Android/NkAndroidEventImpl.h"
using PlatformEventImpl = nkentseu::NkAndroidEventImpl;
#elif defined(NKENTSEU_PLATFORM_WASM)
#include "../Platform/WASM/NkWASMEventImpl.h"
using PlatformEventImpl = nkentseu::NkWASMEventImpl;
#else
#include "../Platform/Noop/NkNoopEventImpl.h"
using PlatformEventImpl = nkentseu::NkNoopEventImpl;
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

NkSystem &NkSystem::Instance() {
	static NkSystem sInstance;
	return sInstance;
}

bool NkSystem::Initialise(const NkAppData &data) {
	if (mInitialised)
		return true;

	mAppData = data;

	// One EventImpl instance shared by all windows.
	auto impl = std::make_unique<PlatformEventImpl>();
	if (!NkEventSystem::Instance().Init(std::move(impl)))
		return false;

	// One GamepadSystem instance per NkSystem.
	NkGamepadSystem::Instance().Init();

	mInitialised = true;
	return true;
}

void NkSystem::Close() {
	if (!mInitialised)
		return;

	NkGamepadSystem::Instance().Shutdown();

	NkEventSystem::Instance().Shutdown();

	mInitialised = false;
}

} // namespace nkentseu
