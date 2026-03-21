//
// Created by TEUGUIA TADJUIDJE Rodolf Séderis on 2024-07-25 at 02:38:18 PM AM.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#ifndef __NKENTSEU_WINDOW_INFO_H__
#define __NKENTSEU_WINDOW_INFO_H__

#pragma once

#include <NTSCore/System.h>
#include <string>

#ifdef NKENTSEU_PLATFORM_WINDOWS
#include <Windows.h>
#elif defined(NKENTSEU_PLATFORM_LINUX_XLIB)
#elif defined(NKENTSEU_PLATFORM_LINUX_XCB)
#endif

namespace nkentseu {
    
    struct NKENTSEU_API WindowInfo {
        std::string title;
        std::string appName;
        std::string engineName;
#ifdef NKENTSEU_PLATFORM_WINDOWS
        HWND        windowHandle = nullptr;
        HINSTANCE   instance = nullptr;
#elif defined(NKENTSEU_PLATFORM_LINUX_XLIB)
#elif defined(NKENTSEU_PLATFORM_LINUX_XCB)
#endif
    };

}  //  nkentseu

#endif  // __NKENTSEU_WINDOW_INFO_H__!