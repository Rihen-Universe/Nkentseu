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
