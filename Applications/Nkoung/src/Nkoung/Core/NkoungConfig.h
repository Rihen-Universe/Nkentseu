// =============================================================================
// Core/NkoungConfig.h
// Configuration globale Nkoung : allocateurs, logger, constantes.
// =============================================================================
#pragma once

#ifndef NKOUNG_CONFIG_H
#define NKOUNG_CONFIG_H

#include "NKCore/NkTypes.h"
#include "NKMemory/NKMemory.h"
#include "NKLogger/NkLog.h"

namespace nkoung {

    // =========================================================================
    // Configuration mémoire Nkoung
    // =========================================================================
    namespace memory {
        /// Allocateur primaire pour les conteneurs/objets jeu Nkoung.
        /// À initialiser une fois au démarrage de la plateforme.
        extern nkentseu::memory::NkAllocator* gDefaultAllocator;

        /// Allocateur pour les ressources (textures, polices, sons, niveaux).
        /// Cycle de vie = lors du chargement d'une scène/jeu.
        extern nkentseu::memory::NkAllocator* gResourceAllocator;

        /// Allocateur pour scratch work temporaire (physics frame, anim update).
        /// Tous les X frames ou end-of-frame, peut être reset.
        extern nkentseu::memory::NkAllocator* gScratchAllocator;

        /// Initialise les allocateurs. Appelé une fois avant la boucle principale.
        void InitializeAllocators() noexcept;

        /// Libère les allocateurs. Appelé avant la fermeture de l'app.
        void ShutdownAllocators() noexcept;
    }  // namespace memory

    // =========================================================================
    // Logger Nkoung
    // =========================================================================
    extern nkentseu::NkLogger& GetLogger() noexcept;

    // Macros de log pratiques.
    #define NKOUNG_LOG_INFO(msg)     nkoung::GetLogger().Info(msg)
    #define NKOUNG_LOG_WARN(msg)     nkoung::GetLogger().Warn(msg)
    #define NKOUNG_LOG_ERROR(msg)    nkoung::GetLogger().Error(msg)
    #define NKOUNG_LOG_INFOF(fmt, ...) nkoung::GetLogger().Infof(fmt, __VA_ARGS__)
    #define NKOUNG_LOG_WARNF(fmt, ...) nkoung::GetLogger().Warnf(fmt, __VA_ARGS__)
    #define NKOUNG_LOG_ERRORF(fmt, ...) nkoung::GetLogger().Errorf(fmt, __VA_ARGS__)

    // =========================================================================
    // Constantes globales Nkoung
    // =========================================================================
    namespace globals {
        // Résolution par défaut de la plateforme Nkoung.
        static constexpr nkentseu::nk_uint32 DEFAULT_WINDOW_WIDTH = 1280;
        static constexpr nkentseu::nk_uint32 DEFAULT_WINDOW_HEIGHT = 720;
        static constexpr bool                 WINDOW_RESIZABLE = true;

        // Taux de rafraîchissement cible.
        static constexpr nkentseu::float32 TARGET_FPS = 60.0f;
        static constexpr nkentseu::float32 MAX_DELTA_TIME = 0.1f;  // Clamp dt pour éviter jumps lors de lag.

        // Noms de plates-formes.
        static constexpr const char* PLATFORM_NAME = "Nkoung";
        static constexpr const char* PLATFORM_VERSION = "0.2.0";

        // Chemin des données (niveaux, assets, config).
        // À définir dynamiquement selon la plateforme (exe dir, APK, etc.).
        extern nkentseu::NkString gDataPath;
    }  // namespace globals

}  // namespace nkoung

#endif // NKOUNG_CONFIG_H
