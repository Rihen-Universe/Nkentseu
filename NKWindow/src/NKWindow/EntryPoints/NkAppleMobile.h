#pragma once

// =============================================================================
// NkAppleMobile.h
// Point d'entrée iOS / tvOS / visionOS (UIKit).
// =============================================================================

#import <UIKit/UIKit.h>
#include "../Core/NkEntry.h"
#include <vector>
#include <string>

#if TARGET_OS_TV
#define NK_APP_NAME "tvos_app"
#elif TARGET_OS_VISION
#define NK_APP_NAME "visionos_app"
#else
#define NK_APP_NAME "ios_app"
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
NkEntryState *gState = nullptr;
}

// Stockage global des args pour la durée de vie de l'app
static std::vector<std::string> g_apple_mobile_args;

@interface NkAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation NkAppDelegate
- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)options {
	nkentseu::NkEntryState state(g_apple_mobile_args);
	state.appName = NK_APP_NAME;
	nkentseu::gState = &state;
	nkmain(state);
	nkentseu::gState = nullptr;
	return YES;
}
@end

int main(int argc, char *argv[]) {
	@autoreleasepool {
		// Infos bundle
		NSBundle *b = [NSBundle mainBundle];
		g_apple_mobile_args.push_back([[b bundleIdentifier] UTF8String] ?: "");
		g_apple_mobile_args.push_back([[b objectForInfoDictionaryKey:@"CFBundleShortVersionString"] UTF8String] ?: "");

		NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
		if ([paths count])
			g_apple_mobile_args.push_back([[paths firstObject] UTF8String]);

		return UIApplicationMain(argc, argv, nil, NSStringFromClass([NkAppDelegate class]));
	}
}
