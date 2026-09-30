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
#include "NKWindow/Core/NkWESystem.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	NkEntryState *gState = nullptr;
	// Nom d'app fourni par NkCocoaRunApp (le NK_APP_NAME de l'application) —
	// le délégué en a besoin après [app run], hors de toute pile C++.
	static NkString sCocoaAppName;
	// Le code de retour de nkmain, rendu par main() — voir ArreterLaBoucle.
	static int sCocoaExitCode = 0;
} // namespace nkentseu

// ---------------------------------------------------------------------------
// Sortir de [NSApp run] SANS passer par exit()
// ---------------------------------------------------------------------------
// ⚠️ `[NSApp terminate:]` finit par appeler exit(0) : le code de retour de
//    nkmain était PERDU, et `UnkenyEditor --selftest` ou `Physic2D --selftest`
//    auraient répondu 0 sur macOS même avec un banc rouge — un verdict de CI
//    qui ne peut pas rougir. On demande donc à la boucle de s'arrêter
//    (`stop:`), et comme elle ne le remarque qu'au prochain événement, on lui
//    en poste un, vide (même patron que GLFW). [NSApp run] rend alors la main
//    à NkCocoaRunApp, qui rend le code à main().
static void ArreterLaBoucle(int code) {
	nkentseu::sCocoaExitCode = code;
	[NSApp stop:nil];
	NSEvent *reveil = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
										 location:NSZeroPoint
									modifierFlags:0
										timestamp:0
									 windowNumber:0
										  context:nil
										  subtype:0
											data1:0
											data2:0];
	[NSApp postEvent:reveil atStart:YES];
}

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
		ArreterLaBoucle(1);
		return;
	}
	nkentseu::NkVector<nkentseu::NkString> &args = *self.argsPtr;
	nkentseu::NkEntryState state(args);
	nkentseu::NkApplyEntryAppName(state, nkentseu::sCocoaAppName.CStr());
	nkentseu::gState = &state;
	const int code = nkmain(state);
	nkentseu::gState = nullptr;
	nkentseu::NkEntryRuntimeShutdown(true);
	ArreterLaBoucle(code);
}

// Cmd+Q, le menu « Quit », une déconnexion : tant que nkmain tourne, c'est une
// DEMANDE de fermeture adressée à chaque fenêtre (la même que la croix, voir
// windowShouldClose: dans NkCocoaWindow.mm), pas un exit() qui couperait
// l'application au milieu de sa boucle — sauvegardes et libérations comprises.
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	(void)sender;
	if (nkentseu::gState == nullptr) {
		return NSTerminateNow;
	}
	nkentseu::NkWESystem &systeme = nkentseu::NkWESystem::Instance();
	const nkentseu::uint32 nombre = systeme.GetWindowCount();
	for (nkentseu::uint32 i = 0; i < nombre; ++i) {
		nkentseu::NkWindow *fenetre = systeme.GetWindowAt(i);
		if (!fenetre) {
			continue;
		}
		nkentseu::NkWindowCloseEvent demande(false);
		nkentseu::NkWESystem::Events().Enqueue_Public(demande, fenetre->GetId());
	}
	return NSTerminateCancel;
}

// NON, et c'est voulu : la fin de l'application appartient à nkmain. Avec YES,
// AppKit lançait terminate: dès que la dernière fenêtre se fermait — y compris
// quand l'application la ferme elle-même, juste avant de libérer ses
// ressources et de rendre son code.
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
	(void)sender;
	return NO;
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
		return sCocoaExitCode;
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_MACOS
