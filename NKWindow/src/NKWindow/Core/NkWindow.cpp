// =============================================================================
// NkWindow.cpp
// Implémentation de Window — sélectionne la bonne impl de plateforme.
// =============================================================================

#include "NkWindow.h"
#include "NkSystem.h"
#include "NKWindow/Events/IEventImpl.h"
#include "NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WIN32)
#include "../Platform/Win32/NkWin32WindowImpl.h"
using PlatformWindowImpl = nkentseu::NkWin32WindowImpl;
#elif defined(NKENTSEU_PLATFORM_UWP) || defined(NKENTSEU_PLATFORM_XBOX)
#include "../Platform/UWP/NkUWPWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkUWPWindowImpl;
#elif defined(NKENTSEU_PLATFORM_COCOA)
#include "../Platform/Cocoa/NkCocoaWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkCocoaWindowImpl;
#elif defined(NKENTSEU_PLATFORM_UIKIT)
#include "../Platform/UIKit/NkUIKitWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkUIKitWindowImpl;
#elif defined(NKENTSEU_PLATFORM_XCB)
#include "../Platform/XCB/NkXCBWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkXCBWindowImpl;
#elif defined(NKENTSEU_PLATFORM_XLIB)
#include "../Platform/XLib/NkXLibWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkXLibWindowImpl;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include "../Platform/Android/NkAndroidWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkAndroidWindowImpl;
#elif defined(NKENTSEU_PLATFORM_WASM)
#include "../Platform/WASM/NkWASMWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkWASMWindowImpl;
#else
#include "../Platform/Noop/NkNoopWindowImpl.h"
using PlatformWindowImpl = nkentseu::NkNoopWindowImpl;
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

NkWindow::NkWindow() : mImpl(std::make_unique<PlatformWindowImpl>()) {
}

NkWindow::NkWindow(const NkWindowConfig &config) : mImpl(std::make_unique<PlatformWindowImpl>()), mConfig(config) {
	Create(config);
}

NkWindow::~NkWindow() {
	if (mImpl && mImpl->IsOpen())
		Close();
}

// ---------------------------------------------------------------------------
// Cycle de vie
// ---------------------------------------------------------------------------

bool NkWindow::Create(const NkWindowConfig &config) {
	mConfig = config;
	// L'impl crée la fenêtre et appelle NkGetEventImpl()->Initialize()
	return mImpl->Create(config);
}

void NkWindow::Close() {
	if (mImpl)
		mImpl->Close();
}

bool NkWindow::IsOpen() const {
	return mImpl && mImpl->IsOpen();
}
bool NkWindow::IsValid() const {
	return IsOpen();
}

// ---------------------------------------------------------------------------
// Propriétés
// ---------------------------------------------------------------------------

std::string NkWindow::GetTitle() const {
	return mImpl ? mImpl->GetTitle() : "";
}
void NkWindow::SetTitle(const std::string &t) {
	if (mImpl)
		mImpl->SetTitle(t);
}
NkVec2u NkWindow::GetSize() const {
	return mImpl ? mImpl->GetSize() : NkVec2u{};
}
NkVec2u NkWindow::GetPosition() const {
	return mImpl ? mImpl->GetPosition() : NkVec2u{};
}
float NkWindow::GetDpiScale() const {
	return mImpl ? mImpl->GetDpiScale() : 1.f;
}
NkVec2u NkWindow::GetDisplaySize() const {
	return mImpl ? mImpl->GetDisplaySize() : NkVec2u{};
}
NkVec2u NkWindow::GetDisplayPosition() const {
	return mImpl ? mImpl->GetDisplayPosition() : NkVec2u{};
}
NkError NkWindow::GetLastError() const {
	return mImpl ? mImpl->GetLastError() : NkError::Ok();
}
NkWindowConfig NkWindow::GetConfig() const {
	return mConfig;
}

// ---------------------------------------------------------------------------
// Manipulation
// ---------------------------------------------------------------------------

void NkWindow::SetSize(NkU32 w, NkU32 h) {
	if (mImpl)
		mImpl->SetSize(w, h);
}
void NkWindow::SetPosition(NkI32 x, NkI32 y) {
	if (mImpl)
		mImpl->SetPosition(x, y);
}
void NkWindow::SetVisible(bool v) {
	if (mImpl)
		mImpl->SetVisible(v);
}
void NkWindow::Minimize() {
	if (mImpl)
		mImpl->Minimize();
}
void NkWindow::Maximize() {
	if (mImpl)
		mImpl->Maximize();
}
void NkWindow::Restore() {
	if (mImpl)
		mImpl->Restore();
}
void NkWindow::SetFullscreen(bool fs) {
	if (mImpl)
		mImpl->SetFullscreen(fs);
}
bool NkWindow::SupportsOrientationControl() const {
	return mImpl ? mImpl->SupportsOrientationControl() : false;
}
void NkWindow::SetScreenOrientation(NkScreenOrientation orientation) {
	mConfig.screenOrientation = orientation;
	if (mImpl)
		mImpl->SetScreenOrientation(orientation);
}
NkScreenOrientation NkWindow::GetScreenOrientation() const {
	return mImpl ? mImpl->GetScreenOrientation() : mConfig.screenOrientation;
}
void NkWindow::SetAutoRotateEnabled(bool enabled) {
	if (mImpl)
		mImpl->SetAutoRotateEnabled(enabled);
	mConfig.screenOrientation = GetScreenOrientation();
}
bool NkWindow::IsAutoRotateEnabled() const {
	return mImpl ? mImpl->IsAutoRotateEnabled()
				 : (mConfig.screenOrientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO);
}

// ---------------------------------------------------------------------------
// Souris
// ---------------------------------------------------------------------------

void NkWindow::SetMousePosition(NkU32 x, NkU32 y) {
	if (mImpl)
		mImpl->SetMousePosition(x, y);
}
void NkWindow::ShowMouse(bool show) {
	if (mImpl)
		mImpl->ShowMouse(show);
}
void NkWindow::CaptureMouse(bool cap) {
	if (mImpl)
		mImpl->CaptureMouse(cap);
}

void NkWindow::SetWebInputOptions(const NkWebInputOptions &options) {
	mConfig.webInput = options;
	if (mImpl)
		mImpl->SetWebInputOptions(options);
}

NkWebInputOptions NkWindow::GetWebInputOptions() const {
	return mConfig.webInput;
}

// ---------------------------------------------------------------------------
// OS extras
// ---------------------------------------------------------------------------

void NkWindow::SetProgress(float p) {
	if (mImpl)
		mImpl->SetProgress(p);
}

// ---------------------------------------------------------------------------
// Safe Area
// ---------------------------------------------------------------------------

NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
	return mImpl ? mImpl->GetSafeAreaInsets() : NkSafeAreaInsets{};
}

// ---------------------------------------------------------------------------
// Surface (pour Renderer)
// ---------------------------------------------------------------------------

NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
	return mImpl ? mImpl->GetSurfaceDesc() : NkSurfaceDesc{};
}

// ---------------------------------------------------------------------------
// Callback (délégué à l'EventImpl)
// ---------------------------------------------------------------------------

void NkWindow::SetEventCallback(NkEventCallback cb) {
	if (!mImpl)
		return;
	IEventImpl *ev = NkGetEventImpl();
	if (ev) {
		NkSurfaceDesc sd = mImpl->GetSurfaceDesc();
		// Passe le nativeHandle approprié à la plateforme
#if defined(NKENTSEU_FAMILY_WINDOWS)
		ev->SetWindowCallback(sd.hwnd, std::move(cb));
#elif defined(NKENTSEU_PLATFORM_XCB)
		ev->SetWindowCallback(reinterpret_cast<void *>(static_cast<uintptr_t>(sd.window)), std::move(cb));
#else
		ev->SetWindowCallback(nullptr, std::move(cb));
#endif
	}
}

} // namespace nkentseu
