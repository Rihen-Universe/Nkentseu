// =============================================================================
// NKFileSystem/NkPath.h
// Manipulation cross-platform des chemins de fichiers.
//
// Design :
//  - Supporte la notation Windows (C:\\path\\file.txt) et Unix (/path/file.txt)
//  - Séparateur interne préféré toujours '/' pour cohérence multiplateforme
//  - Aucune dépendance à std::filesystem pour compatibilité embarquée
//  - Toutes les opérations de jointure, décomposition et normalisation fournies
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

#ifndef NKENTSEU_FILESYSTEM_NKPATH_H
#define NKENTSEU_FILESYSTEM_NKPATH_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion de l'API d'export NKFileSystem (remplace l'ancien NkFileSystemExport.h)
    // Inclusion des types de base NKCore et des conteneurs NKEntseu

    #include "NKFileSystem/NkFileSystemApi.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Sequential/NkVector.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATION DE LA CLASSE
    // -------------------------------------------------------------------------
    // La classe NkPath représente un chemin de système de fichiers (fichier ou répertoire).
    // Toutes les opérations sont fournies sans utiliser std::filesystem.

    namespace nkentseu {

        // Classe exportée : les méthodes n'ont PAS besoin de macro d'export
        class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkPath {

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.1 : Membres privés
            // -------------------------------------------------------------
            // Stockage interne du chemin normalisé avec séparateur universel '/'.

            NkString mPath;  ///< Chaîne interne stockant le chemin normalisé

            // Séparateur universel utilisé en interne pour la cohérence multiplateforme
            static constexpr char PREFERRED_SEPARATOR = '/';

            // Séparateur Windows, utile pour la normalisation depuis les chemins entrants
            static constexpr char WINDOWS_SEPARATOR = '\\';

            // -------------------------------------------------------------
            // SOUS-SECTION 2.2 : Méthodes privées
            // -------------------------------------------------------------
            // Helpers internes pour la manipulation du chemin.

            /// Normalise les séparateurs internes vers le séparateur préféré '/'.
            /// @note Modifie mPath in-place : remplace tous '\\' par '/'
            void NormalizeSeparators();

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3 : Constructeurs
            // -------------------------------------------------------------
            // Tous les constructeurs normalisent automatiquement les séparateurs.

            /// Constructeur par défaut : chemin vide.
            NkPath();

            /// Constructeur depuis une C-string.
            /// @param path Chemin en format C-string (peut contenir '\\' ou '/')
            /// @note Les séparateurs Windows sont automatiquement normalisés vers '/'
            NkPath(const char* path);

            /// Constructeur depuis NkString.
            /// @param path Chemin en format NkString
            /// @note Les séparateurs Windows sont automatiquement normalisés vers '/'
            NkPath(const NkString& path);

            /// Constructeur de copie.
            /// @param other Autre instance NkPath à copier
            NkPath(const NkPath& other);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.4 : Opérateurs d'affectation
            // -------------------------------------------------------------
            // Permettent l'assignation depuis différents types de chaînes.

            /// Affectation depuis une autre instance NkPath.
            /// @param other Instance source
            /// @return Référence vers cet objet pour chaînage
            NkPath& operator=(const NkPath& other);

            /// Affectation depuis une C-string.
            /// @param path Chemin en format C-string
            /// @return Référence vers cet objet pour chaînage
            NkPath& operator=(const char* path);

            /// Affectation depuis NkString.
            /// @param path Chemin en format NkString
            /// @return Référence vers cet objet pour chaînage
            NkPath& operator=(const NkString& path);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.5 : Opérations de jointure
            // -------------------------------------------------------------
            // Méthodes pour concaténer des composants de chemin avec gestion automatique des séparateurs.

            /// Ajoute un composant au chemin courant.
            /// @param component Composant à ajouter (fichier ou sous-répertoire)
            /// @return Référence vers cet objet pour chaînage
            /// @note Ajoute automatiquement un séparateur si nécessaire
            NkPath& Append(const char* component);

            /// Ajoute un autre chemin au chemin courant.
            /// @param other Chemin à ajouter
            /// @return Référence vers cet objet pour chaînage
            NkPath& Append(const NkPath& other);

            /// Ajoute une chaîne au chemin courant.
            /// @param other Chaîne à ajouter
            /// @return Référence vers cet objet pour chaînage
            NkPath& Append(const NkString& other);

            /// Opérateur de jointure : crée un nouveau chemin combiné.
            /// @param component Composant à ajouter
            /// @return Nouveau NkPath résultant de la combinaison
            /// @note N'altère pas l'instance courante
            NkPath operator/(const NkString& component) const;

            /// Opérateur de jointure : crée un nouveau chemin combiné.
            /// @param component Composant C-string à ajouter
            /// @return Nouveau NkPath résultant de la combinaison
            NkPath operator/(const char* component) const;

            /// Opérateur de jointure : crée un nouveau chemin combiné.
            /// @param other Autre NkPath à ajouter
            /// @return Nouveau NkPath résultant de la combinaison
            NkPath operator/(const NkPath& other) const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.6 : Décomposition du chemin
            // -------------------------------------------------------------
            // Méthodes pour extraire des composants spécifiques d'un chemin.

            /// Obtient le répertoire parent du chemin.
            /// @return NkString contenant le chemin du répertoire, ou chaîne vide si aucun parent
            /// @note Retourne "" pour un chemin sans séparateur (fichier seul)
            NkString GetDirectory() const;

            /// Obtient le nom du fichier avec extension.
            /// @return NkString contenant le nom du fichier, ou le chemin complet si pas de séparateur
            NkString GetFileName() const;

            /// Obtient le nom du fichier sans son extension.
            /// @return NkString contenant le nom sans extension
            /// @note Retourne le nom complet si aucun point trouvé
            NkString GetFileNameWithoutExtension() const;

            /// Obtient l'extension du fichier (incluant le point).
            /// @return NkString contenant l'extension (ex: ".txt"), ou chaîne vide si aucune
            NkString GetExtension() const;

            /// Obtient la racine du chemin (ex: "C:/" ou "/").
            /// @return NkString contenant la racine, ou chaîne vide pour chemin relatif
            NkString GetRoot() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.7 : Propriétés du chemin
            // -------------------------------------------------------------
            // Méthodes de prédicat pour inspecter les caractéristiques du chemin.

            /// Vérifie si le chemin est absolu.
            /// @return true si le chemin commence par une racine (C:/ ou /), false sinon
            bool IsAbsolute() const;

            /// Vérifie si le chemin est relatif.
            /// @return true si !IsAbsolute(), false sinon
            bool IsRelative() const;

            /// Vérifie si le chemin a une extension.
            /// @return true si GetExtension() n'est pas vide, false sinon
            bool HasExtension() const;

            /// Vérifie si le chemin a un nom de fichier.
            /// @return true si GetFileName() n'est pas vide, false sinon
            bool HasFileName() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.8 : Modification du chemin
            // -------------------------------------------------------------
            // Méthodes mutables pour transformer le chemin courant.

            /// Remplace l'extension du fichier.
            /// @param newExt Nouvelle extension (avec ou sans point initial)
            /// @return Référence vers cet objet pour chaînage
            /// @note Si newExt ne commence pas par '.', un point est ajouté automatiquement
            NkPath& ReplaceExtension(const char* newExt);

            /// Remplace le nom du fichier (garde le répertoire et change l'extension si incluse).
            /// @param newName Nouveau nom de fichier
            /// @return Référence vers cet objet pour chaînage
            NkPath& ReplaceFileName(const char* newName);

            /// Remplace l'extension du fichier (version NkString).
            /// @param newExt Nouvelle extension
            /// @return Référence vers cet objet pour chaînage
            NkPath& ReplaceExtension(const NkString& newExt);

            /// Remplace le nom du fichier (version NkString).
            /// @param newName Nouveau nom de fichier
            /// @return Référence vers cet objet pour chaînage
            NkPath& ReplaceFileName(const NkString& newName);

            /// Supprime le nom du fichier, ne garde que le répertoire.
            /// @return Référence vers cet objet pour chaînage
            NkPath& RemoveFileName();

            /// Obtient le chemin parent sans modifier l'instance courante.
            /// @return Nouveau NkPath contenant le répertoire parent
            NkPath GetParent() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.9 : Conversion et accès
            // -------------------------------------------------------------
            // Méthodes pour obtenir des représentations alternatives du chemin.

            /// Obtient le chemin en tant que C-string.
            /// @return Pointeur vers la chaîne interne (valide tant que l'objet existe)
            /// @note Retourne le chemin avec séparateurs normalisés '/'
            const char* CStr() const;

            /// Convertit le chemin en NkString.
            /// @return Copie du chemin interne en tant que NkString
            NkString ToString() const;

            /// Convertit le chemin vers le format natif de la plateforme.
            /// @return NkString avec séparateurs de la plateforme cible
            /// @note Sur Windows : '/' → '\\', sur Unix : inchangé
            NkString ToNative() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.10 : Comparaison
            // -------------------------------------------------------------
            // Opérateurs pour comparer deux chemins.

            /// Égalité : comparaison chaîne par chaîne.
            /// @param other Autre chemin à comparer
            /// @return true si les chemins internes sont identiques, false sinon
            bool operator==(const NkPath& other) const;

            /// Inégalité : inverse de operator==.
            /// @param other Autre chemin à comparer
            /// @return true si les chemins diffèrent, false sinon
            bool operator!=(const NkPath& other) const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.11 : Méthodes statiques utilitaires
            // -------------------------------------------------------------
            // Fonctions utilitaires ne nécessitant pas d'instance.

            /// Obtient le répertoire de travail courant du processus.
            /// @return NkPath contenant le chemin absolu du cwd
            /// @note Utilise getcwd/_getcwd selon la plateforme
            static NkPath GetCurrentDirectory();

            /// Obtient le répertoire temporaire du système.
            /// @return NkPath contenant le chemin du répertoire temp
            /// @note Windows : GetTempPathA ou fallback "C:/Temp"
            /// @note Unix : variable $TMPDIR ou fallback "/tmp"
            static NkPath GetTempDirectory();

            /// Combine deux chemins en un seul.
            /// @param path1 Premier chemin (peut être null ou vide)
            /// @param path2 Second chemin à ajouter (peut être null ou vide)
            /// @return Nouveau NkPath résultant de la combinaison
            static NkPath Combine(const char* path1, const char* path2);

            /// Valide qu'un chemin ne contient pas de caractères interdits.
            /// @param path Chemin à valider en format C-string
            /// @return true si le chemin est valide, false sinon
            /// @note Rejette : < > " | ? * et caractères de contrôle (< 32)
            static bool IsValidPath(const char* path);

            /// Combine deux chemins en un seul (version NkString).
            /// @param path1 Premier chemin
            /// @param path2 Second chemin à ajouter
            /// @return Nouveau NkPath résultant de la combinaison
            static NkPath Combine(const NkString& path1, const NkString& path2);

            /// Valide qu'un chemin ne contient pas de caractères interdits (version NkString).
            /// @param path Chemin à valider en format NkString
            /// @return true si le chemin est valide, false sinon
            static bool IsValidPath(const NkString& path);

        }; // class NkPath

        // -------------------------------------------------------------------------
        // SECTION 3 : ALIAS DE COMPATIBILITÉ LEGACY
        // -------------------------------------------------------------------------
        // Fournit un alias pour compatibilité avec l'ancien namespace nkentseu::entseu.

        namespace entseu {
            // Alias vers le type principal pour compatibilité ascendante
            using NkPath = ::nkentseu::NkPath;
        } // namespace entseu

    } // namespace nkentseu

#endif // NKENTSEU_FILESYSTEM_NKPATH_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKPATH.H
// =============================================================================
// La classe NkPath permet de manipuler des chemins de fichiers de manière
// cross-platform, avec normalisation automatique des séparateurs et opérations
// de jointure, décomposition et validation.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Construction et jointure de chemins
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"

    void PathConstructionExample() {
        using namespace nkentseu;

        // Construction depuis différents types
        NkPath empty;                           // Chemin vide
        NkPath fromCStr("/home/user/projects"); // Depuis C-string
        NkPath fromString(NkString("/var/log")); // Depuis NkString
        NkPath copy = fromCStr;                 // Copie

        // Jointure avec l'opérateur / (style POSIX)
        NkPath base("/home/user");
        NkPath full = base / "projects" / "myapp" / "config.ini";
        // full.ToString() == "/home/user/projects/myapp/config.ini"

        // Jointure avec Append (modification in-place)
        NkPath mutablePath("/tmp");
        mutablePath.Append("cache").Append("data.bin");
        // mutablePath.ToString() == "/tmp/cache/data.bin"

        // Gestion automatique des séparateurs Windows
        NkPath windowsStyle("C:\\Users\\Rihen\\Documents");
        // windowsStyle.ToString() == "C:/Users/Rihen/Documents" (normalisé)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Décomposition d'un chemin
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"
    #include "NKCore/Logger/NkLogger.h"

    void PathDecompositionExample() {
        using namespace nkentseu;

        NkPath p("C:/Users/Rihen/Documents/rapport.pdf");

        // Extraction des composants
        NkString dir = p.GetDirectory();                // "C:/Users/Rihen/Documents"
        NkString file = p.GetFileName();                // "rapport.pdf"
        NkString name = p.GetFileNameWithoutExtension(); // "rapport"
        NkString ext = p.GetExtension();                // ".pdf"
        NkString root = p.GetRoot();                    // "C:/"

        // Logging des résultats
        NK_LOG_INFO("Directory: {}", dir.CStr());
        NK_LOG_INFO("Filename: {}", file.CStr());
        NK_LOG_INFO("Name only: {}", name.CStr());
        NK_LOG_INFO("Extension: {}", ext.CStr());
        NK_LOG_INFO("Root: {}", root.CStr());

        // Chemins Unix : même API, même comportement
        NkPath unixPath("/var/log/nginx/access.log");
        NK_LOG_INFO("Unix dir: {}", unixPath.GetDirectory().CStr());  // "/var/log/nginx"
        NK_LOG_INFO("Unix root: {}", unixPath.GetRoot().CStr());      // "/"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Propriétés et validation
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"
    #include "NKCore/Assert/NkAssert.h"

    void PathPropertiesExample() {
        using namespace nkentseu;

        // Chemins absolus vs relatifs
        NkPath abs1("/etc/hosts");
        NkPath abs2("C:/Windows/System32");
        NkPath rel1("config/settings.json");
        NkPath rel2("../parent/file.txt");

        NKENTSEU_ASSERT(abs1.IsAbsolute() == true);
        NKENTSEU_ASSERT(abs2.IsAbsolute() == true);
        NKENTSEU_ASSERT(rel1.IsRelative() == true);
        NKENTSEU_ASSERT(rel2.IsRelative() == true);

        // Détection d'extension
        NkPath withExt("archive.tar.gz");
        NkPath noExt("README");
        NkPath dotfile(".gitignore");

        NKENTSEU_ASSERT(withExt.HasExtension() == true);   // ".gz"
        NKENTSEU_ASSERT(noExt.HasExtension() == false);
        NKENTSEU_ASSERT(dotfile.HasExtension() == true);   // ".gitignore" est considéré comme extension

        // Validation de chemins (rejet des caractères interdits)
        NKENTSEU_ASSERT(NkPath::IsValidPath("/valid/path.txt") == true);
        NKENTSEU_ASSERT(NkPath::IsValidPath("C:/normal/path") == true);
        NKENTSEU_ASSERT(NkPath::IsValidPath("bad<name>.txt") == false);   // '<' interdit
        NKENTSEU_ASSERT(NkPath::IsValidPath("file?.log") == false);        // '?' interdit
        NKENTSEU_ASSERT(NkPath::IsValidPath("") == false);                 // Vide invalide
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Modification et transformation de chemins
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"

    void PathModificationExample() {
        using namespace nkentseu;

        // Remplacement d'extension
        NkPath src("project/src/main.cpp");
        src.ReplaceExtension(".o");
        // src.ToString() == "project/src/main.o"

        // Remplacement avec point optionnel
        NkPath dst("output/result");
        dst.ReplaceExtension("txt");   // Sans point initial
        // dst.ToString() == "output/result.txt"

        // Remplacement du nom de fichier
        NkPath log("/var/log/app/old.log");
        log.ReplaceFileName("new.log");
        // log.ToString() == "/var/log/app/new.log"

        // Suppression du nom de fichier (ne garde que le répertoire)
        NkPath file("/home/user/docs/report.pdf");
        file.RemoveFileName();
        // file.ToString() == "/home/user/docs"

        // Obtention du parent sans modifier l'original
        NkPath original("/a/b/c/d.txt");
        NkPath parent = original.GetParent();
        // parent.ToString() == "/a/b/c"
        // original.ToString() == "/a/b/c/d.txt" (inchangé)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Conversions et accès aux représentations
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"
    #include <iostream>

    void PathConversionExample() {
        using namespace nkentseu;

        NkPath path("/home/user/projects/myapp/config.ini");

        // Accès en lecture seule via CStr()
        const char* cstr = path.CStr();
        std::printf("C-str: %s\n", cstr);

        // Conversion vers NkString pour manipulation
        NkString str = path.ToString();
        if (str.Contains("config")) {
            // ... traitement ...
        }

        // Conversion vers format natif de la plateforme
        NkString native = path.ToNative();
    #ifdef _WIN32
        // Sur Windows : "/home/..." → "\\home\\..."
        // Note : les chemins Unix restent inchangés sur Unix
    #endif
        std::printf("Native: %s\n", native.CStr());

        // Comparaison de chemins
        NkPath p1("/same/path");
        NkPath p2("/same/path");
        NkPath p3("/different/path");

        if (p1 == p2) {
            // Égaux : comparaison chaîne par chaîne
        }
        if (p1 != p3) {
            // Différents
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Méthodes statiques utilitaires
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"
    #include "NKCore/Logger/NkLogger.h"

    void StaticUtilsExample() {
        using namespace nkentseu;

        // Répertoire de travail courant
        NkPath cwd = NkPath::GetCurrentDirectory();
        NK_LOG_INFO("Current directory: {}", cwd.ToString().CStr());

        // Répertoire temporaire
        NkPath tmp = NkPath::GetTempDirectory();
        NK_LOG_INFO("Temp directory: {}", tmp.ToString().CStr());

        // Combinaison de chemins
        NkPath combined1 = NkPath::Combine("/var/log", "app.log");
        // combined1.ToString() == "/var/log/app.log"

        NkPath combined2 = NkPath::Combine(
            NkString("/home/user"),
            NkString("projects/myapp")
        );
        // combined2.ToString() == "/home/user/projects/myapp"

        // Validation avant utilisation
        const char* userInput = "../../../etc/passwd";
        if (NkPath::IsValidPath(userInput)) {
            NkPath safe(userInput);
            // Utilisation sûre...
        } else {
            NK_LOG_ERROR("Invalid path provided: {}", userInput);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Pattern de construction de chemins pour I/O
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"
    #include "NKCore/File/NkFile.h"  // Hypothétique

    namespace myapp {

        class AssetLoader {
        public:
            AssetLoader(const nkentseu::NkPath& basePath)
                : mBasePath(basePath)
            {
            }

            nkentseu::NkPath LoadTexture(const nkentseu::NkString& name) {
                using namespace nkentseu;

                // Construction du chemin complet
                NkPath texturePath = mBasePath / "textures" / name;

                // Validation optionnelle
                if (!NkPath::IsValidPath(texturePath.CStr())) {
                    NK_LOG_ERROR("Invalid texture path: {}", texturePath.ToString().CStr());
                    return NkPath();
                }

                // Chargement du fichier (hypothétique)
                // return NkFile::Load(texturePath);
                return texturePath;
            }

            void SetOutputDirectory(const nkentseu::NkPath& dir) {
                mOutputDir = dir;
            }

            nkentseu::NkPath GetOutputPath(const nkentseu::NkString& filename) {
                return mOutputDir / filename;
            }

        private:
            nkentseu::NkPath mBasePath;
            nkentseu::NkPath mOutputDir;
        };

    } // namespace myapp
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Gestion cross-platform des chemins
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkPath.h"
    #include "NKPlatform/NkPlatformDetect.h"

    void CrossPlatformPathExample() {
        using namespace nkentseu;

        // NkPath normalise automatiquement les séparateurs en interne
        // Vous pouvez utiliser '/' partout dans votre code, même sur Windows

        NkPath config("/app/config/settings.json");  // Fonctionne sur Windows et Unix

        // Pour interop avec les APIs système qui attendent le format natif :
    #ifdef NKENTSEU_PLATFORM_WINDOWS
        // Convertir vers format Windows avant appel système
        NkString nativePath = config.ToNative();  // "/app/..." → "\\app\\..."
        ::CreateFileA(nativePath.CStr(), ...);
    #else
        // Sur Unix, ToString() suffit (déjà au format natif)
        ::open(config.ToString().CStr(), ...);
    #endif

        // Pattern recommandé : stocker en interne avec '/', convertir à la frontière
        class PlatformFile {
        public:
            PlatformFile(const NkPath& path) : mPath(path) {}

            bool Open() {
                NkString sysPath =
                #ifdef NKENTSEU_PLATFORM_WINDOWS
                    mPath.ToNative();
                #else
                    mPath.ToString();
                #endif
                // Appel système avec sysPath...
                return true;
            }

        private:
            NkPath mPath;  // Toujours stocké avec séparateurs '/'
        };
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================