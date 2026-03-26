#pragma once

// =============================================================================
// NkDropEvent.h
// Événements Drag & Drop
//
// Classes d'événements :
//   - NkDropEnterEvent      — objet entrant dans la fenêtre
//   - NkDropOverEvent       — objet survolant la fenêtre
//   - NkDropLeaveEvent      — objet quittant la fenêtre
//   - NkDropFileEvent       — fichier(s) déposé(s)
//   - NkDropTextEvent       — texte déposé
//   - NkDropImageEvent      — image déposée
// =============================================================================

#include "NkEvent.h"
#include "NkEventState.h"
#include "NKContainers/String/NkStringUtils.h"
#include <string>
#include <vector>
#include <cstring>

namespace nkentseu {

    enum class NkDropType : uint32 {
        NK_DROP_TYPE_UNKNOWN = 0,
        NK_DROP_TYPE_FILE,           ///< Un ou plusieurs fichiers
        NK_DROP_TYPE_TEXT,           ///< Texte UTF-8
        NK_DROP_TYPE_IMAGE,          ///< Image brute / URI d'image
        NK_DROP_TYPE_URL,            ///< URL
        NK_DROP_TYPE_MAX
    };


    // ===========================================================================
    // NkDropEnterData — objet glissé entrant dans la fenêtre
    // ===========================================================================

    struct NkDropEnterData {
        int32      x = 0, y = 0;    ///< Position du curseur dans la zone client

        // Résumé de ce qui est proposé (avant l'acceptation)
        uint32 numFiles = 0;          ///< Nombre de fichiers
        bool  hasText  = false;      ///< Contient du texte
        bool  hasImage = false;      ///< Contient une image

        NkString ToString() const {
            NkString s = NkString::Fmt("DropEnter(at {0},{1}", x, y);
            if (numFiles) s += NkString::Fmt(" files={0}", numFiles);
            if (hasText)  s += " text";
            if (hasImage) s += " image";
            s += ")";
            return s;
        }
    };

    // ===========================================================================
    // NkDropEnterEvent
    // ===========================================================================

    class NkDropEnterEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_DROP_ENTER)

        NkDropEnterData data;

        explicit NkDropEnterEvent(const NkDropEnterData& d = {}, uint64 windowId = 0) noexcept
            : NkEvent(windowId), data(d) {}

        NkEvent* Clone() const override { return new NkDropEnterEvent(*this); }

        NkString ToString() const override {
            return "NkDropEnterEvent(" + data.ToString() + ")";
        }
    };

    // ===========================================================================
    // NkDropOverData — survol en cours
    // ===========================================================================

    struct NkDropOverData {
        int32 x = 0, y = 0;

        NkString ToString() const {
            return NkString::Fmt("DropOver({0},{1})", x, y);
        }
    };

    // ===========================================================================
    // NkDropOverEvent
    // ===========================================================================

    class NkDropOverEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_DROP_OVER)

        NkDropOverData data;

        explicit NkDropOverEvent(const NkDropOverData& d = {}, uint64 windowId = 0) noexcept
            : NkEvent(windowId), data(d) {}

        NkEvent* Clone() const override { return new NkDropOverEvent(*this); }

        NkString ToString() const override {
            return "NkDropOverEvent(" + data.ToString() + ")";
        }
    };

    // ===========================================================================
    // NkDropLeaveData — objet qui quitte la fenêtre
    // ===========================================================================

    struct NkDropLeaveData {
        NkString ToString() const { return "DropLeave"; }
    };

    // ===========================================================================
    // NkDropLeaveEvent
    // ===========================================================================

    class NkDropLeaveEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_DROP_LEAVE)

        explicit NkDropLeaveEvent(uint64 windowId = 0) noexcept : NkEvent(windowId) {}

        NkEvent* Clone() const override { return new NkDropLeaveEvent(*this); }

        NkString ToString() const override { return "NkDropLeaveEvent()"; }
    };

    // ===========================================================================
    // NkDropFilePath — un chemin de fichier déposé
    // ===========================================================================

    struct NkDropFilePath {
        char path[512] = {};

        NkDropFilePath() = default;
        explicit NkDropFilePath(const char* p) {
            if (p) strncpy(path, p, sizeof(path) - 1);
        }

        NkString ToString() const { return NkString(path); }
    };

    // ===========================================================================
    // NkDropFileData — fichier(s) déposé(s)
    // ===========================================================================

    struct NkDropFileData {
        int32 x = 0, y = 0;        ///< Position de dépose dans la zone client
        NkVector<NkString> paths;

        NkDropFileData() = default;

        void AddPath(const NkString& p) { paths.PushBack(p); }
        uint32 Count() const { return static_cast<uint32>(paths.size()); }

        NkString ToString() const {
            return NkString::Fmt("DropFile({0} file(s) at {1},{2})", Count(), x, y);
        }
    };

    // ===========================================================================
    // NkDropFileEvent
    // ===========================================================================

    class NkDropFileEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_DROP_FILE)

        NkDropFileData data;

        explicit NkDropFileEvent(const NkDropFileData& d = {}, uint64 windowId = 0) noexcept
            : NkEvent(windowId), data(d) {}

        NkEvent* Clone() const override { return new NkDropFileEvent(*this); }

        NkString ToString() const override {
            return "NkDropFileEvent(" + data.ToString() + ")";
        }
    };

    // ===========================================================================
    // NkDropTextData — texte déposé
    // ===========================================================================

    struct NkDropTextData {
        int32       x = 0, y = 0;
        NkString text;     ///< Texte en UTF-8
        NkString mimeType; ///< MIME type (ex : "text/plain", "text/html")

        NkString ToString() const {
            NkString preview = text.Size() > 40 ? text.SubStr(0, 40) : text;
            return "DropText(\"" + preview + (text.Size() > 40 ? "..." : "") + "\")";
        }
    };

    // ===========================================================================
    // NkDropTextEvent
    // ===========================================================================

    class NkDropTextEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_DROP_TEXT)

        NkDropTextData data;

        explicit NkDropTextEvent(const NkDropTextData& d = {}, uint64 windowId = 0) noexcept
            : NkEvent(windowId), data(d) {}

        NkEvent* Clone() const override { return new NkDropTextEvent(*this); }

        NkString ToString() const override {
            return "NkDropTextEvent(" + data.ToString() + ")";
        }
    };

    // ===========================================================================
    // NkDropImageData — image déposée
    // ===========================================================================

    struct NkDropImageData {
        int32       x = 0, y = 0;
        NkString sourceUri;  ///< URI de la source
        NkString mimeType;   ///< "image/png", "image/jpeg"…
        uint32       width  = 0;
        uint32       height = 0;
        NkVector<uint8> pixels;  ///< Données brutes (si disponibles) — RGBA8

        bool HasPixels() const { return !pixels.empty(); }

        NkString ToString() const {
            return NkString::Fmt("DropImage({0}x{1} {2})", width, height, mimeType);
        }
    };

    // ===========================================================================
    // NkDropImageEvent
    // ===========================================================================

    class NkDropImageEvent final : public NkEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_DROP_IMAGE)

            NkDropImageData data;

            explicit NkDropImageEvent(const NkDropImageData& d = {}, uint64 windowId = 0) noexcept
                : NkEvent(windowId), data(d) {}

            NkEvent* Clone() const override { return new NkDropImageEvent(*this); }

            NkString ToString() const override {
                return "NkDropImageEvent(" + data.ToString() + ")";
            }
    };

} // namespace nkentseu