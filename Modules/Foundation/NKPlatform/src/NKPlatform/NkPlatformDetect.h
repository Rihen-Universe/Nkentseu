// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkPlatformDetect.h
// DESCRIPTION: Détection complète de plateforme et définition des macros
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORMDETECT_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORMDETECT_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include <cstddef>
#include <cstdint>

// ============================================================
// MACROS DE DÉTECTION DE SYSTÈME D'EXPLOITATION - PC/MOBILE
// ============================================================

/**
 * @brief Macro de détection de plateforme Windows
 * @def NKENTSEU_PLATFORM_WINDOWS
 * @ingroup PlatformDetection
 */

/**
 * @brief Macro de détection de plateforme Linux
 * @def NKENTSEU_PLATFORM_LINUX
 * @ingroup PlatformDetection
 */

/**
 * @brief Macro de détection de plateforme macOS
 * @def NKENTSEU_PLATFORM_MACOS
 * @ingroup PlatformDetection
 */

/**
 * @brief Macro de détection de plateforme iOS
 * @def NKENTSEU_PLATFORM_IOS
 * @ingroup PlatformDetection
 */

/**
 * @brief Macro de détection de plateforme Android
 * @def NKENTSEU_PLATFORM_ANDROID
 * @ingroup PlatformDetection
 */

// Windows
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
	#define NKENTSEU_PLATFORM_WINDOWS
	#define NKENTSEU_PLATFORM_NAME "Windows"

	#ifdef _WIN64
		#define NKENTSEU_PLATFORM_WINDOWS_64
		#define NKENTSEU_PLATFORM_VERSION "Windows 64-bit"
	#else
		#define NKENTSEU_PLATFORM_WINDOWS_32
		#define NKENTSEU_PLATFORM_VERSION "Windows 32-bit"
	#endif

	// Détection de version Windows spécifique
	#if defined(_WIN32_WINNT)
		#if _WIN32_WINNT >= 0x0A00
			#define NKENTSEU_PLATFORM_WINDOWS_10_OR_LATER
		#elif _WIN32_WINNT >= 0x0603
			#define NKENTSEU_PLATFORM_WINDOWS_8_1
		#elif _WIN32_WINNT >= 0x0602
			#define NKENTSEU_PLATFORM_WINDOWS_8
		#elif _WIN32_WINNT >= 0x0601
			#define NKENTSEU_PLATFORM_WINDOWS_7
		#endif
	#endif

	// UWP/AppContainer (Windows Runtime).
	// Permet de distinguer UWP du desktop Win32 classique.
	#if !defined(NKENTSEU_PLATFORM_UWP) && defined(WINAPI_FAMILY)
		#include <winapifamily.h>
		#if defined(WINAPI_FAMILY_APP) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
			#define NKENTSEU_PLATFORM_UWP
			#undef NKENTSEU_PLATFORM_NAME
			#undef NKENTSEU_PLATFORM_VERSION
			#define NKENTSEU_PLATFORM_NAME "UWP"
			#define NKENTSEU_PLATFORM_VERSION "Universal Windows Platform"
		#endif
	#endif

// macOS et famille Apple
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>

#if TARGET_OS_MAC && !TARGET_OS_IPHONE
#define NKENTSEU_PLATFORM_MACOS
#define NKENTSEU_PLATFORM_NAME "macOS"
#define NKENTSEU_PLATFORM_VERSION "macOS"

#elif TARGET_OS_IPHONE
#define NKENTSEU_PLATFORM_IOS
#define NKENTSEU_PLATFORM_NAME "iOS"

#if TARGET_OS_SIMULATOR
#define NKENTSEU_PLATFORM_IOS_SIMULATOR
#define NKENTSEU_PLATFORM_VERSION "iOS Simulator"
#else
#define NKENTSEU_PLATFORM_VERSION "iOS"
#endif

#elif TARGET_OS_TV
#define NKENTSEU_PLATFORM_TVOS
#define NKENTSEU_PLATFORM_NAME "tvOS"
#define NKENTSEU_PLATFORM_VERSION "tvOS"

#elif TARGET_OS_WATCH
#define NKENTSEU_PLATFORM_WATCHOS
#define NKENTSEU_PLATFORM_NAME "watchOS"
#define NKENTSEU_PLATFORM_VERSION "watchOS"

#elif TARGET_OS_VISION || TARGET_OS_XROS
#define NKENTSEU_PLATFORM_VISIONOS
#define NKENTSEU_PLATFORM_NAME "visionOS"
#if TARGET_OS_SIMULATOR
#define NKENTSEU_PLATFORM_VISIONOS_SIMULATOR
#define NKENTSEU_PLATFORM_VERSION "visionOS Simulator"
#else
#define NKENTSEU_PLATFORM_VERSION "visionOS"
#endif

#elif TARGET_OS_MACCATALYST
#define NKENTSEU_PLATFORM_MACCATALYST
#define NKENTSEU_PLATFORM_NAME "Mac Catalyst"
#define NKENTSEU_PLATFORM_VERSION "Mac Catalyst"
#endif

// Linux et dérivés
#elif defined(__linux__) || defined(__linux) || defined(linux)
#if defined(__ANDROID__)
	#define NKENTSEU_PLATFORM_ANDROID
	#define NKENTSEU_PLATFORM_NAME "Android"
	#define NKENTSEU_PLATFORM_VERSION "Android"
#else
	#define NKENTSEU_PLATFORM_LINUX
	#define NKENTSEU_PLATFORM_NAME "Linux"
	#define NKENTSEU_PLATFORM_VERSION "Linux"
#endif

// FreeBSD
#elif defined(__FreeBSD__)
#define NKENTSEU_PLATFORM_FREEBSD
#define NKENTSEU_PLATFORM_NAME "FreeBSD"
#define NKENTSEU_PLATFORM_VERSION "FreeBSD"

// OpenBSD
#elif defined(__OpenBSD__)
#define NKENTSEU_PLATFORM_OPENBSD
#define NKENTSEU_PLATFORM_NAME "OpenBSD"
#define NKENTSEU_PLATFORM_VERSION "OpenBSD"

// NetBSD
#elif defined(__NetBSD__)
#define NKENTSEU_PLATFORM_NETBSD
#define NKENTSEU_PLATFORM_NAME "NetBSD"
#define NKENTSEU_PLATFORM_VERSION "NetBSD"

// Solaris
#elif defined(__sun) || defined(__sun__)
#define NKENTSEU_PLATFORM_SOLARIS
#define NKENTSEU_PLATFORM_NAME "Solaris"
#define NKENTSEU_PLATFORM_VERSION "Solaris"

// Unix génériques
#elif defined(__unix__) || defined(__unix)
#define NKENTSEU_PLATFORM_UNIX
#define NKENTSEU_PLATFORM_NAME "Unix"
#define NKENTSEU_PLATFORM_VERSION "Unix"
#endif

// ============================================================
// MACROS DE DÉTECTION - WEB / EMSCRIPTEN
// ============================================================

/**
 * @brief Macro de détection de plateforme Web (Emscripten/WebAssembly)
 * @def NKENTSEU_PLATFORM_WEB
 * @ingroup PlatformDetection
 */

#if defined(__EMSCRIPTEN__)
#define NKENTSEU_PLATFORM_WEB
#define NKENTSEU_PLATFORM_EMSCRIPTEN
#undef NKENTSEU_PLATFORM_NAME
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_NAME "Web"
#define NKENTSEU_PLATFORM_VERSION "Emscripten/WebAssembly"
#endif

// ============================================================
// MACROS DE DÉTECTION - SYSTÈMES DE FENÊTRES X11/WAYLAND (UNIX/LINUX)
// ============================================================

/**
 * @brief Macro de détection du système de fenêtres XCB
 * @def NKENTSEU_WINDOWING_XCB
 * @ingroup WindowingDetection
 * 
 * XCB (X11 Core Protocol) - Remplaçant moderne de Xlib
 * Détecté via compilation avec libxcb
 */

/**
 * @brief Macro de détection du système de fenêtres Xlib
 * @def NKENTSEU_WINDOWING_XLIB
 * @ingroup WindowingDetection
 * 
 * Xlib - Library client X11 (legacy)
 * Détecté via compilation avec libx11
 */

/**
 * @brief Macro de détection du système de fenêtres Wayland
 * @def NKENTSEU_WINDOWING_WAYLAND
 * @ingroup WindowingDetection
 * 
 * Wayland - Nouveau protocole d'affichage remplaçant X11
 * Détecté via compilation avec libwayland-client
 */

// ============================================================
// OPTIONS DE SÉLECTION EXPLICITE (Forcer une implémentation)
// ============================================================
/**
 * @brief Forcer l'utilisation de XCB uniquement
 * @def NKENTSEU_FORCE_WINDOWING_XCB_ONLY
 * 
 * Si défini, désactiver toutes les autres implémentations (Xlib, Wayland)
 * et forcer XCB. Les en-têtes XCB doivent être disponibles.
 * 
 * Usage:
 *   #define NKENTSEU_FORCE_WINDOWING_XCB_ONLY
 *   #include "NkPlatformDetect.h"
 */

/**
 * @brief Forcer l'utilisation de Xlib uniquement
 * @def NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
 * 
 * Si défini, désactiver toutes les autres implémentations (XCB, Wayland)
 * et forcer Xlib. Les en-têtes Xlib doivent être disponibles.
 * 
 * Usage:
 *   #define NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
 *   #include "NkPlatformDetect.h"
 */

/**
 * @brief Forcer l'utilisation de Wayland uniquement
 * @def NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
 * 
 * Si défini, désactiver toutes les autres implémentations (XCB, Xlib)
 * et forcer Wayland. Les en-têtes Wayland doivent être disponibles.
 * 
 * Usage:
 *   #define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
 *   #include "NkPlatformDetect.h"
 */

/**
 * @brief Forcer le backend headless/noop uniquement
 * @def NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
 *
 * Si défini, aucune implémentation X11/Wayland n'est activée.
 * Le backend de fenêtre noop/headless doit être utilisé.
 *
 * Usage:
 *   #define NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
 *   #include "NkPlatformDetect.h"
 */

// ============================================================
// SÉLECTION EXPLICITE (DÉTERMINISTE)
// ============================================================

// Déterminer le système de fenêtres sur Linux.
// Note: aucune auto-détection d'en-têtes (__has_include) pour éviter les
// divergences de backend entre unités de compilation.
#ifdef NKENTSEU_PLATFORM_LINUX
	// Gestion des sélections exclusives - désactiver les conflits
	#if defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
		#undef NKENTSEU_FORCE_WINDOWING_XCB_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
	#elif defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
		#undef NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
	#elif defined(NKENTSEU_FORCE_WINDOWING_XLIB_ONLY)
		#undef NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_XCB_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
	#elif defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
		#undef NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_XCB_ONLY
		#undef NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
	#endif

	// Sélection déterministe :
	// 1) macros FORCE_* si présentes
	// 2) fallback défaut = Xlib
	#if defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
		#define NKENTSEU_WINDOWING_XCB
	#elif defined(NKENTSEU_FORCE_WINDOWING_XLIB_ONLY)
		#define NKENTSEU_WINDOWING_XLIB
	#elif defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
		#define NKENTSEU_WINDOWING_WAYLAND
	#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
		// Backend headless/noop : aucun define X11/Wayland.
	#else
		#define NKENTSEU_WINDOWING_XLIB
	#endif

	// Catégorie X11 (Xlib ou XCB)
	#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
		#define NKENTSEU_WINDOWING_X11
	#endif

	#if defined(NKENTSEU_WINDOWING_WAYLAND)
		#define NKENTSEU_WINDOWING_PREFERRED "Wayland"
	#elif defined(NKENTSEU_WINDOWING_XCB)
		#define NKENTSEU_WINDOWING_PREFERRED "XCB"
	#elif defined(NKENTSEU_WINDOWING_XLIB)
		#define NKENTSEU_WINDOWING_PREFERRED "Xlib"
	#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
		#define NKENTSEU_WINDOWING_PREFERRED "Noop"
	#else
		#define NKENTSEU_WINDOWING_PREFERRED "Unknown"
	#endif
#endif

// ============================================================
// MACROS DE DÉTECTION - HARMONY OS
// ============================================================

/**
 * @brief Macro de détection de plateforme HarmonyOS
 * @def NKENTSEU_PLATFORM_HARMONYOS
 * @ingroup PlatformDetection
 */

#if defined(__HARMONY__) || defined(__OHOS__)
#define NKENTSEU_PLATFORM_HARMONYOS
#undef NKENTSEU_PLATFORM_NAME
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_NAME "HarmonyOS"
#define NKENTSEU_PLATFORM_VERSION "HarmonyOS"
#endif

// ============================================================
// MACROS DE DÉTECTION - CONSOLES SONY PLAYSTATION
// ============================================================

/**
 * @brief Macro de détection de famille PlayStation
 * @def NKENTSEU_PLATFORM_PLAYSTATION
 * @ingroup PlatformDetection
 */

// PlayStation 3
#if defined(__PPU__) || defined(__CELLOS_LV2__) || defined(_PS3)
#define NKENTSEU_PLATFORM_PS3
#define NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLATFORM_NAME "PlayStation 3"
#define NKENTSEU_PLATFORM_VERSION "PS3"

// PlayStation 4
#elif defined(__ORBIS__)
#define NKENTSEU_PLATFORM_PS4
#define NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLATFORM_NAME "PlayStation 4"
#define NKENTSEU_PLATFORM_VERSION "PS4"

// PlayStation 5
#elif defined(__PROSPERO__)
#define NKENTSEU_PLATFORM_PS5
#define NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLATFORM_NAME "PlayStation 5"
#define NKENTSEU_PLATFORM_VERSION "PS5"
#endif

// PlayStation Portable
#if defined(__psp__) || defined(PSP)
#define NKENTSEU_PLATFORM_PSP
#define NKENTSEU_PLATFORM_PLAYSTATION
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "PlayStation Portable"
#define NKENTSEU_PLATFORM_VERSION "PSP"
#endif
#endif

// PlayStation Vita
#if defined(__vita__) || defined(__psp2__)
#define NKENTSEU_PLATFORM_PSVITA
#define NKENTSEU_PLATFORM_PLAYSTATION
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "PlayStation Vita"
#define NKENTSEU_PLATFORM_VERSION "PS Vita"
#endif
#endif

// ============================================================
// MACROS DE DÉTECTION - CONSOLES MICROSOFT XBOX
// ============================================================

/**
 * @brief Macro de détection de famille Xbox
 * @def NKENTSEU_PLATFORM_XBOX
 * @ingroup PlatformDetection
 */

// Xbox Original
#if defined(_XBOX) && !defined(_XBOX_VER)
#define NKENTSEU_PLATFORM_XBOX_ORIGINAL
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox"
#define NKENTSEU_PLATFORM_VERSION "Xbox Original"

// Xbox 360
#elif defined(_XBOX_VER) && (_XBOX_VER == 200)
#define NKENTSEU_PLATFORM_XBOX360
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox 360"
#define NKENTSEU_PLATFORM_VERSION "Xbox 360"

// Xbox One
#elif defined(_DURANGO) || defined(_XBOX_ONE)
#define NKENTSEU_PLATFORM_XBOXONE
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox One"
#define NKENTSEU_PLATFORM_VERSION "Xbox One"

// Xbox Series X/S
#elif defined(_GAMING_XBOX_SCARLETT) || defined(_GAMING_XBOX)
#define NKENTSEU_PLATFORM_XBOX_SERIES
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox Series"
#define NKENTSEU_PLATFORM_VERSION "Xbox Series X|S"
#endif

// ============================================================
// MACROS DE DÉTECTION - CONSOLES NINTENDO
// ============================================================

/**
 * @brief Macro de détection de famille Nintendo
 * @def NKENTSEU_PLATFORM_NINTENDO
 * @ingroup PlatformDetection
 */

// Nintendo Entertainment System (NES)
#if defined(__NES__) || defined(NES)
#define NKENTSEU_PLATFORM_NES
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "NES"
#define NKENTSEU_PLATFORM_VERSION "Nintendo Entertainment System"

// Super Nintendo (SNES)
#elif defined(__SNES__) || defined(SNES)
#define NKENTSEU_PLATFORM_SNES
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "SNES"
#define NKENTSEU_PLATFORM_VERSION "Super Nintendo"

// Nintendo 64
#elif defined(__N64__) || defined(_ULTRA64)
#define NKENTSEU_PLATFORM_N64
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Nintendo 64"
#define NKENTSEU_PLATFORM_VERSION "N64"

// Nintendo GameCube
#elif defined(__gamecube__) || defined(GAMECUBE) || defined(GEKKO)
#define NKENTSEU_PLATFORM_GAMECUBE
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "GameCube"
#define NKENTSEU_PLATFORM_VERSION "Nintendo GameCube"

// Nintendo Wii
#elif defined(__wii__) || defined(WII) || defined(RVL)
#define NKENTSEU_PLATFORM_WII
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Wii"
#define NKENTSEU_PLATFORM_VERSION "Nintendo Wii"

// Nintendo Wii U
#elif defined(__wiiu__) || defined(CAFE)
#define NKENTSEU_PLATFORM_WIIU
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Wii U"
#define NKENTSEU_PLATFORM_VERSION "Nintendo Wii U"

// Nintendo Switch
#elif defined(__SWITCH__) || defined(__NX__)
#define NKENTSEU_PLATFORM_SWITCH
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Nintendo Switch"
#define NKENTSEU_PLATFORM_VERSION "Switch"
#endif

// Game Boy
#if defined(__GB__) || defined(GB)
#define NKENTSEU_PLATFORM_GAMEBOY
#define NKENTSEU_PLATFORM_NINTENDO
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Boy"
#define NKENTSEU_PLATFORM_VERSION "GB"
#endif
#endif

// Game Boy Color
#if defined(__GBC__) || defined(GBC)
#define NKENTSEU_PLATFORM_GAMEBOY_COLOR
#define NKENTSEU_PLATFORM_NINTENDO
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Boy Color"
#define NKENTSEU_PLATFORM_VERSION "GBC"
#endif
#endif

// Game Boy Advance
#if defined(__GBA__) || defined(__gba__)
#define NKENTSEU_PLATFORM_GBA
#define NKENTSEU_PLATFORM_NINTENDO
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Boy Advance"
#define NKENTSEU_PLATFORM_VERSION "GBA"
#endif
#endif

// Nintendo DS
#if defined(__NDS__) || defined(ARM9) && defined(ARM7)
#define NKENTSEU_PLATFORM_NDS
#define NKENTSEU_PLATFORM_NINTENDO
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Nintendo DS"
#define NKENTSEU_PLATFORM_VERSION "NDS"
#endif
#endif

// Nintendo 3DS
#if defined(__3DS__) || defined(_3DS)
#define NKENTSEU_PLATFORM_3DS
#define NKENTSEU_PLATFORM_NINTENDO
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Nintendo 3DS"
#define NKENTSEU_PLATFORM_VERSION "3DS"
#endif
#endif

// ============================================================
// MACROS DE DÉTECTION - CONSOLES SEGA
// ============================================================

/**
 * @brief Macro de détection de famille Sega
 * @def NKENTSEU_PLATFORM_SEGA
 * @ingroup PlatformDetection
 */

// Sega Master System
#if defined(__SMS__) || defined(SMS)
#define NKENTSEU_PLATFORM_MASTER_SYSTEM
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Master System"
#define NKENTSEU_PLATFORM_VERSION "Sega Master System"

// Sega Genesis/Mega Drive
#elif defined(__GENESIS__) || defined(__MEGADRIVE__)
#define NKENTSEU_PLATFORM_GENESIS
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Genesis"
#define NKENTSEU_PLATFORM_VERSION "Sega Genesis/Mega Drive"

// Sega Saturn
#elif defined(__saturn__) || defined(_SATURN)
#define NKENTSEU_PLATFORM_SATURN
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Saturn"
#define NKENTSEU_PLATFORM_VERSION "Sega Saturn"

// Sega Dreamcast
#elif defined(__DREAMCAST__) || defined(_arch_dreamcast)
#define NKENTSEU_PLATFORM_DREAMCAST
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Dreamcast"
#define NKENTSEU_PLATFORM_VERSION "Sega Dreamcast"
#endif

// Sega Game Gear
#if defined(__GG__) || defined(GG)
#define NKENTSEU_PLATFORM_GAME_GEAR
#define NKENTSEU_PLATFORM_SEGA
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Gear"
#define NKENTSEU_PLATFORM_VERSION "Sega Game Gear"
#endif
#endif

// ============================================================
// MACROS DE DÉTECTION - AUTRES CONSOLES
// ============================================================

// Atari 2600
#if defined(__ATARI2600__) || defined(ATARI2600)
#define NKENTSEU_PLATFORM_ATARI2600
#define NKENTSEU_PLATFORM_NAME "Atari 2600"
#define NKENTSEU_PLATFORM_VERSION "Atari 2600"
#endif

// Atari Jaguar
#if defined(__ATARI_JAGUAR__) || defined(JAGUAR)
#define NKENTSEU_PLATFORM_ATARI_JAGUAR
#define NKENTSEU_PLATFORM_NAME "Atari Jaguar"
#define NKENTSEU_PLATFORM_VERSION "Atari Jaguar"
#endif

// Neo Geo
#if defined(__NEOGEO__) || defined(NEOGEO)
#define NKENTSEU_PLATFORM_NEOGEO
#define NKENTSEU_PLATFORM_NAME "Neo Geo"
#define NKENTSEU_PLATFORM_VERSION "Neo Geo"
#endif

// 3DO
#if defined(__3DO__) || defined(_3DO)
#define NKENTSEU_PLATFORM_3DO
#define NKENTSEU_PLATFORM_NAME "3DO"
#define NKENTSEU_PLATFORM_VERSION "3DO Interactive Multiplayer"
#endif

// ============================================================
// MACROS DE DÉTECTION - SYSTÈMES EMBARQUÉS
// ============================================================

/**
 * @brief Macro de détection de système embarqué
 * @def NKENTSEU_PLATFORM_EMBEDDED
 * @ingroup PlatformDetection
 */

// Arduino
#if defined(ARDUINO)
#define NKENTSEU_PLATFORM_ARDUINO
#define NKENTSEU_PLATFORM_EMBEDDED
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Arduino"
#define NKENTSEU_PLATFORM_VERSION "Arduino"
#endif
#endif

// ESP32
#if defined(ESP32) || defined(CONFIG_IDF_TARGET_ESP32)
#define NKENTSEU_PLATFORM_ESP32
#define NKENTSEU_PLATFORM_EMBEDDED
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "ESP32"
#define NKENTSEU_PLATFORM_VERSION "ESP32"
#endif
#endif

// ESP8266
#if defined(ESP8266)
#define NKENTSEU_PLATFORM_ESP8266
#define NKENTSEU_PLATFORM_EMBEDDED
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "ESP8266"
#define NKENTSEU_PLATFORM_VERSION "ESP8266"
#endif
#endif

// STM32
#if defined(STM32) || defined(__STM32__)
#define NKENTSEU_PLATFORM_STM32
#define NKENTSEU_PLATFORM_EMBEDDED
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "STM32"
#define NKENTSEU_PLATFORM_VERSION "STM32"
#endif
#endif

// Raspberry Pi (détection approximative)
#if defined(__arm__) && defined(__linux__) &&                                                                          \
	(defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_8A__))
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_RASPBERRY_PI
#define NKENTSEU_PLATFORM_EMBEDDED
#define NKENTSEU_PLATFORM_NAME "Raspberry Pi"
#define NKENTSEU_PLATFORM_VERSION "Raspberry Pi"
#endif
#endif

// Teensy
#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
#define NKENTSEU_PLATFORM_TEENSY
#define NKENTSEU_PLATFORM_EMBEDDED
#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Teensy"
#define NKENTSEU_PLATFORM_VERSION "Teensy"
#endif
#endif

// ============================================================
// MACROS DE DÉTECTION - STEAM ET DISTRIBUTIONS GAMING
// ============================================================

/**
 * @brief Macro de détection de Steam Deck
 * @def NKENTSEU_PLATFORM_STEAM_DECK
 * @ingroup PlatformDetection
 */

// Steam Deck (détection approximative via environnement)
#if defined(__linux__) && defined(__x86_64__)
// Note: La détection précise nécessite une vérification runtime
// via variables d'environnement ou fichiers système
#if defined(STEAM_DECK) || defined(__STEAM_RUNTIME__)
#define NKENTSEU_PLATFORM_STEAM_DECK
#define NKENTSEU_PLATFORM_STEAM
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_VERSION "Steam Deck"
#endif
#endif

// Steam Runtime
#if defined(__STEAM_RUNTIME__)
#define NKENTSEU_PLATFORM_STEAM_RUNTIME
#define NKENTSEU_PLATFORM_STEAM
#endif

// ============================================================
// CATÉGORIES DE PLATEFORMES
// ============================================================

/**
 * @brief Macro de catégorie Desktop
 * @def NKENTSEU_PLATFORM_DESKTOP
 * @ingroup PlatformCategories
 */

/**
 * @brief Macro de catégorie Mobile
 * @def NKENTSEU_PLATFORM_MOBILE
 * @ingroup PlatformCategories
 */

/**
 * @brief Macro de catégorie Console
 * @def NKENTSEU_PLATFORM_CONSOLE
 * @ingroup PlatformCategories
 */

// Desktop
#if (defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)) || defined(NKENTSEU_PLATFORM_MACOS) ||   \
	(defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_ANDROID))
#define NKENTSEU_PLATFORM_DESKTOP
#endif

// Mobile
#if defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_WATCHOS) ||      \
	defined(NKENTSEU_PLATFORM_VISIONOS)
#define NKENTSEU_PLATFORM_MOBILE
#endif

// Handheld Gaming
#if defined(NKENTSEU_PLATFORM_PSP) || defined(NKENTSEU_PLATFORM_PSVITA) || defined(NKENTSEU_PLATFORM_NDS) ||           \
	defined(NKENTSEU_PLATFORM_3DS) || defined(NKENTSEU_PLATFORM_GBA) || defined(NKENTSEU_PLATFORM_GAMEBOY) ||          \
	defined(NKENTSEU_PLATFORM_GAMEBOY_COLOR) || defined(NKENTSEU_PLATFORM_GAME_GEAR) ||                                \
	defined(NKENTSEU_PLATFORM_SWITCH)
#define NKENTSEU_PLATFORM_HANDHELD
#endif

// Console
#if defined(NKENTSEU_PLATFORM_PLAYSTATION) || defined(NKENTSEU_PLATFORM_XBOX) ||                                       \
	defined(NKENTSEU_PLATFORM_NINTENDO) || defined(NKENTSEU_PLATFORM_SEGA) || defined(NKENTSEU_PLATFORM_ATARI2600) ||  \
	defined(NKENTSEU_PLATFORM_ATARI_JAGUAR) || defined(NKENTSEU_PLATFORM_NEOGEO) || defined(NKENTSEU_PLATFORM_3DO)
#define NKENTSEU_PLATFORM_CONSOLE
#endif

// Web
#if defined(NKENTSEU_PLATFORM_WEB)
#define NKENTSEU_PLATFORM_WEB_BROWSER
#endif

// Unix-like
#if defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_FREEBSD) ||      \
	defined(NKENTSEU_PLATFORM_OPENBSD) || defined(NKENTSEU_PLATFORM_NETBSD) || defined(NKENTSEU_PLATFORM_UNIX)
#define NKENTSEU_PLATFORM_POSIX
#define NKENTSEU_PLATFORM_UNIX_LIKE
#endif

// ============================================================
// MACROS PAR DÉFAUT SI PLATEFORME INCONNUE
// ============================================================

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_UNKNOWN
#define NKENTSEU_PLATFORM_NAME "Unknown"
#define NKENTSEU_PLATFORM_VERSION "Unknown Platform"
#warning "Nkentseu: Plateforme non détectée"
#endif

// ============================================================
// MACROS DE VALIDATION
// ============================================================

// Vérification qu'une seule catégorie majeure est définie
#if defined(NKENTSEU_PLATFORM_WINDOWS) + defined(NKENTSEU_PLATFORM_LINUX) + defined(NKENTSEU_PLATFORM_MACOS) +         \
		defined(NKENTSEU_PLATFORM_CONSOLE) >                                                                           \
	1
#if !defined(NKENTSEU_PLATFORM_WEB) && !defined(NKENTSEU_PLATFORM_STEAM_DECK)
#warning "Nkentseu: Plusieurs plateformes principales détectées simultanément"
#endif
#endif

// ============================================================
// MACROS CONDITIONNELLES - EXÉCUTION SÉLECTIVE DE CODE
// ============================================================

/**
 * @brief Macros pour exécuter du code uniquement sur une plateforme spécifique
 * @defgroup ConditionalExecution Exécution Conditionnelle
 *
 * Ces macros permettent d'écrire du code qui ne sera compilé et exécuté
 * que sur la plateforme cible spécifiée.
 *
 * @example
 * @code
 * NKENTSEU_WINDOWS_ONLY({
 *     // Ce code ne s'exécute que sur Windows
 *     MessageBox(NULL, "Hello Windows", "Info", MB_OK);
 * });
 *
 * NKENTSEU_PS5_ONLY({
 *     // Ce code ne s'exécute que sur PS5
 *     InitializePS5Graphics();
 * });
 * @endcode
 */

// ============================================================
// MACROS CONDITIONNELLES - SYSTÈMES D'EXPLOITATION
// ============================================================

#ifdef NKENTSEU_PLATFORM_WINDOWS
	#define NKENTSEU_WINDOWS_ONLY(...) __VA_ARGS__
	#define NKENTSEU_NOT_WINDOWS(...)
#else
	#define NKENTSEU_WINDOWS_ONLY(...)
	#define NKENTSEU_NOT_WINDOWS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_LINUX
	#define NKENTSEU_LINUX_ONLY(...) __VA_ARGS__
	#define NKENTSEU_NOT_LINUX(...)
#else
	#define NKENTSEU_LINUX_ONLY(...)
	#define NKENTSEU_NOT_LINUX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_MACOS
	#define NKENTSEU_MACOS_ONLY(...) __VA_ARGS__
	#define NKENTSEU_NOT_MACOS(...)
#else
	#define NKENTSEU_MACOS_ONLY(...)
	#define NKENTSEU_NOT_MACOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_IOS
#define NKENTSEU_IOS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_IOS(...)
#else
#define NKENTSEU_IOS_ONLY(...)
#define NKENTSEU_NOT_IOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_ANDROID
#define NKENTSEU_ANDROID_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ANDROID(...)
#else
#define NKENTSEU_ANDROID_ONLY(...)
#define NKENTSEU_NOT_ANDROID(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_TVOS
#define NKENTSEU_TVOS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_TVOS(...)
#else
#define NKENTSEU_TVOS_ONLY(...)
#define NKENTSEU_NOT_TVOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_WATCHOS
#define NKENTSEU_WATCHOS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_WATCHOS(...)
#else
#define NKENTSEU_WATCHOS_ONLY(...)
#define NKENTSEU_NOT_WATCHOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_VISIONOS
#define NKENTSEU_VISIONOS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_VISIONOS(...)
#else
#define NKENTSEU_VISIONOS_ONLY(...)
#define NKENTSEU_NOT_VISIONOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_IPADOS
#define NKENTSEU_IPADOS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_IPADOS(...)
#else
#define NKENTSEU_IPADOS_ONLY(...)
#define NKENTSEU_NOT_IPADOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_FREEBSD
#define NKENTSEU_FREEBSD_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_FREEBSD(...)
#else
#define NKENTSEU_FREEBSD_ONLY(...)
#define NKENTSEU_NOT_FREEBSD(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_HARMONYOS
#define NKENTSEU_HARMONYOS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_HARMONYOS(...)
#else
#define NKENTSEU_HARMONYOS_ONLY(...)
#define NKENTSEU_NOT_HARMONYOS(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - CONSOLES PLAYSTATION
// ============================================================

#ifdef NKENTSEU_PLATFORM_PS5
#define NKENTSEU_PS5_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PS5(...)
#else
#define NKENTSEU_PS5_ONLY(...)
#define NKENTSEU_NOT_PS5(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS4
#define NKENTSEU_PS4_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PS4(...)
#else
#define NKENTSEU_PS4_ONLY(...)
#define NKENTSEU_NOT_PS4(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS3
#define NKENTSEU_PS3_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PS3(...)
#else
#define NKENTSEU_PS3_ONLY(...)
#define NKENTSEU_NOT_PS3(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS2
#define NKENTSEU_PS2_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PS2(...)
#else
#define NKENTSEU_PS2_ONLY(...)
#define NKENTSEU_NOT_PS2(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS1
#define NKENTSEU_PS1_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PS1(...)
#else
#define NKENTSEU_PS1_ONLY(...)
#define NKENTSEU_NOT_PS1(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PSP
#define NKENTSEU_PSP_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PSP(...)
#else
#define NKENTSEU_PSP_ONLY(...)
#define NKENTSEU_NOT_PSP(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PSVITA
#define NKENTSEU_PSVITA_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PSVITA(...)
#else
#define NKENTSEU_PSVITA_ONLY(...)
#define NKENTSEU_NOT_PSVITA(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - CONSOLES XBOX
// ============================================================

#ifdef NKENTSEU_PLATFORM_XBOX_SERIES
#define NKENTSEU_XBOX_SERIES_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XBOX_SERIES(...)
#else
#define NKENTSEU_XBOX_SERIES_ONLY(...)
#define NKENTSEU_NOT_XBOX_SERIES(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOXONE
#define NKENTSEU_XBOXONE_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XBOXONE(...)
#else
#define NKENTSEU_XBOXONE_ONLY(...)
#define NKENTSEU_NOT_XBOXONE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOX360
#define NKENTSEU_XBOX360_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XBOX360(...)
#else
#define NKENTSEU_XBOX360_ONLY(...)
#define NKENTSEU_NOT_XBOX360(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOX_ORIGINAL
#define NKENTSEU_XBOX_ORIGINAL_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XBOX_ORIGINAL(...)
#else
#define NKENTSEU_XBOX_ORIGINAL_ONLY(...)
#define NKENTSEU_NOT_XBOX_ORIGINAL(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - CONSOLES NINTENDO
// ============================================================

#ifdef NKENTSEU_PLATFORM_SWITCH
#define NKENTSEU_SWITCH_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SWITCH(...)
#else
#define NKENTSEU_SWITCH_ONLY(...)
#define NKENTSEU_NOT_SWITCH(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_WIIU
#define NKENTSEU_WIIU_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_WIIU(...)
#else
#define NKENTSEU_WIIU_ONLY(...)
#define NKENTSEU_NOT_WIIU(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_WII
#define NKENTSEU_WII_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_WII(...)
#else
#define NKENTSEU_WII_ONLY(...)
#define NKENTSEU_NOT_WII(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GAMECUBE
#define NKENTSEU_GAMECUBE_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_GAMECUBE(...)
#else
#define NKENTSEU_GAMECUBE_ONLY(...)
#define NKENTSEU_NOT_GAMECUBE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_N64
#define NKENTSEU_N64_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_N64(...)
#else
#define NKENTSEU_N64_ONLY(...)
#define NKENTSEU_NOT_N64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_3DS
#define NKENTSEU_3DS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_3DS(...)
#else
#define NKENTSEU_3DS_ONLY(...)
#define NKENTSEU_NOT_3DS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_NDS
#define NKENTSEU_NDS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_NDS(...)
#else
#define NKENTSEU_NDS_ONLY(...)
#define NKENTSEU_NOT_NDS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GBA
#define NKENTSEU_GBA_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_GBA(...)
#else
#define NKENTSEU_GBA_ONLY(...)
#define NKENTSEU_NOT_GBA(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GAMEBOY
#define NKENTSEU_GAMEBOY_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_GAMEBOY(...)
#else
#define NKENTSEU_GAMEBOY_ONLY(...)
#define NKENTSEU_NOT_GAMEBOY(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - CONSOLES SEGA
// ============================================================

#ifdef NKENTSEU_PLATFORM_DREAMCAST
#define NKENTSEU_DREAMCAST_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_DREAMCAST(...)
#else
#define NKENTSEU_DREAMCAST_ONLY(...)
#define NKENTSEU_NOT_DREAMCAST(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_SATURN
#define NKENTSEU_SATURN_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_SATURN(...)
#else
#define NKENTSEU_SATURN_ONLY(...)
#define NKENTSEU_NOT_SATURN(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GENESIS
#define NKENTSEU_GENESIS_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_GENESIS(...)
#else
#define NKENTSEU_GENESIS_ONLY(...)
#define NKENTSEU_NOT_GENESIS(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - WEB & EMBARQUÉ
// ============================================================

#ifdef NKENTSEU_PLATFORM_WEB
#define NKENTSEU_WEB_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_WEB(...)
#else
#define NKENTSEU_WEB_ONLY(...)
#define NKENTSEU_NOT_WEB(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_EMSCRIPTEN
#define NKENTSEU_EMSCRIPTEN_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_EMSCRIPTEN(...)
#else
#define NKENTSEU_EMSCRIPTEN_ONLY(...)
#define NKENTSEU_NOT_EMSCRIPTEN(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_ESP32
#define NKENTSEU_ESP32_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ESP32(...)
#else
#define NKENTSEU_ESP32_ONLY(...)
#define NKENTSEU_NOT_ESP32(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_ARDUINO
#define NKENTSEU_ARDUINO_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_ARDUINO(...)
#else
#define NKENTSEU_ARDUINO_ONLY(...)
#define NKENTSEU_NOT_ARDUINO(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_STM32
#define NKENTSEU_STM32_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_STM32(...)
#else
#define NKENTSEU_STM32_ONLY(...)
#define NKENTSEU_NOT_STM32(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_RASPBERRY_PI
#define NKENTSEU_RASPBERRY_PI_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_RASPBERRY_PI(...)
#else
#define NKENTSEU_RASPBERRY_PI_ONLY(...)
#define NKENTSEU_NOT_RASPBERRY_PI(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_STEAM_DECK
#define NKENTSEU_STEAM_DECK_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_STEAM_DECK(...)
#else
#define NKENTSEU_STEAM_DECK_ONLY(...)
#define NKENTSEU_NOT_STEAM_DECK(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - SYSTÈMES DE FENÊTRES (X11/WAYLAND)
// ============================================================

#ifdef NKENTSEU_WINDOWING_XCB
#define NKENTSEU_XCB_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XCB(...)
#else
#define NKENTSEU_XCB_ONLY(...)
#define NKENTSEU_NOT_XCB(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_WINDOWING_XLIB
#define NKENTSEU_XLIB_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XLIB(...)
#else
#define NKENTSEU_XLIB_ONLY(...)
#define NKENTSEU_NOT_XLIB(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_WINDOWING_WAYLAND
#define NKENTSEU_WAYLAND_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_WAYLAND(...)
#else
#define NKENTSEU_WAYLAND_ONLY(...)
#define NKENTSEU_NOT_WAYLAND(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_WINDOWING_X11
#define NKENTSEU_X11_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_X11(...)
#else
#define NKENTSEU_X11_ONLY(...)
#define NKENTSEU_NOT_X11(...) __VA_ARGS__
#endif

// ============================================================
// MACROS CONDITIONNELLES - CATÉGORIES
// ============================================================

#ifdef NKENTSEU_PLATFORM_DESKTOP
#define NKENTSEU_DESKTOP_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_DESKTOP(...)
#else
#define NKENTSEU_DESKTOP_ONLY(...)
#define NKENTSEU_NOT_DESKTOP(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_MOBILE
#define NKENTSEU_MOBILE_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_MOBILE(...)
#else
#define NKENTSEU_MOBILE_ONLY(...)
#define NKENTSEU_NOT_MOBILE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_CONSOLE
#define NKENTSEU_CONSOLE_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_CONSOLE(...)
#else
#define NKENTSEU_CONSOLE_ONLY(...)
#define NKENTSEU_NOT_CONSOLE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_HANDHELD
#define NKENTSEU_HANDHELD_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_HANDHELD(...)
#else
#define NKENTSEU_HANDHELD_ONLY(...)
#define NKENTSEU_NOT_HANDHELD(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_EMBEDDED
#define NKENTSEU_EMBEDDED_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_EMBEDDED(...)
#else
#define NKENTSEU_EMBEDDED_ONLY(...)
#define NKENTSEU_NOT_EMBEDDED(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_UNIX_LIKE
#define NKENTSEU_UNIX_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_UNIX(...)
#else
#define NKENTSEU_UNIX_ONLY(...)
#define NKENTSEU_NOT_UNIX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLAYSTATION_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_PLAYSTATION(...)
#else
#define NKENTSEU_PLAYSTATION_ONLY(...)
#define NKENTSEU_NOT_PLAYSTATION(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_XBOX_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_XBOX(...)
#else
#define NKENTSEU_XBOX_ONLY(...)
#define NKENTSEU_NOT_XBOX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_NINTENDO_ONLY(...) __VA_ARGS__
#define NKENTSEU_NOT_NINTENDO(...)
#else
#define NKENTSEU_NINTENDO_ONLY(...)
#define NKENTSEU_NOT_NINTENDO(...) __VA_ARGS__
#endif

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKPLATFORMDETECT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================
