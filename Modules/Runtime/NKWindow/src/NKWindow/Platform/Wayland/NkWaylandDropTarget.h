#pragma once

// =============================================================================
// NkWaylandDropTarget.h - Wayland drag and drop target (wl_data_device)
//
// API aligned with Win32/XLib/XCB drop targets:
// - event-oriented callbacks (NkDropFileEvent, NkDropTextEvent, ...)
// - legacy data callbacks kept for compatibility.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND)

#include <wayland-client.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <utility>
#include <unistd.h>

#include "NKWindow/Events/NkDropEvent.h"

namespace nkentseu {

    class NkWaylandDropTarget {
        public:
            using DropFileCallback  = std::function<void(const NkDropFileEvent&)>;
            using DropTextCallback  = std::function<void(const NkDropTextEvent&)>;
            using DropEnterCallback = std::function<void(const NkDropEnterEvent&)>;
            using DropLeaveCallback = std::function<void(const NkDropLeaveEvent&)>;

            // Legacy compatibility callbacks (data-only API)
            using DropFilesDataCallback = std::function<void(const NkDropFileData&)>;
            using DropTextDataCallback  = std::function<void(const NkDropTextData&)>;
            using DropEnterDataCallback = std::function<void(const NkDropEnterData&)>;
            using DropLeaveDataCallback = std::function<void()>;

            NkWaylandDropTarget(wl_display* display, wl_seat* seat, wl_surface* surface)
                : mDisplay(display), mSeat(seat), mSurface(surface) {}

            ~NkWaylandDropTarget() {
                if (mOffer) {
                    wl_data_offer_destroy(mOffer);
                    mOffer = nullptr;
                }
                if (mDataDevice) {
                    wl_data_device_destroy(mDataDevice);
                    mDataDevice = nullptr;
                }
            }

            void SetDataDeviceManager(wl_data_device_manager* manager) {
                if (!manager || !mSeat) {
                    return;
                }

                if (mDataDevice) {
                    wl_data_device_destroy(mDataDevice);
                    mDataDevice = nullptr;
                }

                mDataDevice = wl_data_device_manager_get_data_device(manager, mSeat);
                if (mDataDevice) {
                    wl_data_device_add_listener(mDataDevice, &kDataDeviceListener, this);
                }
            }

            // Event-oriented API
            void SetDropFileCallback(DropFileCallback cb)   { mDropFile = std::move(cb); }
            void SetDropTextCallback(DropTextCallback cb)   { mDropText = std::move(cb); }
            void SetDropEnterCallback(DropEnterCallback cb) { mDropEnter = std::move(cb); }
            void SetDropLeaveCallback(DropLeaveCallback cb) { mDropLeave = std::move(cb); }

            // Legacy API compatibility
            void SetDropFilesCallback(DropFilesDataCallback cb) { mDropFilesData = std::move(cb); }
            void SetDropTextCallback(DropTextDataCallback cb)   { mDropTextData = std::move(cb); }
            void SetDropEnterCallback(DropEnterDataCallback cb) { mDropEnterData = std::move(cb); }
            void SetDropLeaveCallback(DropLeaveDataCallback cb) { mDropLeaveData = std::move(cb); }

        private:
            wl_display*     mDisplay    = nullptr;
            wl_seat*        mSeat       = nullptr;
            wl_surface*     mSurface    = nullptr;
            wl_data_device* mDataDevice = nullptr;
            wl_data_offer*  mOffer      = nullptr;

            NkVector<NkString> mMimeTypes;
            NkF32 mDragX = 0.f;
            NkF32 mDragY = 0.f;
            bool mDragOnTargetSurface = false;

            DropFileCallback  mDropFile;
            DropTextCallback  mDropText;
            DropEnterCallback mDropEnter;
            DropLeaveCallback mDropLeave;

            DropFilesDataCallback mDropFilesData;
            DropTextDataCallback  mDropTextData;
            DropEnterDataCallback mDropEnterData;
            DropLeaveDataCallback mDropLeaveData;

            static bool HasMime(const NkVector<NkString>& mimeTypes, const char* mime) {
                for (NkU32 i = 0; i < mimeTypes.Size(); ++i) {
                    if (std::strcmp(mimeTypes[i].CStr(), mime) == 0) {
                        return true;
                    }
                }
                return false;
            }

            static NkVector<NkString> ParseUriList(const NkString& raw) {
                NkVector<NkString> paths;
                NkU32 start = 0;
                const NkU32 rawSize = raw.Size();

                while (start < rawSize) {
                    // Find '\n'
                    NkU32 end = rawSize;
                    for (NkU32 k = start; k < rawSize; ++k) {
                        if (raw[k] == '\n') { end = k; break; }
                    }

                    NkString line = raw.SubStr(start, end - start);
                    if (!line.Empty() && line.Back() == '\r') {
                        line.PopBack();
                    }

                    if (!line.Empty() && line[0] != '#' && line.StartsWith("file://")) {
                        NkString encoded = line.SubStr(7);
                        NkString decoded;
                        decoded.Reserve(encoded.Size());

                        for (NkU32 i = 0; i < encoded.Size(); ++i) {
                            if (encoded[i] == '%' && i + 2 < encoded.Size()) {
                                int value = 0;
                                std::sscanf(encoded.CStr() + i + 1, "%2x", &value);
                                decoded.PushBack(static_cast<char>(value));
                                i += 2;
                            } else {
                                decoded.PushBack(encoded[i]);
                            }
                        }

                        paths.PushBack(std::move(decoded));
                    }

                    start = end + 1;
                }

                return paths;
            }

            NkString ReadOfferData(wl_data_offer* offer, const NkString& mime) const {
                if (!offer || !mDisplay) {
                    return {};
                }

                int pipefd[2] = {-1, -1};
                if (pipe(pipefd) < 0) {
                    return {};
                }

                wl_data_offer_receive(offer, mime.CStr(), pipefd[1]);
                close(pipefd[1]);
                wl_display_flush(mDisplay);

                NkString out;
                char buffer[4096];
                ssize_t n = 0;
                while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
                    for (ssize_t i = 0; i < n; ++i) {
                        out.PushBack(buffer[i]);
                    }
                }
                close(pipefd[0]);

                return out;
            }

            void EmitDropEnter(const NkDropEnterData& data) {
                if (mDropEnter) {
                    mDropEnter(NkDropEnterEvent(data));
                }
                if (mDropEnterData) {
                    mDropEnterData(data);
                }
            }

            void EmitDropLeave() {
                if (mDropLeave) {
                    mDropLeave(NkDropLeaveEvent());
                }
                if (mDropLeaveData) {
                    mDropLeaveData();
                }
            }

            void EmitDropFiles(const NkDropFileData& data) {
                if (mDropFile) {
                    mDropFile(NkDropFileEvent(data));
                }
                if (mDropFilesData) {
                    mDropFilesData(data);
                }
            }

            void EmitDropText(const NkDropTextData& data) {
                if (mDropText) {
                    mDropText(NkDropTextEvent(data));
                }
                if (mDropTextData) {
                    mDropTextData(data);
                }
            }

            static void OnOfferOffer(void* data, wl_data_offer*, const char* mimeType) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!mimeType) {
                    return;
                }
                self->mMimeTypes.PushBack(NkString(mimeType));
            }

            static void OnOfferSourceActions(void*, wl_data_offer*, uint32_t) {}
            static void OnOfferAction(void*, wl_data_offer*, uint32_t) {}

            static constexpr wl_data_offer_listener kOfferListener = {
                .offer = OnOfferOffer,
                .source_actions = OnOfferSourceActions,
                .action = OnOfferAction,
            };

            static void OnDataDeviceDataOffer(void* data, wl_data_device*, wl_data_offer* offer) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!self) {
                    return;
                }

                if (self->mOffer && self->mOffer != offer) {
                    wl_data_offer_destroy(self->mOffer);
                }

                self->mOffer = offer;
                self->mMimeTypes.Clear();

                if (offer) {
                    wl_data_offer_add_listener(offer, &kOfferListener, self);
                }
            }

            static void OnDataDeviceEnter(void* data,
                                        wl_data_device*,
                                        uint32_t,
                                        wl_surface* surface,
                                        wl_fixed_t x,
                                        wl_fixed_t y,
                                        wl_data_offer* offer) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!self) {
                    return;
                }
                if (surface != self->mSurface) {
                    self->mDragOnTargetSurface = false;
                    return;
                }

                self->mDragOnTargetSurface = true;
                self->mDragX = static_cast<NkF32>(wl_fixed_to_double(x));
                self->mDragY = static_cast<NkF32>(wl_fixed_to_double(y));
                self->mOffer = offer;

                NkDropEnterData enterData{};
                enterData.x = static_cast<NkI32>(self->mDragX);
                enterData.y = static_cast<NkI32>(self->mDragY);
                enterData.numFiles = 0;
                enterData.hasText = HasMime(self->mMimeTypes, "text/plain;charset=utf-8") ||
                                    HasMime(self->mMimeTypes, "text/plain");
                enterData.hasImage = false;
                self->EmitDropEnter(enterData);

                if (offer) {
                    wl_data_offer_set_actions(
                        offer,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);

                    const char* acceptMime = nullptr;
                    if (HasMime(self->mMimeTypes, "text/uri-list")) {
                        acceptMime = "text/uri-list";
                    } else if (HasMime(self->mMimeTypes, "text/plain;charset=utf-8")) {
                        acceptMime = "text/plain;charset=utf-8";
                    } else if (HasMime(self->mMimeTypes, "text/plain")) {
                        acceptMime = "text/plain";
                    }
                    wl_data_offer_accept(offer, 0, acceptMime);
                }
            }

            static void OnDataDeviceLeave(void* data, wl_data_device*) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!self) {
                    return;
                }
                if (!self->mDragOnTargetSurface) {
                    return;
                }

                self->EmitDropLeave();
                self->mDragOnTargetSurface = false;

                if (self->mOffer) {
                    wl_data_offer_destroy(self->mOffer);
                    self->mOffer = nullptr;
                }
                self->mMimeTypes.Clear();
            }

            static void OnDataDeviceMotion(void* data, wl_data_device*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!self) {
                    return;
                }
                if (!self->mDragOnTargetSurface) {
                    return;
                }

                self->mDragX = static_cast<NkF32>(wl_fixed_to_double(x));
                self->mDragY = static_cast<NkF32>(wl_fixed_to_double(y));
            }

            static void OnDataDeviceDrop(void* data, wl_data_device*) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!self || !self->mDragOnTargetSurface || !self->mOffer) {
                    return;
                }

                const bool hasUri = HasMime(self->mMimeTypes, "text/uri-list");
                const bool hasUtf8Text = HasMime(self->mMimeTypes, "text/plain;charset=utf-8");
                const bool hasPlainText = HasMime(self->mMimeTypes, "text/plain");

                if (hasUri) {
                    NkString raw = self->ReadOfferData(self->mOffer, "text/uri-list");
                    NkVector<NkString> paths = ParseUriList(raw);
                    if (!paths.Empty()) {
                        NkDropFileData fileData{};
                        fileData.x = static_cast<NkI32>(self->mDragX);
                        fileData.y = static_cast<NkI32>(self->mDragY);
                        fileData.paths = std::move(paths);
                        self->EmitDropFiles(fileData);
                    }
                } else if (hasUtf8Text || hasPlainText) {
                    const char* mime = hasUtf8Text ? "text/plain;charset=utf-8" : "text/plain";
                    NkString text = self->ReadOfferData(self->mOffer, mime);

                    NkDropTextData textData{};
                    textData.x = static_cast<NkI32>(self->mDragX);
                    textData.y = static_cast<NkI32>(self->mDragY);
                    textData.text = std::move(text);
                    textData.mimeType = mime;
                    self->EmitDropText(textData);
                }

                wl_data_offer_finish(self->mOffer);
                wl_data_offer_destroy(self->mOffer);
                self->mOffer = nullptr;
                self->mMimeTypes.Clear();
                self->mDragOnTargetSurface = false;
            }

            static void OnDataDeviceSelection(void* data, wl_data_device*, wl_data_offer* offer) {
                auto* self = static_cast<NkWaylandDropTarget*>(data);
                if (!self) {
                    return;
                }
                if (offer && offer != self->mOffer) {
                    wl_data_offer_destroy(offer);
                }
            }

            static constexpr wl_data_device_listener kDataDeviceListener = {
                .data_offer = OnDataDeviceDataOffer,
                .enter = OnDataDeviceEnter,
                .leave = OnDataDeviceLeave,
                .motion = OnDataDeviceMotion,
                .drop = OnDataDeviceDrop,
                .selection = OnDataDeviceSelection,
            };
    };

    // Backward-compatible alias (legacy name kept for old includes)
    using NkWaylandDropImpl = NkWaylandDropTarget;

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_WAYLAND
