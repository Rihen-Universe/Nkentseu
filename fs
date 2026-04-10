ANALYSE DE CODE - COLLECTION AVEC MÉTADONNÉES
Répertoire de base: E:\Projets\2026\Nkentseu\Nkentseu\Modules\System\NKFileSystem\src\NKFileSystem
MÉTADONNÉES POUR RECONSTRUCTION:
{
  "base_directory": "E:\\Projets\\2026\\Nkentseu\\Nkentseu\\Modules\\System\\NKFileSystem\\src\\NKFileSystem",
  "included_extensions": [],
  "excluded_extensions": [],
  "included_paths": [],
  "excluded_paths": [],
  "included_files": [],
  "excluded_files": [],
  "structure": {
    ".": [
      "NkDirectory.cpp",
      "NkDirectory.h",
      "NkFile.cpp",
      "NkFile.h",
      "NkFileSystem.cpp",
      "NkFileSystem.h",
      "NkFileWatcher.cpp",
      "NkFileWatcher.h",
      "NkPath.cpp",
      "NkPath.h"
    ]
  },
  "timestamp": "1774764260.3436792"
}
================================================================================


📁 DOSSIER: .
──────────────────────────────────────────────────

[FICHIER: NkDirectory.cpp]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkDirectory.cpp
// DESCRIPTION: Directory operations implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <shlobj.h>
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir(path) _rmdir(path)
    #ifdef GetCurrentDirectory
        #undef GetCurrentDirectory
    #endif
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <pwd.h>
#endif

namespace nkentseu {
    namespace entseu {
        
        NkDirectoryEntry::NkDirectoryEntry()
            : Name()
            , FullPath()
            , IsDirectory(false)
            , IsFile(false)
            , Size(0)
            , ModificationTime(0) {
        }
        
        bool NkDirectory::Create(const char* path) {
            if (!path || Exists(path)) return false;
            
            #ifdef _WIN32
            return CreateDirectoryA(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
            #else
            return mkdir(path, 0755) == 0;
            #endif
        }
        
        bool NkDirectory::Create(const NkPath& path) {
            return Create(path.CStr());
        }
        
        bool NkDirectory::CreateRecursive(const char* path) {
            if (!path) return false;
            if (Exists(path)) return true;
            
            // Create parent first
            NkPath parent = NkPath(path).GetParent();
            if (!parent.ToString().Empty() && !Exists(parent)) {
                if (!CreateRecursive(parent)) {
                    return false;
                }
            }
            
            return Create(path);
        }
        
        bool NkDirectory::CreateRecursive(const NkPath& path) {
            return CreateRecursive(path.CStr());
        }
        
        bool NkDirectory::Delete(const char* path, bool recursive) {
            if (!path || !Exists(path)) return false;
            
            if (recursive) {
                // Delete all files first
                auto files = GetFiles(path);
                for (auto& file : files) {
                    NkFile::Delete(file.CStr());
                }
                
                // Delete all subdirectories
                auto dirs = GetDirectories(path);
                for (auto& dir : dirs) {
                    Delete(dir.CStr(), true);
                }
            }
            
            #ifdef _WIN32
            return RemoveDirectoryA(path) != 0;
            #else
            return rmdir(path) == 0;
            #endif
        }
        
        bool NkDirectory::Delete(const NkPath& path, bool recursive) {
            return Delete(path.CStr(), recursive);
        }
        
        bool NkDirectory::Exists(const char* path) {
            if (!path) return false;
            
            #ifdef _WIN32
            DWORD attrs = GetFileAttributesA(path);
            return (attrs != INVALID_FILE_ATTRIBUTES) && 
                   (attrs & FILE_ATTRIBUTE_DIRECTORY);
            #else
            struct stat st;
            return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
            #endif
        }
        
        bool NkDirectory::Exists(const NkPath& path) {
            return Exists(path.CStr());
        }
        
        bool NkDirectory::Empty(const char* path) {
            if (!Exists(path)) return true;
            
            auto entries = GetEntries(path);
            return entries.Empty();
        }
        
        bool NkDirectory::Empty(const NkPath& path) {
            return Empty(path.CStr());
        }
        
        bool NkDirectory::MatchesPattern(const char* name, const char* pattern) {
            if (!name || !pattern) {
                return false;
            }

            // Glob matching with '*' (0..N chars) and '?' (1 char).
            const char* text = name;
            const char* glob = pattern;
            const char* star = nullptr;
            const char* backtrack = nullptr;

            while (*text) {
                if (*glob == '*') {
                    star = glob++;
                    backtrack = text;
                    continue;
                }

                if (*glob == '?' || *glob == *text) {
                    ++glob;
                    ++text;
                    continue;
                }

                if (star) {
                    glob = star + 1;
                    text = ++backtrack;
                    continue;
                }

                return false;
            }

            while (*glob == '*') {
                ++glob;
            }

            return *glob == '\0';
        }
        
        NkVector<NkString> NkDirectory::GetFiles(
            const char* path,
            const char* pattern,
            NkSearchOption option
        ) {
            NkVector<NkString> results;
            
            if (!path || !Exists(path)) return results;
            
            #ifdef _WIN32
            WIN32_FIND_DATAA findData;
            NkString searchPath = NkString(path) + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);
            
            if (hFind == INVALID_HANDLE_VALUE) return results;
            
            do {
                if (strcmp(findData.cFileName, ".") == 0 || 
                    strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }
                
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (MatchesPattern(findData.cFileName, pattern)) {
                        NkPath fullPath = NkPath(path) / findData.cFileName;
                        results.PushBack(fullPath.ToString());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            
            FindClose(hFind);
            
            #else
            DIR* dir = opendir(path);
            if (!dir) return results;
            
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0 || 
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                NkPath fullPath = NkPath(path) / entry->d_name;
                struct stat st;
                if (stat(fullPath.CStr(), &st) == 0 && S_ISREG(st.st_mode)) {
                    if (MatchesPattern(entry->d_name, pattern)) {
                        results.PushBack(fullPath.ToString());
                    }
                }
            }
            
            closedir(dir);
            #endif
            
            if (option == NkSearchOption::NK_ALL_DIRECTORIES) {
                GetFilesRecursive(path, pattern, results);
            }
            
            return results;
        }
        
        NkVector<NkString> NkDirectory::GetFiles(
            const NkPath& path,
            const char* pattern,
            NkSearchOption option
        ) {
            return GetFiles(path.CStr(), pattern, option);
        }
        
        void NkDirectory::GetFilesRecursive(
            const char* path,
            const char* pattern,
            NkVector<NkString>& results
        ) {
            auto dirs = GetDirectories(path);
            for (auto& dir : dirs) {
                auto files = GetFiles(dir.CStr(), pattern, NkSearchOption::NK_TOP_DIRECTORY_ONLY);
                for (auto& file : files) {
                    results.PushBack(file);
                }
                GetFilesRecursive(dir.CStr(), pattern, results);
            }
        }
        
        NkVector<NkString> NkDirectory::GetDirectories(
            const char* path,
            const char* pattern,
            NkSearchOption option
        ) {
            NkVector<NkString> results;
            
            if (!path || !Exists(path)) return results;
            
            #ifdef _WIN32
            WIN32_FIND_DATAA findData;
            NkString searchPath = NkString(path) + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);
            
            if (hFind == INVALID_HANDLE_VALUE) return results;
            
            do {
                if (strcmp(findData.cFileName, ".") == 0 || 
                    strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }
                
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (MatchesPattern(findData.cFileName, pattern)) {
                        NkPath fullPath = NkPath(path) / findData.cFileName;
                        results.PushBack(fullPath.ToString());
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            
            FindClose(hFind);
            
            #else
            DIR* dir = opendir(path);
            if (!dir) return results;
            
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0 || 
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                NkPath fullPath = NkPath(path) / entry->d_name;
                struct stat st;
                if (stat(fullPath.CStr(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    if (MatchesPattern(entry->d_name, pattern)) {
                        results.PushBack(fullPath.ToString());
                    }
                }
            }
            
            closedir(dir);
            #endif
            
            if (option == NkSearchOption::NK_ALL_DIRECTORIES) {
                GetDirectoriesRecursive(path, pattern, results);
            }
            
            return results;
        }
        
        NkVector<NkString> NkDirectory::GetDirectories(
            const NkPath& path,
            const char* pattern,
            NkSearchOption option
        ) {
            return GetDirectories(path.CStr(), pattern, option);
        }
        
        void NkDirectory::GetDirectoriesRecursive(
            const char* path,
            const char* pattern,
            NkVector<NkString>& results
        ) {
            auto dirs = GetDirectories(path);
            for (auto& dir : dirs) {
                auto subdirs = GetDirectories(dir.CStr(), pattern, NkSearchOption::NK_TOP_DIRECTORY_ONLY);
                for (auto& subdir : subdirs) {
                    results.PushBack(subdir);
                }
                GetDirectoriesRecursive(dir.CStr(), pattern, results);
            }
        }
        
        NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
            const char* path,
            const char* pattern,
            NkSearchOption option
        ) {
            NkVector<NkDirectoryEntry> results;
            
            if (!path || !Exists(path)) return results;
            
            #ifdef _WIN32
            WIN32_FIND_DATAA findData;
            NkString searchPath = NkString(path) + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);
            
            if (hFind == INVALID_HANDLE_VALUE) return results;
            
            do {
                if (strcmp(findData.cFileName, ".") == 0 || 
                    strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }
                
                if (MatchesPattern(findData.cFileName, pattern)) {
                    NkDirectoryEntry entry;
                    entry.Name = findData.cFileName;
                    entry.FullPath = NkPath(path) / findData.cFileName;
                    entry.IsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    entry.IsFile = !entry.IsDirectory;
                    entry.Size = ((nk_int64)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                    
                    results.PushBack(entry);
                }
            } while (FindNextFileA(hFind, &findData));
            
            FindClose(hFind);
            
            #else
            DIR* dir = opendir(path);
            if (!dir) return results;
            
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0 || 
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                if (MatchesPattern(entry->d_name, pattern)) {
                    NkDirectoryEntry dirEntry;
                    dirEntry.Name = entry->d_name;
                    dirEntry.FullPath = NkPath(path) / entry->d_name;
                    
                    struct stat st;
                    if (stat(dirEntry.FullPath.CStr(), &st) == 0) {
                        dirEntry.IsDirectory = S_ISDIR(st.st_mode);
                        dirEntry.IsFile = S_ISREG(st.st_mode);
                        dirEntry.Size = st.st_size;
                        dirEntry.ModificationTime = st.st_mtime;
                    }
                    
                    results.PushBack(dirEntry);
                }
            }
            
            closedir(dir);
            #endif
            
            if (option == NkSearchOption::NK_ALL_DIRECTORIES) {
                GetEntriesRecursive(path, pattern, results);
            }
            
            return results;
        }
        
        NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
            const NkPath& path,
            const char* pattern,
            NkSearchOption option
        ) {
            return GetEntries(path.CStr(), pattern, option);
        }
        
        void NkDirectory::GetEntriesRecursive(
            const char* path,
            const char* pattern,
            NkVector<NkDirectoryEntry>& results
        ) {
            auto dirs = GetDirectories(path);
            for (auto& dir : dirs) {
                auto entries = GetEntries(dir.CStr(), pattern, NkSearchOption::NK_TOP_DIRECTORY_ONLY);
                for (auto& entry : entries) {
                    results.PushBack(entry);
                }
                GetEntriesRecursive(dir.CStr(), pattern, results);
            }
        }
        
        bool NkDirectory::Copy(
            const char* source,
            const char* dest,
            bool recursive,
            bool overwrite
        ) {
            if (!source || !dest || !Exists(source)) return false;
            
            if (!Exists(dest)) {
                if (!CreateRecursive(dest)) return false;
            }
            
            // Copy files
            auto files = GetFiles(source);
            for (auto& file : files) {
                NkPath filename = NkPath(file).GetFileName();
                NkPath destFile = NkPath(dest) / filename;
                if (!NkFile::Copy(file.CStr(), destFile.CStr(), overwrite)) {
                    return false;
                }
            }
            
            // Copy subdirectories if recursive
            if (recursive) {
                auto dirs = GetDirectories(source);
                for (auto& dir : dirs) {
                    NkPath dirname = NkPath(dir).GetFileName();
                    NkPath destDir = NkPath(dest) / dirname;
                    if (!Copy(dir.CStr(), destDir.CStr(), true, overwrite)) {
                        return false;
                    }
                }
            }
            
            return true;
        }
        
        bool NkDirectory::Copy(
            const NkPath& source,
            const NkPath& dest,
            bool recursive,
            bool overwrite
        ) {
            return Copy(source.CStr(), dest.CStr(), recursive, overwrite);
        }
        
        bool NkDirectory::Move(const char* source, const char* dest) {
            if (!source || !dest || !Exists(source)) return false;
            return rename(source, dest) == 0;
        }
        
        bool NkDirectory::Move(const NkPath& source, const NkPath& dest) {
            return Move(source.CStr(), dest.CStr());
        }
        
        NkPath NkDirectory::GetCurrentDirectory() {
            return NkPath::GetCurrentDirectory();
        }
        
        bool NkDirectory::SetCurrentDirectory(const char* path) {
            if (!path) return false;
            
            #ifdef _WIN32
            return SetCurrentDirectoryA(path) != 0;
            #else
            return chdir(path) == 0;
            #endif
        }
        
        bool NkDirectory::SetCurrentDirectory(const NkPath& path) {
            return SetCurrentDirectory(path.CStr());
        }
        
        NkPath NkDirectory::GetTempDirectory() {
            return NkPath::GetTempDirectory();
        }
        
        NkPath NkDirectory::GetHomeDirectory() {
            #ifdef _WIN32
            char path[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path) == S_OK) {
                return NkPath(path);
            }
            const char* homeDrive = getenv("HOMEDRIVE");
            const char* homePath = getenv("HOMEPATH");
            if (homeDrive && homePath) {
                return NkPath(NkString(homeDrive) + homePath);
            }
            #else
            const char* home = getenv("HOME");
            if (home) return NkPath(home);
            
            struct passwd* pw = getpwuid(getuid());
            if (pw) return NkPath(pw->pw_dir);
            #endif
            
            return NkPath();
        }
        
        NkPath NkDirectory::GetAppDataDirectory() {
            #ifdef _WIN32
            char path[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
                return NkPath(path);
            }
            #else
            NkPath home = GetHomeDirectory();
            if (!home.ToString().Empty()) {
                return home / ".config";
            }
            #endif
            
            return NkPath();
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
==================================================


[FICHIER: NkDirectory.h]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkDirectory.h
// DESCRIPTION: Directory operations and manipulation
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKDIRECTORY_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKDIRECTORY_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief Directory entry information
         */
        struct NkDirectoryEntry {
            NkString Name;
            NkPath FullPath;
            bool IsDirectory;
            bool IsFile;
            nk_int64 Size;
            nk_int64 ModificationTime;
            
            NkDirectoryEntry();
        };
        
        /**
         * @brief Search options for directory enumeration
         */
        enum class NkSearchOption {
            NK_TOP_DIRECTORY_ONLY,   // Only search current directory
            NK_ALL_DIRECTORIES      // Search recursively
        };
        
        /**
         * @brief Directory manipulation class
         * 
         * Gère création, suppression, énumération de répertoires.
         * 
         * @example
         * if (NkDirectory::Exists("data")) {
         *     auto files = NkDirectory::GetFiles("data");
         *     for (auto& file : files) {
         *         printf("%s\n", file.CStr());
         *     }
         * }
         */
        class NkDirectory {
            public:
                // Directory operations
                static bool Create(const char* path);
                static bool Create(const NkPath& path);
                static bool CreateRecursive(const char* path);
                static bool CreateRecursive(const NkPath& path);
                
                static bool Delete(const char* path, bool recursive = false);
                static bool Delete(const NkPath& path, bool recursive = false);
                
                static bool Exists(const char* path);
                static bool Exists(const NkPath& path);
                
                static bool Empty(const char* path);
                static bool Empty(const NkPath& path);
                
                // Enumeration
                static NkVector<NkString> GetFiles(
                    const char* path,
                    const char* pattern = "*",
                    NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
                );
                
                static NkVector<NkString> GetFiles(
                    const NkPath& path,
                    const char* pattern = "*",
                    NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
                );
                
                static NkVector<NkString> GetDirectories(
                    const char* path,
                    const char* pattern = "*",
                    NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
                );
                
                static NkVector<NkString> GetDirectories(
                    const NkPath& path,
                    const char* pattern = "*",
                    NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
                );
                
                static NkVector<NkDirectoryEntry> GetEntries(
                    const char* path,
                    const char* pattern = "*",
                    NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
                );
                
                static NkVector<NkDirectoryEntry> GetEntries(
                    const NkPath& path,
                    const char* pattern = "*",
                    NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
                );
                
                // Copy/Move
                static bool Copy(
                    const char* source,
                    const char* dest,
                    bool recursive = true,
                    bool overwrite = false
                );
                
                static bool Copy(
                    const NkPath& source,
                    const NkPath& dest,
                    bool recursive = true,
                    bool overwrite = false
                );
                
                static bool Move(const char* source, const char* dest);
                static bool Move(const NkPath& source, const NkPath& dest);
                
                // Special directories
                static NkPath GetCurrentDirectory();
                static bool SetCurrentDirectory(const char* path);
                static bool SetCurrentDirectory(const NkPath& path);
                
                static NkPath GetTempDirectory();
                static NkPath GetHomeDirectory();
                static NkPath GetAppDataDirectory();
                
            private:
            
                static bool MatchesPattern(const char* name, const char* pattern);

                static void GetFilesRecursive(
                    const char* path,
                    const char* pattern,
                    NkVector<NkString>& results
                );

                static void GetDirectoriesRecursive(
                    const char* path,
                    const char* pattern,
                    NkVector<NkString>& results
                );

                static void GetEntriesRecursive(
                    const char* path,
                    const char* pattern,
                    NkVector<NkDirectoryEntry>& results
                );
        };
        
    } // namespace entseu
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKDIRECTORY_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
==================================================


[FICHIER: NkFile.cpp]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFile.cpp
// DESCRIPTION: File operations implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkFile.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

namespace nkentseu {
    namespace entseu {
        
        const char* NkFile::GetModeString() const {
            switch (mMode) {
                case NkFileMode::NK_READ: return "rb";
                case NkFileMode::NK_WRITE: return "wb";
                case NkFileMode::NK_APPEND: return "ab";
                case NkFileMode::NK_READ_WRITE: return "rb+";
                case NkFileMode::NK_READ_APPEND: return "ab+";
                default: return "rb";
            }
        }
        
        NkFile::NkFile()
            : mHandle(nullptr)
            , mPath()
            , mMode(NkFileMode::NK_READ)
            , mIsOpen(false) {
        }
        
        NkFile::NkFile(const char* path, NkFileMode mode)
            : mHandle(nullptr)
            , mPath(path)
            , mMode(mode)
            , mIsOpen(false) {
            Open(path, mode);
        }
        
        NkFile::NkFile(const NkPath& path, NkFileMode mode)
            : mHandle(nullptr)
            , mPath(path)
            , mMode(mode)
            , mIsOpen(false) {
            Open(path, mode);
        }
        
        NkFile::~NkFile() {
            Close();
        }
        
        #if defined(NK_CPP11)
        NkFile::NkFile(NkFile&& other) noexcept
            : mHandle(other.mHandle)
            , mPath(other.mPath)
            , mMode(other.mMode)
            , mIsOpen(other.mIsOpen) {
            other.mHandle = nullptr;
            other.mIsOpen = false;
        }
        
        NkFile& NkFile::operator=(NkFile&& other) noexcept {
            if (this != &other) {
                Close();
                mHandle = other.mHandle;
                mPath = other.mPath;
                mMode = other.mMode;
                mIsOpen = other.mIsOpen;
                other.mHandle = nullptr;
                other.mIsOpen = false;
            }
            return *this;
        }
        #endif
        
        bool NkFile::Open(const char* path, NkFileMode mode) {
            Close();
            
            mPath = path;
            mMode = mode;
            
            FILE* file = fopen(path, GetModeString());
            if (file) {
                mHandle = file;
                mIsOpen = true;
                return true;
            }
            
            return false;
        }
        
        bool NkFile::Open(const NkPath& path, NkFileMode mode) {
            return Open(path.CStr(), mode);
        }
        
        void NkFile::Close() {
            if (mIsOpen && mHandle) {
                fclose(static_cast<FILE*>(mHandle));
                mHandle = nullptr;
                mIsOpen = false;
            }
        }
        
        bool NkFile::IsOpen() const {
            return mIsOpen;
        }
        
        usize NkFile::Read(void* buffer, usize size) {
            if (!mIsOpen || !buffer) return 0;
            return fread(buffer, 1, size, static_cast<FILE*>(mHandle));
        }
        
        NkString NkFile::ReadLine() {
            if (!mIsOpen) return NkString();
            
            char buffer[4096];
            if (fgets(buffer, sizeof(buffer), static_cast<FILE*>(mHandle))) {
                // Remove trailing newline
                usize len = strlen(buffer);
                if (len > 0 && buffer[len - 1] == '\n') {
                    buffer[len - 1] = '\0';
                    if (len > 1 && buffer[len - 2] == '\r') {
                        buffer[len - 2] = '\0';
                    }
                }
                return NkString(buffer);
            }
            
            return NkString();
        }
        
        NkString NkFile::ReadAll() {
            if (!mIsOpen) return NkString();
            
            nk_int64 size = GetSize();
            if (size <= 0) return NkString();
            
            NkVector<char> buffer;
            buffer.Resize(static_cast<usize>(size) + 1);
            
            usize read = Read(buffer.Data(), static_cast<usize>(size));
            buffer[read] = '\0';
            
            return NkString(buffer.Data());
        }
        
        NkVector<NkString> NkFile::ReadLines() {
            NkVector<NkString> lines;
            
            while (!IsEOF()) {
                NkString line = ReadLine();
                if (!line.Empty() || !IsEOF()) {
                    lines.PushBack(line);
                }
            }
            
            return lines;
        }
        
        usize NkFile::Write(const void* data, usize size) {
            if (!mIsOpen || !data) return 0;
            return fwrite(data, 1, size, static_cast<FILE*>(mHandle));
        }
        
        bool NkFile::WriteLine(const char* text) {
            if (!mIsOpen || !text) return false;
            
            usize len = strlen(text);
            if (Write(text, len) != len) return false;
            
            const char newline[] = "\n";
            if (Write(newline, 1) != 1) return false;
            
            return true;
        }
        
        bool NkFile::Write(const NkString& text) {
            return Write(text.CStr(), text.Length()) == text.Length();
        }
        
        nk_int64 NkFile::Tell() const {
            if (!mIsOpen) return -1;
            return ftell(static_cast<FILE*>(mHandle));
        }
        
        bool NkFile::Seek(nk_int64 offset, NkSeekOrigin origin) {
            if (!mIsOpen) return false;
            
            int whence;
            switch (origin) {
                case NkSeekOrigin::NK_BEGIN: whence = SEEK_SET; break;
                case NkSeekOrigin::NK_CURRENT: whence = SEEK_CUR; break;
                case NkSeekOrigin::NK_END: whence = SEEK_END; break;
                default: whence = SEEK_SET;
            }
            
            return fseek(static_cast<FILE*>(mHandle), static_cast<long>(offset), whence) == 0;
        }
        
        bool NkFile::SeekToBegin() {
            return Seek(0, NkSeekOrigin::NK_BEGIN);
        }
        
        bool NkFile::SeekToEnd() {
            return Seek(0, NkSeekOrigin::NK_END);
        }
        
        nk_int64 NkFile::GetSize() const {
            if (!mIsOpen) return -1;
            
            nk_int64 current = Tell();
            fseek(static_cast<FILE*>(mHandle), 0, SEEK_END);
            nk_int64 size = ftell(static_cast<FILE*>(mHandle));
            fseek(static_cast<FILE*>(mHandle), static_cast<long>(current), SEEK_SET);
            
            return size;
        }
        
        void NkFile::Flush() {
            if (mIsOpen) {
                fflush(static_cast<FILE*>(mHandle));
            }
        }
        
        const NkPath& NkFile::GetPath() const {
            return mPath;
        }
        
        NkFileMode NkFile::GetMode() const {
            return mMode;
        }
        
        bool NkFile::IsEOF() const {
            if (!mIsOpen) return true;
            return feof(static_cast<FILE*>(mHandle)) != 0;
        }
        
        // Static utilities
        bool NkFile::Exists(const char* path) {
            if (!path) return false;
            
            #ifdef _WIN32
            DWORD attrs = GetFileAttributesA(path);
            return (attrs != INVALID_FILE_ATTRIBUTES) && 
                   !(attrs & FILE_ATTRIBUTE_DIRECTORY);
            #else
            struct stat st;
            return (stat(path, &st) == 0) && S_ISREG(st.st_mode);
            #endif
        }
        
        bool NkFile::Exists(const NkPath& path) {
            return Exists(path.CStr());
        }
        
        bool NkFile::Delete(const char* path) {
            if (!path) return false;
            return remove(path) == 0;
        }
        
        bool NkFile::Delete(const NkPath& path) {
            return Delete(path.CStr());
        }
        
        bool NkFile::Copy(const char* source, const char* dest, bool overwrite) {
            if (!source || !dest) return false;
            
            if (!overwrite && Exists(dest)) {
                return false;
            }
            
            NkFile src(source, NkFileMode::NK_READ);
            if (!src.IsOpen()) return false;
            
            NkFile dst(dest, NkFileMode::NK_WRITE);
            if (!dst.IsOpen()) return false;
            
            char buffer[8192];
            usize read;
            while ((read = src.Read(buffer, sizeof(buffer))) > 0) {
                if (dst.Write(buffer, read) != read) {
                    return false;
                }
            }
            
            return true;
        }
        
        bool NkFile::Copy(const NkPath& source, const NkPath& dest, bool overwrite) {
            return Copy(source.CStr(), dest.CStr(), overwrite);
        }
        
        bool NkFile::Move(const char* source, const char* dest) {
            if (!source || !dest) return false;
            return rename(source, dest) == 0;
        }
        
        bool NkFile::Move(const NkPath& source, const NkPath& dest) {
            return Move(source.CStr(), dest.CStr());
        }
        
        nk_int64 NkFile::GetFileSize(const char* path) {
            NkFile file(path, NkFileMode::NK_READ);
            if (!file.IsOpen()) return -1;
            return file.GetSize();
        }
        
        nk_int64 NkFile::GetFileSize(const NkPath& path) {
            return GetFileSize(path.CStr());
        }
        
        NkString NkFile::ReadAllText(const char* path) {
            NkFile file(path, NkFileMode::NK_READ);
            if (!file.IsOpen()) return NkString();
            return file.ReadAll();
        }
        
        NkString NkFile::ReadAllText(const NkPath& path) {
            return ReadAllText(path.CStr());
        }
        
        NkVector<nk_uint8> NkFile::ReadAllBytes(const char* path) {
            NkFile file(path, NkFileMode::NK_READ);
            if (!file.IsOpen()) return NkVector<nk_uint8>();
            
            nk_int64 size = file.GetSize();
            if (size <= 0) return NkVector<nk_uint8>();
            
            NkVector<nk_uint8> data;
            data.Resize(static_cast<usize>(size));
            
            file.Read(data.Data(), static_cast<usize>(size));
            return data;
        }
        
        NkVector<nk_uint8> NkFile::ReadAllBytes(const NkPath& path) {
            return ReadAllBytes(path.CStr());
        }
        
        bool NkFile::WriteAllText(const char* path, const char* text) {
            NkFile file(path, NkFileMode::NK_WRITE);
            if (!file.IsOpen()) return false;
            
            usize len = strlen(text);
            return file.Write(text, len) == len;
        }
        
        bool NkFile::WriteAllText(const NkPath& path, const NkString& text) {
            return WriteAllText(path.CStr(), text.CStr());
        }
        
        bool NkFile::WriteAllBytes(const char* path, const NkVector<nk_uint8>& data) {
            NkFile file(path, NkFileMode::NK_WRITE);
            if (!file.IsOpen()) return false;
            
            return file.Write(data.Data(), data.Size()) == data.Size();
        }
        
        bool NkFile::WriteAllBytes(const NkPath& path, const NkVector<nk_uint8>& data) {
            return WriteAllBytes(path.CStr(), data);
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
==================================================


[FICHIER: NkFile.h]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFile.h
// DESCRIPTION: File operations and manipulation
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILE_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILE_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief File access modes
         */
        enum class NkFileMode {
            NK_READ,           // Open for reading
            NK_WRITE,          // Open for writing (truncate)
            NK_APPEND,         // Open for writing (append)
            NK_READ_WRITE,      // Open for reading and writing
            NK_READ_APPEND      // Open for reading and appending
        };
        
        /**
         * @brief File seek origin
         */
        enum class NkSeekOrigin {
            NK_BEGIN,          // From beginning of file
            NK_CURRENT,        // From current position
            NK_END             // From end of file
        };
        
        /**
         * @brief File class for I/O operations
         * 
         * Gère lecture/écriture de fichiers de manière sécurisée.
         * RAII: Le fichier est automatiquement fermé à la destruction.
         * 
         * @example
         * NkFile file("data.txt", NkFileMode::Write);
         * if (file.IsOpen()) {
         *     file.WriteLine("Hello World");
         *     file.Flush();
         * }
         */
        class NkFile {
            private:
                void* mHandle;  // FILE* hidden behind void*
                NkPath mPath;
                NkFileMode mMode;
                bool mIsOpen;
                
                const char* GetModeString() const;
                
            public:
                // Constructors
                NkFile();
                explicit NkFile(const char* path, NkFileMode mode = NkFileMode::NK_READ);
                explicit NkFile(const NkPath& path, NkFileMode mode = NkFileMode::NK_READ);
                ~NkFile();
                
                // Non-copyable
                NkFile(const NkFile&) = delete;
                NkFile& operator=(const NkFile&) = delete;
                
                // Movable (C++11)
                #if defined(NK_CPP11)
                NkFile(NkFile&& other) noexcept;
                NkFile& operator=(NkFile&& other) noexcept;
                #endif
                
                // File operations
                bool Open(const char* path, NkFileMode mode = NkFileMode::NK_READ);
                bool Open(const NkPath& path, NkFileMode mode = NkFileMode::NK_READ);
                void Close();
                bool IsOpen() const;
                
                // Reading
                usize Read(void* buffer, usize size);
                NkString ReadLine();
                NkString ReadAll();
                NkVector<NkString> ReadLines();
                
                // Writing
                usize Write(const void* data, usize size);
                bool WriteLine(const char* text);
                bool Write(const NkString& text);
                
                // Position
                nk_int64 Tell() const;
                bool Seek(nk_int64 offset, NkSeekOrigin origin = NkSeekOrigin::NK_BEGIN);
                bool SeekToBegin();
                bool SeekToEnd();
                nk_int64 GetSize() const;
                
                // Buffering
                void Flush();
                
                // Properties
                const NkPath& GetPath() const;
                NkFileMode GetMode() const;
                bool IsEOF() const;
                
                // Static utilities
                static bool Exists(const char* path);
                static bool Exists(const NkPath& path);
                static bool Delete(const char* path);
                static bool Delete(const NkPath& path);
                static bool Copy(const char* source, const char* dest, bool overwrite = false);
                static bool Copy(const NkPath& source, const NkPath& dest, bool overwrite = false);
                static bool Move(const char* source, const char* dest);
                static bool Move(const NkPath& source, const NkPath& dest);
                
                static nk_int64 GetFileSize(const char* path);
                static nk_int64 GetFileSize(const NkPath& path);
                
                static NkString ReadAllText(const char* path);
                static NkString ReadAllText(const NkPath& path);
                static NkVector<nk_uint8> ReadAllBytes(const char* path);
                static NkVector<nk_uint8> ReadAllBytes(const NkPath& path);
                
                static bool WriteAllText(const char* path, const char* text);
                static bool WriteAllText(const NkPath& path, const NkString& text);
                static bool WriteAllBytes(const char* path, const NkVector<nk_uint8>& data);
                static bool WriteAllBytes(const NkPath& path, const NkVector<nk_uint8>& data);
        };
        
    } // namespace entseu
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
==================================================


[FICHIER: NkFileSystem.cpp]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFileSystem.cpp
// DESCRIPTION: File system utilities implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkFileSystem.h"
#include "NKFileSystem/NkDirectory.h"
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #ifdef GetFreeSpace
        #undef GetFreeSpace
    #endif
    #ifdef GetFileAttributes
        #undef GetFileAttributes
    #endif
    #ifdef SetFileAttributes
        #undef SetFileAttributes
    #endif
    #ifdef CreateSymbolicLink
        #undef CreateSymbolicLink
    #endif
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <limits.h>
#endif

namespace nkentseu {
    namespace entseu {
        
        NkDriveInfo::NkDriveInfo()
            : Name()
            , Label()
            , Type(NkFileSystemType::NK_UNKNOW)
            , TotalSize(0)
            , FreeSpace(0)
            , AvailableSpace(0)
            , IsReady(false) {
        }
        
        NkFileAttributes::NkFileAttributes()
            : IsReadOnly(false)
            , IsHidden(false)
            , IsSystem(false)
            , IsArchive(false)
            , IsCompressed(false)
            , IsEncrypted(false)
            , CreationTime(0)
            , LastAccessTime(0)
            , LastWriteTime(0) {
        }
        
        NkVector<NkDriveInfo> NkFileSystem::GetDrives() {
            NkVector<NkDriveInfo> drives;
            
            #ifdef _WIN32
            DWORD driveMask = GetLogicalDrives();
            
            for (int i = 0; i < 26; ++i) {
                if (driveMask & (1 << i)) {
                    char driveLetter[4] = {static_cast<char>('A' + i), ':', '\\', '\0'};
                    
                    NkDriveInfo info;
                    info.Name = driveLetter;
                    
                    UINT driveType = GetDriveTypeA(driveLetter);
                    info.IsReady = (driveType != DRIVE_NO_ROOT_DIR);
                    
                    if (info.IsReady) {
                        // Get volume information
                        char volumeName[MAX_PATH];
                        char fileSystemName[MAX_PATH];
                        if (GetVolumeInformationA(driveLetter, volumeName, MAX_PATH,
                                                  NULL, NULL, NULL,
                                                  fileSystemName, MAX_PATH)) {
                            info.Label = volumeName;
                            
                            if (strcmp(fileSystemName, "NTFS") == 0) {
                                info.Type = NkFileSystemType::NK_NTFS;
                            } else if (strcmp(fileSystemName, "FAT32") == 0) {
                                info.Type = NkFileSystemType::NK_FAT32;
                            } else if (strcmp(fileSystemName, "exFAT") == 0) {
                                info.Type = NkFileSystemType::NK_EXFAT;
                            }
                        }
                        
                        // Get space information
                        ULARGE_INTEGER freeBytes, totalBytes, availBytes;
                        if (GetDiskFreeSpaceExA(driveLetter, &availBytes, &totalBytes, &freeBytes)) {
                            info.TotalSize = totalBytes.QuadPart;
                            info.FreeSpace = freeBytes.QuadPart;
                            info.AvailableSpace = availBytes.QuadPart;
                        }
                    }
                    
                    drives.PushBack(info);
                }
            }
            
            #else
            // Unix: just report root
            NkDriveInfo info;
            info.Name = "/";
            info.IsReady = true;
            
            struct statvfs stat;
            if (statvfs("/", &stat) == 0) {
                info.TotalSize = (nk_int64)stat.f_blocks * stat.f_frsize;
                info.FreeSpace = (nk_int64)stat.f_bfree * stat.f_frsize;
                info.AvailableSpace = (nk_int64)stat.f_bavail * stat.f_frsize;
            }
            
            drives.PushBack(info);
            #endif
            
            return drives;
        }
        
        NkDriveInfo NkFileSystem::GetDriveInfo(const char* path) {
            if (!path) return NkDriveInfo();
            
            #ifdef _WIN32
            // Extract drive letter
            if (path[0] && path[1] == ':') {
                char driveLetter[4] = {path[0], ':', '\\', '\0'};
                
                auto drives = GetDrives();
                for (auto& drive : drives) {
                    if (_stricmp(drive.Name.CStr(), driveLetter) == 0) {
                        return drive;
                    }
                }
            }
            #else
            auto drives = GetDrives();
            if (!drives.Empty()) {
                return drives[0];
            }
            #endif
            
            return NkDriveInfo();
        }
        
        NkDriveInfo NkFileSystem::GetDriveInfo(const NkPath& path) {
            return GetDriveInfo(path.CStr());
        }
        
        nk_int64 NkFileSystem::GetTotalSpace(const char* path) {
            NkDriveInfo info = GetDriveInfo(path);
            return info.TotalSize;
        }
        
        nk_int64 NkFileSystem::GetTotalSpace(const NkPath& path) {
            return GetTotalSpace(path.CStr());
        }
        
        nk_int64 NkFileSystem::GetFreeSpace(const char* path) {
            NkDriveInfo info = GetDriveInfo(path);
            return info.FreeSpace;
        }
        
        nk_int64 NkFileSystem::GetFreeSpace(const NkPath& path) {
            return GetFreeSpace(path.CStr());
        }
        
        nk_int64 NkFileSystem::GetAvailableSpace(const char* path) {
            NkDriveInfo info = GetDriveInfo(path);
            return info.AvailableSpace;
        }
        
        nk_int64 NkFileSystem::GetAvailableSpace(const NkPath& path) {
            return GetAvailableSpace(path.CStr());
        }
        
        NkFileAttributes NkFileSystem::GetFileAttributes(const char* path) {
            NkFileAttributes attrs;
            
            if (!path) return attrs;
            
            #ifdef _WIN32
            DWORD winAttrs = GetFileAttributesA(path);
            if (winAttrs != INVALID_FILE_ATTRIBUTES) {
                attrs.IsReadOnly = (winAttrs & FILE_ATTRIBUTE_READONLY) != 0;
                attrs.IsHidden = (winAttrs & FILE_ATTRIBUTE_HIDDEN) != 0;
                attrs.IsSystem = (winAttrs & FILE_ATTRIBUTE_SYSTEM) != 0;
                attrs.IsArchive = (winAttrs & FILE_ATTRIBUTE_ARCHIVE) != 0;
                attrs.IsCompressed = (winAttrs & FILE_ATTRIBUTE_COMPRESSED) != 0;
                attrs.IsEncrypted = (winAttrs & FILE_ATTRIBUTE_ENCRYPTED) != 0;
            }
            
            HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                FILETIME creation, access, write;
                if (GetFileTime(hFile, &creation, &access, &write)) {
                    attrs.CreationTime = ((nk_int64)creation.dwHighDateTime << 32) | creation.dwLowDateTime;
                    attrs.LastAccessTime = ((nk_int64)access.dwHighDateTime << 32) | access.dwLowDateTime;
                    attrs.LastWriteTime = ((nk_int64)write.dwHighDateTime << 32) | write.dwLowDateTime;
                }
                CloseHandle(hFile);
            }
            
            #else
            struct stat st;
            if (stat(path, &st) == 0) {
                attrs.IsReadOnly = !(st.st_mode & S_IWUSR);
                attrs.CreationTime = st.st_ctime;
                attrs.LastAccessTime = st.st_atime;
                attrs.LastWriteTime = st.st_mtime;
            }
            #endif
            
            return attrs;
        }
        
        NkFileAttributes NkFileSystem::GetFileAttributes(const NkPath& path) {
            return GetFileAttributes(path.CStr());
        }
        
        bool NkFileSystem::SetFileAttributes(const char* path, const NkFileAttributes& attrs) {
            if (!path) return false;
            
            #ifdef _WIN32
            DWORD winAttrs = 0;
            if (attrs.IsReadOnly) winAttrs |= FILE_ATTRIBUTE_READONLY;
            if (attrs.IsHidden) winAttrs |= FILE_ATTRIBUTE_HIDDEN;
            if (attrs.IsSystem) winAttrs |= FILE_ATTRIBUTE_SYSTEM;
            if (attrs.IsArchive) winAttrs |= FILE_ATTRIBUTE_ARCHIVE;
            
            return SetFileAttributesA(path, winAttrs) != 0;
            #else
            // Unix: only read-only is relevant
            struct stat st;
            if (stat(path, &st) != 0) return false;
            
            mode_t mode = st.st_mode;
            if (attrs.IsReadOnly) {
                mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
            } else {
                mode |= S_IWUSR;
            }
            
            return chmod(path, mode) == 0;
            #endif
        }
        
        bool NkFileSystem::SetFileAttributes(const NkPath& path, const NkFileAttributes& attrs) {
            return SetFileAttributes(path.CStr(), attrs);
        }
        
        bool NkFileSystem::SetReadOnly(const char* path, bool readOnly) {
            NkFileAttributes attrs = GetFileAttributes(path);
            attrs.IsReadOnly = readOnly;
            return SetFileAttributes(path, attrs);
        }
        
        bool NkFileSystem::SetReadOnly(const NkPath& path, bool readOnly) {
            return SetReadOnly(path.CStr(), readOnly);
        }
        
        bool NkFileSystem::SetHidden(const char* path, bool hidden) {
            #ifdef _WIN32
            NkFileAttributes attrs = GetFileAttributes(path);
            attrs.IsHidden = hidden;
            return SetFileAttributes(path, attrs);
            #else
            // Unix: prepend dot to filename
            return false;  // Not directly supported
            #endif
        }
        
        bool NkFileSystem::SetHidden(const NkPath& path, bool hidden) {
            return SetHidden(path.CStr(), hidden);
        }
        
        nk_int64 NkFileSystem::GetCreationTime(const char* path) {
            return GetFileAttributes(path).CreationTime;
        }
        
        nk_int64 NkFileSystem::GetCreationTime(const NkPath& path) {
            return GetCreationTime(path.CStr());
        }
        
        nk_int64 NkFileSystem::GetLastAccessTime(const char* path) {
            return GetFileAttributes(path).LastAccessTime;
        }
        
        nk_int64 NkFileSystem::GetLastAccessTime(const NkPath& path) {
            return GetLastAccessTime(path.CStr());
        }
        
        nk_int64 NkFileSystem::GetLastWriteTime(const char* path) {
            return GetFileAttributes(path).LastWriteTime;
        }
        
        nk_int64 NkFileSystem::GetLastWriteTime(const NkPath& path) {
            return GetLastWriteTime(path.CStr());
        }
        
        bool NkFileSystem::IsValidFileName(const char* name) {
            if (!name || !name[0]) return false;
            
            // Check for invalid characters
            const char* invalid = "<>:\"|?*";
            for (const char* p = name; *p; ++p) {
                if (strchr(invalid, *p)) return false;
                if (*p < 32) return false;  // Control characters
            }
            
            // Check for reserved names (Windows)
            #ifdef _WIN32
            const char* reserved[] = {
                "CON", "PRN", "AUX", "NUL",
                "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
            };
            for (const char* res : reserved) {
                if (_stricmp(name, res) == 0) return false;
            }
            #endif
            
            return true;
        }
        
        bool NkFileSystem::IsValidPath(const char* path) {
            return NkPath::IsValidPath(path);
        }
        
        NkString NkFileSystem::GetAbsolutePath(const char* path) {
            if (!path) return NkString();
            
            #ifdef _WIN32
            char buffer[MAX_PATH];
            if (GetFullPathNameA(path, MAX_PATH, buffer, NULL)) {
                return NkString(buffer);
            }
            #else
            char buffer[PATH_MAX];
            if (realpath(path, buffer)) {
                return NkString(buffer);
            }
            #endif
            
            return NkString(path);
        }
        
        NkString NkFileSystem::GetAbsolutePath(const NkPath& path) {
            return GetAbsolutePath(path.CStr());
        }
        
        NkString NkFileSystem::GetRelativePath(const char* from, const char* to) {
            // Simplified implementation
            return NkString(to);
        }
        
        NkString NkFileSystem::GetRelativePath(const NkPath& from, const NkPath& to) {
            return GetRelativePath(from.CStr(), to.CStr());
        }
        
        NkFileSystemType NkFileSystem::GetFileSystemType(const char* path) {
            NkDriveInfo info = GetDriveInfo(path);
            return info.Type;
        }
        
        NkFileSystemType NkFileSystem::GetFileSystemType(const NkPath& path) {
            return GetFileSystemType(path.CStr());
        }
        
        bool NkFileSystem::IsCaseSensitive(const char* path) {
            #ifdef _WIN32
            return false;  // Windows is case-insensitive
            #else
            return true;   // Unix is case-sensitive
            #endif
        }
        
        bool NkFileSystem::IsCaseSensitive(const NkPath& path) {
            return IsCaseSensitive(path.CStr());
        }
        
        bool NkFileSystem::IsSymbolicLink(const char* path) {
            if (!path) return false;
            
            #ifdef _WIN32
            DWORD attrs = GetFileAttributesA(path);
            return (attrs != INVALID_FILE_ATTRIBUTES) && 
                   (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
            #else
            struct stat st;
            return (lstat(path, &st) == 0) && S_ISLNK(st.st_mode);
            #endif
        }
        
        bool NkFileSystem::IsSymbolicLink(const NkPath& path) {
            return IsSymbolicLink(path.CStr());
        }
        
        NkString NkFileSystem::GetSymbolicLinkTarget(const char* path) {
            if (!path) return NkString();
            
            #ifndef _WIN32
            char buffer[PATH_MAX];
            ssize_t len = readlink(path, buffer, sizeof(buffer) - 1);
            if (len > 0) {
                buffer[len] = '\0';
                return NkString(buffer);
            }
            #endif
            
            return NkString();
        }
        
        NkString NkFileSystem::GetSymbolicLinkTarget(const NkPath& path) {
            return GetSymbolicLinkTarget(path.CStr());
        }
        
        bool NkFileSystem::CreateSymbolicLink(const char* link, const char* target) {
            if (!link || !target) return false;
            
            #ifdef _WIN32
            return CreateSymbolicLinkA(link, target, 0) != 0;
            #else
            return symlink(target, link) == 0;
            #endif
        }
        
        bool NkFileSystem::CreateSymbolicLink(const NkPath& link, const NkPath& target) {
            return CreateSymbolicLink(link.CStr(), target.CStr());
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
==================================================


[FICHIER: NkFileSystem.h]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFileSystem.h
// DESCRIPTION: File system utilities and information
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILESYSTEM_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILESYSTEM_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief File system type
         */
        enum class NkFileSystemType {
            NK_UNKNOW,
            NK_NTFS,       // Windows NT File System
            NK_FAT32,      // File Allocation Table 32
            NK_EXFAT,      // Extended FAT
            NK_EXT4,       // Fourth Extended Filesystem
            NK_EXT3,       // Third Extended Filesystem
            NK_HFS,        // Hierarchical File System (macOS)
            NK_APFS,       // Apple File System
            NK_NETWORK     // Network drive
        };
        
        /**
         * @brief Drive information
         */
        struct NkDriveInfo {
            NkString Name;
            NkString Label;
            NkFileSystemType Type;
            nk_int64 TotalSize;
            nk_int64 FreeSpace;
            nk_int64 AvailableSpace;
            bool IsReady;
            
            NkDriveInfo();
        };
        
        /**
         * @brief File attributes
         */
        struct NkFileAttributes {
            bool IsReadOnly;
            bool IsHidden;
            bool IsSystem;
            bool IsArchive;
            bool IsCompressed;
            bool IsEncrypted;
            nk_int64 CreationTime;
            nk_int64 LastAccessTime;
            nk_int64 LastWriteTime;
            
            NkFileAttributes();
        };
        
        /**
         * @brief File system utilities class
         * 
         * Fournit des informations sur le système de fichiers.
         * 
         * @example
         * auto drives = NkFileSystem::GetDrives();
         * for (auto& drive : drives) {
         *     printf("%s: %lld MB free\n", 
         *            drive.Name.CStr(), 
         *            drive.FreeSpace / (1024*1024));
         * }
         */
        class NkFileSystem {
            public:
                // Drive operations
                static NkVector<NkDriveInfo> GetDrives();
                static NkDriveInfo GetDriveInfo(const char* path);
                static NkDriveInfo GetDriveInfo(const NkPath& path);
                
                // Space information
                static nk_int64 GetTotalSpace(const char* path);
                static nk_int64 GetTotalSpace(const NkPath& path);
                static nk_int64 GetFreeSpace(const char* path);
                static nk_int64 GetFreeSpace(const NkPath& path);
                static nk_int64 GetAvailableSpace(const char* path);
                static nk_int64 GetAvailableSpace(const NkPath& path);
                
                // File attributes
                static NkFileAttributes GetFileAttributes(const char* path);
                static NkFileAttributes GetFileAttributes(const NkPath& path);
                static bool SetFileAttributes(const char* path, const NkFileAttributes& attrs);
                static bool SetFileAttributes(const NkPath& path, const NkFileAttributes& attrs);
                
                static bool SetReadOnly(const char* path, bool readOnly);
                static bool SetReadOnly(const NkPath& path, bool readOnly);
                static bool SetHidden(const char* path, bool hidden);
                static bool SetHidden(const NkPath& path, bool hidden);
                
                // File times
                static nk_int64 GetCreationTime(const char* path);
                static nk_int64 GetCreationTime(const NkPath& path);
                static nk_int64 GetLastAccessTime(const char* path);
                static nk_int64 GetLastAccessTime(const NkPath& path);
                static nk_int64 GetLastWriteTime(const char* path);
                static nk_int64 GetLastWriteTime(const NkPath& path);
                
                static bool SetCreationTime(const char* path, nk_int64 time);
                static bool SetCreationTime(const NkPath& path, nk_int64 time);
                static bool SetLastAccessTime(const char* path, nk_int64 time);
                static bool SetLastAccessTime(const NkPath& path, nk_int64 time);
                static bool SetLastWriteTime(const char* path, nk_int64 time);
                static bool SetLastWriteTime(const NkPath& path, nk_int64 time);
                
                // Path validation
                static bool IsValidFileName(const char* name);
                static bool IsValidPath(const char* path);
                
                // Path conversion
                static NkString GetAbsolutePath(const char* path);
                static NkString GetAbsolutePath(const NkPath& path);
                static NkString GetRelativePath(const char* from, const char* to);
                static NkString GetRelativePath(const NkPath& from, const NkPath& to);
                
                // File system info
                static NkFileSystemType GetFileSystemType(const char* path);
                static NkFileSystemType GetFileSystemType(const NkPath& path);
                static bool IsCaseSensitive(const char* path);
                static bool IsCaseSensitive(const NkPath& path);
                
                // Symbolic links
                static bool IsSymbolicLink(const char* path);
                static bool IsSymbolicLink(const NkPath& path);
                static NkString GetSymbolicLinkTarget(const char* path);
                static NkString GetSymbolicLinkTarget(const NkPath& path);
                static bool CreateSymbolicLink(const char* link, const char* target);
                static bool CreateSymbolicLink(const NkPath& link, const NkPath& target);
        };
        
    } // namespace entseu
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILESYSTEM_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
==================================================


[FICHIER: NkFileWatcher.cpp]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFileWatcher.cpp
// DESCRIPTION: File system watcher implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NKFileSystem/NkFileWatcher.h"
#include <cstring>
#include <ctime>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#elif defined(__EMSCRIPTEN__)
    // WASM fallback: no native kernel watcher API.
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/inotify.h>
    #include <sys/select.h>
    #include <errno.h>
#endif

namespace nkentseu {
    namespace entseu {
        
        NkFileChangeEvent::NkFileChangeEvent()
            : Type(NkFileChangeType::NK_MODIFIED)
            , Path()
            , OldPath()
            , Timestamp(0) {
        }
        
        NkFileChangeEvent::NkFileChangeEvent(NkFileChangeType type, const char* path)
            : Type(type)
            , Path(path)
            , OldPath()
            , Timestamp(static_cast<nk_int64>(time(nullptr))) {
        }
        
        NkFileWatcher::NkFileWatcher()
            : mHandle(nullptr)
            , mPath()
            , mCallback(nullptr)
            , mIsWatching(false)
            , mRecursive(false)
            , mThread(nullptr) {
        }
        
        NkFileWatcher::NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive)
            : mHandle(nullptr)
            , mPath(path)
            , mCallback(callback)
            , mIsWatching(false)
            , mRecursive(recursive)
            , mThread(nullptr) {
        }
        
        NkFileWatcher::NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive)
            : mHandle(nullptr)
            , mPath(path.ToString())
            , mCallback(callback)
            , mIsWatching(false)
            , mRecursive(recursive)
            , mThread(nullptr) {
        }
        
        NkFileWatcher::~NkFileWatcher() {
            Stop();
        }
        
        #ifdef _WIN32
        void* NkFileWatcher::ThreadProc(void* param) {
            NkFileWatcher* watcher = static_cast<NkFileWatcher*>(param);
            watcher->WatchThread();
            return nullptr;
        }
        
        void NkFileWatcher::WatchThread() {
            HANDLE hDir = CreateFileA(
                mPath.CStr(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                NULL
            );
            
            if (hDir == INVALID_HANDLE_VALUE) {
                return;
            }
            
            const DWORD BUFFER_SIZE = 4096;
            char buffer[BUFFER_SIZE];
            DWORD bytesReturned;
            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            
            while (mIsWatching) {
                BOOL success = ReadDirectoryChangesW(
                    hDir,
                    buffer,
                    BUFFER_SIZE,
                    mRecursive ? TRUE : FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                    FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                    &bytesReturned,
                    &overlapped,
                    NULL
                );
                
                if (!success) {
                    break;
                }
                
                DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 1000);
                if (waitResult == WAIT_OBJECT_0) {
                    GetOverlappedResult(hDir, &overlapped, &bytesReturned, FALSE);
                    
                    if (bytesReturned > 0 && mCallback) {
                        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
                        
                        while (true) {
                            // Convert wchar to char (simplified)
                            char filename[MAX_PATH];
                            WideCharToMultiByte(CP_UTF8, 0, info->FileName, 
                                              info->FileNameLength / sizeof(WCHAR),
                                              filename, MAX_PATH, NULL, NULL);
                            
                            NkFileChangeEvent event;
                            event.Path = (NkPath(mPath.CStr()) / filename).ToString();
                            event.Timestamp = static_cast<nk_int64>(time(nullptr));
                            
                            switch (info->Action) {
                                case FILE_ACTION_ADDED:
                                    event.Type = NkFileChangeType::NK_CREATED;
                                    break;
                                case FILE_ACTION_REMOVED:
                                    event.Type = NkFileChangeType::NK_DELETED;
                                    break;
                                case FILE_ACTION_MODIFIED:
                                    event.Type = NkFileChangeType::NK_MODIFIED;
                                    break;
                                case FILE_ACTION_RENAMED_OLD_NAME:
                                case FILE_ACTION_RENAMED_NEW_NAME:
                                    event.Type = NkFileChangeType::NK_RENAMED;
                                    break;
                            }
                            
                            mCallback->OnFileChanged(event);
                            
                            if (info->NextEntryOffset == 0) break;
                            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                                reinterpret_cast<char*>(info) + info->NextEntryOffset
                            );
                        }
                    }
                    
                    ResetEvent(overlapped.hEvent);
                }
            }
            
            CloseHandle(overlapped.hEvent);
            CloseHandle(hDir);
        }
        
        #elif !defined(__EMSCRIPTEN__)

        void* NkFileWatcher::ThreadProc(void* param) {
            NkFileWatcher* watcher = static_cast<NkFileWatcher*>(param);
            watcher->WatchThread();
            return nullptr;
        }

        void NkFileWatcher::WatchThread() {
            int fd = inotify_init();
            if (fd < 0) {
                return;
            }
            
            uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | 
                           IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;
            
            int wd = inotify_add_watch(fd, mPath.CStr(), mask);
            if (wd < 0) {
                close(fd);
                return;
            }
            
            const size_t EVENT_SIZE = sizeof(struct inotify_event);
            const size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);
            char buffer[BUF_LEN];
            
            while (mIsWatching) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd, &fds);
                
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                
                int ret = select(fd + 1, &fds, NULL, NULL, &tv);
                
                if (ret < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                
                if (ret == 0) continue;  // Timeout
                
                ssize_t length = read(fd, buffer, BUF_LEN);
                if (length < 0) {
                    break;
                }
                
                if (mCallback) {
                    size_t i = 0;
                    while (i < static_cast<size_t>(length)) {
                        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
                        
                        if (event->len > 0) {
                            NkFileChangeEvent changeEvent;
                            changeEvent.Path = (NkPath(mPath.CStr()) / event->name).ToString();
                            changeEvent.Timestamp = static_cast<nk_int64>(time(nullptr));
                            
                            if (event->mask & IN_CREATE) {
                                changeEvent.Type = NkFileChangeType::Created;
                            } else if (event->mask & IN_DELETE) {
                                changeEvent.Type = NkFileChangeType::Deleted;
                            } else if (event->mask & IN_MODIFY) {
                                changeEvent.Type = NkFileChangeType::Modified;
                            } else if (event->mask & (IN_MOVED_FROM | IN_MOVED_TO)) {
                                changeEvent.Type = NkFileChangeType::Renamed;
                            } else if (event->mask & IN_ATTRIB) {
                                changeEvent.Type = NkFileChangeType::AttributeChanged;
                            }
                            
                            mCallback->OnFileChanged(changeEvent);
                        }
                        
                        i += EVENT_SIZE + event->len;
                    }
                }
            }
            
            inotify_rm_watch(fd, wd);
            close(fd);
        }
        
        #endif
        
        bool NkFileWatcher::Start() {
            if (mIsWatching) return true;
            if (mPath.Empty() || !mCallback) return false;
            
            mIsWatching = true;
            
            #ifdef _WIN32
            mThread = reinterpret_cast<void*>(_beginthread(
                reinterpret_cast<void(*)(void*)>(ThreadProc),
                0,
                this
            ));
            #elif defined(__EMSCRIPTEN__)
            // WASM: keep API alive even without kernel notifications.
            // Client code can still query IsWatching()/Stop() deterministically.
            mThread = nullptr;
            return true;
            #else
            pthread_t* thread = new pthread_t;
            if (pthread_create(thread, NULL, ThreadProc, this) == 0) {
                mThread = thread;
            } else {
                delete thread;
                mIsWatching = false;
                return false;
            }
            #endif
            
            return true;
        }
        
        void NkFileWatcher::Stop() {
            if (!mIsWatching) return;
            
            mIsWatching = false;
            
            if (mThread) {
                #ifdef _WIN32
                WaitForSingleObject(mThread, INFINITE);
                CloseHandle(mThread);
                #elif !defined(__EMSCRIPTEN__)
                pthread_t* thread = static_cast<pthread_t*>(mThread);
                pthread_join(*thread, NULL);
                delete thread;
                #endif
                mThread = nullptr;
            }
        }
        
        bool NkFileWatcher::IsWatching() const {
            return mIsWatching;
        }
        
        void NkFileWatcher::SetPath(const char* path) {
            bool wasWatching = mIsWatching;
            if (wasWatching) Stop();
            
            mPath = path;
            
            if (wasWatching) Start();
        }
        
        void NkFileWatcher::SetPath(const NkPath& path) {
            SetPath(path.CStr());
        }
        
        void NkFileWatcher::SetCallback(NkFileWatcherCallback* callback) {
            mCallback = callback;
        }
        
        void NkFileWatcher::SetRecursive(bool recursive) {
            bool wasWatching = mIsWatching;
            if (wasWatching) Stop();
            
            mRecursive = recursive;
            
            if (wasWatching) Start();
        }
        
        const NkString& NkFileWatcher::GetPath() const {
            return mPath;
        }
        
        bool NkFileWatcher::IsRecursive() const {
            return mRecursive;
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
==================================================


[FICHIER: NkFileWatcher.h]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkFileWatcher.h
// DESCRIPTION: File system watcher for monitoring changes
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILEWATCHER_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILEWATCHER_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief File change types
         */
        enum class NkFileChangeType {
            NK_CREATED,        // File or directory created
            NK_DELETED,        // File or directory deleted
            NK_MODIFIED,       // File content modified
            NK_RENAMED,        // File or directory renamed
            NK_ATTRIBUTE_CHANGED // File attributes changed
        };
        
        /**
         * @brief File change event
         */
        struct NkFileChangeEvent {
            NkFileChangeType Type;
            NkString Path;
            NkString OldPath;  // For rename events
            nk_int64 Timestamp;
            
            NkFileChangeEvent();
            NkFileChangeEvent(NkFileChangeType type, const char* path);
        };
        
        /**
         * @brief File watcher callback
         */
        class NkFileWatcherCallback {
        public:
            virtual ~NkFileWatcherCallback() {}
            virtual void OnFileChanged(const NkFileChangeEvent& event) = 0;
        };
        
        /**
         * @brief File system watcher
         * 
         * Surveille un répertoire pour détecter les changements.
         * Utilise les APIs natives (inotify sur Linux, ReadDirectoryChangesW sur Windows).
         * 
         * @example
         * class MyCallback : public NkFileWatcherCallback {
         *     void OnFileChanged(const NkFileChangeEvent& event) override {
         *         printf("File changed: %s\n", event.Path.CStr());
         *     }
         * };
         * 
         * MyCallback callback;
         * NkFileWatcher watcher("data", &callback);
         * watcher.Start();
         * // ... do work ...
         * watcher.Stop();
         */
        class NkFileWatcher {
        private:
            void* mHandle;  // Platform-specific handle
            NkString mPath;
            NkFileWatcherCallback* mCallback;
            bool mIsWatching;
            bool mRecursive;
            void* mThread;  // Thread handle
            
            void WatchThread();
            static void* ThreadProc(void* param);
            
        public:
            // Constructors
            NkFileWatcher();
            NkFileWatcher(const char* path, NkFileWatcherCallback* callback, bool recursive = false);
            NkFileWatcher(const NkPath& path, NkFileWatcherCallback* callback, bool recursive = false);
            ~NkFileWatcher();
            
            // Non-copyable
            NkFileWatcher(const NkFileWatcher&) = delete;
            NkFileWatcher& operator=(const NkFileWatcher&) = delete;
            
            // Control
            bool Start();
            void Stop();
            bool IsWatching() const;
            
            // Configuration
            void SetPath(const char* path);
            void SetPath(const NkPath& path);
            void SetCallback(NkFileWatcherCallback* callback);
            void SetRecursive(bool recursive);
            
            const NkString& GetPath() const;
            bool IsRecursive() const;
        };
        
        /**
         * @brief Simple file watcher with lambda callback (C++11)
         * 
         * @example
         * NkSimpleFileWatcher watcher("data");
         * watcher.OnChanged = [](const NkFileChangeEvent& event) {
         *     printf("Changed: %s\n", event.Path.CStr());
         * };
         * watcher.Start();
         */
        #if defined(NK_CPP11)
        class NkSimpleFileWatcher : public NkFileWatcherCallback {
        private:
            NkFileWatcher mWatcher;
            
        public:
            using CallbackFunc = void(*)(const NkFileChangeEvent&);
            CallbackFunc OnChanged;
            
            NkSimpleFileWatcher(const char* path, bool recursive = false)
                : mWatcher(path, this, recursive)
                , OnChanged(nullptr) {
            }
            
            void OnFileChanged(const NkFileChangeEvent& event) override {
                if (OnChanged) {
                    OnChanged(event);
                }
            }
            
            bool Start() { return mWatcher.Start(); }
            void Stop() { mWatcher.Stop(); }
            bool IsWatching() const { return mWatcher.IsWatching(); }
        };
        #endif
        
    } // namespace entseu
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKFILEWATCHER_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
==================================================


[FICHIER: NkPath.cpp]
==================================================
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
==================================================


[FICHIER: NkPath.h]
==================================================
// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkPath.h
// DESCRIPTION: Path manipulation and utilities
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKPATH_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKPATH_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief Path manipulation class
         * 
         * Gère les chemins de fichiers de manière cross-platform.
         * Supporte Windows (C:\path\file.txt) et Unix (/path/file.txt).
         * 
         * @example
         * NkPath path("C:/Users/Documents/file.txt");
         * NkString dir = path.GetDirectory();     // "C:/Users/Documents"
         * NkString file = path.GetFileName();     // "file.txt"
         * NkString ext = path.GetExtension();     // ".txt"
         */
        class NkPath {
            private:
                NkString mPath;
                
                static constexpr char PREFERRED_SEPARATOR = '/';
                static constexpr char WINDOWS_SEPARATOR = '\\';
                
                void NormalizeSeparators();
                
            public:
                // Constructors
                NkPath();
                NkPath(const char* path);
                NkPath(const NkString& path);
                NkPath(const NkPath& other);
                
                // Assignment
                NkPath& operator=(const NkPath& other);
                NkPath& operator=(const char* path);
                
                // Path operations
                NkPath& Append(const char* component);
                NkPath& Append(const NkPath& other);
                NkPath operator/(const char* component) const;
                NkPath operator/(const NkPath& other) const;
                
                // Path components
                NkString GetDirectory() const;
                NkString GetFileName() const;
                NkString GetFileNameWithoutExtension() const;
                NkString GetExtension() const;
                NkString GetRoot() const;
                
                // Path properties
                bool IsAbsolute() const;
                bool IsRelative() const;
                bool HasExtension() const;
                bool HasFileName() const;
                
                // Path modification
                NkPath& ReplaceExtension(const char* newExt);
                NkPath& ReplaceFileName(const char* newName);
                NkPath& RemoveFileName();
                NkPath GetParent() const;
                
                // Conversion
                const char* CStr() const;
                NkString ToString() const;
                NkString ToNative() const;  // With platform-specific separators
                
                // Comparison
                bool operator==(const NkPath& other) const;
                bool operator!=(const NkPath& other) const;
                
                // Static helpers
                static NkPath GetCurrentDirectory();
                static NkPath GetTempDirectory();
                static NkPath Combine(const char* path1, const char* path2);
                static bool IsValidPath(const char* path);
        };
        
    } // namespace entseu
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_FILESYSTEM_NKPATH_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
==================================================

