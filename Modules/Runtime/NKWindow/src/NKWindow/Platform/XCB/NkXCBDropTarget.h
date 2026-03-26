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
#include <cstdlib>
#include <cstring>

#include "NKEvent/NkDropEvent.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/String/Encoding/NkASCII.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {

    class NkXCBDropTarget {
        public:
            using DropFileCallback  = NkFunction<void(const NkDropFileEvent&)>;
            using DropTextCallback  = NkFunction<void(const NkDropTextEvent&)>;
            using DropEnterCallback = NkFunction<void(const NkDropEnterEvent&)>;
            using DropLeaveCallback = NkFunction<void(const NkDropLeaveEvent&)>;

            // Legacy compatibility callbacks (data-only API)
            using DropFilesDataCallback = NkFunction<void(const NkDropFileData&)>;
            using DropTextDataCallback  = NkFunction<void(const NkDropTextData&)>;
            using DropEnterDataCallback = NkFunction<void(const NkDropEnterData&)>;
            using DropLeaveDataCallback = NkFunction<void()>;

            explicit NkXCBDropTarget(xcb_connection_t* connection, xcb_window_t window)
                : mConnection(connection), mWindow(window) {
                InitAtoms();
                SetXdndAware();
            }

            // Event-oriented API
            void SetDropFileCallback(DropFileCallback cb)   { mDropFile = traits::NkMove(cb); }
            void SetDropTextCallback(DropTextCallback cb)   { mDropText = traits::NkMove(cb); }
            void SetDropEnterCallback(DropEnterCallback cb) { mDropEnter = traits::NkMove(cb); }
            void SetDropLeaveCallback(DropLeaveCallback cb) { mDropLeave = traits::NkMove(cb); }

            // Legacy API compatibility
            void SetDropFilesCallback(DropFilesDataCallback cb) { mDropFilesData = traits::NkMove(cb); }
            void SetDropTextCallback(DropTextDataCallback cb)   { mDropTextData = traits::NkMove(cb); }
            void SetDropEnterCallback(DropEnterDataCallback cb) { mDropEnterData = traits::NkMove(cb); }
            void SetDropLeaveCallback(DropLeaveDataCallback cb) { mDropLeaveData = traits::NkMove(cb); }

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
                    xcb_intern_atom(mConnection, 0, static_cast<uint16_t>(::strlen(name)), name);
                xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(mConnection, cookie, nullptr);
                xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
                ::free(reply);
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
                    out = NkString(bytes, static_cast<NkString::SizeType>(len));
                }

                ::free(reply);
                return out;
            }

            static NkVector<NkString> ParseUriList(const NkString& raw) {
                NkVector<NkString> paths;
                NkString::SizeType start = 0;
                const NkString::SizeType rawSize = raw.Size();

                while (start < rawSize) {
                    NkString::SizeType end = rawSize;
                    for (NkString::SizeType k = start; k < rawSize; ++k) {
                        if (raw[k] == '\n') {
                            end = k;
                            break;
                        }
                    }

                    NkString line = raw.SubStr(start, end - start);
                    if (!line.Empty() && line.Back() == '\r') {
                        line.PopBack();
                    }

                    if (line.Empty() || line[0] == '#') {
                        start = (end < rawSize) ? (end + 1) : rawSize;
                        continue;
                    }

                    if (!line.StartsWith("file://")) {
                        start = (end < rawSize) ? (end + 1) : rawSize;
                        continue;
                    }

                    NkString encoded = line.SubStr(7);
                    NkString decoded;
                    decoded.Reserve(encoded.Size());

                    for (NkString::SizeType i = 0; i < encoded.Size(); ++i) {
                        if (encoded[i] == '%' && i + 2 < encoded.Size()) {
                            const int32 hi = encoding::ascii::NkToHexDigit(encoded[i + 1]);
                            const int32 lo = encoding::ascii::NkToHexDigit(encoded[i + 2]);
                            if (hi >= 0 && lo >= 0) {
                                decoded.PushBack(static_cast<char>((hi << 4) | lo));
                                i += 2;
                                continue;
                            }
                        }
                        decoded.PushBack(encoded[i]);
                    }

                    paths.PushBack(traits::NkMove(decoded));
                    start = (end < rawSize) ? (end + 1) : rawSize;
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
