// =============================================================================
// NkCocoaEventSystem.mm
// Implémentation Cocoa (macOS) des méthodes platform-spécifiques de NkEventSystem.
//
// Architecture Cocoa :
//   PumpOS() pompe la queue NSApp via nextEventMatchingMask et appelle
//   Enqueue(evt, winId) pour chaque événement trouvé.
//
// Les événements NSApp sont processés en @autoreleasepool pour éviter les
// fuites mémoire Objective-C.
// =============================================================================

#import <Cocoa/Cocoa.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Platform/Cocoa/NkCocoaEventSystem.h"
#include "NKWindow/Events/NkKeyboardEvent.h"
#include "NKWindow/Events/NkMouseEvent.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkKeycodeMap.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include <mutex>

namespace nkentseu {

    // =============================================================================
    // Helpers
    // =============================================================================

    static NkModifierState CocoaNsMods(NSEventModifierFlags flags) {
        NkModifierState m;
        m.ctrl    = !!(flags & NSEventModifierFlagControl);
        m.alt     = !!(flags & NSEventModifierFlagOption);
        m.shift   = !!(flags & NSEventModifierFlagShift);
        m.super   = !!(flags & NSEventModifierFlagCommand);
        m.capLock = !!(flags & NSEventModifierFlagCapsLock);
        return m;
    }

    static NkWindowId FindWindowForNSWindow(NSWindow* nswin) {
        if (!nswin) return NK_INVALID_WINDOW_ID;
        uint32 count = NkSystem::Instance().GetWindowCount();
        for (uint32 i = 0; i < count; ++i) {
            NkWindow* w = NkSystem::Instance().GetWindowAt(i);
            if (w && w->mData.mNSWindow == nswin)
                return w->GetId();
        }
        return NK_INVALID_WINDOW_ID;
    }

    // =============================================================================
    // NkEventSystem — méthodes platform-spécifiques Cocoa
    // =============================================================================

    bool NkEventSystem::Init() {
        if (mReady) return true;
        mTotalEventCount = 0;
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;

        // S'assurer que NSApp est initialisé
        if (![NSApplication sharedApplication]) {
            [NSApplication sharedApplication];
        }
        if (![NSApp isRunning]) {
            [NSApp finishLaunching];
        }

        mData.mInitialized = true;
        mReady = true;
        return true;
    }

    void NkEventSystem::PumpOS() {
        if (mPumping) return;
        mPumping = true;

        @autoreleasepool {
            NSEvent* ev = nil;
            while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                            inMode:NSDefaultRunLoopMode
                                            dequeue:YES]) != nil) {
                // Laisser NSApp traiter ses propres événements internes
                [NSApp sendEvent:ev];

                NSWindow* srcWin = [ev window];
                NkWindowId winId = FindWindowForNSWindow(srcWin);

                switch ([ev type]) {

                // ----------------------------------------------------------------
                // Clavier
                // ----------------------------------------------------------------
                case NSEventTypeKeyDown:
                case NSEventTypeKeyUp: {
                    unsigned short macKC = [ev keyCode];
                    NkScancode sc  = NkScancodeFromMac(macKC);
                    NkKey      key = NkScancodeToKey(sc);
                    if (key == NkKey::NK_UNKNOWN)
                        key = NkKeycodeMap::NkKeyFromMacKeyCode(macKC);

                    if (key != NkKey::NK_UNKNOWN) {
                        NkModifierState mods = CocoaNsMods([ev modifierFlags]);
                        uint32 nativeKey = static_cast<uint32>(macKC);
                        if ([ev type] == NSEventTypeKeyUp) {
                            NkKeyReleaseEvent e(key, sc, mods, nativeKey);
                            Enqueue(e, winId);
                        } else if ([ev isARepeat]) {
                            NkKeyRepeatEvent e(key, 1, sc, mods, nativeKey);
                            Enqueue(e, winId);
                        } else {
                            NkKeyPressEvent e(key, sc, mods, nativeKey);
                            Enqueue(e, winId);
                        }
                    }

                    // Texte saisi
                    if ([ev type] == NSEventTypeKeyDown) {
                        NSString* chars = [ev characters];
                        if (chars && [chars length] > 0) {
                            unichar c = [chars characterAtIndex:0];
                            if (c >= 0x20 && c != 0x7F) {
                                NkTextInputEvent e(static_cast<uint32>(c));
                                Enqueue(e, winId);
                            }
                        }
                    }
                    break;
                }

                // ----------------------------------------------------------------
                // Souris — déplacement
                // ----------------------------------------------------------------
                case NSEventTypeMouseMoved:
                case NSEventTypeLeftMouseDragged:
                case NSEventTypeRightMouseDragged:
                case NSEventTypeOtherMouseDragged: {
                    NSPoint p      = [ev locationInWindow];
                    NSPoint screen = [NSEvent mouseLocation];
                    NkMouseButtons btns;
                    NSUInteger pb = [NSEvent pressedMouseButtons];
                    if (pb & (1u << 0)) btns.Set(NkMouseButton::NK_MB_LEFT);
                    if (pb & (1u << 1)) btns.Set(NkMouseButton::NK_MB_RIGHT);
                    if (pb & (1u << 2)) btns.Set(NkMouseButton::NK_MB_MIDDLE);
                    NkMouseMoveEvent e(
                        (int32)p.x, (int32)p.y,
                        (int32)screen.x, (int32)screen.y,
                        (int32)[ev deltaX], (int32)[ev deltaY],
                        btns, CocoaNsMods([ev modifierFlags]));
                    Enqueue(e, winId);
                    break;
                }

                // ----------------------------------------------------------------
                // Souris — boutons
                // ----------------------------------------------------------------
                case NSEventTypeLeftMouseDown:
                case NSEventTypeLeftMouseUp:
                case NSEventTypeRightMouseDown:
                case NSEventTypeRightMouseUp:
                case NSEventTypeOtherMouseDown:
                case NSEventTypeOtherMouseUp: {
                    NkMouseButton btn = NkMouseButton::NK_MB_UNKNOWN;
                    NSEventType t = [ev type];
                    if (t == NSEventTypeLeftMouseDown  || t == NSEventTypeLeftMouseUp)  btn = NkMouseButton::NK_MB_LEFT;
                    else if (t == NSEventTypeRightMouseDown || t == NSEventTypeRightMouseUp) btn = NkMouseButton::NK_MB_RIGHT;
                    else {
                        NSInteger n = [ev buttonNumber];
                        if (n == 2) btn = NkMouseButton::NK_MB_MIDDLE;
                        else if (n == 3) btn = NkMouseButton::NK_MB_BACK;
                        else if (n == 4) btn = NkMouseButton::NK_MB_FORWARD;
                    }
                    if (btn != NkMouseButton::NK_MB_UNKNOWN) {
                        NSPoint p      = [ev locationInWindow];
                        NSPoint screen = [NSEvent mouseLocation];
                        NkModifierState mods = CocoaNsMods([ev modifierFlags]);
                        uint32 clicks = (uint32)[ev clickCount];
                        bool isDown = (t == NSEventTypeLeftMouseDown  ||
                                    t == NSEventTypeRightMouseDown ||
                                    t == NSEventTypeOtherMouseDown);
                        if (isDown) {
                            NkMouseButtonPressEvent e(btn, (int32)p.x, (int32)p.y,
                                                    (int32)screen.x, (int32)screen.y, clicks, mods);
                            Enqueue(e, winId);
                        } else {
                            NkMouseButtonReleaseEvent e(btn, (int32)p.x, (int32)p.y,
                                                        (int32)screen.x, (int32)screen.y, clicks, mods);
                            Enqueue(e, winId);
                        }
                    }
                    break;
                }

                // ----------------------------------------------------------------
                // Molette
                // ----------------------------------------------------------------
                case NSEventTypeScrollWheel: {
                    NSPoint p = [ev locationInWindow];
                    NkModifierState mods = CocoaNsMods([ev modifierFlags]);
                    bool precise = ([ev hasPreciseScrollingDeltas] == YES);
                    double dy = [ev scrollingDeltaY];
                    double dx = [ev scrollingDeltaX];
                    int32 px = (int32)p.x, py = (int32)p.y;
                    if (dy != 0.0) {
                        NkMouseWheelVerticalEvent   e(dy, px, py, precise ? dy : 0.0, precise, mods);
                        Enqueue(e, winId);
                    }
                    if (dx != 0.0) {
                        NkMouseWheelHorizontalEvent e(dx, px, py, precise ? dx : 0.0, precise, mods);
                        Enqueue(e, winId);
                    }
                    break;
                }

                // ----------------------------------------------------------------
                // Entrée/sortie curseur
                // ----------------------------------------------------------------
                case NSEventTypeMouseEntered: {
                    NkMouseEnterEvent e;
                    Enqueue(e, winId);
                    break;
                }
                case NSEventTypeMouseExited: {
                    NkMouseLeaveEvent e;
                    Enqueue(e, winId);
                    break;
                }

                default:
                    break;
                }
            }
        } // @autoreleasepool

        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Cocoa";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
