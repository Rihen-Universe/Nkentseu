// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkDirectory.h
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Classe utilitaire NkDirectory pour la creation, suppression, parcours
//   et copie de repertoires. API orientee utilitaire statique.
// =============================================================================

#pragma once

#ifndef NK_FILESYSTEM_NKDIRECTORY_H_INCLUDED
#define NK_FILESYSTEM_NKDIRECTORY_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {

    // =========================================================================
    // Structure : NkDirectoryEntry
    // =========================================================================
    struct NkDirectoryEntry {
        NkString Name;
        NkPath FullPath;
        bool IsDirectory;
        bool IsFile;
        nk_int64 Size;
        nk_int64 ModificationTime;

        NkDirectoryEntry();
    };

    // =========================================================================
    // Enum : NkSearchOption
    // =========================================================================
    enum class NkSearchOption {
        NK_TOP_DIRECTORY_ONLY,
        NK_ALL_DIRECTORIES
    };

    // =========================================================================
    // Classe : NkDirectory
    // =========================================================================
    class NkDirectory {

        // -- Section publique --------------------------------------------------
        public:

            // -----------------------------------------------------------------
            // Operations de repertoire
            // -----------------------------------------------------------------
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

            // -----------------------------------------------------------------
            // Enumeration
            // -----------------------------------------------------------------
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

            // -----------------------------------------------------------------
            // Copie / deplacement
            // -----------------------------------------------------------------
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

            // -----------------------------------------------------------------
            // Repertoires speciaux
            // -----------------------------------------------------------------
            static NkPath GetCurrentDirectory();
            static bool SetCurrentDirectory(const char* path);
            static bool SetCurrentDirectory(const NkPath& path);

            static NkPath GetTempDirectory();
            static NkPath GetHomeDirectory();
            static NkPath GetAppDataDirectory();

        // -- Section privee ----------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Compatibilite legacy : nkentseu::entseu::*
    // -------------------------------------------------------------------------
    namespace entseu {
        using NkDirectoryEntry = ::nkentseu::NkDirectoryEntry;
        using NkSearchOption = ::nkentseu::NkSearchOption;
        using NkDirectory = ::nkentseu::NkDirectory;
    } // namespace entseu

} // namespace nkentseu

#endif // NK_FILESYSTEM_NKDIRECTORY_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkDirectory
// =============================================================================
//
// -- Creation et suppression --------------------------------------------------
//
//   NkDirectory::CreateRecursive("data/cache/images");
//   bool exists = NkDirectory::Exists("data/cache");
//   bool removed = NkDirectory::Delete("data/cache", true);
//
// -- Enumeration --------------------------------------------------------------
//
//   NkVector<NkString> files = NkDirectory::GetFiles(
//       "assets",
//       "*.png",
//       NkSearchOption::NK_ALL_DIRECTORIES
//   );
//
// -- Dossiers speciaux --------------------------------------------------------
//
//   NkPath cwd = NkDirectory::GetCurrentDirectory();
//   NkPath tmp = NkDirectory::GetTempDirectory();
//   NkPath home = NkDirectory::GetHomeDirectory();
//
// =============================================================================
