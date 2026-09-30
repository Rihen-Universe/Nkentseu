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

#include "NKEvent/NkEventSystem.h"
#include "NKMemory/NkAllocator.h" // NkGetDefaultAllocator().New/Delete (regle maison : pas de new/delete)
#include "NKWindow/Platform/Cocoa/NkCocoaEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeycodeMap.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKWindow/Core/NkWindow.h"

namespace nkentseu {
	using namespace math;

	// =============================================================================
	// Helpers
	// =============================================================================

	static NkModifierState CocoaNsMods(NSEventModifierFlags flags) {
		NkModifierState m;
		m.ctrl = !!(flags & NSEventModifierFlagControl);
		m.alt = !!(flags & NSEventModifierFlagOption);
		m.shift = !!(flags & NSEventModifierFlagShift);
		m.super = !!(flags & NSEventModifierFlagCommand);
		m.capLock = !!(flags & NSEventModifierFlagCapsLock);
		return m;
	}

	// ── Coordonnées : le contrat de NkMouseEvent, pas celui d'AppKit ──
	// NkMouseMoveEvent promet des PIXELS de la zone client, origine en HAUT à
	// gauche (Win32, X11, Wayland font ainsi). AppKit donne des POINTS, origine
	// en BAS à gauche. Sans conversion, sur l'écran Retina d'un MacBook, le clic
	// arrivait à la moitié de sa position et renversé de haut en bas.
	static NSPoint CocoaClientPx(NSEvent *ev) {
		NSWindow *w = [ev window];
		const NSPoint p = [ev locationInWindow];
		if (!w) {
			return p;
		}
		NSView *v = [w contentView];
		const NSPoint local = v ? [v convertPoint:p fromView:nil] : p;
		const CGFloat hauteur = v ? v.bounds.size.height : [w contentRectForFrameRect:w.frame].size.height;
		const CGFloat echelle = w.backingScaleFactor;
		const CGFloat y = (v && v.isFlipped) ? local.y : (hauteur - local.y);
		return NSMakePoint(local.x * echelle, y * echelle);
	}

	// Même contrat pour l'écran : origine en haut à gauche de l'écran PRINCIPAL
	// (celui de la barre de menus, [NSScreen screens][0]), en pixels.
	static NSPoint CocoaScreenPx() {
		const NSPoint p = [NSEvent mouseLocation];
		NSArray<NSScreen *> *ecrans = [NSScreen screens];
		NSScreen *principal = ecrans.count > 0 ? ecrans[0] : [NSScreen mainScreen];
		if (!principal) {
			return p;
		}
		const CGFloat echelle = principal.backingScaleFactor;
		return NSMakePoint(p.x * echelle, (principal.frame.size.height - p.y) * echelle);
	}

	static CGFloat CocoaEchelle(NSEvent *ev) {
		NSWindow *w = [ev window];
		return w ? w.backingScaleFactor : 1.0;
	}

	// ── Touches de modification seules (Maj, Ctrl, Option, Cmd, Verr. Maj) ──
	// AppKit ne les envoie PAS en KeyDown/KeyUp mais en FlagsChanged, sans dire
	// si la touche descend ou remonte : on le lit dans les bits « par côté »
	// des drapeaux (NX_DEVICE*KEYMASK de IOKit, stables depuis 10.0). Sans ce
	// cas, appuyer sur Maj seul n'émettait rien.
	static bool CocoaModificateurEnfonce(unsigned short macKC, NSEventModifierFlags flags) {
		const NSUInteger bits = static_cast<NSUInteger>(flags);
		switch (macKC) {
			case 0x38: return (bits & 0x00000002u) != 0;  // Maj gauche
			case 0x3C: return (bits & 0x00000004u) != 0;  // Maj droite
			case 0x3B: return (bits & 0x00000001u) != 0;  // Ctrl gauche
			case 0x3E: return (bits & 0x00002000u) != 0;  // Ctrl droite
			case 0x3A: return (bits & 0x00000020u) != 0;  // Option gauche
			case 0x3D: return (bits & 0x00000040u) != 0;  // Option droite
			case 0x37: return (bits & 0x00000008u) != 0;  // Cmd gauche
			case 0x36: return (bits & 0x00000010u) != 0;  // Cmd droite
			case 0x39: return (flags & NSEventModifierFlagCapsLock) != 0;
			default:   return false;
		}
	}

	static NkWindowId FindWindowForNSWindow(NSWindow *nswin) {
		if (!nswin)
			return NK_INVALID_WINDOW_ID;
		uint32 count = NkWESystem::Instance().GetWindowCount();
		for (uint32 i = 0; i < count; ++i) {
			NkWindow *w = NkWESystem::Instance().GetWindowAt(i);
			if (w && w->mData.mNSWindow == nswin)
				return w->GetId();
		}
		return NK_INVALID_WINDOW_ID;
	}

	// =============================================================================
	// NkEventSystem — méthodes platform-spécifiques Cocoa
	// =============================================================================

	bool NkEventSystem::Init() {
		if (mReady)
			return true;

		mData = memory::NkGetDefaultAllocator().New<NkEventSystemData>();
		if (mData == nullptr)
			return false;

		mTotalEventCount = 0;
		{
			NkScopedSpinLock lock(mQueueMutex);
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

		mData->mInitialized = true;
		mReady = true;
		return true;
	}

	void NkEventSystem::Shutdown() {
		memory::NkGetDefaultAllocator().Delete(mData);
		mData = nullptr;
	}

	void NkEventSystem::PumpOS() {
		if (mPumping)
			return;
		mPumping = true;

		@autoreleasepool {
			NSEvent *ev = nil;
			while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
											untilDate:[NSDate distantPast]
											   inMode:NSDefaultRunLoopMode
											  dequeue:YES]) != nil) {
				// Laisser NSApp traiter ses propres événements internes
				[NSApp sendEvent:ev];

				NSWindow *srcWin = [ev window];
				NkWindowId winId = FindWindowForNSWindow(srcWin);

				switch ([ev type]) {
					// ----------------------------------------------------------------
					// Clavier
					// ----------------------------------------------------------------
					case NSEventTypeKeyDown:
					case NSEventTypeKeyUp: {
						unsigned short macKC = [ev keyCode];
						NkScancode sc = NkScancodeFromMac(macKC);
						NkKey key = NkScancodeToKey(sc);
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
						// ⚠️ Deux filtres que Win32 fait sans qu'on le voie (WM_CHAR) :
						//    - les touches de fonction et les flèches portent un
						//      caractère de la zone privée 0xF700-0xF8FF
						//      (NSUpArrowFunctionKey…) : sans filtre, chaque flèche
						//      tapait un glyphe invisible dans les champs de texte ;
						//    - Cmd+C n'est pas la lettre « c » : un raccourci ne
						//      saisit rien.
						const NSEventModifierFlags drapeaux = [ev modifierFlags];
						const bool raccourci =
							(drapeaux & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;
						if ([ev type] == NSEventTypeKeyDown && !raccourci) {
							NSString *chars = [ev characters];
							const NSUInteger n = chars ? [chars length] : 0;
							for (NSUInteger i = 0; i < n; ++i) {
								const unichar c = [chars characterAtIndex:i];
								const bool fonction = (c >= 0xF700 && c <= 0xF8FF);
								if (c >= 0x20 && c != 0x7F && !fonction) {
									NkTextInputEvent e(static_cast<uint32>(c));
									Enqueue(e, winId);
								}
							}
						}
						break;
					}

					// ----------------------------------------------------------------
					// Touches de modification seules
					// ----------------------------------------------------------------
					case NSEventTypeFlagsChanged: {
						const unsigned short macKC = [ev keyCode];
						const NkScancode sc = NkScancodeFromMac(macKC);
						NkKey key = NkScancodeToKey(sc);
						if (key == NkKey::NK_UNKNOWN)
							key = NkKeycodeMap::NkKeyFromMacKeyCode(macKC);
						if (key == NkKey::NK_UNKNOWN)
							break;
						const NkModifierState mods = CocoaNsMods([ev modifierFlags]);
						const uint32 nativeKey = static_cast<uint32>(macKC);
						if (CocoaModificateurEnfonce(macKC, [ev modifierFlags])) {
							NkKeyPressEvent e(key, sc, mods, nativeKey);
							Enqueue(e, winId);
						} else {
							NkKeyReleaseEvent e(key, sc, mods, nativeKey);
							Enqueue(e, winId);
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
						const NSPoint p = CocoaClientPx(ev);
						const NSPoint screen = CocoaScreenPx();
						// deltaY d'un mouvement est deja « vers le bas positif »
						// (repere Quartz) : seule l'echelle manque.
						const CGFloat echelle = CocoaEchelle(ev);
						NkMouseButtons btns;
						NSUInteger pb = [NSEvent pressedMouseButtons];
						if (pb & (1u << 0))
							btns.Set(NkMouseButton::NK_MB_LEFT);
						if (pb & (1u << 1))
							btns.Set(NkMouseButton::NK_MB_RIGHT);
						if (pb & (1u << 2))
							btns.Set(NkMouseButton::NK_MB_MIDDLE);
						NkMouseMoveEvent e((int32)p.x, (int32)p.y, (int32)screen.x, (int32)screen.y,
										   (int32)([ev deltaX] * echelle), (int32)([ev deltaY] * echelle), btns,
										   CocoaNsMods([ev modifierFlags]));
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
						if (t == NSEventTypeLeftMouseDown || t == NSEventTypeLeftMouseUp)
							btn = NkMouseButton::NK_MB_LEFT;
						else if (t == NSEventTypeRightMouseDown || t == NSEventTypeRightMouseUp)
							btn = NkMouseButton::NK_MB_RIGHT;
						else {
							NSInteger n = [ev buttonNumber];
							if (n == 2)
								btn = NkMouseButton::NK_MB_MIDDLE;
							else if (n == 3)
								btn = NkMouseButton::NK_MB_BACK;
							else if (n == 4)
								btn = NkMouseButton::NK_MB_FORWARD;
						}
						if (btn != NkMouseButton::NK_MB_UNKNOWN) {
							const NSPoint p = CocoaClientPx(ev);
							const NSPoint screen = CocoaScreenPx();
							NkModifierState mods = CocoaNsMods([ev modifierFlags]);
							uint32 clicks = (uint32)[ev clickCount];
							bool isDown = (t == NSEventTypeLeftMouseDown || t == NSEventTypeRightMouseDown ||
										   t == NSEventTypeOtherMouseDown);
							if (isDown) {
								NkMouseButtonPressEvent e(btn, (int32)p.x, (int32)p.y, (int32)screen.x, (int32)screen.y,
														  clicks, mods);
								Enqueue(e, winId);
							} else {
								NkMouseButtonReleaseEvent e(btn, (int32)p.x, (int32)p.y, (int32)screen.x,
															(int32)screen.y, clicks, mods);
								Enqueue(e, winId);
							}
						}
						break;
					}

					// ----------------------------------------------------------------
					// Molette
					// ----------------------------------------------------------------
					case NSEventTypeScrollWheel: {
						const NSPoint p = CocoaClientPx(ev);
						NkModifierState mods = CocoaNsMods([ev modifierFlags]);
						bool precise = ([ev hasPreciseScrollingDeltas] == YES);
						double dy = [ev scrollingDeltaY];
						double dx = [ev scrollingDeltaX];
						int32 px = (int32)p.x, py = (int32)p.y;
						if (dy != 0.0) {
							NkMouseWheelVerticalEvent e(dy, px, py, precise ? dy : 0.0, precise, mods);
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

	const char *NkEventSystem::GetPlatformName() const noexcept {
		return "Cocoa";
	}

	void NkEventSystem::Enqueue_Public(NkEvent &evt, NkWindowId winId) {
		Enqueue(evt, winId);
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
