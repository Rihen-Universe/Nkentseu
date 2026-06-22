// =============================================================================
// Core/MouConfig.h
// Configuration globale Mú : allocateurs, logger, constantes.
// =============================================================================
#pragma once

#ifndef MOU_CONFIG_H
#define MOU_CONFIG_H

#include "NKCore/NkTypes.h"
#include "NKMemory/NKMemory.h"
#include "NKLogger/NkLog.h"

namespace mou {

    // =========================================================================
    // Mémoire Mú (pattern Nkentseu : tout via NKMemory)
    // =========================================================================
    namespace memory {
        extern nkentseu::memory::NkAllocator* gDefaultAllocator;   // conteneurs / objets jeu
        extern nkentseu::memory::NkAllocator* gResourceAllocator;  // textures / sons / niveaux
        extern nkentseu::memory::NkAllocator* gScratchAllocator;   // scratch frame

        void InitializeAllocators() noexcept;
        void ShutdownAllocators() noexcept;
    }  // namespace memory

    // =========================================================================
    // Logger Mú
    // =========================================================================
    extern nkentseu::NkLogger& GetLogger() noexcept;

    #define MOU_LOG_INFO(msg)        mou::GetLogger().Info(msg)
    #define MOU_LOG_WARN(msg)        mou::GetLogger().Warn(msg)
    #define MOU_LOG_ERROR(msg)       mou::GetLogger().Error(msg)
    #define MOU_LOG_INFOF(fmt, ...)  mou::GetLogger().Infof(fmt, __VA_ARGS__)
    #define MOU_LOG_WARNF(fmt, ...)  mou::GetLogger().Warnf(fmt, __VA_ARGS__)
    #define MOU_LOG_ERRORF(fmt, ...) mou::GetLogger().Errorf(fmt, __VA_ARGS__)

    // =========================================================================
    // Réglages utilisateur (volume) — modifiables via l'écran Réglages, persistés.
    // Les volumes sont 0..1 ; appliqués à NKAudio quand l'audio sera câblé.
    // =========================================================================
    namespace settings {
        extern nkentseu::float32 musicVolume;   // 0..1
        extern nkentseu::float32 sfxVolume;     // 0..1
        extern bool              muted;         // coupe tout

        /// Volume effectif (tient compte de muted) pour la musique / les effets.
        nkentseu::float32 EffectiveMusic() noexcept;
        nkentseu::float32 EffectiveSfx() noexcept;

        void Load() noexcept;   // lit settings.cfg (best-effort)
        void Save() noexcept;   // écrit settings.cfg (best-effort)
    }  // namespace settings

    // =========================================================================
    // Constantes globales
    // =========================================================================
    namespace globals {
        static constexpr nkentseu::nk_uint32 DEFAULT_WINDOW_WIDTH  = 1280;
        static constexpr nkentseu::nk_uint32 DEFAULT_WINDOW_HEIGHT = 720;
        static constexpr bool                 WINDOW_RESIZABLE     = true;

        static constexpr nkentseu::float32 TARGET_FPS     = 60.0f;
        static constexpr nkentseu::float32 MAX_DELTA_TIME = 0.1f;

        static constexpr const char* PLATFORM_NAME    = "Mu";   // branding affiche : "Mu"
        static constexpr const char* PLATFORM_VERSION = "0.1.0";

        // Racine des assets (svg/levels/voice/sfx). A definir selon la plateforme.
        extern nkentseu::NkString gDataPath;
    }  // namespace globals

}  // namespace mou

#endif // MOU_CONFIG_H
