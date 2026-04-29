// =============================================================================
// NKEvent/NkWindowId.h
// Identifiant unique de fenêtre — définition centralisée pour éviter la duplication.
//
// Design :
//   - Alias de type NkWindowId basé sur uint64 pour identification unique
//   - Constante NK_INVALID_WINDOW_ID pour représenter l'absence de fenêtre valide
//   - Inclusion minimale : dépend uniquement de NKMath/NKMath.h pour les types de base
//
// Usage :
//   NkWindowId id = window.GetId();
//   if (id != NK_INVALID_WINDOW_ID) { /* fenêtre valide */ }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKWINDOWID_H
#define NKENTSEU_EVENT_NKWINDOWID_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion minimale des types mathématiques de base du projet.
    // NKMath fournit les alias de types standardisés (uint64, float32, etc.)

    #include "NKMath/NKMath.h"

    namespace nkentseu {

        // =====================================================================
        // NkWindowId — Alias de type pour l'identifiant unique de fenêtre
        // =====================================================================

        /// @brief Type alias pour l'identifiant unique d'une fenêtre
        /// @note Basé sur uint64 pour garantir un espace d'adressage suffisant
        /// @note Valeur 0 réservée pour NK_INVALID_WINDOW_ID (fenêtre invalide)
        using NkWindowId = uint64;

        /// @brief Constante représentant un identifiant de fenêtre invalide
        /// @note Utilisée pour initialiser les références optionnelles de fenêtre
        /// @note Tout événement avec ce windowId est considéré comme "global" (sans fenêtre cible)
        static constexpr NkWindowId NK_INVALID_WINDOW_ID = 0;

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKWINDOWID_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKWINDOWID.H
// =============================================================================
// Ce fichier définit le type NkWindowId pour l'identification des fenêtres.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Validation d'un identifiant de fenêtre
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowId.h"

    void ProcessWindow(nkentseu::NkWindowId windowId) {
        using namespace nkentseu;

        // Vérification de validité basique
        if (windowId == NK_INVALID_WINDOW_ID) {
            NK_FOUNDATION_LOG_WARNING("Received invalid window ID");
            return;
        }

        // Utilisation sécurisée de l'identifiant
        auto* window = WindowManager::GetInstance().FindById(windowId);
        if (window) {
            window->Refresh();
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Association d'événements à une fenêtre spécifique
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowId.h"
    #include "NKEvent/NkEvent.h"

    class WindowEventRouter {
    public:
        // Dispatch d'un événement vers la fenêtre cible
        void RouteEvent(nkentseu::NkEvent& event) {
            using namespace nkentseu;

            NkWindowId targetId = event.GetWindowId();

            // Événements globaux (sans fenêtre cible)
            if (targetId == NK_INVALID_WINDOW_ID) {
                HandleGlobalEvent(event);
                return;
            }

            // Routage vers la fenêtre spécifique
            auto* window = WindowManager::GetInstance().FindById(targetId);
            if (window) {
                window->OnEvent(event);
            }
            else {
                NK_FOUNDATION_LOG_DEBUG("Event for unknown window {}", targetId);
            }
        }

    private:
        void HandleGlobalEvent(nkentseu::NkEvent& event);
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des fenêtres multiples avec mapping d'IDs
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkWindowId.h"
    #include <unordered_map>

    class MultiWindowManager {
    public:
        // Création d'une nouvelle fenêtre avec ID unique
        nkentseu::NkWindowId CreateWindow(const WindowConfig& config) {
            using namespace nkentseu;

            // Génération d'un ID unique (simple increment ou UUID selon besoin)
            static NkWindowId sNextId = 1;  // 0 réservé pour INVALID
            NkWindowId newId = sNextId++;

            // Stockage du mapping ID → fenêtre
            mWindows[newId] = std::make_unique<Window>(config, newId);

            return newId;
        }

        // Destruction et nettoyage d'une fenêtre
        void DestroyWindow(nkentseu::NkWindowId windowId) {
            using namespace nkentseu;

            if (windowId == NK_INVALID_WINDOW_ID) {
                return;  // Rien à détruire
            }

            // Suppression du mapping et libération des ressources
            mWindows.erase(windowId);
        }

        // Accès sécurisé à une fenêtre par ID
        Window* GetWindow(nkentseu::NkWindowId windowId) const {
            using namespace nkentseu;

            if (windowId == NK_INVALID_WINDOW_ID) {
                return nullptr;
            }

            auto it = mWindows.find(windowId);
            return (it != mWindows.end()) ? it->second.get() : nullptr;
        }

    private:
        std::unordered_map<nkentseu::NkWindowId, std::unique_ptr<Window>> mWindows;
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE
// =============================================================================
/*
    1. EXTENSIBILITÉ DU TYPE NkWindowId :
       - Basé sur uint64 : espace d'adressage de 2^64 valeurs, suffisant pour tout usage
       - Si besoin d'IDs structurées (ex: high32 = process, low32 = window), utiliser un wrapper

    2. GESTION DE NK_INVALID_WINDOW_ID :
       - Toujours comparer avec == ou !=, jamais avec des opérations arithmétiques
       - Documenter clairement quand un événement "global" (windowId=0) est attendu

    3. THREAD-SAFETY :
       - NkWindowId est un type trivial (uint64) : copie et lecture atomiques sur la plupart des archis
       - Pour un accès concurrent au mapping ID→Window, utiliser une synchronisation externe

    4. COMPATIBILITÉ RÉSEAU :
       - uint64 est portable et sérialisable directement (big-endian/little-endian à gérer si nécessaire)
       - Pour la transmission réseau, convertir en big-endian si le protocole l'exige
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================