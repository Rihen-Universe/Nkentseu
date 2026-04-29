// =============================================================================
// NKFileSystem/NkFileSystem.h
// Utilitaires cross-platform pour l'accès aux métadonnées du système de fichiers.
//
// Design :
//  - API statique sans état : toutes les méthodes sont des fonctions utilitaires
//  - Abstraction des APIs natives : Windows API, POSIX statvfs/stat, utime
//  - Gestion unifiée des chemins via NkPath (normalisation '/' interne)
//  - Support des liens symboliques avec fallback silencieux sur plateformes limitées
//  - Timestamps en format Unix epoch (nk_int64) pour cohérence cross-platform
//
// Règles d'application des macros :
//  - NKENTSEU_FILESYSTEM_CLASS_EXPORT sur la classe uniquement
//  - PAS de NKENTSEU_FILESYSTEM_API sur les méthodes (l'export de classe suffit)
//  - PAS de macro sur les fonctions inline définies dans le header
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_FILESYSTEM_NKFILESYSTEM_H
#define NKENTSEU_FILESYSTEM_NKFILESYSTEM_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKFileSystem
    // Inclusion des types de base NKCore et des conteneurs NKEntseu

    #include "NKFileSystem/NkFileSystemApi.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKFileSystem/NkPath.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Déclarations des types et de la classe utilitaire NkFileSystem.

    namespace nkentseu {

        // =====================================================================
        //  Énumération : NkFileSystemType
        // =====================================================================
        // Types de systèmes de fichiers supportés pour identification des volumes.
        // NOTE : NK_UNKNOW est conservé pour compatibilité API (faute de frappe historique).

        enum class NkFileSystemType
        {
            NK_UNKNOW,      ///< Type inconnu ou non détecté (compatibilité legacy).
            NK_NTFS,        ///< NTFS : système de fichiers Windows moderne.
            NK_FAT32,       ///< FAT32 : système legacy compatible multi-plateforme.
            NK_EXFAT,       ///< exFAT : optimisé pour supports amovibles de grande capacité.
            NK_EXT4,        ///< ext4 : système de fichiers Linux par défaut.
            NK_EXT3,        ///< ext3 : prédécesseur d'ext4, encore utilisé.
            NK_HFS,         ///< HFS/HFS+ : système de fichiers macOS legacy.
            NK_APFS,        ///< APFS : Apple File System, macOS/iOS moderne.
            NK_NETWORK      ///< Système de fichiers réseau (SMB, NFS, etc.).
        };

        // =====================================================================
        //  Structure : NkDriveInfo
        // =====================================================================
        // Conteneur d'informations sur un lecteur/volume du système.

        struct NKENTSEU_FILESYSTEM_CLASS_EXPORT NkDriveInfo
        {
            NkString Name;              ///< Nom du lecteur (ex: "C:/" ou "/").
            NkString Label;             ///< Étiquette volumique (nom convivial du volume).
            NkFileSystemType Type;      ///< Type de système de fichiers détecté.
            nk_int64 TotalSize;         ///< Taille totale du volume en octets.
            nk_int64 FreeSpace;         ///< Espace libre total en octets (inclut réservé système).
            nk_int64 AvailableSpace;    ///< Espace disponible pour l'utilisateur en octets.
            bool IsReady;               ///< Indicateur : le lecteur est accessible et monté.

            // -------------------------------------------------------------
            // Constructeurs
            // -------------------------------------------------------------

            /// Constructeur par défaut : initialise tous les membres à des valeurs neutres.
            NkDriveInfo();
        };

        // =====================================================================
        //  Structure : NkFileAttributes
        // =====================================================================
        // Conteneur d'attributs et métadonnées d'un fichier ou répertoire.

        struct NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFileAttributes
        {
            bool IsReadOnly;            ///< Indicateur : fichier en lecture seule.
            bool IsHidden;              ///< Indicateur : fichier masqué (Windows uniquement).
            bool IsSystem;              ///< Indicateur : fichier système critique (Windows).
            bool IsArchive;             ///< Indicateur : fichier marqué pour archivage.
            bool IsCompressed;          ///< Indicateur : fichier compressé au niveau FS.
            bool IsEncrypted;           ///< Indicateur : fichier chiffré (EFS/autres).
            nk_int64 CreationTime;      ///< Horodatage de création (epoch Unix ou FILETIME).
            nk_int64 LastAccessTime;    ///< Horodatage du dernier accès en lecture.
            nk_int64 LastWriteTime;     ///< Horodatage de la dernière modification.

            // -------------------------------------------------------------
            // Constructeurs
            // -------------------------------------------------------------

            /// Constructeur par défaut : initialise tous les indicateurs à false et timestamps à 0.
            NkFileAttributes();
        };

        // =====================================================================
        //  Classe : NkFileSystem
        // =====================================================================
        // Classe utilitaire statique pour l'accès aux métadonnées du système de fichiers.
        // Toutes les méthodes sont statiques : aucune instanciation requise.

        class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFileSystem
        {

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.1 : Gestion des lecteurs et volumes
            // -------------------------------------------------------------
            // Méthodes pour énumérer et interroger les lecteurs montés.

            /// Énumère tous les lecteurs/logical drives disponibles sur le système.
            /// @return Vecteur contenant les informations de chaque lecteur détecté.
            /// @note Sur Unix, retourne uniquement le répertoire racine "/".
            static NkVector<NkDriveInfo> GetDrives();

            /// Obtient les informations détaillées d'un lecteur à partir d'un chemin.
            /// @param path Chemin C-string sur le volume à interroger.
            /// @return NkDriveInfo contenant les métadonnées du volume, ou objet vide en cas d'erreur.
            static NkDriveInfo GetDriveInfo(const char* path);

            /// Obtient les informations détaillées d'un lecteur à partir d'un NkPath.
            /// @param path Objet NkPath représentant un chemin sur le volume à interroger.
            /// @return NkDriveInfo contenant les métadonnées du volume, ou objet vide en cas d'erreur.
            static NkDriveInfo GetDriveInfo(const NkPath& path);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.2 : Informations d'espace disque
            // -------------------------------------------------------------
            // Méthodes pour obtenir les capacités de stockage d'un volume.

            /// Obtient la taille totale d'un volume en octets.
            /// @param path Chemin C-string sur le volume à interroger.
            /// @return Taille totale en octets, ou 0 en cas d'erreur.
            static nk_int64 GetTotalSpace(const char* path);

            /// Obtient la taille totale d'un volume en octets (version NkPath).
            /// @param path Objet NkPath sur le volume à interroger.
            /// @return Taille totale en octets, ou 0 en cas d'erreur.
            static nk_int64 GetTotalSpace(const NkPath& path);

            /// Obtient l'espace libre total d'un volume en octets.
            /// @param path Chemin C-string sur le volume à interroger.
            /// @return Espace libre en octets (inclut espace réservé système), ou 0 en cas d'erreur.
            static nk_int64 GetFreeSpace(const char* path);

            /// Obtient l'espace libre total d'un volume en octets (version NkPath).
            /// @param path Objet NkPath sur le volume à interroger.
            /// @return Espace libre en octets, ou 0 en cas d'erreur.
            static nk_int64 GetFreeSpace(const NkPath& path);

            /// Obtient l'espace disponible pour l'utilisateur en octets.
            /// @param path Chemin C-string sur le volume à interroger.
            /// @return Espace disponible en octets (exclut quotas et réservé système), ou 0 en cas d'erreur.
            static nk_int64 GetAvailableSpace(const char* path);

            /// Obtient l'espace disponible pour l'utilisateur en octets (version NkPath).
            /// @param path Objet NkPath sur le volume à interroger.
            /// @return Espace disponible en octets, ou 0 en cas d'erreur.
            static nk_int64 GetAvailableSpace(const NkPath& path);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3 : Gestion des attributs de fichiers
            // -------------------------------------------------------------
            // Méthodes pour lire et modifier les attributs des fichiers.

            /// Obtient tous les attributs d'un fichier ou répertoire.
            /// @param path Chemin C-string de l'élément à interroger.
            /// @return NkFileAttributes contenant les métadonnées, ou objet vide en cas d'erreur.
            static NkFileAttributes GetFileAttributes(const char* path);

            /// Obtient tous les attributs d'un fichier ou répertoire (version NkPath).
            /// @param path Objet NkPath de l'élément à interroger.
            /// @return NkFileAttributes contenant les métadonnées, ou objet vide en cas d'erreur.
            static NkFileAttributes GetFileAttributes(const NkPath& path);

            /// Définit les attributs d'un fichier ou répertoire.
            /// @param path Chemin C-string de l'élément à modifier.
            /// @param attrs Structure NkFileAttributes contenant les nouveaux attributs.
            /// @return true si la modification a réussi, false sinon.
            static bool SetFileAttributes(const char* path, const NkFileAttributes& attrs);

            /// Définit les attributs d'un fichier ou répertoire (version NkPath).
            /// @param path Objet NkPath de l'élément à modifier.
            /// @param attrs Structure NkFileAttributes contenant les nouveaux attributs.
            /// @return true si la modification a réussi, false sinon.
            static bool SetFileAttributes(const NkPath& path, const NkFileAttributes& attrs);

            /// Définit ou supprime l'attribut lecture seule d'un fichier.
            /// @param path Chemin C-string de l'élément à modifier.
            /// @param readOnly true pour activer la lecture seule, false pour désactiver.
            /// @return true si la modification a réussi, false sinon.
            static bool SetReadOnly(const char* path, bool readOnly);

            /// Définit ou supprime l'attribut lecture seule d'un fichier (version NkPath).
            /// @param path Objet NkPath de l'élément à modifier.
            /// @param readOnly true pour activer la lecture seule, false pour désactiver.
            /// @return true si la modification a réussi, false sinon.
            static bool SetReadOnly(const NkPath& path, bool readOnly);

            /// Définit ou supprime l'attribut caché d'un fichier (Windows uniquement).
            /// @param path Chemin C-string de l'élément à modifier.
            /// @param hidden true pour masquer le fichier, false pour l'afficher.
            /// @return true si la modification a réussi, false sinon (toujours false sur Unix).
            static bool SetHidden(const char* path, bool hidden);

            /// Définit ou supprime l'attribut caché d'un fichier (version NkPath, Windows uniquement).
            /// @param path Objet NkPath de l'élément à modifier.
            /// @param hidden true pour masquer le fichier, false pour l'afficher.
            /// @return true si la modification a réussi, false sinon (toujours false sur Unix).
            static bool SetHidden(const NkPath& path, bool hidden);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.4 : Gestion des horodatages (timestamps)
            // -------------------------------------------------------------
            // Méthodes pour lire et modifier les timestamps des fichiers.

            /// Obtient l'horodatage de création d'un fichier.
            /// @param path Chemin C-string de l'élément à interroger.
            /// @return Timestamp en format epoch Unix (ou FILETIME converti sur Windows), ou 0 en cas d'erreur.
            static nk_int64 GetCreationTime(const char* path);

            /// Obtient l'horodatage de création d'un fichier (version NkPath).
            /// @param path Objet NkPath de l'élément à interroger.
            /// @return Timestamp en format epoch Unix, ou 0 en cas d'erreur.
            static nk_int64 GetCreationTime(const NkPath& path);

            /// Obtient l'horodatage du dernier accès d'un fichier.
            /// @param path Chemin C-string de l'élément à interroger.
            /// @return Timestamp du dernier accès en lecture, ou 0 en cas d'erreur.
            static nk_int64 GetLastAccessTime(const char* path);

            /// Obtient l'horodatage du dernier accès d'un fichier (version NkPath).
            /// @param path Objet NkPath de l'élément à interroger.
            /// @return Timestamp du dernier accès en lecture, ou 0 en cas d'erreur.
            static nk_int64 GetLastAccessTime(const NkPath& path);

            /// Obtient l'horodatage de la dernière modification d'un fichier.
            /// @param path Chemin C-string de l'élément à interroger.
            /// @return Timestamp de la dernière écriture, ou 0 en cas d'erreur.
            static nk_int64 GetLastWriteTime(const char* path);

            /// Obtient l'horodatage de la dernière modification d'un fichier (version NkPath).
            /// @param path Objet NkPath de l'élément à interroger.
            /// @return Timestamp de la dernière écriture, ou 0 en cas d'erreur.
            static nk_int64 GetLastWriteTime(const NkPath& path);

            /// Définit l'horodatage de création d'un fichier (Windows uniquement).
            /// @param path Chemin C-string de l'élément à modifier.
            /// @param time Nouveau timestamp en format epoch Unix.
            /// @return true si la modification a réussi, false sinon (toujours false sur Unix).
            static bool SetCreationTime(const char* path, nk_int64 time);

            /// Définit l'horodatage de création d'un fichier (version NkPath, Windows uniquement).
            /// @param path Objet NkPath de l'élément à modifier.
            /// @param time Nouveau timestamp en format epoch Unix.
            /// @return true si la modification a réussi, false sinon.
            static bool SetCreationTime(const NkPath& path, nk_int64 time);

            /// Définit l'horodatage du dernier accès d'un fichier.
            /// @param path Chemin C-string de l'élément à modifier.
            /// @param time Nouveau timestamp en format epoch Unix.
            /// @return true si la modification a réussi, false sinon.
            static bool SetLastAccessTime(const char* path, nk_int64 time);

            /// Définit l'horodatage du dernier accès d'un fichier (version NkPath).
            /// @param path Objet NkPath de l'élément à modifier.
            /// @param time Nouveau timestamp en format epoch Unix.
            /// @return true si la modification a réussi, false sinon.
            static bool SetLastAccessTime(const NkPath& path, nk_int64 time);

            /// Définit l'horodatage de la dernière modification d'un fichier.
            /// @param path Chemin C-string de l'élément à modifier.
            /// @param time Nouveau timestamp en format epoch Unix.
            /// @return true si la modification a réussi, false sinon.
            static bool SetLastWriteTime(const char* path, nk_int64 time);

            /// Définit l'horodatage de la dernière modification d'un fichier (version NkPath).
            /// @param path Objet NkPath de l'élément à modifier.
            /// @param time Nouveau timestamp en format epoch Unix.
            /// @return true si la modification a réussi, false sinon.
            static bool SetLastWriteTime(const NkPath& path, nk_int64 time);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.5 : Validation et conversion de chemins
            // -------------------------------------------------------------
            // Méthodes utilitaires pour la manipulation et validation des chemins.

            /// Vérifie si un nom de fichier est valide (sans chemin).
            /// @param name Nom de fichier C-string à valider.
            /// @return true si le nom ne contient pas de caractères interdits, false sinon.
            /// @note Rejette : < > " | ? * et caractères de contrôle (< 32).
            static bool IsValidFileName(const char* name);

            /// Vérifie si un chemin complet est syntaxiquement valide.
            /// @param path Chemin C-string à valider.
            /// @return true si le chemin est valide, false sinon.
            /// @note Délègue à NkPath::IsValidPath() pour cohérence.
            static bool IsValidPath(const char* path);

            /// Obtient le chemin absolu équivalent d'un chemin relatif.
            /// @param path Chemin C-string (relatif ou absolu) à résoudre.
            /// @return NkString contenant le chemin absolu résolu, ou chaîne vide en cas d'erreur.
            static NkString GetAbsolutePath(const char* path);

            /// Obtient le chemin absolu équivalent d'un chemin relatif (version NkPath).
            /// @param path Objet NkPath à résoudre.
            /// @return NkString contenant le chemin absolu résolu, ou chaîne vide en cas d'erreur.
            static NkString GetAbsolutePath(const NkPath& path);

            /// Calcule le chemin relatif d'une cible par rapport à une origine.
            /// @param from Chemin C-string de l'origine.
            /// @param to Chemin C-string de la cible.
            /// @return NkString contenant le chemin relatif, ou le chemin absolu de 'to' en cas d'échec.
            /// @note Implémentation basique : retourne 'to' tel quel (à étendre si nécessaire).
            static NkString GetRelativePath(const char* from, const char* to);

            /// Calcule le chemin relatif d'une cible par rapport à une origine (version NkPath).
            /// @param from Objet NkPath de l'origine.
            /// @param to Objet NkPath de la cible.
            /// @return NkString contenant le chemin relatif, ou le chemin absolu de 'to' en cas d'échec.
            static NkString GetRelativePath(const NkPath& from, const NkPath& to);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.6 : Informations sur le système de fichiers
            // -------------------------------------------------------------
            // Méthodes pour interroger les caractéristiques du FS sous-jacent.

            /// Obtient le type de système de fichiers d'un volume.
            /// @param path Chemin C-string sur le volume à interroger.
            /// @return NkFileSystemType identifiant le FS, ou NK_UNKNOW en cas d'erreur.
            static NkFileSystemType GetFileSystemType(const char* path);

            /// Obtient le type de système de fichiers d'un volume (version NkPath).
            /// @param path Objet NkPath sur le volume à interroger.
            /// @return NkFileSystemType identifiant le FS, ou NK_UNKNOW en cas d'erreur.
            static NkFileSystemType GetFileSystemType(const NkPath& path);

            /// Vérifie si le système de fichiers est sensible à la casse.
            /// @param path Chemin C-string sur le volume à interroger.
            /// @return true si le FS est case-sensitive (Unix), false sinon (Windows par défaut).
            static bool IsCaseSensitive(const char* path);

            /// Vérifie si le système de fichiers est sensible à la casse (version NkPath).
            /// @param path Objet NkPath sur le volume à interroger.
            /// @return true si le FS est case-sensitive, false sinon.
            static bool IsCaseSensitive(const NkPath& path);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.7 : Gestion des liens symboliques
            // -------------------------------------------------------------
            // Méthodes pour créer, lire et tester les liens symboliques.

            /// Vérifie si un chemin pointe vers un lien symbolique.
            /// @param path Chemin C-string à tester.
            /// @return true si le chemin est un symlink, false sinon.
            static bool IsSymbolicLink(const char* path);

            /// Vérifie si un chemin pointe vers un lien symbolique (version NkPath).
            /// @param path Objet NkPath à tester.
            /// @return true si le chemin est un symlink, false sinon.
            static bool IsSymbolicLink(const NkPath& path);

            /// Obtient la cible d'un lien symbolique.
            /// @param path Chemin C-string du lien à résoudre.
            /// @return NkString contenant le chemin cible, ou chaîne vide en cas d'erreur ou si non-symlink.
            /// @note Sur Windows, retourne toujours une chaîne vide (non implémenté).
            static NkString GetSymbolicLinkTarget(const char* path);

            /// Obtient la cible d'un lien symbolique (version NkPath).
            /// @param path Objet NkPath du lien à résoudre.
            /// @return NkString contenant le chemin cible, ou chaîne vide en cas d'erreur.
            static NkString GetSymbolicLinkTarget(const NkPath& path);

            /// Crée un nouveau lien symbolique pointant vers une cible.
            /// @param link Chemin C-string du nouveau lien à créer.
            /// @param target Chemin C-string de la cible du lien.
            /// @return true si la création a réussi, false sinon.
            /// @note Sur Windows, nécessite les privilèges administrateur ou mode développeur.
            static bool CreateSymbolicLink(const char* link, const char* target);

            /// Crée un nouveau lien symbolique pointant vers une cible (version NkPath).
            /// @param link Objet NkPath du nouveau lien à créer.
            /// @param target Objet NkPath de la cible du lien.
            /// @return true si la création a réussi, false sinon.
            static bool CreateSymbolicLink(const NkPath& link, const NkPath& target);

        }; // class NkFileSystem

    } // namespace nkentseu

#endif // NKENTSEU_FILESYSTEM_NKFILESYSTEM_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKFILESYSTEM.H
// =============================================================================
// La classe NkFileSystem fournit une API statique cross-platform pour accéder
// aux métadonnées du système de fichiers : volumes, attributs, timestamps, liens.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Énumération et inspection des lecteurs
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystem.h"
    #include "NKCore/Logger/NkLogger.h"

    void ListAvailableDrives()
    {
        using namespace nkentseu;

        // Récupération de tous les lecteurs montés
        NkVector<NkDriveInfo> drives = NkFileSystem::GetDrives();

        for (usize i = 0; i < drives.Size(); ++i)
        {
            const NkDriveInfo& drive = drives[i];

            if (!drive.IsReady)
            {
                NK_LOG_INFO("Lecteur {} : non prêt", drive.Name.CStr());
                continue;
            }

            NK_LOG_INFO("=== Lecteur {} ===", drive.Name.CStr());
            NK_LOG_INFO("  Étiquette   : {}", drive.Label.CStr());
            NK_LOG_INFO("  Type FS     : {}", static_cast<int>(drive.Type));
            NK_LOG_INFO("  Taille      : {} Go", drive.TotalSize / (1024 * 1024 * 1024));
            NK_LOG_INFO("  Libre       : {} Go", drive.FreeSpace / (1024 * 1024 * 1024));
            NK_LOG_INFO("  Disponible  : {} Go", drive.AvailableSpace / (1024 * 1024 * 1024));
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion des attributs de fichier (lecture seule, caché)
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystem.h"
    #include "NKCore/Logger/NkLogger.h"

    void ManageFileAttributes()
    {
        using namespace nkentseu;

        const char* filePath = "config/settings.json";

        // Lecture des attributs actuels
        NkFileAttributes attrs = NkFileSystem::GetFileAttributes(filePath);

        if (!attrs.IsReadOnly)
        {
            // Activation de l'attribut lecture seule
            attrs.IsReadOnly = true;

            if (NkFileSystem::SetFileAttributes(filePath, attrs))
            {
                NK_LOG_INFO("Fichier '{}' passé en lecture seule", filePath);
            }
            else
            {
                NK_LOG_ERROR("Échec de modification des attributs pour '{}'", filePath);
            }
        }

        // Masquage du fichier (Windows uniquement, silencieux sur Unix)
        if (NkFileSystem::SetHidden(filePath, true))
        {
            NK_LOG_INFO("Fichier '{}' masqué", filePath);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Manipulation des horodatages (timestamps)
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystem.h"
    #include <ctime>

    void UpdateFileTimestamps()
    {
        using namespace nkentseu;

        const char* logFile = "logs/app.log";

        // Lecture des timestamps actuels
        nk_int64 created = NkFileSystem::GetCreationTime(logFile);
        nk_int64 accessed = NkFileSystem::GetLastAccessTime(logFile);
        nk_int64 modified = NkFileSystem::GetLastWriteTime(logFile);

        // Mise à jour du timestamp de modification à l'heure actuelle
        nk_int64 now = static_cast<nk_int64>(time(nullptr));

        if (NkFileSystem::SetLastWriteTime(logFile, now))
        {
            // Vérification de la mise à jour
            nk_int64 newModified = NkFileSystem::GetLastWriteTime(logFile);
            if (newModified == now)
            {
                // Succès : timestamp mis à jour
            }
        }

        // Note : SetCreationTime() fonctionne uniquement sur Windows
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Validation et normalisation de chemins
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystem.h"
    #include "NKCore/Assert/NkAssert.h"

    void ValidateAndNormalizePaths()
    {
        using namespace nkentseu;

        // Validation de noms de fichiers
        NKENTSEU_ASSERT(NkFileSystem::IsValidFileName("document.txt") == true);
        NKENTSEU_ASSERT(NkFileSystem::IsValidFileName("file<bad>.txt") == false);  // '<' interdit
        NKENTSEU_ASSERT(NkFileSystem::IsValidFileName("CON") == false);            // Nom réservé Windows

        // Validation de chemins complets
        NKENTSEU_ASSERT(NkFileSystem::IsValidPath("/valid/path/file.log") == true);
        NKENTSEU_ASSERT(NkFileSystem::IsValidPath("C:/Windows/System32") == true);
        NKENTSEU_ASSERT(NkFileSystem::IsValidPath("bad|path?.txt") == false);      // '|' et '?' interdits

        // Conversion vers chemin absolu
        NkString absolute = NkFileSystem::GetAbsolutePath("../relative/path");
        // absolute contient maintenant le chemin résolu depuis le répertoire courant

        // Note : GetRelativePath() est une implémentation basique
        // Pour un calcul robuste de chemins relatifs, utiliser NkPath::GetParent()/GetDirectory()
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion des liens symboliques
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystem.h"
    #include "NKCore/Logger/NkLogger.h"

    void ManageSymbolicLinks()
    {
        using namespace nkentseu;

        const char* linkPath = "current-config";
        const char* targetPath = "configs/v2/settings.json";

        // Vérification si le lien existe déjà
        if (!NkFileSystem::IsSymbolicLink(linkPath))
        {
            // Création du lien symbolique
            if (NkFileSystem::CreateSymbolicLink(linkPath, targetPath))
            {
                NK_LOG_INFO("Lien symbolique créé : {} -> {}", linkPath, targetPath);
            }
            else
            {
                NK_LOG_ERROR("Échec de création du lien symbolique");
                // Sur Windows : vérifier les privilèges administrateur / mode développeur
            }
        }
        else
        {
            // Lecture de la cible du lien existant
            NkString target = NkFileSystem::GetSymbolicLinkTarget(linkPath);

            #ifndef _WIN32
                // Sur Unix, target contient le chemin de la cible
                NK_LOG_INFO("Lien {} pointe vers : {}", linkPath, target.CStr());
            #else
                // Sur Windows, GetSymbolicLinkTarget retourne une chaîne vide (non implémenté)
                NK_LOG_INFO("Lien {} détecté (cible non résolue sur Windows)", linkPath);
            #endif
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Surveillance de l'espace disque avant écriture
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFileSystem.h"
    #include "NKCore/Logger/NkLogger.h"

    bool CanWriteLargeFile(const nkentseu::NkPath& outputPath, nkentseu::nk_int64 requiredBytes)
    {
        using namespace nkentseu;

        // Obtention de l'espace disponible pour l'utilisateur
        nk_int64 available = NkFileSystem::GetAvailableSpace(outputPath);

        if (available < requiredBytes)
        {
            NK_LOG_WARN("Espace insuffisant : {} octets requis, {} disponibles",
                requiredBytes, available);
            return false;
        }

        // Marge de sécurité de 10% pour éviter le remplissage complet
        const nk_int64 safetyMargin = requiredBytes / 10;

        if (available - requiredBytes < safetyMargin)
        {
            NK_LOG_WARN("Espace critique : écriture possible mais risque de saturation");
        }

        return true;
    }

    // Utilisation
    void SaveLargeAsset()
    {
        using namespace nkentseu;

        NkPath output("exports/large_asset.bin");
        constexpr nk_int64 assetSize = 2LL * 1024 * 1024 * 1024;  // 2 Go

        if (CanWriteLargeFile(output, assetSize))
        {
            // Procéder à l'écriture du fichier...
        }
        else
        {
            NK_LOG_ERROR("Sauvegarde annulée : espace disque insuffisant");
        }
    }
*/

// -----------------------------------------------------------------------------
// Notes d'utilisation et bonnes pratiques
// -----------------------------------------------------------------------------
/*
    Timestamps et portabilité :
    --------------------------
    - Les timestamps sont retournés en format epoch Unix (nk_int64) pour cohérence
    - Sur Windows, les FILETIME (100-nanosecond intervals depuis 1601) sont convertis
    - La précision réelle dépend du système de fichiers sous-jacent (ex: FAT32 = 2s)

    Attributs plateforme-specific :
    ------------------------------
    - IsHidden, IsSystem, IsEncrypted : fonctionnels uniquement sur Windows
    - Sur Unix, ces attributs sont ignorés en lecture et SetHidden() retourne false
    - IsReadOnly sur Unix correspond à l'absence de bit d'écriture utilisateur (S_IWUSR)

    Liens symboliques :
    ------------------
    - Unix : support complet via readlink()/symlink()/lstat()
    - Windows : CreateSymbolicLinkA() nécessite privilèges élevés ou mode développeur
    - GetSymbolicLinkTarget() retourne chaîne vide sur Windows (non implémenté)

    Performance des énumérations :
    -----------------------------
    - GetDrives() sur Windows : énumère 26 lettres potentielles (A-Z), rapide
    - GetDrives() sur Unix : retourne uniquement "/", pas d'énumération de mounts
    - Pour une liste complète des points de montage sur Unix, utiliser /proc/mounts ou getmntent()

    Gestion d'erreurs :
    ------------------
    - La plupart des méthodes retournent des valeurs par défaut en cas d'erreur (0, false, chaîne vide)
    - Vérifier IsReady sur NkDriveInfo avant d'utiliser les champs de taille
    - Les chemins invalides ou inaccessibles ne lèvent pas d'exception : retour silencieux

    Sécurité :
    ---------
    - IsValidFileName() rejette les caractères dangereux mais ne garantit pas l'absence de path traversal
    - Toujours valider les chemins utilisateur avec NkPath::IsValidPath() avant utilisation
    - Éviter d'exposer directement les chemins système dans les logs ou interfaces utilisateur

    Extensions futures possibles :
    -----------------------------
    - Support des quotas disque par utilisateur/groupe
    - Résolution complète des chemins relatifs dans GetRelativePath()
    - Implémentation de GetSymbolicLinkTarget() sur Windows via DeviceIoControl
    - Détection de la sensibilité à la casse réelle (Windows Subsystem for Linux, APFS case-fold, etc.)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================