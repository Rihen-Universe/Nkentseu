// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkPath.cpp
// DESCRIPTION: Path manipulation implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkPath.h"
#include <cstring>
#include <cstdlib>

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
    #include <limits.h>
#endif

namespace nkentseu {
    namespace entseu {
        
        void NkPath::NormalizeSeparators() {
            for (usize i = 0; i < mPath.Length(); ++i) {
                if (mPath[i] == WINDOWS_SEPARATOR) {
                    // Note: NkString needs to be mutable for this
                    // Assuming we have a way to modify characters
                }
            }
        }
        
        NkPath::NkPath() : mPath() {
        }
        
        NkPath::NkPath(const char* path) : mPath(path) {
            NormalizeSeparators();
        }
        
        NkPath::NkPath(const NkString& path) : mPath(path) {
            NormalizeSeparators();
        }
        
        NkPath::NkPath(const NkPath& other) : mPath(other.mPath) {
        }
        
        NkPath& NkPath::operator=(const NkPath& other) {
            if (this != &other) {
                mPath = other.mPath;
            }
            return *this;
        }
        
        NkPath& NkPath::operator=(const char* path) {
            mPath = path;
            NormalizeSeparators();
            return *this;
        }
        
        NkPath& NkPath::Append(const char* component) {
            if (mPath.Empty()) {
                mPath = component;
            } else {
                if (mPath[mPath.Length() - 1] != PREFERRED_SEPARATOR) {
                    mPath += PREFERRED_SEPARATOR;
                }
                mPath += component;
            }
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
        
        NkString NkPath::GetDirectory() const {
            const char* path = mPath.CStr();
            const char* lastSep = nullptr;
            
            for (const char* p = path; *p; ++p) {
                if (*p == PREFERRED_SEPARATOR || *p == WINDOWS_SEPARATOR) {
                    lastSep = p;
                }
            }
            
            if (lastSep) {
                return NkString(path, lastSep - path);
            }
            return NkString();
        }
        
        NkString NkPath::GetFileName() const {
            const char* path = mPath.CStr();
            const char* lastSep = nullptr;
            
            for (const char* p = path; *p; ++p) {
                if (*p == PREFERRED_SEPARATOR || *p == WINDOWS_SEPARATOR) {
                    lastSep = p;
                }
            }
            
            if (lastSep) {
                return NkString(lastSep + 1);
            }
            return mPath;
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
                return NkString(name, lastDot - name);
            }
            return filename;
        }
        
        NkString NkPath::GetExtension() const {
            const char* filename = GetFileName().CStr();
            const char* lastDot = nullptr;
            
            for (const char* p = filename; *p; ++p) {
                if (*p == '.') {
                    lastDot = p;
                }
            }
            
            if (lastDot && lastDot != filename) {
                return NkString(lastDot);
            }
            return NkString();
        }
        
        NkString NkPath::GetRoot() const {
            const char* path = mPath.CStr();
            
            // Windows drive root example: C:/ or C:\\.
            if (path[0] && path[1] == ':') {
                char root[4] = {path[0], ':', PREFERRED_SEPARATOR, '\0'};
                return NkString(root);
            }
            
            // Unix: /
            if (path[0] == PREFERRED_SEPARATOR) {
                return NkString("/");
            }
            
            return NkString();
        }
        
        bool NkPath::IsAbsolute() const {
            const char* path = mPath.CStr();
            
            // Windows: C:\\ or C:/
            if (path[0] && path[1] == ':') {
                return true;
            }
            
            // Unix: /
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
            mPath += newName;
            return *this;
        }
        
        NkPath& NkPath::RemoveFileName() {
            mPath = GetDirectory();
            return *this;
        }
        
        NkPath NkPath::GetParent() const {
            return NkPath(GetDirectory());
        }
        
        const char* NkPath::CStr() const {
            return mPath.CStr();
        }
        
        NkString NkPath::ToString() const {
            return mPath;
        }
        
        NkString NkPath::ToNative() const {
            NkString result = mPath;
            
            #ifdef _WIN32
            // Convert / to \ on Windows
            for (usize i = 0; i < result.Length(); ++i) {
                // Need mutable access to string
            }
            #endif
            
            return result;
        }
        
        bool NkPath::operator==(const NkPath& other) const {
            return mPath == other.mPath;
        }
        
        bool NkPath::operator!=(const NkPath& other) const {
            return !(*this == other);
        }
        
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
            if (tmpdir) return NkPath(tmpdir);
            return NkPath("/tmp");
            #endif
        }
        
        NkPath NkPath::Combine(const char* path1, const char* path2) {
            NkPath result(path1);
            result.Append(path2);
            return result;
        }
        
        bool NkPath::IsValidPath(const char* path) {
            if (!path || !path[0]) return false;
            
            // Check for invalid characters
            for (const char* p = path; *p; ++p) {
                char c = *p;
                // Invalid characters: < > " | ? *
                if (c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*') {
                    return false;
                }
                // Control characters
                if (c < 32) {
                    return false;
                }
            }
            
            return true;
        }
        
    } // namespace entseu
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
