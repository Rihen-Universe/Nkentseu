#pragma once
// =============================================================================
// NkCocoaWindow.h - Cocoa (macOS) platform data for NkWindow (data only)
// =============================================================================

#ifdef __OBJC__
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#else
struct objc_object;
using NSWindow = struct objc_object;
using NSView = struct objc_object;
using CAMetalLayer = struct objc_object;
#endif

#include "NKWindow/Core/NkTypes.h"
#include "NKWindow/Core/NkSurfaceHint.h"
#include "NKWindow/Core/NkSurface.h" // NK_OBJC_NON_RETENU

namespace nkentseu {

	// Membres Objective-C en NK_OBJC_NON_RETENU (voir NkSurface.h) : NkWindow
	// est inclus par des unites ARC (NKCanvas, NKRHI) ; ses donnees doivent y
	// garder la MEME definition triviale que dans NKWindow (retain/release
	// manuel, ou ce sont ces membres qui portent les references).
	struct NkWindowData {
			NSWindow *NK_OBJC_NON_RETENU mNSWindow = nullptr;
			NSView *NK_OBJC_NON_RETENU mNSView = nullptr;
			CAMetalLayer *NK_OBJC_NON_RETENU mMetalLayer = nullptr;
			NSWindow *NK_OBJC_NON_RETENU mParentWindow = nullptr;
#ifdef __OBJC__
			id NK_OBJC_NON_RETENU mDelegate = nil;		  // NkCocoaWindowDelegate*
			id NK_OBJC_NON_RETENU mScreenObserver = nil; // token NSNotificationCenter (hot-plug écrans)
#else
			void *mDelegate = nullptr;
			void *mScreenObserver = nullptr;
#endif
			NkSurfaceHints mAppliedHints{};
			uint32 mWidth = 0;
			uint32 mHeight = 0;
			bool mVisible = false;
			bool mFullscreen = false;
			bool mExternal = false;
			bool mOwnsWindow = true;
	};

} // namespace nkentseu
