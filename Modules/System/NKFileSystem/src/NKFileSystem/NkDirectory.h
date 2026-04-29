// =============================================================================
// NKFileSystem/NkDirectory.h
// Opérations utilitaires sur les répertoires : création, suppression, parcours.
//
// Design :
//  - API orientée utilitaire statique : pas d'instanciation requise
//  - Support des opérations récursives avec contrôle via NkSearchOption
//  - Filtrage par pattern (glob-style : *, ?) pour l'énumération
//  - Gestion cross-platform via détection NKPlatform
//  - Aucune dépendance STL dans l'API publique
//
// Règles d'application des macros :
//  - NKENTSEU_FILESYSTEM_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_FILESYSTEM_API sur les méthodes statiques
//  - PAS de macro sur les fonctions inline/constexpr définies dans le header
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_FILESYSTEM_NKDIRECTORY_H
#define NKENTSEU_FILESYSTEM_NKDIRECTORY_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKFileSystem (remplace l'ancien NkFileSystemExport.h)
    // Inclusion des types de base NKCore, conteneurs, et NkPath pour la gestion des chemins

    #include "NKFileSystem/NkFileSystemApi.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKFileSystem/NkPath.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module NKFileSystem résident dans le namespace nkentseu.

    namespace nkentseu {

        // =====================================================================
        // SOUS-SECTION 2.1 : Structure NkDirectoryEntry
        // =====================================================================
        // Représente une entrée de répertoire (fichier ou sous-répertoire)
        // avec métadonnées associées.

        struct NKENTSEU_FILESYSTEM_CLASS_EXPORT NkDirectoryEntry {
            // -------------------------------------------------------------
            // Champs publics — métadonnées de l'entrée
            // -------------------------------------------------------------

            NkString Name;              ///< Nom de l'entrée (sans le chemin)
            NkPath FullPath;            ///< Chemin complet vers l'entrée
            bool IsDirectory;           ///< true si c'est un répertoire
            bool IsFile;                ///< true si c'est un fichier régulier
            nk_int64 Size;              ///< Taille en octets (0 pour les répertoires)
            nk_int64 ModificationTime;  ///< Timestamp de dernière modification (epoch)

            // -------------------------------------------------------------
            // Constructeur
            // -------------------------------------------------------------

            /// Constructeur par défaut : initialise tous les champs à zéro/vide.
            NkDirectoryEntry();
        };

        // =====================================================================
        // SOUS-SECTION 2.2 : Enum NkSearchOption — Options de recherche
        // =====================================================================
        // Contrôle la profondeur de parcours lors de l'énumération.

        enum class NkSearchOption {
            NK_TOP_DIRECTORY_ONLY,  ///< Ne parcourir que le répertoire spécifié
            NK_ALL_DIRECTORIES      ///< Parcours récursif de tous les sous-répertoires
        };

        // =====================================================================
        // SOUS-SECTION 2.3 : Classe NkDirectory — Utilitaires statiques
        // =====================================================================
        // Classe utilitaire fournissant des opérations statiques sur les répertoires.
        // Aucune instanciation requise : toutes les méthodes sont statiques.

        class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkDirectory {

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.1 : Opérations de base sur les répertoires
            // -------------------------------------------------------------
            // Création, suppression, vérification d'existence et vidage.

            /// Crée un répertoire au chemin spécifié.
            /// @param path Chemin du répertoire à créer en format C-string
            /// @return true si la création a réussi ou si le répertoire existe déjà
            /// @note Échoue si le répertoire parent n'existe pas (utiliser CreateRecursive)
            static bool Create(const char* path);

            /// Crée un répertoire au chemin spécifié (version NkPath).
            /// @param path Chemin du répertoire à créer en format NkPath
            /// @return true si la création a réussi ou si le répertoire existe déjà
            static bool Create(const NkPath& path);

            /// Crée un répertoire et tous ses parents manquants (récursif).
            /// @param path Chemin du répertoire à créer en format C-string
            /// @return true si tous les répertoires ont été créés avec succès
            /// @note Idempotent : retourne true si le chemin existe déjà
            static bool CreateRecursive(const char* path);

            /// Crée un répertoire et tous ses parents manquants (version NkPath).
            /// @param path Chemin du répertoire à créer en format NkPath
            /// @return true si tous les répertoires ont été créés avec succès
            static bool CreateRecursive(const NkPath& path);

            /// Supprime un répertoire.
            /// @param path Chemin du répertoire à supprimer en format C-string
            /// @param recursive Si true, supprime récursivement tout le contenu
            /// @return true si la suppression a réussi, false sinon
            /// @note En mode non-récursif, échoue si le répertoire n'est pas vide
            static bool Delete(const char* path, bool recursive = false);

            /// Supprime un répertoire (version NkPath).
            /// @param path Chemin du répertoire à supprimer en format NkPath
            /// @param recursive Si true, supprime récursivement tout le contenu
            /// @return true si la suppression a réussi, false sinon
            static bool Delete(const NkPath& path, bool recursive = false);

            /// Vérifie si un répertoire existe sur le système.
            /// @param path Chemin à tester en format C-string
            /// @return true si le chemin existe et est un répertoire, false sinon
            /// @note Ne vérifie pas les permissions d'accès en lecture/écriture
            static bool Exists(const char* path);

            /// Vérifie si un répertoire existe (version NkPath).
            /// @param path Chemin à tester en format NkPath
            /// @return true si le chemin existe et est un répertoire, false sinon
            static bool Exists(const NkPath& path);

            /// Vérifie si un répertoire est vide (aucune entrée).
            /// @param path Chemin du répertoire à tester en format C-string
            /// @return true si le répertoire existe et ne contient aucune entrée
            /// @note Retourne true si le répertoire n'existe pas (considéré comme "vide")
            static bool Empty(const char* path);

            /// Vérifie si un répertoire est vide (version NkPath).
            /// @param path Chemin du répertoire à tester en format NkPath
            /// @return true si le répertoire existe et ne contient aucune entrée
            static bool Empty(const NkPath& path);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.2 : Énumération d'entrées
            // -------------------------------------------------------------
            // Méthodes pour lister les fichiers, répertoires ou entrées mixtes.

            /// Liste les fichiers correspondant à un pattern dans un répertoire.
            /// @param path Répertoire à parcourir en format C-string
            /// @param pattern Pattern de filtrage (glob-style : "*", "*.txt", "file?.log")
            /// @param option Option de recherche (défaut : NK_TOP_DIRECTORY_ONLY)
            /// @return NkVector<NkString> contenant les chemins complets des fichiers trouvés
            /// @note Les chemins retournés sont normalisés avec '/' comme séparateur
            static NkVector<NkString> GetFiles(
                const char* path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            /// Liste les fichiers (version NkPath).
            /// @param path Répertoire à parcourir en format NkPath
            /// @param pattern Pattern de filtrage (glob-style)
            /// @param option Option de recherche
            /// @return NkVector<NkString> contenant les chemins complets des fichiers trouvés
            static NkVector<NkString> GetFiles(
                const NkPath& path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            /// Liste les sous-répertoires correspondant à un pattern.
            /// @param path Répertoire parent à parcourir en format C-string
            /// @param pattern Pattern de filtrage (glob-style)
            /// @param option Option de recherche (défaut : NK_TOP_DIRECTORY_ONLY)
            /// @return NkVector<NkString> contenant les chemins des sous-répertoires trouvés
            static NkVector<NkString> GetDirectories(
                const char* path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            /// Liste les sous-répertoires (version NkPath).
            /// @param path Répertoire parent à parcourir en format NkPath
            /// @param pattern Pattern de filtrage (glob-style)
            /// @param option Option de recherche
            /// @return NkVector<NkString> contenant les chemins des sous-répertoires trouvés
            static NkVector<NkString> GetDirectories(
                const NkPath& path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            /// Liste toutes les entrées (fichiers + répertoires) avec métadonnées.
            /// @param path Répertoire à parcourir en format C-string
            /// @param pattern Pattern de filtrage (glob-style)
            /// @param option Option de recherche (défaut : NK_TOP_DIRECTORY_ONLY)
            /// @return NkVector<NkDirectoryEntry> contenant les entrées trouvées
            /// @note Les métadonnées (Size, ModificationTime) peuvent être 0 selon la plateforme
            static NkVector<NkDirectoryEntry> GetEntries(
                const char* path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            /// Liste toutes les entrées (version NkPath).
            /// @param path Répertoire à parcourir en format NkPath
            /// @param pattern Pattern de filtrage (glob-style)
            /// @param option Option de recherche
            /// @return NkVector<NkDirectoryEntry> contenant les entrées trouvées
            static NkVector<NkDirectoryEntry> GetEntries(
                const NkPath& path,
                const char* pattern = "*",
                NkSearchOption option = NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.3 : Copie et déplacement de répertoires
            // -------------------------------------------------------------
            // Opérations de manipulation de répertoires au niveau système.

            /// Copie un répertoire vers une destination.
            /// @param source Chemin source en format C-string
            /// @param dest Chemin destination en format C-string
            /// @param recursive Si true, copie récursivement tout le contenu (défaut : true)
            /// @param overwrite Si true, écrase les fichiers existants à la destination
            /// @return true si la copie a réussi, false sinon
            /// @note Crée automatiquement le répertoire de destination s'il n'existe pas
            static bool Copy(
                const char* source,
                const char* dest,
                bool recursive = true,
                bool overwrite = false
            );

            /// Copie un répertoire (version NkPath).
            /// @param source Chemin source en format NkPath
            /// @param dest Chemin destination en format NkPath
            /// @param recursive Si true, copie récursivement tout le contenu
            /// @param overwrite Si true, écrase les fichiers existants à la destination
            /// @return true si la copie a réussi, false sinon
            static bool Copy(
                const NkPath& source,
                const NkPath& dest,
                bool recursive = true,
                bool overwrite = false
            );

            /// Déplace/renomme un répertoire.
            /// @param source Chemin source en format C-string
            /// @param dest Chemin destination en format C-string
            /// @return true si le déplacement a réussi, false sinon
            /// @note Échoue si source et dest sont sur des volumes différents (selon OS)
            static bool Move(const char* source, const char* dest);

            /// Déplace/renomme un répertoire (version NkPath).
            /// @param source Chemin source en format NkPath
            /// @param dest Chemin destination en format NkPath
            /// @return true si le déplacement a réussi, false sinon
            static bool Move(const NkPath& source, const NkPath& dest);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.4 : Répertoires spéciaux du système
            // -------------------------------------------------------------
            // Accès aux chemins standards de l'environnement utilisateur.

            /// Obtient le répertoire de travail courant du processus.
            /// @return NkPath contenant le chemin absolu du cwd
            /// @note Utilise getcwd/_getcwd selon la plateforme
            static NkPath GetCurrentDirectory();

            /// Définit le répertoire de travail courant du processus.
            /// @param path Nouveau répertoire de travail en format C-string
            /// @return true si le changement a réussi, false sinon
            /// @note Nécessite que le répertoire existe et soit accessible
            static bool SetCurrentDirectory(const char* path);

            /// Définit le répertoire de travail courant (version NkPath).
            /// @param path Nouveau répertoire de travail en format NkPath
            /// @return true si le changement a réussi, false sinon
            static bool SetCurrentDirectory(const NkPath& path);

            /// Obtient le répertoire temporaire du système.
            /// @return NkPath contenant le chemin du répertoire temp
            /// @note Windows : GetTempPathA ou fallback "C:/Temp"
            /// @note Unix : variable $TMPDIR ou fallback "/tmp"
            static NkPath GetTempDirectory();

            /// Obtient le répertoire personnel de l'utilisateur (home).
            /// @return NkPath contenant le chemin du home directory
            /// @note Windows : CSIDL_PROFILE ou HOMEDRIVE+HOMEPATH
            /// @note Unix : variable $HOME ou entrée passwd
            static NkPath GetHomeDirectory();

            /// Obtient le répertoire de configuration applicative.
            /// @return NkPath contenant le chemin AppData/.config
            /// @note Windows : CSIDL_APPDATA (%APPDATA%)
            /// @note Unix : ~/.config (convention XDG)
            static NkPath GetAppDataDirectory();

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.5 : Méthodes privées utilitaires
            // -------------------------------------------------------------
            // Helpers internes pour l'implémentation des méthodes publiques.

            /// Vérifie si un nom correspond à un pattern glob-style.
            /// @param name Nom à tester (sans chemin)
            /// @param pattern Pattern à appliquer (supporte * et ?)
            /// @return true si le nom correspond au pattern, false sinon
            /// @note Implémentation simple : * = n'importe quelle séquence, ? = un caractère
            static bool MatchesPattern(const char* name, const char* pattern);

            /// Parcours récursif pour collecter les fichiers.
            /// @param path Répertoire de départ en format C-string
            /// @param pattern Pattern de filtrage
            /// @param results Vecteur de sortie pour accumuler les résultats
            static void GetFilesRecursive(
                const char* path,
                const char* pattern,
                NkVector<NkString>& results
            );

            /// Parcours récursif pour collecter les répertoires.
            /// @param path Répertoire de départ en format C-string
            /// @param pattern Pattern de filtrage
            /// @param results Vecteur de sortie pour accumuler les résultats
            static void GetDirectoriesRecursive(
                const char* path,
                const char* pattern,
                NkVector<NkString>& results
            );

            /// Parcours récursif pour collecter les entrées avec métadonnées.
            /// @param path Répertoire de départ en format C-string
            /// @param pattern Pattern de filtrage
            /// @param results Vecteur de sortie pour accumuler les résultats
            static void GetEntriesRecursive(
                const char* path,
                const char* pattern,
                NkVector<NkDirectoryEntry>& results
            );

        }; // class NkDirectory

        // -------------------------------------------------------------------------
        // SECTION 3 : ALIAS DE COMPATIBILITÉ LEGACY
        // -------------------------------------------------------------------------
        // Fournit des alias pour compatibilité avec l'ancien namespace nkentseu::entseu.

        namespace entseu {
            using NkDirectoryEntry = ::nkentseu::NkDirectoryEntry;
            using NkSearchOption = ::nkentseu::NkSearchOption;
            using NkDirectory = ::nkentseu::NkDirectory;
        } // namespace entseu

    } // namespace nkentseu

#endif // NKENTSEU_FILESYSTEM_NKDIRECTORY_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKDIRECTORY.H
// =============================================================================
// La classe NkDirectory fournit des utilitaires statiques pour la gestion
// des répertoires : création, suppression, énumération et copie.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création et suppression de répertoires
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKCore/Logger/NkLogger.h"

    void DirectoryManagementExample() {
        using namespace nkentseu;

        // Création simple (échoue si parent n'existe pas)
        bool created = NkDirectory::Create("data/cache");
        NK_LOG_INFO("Created: {}", created ? "yes" : "no");

        // Création récursive (crée tous les parents manquants)
        bool recursiveCreated = NkDirectory::CreateRecursive("logs/2024/01/app");
        NK_LOG_INFO("Recursive created: {}", recursiveCreated ? "yes" : "no");

        // Vérification d'existence
        if (NkDirectory::Exists("data/cache")) {
            NK_LOG_INFO("Directory exists");
        }

        // Vérification si vide
        if (NkDirectory::Empty("data/cache")) {
            NK_LOG_INFO("Directory is empty");
        }

        // Suppression (non-récursive : échoue si non vide)
        bool deleted = NkDirectory::Delete("data/cache", false);

        // Suppression récursive (supprime tout le contenu)
        bool recursiveDeleted = NkDirectory::Delete("logs", true);
        NK_LOG_INFO("Recursive deleted: {}", recursiveDeleted ? "yes" : "no");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Énumération avec filtrage par pattern
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKCore/Logger/NkLogger.h"

    void EnumerationExample() {
        using namespace nkentseu;

        // Liste tous les fichiers .png dans "assets" (non-récursif)
        NkVector<NkString> pngFiles = NkDirectory::GetFiles(
            "assets",
            "*.png",
            NkSearchOption::NK_TOP_DIRECTORY_ONLY
        );

        NK_LOG_INFO("Found {} PNG files in assets/", pngFiles.Size());
        for (const auto& file : pngFiles) {
            NK_LOG_INFO("  - {}", file.CStr());
        }

        // Liste tous les fichiers .log dans "logs" et sous-répertoires (récursif)
        NkVector<NkString> allLogs = NkDirectory::GetFiles(
            "logs",
            "*.log",
            NkSearchOption::NK_ALL_DIRECTORIES
        );

        NK_LOG_INFO("Found {} log files recursively", allLogs.Size());

        // Liste les sous-répertoires commençant par "temp"
        NkVector<NkString> tempDirs = NkDirectory::GetDirectories(
            "cache",
            "temp*",
            NkSearchOption::NK_TOP_DIRECTORY_ONLY
        );

        // Liste toutes les entrées avec métadonnées
        NkVector<NkDirectoryEntry> entries = NkDirectory::GetEntries("data");
        for (const auto& entry : entries) {
            if (entry.IsFile) {
                NK_LOG_INFO("File: {} ({} bytes)",
                            entry.Name.CStr(),
                            entry.Size);
            } else if (entry.IsDirectory) {
                NK_LOG_INFO("Dir: {}", entry.Name.CStr());
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Copie et déplacement de répertoires
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKCore/Logger/NkLogger.h"

    void CopyMoveExample() {
        using namespace nkentseu;

        // Copie récursive avec overwrite
        bool copied = NkDirectory::Copy(
            "source/config",
            "backup/config",
            true,   // recursive
            true    // overwrite existing files
        );

        if (copied) {
            NK_LOG_INFO("Backup created successfully");
        }

        // Déplacement/renommage (atomique si même volume)
        bool moved = NkDirectory::Move("temp/download", "data/completed");
        if (moved) {
            NK_LOG_INFO("Directory moved");
        } else {
            NK_LOG_ERROR("Move failed - may be cross-volume");
            // Fallback : copie puis suppression
            if (NkDirectory::Copy("temp/download", "data/completed", true, true)) {
                NkDirectory::Delete("temp/download", true);
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Accès aux répertoires spéciaux du système
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKCore/Logger/NkLogger.h"

    void SpecialDirectoriesExample() {
        using namespace nkentseu;

        // Répertoire de travail courant
        NkPath cwd = NkDirectory::GetCurrentDirectory();
        NK_LOG_INFO("Working directory: {}", cwd.ToString().CStr());

        // Changer de répertoire de travail
        if (NkDirectory::SetCurrentDirectory("/tmp")) {
            NK_LOG_INFO("Changed to /tmp");
        }

        // Répertoire temporaire pour fichiers intermédiaires
        NkPath tempDir = NkDirectory::GetTempDirectory();
        NkPath tempFile = tempDir / "myapp" / "cache.dat";
        NK_LOG_INFO("Temp file path: {}", tempFile.ToString().CStr());

        // Répertoire personnel pour sauvegardes utilisateur
        NkPath home = NkDirectory::GetHomeDirectory();
        NkPath saveDir = home / "MyApp" / "Saves";
        NkDirectory::CreateRecursive(saveDir);

        // Répertoire de configuration applicative
        NkPath configDir = NkDirectory::GetAppDataDirectory() / "MyApp";
        NkDirectory::CreateRecursive(configDir);
        NkPath configFile = configDir / "settings.json";

        NK_LOG_INFO("Config path: {}", configFile.ToString().CStr());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Pattern glob pour filtrage avancé
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"

    void PatternMatchingExample() {
        using namespace nkentseu;

        // Pattern "*" : tous les fichiers
        auto all = NkDirectory::GetFiles("data", "*");

        // Pattern "*.txt" : extension spécifique
        auto texts = NkDirectory::GetFiles("docs", "*.txt");

        // Pattern "file?.log" : un caractère wildcard (file1.log, fileA.log, etc.)
        auto numbered = NkDirectory::GetFiles("logs", "file?.log");

        // Pattern "backup_*_2024.*" : combinaison
        auto backups = NkDirectory::GetFiles("archives", "backup_*_2024.*");

        // Pattern récursif : tous les .cpp dans le projet
        auto sources = NkDirectory::GetFiles(
            "src",
            "*.cpp",
            NkSearchOption::NK_ALL_DIRECTORIES
        );
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Nettoyage de cache avec énumération
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKCore/Logger/NkLogger.h"

    void CacheCleanupExample() {
        using namespace nkentseu;

        const char* cacheDir = "cache/temp";

        if (!NkDirectory::Exists(cacheDir)) {
            return;  // Rien à nettoyer
        }

        // Supprimer tous les fichiers .tmp
        auto tmpFiles = NkDirectory::GetFiles(cacheDir, "*.tmp");
        for (const auto& file : tmpFiles) {
            NkFile::Delete(file.CStr());
            NK_LOG_DEBUG("Deleted temp file: {}", file.CStr());
        }

        // Supprimer les sous-répertoires "old_*"
        auto oldDirs = NkDirectory::GetDirectories(cacheDir, "old_*");
        for (const auto& dir : oldDirs) {
            NkDirectory::Delete(dir.CStr(), true);  // récursif
            NK_LOG_DEBUG("Deleted old directory: {}", dir.CStr());
        }

        // Si le cache est maintenant vide, le supprimer aussi
        if (NkDirectory::Empty(cacheDir)) {
            NkDirectory::Delete(cacheDir, false);
            NK_LOG_INFO("Cache directory cleaned and removed");
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Sauvegarde incrémentale avec copie conditionnelle
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKFileSystem/NkFile.h"

    namespace backup {

        bool IncrementalBackup(
            const nkentseu::NkPath& source,
            const nkentseu::NkPath& destination,
            const char* pattern = "*"
        ) {
            using namespace nkentseu;

            if (!NkDirectory::Exists(source)) {
                return false;
            }

            // Créer la destination si nécessaire
            if (!NkDirectory::Exists(destination)) {
                if (!NkDirectory::CreateRecursive(destination)) {
                    return false;
                }
            }

            // Copier uniquement les fichiers correspondant au pattern
            auto files = NkDirectory::GetFiles(source, pattern);
            for (const auto& file : files) {
                NkPath filename = NkPath(file).GetFileName();
                NkPath destFile = destination / filename;

                // Copier seulement si source est plus récent ou destination n'existe pas
                bool shouldCopy = !NkFile::Exists(destFile);
                if (!shouldCopy) {
                    nk_int64 srcTime = NkFile::GetFileSize(file.CStr());  // Simplifié
                    nk_int64 dstTime = NkFile::GetFileSize(destFile.CStr());
                    shouldCopy = srcTime > dstTime;
                }

                if (shouldCopy) {
                    if (!NkFile::Copy(file.CStr(), destFile.CStr(), true)) {
                        return false;  // Échec de copie
                    }
                }
            }

            // Récursivité pour les sous-répertoires
            auto dirs = NkDirectory::GetDirectories(source, "*");
            for (const auto& dir : dirs) {
                NkPath dirname = NkPath(dir).GetFileName();
                NkPath destDir = destination / dirname;
                if (!IncrementalBackup(dir, destDir, pattern)) {
                    return false;
                }
            }

            return true;
        }

    } // namespace backup
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Validation de structure de projet
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkDirectory.h"
    #include "NKCore/Logger/NkLogger.h"

    bool ValidateProjectStructure(const nkentseu::NkPath& projectRoot) {
        using namespace nkentseu;

        // Vérifier les répertoires requis
        const char* requiredDirs[] = {
            "src", "include", "build", "docs", "tests"
        };

        for (const char* dir : requiredDirs) {
            NkPath fullPath = projectRoot / dir;
            if (!NkDirectory::Exists(fullPath)) {
                NK_LOG_ERROR("Missing required directory: {}", fullPath.ToString().CStr());
                return false;
            }
        }

        // Vérifier la présence de fichiers de configuration
        const char* requiredFiles[] = {
            "CMakeLists.txt", "README.md", ".gitignore"
        };

        for (const char* file : requiredFiles) {
            NkPath fullPath = projectRoot / file;
            if (!NkFile::Exists(fullPath)) {
                NK_LOG_WARN("Missing optional file: {}", fullPath.ToString().CStr());
            }
        }

        // Lister les fichiers sources pour statistique
        auto sources = NkDirectory::GetFiles(
            projectRoot / "src",
            "*.cpp",
            NkSearchOption::NK_ALL_DIRECTORIES
        );

        NK_LOG_INFO("Project validation: {} source files found", sources.Size());
        return true;
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================