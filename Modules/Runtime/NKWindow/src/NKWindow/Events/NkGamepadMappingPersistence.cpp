// =============================================================================
// NkGamepadMappingPersistence.cpp
// Backend texte simple pour persistance des mappings gamepad.
// =============================================================================

#include "NkGamepadMappingPersistence.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>

#if defined(_WIN32)
#   include <direct.h>
#   include <sys/stat.h>
#else
#   include <sys/stat.h>
#   include <sys/types.h>
#endif

namespace nkentseu {

    namespace {
        static NkString Trim(NkString s) {
            s.Trim();
            return s;
        }

        static bool DirectoryExists(const NkString& path) {
            if (path.Empty()) return false;
#if defined(_WIN32)
            struct _stat st{};
            if (_stat(path.CStr(), &st) != 0) return false;
            return (st.st_mode & _S_IFDIR) != 0;
#else
            struct stat st{};
            if (stat(path.CStr(), &st) != 0) return false;
            return S_ISDIR(st.st_mode);
#endif
        }

        static bool CreateDirectorySingle(const NkString& path) {
            if (path.Empty()) return false;
            if (DirectoryExists(path)) return true;
#if defined(_WIN32)
            if (_mkdir(path.CStr()) == 0) return true;
            return errno == EEXIST;
#else
            if (mkdir(path.CStr(), 0755) == 0) return true;
            return errno == EEXIST;
#endif
        }

        static bool EnsureDirectories(const NkString& inPath) {
            if (inPath.Empty()) return false;

            NkString path = inPath;
            std::replace(path.begin(), path.end(), '\\', '/');

            NkString current;
            NkString::SizeType cursor = 0;

            if (path.Size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
                current = path.SubStr(0, 2);
                cursor = 2;
                if (cursor < path.Size() && path[cursor] == '/') {
                    current.PushBack('/');
                    ++cursor;
                }
            } else if (!path.Empty() && path[0] == '/') {
                current = "/";
                cursor = 1;
            }

            NkString::SizeType segmentStart = cursor;
            for (NkString::SizeType i = cursor; i <= path.Size(); ++i) {
                if (i < path.Size() && path[i] != '/') continue;
                const NkString segment = path.SubStr(segmentStart, i - segmentStart);
                segmentStart = i + 1;
                if (segment.Empty()) continue;

                if (!current.Empty() && current.Back() != '/') current.PushBack('/');
                current += segment;

                if (!CreateDirectorySingle(current)) return false;
            }
            return true;
        }

        static NkString JoinPath(const NkString& a, const NkString& b) {
            if (a.Empty()) return b;
            if (b.Empty()) return a;
            const char last = a.Back();
            if (last == '/' || last == '\\') return a + b;
            return a + "/" + b;
        }
    } // namespace

    NkTextGamepadMappingPersistence::NkTextGamepadMappingPersistence(NkString baseDirectory,
                                                                     NkString fileExtension)
        : mBaseDirectory(std::move(baseDirectory))
        , mExtension(std::move(fileExtension))
    {
        if (mBaseDirectory.Empty()) mBaseDirectory = ResolveDefaultBaseDirectory();
        if (mExtension.Empty()) mExtension = ".nkmap";
        if (!mExtension.Empty() && mExtension[0] != '.') mExtension.Insert(0, 1, '.');
    }

    NkString NkTextGamepadMappingPersistence::ResolveDefaultBaseDirectory() {
        if (const char* env = std::getenv("NKENTSEU_GAMEPAD_MAPPING_DIR")) {
            if (*env) return env;
        }

#if defined(_WIN32)
        const char* base = std::getenv("LOCALAPPDATA");
        if (!base || !*base) base = std::getenv("APPDATA");
        if (base && *base) {
            return JoinPath(JoinPath(JoinPath(base, "Nkentseu"), "NKWindow"), "GamepadMappings");
        }
#else
        if (const char* home = std::getenv("HOME")) {
            if (*home) {
                return JoinPath(JoinPath(home, ".config"), "nkentseu/nkwindow/gamepad_mappings");
            }
        }
#endif

        return "Build/GamepadMappings";
    }

    NkString NkTextGamepadMappingPersistence::ResolveCurrentUserId() {
        if (const char* env = std::getenv("NKENTSEU_GAMEPAD_USER")) {
            if (*env) return SanitizeUserId(env);
        }
#if defined(_WIN32)
        if (const char* user = std::getenv("USERNAME")) {
            if (*user) return SanitizeUserId(user);
        }
#else
        if (const char* user = std::getenv("USER")) {
            if (*user) return SanitizeUserId(user);
        }
#endif
        return "default";
    }

    NkString NkTextGamepadMappingPersistence::SanitizeUserId(const NkString& raw) {
        if (raw.Empty()) return "default";
        NkString out;
        out.Reserve(raw.Size());
        for (char c : raw) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc) || c == '_' || c == '-' || c == '.') out.PushBack(c);
            else out.PushBack('_');
        }
        if (out.Empty()) out = "default";
        return out;
    }

    NkString NkTextGamepadMappingPersistence::BuildUserFilePath(const NkString& userId) const {
        const NkString id = SanitizeUserId(userId.Empty() ? ResolveCurrentUserId() : userId);
        return JoinPath(mBaseDirectory, id + mExtension);
    }

    bool NkTextGamepadMappingPersistence::Save(const NkString& userId,
                                               const NkGamepadMappingProfileData& profile,
                                               NkString* outError)
    {
        if (!EnsureDirectories(mBaseDirectory)) {
            if (outError) *outError = "Cannot create mapping directory: " + mBaseDirectory;
            return false;
        }

        const NkString filePath = BuildUserFilePath(userId);
        std::ofstream out(filePath.CStr(), std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            if (outError) *outError = "Cannot open mapping file for write: " + filePath;
            return false;
        }

        out << "nkmap " << profile.version << "\n";
        out << "backend " << profile.backendName.CStr() << "\n";

        for (const NkGamepadMappingSlotData& slot : profile.slots) {
            out << "slot " << slot.slotIndex << " " << (slot.active ? 1 : 0) << "\n";
            for (const NkGamepadButtonMapEntry& b : slot.buttons) {
                out << "button " << b.physicalButton << " " << b.logicalButton << "\n";
            }
            for (const NkGamepadAxisMapEntry& a : slot.axes) {
                out << "axis " << a.physicalAxis << " " << a.logicalAxis
                    << " " << a.scale << " " << (a.invert ? 1 : 0) << "\n";
            }
            out << "end_slot\n";
        }

        if (!out.good()) {
            if (outError) *outError = "Write failed for mapping file: " + filePath;
            return false;
        }

        return true;
    }

    bool NkTextGamepadMappingPersistence::Load(const NkString& userId,
                                               NkGamepadMappingProfileData& outProfile,
                                               NkString* outError)
    {
        const NkString filePath = BuildUserFilePath(userId);
        std::ifstream in(filePath.CStr(), std::ios::binary);
        if (!in.is_open()) {
            if (outError) *outError = "Cannot open mapping file for read: " + filePath;
            return false;
        }

        outProfile = {};

        std::string header;
        uint32 version = 0;
        if (!(in >> header >> version) || header != "nkmap") {
            if (outError) *outError = "Invalid mapping file header: " + filePath;
            return false;
        }
        outProfile.version = version;

        std::string token;
        NkGamepadMappingSlotData* currentSlot = nullptr;

        while (in >> token) {
            if (token == "backend") {
                std::string rest;
                std::getline(in, rest);
                outProfile.backendName = Trim(rest.c_str());
            } else if (token == "slot") {
                NkGamepadMappingSlotData slot{};
                int active = 0;
                if (!(in >> slot.slotIndex >> active)) {
                    if (outError) *outError = "Invalid slot entry in mapping file: " + filePath;
                    return false;
                }
                slot.active = (active != 0);
                outProfile.slots.PushBack(slot);
                currentSlot = &outProfile.slots.Back();
            } else if (token == "button") {
                if (!currentSlot) {
                    std::string line;
                    std::getline(in, line);
                    continue;
                }
                NkGamepadButtonMapEntry e{};
                if (!(in >> e.physicalButton >> e.logicalButton)) {
                    if (outError) *outError = "Invalid button entry in mapping file: " + filePath;
                    return false;
                }
                currentSlot->buttons.PushBack(e);
            } else if (token == "axis") {
                if (!currentSlot) {
                    std::string line;
                    std::getline(in, line);
                    continue;
                }
                NkGamepadAxisMapEntry e{};
                int invert = 0;
                if (!(in >> e.physicalAxis >> e.logicalAxis >> e.scale >> invert)) {
                    if (outError) *outError = "Invalid axis entry in mapping file: " + filePath;
                    return false;
                }
                e.invert = (invert != 0);
                currentSlot->axes.PushBack(e);
            } else if (token == "end_slot") {
                currentSlot = nullptr;
            } else {
                std::string ignored;
                std::getline(in, ignored);
            }
        }

        return true;
    }

} // namespace nkentseu
