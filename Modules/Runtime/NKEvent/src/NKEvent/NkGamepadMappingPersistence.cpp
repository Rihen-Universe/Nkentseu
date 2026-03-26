// =============================================================================
// NkGamepadMappingPersistence.cpp
// Backend texte simple pour persistance des mappings gamepad.
// =============================================================================

#include "NkGamepadMappingPersistence.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include "NKCore/NkTraits.h"
#include "NKContainers/String/Encoding/NkASCII.h"

#if defined(_WIN32)
#   include <direct.h>
#   include <sys/stat.h>
#else
#   include <sys/stat.h>
#   include <sys/types.h>
#endif

namespace nkentseu {

    namespace {
        static void NormalizePathSeparators(NkString& path) {
            for (NkString::SizeType i = 0; i < path.Size(); ++i) {
                if (path[i] == '\\') {
                    path[i] = '/';
                }
            }
        }

        static bool IsAsciiSpace(const char c) {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        static bool ReadTextLine(FILE* file, NkString& outLine) {
            outLine.Clear();
            if (!file) return false;

            int ch = 0;
            while ((ch = ::fgetc(file)) != EOF) {
                const char c = static_cast<char>(ch);
                if (c == '\r') continue;
                if (c == '\n') break;
                outLine.PushBack(c);
            }

            return ch != EOF || !outLine.Empty();
        }

        static void TokenizeWhitespace(const NkString& line, NkVector<NkString>& outTokens) {
            outTokens.Clear();
            const NkString::SizeType size = line.Size();
            NkString::SizeType i = 0;

            while (i < size) {
                while (i < size && IsAsciiSpace(line[i])) ++i;
                if (i >= size) break;

                const NkString::SizeType start = i;
                while (i < size && !IsAsciiSpace(line[i])) ++i;
                outTokens.PushBack(line.SubStr(start, i - start));
            }
        }

        static NkString SliceAfterFirstToken(const NkString& line) {
            const NkString::SizeType size = line.Size();
            NkString::SizeType i = 0;

            while (i < size && !IsAsciiSpace(line[i])) ++i;
            while (i < size && IsAsciiSpace(line[i])) ++i;

            if (i >= size) return {};
            NkString out = line.SubStr(i);
            out.Trim();
            return out;
        }

        static bool ParseUIntToken(const NkString& token, uint32& outValue) {
            return token.ToUInt(outValue);
        }

        static bool ParseIntToken(const NkString& token, int32& outValue) {
            return token.ToInt(outValue);
        }

        static bool ParseFloatToken(const NkString& token, float32& outValue) {
            return token.ToFloat(outValue);
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
            NormalizePathSeparators(path);

            NkString current;
            NkString::SizeType cursor = 0;

            if (path.Size() >= 2 && encoding::ascii::NkIsAlpha(path[0]) && path[1] == ':') {
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
        : mBaseDirectory(traits::NkMove(baseDirectory))
        , mExtension(traits::NkMove(fileExtension))
    {
        if (mBaseDirectory.Empty()) mBaseDirectory = ResolveDefaultBaseDirectory();
        if (mExtension.Empty()) mExtension = ".nkmap";
        if (!mExtension.Empty() && mExtension[0] != '.') mExtension.Insert(0, 1, '.');
    }

    NkString NkTextGamepadMappingPersistence::ResolveDefaultBaseDirectory() {
        if (const char* env = ::getenv("NKENTSEU_GAMEPAD_MAPPING_DIR")) {
            if (*env) return env;
        }

#if defined(_WIN32)
        const char* base = ::getenv("LOCALAPPDATA");
        if (!base || !*base) base = ::getenv("APPDATA");
        if (base && *base) {
            return JoinPath(JoinPath(JoinPath(base, "Nkentseu"), "NKWindow"), "GamepadMappings");
        }
#else
        if (const char* home = ::getenv("HOME")) {
            if (*home) {
                return JoinPath(JoinPath(home, ".config"), "nkentseu/nkwindow/gamepad_mappings");
            }
        }
#endif

        return "Build/GamepadMappings";
    }

    NkString NkTextGamepadMappingPersistence::ResolveCurrentUserId() {
        if (const char* env = ::getenv("NKENTSEU_GAMEPAD_USER")) {
            if (*env) return SanitizeUserId(env);
        }
#if defined(_WIN32)
        if (const char* user = ::getenv("USERNAME")) {
            if (*user) return SanitizeUserId(user);
        }
#else
        if (const char* user = ::getenv("USER")) {
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
            if (encoding::ascii::NkIsAlphaNumeric(c) || c == '_' || c == '-' || c == '.') out.PushBack(c);
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
        FILE* out = ::fopen(filePath.CStr(), "wb");
        if (!out) {
            if (outError) *outError = "Cannot open mapping file for write: " + filePath;
            return false;
        }

        if (::fprintf(out, "nkmap %u\n", static_cast<unsigned>(profile.version)) < 0 ||
            ::fprintf(out, "backend %s\n", profile.backendName.CStr()) < 0)
        {
            ::fclose(out);
            if (outError) *outError = "Write failed for mapping file: " + filePath;
            return false;
        }

        for (const NkGamepadMappingSlotData& slot : profile.slots) {
            if (::fprintf(out, "slot %u %d\n",
                          static_cast<unsigned>(slot.slotIndex),
                          slot.active ? 1 : 0) < 0)
            {
                ::fclose(out);
                if (outError) *outError = "Write failed for mapping file: " + filePath;
                return false;
            }

            for (const NkGamepadButtonMapEntry& b : slot.buttons) {
                if (::fprintf(out, "button %u %u\n",
                              static_cast<unsigned>(b.physicalButton),
                              static_cast<unsigned>(b.logicalButton)) < 0)
                {
                    ::fclose(out);
                    if (outError) *outError = "Write failed for mapping file: " + filePath;
                    return false;
                }
            }

            for (const NkGamepadAxisMapEntry& a : slot.axes) {
                if (::fprintf(out, "axis %u %u %.9g %d\n",
                              static_cast<unsigned>(a.physicalAxis),
                              static_cast<unsigned>(a.logicalAxis),
                              static_cast<double>(a.scale),
                              a.invert ? 1 : 0) < 0)
                {
                    ::fclose(out);
                    if (outError) *outError = "Write failed for mapping file: " + filePath;
                    return false;
                }
            }

            if (::fprintf(out, "end_slot\n") < 0) {
                ::fclose(out);
                if (outError) *outError = "Write failed for mapping file: " + filePath;
                return false;
            }
        }

        if (::fflush(out) != 0 || ::fclose(out) != 0) {
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
        FILE* in = ::fopen(filePath.CStr(), "rb");
        if (!in) {
            if (outError) *outError = "Cannot open mapping file for read: " + filePath;
            return false;
        }

        outProfile = {};

        NkString line;
        if (!ReadTextLine(in, line)) {
            ::fclose(in);
            if (outError) *outError = "Invalid mapping file header: " + filePath;
            return false;
        }
        line.Trim();

        NkVector<NkString> tokens;
        TokenizeWhitespace(line, tokens);
        uint32 version = 0;
        if (tokens.Size() < 2 || tokens[0].Compare("nkmap") != 0 || !ParseUIntToken(tokens[1], version)) {
            ::fclose(in);
            if (outError) *outError = "Invalid mapping file header: " + filePath;
            return false;
        }
        outProfile.version = version;

        NkGamepadMappingSlotData* currentSlot = nullptr;

        while (ReadTextLine(in, line)) {
            line.Trim();
            if (line.Empty()) continue;

            TokenizeWhitespace(line, tokens);
            if (tokens.Empty()) continue;

            const NkString& token = tokens[0];
            if (token == "backend") {
                outProfile.backendName = SliceAfterFirstToken(line);
            } else if (token == "slot") {
                if (tokens.Size() < 3) {
                    ::fclose(in);
                    if (outError) *outError = "Invalid slot entry in mapping file: " + filePath;
                    return false;
                }

                NkGamepadMappingSlotData slot{};
                int32 active = 0;
                if (!ParseUIntToken(tokens[1], slot.slotIndex) ||
                    !ParseIntToken(tokens[2], active))
                {
                    ::fclose(in);
                    if (outError) *outError = "Invalid slot entry in mapping file: " + filePath;
                    return false;
                }
                slot.active = (active != 0);
                outProfile.slots.PushBack(slot);
                currentSlot = &outProfile.slots.Back();
            } else if (token == "button") {
                if (!currentSlot) {
                    continue;
                }
                if (tokens.Size() < 3) {
                    ::fclose(in);
                    if (outError) *outError = "Invalid button entry in mapping file: " + filePath;
                    return false;
                }

                NkGamepadButtonMapEntry e{};
                if (!ParseUIntToken(tokens[1], e.physicalButton) ||
                    !ParseUIntToken(tokens[2], e.logicalButton))
                {
                    ::fclose(in);
                    if (outError) *outError = "Invalid button entry in mapping file: " + filePath;
                    return false;
                }
                currentSlot->buttons.PushBack(e);
            } else if (token == "axis") {
                if (!currentSlot) {
                    continue;
                }
                if (tokens.Size() < 5) {
                    ::fclose(in);
                    if (outError) *outError = "Invalid axis entry in mapping file: " + filePath;
                    return false;
                }

                NkGamepadAxisMapEntry e{};
                int32 invert = 0;
                if (!ParseUIntToken(tokens[1], e.physicalAxis) ||
                    !ParseUIntToken(tokens[2], e.logicalAxis) ||
                    !ParseFloatToken(tokens[3], e.scale) ||
                    !ParseIntToken(tokens[4], invert))
                {
                    ::fclose(in);
                    if (outError) *outError = "Invalid axis entry in mapping file: " + filePath;
                    return false;
                }
                e.invert = (invert != 0);
                currentSlot->axes.PushBack(e);
            } else if (token == "end_slot") {
                currentSlot = nullptr;
            }
        }

        ::fclose(in);
        return true;
    }

} // namespace nkentseu
