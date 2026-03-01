#pragma once
// NkNoopWindowImpl.h — fenêtre stub (headless, tests)
#include "../../Core/IWindowImpl.h"
#include "../../Core/NkSystem.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Class NkNoopWindowImpl.
 */
class NkNoopWindowImpl : public IWindowImpl {
public:
	bool Create(const NkWindowConfig &cfg) override {
		mConfig = cfg;
		mOpen = true;
		auto *ev = NkGetEventImpl();
		if (ev)
			ev->Initialize(this, reinterpret_cast<void *>(this));
		return true;
	}
	void Close() override {
		auto *ev = NkGetEventImpl();
		if (ev)
			ev->Shutdown(reinterpret_cast<void *>(this));
		mOpen = false;
	}
	bool IsOpen() const override {
		return mOpen;
	}
	std::string GetTitle() const override {
		return mConfig.title;
	}
	NkVec2u GetSize() const override {
		return {mConfig.width, mConfig.height};
	}
	NkVec2u GetPosition() const override {
		return {};
	}
	float GetDpiScale() const override {
		return 1.f;
	}
	NkVec2u GetDisplaySize() const override {
		return {};
	}
	NkVec2u GetDisplayPosition() const override {
		return {};
	}
	NkError GetLastError() const override {
		return mLastError;
	}

	void SetTitle(const std::string &t) override {
		mConfig.title = t;
	}
	void SetSize(NkU32 w, NkU32 h) override {
		mConfig.width = w;
		mConfig.height = h;
	}
	void SetPosition(NkI32, NkI32) override {
	}
	void SetVisible(bool) override {
	}
	void Minimize() override {
	}
	void Maximize() override {
	}
	void Restore() override {
	}
	void SetFullscreen(bool fs) override {
		mConfig.fullscreen = fs;
	}
	void SetMousePosition(NkU32, NkU32) override {
	}
	void ShowMouse(bool) override {
	}
	void CaptureMouse(bool) override {
	}
	void SetProgress(float) override {
	}

	NkSurfaceDesc GetSurfaceDesc() const override {
		NkSurfaceDesc sd;
		sd.width = mConfig.width;
		sd.height = mConfig.height;
		sd.dpi = 1.f;
		sd.dummy = reinterpret_cast<void *>(const_cast<NkNoopWindowImpl *>(this));
		return sd;
	}

private:
	bool mOpen = false;
};
} // namespace nkentseu
