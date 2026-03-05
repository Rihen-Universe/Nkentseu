// =============================================================================
// NkGamepadMappingPersistence.cpp
// Backend texte simple pour persistance des mappings gamepad.
// =============================================================================

#include "NkGamepadMappingPersistence.h"

#include <fstream>
#include <sstream>
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
        static std::string Trim(std::string s) {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }).base(), s.end());
            return s;
        }

        static bool DirectoryExists(const std::string& path) {
            if (path.empty()) return false;
#if defined(_WIN32)
            struct _stat st{};
            if (_stat(path.c_str(), &st) != 0) return false;
            return (st.st_mode & _S_IFDIR) != 0;
#else
            struct stat st{};
            if (stat(path.c_str(), &st) != 0) return false;
            return S_ISDIR(st.st_mode);
#endif
        }

        static bool CreateDirectorySingle(const std::string& path) {
            if (path.empty()) return false;
            if (DirectoryExists(path)) return true;
#if defined(_WIN32)
            if (_mkdir(path.c_str()) == 0) return true;
            return errno == EEXIST;
#else
            if (mkdir(path.c_str(), 0755) == 0) return true;
            return errno == EEXIST;
#endif
        }

        static bool EnsureDirectories(const std::string& inPath) {
            if (inPath.empty()) return false;

            std::string path = inPath;
            std::replace(path.begin(), path.end(), '\\', '/');

            std::string current;
            std::size_t cursor = 0;

            if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
                current = path.substr(0, 2);
                cursor = 2;
                if (cursor < path.size() && path[cursor] == '/') {
                    current.push_back('/');
                    ++cursor;
                }
            } else if (!path.empty() && path[0] == '/') {
                current = "/";
                cursor = 1;
            }

            std::size_t segmentStart = cursor;
            for (std::size_t i = cursor; i <= path.size(); ++i) {
                if (i < path.size() && path[i] != '/') continue;
                const std::string segment = path.substr(segmentStart, i - segmentStart);
                segmentStart = i + 1;
                if (segment.empty()) continue;

                if (!current.empty() && current.back() != '/') current.push_back('/');
                current += segment;

                if (!CreateDirectorySingle(current)) return false;
            }
            return true;
        }

        static std::string JoinPath(const std::string& a, const std::string& b) {
            if (a.empty()) return b;
            if (b.empty()) return a;
            const char last = a.back();
            if (last == '/' || last == '\\') return a + b;
            return a + "/" + b;
        }
    } // namespace

    NkTextGamepadMappingPersistence::NkTextGamepadMappingPersistence(std::string baseDirectory,
                                                                     std::string fileExtension)
        : mBaseDirectory(std::move(baseDirectory))
        , mExtension(std::move(fileExtension))
    {
        if (mBaseDirectory.empty()) mBaseDirectory = ResolveDefaultBaseDirectory();
        if (mExtension.empty()) mExtension = ".nkmap";
        if (!mExtension.empty() && mExtension[0] != '.') mExtension.insert(mExtension.begin(), '.');
    }

    std::string NkTextGamepadMappingPersistence::ResolveDefaultBaseDirectory() {
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

    std::string NkTextGamepadMappingPersistence::ResolveCurrentUserId() {
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

    std::string NkTextGamepadMappingPersistence::SanitizeUserId(const std::string& raw) {
        if (raw.empty()) return "default";
        std::string out;
        out.reserve(raw.size());
        for (char c : raw) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc) || c == '_' || c == '-' || c == '.') out.push_back(c);
            else out.push_back('_');
        }
        if (out.empty()) out = "default";
        return out;
    }

    std::string NkTextGamepadMappingPersistence::BuildUserFilePath(const std::string& userId) const {
        const std::string id = SanitizeUserId(userId.empty() ? ResolveCurrentUserId() : userId);
        return JoinPath(mBaseDirectory, id + mExtension);
    }

    bool NkTextGamepadMappingPersistence::Save(const std::string& userId,
                                               const NkGamepadMappingProfileData& profile,
                                               std::string* outError)
    {
        if (!EnsureDirectories(mBaseDirectory)) {
            if (outError) *outError = "Cannot create mapping directory: " + mBaseDirectory;
            return false;
        }

        const std::string filePath = BuildUserFilePath(userId);
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            if (outError) *outError = "Cannot open mapping file for write: " + filePath;
            return false;
        }

        out << "nkmap " << profile.version << "\n";
        out << "backend " << profile.backendName << "\n";

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

    bool NkTextGamepadMappingPersistence::Load(const std::string& userId,
                                               NkGamepadMappingProfileData& outProfile,
                                               std::string* outError)
    {
        const std::string filePath = BuildUserFilePath(userId);
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) {
            if (outError) *outError = "Cannot open mapping file for read: " + filePath;
            return false;
        }

        outProfile = {};

        std::string header;
        NkU32 version = 0;
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
                outProfile.backendName = Trim(rest);
            } else if (token == "slot") {
                NkGamepadMappingSlotData slot{};
                int active = 0;
                if (!(in >> slot.slotIndex >> active)) {
                    if (outError) *outError = "Invalid slot entry in mapping file: " + filePath;
                    return false;
                }
                slot.active = (active != 0);
                outProfile.slots.push_back(slot);
                currentSlot = &outProfile.slots.back();
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
                currentSlot->buttons.push_back(e);
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
                currentSlot->axes.push_back(e);
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
