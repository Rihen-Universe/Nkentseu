// =============================================================================
// NkUIKitWindow.mm
// UIKit implementation of NkWindow without PIMPL.
// =============================================================================

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_IOS)

#include "NKWindow/Platform/UIKit/NkUIKitWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKWindow/Events/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"

#include <algorithm>
#include <cstdint>

@interface NkUIKitTouchView : UIView {
@public
    nkentseu::NkWindow* mOwner;
}
@end

@implementation NkUIKitTouchView

- (void)nk_dispatchTouches:(NSSet<UITouch*>*)touches phase:(nkentseu::NkTouchPhase)phase {
    if (!mOwner || !mOwner->IsOpen() || touches.count == 0) {
        return;
    }

    nkentseu::NkTouchPoint points[nkentseu::NK_MAX_TOUCH_POINTS];
    nkentseu::uint32 count = 0;
    const float scale = static_cast<float>([UIScreen mainScreen].scale);
    const float width = math::NkMax(1.0f, static_cast<float>(self.bounds.size.width));
    const float height = math::NkMax(1.0f, static_cast<float>(self.bounds.size.height));

    for (UITouch* touch in touches) {
        if (count >= nkentseu::NK_MAX_TOUCH_POINTS) {
            break;
        }

        CGPoint pos = [touch locationInView:self];
        CGPoint prev = [touch previousLocationInView:self];
        CGPoint screen = [touch locationInView:nil];

        nkentseu::NkTouchPoint& out = points[count++];
        out.id = static_cast<nkentseu::uint64>(reinterpret_cast<std::uintptr_t>(touch));
        out.phase = phase;
        out.clientX = static_cast<float>(pos.x) * scale;
        out.clientY = static_cast<float>(pos.y) * scale;
        out.screenX = static_cast<float>(screen.x) * scale;
        out.screenY = static_cast<float>(screen.y) * scale;
        out.normalX = static_cast<float>(pos.x) / width;
        out.normalY = static_cast<float>(pos.y) / height;
        out.deltaX = static_cast<float>(pos.x - prev.x) * scale;
        out.deltaY = static_cast<float>(pos.y - prev.y) * scale;
        out.pressure = 1.0f;

        if (@available(iOS 9.0, tvOS 9.0, *)) {
            if (touch.maximumPossibleForce > 0.0f) {
                out.pressure = static_cast<float>(touch.force / touch.maximumPossibleForce);
            }
        }
        if (@available(iOS 8.0, tvOS 9.0, *)) {
            const float r = static_cast<float>(touch.majorRadius) * scale;
            out.radiusX = r;
            out.radiusY = r;
        }
    }

    if (count == 0) {
        return;
    }

    switch (phase) {
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_BEGAN: {
            nkentseu::NkTouchBeginEvent event(points, count);
            nkentseu::NkSystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_MOVED: {
            nkentseu::NkTouchMoveEvent event(points, count);
            nkentseu::NkSystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_ENDED: {
            nkentseu::NkTouchEndEvent event(points, count);
            nkentseu::NkSystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_CANCELLED: {
            nkentseu::NkTouchCancelEvent event(points, count);
            nkentseu::NkSystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        default:
            break;
    }
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self nk_dispatchTouches:touches phase:nkentseu::NkTouchPhase::NK_TOUCH_PHASE_BEGAN];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self nk_dispatchTouches:touches phase:nkentseu::NkTouchPhase::NK_TOUCH_PHASE_MOVED];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self nk_dispatchTouches:touches phase:nkentseu::NkTouchPhase::NK_TOUCH_PHASE_ENDED];
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    [self nk_dispatchTouches:touches phase:nkentseu::NkTouchPhase::NK_TOUCH_PHASE_CANCELLED];
}

@end

namespace nkentseu {

    static NkVec2u QueryViewSizePx(UIView* view) {
        if (!view) {
            return {0u, 0u};
        }
        const float scale = static_cast<float>([UIScreen mainScreen].scale);
        const uint32 w = static_cast<uint32>(math::NkMax(0.0f, static_cast<float>(view.bounds.size.width)) * scale);
        const uint32 h = static_cast<uint32>(math::NkMax(0.0f, static_cast<float>(view.bounds.size.height)) * scale);
        return {w, h};
    }

    NkWindow::NkWindow() = default;

    NkWindow::NkWindow(const NkWindowConfig& config) {
        Create(config);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) {
            Close();
        }
    }

    bool NkWindow::Create(const NkWindowConfig& config) {
        if (mIsOpen) {
            Close();
        }

        mConfig = config;

        @autoreleasepool {
            UIScreen* screen = [UIScreen mainScreen];
            CGRect frame = screen.bounds;

            UIWindow* window = [[UIWindow alloc] initWithFrame:frame];
            NkUIKitTouchView* view = [[NkUIKitTouchView alloc] initWithFrame:frame];
            view->mOwner = this;
            view.multipleTouchEnabled = YES;
            view.backgroundColor = [UIColor blackColor];
            view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

            CAMetalLayer* metalLayer = [CAMetalLayer layer];
            metalLayer.frame = view.bounds;
            metalLayer.contentsScale = screen.scale;
            [view.layer addSublayer:metalLayer];

            UIViewController* controller = [[UIViewController alloc] init];
            controller.view = view;
            window.rootViewController = controller;

            mData.mUIWindow = window;
            mData.mUIView = view;
            mData.mMetalLayer = metalLayer;
            mData.mVisible = config.visible;
            mData.mFullscreen = config.fullscreen;

            const NkVec2u sizePx = QueryViewSizePx(view);
            mData.mWidth = sizePx.x;
            mData.mHeight = sizePx.y;

            mId = NkSystem::Instance().RegisterWindow(this);
            if (mId == NK_INVALID_WINDOW_ID) {
                mLastError = NkError(1, "UIKit: failed to register window");
                return false;
            }

            if (config.visible) {
                [window makeKeyAndVisible];
            } else {
                window.hidden = YES;
            }
        }

        mIsOpen = true;

        NkWindowCreateEvent createEvent(mData.mWidth, mData.mHeight);
        NkSystem::Events().Enqueue_Public(createEvent, mId);

        if (mData.mVisible) {
            NkWindowShownEvent shownEvent;
            NkSystem::Events().Enqueue_Public(shownEvent, mId);
        }

        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }

        const NkWindowId closingId = mId;

        NkWindowCloseEvent closeEvent(false);
        NkSystem::Events().Enqueue_Public(closeEvent, closingId);

        @autoreleasepool {
            if (mData.mUIView) {
                static_cast<NkUIKitTouchView*>(mData.mUIView)->mOwner = nullptr;
            }
            if (mData.mUIWindow) {
                mData.mUIWindow.hidden = YES;
            }
            mData.mUIWindow = nil;
            mData.mUIView = nil;
            mData.mMetalLayer = nil;
        }

        NkWindowDestroyEvent destroyEvent;
        NkSystem::Events().Enqueue_Public(destroyEvent, closingId);

        NkSystem::Instance().UnregisterWindow(closingId);

        mId = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
        mData.mWidth = 0;
        mData.mHeight = 0;
        mData.mVisible = false;
        mData.mFullscreen = false;
    }

    bool NkWindow::IsOpen() const {
        return mIsOpen;
    }

    bool NkWindow::IsValid() const {
        return mIsOpen && mData.mUIView != nil;
    }

    NkError NkWindow::GetLastError() const {
        return mLastError;
    }

    NkWindowConfig NkWindow::GetConfig() const {
        return mConfig;
    }

    NkString NkWindow::GetTitle() const {
        return mConfig.title;
    }

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
    }

    NkVec2u NkWindow::GetSize() const {
        const NkVec2u sizePx = QueryViewSizePx(mData.mUIView);
        return sizePx;
    }

    NkVec2u NkWindow::GetPosition() const {
        return {0u, 0u};
    }

    float NkWindow::GetDpiScale() const {
        return static_cast<float>([UIScreen mainScreen].scale);
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        UIScreen* screen = [UIScreen mainScreen];
        const float scale = static_cast<float>(screen.scale);
        return {
            static_cast<uint32>(screen.bounds.size.width * scale),
            static_cast<uint32>(screen.bounds.size.height * scale)
        };
    }

    NkVec2u NkWindow::GetDisplayPosition() const {
        return {0u, 0u};
    }

    void NkWindow::SetSize(uint32 width, uint32 height) {
        if (!mData.mUIView) {
            return;
        }

        const float scale = GetDpiScale();
        const CGFloat wPt = static_cast<CGFloat>(std::max<uint32>(width, 1u)) / scale;
        const CGFloat hPt = static_cast<CGFloat>(std::max<uint32>(height, 1u)) / scale;
        const CGRect frame = CGRectMake(0.0, 0.0, wPt, hPt);

        const uint32 oldW = mData.mWidth;
        const uint32 oldH = mData.mHeight;

        mData.mUIView.frame = frame;
        if (mData.mMetalLayer) {
            mData.mMetalLayer.frame = mData.mUIView.bounds;
        }

        const NkVec2u sizePx = QueryViewSizePx(mData.mUIView);
        mData.mWidth = sizePx.x;
        mData.mHeight = sizePx.y;
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;

        NkWindowResizeEvent resizeEvent(mData.mWidth, mData.mHeight, oldW, oldH);
        NkSystem::Events().Enqueue_Public(resizeEvent, mId);
    }

    void NkWindow::SetPosition(int32, int32) {}

    void NkWindow::SetVisible(bool visible) {
        if (!mData.mUIWindow || mData.mVisible == visible) {
            return;
        }
        mData.mVisible = visible;
        mConfig.visible = visible;
        mData.mUIWindow.hidden = !visible;

        if (visible) {
            NkWindowShownEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowHiddenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        }
    }

    void NkWindow::Minimize() {}

    void NkWindow::Maximize() {}

    void NkWindow::Restore() {}

    void NkWindow::SetFullscreen(bool fullscreen) {
        if (mData.mFullscreen == fullscreen) {
            return;
        }
        mData.mFullscreen = fullscreen;
        mConfig.fullscreen = fullscreen;

        if (fullscreen) {
            NkWindowFullscreenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowWindowedEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        }
    }

    bool NkWindow::SupportsOrientationControl() const {
        return true;
    }

    void NkWindow::SetScreenOrientation(NkScreenOrientation orientation) {
        mConfig.screenOrientation = orientation;
    }

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return mConfig.screenOrientation;
    }

    void NkWindow::SetAutoRotateEnabled(bool enabled) {
        if (enabled) {
            mConfig.screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
        }
    }

    bool NkWindow::IsAutoRotateEnabled() const {
        return mConfig.screenOrientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
    }

    void NkWindow::SetMousePosition(uint32, uint32) {}

    void NkWindow::ShowMouse(bool) {}

    void NkWindow::CaptureMouse(bool) {}

    void NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}

    NkWebInputOptions NkWindow::GetWebInputOptions() const {
        return {};
    }

    void NkWindow::SetProgress(float) {}

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
        NkSafeAreaInsets insets{};
        if (!mData.mUIView) {
            return insets;
        }

        if (@available(iOS 11.0, tvOS 11.0, *)) {
            const float scale = GetDpiScale();
            UIEdgeInsets safe = mData.mUIView.safeAreaInsets;
            insets.top = static_cast<float>(safe.top) * scale;
            insets.bottom = static_cast<float>(safe.bottom) * scale;
            insets.left = static_cast<float>(safe.left) * scale;
            insets.right = static_cast<float>(safe.right) * scale;
        }
        return insets;
    }

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        const NkVec2u size = GetSize();
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.view = mData.mUIView;
        desc.metalLayer = mData.mMetalLayer;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_IOS
