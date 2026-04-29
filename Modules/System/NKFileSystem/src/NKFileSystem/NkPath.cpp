// =============================================================================
// NKFileSystem/NkPath.cpp
// Implémentation de la classe NkPath.
//
// Design :
//  - Normalisation automatique des séparateurs vers '/' en interne
//  - Décomposition et transformation sans dépendance STL
//  - Gestion cross-platform via détection de plateforme NKPlatform
//  - Validation des chemins : rejet des caractères interdits Windows/Unix
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
#include "NKFileSystem/NkPath.h"

// En-têtes C standard pour les fonctions système
#include <cstdlib>

// En-têtes plateforme pour getcwd, GetTempPath, etc.
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    // Alias pour compatibilité : getcwd sur Windows est _getcwd
    #define getcwd _getcwd
    // Undef des macros Windows qui pourraient entrer en conflit
    #ifdef GetCurrentDirectory
        #undef GetCurrentDirectory
    #endif
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
#else
    #include <unistd.h>  // getcwd sur POSIX
#endif

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes de NkPath dans le namespace nkentseu.

namespace nkentseu {

    // =============================================================================
    //  Méthodes privées
    // =============================================================================

    void NkPath::NormalizeSeparators() {
        // Parcours de la chaîne pour remplacer tous les séparateurs Windows par le préféré
        // Cette méthode est appelée après toute modification du chemin pour maintenir
        // la cohérence interne : tous les chemins utilisent '/' en interne
        for (usize i = 0; i < mPath.Length(); ++i) {
            if (mPath[i] == WINDOWS_SEPARATOR) {
                mPath[i] = PREFERRED_SEPARATOR;
            }
        }
    }

    // =============================================================================
    //  Constructeurs
    // =============================================================================
    // Tous les constructeurs initialisent mPath puis normalisent les séparateurs.

    NkPath::NkPath()
        : mPath()
    {
        // Constructeur par défaut : mPath est déjà initialisé à vide par NkString
        // Aucune normalisation nécessaire pour un chemin vide
    }

    NkPath::NkPath(const char* path)
        : mPath(path ? path : "")
    {
        // Gestion défensive : si path est null, utiliser chaîne vide
        // Normalisation automatique des séparateurs après construction
        NormalizeSeparators();
    }

    NkPath::NkPath(const NkString& path)
        : mPath(path)
    {
        // Copie du NkString puis normalisation
        // Permet de construire depuis un chemin déjà en format NkString
        NormalizeSeparators();
    }

    NkPath::NkPath(const NkPath& other)
        : mPath(other.mPath)
    {
        // Constructeur de copie : mPath est déjà normalisé dans other
        // Pas besoin de re-normaliser, copie directe
    }

    // =============================================================================
    //  Opérateurs d'affectation
    // =============================================================================
    // Permettent l'assignation depuis différents types avec normalisation automatique.

    NkPath& NkPath::operator=(const NkPath& other) {
        // Guard contre l'auto-affectation pour sécurité (bien que rarement nécessaire)
        if (this != &other) {
            mPath = other.mPath;
            // Pas de normalisation : other.mPath est déjà normalisé
        }
        return *this;
    }

    NkPath& NkPath::operator=(const char* path) {
        // Affectation depuis C-string avec gestion null
        mPath = (path ? path : "");
        // Normalisation requise car l'entrée peut contenir des '\\'
        NormalizeSeparators();
        return *this;
    }

    NkPath& NkPath::operator=(const NkString& path) {
        // Affectation depuis NkString
        mPath = path;
        // Normalisation requise car l'entrée peut contenir des '\\'
        NormalizeSeparators();
        return *this;
    }

    // =============================================================================
    //  Opérations de jointure
    // =============================================================================
    // Méthodes pour concaténer des composants de chemin avec gestion des séparateurs.

    NkPath& NkPath::Append(const char* component) {
        // Guard : ignorer les composants vides ou null
        if (!component || !component[0]) {
            return *this;
        }

        // Cas spécial : si le chemin courant est vide, remplacer par le composant
        if (mPath.Empty()) {
            mPath = component;
            NormalizeSeparators();
            return *this;
        }

        // Ajouter un séparateur si le chemin courant n'en a pas déjà un à la fin
        if (mPath[mPath.Length() - 1] != PREFERRED_SEPARATOR) {
            mPath += PREFERRED_SEPARATOR;
        }

        // Concaténation du composant
        mPath += component;

        // Normalisation finale pour gérer les '\\' potentiellement dans component
        NormalizeSeparators();
        return *this;
    }

    NkPath& NkPath::Append(const NkPath& other) {
        // Délégation à la version C-string pour éviter la duplication de code
        return Append(other.CStr());
    }

    NkPath& NkPath::Append(const NkString& other) {
        // Délégation à la version C-string pour éviter la duplication de code
        return Append(other.CStr());
    }

    NkPath NkPath::operator/(const char* component) const {
        // Opérateur de jointure : crée un nouveau chemin sans modifier l'original
        NkPath result = *this;  // Copie de l'instance courante
        result.Append(component);  // Ajout du composant à la copie
        return result;  // Retour par valeur (optimisé par RVO/NRVO)
    }

    NkPath NkPath::operator/(const NkPath& other) const {
        // Délégation à la version C-string pour éviter la duplication de code
        return *this / other.CStr();
    }

    // =============================================================================
    //  Décomposition du chemin
    // =============================================================================
    // Méthodes pour extraire des composants spécifiques d'un chemin.

    NkString NkPath::GetDirectory() const {
        // Recherche du dernier séparateur pour isoler le répertoire
        const char* path = mPath.CStr();
        const char* lastSep = nullptr;

        // Parcours linéaire pour trouver la dernière occurrence d'un séparateur
        // Supporte à la fois '/' et '\\' pour robustesse
        for (const char* p = path; *p; ++p) {
            if (*p == PREFERRED_SEPARATOR || *p == WINDOWS_SEPARATOR) {
                lastSep = p;
            }
        }

        // Si aucun séparateur trouvé, retourne une chaîne vide (pas de répertoire)
        if (!lastSep) {
            return NkString();
        }

        // Construction du sous-chemin depuis le début jusqu'au dernier séparateur
        // lastSep - path donne la longueur du répertoire en nombre de caractères
        return NkString(path, static_cast<usize>(lastSep - path));
    }

    NkString NkPath::GetFileName() const {
        // Recherche du dernier séparateur pour isoler le nom de fichier
        const char* path = mPath.CStr();
        const char* lastSep = nullptr;

        // Même logique que GetDirectory : trouver la dernière occurrence
        for (const char* p = path; *p; ++p) {
            if (*p == PREFERRED_SEPARATOR || *p == WINDOWS_SEPARATOR) {
                lastSep = p;
            }
        }

        // Si aucun séparateur, le chemin entier est le nom de fichier
        if (!lastSep) {
            return mPath;
        }

        // Construction depuis le caractère après le dernier séparateur jusqu'à la fin
        return NkString(lastSep + 1);
    }

    NkString NkPath::GetFileNameWithoutExtension() const {
        // Obtention du nom de fichier complet d'abord
        NkString filename = GetFileName();
        const char* name = filename.CStr();
        const char* lastDot = nullptr;

        // Recherche du dernier point pour isoler l'extension
        // Parcours jusqu'au terminateur nul
        for (const char* p = name; *p; ++p) {
            if (*p == '.') {
                lastDot = p;
            }
        }

        // Si un point est trouvé et qu'il n'est pas en première position (fichier caché)
        if (lastDot && lastDot != name) {
            // Retourne le nom depuis le début jusqu'au point (excluant le point)
            return NkString(name, static_cast<usize>(lastDot - name));
        }

        // Pas d'extension trouvée : retourne le nom complet
        return filename;
    }

    NkString NkPath::GetExtension() const {
        // Obtention du nom de fichier complet d'abord
        NkString filename = GetFileName();
        const char* name = filename.CStr();
        const char* lastDot = nullptr;

        // Recherche du dernier point pour isoler l'extension
        for (const char* p = name; *p; ++p) {
            if (*p == '.') {
                lastDot = p;
            }
        }

        // Si un point est trouvé et qu'il n'est pas en première position
        if (lastDot && lastDot != name) {
            // Retourne depuis le point jusqu'à la fin (incluant le point)
            return NkString(lastDot);
        }

        // Pas d'extension : retourne chaîne vide
        return NkString();
    }

    NkString NkPath::GetRoot() const {
        // Extraction de la racine du chemin (ex: "C:/" ou "/")
        const char* path = mPath.CStr();

        // Cas Windows : chemin avec lettre de lecteur (ex: "C:/...")
        if (path[0] && path[1] == ':') {
            // Construction de la racine : "X:/" où X est la lettre du lecteur
            char root[4] = {
                path[0],      // Lettre du lecteur (ex: 'C')
                ':',          // Deux-points séparateur
                PREFERRED_SEPARATOR,  // Séparateur de racine
                '\0'          // Terminateur nul
            };
            return NkString(root);
        }

        // Cas Unix : chemin absolu commençant par '/'
        if (path[0] == PREFERRED_SEPARATOR) {
            return NkString("/");
        }

        // Chemin relatif : pas de racine
        return NkString();
    }

    // =============================================================================
    //  Propriétés du chemin
    // =============================================================================
    // Méthodes de prédicat pour inspecter les caractéristiques du chemin.

    bool NkPath::IsAbsolute() const {
        // Un chemin est absolu s'il commence par une racine reconnue
        const char* path = mPath.CStr();

        // Détection de la racine Windows : "X:..."
        if (path[0] && path[1] == ':') {
            return true;
        }

        // Détection de la racine Unix : commence par '/' ou '\\'
        if (path[0] == PREFERRED_SEPARATOR || path[0] == WINDOWS_SEPARATOR) {
            return true;
        }

        // Aucun des cas ci-dessus : chemin relatif
        return false;
    }

    bool NkPath::IsRelative() const {
        // Un chemin est relatif s'il n'est pas absolu
        // Implémentation déléguée pour éviter la duplication de logique
        return !IsAbsolute();
    }

    bool NkPath::HasExtension() const {
        // Un chemin a une extension si GetExtension() retourne une chaîne non vide
        return !GetExtension().Empty();
    }

    bool NkPath::HasFileName() const {
        // Un chemin a un nom de fichier si GetFileName() retourne une chaîne non vide
        return !GetFileName().Empty();
    }

    // =============================================================================
    //  Modification du chemin
    // =============================================================================
    // Méthodes mutables pour transformer le chemin courant.

    NkPath& NkPath::ReplaceExtension(const char* newExt) {
        // Extraction des composants existants
        NkString dir = GetDirectory();
        NkString name = GetFileNameWithoutExtension();

        // Reconstruction du chemin : répertoire + nouveau nom + nouvelle extension
        mPath = dir;

        // Ajout du séparateur si le répertoire n'est pas vide
        if (!dir.Empty()) {
            mPath += PREFERRED_SEPARATOR;
        }

        // Ajout du nom de fichier sans extension
        mPath += name;

        // Ajout de la nouvelle extension si fournie et non vide
        if (newExt && newExt[0]) {
            // Ajout automatique du point si l'extension ne commence pas par '.'
            if (newExt[0] != '.') {
                mPath += '.';
            }
            mPath += newExt;
        }

        // Pas de normalisation nécessaire : on construit avec PREFERRED_SEPARATOR
        return *this;
    }

    NkPath& NkPath::ReplaceFileName(const char* newName) {
        // Extraction du répertoire existant
        NkString dir = GetDirectory();

        // Reconstruction : répertoire + nouveau nom
        mPath = dir;

        // Ajout du séparateur si le répertoire n'est pas vide
        if (!dir.Empty()) {
            mPath += PREFERRED_SEPARATOR;
        }

        // Ajout du nouveau nom (gestion null défensive)
        mPath += (newName ? newName : "");

        // Pas de normalisation nécessaire : construction contrôlée
        return *this;
    }

    NkPath& NkPath::ReplaceExtension(const NkString& newExt) {
        // Délégation à la version C-string pour éviter la duplication
        return ReplaceExtension(newExt.CStr());
    }

    NkPath& NkPath::ReplaceFileName(const NkString& newName) {
        // Délégation à la version C-string pour éviter la duplication
        return ReplaceFileName(newName.CStr());
    }

    NkPath& NkPath::RemoveFileName() {
        // Remplacement du chemin par son répertoire parent
        mPath = GetDirectory();
        // Pas de normalisation nécessaire : GetDirectory() retourne déjà normalisé
        return *this;
    }

    NkPath NkPath::GetParent() const {
        // Obtention du chemin parent sans modifier l'instance courante
        // Construction d'un nouveau NkPath depuis le répertoire
        return NkPath(GetDirectory());
    }

    // =============================================================================
    //  Conversion et accès
    // =============================================================================
    // Méthodes pour obtenir des représentations alternatives du chemin.

    const char* NkPath::CStr() const {
        // Accès en lecture seule à la chaîne interne
        // Retourne le chemin avec séparateurs normalisés '/'
        return mPath.CStr();
    }

    NkString NkPath::ToString() const {
        // Conversion explicite vers NkString (copie)
        // Utile pour les opérations de manipulation de chaîne
        return mPath;
    }

    NkString NkPath::ToNative() const {
        // Conversion vers le format natif de la plateforme cible
        NkString result = mPath;

        #ifdef _WIN32
            // Sur Windows : remplacer '/' par '\\' pour compatibilité APIs système
            for (usize i = 0; i < result.Length(); ++i) {
                if (result[i] == PREFERRED_SEPARATOR) {
                    result[i] = WINDOWS_SEPARATOR;
                }
            }
        #endif
        // Sur Unix/macOS : le format interne '/' est déjà natif, rien à faire

        return result;
    }

    // =============================================================================
    //  Comparaison
    // =============================================================================
    // Opérateurs pour comparer deux chemins.

    bool NkPath::operator==(const NkPath& other) const {
        // Comparaison chaîne par chaîne des chemins internalisés
        // Note : cette comparaison est sensible à la casse et aux séparateurs
        // Pour une comparaison insensible à la casse, utiliser une fonction dédiée
        return mPath == other.mPath;
    }

    bool NkPath::operator!=(const NkPath& other) const {
        // Inégalité : déléguée à l'opérateur d'égalité pour cohérence
        return !(*this == other);
    }

    // =============================================================================
    //  Méthodes statiques utilitaires
    // =============================================================================
    // Fonctions ne nécessitant pas d'instance de NkPath.

    NkPath NkPath::GetCurrentDirectory() {
        // Buffer temporaire pour recevoir le chemin du répertoire courant
        // Taille généreuse (4096) pour couvrir la plupart des chemins réels
        char buffer[4096];

        // Appel système pour obtenir le cwd
        // getcwd sur POSIX, _getcwd sur Windows (via #define plus haut)
        if (getcwd(buffer, sizeof(buffer))) {
            // Succès : construction et retour du NkPath
            // La construction appelle automatiquement NormalizeSeparators()
            return NkPath(buffer);
        }

        // Échec : retour d'un chemin vide
        // L'appelant doit vérifier le résultat avant utilisation
        return NkPath();
    }

    NkPath NkPath::GetTempDirectory() {
        #ifdef _WIN32
            // Windows : utilisation de GetTempPathA pour obtenir le répertoire temp
            char buffer[MAX_PATH];

            if (GetTempPathA(MAX_PATH, buffer)) {
                // Succès : retour du chemin obtenu
                return NkPath(buffer);
            }

            // Fallback en cas d'échec de GetTempPathA
            return NkPath("C:/Temp");
        #else
            // POSIX : utilisation de la variable d'environnement TMPDIR
            const char* tmpdir = getenv("TMPDIR");

            if (tmpdir) {
                // TMPDIR définie : utilisation de sa valeur
                return NkPath(tmpdir);
            }

            // Fallback standard pour Unix/Linux
            return NkPath("/tmp");
        #endif
    }

    NkPath NkPath::Combine(const char* path1, const char* path2) {
        // Combinaison de deux chemins en un seul
        // Gestion défensive des paramètres null ou vides

        // Construction depuis path1 (ou chaîne vide si null)
        NkPath result(path1 ? path1 : "");

        // Ajout de path2 via Append (gère automatiquement les séparateurs)
        result.Append(path2 ? path2 : "");

        return result;
    }

    bool NkPath::IsValidPath(const char* path) {
        // Validation basique : rejet des caractères interdits sur Windows/Unix
        // Cette validation n'est pas exhaustive mais couvre les cas les plus courants

        // Rejet des chemins null ou vides
        if (!path || !path[0]) {
            return false;
        }

        // Parcours caractère par caractère pour détection des interdits
        for (const char* p = path; *p; ++p) {
            const char c = *p;

            // Caractères interdits sur Windows : < > " | ? *
            // Ces caractères sont aussi problématiques sur la plupart des systèmes
            if (c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*') {
                return false;
            }

            // Rejet des caractères de contrôle (ASCII < 32)
            // Inclut : newline, tab, bell, etc. qui peuvent corrompre les chemins
            if (c < 32) {
                return false;
            }
        }

        // Aucun caractère interdit trouvé : chemin considéré comme valide
        // Note : cette validation ne vérifie pas l'existence du chemin sur le filesystem
        return true;
    }

    NkPath NkPath::Combine(const NkString& path1, const NkString& path2) {
        // Délégation à la version C-string pour éviter la duplication de code
        return Combine(path1.CStr(), path2.CStr());
    }

    bool NkPath::IsValidPath(const NkString& path) {
        // Délégation à la version C-string pour éviter la duplication de code
        return IsValidPath(path.CStr());
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Normalisation des séparateurs :
    ------------------------------
    - Tous les chemins sont stockés en interne avec '/' comme séparateur
    - Cette approche simplifie la manipulation et la comparaison des chemins
    - ToNative() convertit vers le format de la plateforme uniquement à la frontière
      avec les APIs système (open, CreateFile, etc.)

    Performance des opérations de décomposition :
    --------------------------------------------
    - GetDirectory/GetFileName : parcours linéaire O(n) du chemin
    - Pas d'allocation dynamique supplémentaire : utilisation de NkString(substring)
    - Pour les chemins très longs, considérer la mise en cache si nécessaire

    Validation des chemins :
    -----------------------
    - IsValidPath() effectue une validation syntaxique basique
    - Ne vérifie pas l'existence du chemin sur le filesystem
    - Ne vérifie pas les permissions d'accès
    - Pour une validation complète, combiner avec des appels système appropriés

    Thread-safety :
    --------------
    - NkPath n'est PAS thread-safe pour les opérations mutables
    - Les méthodes const sont thread-safe pour la lecture
    - Pour un accès concurrent, utiliser une synchronisation externe

    Compatibilité cross-platform :
    -----------------------------
    - Windows : support des chemins avec lettre de lecteur (C:/...)
    - Unix/Linux/macOS : support des chemins absolus (/...) et relatifs
    - Les chemins UNC (\\server\share) ne sont pas explicitement supportés
      mais fonctionneront partiellement via la normalisation

    Extensions futures possibles :
    -----------------------------
    - Support des chemins UNC Windows (\\?\, \\server\share)
    - Résolution des liens symboliques (readlink sur Unix)
    - Normalisation des ".." et "." dans les chemins
    - Comparaison insensible à la casse (optionnelle, selon la plateforme)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================