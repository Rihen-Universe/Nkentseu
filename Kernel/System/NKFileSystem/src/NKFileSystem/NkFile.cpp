// =============================================================================
// NKFileSystem/NkFile.cpp
// Implémentation de la classe NkFile.
//
// Design :
//  - Utilise l'API C standard de fichier (fopen/fread/fwrite/fseek)
//  - Gestion RAII : fermeture automatique dans le destructeur
//  - Support multiplateforme via détection NKPlatform
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
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"   // MoveToTrash : impl partagee (fichiers + dossiers)

// En-têtes C standard pour les opérations fichier
#include <cstdio>
#include <cstring>

// En-têtes plateforme pour les opérations système (stat, GetFileAttributes, etc.)
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/stat.h>
#endif

// En-têtes Android NDK pour AAssetManager (fallback assets/ dans l'APK)
#include "NKPlatform/NkPlatformDetect.h"
#if defined(NKENTSEU_PLATFORM_ANDROID)
    #include <android/asset_manager.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes de NkFile dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  Etat global — AAssetManager Android
    // =============================================================================
    // Pointeur global vers l'AAssetManager fourni par la NativeActivity au
    // demarrage. Permet a NkFile::Open() de tomber en fallback sur les assets
    // de l'APK quand fopen echoue (cas typique : path relatif a Resources/).

#if defined(NKENTSEU_PLATFORM_ANDROID)
    static AAssetManager* sAndroidAssetMgr = nullptr;
#else
    static void* sAndroidAssetMgr = nullptr;  ///< unused hors Android
#endif
    static char sAndroidAssetSubFolder[64] = {0};  ///< ex: "Pong"
    static size_t sAndroidAssetSubFolderLen = 0;

    void NkFile::SetAndroidAssetManager(void* manager) {
#if defined(NKENTSEU_PLATFORM_ANDROID)
        sAndroidAssetMgr = static_cast<AAssetManager*>(manager);
#else
        (void)manager;  // no-op
#endif
    }

    void* NkFile::GetAndroidAssetManager() {
        return static_cast<void*>(sAndroidAssetMgr);
    }

    void NkFile::SetAndroidAssetSubFolder(const char* name) {
        if (!name || !name[0]) {
            sAndroidAssetSubFolder[0] = '\0';
            sAndroidAssetSubFolderLen = 0;
            return;
        }
        const size_t n = std::strlen(name);
        const size_t maxN = sizeof(sAndroidAssetSubFolder) - 1;
        const size_t copyN = (n < maxN) ? n : maxN;
        std::memcpy(sAndroidAssetSubFolder, name, copyN);
        sAndroidAssetSubFolder[copyN] = '\0';
        sAndroidAssetSubFolderLen = copyN;
    }

#if defined(NKENTSEU_PLATFORM_ANDROID)
    // -------------------------------------------------------------------------
    // Helper : tente d'ouvrir un asset depuis l'AAssetManager avec quelques
    // variantes du path (strip "Resources/" si present, puis brut). Retourne
    // un AAsset* (a fermer avec AAsset_close) ou nullptr.
    // -------------------------------------------------------------------------
    static AAsset* TryOpenAndroidAsset(const char* path) {
        if (!sAndroidAssetMgr || !path) return nullptr;
        // 1. Path tel quel (cas ou androidassets a ete configure pour
        // preserver l'arborescence complete).
        AAsset* asset = AAssetManager_open(sAndroidAssetMgr, path,
                                           AASSET_MODE_BUFFER);
        if (asset) return asset;

        const char* kPrefix = "Resources/";
        const size_t prefixLen = 10;
        const bool hasResPrefix =
            std::strncmp(path, kPrefix, prefixLen) == 0;

        // 2. Strip "Resources/<SubFolder>/" si SubFolder est configure.
        // Cas typique : androidassets(["../../Resources/Pong"]) -> les
        // fichiers vont dans assets/Textures/... ; le path C++
        // "Resources/Pong/Textures/logo.png" doit donc voir "Resources/Pong/"
        // strippes pour matcher "Textures/logo.png".
        if (hasResPrefix && sAndroidAssetSubFolderLen > 0) {
            const size_t subStart = prefixLen;  // apres "Resources/"
            if (std::strncmp(path + subStart, sAndroidAssetSubFolder,
                             sAndroidAssetSubFolderLen) == 0
                && path[subStart + sAndroidAssetSubFolderLen] == '/')
            {
                const size_t fullStrip = subStart
                                       + sAndroidAssetSubFolderLen
                                       + 1;  // '/'
                asset = AAssetManager_open(sAndroidAssetMgr, path + fullStrip,
                                           AASSET_MODE_BUFFER);
                if (asset) return asset;
            }
        }

        // 3. Strip "Resources/" seul, en dernier recours (cas
        // androidassets(["../../Resources"]) qui bundle tout).
        if (hasResPrefix) {
            asset = AAssetManager_open(sAndroidAssetMgr, path + prefixLen,
                                       AASSET_MODE_BUFFER);
            if (asset) return asset;
        }
        return nullptr;
    }
#endif

    // =============================================================================
    //  Méthodes privées
    // =============================================================================

    const char* NkFile::GetModeString() const {
        // Extraction des flags individuels depuis le mode combiné
        const bool read = NkHasFlag(mMode, NkFileMode::NK_READ);
        const bool write = NkHasFlag(mMode, NkFileMode::NK_WRITE);
        const bool append = NkHasFlag(mMode, NkFileMode::NK_APPEND);
        const bool truncate = NkHasFlag(mMode, NkFileMode::NK_TRUNCATE);
        const bool binary = NkHasFlag(mMode, NkFileMode::NK_BINARY);

        // ---------------------------------------------------------------------
        // Validation stricte des combinaisons
        // ---------------------------------------------------------------------
        // Append et truncate sont mutuellement exclusifs : comportement contradictoire
        if (append && truncate) {
            return "";  // Combinaison invalide
        }

        // ---------------------------------------------------------------------
        // Cas principaux : mapping vers les modes fopen C standard
        // ---------------------------------------------------------------------

        // Lecture seule : "r" ou "rb"
        if (read && !write && !append) {
            return binary ? "rb" : "r";
        }

        // Écriture seule (truncate) : "w" ou "wb"
        if (write && !read && !append) {
            return binary ? "wb" : "w";
        }

        // Ajout seul : "a" ou "ab"
        if (append && !read && !write) {
            return binary ? "ab" : "a";
        }

        // Lecture + Écriture sans truncate : "r+" ou "rb+"
        if (read && write && !append && !truncate) {
            return binary ? "rb+" : "r+";
        }

        // Écriture + Lecture avec truncate : "w+" ou "wb+"
        if (read && write && truncate) {
            return binary ? "wb+" : "w+";
        }

        // Lecture + Ajout : "a+" ou "ab+"
        if (read && append) {
            return binary ? "ab+" : "a+";
        }

        // Combinaison non supportée : retour chaîne vide pour échec d'ouverture
        return "";
    }

    // =============================================================================
    //  Constructeurs / Destructeur
    // =============================================================================
    // Gestion du cycle de vie avec politique RAII : fermeture automatique.

    NkFile::NkFile()
        : mHandle(nullptr)
        , mPath()
        , mMode(NkFileMode::NK_READ)
        , mIsOpen(false)
        , mIsAsset(false)
    {
        // Constructeur par défaut : état initial "fermé"
    }

    NkFile::NkFile(const char* path, NkFileMode mode)
        : mHandle(nullptr)
        , mPath(path ? path : "")
        , mMode(mode)
        , mIsOpen(false)
        , mIsAsset(false)
    {
        Open(path, mode);
    }

    NkFile::NkFile(const NkPath& path, NkFileMode mode)
        : mHandle(nullptr)
        , mPath(path)
        , mMode(mode)
        , mIsOpen(false)
        , mIsAsset(false)
    {
        Open(path, mode);
    }

    NkFile::~NkFile() {
        // Destructeur RAII : garantit la fermeture du fichier
        // Idempotent : Close() gère déjà le cas "déjà fermé"
        Close();
    }

    // =============================================================================
    //  Sémantique de mouvement
    // =============================================================================
    // Transfert de propriété du descripteur fichier sans duplication.

    NkFile::NkFile(NkFile&& other) noexcept
        : mHandle(other.mHandle)
        , mPath(other.mPath)
        , mMode(other.mMode)
        , mIsOpen(other.mIsOpen)
        , mIsAsset(other.mIsAsset)
    {
        other.mHandle  = nullptr;
        other.mIsOpen  = false;
        other.mIsAsset = false;
    }

    NkFile& NkFile::operator=(NkFile&& other) noexcept {
        if (this != &other) {
            Close();
            mHandle  = other.mHandle;
            mPath    = other.mPath;
            mMode    = other.mMode;
            mIsOpen  = other.mIsOpen;
            mIsAsset = other.mIsAsset;
            other.mHandle  = nullptr;
            other.mIsOpen  = false;
            other.mIsAsset = false;
        }
        return *this;
    }

    // =============================================================================
    //  Ouverture / Fermeture
    // =============================================================================
    // Gestion du descripteur système avec conversion des modes.

    bool NkFile::Open(const char* path, NkFileMode mode) {
        // Fermeture préalable : garantit qu'un seul fichier est ouvert à la fois
        Close();

        // Validation du paramètre path
        if (!path) {
            mPath = "";
            mMode = mode;
            return false;
        }

        // Mise à jour de l'état interne avant tentative d'ouverture
        mPath    = path;
        mMode    = mode;
        mIsAsset = false;

        // Conversion du mode NKEntseu vers mode fopen C
        const char* modeStr = GetModeString();
        if (!modeStr || !modeStr[0]) {
            return false;
        }

        // Ouverture via l'API C standard
        FILE* file = fopen(path, modeStr);
        if (file) {
            mHandle = file;
            mIsOpen = true;
            return true;
        }

#if defined(NKENTSEU_PLATFORM_ANDROID)
        // Fallback Android : si fopen echoue ET qu'on est en lecture seule
        // (les assets de l'APK sont read-only), tenter via AAssetManager.
        const bool isWriteMode = NkHasFlag(mode, NkFileMode::NK_WRITE)
                              || NkHasFlag(mode, NkFileMode::NK_APPEND)
                              || NkHasFlag(mode, NkFileMode::NK_TRUNCATE);
        if (!isWriteMode) {
            AAsset* asset = TryOpenAndroidAsset(path);
            if (asset) {
                mHandle  = asset;
                mIsOpen  = true;
                mIsAsset = true;
                return true;
            }
        }
#endif
        return false;
    }

    bool NkFile::Open(const NkPath& path, NkFileMode mode) {
        // Délégation à la version C-string pour éviter la duplication de code
        return Open(path.CStr(), mode);
    }

    void NkFile::Close() {
        if (mIsOpen && mHandle) {
#if defined(NKENTSEU_PLATFORM_ANDROID)
            if (mIsAsset) {
                AAsset_close(static_cast<AAsset*>(mHandle));
            } else {
                fclose(static_cast<FILE*>(mHandle));
            }
#else
            fclose(static_cast<FILE*>(mHandle));
#endif
            mHandle  = nullptr;
            mIsOpen  = false;
            mIsAsset = false;
        }
    }

    bool NkFile::IsOpen() const {
        // Accesseur simple : retourne l'état d'ouverture
        return mIsOpen;
    }

    // =============================================================================
    //  Lecture
    // =============================================================================
    // Méthodes pour extraire des données depuis le fichier.

    usize NkFile::Read(void* buffer, usize size) {
        if (!mIsOpen || !buffer) {
            return 0;
        }
#if defined(NKENTSEU_PLATFORM_ANDROID)
        if (mIsAsset) {
            const int r = AAsset_read(static_cast<AAsset*>(mHandle), buffer,
                                      static_cast<size_t>(size));
            return (r < 0) ? 0 : static_cast<usize>(r);
        }
#endif
        return fread(buffer, 1, size, static_cast<FILE*>(mHandle));
    }

    NkString NkFile::ReadLine() {
        // Guard : fichier doit être ouvert
        if (!mIsOpen) {
            return NkString();
        }

        // Buffer temporaire pour la lecture ligne par ligne
        // Taille de 4096 : compromis entre performance et usage mémoire
        char buffer[4096];

        // fgets lit jusqu'au newline ou EOF, incluant le newline dans le buffer
        if (!fgets(buffer, sizeof(buffer), static_cast<FILE*>(mHandle))) {
            // EOF ou erreur : retour chaîne vide
            return NkString();
        }

        // Calcul de la longueur lue
        usize len = strlen(buffer);

        // ---------------------------------------------------------------------
        // Normalisation des newlines : suppression de \r\n ou \n en fin de ligne
        // ---------------------------------------------------------------------
        if (len > 0 && buffer[len - 1] == '\n') {
            // Suppression du \n
            buffer[len - 1] = '\0';

            // Suppression du \r précédent si présent (format Windows \r\n)
            if (len > 1 && buffer[len - 2] == '\r') {
                buffer[len - 2] = '\0';
            }
        }

        // Construction du NkString depuis le buffer normalisé
        return NkString(buffer);
    }

    NkString NkFile::ReadAll() {
        // Guard : fichier doit être ouvert
        if (!mIsOpen) {
            return NkString();
        }

        // Obtention de la taille pour pré-allouer le buffer
        const nk_int64 size = GetSize();
        if (size <= 0) {
            // Fichier vide ou erreur de taille : retour chaîne vide
            return NkString();
        }

        // Allocation du buffer avec +1 pour le terminateur nul
        NkVector<char> buffer;
        buffer.Resize(static_cast<usize>(size) + 1);

        // Lecture de la totalité du contenu
        const usize read = Read(buffer.Data(), static_cast<usize>(size));

        // Terminaison de la chaîne C pour construction NkString
        buffer[read] = '\0';

        // Construction depuis le buffer lu (peut être < size en cas d'erreur partielle)
        return NkString(buffer.Data());
    }

    NkVector<NkString> NkFile::ReadLines() {
        // Vecteur de résultat pour accumuler les lignes
        NkVector<NkString> lines;

        // Boucle de lecture jusqu'à EOF
        while (!IsEOF()) {
            NkString line = ReadLine();

            // Ajout de la ligne si non vide OU si pas encore à EOF
            // Cette logique préserve les lignes vides légitimes dans le fichier
            if (!line.Empty() || !IsEOF()) {
                lines.PushBack(line);
            }
        }

        return lines;
    }

    // =============================================================================
    //  Écriture
    // =============================================================================
    // Méthodes pour écrire des données dans le fichier.

    usize NkFile::Write(const void* data, usize size) {
        if (!mIsOpen || !data) {
            return 0;
        }
        // Les assets Android sont en lecture seule — interdire l'ecriture.
        if (mIsAsset) {
            return 0;
        }
        return fwrite(data, 1, size, static_cast<FILE*>(mHandle));
    }

    bool NkFile::WriteLine(const char* text) {
        // Guards : fichier ouvert et texte non-null requis
        if (!mIsOpen || !text) {
            return false;
        }

        // Écriture du texte principal
        const usize len = strlen(text);
        if (Write(text, len) != len) {
            // Écriture partielle ou échec : retour erreur
            return false;
        }

        // Ajout du newline plateforme-indépendant (\n)
        // Sur Windows en mode texte, fopen convertira \n en \r\n automatiquement
        const char newline[] = "\n";
        if (Write(newline, 1) != 1) {
            return false;
        }

        return true;
    }

    bool NkFile::Write(const NkString& text) {
        // Délégation à la version C-string avec vérification d'écriture complète
        return Write(text.CStr(), text.Length()) == text.Length();
    }

    // =============================================================================
    //  Position du curseur
    // =============================================================================
    // Navigation dans le fichier via fseek/ftell.

    nk_int64 NkFile::Tell() const {
        if (!mIsOpen) {
            return -1;
        }
#if defined(NKENTSEU_PLATFORM_ANDROID)
        if (mIsAsset) {
            AAsset* a = static_cast<AAsset*>(mHandle);
            const off_t total = AAsset_getLength(a);
            const off_t remaining = AAsset_getRemainingLength(a);
            return static_cast<nk_int64>(total - remaining);
        }
#endif
        return static_cast<nk_int64>(ftell(static_cast<FILE*>(mHandle)));
    }

    bool NkFile::Seek(nk_int64 offset, NkSeekOrigin origin) {
        if (!mIsOpen) {
            return false;
        }

        int whence = SEEK_SET;
        switch (origin) {
            case NkSeekOrigin::NK_BEGIN:   whence = SEEK_SET; break;
            case NkSeekOrigin::NK_CURRENT: whence = SEEK_CUR; break;
            case NkSeekOrigin::NK_END:     whence = SEEK_END; break;
            default:                       whence = SEEK_SET; break;
        }

#if defined(NKENTSEU_PLATFORM_ANDROID)
        if (mIsAsset) {
            const off_t pos = AAsset_seek(static_cast<AAsset*>(mHandle),
                                          static_cast<off_t>(offset), whence);
            return pos != (off_t)-1;
        }
#endif
        return fseek(static_cast<FILE*>(mHandle),
                     static_cast<long>(offset), whence) == 0;
    }

    bool NkFile::SeekToBegin() {
        // Délégation à Seek avec paramètres prédéfinis
        return Seek(0, NkSeekOrigin::NK_BEGIN);
    }

    bool NkFile::SeekToEnd() {
        // Délégation à Seek avec paramètres prédéfinis
        return Seek(0, NkSeekOrigin::NK_END);
    }

    nk_int64 NkFile::GetSize() const {
        if (!mIsOpen) {
            return -1;
        }
#if defined(NKENTSEU_PLATFORM_ANDROID)
        if (mIsAsset) {
            return static_cast<nk_int64>(
                AAsset_getLength(static_cast<AAsset*>(mHandle)));
        }
#endif
        const nk_int64 current = Tell();
        fseek(static_cast<FILE*>(mHandle), 0, SEEK_END);
        const nk_int64 size = static_cast<nk_int64>(
            ftell(static_cast<FILE*>(mHandle)));
        fseek(static_cast<FILE*>(mHandle), static_cast<long>(current), SEEK_SET);
        return size;
    }

    // =============================================================================
    //  Buffers et synchronisation
    // =============================================================================

    void NkFile::Flush() {
        // Sans effet sur les assets Android (read-only).
        if (mIsOpen && !mIsAsset) {
            fflush(static_cast<FILE*>(mHandle));
        }
    }

    // =============================================================================
    //  Propriétés
    // =============================================================================
    // Accesseurs en lecture seule pour l'état interne.

    const NkPath& NkFile::GetPath() const {
        // Retour par référence constante : zéro copie, zéro allocation
        return mPath;
    }

    NkFileMode NkFile::GetMode() const {
        // Retour direct de la valeur de mode configurée
        return mMode;
    }

    bool NkFile::IsEOF() const {
        if (!mIsOpen) {
            return true;
        }
#if defined(NKENTSEU_PLATFORM_ANDROID)
        if (mIsAsset) {
            return AAsset_getRemainingLength(
                static_cast<AAsset*>(mHandle)) == 0;
        }
#endif
        return feof(static_cast<FILE*>(mHandle)) != 0;
    }

    // =============================================================================
    //  Utilitaires statiques — Existence et suppression
    // =============================================================================
    // Opérations ne nécessitant pas d'ouverture explicite du fichier.

    bool NkFile::Exists(const char* path) {
        // Guard : chemin null ou vide considéré comme inexistant
        if (!path) {
            return false;
        }

        #ifdef _WIN32
            // Windows : GetFileAttributesA retourne INVALID_FILE_ATTRIBUTES si inexistant
            const DWORD attrs = GetFileAttributesA(path);
            return (attrs != INVALID_FILE_ATTRIBUTES)
                && !(attrs & FILE_ATTRIBUTE_DIRECTORY);  // Exclure les répertoires
        #else
            // POSIX : stat remplit une structure avec les métadonnées du fichier
            struct stat st;
            return (stat(path, &st) == 0)  // Succès de l'appel système
                && S_ISREG(st.st_mode);     // Vérification que c'est un fichier régulier
        #endif
    }

    bool NkFile::Exists(const NkPath& path) {
        // Délégation à la version C-string pour éviter la duplication
        return Exists(path.CStr());
    }

    bool NkFile::Delete(const char* path) {
        // Guard : chemin null ne peut être supprimé
        if (!path) {
            return false;
        }

        // remove() supprime le fichier du système
        // Retourne 0 en cas de succès, non-zéro en cas d'erreur
        return remove(path) == 0;
    }

    bool NkFile::MoveToTrash(const char* path) { return NkDirectory::MoveToTrash(path); }
    bool NkFile::MoveToTrash(const NkPath& path) { return NkDirectory::MoveToTrash(path.CStr()); }

    bool NkFile::Delete(const NkPath& path) {
        // Délégation à la version C-string
        return Delete(path.CStr());
    }

    // =============================================================================
    //  Utilitaires statiques — Copie et déplacement
    // =============================================================================
    // Opérations de manipulation de fichiers au niveau système.

    bool NkFile::Copy(
        const char* source,
        const char* dest,
        bool overwrite
    ) {
        // Guards : chemins source et destination requis
        if (!source || !dest) {
            return false;
        }

        // Gestion de l'overwrite : échec si destination existe et overwrite=false
        if (!overwrite && Exists(dest)) {
            return false;
        }

        // Ouverture du fichier source en lecture binaire
        NkFile src(source, NkFileMode::NK_READ_BINARY);
        if (!src.IsOpen()) {
            return false;
        }

        // Ouverture du fichier destination en écriture binaire (truncate implicite)
        NkFile dst(dest, NkFileMode::NK_WRITE_BINARY);
        if (!dst.IsOpen()) {
            return false;
        }

        // Buffer de copie : 8KB pour équilibrer performance et usage mémoire
        char buffer[8192];
        usize read = 0;

        // Boucle de copie : lecture depuis source, écriture vers destination
        while ((read = src.Read(buffer, sizeof(buffer))) > 0) {
            // Vérification que l'écriture a écrit autant que la lecture
            if (dst.Write(buffer, read) != read) {
                // Écriture partielle : échec de la copie
                return false;
            }
        }

        // Succès : les destructeurs RAII ferment automatiquement les fichiers
        return true;
    }

    bool NkFile::Copy(
        const NkPath& source,
        const NkPath& dest,
        bool overwrite
    ) {
        // Délégation à la version C-string
        return Copy(source.CStr(), dest.CStr(), overwrite);
    }

    bool NkFile::Move(const char* source, const char* dest) {
        // Guards : chemins requis
        if (!source || !dest) {
            return false;
        }

        // rename() déplace/renomme le fichier au niveau système
        // Comportement : échoue si source et dest sur volumes différents (selon OS)
        return rename(source, dest) == 0;
    }

    bool NkFile::Move(const NkPath& source, const NkPath& dest) {
        // Délégation à la version C-string
        return Move(source.CStr(), dest.CStr());
    }

    // =============================================================================
    //  Utilitaires statiques — Lecture/Écriture atomiques
    // =============================================================================
    // Méthodes de commodité pour les opérations courantes en une ligne.

    nk_int64 NkFile::GetFileSize(const char* path) {
        // Ouverture temporaire en lecture binaire pour obtenir la taille
        NkFile file(path, NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) {
            return -1;  // Échec d'ouverture
        }
        // GetSize() gère l'ouverture/fermeture interne
        return file.GetSize();
    }

    nk_int64 NkFile::GetFileSize(const NkPath& path) {
        // Délégation à la version C-string
        return GetFileSize(path.CStr());
    }

    NkString NkFile::ReadAllText(const char* path) {
        // Ouverture temporaire en mode texte
        NkFile file(path, NkFileMode::NK_READ);
        if (!file.IsOpen()) {
            return NkString();  // Échec : retour chaîne vide
        }
        // ReadAll() gère l'allocation et la lecture complète
        return file.ReadAll();
    }

    NkString NkFile::ReadAllText(const NkPath& path) {
        // Délégation à la version C-string
        return ReadAllText(path.CStr());
    }

    NkVector<nk_uint8> NkFile::ReadAllBytes(const char* path) {
        // Ouverture temporaire en mode binaire
        NkFile file(path, NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) {
            return NkVector<nk_uint8>();  // Échec : retour vecteur vide
        }

        // Obtention de la taille pour pré-alllocation
        const nk_int64 size = file.GetSize();
        if (size <= 0) {
            return NkVector<nk_uint8>();  // Fichier vide ou erreur
        }

        // Allocation et lecture directe dans le vecteur
        NkVector<nk_uint8> data;
        data.Resize(static_cast<usize>(size));
        file.Read(data.Data(), static_cast<usize>(size));

        return data;
    }

    NkVector<nk_uint8> NkFile::ReadAllBytes(const NkPath& path) {
        // Délégation à la version C-string
        return ReadAllBytes(path.CStr());
    }

    bool NkFile::WriteAllText(const char* path, const char* text) {
        // Guards : chemins et contenu requis
        if (!path || !text) {
            return false;
        }

        // Ouverture en mode écriture (truncate implicite)
        NkFile file(path, NkFileMode::NK_WRITE);
        if (!file.IsOpen()) {
            return false;
        }

        // Écriture avec vérification que tout le contenu a été écrit
        const usize len = strlen(text);
        return file.Write(text, len) == len;
    }

    bool NkFile::WriteAllText(const NkPath& path, const NkString& text) {
        // Délégation à la version C-string
        return WriteAllText(path.CStr(), text.CStr());
    }

    bool NkFile::WriteAllBytes(
        const char* path,
        const NkVector<nk_uint8>& data
    ) {
        // Guard : chemin requis
        if (!path) {
            return false;
        }

        // Ouverture en mode écriture binaire (truncate implicite)
        NkFile file(path, NkFileMode::NK_WRITE_BINARY);
        if (!file.IsOpen()) {
            return false;
        }

        // Écriture avec vérification que toutes les données ont été écrites
        return file.Write(data.Data(), data.Size()) == data.Size();
    }

    bool NkFile::WriteAllBytes(
        const NkPath& path,
        const NkVector<nk_uint8>& data
    ) {
        // Délégation à la version C-string
        return WriteAllBytes(path.CStr(), data);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Gestion des modes d'ouverture :
    ------------------------------
    - GetModeString() mappe les flags NKEntseu vers les modes fopen C standard
    - Validation stricte : append + truncate = invalide (comportements contradictoires)
    - Mode binaire : désactive la conversion automatique des newlines (\r\n ↔ \n)

    RAII et sécurité des ressources :
    --------------------------------
    - Le destructeur appelle toujours Close() : pas de fuite de descripteurs
    - Close() est idempotent : peut être appelé multiples fois sans effet
    - Move sémantique : transfert de propriété sans duplication de handle

    Performance des lectures :
    -------------------------
    - ReadLine() : buffer fixe 4096 bytes, adapté pour la majorité des lignes
    - ReadAll() : pré-allocation via GetSize() pour éviter les reallocs multiples
    - Copy() : buffer 8KB pour équilibrer I/O et usage mémoire

    Compatibilité multiplateforme :
    ------------------------------
    - Windows : GetFileAttributesA pour Exists(), fopen avec modes "rb"/"wb"
    - POSIX : stat() pour Exists(), fopen standard
    - Newlines : en mode texte, fopen gère automatiquement \r\n ↔ \n sur Windows

    Limitations connues :
    --------------------
    - GetSize() effectue un Seek temporaire : peut perturber la position courante
    - ReadAll() charge tout le fichier en mémoire : attention aux gros fichiers
    - Les chemins UNC Windows (\\server\share) ne sont pas testés explicitement

    Évolutions futures possibles :
    -----------------------------
    - Support des fichiers > 4GB sur Windows 32-bit via _fseeki64/_ftelli64
    - Méthode async Read/Write pour I/O non-bloquant
    - Support des encodings UTF-8/UTF-16 avec conversion automatique
    - Méthode Map() pour memory-mapped files sur les gros fichiers
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================