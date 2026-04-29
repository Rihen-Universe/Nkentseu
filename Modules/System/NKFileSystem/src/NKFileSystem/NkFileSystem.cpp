// =============================================================================
// NKFileSystem/NkFileSystem.cpp
// Implémentation des utilitaires système de fichiers cross-platform.
//
// Design :
//  - Abstraction des APIs natives via guards conditionnels (_WIN32, POSIX)
//  - Conversion systématique des chemins via NkPath pour cohérence interne
//  - Timestamps normalisés en epoch Unix pour portabilité des métadonnées
//  - Gestion défensive : vérifications null, retours silencieux en cas d'erreur
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme

#include "pch.h"
#include "NKFileSystem/NkFileSystem.h"

// En-têtes C standard pour les fonctions système et chaînes
#include <cstring>
#include <cstdlib>

// En-têtes plateforme pour les APIs système de fichiers
#ifdef _WIN32
    #include <windows.h>
    // Undef des macros Windows qui pourraient entrer en conflit avec nos méthodes
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

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes statiques dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  Implémentation : Constructeurs des structures
    // =============================================================================
    // Initialisation sécurisée avec valeurs par défaut neutres.

    NkDriveInfo::NkDriveInfo()
        : Name()
        , Label()
        , Type(NkFileSystemType::NK_UNKNOW)
        , TotalSize(0)
        , FreeSpace(0)
        , AvailableSpace(0)
        , IsReady(false)
    {
        // Constructeur par défaut : tous les champs à l'état neutre
        // IsReady = false signifie que les champs de taille ne sont pas fiables
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
        , LastWriteTime(0)
    {
        // Constructeur par défaut : tous les flags à false, timestamps à 0
        // Timestamp 0 signifie "non initialisé" pour le consommateur
    }

    // =============================================================================
    //  Implémentation : Gestion des lecteurs et volumes
    // =============================================================================

    NkVector<NkDriveInfo> NkFileSystem::GetDrives()
    {
        NkVector<NkDriveInfo> drives;

        #ifdef _WIN32
            // Windows : énumération via GetLogicalDrives() (bitmask 26 bits pour A-Z)
            const DWORD driveMask = GetLogicalDrives();

            for (int i = 0; i < 26; ++i)
            {
                // Test du bit correspondant à la lettre de lecteur
                if (driveMask & (1 << i))
                {
                    // Construction du chemin racine "X:/"
                    char driveLetter[4] = {
                        static_cast<char>('A' + i),
                        ':',
                        '\\',
                        '\0'
                    };

                    NkDriveInfo info;
                    info.Name = driveLetter;

                    // Vérification du type et de l'état du lecteur
                    const UINT driveType = GetDriveTypeA(driveLetter);
                    info.IsReady = (driveType != DRIVE_NO_ROOT_DIR);

                    if (info.IsReady)
                    {
                        // Récupération des métadonnées du volume
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
                        ))
                        {
                            // Stockage de l'étiquette volumique
                            info.Label = volumeName;

                            // Mapping du nom de FS vers NkFileSystemType
                            if (strcmp(fileSystemName, "NTFS") == 0)
                            {
                                info.Type = NkFileSystemType::NK_NTFS;
                            }
                            else if (strcmp(fileSystemName, "FAT32") == 0)
                            {
                                info.Type = NkFileSystemType::NK_FAT32;
                            }
                            else if (strcmp(fileSystemName, "exFAT") == 0)
                            {
                                info.Type = NkFileSystemType::NK_EXFAT;
                            }
                            // Autres types : restent NK_UNKNOW par défaut
                        }

                        // Récupération des capacités de stockage
                        ULARGE_INTEGER freeBytes;
                        ULARGE_INTEGER totalBytes;
                        ULARGE_INTEGER availBytes;

                        if (GetDiskFreeSpaceExA(driveLetter, &availBytes, &totalBytes, &freeBytes))
                        {
                            info.TotalSize = static_cast<nk_int64>(totalBytes.QuadPart);
                            info.FreeSpace = static_cast<nk_int64>(freeBytes.QuadPart);
                            info.AvailableSpace = static_cast<nk_int64>(availBytes.QuadPart);
                        }
                    }

                    // Ajout du lecteur à la liste de résultat
                    drives.PushBack(info);
                }
            }
        #else
            // Unix/Linux : approche simplifiée, retourne uniquement la racine "/"
            // Pour une énumération complète, utiliser /proc/mounts ou getmntent()
            NkDriveInfo info;
            info.Name = "/";
            info.IsReady = true;

            // Récupération des statistiques du système de fichiers via statvfs
            struct statvfs st;
            if (statvfs("/", &st) == 0)
            {
                // Calcul des tailles : blocs * taille de bloc
                info.TotalSize = static_cast<nk_int64>(st.f_blocks) * static_cast<nk_int64>(st.f_frsize);
                info.FreeSpace = static_cast<nk_int64>(st.f_bfree) * static_cast<nk_int64>(st.f_frsize);
                info.AvailableSpace = static_cast<nk_int64>(st.f_bavail) * static_cast<nk_int64>(st.f_frsize);
            }

            // Ajout de l'entrée unique à la liste
            drives.PushBack(info);
        #endif

        return drives;
    }

    NkDriveInfo NkFileSystem::GetDriveInfo(const char* path)
    {
        // Gestion défensive : retour d'objet vide si path est null
        if (!path)
        {
            return NkDriveInfo();
        }

        #ifdef _WIN32
            // Détection de chemin Windows avec lettre de lecteur "X:..."
            if (path[0] && path[1] == ':')
            {
                // Reconstruction du chemin racine pour comparaison
                char driveLetter[4] = {
                    path[0],
                    ':',
                    '\\',
                    '\0'
                };

                // Recherche dans la liste des lecteurs disponibles
                NkVector<NkDriveInfo> drives = GetDrives();
                for (usize i = 0; i < drives.Size(); ++i)
                {
                    // Comparaison insensible à la casse pour robustesse
                    if (_stricmp(drives[i].Name.CStr(), driveLetter) == 0)
                    {
                        return drives[i];
                    }
                }
            }
        #else
            // Unix : tous les chemins appartiennent au même arbre, retour de la racine
            NkVector<NkDriveInfo> drives = GetDrives();
            if (!drives.Empty())
            {
                return drives[0];
            }
        #endif

        // Fallback : retour d'objet vide si non trouvé
        return NkDriveInfo();
    }

    NkDriveInfo NkFileSystem::GetDriveInfo(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return GetDriveInfo(path.CStr());
    }

    // =============================================================================
    //  Implémentation : Informations d'espace disque
    // =============================================================================
    // Méthodes déléguées à GetDriveInfo() pour cohérence et maintenance.

    nk_int64 NkFileSystem::GetTotalSpace(const char* path)
    {
        return GetDriveInfo(path).TotalSize;
    }

    nk_int64 NkFileSystem::GetTotalSpace(const NkPath& path)
    {
        return GetTotalSpace(path.CStr());
    }

    nk_int64 NkFileSystem::GetFreeSpace(const char* path)
    {
        return GetDriveInfo(path).FreeSpace;
    }

    nk_int64 NkFileSystem::GetFreeSpace(const NkPath& path)
    {
        return GetFreeSpace(path.CStr());
    }

    nk_int64 NkFileSystem::GetAvailableSpace(const char* path)
    {
        return GetDriveInfo(path).AvailableSpace;
    }

    nk_int64 NkFileSystem::GetAvailableSpace(const NkPath& path)
    {
        return GetAvailableSpace(path.CStr());
    }

    // =============================================================================
    //  Implémentation : Gestion des attributs de fichiers
    // =============================================================================

    NkFileAttributes NkFileSystem::GetFileAttributes(const char* path)
    {
        NkFileAttributes attrs;

        // Gestion défensive : retour d'objet vide si path est null
        if (!path)
        {
            return attrs;
        }

        #ifdef _WIN32
            // Lecture des attributs Windows via GetFileAttributesA
            const DWORD winAttrs = GetFileAttributesA(path);

            if (winAttrs != INVALID_FILE_ATTRIBUTES)
            {
                // Mapping des flags Windows vers notre structure
                attrs.IsReadOnly = (winAttrs & FILE_ATTRIBUTE_READONLY) != 0;
                attrs.IsHidden = (winAttrs & FILE_ATTRIBUTE_HIDDEN) != 0;
                attrs.IsSystem = (winAttrs & FILE_ATTRIBUTE_SYSTEM) != 0;
                attrs.IsArchive = (winAttrs & FILE_ATTRIBUTE_ARCHIVE) != 0;
                attrs.IsCompressed = (winAttrs & FILE_ATTRIBUTE_COMPRESSED) != 0;
                attrs.IsEncrypted = (winAttrs & FILE_ATTRIBUTE_ENCRYPTED) != 0;
            }

            // Lecture des timestamps via GetFileTime (nécessite handle de fichier)
            HANDLE hFile = CreateFileA(
                path,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile != INVALID_HANDLE_VALUE)
            {
                FILETIME creation;
                FILETIME access;
                FILETIME write;

                if (GetFileTime(hFile, &creation, &access, &write))
                {
                    // Conversion FILETIME (100ns depuis 1601) vers epoch Unix simplifiée
                    // Note : pour une conversion exacte, soustraire l'époque Windows
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

                // Libération du handle de fichier
                CloseHandle(hFile);
            }
        #else
            // POSIX : utilisation de stat() pour obtenir métadonnées
            struct stat st;

            if (stat(path, &st) == 0)
            {
                // Détection lecture seule via bit de permission utilisateur
                attrs.IsReadOnly = !(st.st_mode & S_IWUSR);
                // Timestamps POSIX déjà en time_t (epoch Unix)
                attrs.CreationTime = st.st_ctime;
                attrs.LastAccessTime = st.st_atime;
                attrs.LastWriteTime = st.st_mtime;
            }
        #endif

        return attrs;
    }

    NkFileAttributes NkFileSystem::GetFileAttributes(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return GetFileAttributes(path.CStr());
    }

    bool NkFileSystem::SetFileAttributes(const char* path, const NkFileAttributes& attrs)
    {
        // Gestion défensive : retour false si path est null
        if (!path)
        {
            return false;
        }

        #ifdef _WIN32
            // Reconstruction des flags Windows à partir de notre structure
            DWORD winAttrs = 0;
            if (attrs.IsReadOnly)
            {
                winAttrs |= FILE_ATTRIBUTE_READONLY;
            }
            if (attrs.IsHidden)
            {
                winAttrs |= FILE_ATTRIBUTE_HIDDEN;
            }
            if (attrs.IsSystem)
            {
                winAttrs |= FILE_ATTRIBUTE_SYSTEM;
            }
            if (attrs.IsArchive)
            {
                winAttrs |= FILE_ATTRIBUTE_ARCHIVE;
            }

            // Application via SetFileAttributesA
            return SetFileAttributesA(path, winAttrs) != 0;
        #else
            // POSIX : modification des permissions via chmod
            struct stat st;
            if (stat(path, &st) != 0)
            {
                return false;
            }

            mode_t mode = st.st_mode;

            // Gestion de l'attribut lecture seule via bits de permission
            if (attrs.IsReadOnly)
            {
                // Suppression des bits d'écriture pour user/group/other
                mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
            }
            else
            {
                // Restauration du bit d'écriture utilisateur
                mode |= S_IWUSR;
            }

            // Application des nouvelles permissions
            return chmod(path, mode) == 0;
        #endif
    }

    bool NkFileSystem::SetFileAttributes(const NkPath& path, const NkFileAttributes& attrs)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return SetFileAttributes(path.CStr(), attrs);
    }

    bool NkFileSystem::SetReadOnly(const char* path, bool readOnly)
    {
        // Pattern : lecture-modification-écriture pour attribut unique
        NkFileAttributes attrs = GetFileAttributes(path);
        attrs.IsReadOnly = readOnly;
        return SetFileAttributes(path, attrs);
    }

    bool NkFileSystem::SetReadOnly(const NkPath& path, bool readOnly)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return SetReadOnly(path.CStr(), readOnly);
    }

    bool NkFileSystem::SetHidden(const char* path, bool hidden)
    {
        #ifdef _WIN32
            // Windows : utilisation du même pattern lecture-modification-écriture
            NkFileAttributes attrs = GetFileAttributes(path);
            attrs.IsHidden = hidden;
            return SetFileAttributes(path, attrs);
        #else
            // Unix : attribut "caché" n'existe pas au niveau FS (convention nommage uniquement)
            // Retour false pour indiquer l'opération non supportée
            (void)path;
            (void)hidden;
            return false;
        #endif
    }

    bool NkFileSystem::SetHidden(const NkPath& path, bool hidden)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return SetHidden(path.CStr(), hidden);
    }

    // =============================================================================
    //  Implémentation : Gestion des horodatages (timestamps)
    // =============================================================================
    // Méthodes de lecture déléguées à GetFileAttributes() pour cohérence.

    nk_int64 NkFileSystem::GetCreationTime(const char* path)
    {
        return GetFileAttributes(path).CreationTime;
    }

    nk_int64 NkFileSystem::GetCreationTime(const NkPath& path)
    {
        return GetCreationTime(path.CStr());
    }

    nk_int64 NkFileSystem::GetLastAccessTime(const char* path)
    {
        return GetFileAttributes(path).LastAccessTime;
    }

    nk_int64 NkFileSystem::GetLastAccessTime(const NkPath& path)
    {
        return GetLastAccessTime(path.CStr());
    }

    nk_int64 NkFileSystem::GetLastWriteTime(const char* path)
    {
        return GetFileAttributes(path).LastWriteTime;
    }

    nk_int64 NkFileSystem::GetLastWriteTime(const NkPath& path)
    {
        return GetLastWriteTime(path.CStr());
    }

    bool NkFileSystem::SetCreationTime(const char* path, nk_int64 time)
    {
        #ifdef _WIN32
            // Windows : modification via SetFileTime avec handle en écriture d'attributs
            HANDLE hFile = CreateFileA(
                path,
                FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            // Conversion epoch Unix vers FILETIME (simplifiée, voir note GetFileAttributes)
            FILETIME ft;
            ft.dwLowDateTime = static_cast<DWORD>(time & 0xFFFFFFFF);
            ft.dwHighDateTime = static_cast<DWORD>((time >> 32) & 0xFFFFFFFF);

            // Application : création uniquement, accès et écriture inchangés (NULL)
            const BOOL ok = SetFileTime(hFile, &ft, NULL, NULL);

            // Libération du handle
            CloseHandle(hFile);
            return ok != 0;
        #else
            // Unix : pas d'API native pour modifier le temps de création (st_ctime est métadonnée)
            // Retour false pour indiquer l'opération non supportée
            (void)path;
            (void)time;
            return false;
        #endif
    }

    bool NkFileSystem::SetCreationTime(const NkPath& path, nk_int64 time)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return SetCreationTime(path.CStr(), time);
    }

    bool NkFileSystem::SetLastAccessTime(const char* path, nk_int64 time)
    {
        #ifdef _WIN32
            // Windows : même pattern que SetCreationTime, mais pour le champ d'accès
            HANDLE hFile = CreateFileA(
                path,
                FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            FILETIME ft;
            ft.dwLowDateTime = static_cast<DWORD>(time & 0xFFFFFFFF);
            ft.dwHighDateTime = static_cast<DWORD>((time >> 32) & 0xFFFFFFFF);

            // Application : accès uniquement, création et écriture inchangées (NULL)
            const BOOL ok = SetFileTime(hFile, NULL, &ft, NULL);

            CloseHandle(hFile);
            return ok != 0;
        #else
            // POSIX : utilisation de utime() pour modifier accès et/ou modification
            struct stat st;
            if (stat(path, &st) != 0)
            {
                return false;
            }

            // Préparation de la structure utimbuf
            struct utimbuf tb;
            tb.actime = static_cast<time_t>(time);      // Nouveau temps d'accès
            tb.modtime = st.st_mtime;                   // Temps de modification inchangé

            // Application via utime()
            return utime(path, &tb) == 0;
        #endif
    }

    bool NkFileSystem::SetLastAccessTime(const NkPath& path, nk_int64 time)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return SetLastAccessTime(path.CStr(), time);
    }

    bool NkFileSystem::SetLastWriteTime(const char* path, nk_int64 time)
    {
        #ifdef _WIN32
            // Windows : même pattern, application sur le champ de dernière écriture
            HANDLE hFile = CreateFileA(
                path,
                FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (hFile == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            FILETIME ft;
            ft.dwLowDateTime = static_cast<DWORD>(time & 0xFFFFFFFF);
            ft.dwHighDateTime = static_cast<DWORD>((time >> 32) & 0xFFFFFFFF);

            // Application : écriture uniquement, création et accès inchangés (NULL)
            const BOOL ok = SetFileTime(hFile, NULL, NULL, &ft);

            CloseHandle(hFile);
            return ok != 0;
        #else
            // POSIX : utime() pour modifier le temps de modification
            struct stat st;
            if (stat(path, &st) != 0)
            {
                return false;
            }

            struct utimbuf tb;
            tb.actime = st.st_atime;                    // Temps d'accès inchangé
            tb.modtime = static_cast<time_t>(time);     // Nouveau temps de modification

            return utime(path, &tb) == 0;
        #endif
    }

    bool NkFileSystem::SetLastWriteTime(const NkPath& path, nk_int64 time)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return SetLastWriteTime(path.CStr(), time);
    }

    // =============================================================================
    //  Implémentation : Validation et conversion de chemins
    // =============================================================================

    bool NkFileSystem::IsValidFileName(const char* name)
    {
        // Gestion défensive : rejet des noms null ou vides
        if (!name || !name[0])
        {
            return false;
        }

        // Liste des caractères interdits sur Windows (et problématiques sur Unix)
        const char* invalid = "<>:\"|?*";

        // Parcours caractère par caractère pour détection des interdits
        for (const char* p = name; *p; ++p)
        {
            // Rejet si caractère dans la liste des interdits
            if (strchr(invalid, *p))
            {
                return false;
            }

            // Rejet des caractères de contrôle (ASCII < 32)
            if (*p < 32)
            {
                return false;
            }
        }

        #ifdef _WIN32
            // Vérification des noms réservés Windows (CON, PRN, AUX, etc.)
            static const char* reserved[] = {
                "CON", "PRN", "AUX", "NUL",
                "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
            };

            const usize kCount = sizeof(reserved) / sizeof(reserved[0]);

            // Comparaison insensible à la casse avec chaque nom réservé
            for (usize i = 0; i < kCount; ++i)
            {
                if (_stricmp(name, reserved[i]) == 0)
                {
                    return false;
                }
            }
        #endif

        // Aucun caractère interdit trouvé : nom considéré comme valide
        return true;
    }

    bool NkFileSystem::IsValidPath(const char* path)
    {
        // Délégation à NkPath::IsValidPath() pour cohérence de validation
        return NkPath::IsValidPath(path);
    }

    NkString NkFileSystem::GetAbsolutePath(const char* path)
    {
        // Gestion défensive : retour de chaîne vide si path est null
        if (!path)
        {
            return NkString();
        }

        #ifdef _WIN32
            // Windows : résolution via GetFullPathNameA
            char buffer[MAX_PATH];
            if (GetFullPathNameA(path, MAX_PATH, buffer, NULL))
            {
                return NkString(buffer);
            }
        #else
            // POSIX : résolution via realpath() qui résout aussi les symlinks
            char buffer[PATH_MAX];
            if (realpath(path, buffer))
            {
                return NkString(buffer);
            }
        #endif

        // Fallback : retour du chemin d'entrée tel quel si résolution échoue
        return NkString(path);
    }

    NkString NkFileSystem::GetAbsolutePath(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return GetAbsolutePath(path.CStr());
    }

    NkString NkFileSystem::GetRelativePath(const char* from, const char* to)
    {
        // Implémentation basique : retourne 'to' tel quel
        // Pour un calcul robuste, utiliser une bibliothèque de manipulation de chemins
        (void)from;
        return NkString(to ? to : "");
    }

    NkString NkFileSystem::GetRelativePath(const NkPath& from, const NkPath& to)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return GetRelativePath(from.CStr(), to.CStr());
    }

    // =============================================================================
    //  Implémentation : Informations sur le système de fichiers
    // =============================================================================

    NkFileSystemType NkFileSystem::GetFileSystemType(const char* path)
    {
        // Délégation à GetDriveInfo() qui détecte déjà le type de FS
        return GetDriveInfo(path).Type;
    }

    NkFileSystemType NkFileSystem::GetFileSystemType(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return GetFileSystemType(path.CStr());
    }

    bool NkFileSystem::IsCaseSensitive(const char* path)
    {
        // Détection basique par plateforme
        (void)path;
        #ifdef _WIN32
            // Windows : par défaut insensible à la casse (sauf configurations avancées)
            return false;
        #else
            // Unix/Linux/macOS : sensible à la casse par défaut
            return true;
        #endif
    }

    bool NkFileSystem::IsCaseSensitive(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return IsCaseSensitive(path.CStr());
    }

    // =============================================================================
    //  Implémentation : Gestion des liens symboliques
    // =============================================================================

    bool NkFileSystem::IsSymbolicLink(const char* path)
    {
        // Gestion défensive : retour false si path est null
        if (!path)
        {
            return false;
        }

        #ifdef _WIN32
            // Windows : détection via flag REPARSE_POINT dans les attributs
            const DWORD attrs = GetFileAttributesA(path);
            return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
        #else
            // POSIX : utilisation de lstat() pour ne pas suivre le symlink, puis test S_ISLNK
            struct stat st;
            return (lstat(path, &st) == 0) && S_ISLNK(st.st_mode);
        #endif
    }

    bool NkFileSystem::IsSymbolicLink(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return IsSymbolicLink(path.CStr());
    }

    NkString NkFileSystem::GetSymbolicLinkTarget(const char* path)
    {
        // Gestion défensive : retour de chaîne vide si path est null
        if (!path)
        {
            return NkString();
        }

        #ifdef _WIN32
            // Windows : non implémenté (nécessite DeviceIoControl avec FSCTL_GET_REPARSE_POINT)
            // Retour de chaîne vide pour indiquer l'indisponibilité
            return NkString();
        #else
            // POSIX : lecture de la cible via readlink()
            char buffer[PATH_MAX];
            const ssize_t len = readlink(path, buffer, sizeof(buffer) - 1);

            if (len > 0)
            {
                // Terminaison de la chaîne et retour du résultat
                buffer[len] = '\0';
                return NkString(buffer);
            }

            // Échec de lecture : retour de chaîne vide
            return NkString();
        #endif
    }

    NkString NkFileSystem::GetSymbolicLinkTarget(const NkPath& path)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return GetSymbolicLinkTarget(path.CStr());
    }

    bool NkFileSystem::CreateSymbolicLink(const char* link, const char* target)
    {
        // Gestion défensive : rejet si l'un des paramètres est null
        if (!link || !target)
        {
            return false;
        }

        #ifdef _WIN32
            // Windows : création via CreateSymbolicLinkA
            // Paramètre 0 = symlink de fichier, utiliser SYMBOLIC_LINK_FLAG_DIRECTORY pour répertoire
            // Nécessite privilèges administrateur ou mode développeur activé
            return CreateSymbolicLinkA(link, target, 0) != 0;
        #else
            // POSIX : création via symlink() avec ordre cible->lien (convention Unix)
            return symlink(target, link) == 0;
        #endif
    }

    bool NkFileSystem::CreateSymbolicLink(const NkPath& link, const NkPath& target)
    {
        // Délégation à la version C-string pour éviter la duplication de code
        return CreateSymbolicLink(link.CStr(), target.CStr());
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Conversion des timestamps :
    --------------------------
    - Windows utilise FILETIME : 100-nanosecond intervals depuis le 1er janvier 1601 UTC
    - Unix utilise time_t : secondes depuis le 1er janvier 1970 UTC (epoch Unix)
    - La conversion simplifiée utilisée ici (bit-shifting) préserve l'ordre mais pas la valeur exacte
    - Pour une conversion précise : soustraire 116444736000000000LL (différence d'époques en 100ns)

    Attribution IsReadOnly sur Unix :
    --------------------------------
    - Unix n'a pas d'attribut "lecture seule" au niveau fichier
    - L'implémentation mappe IsReadOnly sur l'absence de bit S_IWUSR (écriture utilisateur)
    - Cela signifie qu'un fichier peut être modifiable par root même si IsReadOnly = true

    Espace libre vs disponible :
    ---------------------------
    - FreeSpace : espace libre total (inclut blocs réservés au super-utilisateur sur Unix)
    - AvailableSpace : espace réellement utilisable par l'utilisateur courant (exclut quotas)
    - Sur Windows, les deux valeurs sont généralement identiques sauf quotas NTFS activés

    Liens symboliques Windows :
    --------------------------
    - CreateSymbolicLinkA() nécessite :
      * Soit les privilèges SeCreateSymbolicLinkPrivilege (administrateur par défaut)
      * Soit le "Developer Mode" activé sur Windows 10 1703+
    - Les symlinks Windows peuvent pointer vers fichiers OU répertoires (flag requis)
    - GetSymbolicLinkTarget() n'est pas implémenté : retourne toujours chaîne vide

    Performance des appels système :
    -------------------------------
    - GetFileAttributes/GetFileTime sur Windows : un seul appel système par attribut
    - stat() sur Unix : récupère toutes les métadonnées en un seul appel
    - Pour lire plusieurs attributs, préférer GetFileAttributes() complet plutôt que des appels individuels

    Thread-safety :
    --------------
    - Toutes les méthodes sont stateless et thread-safe en lecture
    - Les opérations d'écriture (SetFileAttributes, etc.) ne sont pas atomiques
    - Pour des modifications concurrentes, utiliser une synchronisation externe

    Extensions futures possibles :
    -----------------------------
    - Support des hard links via GetNumberOfLinks()/CreateHardLink
    - Détection des points de montage sur Unix via parsing de /proc/mounts
    - Conversion exacte des timestamps Windows FILETIME ↔ epoch Unix
    - Support des symlinks Windows via DeviceIoControl(FSCTL_GET_REPARSE_POINT)
    - Gestion des ACLs (Access Control Lists) pour attributs de permission avancés
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================