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
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKMath/NkFunctions.h"

#include <algorithm>
#include <cstring>

// =============================================================================
// NkCocoaWindowDelegate — captures live-resize lifecycle events
// =============================================================================
@interface NkCocoaWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) nkentseu::NkWindow* nkWindow;
@end

@implementation NkCocoaWindowDelegate

- (void)windowWillStartLiveResize:(NSNotification*)notification {
    (void)notification;
    nkentseu::NkWindow* win = self.nkWindow;
    if (!win) return;
    nkentseu::NkWindowResizeBeginEvent e;
    nkentseu::NkWESystem::Events().Enqueue_Public(e, win->GetId());
}

- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    nkentseu::NkWindow* win = self.nkWindow;
    if (!win) return;
    NSWindow* nswin = win->mData.mNSWindow;
    if (!nswin) return;

    NSRect contentRect = [nswin contentRectForFrameRect:nswin.frame];
    const float scale = static_cast<float>(nswin.backingScaleFactor);
    uint32 newW = static_cast<uint32>(nkentseu::math::NkMax(0.0f, static_cast<float>(contentRect.size.width))  * scale);
    uint32 newH = static_cast<uint32>(nkentseu::math::NkMax(0.0f, static_cast<float>(contentRect.size.height)) * scale);

    // Mettre à jour mData
    win->mData.mWidth  = newW;
    win->mData.mHeight = newH;
    
    // Synchroniser mConfig
    win->mConfig.width = newW;
    win->mConfig.height = newH;

    nkentseu::NkWindowResizeEvent e(newW, newH, win->mData.mWidth, win->mData.mHeight);
    nkentseu::NkWESystem::Events().Enqueue_Public(e, win->GetId());
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
    (void)notification;
    nkentseu::NkWindow* win = self.nkWindow;
    if (!win) return;
    nkentseu::NkWindowResizeEndEvent e;
    nkentseu::NkWESystem::Events().Enqueue_Public(e, win->GetId());
}

- (void)windowWillMove:(NSNotification*)notification {
    (void)notification;
    nkentseu::NkWindow* win = self.nkWindow;
    if (!win) return;
    nkentseu::NkWindowMoveBeginEvent e;
    nkentseu::NkWESystem::Events().Enqueue_Public(e, win->GetId());
}

- (void)windowDidMove:(NSNotification*)notification {
    (void)notification;
    nkentseu::NkWindow* win = self.nkWindow;
    if (!win) return;
    NSWindow* nswin = win->mData.mNSWindow;
    if (!nswin) return;
    NSRect frame = nswin.frame;
    
    // Mettre à jour mConfig avec la nouvelle position
    win->mConfig.x = static_cast<int32>(frame.origin.x);
    win->mConfig.y = static_cast<int32>(frame.origin.y);
    
    nkentseu::NkWindowMoveEvent mv(
        static_cast<int32>(frame.origin.x),
        static_cast<int32>(frame.origin.y));
    nkentseu::NkWESystem::Events().Enqueue_Public(mv, win->GetId());
    nkentseu::NkWindowMoveEndEvent e;
    nkentseu::NkWESystem::Events().Enqueue_Public(e, win->GetId());
}

@end

namespace nkentseu {
    using namespace math;

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

    // =========================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =========================================================================

    static void SyncConfigFromWindow(const NkCocoaWindowData& data, NkWindowConfig& config) {
        config.width = data.mWidth;
        config.height = data.mHeight;
        config.visible = data.mVisible;
        config.fullscreen = data.mFullscreen;
        // La position est mise à jour dans windowDidMove
        // Le titre est mis à jour dans GetTitle/SetTitle
    }

    static void SyncWindowFromConfig(NkCocoaWindowData& data, const NkWindowConfig& config) {
        data.mVisible = config.visible;
        data.mFullscreen = config.fullscreen;
        // Les autres propriétés seront appliquées via les méthodes dédiées
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
                    
                    // Mettre à jour mConfig avec les coordonnées calculées
                    mConfig.x = static_cast<int32>(frame.origin.x);
                    mConfig.y = static_cast<int32>(frame.origin.y);
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
            
            // Synchroniser mConfig avec les dimensions réelles
            mConfig.width = mData.mWidth;
            mConfig.height = mData.mHeight;

            mId = NkWESystem::Instance().RegisterWindow(this);
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

            // Attach live-resize delegate
            {
                NkCocoaWindowDelegate* delegate = [[NkCocoaWindowDelegate alloc] init];
                delegate.nkWindow = this;
                [window setDelegate:delegate];
                mData.mDelegate = delegate;
            }

            // Observer hot-plug / changement de config écrans -> NkSystemDisplayEvent.
            {
                NkWindow* self = this;
                mData.mScreenObserver = [[NSNotificationCenter defaultCenter]
                    addObserverForName:NSApplicationDidChangeScreenParametersNotification
                                object:nil
                                 queue:nil
                            usingBlock:^(NSNotification* note) {
                    (void)note;
                    if (!self->mIsOpen) {
                        return;
                    }
                    // On ne peut pas distinguer trivialement add/remove/resolution
                    // ici : on rapporte un changement de résolution sur le moniteur
                    // courant (l'app peut ré-énumérer via EnumerateMonitors()).
                    NkDisplayInfo info = self->GetCurrentMonitor();
                    NkSystemDisplayEvent evt(NkDisplayChange::NK_DISPLAY_RESOLUTION_CHANGED, info, self->GetId());
                    NkWESystem::Events().Enqueue_Public(evt, self->GetId());
                }];
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
            if (mData.mScreenObserver) {
                [[NSNotificationCenter defaultCenter] removeObserver:mData.mScreenObserver];
                mData.mScreenObserver = nil;
            }
            if (mData.mNSWindow && mData.mDelegate) {
                [mData.mNSWindow setDelegate:nil];
                mData.mDelegate = nil;
            }
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
        // Synchroniser avant de retourner
        if (mIsOpen) {
            SyncConfigFromWindow(mData, const_cast<NkWindow*>(this)->mConfig);
        }
        return mConfig;
    }

    NkString NkWindow::GetTitle() const {
        if (!mData.mNSWindow) {
            return mConfig.title;
        }
        const char* utf8 = mData.mNSWindow.title.UTF8String;
        NkString title = utf8 ? NkString(utf8) : NkString();
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.title = title;
        
        return title;
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
        NkVec2u size = QueryContentSizePx(mData.mNSWindow);
        
        // Synchroniser mData et mConfig
        const_cast<NkWindow*>(this)->mData.mWidth = size.x;
        const_cast<NkWindow*>(this)->mData.mHeight = size.y;
        const_cast<NkWindow*>(this)->mConfig.width = size.x;
        const_cast<NkWindow*>(this)->mConfig.height = size.y;
        
        return size;
    }

    NkVec2u NkWindow::GetPosition() const {
        if (!mData.mNSWindow) {
            return {0u, 0u};
        }
        NSRect frame = mData.mNSWindow.frame;
        NkVec2u pos = {
            static_cast<uint32>(math::NkMax(0.0, frame.origin.x)),
            static_cast<uint32>(math::NkMax(0.0, frame.origin.y))
        };
        
        // Synchroniser mConfig
        const_cast<NkWindow*>(this)->mConfig.x = static_cast<int32>(frame.origin.x);
        const_cast<NkWindow*>(this)->mConfig.y = static_cast<int32>(frame.origin.y);
        
        return pos;
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

    // =========================================================================
    // Moniteurs / Display (DPI runtime, multi-écran)
    // =========================================================================

    // Remplit un NkDisplayInfo depuis un NSScreen : géométrie (frame en points),
    // facteur d'échelle (backingScaleFactor), résolution physique (frame * scale),
    // fréquence de rafraîchissement (CGDisplayMode), nom lisible et drapeau primaire.
    // dpiX/dpiY = scale * 96 pour cohérence cross-platform (baseline macOS = 72 pt
    // mais on aligne sur la convention 96 dpi du reste du framework).
    static NkDisplayInfo CocoaFillDisplayInfo(NSScreen* screen, uint32 index) {
        NkDisplayInfo info;
        info.index = index;
        if (!screen) {
            return info;
        }

        const NSRect frame = [screen frame];
        const float32 scale = static_cast<float32>([screen backingScaleFactor]);

        info.posX   = static_cast<int32>(frame.origin.x);
        info.posY   = static_cast<int32>(frame.origin.y);
        info.width  = static_cast<uint32>(math::NkMax(0.0, frame.size.width));
        info.height = static_cast<uint32>(math::NkMax(0.0, frame.size.height));

        // Résolution physique = taille logique en points * facteur d'échelle.
        info.physWidth  = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(frame.size.width)  * scale));
        info.physHeight = static_cast<uint32>(math::NkMax(0.0f, static_cast<float32>(frame.size.height) * scale));

        // Facteur d'échelle DPI + densité alignée sur la baseline 96 dpi du framework.
        info.dpiScale = scale;
        info.dpiX     = scale * 96.0f;
        info.dpiY     = scale * 96.0f;

        // Moniteur principal = celui qui contient le menu bar ([NSScreen mainScreen]).
        info.isPrimary = (screen == [NSScreen mainScreen]);

        // Fréquence de rafraîchissement via CGDisplayCopyDisplayMode.
        NSDictionary* deviceDesc = [screen deviceDescription];
        NSNumber* screenNumber = [deviceDesc objectForKey:@"NSScreenNumber"];
        if (screenNumber) {
            const CGDirectDisplayID displayId =
                static_cast<CGDirectDisplayID>([screenNumber unsignedIntValue]);
            CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayId);
            if (mode) {
                const double hz = CGDisplayModeGetRefreshRate(mode);
                if (hz > 0.0) {
                    info.refreshRate = static_cast<uint32>(hz + 0.5);
                }
                CGDisplayModeRelease(mode);
            }
        }

        // Nom lisible : localizedName (macOS 10.15+), sinon "Display N".
        const char* utf8Name = nullptr;
        if (@available(macOS 10.15, *)) {
            NSString* localized = [screen localizedName];
            if (localized) {
                utf8Name = localized.UTF8String;
            }
        }
        if (utf8Name && utf8Name[0] != '\0') {
            ::strncpy(info.name, utf8Name, sizeof(info.name) - 1);
            info.name[sizeof(info.name) - 1] = '\0';
        } else {
            NkString fallback = NkString::Fmt("Display {0}", index + 1);
            ::strncpy(info.name, fallback.c_str(), sizeof(info.name) - 1);
            info.name[sizeof(info.name) - 1] = '\0';
        }
        return info;
    }

    NkVector<NkDisplayInfo> NkWindow::EnumerateMonitors() const {
        NkVector<NkDisplayInfo> out;
        NSArray<NSScreen*>* screens = [NSScreen screens];
        const uint32 count = static_cast<uint32>(screens.count);
        for (uint32 i = 0; i < count; ++i) {
            out.PushBack(CocoaFillDisplayInfo(screens[i], i));
        }
        // Garantir au moins une entrée (mainScreen) si l'API renvoie une liste vide.
        if (out.Size() == 0) {
            out.PushBack(CocoaFillDisplayInfo([NSScreen mainScreen], 0));
        }
        return out;
    }

    NkDisplayInfo NkWindow::GetCurrentMonitor() const {
        // Écran qui contient (majoritairement) la fenêtre, sinon mainScreen.
        NSScreen* screen = (mData.mNSWindow && mData.mNSWindow.screen)
            ? mData.mNSWindow.screen
            : [NSScreen mainScreen];

        // Retrouver l'index réel de cet écran dans [NSScreen screens].
        NSArray<NSScreen*>* screens = [NSScreen screens];
        uint32 index = 0;
        for (uint32 i = 0; i < static_cast<uint32>(screens.count); ++i) {
            if (screens[i] == screen) {
                index = i;
                break;
            }
        }
        return CocoaFillDisplayInfo(screen, index);
    }

    uint32 NkWindow::GetMonitorCount() const {
        const NSUInteger count = [[NSScreen screens] count];
        return count > 0 ? static_cast<uint32>(count) : 1u;
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
        NkWESystem::Events().Enqueue_Public(resizeEvent, mId);
    }

    void NkWindow::SetPosition(int32 x, int32 y) {
        if (!mData.mNSWindow) {
            return;
        }
        mConfig.x = x;
        mConfig.y = y;
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
            NkWESystem::Events().Enqueue_Public(event, mId);
        } else {
            [mData.mNSWindow orderOut:nil];
            NkWindowHiddenEvent event;
            NkWESystem::Events().Enqueue_Public(event, mId);
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
            NkWESystem::Events().Enqueue_Public(event, mId);
        } else {
            NkWindowWindowedEvent event;
            NkWESystem::Events().Enqueue_Public(event, mId);
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

    // macOS n'expose pas de ClipCursor natif. On simule via le decouplage
    // souris/cursor + un re-snap a la position fenetre dans le main loop si
    // necessaire (TODO si l'user veut un confinement strict). En l'etat,
    // alias vers CaptureMouse pour fournir une API symetrique cross-platform.
    void NkWindow::ClipMouseToClient(bool clip) {
        CGAssociateMouseAndMouseCursorPosition(clip ? false : true);
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
        const NkVec2u size = GetSize();  // GetSize synchronise déjà mConfig
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