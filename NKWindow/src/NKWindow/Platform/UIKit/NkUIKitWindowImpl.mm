// =============================================================================
// NkUIKitWindowImpl.mm — UIKit (iOS/tvOS) V2
// =============================================================================

#import "NkUIKitWindowImpl.h"
#import "NkUIKitEventImpl.h"
#import "../../Core/NkSystem.h"

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

NkUIKitWindowImpl::~NkUIKitWindowImpl() {
	if (mIsOpen)
		Close();
}

bool NkUIKitWindowImpl::Create(const NkWindowConfig &config) {
	mConfig = config;
	@autoreleasepool {
		UIScreen *screen = [UIScreen mainScreen];
		CGRect frame = screen.bounds;

		mUIWindow = [[UIWindow alloc] initWithFrame:frame];
		mUIView = [[UIView alloc] initWithFrame:frame];
		mUIView.backgroundColor = [UIColor blackColor];
		mUIView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

		mMetalLayer = [CAMetalLayer layer];
		mMetalLayer.frame = mUIView.bounds;
		mMetalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		[mUIView.layer addSublayer:mMetalLayer];

		UIViewController *vc = [[UIViewController alloc] init];
		vc.view = mUIView;
		mUIWindow.rootViewController = vc;
		[mUIWindow makeKeyAndVisible];

		mIsOpen = true;

		IEventImpl *ev = NkGetEventImpl();
		if (ev)
			ev->Initialize(this, (__bridge void *)mUIView);
	}
	return mIsOpen;
}

void NkUIKitWindowImpl::Close() {
	@autoreleasepool {
		IEventImpl *ev = NkGetEventImpl();
		if (ev)
			ev->Shutdown((__bridge void *)mUIView);
		mUIWindow = nil;
		mUIView = nil;
		mMetalLayer = nil;
		mIsOpen = false;
	}
}

bool NkUIKitWindowImpl::IsOpen() const {
	return mIsOpen;
}
NkError NkUIKitWindowImpl::GetLastError() const {
	return mLastError;
}

std::string NkUIKitWindowImpl::GetTitle() const {
	return mConfig.title;
}
void NkUIKitWindowImpl::SetTitle(const std::string &t) {
	mConfig.title = t;
}

void NkUIKitWindowImpl::SetVisible(bool v) {
	@autoreleasepool {
		mUIWindow.hidden = !v;
	}
}

NkVec2u NkUIKitWindowImpl::GetSize() const {
	@autoreleasepool {
		if (!mUIView)
			return {};
		float scale = [UIScreen mainScreen].scale;
		return {static_cast<NkU32>(mUIView.bounds.size.width * scale),
				static_cast<NkU32>(mUIView.bounds.size.height * scale)};
	}
}

float NkUIKitWindowImpl::GetDpiScale() const {
	@autoreleasepool {
		return static_cast<float>([UIScreen mainScreen].scale);
	}
}

NkVec2u NkUIKitWindowImpl::GetDisplaySize() const {
	@autoreleasepool {
		CGSize sz = [UIScreen mainScreen].bounds.size;
		float sc = [UIScreen mainScreen].scale;
		return {static_cast<NkU32>(sz.width * sc), static_cast<NkU32>(sz.height * sc)};
	}
}

NkSurfaceDesc NkUIKitWindowImpl::GetSurfaceDesc() const {
	@autoreleasepool {
		NkSurfaceDesc sd;
		auto sz = GetSize();
		sd.width = sz.x;
		sd.height = sz.y;
		sd.dpi = GetDpiScale();
		sd.view = (__bridge void *)mUIView;
		sd.metalLayer = (__bridge void *)mMetalLayer;
		return sd;
	}
}

NkSafeAreaInsets NkUIKitWindowImpl::GetSafeAreaInsets() const {
	@autoreleasepool {
		if (!mUIView)
			return {};
		NkSafeAreaInsets ins;
		if (@available(iOS 11.0, tvOS 11.0, *)) {
			float scale = [UIScreen mainScreen].scale;
			UIEdgeInsets sa = mUIView.safeAreaInsets;
			ins.top = static_cast<float>(sa.top * scale);
			ins.bottom = static_cast<float>(sa.bottom * scale);
			ins.left = static_cast<float>(sa.left * scale);
			ins.right = static_cast<float>(sa.right * scale);
		}
		return ins;
	}
}

} // namespace nkentseu
