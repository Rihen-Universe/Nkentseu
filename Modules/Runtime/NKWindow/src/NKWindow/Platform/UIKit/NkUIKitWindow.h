#pragma once
// =============================================================================
// NkUIKitWindow.h - UIKit (iOS/tvOS) platform data for NkWindow (data only)
// =============================================================================

#ifdef __OBJC__
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>
#else
struct objc_object;
using UIWindow = struct objc_object;
using UIView = struct objc_object;
using CAMetalLayer = struct objc_object;
#endif

#include "NKWindow/Core/NkTypes.h"

namespace nkentseu {

    struct NkWindowData {
            UIWindow*      mUIWindow   = nullptr;
            UIView*        mUIView     = nullptr;
            CAMetalLayer*  mMetalLayer = nullptr;
            uint32 mWidth      = 0;
            uint32 mHeight     = 0;
            bool  mVisible    = false;
            bool  mFullscreen = false;
    };

} // namespace nkentseu
