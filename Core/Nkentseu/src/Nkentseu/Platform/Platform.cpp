#include "pch.h"
#include "Platform.h"

#if defined(NKENTSEU_PLATFORM_LINUX)
    #include <fstream>

    bool nkentseu::platform::IsWSL() noexcept {
        std::ifstream version("/proc/version");
        if(version) {
            std::string content;
            std::getline(version, content);
            return content.find("Microsoft") != std::string::npos || 
                   content.find("WSL") != std::string::npos;
        }
        return false;
    }
#endif

#include "PlatformDetection.h"
#include <fstream>
#include <cstring>
#include <locale>
#include <codecvt>

#include <algorithm> // Pour std::transform
#include <cctype>    // Pour std::tolower

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #include <windows.h>
    #include <regstr.h>
#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_BSD)
    #include <unistd.h>
    #include <sys/statvfs.h>
    #include <unicode/ucnv.h> // Requiert libicu-dev
#endif

using namespace nkentseu::platform;

namespace nkentseu {
    namespace platform {

        std::string ToString(bool value) {
            return value ? "True" : "False";
        }

        std::string UnicodeToUTF8(uint64 unicode_code) {
            #if defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_BSD)
                UErrorCode status = U_ZERO_ERROR;
                char buffer[6] = {0};
                
                ucnv_fromUChars(
                    ucnv_open("UTF-8", &status),
                    buffer,
                    sizeof(buffer),
                    reinterpret_cast<const UChar*>(&unicode_code),
                    1,
                    &status
                );
                
                if(U_SUCCESS(status)) {
                    return std::string(buffer);
                }
            #else
                std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
                return conv.to_bytes(static_cast<char32_t>(unicode_code));
            #endif
            return "";
        }
        
        std::string ToLower(std::string str) {
            std::transform(str.begin(), str.end(), str.begin(), 
                [](unsigned char c){ return std::tolower(c); });
            return str;
        }

        ///////////////////////////////////////////////////////////////////////////////
        // Implémentations des méthodes
        ///////////////////////////////////////////////////////////////////////////////

        Arch Platform::GetArchitecture() {
            #if defined(NKENTSEU_ARCH_X64)
                return Arch::X64;
            #elif defined(NKENTSEU_ARCH_X86)
                return Arch::X86;
            #elif defined(NKENTSEU_ARCH_ARM64)
                return Arch::ARM64;
            #elif defined(NKENTSEU_ARCH_ARM)
                return Arch::ARM;
            #elif defined(NKENTSEU_ARCH_MIPS)
                return Arch::MIPS;
            #elif defined(NKENTSEU_ARCH_PPC)
                return Arch::PPC;
            #elif defined(__riscv)
                return Arch::RISCV;
            #else
                return Arch::Unknown;
            #endif
        }

        PlatformInfo Platform::GetPlatformInfo() {
            PlatformInfo info{};
            info.architecture = GetArchitecture();
            info.environment = Environment::Native;
            info.display = DisplaySystem::Unknown;
            info.isMobile = false;
            info.isDesktop = false;
            info.isEmbedded = false;
            info.isVirtualized = IsVirtualized();
            info.osName = GetOSName();
            info.displayServer = GetDisplayServerInfo();

            // Détection plateforme principale
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                info.platform = PlatformType::Windows;
                info.display = DisplaySystem::Win32;
                info.isDesktop = true;
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                info.platform = PlatformType::Linux;
                info.isDesktop = !IsWSL();
                #if defined(NKENTSEU_DISPLAY_WAYLAND)
                    info.display = DisplaySystem::Wayland;
                #elif defined(NKENTSEU_DISPLAY_X11)
                    info.display = DisplaySystem::X11;
                #elif defined(NKENTSEU_DISPLAY_XCB)
                    info.display = DisplaySystem::XCB;
                #endif
            #elif defined(NKENTSEU_PLATFORM_MACOS)
                info.platform = PlatformType::MacOS;
                info.display = DisplaySystem::Metal;
                info.isDesktop = true;
            #elif defined(NKENTSEU_PLATFORM_ANDROID)
                info.platform = PlatformType::Android;
                info.display = DisplaySystem::Vulkan;
                info.isMobile = true;
            #elif defined(NKENTSEU_PLATFORM_IOS)
                info.platform = PlatformType::iOS;
                info.display = DisplaySystem::Metal;
                info.isMobile = true;
            #elif defined(NKENTSEU_PLATFORM_WEB)
                info.platform = PlatformType::Web;
                info.display = DisplaySystem::WebGL;
            #elif defined(NKENTSEU_ENV_NINTENDO_SWITCH)
                info.platform = PlatformType::NintendoSwitch;
                info.display = DisplaySystem::Console;
                info.isEmbedded = true;
            #endif

            // Détection environnement spécifique
            #if defined(NKENTSEU_ENV_WSL)
                info.environment = Environment::WSL;
            #elif defined(NKENTSEU_ENV_ANDROID_NDK)
                info.environment = Environment::AndroidNDK;
            #elif defined(__EMSCRIPTEN__)
                info.environment = Environment::Emscripten;
            #elif defined(__QNX__)
                info.platform = PlatformType::Embedded;
            #endif

            return info;
        }

        std::string Platform::GetOSName() {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return "Windows";
            #elif defined(NKENTSEU_PLATFORM_LINUX)
                #if defined(NKENTSEU_PLATFORM_WSL)
                    return "Windows Subsystem for Linux";
                #else
                    // Lecture du fichier /etc/os-release
                    std::ifstream osRelease("/etc/os-release");
                    if(osRelease) {
                        std::string line;
                        while(std::getline(osRelease, line)) {
                            if(line.find("PRETTY_NAME=") != std::string::npos) {
                                return line.substr(13, line.size() - 14);
                            }
                        }
                    }
                    return "Linux";
                #endif
            #elif defined(NKENTSEU_PLATFORM_MACOS)
                return "macOS";
            #elif defined(NKENTSEU_PLATFORM_ANDROID)
                return "Android";
            #elif defined(NKENTSEU_PLATFORM_WEB)
                return "Web";
            #else
                return "Unknown";
            #endif
        }

        Version Platform::GetVersion() {
            return Version{
                NKENTSEU_VERSION_MAJOR,
                NKENTSEU_VERSION_MINOR,
                NKENTSEU_VERSION_PATCH,
                NKENTSEU_VERSION_STRING
            };
        }

        bool Platform::IsMobile() {
            auto info = GetPlatformInfo();
            return info.platform == PlatformType::Android || 
                info.platform == PlatformType::iOS;
        }

        bool Platform::IsConsole() {
            auto info = GetPlatformInfo();
            return info.platform == PlatformType::NintendoSwitch ||
                info.platform == PlatformType::PlayStation4 ||
                info.platform == PlatformType::Xbox;
        }

        bool Platform::IsVirtualized() {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Vérification via le registre Windows
                HKEY hKey;
                if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\SystemInformation", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    
                    char buffer[256];
                    DWORD bufferSize = sizeof(buffer);
                    
                    if(RegQueryValueEx(hKey, "SystemProductName", NULL, NULL, 
                        (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                        
                        std::string productName(buffer);
                        return productName.find("Virtual") != std::string::npos ||
                            productName.find("VMware") != std::string::npos ||
                            productName.find("QEMU") != std::string::npos;
                    }
                }
                return false;

            #elif defined(NKENTSEU_PLATFORM_LINUX)
                // Vérification Docker
                std::ifstream docker("/.dockerenv");
                if(docker.good()) return true;

                // Vérification hyperviseur via /proc/cpuinfo
                std::ifstream cpuinfo("/proc/cpuinfo");
                std::string line;
                while(std::getline(cpuinfo, line)) {
                    if(line.find("hypervisor") != std::string::npos)
                        return true;
                }

                // Vérification cgroups
                std::ifstream cgroup("/proc/self/cgroup");
                while(std::getline(cgroup, line)) {
                    if(line.find("docker") != std::string::npos ||
                    line.find("kubepods") != std::string::npos)
                        return true;
                }
                return false;

            #else
                return false;
            #endif
        }

        std::string Platform::GetDisplayServerInfo() {
            #if defined(NKENTSEU_PLATFORM_LINUX)
                if(std::getenv("WAYLAND_DISPLAY"))
                    return "Wayland";
                if(std::getenv("DISPLAY"))
                    return "X11";
                return "Unknown";
            #elif defined(NKENTSEU_PLATFORM_WINDOWS)
                return "Win32";
            #elif defined(NKENTSEU_PLATFORM_WEB)
                return "WebGL";
            #else
                return "N/A";
            #endif
        }
    }
}