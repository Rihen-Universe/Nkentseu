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

namespace nkentseu {

    struct NkWindowData {
            NSWindow*      mNSWindow   = nullptr;
            NSView*        mNSView     = nullptr;
            CAMetalLayer*  mMetalLayer = nullptr;
            NSWindow*      mParentWindow = nullptr;
#ifdef __OBJC__
            id             mDelegate   = nil;   // NkCocoaWindowDelegate*
#else
            void*          mDelegate   = nullptr;
#endif
            NkSurfaceHints mAppliedHints{};
            uint32 mWidth      = 0;
            uint32 mHeight     = 0;
            bool  mVisible    = false;
            bool  mFullscreen = false;
            bool  mExternal   = false;
            bool  mOwnsWindow = true;
    };

} // namespace nkentseu
