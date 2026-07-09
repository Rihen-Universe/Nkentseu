// =============================================================================
// NkDialogs_iOS.mm
// Implémentation UIKit des dialogues natifs iOS :
//   - OpenMessageBox            -> UIAlertController
//   - OpenFileDialogAsync       -> UIDocumentPickerViewController (import)
//   - OpenFolderDialogAsync     -> UIDocumentPickerViewController (open folder)
//   - SaveFileDialogAsync       -> chemin Documents (sandbox) + extension
//
// iOS présente ses pickers de façon ASYNCHRONE : seules les variantes *Async
// sont réelles ; les versions synchrones restent des stubs (cf NkDialogs.cpp).
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_IOS)

#import <UIKit/UIKit.h>
#include "NKWindow/Core/NkDialogs.h"

// -----------------------------------------------------------------------------
// Récupère le view controller le plus en avant (pour présenter le dialogue).
// -----------------------------------------------------------------------------
static UIViewController *NkTopViewController() {
	UIWindow *key = nil;
	for (UIWindow *w in UIApplication.sharedApplication.windows) {
		if (w.isKeyWindow) {
			key = w;
			break;
		}
	}
	if (!key && UIApplication.sharedApplication.windows.count > 0)
		key = UIApplication.sharedApplication.windows.firstObject;
	UIViewController *vc = key.rootViewController;
	while (vc.presentedViewController)
		vc = vc.presentedViewController;
	return vc;
}

// -----------------------------------------------------------------------------
// Delegate auto-retenu du document picker : garde une référence forte sur
// lui-même jusqu'à ce que le callback soit invoqué (le picker est asynchrone).
// -----------------------------------------------------------------------------
@interface NkDocPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property(nonatomic, strong) NkDocPickerDelegate *selfRef;
@end

@implementation NkDocPickerDelegate {
	nkentseu::NkDialogs::Callback _cb; // ivar C++ (ARC gère ctor/dtor en ObjC++)
}

- (instancetype)initWithCallback:(const nkentseu::NkDialogs::Callback &)cb {
	if ((self = [super init])) {
		_cb = cb;
		self.selfRef = self;
	}
	return self;
}

- (void)finishWithPath:(NSString *)path confirmed:(bool)ok {
	nkentseu::NkDialogResult r;
	r.confirmed = ok;
	if (path)
		r.path = nkentseu::NkString([path UTF8String]);
	if (_cb)
		_cb(r);
	self.selfRef = nil; // relâche l'auto-rétention -> ARC libère
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
	NSURL *u = urls.firstObject;
	[self finishWithPath:(u ? u.path : nil) confirmed:(u != nil)];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller {
	[self finishWithPath:nil confirmed:false];
}

@end

// -----------------------------------------------------------------------------
namespace nkentseu {

	// Présente un UIDocumentPicker sur le thread principal, relie son delegate.
	static void NkPresentPicker(NSArray<NSString *> *types, UIDocumentPickerMode mode, const NkDialogs::Callback &cb) {
		NkDialogs::Callback cbCopy = cb;
		dispatch_async(dispatch_get_main_queue(), ^{
		  UIViewController *vc = NkTopViewController();
		  if (!vc) {
			  if (cbCopy) {
				  NkDialogResult r;
				  cbCopy(r);
			  }
			  return;
		  }
		  NkDocPickerDelegate *del = [[NkDocPickerDelegate alloc] initWithCallback:cbCopy];
		  UIDocumentPickerViewController *p = [[UIDocumentPickerViewController alloc] initWithDocumentTypes:types
																									 inMode:mode];
		  p.delegate = del;
		  [vc presentViewController:p animated:YES completion:nil];
		});
	}

	void NkDialogs::OpenMessageBox(const NkString &message, const NkString &title, int type) {
		(void)type;
		NSString *msg = [NSString stringWithUTF8String:(message.CStr() ? message.CStr() : "")];
		NSString *ttl = [NSString stringWithUTF8String:(title.CStr() ? title.CStr() : "")];
		dispatch_async(dispatch_get_main_queue(), ^{
		  UIViewController *vc = NkTopViewController();
		  if (!vc)
			  return;
		  UIAlertController *a = [UIAlertController alertControllerWithTitle:ttl
																	 message:msg
															  preferredStyle:UIAlertControllerStyleAlert];
		  [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
		  [vc presentViewController:a animated:YES completion:nil];
		});
	}

	void NkDialogs::OpenFileDialogAsync(const Callback &cb, const NkString & /*filter*/, const NkString & /*title*/) {
		NkPresentPicker(@[ @"public.data", @"public.content", @"public.item" ], UIDocumentPickerModeImport, cb);
	}

	void NkDialogs::OpenFolderDialogAsync(const Callback &cb, const NkString & /*title*/) {
		NkPresentPicker(@[ @"public.folder" ], UIDocumentPickerModeOpen, cb);
	}

	void NkDialogs::SaveFileDialogAsync(const Callback &cb, const NkString &defaultExt, const NkString & /*title*/) {
		// iOS n'offre pas de "Save As" générique sans fichier source. On renvoie un
		// chemin dans le répertoire Documents de l'app (sandbox) + extension suggérée.
		NkDialogResult r;
		NSArray<NSString *> *dirs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
		if (dirs.count > 0) {
			NSString *full = [dirs.firstObject stringByAppendingPathComponent:@"untitled"];
			if (defaultExt.CStr() && *defaultExt.CStr())
				full = [full stringByAppendingPathExtension:[NSString stringWithUTF8String:defaultExt.CStr()]];
			r.confirmed = true;
			r.path = NkString([full UTF8String]);
		}
		if (cb)
			cb(r);
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_IOS
