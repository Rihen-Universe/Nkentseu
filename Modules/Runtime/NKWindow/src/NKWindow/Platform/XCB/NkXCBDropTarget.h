#pragma once

// =============================================================================
// NkXCBDropTarget.h
// XDND v5 drag and drop target for XCB.
//
// API mirrors NkXLibDropTarget:
// - event callbacks (NkDropFileEvent, NkDropTextEvent, ...)
// - legacy data callbacks kept for compatibility.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB)

#include <xcb/xcb.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "NKWindow/Events/NkDropEvent.h"

namespace nkentseu {

    class NkXCBDropTarget {
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

            explicit NkXCBDropTarget(xcb_connection_t* connection, xcb_window_t window)
                : mConnection(connection), mWindow(window) {
                InitAtoms();
                SetXdndAware();
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

            // Call from event loop:
            // - XCB_CLIENT_MESSAGE  -> HandleClientMessage(...)
            // - XCB_SELECTION_NOTIFY -> HandleSelectionNotify(...)
            void HandleClientMessage(const xcb_client_message_event_t& ev);
            void HandleSelectionNotify(const xcb_selection_notify_event_t& ev);

        private:
            xcb_connection_t* mConnection = nullptr;
            xcb_window_t      mWindow = XCB_NONE;

            // XDND atoms
            xcb_atom_t aXdndAware = XCB_ATOM_NONE;
            xcb_atom_t aXdndEnter = XCB_ATOM_NONE;
            xcb_atom_t aXdndPosition = XCB_ATOM_NONE;
            xcb_atom_t aXdndDrop = XCB_ATOM_NONE;
            xcb_atom_t aXdndLeave = XCB_ATOM_NONE;
            xcb_atom_t aXdndFinished = XCB_ATOM_NONE;
            xcb_atom_t aXdndStatus = XCB_ATOM_NONE;
            xcb_atom_t aXdndSelection = XCB_ATOM_NONE;
            xcb_atom_t aActionCopy = XCB_ATOM_NONE;

            // MIME atoms
            xcb_atom_t aUriList = XCB_ATOM_NONE;
            xcb_atom_t aTextPlain = XCB_ATOM_NONE;
            xcb_atom_t aTextPlainUtf8 = XCB_ATOM_NONE;

            // Current drag state
            xcb_window_t mSourceWindow = XCB_NONE;
            int32        mDragX = 0;
            int32        mDragY = 0;
            bool         mHasDrop = false;

            // Event callbacks
            DropFileCallback  mDropFile;
            DropTextCallback  mDropText;
            DropEnterCallback mDropEnter;
            DropLeaveCallback mDropLeave;

            // Legacy callbacks
            DropFilesDataCallback mDropFilesData;
            DropTextDataCallback  mDropTextData;
            DropEnterDataCallback mDropEnterData;
            DropLeaveDataCallback mDropLeaveData;

            xcb_atom_t GetAtom(const char* name) const {
                xcb_intern_atom_cookie_t cookie =
                    xcb_intern_atom(mConnection, 0, static_cast<uint16_t>(std::strlen(name)), name);
                xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(mConnection, cookie, nullptr);
                xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
                std::free(reply);
                return atom;
            }

            void InitAtoms() {
                aXdndAware = GetAtom("XdndAware");
                aXdndEnter = GetAtom("XdndEnter");
                aXdndPosition = GetAtom("XdndPosition");
                aXdndDrop = GetAtom("XdndDrop");
                aXdndLeave = GetAtom("XdndLeave");
                aXdndFinished = GetAtom("XdndFinished");
                aXdndStatus = GetAtom("XdndStatus");
                aXdndSelection = GetAtom("XdndSelection");
                aActionCopy = GetAtom("XdndActionCopy");

                aUriList = GetAtom("text/uri-list");
                aTextPlain = GetAtom("text/plain");
                aTextPlainUtf8 = GetAtom("text/plain;charset=utf-8");
            }

            void SetXdndAware() {
                uint32_t version = 5;
                xcb_change_property(
                    mConnection,
                    XCB_PROP_MODE_REPLACE,
                    mWindow,
                    aXdndAware,
                    XCB_ATOM_ATOM,
                    32,
                    1,
                    &version);
                xcb_flush(mConnection);
            }

            void SendStatus(bool accept) {
                if (mSourceWindow == XCB_NONE) {
                    return;
                }

                xcb_client_message_event_t reply{};
                reply.response_type = XCB_CLIENT_MESSAGE;
                reply.format = 32;
                reply.window = mSourceWindow;
                reply.type = aXdndStatus;
                reply.data.data32[0] = mWindow;
                reply.data.data32[1] = accept ? 1u : 0u;
                reply.data.data32[4] = accept ? aActionCopy : XCB_ATOM_NONE;

                xcb_send_event(
                    mConnection,
                    0,
                    mSourceWindow,
                    XCB_EVENT_MASK_NO_EVENT,
                    reinterpret_cast<const char*>(&reply));
                xcb_flush(mConnection);
            }

            void SendFinished(bool accepted = true) {
                if (mSourceWindow == XCB_NONE) {
                    return;
                }

                xcb_client_message_event_t reply{};
                reply.response_type = XCB_CLIENT_MESSAGE;
                reply.format = 32;
                reply.window = mSourceWindow;
                reply.type = aXdndFinished;
                reply.data.data32[0] = mWindow;
                reply.data.data32[1] = accepted ? 1u : 0u;
                reply.data.data32[2] = accepted ? aActionCopy : XCB_ATOM_NONE;

                xcb_send_event(
                    mConnection,
                    0,
                    mSourceWindow,
                    XCB_EVENT_MASK_NO_EVENT,
                    reinterpret_cast<const char*>(&reply));
                xcb_flush(mConnection);
            }

            void RequestSelection(xcb_atom_t targetType) {
                xcb_convert_selection(
                    mConnection,
                    mWindow,
                    aXdndSelection,
                    targetType,
                    aXdndSelection,
                    XCB_CURRENT_TIME);
                xcb_flush(mConnection);
            }

            NkString ReadProperty(xcb_atom_t property) const {
                xcb_get_property_cookie_t cookie = xcb_get_property(
                    mConnection,
                    1,
                    mWindow,
                    property,
                    XCB_GET_PROPERTY_TYPE_ANY,
                    0,
                    0x7FFFFFFF);

                xcb_get_property_reply_t* reply = xcb_get_property_reply(mConnection, cookie, nullptr);
                if (!reply) {
                    return {};
                }

                const int len = xcb_get_property_value_length(reply);
                const char* bytes = static_cast<const char*>(xcb_get_property_value(reply));

                NkString out;
                if (bytes && len > 0) {
                    std::string tmp(bytes, static_cast<std::size_t>(len));
                    out = NkString(tmp.c_str());
                }

                std::free(reply);
                return out;
            }

            static NkVector<NkString> ParseUriList(const NkString& raw) {
                // Use std::string internally for parsing (std::getline / istringstream)
                NkVector<NkString> paths;
                std::istringstream ss(raw.CStr());
                std::string line;

                while (std::getline(ss, line)) {
                    if (line.empty() || line[0] == '#') {
                        continue;
                    }
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    if (line.rfind("file://", 0) != 0) {
                        continue;
                    }

                    const std::string encoded = line.substr(7);
                    std::string decoded;
                    decoded.reserve(encoded.size());

                    for (std::size_t i = 0; i < encoded.size(); ++i) {
                        if (encoded[i] == '%' && i + 2 < encoded.size()) {
                            int value = 0;
                            std::sscanf(encoded.c_str() + i + 1, "%2x", &value);
                            decoded.push_back(static_cast<char>(value));
                            i += 2;
                        } else {
                            decoded.push_back(encoded[i]);
                        }
                    }

                    paths.PushBack(NkString(decoded.c_str()));
                }

                return paths;
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
        };

        inline void NkXCBDropTarget::HandleClientMessage(const xcb_client_message_event_t& ev) {
            const xcb_atom_t type = ev.type;

            if (type == aXdndEnter) {
                mSourceWindow = static_cast<xcb_window_t>(ev.data.data32[0]);

                NkDropEnterData data{};
                data.x = mDragX;
                data.y = mDragY;
                data.numFiles = 0;
                data.hasText = false;
                data.hasImage = false;
                EmitDropEnter(data);

                SendStatus(true);
                return;
            }

            if (type == aXdndPosition) {
                mSourceWindow = static_cast<xcb_window_t>(ev.data.data32[0]);

                const uint32_t packed = ev.data.data32[2];
                mDragX = static_cast<int32>(static_cast<int16_t>((packed >> 16) & 0xFFFFu));
                mDragY = static_cast<int32>(static_cast<int16_t>(packed & 0xFFFFu));

                SendStatus(true);
                return;
            }

            if (type == aXdndLeave) {
                mHasDrop = false;
                EmitDropLeave();
                return;
            }

            if (type == aXdndDrop) {
                mSourceWindow = static_cast<xcb_window_t>(ev.data.data32[0]);
                mHasDrop = true;
                RequestSelection(aUriList);
                return;
            }
        }

        inline void NkXCBDropTarget::HandleSelectionNotify(const xcb_selection_notify_event_t& ev) {
            if (!mHasDrop || ev.property == XCB_ATOM_NONE) {
                mHasDrop = false;
                SendFinished(false);
                return;
            }

            const NkString payload = ReadProperty(ev.property);

            if (ev.target == aUriList) {
                const NkVector<NkString> paths = ParseUriList(payload);
                if (!paths.empty()) {
                    NkDropFileData data{};
                    data.x = mDragX;
                    data.y = mDragY;
                    data.paths = paths;
                    EmitDropFiles(data);
                } else {
                    RequestSelection(aTextPlainUtf8);
                    return;
                }
            } else if (ev.target == aTextPlainUtf8 || ev.target == aTextPlain) {
                NkDropTextData data{};
                data.x = mDragX;
                data.y = mDragY;
                data.text = payload;
                data.mimeType = (ev.target == aTextPlainUtf8)
                    ? "text/plain;charset=utf-8"
                    : "text/plain";
                EmitDropText(data);
            } else {
                RequestSelection(aTextPlainUtf8);
                return;
            }

            mHasDrop = false;
            SendFinished(true);
    }

    // Backward-compatible alias
    using NkXCBDropImpl = NkXCBDropTarget;

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XCB
