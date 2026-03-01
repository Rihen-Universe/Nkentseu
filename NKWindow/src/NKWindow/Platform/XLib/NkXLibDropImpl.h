#pragma once

// =============================================================================
// NkXLibDropImpl.h  —  Drag & Drop XDND v5 pour XLib (API pure XLib)
//
// Protocole XDND v5 — aucune dépendance XCB.
// Toutes les opérations passent par :
//   XInternAtom / XChangeProperty / XGetWindowProperty
//   XConvertSelection / XSendEvent / XFlush
//
// Types MIME supportés :
//   text/uri-list               → fichiers
//   text/plain;charset=utf-8   → texte UTF-8
//   text/plain                  → texte fallback
//
// Intégration dans NkXLibEventImpl::PollEvents() :
//   case ClientMessage    : dropImpl.HandleClientMessage(xev.xclient);
//   case SelectionNotify  : dropImpl.HandleSelectionNotify(xev.xselection);
// =============================================================================

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <cstdio>
#include <cstring>
#include "NKWindow/Events/NkDropEvent.h"

namespace nkentseu {

    // =========================================================================
    // NkXLibDropImpl
    // =========================================================================

    class NkXLibDropImpl {
    public:
        using DropFilesCallback = std::function<void(const NkDropFileData &)>;
        using DropTextCallback  = std::function<void(const NkDropTextData &)>;
        using DropEnterCallback = std::function<void(const NkDropEnterData &)>;
        using DropLeaveCallback = std::function<void()>;

        NkXLibDropImpl(Display *display, ::Window window)
            : mDisplay(display), mWindow(window)
        {
            InitAtoms();
            SetXdndAware();
        }

        void SetDropFilesCallback(DropFilesCallback cb) { mDropFiles = std::move(cb); }
        void SetDropTextCallback (DropTextCallback  cb) { mDropText  = std::move(cb); }
        void SetDropEnterCallback(DropEnterCallback cb) { mDropEnter = std::move(cb); }
        void SetDropLeaveCallback(DropLeaveCallback cb) { mDropLeave = std::move(cb); }

        /// Appeler depuis PollEvents() sur xev.type == ClientMessage
        void HandleClientMessage(const XClientMessageEvent &ev);

        /// Appeler depuis PollEvents() sur xev.type == SelectionNotify
        void HandleSelectionNotify(const XSelectionEvent &ev);

    private:
        Display  *mDisplay = nullptr;
        ::Window  mWindow  = 0;

        // Atoms XDND
        Atom aXdndAware     = 0;
        Atom aXdndEnter     = 0;
        Atom aXdndPosition  = 0;
        Atom aXdndDrop      = 0;
        Atom aXdndLeave     = 0;
        Atom aXdndFinished  = 0;
        Atom aXdndStatus    = 0;
        Atom aXdndSelection = 0;
        Atom aActionCopy    = 0;

        // Types MIME
        Atom aUriList       = 0;
        Atom aTextPlain     = 0;
        Atom aTextPlainUtf8 = 0;

        // État courant de la session DnD
        ::Window mSourceWin = 0;
        int32_t  mDragX     = 0;
        int32_t  mDragY     = 0;
        bool     mHasDrop   = false;

        DropFilesCallback mDropFiles;
        DropTextCallback  mDropText;
        DropEnterCallback mDropEnter;
        DropLeaveCallback mDropLeave;

        // -------------------------------------------------------------------
        // Helpers internes
        // -------------------------------------------------------------------

        Atom GetAtom(const char *name) const {
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
            XChangeProperty(mDisplay, mWindow, aXdndAware, XA_ATOM, 32,
                            PropModeReplace,
                            reinterpret_cast<unsigned char *>(&version), 1);
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
            XSendEvent(mDisplay, mSourceWin, False, NoEventMask,
                       reinterpret_cast<XEvent *>(&reply));
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
            XSendEvent(mDisplay, mSourceWin, False, NoEventMask,
                       reinterpret_cast<XEvent *>(&reply));
            XFlush(mDisplay);
        }

        void RequestSelection(Atom type) {
            XConvertSelection(mDisplay, aXdndSelection, type,
                              aXdndSelection, mWindow, CurrentTime);
            XFlush(mDisplay);
        }

        std::string ReadProperty(Atom property) const {
            Atom          actualType;
            int           actualFormat;
            unsigned long nItems    = 0;
            unsigned long bytesLeft = 0;
            unsigned char *data     = nullptr;

            XGetWindowProperty(mDisplay, mWindow, property,
                               0, 0x7FFFFFFF, True,
                               AnyPropertyType,
                               &actualType, &actualFormat,
                               &nItems, &bytesLeft, &data);

            std::string result;
            if (data) {
                result = std::string(reinterpret_cast<char *>(data), nItems);
                XFree(data);
            }
            return result;
        }

        static std::vector<std::string> ParseUriList(const std::string &raw) {
            std::vector<std::string> result;
            std::istringstream ss(raw);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty() || line[0] == '#') continue;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.substr(0, 7) == "file://") {
                    std::string path = line.substr(7);
                    std::string decoded;
                    decoded.reserve(path.size());
                    for (size_t i = 0; i < path.size(); ++i) {
                        if (path[i] == '%' && i + 2 < path.size()) {
                            int v = 0;
                            sscanf(path.c_str() + i + 1, "%2x", &v);
                            decoded += static_cast<char>(v);
                            i += 2;
                        } else {
                            decoded += path[i];
                        }
                    }
                    result.push_back(decoded);
                }
            }
            return result;
        }
    };

    // =========================================================================
    // Implémentation inline
    // =========================================================================

    inline void NkXLibDropImpl::HandleClientMessage(const XClientMessageEvent &ev) {
        Atom type = ev.message_type;

        if (type == aXdndEnter) {
            mSourceWin = static_cast<::Window>(ev.data.l[0]);
            NkDropEnterData d{};
            d.dropType = NkDropType::NK_DROP_TYPE_FILE;
            if (mDropEnter) mDropEnter(d);
            SendStatus(true);

        } else if (type == aXdndPosition) {
            mSourceWin = static_cast<::Window>(ev.data.l[0]);
            unsigned long coords = static_cast<unsigned long>(ev.data.l[2]);
            mDragX = static_cast<int32_t>((coords >> 16) & 0xFFFF);
            mDragY = static_cast<int32_t>( coords         & 0xFFFF);
            SendStatus(true);

        } else if (type == aXdndLeave) {
            if (mDropLeave) mDropLeave();

        } else if (type == aXdndDrop) {
            mSourceWin = static_cast<::Window>(ev.data.l[0]);
            mHasDrop   = true;
            RequestSelection(aUriList);
        }
    }

    inline void NkXLibDropImpl::HandleSelectionNotify(const XSelectionEvent &ev) {
        if (!mHasDrop || ev.property == None) {
            SendFinished(false);
            return;
        }

        std::string data = ReadProperty(ev.property);

        if (ev.target == aUriList) {
            auto paths = ParseUriList(data);
            if (!paths.empty() && mDropFiles) {
                NkDropFileData fd{};
                fd.x        = mDragX;
                fd.y        = mDragY;
                fd.numFiles = static_cast<NkU32>(paths.size());
                for (const auto &p : paths) {
                    NkDropFilePath fp{};
                    strncpy(fp.path, p.c_str(), sizeof(fp.path) - 1);
                    fd.files.push_back(fp);
                }
                mDropFiles(fd);
            }
        } else if (ev.target == aTextPlainUtf8 || ev.target == aTextPlain) {
            if (mDropText) {
                NkDropTextData td{};
                td.x    = mDragX;
                td.y    = mDragY;
                td.text = data;
                mDropText(td);
            }
        } else {
            // Fallback : texte UTF-8
            RequestSelection(aTextPlainUtf8);
            return;
        }

        mHasDrop = false;
        SendFinished(true);
    }

} // namespace nkentseu
