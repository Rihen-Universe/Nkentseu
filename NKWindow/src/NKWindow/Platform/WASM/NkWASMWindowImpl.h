#pragma once

// =============================================================================
// NkWASMWindowImpl.h  —  Fenêtre WebAssembly / Emscripten
// Aucun pointeur vers EventImpl dans les données.
// =============================================================================

#include "../../Core/IWindowImpl.h"
#include <emscripten.h>
#include <emscripten/html5.h>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Class NkWASMWindowImpl.
 */
class NkWASMWindowImpl : public IWindowImpl {
public:
	bool Create(const NkWindowConfig &config) override;
	void Close() override;
	bool IsOpen() const override {
		return mIsOpen;
	}

	std::string GetTitle() const override {
		return mConfig.title;
	}
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

	void SetSize(NkU32 w, NkU32 h) override;
	void SetPosition(NkI32, NkI32) override {
	}
	void SetVisible(bool v) override;
	void Minimize() override {
	}
	void Maximize() override {
	}
	void Restore() override {
	}
	void SetFullscreen(bool fs) override;
	void SetMousePosition(NkU32, NkU32) override {
	}
	void ShowMouse(bool show) override;
	void CaptureMouse(bool cap) override;
	void SetProgress(float) override {
	}
	void SetWebInputOptions(const NkWebInputOptions &options) override;
	NkWebInputOptions GetWebInputOptions() const override {
		return mConfig.webInput;
	}
	NkSurfaceDesc GetSurfaceDesc() const override;
	bool SupportsOrientationControl() const override {
		return true;
	}
	void SetScreenOrientation(NkScreenOrientation orientation) override;
	NkScreenOrientation GetScreenOrientation() const override {
		return mConfig.screenOrientation;
	}
	NkSafeAreaInsets GetSafeAreaInsets() const override;

private:
	bool mIsOpen = false;
};
} // namespace nkentseu
