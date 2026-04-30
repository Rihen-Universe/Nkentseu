// =============================================================================
// Fichier : NkGamepadMappingPersistence.cpp
// =============================================================================
// Description :
//   Implémentation du backend texte pour la persistance des profils de 
//   mapping gamepad, utilisant un format lisible et sans dépendances externes.
//
// Format de fichier texte (.nkmap) :
//   - Ligne 1 : "nkmap <version>" (en-tête de version)
//   - Ligne 2 : "backend <nom>" (identification du backend)
//   - Blocs slot : délimités par "slot <index> <active>" et "end_slot"
//   - Entrées bouton : "button <physique> <logique>"
//   - Entrées axe : "axis <physique> <logique> <scale> <invert>"
//   - Commentaires : lignes ignorées (préfixées par '#' en futur)
//
// Gestion des erreurs :
//   - Retour booléen avec message d'erreur optionnel via pointeur NkString*.
//   - Fermeture propre des fichiers même en cas d'échec partiel.
//   - Validation des tokens avant conversion numérique.
//
// Portabilité :
//   - Gestion des séparateurs de chemin (\ vs /) normalisés en '/'.
//   - Création récursive des répertoires sur Windows et POSIX.
//   - Utilisation de fonctions C standard pour compatibilité maximale.
//
// Auteur : Équipe NkEntseu
// Version : 1.0.0
// Date : 2026
// =============================================================================

#include "NkGamepadMappingPersistence.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include "NKCore/NkTraits.h"
#include "NKContainers/String/Encoding/NkASCII.h"

#if defined(_WIN32)
    #include <direct.h>
    #include <sys/stat.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

namespace nkentseu {
    // =========================================================================
    // Section : Fonctions utilitaires internes (namespace anonyme)
    // =========================================================================

    namespace {
        // -------------------------------------------------------------------------
        // Fonction : NormalizePathSeparators
        // -------------------------------------------------------------------------
        // Description :
        //   Normalise les séparateurs de chemin en remplaçant les backslashes 
        //   Windows par des slashes UNIX pour une manipulation cohérente.
        // Paramètres :
        //   - path : Référence vers la chaîne de chemin à normaliser (modifiée in-place).
        // Notes :
        //   - Opération in-place pour éviter les allocations supplémentaires.
        //   - Conservé pour compatibilité avec les chemins saisis manuellement.
        // -------------------------------------------------------------------------
        static void NormalizePathSeparators(NkString& path) {
            for (NkString::SizeType i = 0; i < path.Size(); ++i) {
                if (path[i] == '\\') {
                    path[i] = '/';
                }
            }
        }

        // -------------------------------------------------------------------------
        // Fonction : IsAsciiSpace
        // -------------------------------------------------------------------------
        // Description :
        //   Détermine si un caractère est un espace ASCII (whitespace).
        // Paramètres :
        //   - c : Caractère à tester.
        // Retour :
        //   - true si le caractère est espace, tabulation, retour chariot ou saut de ligne.
        //   - false sinon.
        // Utilisation :
        //   - Tokenisation des lignes du fichier de configuration.
        //   - Ignorance des espaces lors du parsing.
        // -------------------------------------------------------------------------
        static bool IsAsciiSpace(const char c) {
            return c == ' '
                || c == '\t'
                || c == '\r'
                || c == '\n';
        }

        // -------------------------------------------------------------------------
        // Fonction : ReadTextLine
        // -------------------------------------------------------------------------
        // Description :
        //   Lit une ligne complète depuis un fichier texte, en gérant les 
        //   fins de ligne multi-plateformes (CRLF, LF, CR).
        // Paramètres :
        //   - file    : Pointeur FILE ouvert en mode lecture.
        //   - outLine : Référence vers la chaîne de sortie pour le contenu de la ligne.
        // Retour :
        //   - true si une ligne a été lue (même vide).
        //   - false en cas de fin de fichier ou d'erreur de lecture.
        // Notes :
        //   - Les caractères '\r' sont ignorés pour normaliser CRLF → LF.
        //   - La ligne retournée ne contient pas le caractère de fin de ligne.
        // -------------------------------------------------------------------------
        static bool ReadTextLine(FILE* file, NkString& outLine) {
            outLine.Clear();
            if (!file) {
                return false;
            }

            int ch = 0;
            while ((ch = ::fgetc(file)) != EOF) {
                const char c = static_cast<char>(ch);
                if (c == '\r') {
                    continue;
                }
                if (c == '\n') {
                    break;
                }
                outLine.PushBack(c);
            }

            return ch != EOF || !outLine.Empty();
        }

        // -------------------------------------------------------------------------
        // Fonction : TokenizeWhitespace
        // -------------------------------------------------------------------------
        // Description :
        //   Découpe une chaîne en tokens séparés par des espaces ASCII.
        // Paramètres :
        //   - line      : Chaîne d'entrée à tokeniser.
        //   - outTokens : Vecteur de sortie pour les tokens extraits.
        // Comportement :
        //   - Ignore les espaces multiples consécutifs.
        //   - Préserve l'ordre d'apparition des tokens.
        //   - Vide le vecteur de sortie avant remplissage.
        // Utilisation :
        //   - Parsing des lignes de commande du fichier de mapping.
        // -------------------------------------------------------------------------
        static void TokenizeWhitespace(const NkString& line, NkVector<NkString>& outTokens) {
            outTokens.Clear();
            const NkString::SizeType size = line.Size();
            NkString::SizeType i = 0;

            while (i < size) {
                while (i < size && IsAsciiSpace(line[i])) {
                    ++i;
                }
                if (i >= size) {
                    break;
                }

                const NkString::SizeType start = i;
                while (i < size && !IsAsciiSpace(line[i])) {
                    ++i;
                }
                outTokens.PushBack(line.SubStr(start, i - start));
            }
        }

        // -------------------------------------------------------------------------
        // Fonction : SliceAfterFirstToken
        // -------------------------------------------------------------------------
        // Description :
        //   Extrait le reste d'une ligne après le premier token et ses espaces.
        // Paramètres :
        //   - line : Chaîne d'entrée contenant au moins un token.
        // Retour :
        //   - Sous-chaîne trimmée après le premier token, ou chaîne vide si absent.
        // Utilisation :
        //   - Extraction de valeurs string (noms de backend, chemins...) 
        //     après un mot-clé de commande.
        // -------------------------------------------------------------------------
        static NkString SliceAfterFirstToken(const NkString& line) {
            const NkString::SizeType size = line.Size();
            NkString::SizeType i = 0;

            while (i < size && !IsAsciiSpace(line[i])) {
                ++i;
            }
            while (i < size && IsAsciiSpace(line[i])) {
                ++i;
            }

            if (i >= size) {
                return {};
            }
            NkString out = line.SubStr(i);
            out.Trim();
            return out;
        }

        // -------------------------------------------------------------------------
        // Fonction : ParseUIntToken
        // -------------------------------------------------------------------------
        // Description :
        //   Tente de convertir un token en entier non signé 32 bits.
        // Paramètres :
        //   - token    : Chaîne représentant la valeur numérique.
        //   - outValue : Référence pour stocker la valeur convertie.
        // Retour :
        //   - true si la conversion a réussi.
        //   - false si le token est invalide ou hors plage.
        // Notes :
        //   - Délègue à NkString::ToUInt() pour la logique de parsing.
        // -------------------------------------------------------------------------
        static bool ParseUIntToken(const NkString& token, uint32& outValue) {
            return token.ToUInt(outValue);
        }

        // -------------------------------------------------------------------------
        // Fonction : ParseIntToken
        // -------------------------------------------------------------------------
        // Description :
        //   Tente de convertir un token en entier signé 32 bits.
        // Paramètres :
        //   - token    : Chaîne représentant la valeur numérique.
        //   - outValue : Référence pour stocker la valeur convertie.
        // Retour :
        //   - true si la conversion a réussi.
        //   - false si le token est invalide ou hors plage.
        // Utilisation :
        //   - Parsing des flags booléens stockés comme 0/1.
        // -------------------------------------------------------------------------
        static bool ParseIntToken(const NkString& token, int32& outValue) {
            return token.ToInt(outValue);
        }

        // -------------------------------------------------------------------------
        // Fonction : ParseFloatToken
        // -------------------------------------------------------------------------
        // Description :
        //   Tente de convertir un token en flottant 32 bits.
        // Paramètres :
        //   - token    : Chaîne représentant la valeur numérique.
        //   - outValue : Référence pour stocker la valeur convertie.
        // Retour :
        //   - true si la conversion a réussi.
        //   - false si le token est invalide ou hors plage.
        // Utilisation :
        //   - Parsing des facteurs de scale pour les axes.
        // -------------------------------------------------------------------------
        static bool ParseFloatToken(const NkString& token, float32& outValue) {
            return token.ToFloat(outValue);
        }

        // -------------------------------------------------------------------------
        // Fonction : DirectoryExists
        // -------------------------------------------------------------------------
        // Description :
        //   Vérifie si un chemin spécifié existe et est un répertoire.
        // Paramètres :
        //   - path : Chemin à tester.
        // Retour :
        //   - true si le chemin existe et est un répertoire.
        //   - false sinon ou en cas d'erreur système.
        // Portabilité :
        //   - Utilise _stat sur Windows, stat sur POSIX.
        // -------------------------------------------------------------------------
        static bool DirectoryExists(const NkString& path) {
            if (path.Empty()) {
                return false;
            }
            #if defined(_WIN32)
                struct _stat st{};
                if (_stat(path.CStr(), &st) != 0) {
                    return false;
                }
                return (st.st_mode & _S_IFDIR) != 0;
            #else
                struct stat st{};
                if (stat(path.CStr(), &st) != 0) {
                    return false;
                }
                return S_ISDIR(st.st_mode);
            #endif
        }

        // -------------------------------------------------------------------------
        // Fonction : CreateDirectorySingle
        // -------------------------------------------------------------------------
        // Description :
        //   Tente de créer un répertoire unique (pas récursif).
        // Paramètres :
        //   - path : Chemin du répertoire à créer.
        // Retour :
        //   - true si le répertoire a été créé ou existe déjà.
        //   - false en cas d'échec (permissions, chemin invalide...).
        // Notes :
        //   - Ne crée pas les répertoires parents intermédiaires.
        //   - Retourne true si le répertoire existe déjà (EEXIST).
        // -------------------------------------------------------------------------
        static bool CreateDirectorySingle(const NkString& path) {
            if (path.Empty()) {
                return false;
            }
            if (DirectoryExists(path)) {
                return true;
            }
            #if defined(_WIN32)
                if (_mkdir(path.CStr()) == 0) {
                    return true;
                }
                return errno == EEXIST;
            #else
                if (mkdir(path.CStr(), 0755) == 0) {
                    return true;
                }
                return errno == EEXIST;
            #endif
        }

        // -------------------------------------------------------------------------
        // Fonction : EnsureDirectories
        // -------------------------------------------------------------------------
        // Description :
        //   Crée récursivement tous les répertoires intermédiaires d'un chemin.
        // Paramètres :
        //   - inPath : Chemin complet dont les répertoires doivent exister.
        // Retour :
        //   - true si tous les répertoires ont été créés ou existent déjà.
        //   - false en cas d'échec à n'importe quel niveau.
        // Algorithme :
        //   1. Normalisation des séparateurs de chemin.
        //   2. Gestion des préfixes de lecteur Windows (C:) ou racine UNIX (/).
        //   3. Parcours segment par segment avec création cumulative.
        // -------------------------------------------------------------------------
        static bool EnsureDirectories(const NkString& inPath) {
            if (inPath.Empty()) {
                return false;
            }

            NkString path = inPath;
            NormalizePathSeparators(path);

            NkString current;
            NkString::SizeType cursor = 0;

            if (path.Size() >= 2
                && encoding::ascii::NkIsAlpha(path[0])
                && path[1] == ':') {
                current = path.SubStr(0, 2);
                cursor = 2;
                if (cursor < path.Size() && path[cursor] == '/') {
                    current.PushBack('/');
                    ++cursor;
                }
            } else if (!path.Empty() && path[0] == '/') {
                current = "/";
                cursor = 1;
            }

            NkString::SizeType segmentStart = cursor;
            for (NkString::SizeType i = cursor; i <= path.Size(); ++i) {
                if (i < path.Size() && path[i] != '/') {
                    continue;
                }
                const NkString segment = path.SubStr(segmentStart, i - segmentStart);
                segmentStart = i + 1;
                if (segment.Empty()) {
                    continue;
                }

                if (!current.Empty() && current.Back() != '/') {
                    current.PushBack('/');
                }
                current += segment;

                if (!CreateDirectorySingle(current)) {
                    return false;
                }
            }
            return true;
        }

        // -------------------------------------------------------------------------
        // Fonction : JoinPath
        // -------------------------------------------------------------------------
        // Description :
        //   Concatène deux chemins en gérant automatiquement le séparateur.
        // Paramètres :
        //   - a : Premier segment de chemin.
        //   - b : Second segment de chemin.
        // Retour :
        //   - Chemin concaténé avec séparateur unique si nécessaire.
        // Règles :
        //   - Si a est vide, retourne b.
        //   - Si b est vide, retourne a.
        //   - Si a se termine déjà par '/' ou '\', concatène directement.
        //   - Sinon, insère '/' entre a et b.
        // -------------------------------------------------------------------------
        static NkString JoinPath(const NkString& a, const NkString& b) {
            if (a.Empty()) {
                return b;
            }
            if (b.Empty()) {
                return a;
            }
            const char last = a.Back();
            if (last == '/' || last == '\\') {
                return a + b;
            }
            return a + "/" + b;
        }
    } // namespace

    // =========================================================================
    // Section : Implémentation de NkTextGamepadMappingPersistence
    // =========================================================================

    // -------------------------------------------------------------------------
    // Constructeur : NkTextGamepadMappingPersistence
    // -------------------------------------------------------------------------
    // Description :
    //   Initialise le backend de persistance avec répertoire et extension 
    //   configurables, avec valeurs par défaut résolues automatiquement.
    // Paramètres :
    //   - baseDirectory : Répertoire de base pour le stockage (vide = auto-résolu).
    //   - fileExtension : Extension de fichier pour les profils (vide = ".nkmap").
    // Initialisation :
    //   - Résolution du répertoire par défaut si non spécifié.
    //   - Normalisation de l'extension avec préfixe '.' si absent.
    //   - Déplacement sémantique des paramètres pour éviter les copies.
    // -------------------------------------------------------------------------
    NkTextGamepadMappingPersistence::NkTextGamepadMappingPersistence(
        NkString baseDirectory,
        NkString fileExtension
    )
        : mBaseDirectory(traits::NkMove(baseDirectory))
        , mExtension(traits::NkMove(fileExtension)) {
        if (mBaseDirectory.Empty()) {
            mBaseDirectory = ResolveDefaultBaseDirectory();
        }
        if (mExtension.Empty()) {
            mExtension = ".nkmap";
        }
        if (!mExtension.Empty() && mExtension[0] != '.') {
            mExtension.Insert(0, 1, '.');
        }
    }

    // -------------------------------------------------------------------------
    // Méthode statique : ResolveDefaultBaseDirectory
    // -------------------------------------------------------------------------
    // Description :
    //   Résout le répertoire de stockage par défaut selon la plateforme 
    //   et les variables d'environnement disponibles.
    // Priorité de résolution :
    //   1. Variable d'environnement NKENTSEU_GAMEPAD_MAPPING_DIR (si définie).
    //   2. Windows : %LOCALAPPDATA%/Nkentseu/NKWindow/GamepadMappings
    //      ou fallback sur %APPDATA% si LOCALAPPDATA indisponible.
    //   3. POSIX : ~/.config/nkentseu/nkwindow/gamepad_mappings
    //   4. Fallback développement : "Build/GamepadMappings"
    // Retour :
    //   - Chemin absolu ou relatif vers le répertoire de stockage.
    // Utilisation :
    //   - Initialisation automatique quand aucun répertoire n'est configuré.
    // -------------------------------------------------------------------------
    NkString NkTextGamepadMappingPersistence::ResolveDefaultBaseDirectory() {
        if (const char* env = ::getenv("NKENTSEU_GAMEPAD_MAPPING_DIR")) {
            if (*env) {
                return env;
            }
        }

        #if defined(_WIN32)
            const char* base = ::getenv("LOCALAPPDATA");
            if (!base || !*base) {
                base = ::getenv("APPDATA");
            }
            if (base && *base) {
                return JoinPath(
                    JoinPath(
                        JoinPath(base, "Nkentseu"),
                        "NKWindow"
                    ),
                    "GamepadMappings"
                );
            }
        #else
            if (const char* home = ::getenv("HOME")) {
                if (*home) {
                    return JoinPath(
                        JoinPath(home, ".config"),
                        "nkentseu/nkwindow/gamepad_mappings"
                    );
                }
            }
        #endif

        return "Build/GamepadMappings";
    }

    // -------------------------------------------------------------------------
    // Méthode statique : ResolveCurrentUserId
    // -------------------------------------------------------------------------
    // Description :
    //   Résout l'identifiant utilisateur courant pour le stockage des profils.
    // Priorité de résolution :
    //   1. Variable d'environnement NKENTSEU_GAMEPAD_USER (si définie).
    //   2. Windows : variable %USERNAME%
    //   3. POSIX : variable $USER
    //   4. Fallback : "default"
    // Sécurité :
    //   - Le résultat est automatiquement sanitizé via SanitizeUserId().
    // Retour :
    //   - Identifiant utilisateur sanitizé et sûr pour les chemins de fichier.
    // -------------------------------------------------------------------------
    NkString NkTextGamepadMappingPersistence::ResolveCurrentUserId() {
        if (const char* env = ::getenv("NKENTSEU_GAMEPAD_USER")) {
            if (*env) {
                return SanitizeUserId(env);
            }
        }
        #if defined(_WIN32)
            if (const char* user = ::getenv("USERNAME")) {
                if (*user) {
                    return SanitizeUserId(user);
                }
            }
        #else
            if (const char* user = ::getenv("USER")) {
                if (*user) {
                    return SanitizeUserId(user);
                }
            }
        #endif
        return "default";
    }

    // -------------------------------------------------------------------------
    // Méthode statique : SanitizeUserId
    // -------------------------------------------------------------------------
    // Description :
    //   Nettoie une chaîne d'identifiant utilisateur pour un usage sécurisé 
    //   dans les chemins de fichier, en remplaçant les caractères dangereux.
    // Paramètres :
    //   - raw : Chaîne brute de l'identifiant utilisateur.
    // Retour :
    //   - Chaîne sanitizée contenant uniquement : alphanumérique, '_', '-', '.'
    //   - Retourne "default" si le résultat serait vide.
    // Règles de transformation :
    //   - Caractères autorisés : [a-zA-Z0-9_.-]
    //   - Tout autre caractère est remplacé par '_'.
    //   - Chaîne vide en entrée ou résultat → "default".
    // Sécurité :
    //   - Prévention contre l'injection de chemins (../, \, etc.).
    //   - Compatibilité avec tous les systèmes de fichiers courants.
    // -------------------------------------------------------------------------
    NkString NkTextGamepadMappingPersistence::SanitizeUserId(const NkString& raw) {
        if (raw.Empty()) {
            return "default";
        }
        NkString out;
        out.Reserve(raw.Size());
        for (char c : raw) {
            if (encoding::ascii::NkIsAlphaNumeric(c)
                || c == '_'
                || c == '-'
                || c == '.') {
                out.PushBack(c);
            } else {
                out.PushBack('_');
            }
        }
        if (out.Empty()) {
            out = "default";
        }
        return out;
    }

    // -------------------------------------------------------------------------
    // Méthode privée : BuildUserFilePath
    // -------------------------------------------------------------------------
    // Description :
    //   Construit le chemin complet du fichier de profil pour un utilisateur.
    // Paramètres :
    //   - userId : Identifiant utilisateur brut (vide = résolution automatique).
    // Retour :
    //   - Chemin complet : <baseDirectory>/<sanitizedUserId><extension>
    // Étapes :
    //   1. Sanitisation de l'userId (ou résolution du courant si vide).
    //   2. Concaténation avec le répertoire de base et l'extension.
    // Notes :
    //   - Méthode const : n'affecte pas l'état de l'objet.
    //   - Utilisée par Save() et Load() pour localiser les fichiers.
    // -------------------------------------------------------------------------
    NkString NkTextGamepadMappingPersistence::BuildUserFilePath(const NkString& userId) const {
        const NkString id = SanitizeUserId(
            userId.Empty() ? ResolveCurrentUserId() : userId
        );
        return JoinPath(mBaseDirectory, id + mExtension);
    }

    // -------------------------------------------------------------------------
    // Méthode : Save
    // -------------------------------------------------------------------------
    // Description :
    //   Sérialise un profil de mapping gamepad vers un fichier texte.
    // Paramètres :
    //   - userId  : Identifiant utilisateur pour la localisation du fichier.
    //   - profile : Données du profil à sauvegarder.
    //   - outError : Pointeur optionnel pour message d'erreur détaillé.
    // Retour :
    //   - true si la sauvegarde a réussi.
    //   - false en cas d'erreur (avec message dans outError si fourni).
    // Format de sortie :
    //   - En-tête : "nkmap <version>" + "backend <nom>"
    //   - Blocs slot : "slot <index> <active>" ... entrées ... "end_slot"
    //   - Boutons : "button <physique> <logique>"
    //   - Axes : "axis <physique> <logique> <scale> <invert>"
    // Gestion des erreurs :
    //   - Création des répertoires intermédiaires si nécessaire.
    //   - Fermeture propre du fichier même en cas d'échec d'écriture.
    //   - Messages d'erreur explicites pour débogage.
    // -------------------------------------------------------------------------
    bool NkTextGamepadMappingPersistence::Save(
        const NkString& userId,
        const NkGamepadMappingProfileData& profile,
        NkString* outError
    ) {
        if (!EnsureDirectories(mBaseDirectory)) {
            if (outError) {
                *outError = "Cannot create mapping directory: " + mBaseDirectory;
            }
            return false;
        }

        const NkString filePath = BuildUserFilePath(userId);
        FILE* out = ::fopen(filePath.CStr(), "wb");
        if (!out) {
            if (outError) {
                *outError = "Cannot open mapping file for write: " + filePath;
            }
            return false;
        }

        if (::fprintf(out, "nkmap %u\n", static_cast<unsigned>(profile.version)) < 0
            || ::fprintf(out, "backend %s\n", profile.backendName.CStr()) < 0) {
            ::fclose(out);
            if (outError) {
                *outError = "Write failed for mapping file: " + filePath;
            }
            return false;
        }

        for (const NkGamepadMappingSlotData& slot : profile.slots) {
            if (::fprintf(out, "slot %u %d\n",
                    static_cast<unsigned>(slot.slotIndex),
                    slot.active ? 1 : 0) < 0) {
                ::fclose(out);
                if (outError) {
                    *outError = "Write failed for mapping file: " + filePath;
                }
                return false;
            }

            for (const NkGamepadButtonMapEntry& b : slot.buttons) {
                if (::fprintf(out, "button %u %u\n",
                        static_cast<unsigned>(b.physicalButton),
                        static_cast<unsigned>(b.logicalButton)) < 0) {
                    ::fclose(out);
                    if (outError) {
                        *outError = "Write failed for mapping file: " + filePath;
                    }
                    return false;
                }
            }

            for (const NkGamepadAxisMapEntry& a : slot.axes) {
                if (::fprintf(out, "axis %u %u %.9g %d\n",
                        static_cast<unsigned>(a.physicalAxis),
                        static_cast<unsigned>(a.logicalAxis),
                        static_cast<double>(a.scale),
                        a.invert ? 1 : 0) < 0) {
                    ::fclose(out);
                    if (outError) {
                        *outError = "Write failed for mapping file: " + filePath;
                    }
                    return false;
                }
            }

            if (::fprintf(out, "end_slot\n") < 0) {
                ::fclose(out);
                if (outError) {
                    *outError = "Write failed for mapping file: " + filePath;
                }
                return false;
            }
        }

        if (::fflush(out) != 0 || ::fclose(out) != 0) {
            if (outError) {
                *outError = "Write failed for mapping file: " + filePath;
            }
            return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Méthode : Load
    // -------------------------------------------------------------------------
    // Description :
    //   Désérialise un profil de mapping gamepad depuis un fichier texte.
    // Paramètres :
    //   - userId     : Identifiant utilisateur pour la localisation du fichier.
    //   - outProfile : Référence pour stocker le profil chargé.
    //   - outError   : Pointeur optionnel pour message d'erreur détaillé.
    // Retour :
    //   - true si le chargement a réussi.
    //   - false en cas d'erreur (fichier absent, format invalide...).
    // Parsing :
    //   - Lecture ligne par ligne avec tokenisation par espaces.
    //   - Reconnaissance des commandes : backend, slot, button, axis, end_slot.
    //   - Validation stricte du nombre et type de tokens attendus.
    // Gestion d'état :
    //   - currentSlot pointe vers le slot en cours de remplissage.
    //   - Les entrées button/axis hors d'un slot sont ignorées.
    // Robustesse :
    //   - Réinitialisation de outProfile avant chargement.
    //   - Fermeture garantie du fichier même en cas d'erreur de parsing.
    // -------------------------------------------------------------------------
    bool NkTextGamepadMappingPersistence::Load(
        const NkString& userId,
        NkGamepadMappingProfileData& outProfile,
        NkString* outError
    ) {
        const NkString filePath = BuildUserFilePath(userId);
        FILE* in = ::fopen(filePath.CStr(), "rb");
        if (!in) {
            if (outError) {
                *outError = "Cannot open mapping file for read: " + filePath;
            }
            return false;
        }

        outProfile = {};

        NkString line;
        if (!ReadTextLine(in, line)) {
            ::fclose(in);
            if (outError) {
                *outError = "Invalid mapping file header: " + filePath;
            }
            return false;
        }
        line.Trim();

        NkVector<NkString> tokens;
        TokenizeWhitespace(line, tokens);
        uint32 version = 0;
        if (tokens.Size() < 2
            || tokens[0].Compare("nkmap") != 0
            || !ParseUIntToken(tokens[1], version)) {
            ::fclose(in);
            if (outError) {
                *outError = "Invalid mapping file header: " + filePath;
            }
            return false;
        }
        outProfile.version = version;

        NkGamepadMappingSlotData* currentSlot = nullptr;

        while (ReadTextLine(in, line)) {
            line.Trim();
            if (line.Empty()) {
                continue;
            }

            TokenizeWhitespace(line, tokens);
            if (tokens.Empty()) {
                continue;
            }

            const NkString& token = tokens[0];
            if (token == "backend") {
                outProfile.backendName = SliceAfterFirstToken(line);
            } else if (token == "slot") {
                if (tokens.Size() < 3) {
                    ::fclose(in);
                    if (outError) {
                        *outError = "Invalid slot entry in mapping file: " + filePath;
                    }
                    return false;
                }

                NkGamepadMappingSlotData slot{};
                int32 active = 0;
                if (!ParseUIntToken(tokens[1], slot.slotIndex)
                    || !ParseIntToken(tokens[2], active)) {
                    ::fclose(in);
                    if (outError) {
                        *outError = "Invalid slot entry in mapping file: " + filePath;
                    }
                    return false;
                }
                slot.active = (active != 0);
                outProfile.slots.PushBack(slot);
                currentSlot = &outProfile.slots.Back();
            } else if (token == "button") {
                if (!currentSlot) {
                    continue;
                }
                if (tokens.Size() < 3) {
                    ::fclose(in);
                    if (outError) {
                        *outError = "Invalid button entry in mapping file: " + filePath;
                    }
                    return false;
                }

                NkGamepadButtonMapEntry e{};
                if (!ParseUIntToken(tokens[1], e.physicalButton)
                    || !ParseUIntToken(tokens[2], e.logicalButton)) {
                    ::fclose(in);
                    if (outError) {
                        *outError = "Invalid button entry in mapping file: " + filePath;
                    }
                    return false;
                }
                currentSlot->buttons.PushBack(e);
            } else if (token == "axis") {
                if (!currentSlot) {
                    continue;
                }
                if (tokens.Size() < 5) {
                    ::fclose(in);
                    if (outError) {
                        *outError = "Invalid axis entry in mapping file: " + filePath;
                    }
                    return false;
                }

                NkGamepadAxisMapEntry e{};
                int32 invert = 0;
                if (!ParseUIntToken(tokens[1], e.physicalAxis)
                    || !ParseUIntToken(tokens[2], e.logicalAxis)
                    || !ParseFloatToken(tokens[3], e.scale)
                    || !ParseIntToken(tokens[4], invert)) {
                    ::fclose(in);
                    if (outError) {
                        *outError = "Invalid axis entry in mapping file: " + filePath;
                    }
                    return false;
                }
                e.invert = (invert != 0);
                currentSlot->axes.PushBack(e);
            } else if (token == "end_slot") {
                currentSlot = nullptr;
            }
        }

        ::fclose(in);
        return true;
    }

} // namespace nkentseu