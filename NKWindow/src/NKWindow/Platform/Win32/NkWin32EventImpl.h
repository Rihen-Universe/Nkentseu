#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <unordered_map>
#include <functional>
#include <vector>

#include "NKWindow/Events/IEventImpl.h"
#include "NKWindow/Core/NkEvents.h"

namespace nkentseu {

    class NkWin32WindowImpl;

    class NkKeycodeMap {
		public:
			static NkKey NkKeyFromWin32VK(NkU32 vk, bool extended) noexcept;
			static NkKey NkKeyFromWin32Scancode(NkU32 scancode, bool extended) noexcept;
    };

    class NkWin32EventImpl : public IEventImpl {
		public:
			NkWin32EventImpl() = default;
			~NkWin32EventImpl() override = default;

			// IEventImpl
			void Initialize(IWindowImpl* owner, void* nativeHandle) override;
			void Shutdown(void* nativeHandle) override;
			void PollEvents() override;
			NkEvent* Front() const override;
			void Pop() override;
			bool IsEmpty() const override;
			void PushEvent(std::unique_ptr<NkEvent> event) override;
			std::size_t Size() const override;

			void SetGlobalCallback(NkEventCallback cb) override;
			void SetWindowCallback(void* nativeHandle, NkEventCallback cb) override;
			void AddTypedCallback(NkEventType::Value type, NkEventCallback cb) override;
			void RemoveTypedCallback(NkEventType::Value type, NkEventCallback cb) override;
			void ClearTypedCallbacks(NkEventType::Value type) override;
			void ClearAllCallbacks() override;
			void DispatchEvent(NkEvent* event, void* nativeHandle) override;

			const NkEventState& GetInputState() const override;
			NkEventState& GetInputState() override;
			const char* GetPlatformName() const noexcept override;
			bool IsReady() const noexcept override;

			// WndProc statique
			static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
			static void BlitToHwnd(HWND hwnd, const NkU8* rgbaPixels, NkU32 w, NkU32 h);

			// Pour NkWin32WindowImpl
			void SetCurrentImpl() noexcept { sCurrentImpl = this; }

		private:
			LRESULT ProcessWin32Message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
										NkWin32WindowImpl* owner);
			static NkKey VkeyToNkKey(WPARAM vk, LPARAM flags);
			static NkModifierState CurrentMods();

			NkEventCallback mGlobalCallback;
			std::unordered_map<HWND, NkEventCallback> mWindowCallbacks;
			std::unordered_map<NkEventType::Value, std::vector<NkEventCallback>> mTypedCallbacks;

			bool mRawInputRegistered = false;
			NkI32 mPrevMouseX = 0;
			NkI32 mPrevMouseY = 0;

			static thread_local NkWin32EventImpl* sCurrentImpl;
    };

} // namespace nkentseu