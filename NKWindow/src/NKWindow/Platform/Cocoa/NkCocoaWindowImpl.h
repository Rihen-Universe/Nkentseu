#pragma once

// =============================================================================
// NkCocoaWindowImpl.h  —  Fenetre Cocoa (macOS)
// =============================================================================

#include "../../Core/IWindowImpl.h"

#ifdef __OBJC__
@class NSWindow;
@class NSView;
@class CAMetalLayer;
@class NkNSWindow;
@class NkNSView;
#else
using NSWindow = struct objc_object;
using NSView = struct objc_object;
using CAMetalLayer = struct objc_object;
using NkNSWindow = struct objc_object;
using NkNSView = struct objc_object;
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkCocoaWindowImpl.
 */
class NkCocoaWindowImpl : public IWindowImpl {
public:
	NkCocoaWindowImpl();
	~NkCocoaWindowImpl() override;

	bool Create(const NkWindowConfig &config) override;
	void Close() override;
	bool IsOpen() const override;

	std::string GetTitle() const override;
	void SetTitle(const std::string &t) override;
	NkVec2u GetSize() const override;
	NkVec2u GetPosition() const override;
	float GetDpiScale() const override;
	NkVec2u GetDisplaySize() const override;
	NkVec2u GetDisplayPosition() const override;
	NkError GetLastError() const override;

	void SetSize(NkU32 w, NkU32 h) override;
	void SetPosition(NkI32 x, NkI32 y) override;
	void SetVisible(bool v) override;
	void Minimize() override;
	void Maximize() override;
	void Restore() override;
	void SetFullscreen(bool fs) override;
	void SetMousePosition(NkU32 x, NkU32 y) override;
	void ShowMouse(bool show) override;
	void CaptureMouse(bool cap) override;
	void SetProgress(float) override {
	}

	void SetBackgroundColor(NkU32 c);
	NkU32 GetBackgroundColor() const;

	NkSurfaceDesc GetSurfaceDesc() const override;

	NkNSWindow *GetNSWindow() const {
		return mWindow;
	}

private:
	void BlitSoftwareFramebuffer(const NkU8 *rgba8, NkU32 w, NkU32 h);

	NkNSWindow *mWindow = nullptr;
	NkNSView *mView = nullptr;
	CAMetalLayer *mMetalLayer = nullptr;

	bool mIsOpen = false;
	NkU32 mBgColor = 0x000000FF;
	IEventImpl *mEventImpl = nullptr; // reference faible, non possedee
};

} // namespace nkentseu
