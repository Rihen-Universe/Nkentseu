// @File Platform.h
// @Description Détection et propriétés de la plateforme d'exécution
// @Author [Votre Nom]
// @Date [AAAA-MM-JJ]
// @License Rihen

#pragma once

#include "Export.h"
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace nkentseu {
    namespace platform {

        ///////////////////////////////////////////////////////////////////////////////
        // Types fondamentaux
        ///////////////////////////////////////////////////////////////////////////////

        // Entiers non signés
        using uint8  = unsigned char;
        using uint16 = unsigned short;
        using uint32 = unsigned int;
        using uintl32 = unsigned long int;
        using uint64 = unsigned long long int;
        using usize  = uint64;

        // Entiers signés
        using int8  = signed char;
        using int16 = signed short int;
        using int32 = signed int;
        using int64 = signed long long int;
        using usize_gpu = signed long long;

        // Flottants
        using float32  = float;
        using float64  = double;
        using float80  = long double; // Support limité

        // Booléens
        using Boolean = uint8;
        using bool32 = int32;
        static constexpr Boolean True  = 1;
        static constexpr Boolean False = 0;

        // Utilitaires
        using ARGV = std::vector<std::string>;

        ///////////////////////////////////////////////////////////////////////////////
        // Constantes globales
        ///////////////////////////////////////////////////////////////////////////////
        namespace constants {
            static constexpr uint64 INVALID_ID_UINT64 = 18446744073709551615UL;
            static constexpr uint32 INVALID_ID        = 4294967295U;
            static constexpr uint16 INVALID_ID_UINT16 = 65535U;
            static constexpr uint8  INVALID_ID_UINT8  = 255U;
            static constexpr void*  Null              = nullptr;
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Macros utilitaires
        ///////////////////////////////////////////////////////////////////////////////
        #define BIT(x)          (1 << (x))
        #define STDPH(v)        std::placeholders::_##v
        #define STR_BOOL(b)     ((b) ? "True" : "False")
        #define KCLAMP(v,mi,ma) (((v) <= (mi)) ? (mi) : (((v) >= (ma)) ? (ma) : (v)))
        #define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))

        ///////////////////////////////////////////////////////////////////////////////
        // Utilitaires mémoire
        ///////////////////////////////////////////////////////////////////////////////
        namespace memory {
            NKENTSEU_INLINE_API uint64 GetAligned(uint64 operand, uint64 granularity) {
                return (operand + (granularity - 1)) & ~(granularity - 1);
            }

            template <typename T>
            T AlignUp(T value, T alignment) {
                return (value + alignment - 1) & ~(alignment - 1);
            }

            // Conversions de tailles
            constexpr uint64 GiB(uint64 amount) { return amount * 1024ULL * 1024ULL * 1024ULL; }
            constexpr uint64 MiB(uint64 amount) { return amount * 1024ULL * 1024ULL; }
            constexpr uint64 KiB(uint64 amount) { return amount * 1024ULL; }
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Énumérations
        ///////////////////////////////////////////////////////////////////////////////

        enum class NKENTSEU_API Arch {
            Unknown, X86, X64, ARM, ARM64, MIPS, PPC, RISCV
        };

        enum class NKENTSEU_API PlatformType {
            Unknown,
            Windows, Linux, MacOS, Android, iOS,
            FreeBSD, OpenBSD, NetBSD, DragonFlyBSD,
            NintendoDS, NintendoSwitch, PlayStation4, Xbox,
            Web, Unix, Embedded
        };

        enum class NKENTSEU_API DisplaySystem {
            Unknown, X11, XCB, Wayland, DirectFB, Console, 
            WebGL, Win32, Metal, Vulkan
        };

        enum class NKENTSEU_API Environment {
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

    } // namespace platform
} // namespace nkentseu

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.