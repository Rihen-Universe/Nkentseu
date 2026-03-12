#pragma once
// =============================================================================
// NkAndroidWindow.h — Android platform data for NkWindow (data only)
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkSafeArea.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkWindowId.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include <android/native_window.h>
#include <vector>
#include "NKContainers/Sequential/NkVector.h"

struct AConfiguration;
struct android_app;

namespace nkentseu {

    class NkAndroidDropTarget;

	    struct NkWindowData {
	            ANativeWindow*      mNativeWindow  = nullptr;
	            AConfiguration*     mAConfig       = nullptr;
	            struct android_app* mAndroidApp    = nullptr;
	            NkAndroidDropTarget* mDropTarget   = nullptr;
	            NkSurfaceHints      mAppliedHints{};
	            bool                mExternal      = false;

	            uint32               mWidth         = 0;
	            uint32               mHeight        = 0;
	            uint32               mPrevWidth     = 0;
            uint32               mPrevHeight    = 0;

            NkSafeAreaInsets    mSafeArea{};
            NkScreenOrientation mOrientation   = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
    };

    class NkWindow;
    NkWindow*              NkAndroidFindWindowById(NkWindowId id);
    NkVector<NkWindow*> NkAndroidGetWindowsSnapshot();
    NkWindow*              NkAndroidGetLastWindow();
    void                   NkAndroidRegisterWindow(NkWindow* window);
    void                   NkAndroidUnregisterWindow(NkWindow* window);

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_ANDROID
