// =============================================================================
// NkMetalEntryPoint.mm
// Point d'entrée applicatif macOS / iOS pour le contexte Metal NkEngine.
//
// Ce fichier remplace main.cpp sur Apple — il crée l'infrastructure
// NSApplication (macOS) ou UIApplication (iOS), configure la fenêtre
// native, démarre la boucle d'événements et délègue à nkmain().
//
// Compiler avec -x objective-c++
// Lier : -framework AppKit (macOS) ou -framework UIKit (iOS)
//         -framework Metal -framework QuartzCore
//
// Intégration NkEngine :
//   Ce fichier DOIT être le seul avec main() sur Apple.
//   Supprimer ou exclure main.cpp de la target CMake macOS/iOS.
//   CMakeLists : set_source_files_properties(...mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
// =============================================================================

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)

#include "../Core/NkSurfaceDesc.h"
#include "../Core/NkContextDesc.h"
#include "../Factory/NkContextFactory.h"

// Forward-declare nkmain — défini dans votre code applicatif
namespace nkentseu { struct NkEntryState {}; }
int nkmain(nkentseu::NkEntryState& state);

// =============================================================================
// ─── macOS ───────────────────────────────────────────────────────────────────
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS)

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// ─────────────────────────────────────────────────────────────────────────────
// NkMetalView — NSView avec CAMetalLayer
// ─────────────────────────────────────────────────────────────────────────────
@interface NkMetalView : NSView
@property (nonatomic, strong) CAMetalLayer* metalLayer;
@end

@implementation NkMetalView

+ (Class)layerClass { return [CAMetalLayer class]; }

- (instancetype)initWithFrame:(NSRect)frame device:(id<MTLDevice>)device {
    self = [super initWithFrame:frame];
    if (self) {
        self.wantsLayer  = YES;
        _metalLayer      = (CAMetalLayer*)self.layer;
        _metalLayer.device            = device;
        _metalLayer.pixelFormat       = MTLPixelFormatBGRA8Unorm_sRGB;
        _metalLayer.framebufferOnly   = YES;
        _metalLayer.drawableSize      = frame.size;
        _metalLayer.displaySyncEnabled= YES;
    }
    return self;
}

- (CALayer*)makeBackingLayer { return [CAMetalLayer layer]; }

- (BOOL)acceptsFirstResponder { return YES; }

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    // Mettre à jour le contentsScale pour les écrans Retina
    CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
    _metalLayer.contentsScale = scale;
    CGSize sz = self.bounds.size;
    _metalLayer.drawableSize = CGSizeMake(sz.width * scale, sz.height * scale);
}

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
    _metalLayer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
}
@end

// ─────────────────────────────────────────────────────────────────────────────
// NkAppDelegate — démarre nkmain sur un thread séparé pour ne pas bloquer
// la boucle d'événements NSApplication
// ─────────────────────────────────────────────────────────────────────────────
@interface NkAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (nonatomic, strong) NSWindow*    window;
@property (nonatomic, strong) NkMetalView* metalView;
@property (nonatomic, assign) BOOL         shouldClose;
@end

@implementation NkAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notif {
    (void)notif;

    // Créer le device Metal
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        NSLog(@"[NkMetal] MTLCreateSystemDefaultDevice failed — Metal not supported?");
        [NSApp terminate:nil];
        return;
    }
    NSLog(@"[NkMetal] GPU: %@", device.name);

    // Fenêtre
    NSRect frame = NSMakeRect(100, 100, 1280, 720);
    NSUInteger style = NSWindowStyleMaskTitled
                     | NSWindowStyleMaskClosable
                     | NSWindowStyleMaskMiniaturizable
                     | NSWindowStyleMaskResizable;

    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:style
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title       = @"NkEngine — Metal";
    _window.delegate    = self;
    _window.acceptsMouseMovedEvents = YES;

    // Vue Metal
    _metalView = [[NkMetalView alloc] initWithFrame:frame device:device];
    [_window setContentView:_metalView];
    [_window makeKeyAndOrderFront:nil];
    [_window center];

    // Préparer NkSurfaceDesc
    nkentseu::NkSurfaceDesc surf;
    surf.nsView     = (void*)CFBridgingRetain(_metalView);
    surf.nsWindow   = (void*)CFBridgingRetain(_window);
    surf.metalLayer = (void*)CFBridgingRetain(_metalView.metalLayer);
    surf.width      = (nkentseu::uint32)(frame.size.width  * _window.backingScaleFactor);
    surf.height     = (nkentseu::uint32)(frame.size.height * _window.backingScaleFactor);
    surf.dpiScale   = (nkentseu::float32)_window.backingScaleFactor;

    // Lancer nkmain sur un thread séparé
    // (la boucle NSApplication tourne sur le thread principal)
    NSThread* gameThread = [[NSThread alloc] initWithBlock:^{
        nkentseu::NkEntryState state;
        int result = nkmain(state);
        NSLog(@"[NkMetal] nkmain returned %d", result);
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp terminate:nil];
        });
    }];
    gameThread.name = @"NkEngine-GameThread";
    gameThread.qualityOfService = NSQualityOfServiceUserInteractive;
    [gameThread start];

    // Libérer les références CFBridgingRetain (la vue les garde)
    // Note : surf est passé à nkmain via NkEntryState ou variable globale
    // — adaptez selon votre architecture NkWindow
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app {
    (void)app; return YES;
}

- (void)windowWillClose:(NSNotification*)notif {
    (void)notif;
    _shouldClose = YES;
    [NSApp terminate:nil];
}

- (void)windowDidResize:(NSNotification*)notif {
    (void)notif;
    // NkOpenGLContext / NkMetalContext reçoit OnResize via votre NkWindow
    // Déclencher l'event resize ici si nécessaire
    NSSize sz = _window.contentView.bounds.size;
    CGFloat scale = _window.backingScaleFactor;
    _metalView.metalLayer.drawableSize = CGSizeMake(sz.width*scale, sz.height*scale);
    NSLog(@"[NkMetal] Resize: %.0f×%.0f (×%.1f)", sz.width, sz.height, scale);
}

@end

// =============================================================================
int main(int argc, const char* argv[]) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Menu minimal (Quit)
        NSMenu* menu = [[NSMenu alloc] init];
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menu addItem:appMenuItem];
        [app setMainMenu:menu];
        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* quitTitle = [NSString stringWithFormat:@"Quit %@",
            [[NSProcessInfo processInfo] processName]];
        NSMenuItem* quitItem = [[NSMenuItem alloc]
            initWithTitle:quitTitle
                   action:@selector(terminate:)
            keyEquivalent:@"q"];
        [appMenu addItem:quitItem];
        [appMenuItem setSubmenu:appMenu];

        NkAppDelegate* delegate = [[NkAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}

// =============================================================================
// ─── iOS ─────────────────────────────────────────────────────────────────────
// =============================================================================
#elif defined(NKENTSEU_PLATFORM_IOS)

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// ─────────────────────────────────────────────────────────────────────────────
// NkMetalUIView — UIView avec layer Metal
// ─────────────────────────────────────────────────────────────────────────────
@interface NkMetalUIView : UIView
@property (nonatomic, readonly) CAMetalLayer* metalLayer;
@end

@implementation NkMetalUIView

+ (Class)layerClass { return [CAMetalLayer class]; }

- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device {
    self = [super initWithFrame:frame];
    if (self) {
        _metalLayer = (CAMetalLayer*)self.layer;
        _metalLayer.device          = device;
        _metalLayer.pixelFormat     = MTLPixelFormatBGRA8Unorm_sRGB;
        _metalLayer.framebufferOnly = YES;
        CGFloat scale = [UIScreen mainScreen].nativeScale;
        _metalLayer.contentsScale  = scale;
        _metalLayer.drawableSize   = CGSizeMake(frame.size.width * scale,
                                                 frame.size.height * scale);
    }
    return self;
}
@end

// ─────────────────────────────────────────────────────────────────────────────
@interface NkViewController : UIViewController
@property (nonatomic, strong) NkMetalUIView* metalView;
@end

@implementation NkViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    CGRect bounds = self.view.bounds;
    _metalView = [[NkMetalUIView alloc] initWithFrame:bounds device:device];
    _metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth
                                | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_metalView];

    // Lancer nkmain
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        nkentseu::NkEntryState state;
        nkmain(state);
    });
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    CGFloat scale = [UIScreen mainScreen].nativeScale;
    CGSize  sz    = self.view.bounds.size;
    _metalView.metalLayer.drawableSize = CGSizeMake(sz.width*scale, sz.height*scale);
}
@end

// ─────────────────────────────────────────────────────────────────────────────
@interface NkAppDelegate_iOS : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow* window;
@end

@implementation NkAppDelegate_iOS

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)application; (void)launchOptions;
    _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    NkViewController* vc = [[NkViewController alloc] init];
    _window.rootViewController = vc;
    [_window makeKeyAndVisible];
    return YES;
}
@end

// =============================================================================
int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil,
            NSStringFromClass([NkAppDelegate_iOS class]));
    }
}

#endif // IOS

#endif // APPLE
