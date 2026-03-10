#pragma once
// =============================================================================
// NkEmscriptenDropTarget.h - WASM drag and drop target facade
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

#include "NKWindow/Core/NkWindowId.h"
#include "NKWindow/Events/NkDropEvent.h"

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nkentseu {

    class NkEmscriptenDropTarget {
        public:
            using DropFileCallback  = std::function<void(const NkDropFileEvent&)>;
            using DropTextCallback  = std::function<void(const NkDropTextEvent&)>;
            using DropEnterCallback = std::function<void(const NkDropEnterEvent&)>;
            using DropOverCallback  = std::function<void(const NkDropOverEvent&)>;
            using DropLeaveCallback = std::function<void(const NkDropLeaveEvent&)>;

            explicit NkEmscriptenDropTarget(NkWindowId windowId = NK_INVALID_WINDOW_ID,
                                      NkString canvasId = "#canvas")
                : mCanvasId(std::move(canvasId))
            {
                BindWindow(windowId);
            }

            ~NkEmscriptenDropTarget() {
                UnbindWindow();
            }

            NkEmscriptenDropTarget(const NkEmscriptenDropTarget&) = delete;
            NkEmscriptenDropTarget& operator=(const NkEmscriptenDropTarget&) = delete;

            void BindWindow(NkWindowId windowId) {
                UnbindWindow();
                if (windowId == NK_INVALID_WINDOW_ID) {
                    return;
                }

                std::lock_guard<std::mutex> lock(RegistryMutex());
                Registry()[windowId] = this;
                mWindowId = windowId;
            }

            void UnbindWindow() {
                if (mWindowId == NK_INVALID_WINDOW_ID) {
                    return;
                }

                std::lock_guard<std::mutex> lock(RegistryMutex());
                auto& map = Registry();
                NkEmscriptenDropTarget** ptr = map.Find(mWindowId);
                if (ptr && *ptr == this) {
                    map.Erase(mWindowId);
                }
                mWindowId = NK_INVALID_WINDOW_ID;
            }

            NkWindowId GetWindowId() const {
                return mWindowId;
            }

            const NkString& GetCanvasId() const {
                return mCanvasId;
            }

            void SetDropFileCallback(DropFileCallback cb) {
                mDropFile = std::move(cb);
            }

            void SetDropTextCallback(DropTextCallback cb) {
                mDropText = std::move(cb);
            }

            void SetDropEnterCallback(DropEnterCallback cb) {
                mDropEnter = std::move(cb);
            }

            void SetDropOverCallback(DropOverCallback cb) {
                mDropOver = std::move(cb);
            }

            void SetDropLeaveCallback(DropLeaveCallback cb) {
                mDropLeave = std::move(cb);
            }

            void OnDragEnter(float x, float y, NkU32 numItems, bool hasText, bool hasImage) {
                NkDropEnterData data{};
                data.x = static_cast<NkI32>(x);
                data.y = static_cast<NkI32>(y);
                data.numFiles = numItems;
                data.hasText = hasText;
                data.hasImage = hasImage;
                EmitDropEnter(data);
            }

            void OnDragOver(float x, float y) {
                NkDropOverData data{};
                data.x = static_cast<NkI32>(x);
                data.y = static_cast<NkI32>(y);
                EmitDropOver(data);
            }

            void OnDragLeave() {
                EmitDropLeave();
            }

            void OnDropText(float x, float y, const NkString& text, const NkString& mimeType = "text/plain") {
                NkDropTextData data{};
                data.x = static_cast<NkI32>(x);
                data.y = static_cast<NkI32>(y);
                data.text = text;
                data.mimeType = mimeType;
                EmitDropText(data);
            }

            void OnDropFiles(float x, float y, const NkVector<NkString>& names) {
                NkDropFileData data{};
                data.x = static_cast<NkI32>(x);
                data.y = static_cast<NkI32>(y);
                data.paths = names;
                EmitDropFiles(data);
            }

            static NkEmscriptenDropTarget* FindTarget(NkWindowId windowId) {
                if (windowId == NK_INVALID_WINDOW_ID) {
                    return nullptr;
                }

                std::lock_guard<std::mutex> lock(RegistryMutex());
                auto& map = Registry();
                NkEmscriptenDropTarget** ptr = map.Find(windowId);
                return ptr ? *ptr : nullptr;
            }

            static bool DispatchDragEnter(NkWindowId windowId,
                                          float x,
                                          float y,
                                          NkU32 numItems,
                                          bool hasText,
                                          bool hasImage)
            {
                NkEmscriptenDropTarget* target = FindTarget(windowId);
                if (!target) {
                    return false;
                }
                target->OnDragEnter(x, y, numItems, hasText, hasImage);
                return true;
            }

            static bool DispatchDragOver(NkWindowId windowId, float x, float y) {
                NkEmscriptenDropTarget* target = FindTarget(windowId);
                if (!target) {
                    return false;
                }
                target->OnDragOver(x, y);
                return true;
            }

            static bool DispatchDragLeave(NkWindowId windowId) {
                NkEmscriptenDropTarget* target = FindTarget(windowId);
                if (!target) {
                    return false;
                }
                target->OnDragLeave();
                return true;
            }

            static bool DispatchDropText(NkWindowId windowId,
                                         float x,
                                         float y,
                                         const NkString& text,
                                         const NkString& mimeType)
            {
                NkEmscriptenDropTarget* target = FindTarget(windowId);
                if (!target) {
                    return false;
                }
                target->OnDropText(x, y, text, mimeType.Empty() ? "text/plain" : mimeType);
                return true;
            }

            static bool DispatchDropFiles(NkWindowId windowId,
                                          float x,
                                          float y,
                                          const NkVector<NkString>& names)
            {
                NkEmscriptenDropTarget* target = FindTarget(windowId);
                if (!target) {
                    return false;
                }
                target->OnDropFiles(x, y, names);
                return true;
            }

        private:
            static NkUnorderedMap<NkWindowId, NkEmscriptenDropTarget*>& Registry() {
                static NkUnorderedMap<NkWindowId, NkEmscriptenDropTarget*> map;
                return map;
            }

            static std::mutex& RegistryMutex() {
                static std::mutex mutex;
                return mutex;
            }

            void EmitDropEnter(const NkDropEnterData& data) {
                if (mDropEnter) {
                    mDropEnter(NkDropEnterEvent(data));
                }
            }

            void EmitDropOver(const NkDropOverData& data) {
                if (mDropOver) {
                    mDropOver(NkDropOverEvent(data));
                }
            }

            void EmitDropLeave() {
                if (mDropLeave) {
                    mDropLeave(NkDropLeaveEvent());
                }
            }

            void EmitDropFiles(const NkDropFileData& data) {
                if (mDropFile) {
                    mDropFile(NkDropFileEvent(data));
                }
            }

            void EmitDropText(const NkDropTextData& data) {
                if (mDropText) {
                    mDropText(NkDropTextEvent(data));
                }
            }

            NkWindowId mWindowId = NK_INVALID_WINDOW_ID;
            NkString mCanvasId = "#canvas";

            DropFileCallback  mDropFile;
            DropTextCallback  mDropText;
            DropEnterCallback mDropEnter;
            DropOverCallback  mDropOver;
            DropLeaveCallback mDropLeave;
    };

    using NkEmscriptenDropImpl = NkEmscriptenDropTarget;

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
