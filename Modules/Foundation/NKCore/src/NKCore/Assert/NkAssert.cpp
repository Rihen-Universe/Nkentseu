// =============================================================================
// NKCore/Assert/NkAssert.cpp
// Implémentation du gestionnaire d'assertions et callbacks par défaut.
//
// Design :
//  - Définition de la variable statique sAssertCallback pour stockage du handler
//  - Implémentation des méthodes statiques de NkAssertHandler
//  - Callback par défaut avec logging stderr et comportement debug/release adaptatif
//  - Thread-safety : accès au callback protégé (à compléter selon besoins)
//  - Intégration avec NkDebugBreak.h pour breakpoints debugger
//
// Intégration :
//  - Utilise NKCore/Assert/NkAssertHandler.h pour déclaration de la classe
//  - Utilise NKCore/Assert/NkDebugBreak.h pour NKENTSEU_DEBUG_BREAK()
//  - Utilise <cstdio> pour fprintf vers stderr
//  - Utilise <cstdlib> pour ::abort()
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRECOMPILED HEADER (requis pour tous les fichiers .cpp du projet)
// -------------------------------------------------------------------------
#include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES DU MODULE
// -------------------------------------------------------------------------
#include "NkAssert.h"
#include "NkAssertHandler.h"
#include "NKCore/NkTypes.h"
#include "NKCore/NkMacros.h"
#include "NKCore/NkPlatform.h"

// En-têtes standards pour I/O et contrôle de flux
#include <cstdio>
#include <cstdlib>

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    // ====================================================================
    // VARIABLE STATIQUE INTERNE : STOCKAGE DU CALLBACK
    // ====================================================================
    // Initialisée au callback par défaut : comportement safe sans configuration

    /**
     * @brief Variable statique stockant le callback courant d'assertions
     * @internal
     * @note
     *   - Initialisée à &NkAssertHandler::DefaultCallback pour sécurité
     *   - Modifiable via NkAssertHandler::SetCallback()
     *   - Accès non protégé : appeler SetCallback() depuis un contexte single-thread
     */
    static NkAssertCallback sAssertCallback = &NkAssertHandler::DefaultCallback;

    // ====================================================================
    // IMPLÉMENTATION DES MÉTHODES DE NKASSERTHANDLER
    // ====================================================================

    /**
     * @brief Récupère le callback actuellement enregistré
     * @return Pointeur vers la fonction callback courante
     * @note Thread-safe en lecture seule (pas de modification de sAssertCallback)
     */
    NkAssertCallback NkAssertHandler::GetCallback() {
        return sAssertCallback;
    }

    /**
     * @brief Définit un callback personnalisé pour gestion d'assertions
     * @param callback Pointeur vers la fonction handler à installer
     * @note
     *   - Passer nullptr restaure le callback par défaut
     *   - Non thread-safe : protéger l'appel si utilisé depuis plusieurs threads
     *   - Le nouveau callback prend effet immédiatement pour les assertions suivantes
     */
    void NkAssertHandler::SetCallback(NkAssertCallback callback) {
        sAssertCallback = callback ? callback : &DefaultCallback;
    }

    /**
     * @brief Callback par défaut avec comportement adaptatif debug/release
     * @param info Référence const vers les informations de l'assertion échouée
     * @return NK_BREAK en debug, NK_ABORT en release
     *
     * @note
     *   - Affiche les détails de l'assertion sur stderr pour diagnostic
     *   - Format de sortie lisible et parseable pour outils externes
     *   - Le comportement exact dépend de la définition de NK_DEBUG
     *
     * @warning
     *   En mode release, retourne NK_ABORT qui mènera à ::abort() :
     *   assurer que les ressources critiques sont libérées via RAII
     *   avant que les assertions ne puissent échouer.
     */
    NkAssertAction NkAssertHandler::DefaultCallback(const NkAssertionInfo& info) {
        // Formatage structuré pour lisibilité et parsing automatique
        std::fprintf(stderr, "\n=== ASSERTION FAILED ===\n");
        std::fprintf(stderr, "Expression: %s\n", info.expression);

        // Message personnalisé optionnel
        if (info.message && info.message[0] != '\0') {
            std::fprintf(stderr, "Message: %s\n", info.message);
        }

        // Contexte source pour localisation rapide dans l'IDE
        std::fprintf(stderr, "File: %s:%d\n", info.file, info.line);
        std::fprintf(stderr, "Function: %s\n", info.function);
        std::fprintf(stderr, "========================\n\n");

        // Flush pour garantir l'affichage avant break/abort
        std::fflush(stderr);

        // Décision selon le mode de build
        #if defined(NK_DEBUG)
            // Mode debug : breakpoint pour inspection interactive au debugger
            return NkAssertAction::NK_BREAK;
        #else
            // Mode release : abort pour éviter propagation d'état corrompu
            return NkAssertAction::NK_ABORT;
        #endif
    }

    /**
     * @brief Point d'entrée principal pour gestion d'une assertion échouée
     * @param info Référence const vers les informations de l'assertion
     * @return Action résultante après traitement par le callback enregistré
     *
     * @note
     *   - Délègue simplement au callback courant (sAssertCallback)
     *   - Permet l'override du comportement via SetCallback()
     *   - Thread-safe si le callback lui-même est thread-safe
     */
    NkAssertAction NkAssertHandler::HandleAssertion(const NkAssertionInfo& info) {
        return sAssertCallback(info);
    }

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET EXTENSIONS POSSIBLES
// =============================================================================
//
// Thread-safety :
//   L'accès à sAssertCallback n'est pas protégé par un mutex. Pour un usage
//   multithreadé avec modification dynamique du callback :
//
//   @code
//   #include <atomic>
//   static std::atomic<NkAssertCallback> sAssertCallback{&DefaultCallback};
//
//   NkAssertCallback NkAssertHandler::GetCallback() {
//       return sAssertCallback.load(std::memory_order_acquire);
//   }
//
//   void NkAssertHandler::SetCallback(NkAssertCallback callback) {
//       sAssertCallback.store(
//           callback ? callback : &DefaultCallback,
//           std::memory_order_release
//       );
//   }
//   @endcode
//
// Logging avancé :
//   Pour intégrer avec le système de logging du framework :
//
//   @code
//   #include "NKCore/NkFoundationLog.h"
//
//   NkAssertAction NkAssertHandler::DefaultCallback(const NkAssertionInfo& info) {
//       NK_FOUNDATION_LOG_ERROR(
//           "Assertion failed: %s at %s:%d in %s - %s",
//           info.expression,
//           info.file,
//           info.line,
//           info.function,
//           info.message ? info.message : "(no message)"
//       );
//       // ... suite du traitement ...
//   }
//   @endcode
//
// Platform-specific enhancements :
//   Sur Windows, ajouter OutputDebugString pour affichage dans le debugger :
//
//   @code
//   #if defined(NKENTSEU_PLATFORM_WINDOWS)
//       #include <windows.h>
//       char buffer[1024];
//       std::snprintf(buffer, sizeof(buffer),
//           "ASSERT: %s (%s:%d)\n",
//           info.expression, info.file, info.line);
//       OutputDebugStringA(buffer);
//   #endif
//   @endcode
//
// =============================================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================