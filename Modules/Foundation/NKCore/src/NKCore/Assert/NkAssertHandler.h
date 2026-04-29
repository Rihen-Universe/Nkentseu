// =============================================================================
// NKCore/Assert/NkAssertHandler.h
// Gestionnaire global d'assertions avec callback personnalisable.
//
// Design :
//  - Classe statique NkAssertHandler pour gestion centralisée des assertions
//  - Callback configurables via NkAssertCallback (fonction pointeur)
//  - Callback par défaut avec comportement debug/release adaptatif
//  - Méthode HandleAssertion() pour invocation uniforme depuis les macros
//  - Export NKENTSEU_CORE_API pour compatibilité DLL/shared libraries
//  - Thread-safe : accès au callback protégé (à implémenter dans le .cpp)
//
// Intégration :
//  - Utilise NKCore/Assert/NkAssertion.h pour NkAssertionInfo et NkAssertAction
//  - Utilise NKCore/NkCoreExport.h pour macro d'export NKENTSEU_CORE_API
//  - Utilise NKCore/NkTypes.h pour types fondamentaux portables
//  - Conçu pour être appelé par les macros NK_ASSERT() du framework
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKASSERTHANDLER_H_INCLUDED
#define NKENTSEU_CORE_NKASSERTHANDLER_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis pour les types et l'export.

    #include "NkAssertion.h"
    #include "NKCore/NkCoreExport.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : TYPE DE CALLBACK POUR ASSERTIONS
        // ====================================================================
        // Définition du type de fonction pour handlers personnalisés.

        /**
         * @defgroup AssertHandlerTypes Types pour Gestion d'Assertions
         * @brief Types et callbacks pour système d'assertions extensible
         * @ingroup AssertUtilities
         *
         * Ces types permettent de personnaliser le comportement du système
         * d'assertions sans modifier le code des macros NK_ASSERT().
         *
         * @note
         *   - NkAssertCallback : signature standard pour tout handler personnalisé
         *   - Le callback reçoit une référence const à NkAssertionInfo (lecture seule)
         *   - Le retour NkAssertAction contrôle le flux post-échec
         *
         * @example
         * @code
         * // Handler personnalisé avec logging vers fichier
         * nkentseu::NkAssertAction FileLoggerCallback(const nkentseu::NkAssertionInfo& info) {
         *     FILE* log = fopen("assertions.log", "a");
         *     if (log) {
         *         fprintf(log, "[%s:%d] %s failed: %s\n",
         *             info.file, info.line, info.function, info.expression);
         *         fclose(log);
         *     }
         *     // Retourner l'action par défaut pour le reste du traitement
         *     return nkentseu::NkAssertHandler::DefaultCallback(info);
         * }
         *
         * // Enregistrement du handler au démarrage de l'application
         * void InitializeApp() {
         *     nkentseu::NkAssertHandler::SetCallback(FileLoggerCallback);
         * }
         * @endcode
         */
        /** @{ */

        /**
         * @brief Signature de fonction pour callback de gestion d'assertions
         * @typedef NkAssertCallback
         * @param info Référence const vers les informations de l'assertion échouée
         * @return Action à entreprendre suite à l'échec
         * @ingroup AssertHandlerTypes
         *
         * Ce type de fonction définit le contrat pour tout handler d'assertion
         * personnalisé. Le callback est invoqué chaque fois qu'une assertion
         * échoue, permettant une réaction contextuelle et configurable.
         *
         * @note
         *   - Le paramètre info est en lecture seule : ne pas modifier son contenu
         *   - Le callback peut être appelé depuis n'importe quel thread : doit être thread-safe
         *   - En mode release, le callback par défaut peut appeler std::abort() : éviter les ressources non-libérées
         *
         * @warning
         *   Le callback ne doit pas lever d'exceptions : cela pourrait corrompre
         *   l'état du programme lors d'un échec d'assertion déjà critique.
         */
        using NkAssertCallback = NkAssertAction (*)(const NkAssertionInfo&);

        /** @} */ // End of AssertHandlerTypes

        // ====================================================================
        // SECTION 4 : CLASSE NKASSERTHANDLER (GESTIONNAIRE GLOBAL)
        // ====================================================================
        // Interface statique pour configuration et invocation du handler.

        /**
         * @defgroup AssertHandlerClass Gestionnaire Global d'Assertions
         * @brief Classe statique pour configuration centralisée du système d'assertions
         * @ingroup AssertUtilities
         *
         * NkAssertHandler fournit un point d'entrée unique pour :
         *   - Configurer le callback de gestion d'assertions
         *   - Accéder au callback courant pour inspection ou chaînage
         *   - Invoquer le handler via HandleAssertion() depuis les macros
         *   - Fournir un comportement par défaut adaptatif (debug/release)
         *
         * @note
         *   - Toutes les méthodes sont statiques : pas d'instanciation requise
         *   - NKENTSEU_CORE_API : export pour compatibilité avec builds DLL/shared
         *   - Thread-safety : l'implémentation doit protéger l'accès au callback global
         *
         * @example
         * @code
         * // Configuration au démarrage
         * void SetupAssertions() {
         *     #if defined(NKENTSEU_DEBUG)
         *         // En debug : handler avec breakpoint debugger
         *         nkentseu::NkAssertHandler::SetCallback(DebugBreakCallback);
         *     #else
         *         // En release : handler avec logging et abort contrôlé
         *         nkentseu::NkAssertHandler::SetCallback(ReleaseCallback);
         *     #endif
         * }
         *
         * // Invocation depuis une macro d'assertion
         * #define NK_ASSERT(expr, msg) \
         * do { \
         *     if (!(expr)) { \
         *         nkentseu::NkAssertionInfo info{#expr, msg, __FILE__, __LINE__, __func__}; \
         *         nkentseu::NkAssertHandler::HandleAssertion(info); \
         *     } \
         * } while(0)
         * @endcode
         */
        /** @{ */

        /**
         * @brief Gestionnaire global d'assertions avec callback configurable
         * @class NkAssertHandler
         * @ingroup AssertHandlerClass
         *
         * Cette classe encapsule la logique de dispatch des assertions échouées
         * vers un callback configurables, avec fallback vers un comportement
         * par défaut sécurisé.
         *
         * @note
         *   - Design singleton statique : une seule instance globale implicite
         *   - Le callback est stocké dans une variable statique protégée
         *   - Les méthodes publiques sont thread-safe (à garantir dans l'implémentation)
         *
         * @warning
         *   Modifier le callback depuis un handler d'assertion peut causer
         *   des comportements inattendus. Préférer la configuration au démarrage.
         */
        class NKENTSEU_CORE_API NkAssertHandler {
            public:
                // --------------------------------------------------------
                // Méthodes de configuration du callback
                // --------------------------------------------------------

                /**
                 * @brief Récupère le callback actuellement enregistré
                 * @return Pointeur vers la fonction callback courante
                 * @note Retourne DefaultCallback si aucun callback custom n'a été défini
                 * @ingroup AssertHandlerClass
                 *
                 * @example
                 * @code
                 * // Vérifier le handler actuel avant modification
                 * auto current = nkentseu::NkAssertHandler::GetCallback();
                 * if (current != nkentseu::NkAssertHandler::DefaultCallback) {
                 *     Logger::Warning("Custom assert handler already installed");
                 * }
                 * @endcode
                 */
                static NkAssertCallback GetCallback();

                /**
                 * @brief Définit un callback personnalisé pour gestion d'assertions
                 * @param callback Pointeur vers la fonction handler à installer
                 * @note Passer nullptr pour restaurer le callback par défaut
                 * @ingroup AssertHandlerClass
                 *
                 * @warning
                 *   Cette méthode n'est pas thread-safe par défaut : appeler depuis
                 *   un contexte single-threadé (ex: initialisation) ou protéger
                 *   l'appel avec un mutex si appelé depuis plusieurs threads.
                 *
                 * @example
                 * @code
                 * // Installation d'un handler avec chaînage
                 * nkentseu::NkAssertAction ChainedCallback(const nkentseu::NkAssertionInfo& info) {
                 *     // Logging personnalisé
                 *     CustomLogger::LogAssertion(info);
                 *
                 *     // Chaînage vers le handler précédent
                 *     auto prev = nkentseu::NkAssertHandler::GetCallback();
                 *     if (prev && prev != ChainedCallback) {
                 *         return prev(info);
                 *     }
                 *     return nkentseu::NkAssertAction::NK_ABORT;
                 * }
                 *
                 * nkentseu::NkAssertHandler::SetCallback(ChainedCallback);
                 * @endcode
                 */
                static void SetCallback(NkAssertCallback callback);

                // --------------------------------------------------------
                // Callback par défaut et invocation
                // --------------------------------------------------------

                /**
                 * @brief Callback par défaut avec comportement adaptatif debug/release
                 * @param info Référence const vers les informations de l'assertion
                 * @return Action recommandée selon le mode de build
                 * @ingroup AssertHandlerClass
                 *
                 * Ce callback implémente la politique par défaut du framework :
                 *   - Mode DEBUG : NK_BREAK (breakpoint debugger) pour inspection
                 *   - Mode RELEASE : NK_ABORT (std::abort) pour sécurité
                 *
                 * @note
                 *   - Ce callback est thread-safe et sans état interne
                 *   - Il peut être appelé directement ou via HandleAssertion()
                 *   - Le comportement exact peut varier selon la plateforme
                 *
                 * @example
                 * @code
                 * // Utilisation directe pour test du comportement par défaut
                 * nkentseu::NkAssertionInfo testInfo{
                 *     "test_expr", "test message", "test.cpp", 42, "test_func"
                 * };
                 * auto action = nkentseu::NkAssertHandler::DefaultCallback(testInfo);
                 * // En debug : action == NK_BREAK
                 * // En release : action == NK_ABORT
                 * @endcode
                 */
                static NkAssertAction DefaultCallback(const NkAssertionInfo& info);

                /**
                 * @brief Point d'entrée principal pour gestion d'une assertion échouée
                 * @param info Référence const vers les informations de l'assertion
                 * @return Action résultante après traitement par le callback
                 * @ingroup AssertHandlerClass
                 *
                 * Cette méthode est le point d'invocation depuis les macros
                 * d'assertion. Elle délègue au callback enregistré ou au default
                 * si aucun callback custom n'est défini.
                 *
                 * @note
                 *   - Thread-safe : protège l'accès au callback global
                 *   - Gère le cas nullptr pour callback (fallback vers DefaultCallback)
                 *   - Retourne l'action pour que l'appelant puisse l'exécuter
                 *
                 * @example d'usage dans une macro NK_ASSERT :
                 * @code
                 * #define NK_ASSERT(expr, msg) \
                 * do { \
                 *     if (!(expr)) { \
                 *         nkentseu::NkAssertionInfo info{#expr, msg, __FILE__, __LINE__, __func__}; \
                 *         nkentseu::NkAssertAction action = \
                 *             nkentseu::NkAssertHandler::HandleAssertion(info); \
                 *         switch (action) { \
                 *             case nkentseu::NkAssertAction::NK_BREAK: NKENTSEU_DEBUGBREAK(); break; \
                 *             case nkentseu::NkAssertAction::NK_ABORT: std::abort(); break; \
                 *             case nkentseu::NkAssertAction::NK_CONTINUE: \
                 *             case nkentseu::NkAssertAction::NK_IGNORE: \
                 *             case nkentseu::NkAssertAction::NK_IGNORE_ALL: break; \
                 *         } \
                 *     } \
                 * } while(0)
                 * @endcode
                 */
                static NkAssertAction HandleAssertion(const NkAssertionInfo& info);

            // --------------------------------------------------------
            // Section privée : implémentation interne
            // --------------------------------------------------------
            private:
                /**
                 * @brief Variable statique stockant le callback courant
                 * @note Initialisée à nullptr : signifie "utiliser DefaultCallback"
                 * @internal
                 */
                static NkAssertCallback s_callback;
        };

        /** @} */ // End of AssertHandlerClass

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKASSERTHANDLER_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKASSERTHANDLER.H
// =============================================================================
// Ce fichier fournit le gestionnaire global pour le système d'assertions.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration de base au démarrage de l'application
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertHandler.h"
    #include "NKCore/Assert/NkDebugBreak.h"
    #include "NKCore/NkFoundationLog.h"

    // Handler debug : breakpoint + logging
    nkentseu::NkAssertAction DebugCallback(const nkentseu::NkAssertionInfo& info) {
        NK_FOUNDATION_LOG_ERROR(
            "[DEBUG ASSERT] %s\n  at %s:%d in %s\n  message: %s",
            info.expression,
            info.file,
            info.line,
            info.function,
            info.message ? info.message : "(none)"
        );
        // Break into debugger for immediate inspection
        return nkentseu::NkAssertAction::NK_BREAK;
    }

    // Handler release : logging + abort contrôlé
    nkentseu::NkAssertAction ReleaseCallback(const nkentseu::NkAssertionInfo& info) {
        NK_FOUNDATION_LOG_CRITICAL(
            "[RELEASE ASSERT] %s failed at %s:%d - ABORTING",
            info.expression,
            info.file,
            info.line
        );
        // Abort to avoid corrupted state propagation
        return nkentseu::NkAssertAction::NK_ABORT;
    }

    // Configuration conditionnelle au démarrage
    void InitializeAssertionSystem() {
        #if defined(NKENTSEU_DEBUG)
            nkentseu::NkAssertHandler::SetCallback(DebugCallback);
            NK_FOUNDATION_LOG_INFO("Assertions: Debug mode with breakpoint handler");
        #else
            nkentseu::NkAssertHandler::SetCallback(ReleaseCallback);
            NK_FOUNDATION_LOG_INFO("Assertions: Release mode with abort handler");
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Handler personnalisé avec filtrage par module
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertHandler.h"
    #include <unordered_map>
    #include <string>

    // Registry de configuration par module
    class AssertModuleConfig {
    public:
        // Définir la politique pour un module spécifique
        static void SetModulePolicy(const char* moduleName, nkentseu::NkAssertAction policy) {
            GetPolicyMap()[moduleName] = policy;
        }

        // Handler qui applique les politiques par module
        static nkentseu::NkAssertAction ModuleAwareCallback(const nkentseu::NkAssertionInfo& info) {
            auto& policies = GetPolicyMap();

            // Recherche de politique spécifique au module/fichier
            std::string key = ExtractModuleName(info.file);
            auto it = policies.find(key);

            if (it != policies.end()) {
                // Politique explicite : appliquer directement
                if (it->second == nkentseu::NkAssertAction::NK_IGNORE) {
                    return nkentseu::NkAssertAction::NK_IGNORE;
                }
                // Autres politiques : logging + action
                LogWithPolicy(info, it->second);
                return it->second;
            }

            // Fallback vers handler par défaut
            return nkentseu::NkAssertHandler::DefaultCallback(info);
        }

    private:
        static std::unordered_map<std::string, nkentseu::NkAssertAction>& GetPolicyMap() {
            static std::unordered_map<std::string, nkentseu::NkAssertAction> policies;
            return policies;
        }

        static std::string ExtractModuleName(const nk_char* filePath) {
            // Extraction simplifiée : nom du dossier parent du fichier
            // Implémentation réelle : parser de chemin cross-platform
            return "unknown";
        }

        static void LogWithPolicy(const nkentseu::NkAssertionInfo& info, nkentseu::NkAssertAction policy) {
            // Logging adapté à la politique...
        }
    };

    // Configuration des modules au démarrage
    void SetupModulePolicies() {
        using namespace nkentseu;

        // Module expérimental : ignorer les assertions non-critiques
        AssertModuleConfig::SetModulePolicy("Experimental", NkAssertAction::NK_CONTINUE);

        // Module critique : toujours abort sur échec
        AssertModuleConfig::SetModulePolicy("Security", NkAssertAction::NK_ABORT);

        // Installer le handler module-aware
        NkAssertHandler::SetCallback(AssertModuleConfig::ModuleAwareCallback);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Chaînage de handlers pour logging multiple
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertHandler.h"
    #include "NKCore/NkFoundationLog.h"

    // Handler qui loggue vers plusieurs destinations
    class MultiLoggerAssertHandler {
    public:
        // Enregistrer un handler dans la chaîne
        static void AddHandler(nkentseu::NkAssertCallback handler) {
            if (handler) {
                GetHandlerChain().push_back(handler);
            }
        }

        // Callback principal qui invoque toute la chaîne
        static nkentseu::NkAssertAction ChainCallback(const nkentseu::NkAssertionInfo& info) {
            nkentseu::NkAssertAction finalAction = nkentseu::NkAssertAction::NK_CONTINUE;

            // Invoquer chaque handler de la chaîne
            for (auto handler : GetHandlerChain()) {
                if (handler) {
                    auto action = handler(info);
                    // Dernière action non-CONTINUE devient l'action finale
                    if (action != nkentseu::NkAssertAction::NK_CONTINUE) {
                        finalAction = action;
                    }
                }
            }

            return finalAction;
        }

    private:
        static std::vector<nkentseu::NkAssertCallback>& GetHandlerChain() {
            static std::vector<nkentseu::NkAssertCallback> chain;
            return chain;
        }
    };

    // Handlers individuels pour différentes destinations
    nkentseu::NkAssertAction ConsoleLogger(const nkentseu::NkAssertionInfo& info) {
        fprintf(stderr, "[ASSERT] %s:%d: %s\n", info.file, info.line, info.expression);
        return nkentseu::NkAssertAction::NK_CONTINUE;  // Continuer vers next handler
    }

    nkentseu::NkAssertAction FileLogger(const nkentseu::NkAssertionInfo& info) {
        // Append to assertion log file...
        return nkentseu::NkAssertAction::NK_CONTINUE;
    }

    nkentseu::NkAssertAction DebuggerHandler(const nkentseu::NkAssertionInfo& info) {
        #if defined(NKENTSEU_DEBUG)
            NKENTSEU_DEBUGBREAK();
            return nkentseu::NkAssertAction::NK_BREAK;
        #else
            return nkentseu::NkAssertAction::NK_CONTINUE;
        #endif
    }

    // Installation de la chaîne de handlers
    void SetupChainedHandlers() {
        using namespace nkentseu;

        // Ajouter les handlers dans l'ordre d'exécution
        MultiLoggerAssertHandler::AddHandler(ConsoleLogger);
        MultiLoggerAssertHandler::AddHandler(FileLogger);
        MultiLoggerAssertHandler::AddHandler(DebuggerHandler);

        // Installer le callback chaîne comme handler global
        NkAssertHandler::SetCallback(MultiLoggerAssertHandler::ChainCallback);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Macro NK_ASSERT avec intégration du handler
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertHandler.h"
    #include "NKCore/Assert/NkDebugBreak.h"
    #include "NKCore/NkBuiltin.h"

    // Macro principale d'assertion du framework
    #define NK_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            /* Capture du contexte d'assertion *\/ \
            nkentseu::NkAssertionInfo info; \
            info.expression = #expr; \
            info.message = msg; \
            info.file = NKENTSEU_BUILTIN_FILE; \
            info.line = static_cast<nkentseu::nk_int32>(NKENTSEU_BUILTIN_LINE); \
            info.function = NKENTSEU_BUILTIN_FUNCTION; \
            \
            /* Invocation du handler global *\/ \
            nkentseu::NkAssertAction action = \
                nkentseu::NkAssertHandler::HandleAssertion(info); \
            \
            /* Exécution de l'action retournée *\/ \
            switch (action) { \
                case nkentseu::NkAssertAction::NK_BREAK: \
                    NKENTSEU_DEBUGBREAK(); \
                    break; \
                case nkentseu::NkAssertAction::NK_ABORT: \
                    std::abort(); \
                    break; \
                case nkentseu::NkAssertAction::NK_CONTINUE: \
                case nkentseu::NkAssertAction::NK_IGNORE: \
                case nkentseu::NkAssertAction::NK_IGNORE_ALL: \
                    /* Continuer l'exécution *\/ \
                    break; \
            } \
        } \
    } while (0)

    // Variante "soft" qui ne peut pas abort (pour code non-critique)
    #define NK_VERIFY(expr, msg) \
    do { \
        if (!(expr)) { \
            nkentseu::NkAssertionInfo info{#expr, msg, NKENTSEU_BUILTIN_FILE, \
                static_cast<nkentseu::nk_int32>(NKENTSEU_BUILTIN_LINE), \
                NKENTSEU_BUILTIN_FUNCTION}; \
            \
            nkentseu::NkAssertAction action = \
                nkentseu::NkAssertHandler::HandleAssertion(info); \
            \
            /* Forcer CONTINUE même si handler retourne ABORT *\/ \
            if (action == nkentseu::NkAssertAction::NK_ABORT) { \
                action = nkentseu::NkAssertAction::NK_CONTINUE; \
            } \
            \
            if (action == nkentseu::NkAssertAction::NK_BREAK) { \
                NKENTSEU_DEBUGBREAK(); \
            } \
        } \
    } while (0)

    // Usage
    void ProcessData(void* buffer, size_t size) {
        // Assertion critique : peut abort en release
        NK_ASSERT(buffer != nullptr, "Buffer pointer must be valid");

        // Vérification non-critique : continue toujours
        NK_VERIFY(size <= MAX_BUFFER_SIZE, "Buffer size may cause issues");

        // ... traitement ...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Tests unitaires avec mock du handler d'assertions
// -----------------------------------------------------------------------------
/*
    #include "NKCore/Assert/NkAssertHandler.h"
    #include <vector>
    #include <memory>

    // Fixture pour tests d'assertions
    class AssertTestFixture {
    public:
        // Sauvegarder le handler original avant test
        void SetUp() {
            m_originalCallback = nkentseu::NkAssertHandler::GetCallback();
            m_capturedAssertions.clear();
        }

        // Restaurer le handler original après test
        void TearDown() {
            nkentseu::NkAssertHandler::SetCallback(m_originalCallback);
        }

        // Installer un mock handler qui capture les assertions
        void EnableCapture() {
            nkentseu::NkAssertHandler::SetCallback(
                [](const nkentseu::NkAssertionInfo& info) -> nkentseu::NkAssertAction {
                    GetCapturedList().push_back(info);
                    // Retourner CONTINUE pour ne pas interrompre le test
                    return nkentseu::NkAssertAction::NK_CONTINUE;
                }
            );
        }

        // Vérifier qu'une assertion spécifique a été déclenchée
        bool WasAssertionTriggered(const char* expectedExpr) const {
            for (const auto& info : m_capturedAssertions) {
                if (info.expression && strcmp(info.expression, expectedExpr) == 0) {
                    return true;
                }
            }
            return false;
        }

        // Obtenir le nombre d'assertions capturées
        size_t GetAssertionCount() const {
            return m_capturedAssertions.size();
        }

        // Obtenir la dernière assertion capturée
        const nkentseu::NkAssertionInfo* GetLastAssertion() const {
            if (m_capturedAssertions.empty()) {
                return nullptr;
            }
            return &m_capturedAssertions.back();
        }

    private:
        static std::vector<nkentseu::NkAssertionInfo>& GetCapturedList() {
            // Note: dans un vrai test, utiliser un membre d'instance
            // Ici simplifié pour l'exemple
            static std::vector<nkentseu::NkAssertionInfo> captured;
            return captured;
        }

        nkentseu::NkAssertCallback m_originalCallback;
        std::vector<nkentseu::NkAssertionInfo> m_capturedAssertions;
    };

    // Test unitaire exemple
    bool Test_NullPointerAssertion() {
        AssertTestFixture fixture;
        fixture.SetUp();
        fixture.EnableCapture();

        // Code qui devrait déclencher une assertion
        void* nullPtr = nullptr;
        NK_ASSERT(nullPtr != nullptr, "Pointer should not be null");

        // Vérifications
        bool passed = fixture.WasAssertionTriggered("nullPtr != nullptr") &&
                     fixture.GetAssertionCount() == 1;

        fixture.TearDown();
        return passed;
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================