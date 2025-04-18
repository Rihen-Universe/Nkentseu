/**
* @File Platform.h
* @Description Détection et propriétés de la plateforme d'exécution
* @Author TEUGUIA TADJUIDJE Rodolf Séderis
* @Date 2025-01-05
* @License Rihen
*/

#pragma once

#include "Export.h"
#include "Platform.h"
#include "Types.h"
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace nkentseu {

    ///////////////////////////////////////////////////////////////////////////////
    // Énumérations
    ///////////////////////////////////////////////////////////////////////////////

    enum class Arch {
        Unknown, X86, X64, ARM, ARM64, MIPS, PPC, RISCV
    };

    enum class PlatformType {
        Unknown,
        Windows, Linux, MacOS, Android, iOS,
        FreeBSD, OpenBSD, NetBSD, DragonFlyBSD,
        NintendoDS, NintendoSwitch, PlayStation4, Xbox,
        Web, Unix, Embedded
    };

    enum class DisplaySystem {
        Unknown, X11, XCB, Wayland, DirectFB, Console, 
        WebGL, Win32, Metal, Vulkan
    };

    enum class Environment {
        Native,
        AndroidNDK, Emscripten, Cheerp,
        WSL, Docker, VM, ConsoleSDK, Hypervisor
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Structures de données
    ///////////////////////////////////////////////////////////////////////////////

    struct NKENTSEU_API PlatformInfo {
        Arch architecture;
        PlatformType platform;
        DisplaySystem display;
        Environment environment;
        bool isMobile;
        bool isDesktop;
        bool isEmbedded;
        bool isVirtualized;
        std::string osName;
        std::string displayServer;
    };

    struct NKENTSEU_API Version {
        int major;
        int minor;
        int patch;
        std::string full;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Classe Platform
    ///////////////////////////////////////////////////////////////////////////////

    class NKENTSEU_API Platform {
    public:
        static Arch GetArchitecture();
        static PlatformInfo GetPlatformInfo();
        static std::string GetOSName();
        static Version GetVersion();
        static bool IsMobile();
        static bool IsConsole();
        static bool IsVirtualized();
        static std::string GetDisplayServerInfo();
    };

} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.