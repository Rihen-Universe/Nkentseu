#pragma once

// =============================================================================
// NkAndroidWindowImpl.h — Fenêtre Android (V2)
// - Create(config) sans IEventImpl
// - GetSafeAreaInsets() via DisplayCutout (Android 9+)
// - BlitSoftwareFramebuffer retiré → NkSoftwareRendererImpl::Present()
// =============================================================================

#include "../../Core/IWindowImpl.h"
#include "../../Core/NkSafeArea.h"
#include <android/native_window.h>
#include <android_native_app_glue.h>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	extern android_app *nk_android_global_app; ///< Fourni par NkAndroid.h

	/**
	 * @brief Class NkAndroidWindowImpl.
	 */
	class NkAndroidWindowImpl : public IWindowImpl {
		public:
			NkAndroidWindowImpl() = default;
			~NkAndroidWindowImpl() override {
				if (mIsOpen)
					Close();
			}

			bool Create(const NkWindowConfig &config) override;
			void Close() override;
			bool IsOpen() const override {
				return mIsOpen;
			}

			std::string GetTitle() const override {
				return mConfig.title;
			}
			void SetTitle(const std::string &t) override {
				mConfig.title = t;
			}
			NkVec2u GetSize() const override;
			NkVec2u GetPosition() const override {
				return {};
			}
			float GetDpiScale() const override;
			NkVec2u GetDisplaySize() const override {
				return GetSize();
			}
			NkVec2u GetDisplayPosition() const override {
				return {};
			}
			NkError GetLastError() const override {
				return mLastError;
			}

			void SetSize(NkU32, NkU32) override {
			} // plein écran forcé
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
			void SetFullscreen(bool) override {
			} // toujours full sur mobile
			bool SupportsOrientationControl() const override {
				return true;
			}
			void SetScreenOrientation(NkScreenOrientation orientation) override;
			NkScreenOrientation GetScreenOrientation() const override {
				return mOrientation;
			}
			void SetAutoRotateEnabled(bool enabled) override;
			bool IsAutoRotateEnabled() const override {
				return mOrientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
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

			ANativeWindow *GetNativeWindow() const {
				return mNativeWindow;
			}

		private:
			ANativeWindow *mNativeWindow = nullptr;
			bool mIsOpen = false;
			NkSafeAreaInsets mSafeArea;
			NkScreenOrientation mOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;

			void UpdateSafeArea();
			bool ApplyOrientation(NkScreenOrientation orientation);
	};

} // namespace nkentseu
