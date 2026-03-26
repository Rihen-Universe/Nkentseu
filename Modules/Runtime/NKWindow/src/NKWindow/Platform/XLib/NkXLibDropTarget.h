#pragma once

// =============================================================================
// NkXLibDropTarget.h
// Drag & Drop XDND v5 pour XLib.
//
// Aligne l'API sur NkWin32DropTarget:
// - callbacks orientes events (NkDropFileEvent, NkDropTextEvent, ...)
// - garde une compatibilite data-callback pour le code existant.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB)

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "NKEvent/NkDropEvent.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKCore/NkTraits.h"

namespace nkentseu {

    class NkXLibDropTarget {
    public:
        using DropFileCallback  = NkFunction<void(const NkDropFileEvent&)>;
        using DropTextCallback  = NkFunction<void(const NkDropTextEvent&)>;
        using DropEnterCallback = NkFunction<void(const NkDropEnterEvent&)>;
        using DropLeaveCallback = NkFunction<void(const NkDropLeaveEvent&)>;

        // Legacy compatibility callbacks (ancienne API data-only)
        using DropFilesDataCallback = NkFunction<void(const NkDropFileData&)>;
        using DropTextDataCallback  = NkFunction<void(const NkDropTextData&)>;
        using DropEnterDataCallback = NkFunction<void(const NkDropEnterData&)>;
        using DropLeaveDataCallback = NkFunction<void()>;

        explicit NkXLibDropTarget(Display* display, ::Window window)
            : mDisplay(display), mWindow(window) {
            InitAtoms();
            SetXdndAware();
        }

        // API Win32-like (events)
        void SetDropFileCallback(DropFileCallback cb)   { mDropFile = traits::NkMove(cb); }
        void SetDropTextCallback(DropTextCallback cb)   { mDropText = traits::NkMove(cb); }
        void SetDropEnterCallback(DropEnterCallback cb) { mDropEnter = traits::NkMove(cb); }
        void SetDropLeaveCallback(DropLeaveCallback cb) { mDropLeave = traits::NkMove(cb); }

        // Legacy API compatibility
        void SetDropFilesCallback(DropFilesDataCallback cb) { mDropFilesData = traits::NkMove(cb); }
        void SetDropTextCallback(DropTextDataCallback cb)   { mDropTextData = traits::NkMove(cb); }
        void SetDropEnterCallback(DropEnterDataCallback cb) { mDropEnterData = traits::NkMove(cb); }
        void SetDropLeaveCallback(DropLeaveDataCallback cb) { mDropLeaveData = traits::NkMove(cb); }

        // Appeler depuis la boucle events XLib:
        // - sur ClientMessage  : HandleClientMessage(xev.xclient)
        // - sur SelectionNotify: HandleSelectionNotify(xev.xselection)
        void HandleClientMessage(const XClientMessageEvent& ev);
        void HandleSelectionNotify(const XSelectionEvent& ev);

    private:
        Display*  mDisplay = nullptr;
        ::Window  mWindow  = 0;

        // XDND atoms
        Atom aXdndAware     = 0;
        Atom aXdndEnter     = 0;
        Atom aXdndPosition  = 0;
        Atom aXdndDrop      = 0;
        Atom aXdndLeave     = 0;
        Atom aXdndFinished  = 0;
        Atom aXdndStatus    = 0;
        Atom aXdndSelection = 0;
        Atom aActionCopy    = 0;

        // Mime types
        Atom aUriList       = 0;
        Atom aTextPlain     = 0;
        Atom aTextPlainUtf8 = 0;

        // Current drag state
        ::Window mSourceWin = 0;
        int32    mDragX     = 0;
        int32    mDragY     = 0;
        bool     mHasDrop   = false;

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

        Atom GetAtom(const char* name) const {
            return XInternAtom(mDisplay, name, False);
        }

        void InitAtoms() {
            aXdndAware     = GetAtom("XdndAware");
            aXdndEnter     = GetAtom("XdndEnter");
            aXdndPosition  = GetAtom("XdndPosition");
            aXdndDrop      = GetAtom("XdndDrop");
            aXdndLeave     = GetAtom("XdndLeave");
            aXdndFinished  = GetAtom("XdndFinished");
            aXdndStatus    = GetAtom("XdndStatus");
            aXdndSelection = GetAtom("XdndSelection");
            aActionCopy    = GetAtom("XdndActionCopy");

            aUriList       = GetAtom("text/uri-list");
            aTextPlain     = GetAtom("text/plain");
            aTextPlainUtf8 = GetAtom("text/plain;charset=utf-8");
        }

        void SetXdndAware() {
            long version = 5;
            XChangeProperty(
                mDisplay, mWindow,
                aXdndAware, XA_ATOM, 32,
                PropModeReplace,
                reinterpret_cast<unsigned char*>(&version), 1);
            XFlush(mDisplay);
        }

        void SendStatus(bool accept) {
            XClientMessageEvent reply{};
            reply.type         = ClientMessage;
            reply.display      = mDisplay;
            reply.window       = mSourceWin;
            reply.message_type = aXdndStatus;
            reply.format       = 32;
            reply.data.l[0]    = static_cast<long>(mWindow);
            reply.data.l[1]    = accept ? 1L : 0L;
            reply.data.l[4]    = accept ? static_cast<long>(aActionCopy) : 0L;

            XSendEvent(
                mDisplay, mSourceWin, False, NoEventMask,
                reinterpret_cast<XEvent*>(&reply));
            XFlush(mDisplay);
        }

        void SendFinished(bool accepted = true) {
            XClientMessageEvent reply{};
            reply.type         = ClientMessage;
            reply.display      = mDisplay;
            reply.window       = mSourceWin;
            reply.message_type = aXdndFinished;
            reply.format       = 32;
            reply.data.l[0]    = static_cast<long>(mWindow);
            reply.data.l[1]    = accepted ? 1L : 0L;
            reply.data.l[2]    = accepted ? static_cast<long>(aActionCopy) : 0L;

            XSendEvent(
                mDisplay, mSourceWin, False, NoEventMask,
                reinterpret_cast<XEvent*>(&reply));
            XFlush(mDisplay);
        }

        void RequestSelection(Atom type) {
            XConvertSelection(
                mDisplay,
                aXdndSelection,
                type,
                aXdndSelection,
                mWindow,
                CurrentTime);
            XFlush(mDisplay);
        }

        NkString ReadProperty(Atom property) const {
            Atom actualType = None;
            int actualFormat = 0;
            unsigned long nItems = 0;
            unsigned long bytesLeft = 0;
            unsigned char* data = nullptr;

            XGetWindowProperty(
                mDisplay, mWindow, property,
                0, 0x7FFFFFFF, True,
                AnyPropertyType,
                &actualType, &actualFormat,
                &nItems, &bytesLeft, &data);

            NkString result;
            if (data) {
                // Xdnd uses 8-bit strings for text/uri-list and text/plain
                result = NkString(reinterpret_cast<const char*>(data), static_cast<NkString::SizeType>(nItems));
                XFree(data);
            }
            return result;
        }

        static int HexNibble(char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        }

        static NkVector<NkString> ParseUriList(const NkString& raw) {
            NkVector<NkString> paths;
            NkString::SizeType len   = raw.Size();
            NkString::SizeType start = 0;
            for (NkString::SizeType i = 0; i <= len; ++i) {
                bool atEnd     = (i == len);
                bool atNewline = (!atEnd && raw[i] == '\n');
                if (!atEnd && !atNewline) continue;

                NkString line = raw.SubStr(start, i - start);
                start = i + 1;

                // Strip trailing '\r'
                if (!line.Empty() && line.Back() == '\r') line.PopBack();

                // Skip empty lines and comments
                if (line.Empty() || line[0] == '#') continue;

                // Must start with "file://"
                if (!line.StartsWith("file://")) continue;

                NkString encoded = line.SubStr(7);
                NkString decoded;
                decoded.Reserve(encoded.Size());
                for (NkString::SizeType j = 0; j < encoded.Size(); ++j) {
                    if (encoded[j] == '%' && j + 2 < encoded.Size()) {
                        const int hi = HexNibble(encoded[j + 1]);
                        const int lo = HexNibble(encoded[j + 2]);
                        if (hi >= 0 && lo >= 0) {
                            decoded.PushBack(static_cast<char>((hi << 4) | lo));
                            j += 2;
                            continue;
                        }
                    }
                    decoded.PushBack(encoded[j]);
                }
                paths.PushBack(traits::NkMove(decoded));
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

    inline void NkXLibDropTarget::HandleClientMessage(const XClientMessageEvent& ev) {
        const Atom type = ev.message_type;

        if (type == aXdndEnter) {
            mSourceWin = static_cast<::Window>(ev.data.l[0]);

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
            mSourceWin = static_cast<::Window>(ev.data.l[0]);
            const unsigned long coords = static_cast<unsigned long>(ev.data.l[2]);
            mDragX = static_cast<int32>((coords >> 16) & 0xFFFF);
            mDragY = static_cast<int32>(coords & 0xFFFF);
            SendStatus(true);
            return;
        }

        if (type == aXdndLeave) {
            EmitDropLeave();
            return;
        }

        if (type == aXdndDrop) {
            mSourceWin = static_cast<::Window>(ev.data.l[0]);
            mHasDrop = true;
            RequestSelection(aUriList);
            return;
        }
    }

    inline void NkXLibDropTarget::HandleSelectionNotify(const XSelectionEvent& ev) {
        if (!mHasDrop || ev.property == None) {
            SendFinished(false);
            return;
        }

        const NkString data = ReadProperty(ev.property);

        if (ev.target == aUriList) {
            const NkVector<NkString> paths = ParseUriList(data);
            if (!paths.empty()) {
                NkDropFileData drop{};
                drop.x = mDragX;
                drop.y = mDragY;
                drop.paths = paths;
                EmitDropFiles(drop);
            } else {
                // URI list vide: fallback texte
                RequestSelection(aTextPlainUtf8);
                return;
            }
        } else if (ev.target == aTextPlainUtf8 || ev.target == aTextPlain) {
            NkDropTextData drop{};
            drop.x = mDragX;
            drop.y = mDragY;
            drop.text = data;
            drop.mimeType = (ev.target == aTextPlainUtf8)
                ? "text/plain;charset=utf-8"
                : "text/plain";
            EmitDropText(drop);
        } else {
            // Type non reconnu -> tente texte UTF-8
            RequestSelection(aTextPlainUtf8);
            return;
        }

        mHasDrop = false;
        SendFinished(true);
    }

    // Backward-compatible alias
    using NkXLibDropImpl = NkXLibDropTarget;

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_LINUX && NKENTSEU_WINDOWING_XLIB
