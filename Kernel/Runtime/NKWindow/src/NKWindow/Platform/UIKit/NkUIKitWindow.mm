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
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"

#include <cstdint>
#include <cstring>

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
        out.id = static_cast<nkentseu::uint64>(reinterpret_cast<uintptr_t>(touch));
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
            nkentseu::NkWESystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_MOVED: {
            nkentseu::NkTouchMoveEvent event(points, count);
            nkentseu::NkWESystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_ENDED: {
            nkentseu::NkTouchEndEvent event(points, count);
            nkentseu::NkWESystem::Events().Enqueue_Public(event, mOwner->GetId());
            break;
        }
        case nkentseu::NkTouchPhase::NK_TOUCH_PHASE_CANCELLED: {
            nkentseu::NkTouchCancelEvent event(points, count);
            nkentseu::NkWESystem::Events().Enqueue_Public(event, mOwner->GetId());
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
    using namespace math;

    static NkVec2u QueryViewSizePx(UIView* view) {
        if (!view) {
            return {0u, 0u};
        }
        const float scale = static_cast<float>([UIScreen mainScreen].scale);
        const uint32 w = static_cast<uint32>(math::NkMax(0.0f, static_cast<float>(view.bounds.size.width)) * scale);
        const uint32 h = static_cast<uint32>(math::NkMax(0.0f, static_cast<float>(view.bounds.size.height)) * scale);
        return {w, h};
    }

    static CAMetalLayer* EnsureUIKitMetalLayer(UIView* view, UIScreen* screen) {
        if (!view) {
            return nil;
        }
        CAMetalLayer* metalLayer = nil;
        if (view.layer) {
            for (CALayer* sublayer in view.layer.sublayers) {
                if ([sublayer isKindOfClass:[CAMetalLayer class]]) {
                    metalLayer = (CAMetalLayer*)sublayer;
                    break;
                }
            }
        }
        if (!metalLayer) {
            metalLayer = [CAMetalLayer layer];
            metalLayer.frame = view.bounds;
            [view.layer addSublayer:metalLayer];
        }
        metalLayer.contentsScale = screen ? screen.scale : [UIScreen mainScreen].scale;
        metalLayer.frame = view.bounds;
        return metalLayer;
    }

    static void ApplyUIKitTransparency(UIWindow* window, UIView* view, bool transparent) {
        if (transparent) {
            if (window) {
                window.backgroundColor = [UIColor clearColor];
                window.opaque = NO;
            }
            if (view) {
                view.backgroundColor = [UIColor clearColor];
                view.opaque = NO;
            }
        } else {
            if (view) {
                view.backgroundColor = [UIColor blackColor];
                view.opaque = YES;
            }
            if (window) {
                window.opaque = YES;
            }
        }
    }

    // =========================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =========================================================================

    static void SyncConfigFromWindow(const NkUIKitWindowData& data, NkWindowConfig& config) {
        config.width = data.mWidth;
        config.height = data.mHeight;
        config.visible = data.mVisible;
        config.fullscreen = data.mFullscreen;
        // Le titre n'est pas récupérable depuis UIKit facilement, on garde config.title
        // La position n'est pas pertinente, on garde config.x/config.y
    }

    static void SyncWindowFromConfig(NkUIKitWindowData& data, const NkWindowConfig& config) {
        data.mVisible = config.visible;
        data.mFullscreen = config.fullscreen;
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
        mData.mAppliedHints = config.surfaceHints;
        mData.mExternal = false;
        mData.mOwnsWindow = true;
        mData.mOwnsView = false;
        mData.mParentView = nil;

        @autoreleasepool {
            const bool wantsExternal = config.native.useExternalWindow;
            const bool hasExternalHandle = (config.native.externalWindowHandle != 0);
            if (wantsExternal && !hasExternalHandle) {
                mLastError = NkError(1, "UIKit: useExternalWindow=true but externalWindowHandle is null");
                return false;
            }

            UIScreen* screen = [UIScreen mainScreen];
            if (!wantsExternal && config.native.externalDisplayHandle != 0) {
                UIScreen* requestedScreen = reinterpret_cast<UIScreen*>(config.native.externalDisplayHandle);
                if (requestedScreen) {
                    screen = requestedScreen;
                }
            }

            UIWindow* window = nil;
            UIView* view = nil;
            CAMetalLayer* metalLayer = nil;

            if (wantsExternal && hasExternalHandle) {
                window = reinterpret_cast<UIWindow*>(config.native.externalWindowHandle);
                if (!window) {
                    mLastError = NkError(1, "UIKit: external UIWindow is null");
                    return false;
                }
                mData.mExternal = true;
                mData.mOwnsWindow = false;

                view = window.rootViewController ? window.rootViewController.view : nil;
                if (!view) {
                    NkUIKitTouchView* touchView = [[NkUIKitTouchView alloc] initWithFrame:window.bounds];
                    touchView->mOwner = this;
                    touchView.multipleTouchEnabled = YES;
                    touchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

                    UIViewController* controller = [[UIViewController alloc] init];
                    controller.view = touchView;
                    window.rootViewController = controller;
                    view = touchView;
                    mData.mOwnsView = true;
                } else if ([view isKindOfClass:[NkUIKitTouchView class]]) {
                    static_cast<NkUIKitTouchView*>(view)->mOwner = this;
                }
            } else if (config.native.parentWindowHandle != 0) {
                UIView* parent = reinterpret_cast<UIView*>(config.native.parentWindowHandle);
                if (!parent) {
                    mLastError = NkError(1, "UIKit: parentWindowHandle is invalid");
                    return false;
                }
                mData.mParentView = parent;
                mData.mOwnsWindow = false;

                NkUIKitTouchView* touchView = [[NkUIKitTouchView alloc] initWithFrame:parent.bounds];
                touchView->mOwner = this;
                touchView.multipleTouchEnabled = YES;
                touchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
                [parent addSubview:touchView];
                view = touchView;
                window = parent.window;
                mData.mOwnsView = true;
            } else {
                const CGRect frame = screen.bounds;
                window = [[UIWindow alloc] initWithFrame:frame];
                if (screen) {
                    window.screen = screen;
                }

                NkUIKitTouchView* touchView = [[NkUIKitTouchView alloc] initWithFrame:frame];
                touchView->mOwner = this;
                touchView.multipleTouchEnabled = YES;
                touchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

                UIViewController* controller = [[UIViewController alloc] init];
                controller.view = touchView;
                window.rootViewController = controller;
                view = touchView;
                mData.mOwnsView = true;
            }

            if (!view) {
                mLastError = NkError(1, "UIKit: failed to resolve render view");
                return false;
            }

            metalLayer = EnsureUIKitMetalLayer(view, screen);
            ApplyUIKitTransparency(window, view, config.transparent);

            mData.mUIWindow = window;
            mData.mUIView = view;
            mData.mMetalLayer = metalLayer;
            mData.mVisible = config.visible;
            mData.mFullscreen = config.fullscreen;

            const NkVec2u sizePx = QueryViewSizePx(view);
            mData.mWidth = sizePx.x;
            mData.mHeight = sizePx.y;

            // Synchroniser mConfig avec les dimensions réelles
            mConfig.width = mData.mWidth;
            mConfig.height = mData.mHeight;

            mId = NkWESystem::Instance().RegisterWindow(this);
            if (mId == NK_INVALID_WINDOW_ID) {
                mLastError = NkError(1, "UIKit: failed to register window");
                if (mData.mOwnsView && mData.mUIView) {
                    [mData.mUIView removeFromSuperview];
                }
                mData.mUIWindow = nil;
                mData.mUIView = nil;
                mData.mMetalLayer = nil;
                mData.mParentView = nil;
                mData.mOwnsView = false;
                return false;
            }

            if (mData.mOwnsWindow && window) {
                if (config.visible) {
                    [window makeKeyAndVisible];
                } else {
                    window.hidden = YES;
                }
            } else if (view) {
                view.hidden = !config.visible;
            }

            // Observers hot-plug écrans externes -> NkSystemDisplayEvent.
            {
                NkWindow* self = this;
                mData.mScreenConnectObserver = [[NSNotificationCenter defaultCenter]
                    addObserverForName:UIScreenDidConnectNotification
                                object:nil
                                 queue:nil
                            usingBlock:^(NSNotification* note) {
                    if (!self->mIsOpen) {
                        return;
                    }
                    UIScreen* connected = [note object];
                    NkDisplayInfo info = UIKitFillDisplayInfo(connected, 0);
                    NkSystemDisplayEvent evt(NkDisplayChange::NK_DISPLAY_ADDED, info, self->GetId());
                    NkWESystem::Events().Enqueue_Public(evt, self->GetId());
                }];

                mData.mScreenDisconnectObserver = [[NSNotificationCenter defaultCenter]
                    addObserverForName:UIScreenDidDisconnectNotification
                                object:nil
                                 queue:nil
                            usingBlock:^(NSNotification* note) {
                    if (!self->mIsOpen) {
                        return;
                    }
                    UIScreen* removed = [note object];
                    NkDisplayInfo info = UIKitFillDisplayInfo(removed, 0);
                    NkSystemDisplayEvent evt(NkDisplayChange::NK_DISPLAY_REMOVED, info, self->GetId());
                    NkWESystem::Events().Enqueue_Public(evt, self->GetId());
                }];
            }
        }

        mIsOpen = true;

        NkWindowCreateEvent createEvent(mData.mWidth, mData.mHeight);
        NkWESystem::Events().Enqueue_Public(createEvent, mId);

        if (mData.mVisible) {
            NkWindowShownEvent shownEvent;
            NkWESystem::Events().Enqueue_Public(shownEvent, mId);
        }

        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }

        const NkWindowId closingId = mId;

        NkWindowCloseEvent closeEvent(false);
        NkWESystem::Events().Enqueue_Public(closeEvent, closingId);

        @autoreleasepool {
            if (mData.mScreenConnectObserver) {
                [[NSNotificationCenter defaultCenter] removeObserver:mData.mScreenConnectObserver];
                mData.mScreenConnectObserver = nil;
            }
            if (mData.mScreenDisconnectObserver) {
                [[NSNotificationCenter defaultCenter] removeObserver:mData.mScreenDisconnectObserver];
                mData.mScreenDisconnectObserver = nil;
            }
            if (mData.mUIView) {
                if ([mData.mUIView isKindOfClass:[NkUIKitTouchView class]]) {
                    static_cast<NkUIKitTouchView*>(mData.mUIView)->mOwner = nullptr;
                }
            }
            if (mData.mOwnsWindow && mData.mUIWindow) {
                mData.mUIWindow.hidden = YES;
            }
            if (mData.mOwnsView && mData.mUIView) {
                [mData.mUIView removeFromSuperview];
            }
            mData.mUIWindow = nil;
            mData.mUIView = nil;
            mData.mMetalLayer = nil;
            mData.mParentView = nil;
        }

        NkWindowDestroyEvent destroyEvent;
        NkWESystem::Events().Enqueue_Public(destroyEvent, closingId);

        NkWESystem::Instance().UnregisterWindow(closingId);

        mId = NK_INVALID_WINDOW_ID;
        mIsOpen = false;
        mData.mWidth = 0;
        mData.mHeight = 0;
        mData.mVisible = false;
        mData.mFullscreen = false;
        mData.mExternal = false;
        mData.mOwnsWindow = true;
        mData.mOwnsView = false;
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
        // Synchroniser avant de retourner
        if (mIsOpen) {
            SyncConfigFromWindow(mData, const_cast<NkWindow*>(this)->mConfig);
        }
        return mConfig;
    }

    NkString NkWindow::GetTitle() const {
        return mConfig.title;
    }

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
        // Sur iOS, le titre n'est pas directement modifiable après création
    }

    NkVec2u NkWindow::GetSize() const {
        const NkVec2u sizePx = QueryViewSizePx(mData.mUIView);
        
        // Synchroniser mData et mConfig
        const_cast<NkWindow*>(this)->mData.mWidth = sizePx.x;
        const_cast<NkWindow*>(this)->mData.mHeight = sizePx.y;
        const_cast<NkWindow*>(this)->mConfig.width = sizePx.x;
        const_cast<NkWindow*>(this)->mConfig.height = sizePx.y;
        
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

    // =========================================================================
    // Moniteurs / Display (DPI runtime, écrans externes)
    // =========================================================================

    // Remplit un NkDisplayInfo depuis un UIScreen : géométrie (bounds en points),
    // facteur d'échelle (.scale), résolution physique (.nativeBounds en pixels),
    // fréquence de rafraîchissement (.maximumFramesPerSecond) et drapeau primaire.
    // dpiX/dpiY = scale * 96 pour cohérence cross-platform.
    static NkDisplayInfo UIKitFillDisplayInfo(UIScreen* screen, uint32 index) {
        NkDisplayInfo info;
        info.index = index;
        if (!screen) {
            return info;
        }

        const CGRect bounds = screen.bounds;
        const float32 scale = static_cast<float32>(screen.scale);

        info.posX   = 0;
        info.posY   = 0;
        info.width  = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(bounds.size.width)));
        info.height = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(bounds.size.height)));

        // Résolution physique réelle en pixels (nativeBounds, iOS 8.0+).
        if (@available(iOS 8.0, tvOS 9.0, *)) {
            const CGRect native = screen.nativeBounds;
            info.physWidth  = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(native.size.width)));
            info.physHeight = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(native.size.height)));
        } else {
            info.physWidth  = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(bounds.size.width)  * scale));
            info.physHeight = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(bounds.size.height) * scale));
        }

        // Facteur d'échelle DPI + densité alignée sur la baseline 96 dpi du framework.
        info.dpiScale = scale;
        info.dpiX     = scale * 96.0f;
        info.dpiY     = scale * 96.0f;

        // Moniteur principal = écran intégré ([UIScreen mainScreen]).
        info.isPrimary = (screen == [UIScreen mainScreen]);

        // Fréquence de rafraîchissement (iOS 10.3+, ProMotion 120 Hz...).
        if (@available(iOS 10.3, tvOS 10.3, *)) {
            const NSInteger fps = screen.maximumFramesPerSecond;
            if (fps > 0) {
                info.refreshRate = static_cast<uint32>(fps);
            }
        }

        // Nom lisible : écran intégré vs externe.
        const char* name = (screen == [UIScreen mainScreen]) ? "Built-in Screen" : "External Screen";
        ::strncpy(info.name, name, sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';
        return info;
    }

    NkVector<NkDisplayInfo> NkWindow::EnumerateMonitors() const {
        NkVector<NkDisplayInfo> out;
        NSArray<UIScreen*>* screens = [UIScreen screens];
        const uint32 count = static_cast<uint32>(screens.count);
        for (uint32 i = 0; i < count; ++i) {
            out.PushBack(UIKitFillDisplayInfo(screens[i], i));
        }
        // Garantir au moins une entrée (mainScreen).
        if (out.Size() == 0) {
            out.PushBack(UIKitFillDisplayInfo([UIScreen mainScreen], 0));
        }
        return out;
    }

    NkDisplayInfo NkWindow::GetCurrentMonitor() const {
        // Écran de la fenêtre courante, sinon écran principal.
        UIScreen* screen = (mData.mUIWindow && mData.mUIWindow.screen)
            ? mData.mUIWindow.screen
            : [UIScreen mainScreen];

        // Retrouver l'index réel de cet écran dans [UIScreen screens].
        NSArray<UIScreen*>* screens = [UIScreen screens];
        uint32 index = 0;
        for (uint32 i = 0; i < static_cast<uint32>(screens.count); ++i) {
            if (screens[i] == screen) {
                index = i;
                break;
            }
        }
        return UIKitFillDisplayInfo(screen, index);
    }

    uint32 NkWindow::GetMonitorCount() const {
        const NSUInteger count = [[UIScreen screens] count];
        return count > 0 ? static_cast<uint32>(count) : 1u;
    }

    void NkWindow::SetSize(uint32 width, uint32 height) {
        if (!mData.mUIView) {
            return;
        }

        const float scale = GetDpiScale();
        const CGFloat wPt = static_cast<CGFloat>(math::NkMax(width, 1u)) / scale;
        const CGFloat hPt = static_cast<CGFloat>(math::NkMax(height, 1u)) / scale;
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
        NkWESystem::Events().Enqueue_Public(resizeEvent, mId);
    }

    void NkWindow::SetPosition(int32, int32) {}

    void NkWindow::SetVisible(bool visible) {
        if ((!mData.mUIWindow && !mData.mUIView) || mData.mVisible == visible) {
            return;
        }
        mData.mVisible = visible;
        mConfig.visible = visible;
        
        if (mData.mOwnsWindow && mData.mUIWindow) {
            mData.mUIWindow.hidden = !visible;
        } else if (mData.mUIView) {
            mData.mUIView.hidden = !visible;
        }

        if (visible) {
            NkWindowShownEvent event;
            NkWESystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowHiddenEvent event;
            NkWESystem::Events().Enqueue_Public(event, mId);
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
            NkWESystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowWindowedEvent event;
            NkWESystem::Events().Enqueue_Public(event, mId);
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

    // iOS : pas de curseur natif (touch only), no-op.
    void NkWindow::ClipMouseToClient(bool) {}

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
        const NkVec2u size = GetSize();  // GetSize synchronise déjà mConfig
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.view = mData.mUIView;
        desc.metalLayer = mData.mMetalLayer;
        desc.appliedHints = mData.mAppliedHints;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_IOS