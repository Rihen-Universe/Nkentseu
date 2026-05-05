// =============================================================================
// NKPlatform/NkFoundationLog.h
// Système de logging ultra-léger pour la couche Foundation.
//
// Design :
//  - Logging minimaliste sans dépendance à NKLogger (couche basse niveau)
//  - Support multi-plateforme : stdout, stderr, Android logcat
//  - Niveaux de log configurables à la compilation (ERROR/WARN/INFO/DEBUG/TRACE)
//  - Sink utilisateur optionnel pour rediriger les logs vers un backend personnalisé
//  - Formatage de valeurs extensible via ADL (Argument-Dependent Lookup)
//  - Macros ergonomiques pour un usage simplifié dans le code client
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKFOUNDATIONLOG_H
#define NKENTSEU_PLATFORM_NKFOUNDATIONLOG_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de détection de plateforme et des macros d'export.
    // NkPlatformDetect.h fournit les macros NKENTSEU_PLATFORM_*.
    // NkPlatformExport.h fournit NKENTSEU_PLATFORM_API pour l'export de symboles.
    // NkPlatformInline.h fournit NKENTSEU_API_INLINE pour les fonctions inline exportées.

    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    #include <stddef.h>
    #include <stdio.h>
    #include <string.h>

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉTECTION DU SUPPORT ANDROID LOG
    // -------------------------------------------------------------------------
    // Détection conditionnelle de la disponibilité de <android/log.h>.
    // Cette détection utilise __has_include si disponible, sinon des macros de plateforme.

    /**
     * @brief Indicateur de disponibilité d'Android Log
     * @def NK_FOUNDATION_HAS_ANDROID_LOG
     * @ingroup FoundationLog
     * @value 1 si android/log.h est disponible, 0 sinon
     *
     * Permet d'activer le backend logcat sur Android tout en conservant
     * la portabilité vers d'autres plateformes via fprintf(stderr).
     */

    #if !defined(NK_FOUNDATION_HAS_ANDROID_LOG)
        #if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
            #define NK_FOUNDATION_HAS_ANDROID_LOG 1
        #elif defined(__has_include)
            #if __has_include(<android/log.h>)
                #define NK_FOUNDATION_HAS_ANDROID_LOG 1
            #else
                #define NK_FOUNDATION_HAS_ANDROID_LOG 0
            #endif
        #else
            #define NK_FOUNDATION_HAS_ANDROID_LOG 0
        #endif
    #endif

    // -------------------------------------------------------------------------
    // SECTION 3 : INCLUSION CONDITIONNELLE DE android/log.h
    // -------------------------------------------------------------------------
    // Gestion robuste de l'inclusion de android/log.h avec sauvegarde/restauration
    // de la macro 'logger' qui peut entrer en conflit avec d'autres bibliothèques.

    #if NK_FOUNDATION_HAS_ANDROID_LOG
        // Sauvegarde de la macro 'logger' si elle existe (conflit potentiel)
        #if defined(logger)
            #pragma push_macro("logger")
            #undef logger
            #define NK_FOUNDATION_RESTORE_LOGGER_MACRO
        #endif

        #include <android/log.h>

        // Restauration de la macro 'logger' après inclusion
        #if defined(NK_FOUNDATION_RESTORE_LOGGER_MACRO)
            #pragma pop_macro("logger")
            #undef NK_FOUNDATION_RESTORE_LOGGER_MACRO
        #endif
    #endif

    // =========================================================================
    // SECTION 4 : NIVEAUX DE LOG ET CONFIGURATION
    // =========================================================================
    // Définition des niveaux de log disponibles et du niveau actif à la compilation.
    // Le niveau actif contrôle quelles macros de log génèrent du code.

    /**
     * @brief Niveau de log : aucun log
     * @def NK_FOUNDATION_LOG_LEVEL_NONE
     * @value 0
     * @ingroup FoundationLogLevels
     */
    #define NK_FOUNDATION_LOG_LEVEL_NONE  0

    /**
     * @brief Niveau de log : erreurs critiques
     * @def NK_FOUNDATION_LOG_LEVEL_ERROR
     * @value 1
     * @ingroup FoundationLogLevels
     */
    #define NK_FOUNDATION_LOG_LEVEL_ERROR 1

    /**
     * @brief Niveau de log : avertissements
     * @def NK_FOUNDATION_LOG_LEVEL_WARN
     * @value 2
     * @ingroup FoundationLogLevels
     */
    #define NK_FOUNDATION_LOG_LEVEL_WARN  2

    /**
     * @brief Niveau de log : informations générales
     * @def NK_FOUNDATION_LOG_LEVEL_INFO
     * @value 3
     * @ingroup FoundationLogLevels
     */
    #define NK_FOUNDATION_LOG_LEVEL_INFO  3

    /**
     * @brief Niveau de log : messages de debug
     * @def NK_FOUNDATION_LOG_LEVEL_DEBUG
     * @value 4
     * @ingroup FoundationLogLevels
     */
    #define NK_FOUNDATION_LOG_LEVEL_DEBUG 4

    /**
     * @brief Niveau de log : traces détaillées (très verbeux)
     * @def NK_FOUNDATION_LOG_LEVEL_TRACE
     * @value 5
     * @ingroup FoundationLogLevels
     */
    #define NK_FOUNDATION_LOG_LEVEL_TRACE 5

    /**
     * @brief Niveau de log actif à la compilation
     * @def NK_FOUNDATION_LOG_LEVEL
     * @ingroup FoundationLogConfig
     *
     * Par défaut :
     *  - DEBUG en mode debug (_DEBUG, DEBUG, NKENTSEU_DEBUG définis)
     *  - WARN en mode release
     *
     * Peut être redéfini avant l'inclusion de ce fichier pour un contrôle fin.
     *
     * @example
     * @code
     * #define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_TRACE
     * #include "NKPlatform/NkFoundationLog.h"
     * @endcode
     */
    #ifndef NK_FOUNDATION_LOG_LEVEL
        #if defined(_DEBUG) || defined(DEBUG) || defined(NKENTSEU_DEBUG)
            #define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_DEBUG
        #else
            #define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_WARN
        #endif
    #endif

    // =========================================================================
    // SECTION 5 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // =========================================================================
    // Déclaration de l'espace de noms principal nkentseu::platform.
    // Les symboles publics sont exportés via NKENTSEU_PLATFORM_API.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Le contenu de 'platform' (dans 'nkentseu') est indenté de deux niveaux
    //  - Le contenu de 'detail' (dans 'platform') est indenté de trois niveaux

    namespace nkentseu {

        namespace platform {

            // -----------------------------------------------------------------
            // Sous-section : Type de fonction pour le sink de log utilisateur
            // -----------------------------------------------------------------
            // Signature du callback permettant à l'utilisateur de rediriger les logs
            // vers un backend personnalisé (fichier, réseau, UI, etc.).

            /**
             * @brief Type de fonction pour le sink de log personnalisé
             * @typedef NkFoundationLogSink
             * @param level Niveau de log sous forme de chaîne ("ERR", "WRN", "INF", etc.)
             * @param file Nom du fichier source où le log a été émis
             * @param line Numéro de ligne dans le fichier source
             * @param message Message de log formaté
             * @ingroup FoundationLogAPI
             *
             * @example
             * @code
             * void MyCustomSink(const char* level, const char* file, int line, const char* message) {
             *     // Envoyer vers un serveur de logs, écrire dans un fichier, etc.
             *     fprintf(myLogFile, "[%s] %s:%d %s\n", level, file, line, message);
             * }
             *
             * NkFoundationSetLogSink(MyCustomSink);
             * @endcode
             */
            using NkFoundationLogSink = void (*)(
                const char* level,
                const char* file,
                int line,
                const char* message
            );

            // -----------------------------------------------------------------
            // Sous-section : Variable globale pour le sink de log
            // -----------------------------------------------------------------
            // Pointeur vers le sink personnalisé. nullptr utilise le backend par défaut.
            // Exporté via NKENTSEU_PLATFORM_API pour un accès depuis les DLL.

            /**
             * @brief Sink de log global configuré par l'utilisateur
             * @var gNkFoundationLogSink
             * @ingroup FoundationLogInternals
             *
             * Ne pas modifier directement. Utiliser NkFoundationSetLogSink().
             */
            NKENTSEU_PLATFORM_API extern NkFoundationLogSink gNkFoundationLogSink;

            // Initialisation inline de la variable globale
            inline NkFoundationLogSink gNkFoundationLogSink = nullptr;

            // -----------------------------------------------------------------
            // Sous-section : Fonctions d'accès au sink de log (API publique)
            // -----------------------------------------------------------------
            // Fonctions inline exportées pour définir et récupérer le sink personnalisé.
            //
            // NOTE SUR L'EXPORT DES FONCTIONS INLINE :
            //  - Pour les bibliothèques partagées (DLL/.so), les fonctions inline
            //    avec NKENTSEU_PLATFORM_API peuvent causer des problèmes de linkage.
            //  - Solution recommandée : utiliser NKENTSEU_API_INLINE uniquement
            //    pour les fonctions critiques, ou désactiver l'export en mode header-only.
            //  - En mode statique (NKENTSEU_STATIC_LIB), NKENTSEU_PLATFORM_API est vide,
            //    donc aucun problème.

            /**
             * @brief Définir un sink de log personnalisé
             * @param sink Pointeur vers la fonction de callback, ou nullptr pour le défaut
             * @ingroup FoundationLogAPI
             *
             * Thread-safe : l'assignation atomique d'un pointeur est garantie sur toutes
             * les plateformes supportées. Pour une configuration thread-safe avancée,
             * synchroniser l'appel à cette fonction.
             *
             * @example
             * @code
             * // Définir un sink personnalisé au démarrage de l'application
             * NkFoundationSetLogSink(MyFileLogger);
             *
             * // Réinitialiser vers le backend par défaut
             * NkFoundationSetLogSink(nullptr);
             * @endcode
             */
            NKENTSEU_API_INLINE void NkFoundationSetLogSink(NkFoundationLogSink sink) {
                gNkFoundationLogSink = sink;
            }

            /**
             * @brief Récupérer le sink de log actuellement configuré
             * @return Pointeur vers le callback personnalisé, ou nullptr si non configuré
             * @ingroup FoundationLogAPI
             *
             * Utile pour sauvegarder/restaurer un sink temporaire ou pour du debugging.
             */
            NKENTSEU_API_INLINE NkFoundationLogSink NkFoundationGetLogSink() {
                return gNkFoundationLogSink;
            }

            // =================================================================
            // SECTION 6 : ESPACE DE NOMS INTERNE (DETAIL) - INDENTATION +3
            // =================================================================
            // Fonctions et templates d'implémentation interne. Non destinés à un usage direct.
            // Indentés d'un niveau supplémentaire car dans le namespace 'detail'.

            namespace detail {

                // -----------------------------------------------------------------
                // Sous-section : Conversion niveau de log -> priorité Android
                // -----------------------------------------------------------------
                // Fonction utilitaire pour mapper nos niveaux de log vers les priorités
                // d'Android Logcat (VERBOSE, DEBUG, INFO, WARN, ERROR, etc.).

                #if NK_FOUNDATION_HAS_ANDROID_LOG
                    /**
                     * @brief Convertir un niveau de log en priorité Android
                     * @param level Chaîne de niveau ("TRC", "DBG", "WRN", "ERR", "INF")
                     * @return Priorité android_log_t correspondante
                     * @ingroup FoundationLogInternals
                     */
                    NKENTSEU_API_INLINE int NkFoundationAndroidPriority(const char* level) {
                        if (!level) {
                            return ANDROID_LOG_INFO;
                        }
                        if (strcmp(level, "TRC") == 0) {
                            return ANDROID_LOG_VERBOSE;
                        }
                        if (strcmp(level, "DBG") == 0) {
                            return ANDROID_LOG_DEBUG;
                        }
                        if (strcmp(level, "WRN") == 0) {
                            return ANDROID_LOG_WARN;
                        }
                        if (strcmp(level, "ERR") == 0) {
                            return ANDROID_LOG_ERROR;
                        }
                        return ANDROID_LOG_INFO;
                    }
                #endif

                // -----------------------------------------------------------------
                // Sous-section : Fonction d'écriture de log (version simple)
                // -----------------------------------------------------------------
                // Version non-template pour les messages déjà formatés.
                // Gère le sink personnalisé, Android logcat, ou fprintf(stderr).

                /**
                 * @brief Écrire un message de log déjà formaté
                 * @param level Niveau de log ("ERR", "WRN", "INF", "DBG", "TRC")
                 * @param file Fichier source d'origine
                 * @param line Ligne source d'origine
                 * @param message Message déjà formaté (sans format string)
                 * @ingroup FoundationLogInternals
                 */
                NKENTSEU_API_INLINE void NkFoundationLogWrite(
                    const char* level,
                    const char* file,
                    int line,
                    const char* message
                ) {
                    if (!message) {
                        return;
                    }

                    char payload[1024];
                    const int payloadLen = snprintf(payload, sizeof(payload), "%s", message);

                    if (payloadLen <= 0) {
                        return;
                    }

                    // Priorité au sink personnalisé si configuré
                    if (const NkFoundationLogSink sink = ::nkentseu::platform::NkFoundationGetLogSink()) {
                        sink(level, file, line, payload);
                        return;
                    }

                    // Backend par défaut : Android logcat ou stderr
                    #if NK_FOUNDATION_HAS_ANDROID_LOG
                        __android_log_print(
                            NkFoundationAndroidPriority(level),
                            "NkFoundation",
                            "[%s] %s:%d %s",
                            level ? level : "INF",
                            file ? file : "<unknown>",
                            line,
                            payload
                        );
                    #else
                        fprintf(stderr, "[%s] %s:%d %s\n", level, file ? file : "<unknown>", line, payload);
                    #endif
                }

                // -----------------------------------------------------------------
                // Sous-section : Fonction d'écriture de log (version template avec format)
                // -----------------------------------------------------------------
                // Version template acceptant une format string et des arguments variables.
                // Utilise snprintf pour le formatage sécurisé dans un buffer fixe.

                /**
                 * @brief Écrire un message de log avec formatage printf-style
                 * @tparam Args Types des arguments de formatage
                 * @param level Niveau de log ("ERR", "WRN", "INF", "DBG", "TRC")
                 * @param file Fichier source d'origine
                 * @param line Ligne source d'origine
                 * @param fmt Chaîne de format style printf
                 * @param args Arguments à formatter
                 * @ingroup FoundationLogInternals
                 *
                 * @note Le buffer de formatage est limité à 1024 octets.
                 * Les messages plus longs seront tronqués.
                 */
                template <typename... Args>
                NKENTSEU_API_INLINE void NkFoundationLogWrite(
                    const char* level,
                    const char* file,
                    int line,
                    const char* fmt,
                    Args... args
                ) {
                    if (!fmt) {
                        return;
                    }

                    char payload[1024];
                    const int payloadLen = snprintf(payload, sizeof(payload), fmt, args...);

                    if (payloadLen <= 0) {
                        return;
                    }

                    // Priorité au sink personnalisé si configuré
                    if (const NkFoundationLogSink sink = ::nkentseu::platform::NkFoundationGetLogSink()) {
                        sink(level, file, line, payload);
                        return;
                    }

                    // Backend par défaut : Android logcat ou stderr
                    #if NK_FOUNDATION_HAS_ANDROID_LOG
                        __android_log_print(
                            NkFoundationAndroidPriority(level),
                            "NkFoundation",
                            "[%s] %s:%d %s",
                            level ? level : "INF",
                            file ? file : "<unknown>",
                            line,
                            payload
                        );
                    #else
                        fprintf(stderr, "[%s] %s:%d %s\n", level, file ? file : "<unknown>", line, payload);
                    #endif
                }

                // -----------------------------------------------------------------
                // SECTION 7 : FORMATAGE DE VALEURS (EXTENSIBLE PAR L'UTILISATEUR)
                // -----------------------------------------------------------------
                // Système de formatage basé sur ADL (Argument-Dependent Lookup).
                // Permet à l'utilisateur de définir NKFoundationToString(T) pour ses types.

                /**
                 * @brief Point d'extension pour le formatage de types personnalisés
                 * @defgroup FoundationLogFormatting Formatage de Valeurs
                 *
                 * Pour rendre un type T loggable, définir dans le même namespace que T :
                 * @code
                 * int NKFoundationToString(const MyType& value, char* out, size_t outSize);
                 * @endcode
                 *
                 * La fonction doit :
                 *  - Écrire la représentation texte de value dans out (taille max outSize)
                 *  - Retourner le nombre de caractères écrits (sans le null terminator)
                 *  - Retourner -1 en cas d'erreur
                 *
                 * Le système de log utilisera automatiquement cette surcharge via ADL.
                 */

                // -------------------------------------------------------------------------
                // Sous-section : Implémentations par défaut pour les types primitifs
                // -------------------------------------------------------------------------
                // Surcharges de NkFoundationFormatValueDefault pour les types built-in.
                // Utilisent snprintf pour un formatage sécurisé et portable.

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    const char* value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%s", value ? value : "");
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    char* value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%s", value ? value : "");
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    bool value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%s", value ? "true" : "false");
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    signed char value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%d", static_cast<int>(value));
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    unsigned char value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%u", static_cast<unsigned int>(value));
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    short value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%d", static_cast<int>(value));
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    unsigned short value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%u", static_cast<unsigned int>(value));
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    int value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%d", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    unsigned int value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%u", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    long value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%ld", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    unsigned long value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%lu", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    long long value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%lld", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    unsigned long long value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%llu", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    float value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%g", static_cast<double>(value));
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    double value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%g", value);
                }

                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    long double value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%Lg", value);
                }

                // Formatage des pointeurs (version générique template)
                template <typename T>
                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    T* value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%p", static_cast<void*>(value));
                }

                template <typename T>
                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    const T* value,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "%p", static_cast<const void*>(value));
                }

                // Fallback pour les types non supportés
                template <typename T>
                NKENTSEU_API_INLINE int NkFoundationFormatValueDefault(
                    const T&,
                    char* out,
                    size_t outSize
                ) {
                    return snprintf(out, outSize, "<unformattable>");
                }

                // -------------------------------------------------------------------------
                // Sous-section : Sélection SFINAE entre extension utilisateur et défaut
                // -------------------------------------------------------------------------
                // Mécanisme ADL : tente d'appeler NKFoundationToString(T) si défini,
                // sinon fallback vers NkFoundationFormatValueDefault(T).

                /**
                 * @brief Implémentation avec détection ADL de NKFoundationToString
                 * @tparam T Type de la valeur à formatter
                 * @param value Valeur à formatter
                 * @param out Buffer de sortie
                 * @param outSize Taille du buffer de sortie
                 * @param priority Paramètre de priorité pour la sélection SFINAE (0 = haute priorité)
                 * @return Nombre de caractères écrits, ou -1 en cas d'erreur
                 * @ingroup FoundationLogFormatting
                 */
                template <typename T>
                NKENTSEU_API_INLINE auto NkFoundationFormatValueImpl(
                    const T& value,
                    char* out,
                    size_t outSize,
                    int priority
                ) -> decltype(NKFoundationToString(value, out, outSize), int()) {
                    // Si NKFoundationToString(T) est trouvé par ADL, l'utiliser
                    return NKFoundationToString(value, out, outSize);
                }

                /**
                 * @brief Fallback vers le formatage par défaut
                 * @tparam T Type de la valeur à formatter
                 * @param value Valeur à formatter
                 * @param out Buffer de sortie
                 * @param outSize Taille du buffer de sortie
                 * @param priority Paramètre de priorité pour la sélection SFINAE (1 = basse priorité)
                 * @return Nombre de caractères écrits par le formatter par défaut
                 * @ingroup FoundationLogFormatting
                 */
                template <typename T>
                NKENTSEU_API_INLINE int NkFoundationFormatValueImpl(
                    const T& value,
                    char* out,
                    size_t outSize,
                    long priority
                ) {
                    // Fallback : utiliser l'implémentation par défaut
                    return NkFoundationFormatValueDefault(value, out, outSize);
                }

                // -------------------------------------------------------------------------
                // Sous-section : Wrapper sécurisé pour le formatage avec gestion des erreurs
                // -------------------------------------------------------------------------
                // Fonction utilitaire qui garantit un buffer null-terminé même en cas d'erreur.

                /**
                 * @brief Formater une valeur avec gestion sécurisée des erreurs
                 * @tparam T Type de la valeur à formatter
                 * @param value Valeur à formatter
                 * @param out Buffer de sortie
                 * @param outSize Taille du buffer de sortie
                 * @return Pointeur vers out (ou chaîne vide si erreur)
                 * @ingroup FoundationLogFormatting
                 *
                 * @note Garantit que out est toujours null-terminé.
                 * Tronque silencieusement si le résultat dépasse outSize.
                 */
                template <typename T>
                NKENTSEU_API_INLINE const char* NkFoundationFormatValueCStr(
                    const T& value,
                    char* out,
                    size_t outSize
                ) {
                    if (!out || outSize == 0) {
                        return "";
                    }

                    // Appel avec priorité 0 pour tenter l'extension utilisateur d'abord
                    const int written = NkFoundationFormatValueImpl(value, out, outSize, 0);

                    if (written < 0) {
                        // Erreur de formatage : buffer vide
                        out[0] = '\0';
                    } else if (static_cast<size_t>(written) >= outSize) {
                        // Troncature : garantir null-termination
                        out[outSize - 1] = '\0';
                    }

                    return out;
                }

                // -------------------------------------------------------------------------
                // Sous-section : Fonction de log typée avec label optionnel
                // -------------------------------------------------------------------------
                // Fonction utilitaire pour logger une valeur avec un label "key=value".

                /**
                 * @brief Logger une valeur avec un label optionnel
                 * @tparam T Type de la valeur à logger
                 * @param level Niveau de log ("ERR", "WRN", "INF", "DBG", "TRC")
                 * @param file Fichier source d'origine
                 * @param line Ligne source d'origine
                 * @param label Label optionnel (ex: "userId", nullptr pour aucun label)
                 * @param value Valeur à logger (sera formatée via ADL)
                 * @ingroup FoundationLogInternals
                 *
                 * @example
                 * @code
                 * // Log avec label : "userId=12345"
                 * NkFoundationLogValue("INF", __FILE__, __LINE__, "userId", 12345);
                 *
                 * // Log sans label : "Operation completed"
                 * NkFoundationLogValue("INF", __FILE__, __LINE__, nullptr, "Operation completed");
                 * @endcode
                 */
                template <typename T>
                NKENTSEU_API_INLINE void NkFoundationLogValue(
                    const char* level,
                    const char* file,
                    int line,
                    const char* label,
                    const T& value
                ) {
                    char valueBuffer[256];
                    const char* text = NkFoundationFormatValueCStr(value, valueBuffer, sizeof(valueBuffer));

                    if (label && label[0] != '\0') {
                        // Format avec label : "label=value"
                        NkFoundationLogWrite(level, file, line, "%s=%s", label, text);
                    } else {
                        // Format sans label : "value"
                        NkFoundationLogWrite(level, file, line, "%s", text);
                    }
                }

            } // namespace detail

        } // namespace platform

    } // namespace nkentseu

    // =========================================================================
    // SECTION 8 : MACROS PUBLIQUES DE LOGGING
    // =========================================================================
    // Macros ergonomiques pour un usage simplifié dans le code client.
    // Chaque macro vérifie le niveau de log avant d'évaluer ses arguments.

    // -------------------------------------------------------------------------
    // Sous-section : Macros utilitaires de base
    // -------------------------------------------------------------------------

    /**
     * @brief Macro de log générique (niveau INFO)
     * @def NK_FOUNDATION_PRINT
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @ingroup FoundationLogMacros
     *
     * Toujours actif, indépendamment de NK_FOUNDATION_LOG_LEVEL.
     * À utiliser avec parcimonie pour éviter le bruit en production.
     *
     * @example
     * @code
     * NK_FOUNDATION_PRINT("Initialisation de la bibliothèque v%d.%d", 1, 2);
     * @endcode
     */
    #define NK_FOUNDATION_PRINT(fmt, ...) \
        do { \
            ::nkentseu::platform::detail::NkFoundationLogWrite( \
                "INF", \
                __FILE__, \
                __LINE__, \
                (fmt), \
                ##__VA_ARGS__ \
            ); \
        } while (0)

    /**
     * @brief Macro de formatage sprintf-style sécurisée
     * @def NK_FOUNDATION_SPRINT
     * @param buffer Buffer de destination
     * @param bufferSize Taille du buffer de destination
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @return Nombre de caractères écrits (sans le null terminator)
     * @ingroup FoundationLogMacros
     *
     * Wrapper portable autour de snprintf avec vérification de buffer.
     */
    #define NK_FOUNDATION_SPRINT(buffer, bufferSize, fmt, ...) \
        snprintf((buffer), (bufferSize), (fmt), ##__VA_ARGS__)

    // -------------------------------------------------------------------------
    // Sous-section : Macros de log par niveau (avec filtrage compile-time)
    // -------------------------------------------------------------------------

    /**
     * @brief Macro de log niveau ERROR
     * @def NK_FOUNDATION_LOG_ERROR
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @ingroup FoundationLogMacros
     *
     * Actif si NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_ERROR.
     * Pour les erreurs critiques nécessitant une intervention immédiate.
     *
     * @example
     * @code
     * if (!file.Open("config.ini")) {
     *     NK_FOUNDATION_LOG_ERROR("Échec d'ouverture: %s", file.GetLastError());
     *     return false;
     * }
     * @endcode
     */
    #define NK_FOUNDATION_LOG_ERROR(fmt, ...) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_ERROR) { \
                ::nkentseu::platform::detail::NkFoundationLogWrite( \
                    "ERR", \
                    __FILE__, \
                    __LINE__, \
                    (fmt), \
                    ##__VA_ARGS__ \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau WARN
     * @def NK_FOUNDATION_LOG_WARN
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @ingroup FoundationLogMacros
     *
     * Actif si NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_WARN.
     * Pour les situations anormales mais non bloquantes.
     */
    #define NK_FOUNDATION_LOG_WARN(fmt, ...) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_WARN) { \
                ::nkentseu::platform::detail::NkFoundationLogWrite( \
                    "WRN", \
                    __FILE__, \
                    __LINE__, \
                    (fmt), \
                    ##__VA_ARGS__ \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau INFO
     * @def NK_FOUNDATION_LOG_INFO
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @ingroup FoundationLogMacros
     *
     * Actif si NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_INFO.
     * Pour les messages informatifs sur le flux d'exécution.
     */
    #define NK_FOUNDATION_LOG_INFO(fmt, ...) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_INFO) { \
                ::nkentseu::platform::detail::NkFoundationLogWrite( \
                    "INF", \
                    __FILE__, \
                    __LINE__, \
                    (fmt), \
                    ##__VA_ARGS__ \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau DEBUG
     * @def NK_FOUNDATION_LOG_DEBUG
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @ingroup FoundationLogMacros
     *
     * Actif si NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_DEBUG.
     * Pour les messages de debug pendant le développement.
     */
    #define NK_FOUNDATION_LOG_DEBUG(fmt, ...) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_DEBUG) { \
                ::nkentseu::platform::detail::NkFoundationLogWrite( \
                    "DBG", \
                    __FILE__, \
                    __LINE__, \
                    (fmt), \
                    ##__VA_ARGS__ \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau TRACE
     * @def NK_FOUNDATION_LOG_TRACE
     * @param fmt Chaîne de format style printf
     * @param ... Arguments de formatage
     * @ingroup FoundationLogMacros
     *
     * Actif si NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE.
     * Pour les traces très détaillées (entrée/sortie de fonctions, valeurs intermédiaires).
     */
    #define NK_FOUNDATION_LOG_TRACE(fmt, ...) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE) { \
                ::nkentseu::platform::detail::NkFoundationLogWrite( \
                    "TRC", \
                    __FILE__, \
                    __LINE__, \
                    (fmt), \
                    ##__VA_ARGS__ \
                ); \
            } \
        } while (0)

    // -------------------------------------------------------------------------
    // Sous-section : Macros de log de valeurs avec label (format "key=value")
    // -------------------------------------------------------------------------

    /**
     * @brief Macro de log niveau ERROR pour une valeur avec label
     * @def NK_FOUNDATION_LOG_ERROR_VALUE
     * @param label Nom de la variable/paramètre (ou nullptr pour aucun label)
     * @param value Valeur à logger (tout type supporté par le formateur)
     * @ingroup FoundationLogMacros
     *
     * Format de sortie : "[ERR] file:line label=value"
     *
     * @example
     * @code
     * NK_FOUNDATION_LOG_ERROR_VALUE("errorCode", result);
     * // Sortie : [ERR] myfile.cpp:42 errorCode=-1
     * @endcode
     */
    #define NK_FOUNDATION_LOG_ERROR_VALUE(label, value) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_ERROR) { \
                ::nkentseu::platform::detail::NkFoundationLogValue( \
                    "ERR", \
                    __FILE__, \
                    __LINE__, \
                    (label), \
                    (value) \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau WARN pour une valeur avec label
     * @def NK_FOUNDATION_LOG_WARN_VALUE
     * @param label Nom de la variable/paramètre
     * @param value Valeur à logger
     * @ingroup FoundationLogMacros
     */
    #define NK_FOUNDATION_LOG_WARN_VALUE(label, value) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_WARN) { \
                ::nkentseu::platform::detail::NkFoundationLogValue( \
                    "WRN", \
                    __FILE__, \
                    __LINE__, \
                    (label), \
                    (value) \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau INFO pour une valeur avec label
     * @def NK_FOUNDATION_LOG_INFO_VALUE
     * @param label Nom de la variable/paramètre
     * @param value Valeur à logger
     * @ingroup FoundationLogMacros
     */
    #define NK_FOUNDATION_LOG_INFO_VALUE(label, value) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_INFO) { \
                ::nkentseu::platform::detail::NkFoundationLogValue( \
                    "INF", \
                    __FILE__, \
                    __LINE__, \
                    (label), \
                    (value) \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau DEBUG pour une valeur avec label
     * @def NK_FOUNDATION_LOG_DEBUG_VALUE
     * @param label Nom de la variable/paramètre
     * @param value Valeur à logger
     * @ingroup FoundationLogMacros
     */
    #define NK_FOUNDATION_LOG_DEBUG_VALUE(label, value) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_DEBUG) { \
                ::nkentseu::platform::detail::NkFoundationLogValue( \
                    "DBG", \
                    __FILE__, \
                    __LINE__, \
                    (label), \
                    (value) \
                ); \
            } \
        } while (0)

    /**
     * @brief Macro de log niveau TRACE pour une valeur avec label
     * @def NK_FOUNDATION_LOG_TRACE_VALUE
     * @param label Nom de la variable/paramètre
     * @param value Valeur à logger
     * @ingroup FoundationLogMacros
     */
    #define NK_FOUNDATION_LOG_TRACE_VALUE(label, value) \
        do { \
            if (NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE) { \
                ::nkentseu::platform::detail::NkFoundationLogValue( \
                    "TRC", \
                    __FILE__, \
                    __LINE__, \
                    (label), \
                    (value) \
                ); \
            } \
        } while (0)

#endif // NKENTSEU_PLATFORM_NKFOUNDATIONLOG_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKFOUNDATIONLOG.H
// =============================================================================
// Ce fichier fournit un système de logging minimaliste et portable pour la
// couche Foundation, avec support multi-plateforme et extensibilité.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Logging basique avec les macros de niveau
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkFoundationLog.h"

    void InitializeSubsystem() {
        NK_FOUNDATION_LOG_INFO("Initialisation du sous-système...");

        if (!LoadConfiguration()) {
            NK_FOUNDATION_LOG_ERROR("Échec de chargement de la configuration");
            return;
        }

        NK_FOUNDATION_LOG_DEBUG("Configuration chargée avec succès");

        #if NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE
            NK_FOUNDATION_LOG_TRACE("Détails: configPath=%s", GetConfigPath());
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Logging de valeurs avec formatage automatique
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkFoundationLog.h"

    void ProcessData(int userId, float score, bool isValid) {
        // Formatage automatique des types primitifs
        NK_FOUNDATION_LOG_INFO_VALUE("userId", userId);
        NK_FOUNDATION_LOG_INFO_VALUE("score", score);
        NK_FOUNDATION_LOG_INFO_VALUE("isValid", isValid);

        // Formatage de pointeurs (adresse mémoire)
        void* buffer = AllocateBuffer(1024);
        NK_FOUNDATION_LOG_DEBUG_VALUE("buffer", buffer);

        // Formatage de chaînes
        const char* status = "OK";
        NK_FOUNDATION_LOG_INFO_VALUE("status", status);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Extension du formatage pour un type personnalisé (ADL)
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkFoundationLog.h"

    // Type personnalisé dans votre namespace
    namespace myapp {
        struct Vector3 {
            float x, y, z;
        };

        // Fonction d'extension pour le formatage (même namespace que Vector3)
        // NOTE: Le nom doit être NKFoundationToString (sans préfixe)
        int NKFoundationToString(const Vector3& v, char* out, size_t outSize) {
            return snprintf(out, outSize, "(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
        }
    }

    // Utilisation : le formateur trouve automatiquement NKFoundationToString via ADL
    void LogPlayerPosition(const myapp::Vector3& position) {
        NK_FOUNDATION_LOG_INFO_VALUE("position", position);
        // Sortie : [INF] file.cpp:42 position=(12.34, 56.78, 90.12)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Configuration d'un sink de log personnalisé
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkFoundationLog.h"
    #include <fstream>

    // Sink personnalisé pour écrire dans un fichier
    void FileLogSink(const char* level, const char* file, int line, const char* message) {
        static std::ofstream logFile("app.log", std::ios::app);
        if (logFile.is_open()) {
            logFile << "[" << level << "] " << file << ":" << line << " " << message << "\n";
            logFile.flush();
        }
    }

    // Sink personnalisé pour envoyer vers un serveur de logs
    void NetworkLogSink(const char* level, const char* file, int line, const char* message) {
        // Pseudo-code : envoyer via HTTP, UDP, etc.
        // LogServer::Send(level, file, line, message);
    }

    void SetupLogging() {
        // Utiliser le sink fichier en mode debug
        #if defined(_DEBUG)
            nkentseu::platform::NkFoundationSetLogSink(FileLogSink);
        #else
            // En production, utiliser le sink réseau ou le défaut (stderr)
            // nkentseu::platform::NkFoundationSetLogSink(NetworkLogSink);
        #endif
    }

    // Pour restaurer le backend par défaut :
    // nkentseu::platform::NkFoundationSetLogSink(nullptr);
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Contrôle du niveau de log à la compilation
// -----------------------------------------------------------------------------
/*
    // Avant d'inclure NkFoundationLog.h, redéfinir le niveau souhaité :

    // Mode très verbeux pour le debug intensif
    #define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_TRACE
    #include "NKPlatform/NkFoundationLog.h"

    // Ou mode minimal pour la production
    // #define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_ERROR
    // #include "NKPlatform/NkFoundationLog.h"

    void VerboseFunction() {
        // Ces logs seront compilés uniquement si le niveau le permet
        NK_FOUNDATION_LOG_TRACE("Entrée dans VerboseFunction");

        for (int i = 0; i < 100; ++i) {
            NK_FOUNDATION_LOG_TRACE_VALUE("iteration", i);
            // ... traitement ...
        }

        NK_FOUNDATION_LOG_TRACE("Sortie de VerboseFunction");
    }

    // En mode ERROR uniquement, tout le code ci-dessus est éliminé à la compilation
    // (pas d'évaluation des arguments, pas de surcoût runtime)
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec les autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
    #include "NKPlatform/NkCompilerDetect.h"   // NKENTSEU_COMPILER_*
    #include "NKPlatform/NkFoundationLog.h"    // Logging

    void StartupDiagnostics() {
        // Log d'informations de plateforme
        NK_FOUNDATION_LOG_INFO("Plateforme: %s (%s)",
            NKENTSEU_PLATFORM_NAME,
            NKENTSEU_PLATFORM_VERSION);

        NK_FOUNDATION_LOG_INFO("Architecture: %s (%d-bit)",
            NKENTSEU_ARCH_NAME,
            NKENTSEU_WORD_BITS);

        NK_FOUNDATION_LOG_INFO("Compilateur: version %d",
            NKENTSEU_COMPILER_VERSION);

        // Log conditionnel par plateforme
        #ifdef NKENTSEU_PLATFORM_ANDROID
            NK_FOUNDATION_LOG_DEBUG("Backend Android logcat activé");
        #endif

        // Log avec formatage de pointeur
        void* mainAddr = reinterpret_cast<void*>(&StartupDiagnostics);
        NK_FOUNDATION_LOG_DEBUG_VALUE("mainAddress", mainAddr);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Logging thread-safe avec contexte
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkFoundationLog.h"
    #include <thread>
    #include <sstream>

    // Helper pour préfixer les logs avec l'ID de thread
    std::string ThreadedLogPrefix(const char* level) {
        std::ostringstream oss;
        oss << "[" << std::this_thread::get_id() << "] " << level;
        return oss.str();
    }

    void WorkerThread(int workerId) {
        NK_FOUNDATION_LOG_INFO("Thread %d démarré", workerId);

        // Simulation de travail avec logs
        for (int i = 0; i < 5; ++i) {
            NK_FOUNDATION_LOG_DEBUG_VALUE("workerId", workerId);
            NK_FOUNDATION_LOG_DEBUG_VALUE("progress", i * 20);
            // ... traitement ...
        }

        NK_FOUNDATION_LOG_INFO("Thread %d terminé", workerId);
    }

    void LaunchWorkers() {
        std::thread t1(WorkerThread, 1);
        std::thread t2(WorkerThread, 2);

        t1.join();
        t2.join();

        NK_FOUNDATION_LOG_INFO("Tous les workers terminés");
    }

    // Note : Les appels à fprintf/__android_log_print sont généralement thread-safe,
    // mais l'ordre des messages entre threads n'est pas garanti.
    // Pour un logging strictement ordonné, utiliser un sink personnalisé avec mutex.
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================