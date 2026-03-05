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
#include "NkPath.h"

namespace nkentseu {
    namespace entseu {
        
        /**
         * @brief Directory entry information
         */
        struct NkDirectoryEntry {
            core::NkString Name;
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
            TopDirectoryOnly,   // Only search current directory
            AllDirectories      // Search recursively
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
            
            static bool IsEmpty(const char* path);
            static bool IsEmpty(const NkPath& path);
            
            // Enumeration
            static core::NkVector<core::NkString> GetFiles(
                const char* path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::TopDirectoryOnly
            );
            
            static core::NkVector<core::NkString> GetFiles(
                const NkPath& path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::TopDirectoryOnly
            );
            
            static core::NkVector<core::NkString> GetDirectories(
                const char* path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::TopDirectoryOnly
            );
            
            static core::NkVector<core::NkString> GetDirectories(
                const NkPath& path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::TopDirectoryOnly
            );
            
            static core::NkVector<NkDirectoryEntry> GetEntries(
                const char* path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::TopDirectoryOnly
            );
            
            static core::NkVector<NkDirectoryEntry> GetEntries(
                const NkPath& path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::TopDirectoryOnly
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
                core::NkVector<core::NkString>& results
            );
            static void GetDirectoriesRecursive(
                const char* path,
                const char* pattern,
                core::NkVector<core::NkString>& results
            );
            static void GetEntriesRecursive(
                const char* path,
                const char* pattern,
                core::NkVector<NkDirectoryEntry>& results
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
