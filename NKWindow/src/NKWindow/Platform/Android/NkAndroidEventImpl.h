#pragma once

// =============================================================================
// NkAndroidEventImpl.h  —  Système d'événements Android
// =============================================================================

#include "../../Events/IEventImpl.h"
#include <android_native_app_glue.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <unordered_map>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	class NkAndroidWindowImpl;

	/**
	 * @brief Class NkAndroidEventImpl.
	 */
	class NkAndroidEventImpl : public IEventImpl {
		public:
			NkAndroidEventImpl() = default;
			~NkAndroidEventImpl() override = default;

			void Initialize(IWindowImpl *owner, void *nativeHandle) override;
			void Shutdown(void *nativeHandle) override;

			void PollEvents() override;
			NkEvent *Front() const override;
			void Pop() override;
			bool IsEmpty() const override;
			void PushEvent(std::unique_ptr<NkEvent> event) override;
			std::size_t Size() const override;

			void SetEventCallback(NkEventCallback cb) override;
			void SetWindowCallback(void *nativeHandle, NkEventCallback cb) override;
			void DispatchEvent(NkEvent *event, void *nativeHandle) override;

			const NkEventState& GetInputState() const override;
			NkEventState& GetInputState() override;
			const char* GetPlatformName() const noexcept override;
			bool IsReady() const noexcept override;

		private:
			static void OnAppCmd(android_app *app, int32_t cmd);
			static int32_t OnInputEvent(android_app *app, AInputEvent *ev);
			static NkKey AkeyToNkKey(int32_t keycode);

			/**
			 * @brief Struct WindowEntry.
			 */
			struct WindowEntry {
				NkAndroidWindowImpl *window = nullptr;
				NkEventCallback callback;
			};
			android_app *mApp = nullptr;
			NkEventCallback mGlobalCallback;
			std::unordered_map<void *, WindowEntry> mWindowMap;
			static NkAndroidEventImpl *sInstance;
	};
} // namespace nkentseu
