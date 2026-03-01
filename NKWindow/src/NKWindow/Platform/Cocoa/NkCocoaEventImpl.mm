// =============================================================================
// NkCocoaEventImpl.mm
// Pompe d'evenements NSApp pour macOS.
// =============================================================================

#import <Cocoa/Cocoa.h>

#include "NkCocoaEventImpl.h"
#include "NkCocoaWindowImpl.h"
#include "../../Events/NkKeycodeMap.h"
#include "../../Events/NkScancode.h"
#include "../../Events/NkKeyboardEvent.h"
#include "../../Events/NkMouseEvent.h"
#include "../../Events/NkWindowEvent.h"

#include <utility>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Initialize / Shutdown
// ---------------------------------------------------------------------------

void NkCocoaEventImpl::Initialize(IWindowImpl *owner, void *nativeHandle) {
	auto *window = static_cast<NkCocoaWindowImpl *>(owner);

	if (!nativeHandle && window)
		nativeHandle = reinterpret_cast<void *>(window->GetNSWindow());

	if (!nativeHandle)
		return;

	mWindowMap[nativeHandle] = {window, {}};
}

void NkCocoaEventImpl::Shutdown(void *nativeHandle) {
	if (nativeHandle) {
		mWindowMap.erase(nativeHandle);
		return;
	}

	if (!mWindowMap.empty())
		mWindowMap.erase(mWindowMap.begin());
}

// ---------------------------------------------------------------------------
// Queue
// ---------------------------------------------------------------------------

NkEvent *NkCocoaEventImpl::Front() const {
	return mQueue.empty() ? nullptr : mQueue.front().get();
}

void NkCocoaEventImpl::Pop() {
	if (!mQueue.empty())
		mQueue.pop();
}
bool NkCocoaEventImpl::IsEmpty() const {
	return mQueue.empty();
}
std::size_t NkCocoaEventImpl::Size() const {
	return mQueue.size();
}
void NkCocoaEventImpl::PushEvent(std::unique_ptr<NkEvent> e) {
	mQueue.push(std::move(e));
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void NkCocoaEventImpl::SetEventCallback(NkEventCallback cb) {
	mGlobalCallback = std::move(cb);
}

void NkCocoaEventImpl::SetWindowCallback(void *nativeHandle, NkEventCallback cb) {
	if (!nativeHandle) {
		for (auto it = mWindowMap.begin(); it != mWindowMap.end(); ++it)
			it->second.callback = cb;
		return;
	}

	auto it = mWindowMap.find(nativeHandle);
	if (it != mWindowMap.end())
		it->second.callback = std::move(cb);
}

void NkCocoaEventImpl::DispatchEvent(NkEvent *event, void *nativeHandle) {
	if (!event) return;
	if (nativeHandle) {
		auto it = mWindowMap.find(nativeHandle);
		if (it != mWindowMap.end() && it->second.callback)
			it->second.callback(event);
	} else {
		for (auto it = mWindowMap.begin(); it != mWindowMap.end(); ++it) {
			if (it->second.callback)
				it->second.callback(event);
		}
	}

	if (mGlobalCallback)
		mGlobalCallback(event);
}

// ---------------------------------------------------------------------------
// PollEvents
// ---------------------------------------------------------------------------

void NkCocoaEventImpl::PollEvents() {
	@autoreleasepool {
		NSEvent *ev = nil;
		while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
										untilDate:[NSDate distantPast]
										   inMode:NSDefaultRunLoopMode
										  dequeue:YES]) != nil) {
			[NSApp sendEvent:ev];

			auto Enqueue = [&](const NkEvent &evt) {
				mQueue.push(std::unique_ptr<NkEvent>(evt.Clone()));
			};

			switch ([ev type]) {
				case NSEventTypeKeyDown:
				case NSEventTypeKeyUp: {
					const unsigned short macKC = [ev keyCode];
					const NkScancode sc = NkScancodeFromMac(macKC);

					NkKey key = NkScancodeToKey(sc);
					if (key == NkKey::NK_UNKNOWN)
						key = MacKeycodeToNkKey(macKC);

					if (key != NkKey::NK_UNKNOWN) {
						const NkModifierState mods = NsModsToMods([ev modifierFlags]);
						const NkU32 nativeKey = static_cast<NkU32>(macKC);
						if ([ev type] == NSEventTypeKeyUp) {
							Enqueue(NkKeyReleaseEvent(key, sc, mods, nativeKey));
						} else if ([ev isARepeat] == YES) {
							Enqueue(NkKeyRepeatEvent(key, 1, sc, mods, nativeKey));
						} else {
							Enqueue(NkKeyPressEvent(key, sc, mods, nativeKey));
						}
					}
					break;
				}

				case NSEventTypeMouseMoved:
				case NSEventTypeLeftMouseDragged:
				case NSEventTypeRightMouseDragged:
				case NSEventTypeOtherMouseDragged: {
					const NSPoint p = [ev locationInWindow];
					const NSPoint screenP = [NSEvent mouseLocation];

					NkMouseButtons buttons;
					const NSUInteger pressedButtons = [NSEvent pressedMouseButtons];
					if (pressedButtons & (1u << 0))
						buttons.Set(NkMouseButton::NK_MB_LEFT);
					if (pressedButtons & (1u << 1))
						buttons.Set(NkMouseButton::NK_MB_RIGHT);
					if (pressedButtons & (1u << 2))
						buttons.Set(NkMouseButton::NK_MB_MIDDLE);
					if (pressedButtons & (1u << 3))
						buttons.Set(NkMouseButton::NK_MB_BACK);
					if (pressedButtons & (1u << 4))
						buttons.Set(NkMouseButton::NK_MB_FORWARD);

					Enqueue(NkMouseMoveEvent(
						static_cast<NkI32>(p.x), static_cast<NkI32>(p.y),
						static_cast<NkI32>(screenP.x), static_cast<NkI32>(screenP.y),
						static_cast<NkI32>([ev deltaX]), static_cast<NkI32>([ev deltaY]),
						buttons, NsModsToMods([ev modifierFlags])));
					break;
				}

				case NSEventTypeLeftMouseDown:
				case NSEventTypeLeftMouseUp:
				case NSEventTypeRightMouseDown:
				case NSEventTypeRightMouseUp:
				case NSEventTypeOtherMouseDown:
				case NSEventTypeOtherMouseUp: {
					NkMouseButton button = NkMouseButton::NK_MB_UNKNOWN;

					if ([ev type] == NSEventTypeLeftMouseDown || [ev type] == NSEventTypeLeftMouseUp) {
						button = NkMouseButton::NK_MB_LEFT;
					} else if ([ev type] == NSEventTypeRightMouseDown || [ev type] == NSEventTypeRightMouseUp) {
						button = NkMouseButton::NK_MB_RIGHT;
					} else {
						const NSInteger n = [ev buttonNumber];
						if (n == 2)
							button = NkMouseButton::NK_MB_MIDDLE;
						else if (n == 3)
							button = NkMouseButton::NK_MB_BACK;
						else if (n == 4)
							button = NkMouseButton::NK_MB_FORWARD;
					}

					if (button != NkMouseButton::NK_MB_UNKNOWN) {
						const bool isDown = ([ev type] == NSEventTypeLeftMouseDown) ||
											([ev type] == NSEventTypeRightMouseDown) ||
											([ev type] == NSEventTypeOtherMouseDown);

						const NSPoint p = [ev locationInWindow];
						const NSPoint screenP = [NSEvent mouseLocation];
						const NkModifierState mods = NsModsToMods([ev modifierFlags]);
						const NkU32 clickCount = static_cast<NkU32>([ev clickCount]);

						if (isDown) {
							Enqueue(NkMouseButtonPressEvent(
								button,
								static_cast<NkI32>(p.x), static_cast<NkI32>(p.y),
								static_cast<NkI32>(screenP.x), static_cast<NkI32>(screenP.y),
								clickCount, mods));
						} else {
							Enqueue(NkMouseButtonReleaseEvent(
								button,
								static_cast<NkI32>(p.x), static_cast<NkI32>(p.y),
								static_cast<NkI32>(screenP.x), static_cast<NkI32>(screenP.y),
								clickCount, mods));
						}
					}
					break;
				}

				case NSEventTypeScrollWheel: {
					const NSPoint p = [ev locationInWindow];
					const NkModifierState mods = NsModsToMods([ev modifierFlags]);
					const bool highPrecision = ([ev hasPreciseScrollingDeltas] == YES);
					const double deltaY = [ev scrollingDeltaY];
					const double deltaX = [ev scrollingDeltaX];
					const NkI32 px = static_cast<NkI32>(p.x);
					const NkI32 py = static_cast<NkI32>(p.y);

					if (deltaY != 0.0) {
						Enqueue(NkMouseWheelVerticalEvent(
							deltaY, px, py,
							highPrecision ? deltaY : 0.0,
							highPrecision, mods));
					}
					if (deltaX != 0.0) {
						Enqueue(NkMouseWheelHorizontalEvent(
							deltaX, px, py,
							highPrecision ? deltaX : 0.0,
							highPrecision, mods));
					}
					break;
				}

				default:
					break;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

NkKey NkCocoaEventImpl::MacKeycodeToNkKey(unsigned short code) {
	return NkKeycodeMap::NkKeyFromMacKeyCode(static_cast<NkU16>(code));
}

NkModifierState NkCocoaEventImpl::NsModsToMods(unsigned long flags) {
	NkModifierState mods;
	mods.ctrl = !!(flags & NSEventModifierFlagControl);
	mods.alt = !!(flags & NSEventModifierFlagOption);
	mods.shift = !!(flags & NSEventModifierFlagShift);
	mods.super = !!(flags & NSEventModifierFlagCommand);
	mods.capLock = !!(flags & NSEventModifierFlagCapsLock);
	return mods;
}


const char* NkCocoaEventImpl::GetPlatformName() const noexcept { return "Cocoa"; }
bool NkCocoaEventImpl::IsReady() const noexcept { return true; }

} // namespace nkentseu
