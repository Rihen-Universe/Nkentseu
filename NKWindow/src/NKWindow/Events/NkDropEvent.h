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
#include <string>
#include <vector>
#include <cstring>

namespace nkentseu {

// ===========================================================================
// NkDropEnterData — objet glissé entrant dans la fenêtre
// ===========================================================================

struct NkDropEnterData {
    NkI32      x = 0, y = 0;    ///< Position du curseur dans la zone client

    // Résumé de ce qui est proposé (avant l'acceptation)
    NkU32 numFiles = 0;          ///< Nombre de fichiers
    bool  hasText  = false;      ///< Contient du texte
    bool  hasImage = false;      ///< Contient une image

    std::string ToString() const {
        std::string s = "DropEnter(at " + std::to_string(x) + "," + std::to_string(y);
        if (numFiles) s += " files=" + std::to_string(numFiles);
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

    explicit NkDropEnterEvent(const NkDropEnterData& d = {}, NkU64 windowId = 0) noexcept
        : NkEvent(windowId), data(d) {}

    NkEvent* Clone() const override { return new NkDropEnterEvent(*this); }

    std::string ToString() const override {
        return "NkDropEnterEvent(" + data.ToString() + ")";
    }
};

// ===========================================================================
// NkDropOverData — survol en cours
// ===========================================================================

struct NkDropOverData {
    NkI32 x = 0, y = 0;

    std::string ToString() const {
        return "DropOver(" + std::to_string(x) + "," + std::to_string(y) + ")";
    }
};

// ===========================================================================
// NkDropOverEvent
// ===========================================================================

class NkDropOverEvent final : public NkEvent {
public:
    NK_EVENT_TYPE_FLAGS(NK_DROP_OVER)

    NkDropOverData data;

    explicit NkDropOverEvent(const NkDropOverData& d = {}, NkU64 windowId = 0) noexcept
        : NkEvent(windowId), data(d) {}

    NkEvent* Clone() const override { return new NkDropOverEvent(*this); }

    std::string ToString() const override {
        return "NkDropOverEvent(" + data.ToString() + ")";
    }
};

// ===========================================================================
// NkDropLeaveData — objet qui quitte la fenêtre
// ===========================================================================

struct NkDropLeaveData {
    std::string ToString() const { return "DropLeave"; }
};

// ===========================================================================
// NkDropLeaveEvent
// ===========================================================================

class NkDropLeaveEvent final : public NkEvent {
public:
    NK_EVENT_TYPE_FLAGS(NK_DROP_LEAVE)

    explicit NkDropLeaveEvent(NkU64 windowId = 0) noexcept : NkEvent(windowId) {}

    NkEvent* Clone() const override { return new NkDropLeaveEvent(*this); }

    std::string ToString() const override { return "NkDropLeaveEvent()"; }
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

    std::string ToString() const { return std::string(path); }
};

// ===========================================================================
// NkDropFileData — fichier(s) déposé(s)
// ===========================================================================

struct NkDropFileData {
    NkI32 x = 0, y = 0;        ///< Position de dépose dans la zone client
    std::vector<std::string> paths;

    NkDropFileData() = default;

    void AddPath(const std::string& p) { paths.push_back(p); }
    NkU32 Count() const { return static_cast<NkU32>(paths.size()); }

    std::string ToString() const {
        return "DropFile(" + std::to_string(Count()) + " file(s) at "
             + std::to_string(x) + "," + std::to_string(y) + ")";
    }
};

// ===========================================================================
// NkDropFileEvent
// ===========================================================================

class NkDropFileEvent final : public NkEvent {
public:
    NK_EVENT_TYPE_FLAGS(NK_DROP_FILE)

    NkDropFileData data;

    explicit NkDropFileEvent(const NkDropFileData& d = {}, NkU64 windowId = 0) noexcept
        : NkEvent(windowId), data(d) {}

    NkEvent* Clone() const override { return new NkDropFileEvent(*this); }

    std::string ToString() const override {
        return "NkDropFileEvent(" + data.ToString() + ")";
    }
};

// ===========================================================================
// NkDropTextData — texte déposé
// ===========================================================================

struct NkDropTextData {
    NkI32       x = 0, y = 0;
    std::string text;     ///< Texte en UTF-8
    std::string mimeType; ///< MIME type (ex : "text/plain", "text/html")

    std::string ToString() const {
        std::string preview = text.size() > 40 ? text.substr(0, 40) : text;
        return "DropText(\"" + preview + (text.size() > 40 ? "..." : "") + "\")";
    }
};

// ===========================================================================
// NkDropTextEvent
// ===========================================================================

class NkDropTextEvent final : public NkEvent {
public:
    NK_EVENT_TYPE_FLAGS(NK_DROP_TEXT)

    NkDropTextData data;

    explicit NkDropTextEvent(const NkDropTextData& d = {}, NkU64 windowId = 0) noexcept
        : NkEvent(windowId), data(d) {}

    NkEvent* Clone() const override { return new NkDropTextEvent(*this); }

    std::string ToString() const override {
        return "NkDropTextEvent(" + data.ToString() + ")";
    }
};

// ===========================================================================
// NkDropImageData — image déposée
// ===========================================================================

struct NkDropImageData {
    NkI32       x = 0, y = 0;
    std::string sourceUri;  ///< URI de la source
    std::string mimeType;   ///< "image/png", "image/jpeg"…
    NkU32       width  = 0;
    NkU32       height = 0;
    std::vector<NkU8> pixels;  ///< Données brutes (si disponibles) — RGBA8

    bool HasPixels() const { return !pixels.empty(); }

    std::string ToString() const {
        return "DropImage(" + std::to_string(width) + "x" + std::to_string(height)
             + " " + mimeType + ")";
    }
};

// ===========================================================================
// NkDropImageEvent
// ===========================================================================

class NkDropImageEvent final : public NkEvent {
public:
    NK_EVENT_TYPE_FLAGS(NK_DROP_IMAGE)

    NkDropImageData data;

    explicit NkDropImageEvent(const NkDropImageData& d = {}, NkU64 windowId = 0) noexcept
        : NkEvent(windowId), data(d) {}

    NkEvent* Clone() const override { return new NkDropImageEvent(*this); }

    std::string ToString() const override {
        return "NkDropImageEvent(" + data.ToString() + ")";
    }
};

} // namespace nkentseu

