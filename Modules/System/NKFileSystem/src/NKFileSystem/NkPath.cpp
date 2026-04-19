// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkPath.cpp
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation de NkPath.
//   Gere normalisation des separateurs, decomposition et transformations.
// =============================================================================

#include "NKFileSystem/NkPath.h"

// -- En-tetes C standard ------------------------------------------------------
#include <cstdlib>

// -- En-tetes plateforme ------------------------------------------------------
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define getcwd _getcwd
    #ifdef GetCurrentDirectory
        #undef GetCurrentDirectory
    #endif
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
#else
    #include <unistd.h>
#endif

namespace nkentseu {

    // =========================================================================
    // Methodes privees
    // =========================================================================

    void NkPath::NormalizeSeparators() {
        for (usize i = 0; i < mPath.Length(); ++i) {
            if (mPath[i] == WINDOWS_SEPARATOR) {
                mPath[i] = PREFERRED_SEPARATOR;
            }
        }
    }

    // =========================================================================
    // Constructeurs
    // =========================================================================

    NkPath::NkPath()
        : mPath() {
    }

    NkPath::NkPath(const char* path)
        : mPath(path ? path : "") {
        NormalizeSeparators();
    }

    NkPath::NkPath(const NkString& path)
        : mPath(path) {
        NormalizeSeparators();
    }

    NkPath::NkPath(const NkPath& other)
        : mPath(other.mPath) {
    }

    // =========================================================================
    // Affectation
    // =========================================================================

    NkPath& NkPath::operator=(const NkPath& other) {
        if (this != &other) {
            mPath = other.mPath;
        }
        return *this;
    }

    NkPath& NkPath::operator=(const char* path) {
        mPath = (path ? path : "");
        NormalizeSeparators();
        return *this;
    }

    // =========================================================================
    // Jointure
    // =========================================================================

    NkPath& NkPath::Append(const char* component) {
        if (!component || !component[0]) {
            return *this;
        }

        if (mPath.Empty()) {
            mPath = component;
            NormalizeSeparators();
            return *this;
        }

        if (mPath[mPath.Length() - 1] != PREFERRED_SEPARATOR) {
            mPath += PREFERRED_SEPARATOR;
        }

        mPath += component;
        NormalizeSeparators();
        return *this;
    }

    NkPath& NkPath::Append(const NkPath& other) {
        return Append(other.CStr());
    }

    NkPath NkPath::operator/(const char* component) const {
        NkPath result = *this;
        result.Append(component);
        return result;
    }

    NkPath NkPath::operator/(const NkPath& other) const {
        return *this / other.CStr();
    }

    // =========================================================================
    // Decomposition
    // =========================================================================

    NkString NkPath::GetDirectory() const {
        const char* path = mPath.CStr();
        const char* lastSep = nullptr;

        for (const char* p = path; *p; ++p) {
            if (*p == PREFERRED_SEPARATOR || *p == WINDOWS_SEPARATOR) {
                lastSep = p;
            }
        }

        if (!lastSep) {
            return NkString();
        }

        return NkString(path, static_cast<usize>(lastSep - path));
    }

    NkString NkPath::GetFileName() const {
        const char* path = mPath.CStr();
        const char* lastSep = nullptr;

        for (const char* p = path; *p; ++p) {
            if (*p == PREFERRED_SEPARATOR || *p == WINDOWS_SEPARATOR) {
                lastSep = p;
            }
        }

        if (!lastSep) {
            return mPath;
        }

        return NkString(lastSep + 1);
    }

    NkString NkPath::GetFileNameWithoutExtension() const {
        NkString filename = GetFileName();
        const char* name = filename.CStr();
        const char* lastDot = nullptr;

        for (const char* p = name; *p; ++p) {
            if (*p == '.') {
                lastDot = p;
            }
        }

        if (lastDot && lastDot != name) {
            return NkString(name, static_cast<usize>(lastDot - name));
        }

        return filename;
    }

    NkString NkPath::GetExtension() const {
        NkString filename = GetFileName();
        const char* name = filename.CStr();
        const char* lastDot = nullptr;

        for (const char* p = name; *p; ++p) {
            if (*p == '.') {
                lastDot = p;
            }
        }

        if (lastDot && lastDot != name) {
            return NkString(lastDot);
        }

        return NkString();
    }

    NkString NkPath::GetRoot() const {
        const char* path = mPath.CStr();

        if (path[0] && path[1] == ':') {
            char root[4] = {
                path[0],
                ':',
                PREFERRED_SEPARATOR,
                '\0'
            };
            return NkString(root);
        }

        if (path[0] == PREFERRED_SEPARATOR) {
            return NkString("/");
        }

        return NkString();
    }

    // =========================================================================
    // Proprietes
    // =========================================================================

    bool NkPath::IsAbsolute() const {
        const char* path = mPath.CStr();

        if (path[0] && path[1] == ':') {
            return true;
        }

        if (path[0] == PREFERRED_SEPARATOR || path[0] == WINDOWS_SEPARATOR) {
            return true;
        }

        return false;
    }

    bool NkPath::IsRelative() const {
        return !IsAbsolute();
    }

    bool NkPath::HasExtension() const {
        return !GetExtension().Empty();
    }

    bool NkPath::HasFileName() const {
        return !GetFileName().Empty();
    }

    // =========================================================================
    // Modification
    // =========================================================================

    NkPath& NkPath::ReplaceExtension(const char* newExt) {
        NkString dir = GetDirectory();
        NkString name = GetFileNameWithoutExtension();

        mPath = dir;

        if (!dir.Empty()) {
            mPath += PREFERRED_SEPARATOR;
        }

        mPath += name;

        if (newExt && newExt[0]) {
            if (newExt[0] != '.') {
                mPath += '.';
            }
            mPath += newExt;
        }

        return *this;
    }

    NkPath& NkPath::ReplaceFileName(const char* newName) {
        NkString dir = GetDirectory();

        mPath = dir;

        if (!dir.Empty()) {
            mPath += PREFERRED_SEPARATOR;
        }

        mPath += (newName ? newName : "");
        return *this;
    }

    NkPath& NkPath::RemoveFileName() {
        mPath = GetDirectory();
        return *this;
    }

    NkPath NkPath::GetParent() const {
        return NkPath(GetDirectory());
    }

    // =========================================================================
    // Conversion
    // =========================================================================

    const char* NkPath::CStr() const {
        return mPath.CStr();
    }

    NkString NkPath::ToString() const {
        return mPath;
    }

    NkString NkPath::ToNative() const {
        NkString result = mPath;

#ifdef _WIN32
        for (usize i = 0; i < result.Length(); ++i) {
            if (result[i] == PREFERRED_SEPARATOR) {
                result[i] = WINDOWS_SEPARATOR;
            }
        }
#endif

        return result;
    }

    // =========================================================================
    // Comparaison
    // =========================================================================

    bool NkPath::operator==(const NkPath& other) const {
        return mPath == other.mPath;
    }

    bool NkPath::operator!=(const NkPath& other) const {
        return !(*this == other);
    }

    // =========================================================================
    // Methodes statiques
    // =========================================================================

    NkPath NkPath::GetCurrentDirectory() {
        char buffer[4096];

        if (getcwd(buffer, sizeof(buffer))) {
            return NkPath(buffer);
        }

        return NkPath();
    }

    NkPath NkPath::GetTempDirectory() {
#ifdef _WIN32
        char buffer[MAX_PATH];

        if (GetTempPathA(MAX_PATH, buffer)) {
            return NkPath(buffer);
        }

        return NkPath("C:/Temp");
#else
        const char* tmpdir = getenv("TMPDIR");

        if (tmpdir) {
            return NkPath(tmpdir);
        }

        return NkPath("/tmp");
#endif
    }

    NkPath NkPath::Combine(const char* path1, const char* path2) {
        NkPath result(path1 ? path1 : "");
        result.Append(path2 ? path2 : "");
        return result;
    }

    bool NkPath::IsValidPath(const char* path) {
        if (!path || !path[0]) {
            return false;
        }

        for (const char* p = path; *p; ++p) {
            const char c = *p;

            if (c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*') {
                return false;
            }

            if (c < 32) {
                return false;
            }
        }

        return true;
    }

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
