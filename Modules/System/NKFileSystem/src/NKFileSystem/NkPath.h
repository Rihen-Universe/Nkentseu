// =============================================================================
// FICHIER  : Modules/System/NKFileSystem/src/NKFileSystem/NkPath.h
// MODULE   : NKFileSystem
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Classe NkPath - manipulation cross-platform des chemins de fichiers.
//   Supporte la notation Windows (C:\\path\\file.txt) et Unix (/path/file.txt).
//   Le separateur interne prefere est toujours '/'.
// =============================================================================

#pragma once

#ifndef NK_FILESYSTEM_NKPATH_H_INCLUDED
#define NK_FILESYSTEM_NKPATH_H_INCLUDED

// -- Dependances internes -----------------------------------------------------
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // =========================================================================
    // Classe : NkPath
    // =========================================================================
    // Represente un chemin de systeme de fichiers (fichier ou repertoire).
    // Toutes les operations de jointure, decomposition et normalisation sont
    // fournies sans utiliser std::filesystem.
    // =========================================================================
    class NkPath {

        // -- Section privee ----------------------------------------------------
        private:

            // Chaine interne stockant le chemin normalise.
            NkString mPath;

            // Separateur universel utilise en interne.
            static constexpr char PREFERRED_SEPARATOR = '/';

            // Separateur Windows, utile pour la normalisation.
            static constexpr char WINDOWS_SEPARATOR   = '\\';

            // Normalise les separateurs internes vers '/'.
            void NormalizeSeparators();

        // -- Section publique --------------------------------------------------
        public:

            // -----------------------------------------------------------------
            // Constructeurs
            // -----------------------------------------------------------------
            NkPath();
            NkPath(const char* path);
            NkPath(const NkString& path);
            NkPath(const NkPath& other);

            // -----------------------------------------------------------------
            // Operateurs d'affectation
            // -----------------------------------------------------------------
            NkPath& operator=(const NkPath& other);
            NkPath& operator=(const char* path);
            NkPath& operator=(const NkString& path);

            // -----------------------------------------------------------------
            // Operations de jointure
            // -----------------------------------------------------------------
            NkPath& Append(const char* component);
            NkPath& Append(const NkPath& other);
            NkPath& Append(const NkString& other);
            NkPath operator/(const NkString& component) const;
            NkPath operator/(const char* component) const;
            NkPath operator/(const NkPath& other) const;

            // -----------------------------------------------------------------
            // Decomposition du chemin
            // -----------------------------------------------------------------
            NkString GetDirectory() const;
            NkString GetFileName() const;
            NkString GetFileNameWithoutExtension() const;
            NkString GetExtension() const;
            NkString GetRoot() const;

            // -----------------------------------------------------------------
            // Proprietes du chemin
            // -----------------------------------------------------------------
            bool IsAbsolute() const;
            bool IsRelative() const;
            bool HasExtension() const;
            bool HasFileName() const;

            // -----------------------------------------------------------------
            // Modification du chemin
            // -----------------------------------------------------------------
            NkPath& ReplaceExtension(const char* newExt);
            NkPath& ReplaceFileName(const char* newName);
            NkPath& ReplaceExtension(const NkString& newExt);
            NkPath& ReplaceFileName(const NkString& newName);
            NkPath& RemoveFileName();
            NkPath GetParent() const;

            // -----------------------------------------------------------------
            // Conversion / acces
            // -----------------------------------------------------------------
            const char* CStr() const;
            NkString ToString() const;
            NkString ToNative() const;

            // -----------------------------------------------------------------
            // Comparaison
            // -----------------------------------------------------------------
            bool operator==(const NkPath& other) const;
            bool operator!=(const NkPath& other) const;

            // -----------------------------------------------------------------
            // Methodes statiques utilitaires
            // -----------------------------------------------------------------
            static NkPath GetCurrentDirectory();
            static NkPath GetTempDirectory();
            static NkPath Combine(const char* path1, const char* path2);
            static bool IsValidPath(const char* path);
            static NkPath Combine(const NkString& path1, const NkString& path2);
            static bool IsValidPath(const NkString& path);
    };

    // -------------------------------------------------------------------------
    // Compatibilite legacy : nkentseu::entseu::NkPath
    // -------------------------------------------------------------------------
    namespace entseu {
        using NkPath = ::nkentseu::NkPath;
    } // namespace entseu

} // namespace nkentseu

#endif // NK_FILESYSTEM_NKPATH_H_INCLUDED

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

// =============================================================================
// EXEMPLES D'UTILISATION - NkPath
// =============================================================================
//
// -- Construction et jointure -------------------------------------------------
//
//   NkPath base("/home/user/projects");
//   NkPath full = base / "assets" / "textures" / "stone.png";
//   // full.ToString() == "/home/user/projects/assets/textures/stone.png"
//
// -- Decomposition ------------------------------------------------------------
//
//   NkPath p("C:/Users/Rihen/Documents/rapport.pdf");
//   p.GetDirectory();                // "C:/Users/Rihen/Documents"
//   p.GetFileName();                 // "rapport.pdf"
//   p.GetFileNameWithoutExtension(); // "rapport"
//   p.GetExtension();                // ".pdf"
//   p.GetRoot();                     // "C:/"
//
// -- Proprietes ----------------------------------------------------------------
//
//   NkPath abs("/etc/hosts");
//   abs.IsAbsolute();  // true
//   abs.HasExtension();
//
// -- Utilitaires statiques ----------------------------------------------------
//
//   NkPath cwd = NkPath::GetCurrentDirectory();
//   NkPath tmp = NkPath::GetTempDirectory();
//   NkPath out = NkPath::Combine("/var/log", "app.log");
//   bool ok = NkPath::IsValidPath(out.CStr());
//
// =============================================================================
