// =============================================================================
// NkCocoaWindow.mm
// Cocoa implementation of NkWindow without PIMPL.
// =============================================================================

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <CoreGraphics/CoreGraphics.h>

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#include "NKWindow/Platform/Cocoa/NkCocoaWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKMath/NkFunctions.h"

#include <algorithm>

namespace nkentseu {

    static NkVec2u QueryContentSizePx(NSWindow* window) {
        if (!window) {
            return {0u, 0u};
        }
        NSRect contentRect = [window contentRectForFrameRect:window.frame];
        const float scale = static_cast<float>(window.backingScaleFactor);
        return {
            static_cast<uint32>(math::NkMax(0.0f, static_cast<float>(contentRect.size.width)) * scale),
            static_cast<uint32>(math::NkMax(0.0f, static_cast<float>(contentRect.size.height)) * scale)
        };
    }

    static void ApplyCocoaWindowTransparency(NSWindow* window, bool transparent, bool hasShadow) {
        if (!window) {
            return;
        }
        if (transparent) {
            [window setOpaque:NO];
            [window setBackgroundColor:[NSColor clearColor]];
            [window setHasShadow:hasShadow ? YES : NO];
        } else {
            [window setOpaque:YES];
            [window setHasShadow:hasShadow ? YES : NO];
        }
    }

    static void ApplyCocoaWindowIcon(NSWindow* window, const NkString& iconPath) {
        if (!window || iconPath.Empty()) {
            return;
        }
        NSString* path = [NSString stringWithUTF8String:iconPath.c_str()];
        if (!path || path.length == 0) {
            return;
        }
        NSImage* icon = [[NSImage alloc] initWithContentsOfFile:path];
        if (!icon) {
            return;
        }
        [window setMiniwindowImage:icon];
        [NSApp setApplicationIconImage:icon];
    }

    static CAMetalLayer* EnsureCocoaMetalLayer(NSView* view) {
        if (!view) {
            return nil;
        }
        view.wantsLayer = YES;
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

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
            metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
            [view.layer addSublayer:metalLayer];
        }
        return metalLayer;
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
        mData.mParentWindow = nil;

        @autoreleasepool {
            const bool wantsExternal = config.native.useExternalWindow;
            const bool hasExternalHandle = (config.native.externalWindowHandle != 0);
            if (wantsExternal && !hasExternalHandle) {
                mLastError = NkError(1, "Cocoa: useExternalWindow=true but externalWindowHandle is null");
                return false;
            }

            NSScreen* targetScreen = [NSScreen mainScreen];
            if (config.native.externalDisplayHandle != 0) {
                targetScreen = reinterpret_cast<NSScreen*>(config.native.externalDisplayHandle);
                if (!targetScreen) {
                    targetScreen = [NSScreen mainScreen];
                }
            }

            NSWindow* window = nil;
            NSView* view = nil;
            CAMetalLayer* metalLayer = nil;

            if (wantsExternal && hasExternalHandle) {
                window = reinterpret_cast<NSWindow*>(config.native.externalWindowHandle);
                if (!window) {
                    mLastError = NkError(1, "Cocoa: external NSWindow is null");
                    return false;
                }
                mData.mExternal = true;
                mData.mOwnsWindow = false;

                view = [window contentView];
                if (!view) {
                    NSRect contentRect = [window contentRectForFrameRect:window.frame];
                    view = [[NSView alloc] initWithFrame:contentRect];
                    [window setContentView:view];
                }

                metalLayer = EnsureCocoaMetalLayer(view);
                [window setAcceptsMouseMovedEvents:YES];

                if (!config.title.Empty()) {
                    [window setTitle:[NSString stringWithUTF8String:config.title.c_str()]];
                }
            } else {
            NSWindowStyleMask style = NSWindowStyleMaskBorderless;
            if (config.frame) {
                style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;
                if (config.resizable) {
                    style |= NSWindowStyleMaskResizable;
                }
                if (config.minimizable) {
                    style |= NSWindowStyleMaskMiniaturizable;
                }
            }
#ifdef NSWindowStyleMaskUtilityWindow
                if (config.native.utilityWindow) {
                    style |= NSWindowStyleMaskUtilityWindow;
                }
#endif

            NSRect frame = NSMakeRect(config.x, config.y, config.width, config.height);
            if (config.centered) {
                    NSRect screen = targetScreen ? [targetScreen frame] : [[NSScreen mainScreen] frame];
                frame.origin.x = (screen.size.width - config.width) * 0.5;
                frame.origin.y = (screen.size.height - config.height) * 0.5;
            }

                window = [[NSWindow alloc] initWithContentRect:frame
                                                            styleMask:style
                                                              backing:NSBackingStoreBuffered
                                                                defer:NO];
            if (!window) {
                mLastError = NkError(1, "Cocoa: failed to create NSWindow");
                return false;
            }

                view = [[NSView alloc] initWithFrame:frame];
                metalLayer = EnsureCocoaMetalLayer(view);

            [window setContentView:view];
            [window setReleasedWhenClosed:NO];
            [window setTitle:[NSString stringWithUTF8String:config.title.c_str()]];
            [window setAcceptsMouseMovedEvents:YES];
            }

#ifdef NSWindowStyleMaskUtilityWindow
            if (config.native.utilityWindow && window) {
                const NSWindowStyleMask currentMask = [window styleMask];
                [window setStyleMask:(currentMask | NSWindowStyleMaskUtilityWindow)];
            }
#endif

            ApplyCocoaWindowTransparency(window, config.transparent, config.hasShadow);
            ApplyCocoaWindowIcon(window, config.iconPath);

            if (config.native.parentWindowHandle != 0 && window) {
                NSWindow* parent = reinterpret_cast<NSWindow*>(config.native.parentWindowHandle);
                if (parent && parent != window) {
                    [parent addChildWindow:window ordered:NSWindowAbove];
                    mData.mParentWindow = parent;
                }
            }

            if (config.visible) {
                [window makeKeyAndOrderFront:nil];
                [NSApp activateIgnoringOtherApps:YES];
            } else {
                [window orderOut:nil];
            }

            mData.mNSWindow = window;
            mData.mNSView = view;
            mData.mMetalLayer = metalLayer;
            mData.mVisible = config.visible;
            mData.mFullscreen = config.fullscreen;

            const NkVec2u sizePx = QueryContentSizePx(window);
            mData.mWidth = sizePx.x;
            mData.mHeight = sizePx.y;

            mId = NkSystem::Instance().RegisterWindow(this);
            if (mId == NK_INVALID_WINDOW_ID) {
                mLastError = NkError(1, "Cocoa: failed to register window");
                if (mData.mParentWindow && window) {
                    [mData.mParentWindow removeChildWindow:window];
                    mData.mParentWindow = nil;
                }
                if (!mData.mExternal && window) {
                    [window orderOut:nil];
                    [window close];
                }
                mData.mNSWindow = nil;
                mData.mNSView = nil;
                mData.mMetalLayer = nil;
                return false;
            }

            if (config.fullscreen) {
                const BOOL isFullscreen = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
                if (!isFullscreen) {
                    [window toggleFullScreen:nil];
                }
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
            if (mData.mParentWindow && mData.mNSWindow) {
                [mData.mParentWindow removeChildWindow:mData.mNSWindow];
            }
            if (mData.mNSWindow) {
                if (mData.mOwnsWindow) {
                    [mData.mNSWindow orderOut:nil];
                    [mData.mNSWindow close];
                }
            }
            mData.mNSWindow = nil;
            mData.mNSView = nil;
            mData.mMetalLayer = nil;
            mData.mParentWindow = nil;
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
        mData.mExternal = false;
        mData.mOwnsWindow = true;
    }

    bool NkWindow::IsOpen() const {
        return mIsOpen;
    }

    bool NkWindow::IsValid() const {
        return mIsOpen && mData.mNSWindow != nil;
    }

    NkError NkWindow::GetLastError() const {
        return mLastError;
    }

    NkWindowConfig NkWindow::GetConfig() const {
        return mConfig;
    }

    NkString NkWindow::GetTitle() const {
        if (!mData.mNSWindow) {
            return mConfig.title;
        }
        const char* utf8 = mData.mNSWindow.title.UTF8String;
        return utf8 ? NkString(utf8) : NkString();
    }

    void NkWindow::SetTitle(const NkString& title) {
        mConfig.title = title;
        if (mData.mNSWindow) {
            [mData.mNSWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
        }
    }

    NkVec2u NkWindow::GetSize() const {
        if (!mData.mNSWindow) {
            return {0u, 0u};
        }
        return QueryContentSizePx(mData.mNSWindow);
    }

    NkVec2u NkWindow::GetPosition() const {
        if (!mData.mNSWindow) {
            return {0u, 0u};
        }
        NSRect frame = mData.mNSWindow.frame;
        return {
            static_cast<uint32>(math::NkMax(0.0, frame.origin.x)),
            static_cast<uint32>(math::NkMax(0.0, frame.origin.y))
        };
    }

    float NkWindow::GetDpiScale() const {
        if (!mData.mNSWindow) {
            return 1.0f;
        }
        return static_cast<float>(mData.mNSWindow.backingScaleFactor);
    }

    NkVec2u NkWindow::GetDisplaySize() const {
        NSScreen* screen = [NSScreen mainScreen];
        if (!screen) {
            return {0u, 0u};
        }
        const CGFloat scale = [screen backingScaleFactor];
        NSRect frame = [screen frame];
        return {
            static_cast<uint32>(math::NkMax(0.0, frame.size.width * scale)),
            static_cast<uint32>(math::NkMax(0.0, frame.size.height * scale))
        };
    }

    NkVec2u NkWindow::GetDisplayPosition() const {
        return {0u, 0u};
    }

    void NkWindow::SetSize(uint32 width, uint32 height) {
        if (!mData.mNSWindow) {
            return;
        }

        const uint32 oldW = mData.mWidth;
        const uint32 oldH = mData.mHeight;

        NSRect frame = [mData.mNSWindow frame];
        frame.size.width = static_cast<CGFloat>(width);
        frame.size.height = static_cast<CGFloat>(height);
        [mData.mNSWindow setFrame:frame display:YES];

        const NkVec2u sizePx = QueryContentSizePx(mData.mNSWindow);
        mData.mWidth = sizePx.x;
        mData.mHeight = sizePx.y;
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;

        NkWindowResizeEvent resizeEvent(mData.mWidth, mData.mHeight, oldW, oldH);
        NkSystem::Events().Enqueue_Public(resizeEvent, mId);
    }

    void NkWindow::SetPosition(int32 x, int32 y) {
        if (!mData.mNSWindow) {
            return;
        }
        [mData.mNSWindow setFrameOrigin:NSMakePoint(x, y)];
    }

    void NkWindow::SetVisible(bool visible) {
        if (!mData.mNSWindow || mData.mVisible == visible) {
            return;
        }
        mData.mVisible = visible;
        mConfig.visible = visible;

        if (visible) {
            [mData.mNSWindow makeKeyAndOrderFront:nil];
            NkWindowShownEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        } else {
            [mData.mNSWindow orderOut:nil];
            NkWindowHiddenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        }
    }

    void NkWindow::Minimize() {
        if (mData.mNSWindow) {
            [mData.mNSWindow miniaturize:nil];
        }
    }

    void NkWindow::Maximize() {
        if (mData.mNSWindow) {
            [mData.mNSWindow zoom:nil];
        }
    }

    void NkWindow::Restore() {
        if (mData.mNSWindow) {
            [mData.mNSWindow deminiaturize:nil];
        }
    }

    void NkWindow::SetFullscreen(bool fullscreen) {
        if (!mData.mNSWindow || mData.mFullscreen == fullscreen) {
            return;
        }
        mData.mFullscreen = fullscreen;
        mConfig.fullscreen = fullscreen;

        const BOOL isFullscreen = (mData.mNSWindow.styleMask & NSWindowStyleMaskFullScreen) != 0;
        if ((fullscreen && !isFullscreen) || (!fullscreen && isFullscreen)) {
            [mData.mNSWindow toggleFullScreen:nil];
        }

        if (fullscreen) {
            NkWindowFullscreenEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowWindowedEvent event;
            NkSystem::Events().Enqueue_Public(event, mId);
        }
    }

    bool NkWindow::SupportsOrientationControl() const {
        return false;
    }

    void NkWindow::SetScreenOrientation(NkScreenOrientation) {}

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    }

    void NkWindow::SetAutoRotateEnabled(bool) {}

    bool NkWindow::IsAutoRotateEnabled() const {
        return false;
    }

    void NkWindow::SetMousePosition(uint32 x, uint32 y) {
        CGWarpMouseCursorPosition(CGPointMake(x, y));
    }

    void NkWindow::ShowMouse(bool show) {
        if (show) {
            [NSCursor unhide];
        } else {
            [NSCursor hide];
        }
    }

    void NkWindow::CaptureMouse(bool capture) {
        CGAssociateMouseAndMouseCursorPosition(capture ? false : true);
    }

    void NkWindow::SetWebInputOptions(const NkWebInputOptions&) {}

    NkWebInputOptions NkWindow::GetWebInputOptions() const {
        return {};
    }

    void NkWindow::SetProgress(float) {}

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
        return {};
    }

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        const NkVec2u size = GetSize();
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.view = mData.mNSView;
        desc.metalLayer = mData.mMetalLayer;
        desc.appliedHints = mData.mAppliedHints;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
