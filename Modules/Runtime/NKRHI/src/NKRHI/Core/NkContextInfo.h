#pragma once
// NkContextInfo.h - Infos runtime sur un device/contexte RHI
#include "NKRHI/Core/NkGraphicsApi.h"

namespace nkentseu {

    struct NkContextInfo {
        NkGraphicsApi api              = NkGraphicsApi::NK_API_NONE;
        const char*   renderer         = "";
        const char*   vendor           = "";
        const char*   version          = "";
        uint32        vramMB           = 0;
        bool          debugMode        = false;
        bool          computeSupported = false;
        uint32        maxTextureSize   = 0;
        uint32        maxMSAASamples   = 1;
        uint32        windowWidth      = 0;
        uint32        windowHeight     = 0;
    };

} // namespace nkentseu

