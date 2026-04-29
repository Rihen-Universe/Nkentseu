# NKFileSystem Module

> **Système de fichiers cross-platform pour l'infrastructure NKEntseu**

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](VERSION)
[![Plateformes](https://img.shields.io/badge/plateformes-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20Web-green.svg)](PLATFORMS)
[![Licence](https://img.shields.io/badge/licence-Propriétaire-orange.svg)](LICENSE)

---

## 📋 Table des Matières

1. [Aperçu](#-aperçu)
2. [Fonctionnalités](#-fonctionnalités)
3. [Architecture et Design](#-architecture-et-design)
4. [Installation et Intégration](#-installation-et-intégration)
5. [Référence API](#-référence-api)
6. [Exemples d'Utilisation](#-exemples-dutilisation)
7. [Bonnes Pratiques](#-bonnes-pratiques)
8. [Limitations Connues](#-limitations-connues)
9. [Dépannage](#-dépannage)
10. [Contribuer](#-contribuer)
11. [Licence](#-licence)

---

## 🔍 Aperçu

Le module **NKFileSystem** fournit une abstraction cross-platform complète pour la manipulation du système de fichiers, conçue spécifiquement pour l'infrastructure NKEntseu.

### Objectifs Principaux

| Objectif | Description |
|----------|-------------|
| 🎯 **Abstraction native** | Masquer les différences entre Windows API, POSIX et Web |
| 🔄 **Cohérence multiplateforme** | API unifiée avec comportement prédictible sur toutes les cibles |
| 🚀 **Performance** | Aucune dépendance STL, allocations contrôlées, zéro overhead inutile |
| 🔒 **Sécurité** | Validation des chemins, gestion défensive, pas d'injections de chemin |
| 🧩 **Modularité** | Classes indépendantes, couplage faible, extensibilité facilitée |

### Cas d'Usage Typiques

```cpp
// ✓ Chargement de ressources pour un moteur de jeu
NkPath assetPath = NkPath("assets") / "textures" / "player.png";
NkVector<nk_uint8> data = NkFile::ReadAllBytes(assetPath);

// ✓ Surveillance de fichiers pour le hot-reload
NkSimpleFileWatcher watcher("src/shaders", true);
watcher.OnChanged = [](const NkFileChangeEvent& e) {
    if (e.Type == NkFileChangeType::NK_MODIFIED) {
        ReloadShader(e.Path.CStr());
    }
};

// ✓ Gestion de configuration utilisateur
NkPath configDir = NkDirectory::GetAppDataDirectory() / "MyApp";
NkDirectory::CreateRecursive(configDir);
NkFile::WriteAllText(configDir / "settings.json", configJson);

// ✓ Nettoyage de cache avec filtrage
auto tmpFiles = NkDirectory::GetFiles("cache", "*.tmp");
for (const auto& file : tmpFiles) {
    NkFile::Delete(file);
}
```

---

## ✨ Fonctionnalités

### 🗂️ Manipulation de Chemins (`NkPath`)

```cpp
// Construction et normalisation
NkPath p1("C:\\Users\\Docs\\file.txt");  // Windows → normalisé en interne
NkPath p2("/home/user/docs/file.txt");    // Unix → inchangé

// Jointure intuitive avec opérateur /
NkPath full = NkPath("assets") / "models" / "character.fbx";

// Décomposition
p1.GetDirectory();           // "C:/Users/Docs"
p1.GetFileName();            // "file.txt"
p1.GetExtension();           // ".txt"
p1.GetFileNameWithoutExtension(); // "file"

// Propriétés
p1.IsAbsolute();             // true
p1.HasExtension();           // true

// Conversion plateforme
p1.ToNative();               // "C:\Users\Docs\file.txt" sur Windows
```

### 📄 Gestion de Fichiers (`NkFile`)

```cpp
// RAII avec fermeture automatique
{
    NkFile file("data.bin", NkFileMode::NK_READ_BINARY);
    if (file.IsOpen()) {
        auto bytes = file.ReadAll();  // Lecture complète
        // ... traitement ...
    }
    // Close() appelé automatiquement à la destruction
}

// Modes d'ouverture flexibles
NkFile log("app.log", NkFileMode::NK_APPEND);      // Ajout en fin
NkFile cfg("config.json", NkFileMode::NK_READ);    // Lecture seule
NkFile out("output.dat", NkFileMode::NK_WRITE_BINARY); // Écriture binaire

// Utilitaires statiques
NkFile::Exists("file.txt");              // Vérification d'existence
NkFile::Copy("src.txt", "bak.txt");      // Copie avec buffer interne
NkFile::Delete("temp.tmp");              // Suppression
NkFile::ReadAllText("readme.md");        // Lecture texte atomique
```

### 📁 Opérations sur Répertoires (`NkDirectory`)

```cpp
// Création et suppression
NkDirectory::CreateRecursive("logs/2024/01");     // Crée l'arborescence
NkDirectory::Delete("cache", true);               // Suppression récursive

// Énumération avec filtrage glob-style
auto pngFiles = NkDirectory::GetFiles(
    "assets", 
    "*.png", 
    NkSearchOption::NK_ALL_DIRECTORIES  // Récursif
);

// Métadonnées enrichies
auto entries = NkDirectory::GetEntries("data");
for (const auto& entry : entries) {
    if (entry.IsFile && entry.Size > 1024*1024) {
        NK_LOG_INFO("Large file: {} ({} bytes)", 
                   entry.Name.CStr(), entry.Size);
    }
}

// Répertoires spéciaux du système
NkPath home = NkDirectory::GetHomeDirectory();        // ~/ ou C:/Users/
NkPath temp = NkDirectory::GetTempDirectory();        // /tmp ou %TEMP%
NkPath appData = NkDirectory::GetAppDataDirectory();  // ~/.config ou %APPDATA%
```

### 🔍 Surveillance de Fichiers (`NkFileWatcher`)

```cpp
// Callback orienté objet
class MyWatcher : public NkFileWatcherCallback {
    void OnFileChanged(const NkFileChangeEvent& e) override {
        switch (e.Type) {
            case NkFileChangeType::NK_MODIFIED:
                ReloadResource(e.Path.CStr());
                break;
            // ... autres cas ...
        }
    }
};

MyWatcher cb;
NkFileWatcher watcher("assets/shaders", &cb, true);  // récursif
watcher.Start();  // Thread dédié en arrière-plan

// Alternative C++11 : callback fonctionnel
#if defined(NK_CPP11)
NkSimpleFileWatcher simple("config", true);
simple.OnChanged = [](const NkFileChangeEvent& e) {
    if (e.Path.EndsWith(".json")) {
        ReloadConfig();
    }
};
simple.Start();
#endif
```

### 🗄️ Métadonnées Système (`NkFileSystem`)

```cpp
// Informations sur les volumes
auto drives = NkFileSystem::GetDrives();
for (const auto& drive : drives) {
    if (drive.IsReady) {
        NK_LOG_INFO("{}: {} Go libres / {} Go totaux",
            drive.Name.CStr(),
            drive.AvailableSpace / (1024*1024*1024),
            drive.TotalSize / (1024*1024*1024));
    }
}

// Attributs de fichiers
NkFileAttributes attrs = NkFileSystem::GetFileAttributes("save.dat");
attrs.IsReadOnly = true;
NkFileSystem::SetFileAttributes("save.dat", attrs);

// Timestamps
nk_int64 modified = NkFileSystem::GetLastWriteTime("log.txt");

// Liens symboliques (Unix complet, Windows partiel)
if (!NkFileSystem::IsSymbolicLink("current")) {
    NkFileSystem::CreateSymbolicLink("current", "v2/config.json");
}
```

---

## 🏗️ Architecture et Design

### Principes Fondamentaux

```
┌─────────────────────────────────────────────────┐
│  NKFileSystem — Couche d'Abstraction            │
├─────────────────────────────────────────────────┤
│                                                 │
│  ┌─────────────┐  ┌─────────────┐              │
│  │  NkPath     │  │  NkFile     │              │
│  │  Chemins    │  │  Fichiers   │              │
│  └─────────────┘  └─────────────┘              │
│                                                 │
│  ┌─────────────┐  ┌─────────────┐              │
│  │ NkDirectory │  │NkFileWatcher│              │
│  │ Répertoires │  │ Surveillance│              │
│  └─────────────┘  └─────────────┘              │
│                                                 │
│  ┌─────────────────────────┐                   │
│  │    NkFileSystem         │                   │
│  │ Métadonnées & Utilitaires│                  │
│  └─────────────────────────┘                   │
│                                                 │
├─────────────────────────────────────────────────┤
│  Implémentation Plateforme (guards conditionnels)│
│  • Windows : Win32 API, ReadDirectoryChangesW   │
│  • Linux   : POSIX, inotify, statvfs            │
│  • macOS   : POSIX + extensions Darwin          │
│  • Web     : Fallback limité (EMSCRIPTEN)       │
└─────────────────────────────────────────────────┘
```

### Conventions de Code


| Convention                   | Application                                               | Exemple                                         |
| ---------------------------- | --------------------------------------------------------- | ----------------------------------------------- |
| **Namespace**                | Toutes les déclarations dans`nkentseu::`                 | `nkentseu::NkPath`                              |
| **Export macros**            | `NKENTSEU_FILESYSTEM_CLASS_EXPORT` sur classes uniquement | `class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFile` |
| **Méthodes statiques**      | Pas de macro d'export individuelle                        | `static bool Exists(const char* path)`          |
| **Inline functions**         | Pas de macro dans le header                               | `inline bool NkHasFlag(...)`                    |
| **One instruction per line** | Lisibilité et diff Git optimisés                        | Voir exemples ci-dessus                         |
| **Documentation Doxygen**    | `///` pour méthodes, `/** */` pour blocs                 | Commentaires enrichis                           |

### Gestion des Plateformes

```cpp
// Pattern recommandé dans l'implémentation (.cpp)
#ifdef _WIN32
    // Code Windows spécifique
    #include <windows.h>
    // Undef des macros conflictuelles
    #ifdef CreateFile
        #undef CreateFile
    #endif
#elif defined(__EMSCRIPTEN__)
    // Fallback Web : fonctionnalités limitées
    // Retourner des valeurs par défaut ou false
#else
    // Code POSIX pour Linux/macOS
    #include <unistd.h>
    #include <sys/stat.h>
#endif
```

---

## 📦 Installation et Intégration

### Prérequis


| Dépendance      | Version Minimale | Purpose                                          |
| ---------------- | ---------------- | ------------------------------------------------ |
| **NKCore**       | 1.0.0            | Types de base (`nk_int64`, `usize`, etc.)        |
| **NKContainers** | 1.0.0            | `NkString`, `NkVector<T>`                        |
| **NKPlatform**   | 1.0.0            | Détection de plateforme (`NKENTSEU_PLATFORM_*`) |
| **Compilateur**  | C++11 minimum    | Support des enums class, constexpr, etc.         |

### Intégration dans un Projet CMake

```cmake
# CMakeLists.txt de votre projet

# Ajouter le module NKFileSystem
add_subdirectory(Modules/System/NKFileSystem)

# Lier contre votre cible
target_link_libraries(MyApplication PRIVATE
    NKFileSystem
    NKCore
    NKContainers
)

# Définir les macros de plateforme si nécessaire
target_compile_definitions(MyApplication PRIVATE
    NK_CPP11=1
    $<$<PLATFORM_ID:Windows>:NKENTSEU_PLATFORM_WINDOWS>
    $<$<PLATFORM_ID:Linux>:NKENTSEU_PLATFORM_LINUX>
    $<$<PLATFORM_ID:Darwin>:NKENTSEU_PLATFORM_MACOS>
)
```

### Intégration Manuelle (sans CMake)

```makefile
# Makefile ou script de build personnalisé

# Sources à compiler
NKFS_SOURCES := \
    Modules/System/NKFileSystem/src/NKFileSystem/NkPath.cpp \
    Modules/System/NKFileSystem/src/NKFileSystem/NkFile.cpp \
    Modules/System/NKFileSystem/src/NKFileSystem/NkDirectory.cpp \
    Modules/System/NKFileSystem/src/NKFileSystem/NkFileSystem.cpp \
    Modules/System/NKFileSystem/src/NKFileSystem/NkFileWatcher.cpp

# Headers à inclure
NKFS_INCLUDES := -IModules/System/NKFileSystem/src \
                 -IModules/Core/NKCore/src \
                 -IModules/Containers/NKContainers/src

# Flags de compilation
CXXFLAGS += $(NKFS_INCLUDES) -std=c++11 -DNK_CPP11=1

# Platform-specific
ifeq ($(OS),Windows_NT)
    CXXFLAGS += -D_WIN32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -D__linux__
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -D__APPLE__
    endif
endif
```

### Vérification de l'Installation

```cpp
// test_filesystem.cpp — Compilation de test minimale
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKCore/Logger/NkLogger.h"

int main() {
    using namespace nkentseu;
  
    // Test NkPath
    NkPath p = NkPath("test") / "file.txt";
    NK_LOG_INFO("Path: {}", p.ToString().CStr());
  
    // Test NkFile utilitaires
    if (NkFile::Exists("test/file.txt")) {
        NK_LOG_INFO("File exists");
    }
  
    // Test NkDirectory
    if (NkDirectory::CreateRecursive("output/test")) {
        NK_LOG_INFO("Directory created");
    }
  
    return 0;
}
```

---

## 📚 Référence API

### NkPath — Manipulation de Chemins

#### Constructeurs

```cpp
NkPath();                                    // Chemin vide
NkPath(const char* path);                    // Depuis C-string
NkPath(const NkString& path);                // Depuis NkString
NkPath(const NkPath& other);                 // Copie
```

#### Jointure et Modification

```cpp
NkPath& Append(const char* component);       // Ajout in-place
NkPath operator/(const char* component) const; // Jointure immuable

NkPath& ReplaceExtension(const char* newExt); // Changer l'extension
NkPath& ReplaceFileName(const char* newName); // Changer le nom
NkPath& RemoveFileName();                     // Ne garder que le répertoire
```

#### Décomposition

```cpp
NkString GetDirectory() const;               // Répertoire parent
NkString GetFileName() const;                // Nom avec extension
NkString GetFileNameWithoutExtension() const; // Nom sans extension
NkString GetExtension() const;               // Extension avec point
NkString GetRoot() const;                    // Racine (C:/ ou /)
```

#### Propriétés et Conversion

```cpp
bool IsAbsolute() const;                     // Chemin absolu ?
bool IsRelative() const;                     // Chemin relatif ?
bool HasExtension() const;                   // A une extension ?
const char* CStr() const;                    // Accès C-string
NkString ToString() const;                   // Conversion NkString
NkString ToNative() const;                   // Format plateforme native
```

#### Utilitaires Statiques

```cpp
static NkPath GetCurrentDirectory();         // Répertoire de travail
static NkPath GetTempDirectory();            // Répertoire temporaire
static NkPath Combine(const char* p1, const char* p2); // Combinaison
static bool IsValidPath(const char* path);   // Validation syntaxique
```

---

### NkFile — Gestion RAII des Fichiers

#### Modes d'Ouverture (`NkFileMode`)

```cpp
enum class NkFileMode : uint32 {
    // Flags de base
    NK_READ = 1 << 0,           // "r"
    NK_WRITE = 1 << 1,          // "w"
    NK_APPEND = 1 << 2,         // "a"
    NK_BINARY = 1 << 4,         // Mode binaire
  
    // Combinaisons prédéfinies
    NK_READ_BINARY = NK_READ | NK_BINARY,      // "rb"
    NK_WRITE_BINARY = NK_WRITE | NK_BINARY,    // "wb"
    NK_READ_WRITE = NK_READ | NK_WRITE,        // "r+"
    // ... et autres combinaisons
};
```

#### Cycle de Vie

```cpp
NkFile();                                      // Non ouvert
NkFile(const char* path, NkFileMode = NK_READ); // Ouverture immédiate
~NkFile();                                     // Fermeture automatique (RAII)

bool Open(const char* path, NkFileMode = NK_READ);  // Ouverture manuelle
void Close();                                    // Fermeture explicite
bool IsOpen() const;                             // État courant
```

#### Lecture

```cpp
usize Read(void* buffer, usize size);          // Lecture binaire brute
NkString ReadLine();                           // Lecture ligne par ligne
NkString ReadAll();                            // Lecture complète en chaîne
NkVector<NkString> ReadLines();                // Toutes les lignes dans un vecteur
```

#### Écriture

```cpp
usize Write(const void* data, usize size);     // Écriture binaire brute
bool WriteLine(const char* text);              // Écriture ligne avec newline
bool Write(const NkString& text);              // Écriture de chaîne
void Flush();                                  // Force l'écriture sur disque
```

#### Navigation

```cpp
nk_int64 Tell() const;                         // Position courante
bool Seek(nk_int64 offset, NkSeekOrigin = NK_BEGIN); // Déplacement
bool SeekToBegin();                            // Retour au début
bool SeekToEnd();                              // Aller à la fin
nk_int64 GetSize() const;                      // Taille du fichier
```

#### Utilitaires Statiques

```cpp
// Existence et manipulation
static bool Exists(const char* path);
static bool Delete(const char* path);
static bool Copy(const char* src, const char* dst, bool overwrite = false);
static bool Move(const char* src, const char* dst);

// Lecture/écriture atomique
static NkString ReadAllText(const char* path);
static NkVector<nk_uint8> ReadAllBytes(const char* path);
static bool WriteAllText(const char* path, const char* text);
static bool WriteAllBytes(const char* path, const NkVector<nk_uint8>& data);

// Métadonnées
static nk_int64 GetFileSize(const char* path);
```

---

### NkDirectory — Opérations sur Répertoires

#### Création et Suppression

```cpp
static bool Create(const char* path);                  // Création simple
static bool CreateRecursive(const char* path);         // Avec parents manquants
static bool Delete(const char* path, bool recursive = false); // Suppression
static bool Exists(const char* path);                  // Vérification existence
static bool Empty(const char* path);                   // Vérification vide
```

#### Énumération avec Filtrage

```cpp
enum class NkSearchOption {
    NK_TOP_DIRECTORY_ONLY,   // Niveau courant uniquement
    NK_ALL_DIRECTORIES       // Parcours récursif
};

// Pattern glob-style : "*", "*.txt", "file?.log", "backup_*_2024.*"
static NkVector<NkString> GetFiles(
    const char* path,
    const char* pattern = "*",
    NkSearchOption option = NK_TOP_DIRECTORY_ONLY
);

static NkVector<NkString> GetDirectories(
    const char* path,
    const char* pattern = "*",
    NkSearchOption option = NK_TOP_DIRECTORY_ONLY
);

// Avec métadonnées enrichies
struct NkDirectoryEntry {
    NkString Name;              // Nom sans chemin
    NkPath FullPath;            // Chemin complet
    bool IsDirectory;           // Est un répertoire ?
    bool IsFile;                // Est un fichier ?
    nk_int64 Size;              // Taille en octets
    nk_int64 ModificationTime;  // Timestamp de modification
};

static NkVector<NkDirectoryEntry> GetEntries(
    const char* path,
    const char* pattern = "*",
    NkSearchOption option = NK_TOP_DIRECTORY_ONLY
);
```

#### Copie et Déplacement

```cpp
static bool Copy(
    const char* source,
    const char* dest,
    bool recursive = true,    // Copier le contenu récursivement
    bool overwrite = false    // Écraser les fichiers existants
);

static bool Move(const char* source, const char* dest);  // Déplacement/renommage
```

#### Répertoires Spéciaux

```cpp
static NkPath GetCurrentDirectory();   // Répertoire de travail
static bool SetCurrentDirectory(const char* path);  // Changer de cwd
static NkPath GetTempDirectory();      // Répertoire temporaire système
static NkPath GetHomeDirectory();      // Home de l'utilisateur
static NkPath GetAppDataDirectory();   // AppData / .config pour config applicative
```

---

### NkFileWatcher — Surveillance de Fichiers

#### Types d'Événements

```cpp
enum class NkFileChangeType {
    NK_CREATED,              // Fichier/répertoire créé
    NK_DELETED,              // Supprimé
    NK_MODIFIED,             // Contenu modifié
    NK_RENAMED,              // Renommé ou déplacé
    NK_ATTRIBUTE_CHANGED     // Métadonnées modifiées
};

struct NkFileChangeEvent {
    NkFileChangeType Type;   // Type d'événement
    NkString Path;           // Chemin de l'élément concerné
    NkString OldPath;        // Ancien chemin (pour NK_RENAMED)
    nk_int64 Timestamp;      // Horodatage epoch Unix
};
```

#### Interface Callback

```cpp
class NkFileWatcherCallback {
public:
    virtual ~NkFileWatcherCallback() {}
    virtual void OnFileChanged(const NkFileChangeEvent& event) = 0;
};
```

#### Cycle de Vie du Watcher

```cpp
NkFileWatcher();
NkFileWatcher(const char* path, NkFileWatcherCallback* cb, bool recursive = false);
~NkFileWatcher();  // Stop() automatique si watching

bool Start();      // Démarrer la surveillance (thread dédié)
void Stop();       // Arrêter et libérer les ressources
bool IsWatching() const;  // État courant

// Configuration dynamique
void SetPath(const char* path);
void SetCallback(NkFileWatcherCallback* cb);
void SetRecursive(bool recursive);
```

#### Adaptateur C++11 Simplifié

```cpp
#if defined(NK_CPP11)
class NkSimpleFileWatcher : public NkFileWatcherCallback {
public:
    using CallbackFunc = void(*)(const NkFileChangeEvent&);
    CallbackFunc OnChanged;  // Assigner votre fonction/lambda ici
  
    NkSimpleFileWatcher(const char* path, bool recursive = false);
  
    // Délègue à NkFileWatcher interne
    bool Start();
    void Stop();
    bool IsWatching() const;
};
#endif
```

---

### NkFileSystem — Métadonnées et Utilitaires Système

#### Informations sur les Volumes

```cpp
struct NkDriveInfo {
    NkString Name;              // "C:/" ou "/"
    NkString Label;             // Étiquette volumique
    NkFileSystemType Type;      // NK_NTFS, NK_EXT4, etc.
    nk_int64 TotalSize;         // Taille totale en octets
    nk_int64 FreeSpace;         // Espace libre (inclut réservé système)
    nk_int64 AvailableSpace;    // Espace disponible utilisateur
    bool IsReady;               // Lecteur accessible ?
};

static NkVector<NkDriveInfo> GetDrives();        // Énumérer tous les lecteurs
static NkDriveInfo GetDriveInfo(const char* path); // Infos d'un volume spécifique
```

#### Espace Disque

```cpp
static nk_int64 GetTotalSpace(const char* path);      // Taille totale
static nk_int64 GetFreeSpace(const char* path);        // Libre total
static nk_int64 GetAvailableSpace(const char* path);   // Disponible utilisateur
```

#### Attributs de Fichiers

```cpp
struct NkFileAttributes {
    bool IsReadOnly;      // Lecture seule
    bool IsHidden;        // Caché (Windows uniquement)
    bool IsSystem;        // Fichier système (Windows)
    bool IsArchive;       // Marqué pour archivage
    bool IsCompressed;    // Compressé au niveau FS
    bool IsEncrypted;     // Chiffré (EFS)
    nk_int64 CreationTime;    // Timestamp création
    nk_int64 LastAccessTime;  // Dernier accès lecture
    nk_int64 LastWriteTime;   // Dernière modification
};

static NkFileAttributes GetFileAttributes(const char* path);
static bool SetFileAttributes(const char* path, const NkFileAttributes& attrs);

// Accesseurs/modificateurs individuels
static bool SetReadOnly(const char* path, bool readOnly);
static bool SetHidden(const char* path, bool hidden);  // Windows uniquement
```

#### Timestamps

```cpp
// Lecture
static nk_int64 GetCreationTime(const char* path);
static nk_int64 GetLastAccessTime(const char* path);
static nk_int64 GetLastWriteTime(const char* path);

// Écriture
static bool SetCreationTime(const char* path, nk_int64 time);    // Windows uniquement
static bool SetLastAccessTime(const char* path, nk_int64 time);  // Cross-platform
static bool SetLastWriteTime(const char* path, nk_int64 time);   // Cross-platform
```

#### Validation et Conversion de Chemins

```cpp
static bool IsValidFileName(const char* name);  // Caractères interdits + noms réservés Windows
static bool IsValidPath(const char* path);      // Délègue à NkPath::IsValidPath()

static NkString GetAbsolutePath(const char* path);   // Résolution via GetFullPathName/realpath
static NkString GetRelativePath(const char* from, const char* to);  // Basique : retourne 'to'
```

#### Informations sur le Système de Fichiers

```cpp
enum class NkFileSystemType {
    NK_UNKNOW, NK_NTFS, NK_FAT32, NK_EXFAT,
    NK_EXT4, NK_EXT3, NK_HFS, NK_APFS, NK_NETWORK
};

static NkFileSystemType GetFileSystemType(const char* path);  // Détection du type de FS
static bool IsCaseSensitive(const char* path);                 // true sur Unix, false sur Windows
```

#### Liens Symboliques

```cpp
static bool IsSymbolicLink(const char* path);                  // Détection
static NkString GetSymbolicLinkTarget(const char* path);       // Résolution (Unix uniquement)
static bool CreateSymbolicLink(const char* link, const char* target);  // Création
```

---

## 💡 Exemples d'Utilisation

### 🎮 Chargement de Ressources de Jeu

```cpp
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/Logger/NkLogger.h"

class ResourceManager {
public:
    ResourceManager(const nkentseu::NkPath& basePath)
        : mBasePath(basePath) {}
  
    bool LoadTexture(const nkentseu::NkString& name, 
                     nkentseu::NkVector<nkentseu::nk_uint8>& outData) {
        using namespace nkentseu;
      
        // Construction du chemin complet
        NkPath texturePath = mBasePath / "textures" / name;
      
        // Validation de sécurité
        if (!NkPath::IsValidPath(texturePath.CStr())) {
            NK_LOG_ERROR("Chemin de texture invalide : {}", texturePath.ToString().CStr());
            return false;
        }
      
        // Chargement binaire atomique
        outData = NkFile::ReadAllBytes(texturePath);
      
        if (outData.Empty()) {
            NK_LOG_ERROR("Échec de chargement : {}", texturePath.ToString().CStr());
            return false;
        }
      
        NK_LOG_DEBUG("Texture chargée : {} ({} bytes)", 
                    name.CStr(), outData.Size());
        return true;
    }
  
private:
    nkentseu::NkPath mBasePath;
};
```

### 🔥 Hot-Reload de Shaders

```cpp
#include "NKFileSystem/NkFileWatcher.h"
#include "NKCore/Logger/NkLogger.h"

class ShaderHotReload {
public:
    ShaderHotReload(const nkentseu::NkPath& shaderDir) {
        using namespace nkentseu;
      
        mWatcher = new NkSimpleFileWatcher(shaderDir.CStr(), true);
        mWatcher->OnChanged = [this](const NkFileChangeEvent& e) {
            OnFileChanged(e);
        };
      
        if (!mWatcher->Start()) {
            NK_LOG_WARN("Hot-reload désactivé pour : {}", shaderDir.ToString().CStr());
        }
    }
  
    ~ShaderHotReload() {
        if (mWatcher) {
            mWatcher->Stop();
            delete mWatcher;
        }
    }
  
private:
    void OnFileChanged(const nkentseu::NkFileChangeEvent& event) {
        using namespace nkentseu;
      
        // Filtrer par extension
        if (!event.Path.EndsWith(".glsl") && 
            !event.Path.EndsWith(".hlsl") &&
            !event.Path.EndsWith(".shader")) {
            return;
        }
      
        switch (event.Type) {
            case NkFileChangeType::NK_MODIFIED:
                NK_LOG_INFO("Recompilation shader : {}", event.Path.CStr());
                RecompileShader(event.Path.CStr());
                break;
              
            case NkFileChangeType::NK_DELETED:
                NK_LOG_INFO("Shader supprimé : {}", event.Path.CStr());
                UnloadShader(event.Path.CStr());
                break;
              
            case NkFileChangeType::NK_CREATED:
                NK_LOG_INFO("Nouveau shader détecté : {}", event.Path.CStr());
                LoadNewShader(event.Path.CStr());
                break;
              
            default:
                break;
        }
    }
  
    void RecompileShader(const char* path) { /* ... */ }
    void UnloadShader(const char* path) { /* ... */ }
    void LoadNewShader(const char* path) { /* ... */ }
  
    nkentseu::NkSimpleFileWatcher* mWatcher = nullptr;
};
```

### 💾 Gestion de Configuration Utilisateur

```cpp
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/Logger/NkLogger.h"

class AppConfig {
public:
    static nkentseu::NkPath GetConfigPath() {
        using namespace nkentseu;
      
        // Répertoire de configuration applicative
        NkPath configDir = NkDirectory::GetAppDataDirectory() / "MyApplication";
      
        // Création si nécessaire
        if (!NkDirectory::Exists(configDir)) {
            if (!NkDirectory::CreateRecursive(configDir)) {
                NK_LOG_ERROR("Impossible de créer le répertoire de config");
                return NkPath();
            }
        }
      
        return configDir / "config.json";
    }
  
    static bool Save(const nkentseu::NkString& jsonConfig) {
        using namespace nkentseu;
      
        NkPath configPath = GetConfigPath();
        if (configPath.ToString().Empty()) {
            return false;
        }
      
        // Écriture atomique via fichier temporaire
        NkPath tempPath = configPath + ".tmp";
      
        if (!NkFile::WriteAllText(tempPath.CStr(), jsonConfig)) {
            NK_LOG_ERROR("Échec d'écriture de la config temporaire");
            return false;
        }
      
        // Rename atomique pour éviter la corruption
        if (!NkFile::Move(tempPath.CStr(), configPath.CStr())) {
            NK_LOG_ERROR("Échec de commit de la configuration");
            NkFile::Delete(tempPath.CStr());  // Nettoyage
            return false;
        }
      
        NK_LOG_INFO("Configuration sauvegardée : {}", configPath.ToString().CStr());
        return true;
    }
  
    static nkentseu::NkString Load() {
        using namespace nkentseu;
      
        NkPath configPath = GetConfigPath();
        if (configPath.ToString().Empty() || !NkFile::Exists(configPath)) {
            return NkString();  // Config par défaut
        }
      
        return NkFile::ReadAllText(configPath);
    }
};
```

### 🧹 Nettoyage Intelligent de Cache

```cpp
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKCore/Logger/NkLogger.h"

class CacheManager {
public:
    CacheManager(const nkentseu::NkPath& cacheDir)
        : mCacheDir(cacheDir) {}
  
    // Nettoyer les fichiers temporaires plus anciens que maxAgeSeconds
    void Cleanup(nkentseu::nk_int64 maxAgeSeconds) {
        using namespace nkentseu;
      
        if (!NkDirectory::Exists(mCacheDir)) {
            return;  // Rien à nettoyer
        }
      
        nk_int64 now = time(nullptr);
        nk_uint64 cleanedCount = 0;
        nk_int64 freedBytes = 0;
      
        // Énumérer toutes les entrées avec métadonnées
        auto entries = NkDirectory::GetEntries(mCacheDir, "*", 
                                              NkSearchOption::NK_ALL_DIRECTORIES);
      
        for (const auto& entry : entries) {
            if (!entry.IsFile) {
                continue;  // Ignorer les répertoires
            }
          
            // Vérifier l'âge du fichier
            nk_int64 age = now - entry.ModificationTime;
            if (age < maxAgeSeconds) {
                continue;  // Fichier récent, conserver
            }
          
            // Supprimer le fichier ancien
            if (NkFile::Delete(entry.FullPath)) {
                cleanedCount++;
                freedBytes += entry.Size;
                NK_LOG_DEBUG("Cache nettoyé : {} ({} bytes, {} jours)",
                           entry.Name.CStr(),
                           entry.Size,
                           age / 86400);  // Conversion en jours
            }
        }
      
        // Rapport de nettoyage
        if (cleanedCount > 0) {
            NK_LOG_INFO("Nettoyage cache : {} fichiers, {} Mo libérés",
                       cleanedCount,
                       freedBytes / (1024 * 1024));
        }
      
        // Supprimer le répertoire de cache s'il est vide
        if (NkDirectory::Empty(mCacheDir)) {
            NkDirectory::Delete(mCacheDir, false);
            NK_LOG_DEBUG("Répertoire de cache supprimé (vide)");
        }
    }
  
private:
    nkentseu::NkPath mCacheDir;
};
```

---

## ⚠️ Bonnes Pratiques

### 🔐 Sécurité des Chemins

```cpp
// ❌ À ÉVITER : Injection de chemin via entrée utilisateur
void LoadUserFile(const char* userInput) {
    // DANGEREUX : "../../../etc/passwd" pourrait lire des fichiers système
    NkFile::ReadAllText(userInput);
}

// ✅ RECOMMANDÉ : Validation stricte avant utilisation
bool LoadUserFileSafe(const nkentseu::NkString& userInput) {
    using namespace nkentseu;
  
    // 1. Rejeter les chemins absolus si non attendus
    NkPath path(userInput);
    if (path.IsAbsolute()) {
        NK_LOG_ERROR("Chemin absolu rejeté : {}", userInput.CStr());
        return false;
    }
  
    // 2. Valider syntaxiquement
    if (!NkPath::IsValidPath(userInput.CStr())) {
        NK_LOG_ERROR("Chemin invalide : {}", userInput.CStr());
        return false;
    }
  
    // 3. Restreindre à un répertoire de base
    NkPath basePath("user_data");
    NkPath fullPath = basePath / userInput;
  
    // 4. Optionnel : vérifier que le chemin résolu reste dans basePath
    NkString resolved = NkFileSystem::GetAbsolutePath(fullPath.CStr());
    NkString baseResolved = NkFileSystem::GetAbsolutePath(basePath.CStr());
  
    if (!resolved.StartsWith(baseResolved)) {
        NK_LOG_ERROR("Tentative d'échappement de répertoire détectée");
        return false;
    }
  
    // 5. Procéder au chargement
    return NkFile::Exists(fullPath);
}
```

### 🧵 Thread-Safety

```cpp
// ❌ À ÉVITER : Accès concurrent non synchronisé à NkFile
NkFile sharedFile("log.txt", NkFileMode::NK_APPEND);

void Thread1() {
    sharedFile.WriteLine("Message from thread 1");  // Race condition !
}

void Thread2() {
    sharedFile.WriteLine("Message from thread 2");  // Corruption possible !
}

// ✅ RECOMMANDÉ : Synchronisation externe ou file de messages
class ThreadSafeLogger {
public:
    void Log(const nkentseu::NkString& message) {
        // Verrouillage pour accès exclusif au fichier
        std::lock_guard<std::mutex> lock(mMutex);
      
        if (!mFile.IsOpen()) {
            mFile.Open("app.log", nkentseu::NkFileMode::NK_APPEND);
        }
      
        mFile.WriteLine(message.CStr());
        mFile.Flush();  // Optionnel : garantir persistance immédiate
    }
  
private:
    nkentseu::NkFile mFile;
    std::mutex mMutex;  // Ou nkentseu::NkMutex selon votre infrastructure
};

// ✅ ALTERNATIVE : File de messages pour callbacks de NkFileWatcher
class SafeFileWatcher : public nkentseu::NkFileWatcherCallback {
public:
    void OnFileChanged(const nkentseu::NkFileChangeEvent& event) override {
        // Callback appelé depuis thread dédié → ne pas bloquer
        // Ajouter à une file thread-safe pour traitement dans le thread principal
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mPendingEvents.PushBack(event);
        }
        // Retour immédiat : le traitement réel se fait ailleurs
    }
  
    void ProcessPendingEvents() {
        // À appeler depuis le thread principal ou thread dédié au traitement
        std::lock_guard<std::mutex> lock(mQueueMutex);
      
        for (const auto& event : mPendingEvents) {
            HandleEvent(event);  // Traitement thread-safe
        }
        mPendingEvents.Clear();
    }
  
private:
    void HandleEvent(const nkentseu::NkFileChangeEvent& event) {
        // Logique métier de traitement d'événement...
    }
  
    nkentseu::NkVector<nkentseu::NkFileChangeEvent> mPendingEvents;
    std::mutex mQueueMutex;
};
```

### 🗑️ Gestion des Ressources

```cpp
// ❌ À ÉVITER : Fichiers ouverts sans fermeture explicite ou RAII
void ProcessFile() {
    // DANGEREUX : Si une exception survient, le fichier reste ouvert
    FILE* f = fopen("data.bin", "rb");
    // ... traitement ...
    fclose(f);  // Peut ne jamais être atteint
}

// ✅ RECOMMANDÉ : Utiliser RAII de NkFile
void ProcessFileSafe() {
    using namespace nkentseu;
  
    {
        NkFile file("data.bin", NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) {
            NK_LOG_ERROR("Impossible d'ouvrir data.bin");
            return;  // Fermeture automatique via destructeur
        }
      
        auto data = file.ReadAll();
        // ... traitement ...
      
    } // file.Close() appelé automatiquement ici, même en cas d'exception
  
    NK_LOG_INFO("Traitement terminé, fichier correctement fermé");
}

// ✅ Pour les opérations statiques : gestion interne automatique
void QuickRead() {
    using namespace nkentseu;
  
    // NkFile::ReadAllText gère ouverture/fermeture en interne
    NkString content = NkFile::ReadAllText("config.json");
    // Aucune fuite de ressource possible
}
```

### 📊 Performance et Mémoire

```cpp
// ❌ À ÉVITER : Lecture complète de fichiers très volumineux
void LoadHugeFile() {
    using namespace nkentseu;
  
    // DANGEREUX : Peut allouer des gigaoctets en mémoire
    NkString huge = NkFile::ReadAllText("gigantic_log.txt");
    // Risque d'OOM (Out Of Memory)
}

// ✅ RECOMMANDÉ : Lecture streamée ligne par ligne
void ProcessLargeFileSafely() {
    using namespace nkentseu;
  
    NkFile file("gigantic_log.txt", NkFileMode::NK_READ);
    if (!file.IsOpen()) return;
  
    nk_uint64 lineCount = 0;
    while (!file.IsEOF()) {
        NkString line = file.ReadLine();
        if (line.Empty() && file.IsEOF()) break;  // Fin réelle
      
        // Traitement immédiat sans accumulation en mémoire
        ProcessLine(line, lineCount++);
    }
    // Mémoire constante quel que soit la taille du fichier
}

// ✅ Pour les fichiers binaires : lecture par chunks
void ProcessBinaryChunked(const nkentseu::NkPath& path) {
    using namespace nkentseu;
  
    NkFile file(path, NkFileMode::NK_READ_BINARY);
    if (!file.IsOpen()) return;
  
    constexpr usize CHUNK_SIZE = 64 * 1024;  // 64 KB
    nk_uint8 buffer[CHUNK_SIZE];
  
    while (usize bytesRead = file.Read(buffer, CHUNK_SIZE)) {
        ProcessChunk(buffer, bytesRead);  // Traitement immédiat
    }
}
```

---

## ⚠️ Limitations Connues

### Plateformes et Fonctionnalités


| Fonctionnalité                           | Windows                  | Linux                        | macOS                        | Web (EMSCRIPTEN)       | Notes                                                     |
| ----------------------------------------- | ------------------------ | ---------------------------- | ---------------------------- | ---------------------- | --------------------------------------------------------- |
| **NkPath**                                | ✅ Complet               | ✅ Complet                   | ✅ Complet                   | ✅ Complet             | Normalisation '/' interne                                 |
| **NkFile**                                | ✅ Complet               | ✅ Complet                   | ✅ Complet                   | ⚠️ Limité           | Pas d'accès direct au FS navigateur                      |
| **NkDirectory::GetDrives()**              | ✅ 26 lettres A-Z        | ⚠️ Retourne uniquement "/" | ⚠️ Retourne uniquement "/" | ❌ Non supporté       | Unix : utiliser /proc/mounts pour énumération complète |
| **NkFileWatcher**                         | ✅ ReadDirectoryChangesW | ✅ inotify                   | ✅ inotify                   | ❌ Fallback silencieux | Web : polling HTTP/WebSocket requis en alternative        |
| **NkFileSystem::SetHidden()**             | ✅ Fonctionnel           | ❌ Retourne false            | ❌ Retourne false            | ❌ Non supporté       | Attribut "caché" Unix = convention de nommage (.file)    |
| **NkFileSystem::SetCreationTime()**       | ✅ Fonctionnel           | ❌ Retourne false            | ❌ Retourne false            | ❌ Non supporté       | Unix : st_ctime est métadonnée, non modifiable          |
| **NkFileSystem::GetSymbolicLinkTarget()** | ❌ Retourne ""           | ✅ readlink()                | ✅ readlink()                | ❌ Non supporté       | Windows : requiert DeviceIoControl non implémenté       |
| **Liens symboliques Windows**             | ⚠️ Privilèges requis  | ✅ Standard                  | ✅ Standard                  | ❌ Non supporté       | Windows : nécessite admin ou Developer Mode activé      |

### Limitations Techniques

#### 🕐 Timestamps et Précision

```cpp
// Conversion FILETIME Windows → epoch Unix (simplifiée)
// La conversion utilisée préserve l'ordre mais pas la valeur exacte
// Pour une conversion précise : soustraire 116444736000000000LL

nk_int64 windowsToFiletimeEpoch(nk_int64 filetime) {
    // FILETIME : 100-nanosecond intervals depuis 1601-01-01
    // Epoch Unix : secondes depuis 1970-01-01
    // Différence : 11644473600 secondes = 116444736000000000 en 100ns
    return (filetime / 10000000LL) - 11644473600LL;
}
```

#### 📁 Énumération de Répertoires sur Unix

```cpp
// GetDrives() sur Unix retourne uniquement "/" 
// Pour une liste complète des points de montage :

#ifdef __linux__
// Parser /proc/mounts ou utiliser getmntent()
FILE* mounts = setmntent("/proc/mounts", "r");
struct mntent* entry;
while ((entry = getmntent(mounts)) != nullptr) {
    // entry->mnt_dir contient le point de montage
}
endmntent(mounts);
#endif
```

#### 🔗 Liens Symboliques Windows

```cpp
// CreateSymbolicLink sur Windows nécessite :
// • Soit les privilèges SeCreateSymbolicLinkPrivilege (administrateur)
// • Soit le "Developer Mode" activé (Windows 10 1703+)

// Vérification préalable recommandée :
bool CanCreateSymlinksOnWindows() {
#ifdef _WIN32
    // Test simplifié : tentative de création dans %TEMP%
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath)) {
        std::string testLink = std::string(tempPath) + "symlink_test_" + std::to_string(GetCurrentProcessId());
        std::string testTarget = std::string(tempPath) + "target_test";
      
        // Créer un fichier cible factice
        FILE* f = fopen(testTarget.c_str(), "w");
        if (f) fclose(f);
      
        // Tenter de créer le symlink
        BOOL result = CreateSymbolicLinkA(testLink.c_str(), testTarget.c_str(), 0);
      
        // Nettoyage
        DeleteFileA(testLink.c_str());
        DeleteFileA(testTarget.c_str());
      
        return result != 0;
    }
    return false;
#else
    return true;  // Unix : pas de privilèges spéciaux requis
#endif
}
```

#### 🧵 Concurrence et Atomicité

```cpp
// Les opérations NkFile::Copy/Move ne sont pas atomiques sur tous les systèmes
// Pour une mise à jour atomique de fichier de configuration :

bool AtomicWriteConfig(const nkentseu::NkPath& finalPath, const nkentseu::NkString& content) {
    using namespace nkentseu;
  
    // 1. Écrire dans un fichier temporaire dans le MÊME répertoire
    NkPath tempPath = finalPath.GetDirectory() / (finalPath.GetFileName() + ".tmp." + std::to_string(time(nullptr)));
  
    if (!NkFile::WriteAllText(tempPath, content)) {
        return false;  // Échec d'écriture
    }
  
    // 2. Rename atomique (fonctionne si même volume)
    if (!NkFile::Move(tempPath, finalPath)) {
        NkFile::Delete(tempPath);  // Nettoyage en cas d'échec
        return false;
    }
  
    // 3. Optionnel : sync sur disque pour persistance garantie
    // (nécessite API plateforme-specific)
  
    return true;
}
```

---

## 🔧 Dépannage

### Problèmes Courants

#### ❌ "Échec d'ouverture de fichier sur Windows"

```cpp
// Causes possibles :
// 1. Chemin avec séparateurs '\\' non normalisés
// 2. Permissions insuffisantes
// 3. Fichier verrouillé par un autre processus
// 4. Chemin trop long (> MAX_PATH sur Windows ancien)

// Solutions :
// ✓ Utiliser NkPath pour normalisation automatique
NkPath path(userInput);  // Normalise '\\' → '/'

// ✓ Vérifier les permissions avant ouverture
if (!NkFile::Exists(path) || !NkFileSystem::GetFileAttributes(path.CStr()).IsReadOnly) {
    // ... procéder ...
}

// ✓ Pour les chemins longs sur Windows : préfixer avec "\\?\"
#ifdef _WIN32
NkString native = path.ToNative();  // "C:/long/path/..." → "C:\long\path\..."
if (native.Length() > MAX_PATH - 12) {  // Marge pour "\\?\" + null terminator
    native = "\\\\?\\" + native;  // Active le support des chemins longs
}
#endif
```

#### ❌ "NkFileWatcher ne détecte pas les changements"

```cpp
// Vérifications :
// 1. Le chemin existe-t-il et est-il un répertoire ?
if (!NkDirectory::Exists(watchPath)) { /* ... */ }

// 2. Le callback est-il non-null avant Start() ?
if (!callback) { /* ... */ }

// 3. Sur Linux : limite inotify atteinte ?
//    Vérifier fs.inotify.max_user_watches
//    Augmenter si nécessaire : sudo sysctl fs.inotify.max_user_watches=524288

// 4. Sur Windows : privilèges pour ReadDirectoryChangesW ?
//    Généralement OK pour les répertoires utilisateur

// 5. Le fichier est-il modifié de manière détectable ?
//    Certaines opérations (ex: édition avec certains éditeurs) 
//    peuvent créer un nouveau fichier plutôt que modifier l'existant
```

#### ❌ "GetDrives() retourne une liste vide sur Linux"

```cpp
// Comportement attendu : GetDrives() retourne ["/"] sur Unix
// Ce n'est pas un bug, mais une simplification de l'API

// Pour énumérer les points de montage réels :
#ifdef __linux__
#include <mntent.h>

NkVector<nkentseu::NkString> GetMountPoints() {
    using namespace nkentseu;
    NkVector<NkString> mounts;
  
    FILE* fp = setmntent("/proc/mounts", "r");
    if (!fp) return mounts;
  
    struct mntent* entry;
    while ((entry = getmntent(fp)) != nullptr) {
        mounts.PushBack(NkString(entry->mnt_dir));
    }
    endmntent(fp);
    return mounts;
}
#endif
```

#### ❌ "Les timestamps ne correspondent pas entre plateformes"

```cpp
// Rappels :
// • Windows FILETIME : 100ns depuis 1601-01-01 UTC
// • Unix time_t : secondes depuis 1970-01-01 UTC
// • NKFileSystem retourne nk_int64 en format "epoch-like" mais pas toujours exact

// Pour comparaison fiable :
// ✓ Comparer des différences relatives plutôt que des valeurs absolues
nk_int64 age = currentTime - fileModificationTime;  // OK même si conversion imparfaite

// ✓ Pour affichage humain, convertir localement
time_t displayTime = static_cast<time_t>(fileModificationTime);
struct tm* local = localtime(&displayTime);
// ... formater avec strftime ...

// ✓ Pour une conversion exacte Windows→Unix :
nk_int64 FileTimeToUnixEpoch(nk_int64 filetime) {
    // 11644473600 secondes entre 1601 et 1970
    // filetime est en 100-nanosecond intervals
    return (filetime / 10000000LL) - 11644473600LL;
}
```

### Logs de Débogage

```cpp
// Activer les logs détaillés pour le diagnostic
#ifdef NK_DEBUG
    #define FS_LOG_DEBUG(fmt, ...) NK_LOG_DEBUG("[NKFS] " fmt, ##__VA_ARGS__)
#else
    #define FS_LOG_DEBUG(fmt, ...) ((void)0)
#endif

// Exemple d'instrumentation
bool NkFile::Open(const char* path, NkFileMode mode) {
    FS_LOG_DEBUG("Open request: path='{}', mode={}", path, static_cast<int>(mode));
  
    // ... implémentation ...
  
    if (!mIsOpen) {
        NK_LOG_ERROR("Failed to open '{}': {}", path, GetLastPlatformError());
    }
    return mIsOpen;
}
```

---

## 🤝 Contribuer

### Structure du Module

```
Modules/System/NKFileSystem/
├── src/
│   └── NKFileSystem/
│       ├── NkFileSystemApi.h    // Macros d'export
│       ├── NkPath.h / .cpp      // Manipulation de chemins
│       ├── NkFile.h / .cpp      // Gestion de fichiers RAII
│       ├── NkDirectory.h / .cpp // Opérations sur répertoires
│       ├── NkFileSystem.h / .cpp// Métadonnées système
│       └── NkFileWatcher.h / .cpp // Surveillance de fichiers
├── include/                     // Headers publics (si architecture séparée)
├── tests/                       // Tests unitaires
│   ├── TestNkPath.cpp
│   ├── TestNkFile.cpp
│   └── ...
├── CMakeLists.txt              // Configuration de build
└── README.md                   // Ce fichier
```

### Guidelines de Code

```cpp
// 1. Respecter le style existant : sectionnement, commentaires Doxygen
// 2. Une instruction par ligne pour lisibilité et diff Git
// 3. Macros d'export : uniquement sur les classes, pas sur les méthodes
// 4. Guards de plateforme : regrouper les #ifdef, éviter la duplication
// 5. Gestion d'erreurs : retours silencieux avec valeurs par défaut, pas d'exceptions

// Exemple de nouvelle méthode à ajouter :
class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFile {
public:
    /// Obtient l'encodage détecté du fichier texte.
    /// @param path Chemin du fichier à analyser
    /// @return Chaîne décrivant l'encodage ("UTF-8", "ASCII", "Unknown")
    /// @note Analyse les BOM et heuristiques de contenu
    static NkString DetectTextEncoding(const char* path);
};

// Implémentation dans .cpp :
NkString NkFile::DetectTextEncoding(const char* path) {
    if (!path) return "Unknown";
  
    NkFile file(path, NkFileMode::NK_READ_BINARY);
    if (!file.IsOpen()) return "Unknown";
  
    // ... logique de détection ...
  
    return detectedEncoding;
}
```

### Tests Unitaires

```cpp
// tests/TestNkPath.cpp — Exemple de structure
#include "NKFileSystem/NkPath.h"
#include "NKTest/NkTestFramework.h"  // Framework de test interne

NK_TEST_SUITE(NkPathTests) {
  
    NK_TEST(Constructor_Default) {
        nkentseu::NkPath p;
        NK_ASSERT(p.ToString().Empty());
        NK_ASSERT(!p.IsAbsolute());
    }
  
    NK_TEST(Join_Operator) {
        using namespace nkentseu;
        NkPath base("/home/user");
        NkPath full = base / "docs" / "file.txt";
      
        NK_ASSERT(full.ToString() == "/home/user/docs/file.txt");
        NK_ASSERT(base.ToString() == "/home/user");  // Immutable
    }
  
    NK_TEST(Normalization_WindowsSeparators) {
        using namespace nkentseu;
        NkPath p("C:\\Users\\Test\\file.txt");
      
        NK_ASSERT(p.ToString() == "C:/Users/Test/file.txt");  // Normalisé en interne
        NK_ASSERT(p.ToNative() == "C:\\Users\\Test\\file.txt");  // Format natif
    }
  
    // ... autres tests ...
}
```

### Processus de Contribution

1. **Fork** le dépôt principal
2. **Créer une branche** pour votre fonctionnalité : `git checkout -b feature/mon-ajout`
3. **Implémenter** avec tests et documentation
4. **Vérifier** la compilation sur toutes les plateformes cibles
5. **Exécuter** les tests : `ctest --output-on-failure`
6. **Soumettre** une Pull Request avec description détaillée

---

## 📜 Licence

```
Copyright (c) 2024-2026 Rihen. Tous droits réservés.

LICENCE PROPRIÉTAIRE — UTILISATION ET MODIFICATION AUTORISÉES

Ce module NKFileSystem fait partie de l'infrastructure NKEntseu.
Il est fourni "tel quel", sans garantie d'aucune sorte, expresse ou implicite.

AUTORISATIONS :
✓ Utilisation dans les projets NKEntseu et dérivés autorisés
✓ Modification du code source pour adaptation aux besoins du projet
✓ Redistribution binaire dans les produits finaux NKEntseu

RESTRICTIONS :
✗ Redistribution du code source à des tiers non autorisés
✗ Utilisation dans des projets concurrents sans accord écrit
✗ Suppression des mentions de copyright et d'auteur

Pour toute demande de licence étendue ou clarification, 
contacter : licence@nkentseu.internal
```

---

## 📞 Support et Ressources


| Ressource                | Description                                                    |
| ------------------------ | -------------------------------------------------------------- |
| 📚 Documentation interne | `Docs/NKFileSystem/` — Guides détaillés et spécifications  |
| 🐛 Signalement de bugs   | `Issues/NKFileSystem` — Template de rapport de bug disponible |
| 💬 Discussion technique  | Canal`#dev-filesystem` sur l'espace de collaboration interne   |
| 🎓 Tutoriels             | `Examples/NKFileSystem/` — Projets de démonstration complets |
| 🔄 Mises à jour         | Changelog disponible dans`CHANGELOG.md` à la racine du module |

---

> **Dernière mise à jour** : Avril 2026
> **Mainteneur principal** : Rihen
> **Version du module** : 1.0.0
> **Compatibilité minimale** : C++11, Windows 10 / Linux 4.4+ / macOS 10.15+ / Emscripten 3.1+