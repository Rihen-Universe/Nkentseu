// =============================================================================
// NKLogger/NkLogMessage.cpp
// Implémentation de la structure NkLogMessage.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Implémentations déterministes sans allocation dynamique imprévue
//  - Gestion multiplateforme du temps haute résolution
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/NkLogMessage.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

#include "NKThreading/NkThread.h"

#include <ctime>

#if !defined(_WIN32)
    #include <time.h>
#else
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Fonctions et constantes à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation et optimisation.

namespace {


    // -------------------------------------------------------------------------
    // FONCTION : NkGetNowNs
    // DESCRIPTION : Obtention du timestamp courant en nanosecondes depuis epoch
    // RETURN : uint64 représentant les nanosecondes depuis Unix epoch (UTC)
    // NOTE : Implémentation multiplateforme avec fallback sécurisé
    // -------------------------------------------------------------------------
    static inline nkentseu::uint64 NkGetNowNs() {
        #if defined(_WIN32)
            // Windows : utilisation de GetSystemTimeAsFileTime (précision ~100ns)
            FILETIME fileTime{};
            ::GetSystemTimeAsFileTime(&fileTime);

            // Conversion FILETIME (100-nanosecond intervals since 1601) → Unix epoch
            ULARGE_INTEGER ticks{};
            ticks.LowPart = fileTime.dwLowDateTime;
            ticks.HighPart = fileTime.dwHighDateTime;

            // Offset entre Windows epoch (1601) et Unix epoch (1970) en nanosecondes
            constexpr nkentseu::uint64 kWinToUnixEpochOffsetNs = 11644473600ULL * 1000000000ULL;

            // Conversion : FILETIME est en unités de 100ns → multiplication par 100
            const nkentseu::uint64 nowNs = ticks.QuadPart * 100ULL;

            // Soustraction de l'offset avec protection contre underflow
            return (nowNs >= kWinToUnixEpochOffsetNs)
                ? (nowNs - kWinToUnixEpochOffsetNs)
                : 0ULL;

        #else
            // POSIX : utilisation de clock_gettime avec CLOCK_REALTIME
            timespec ts{};

            // Appel système : retourne 0 en cas de succès, -1 en cas d'erreur
            if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
                // Combinaison secondes + nanosecondes en uint64 unique
                return (static_cast<nkentseu::uint64>(ts.tv_sec) * 1000000000ULL)
                    + static_cast<nkentseu::uint64>(ts.tv_nsec);
            }

            // Fallback en cas d'erreur système : retourne 0 (timestamp invalide)
            return 0;
        #endif
    }


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques de NkLogMessage.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


    // -------------------------------------------------------------------------
    // MÉTHODE : Constructeur par défaut
    // DESCRIPTION : Initialise un message avec valeurs neutres et timestamp courant
    // -------------------------------------------------------------------------
    NkLogMessage::NkLogMessage()
        : timestamp(0)
        , threadId(0)
        , sourceLine(0)
        , level(NkLogLevel::NK_INFO) {

        // Acquisition du timestamp haute résolution au moment de la construction
        timestamp = static_cast<uint64>(NkGetNowNs());

        // Acquisition de l'ID du thread courant via l'abstraction platforme
        threadId = static_cast<uint32>(threading::NkThread::GetCurrentThreadId());
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : Constructeur avec niveau et message
    // DESCRIPTION : Initialisation minimale avec délégation au constructeur par défaut
    // -------------------------------------------------------------------------
    NkLogMessage::NkLogMessage(NkLogLevel lvl, const NkString& msg, const NkString& logger)
        : NkLogMessage() {

        // Affectation des paramètres fournis par l'appelant
        level = lvl;
        message = msg;

        // Affectation conditionnelle du nom de logger (évite copie si vide)
        if (!logger.Empty()) {
            loggerName = logger;
        }
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : Constructeur complet avec métadonnées de source
    // DESCRIPTION : Initialisation détaillée avec délégation au constructeur à 3 paramètres
    // -------------------------------------------------------------------------
    NkLogMessage::NkLogMessage(
        NkLogLevel lvl,
        const NkString& msg,
        const NkString& file,
        uint32 line,
        const NkString& func,
        const NkString& logger
    ) : NkLogMessage(lvl, msg, logger) {

        // Affectation conditionnelle du fichier source (évite copie si vide)
        if (!file.Empty()) {
            sourceFile = file;
        }

        // Affectation de la ligne source uniquement si valeur significative (> 0)
        if (line > 0) {
            sourceLine = line;
        }

        // Affectation conditionnelle du nom de fonction (évite copie si vide)
        if (!func.Empty()) {
            functionName = func;
        }
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : Reset
    // DESCRIPTION : Réinitialisation complète aux valeurs par défaut du constructeur
    // -------------------------------------------------------------------------
    void NkLogMessage::Reset() {
        // Réinitialisation des champs scalaires
        timestamp = 0;
        threadId = 0;
        sourceLine = 0;
        level = NkLogLevel::NK_INFO;

        // Réinitialisation des champs NkString via Clear() (libération mémoire si nécessaire)
        threadName.Clear();
        message.Clear();
        loggerName.Clear();
        sourceFile.Clear();
        functionName.Clear();

        // Recalcul du timestamp au moment de la réinitialisation
        timestamp = static_cast<uint64>(NkGetNowNs());

        // Recalcul de l'ID du thread courant
        threadId = static_cast<uint32>(threading::NkThread::GetCurrentThreadId());
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : IsValid
    // DESCRIPTION : Vérification des critères minimaux de validité du message
    // RETURN : true si message non vide ET timestamp valide, false sinon
    // -------------------------------------------------------------------------
    bool NkLogMessage::IsValid() const {
        // Critère 1 : le contenu du message ne doit pas être vide
        const bool hasContent = !message.Empty();

        // Critère 2 : le timestamp doit être strictement positif (époque valide)
        const bool hasTimestamp = (timestamp > 0);

        // Les deux critères doivent être satisfaits pour considérer le message valide
        return hasContent && hasTimestamp;
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : GetLocalTime
    // DESCRIPTION : Conversion du timestamp en structure tm locale thread-safe
    // RETURN : Copie de la structure tm avec date/heure locale du système
    // -------------------------------------------------------------------------
    tm NkLogMessage::GetLocalTime() const {
        // Conversion nanosecondes → secondes pour compatibilité avec time_t
        const time_t timeSeconds = static_cast<time_t>(timestamp / 1000000000ULL);

        // Structure de résultat initialisée à zéro pour sécurité
        tm localTime{};

        #ifdef _WIN32
            // Windows : utilisation de localtime_s (version thread-safe de localtime)
            // Retourne 0 en cas de succès, errno en cas d'erreur (ignoré ici)
            localtime_s(&localTime, &timeSeconds);
        #else
            // POSIX : utilisation de localtime_r (version thread-safe de localtime)
            // Retourne un pointeur vers la structure remplie, ou nullptr en cas d'erreur
            localtime_r(&timeSeconds, &localTime);
        #endif

        // Retour de la structure par copie : safe pour usage prolongé par le caller
        return localTime;
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : GetUTCTime
    // DESCRIPTION : Conversion du timestamp en structure tm UTC thread-safe
    // RETURN : Copie de la structure tm avec date/heure UTC (indépendante du fuseau)
    // -------------------------------------------------------------------------
    tm NkLogMessage::GetUTCTime() const {
        // Conversion nanosecondes → secondes pour compatibilité avec time_t
        const time_t timeSeconds = static_cast<time_t>(timestamp / 1000000000ULL);

        // Structure de résultat initialisée à zéro pour sécurité
        tm utcTime{};

        #ifdef _WIN32
            // Windows : utilisation de gmtime_s (version thread-safe de gmtime)
            gmtime_s(&utcTime, &timeSeconds);
        #else
            // POSIX : utilisation de gmtime_r (version thread-safe de gmtime)
            gmtime_r(&timeSeconds, &utcTime);
        #endif

        // Retour de la structure par copie : safe pour usage prolongé par le caller
        return utcTime;
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : GetMillis
    // DESCRIPTION : Conversion du timestamp en millisecondes depuis epoch
    // RETURN : uint64 représentant les millisecondes (perte de précision sub-ms)
    // -------------------------------------------------------------------------
    uint64 NkLogMessage::GetMillis() const {
        // Division entière : 1 milliseconde = 1 000 000 nanosecondes
        return timestamp / 1000000ULL;
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : GetMicros
    // DESCRIPTION : Conversion du timestamp en microsecondes depuis epoch
    // RETURN : uint64 représentant les microsecondes (perte de précision sub-µs)
    // -------------------------------------------------------------------------
    uint64 NkLogMessage::GetMicros() const {
        // Division entière : 1 microseconde = 1 000 nanosecondes
        return timestamp / 1000ULL;
    }


    // -------------------------------------------------------------------------
    // MÉTHODE : GetSeconds
    // DESCRIPTION : Conversion du timestamp en secondes depuis epoch avec fraction
    // RETURN : double représentant les secondes avec précision nanoseconde conservée
    // -------------------------------------------------------------------------
    double NkLogMessage::GetSeconds() const {
        // Conversion en double pour préserver la fraction sub-seconde
        // Division par 1e9 (nanosecondes par seconde) en précision flottante
        return static_cast<double>(timestamp) / 1000000000.0;
    }

} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    1. GESTION DU TEMPS HAUTE RÉSOLUTION :
       - NkGetNowNs() utilise les APIs natives les plus précises disponibles
       - Windows : GetSystemTimeAsFileTime (~100ns de précision réelle)
       - POSIX : clock_gettime(CLOCK_REALTIME) (~1ns de précision théorique)
       - Fallback vers 0 en cas d'erreur système : message marqué comme invalide

    2. THREAD-SAFETY DES CONVERSIONS TEMPORELLES :
       - localtime_s/gmtime_s (Windows) et localtime_r/gmtime_r (POSIX) sont thread-safe
       - Retour par copie de la structure tm : pas de pointeur vers mémoire statique
       - Safe pour appel concurrent depuis multiples threads de logging

    3. OPTIMISATION DES CONSTRUCTEURS :
       - Delegation constructor pour éviter la duplication de code d'initialisation
       - Affectations conditionnelles pour éviter les copies inutiles de NkString
       - Initialisation des membres dans l'ordre de déclaration pour éviter warnings

    4. GESTION MÉMOIRE DES NkString :
       - Clear() libère la mémoire si capacité > seuil (dépend de l'implémentation)
       - Copy-on-write possible selon configuration de NkString : vérifier avec l'équipe
       - Pour pooling intensif : envisager NkString avec allocator personnalisé

    5. COMPATIBILITÉ MULTIPLATEFORME :
       - Macros conditionnelles pour isolation des APIs Windows vs POSIX
       - Types uint64/uint32 pour portabilité des tailles quel que soit le compilateur
       - Aucune dépendance à des extensions compiler-specific non portables
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
