#pragma once

// =============================================================================
// NkCocoa.h
// Point d'entrée macOS Cocoa.
// Crée NSApplication + NkAppDelegate qui appelle nkmain() après le lancement.
// =============================================================================

#import <Cocoa/Cocoa.h>
#include "../Core/NkEntry.h"
#include <vector>
#include <string>

#ifndef NK_APP_NAME
#define NK_APP_NAME "cocoa_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

// ---------------------------------------------------------------------------
// Delegate NSApplication
// ---------------------------------------------------------------------------

@interface NkAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, assign) std::vector<std::string> *argsPtr;
@end

@implementation NkAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notif {
	std::vector<std::string> &args = *self.argsPtr;
	nkentseu::NkEntryState state(args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;
	nkmain(state);
	nkentseu::gState = nullptr;
	[NSApp terminate:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	return YES;
}
@end

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, const char *argv[]) {
	@autoreleasepool {
		std::vector<std::string> args(argv, argv + argc);

		NSApplication *app = [NSApplication sharedApplication];
		[app setActivationPolicy:NSApplicationActivationPolicyRegular];

		// Menu minimal avec Quit
		NSMenu *menuBar = [[NSMenu alloc] init];
		NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
		[menuBar addItem:appMenuItem];
		NSMenu *appMenu = [[NSMenu alloc] init];
		[appMenuItem setSubmenu:appMenu];
		NSString *quitTitle = [@"Quit " stringByAppendingString:[NSString stringWithUTF8String:NK_APP_NAME]];
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
