// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkDirectory.cpp
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation des operations de repertoire : creation, suppression,
//   enumeration, copie et deplacement.
// =============================================================================

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

// -- En-tetes C standard ------------------------------------------------------
#include <cstdio>
#include <cstring>
#include <cstdlib>

// -- En-tetes plateforme ------------------------------------------------------
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

    // =========================================================================
    // Structures
    // =========================================================================

    NkDirectoryEntry::NkDirectoryEntry()
        : Name()
        , FullPath()
        , IsDirectory(false)
        , IsFile(false)
        , Size(0)
        , ModificationTime(0) {
    }

    // =========================================================================
    // Operations de base
    // =========================================================================

    bool NkDirectory::Create(const char* path) {
        if (!path || Exists(path)) {
            return false;
        }

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
        if (!path) {
            return false;
        }

        if (Exists(path)) {
            return true;
        }

        NkPath parent = NkPath(path).GetParent();

        if (!parent.ToString().Empty() && !Exists(parent)) {
            if (!CreateRecursive(parent.CStr())) {
                return false;
            }
        }

        return Create(path);
    }

    bool NkDirectory::CreateRecursive(const NkPath& path) {
        return CreateRecursive(path.CStr());
    }

    bool NkDirectory::Delete(const char* path, bool recursive) {
        if (!path || !Exists(path)) {
            return false;
        }

        if (recursive) {
            NkVector<NkString> files = GetFiles(path);
            for (usize i = 0; i < files.Size(); ++i) {
                NkFile::Delete(files[i].CStr());
            }

            NkVector<NkString> dirs = GetDirectories(path);
            for (usize i = 0; i < dirs.Size(); ++i) {
                Delete(dirs[i].CStr(), true);
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
        if (!path) {
            return false;
        }

#ifdef _WIN32
        const DWORD attrs = GetFileAttributesA(path);
        return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
        struct stat st;
        return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
#endif
    }

    bool NkDirectory::Exists(const NkPath& path) {
        return Exists(path.CStr());
    }

    bool NkDirectory::Empty(const char* path) {
        if (!Exists(path)) {
            return true;
        }

        NkVector<NkDirectoryEntry> entries = GetEntries(path);
        return entries.Empty();
    }

    bool NkDirectory::Empty(const NkPath& path) {
        return Empty(path.CStr());
    }

    // =========================================================================
    // Filtres et recursivite
    // =========================================================================

    bool NkDirectory::MatchesPattern(const char* name, const char* pattern) {
        if (!name || !pattern) {
            return false;
        }

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

    void NkDirectory::GetFilesRecursive(
        const char* path,
        const char* pattern,
        NkVector<NkString>& results
    ) {
        NkVector<NkString> dirs = GetDirectories(path);

        for (usize i = 0; i < dirs.Size(); ++i) {
            NkVector<NkString> files = GetFiles(dirs[i].CStr(), pattern, NkSearchOption::NK_TOP_DIRECTORY_ONLY);

            for (usize j = 0; j < files.Size(); ++j) {
                results.PushBack(files[j]);
            }

            GetFilesRecursive(dirs[i].CStr(), pattern, results);
        }
    }

    void NkDirectory::GetDirectoriesRecursive(
        const char* path,
        const char* pattern,
        NkVector<NkString>& results
    ) {
        NkVector<NkString> dirs = GetDirectories(path);

        for (usize i = 0; i < dirs.Size(); ++i) {
            NkVector<NkString> subdirs = GetDirectories(
                dirs[i].CStr(),
                pattern,
                NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            for (usize j = 0; j < subdirs.Size(); ++j) {
                results.PushBack(subdirs[j]);
            }

            GetDirectoriesRecursive(dirs[i].CStr(), pattern, results);
        }
    }

    void NkDirectory::GetEntriesRecursive(
        const char* path,
        const char* pattern,
        NkVector<NkDirectoryEntry>& results
    ) {
        NkVector<NkString> dirs = GetDirectories(path);

        for (usize i = 0; i < dirs.Size(); ++i) {
            NkVector<NkDirectoryEntry> entries = GetEntries(
                dirs[i].CStr(),
                pattern,
                NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            for (usize j = 0; j < entries.Size(); ++j) {
                results.PushBack(entries[j]);
            }

            GetEntriesRecursive(dirs[i].CStr(), pattern, results);
        }
    }

    // =========================================================================
    // Enumeration
    // =========================================================================

    NkVector<NkString> NkDirectory::GetFiles(
        const char* path,
        const char* pattern,
        NkSearchOption option
    ) {
        NkVector<NkString> results;

        if (!path || !Exists(path)) {
            return results;
        }

#ifdef _WIN32
        WIN32_FIND_DATAA findData;
        NkString searchPath = NkString(path) + "\\*";
        HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) {
            return results;
        }

        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
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
        if (!dir) {
            return results;
        }

        struct dirent* entry = nullptr;

        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
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

    NkVector<NkString> NkDirectory::GetDirectories(
        const char* path,
        const char* pattern,
        NkSearchOption option
    ) {
        NkVector<NkString> results;

        if (!path || !Exists(path)) {
            return results;
        }

#ifdef _WIN32
        WIN32_FIND_DATAA findData;
        NkString searchPath = NkString(path) + "\\*";
        HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) {
            return results;
        }

        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
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
        if (!dir) {
            return results;
        }

        struct dirent* entry = nullptr;

        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
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

    NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
        const char* path,
        const char* pattern,
        NkSearchOption option
    ) {
        NkVector<NkDirectoryEntry> results;

        if (!path || !Exists(path)) {
            return results;
        }

#ifdef _WIN32
        WIN32_FIND_DATAA findData;
        NkString searchPath = NkString(path) + "\\*";
        HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);

        if (hFind == INVALID_HANDLE_VALUE) {
            return results;
        }

        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                continue;
            }

            if (MatchesPattern(findData.cFileName, pattern)) {
                NkDirectoryEntry entry;
                entry.Name = findData.cFileName;
                entry.FullPath = NkPath(path) / findData.cFileName;
                entry.IsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry.IsFile = !entry.IsDirectory;
                entry.Size = (static_cast<nk_int64>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                results.PushBack(entry);
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
#else
        DIR* dir = opendir(path);
        if (!dir) {
            return results;
        }

        struct dirent* entry = nullptr;

        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
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

    // =========================================================================
    // Copie / deplacement
    // =========================================================================

    bool NkDirectory::Copy(
        const char* source,
        const char* dest,
        bool recursive,
        bool overwrite
    ) {
        if (!source || !dest || !Exists(source)) {
            return false;
        }

        if (!Exists(dest)) {
            if (!CreateRecursive(dest)) {
                return false;
            }
        }

        NkVector<NkString> files = GetFiles(source);
        for (usize i = 0; i < files.Size(); ++i) {
            NkPath filename = NkPath(files[i]).GetFileName();
            NkPath destFile = NkPath(dest) / filename;

            if (!NkFile::Copy(files[i].CStr(), destFile.CStr(), overwrite)) {
                return false;
            }
        }

        if (recursive) {
            NkVector<NkString> dirs = GetDirectories(source);

            for (usize i = 0; i < dirs.Size(); ++i) {
                NkPath dirname = NkPath(dirs[i]).GetFileName();
                NkPath destDir = NkPath(dest) / dirname;

                if (!Copy(dirs[i].CStr(), destDir.CStr(), true, overwrite)) {
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
        if (!source || !dest || !Exists(source)) {
            return false;
        }

        return rename(source, dest) == 0;
    }

    bool NkDirectory::Move(const NkPath& source, const NkPath& dest) {
        return Move(source.CStr(), dest.CStr());
    }

    // =========================================================================
    // Repertoires speciaux
    // =========================================================================

    NkPath NkDirectory::GetCurrentDirectory() {
        return NkPath::GetCurrentDirectory();
    }

    bool NkDirectory::SetCurrentDirectory(const char* path) {
        if (!path) {
            return false;
        }

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
        if (home) {
            return NkPath(home);
        }

        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            return NkPath(pw->pw_dir);
        }
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

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
