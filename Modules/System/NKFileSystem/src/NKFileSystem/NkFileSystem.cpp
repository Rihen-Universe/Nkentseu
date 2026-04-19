// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkFileSystem.cpp
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation des utilitaires systeme de fichiers.
// =============================================================================

#include "NKFileSystem/NkFileSystem.h"

// -- En-tetes C standard ------------------------------------------------------
#include <cstring>
#include <cstdlib>

// -- En-tetes plateforme ------------------------------------------------------
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
    #include <utime.h>
#endif

namespace nkentseu {

    // =========================================================================
    // Structures
    // =========================================================================

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

    // =========================================================================
    // Volumes et espace disque
    // =========================================================================

    NkVector<NkDriveInfo> NkFileSystem::GetDrives() {
        NkVector<NkDriveInfo> drives;

#ifdef _WIN32
        const DWORD driveMask = GetLogicalDrives();

        for (int i = 0; i < 26; ++i) {
            if (driveMask & (1 << i)) {
                char driveLetter[4] = {
                    static_cast<char>('A' + i),
                    ':',
                    '\\',
                    '\0'
                };

                NkDriveInfo info;
                info.Name = driveLetter;

                const UINT driveType = GetDriveTypeA(driveLetter);
                info.IsReady = (driveType != DRIVE_NO_ROOT_DIR);

                if (info.IsReady) {
                    char volumeName[MAX_PATH];
                    char fileSystemName[MAX_PATH];

                    if (GetVolumeInformationA(
                        driveLetter,
                        volumeName,
                        MAX_PATH,
                        NULL,
                        NULL,
                        NULL,
                        fileSystemName,
                        MAX_PATH
                    )) {
                        info.Label = volumeName;

                        if (strcmp(fileSystemName, "NTFS") == 0) {
                            info.Type = NkFileSystemType::NK_NTFS;
                        } else if (strcmp(fileSystemName, "FAT32") == 0) {
                            info.Type = NkFileSystemType::NK_FAT32;
                        } else if (strcmp(fileSystemName, "exFAT") == 0) {
                            info.Type = NkFileSystemType::NK_EXFAT;
                        }
                    }

                    ULARGE_INTEGER freeBytes;
                    ULARGE_INTEGER totalBytes;
                    ULARGE_INTEGER availBytes;

                    if (GetDiskFreeSpaceExA(driveLetter, &availBytes, &totalBytes, &freeBytes)) {
                        info.TotalSize = static_cast<nk_int64>(totalBytes.QuadPart);
                        info.FreeSpace = static_cast<nk_int64>(freeBytes.QuadPart);
                        info.AvailableSpace = static_cast<nk_int64>(availBytes.QuadPart);
                    }
                }

                drives.PushBack(info);
            }
        }
#else
        NkDriveInfo info;
        info.Name = "/";
        info.IsReady = true;

        struct statvfs st;
        if (statvfs("/", &st) == 0) {
            info.TotalSize = static_cast<nk_int64>(st.f_blocks) * static_cast<nk_int64>(st.f_frsize);
            info.FreeSpace = static_cast<nk_int64>(st.f_bfree) * static_cast<nk_int64>(st.f_frsize);
            info.AvailableSpace = static_cast<nk_int64>(st.f_bavail) * static_cast<nk_int64>(st.f_frsize);
        }

        drives.PushBack(info);
#endif

        return drives;
    }

    NkDriveInfo NkFileSystem::GetDriveInfo(const char* path) {
        if (!path) {
            return NkDriveInfo();
        }

#ifdef _WIN32
        if (path[0] && path[1] == ':') {
            char driveLetter[4] = {
                path[0],
                ':',
                '\\',
                '\0'
            };

            NkVector<NkDriveInfo> drives = GetDrives();
            for (usize i = 0; i < drives.Size(); ++i) {
                if (_stricmp(drives[i].Name.CStr(), driveLetter) == 0) {
                    return drives[i];
                }
            }
        }
#else
        NkVector<NkDriveInfo> drives = GetDrives();
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
        return GetDriveInfo(path).TotalSize;
    }

    nk_int64 NkFileSystem::GetTotalSpace(const NkPath& path) {
        return GetTotalSpace(path.CStr());
    }

    nk_int64 NkFileSystem::GetFreeSpace(const char* path) {
        return GetDriveInfo(path).FreeSpace;
    }

    nk_int64 NkFileSystem::GetFreeSpace(const NkPath& path) {
        return GetFreeSpace(path.CStr());
    }

    nk_int64 NkFileSystem::GetAvailableSpace(const char* path) {
        return GetDriveInfo(path).AvailableSpace;
    }

    nk_int64 NkFileSystem::GetAvailableSpace(const NkPath& path) {
        return GetAvailableSpace(path.CStr());
    }

    // =========================================================================
    // Attributs
    // =========================================================================

    NkFileAttributes NkFileSystem::GetFileAttributes(const char* path) {
        NkFileAttributes attrs;

        if (!path) {
            return attrs;
        }

#ifdef _WIN32
        const DWORD winAttrs = GetFileAttributesA(path);

        if (winAttrs != INVALID_FILE_ATTRIBUTES) {
            attrs.IsReadOnly = (winAttrs & FILE_ATTRIBUTE_READONLY) != 0;
            attrs.IsHidden = (winAttrs & FILE_ATTRIBUTE_HIDDEN) != 0;
            attrs.IsSystem = (winAttrs & FILE_ATTRIBUTE_SYSTEM) != 0;
            attrs.IsArchive = (winAttrs & FILE_ATTRIBUTE_ARCHIVE) != 0;
            attrs.IsCompressed = (winAttrs & FILE_ATTRIBUTE_COMPRESSED) != 0;
            attrs.IsEncrypted = (winAttrs & FILE_ATTRIBUTE_ENCRYPTED) != 0;
        }

        HANDLE hFile = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            FILETIME creation;
            FILETIME access;
            FILETIME write;

            if (GetFileTime(hFile, &creation, &access, &write)) {
                attrs.CreationTime =
                    (static_cast<nk_int64>(creation.dwHighDateTime) << 32) |
                    static_cast<nk_int64>(creation.dwLowDateTime);
                attrs.LastAccessTime =
                    (static_cast<nk_int64>(access.dwHighDateTime) << 32) |
                    static_cast<nk_int64>(access.dwLowDateTime);
                attrs.LastWriteTime =
                    (static_cast<nk_int64>(write.dwHighDateTime) << 32) |
                    static_cast<nk_int64>(write.dwLowDateTime);
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
        if (!path) {
            return false;
        }

#ifdef _WIN32
        DWORD winAttrs = 0;
        if (attrs.IsReadOnly) {
            winAttrs |= FILE_ATTRIBUTE_READONLY;
        }
        if (attrs.IsHidden) {
            winAttrs |= FILE_ATTRIBUTE_HIDDEN;
        }
        if (attrs.IsSystem) {
            winAttrs |= FILE_ATTRIBUTE_SYSTEM;
        }
        if (attrs.IsArchive) {
            winAttrs |= FILE_ATTRIBUTE_ARCHIVE;
        }

        return SetFileAttributesA(path, winAttrs) != 0;
#else
        struct stat st;
        if (stat(path, &st) != 0) {
            return false;
        }

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
        (void)path;
        (void)hidden;
        return false;
#endif
    }

    bool NkFileSystem::SetHidden(const NkPath& path, bool hidden) {
        return SetHidden(path.CStr(), hidden);
    }

    // =========================================================================
    // Timestamps
    // =========================================================================

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

    bool NkFileSystem::SetCreationTime(const char* path, nk_int64 time) {
#ifdef _WIN32
        HANDLE hFile = CreateFileA(
            path,
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }

        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(time & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>((time >> 32) & 0xFFFFFFFF);
        const BOOL ok = SetFileTime(hFile, &ft, NULL, NULL);
        CloseHandle(hFile);
        return ok != 0;
#else
        (void)path;
        (void)time;
        return false;
#endif
    }

    bool NkFileSystem::SetCreationTime(const NkPath& path, nk_int64 time) {
        return SetCreationTime(path.CStr(), time);
    }

    bool NkFileSystem::SetLastAccessTime(const char* path, nk_int64 time) {
#ifdef _WIN32
        HANDLE hFile = CreateFileA(
            path,
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }

        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(time & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>((time >> 32) & 0xFFFFFFFF);
        const BOOL ok = SetFileTime(hFile, NULL, &ft, NULL);
        CloseHandle(hFile);
        return ok != 0;
#else
        struct stat st;
        if (stat(path, &st) != 0) {
            return false;
        }

        struct utimbuf tb;
        tb.actime = static_cast<time_t>(time);
        tb.modtime = st.st_mtime;
        return utime(path, &tb) == 0;
#endif
    }

    bool NkFileSystem::SetLastAccessTime(const NkPath& path, nk_int64 time) {
        return SetLastAccessTime(path.CStr(), time);
    }

    bool NkFileSystem::SetLastWriteTime(const char* path, nk_int64 time) {
#ifdef _WIN32
        HANDLE hFile = CreateFileA(
            path,
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }

        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(time & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>((time >> 32) & 0xFFFFFFFF);
        const BOOL ok = SetFileTime(hFile, NULL, NULL, &ft);
        CloseHandle(hFile);
        return ok != 0;
#else
        struct stat st;
        if (stat(path, &st) != 0) {
            return false;
        }

        struct utimbuf tb;
        tb.actime = st.st_atime;
        tb.modtime = static_cast<time_t>(time);
        return utime(path, &tb) == 0;
#endif
    }

    bool NkFileSystem::SetLastWriteTime(const NkPath& path, nk_int64 time) {
        return SetLastWriteTime(path.CStr(), time);
    }

    // =========================================================================
    // Validation / chemins
    // =========================================================================

    bool NkFileSystem::IsValidFileName(const char* name) {
        if (!name || !name[0]) {
            return false;
        }

        const char* invalid = "<>:\"|?*";

        for (const char* p = name; *p; ++p) {
            if (strchr(invalid, *p)) {
                return false;
            }

            if (*p < 32) {
                return false;
            }
        }

#ifdef _WIN32
        static const char* reserved[] = {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
        };

        const usize kCount = sizeof(reserved) / sizeof(reserved[0]);

        for (usize i = 0; i < kCount; ++i) {
            if (_stricmp(name, reserved[i]) == 0) {
                return false;
            }
        }
#endif

        return true;
    }

    bool NkFileSystem::IsValidPath(const char* path) {
        return NkPath::IsValidPath(path);
    }

    NkString NkFileSystem::GetAbsolutePath(const char* path) {
        if (!path) {
            return NkString();
        }

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
        (void)from;
        return NkString(to ? to : "");
    }

    NkString NkFileSystem::GetRelativePath(const NkPath& from, const NkPath& to) {
        return GetRelativePath(from.CStr(), to.CStr());
    }

    // =========================================================================
    // Infos FS
    // =========================================================================

    NkFileSystemType NkFileSystem::GetFileSystemType(const char* path) {
        return GetDriveInfo(path).Type;
    }

    NkFileSystemType NkFileSystem::GetFileSystemType(const NkPath& path) {
        return GetFileSystemType(path.CStr());
    }

    bool NkFileSystem::IsCaseSensitive(const char* path) {
        (void)path;
#ifdef _WIN32
        return false;
#else
        return true;
#endif
    }

    bool NkFileSystem::IsCaseSensitive(const NkPath& path) {
        return IsCaseSensitive(path.CStr());
    }

    // =========================================================================
    // Liens symboliques
    // =========================================================================

    bool NkFileSystem::IsSymbolicLink(const char* path) {
        if (!path) {
            return false;
        }

#ifdef _WIN32
        const DWORD attrs = GetFileAttributesA(path);
        return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
#else
        struct stat st;
        return (lstat(path, &st) == 0) && S_ISLNK(st.st_mode);
#endif
    }

    bool NkFileSystem::IsSymbolicLink(const NkPath& path) {
        return IsSymbolicLink(path.CStr());
    }

    NkString NkFileSystem::GetSymbolicLinkTarget(const char* path) {
        if (!path) {
            return NkString();
        }

#ifdef _WIN32
        return NkString();
#else
        char buffer[PATH_MAX];
        const ssize_t len = readlink(path, buffer, sizeof(buffer) - 1);

        if (len > 0) {
            buffer[len] = '\0';
            return NkString(buffer);
        }

        return NkString();
#endif
    }

    NkString NkFileSystem::GetSymbolicLinkTarget(const NkPath& path) {
        return GetSymbolicLinkTarget(path.CStr());
    }

    bool NkFileSystem::CreateSymbolicLink(const char* link, const char* target) {
        if (!link || !target) {
            return false;
        }

#ifdef _WIN32
        return CreateSymbolicLinkA(link, target, 0) != 0;
#else
        return symlink(target, link) == 0;
#endif
    }

    bool NkFileSystem::CreateSymbolicLink(const NkPath& link, const NkPath& target) {
        return CreateSymbolicLink(link.CStr(), target.CStr());
    }

} // namespace nkentseu

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
