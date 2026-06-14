// =============================================================================
// Core/NkoungConfig.cpp
// Implémentation config globale.
// =============================================================================
#include "NkoungConfig.h"
#include "NKLogger/NkLog.h"

namespace nkoung {
    namespace memory {
        nkentseu::memory::NkAllocator* gDefaultAllocator = nullptr;
        nkentseu::memory::NkAllocator* gResourceAllocator = nullptr;
        nkentseu::memory::NkAllocator* gScratchAllocator = nullptr;

        void InitializeAllocators() noexcept {
            // Pour MVP, on peut utiliser les allocateurs par défaut du système.
            // À terme : créer des NkArenaAllocator ou NkLinearAllocator typés.
            gDefaultAllocator = &nkentseu::memory::NkGetDefaultAllocator();
            gResourceAllocator = &nkentseu::memory::NkGetDefaultAllocator();
            gScratchAllocator = &nkentseu::memory::NkGetDefaultAllocator();
            NKOUNG_LOG_INFO("Allocators initialized");
        }

        void ShutdownAllocators() noexcept {
            // Si des allocateurs spécialisés sont créés, les supprimer ici.
            gDefaultAllocator = nullptr;
            gResourceAllocator = nullptr;
            gScratchAllocator = nullptr;
            NKOUNG_LOG_INFO("Allocators shutdown");
        }
    }  // namespace memory

    static nkentseu::NkLogger* gLogger = &logger;

    nkentseu::NkLogger& GetLogger() noexcept {
        if (!gLogger) {
            // Crée un logger par défaut si pas encore fait.
            // En production, initialiser avec des sinks (console, fichier, etc.).
            static nkentseu::NkLogger defaultLogger("Nkoung");
            gLogger = &defaultLogger;
        }
        return *gLogger;
    }

    namespace globals {
        nkentseu::NkString gDataPath = "";
    }  // namespace globals

}  // namespace nkoung
