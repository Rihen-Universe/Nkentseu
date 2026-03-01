// =============================================================================
// NkWatchOS.h
// Point d'entrée watchOS (WatchKit).
// Inclure UNE SEULE FOIS via NkMain.h — ne pas inclure directement.
// =============================================================================

#pragma once

#import <WatchKit/WatchKit.h>
#include "../Core/NkEntry.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

// ---------------------------------------------------------------------------
// Arguments watchOS globaux
// ---------------------------------------------------------------------------

struct NkWatchOSArgs {
	std::string bundleId;
	std::string version;
	std::string build;
	std::string cachePath;
	std::vector<std::string> args;
};

static NkWatchOSArgs g_watchos_args;

// ---------------------------------------------------------------------------
// WKApplication delegate
// ---------------------------------------------------------------------------

@interface NkWatchAppDelegate : NSObject <WKApplicationDelegate>
@end

@implementation NkWatchAppDelegate

- (void)applicationDidBecomeActive {
	nkentseu::gState = new nkentseu::NkEntryState(g_watchos_args.args);
	nkentseu::gState->appName = g_watchos_args.bundleId;
	nkmain(*nkentseu::gState);
}

- (void)applicationWillResignActive {
	delete nkentseu::gState;
	nkentseu::gState = nullptr;
}

@end

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
	@autoreleasepool {
		NSBundle *bundle = [NSBundle mainBundle];
		g_watchos_args.bundleId = [[bundle bundleIdentifier] UTF8String] ?: "unknown";
		g_watchos_args.version =
			[[bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] description].UTF8String ?: "1.0";
		g_watchos_args.build = [[bundle objectForInfoDictionaryKey:@"CFBundleVersion"] description].UTF8String ?: "1";

		NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
		if ([paths count] > 0)
			g_watchos_args.cachePath = [[paths firstObject] UTF8String];

		for (int i = 0; i < argc; ++i)
			g_watchos_args.args.push_back(argv[i]);

		NSLog(@"[NK] Platform: watchOS");

		return WKApplicationMain(argc, argv, NSStringFromClass([NkWatchAppDelegate class]));
	}
}
