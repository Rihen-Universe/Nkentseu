// =============================================================================
// NKCore/Assert/NkAssertion.h
// Structure de données pour informations d'assertion échouée et actions associées.
//
// Design :
//  - Struct NkAssertionInfo : encapsulation des métadonnées d'une assertion échouée
//  - Enum NkAssertAction : politique de gestion des échecs d'assertion
//  - Types nk_char et nk_int32 pour portabilité cross-platform
//  - Header-only : définitions légères sans dépendances lourdes
//  - Compatible avec systèmes de logging et handlers d'assertion personnalisés
//
// Intégration :
//  - Utilise NKCore/NkTypes.h pour les types fondamentaux portables
//  - Peut être inclus par NkDebugBreak.h, NkBuiltin.h, ou systèmes de logging
//  - Conçu pour être utilisé dans des macros d'assertion type NK_ASSERT()
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKASSERTION_H_INCLUDED
#define NKENTSEU_CORE_NKASSERTION_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des types fondamentaux pour portabilité.

    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : STRUCTURE NKASSERTIONINFO
        // ====================================================================
        // Conteneur d'informations contextuelles pour une assertion échouée.

        /**
         * @defgroup AssertionStructures Structures pour Gestion d'Assertions
         * @brief Types de données pour capture et traitement des échecs d'assertion
         * @ingroup AssertUtilities
         *
         * Ces structures permettent de capturer le contexte complet d'une
         * assertion échouée pour logging, débogage, ou prise de décision
         * dynamique sur l'action à entreprendre.
         *
         * @note
         *   - Tous les champs sont en lecture seule (const) pour sécurité
         *   - Les chaînes nk_char* pointent vers des littéraux ou buffers statiques
         *   - Ne pas libérer/modifier les pointeurs : durée de vie gérée par l'appelant
         *
         * @example
         * @code
         * // Handler personnalisé d'assertions
         * nkentseu::NkAssertAction HandleAssertion(const nkentseu::NkAssertionInfo& info) {
         *     // Logging structuré avec contexte complet
         *     Logger::Error(
         *         "Assertion failed: %s\n"
         *         "  Location: %s:%d in %s\n"
         *         "  Message: %s\n",
         *         info.expression,
         *         info.file,
         *         info.line,
         *         info.function,
         *         info.message ? info.message : "(none)"
         *     );
         *
         *     // Décision dynamique selon le contexte
         *     #if defined(NKENTSEU_DEBUG)
         *         return nkentseu::NkAssertAction::NK_BREAK;  // Debugger en debug
         *     #else
         *         return nkentseu::NkAssertAction::NK_ABORT;  // Crash contrôlé en release
         *     #endif
         * }
         * @endcode
         */
        /** @{ */

        /**
         * @brief Informations contextuelles capturées lors d'un échec d'assertion
         * @struct NkAssertionInfo
         * @ingroup AssertionStructures
         *
         * Cette structure agrège toutes les métadonnées disponibles au moment
         * où une assertion échoue, permettant un traitement riche et informatif
         * de l'erreur sans dépendre de variables globales ou d'état partagé.
         *
         * @note
         *   - expression : la condition qui a échoué, sous forme de chaîne littérale
         *   - message : message optionnel fourni par le développeur via macro
         *   - file/line/function : contexte source via macros __FILE__, __LINE__, etc.
         *   - Tous les champs sont des pointeurs const : la structure ne possède pas les données
         *
         * @warning
         *   Les chaînes pointées doivent rester valides pendant toute la durée
         *   de traitement de l'assertion. En pratique, ce sont des littéraux ou
         *   des variables statiques, donc cette contrainte est naturellement respectée.
         */
        struct NkAssertionInfo {
            /**
             * @brief Expression de l'assertion qui a échoué
             * @note Exemple: "ptr != nullptr && size > 0"
             */
            const nk_char *expression;

            /**
             * @brief Message personnalisé optionnel fourni par le développeur
             * @note Peut être nullptr si aucun message n'a été spécifié
             * @example "Buffer pointer must be valid before processing"
             */
            const nk_char *message;

            /**
             * @brief Chemin du fichier source contenant l'assertion échouée
             * @note Valeur de __FILE__ ou équivalent selon le compilateur
             */
            const nk_char *file;

            /**
             * @brief Numéro de ligne dans le fichier source
             * @note Valeur de __LINE__ au point d'échec de l'assertion
             */
            nk_int32 line;

            /**
             * @brief Nom de la fonction contenant l'assertion
             * @note Valeur de __func__, __FUNCTION__ ou __PRETTY_FUNCTION__ selon le compilateur
             */
            const nk_char *function;
        };

        /** @} */ // End of AssertionStructures

        // ====================================================================
        // SECTION 4 : ÉNUMÉRATION NKASSERTACTION
        // ====================================================================
        // Politiques de gestion des échecs d'assertion.

        /**
         * @defgroup AssertionActions Actions de Gestion d'Assertions
         * @brief Énumération des réponses possibles à un échec d'assertion
         * @ingroup AssertUtilities
         *
         * Cette énumération définit les actions qu'un handler d'assertion
         * peut retourner pour contrôler le flux d'exécution après un échec.
         *
         * @note
         *   - Le handler peut implémenter une logique complexe pour choisir l'action
         *   - Certaines actions peuvent être ignorées selon la configuration build
         *   - NK_IGNORE et NK_IGNORE_ALL permettent de supprimer du bruit en debug
         *
         * @example
         * @code
         * // Handler avec logique de décision contextuelle
         * nkentseu::NkAssertAction SmartAssertionHandler(
         *     const nkentseu::NkAssertionInfo& info,
         *     nkentseu::nk_bool isCritical
         * ) {
         *     using namespace nkentseu;
         *
         *     // Assertions critiques : toujours abort pour sécurité
         *     if (isCritical) {
         *         Logger::Critical("Critical assertion failed: %s", info.expression);
         *         return NkAssertAction::NK_ABORT;
         *     }
         *
         *     // En mode debug : break pour inspection au débogueur
         *     #if defined(NKENTSEU_DEBUG)
         *         return NkAssertAction::NK_BREAK;
         *     #else
         *         // En release : logger et continuer si possible
         *         Logger::Error("Assertion failed: %s at %s:%d",
         *             info.expression, info.file, info.line);
         *         return NkAssertAction::NK_CONTINUE;
         *     #endif
         * }
         * @endcode
         */
        /** @{ */

        /**
         * @brief Actions possibles suite à un échec d'assertion
         * @enum NkAssertAction
         * @ingroup AssertionActions
         *
         * Chaque valeur représente une stratégie de gestion d'erreur différente,
         * permettant une flexibilité maximale dans la réponse aux assertions
         * échouées selon le contexte d'exécution et de build.
         */
        enum class NkAssertAction : nk_int32 {
            /**
             * @brief Continuer l'exécution normalement après logging
             * @note Utile pour les assertions non-critiques en mode release
             * @warning Peut mener à un comportement indéfini si l'invariant violé est essentiel
             */
            NK_CONTINUE = 0,

            /**
             * @brief Interrompre l'exécution et attacher le débogueur
             * @note Équivalent à un breakpoint programme : permet inspection immédiate
             * @warning Ne fonctionne que si un débogueur est attaché ; sinon comportement plateforme-dépendant
             */
            NK_BREAK = 1,

            /**
             * @brief Terminer le programme immédiatement avec code d'erreur
             * @note Utile pour les échecs critiques où continuer serait dangereux
             * @warning Perte de données possibles : à utiliser avec précaution
             */
            NK_ABORT = 2,

            /**
             * @brief Ignorer cette assertion spécifique pour le reste de l'exécution
             * @note Permet de supprimer du bruit pour des assertions connues et acceptées
             * @warning L'assertion ne sera plus évaluée : risque de masquer de nouveaux problèmes
             */
            NK_IGNORE = 3,

            /**
             * @brief Ignorer toutes les assertions pour le reste de l'exécution
             * @note Mode "quiet" pour sessions de débogage focalisées sur d'autres problèmes
             * @warning Désactive tout le système d'assertions : à utiliser temporairement uniquement
             */
            NK_IGNORE_ALL = 4
        };

        /** @} */ // End of AssertionActions

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKASSERTION_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKASSERTION.H
// =============================================================================
// Ce fichier fournit les types de base pour un système d'assertions flexible.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Macro d'assertion personnalisée avec handler configurable
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertion.h"
    #include "NKCore/Assert/NkDebugBreak.h"
    #include "NKCore/NkBuiltin.h"

    // Handler global d'assertions (à définir dans un .cpp)
    namespace nkentseu {
        NkAssertAction HandleAssertion(const NkAssertionInfo& info);
    }

    // Macro principale d'assertion
    #define NK_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            /\* Construction des informations d'assertion *\/ \
            nkentseu::NkAssertionInfo info; \
            info.expression = #expr; \
            info.message = msg; \
            info.file = NKENTSEU_BUILTIN_FILE; \
            info.line = static_cast<nkentseu::nk_int32>(NKENTSEU_BUILTIN_LINE); \
            info.function = NKENTSEU_BUILTIN_FUNCTION; \
            \
            /\* Appel du handler pour décision *\/ \
            nkentseu::NkAssertAction action = nkentseu::HandleAssertion(info); \
            \
            /\* Exécution de l'action choisie *\/ \
            switch (action) { \
                case nkentseu::NkAssertAction::NK_CONTINUE: \
                    break; \
                case nkentseu::NkAssertAction::NK_BREAK: \
                    NKENTSEU_DEBUGBREAK(); \
                    break; \
                case nkentseu::NkAssertAction::NK_ABORT: \
                    std::abort(); \
                    break; \
                case nkentseu::NkAssertAction::NK_IGNORE: \
                case nkentseu::NkAssertAction::NK_IGNORE_ALL: \
                    /\* Ignoré : continuer silencieusement *\/ \
                    break; \
            } \
        } \
    } while (0)

    // Usage
    void ProcessData(void* buffer, size_t size) {
        NK_ASSERT(buffer != nullptr, "Buffer must be non-null");
        NK_ASSERT(size > 0 && size <= MAX_SIZE, "Invalid buffer size");

        // ... traitement ...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Handler d'assertion avec logging structuré
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertion.h"
    #include "NKCore/NkFoundationLog.h"  // Système de logging du framework

    namespace nkentseu {

        // Handler global : à lier dans le système d'assertions
        NkAssertAction HandleAssertion(const NkAssertionInfo& info) {
            // Formatage du message d'erreur complet
            const nk_char* effectiveMsg = info.message ? info.message : "(no message)";

            // Logging hiérarchisé selon la sévérité
            NK_FOUNDATION_LOG_ERROR(
                "ASSERTION FAILED\n"
                "  Expression : %s\n"
                "  Message    : %s\n"
                "  Location   : %s:%d\n"
                "  Function   : %s",
                info.expression,
                effectiveMsg,
                info.file,
                info.line,
                info.function
            );

            // Décision selon le mode de build
            #if defined(NKENTSEU_DEBUG)
                // En debug : break pour inspection interactive
                return NkAssertAction::NK_BREAK;
            #else
                // En release : abort pour éviter corruption de données
                return NkAssertAction::NK_ABORT;
            #endif
        }

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Assertions conditionnelles avec ignore dynamique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertion.h"
    #include <unordered_set>
    #include <string>

    namespace nkentseu {

        // Gestionnaire avec liste d'assertions à ignorer
        class AssertionManager {
        public:
            // Ajouter une expression à ignorer (par hash ou string)
            static void IgnoreAssertion(const nk_char* expression) {
                GetIgnoredSet().insert(expression);
            }

            // Vérifier si une assertion doit être ignorée
            static bool ShouldIgnore(const nk_char* expression) {
                return GetIgnoredSet().find(expression) != GetIgnoredSet().end();
            }

            // Handler utilisant la liste d'ignore
            static NkAssertAction HandleWithIgnore(const NkAssertionInfo& info) {
                // Ignorer si explicitement demandé
                if (ShouldIgnore(info.expression)) {
                    return NkAssertAction::NK_IGNORE;
                }

                // Logging standard
                LogAssertionFailure(info);

                // Décision par défaut
                #if defined(NKENTSEU_DEBUG)
                    return NkAssertAction::NK_BREAK;
                #else
                    return NkAssertAction::NK_ABORT;
                #endif
            }

        private:
            static std::unordered_set<std::string>& GetIgnoredSet() {
                static std::unordered_set<std::string> ignored;
                return ignored;
            }

            static void LogAssertionFailure(const NkAssertionInfo& info) {
                // Implémentation de logging...
            }
        };

    } // namespace nkentseu

    // Usage : ignorer une assertion bruyante pendant le débogage
    void DebugSession() {
        // Ignorer temporairement une assertion connue pour être fausse dans ce contexte
        nkentseu::AssertionManager::IgnoreAssertion("m_cache.IsValid()");

        // Les assertions suivantes avec cette expression seront ignorées
        NK_ASSERT(m_cache.IsValid(), "Cache should be valid");  // Ignoré silencieusement
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Assertions avec contexte métier enrichi
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertion.h"
    #include "NKCore/NkTypes.h"

    // Extension de NkAssertionInfo avec contexte métier (via héritage ou composition)
    struct BusinessAssertionInfo : public nkentseu::NkAssertionInfo {
        nkentseu::nk_uint32 errorCode;      // Code d'erreur métier
        const nk_char* moduleName;          // Module concerné
        nkentseu::nk_uint64 timestamp;      // Timestamp de l'échec
    };

    // Macro enrichie pour assertions métier
    #define NK_ASSERT_BUSINESS(expr, msg, errorCode, module) \
    do { \
        if (!(expr)) { \
            BusinessAssertionInfo info; \
            /\* Champs de base *\/ \
            info.expression = #expr; \
            info.message = msg; \
            info.file = __FILE__; \
            info.line = static_cast<nkentseu::nk_int32>(__LINE__); \
            info.function = __func__; \
            /\* Champs métier *\/ \
            info.errorCode = errorCode; \
            info.moduleName = module; \
            info.timestamp = GetCurrentTimestamp(); \
            \
            /\* Handler spécialisé métier *\/ \
            HandleBusinessAssertion(info); \
        } \
    } while (0)

    // Handler métier avec routage selon module
    void HandleBusinessAssertion(const BusinessAssertionInfo& info) {
        // Routage vers logger spécifique au module
        if (info.moduleName) {
            ModuleLogger::Get(info.moduleName)->Error(
                "[%u] %s at %s:%d - %s",
                info.errorCode,
                info.expression,
                info.file,
                info.line,
                info.message ? info.message : ""
            );
        }

        // Décision selon sévérité du code d'erreur
        if (IsCriticalError(info.errorCode)) {
            std::abort();  // Erreur critique : arrêt immédiat
        }
        // Sinon : continuer avec logging
    }

    // Usage dans un module métier
    void ProcessTransaction(Transaction* tx) {
        NK_ASSERT_BUSINESS(
            tx != nullptr && tx->amount >= 0,
            "Invalid transaction data",
            ERR_INVALID_TRANSACTION,  // Code d'erreur métier défini ailleurs
            "PaymentModule"           // Nom du module
        );

        // ... traitement de la transaction ...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Tests unitaires avec capture d'assertions
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertion.h"
    #include <vector>
    #include <functional>

    // Framework de test avec capture d'assertions pour validation
    class AssertionCapture {
    public:
        // Structure pour stocker une assertion capturée
        struct CapturedAssertion {
            nkentseu::NkAssertionInfo info;
            bool wasTriggered;
        };

        // Activer la capture des assertions
        static void EnableCapture() {
            GetCaptureList().clear();
            GetEnabled() = true;
        }

        // Désactiver la capture
        static void DisableCapture() {
            GetEnabled() = false;
        }

        // Handler qui capture au lieu d'agir
        static nkentseu::NkAssertAction CaptureHandler(const nkentseu::NkAssertionInfo& info) {
            if (GetEnabled()) {
                CapturedAssertion captured;
                captured.info = info;
                captured.wasTriggered = true;
                GetCaptureList().push_back(captured);
                // Retourner CONTINUE pour ne pas interrompre le test
                return nkentseu::NkAssertAction::NK_CONTINUE;
            }
            // Comportement normal si capture désactivée
            return DefaultHandler(info);
        }

        // Vérifier si une assertion spécifique a été déclenchée
        static bool WasAssertionTriggered(const nk_char* expression) {
            for (const auto& cap : GetCaptureList()) {
                if (cap.wasTriggered && cap.info.expression == expression) {
                    return true;
                }
            }
            return false;
        }

        // Obtenir toutes les assertions capturées
        static const std::vector<CapturedAssertion>& GetCaptured() {
            return GetCaptureList();
        }

    private:
        static std::vector<CapturedAssertion>& GetCaptureList() {
            static std::vector<CapturedAssertion> list;
            return list;
        }

        static bool& GetEnabled() {
            static bool enabled = false;
            return enabled;
        }

        static nkentseu::NkAssertAction DefaultHandler(const nkentseu::NkAssertionInfo& info) {
            // Fallback vers handler normal...
            return nkentseu::NkAssertAction::NK_CONTINUE;
        }
    };

    // Test unitaire exemple
    bool Test_ValidationLogic() {
        // Activer la capture pour ce test
        AssertionCapture::EnableCapture();

        // Code qui devrait déclencher des assertions
        ValidateInput(nullptr);  // Devrait échouer : ptr null
        ValidateInput(invalidData);  // Devrait échouer : données invalides

        // Vérifications des assertions capturées
        NKENTSEU_TEST_ASSERT(
            AssertionCapture::WasAssertionTriggered("ptr != nullptr"),
            "Expected assertion for null pointer check"
        );

        NKENTSEU_TEST_ASSERT(
            AssertionCapture::GetCaptured().size() == 2,
            "Expected exactly 2 assertions to be triggered"
        );

        // Nettoyer
        AssertionCapture::DisableCapture();
        return true;
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================