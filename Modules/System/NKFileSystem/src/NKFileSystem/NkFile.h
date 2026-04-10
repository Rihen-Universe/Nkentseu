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
