// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\FileSystem\NkDirectory.cpp
// DESCRIPTION: Directory operations implementation
// AUTEUR: Rihen
// DATE: 2026-02-10
// -----------------------------------------------------------------------------

#include "NkDirectory.h"
#include "NkFile.h"
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
            if (!parent.ToString().IsEmpty() && !Exists(parent)) {
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
        
        bool NkDirectory::IsEmpty(const char* path) {
            if (!Exists(path)) return true;
            
            auto entries = GetEntries(path);
            return entries.IsEmpty();
        }
        
        bool NkDirectory::IsEmpty(const NkPath& path) {
            return IsEmpty(path.CStr());
        }
        
        bool NkDirectory::MatchesPattern(const char* name, const char* pattern) {
            // Simple pattern matching (* wildcard)
            if (strcmp(pattern, "*") == 0) return true;
            
            // TODO: Implement proper pattern matching
            // For now, just check if pattern is contained in name
            return strstr(name, pattern) != nullptr;
        }
        
        core::NkVector<core::NkString> NkDirectory::GetFiles(
            const char* path,
            const char* pattern,
            NkSearchOption option
        ) {
            core::NkVector<core::NkString> results;
            
            if (!path || !Exists(path)) return results;
            
            #ifdef _WIN32
            WIN32_FIND_DATAA findData;
            core::NkString searchPath = core::NkString(path) + "\\*";
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
            
            if (option == NkSearchOption::AllDirectories) {
                GetFilesRecursive(path, pattern, results);
            }
            
            return results;
        }
        
        core::NkVector<core::NkString> NkDirectory::GetFiles(
            const NkPath& path,
            const char* pattern,
            NkSearchOption option
        ) {
            return GetFiles(path.CStr(), pattern, option);
        }
        
        void NkDirectory::GetFilesRecursive(
            const char* path,
            const char* pattern,
            core::NkVector<core::NkString>& results
        ) {
            auto dirs = GetDirectories(path);
            for (auto& dir : dirs) {
                auto files = GetFiles(dir.CStr(), pattern, NkSearchOption::TopDirectoryOnly);
                for (auto& file : files) {
                    results.PushBack(file);
                }
                GetFilesRecursive(dir.CStr(), pattern, results);
            }
        }
        
        core::NkVector<core::NkString> NkDirectory::GetDirectories(
            const char* path,
            const char* pattern,
            NkSearchOption option
        ) {
            core::NkVector<core::NkString> results;
            
            if (!path || !Exists(path)) return results;
            
            #ifdef _WIN32
            WIN32_FIND_DATAA findData;
            core::NkString searchPath = core::NkString(path) + "\\*";
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
            
            if (option == NkSearchOption::AllDirectories) {
                GetDirectoriesRecursive(path, pattern, results);
            }
            
            return results;
        }
        
        core::NkVector<core::NkString> NkDirectory::GetDirectories(
            const NkPath& path,
            const char* pattern,
            NkSearchOption option
        ) {
            return GetDirectories(path.CStr(), pattern, option);
        }
        
        void NkDirectory::GetDirectoriesRecursive(
            const char* path,
            const char* pattern,
            core::NkVector<core::NkString>& results
        ) {
            auto dirs = GetDirectories(path);
            for (auto& dir : dirs) {
                auto subdirs = GetDirectories(dir.CStr(), pattern, NkSearchOption::TopDirectoryOnly);
                for (auto& subdir : subdirs) {
                    results.PushBack(subdir);
                }
                GetDirectoriesRecursive(dir.CStr(), pattern, results);
            }
        }
        
        core::NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
            const char* path,
            const char* pattern,
            NkSearchOption option
        ) {
            core::NkVector<NkDirectoryEntry> results;
            
            if (!path || !Exists(path)) return results;
            
            #ifdef _WIN32
            WIN32_FIND_DATAA findData;
            core::NkString searchPath = core::NkString(path) + "\\*";
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
            
            if (option == NkSearchOption::AllDirectories) {
                GetEntriesRecursive(path, pattern, results);
            }
            
            return results;
        }
        
        core::NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
            const NkPath& path,
            const char* pattern,
            NkSearchOption option
        ) {
            return GetEntries(path.CStr(), pattern, option);
        }
        
        void NkDirectory::GetEntriesRecursive(
            const char* path,
            const char* pattern,
            core::NkVector<NkDirectoryEntry>& results
        ) {
            auto dirs = GetDirectories(path);
            for (auto& dir : dirs) {
                auto entries = GetEntries(dir.CStr(), pattern, NkSearchOption::TopDirectoryOnly);
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
                return NkPath(core::NkString(homeDrive) + homePath);
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
            if (!home.ToString().IsEmpty()) {
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
