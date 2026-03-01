#pragma once
// =============================================================================
// NkCocoaDropDelegate.h — macOS Drag & Drop via NSView
//
// NkCocoaDropView : NSView category ajoutant le support drop
//   Méthodes override :
//     draggingEntered:  → NkDropEnterData
//     draggingUpdated:  → NkDropOverData
//     draggingExited:   → NkDropLeaveData
//     performDragOperation: → NkDropFileData / NkDropTextData
//
// Dans NkCocoaWindowImpl.mm : appeler NkRegisterCocoaDropView(nsView, callbacks)
// =============================================================================

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif

#include "../../Events/NkDropEvent.h"
#include <functional>
#include <string>
#include <vector>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Struct NkCocoaDropCallbacks.
 */
struct NkCocoaDropCallbacks {
	std::function<void(const NkDropFileData &)> onFiles;
	std::function<void(const NkDropTextData &)> onText;
	std::function<void(const NkDropEnterData &)> onEnter;
	std::function<void()> onLeave;
};

#ifdef __OBJC__

@interface NkDropView : NSView
@property(nonatomic, assign) nkentseu::NkCocoaDropCallbacks *callbacks;
@end

@implementation NkDropView

- (void)awakeFromNib {
	[self registerForDraggedTypes:@[ NSPasteboardTypeFileURL, NSPasteboardTypeString, (NSString *)kUTTypeURL ]];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
	NSPoint pt = [self convertPoint:[sender draggingLocation] fromView:nil];
	nkentseu::NkDropEnterData d;
	d.x = (int)pt.x;
	d.y = (int)pt.y;
	// Compter fichiers
	NSPasteboard *pb = [sender draggingPasteboard];
	NSArray *urls = [pb readObjectsForClasses:@[ [NSURL class] ]
									  options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
	d.numFiles = urls ? (uint32_t)urls.count : 0;
	d.hasText = [pb availableTypeFromArray:@[ NSPasteboardTypeString ]] != nil;
	d.dropType = d.numFiles > 0 ? nkentseu::NkDropType::NK_DROP_TYPE_FILE
				 : d.hasText	? nkentseu::NkDropType::NK_DROP_TYPE_TEXT
								: nkentseu::NkDropType::NK_DROP_TYPE_UNKNOWN;
	if (self.callbacks && self.callbacks->onEnter)
		self.callbacks->onEnter(d);
	return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
	if (self.callbacks && self.callbacks->onLeave)
		self.callbacks->onLeave();
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
	NSPoint pt = [self convertPoint:[sender draggingLocation] fromView:nil];
	NSPasteboard *pb = [sender draggingPasteboard];

	// Fichiers
	NSArray *urls = [pb readObjectsForClasses:@[ [NSURL class] ]
									  options:@{NSPasteboardURLReadingFileURLsOnlyKey : @YES}];
	if (urls.count > 0 && self.callbacks && self.callbacks->onFiles) {
		nkentseu::NkDropFileData d;
		d.x = (int)pt.x;
		d.y = (int)pt.y;
		d.numFiles = (uint32_t)urls.count;
		for (NSURL *url in urls) {
			nkentseu::NkDropFilePath fp;
			const char *p = [[url path] UTF8String];
			if (p)
				strncpy(fp.path, p, sizeof(fp.path) - 1);
			d.files.push_back(fp);
		}
		self.callbacks->onFiles(d);
	}

	// Texte
	NSString *str = [pb stringForType:NSPasteboardTypeString];
	if (str && self.callbacks && self.callbacks->onText) {
		nkentseu::NkDropTextData td;
		td.x = (int)pt.x;
		td.y = (int)pt.y;
		td.text = std::string([str UTF8String] ?: "");
		self.callbacks->onText(td);
	}

	return YES;
}

@end

#endif // __OBJC__

} // namespace nkentseu
