#pragma once

#include "NKWindow/Core/NkEntry.h"
#include "NKLogger/NkLogLevel.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKRHI/Core/NkDeviceInitInfo.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // =========================================================================
    // NkApplicationConfig
    // Configuration complète passée à CreateApplication().
    // Portée : fenêtre, device GPU, boucle temps réel, log, debug.
    // =========================================================================
    struct NkApplicationConfig {

        // ── Identité ─────────────────────────────────────────────────────────
        NkString appName    = "NkApp";
        NkString appVersion = "1.0.0";

        // ── Log & debug ───────────────────────────────────────────────────────
        NkLogLevel logLevel  = NkLogLevel::NK_INFO;
        bool       debugMode = false;

        // ── Boucle temps réel ─────────────────────────────────────────────────
        // fixedTimeStep : pas physique fixe (secondes). 0 = désactivé.
        float fixedTimeStep  = 1.0f / 60.0f;

        // maxDeltaTime : cap du dt pour éviter les "spiral of death".
        float maxDeltaTime   = 0.25f;

        // targetFPS : 0 = illimité (vsync géré par le device).
        nk_uint32 targetFPS  = 0;

        // ── Plateforme ────────────────────────────────────────────────────────
        NkEntryState    entryState;
        NkWindowConfig  windowConfig;
        NkDeviceInitInfo deviceInfo;

        // ── Config fichier optionnel ──────────────────────────────────────────
        // Chemin vers un fichier JSON / YAML de surcharge.
        NkString appFileConfigPath;
    };

} // namespace nkentseu
