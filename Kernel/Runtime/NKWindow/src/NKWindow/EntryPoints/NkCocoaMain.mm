// =============================================================================
// NkCocoaMain.mm — le point d'entrée macOS, côté Objective-C++.
//
// POURQUOI ce fichier : NkCocoa.h définissait NSApplication + délégué + main()
// EN OBJECTIVE-C DANS UN HEADER, inclus par le main.cpp (C++ pur) de chaque
// application → Foundation entrait dans une unité C++ et explosait (révélé par
// la première CI macOS, 2026-08-11). Même doctrine que NkMetalContext : tout
// l'ObjC vit dans un .mm compilé par NKWindow ; le header ne fait que
// déléguer (NkCocoaRunApp).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_MACOS)

#import <Cocoa/Cocoa.h>
#include "NKWindow/Core/NkEntry.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	NkEntryState *gState = nullptr;
	// Nom d'app fourni par NkCocoaRunApp (le NK_APP_NAME de l'application) —
	// le délégué en a besoin après [app run], hors de toute pile C++.
	static NkString sCocoaAppName;
} // namespace nkentseu

// ---------------------------------------------------------------------------
// Delegate NSApplication
// ---------------------------------------------------------------------------

@interface NkAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, assign) nkentseu::NkVector<nkentseu::NkString> *argsPtr;
@end

@implementation NkAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notif {
	(void)notif;
	if (!nkentseu::NkEntryRuntimeInit(nkentseu::sCocoaAppName.CStr())) {
		[NSApp terminate:nil];
		return;
	}
	nkentseu::NkVector<nkentseu::NkString> &args = *self.argsPtr;
	nkentseu::NkEntryState state(args);
	nkentseu::NkApplyEntryAppName(state, nkentseu::sCocoaAppName.CStr());
	nkentseu::gState = &state;
	nkmain(state);
	nkentseu::gState = nullptr;
	nkentseu::NkEntryRuntimeShutdown(true);
	[NSApp terminate:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	(void)sender;
	return YES;
}

@end

// ---------------------------------------------------------------------------
// NkCocoaRunApp — appelé par le main() C++ généré dans NkCocoa.h.
// ---------------------------------------------------------------------------

namespace nkentseu {

	int NkCocoaRunApp(int argc, const char **argv, const char *appName) {
		@autoreleasepool {
			sCocoaAppName = appName ? appName : "cocoa_app";
			NkVector<NkString> args;
			for (int i = 0; i < argc; ++i)
				args.PushBack(NkString(argv[i]));

			NSApplication *app = [NSApplication sharedApplication];
			[app setActivationPolicy:NSApplicationActivationPolicyRegular];

			// Menu minimal avec Quit
			NSMenu *menuBar = [[NSMenu alloc] init];
			NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
			[menuBar addItem:appMenuItem];
			NSMenu *appMenu = [[NSMenu alloc] init];
			[appMenuItem setSubmenu:appMenu];
			NSString *quitTitle =
				[@"Quit " stringByAppendingString:[NSString stringWithUTF8String:sCocoaAppName.CStr()]];
			NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:quitTitle
															  action:@selector(terminate:)
													   keyEquivalent:@"q"];
			[appMenu addItem:quitItem];
			[app setMainMenu:menuBar];

			NkAppDelegate *delegate = [[NkAppDelegate alloc] init];
			delegate.argsPtr = &args;
			[app setDelegate:delegate];

			[app run];
		}
		return 0;
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
