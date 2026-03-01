#pragma once

// =============================================================================
// NkUIKitWindowImpl.h — Fenêtre UIKit (iOS/tvOS) V2
// - GetSafeAreaInsets() via UIView.safeAreaInsets (iOS 11+)
// =============================================================================

#include "../../Core/IWindowImpl.h"
#include "../../Core/NkSafeArea.h"

#ifdef __OBJC__
@class UIWindow;
@class UIView;
@class CAMetalLayer;
#else
using UIWindow = struct objc_object;
using UIView = struct objc_object;
using CAMetalLayer = struct objc_object;
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkUIKitWindowImpl.
 */
class NkUIKitWindowImpl : public IWindowImpl {
public:
	NkUIKitWindowImpl() = default;
	~NkUIKitWindowImpl() override;

	bool Create(const NkWindowConfig &config) override;
	void Close() override;
	bool IsOpen() const override;

	std::string GetTitle() const override;
	void SetTitle(const std::string &t) override;
	NkVec2u GetSize() const override;
	NkVec2u GetPosition() const override {
		return {};
	}
	float GetDpiScale() const override;
	NkVec2u GetDisplaySize() const override;
	NkVec2u GetDisplayPosition() const override {
		return {};
	}
	NkError GetLastError() const override {
		return mLastError;
	}

	void SetSize(NkU32, NkU32) override {
	}
	void SetPosition(NkI32, NkI32) override {
	}
	void SetVisible(bool v) override;
	void Minimize() override {
	}
	void Maximize() override {
	}
	void Restore() override {
	}
	void SetFullscreen(bool) override {
	}
	void SetMousePosition(NkU32, NkU32) override {
	}
	void ShowMouse(bool) override {
	}
	void CaptureMouse(bool) override {
	}
	void SetProgress(float) override {
	}

	NkSurfaceDesc GetSurfaceDesc() const override;
	NkSafeAreaInsets GetSafeAreaInsets() const override;

private:
	UIWindow *mUIWindow = nullptr;
	UIView *mUIView = nullptr;
	CAMetalLayer *mMetalLayer = nullptr;
	bool mIsOpen = false;
};

} // namespace nkentseu
