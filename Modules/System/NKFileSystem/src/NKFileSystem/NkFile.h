// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkFile.h
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Classe NkFile - operations de lecture/ecriture de fichiers avec RAII.
//   Le fichier est automatiquement ferme a la destruction de l'objet.
//   Pas de dependance STL (fstream/string) dans l'API.
// =============================================================================

#pragma once

#ifndef NK_FILESYSTEM_NKFILE_H_INCLUDED
#define NK_FILESYSTEM_NKFILE_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {

    // =========================================================================
    // Enum : NkFileMode
    // =========================================================================
    enum class NkFileMode : uint32 {
        NK_NONE                 = 0,

        // Accès
        NK_READ                 = 1 << 0,                               // "r"
        NK_WRITE                = 1 << 1,                               // "w"

        // Comportement
        NK_APPEND               = 1 << 2,                               // "a"
        NK_TRUNCATE             = 1 << 3,                               // 

        // Format
        NK_BINARY               = 1 << 4,                               // "b"

        // Lecture seule
        NK_READ_BINARY          = NK_READ | NK_BINARY,                  // "rb"

        // Écriture (overwrite)
        NK_WRITE_BINARY         = NK_WRITE | NK_BINARY,                 // "wb"

        // Ajout (append)
        NK_APPEND_BINARY        = NK_APPEND | NK_BINARY,                // "ab"

        // Lecture + écriture
        NK_READ_WRITE           = NK_READ | NK_WRITE,                   / "r+"
        NK_READ_WRITE_BINARY    = NK_READ_WRITE | NK_BINARY,            // "rb+" (ou "r+b")

        // Écriture + lecture (truncate)
        NK_WRITE_READ           = NK_WRITE | NK_READ,                   // "w+"
        NK_WRITE_READ_BINARY    = NK_WRITE_READ | NK_BINARY,            // "wb+" (ou "w+b")

        // Lecture + append
        NK_READ_APPEND          = NK_READ | NK_APPEND,                  // "a+"
        NK_READ_APPEND_BINARY   = NK_READ | NK_APPEND | NK_BINARY       // "ab+" (ou "a+b")
    };

    using NkFileModeFlags = NkFileMode;

    inline NkFileModeFlags operator|(NkFileModeFlags a, NkFileModeFlags b) {
        return static_cast<NkFileModeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline NkFileModeFlags operator&(NkFileModeFlags a, NkFileModeFlags b) {
        return static_cast<NkFileModeFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline bool NkHasFlag(NkFileModeFlags value, NkFileModeFlags flag) {
        return (value & flag) != NkFileModeFlags::NK_NONE;
    }

    // =========================================================================
    // Enum : NkSeekOrigin
    // =========================================================================
    enum class NkSeekOrigin {
        NK_BEGIN,
        NK_CURRENT,
        NK_END
    };

    // =========================================================================
    // Classe : NkFile
    // =========================================================================
    // Encapsule un descripteur fichier (FILE*) derriere void* dans l'API.
    // Politique RAII : Close() est appele par le destructeur.
    // =========================================================================
    class NkFile {

        // -- Section privee ----------------------------------------------------
        private:

            // Descripteur opaque (en pratique FILE*).
            void* mHandle;

            // Chemin associe au fichier ouvert.
            NkPath mPath;

            // Mode d'ouverture actuel.
            NkFileMode mMode;

            // Etat d'ouverture.
            bool mIsOpen;

            // Retourne le mode C correspondant a mMode (rb/wb/ab/rb+/ab+).
            const char* GetModeString() const;

        // -- Section publique --------------------------------------------------
        public:

            // -----------------------------------------------------------------
            // Constructeurs / destructeur
            // -----------------------------------------------------------------
            NkFile();
            explicit NkFile(const char* path, NkFileMode mode = NkFileMode::NK_READ);
            explicit NkFile(const NkPath& path, NkFileMode mode = NkFileMode::NK_READ);
            ~NkFile();

            // -----------------------------------------------------------------
            // Copie / deplacement
            // -----------------------------------------------------------------
            NkFile(const NkFile&) = delete;
            NkFile& operator=(const NkFile&) = delete;

            NkFile(NkFile&& other) noexcept;
            NkFile& operator=(NkFile&& other) noexcept;

            // -----------------------------------------------------------------
            // Ouverture / fermeture
            // -----------------------------------------------------------------
            bool Open(const char* path, NkFileMode mode = NkFileMode::NK_READ);
            bool Open(const NkPath& path, NkFileMode mode = NkFileMode::NK_READ);
            void Close();
            bool IsOpen() const;

            // -----------------------------------------------------------------
            // Lecture
            // -----------------------------------------------------------------
            usize Read(void* buffer, usize size);
            NkString ReadLine();
            NkString ReadAll();
            NkVector<NkString> ReadLines();

            // -----------------------------------------------------------------
            // Ecriture
            // -----------------------------------------------------------------
            usize Write(const void* data, usize size);
            bool WriteLine(const char* text);
            bool Write(const NkString& text);

            // -----------------------------------------------------------------
            // Position du curseur
            // -----------------------------------------------------------------
            nk_int64 Tell() const;
            bool Seek(nk_int64 offset, NkSeekOrigin origin = NkSeekOrigin::NK_BEGIN);
            bool SeekToBegin();
            bool SeekToEnd();
            nk_int64 GetSize() const;
            nk_int64 Size() const { return GetSize(); }

            // -----------------------------------------------------------------
            // Buffers
            // -----------------------------------------------------------------
            void Flush();

            // -----------------------------------------------------------------
            // Proprietes
            // -----------------------------------------------------------------
            const NkPath& GetPath() const;
            NkFileMode GetMode() const;
            bool IsEOF() const;

            // -----------------------------------------------------------------
            // Utilitaires statiques
            // -----------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Compatibilite legacy : nkentseu::entseu::*
    // -------------------------------------------------------------------------
    namespace entseu {
        using NkFileMode = ::nkentseu::NkFileMode;
        using NkSeekOrigin = ::nkentseu::NkSeekOrigin;
        using NkFile = ::nkentseu::NkFile;
    } // namespace entseu

} // namespace nkentseu

#endif // NK_FILESYSTEM_NKFILE_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkFile
// =============================================================================
//
// -- Ecriture ligne par ligne -------------------------------------------------
//
//   NkFile f("log.txt", NkFileMode::NK_WRITE);
//   if (f.IsOpen()) {
//       f.WriteLine("Demarrage application");
//       f.WriteLine("Version 1.0.0");
//       f.Flush();
//   }
//
// -- Lecture complete ---------------------------------------------------------
//
//   NkFile src("config.json", NkFileMode::NK_READ);
//   if (src.IsOpen()) {
//       NkString content = src.ReadAll();
//   }
//
// -- Utilitaires statiques ----------------------------------------------------
//
//   bool ok = NkFile::WriteAllText("settings.cfg", "quality=high");
//   NkString text = NkFile::ReadAllText("settings.cfg");
//   nk_int64 size = NkFile::GetFileSize("settings.cfg");
//
// =============================================================================
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
