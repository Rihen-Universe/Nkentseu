// =============================================================================
// NkAppleMobileMain.mm — le point d'entrée iOS/tvOS/visionOS, côté ObjC++.
//
// Même chirurgie que NkCocoaMain.mm : NkAppleMobile.h définissait délégué
// UIKit + main() EN OBJECTIVE-C dans un header inclus par le main.cpp C++ des
// applications (NSString dans une TU C++ = mur d'erreurs — révélé par le
// premier probe iOS en CI, 2026-08-11). Tout l'ObjC vit ici, compilé par
// NKWindow ; le header ne fait que déléguer (NkAppleMobileRunApp).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_IOS)

#import <UIKit/UIKit.h>
#include "NKWindow/Core/NkEntry.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	NkEntryState *gState = nullptr;
	static NkString sMobileAppName;
	static NkVector<NkString> sMobileArgs;
} // namespace nkentseu

@interface NkAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation NkAppDelegate

- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)options {
	(void)app;
	(void)options;
	if (!nkentseu::NkEntryRuntimeInit(nkentseu::sMobileAppName.CStr())) {
		return NO;
	}
	nkentseu::NkEntryState state(nkentseu::sMobileArgs);
	nkentseu::NkApplyEntryAppName(state, nkentseu::sMobileAppName.CStr());
	nkentseu::gState = &state;
	nkmain(state);
	nkentseu::gState = nullptr;
	return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
	(void)application;
	nkentseu::NkEntryRuntimeShutdown(true);
}

@end

namespace nkentseu {

	int NkAppleMobileRunApp(int argc, char **argv, const char *appName) {
		@autoreleasepool {
			sMobileAppName = appName ? appName : "ios_app";
			// Infos bundle
			NSBundle *b = [NSBundle mainBundle];
			sMobileArgs.PushBack(NkString([[b bundleIdentifier] UTF8String] ?: ""));
			sMobileArgs.PushBack(
				NkString([[b objectForInfoDictionaryKey:@"CFBundleShortVersionString"] UTF8String] ?: ""));

			NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
			if ([paths count])
				sMobileArgs.PushBack(NkString([[paths firstObject] UTF8String]));

			return UIApplicationMain(argc, argv, nil, NSStringFromClass([NkAppDelegate class]));
		}
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_IOS
