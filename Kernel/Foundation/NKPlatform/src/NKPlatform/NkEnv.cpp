// =============================================================================
// NKPlatform/NkEnv.cpp
// Implémentations non-template de la gestion des variables d'environnement.
//
// Design :
//  - Implémentation portable Windows/POSIX pour les APIs système
//  - Gestion robuste des erreurs et des cas limites
//  - Utilisation sécurisée de malloc/free pour NkEnvString
//  - Déclarations explicites pour les APIs Windows manquantes
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NKPlatform/NkEnv.h"

#include <cstdlib>  // Pour getenv()
#include <cstring>  // Pour strlen, strcmp, etc.
#include <cstdio>   // Pour fopen, fgets, etc.

// -------------------------------------------------------------------------
// En-têtes spécifiques à la plateforme pour les APIs d'environnement
// -------------------------------------------------------------------------
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    // Configuration minimale pour Windows API
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600  // Windows Vista ou supérieur
    #endif
    #define WIN32_LEAN_AND_MEAN

    #include <windows.h>
    #include <processenv.h>

    // -------------------------------------------------------------------------
    // Déclarations explicites pour les APIs Windows
    // -------------------------------------------------------------------------
    // Certaines configurations MSVC/MinGW peuvent ne pas exposer ces symboles.
    // Ces déclarations externes assurent la liaison correcte.

    extern "C" __declspec(dllimport) LPCH WINAPI GetEnvironmentStringsA(VOID);
    extern "C" __declspec(dllimport) BOOL WINAPI FreeEnvironmentStringsA(LPCH);

#elif defined(NKENTSEU_PLATFORM_LINUX) || \
      defined(NKENTSEU_PLATFORM_MACOS) || \
      defined(NKENTSEU_PLATFORM_UNIX)
    // POSIX desktop : inclusion de unistd.h et déclaration de environ
    #include <unistd.h>
    extern char** environ;
    
#elif defined(NKENTSEU_PLATFORM_ANDROID)
    // Android : environ n'est pas exposé par défaut
    // On va utiliser /proc/self/environ via des APIs C standard
    #include <unistd.h>
    #include <stdio.h>   // Pour fopen, fgets
    #include <stdlib.h>  // Pour getenv
#endif

// -------------------------------------------------------------------------
// Espace de noms principal
// -------------------------------------------------------------------------
namespace nkentseu {

    namespace env {

        // ====================================================================
        // SECTION 1 : IMPLÉMENTATION DE NKENVSTRING
        // ====================================================================
        // Toutes les méthodes de NkEnvString sont définies ici car non-template.

        // -------------------------------------------------------------------------
        // Constructeurs
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE NkEnvString::NkEnvString() noexcept
            : mData(nullptr), mLength(0) {
            // Chaîne vide : pas d'allocation nécessaire
        }

        NKENTSEU_API_INLINE NkEnvString::NkEnvString(const char* str)
            : mData(nullptr), mLength(0) {
            if (str) {
                mLength = static_cast<NkSize>(std::strlen(str));
                mData = static_cast<char*>(
                    std::malloc(static_cast<size_t>(mLength) + 1)
                );
                if (mData) {
                    std::memcpy(
                        mData,
                        str,
                        static_cast<size_t>(mLength) + 1
                    );
                } else {
                    // Échec d'allocation : réinitialiser à l'état vide
                    mLength = 0;
                }
            }
        }

        NKENTSEU_API_INLINE NkEnvString::NkEnvString(char ch)
            : mData(nullptr), mLength(0) {
            mLength = 0;
            mData = static_cast<char*>(std::malloc(2));
            if (mData) {
                mData[0] = ch;
                mData[1] = '\0';
                mLength = 1;
            }
            // Si malloc échoue, mData reste nullptr et mLength = 0
        }

        NKENTSEU_API_INLINE NkEnvString::NkEnvString(const NkEnvString& other)
            : mData(nullptr), mLength(other.mLength) {
            if (other.mData) {
                mData = static_cast<char*>(
                    std::malloc(static_cast<size_t>(mLength) + 1)
                );
                if (mData) {
                    std::memcpy(
                        mData,
                        other.mData,
                        static_cast<size_t>(mLength) + 1
                    );
                } else {
                    // Échec d'allocation : réinitialiser
                    mLength = 0;
                }
            }
        }

        NKENTSEU_API_INLINE NkEnvString::NkEnvString(NkEnvString&& other) noexcept
            : mData(other.mData), mLength(other.mLength) {
            // Transfert de propriété : vider la source
            other.mData = nullptr;
            other.mLength = 0;
        }

        // -------------------------------------------------------------------------
        // Destructeur
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE NkEnvString::~NkEnvString() {
            if (mData) {
                std::free(mData);
                mData = nullptr;
            }
        }

        // -------------------------------------------------------------------------
        // Opérateurs d'affectation
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE NkEnvString& NkEnvString::operator=(
            const NkEnvString& other
        ) {
            if (this != &other) {
                // Idiom copy-and-swap pour exception safety
                NkEnvString temp(other);
                *this = NkMove(temp);
            }
            return *this;
        }

        NKENTSEU_API_INLINE NkEnvString& NkEnvString::operator=(
            NkEnvString&& other
        ) noexcept {
            if (this != &other) {
                // Libérer l'ancienne mémoire avant de prendre possession
                if (mData) {
                    std::free(mData);
                }
                mData = other.mData;
                mLength = other.mLength;
                // Vider la source
                other.mData = nullptr;
                other.mLength = 0;
            }
            return *this;
        }

        // -------------------------------------------------------------------------
        // Accesseurs
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE const char* NkEnvString::CStr() const noexcept {
            // Retourner une chaîne vide safe si mData est nullptr
            return mData ? mData : "";
        }

        NKENTSEU_API_INLINE NkSize NkEnvString::Size() const noexcept {
            return mLength;
        }

        NKENTSEU_API_INLINE bool NkEnvString::Empty() const noexcept {
            return mLength == 0;
        }

        NKENTSEU_API_INLINE void NkEnvString::Clear() noexcept {
            if (mData) {
                std::free(mData);
                mData = nullptr;
            }
            mLength = 0;
        }

        // -------------------------------------------------------------------------
        // Opérateurs de comparaison
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE bool NkEnvString::operator==(
            const NkEnvString& other
        ) const noexcept {
            if (mLength != other.mLength) {
                return false;
            }
            if (mData == other.mData) {
                return true;  // Même pointeur ou tous deux nullptr
            }
            if (!mData || !other.mData) {
                return false;  // Un seul est nullptr
            }
            return std::strcmp(mData, other.mData) == 0;
        }

        NKENTSEU_API_INLINE bool NkEnvString::operator!=(
            const NkEnvString& other
        ) const noexcept {
            return !(*this == other);
        }

        NKENTSEU_API_INLINE bool NkEnvString::operator<(
            const NkEnvString& other
        ) const noexcept {
            if (!mData && !other.mData) {
                return false;  // Égaux, donc pas <
            }
            if (!mData) {
                return true;  // nullptr < non-null
            }
            if (!other.mData) {
                return false;  // non-null >= nullptr
            }
            return std::strcmp(mData, other.mData) < 0;
        }

        // -------------------------------------------------------------------------
        // Opérateur de concaténation (fonction libre)
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE NkEnvString operator+(
            const NkEnvString& lhs,
            const NkEnvString& rhs
        ) {
            if (lhs.Empty()) {
                return rhs;
            }
            if (rhs.Empty()) {
                return lhs;
            }

            NkSize newLen = lhs.Size() + rhs.Size();
            char* buffer = static_cast<char*>(
                std::malloc(static_cast<size_t>(newLen) + 1)
            );
            if (!buffer) {
                // Fallback : retourner lhs en cas d'échec d'allocation
                return lhs;
            }

            std::memcpy(
                buffer,
                lhs.CStr(),
                static_cast<size_t>(lhs.Size())
            );
            std::memcpy(
                buffer + lhs.Size(),
                rhs.CStr(),
                static_cast<size_t>(rhs.Size()) + 1  // Inclure le null terminator
            );

            NkEnvString result(buffer);
            std::free(buffer);  // Buffer temporaire libéré après construction
            return result;
        }

        // ====================================================================
        // SECTION 2 : FONCTIONS D'ENVIRONNEMENT PORTABLES
        // ====================================================================
        // Implémentations multiplateformes des fonctions d'accès aux variables
        // d'environnement système.

        // -------------------------------------------------------------------------
        // NkGet : Lire une variable d'environnement
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE NkEnvString NkGet(
            const NkEnvString& name,
            NkEnvString& result
        ) noexcept {
            const char* cname = name.CStr();

            // Validation des paramètres
            if (!cname || cname[0] == '\0') {
                result.Clear();
                return result;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : utiliser GetEnvironmentVariableA avec buffer temporaire
                // Taille max d'une variable d'environnement Windows : 32767 caractères
                char buffer[32767];
                DWORD size = GetEnvironmentVariableA(cname, buffer, sizeof(buffer));

                if (size > 0 && size < sizeof(buffer)) {
                    result = NkEnvString(buffer);
                    return result;
                }
                // Variable non trouvée ou erreur : result reste vide

            #else
                // POSIX (y compris Android) : utiliser getenv (retourne nullptr si non trouvé)
                const char* val = std::getenv(cname);
                if (val) {
                    result = NkEnvString(val);
                    return result;
                }
            #endif

            // Cas par défaut : variable non trouvée
            result.Clear();
            return result;
        }

        // -------------------------------------------------------------------------
        // NkSet : Définir une variable d'environnement
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE bool NkSet(
            const NkEnvString& name,
            const NkEnvString& value,
            bool overwrite
        ) noexcept {
            const char* cname = name.CStr();
            const char* cval = value.CStr();

            // Validation : nom de variable ne peut pas être vide
            if (!cname || cname[0] == '\0') {
                return false;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : respecter le flag overwrite
                if (!overwrite && NkExists(name)) {
                    return true;  // Variable existe et on ne veut pas écraser : succès silencieux
                }
                // SetEnvironmentVariableA retourne non-zero en cas de succès
                return SetEnvironmentVariableA(cname, cval) != 0;

            #else
                // POSIX (y compris Android) : setenv avec flag overwrite (0 = ne pas écraser, 1 = écraser)
                return setenv(cname, cval, overwrite ? 1 : 0) == 0;
            #endif
        }

        // -------------------------------------------------------------------------
        // NkUnset : Supprimer une variable d'environnement
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE bool NkUnset(const NkEnvString& name) noexcept {
            const char* cname = name.CStr();

            if (!cname || cname[0] == '\0') {
                return false;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : SetEnvironmentVariable avec valeur nullptr supprime la variable
                return SetEnvironmentVariableA(cname, nullptr) != 0;
            #else
                // POSIX (y compris Android) : unsetenv retourne 0 en cas de succès
                return unsetenv(cname) == 0;
            #endif
        }

        // -------------------------------------------------------------------------
        // NkExists : Vérifier l'existence d'une variable
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE bool NkExists(const NkEnvString& name) noexcept {
            const char* cname = name.CStr();

            if (!cname || cname[0] == '\0') {
                return false;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : GetEnvironmentVariable avec buffer size 0 retourne la taille requise
                // Si > 0, la variable existe
                char buffer[1];
                DWORD size = GetEnvironmentVariableA(cname, buffer, 0);
                return size > 0;
            #else
                // POSIX (y compris Android) : getenv retourne nullptr si non trouvé
                return std::getenv(cname) != nullptr;
            #endif
        }

        // -------------------------------------------------------------------------
        // Fonction utilitaire pour lire /proc/self/environ sur Android
        // Version sans STL : utilise un buffer statique et construit la map directement
        // -------------------------------------------------------------------------
        #if defined(NKENTSEU_PLATFORM_ANDROID)
        static void ReadEnvironFromProc(NkEnvUMap<NkEnvString, NkEnvString>& result) {
            FILE* fp = fopen("/proc/self/environ", "r");
            if (!fp) {
                return;
            }
            
            // Buffer pour lire le fichier (taille raisonnable)
            char buffer[16384];  // 16KB devrait être suffisant pour l'environnement
            size_t total_read = 0;
            
            // Lire le fichier entier (les entrées sont séparées par '\0')
            while (!feof(fp) && total_read < sizeof(buffer) - 1) {
                size_t bytes_read = fread(buffer + total_read, 1, sizeof(buffer) - total_read - 1, fp);
                if (bytes_read == 0) break;
                total_read += bytes_read;
            }
            fclose(fp);
            
            buffer[total_read] = '\0';
            
            // Parser les entrées séparées par '\0'
            char* ptr = buffer;
            char* end = buffer + total_read;
            
            while (ptr < end && *ptr != '\0') {
                char* eqPos = strchr(ptr, '=');
                if (eqPos && eqPos > ptr) {
                    *eqPos = '\0';
                    NkEnvString key(ptr);
                    NkEnvString val(eqPos + 1);
                    result.Set(NkMove(key), NkMove(val));
                    *eqPos = '='; // Restaurer (optionnel)
                }
                // Avancer au prochain caractère nul
                ptr += strlen(ptr) + 1;
            }
        }
        #endif

        // -------------------------------------------------------------------------
        // NkGetAll : Obtenir toutes les variables d'environnement
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE NkEnvUMap<NkEnvString, NkEnvString> NkGetAll() noexcept {
            NkEnvUMap<NkEnvString, NkEnvString> result;

            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : GetEnvironmentStringsA retourne un bloc null-terminé de chaînes
                LPCH envBlock = GetEnvironmentStringsA();
                if (!envBlock) {
                    return result;  // Échec : retourner map vide
                }

                // Parser le bloc : chaque variable est "KEY=VALUE\0", bloc terminé par "\0\0"
                for (LPCH var = envBlock; *var; var += std::strlen(var) + 1) {
                    char* eqPos = std::strchr(var, '=');
                    if (eqPos) {
                        // Séparer temporairement clé et valeur pour construction
                        *eqPos = '\0';
                        NkEnvString key(var);
                        NkEnvString val(eqPos + 1);
                        result.Set(NkMove(key), NkMove(val));
                        *eqPos = '=';  // Restaurer pour intégrité (optionnel)
                    }
                }

                FreeEnvironmentStringsA(envBlock);

            #elif defined(NKENTSEU_PLATFORM_ANDROID)
                // Android : Lire depuis /proc/self/environ
                ReadEnvironFromProc(result);
                
            #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_UNIX)
                // POSIX desktop : environ est un tableau de pointeurs vers "KEY=VALUE\0"
                if (!environ) {
                    return result;  // Environnement non disponible
                }

                for (char** env = environ; *env; ++env) {
                    char* eqPos = std::strchr(*env, '=');
                    if (eqPos) {
                        *eqPos = '\0';
                        NkEnvString key(*env);
                        NkEnvString val(eqPos + 1);
                        result.Set(NkMove(key), NkMove(val));
                        *eqPos = '=';  // Restaurer
                    }
                }
            #endif

            return result;
        }

        // -------------------------------------------------------------------------
        // NkGetPathSeparator : Obtenir le séparateur de chemin système
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE char NkGetPathSeparator() noexcept {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return ';';  // Windows utilise ';' pour séparer les entrées PATH
            #else
                return ':';  // POSIX utilise ':' pour séparer les entrées PATH
            #endif
        }

        // -------------------------------------------------------------------------
        // Helper privé : Modifier une variable de type PATH
        // -------------------------------------------------------------------------

        static bool ModifyPath(
            const NkEnvString& directory,
            const NkEnvString& varName,
            bool prepend
        ) {
            // Validation : répertoire ne peut pas être vide
            if (directory.Empty()) {
                return false;
            }

            // Lire la valeur actuelle de la variable
            NkEnvString current;
            NkGet(varName, current);

            if (current.Empty()) {
                // Variable inexistante : créer avec le répertoire seul
                return NkSet(varName, directory);
            }

            // Obtenir le séparateur approprié à la plateforme
            char sep = NkGetPathSeparator();
            NkEnvString sepStr(sep);

            // Construire le nouveau PATH selon l'ordre demandé
            NkEnvString newPath;
            if (prepend) {
                // directory + sep + current
                newPath = directory + sepStr + current;
            } else {
                // current + sep + directory
                newPath = current + sepStr + directory;
            }

            // Appliquer la modification
            return NkSet(varName, newPath);
        }

        // -------------------------------------------------------------------------
        // NkPrependToPath : Ajouter un répertoire au début du PATH
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE bool NkPrependToPath(
            const NkEnvString& directory,
            const NkEnvString& varName
        ) {
            return ModifyPath(directory, varName, true);
        }

        // -------------------------------------------------------------------------
        // NkAppendToPath : Ajouter un répertoire à la fin du PATH
        // -------------------------------------------------------------------------

        NKENTSEU_API_INLINE bool NkAppendToPath(
            const NkEnvString& directory,
            const NkEnvString& varName
        ) {
            return ModifyPath(directory, varName, false);
        }

    } // namespace env

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================