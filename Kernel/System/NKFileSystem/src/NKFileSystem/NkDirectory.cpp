// =============================================================================
// NKFileSystem/NkDirectory.cpp
// Implémentation des opérations sur les répertoires.
//
// Design :
//  - Utilisation des APIs système natives pour chaque plateforme
//  - Gestion récursive via appels internes privés
//  - Filtrage par pattern glob-style (*, ?) implémenté manuellement
//  - Aucune dépendance STL dans l'implémentation
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
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

// En-têtes C standard pour les opérations système
#include <cstdio>
#include <cstring>
#include <cstdlib>

// En-têtes plateforme pour les opérations sur les répertoires
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <shlobj.h>
    // Alias pour compatibilité : mkdir/rmdir sur Windows
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir(path) _rmdir(path)
    // Undef des macros Windows qui pourraient entrer en conflit
    #ifdef GetCurrentDirectory
        #undef GetCurrentDirectory
    #endif
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <pwd.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes de NkDirectory dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  Structures
    // =============================================================================

    NkDirectoryEntry::NkDirectoryEntry()
        : Name()
        , FullPath()
        , IsDirectory(false)
        , IsFile(false)
        , Size(0)
        , ModificationTime(0)
    {
        // Initialisation explicite de tous les champs via liste de membres
        // Corps vide car tout est initialisé ci-dessus
    }

    // =============================================================================
    //  Opérations de base sur les répertoires
    // =============================================================================
    // Création, suppression, vérification d'existence et vidage.

    bool NkDirectory::Create(const char* path) {
        // Guards : chemin requis et vérification préalable d'existence
        if (!path || Exists(path)) {
            return false;
        }

        #ifdef _WIN32
            // Windows : CreateDirectoryA retourne non-zero en cas de succès
            // Gestion du cas ERROR_ALREADY_EXISTS pour idempotence
            return CreateDirectoryA(path, NULL) != 0
                || GetLastError() == ERROR_ALREADY_EXISTS;
        #else
            // POSIX : mkdir retourne 0 en cas de succès
            // Mode 0755 : rwxr-xr-x (lecture/exécution pour tous, écriture pour propriétaire)
            return mkdir(path, 0755) == 0;
        #endif
    }

    bool NkDirectory::Create(const NkPath& path) {
        // Délégation à la version C-string pour éviter la duplication de code
        return Create(path.CStr());
    }

    bool NkDirectory::CreateRecursive(const char* path) {
        // Guard : chemin null invalide
        if (!path) {
            return false;
        }

        // Cas de base : si le chemin existe déjà, succès immédiat (idempotence)
        if (Exists(path)) {
            return true;
        }

        // Extraction du parent pour création récursive ascendante
        NkPath parent = NkPath(path).GetParent();

        // Création récursive du parent si nécessaire et s'il n'est pas vide
        if (!parent.ToString().Empty() && !Exists(parent)) {
            if (!CreateRecursive(parent.CStr())) {
                // Échec de création du parent : propagation de l'erreur
                return false;
            }
        }

        // Création du répertoire courant (le parent existe maintenant)
        return Create(path);
    }

    bool NkDirectory::CreateRecursive(const NkPath& path) {
        // Délégation à la version C-string
        return CreateRecursive(path.CStr());
    }

    bool NkDirectory::Delete(const char* path, bool recursive) {
        // Guards : chemin requis et existence vérifiée
        if (!path || !Exists(path)) {
            return false;
        }

        // Mode récursif : suppression préalable du contenu
        if (recursive) {
            // Suppression des fichiers d'abord
            NkVector<NkString> files = GetFiles(path);
            for (usize i = 0; i < files.Size(); ++i) {
                NkFile::Delete(files[i].CStr());
            }

            // Suppression des sous-répertoires (récursivement)
            NkVector<NkString> dirs = GetDirectories(path);
            for (usize i = 0; i < dirs.Size(); ++i) {
                Delete(dirs[i].CStr(), true);
            }
        }

        #ifdef _WIN32
            // Windows : RemoveDirectoryA pour supprimer un répertoire vide
            return RemoveDirectoryA(path) != 0;
        #else
            // POSIX : rmdir pour supprimer un répertoire vide
            return rmdir(path) == 0;
        #endif
    }

    bool NkDirectory::Delete(const NkPath& path, bool recursive) {
        // Délégation à la version C-string
        return Delete(path.CStr(), recursive);
    }

    bool NkDirectory::Exists(const char* path) {
        // Guard : chemin null considéré comme inexistant
        if (!path) {
            return false;
        }

        #ifdef _WIN32
            // Windows : GetFileAttributesA retourne INVALID_FILE_ATTRIBUTES si inexistant
            // Vérification du flag FILE_ATTRIBUTE_DIRECTORY pour confirmer que c'est un répertoire
            const DWORD attrs = GetFileAttributesA(path);
            return (attrs != INVALID_FILE_ATTRIBUTES)
                && (attrs & FILE_ATTRIBUTE_DIRECTORY);
        #else
            // POSIX : stat() remplit une structure avec les métadonnées
            // S_ISDIR() vérifie que le type est un répertoire
            struct stat st;
            return (stat(path, &st) == 0) && S_ISDIR(st.st_mode);
        #endif
    }

    bool NkDirectory::Exists(const NkPath& path) {
        // Délégation à la version C-string
        return Exists(path.CStr());
    }

    bool NkDirectory::Empty(const char* path) {
        // Un répertoire inexistant est considéré comme "vide" par convention
        if (!Exists(path)) {
            return true;
        }

        // Obtention de toutes les entrées pour vérifier si aucune n'est présente
        NkVector<NkDirectoryEntry> entries = GetEntries(path);
        return entries.Empty();
    }

    bool NkDirectory::Empty(const NkPath& path) {
        // Délégation à la version C-string
        return Empty(path.CStr());
    }

    // =============================================================================
    //  Filtrage par pattern et récursivité
    // =============================================================================
    // Implémentation manuelle du matching glob-style sans dépendance externe.

    bool NkDirectory::MatchesPattern(const char* name, const char* pattern) {
        // Guards : paramètres requis
        if (!name || !pattern) {
            return false;
        }

        // Pointeurs de parcours pour l'algorithme de matching
        const char* text = name;
        const char* glob = pattern;
        const char* star = nullptr;        // Position du dernier '*' rencontré
        const char* backtrack = nullptr;   // Position de retour pour backtracking

        // Parcours principal du texte et du pattern
        while (*text) {
            // Cas '*' : match zéro ou plusieurs caractères
            if (*glob == '*') {
                star = glob++;              // Mémoriser la position de '*'
                backtrack = text;           // Point de retour pour backtracking
                continue;
            }

            // Cas '?' (un caractère quelconque) ou match exact
            if (*glob == '?' || *glob == *text) {
                ++glob;
                ++text;
                continue;
            }

            // Échec de match : tentative de backtracking si '*' précédent
            if (star) {
                glob = star + 1;            // Reprendre après le '*'
                text = ++backtrack;         // Avancer d'un caractère dans le texte
                continue;
            }

            // Aucun match possible
            return false;
        }

        // Consommer les '*' restants en fin de pattern (matchent la fin du texte)
        while (*glob == '*') {
            ++glob;
        }

        // Succès si le pattern est entièrement consommé
        return *glob == '\0';
    }

    void NkDirectory::GetFilesRecursive(
        const char* path,
        const char* pattern,
        NkVector<NkString>& results
    ) {
        // Obtention des sous-répertoires directs pour parcours récursif
        NkVector<NkString> dirs = GetDirectories(path);

        // Pour chaque sous-répertoire
        for (usize i = 0; i < dirs.Size(); ++i) {
            // Collecte des fichiers au niveau courant (non-récursif)
            NkVector<NkString> files = GetFiles(
                dirs[i].CStr(),
                pattern,
                NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            // Ajout des fichiers trouvés au vecteur de résultats
            for (usize j = 0; j < files.Size(); ++j) {
                results.PushBack(files[j]);
            }

            // Appel récursif pour parcourir ce sous-répertoire
            GetFilesRecursive(dirs[i].CStr(), pattern, results);
        }
    }

    void NkDirectory::GetDirectoriesRecursive(
        const char* path,
        const char* pattern,
        NkVector<NkString>& results
    ) {
        // Obtention des sous-répertoires directs pour parcours récursif
        NkVector<NkString> dirs = GetDirectories(path);

        // Pour chaque sous-répertoire
        for (usize i = 0; i < dirs.Size(); ++i) {
            // Collecte des sous-sous-répertoires au niveau courant
            NkVector<NkString> subdirs = GetDirectories(
                dirs[i].CStr(),
                pattern,
                NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            // Ajout des répertoires trouvés au vecteur de résultats
            for (usize j = 0; j < subdirs.Size(); ++j) {
                results.PushBack(subdirs[j]);
            }

            // Appel récursif pour parcourir ce sous-répertoire
            GetDirectoriesRecursive(dirs[i].CStr(), pattern, results);
        }
    }

    void NkDirectory::GetEntriesRecursive(
        const char* path,
        const char* pattern,
        NkVector<NkDirectoryEntry>& results
    ) {
        // Obtention des sous-répertoires directs pour parcours récursif
        NkVector<NkString> dirs = GetDirectories(path);

        // Pour chaque sous-répertoire
        for (usize i = 0; i < dirs.Size(); ++i) {
            // Collecte des entrées au niveau courant
            NkVector<NkDirectoryEntry> entries = GetEntries(
                dirs[i].CStr(),
                pattern,
                NkSearchOption::NK_TOP_DIRECTORY_ONLY
            );

            // Ajout des entrées trouvées au vecteur de résultats
            for (usize j = 0; j < entries.Size(); ++j) {
                results.PushBack(entries[j]);
            }

            // Appel récursif pour parcourir ce sous-répertoire
            GetEntriesRecursive(dirs[i].CStr(), pattern, results);
        }
    }

    // =============================================================================
    //  Énumération d'entrées
    // =============================================================================
    // Implémentations multiplateformes pour lister les contenus de répertoires.

    NkVector<NkString> NkDirectory::GetFiles(
        const char* path,
        const char* pattern,
        NkSearchOption option
    ) {
        // Vecteur de résultats à retourner
        NkVector<NkString> results;

        // Guards : chemin requis et existence vérifiée
        if (!path || !Exists(path)) {
            return results;
        }

        #ifdef _WIN32
            // Windows : utilisation de FindFirstFile/FindNextFile API
            WIN32_FIND_DATAA findData;
            NkString searchPath = NkString(path) + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);

            // Échec d'ouverture du handle : retour résultats vides
            if (hFind == INVALID_HANDLE_VALUE) {
                return results;
            }

            // Parcours de toutes les entrées du répertoire
            do {
                // Ignorer les entrées spéciales "." et ".."
                if (strcmp(findData.cFileName, ".") == 0
                    || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }

                // Filtrer : ne garder que les fichiers (pas les répertoires)
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    // Appliquer le pattern de filtrage
                    if (MatchesPattern(findData.cFileName, pattern)) {
                        // Construction du chemin complet normalisé
                        NkPath fullPath = NkPath(path) / findData.cFileName;
                        results.PushBack(fullPath.ToString());
                    }
                }
            } while (FindNextFileA(hFind, &findData));

            // Libération du handle de recherche
            FindClose(hFind);

        #else
            // POSIX : utilisation de opendir/readdir API
            DIR* dir = opendir(path);
            if (!dir) {
                return results;
            }

            struct dirent* entry = nullptr;

            // Parcours de toutes les entrées du répertoire
            while ((entry = readdir(dir)) != nullptr) {
                // Ignorer les entrées spéciales "." et ".."
                if (strcmp(entry->d_name, ".") == 0
                    || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                // Construction du chemin complet pour vérification du type
                NkPath fullPath = NkPath(path) / entry->d_name;
                struct stat st;

                // Vérification que c'est un fichier régulier via stat()
                if (stat(fullPath.CStr(), &st) == 0 && S_ISREG(st.st_mode)) {
                    // Appliquer le pattern de filtrage
                    if (MatchesPattern(entry->d_name, pattern)) {
                        results.PushBack(fullPath.ToString());
                    }
                }
            }

            // Fermeture du descripteur de répertoire
            closedir(dir);
        #endif

        // Gestion de l'option récursive : collecte supplémentaire si demandée
        if (option == NkSearchOption::NK_ALL_DIRECTORIES) {
            GetFilesRecursive(path, pattern, results);
        }

        return results;
    }

    NkVector<NkString> NkDirectory::GetFiles(
        const NkPath& path,
        const char* pattern,
        NkSearchOption option
    ) {
        // Délégation à la version C-string
        return GetFiles(path.CStr(), pattern, option);
    }

    NkVector<NkString> NkDirectory::GetDirectories(
        const char* path,
        const char* pattern,
        NkSearchOption option
    ) {
        // Vecteur de résultats à retourner
        NkVector<NkString> results;

        // Guards : chemin requis et existence vérifiée
        if (!path || !Exists(path)) {
            return results;
        }

        #ifdef _WIN32
            // Windows : utilisation de FindFirstFile/FindNextFile API
            WIN32_FIND_DATAA findData;
            NkString searchPath = NkString(path) + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);

            if (hFind == INVALID_HANDLE_VALUE) {
                return results;
            }

            do {
                if (strcmp(findData.cFileName, ".") == 0
                    || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }

                // Filtrer : ne garder que les répertoires
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (MatchesPattern(findData.cFileName, pattern)) {
                        NkPath fullPath = NkPath(path) / findData.cFileName;
                        results.PushBack(fullPath.ToString());
                    }
                }
            } while (FindNextFileA(hFind, &findData));

            FindClose(hFind);

        #else
            // POSIX : utilisation de opendir/readdir API
            DIR* dir = opendir(path);
            if (!dir) {
                return results;
            }

            struct dirent* entry = nullptr;

            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0
                    || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                NkPath fullPath = NkPath(path) / entry->d_name;
                struct stat st;

                // Vérification que c'est un répertoire via stat()
                if (stat(fullPath.CStr(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    if (MatchesPattern(entry->d_name, pattern)) {
                        results.PushBack(fullPath.ToString());
                    }
                }
            }

            closedir(dir);
        #endif

        // Gestion de l'option récursive
        if (option == NkSearchOption::NK_ALL_DIRECTORIES) {
            GetDirectoriesRecursive(path, pattern, results);
        }

        return results;
    }

    NkVector<NkString> NkDirectory::GetDirectories(
        const NkPath& path,
        const char* pattern,
        NkSearchOption option
    ) {
        // Délégation à la version C-string
        return GetDirectories(path.CStr(), pattern, option);
    }

    NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
        const char* path,
        const char* pattern,
        NkSearchOption option
    ) {
        // Vecteur de résultats à retourner
        NkVector<NkDirectoryEntry> results;

        // Guards : chemin requis et existence vérifiée
        if (!path || !Exists(path)) {
            return results;
        }

        #ifdef _WIN32
            // Windows : FindFirstFile fournit directement les métadonnées
            WIN32_FIND_DATAA findData;
            NkString searchPath = NkString(path) + "\\*";
            HANDLE hFind = FindFirstFileA(searchPath.CStr(), &findData);

            if (hFind == INVALID_HANDLE_VALUE) {
                return results;
            }

            do {
                if (strcmp(findData.cFileName, ".") == 0
                    || strcmp(findData.cFileName, "..") == 0) {
                    continue;
                }

                // Appliquer le pattern de filtrage
                if (MatchesPattern(findData.cFileName, pattern)) {
                    NkDirectoryEntry entry;
                    entry.Name = findData.cFileName;
                    entry.FullPath = NkPath(path) / findData.cFileName;

                    // Détermination du type via les attributs Windows
                    entry.IsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    entry.IsFile = !entry.IsDirectory;

                    // Taille : combinaison des parties haute et basse (64-bit)
                    entry.Size = (static_cast<nk_int64>(findData.nFileSizeHigh) << 32)
                               | findData.nFileSizeLow;

                    // Note : ModificationTime n'est pas extrait ici (disponible via findData.ftLastWriteTime)

                    results.PushBack(entry);
                }
            } while (FindNextFileA(hFind, &findData));

            FindClose(hFind);

        #else
            // POSIX : nécessite stat() séparé pour obtenir les métadonnées
            DIR* dir = opendir(path);
            if (!dir) {
                return results;
            }

            struct dirent* entry = nullptr;

            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0
                    || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                if (MatchesPattern(entry->d_name, pattern)) {
                    NkDirectoryEntry dirEntry;
                    dirEntry.Name = entry->d_name;
                    dirEntry.FullPath = NkPath(path) / entry->d_name;

                    // Appel à stat() pour obtenir les métadonnées
                    struct stat st;
                    if (stat(dirEntry.FullPath.CStr(), &st) == 0) {
                        dirEntry.IsDirectory = S_ISDIR(st.st_mode);
                        dirEntry.IsFile = S_ISREG(st.st_mode);
                        dirEntry.Size = st.st_size;
                        dirEntry.ModificationTime = st.st_mtime;
                    }

                    results.PushBack(dirEntry);
                }
            }

            closedir(dir);
        #endif

        // Gestion de l'option récursive
        if (option == NkSearchOption::NK_ALL_DIRECTORIES) {
            GetEntriesRecursive(path, pattern, results);
        }

        return results;
    }

    NkVector<NkDirectoryEntry> NkDirectory::GetEntries(
        const NkPath& path,
        const char* pattern,
        NkSearchOption option
    ) {
        // Délégation à la version C-string
        return GetEntries(path.CStr(), pattern, option);
    }

    // =============================================================================
    //  Copie et déplacement de répertoires
    // =============================================================================
    // Opérations de manipulation de répertoires au niveau système.

    bool NkDirectory::Copy(
        const char* source,
        const char* dest,
        bool recursive,
        bool overwrite
    ) {
        // Guards : chemins requis et source existante
        if (!source || !dest || !Exists(source)) {
            return false;
        }

        // Création de la destination si elle n'existe pas
        if (!Exists(dest)) {
            if (!CreateRecursive(dest)) {
                return false;
            }
        }

        // Copie des fichiers au niveau courant
        NkVector<NkString> files = GetFiles(source);
        for (usize i = 0; i < files.Size(); ++i) {
            // Extraction du nom de fichier pour reconstruction du chemin destination
            NkPath filename = NkPath(files[i]).GetFileName();
            NkPath destFile = NkPath(dest) / filename;

            // Copie du fichier via NkFile::Copy avec gestion d'overwrite
            if (!NkFile::Copy(files[i].CStr(), destFile.CStr(), overwrite)) {
                return false;  // Échec de copie : arrêt immédiat
            }
        }

        // Copie récursive des sous-répertoires si demandée
        if (recursive) {
            NkVector<NkString> dirs = GetDirectories(source);

            for (usize i = 0; i < dirs.Size(); ++i) {
                // Reconstruction du chemin destination pour ce sous-répertoire
                NkPath dirname = NkPath(dirs[i]).GetFileName();
                NkPath destDir = NkPath(dest) / dirname;

                // Appel récursif avec propagation des paramètres
                if (!Copy(dirs[i].CStr(), destDir.CStr(), true, overwrite)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool NkDirectory::Copy(
        const NkPath& source,
        const NkPath& dest,
        bool recursive,
        bool overwrite
    ) {
        // Délégation à la version C-string
        return Copy(source.CStr(), dest.CStr(), recursive, overwrite);
    }

    bool NkDirectory::Move(const char* source, const char* dest) {
        // Guards : chemins requis et source existante
        if (!source || !dest || !Exists(source)) {
            return false;
        }

        // rename() effectue le déplacement au niveau système
        // Comportement : échoue si cross-volume sur certains OS
        return rename(source, dest) == 0;
    }

    bool NkDirectory::Move(const NkPath& source, const NkPath& dest) {
        // Délégation à la version C-string
        return Move(source.CStr(), dest.CStr());
    }

    // =============================================================================
    //  Répertoires spéciaux du système
    // =============================================================================
    // Accès aux chemins standards de l'environnement utilisateur.

    NkPath NkDirectory::GetCurrentDirectory() {
        // Délégation à NkPath::GetCurrentDirectory pour cohérence
        return NkPath::GetCurrentDirectory();
    }

    bool NkDirectory::SetCurrentDirectory(const char* path) {
        // Guard : chemin requis
        if (!path) {
            return false;
        }

        #ifdef _WIN32
            // Windows : SetCurrentDirectoryA retourne non-zero en cas de succès
            return SetCurrentDirectoryA(path) != 0;
        #else
            // POSIX : chdir retourne 0 en cas de succès
            return chdir(path) == 0;
        #endif
    }

    bool NkDirectory::SetCurrentDirectory(const NkPath& path) {
        // Délégation à la version C-string
        return SetCurrentDirectory(path.CStr());
    }

    NkPath NkDirectory::GetTempDirectory() {
        // Délégation à NkPath::GetTempDirectory pour cohérence
        return NkPath::GetTempDirectory();
    }

    NkPath NkDirectory::GetHomeDirectory() {
        #ifdef _WIN32
            // Windows : tentative via SHGetFolderPathA (API shell)
            char path[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path) == S_OK) {
                return NkPath(path);
            }

            // Fallback : variables d'environnement HOMEDRIVE + HOMEPATH
            const char* homeDrive = getenv("HOMEDRIVE");
            const char* homePath = getenv("HOMEPATH");
            if (homeDrive && homePath) {
                return NkPath(NkString(homeDrive) + homePath);
            }
        #else
            // POSIX : variable d'environnement HOME en priorité
            const char* home = getenv("HOME");
            if (home) {
                return NkPath(home);
            }

            // Fallback : entrée passwd pour l'utilisateur courant
            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                return NkPath(pw->pw_dir);
            }
        #endif

        // Échec de toutes les méthodes : retour chemin vide
        return NkPath();
    }

    NkPath NkDirectory::GetAppDataDirectory() {
        #ifdef _WIN32
            // Windows : CSIDL_APPDATA pointe vers %APPDATA% (Roaming)
            char path[MAX_PATH];
            if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
                return NkPath(path);
            }
        #else
            // Unix : convention XDG Base Directory (~/.config)
            NkPath home = GetHomeDirectory();
            if (!home.ToString().Empty()) {
                return home / ".config";
            }
        #endif

        // Échec : retour chemin vide
        return NkPath();
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Pattern matching (glob-style) :
    ------------------------------
    - Implémentation manuelle sans dépendance externe (regex/glob)
    - Supporte '*' (zéro ou plusieurs caractères) et '?' (un caractère)
    - Algorithme avec backtracking pour gérer les cas complexes
    - Performance : O(n*m) dans le pire cas, acceptable pour les noms de fichiers

    Récursivité et pile d'appels :
    -----------------------------
    - Les méthodes Recursive utilisent l'appel de fonction récursif
    - Pour des arbres de répertoires très profonds (>1000 niveaux), risque de stack overflow
    - Solution future : implémentation itérative avec pile explicite si nécessaire

    Gestion des permissions :
    ------------------------
    - mkdir() sur POSIX utilise le mode 0755 (rwxr-xr-x)
    - Les permissions réelles peuvent être modifiées par umask du processus
    - Windows : les permissions sont héritées du parent ou définies par politique système

    Métadonnées et précision :
    -------------------------
    - Windows : FILETIME a une résolution de 100ns, mais GetEntries ne l'extrait pas
    - POSIX : st_mtime a une résolution variable selon le filesystem (souvent 1s)
    - Size sur Windows : combinaison correcte de nFileSizeHigh/nFileSizeLow pour >4GB

    Thread-safety :
    --------------
    - Toutes les méthodes sont statiques et sans état mutable partagé
    - Thread-safe pour lecture concurrente
    - Attention aux opérations concurrentes sur le même répertoire (race conditions)

    Limitations connues :
    --------------------
    - Les liens symboliques sont suivis comme des répertoires/fichiers normaux
    - Pas de support explicite des chemins UNC Windows (\\server\share)
    - GetEntries ne remplit pas ModificationTime sur Windows (disponible mais non extrait)

    Évolutions futures possibles :
    -----------------------------
    - Support des attributs étendus (readonly, hidden, system)
    - Méthode Watch() pour notification de changements (inotify/ReadDirectoryChangesW)
    - Filtrage avancé avec expressions régulières au lieu de glob simple
    - Support des permissions Unix via chmod/chown wrappers
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================