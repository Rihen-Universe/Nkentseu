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
