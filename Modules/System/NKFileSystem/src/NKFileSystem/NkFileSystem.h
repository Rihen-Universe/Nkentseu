// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkFileSystem.h
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Outils systeme de fichiers : lecteurs de metadonnees, espace disque,
//   attributs de fichier et liens symboliques.
// =============================================================================

#pragma once

#ifndef NK_FILESYSTEM_NKFILESYSTEM_H_INCLUDED
#define NK_FILESYSTEM_NKFILESYSTEM_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {

    // =========================================================================
    // Enum : NkFileSystemType
    // =========================================================================
    // NOTE: la valeur NK_UNKNOW est conservee pour compatibilite API.
    enum class NkFileSystemType {
        NK_UNKNOW,
        NK_NTFS,
        NK_FAT32,
        NK_EXFAT,
        NK_EXT4,
        NK_EXT3,
        NK_HFS,
        NK_APFS,
        NK_NETWORK
    };

    // =========================================================================
    // Structure : NkDriveInfo
    // =========================================================================
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

    // =========================================================================
    // Structure : NkFileAttributes
    // =========================================================================
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

    // =========================================================================
    // Classe : NkFileSystem
    // =========================================================================
    class NkFileSystem {

        // -- Section publique --------------------------------------------------
        public:

            // -----------------------------------------------------------------
            // Lecteurs de volumes
            // -----------------------------------------------------------------
            static NkVector<NkDriveInfo> GetDrives();
            static NkDriveInfo GetDriveInfo(const char* path);
            static NkDriveInfo GetDriveInfo(const NkPath& path);

            // -----------------------------------------------------------------
            // Information d'espace disque
            // -----------------------------------------------------------------
            static nk_int64 GetTotalSpace(const char* path);
            static nk_int64 GetTotalSpace(const NkPath& path);
            static nk_int64 GetFreeSpace(const char* path);
            static nk_int64 GetFreeSpace(const NkPath& path);
            static nk_int64 GetAvailableSpace(const char* path);
            static nk_int64 GetAvailableSpace(const NkPath& path);

            // -----------------------------------------------------------------
            // Attributs
            // -----------------------------------------------------------------
            static NkFileAttributes GetFileAttributes(const char* path);
            static NkFileAttributes GetFileAttributes(const NkPath& path);
            static bool SetFileAttributes(const char* path, const NkFileAttributes& attrs);
            static bool SetFileAttributes(const NkPath& path, const NkFileAttributes& attrs);

            static bool SetReadOnly(const char* path, bool readOnly);
            static bool SetReadOnly(const NkPath& path, bool readOnly);
            static bool SetHidden(const char* path, bool hidden);
            static bool SetHidden(const NkPath& path, bool hidden);

            // -----------------------------------------------------------------
            // Timestamps
            // -----------------------------------------------------------------
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

            // -----------------------------------------------------------------
            // Validation et conversion de chemins
            // -----------------------------------------------------------------
            static bool IsValidFileName(const char* name);
            static bool IsValidPath(const char* path);

            static NkString GetAbsolutePath(const char* path);
            static NkString GetAbsolutePath(const NkPath& path);
            static NkString GetRelativePath(const char* from, const char* to);
            static NkString GetRelativePath(const NkPath& from, const NkPath& to);

            // -----------------------------------------------------------------
            // Infos systeme de fichiers
            // -----------------------------------------------------------------
            static NkFileSystemType GetFileSystemType(const char* path);
            static NkFileSystemType GetFileSystemType(const NkPath& path);
            static bool IsCaseSensitive(const char* path);
            static bool IsCaseSensitive(const NkPath& path);

            // -----------------------------------------------------------------
            // Liens symboliques
            // -----------------------------------------------------------------
            static bool IsSymbolicLink(const char* path);
            static bool IsSymbolicLink(const NkPath& path);
            static NkString GetSymbolicLinkTarget(const char* path);
            static NkString GetSymbolicLinkTarget(const NkPath& path);
            static bool CreateSymbolicLink(const char* link, const char* target);
            static bool CreateSymbolicLink(const NkPath& link, const NkPath& target);
    };

    // -------------------------------------------------------------------------
    // Compatibilite legacy : nkentseu::entseu::*
    // -------------------------------------------------------------------------
    namespace entseu {
        using NkFileSystemType = ::nkentseu::NkFileSystemType;
        using NkDriveInfo = ::nkentseu::NkDriveInfo;
        using NkFileAttributes = ::nkentseu::NkFileAttributes;
        using NkFileSystem = ::nkentseu::NkFileSystem;
    } // namespace entseu

} // namespace nkentseu

#endif // NK_FILESYSTEM_NKFILESYSTEM_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkFileSystem
// =============================================================================
//
// -- Lecture des volumes ------------------------------------------------------
//
//   NkVector<NkDriveInfo> drives = NkFileSystem::GetDrives();
//   for (usize i = 0; i < drives.Size(); ++i) {
//       const NkDriveInfo& d = drives[i];
//       // d.Name, d.TotalSize, d.FreeSpace...
//   }
//
// -- Attributs ---------------------------------------------------------------
//
//   NkFileAttributes attrs = NkFileSystem::GetFileAttributes("save.dat");
//   attrs.IsReadOnly = true;
//   NkFileSystem::SetFileAttributes("save.dat", attrs);
//
// -- Liens symboliques --------------------------------------------------------
//
//   if (!NkFileSystem::IsSymbolicLink("current-save")) {
//       NkFileSystem::CreateSymbolicLink("current-save", "save_v2.dat");
//   }
//
// =============================================================================
