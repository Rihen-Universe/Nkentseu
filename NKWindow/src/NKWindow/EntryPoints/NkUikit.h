// =============================================================================
// NkUikit.h
// Point d'entrée iOS/tvOS (UIKit).
// Inclure UNE SEULE FOIS via NkMain.h — ne pas inclure directement.
// =============================================================================

#pragma once

#import <UIKit/UIKit.h>
#include "../Core/NkEntry.h"

// ---------------------------------------------------------------------------
// Arguments iOS globaux
// ---------------------------------------------------------------------------

struct NkAppleMobileArgs {
	std::string bundleId;
	std::string version;
	std::string build;
	std::string documentsPath;
	std::vector<std::string> args;
};

static NkAppleMobileArgs g_ios_args;

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

// ---------------------------------------------------------------------------
// Delegate UIKit
// ---------------------------------------------------------------------------

@interface NkUIAppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation NkUIAppDelegate

- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)opts {
	nkentseu::gState = new nkentseu::NkEntryState(g_ios_args.args);
	nkentseu::gState->appName = g_ios_args.bundleId;
	int ret = nkmain(*nkentseu::gState);
	(void)ret;
	return YES;
}

- (void)applicationWillTerminate:(UIApplication *)app {
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
		g_ios_args.bundleId = [[bundle bundleIdentifier] UTF8String] ?: "unknown";
		g_ios_args.version =
			[[bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"] description].UTF8String ?: "1.0";
		g_ios_args.build = [[bundle objectForInfoDictionaryKey:@"CFBundleVersion"] description].UTF8String ?: "1";

		NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
		if ([paths count] > 0)
			g_ios_args.documentsPath = [[paths firstObject] UTF8String];

		for (int i = 0; i < argc; ++i)
			g_ios_args.args.push_back(argv[i]);

#if TARGET_OS_TV
		NSLog(@"[NK] Platform: tvOS");
#elif TARGET_OS_VISION
		NSLog(@"[NK] Platform: visionOS");
#else
		NSLog(@"[NK] Platform: iOS");
#endif

		return UIApplicationMain(argc, argv, nil, NSStringFromClass([NkUIAppDelegate class]));
	}
}
