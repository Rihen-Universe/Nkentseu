// =============================================================================
// NKFileSystem/NkFile.h
// Opérations de lecture/écriture de fichiers avec RAII.
//
// Design :
//  - Le fichier est automatiquement fermé à la destruction de l'objet (RAII)
//  - Aucune dépendance STL (fstream/string) dans l'API publique
//  - Gestion des modes d'ouverture via flags bitmask (NkFileMode)
//  - Support binaire et texte avec conversion automatique des newlines
//  - Utilitaires statiques pour les opérations courantes (Copy, Delete, etc.)
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

#ifndef NKENTSEU_FILESYSTEM_NKFILE_H
#define NKENTSEU_FILESYSTEM_NKFILE_H

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
        // SOUS-SECTION 2.1 : Enum NkFileMode — Flags d'ouverture de fichier
        // =====================================================================
        // Combinaison de flags pour spécifier le mode d'accès et le comportement
        // lors de l'ouverture d'un fichier. Compatible avec les modes fopen C.

        enum class NkFileMode : uint32 {
            NK_NONE = 0,                          ///< Aucun mode (valeur par défaut)

            // -------------------------------------------------------------
            // Flags d'accès
            // -------------------------------------------------------------
            NK_READ = 1 << 0,                     ///< Mode lecture seule : "r"
            NK_WRITE = 1 << 1,                    ///< Mode écriture seule : "w"

            // -------------------------------------------------------------
            // Flags de comportement
            // -------------------------------------------------------------
            NK_APPEND = 1 << 2,                   ///< Mode ajout en fin de fichier : "a"
            NK_TRUNCATE = 1 << 3,                 ///< Tronquer le fichier à l'ouverture

            // -------------------------------------------------------------
            // Flags de format
            // -------------------------------------------------------------
            NK_BINARY = 1 << 4,                   ///< Mode binaire (pas de conversion newline) : "b"

            // -------------------------------------------------------------
            // Combinaisons prédéfinies — Lecture seule
            // -------------------------------------------------------------
            NK_READ_BINARY = NK_READ | NK_BINARY, ///< Lecture binaire : "rb"

            // -------------------------------------------------------------
            // Combinaisons prédéfinies — Écriture (overwrite)
            // -------------------------------------------------------------
            NK_WRITE_BINARY = NK_WRITE | NK_BINARY, ///< Écriture binaire (truncate) : "wb"

            // -------------------------------------------------------------
            // Combinaisons prédéfinies — Ajout (append)
            // -------------------------------------------------------------
            NK_APPEND_BINARY = NK_APPEND | NK_BINARY, ///< Ajout binaire : "ab"

            // -------------------------------------------------------------
            // Combinaisons prédéfinies — Lecture + Écriture
            // -------------------------------------------------------------
            NK_READ_WRITE = NK_READ | NK_WRITE,     ///< Lecture/écriture sans truncate : "r+"
            NK_READ_WRITE_BINARY = NK_READ_WRITE | NK_BINARY, ///< Lecture/écriture binaire : "rb+"

            // -------------------------------------------------------------
            // Combinaisons prédéfinies — Écriture + Lecture (truncate)
            // -------------------------------------------------------------
            NK_WRITE_READ = NK_WRITE | NK_READ,     ///< Écriture/lecture avec truncate : "w+"
            NK_WRITE_READ_BINARY = NK_WRITE_READ | NK_BINARY, ///< Écriture/lecture binaire avec truncate : "wb+"

            // -------------------------------------------------------------
            // Combinaisons prédéfinies — Lecture + Ajout
            // -------------------------------------------------------------
            NK_READ_APPEND = NK_READ | NK_APPEND,   ///< Lecture avec ajout en fin : "a+"
            NK_READ_APPEND_BINARY = NK_READ | NK_APPEND | NK_BINARY ///< Lecture/ajout binaire : "ab+"
        };

        // Alias pour compatibilité avec les opérations bitwise
        using NkFileModeFlags = NkFileMode;

        // Opérateur OR pour combinaison de flags
        inline NkFileModeFlags operator|(NkFileModeFlags a, NkFileModeFlags b) {
            return static_cast<NkFileModeFlags>(
                static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
            );
        }

        // Opérateur AND pour test de flags
        inline NkFileModeFlags operator&(NkFileModeFlags a, NkFileModeFlags b) {
            return static_cast<NkFileModeFlags>(
                static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
            );
        }

        /// Vérifie si un flag est présent dans une valeur de mode.
        /// @param value Valeur de mode à tester
        /// @param flag Flag à rechercher
        /// @return true si le flag est présent, false sinon
        inline bool NkHasFlag(NkFileModeFlags value, NkFileModeFlags flag) {
            return (value & flag) != NkFileModeFlags::NK_NONE;
        }

        // =====================================================================
        // SOUS-SECTION 2.2 : Enum NkSeekOrigin — Origines de recherche
        // =====================================================================
        // Spécifie le point de référence pour les opérations de Seek().

        enum class NkSeekOrigin {
            NK_BEGIN,    ///< Début du fichier (équivalent à SEEK_SET)
            NK_CURRENT,  ///< Position courante (équivalent à SEEK_CUR)
            NK_END       ///< Fin du fichier (équivalent à SEEK_END)
        };

        // =====================================================================
        // SOUS-SECTION 2.3 : Classe NkFile — Gestion RAII des fichiers
        // =====================================================================
        // Encapsule un descripteur fichier (FILE*) derrière un pointeur void* opaque.
        // Politique RAII : Close() est automatiquement appelé par le destructeur.

        class NKENTSEU_FILESYSTEM_CLASS_EXPORT NkFile {

        private:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.1 : Membres privés
            // -------------------------------------------------------------
            // État interne du fichier : handle opaque, chemin, mode, statut.

            void* mHandle;          ///< Descripteur opaque (en pratique FILE*)
            NkPath mPath;           ///< Chemin associé au fichier ouvert
            NkFileMode mMode;       ///< Mode d'ouverture actuel
            bool mIsOpen;           ///< État d'ouverture : true si fichier ouvert

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.2 : Méthodes privées
            // -------------------------------------------------------------
            // Helpers internes pour la gestion du fichier.

            /// Retourne la chaîne de mode C correspondant à mMode.
            /// @return Chaîne compatible fopen : "rb", "w+", "ab", etc.
            /// @note Retourne "" pour les combinaisons invalides
            const char* GetModeString() const;

        public:

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.3 : Constructeurs / Destructeur
            // -------------------------------------------------------------
            // Gestion du cycle de vie avec politique RAII.

            /// Constructeur par défaut : fichier non ouvert.
            NkFile();

            /// Constructeur avec ouverture immédiate depuis C-string.
            /// @param path Chemin du fichier en format C-string
            /// @param mode Mode d'ouverture (défaut : NK_READ)
            /// @note Appelle Open() en interne : vérifiez IsOpen() après construction
            explicit NkFile(const char* path, NkFileMode mode = NkFileMode::NK_READ);

            /// Constructeur avec ouverture immédiate depuis NkPath.
            /// @param path Chemin du fichier en format NkPath
            /// @param mode Mode d'ouverture (défaut : NK_READ)
            /// @note Appelle Open() en interne : vérifiez IsOpen() après construction
            explicit NkFile(const NkPath& path, NkFileMode mode = NkFileMode::NK_READ);

            /// Destructeur : ferme automatiquement le fichier si ouvert (RAII).
            ~NkFile();

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.4 : Copie / Déplacement
            // -------------------------------------------------------------
            // Sémantique de mouvement uniquement : pas de copie pour éviter
            // les doubles fermetures de descripteurs système.

            // Copie désactivée : un fichier ne peut pas être copié (resource unique)
            NkFile(const NkFile&) = delete;
            NkFile& operator=(const NkFile&) = delete;

            /// Constructeur de déplacement : transfère la propriété du descripteur.
            /// @param other Instance source à déplacer
            /// @note L'instance source est laissée dans un état valide mais vide
            NkFile(NkFile&& other) noexcept;

            /// Opérateur d'affectation par déplacement.
            /// @param other Instance source à déplacer
            /// @return Référence vers cet objet pour chaînage
            NkFile& operator=(NkFile&& other) noexcept;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.5 : Ouverture / Fermeture
            // -------------------------------------------------------------
            // Méthodes pour gérer le cycle de vie du descripteur fichier.

            /// Ouvre un fichier depuis une C-string.
            /// @param path Chemin du fichier en format C-string
            /// @param mode Mode d'ouverture (défaut : NK_READ)
            /// @return true si l'ouverture a réussi, false sinon
            /// @note Ferme automatiquement le fichier précédent si déjà ouvert
            bool Open(const char* path, NkFileMode mode = NkFileMode::NK_READ);

            /// Ouvre un fichier depuis un NkPath.
            /// @param path Chemin du fichier en format NkPath
            /// @param mode Mode d'ouverture (défaut : NK_READ)
            /// @return true si l'ouverture a réussi, false sinon
            bool Open(const NkPath& path, NkFileMode mode = NkFileMode::NK_READ);

            /// Ferme le fichier et libère le descripteur système.
            /// @note Idempotent : sans effet si le fichier est déjà fermé
            void Close();

            /// Vérifie si le fichier est actuellement ouvert.
            /// @return true si ouvert, false sinon
            bool IsOpen() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.6 : Lecture
            // -------------------------------------------------------------
            // Méthodes pour lire des données depuis le fichier.

            /// Lit un bloc de données brutes depuis le fichier.
            /// @param buffer Pointeur vers le buffer de destination
            /// @param size Nombre d'octets à lire
            /// @return Nombre d'octets effectivement lus (peut être < size à EOF)
            /// @note Retourne 0 en cas d'erreur ou si le fichier n'est pas ouvert
            usize Read(void* buffer, usize size);

            /// Lit une ligne de texte jusqu'au newline.
            /// @return NkString contenant la ligne lue (sans le newline)
            /// @note Retourne une chaîne vide à EOF ou en cas d'erreur
            /// @note Gère automatiquement les newlines Windows (\r\n) et Unix (\n)
            NkString ReadLine();

            /// Lit l'intégralité du fichier en une seule chaîne.
            /// @return NkString contenant tout le contenu du fichier
            /// @note Alloue un buffer de taille GetSize() : attention aux gros fichiers
            NkString ReadAll();

            /// Lit le fichier ligne par ligne dans un vecteur.
            /// @return NkVector<NkString> contenant toutes les lignes
            /// @note Les newlines sont supprimés de chaque ligne retournée
            NkVector<NkString> ReadLines();

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.7 : Écriture
            // -------------------------------------------------------------
            // Méthodes pour écrire des données dans le fichier.

            /// Écrit un bloc de données brutes dans le fichier.
            /// @param data Pointeur vers les données à écrire
            /// @param size Nombre d'octets à écrire
            /// @return Nombre d'octets effectivement écrits
            /// @note Retourne 0 en cas d'erreur ou si le fichier n'est pas ouvert
            usize Write(const void* data, usize size);

            /// Écrit une ligne de texte avec newline automatique.
            /// @param text Texte à écrire (sans newline)
            /// @return true si l'écriture a réussi, false sinon
            /// @note Ajoute automatiquement '\n' à la fin de la ligne
            bool WriteLine(const char* text);

            /// Écrit une chaîne NkString dans le fichier.
            /// @param text Chaîne à écrire
            /// @return true si toute la chaîne a été écrite, false sinon
            bool Write(const NkString& text);

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.8 : Position du curseur
            // -------------------------------------------------------------
            // Méthodes pour naviguer dans le fichier.

            /// Obtient la position courante du curseur de lecture/écriture.
            /// @return Position en octets depuis le début, ou -1 en cas d'erreur
            nk_int64 Tell() const;

            /// Déplace le curseur de lecture/écriture.
            /// @param offset Décalage en octets (signé)
            /// @param origin Point de référence (défaut : NK_BEGIN)
            /// @return true si le déplacement a réussi, false sinon
            bool Seek(nk_int64 offset, NkSeekOrigin origin = NkSeekOrigin::NK_BEGIN);

            /// Déplace le curseur au début du fichier.
            /// @return true si réussi, false sinon
            bool SeekToBegin();

            /// Déplace le curseur à la fin du fichier.
            /// @return true si réussi, false sinon
            bool SeekToEnd();

            /// Obtient la taille totale du fichier en octets.
            /// @return Taille en octets, ou -1 en cas d'erreur
            /// @note Effectue un Seek temporaire : peut modifier la position courante
            nk_int64 GetSize() const;

            /// Alias vers GetSize() pour concision.
            /// @return Taille du fichier en octets
            nk_int64 Size() const {
                return GetSize();
            }

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.9 : Buffers et synchronisation
            // -------------------------------------------------------------

            /// Force l'écriture des buffers en attente vers le disque.
            /// @note Utile avant un crash ou pour garantir la persistance immédiate
            void Flush();

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.10 : Propriétés
            // -------------------------------------------------------------
            // Accesseurs en lecture seule pour l'état du fichier.

            /// Obtient le chemin associé au fichier.
            /// @return Référence constante vers le NkPath interne
            const NkPath& GetPath() const;

            /// Obtient le mode d'ouverture actuel.
            /// @return Valeur NkFileMode configurée à l'ouverture
            NkFileMode GetMode() const;

            /// Vérifie si la fin du fichier a été atteinte.
            /// @return true si EOF, false sinon
            /// @note Peut retourner false même si Read() retournerait 0 (buffering)
            bool IsEOF() const;

            // -------------------------------------------------------------
            // SOUS-SECTION 2.3.11 : Utilitaires statiques
            // -------------------------------------------------------------
            // Méthodes de commodité ne nécessitant pas d'instance NkFile.

            /// Vérifie si un fichier existe sur le système.
            /// @param path Chemin à tester en format C-string
            /// @return true si le fichier existe et est régulier, false sinon
            /// @note Ne vérifie pas les permissions d'accès en lecture/écriture
            static bool Exists(const char* path);

            /// Vérifie si un fichier existe sur le système (version NkPath).
            /// @param path Chemin à tester en format NkPath
            /// @return true si le fichier existe et est régulier, false sinon
            static bool Exists(const NkPath& path);

            /// Supprime un fichier du système.
            /// @param path Chemin du fichier à supprimer en format C-string
            /// @return true si la suppression a réussi, false sinon
            /// @note Échoue si le fichier est ouvert ou en cours d'utilisation
            static bool Delete(const char* path);

            /// Supprime un fichier du système (version NkPath).
            /// @param path Chemin du fichier à supprimer en format NkPath
            /// @return true si la suppression a réussi, false sinon
            static bool Delete(const NkPath& path);

            /// Copie un fichier vers une destination.
            /// @param source Chemin du fichier source en format C-string
            /// @param dest Chemin de destination en format C-string
            /// @param overwrite Si true, écrase la destination si elle existe
            /// @return true si la copie a réussi, false sinon
            /// @note Utilise un buffer interne de 8KB pour la copie
            static bool Copy(
                const char* source,
                const char* dest,
                bool overwrite = false
            );

            /// Copie un fichier vers une destination (version NkPath).
            /// @param source Chemin du fichier source en format NkPath
            /// @param dest Chemin de destination en format NkPath
            /// @param overwrite Si true, écrase la destination si elle existe
            /// @return true si la copie a réussi, false sinon
            static bool Copy(
                const NkPath& source,
                const NkPath& dest,
                bool overwrite = false
            );

            /// Déplace/renomme un fichier.
            /// @param source Chemin source en format C-string
            /// @param dest Chemin destination en format C-string
            /// @return true si le déplacement a réussi, false sinon
            /// @note Échoue si source et dest sont sur des volumes différents (selon OS)
            static bool Move(const char* source, const char* dest);

            /// Déplace/renomme un fichier (version NkPath).
            /// @param source Chemin source en format NkPath
            /// @param dest Chemin destination en format NkPath
            /// @return true si le déplacement a réussi, false sinon
            static bool Move(const NkPath& source, const NkPath& dest);

            /// Obtient la taille d'un fichier sans l'ouvrir explicitement.
            /// @param path Chemin du fichier en format C-string
            /// @return Taille en octets, ou -1 en cas d'erreur
            static nk_int64 GetFileSize(const char* path);

            /// Obtient la taille d'un fichier (version NkPath).
            /// @param path Chemin du fichier en format NkPath
            /// @return Taille en octets, ou -1 en cas d'erreur
            static nk_int64 GetFileSize(const NkPath& path);

            /// Lit l'intégralité d'un fichier texte en une seule opération.
            /// @param path Chemin du fichier en format C-string
            /// @return NkString contenant le contenu, ou chaîne vide en cas d'erreur
            /// @note Ouvre et ferme automatiquement le fichier en interne
            static NkString ReadAllText(const char* path);

            /// Lit l'intégralité d'un fichier texte (version NkPath).
            /// @param path Chemin du fichier en format NkPath
            /// @return NkString contenant le contenu, ou chaîne vide en cas d'erreur
            static NkString ReadAllText(const NkPath& path);

            /// Lit l'intégralité d'un fichier binaire en un vecteur d'octets.
            /// @param path Chemin du fichier en format C-string
            /// @return NkVector<nk_uint8> contenant les données, ou vecteur vide en erreur
            static NkVector<nk_uint8> ReadAllBytes(const char* path);

            /// Lit l'intégralité d'un fichier binaire (version NkPath).
            /// @param path Chemin du fichier en format NkPath
            /// @return NkVector<nk_uint8> contenant les données, ou vecteur vide en erreur
            static NkVector<nk_uint8> ReadAllBytes(const NkPath& path);

            /// Écrit une chaîne de texte dans un fichier (overwrite).
            /// @param path Chemin du fichier en format C-string
            /// @param text Contenu à écrire en format C-string
            /// @return true si l'écriture a réussi, false sinon
            /// @note Tronque le fichier existant avant écriture
            static bool WriteAllText(const char* path, const char* text);

            /// Écrit une chaîne de texte dans un fichier (version NkString/NkPath).
            /// @param path Chemin du fichier en format NkPath
            /// @param text Contenu à écrire en format NkString
            /// @return true si l'écriture a réussi, false sinon
            static bool WriteAllText(const NkPath& path, const NkString& text);

            /// Écrit un vecteur d'octets dans un fichier binaire (overwrite).
            /// @param path Chemin du fichier en format C-string
            /// @param data Données à écrire en format NkVector<nk_uint8>
            /// @return true si l'écriture a réussi, false sinon
            static bool WriteAllBytes(
                const char* path,
                const NkVector<nk_uint8>& data
            );

            /// Écrit un vecteur d'octets dans un fichier binaire (version NkPath).
            /// @param path Chemin du fichier en format NkPath
            /// @param data Données à écrire en format NkVector<nk_uint8>
            /// @return true si l'écriture a réussi, false sinon
            static bool WriteAllBytes(
                const NkPath& path,
                const NkVector<nk_uint8>& data
            );

        }; // class NkFile

        // -------------------------------------------------------------------------
        // SECTION 3 : ALIAS DE COMPATIBILITÉ LEGACY
        // -------------------------------------------------------------------------
        // Fournit des alias pour compatibilité avec l'ancien namespace nkentseu::entseu.

        namespace entseu {
            using NkFileMode = ::nkentseu::NkFileMode;
            using NkSeekOrigin = ::nkentseu::NkSeekOrigin;
            using NkFile = ::nkentseu::NkFile;
        } // namespace entseu

    } // namespace nkentseu

#endif // NKENTSEU_FILESYSTEM_NKFILE_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKFILE.H
// =============================================================================
// La classe NkFile fournit une interface RAII pour la lecture/écriture de fichiers
// sans dépendance STL, avec support binaire/texte et utilitaires statiques.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Écriture ligne par ligne avec gestion d'erreur
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Logger/NkLogger.h"

    void WriteLogExample() {
        using namespace nkentseu;

        // Ouverture en mode écriture (truncate par défaut)
        NkFile logFile("application.log", NkFileMode::NK_WRITE);

        if (!logFile.IsOpen()) {
            NK_LOG_ERROR("Failed to open log file");
            return;
        }

        // Écriture de lignes avec vérification
        if (!logFile.WriteLine("=== Application Start ===")) {
            NK_LOG_ERROR("Failed to write header");
            return;
        }

        logFile.WriteLine("Version: 1.0.0");
        logFile.WriteLine("Timestamp: 2024-01-15 10:30:00");

        // Flush optionnel pour garantir l'écriture immédiate
        logFile.Flush();

        // Fermeture automatique par RAII à la fin du scope
        // logFile.Close(); // Optionnel : appelé automatiquement par le destructeur
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Lecture complète d'un fichier de configuration
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Logger/NkLogger.h"

    bool LoadConfig(const char* configPath) {
        using namespace nkentseu;

        // Lecture atomique du fichier entier
        NkString content = NkFile::ReadAllText(configPath);

        if (content.Empty()) {
            NK_LOG_ERROR("Failed to read config: {}", configPath);
            return false;
        }

        // Parsing du contenu (exemple simplifié)
        // ParseConfig(content);

        NK_LOG_INFO("Config loaded successfully: {} bytes", content.Length());
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Lecture binaire d'un fichier de ressources
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Assert/NkAssert.h"

    namespace assets {

        bool LoadTextureData(
            const nkentseu::NkPath& path,
            nkentseu::NkVector<nkentseu::nk_uint8>& outData
        ) {
            using namespace nkentseu;

            // Lecture binaire atomique
            outData = NkFile::ReadAllBytes(path);

            if (outData.Empty()) {
                NK_LOG_ERROR("Failed to load texture: {}", path.ToString().CStr());
                return false;
            }

            NK_LOG_INFO(
                "Loaded {} bytes from {}",
                outData.Size(),
                path.ToString().CStr()
            );
            return true;
        }

    } // namespace assets
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Copie de fichier avec gestion d'erreur
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Logger/NkLogger.h"

    bool BackupFile(
        const nkentseu::NkPath& source,
        const nkentseu::NkPath& backupDir
    ) {
        using namespace nkentseu;

        // Construction du chemin de backup
        NkPath backupPath = backupDir / (source.GetFileName() + ".bak");

        // Copie avec overwrite pour remplacer les anciens backups
        if (!NkFile::Copy(source, backupPath, true)) {
            NK_LOG_ERROR(
                "Backup failed: {} -> {}",
                source.ToString().CStr(),
                backupPath.ToString().CStr()
            );
            return false;
        }

        NK_LOG_INFO("Backup created: {}", backupPath.ToString().CStr());
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Lecture ligne par ligne pour traitement streamé
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Logger/NkLogger.h"

    void ProcessLargeFile(const nkentseu::NkPath& path) {
        using namespace nkentseu;

        // Ouverture en lecture seule
        NkFile file(path, NkFileMode::NK_READ);

        if (!file.IsOpen()) {
            NK_LOG_ERROR("Cannot open file: {}", path.ToString().CStr());
            return;
        }

        // Traitement ligne par ligne pour éviter de charger tout le fichier en mémoire
        nk_uint64 lineCount = 0;
        while (!file.IsEOF()) {
            NkString line = file.ReadLine();

            // Ignorer les lignes vides en fin de fichier
            if (line.Empty() && file.IsEOF()) {
                break;
            }

            // Traitement de la ligne (exemple : comptage)
            ++lineCount;

            // Exemple de filtrage
            if (line.Contains("ERROR")) {
                NK_LOG_WARN("Line {}: {}", lineCount, line.CStr());
            }
        }

        NK_LOG_INFO("Processed {} lines from {}", lineCount, path.ToString().CStr());
        // Fermeture automatique par RAII
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Navigation dans un fichier binaire avec Seek
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"

    struct FileHeader {
        nk_uint32 magic;
        nk_uint32 version;
        nk_uint64 dataSize;
    };

    bool ReadCustomFormat(const nkentseu::NkPath& path) {
        using namespace nkentseu;

        NkFile file(path, NkFileMode::NK_READ_BINARY);
        if (!file.IsOpen()) {
            return false;
        }

        // Lecture de l'en-tête
        FileHeader header;
        if (file.Read(&header, sizeof(header)) != sizeof(header)) {
            return false;
        }

        // Validation du magic number
        if (header.magic != 0x4B4E4554) { // "KNET" en little-endian
            return false;
        }

        // Allocation du buffer pour les données
        NkVector<nk_uint8> data;
        data.Resize(static_cast<usize>(header.dataSize));

        // Lecture des données à la position courante
        if (file.Read(data.Data(), data.Size()) != data.Size()) {
            return false;
        }

        // Exemple : retour au début pour relecture
        file.SeekToBegin();

        // Traitement des données...
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Gestion des erreurs et nettoyage
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Assert/NkAssert.h"

    void SafeWriteExample() {
        using namespace nkentseu;

        const char* tempPath = "output.tmp";
        const char* finalPath = "output.dat";

        // Écriture dans un fichier temporaire d'abord
        {
            NkFile temp(tempPath, NkFileMode::NK_WRITE_BINARY);
            NKENTSEU_ASSERT(temp.IsOpen() && "Failed to create temp file");

            // ... écriture des données ...
            const char* data = "important content";
            temp.Write(data, strlen(data));
            temp.Flush();  // Garantir l'écriture sur disque
            // Fermeture automatique à la fin du scope
        }

        // Atomique : renommer le temp vers le final
        // Si le programme crash avant, le fichier final reste intact
        if (!NkFile::Move(tempPath, finalPath)) {
            NK_LOG_ERROR("Failed to commit output file");
            NkFile::Delete(tempPath);  // Nettoyage en cas d'échec
            return;
        }

        NK_LOG_INFO("Output committed successfully");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Utilitaires statiques pour scripts et outils
// -----------------------------------------------------------------------------
/*
    #include "NKFileSystem/NkFile.h"
    #include "NKCore/Logger/NkLogger.h"

    namespace tools {

        // Vérification rapide d'existence avant traitement
        bool FileExists(const nkentseu::NkPath& path) {
            return nkentseu::NkFile::Exists(path);
        }

        // Obtention de taille sans ouverture manuelle
        nkentseu::nk_int64 GetFileSizeSafe(const nkentseu::NkPath& path) {
            return nkentseu::NkFile::GetFileSize(path);
        }

        // Suppression avec logging
        bool DeleteWithLog(const nkentseu::NkPath& path) {
            using namespace nkentseu;

            if (!NkFile::Exists(path)) {
                NK_LOG_DEBUG("File already deleted: {}", path.ToString().CStr());
                return true;
            }

            if (NkFile::Delete(path)) {
                NK_LOG_INFO("Deleted: {}", path.ToString().CStr());
                return true;
            }

            NK_LOG_ERROR("Failed to delete: {}", path.ToString().CStr());
            return false;
        }

        // Lecture/écriture atomique pour les fichiers de configuration
        bool SaveConfig(const nkentseu::NkPath& path, const nkentseu::NkString& json) {
            return nkentseu::NkFile::WriteAllText(path, json);
        }

        nkentseu::NkString LoadConfig(const nkentseu::NkPath& path) {
            return nkentseu::NkFile::ReadAllText(path);
        }

    } // namespace tools
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================