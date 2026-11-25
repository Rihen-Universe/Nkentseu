// =============================================================================
// Core/Nkentseu/Platform/NkPlatformDetect.h
// Détection complète de plateforme et définition des macros.
//
// Design :
//  - Détection automatique du système d'exploitation, de l'architecture,
//    des consoles, des systèmes embarqués et des backends de fenêtrage.
//  - Fournit des macros de catégorie (DESKTOP, MOBILE, CONSOLE, ...).
//  - Fournit des macros conditionnelles d'exécution (NKENTSEU_WINDOWS_ONLY, etc.)
//  - Aucune dépendance externe — se base uniquement sur les macros du préprocesseur.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKPLATFORMDETECT_H
#define NKENTSEU_PLATFORM_NKPLATFORMDETECT_H

// -------------------------------------------------------------------------
// SECTION 1 : EN-TÊTES STANDARD NÉCESSAIRES
// -------------------------------------------------------------------------
// Inclusion des en-têtes pour les définitions de types de base.
// Ces en-têtes sont requis pour garantir la portabilité des types.

#include <cstddef>
#include <cstdint>

// =========================================================================
// SECTION 2 : MACROS DE DÉTECTION DE SYSTÈME D'EXPLOITATION
// =========================================================================
// Cette section détecte le système d'exploitation cible à l'aide des macros
// prédéfinies par le compilateur. Chaque plateforme définit :
// - NKENTSEU_PLATFORM_<NAME> : Macro d'identification unique
// - NKENTSEU_PLATFORM_NAME : Chaîne lisible pour logs/debug
// - NKENTSEU_PLATFORM_VERSION : Version détaillée de la plateforme

// -------------------------------------------------------------------------
// Documentation des macros de détection OS principales
// -------------------------------------------------------------------------

/**
 * @brief Macro de détection de plateforme Windows
 * @def NKENTSEU_PLATFORM_WINDOWS
 * @ingroup PlatformDetection
 *
 * Définie automatiquement lorsque le code est compilé pour Windows.
 * Permet d'activer du code spécifique à Windows via les macros conditionnelles.
 */

/**
 * @brief Macro de détection de plateforme Linux
 * @def NKENTSEU_PLATFORM_LINUX
 * @ingroup PlatformDetection
 *
 * Définie automatiquement lorsque le code est compilé pour Linux.
 * Exclut Android qui possède sa propre macro dédiée.
 */

/**
 * @brief Macro de détection de plateforme macOS
 * @def NKENTSEU_PLATFORM_MACOS
 * @ingroup PlatformDetection
 *
 * Définie automatiquement pour macOS (exclut iOS, tvOS, watchOS, visionOS).
 * Utilise les macros TargetConditionals.h d'Apple pour la détection précise.
 */

/**
 * @brief Macro de détection de plateforme iOS
 * @def NKENTSEU_PLATFORM_IOS
 * @ingroup PlatformDetection
 *
 * Définie automatiquement pour iOS sur iPhone et iPad (sauf Mac Catalyst).
 * Inclut la détection du simulateur via NKENTSEU_PLATFORM_IOS_SIMULATOR.
 */

/**
 * @brief Macro de détection de plateforme Android
 * @def NKENTSEU_PLATFORM_ANDROID
 * @ingroup PlatformDetection
 *
 * Définie automatiquement pour Android via __ANDROID__.
 * Android étant basé sur Linux, la détection doit vérifier Android en premier.
 */

// -------------------------------------------------------------------------
// Sous-section : Détection de Windows
// -------------------------------------------------------------------------
// Vérification des macros standards de Windows (_WIN32, _WIN64, etc.)
// Définit également l'architecture (32/64 bits) et la version minimale.

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__)
#define NKENTSEU_PLATFORM_WINDOWS
/** @brief Nom lisible de la plateforme cible, en chaîne (par exemple "Windows"). */
#define NKENTSEU_PLATFORM_NAME "Windows"

#ifdef _WIN64
/** @brief Définie, sans valeur, quand la cible est Windows 64 bits (_WIN64). */
#define NKENTSEU_PLATFORM_WINDOWS_64
/** @brief Précision lisible sur la cible, en chaîne (par exemple "Windows 64-bit"). */
#define NKENTSEU_PLATFORM_VERSION "Windows 64-bit"
#else
/** @brief Définie, sans valeur, quand la cible est Windows 32 bits. */
#define NKENTSEU_PLATFORM_WINDOWS_32
#define NKENTSEU_PLATFORM_VERSION "Windows 32-bit"
#endif

// Détection de version Windows spécifique via _WIN32_WINNT
// Valeurs de référence : 0x0601=Win7, 0x0602=Win8, 0x0603=Win8.1, 0x0A00=Win10
#if defined(_WIN32_WINNT)
#if _WIN32_WINNT >= 0x0A00
/** @brief Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 10 ou plus récent. */
#define NKENTSEU_PLATFORM_WINDOWS_10_OR_LATER
#elif _WIN32_WINNT >= 0x0603
/** @brief Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 8.1. */
#define NKENTSEU_PLATFORM_WINDOWS_8_1
#elif _WIN32_WINNT >= 0x0602
/** @brief Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 8. */
#define NKENTSEU_PLATFORM_WINDOWS_8
#elif _WIN32_WINNT >= 0x0601
/** @brief Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 7. */
#define NKENTSEU_PLATFORM_WINDOWS_7
#endif
#endif

// Détection UWP (Universal Windows Platform) / AppContainer
// Permet de distinguer les applications UWP des applications Win32 classiques
#if !defined(NKENTSEU_PLATFORM_UWP) && defined(WINAPI_FAMILY)
#include <winapifamily.h>

#if defined(WINAPI_FAMILY_APP) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
/** @brief Définie, sans valeur, quand la cible est une application UWP (Windows Runtime). */
#define NKENTSEU_PLATFORM_UWP
#undef NKENTSEU_PLATFORM_NAME
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_NAME "UWP"
#define NKENTSEU_PLATFORM_VERSION "Universal Windows Platform"
#endif
#endif

// -------------------------------------------------------------------------
// Sous-section : Détection de l'écosystème Apple (macOS, iOS, tvOS, etc.)
// -------------------------------------------------------------------------
// Utilise TargetConditionals.h fourni par Apple pour une détection précise
// de chaque variante de plateforme au sein de l'écosystème Apple.

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
/** @brief Définie, sans valeur, quand la cible est le simulateur iOS. */
#define NKENTSEU_PLATFORM_IOS_SIMULATOR
#define NKENTSEU_PLATFORM_VERSION "iOS Simulator"
#else
#define NKENTSEU_PLATFORM_VERSION "iOS"
#endif
#elif TARGET_OS_TV
/** @brief Définie, sans valeur, quand la cible est tvOS. */
#define NKENTSEU_PLATFORM_TVOS
#define NKENTSEU_PLATFORM_NAME "tvOS"
#define NKENTSEU_PLATFORM_VERSION "tvOS"
#elif TARGET_OS_WATCH
/** @brief Définie, sans valeur, quand la cible est watchOS. */
#define NKENTSEU_PLATFORM_WATCHOS
#define NKENTSEU_PLATFORM_NAME "watchOS"
#define NKENTSEU_PLATFORM_VERSION "watchOS"
#elif TARGET_OS_VISION || TARGET_OS_XROS
/** @brief Définie, sans valeur, quand la cible est visionOS. */
#define NKENTSEU_PLATFORM_VISIONOS
#define NKENTSEU_PLATFORM_NAME "visionOS"

#if TARGET_OS_SIMULATOR
/** @brief Définie, sans valeur, quand la cible est le simulateur visionOS. */
#define NKENTSEU_PLATFORM_VISIONOS_SIMULATOR
#define NKENTSEU_PLATFORM_VERSION "visionOS Simulator"
#else
#define NKENTSEU_PLATFORM_VERSION "visionOS"
#endif
#elif TARGET_OS_MACCATALYST
/** @brief Définie, sans valeur, quand la cible est Mac Catalyst (une application iPad sur macOS). */
#define NKENTSEU_PLATFORM_MACCATALYST
#define NKENTSEU_PLATFORM_NAME "Mac Catalyst"
#define NKENTSEU_PLATFORM_VERSION "Mac Catalyst"
#endif

// -------------------------------------------------------------------------
// Sous-section : Détection de Linux et dérivés
// -------------------------------------------------------------------------
// Vérifie les macros standards Linux (__linux__, etc.)
// Android étant basé sur Linux, il doit être exclu explicitement.

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

// -------------------------------------------------------------------------
// Sous-section : Détection des BSD
// -------------------------------------------------------------------------

#elif defined(__FreeBSD__)
/** @brief Définie, sans valeur, quand la cible est FreeBSD. */
#define NKENTSEU_PLATFORM_FREEBSD
#define NKENTSEU_PLATFORM_NAME "FreeBSD"
#define NKENTSEU_PLATFORM_VERSION "FreeBSD"

#elif defined(__OpenBSD__)
/** @brief Définie, sans valeur, quand la cible est OpenBSD. */
#define NKENTSEU_PLATFORM_OPENBSD
#define NKENTSEU_PLATFORM_NAME "OpenBSD"
#define NKENTSEU_PLATFORM_VERSION "OpenBSD"

#elif defined(__NetBSD__)
/** @brief Définie, sans valeur, quand la cible est NetBSD. */
#define NKENTSEU_PLATFORM_NETBSD
#define NKENTSEU_PLATFORM_NAME "NetBSD"
#define NKENTSEU_PLATFORM_VERSION "NetBSD"

// -------------------------------------------------------------------------
// Sous-section : Détection de Solaris et Unix générique
// -------------------------------------------------------------------------

#elif defined(__sun) || defined(__sun__)
/** @brief Définie, sans valeur, quand la cible est Solaris. */
#define NKENTSEU_PLATFORM_SOLARIS
#define NKENTSEU_PLATFORM_NAME "Solaris"
#define NKENTSEU_PLATFORM_VERSION "Solaris"

#elif defined(__unix__) || defined(__unix)
/** @brief Définie, sans valeur, quand la cible est un autre Unix, que la détection ne reconnaît pas plus précisément. */
#define NKENTSEU_PLATFORM_UNIX
#define NKENTSEU_PLATFORM_NAME "Unix"
#define NKENTSEU_PLATFORM_VERSION "Unix"
#endif

// =========================================================================
// SECTION 3 : DÉTECTION WEB / EMSCRIPTEN
// =========================================================================
// Détection de la compilation WebAssembly via Emscripten.
// Cette plateforme nécessite une gestion spécifique des APIs système.

/**
 * @brief Macro de détection de plateforme Web (Emscripten/WebAssembly)
 * @def NKENTSEU_PLATFORM_EMSCRIPTEN
 * @ingroup PlatformDetection
 *
 * Définie lorsque le code est compilé avec Emscripten pour cibler le Web.
 * Permet d'activer des fallbacks pour les APIs non disponibles dans le navigateur.
 */

#if defined(__EMSCRIPTEN__)
#define NKENTSEU_PLATFORM_EMSCRIPTEN
// Alias ADDITIF, pas un renommage : NKENTSEU_PLATFORM_EMSCRIPTEN reste defini
// au-dessus. Les deux ecritures sont permises (arbitrage Rodolf, 2026-08-17) et
// doivent repondre vrai SIMULTANEMENT sur la meme cible.
// Temoin : Kernel/Foundation/NKPlatform/tests/run_temoin_noms_additifs.sh
/** @brief Définie, sans valeur, quand la cible est le web, compilé par Emscripten. */
#define NKENTSEU_PLATFORM_WEB
#undef NKENTSEU_PLATFORM_NAME
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_NAME "Web"
#define NKENTSEU_PLATFORM_VERSION "Emscripten/WebAssembly"
#endif

// =========================================================================
// SECTION 4 : DÉTECTION DES SYSTÈMES DE FENÊTRAGE (LINUX)
// =========================================================================
// Gestion des backends graphiques pour Linux : X11 (Xlib/XCB) et Wayland.
// Permet de sélectionner l'implémentation de fenêtrage à la compilation.

/**
 * @brief Macro de détection du système de fenêtres XCB
 * @def NKENTSEU_WINDOWING_XCB
 * @ingroup WindowingDetection
 *
 * XCB (X11 Core Protocol) – Remplaçant moderne de Xlib.
 * Détecté via compilation avec libxcb.
 * Plus performant et asynchrone que Xlib.
 */

/**
 * @brief Macro de détection du système de fenêtres Xlib
 * @def NKENTSEU_WINDOWING_XLIB
 * @ingroup WindowingDetection
 *
 * Xlib – Bibliothèque client X11 historique (legacy).
 * Détecté via compilation avec libx11.
 * Plus simple d'utilisation mais moins performant que XCB.
 */

/**
 * @brief Macro de détection du système de fenêtres Wayland
 * @def NKENTSEU_WINDOWING_WAYLAND
 * @ingroup WindowingDetection
 *
 * Wayland – Nouveau protocole d'affichage remplaçant X11.
 * Détecté via compilation avec libwayland-client.
 * Architecture plus moderne avec meilleure sécurité et performance.
 */

// -------------------------------------------------------------------------
// Sous-section : Options de sélection explicite des backends
// -------------------------------------------------------------------------
// Ces macros permettent de forcer manuellement un backend spécifique,
// ignorant la détection automatique. Utile pour les builds cross-compilés.

/**
 * @brief Forcer l'utilisation de XCB uniquement
 * @def NKENTSEU_FORCE_WINDOWING_XCB_ONLY
 *
 * Si défini, désactive toutes les autres implémentations (Xlib, Wayland)
 * et force XCB. Les en-têtes XCB doivent être disponibles.
 *
 * Usage :
 *   #define NKENTSEU_FORCE_WINDOWING_XCB_ONLY
 *   #include "NkPlatformDetect.h"
 */

/**
 * @brief Forcer l'utilisation de Xlib uniquement
 * @def NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
 *
 * Si défini, désactive toutes les autres implémentations (XCB, Wayland)
 * et force Xlib. Les en-têtes Xlib doivent être disponibles.
 *
 * Usage :
 *   #define NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
 *   #include "NkPlatformDetect.h"
 */

/**
 * @brief Forcer l'utilisation de Wayland uniquement
 * @def NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
 *
 * Si défini, désactive toutes les autres implémentations (XCB, Xlib)
 * et force Wayland. Les en-têtes Wayland doivent être disponibles.
 *
 * Usage :
 *   #define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
 *   #include "NkPlatformDetect.h"
 */

/**
 * @brief Forcer le backend headless/noop uniquement
 * @def NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
 *
 * Si défini, aucune implémentation X11/Wayland n'est activée.
 * Le backend de fenêtre noop/headless doit être utilisé.
 * Utile pour les serveurs ou les tests sans interface graphique.
 *
 * Usage :
 *   #define NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
 *   #include "NkPlatformDetect.h"
 */

// -------------------------------------------------------------------------
// Sous-section : Logique de sélection déterministe pour Linux
// -------------------------------------------------------------------------
// Applique la priorité : macros FORCE_* > fallback par défaut (Xlib)
// Gère l'exclusivité mutuelle entre les différentes options.

// HarmonyOS (OHOS) est basé sur Linux (__linux__ défini) mais possède son
// propre backend fenêtrage (XComponent/OHNativeWindow) : il ne doit PAS
// entrer dans la sélection X11/Wayland (sinon NKENTSEU_WINDOWING_XLIB est
// défini par défaut → inclusion erronée de <X11/Xlib.h>). On garde toutefois
// NKENTSEU_PLATFORM_LINUX actif ailleurs pour les chemins POSIX (threads, IO).
#if defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_HARMONYOS) && !defined(__OHOS__)
// Gestion des sélections exclusives – désactiver les conflits potentiels
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

// Sélection déterministe selon la priorité définie :
// 1) Macros FORCE_* si présentes (priorité maximale)
// 2) Fallback par défaut = Xlib (compatibilité maximale)
#if defined(NKENTSEU_FORCE_WINDOWING_XCB_ONLY)
#define NKENTSEU_WINDOWING_XCB
#elif defined(NKENTSEU_FORCE_WINDOWING_XLIB_ONLY)
#define NKENTSEU_WINDOWING_XLIB
#elif defined(NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY)
#define NKENTSEU_WINDOWING_WAYLAND
#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
// Backend headless/noop : aucun define X11/Wayland activé
#else
#define NKENTSEU_WINDOWING_XLIB
#endif

// Macro de catégorie X11 (regroupe Xlib et XCB sous une même bannière)
#if defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
/** @brief Définie, sans valeur, quand le fenêtrage choisi est X11 (Xlib ou XCB). */
#define NKENTSEU_WINDOWING_X11
#endif

// Définition de la chaîne de préférence pour logs/debug
#if defined(NKENTSEU_WINDOWING_WAYLAND)
/** @brief Nom du fenêtrage Linux choisi, en chaîne ("Wayland", "XCB", "Xlib" ou "Noop"). */
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

// =========================================================================
// SECTION 5 : DÉTECTION AUTRES SYSTÈMES (HARMONYOS, CONSOLES, EMBARQUÉ)
// =========================================================================

// -------------------------------------------------------------------------
// Sous-section : HarmonyOS (Huawei)
// -------------------------------------------------------------------------
#if (defined(__OHOS__) || defined(__HARMONY__)) && !defined(NKENTSEU_PLATFORM_HARMONYOS)
/** @brief Définie, sans valeur, quand la cible est HarmonyOS. */
#define NKENTSEU_PLATFORM_HARMONYOS
#define NKENTSEU_PLATFORM_MOBILE 1
#undef NKENTSEU_PLATFORM_NAME
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_NAME "HarmonyOS"
#define NKENTSEU_PLATFORM_VERSION "HarmonyOS"
#endif

// -------------------------------------------------------------------------
// Sous-section : Consoles Sony PlayStation
// -------------------------------------------------------------------------
// Détection des différentes générations de PlayStation via leurs macros SDK.

#if defined(__PPU__) || defined(__CELLOS_LV2__) || defined(_PS3)
/** @brief Définie, sans valeur, quand la cible est la PlayStation 3. */
#define NKENTSEU_PLATFORM_PS3
/** @brief Définie, sans valeur, quand la cible est une PlayStation, quel que soit le modèle. */
#define NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLATFORM_NAME "PlayStation 3"
#define NKENTSEU_PLATFORM_VERSION "PS3"
#elif defined(__ORBIS__)
/** @brief Définie, sans valeur, quand la cible est la PlayStation 4. */
#define NKENTSEU_PLATFORM_PS4
#define NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLATFORM_NAME "PlayStation 4"
#define NKENTSEU_PLATFORM_VERSION "PS4"
#elif defined(__PROSPERO__)
/** @brief Définie, sans valeur, quand la cible est la PlayStation 5. */
#define NKENTSEU_PLATFORM_PS5
#define NKENTSEU_PLATFORM_PLAYSTATION
#define NKENTSEU_PLATFORM_NAME "PlayStation 5"
#define NKENTSEU_PLATFORM_VERSION "PS5"
#endif

#if defined(__psp__) || defined(PSP)
/** @brief Définie, sans valeur, quand la cible est la PSP. */
#define NKENTSEU_PLATFORM_PSP
#define NKENTSEU_PLATFORM_PLAYSTATION

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "PlayStation Portable"
#define NKENTSEU_PLATFORM_VERSION "PSP"
#endif
#endif

#if defined(__vita__) || defined(__psp2__)
/** @brief Définie, sans valeur, quand la cible est la PS Vita. */
#define NKENTSEU_PLATFORM_PSVITA
#define NKENTSEU_PLATFORM_PLAYSTATION

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "PlayStation Vita"
#define NKENTSEU_PLATFORM_VERSION "PS Vita"
#endif
#endif

// -------------------------------------------------------------------------
// Sous-section : Consoles Microsoft Xbox
// -------------------------------------------------------------------------
// Détection des différentes générations Xbox via leurs macros SDK.

#if defined(_XBOX) && !defined(_XBOX_VER)
/** @brief Définie, sans valeur, quand la cible est la première Xbox. */
#define NKENTSEU_PLATFORM_XBOX_ORIGINAL
/** @brief Définie, sans valeur, quand la cible est une Xbox, quel que soit le modèle. */
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox"
#define NKENTSEU_PLATFORM_VERSION "Xbox Original"
#elif defined(_XBOX_VER) && (_XBOX_VER == 200)
/** @brief Définie, sans valeur, quand la cible est la Xbox 360. */
#define NKENTSEU_PLATFORM_XBOX360
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox 360"
#define NKENTSEU_PLATFORM_VERSION "Xbox 360"
#elif defined(_DURANGO) || defined(_XBOX_ONE)
/** @brief Définie, sans valeur, quand la cible est la Xbox One. */
#define NKENTSEU_PLATFORM_XBOXONE
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox One"
#define NKENTSEU_PLATFORM_VERSION "Xbox One"
#elif defined(_GAMING_XBOX_SCARLETT) || defined(_GAMING_XBOX)
/** @brief Définie, sans valeur, quand la cible est la Xbox Series. */
#define NKENTSEU_PLATFORM_XBOX_SERIES
#define NKENTSEU_PLATFORM_XBOX
#define NKENTSEU_PLATFORM_NAME "Xbox Series"
#define NKENTSEU_PLATFORM_VERSION "Xbox Series X|S"
#endif

// -------------------------------------------------------------------------
// Sous-section : Consoles Nintendo
// -------------------------------------------------------------------------
// Détection complète de l'historique des consoles Nintendo.

#if defined(__NES__) || defined(NES)
/** @brief Définie, sans valeur, quand la cible est la NES. */
#define NKENTSEU_PLATFORM_NES
/** @brief Définie, sans valeur, quand la cible est une console Nintendo de salon (NES à Switch). */
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "NES"
#define NKENTSEU_PLATFORM_VERSION "Nintendo Entertainment System"
#elif defined(__SNES__) || defined(SNES)
/** @brief Définie, sans valeur, quand la cible est la Super Nintendo. */
#define NKENTSEU_PLATFORM_SNES
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "SNES"
#define NKENTSEU_PLATFORM_VERSION "Super Nintendo"
#elif defined(__N64__) || defined(_ULTRA64)
/** @brief Définie, sans valeur, quand la cible est la Nintendo 64. */
#define NKENTSEU_PLATFORM_N64
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Nintendo 64"
#define NKENTSEU_PLATFORM_VERSION "N64"
#elif defined(__gamecube__) || defined(GAMECUBE) || defined(GEKKO)
/** @brief Définie, sans valeur, quand la cible est la GameCube. */
#define NKENTSEU_PLATFORM_GAMECUBE
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "GameCube"
#define NKENTSEU_PLATFORM_VERSION "Nintendo GameCube"
#elif defined(__wii__) || defined(WII) || defined(RVL)
/** @brief Définie, sans valeur, quand la cible est la Wii. */
#define NKENTSEU_PLATFORM_WII
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Wii"
#define NKENTSEU_PLATFORM_VERSION "Nintendo Wii"
#elif defined(__wiiu__) || defined(CAFE)
/** @brief Définie, sans valeur, quand la cible est la Wii U. */
#define NKENTSEU_PLATFORM_WIIU
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Wii U"
#define NKENTSEU_PLATFORM_VERSION "Nintendo Wii U"
#elif defined(__SWITCH__) || defined(__NX__)
/** @brief Définie, sans valeur, quand la cible est la Nintendo Switch. */
#define NKENTSEU_PLATFORM_SWITCH
#define NKENTSEU_PLATFORM_NINTENDO
#define NKENTSEU_PLATFORM_NAME "Nintendo Switch"
#define NKENTSEU_PLATFORM_VERSION "Switch"
#endif

#if defined(__GB__) || defined(GB)
/** @brief Définie, sans valeur, quand la cible est la Game Boy. */
#define NKENTSEU_PLATFORM_GAMEBOY
#define NKENTSEU_PLATFORM_NINTENDO

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Boy"
#define NKENTSEU_PLATFORM_VERSION "GB"
#endif
#endif

#if defined(__GBC__) || defined(GBC)
/** @brief Définie, sans valeur, quand la cible est la Game Boy Color. */
#define NKENTSEU_PLATFORM_GAMEBOY_COLOR
#define NKENTSEU_PLATFORM_NINTENDO

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Boy Color"
#define NKENTSEU_PLATFORM_VERSION "GBC"
#endif
#endif

#if defined(__GBA__) || defined(__gba__)
/** @brief Définie, sans valeur, quand la cible est la Game Boy Advance. */
#define NKENTSEU_PLATFORM_GBA
#define NKENTSEU_PLATFORM_NINTENDO

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Boy Advance"
#define NKENTSEU_PLATFORM_VERSION "GBA"
#endif
#endif

#if defined(__NDS__) || (defined(ARM9) && defined(ARM7))
/** @brief Définie, sans valeur, quand la cible est la Nintendo DS. */
#define NKENTSEU_PLATFORM_NDS
#define NKENTSEU_PLATFORM_NINTENDO

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Nintendo DS"
#define NKENTSEU_PLATFORM_VERSION "NDS"
#endif
#endif

#if defined(__3DS__) || defined(_3DS)
/** @brief Définie, sans valeur, quand la cible est la Nintendo 3DS. */
#define NKENTSEU_PLATFORM_3DS
#define NKENTSEU_PLATFORM_NINTENDO

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Nintendo 3DS"
#define NKENTSEU_PLATFORM_VERSION "3DS"
#endif
#endif

// -------------------------------------------------------------------------
// Sous-section : Consoles Sega
// -------------------------------------------------------------------------
#if defined(__SMS__) || defined(SMS)
/** @brief Définie, sans valeur, quand la cible est la Master System. */
#define NKENTSEU_PLATFORM_MASTER_SYSTEM
/** @brief Définie, sans valeur, quand la cible est une console Sega de salon. */
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Master System"
#define NKENTSEU_PLATFORM_VERSION "Sega Master System"
#elif defined(__GENESIS__) || defined(__MEGADRIVE__)
/** @brief Définie, sans valeur, quand la cible est la Mega Drive (Genesis). */
#define NKENTSEU_PLATFORM_GENESIS
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Genesis"
#define NKENTSEU_PLATFORM_VERSION "Sega Genesis/Mega Drive"
#elif defined(__saturn__) || defined(_SATURN)
/** @brief Définie, sans valeur, quand la cible est la Saturn. */
#define NKENTSEU_PLATFORM_SATURN
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Saturn"
#define NKENTSEU_PLATFORM_VERSION "Sega Saturn"
#elif defined(__DREAMCAST__) || defined(_arch_dreamcast)
/** @brief Définie, sans valeur, quand la cible est la Dreamcast. */
#define NKENTSEU_PLATFORM_DREAMCAST
#define NKENTSEU_PLATFORM_SEGA
#define NKENTSEU_PLATFORM_NAME "Dreamcast"
#define NKENTSEU_PLATFORM_VERSION "Sega Dreamcast"
#endif

#if defined(__GG__) || defined(GG)
/** @brief Définie, sans valeur, quand la cible est la Game Gear. */
#define NKENTSEU_PLATFORM_GAME_GEAR
#define NKENTSEU_PLATFORM_SEGA

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Game Gear"
#define NKENTSEU_PLATFORM_VERSION "Sega Game Gear"
#endif
#endif

// -------------------------------------------------------------------------
// Sous-section : Autres consoles rétro
// -------------------------------------------------------------------------
#if defined(__ATARI2600__) || defined(ATARI2600)
/** @brief Définie, sans valeur, quand la cible est l'Atari 2600. */
#define NKENTSEU_PLATFORM_ATARI2600
#define NKENTSEU_PLATFORM_NAME "Atari 2600"
#define NKENTSEU_PLATFORM_VERSION "Atari 2600"
#endif

#if defined(__ATARI_JAGUAR__) || defined(JAGUAR)
/** @brief Définie, sans valeur, quand la cible est l'Atari Jaguar. */
#define NKENTSEU_PLATFORM_ATARI_JAGUAR
#define NKENTSEU_PLATFORM_NAME "Atari Jaguar"
#define NKENTSEU_PLATFORM_VERSION "Atari Jaguar"
#endif

#if defined(__NEOGEO__) || defined(NEOGEO)
/** @brief Définie, sans valeur, quand la cible est la Neo Geo. */
#define NKENTSEU_PLATFORM_NEOGEO
#define NKENTSEU_PLATFORM_NAME "Neo Geo"
#define NKENTSEU_PLATFORM_VERSION "Neo Geo"
#endif

#if defined(__3DO__) || defined(_3DO)
/** @brief Définie, sans valeur, quand la cible est la 3DO. */
#define NKENTSEU_PLATFORM_3DO
#define NKENTSEU_PLATFORM_NAME "3DO"
#define NKENTSEU_PLATFORM_VERSION "3DO Interactive Multiplayer"
#endif

// -------------------------------------------------------------------------
// Sous-section : Systèmes embarqués et IoT
// -------------------------------------------------------------------------
// Détection des plateformes embarquées courantes pour l'IoT et le prototypage.

#if defined(ARDUINO)
/** @brief Définie, sans valeur, quand la cible est une carte Arduino. */
#define NKENTSEU_PLATFORM_ARDUINO
/** @brief Définie, sans valeur, quand la cible est un système embarqué. */
#define NKENTSEU_PLATFORM_EMBEDDED

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Arduino"
#define NKENTSEU_PLATFORM_VERSION "Arduino"
#endif
#endif

#if defined(ESP32) || defined(CONFIG_IDF_TARGET_ESP32)
/** @brief Définie, sans valeur, quand la cible est un ESP32. */
#define NKENTSEU_PLATFORM_ESP32
#define NKENTSEU_PLATFORM_EMBEDDED

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "ESP32"
#define NKENTSEU_PLATFORM_VERSION "ESP32"
#endif
#endif

#if defined(ESP8266)
/** @brief Définie, sans valeur, quand la cible est un ESP8266. */
#define NKENTSEU_PLATFORM_ESP8266
#define NKENTSEU_PLATFORM_EMBEDDED

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "ESP8266"
#define NKENTSEU_PLATFORM_VERSION "ESP8266"
#endif
#endif

#if defined(STM32) || defined(__STM32__)
/** @brief Définie, sans valeur, quand la cible est un microcontrôleur STM32. */
#define NKENTSEU_PLATFORM_STM32
#define NKENTSEU_PLATFORM_EMBEDDED

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "STM32"
#define NKENTSEU_PLATFORM_VERSION "STM32"
#endif
#endif

// Détection approximative de Raspberry Pi via architecture ARM + Linux
#if defined(__arm__) && defined(__linux__) &&                                                                          \
	(defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_8A__))
#ifndef NKENTSEU_PLATFORM_NAME
/** @brief Définie, sans valeur, quand la cible est probablement un Raspberry Pi (ARM 32 bits sous Linux, ARMv6, v7-A ou v8-A ; détection approximative). */
#define NKENTSEU_PLATFORM_RASPBERRY_PI
#define NKENTSEU_PLATFORM_EMBEDDED
#define NKENTSEU_PLATFORM_NAME "Raspberry Pi"
#define NKENTSEU_PLATFORM_VERSION "Raspberry Pi"
#endif
#endif

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
/** @brief Définie, sans valeur, quand la cible est une carte Teensy. */
#define NKENTSEU_PLATFORM_TEENSY
#define NKENTSEU_PLATFORM_EMBEDDED

#ifndef NKENTSEU_PLATFORM_NAME
#define NKENTSEU_PLATFORM_NAME "Teensy"
#define NKENTSEU_PLATFORM_VERSION "Teensy"
#endif
#endif

// -------------------------------------------------------------------------
// Sous-section : Steam Deck et Steam Runtime
// -------------------------------------------------------------------------
#if defined(__linux__) && defined(__x86_64__)
#if defined(STEAM_DECK) || defined(__STEAM_RUNTIME__)
/** @brief Définie, sans valeur, quand la cible est le Steam Deck (Linux x86-64 avec STEAM_DECK ou le runtime Steam). */
#define NKENTSEU_PLATFORM_STEAM_DECK
/** @brief Définie, sans valeur, quand la cible est le Steam Deck ou le runtime Steam. */
#define NKENTSEU_PLATFORM_STEAM
#undef NKENTSEU_PLATFORM_VERSION
#define NKENTSEU_PLATFORM_VERSION "Steam Deck"
#endif
#endif

#if defined(__STEAM_RUNTIME__)
/** @brief Définie, sans valeur, quand la cible est le runtime Steam (__STEAM_RUNTIME__). */
#define NKENTSEU_PLATFORM_STEAM_RUNTIME
#define NKENTSEU_PLATFORM_STEAM
#endif

// =========================================================================
// SECTION 6 : CATÉGORIES DE PLATEFORMES (MACROS DE GROUPEMENT)
// =========================================================================
// Ces macros regroupent les plateformes par famille pour simplifier
// les conditions de compilation dans le code utilisateur.

/**
 * @brief Macro de catégorie Desktop
 * @def NKENTSEU_PLATFORM_DESKTOP
 * @ingroup PlatformCategories
 *
 * Définie pour les plateformes de bureau : Windows (non-UWP), macOS, Linux (non-Android).
 * Permet d'activer des fonctionnalités spécifiques aux PC.
 */

/**
 * @brief Macro de catégorie Mobile
 * @def NKENTSEU_PLATFORM_MOBILE
 * @ingroup PlatformCategories
 *
 * Définie pour les plateformes mobiles : iOS, Android, watchOS, visionOS.
 * Permet d'optimiser pour les contraintes mobiles (batterie, mémoire).
 */

/**
 * @brief Macro de catégorie Console
 * @def NKENTSEU_PLATFORM_CONSOLE
 * @ingroup PlatformCategories
 *
 * Définie pour toutes les consoles de salon : PlayStation, Xbox, Nintendo, Sega, etc.
 * Permet d'activer les APIs et optimisations spécifiques aux consoles.
 */

// -------------------------------------------------------------------------
// Sous-section : Définition des catégories
// -------------------------------------------------------------------------

// Catégorie Desktop : Windows (exclut UWP/Xbox), macOS, Linux (exclut Android)
#if (defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)) ||     \
	defined(NKENTSEU_PLATFORM_MACOS) || (defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_ANDROID))
#define NKENTSEU_PLATFORM_DESKTOP
#endif

// Catégorie Mobile : iOS, Android, watchOS, visionOS
#if defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_WATCHOS) ||      \
	defined(NKENTSEU_PLATFORM_VISIONOS)
#define NKENTSEU_PLATFORM_MOBILE
#endif

// Catégorie Handheld Gaming : consoles portables dédiées au jeu
#if defined(NKENTSEU_PLATFORM_PSP) || defined(NKENTSEU_PLATFORM_PSVITA) || defined(NKENTSEU_PLATFORM_NDS) ||           \
	defined(NKENTSEU_PLATFORM_3DS) || defined(NKENTSEU_PLATFORM_GBA) || defined(NKENTSEU_PLATFORM_GAMEBOY) ||          \
	defined(NKENTSEU_PLATFORM_GAMEBOY_COLOR) || defined(NKENTSEU_PLATFORM_GAME_GEAR) ||                                \
	defined(NKENTSEU_PLATFORM_SWITCH)
/** @brief Définie, sans valeur, quand la cible est une console portable. */
#define NKENTSEU_PLATFORM_HANDHELD
#endif

// Catégorie Console : toutes les consoles de salon
#if defined(NKENTSEU_PLATFORM_PLAYSTATION) || defined(NKENTSEU_PLATFORM_XBOX) ||                                       \
	defined(NKENTSEU_PLATFORM_NINTENDO) || defined(NKENTSEU_PLATFORM_SEGA) || defined(NKENTSEU_PLATFORM_ATARI2600) ||  \
	defined(NKENTSEU_PLATFORM_ATARI_JAGUAR) || defined(NKENTSEU_PLATFORM_NEOGEO) || defined(NKENTSEU_PLATFORM_3DO)
#define NKENTSEU_PLATFORM_CONSOLE
#endif

// Catégorie Web : compilation Emscripten pour navigateur
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
/** @brief Définie, sans valeur, quand la cible est le web (compilation Emscripten pour navigateur). */
#define NKENTSEU_PLATFORM_EMSCRIPTEN_BROWSER
#endif

// Catégorie Unix-like : regroupe les systèmes POSIX compatibles
// Android est POSIX-compatible (bionic libc, sys/socket.h, etc.) — on
// l'inclut pour que les modules comme NKNetwork compilent leur branche
// POSIX sur NDK.
// iOS/tvOS/watchOS/visionOS sont des dérivés Darwin/BSD : POSIX complet
// (sys/socket.h, pthread, clock_gettime, statvfs…). Les inclure évite que
// des modules gatés sur NKENTSEU_PLATFORM_POSIX (NKNetwork, NkSocket…)
// tombent silencieusement dans un #else vide / type non déclaré sur mobile
// Apple. (NKENTSEU_PLATFORM_MACOS reste, lui, strictement macOS.)
#if defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_FREEBSD) ||      \
	defined(NKENTSEU_PLATFORM_OPENBSD) || defined(NKENTSEU_PLATFORM_NETBSD) || defined(NKENTSEU_PLATFORM_UNIX) ||      \
	defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_IOS) || defined(NKENTSEU_PLATFORM_TVOS) ||         \
	defined(NKENTSEU_PLATFORM_WATCHOS) || defined(NKENTSEU_PLATFORM_VISIONOS)
/** @brief Définie, sans valeur, quand la cible est un système POSIX (Linux, macOS, les BSD, Android, iOS et leurs dérivés). */
#define NKENTSEU_PLATFORM_POSIX
/** @brief Définie, sans valeur, quand la cible est un système POSIX de la famille Unix. */
#define NKENTSEU_PLATFORM_UNIX_LIKE
#endif

// =========================================================================
// SECTION 7 : MACROS PAR DÉFAUT ET VALIDATION
// =========================================================================

// -------------------------------------------------------------------------
// Sous-section : Gestion de plateforme inconnue
// -------------------------------------------------------------------------
// Si aucune plateforme n'a été détectée, on définit des macros par défaut
// et on émet un warning pour alerter le développeur.

#ifndef NKENTSEU_PLATFORM_NAME
/** @brief Définie, sans valeur, quand la cible est aucune plateforme n'a été reconnue (le nom vaut alors "Unknown"). */
#define NKENTSEU_PLATFORM_UNKNOWN
#define NKENTSEU_PLATFORM_NAME "Unknown"
#define NKENTSEU_PLATFORM_VERSION "Unknown Platform"
#warning "Nkentseu: Plateforme non détectée"
#endif

// -------------------------------------------------------------------------
// Sous-section : Validation de cohérence des détections
// -------------------------------------------------------------------------
// Vérifie qu'une seule plateforme principale est définie (sauf cas spéciaux).
// Émet un warning en cas de détection multiple potentiellement problématique.

#if defined(NKENTSEU_PLATFORM_WINDOWS) + defined(NKENTSEU_PLATFORM_LINUX) + defined(NKENTSEU_PLATFORM_MACOS) +         \
		defined(NKENTSEU_PLATFORM_CONSOLE) >                                                                           \
	1
#if !defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && !defined(NKENTSEU_PLATFORM_STEAM_DECK)
#warning "Nkentseu: Plusieurs plateformes principales détectées simultanément"
#endif
#endif

// =========================================================================
// SECTION 8 : MACROS CONDITIONNELLES D'EXÉCUTION (CODE BLOCKS)
// =========================================================================
// Ces macros permettent d'écrire du code qui ne sera compilé et exécuté
// que sur la plateforme cible spécifiée, avec une syntaxe élégante.

/**
 * @brief Macros pour exécuter du code uniquement sur une plateforme spécifique.
 * @defgroup ConditionalExecution Exécution Conditionnelle
 *
 * Ces macros permettent d'écrire du code qui ne sera compilé et exécuté
 * que sur la plateforme cible spécifiée.
 *
 * @example
 * @code
 * NKENTSEU_WINDOWS_ONLY({
 *     MessageBox(nullptr, "Hello Windows", "Info", MB_OK);
 * });
 *
 * NKENTSEU_PS5_ONLY({
 *     InitializePS5Graphics();
 * });
 * @endcode
 */

// -------------------------------------------------------------------------
// Sous-section : Macros pour systèmes d'exploitation
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_WINDOWS
/** @brief Garde son argument quand la cible est Windows, le retire sinon. */
#define NKENTSEU_WINDOWS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est Windows, le garde sinon. */
#define NKENTSEU_NOT_WINDOWS(...)
#else
#define NKENTSEU_WINDOWS_ONLY(...)
#define NKENTSEU_NOT_WINDOWS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_LINUX
/** @brief Garde son argument quand la cible est Linux, le retire sinon. */
#define NKENTSEU_LINUX_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est Linux, le garde sinon. */
#define NKENTSEU_NOT_LINUX(...)
#else
#define NKENTSEU_LINUX_ONLY(...)
#define NKENTSEU_NOT_LINUX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_MACOS
/** @brief Garde son argument quand la cible est macOS, le retire sinon. */
#define NKENTSEU_MACOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est macOS, le garde sinon. */
#define NKENTSEU_NOT_MACOS(...)
#else
#define NKENTSEU_MACOS_ONLY(...)
#define NKENTSEU_NOT_MACOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_IOS
/** @brief Garde son argument quand la cible est iOS, le retire sinon. */
#define NKENTSEU_IOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est iOS, le garde sinon. */
#define NKENTSEU_NOT_IOS(...)
#else
#define NKENTSEU_IOS_ONLY(...)
#define NKENTSEU_NOT_IOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_ANDROID
/** @brief Garde son argument quand la cible est Android, le retire sinon. */
#define NKENTSEU_ANDROID_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est Android, le garde sinon. */
#define NKENTSEU_NOT_ANDROID(...)
#else
#define NKENTSEU_ANDROID_ONLY(...)
#define NKENTSEU_NOT_ANDROID(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_TVOS
/** @brief Garde son argument quand la cible est tvOS, le retire sinon. */
#define NKENTSEU_TVOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est tvOS, le garde sinon. */
#define NKENTSEU_NOT_TVOS(...)
#else
#define NKENTSEU_TVOS_ONLY(...)
#define NKENTSEU_NOT_TVOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_WATCHOS
/** @brief Garde son argument quand la cible est watchOS, le retire sinon. */
#define NKENTSEU_WATCHOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est watchOS, le garde sinon. */
#define NKENTSEU_NOT_WATCHOS(...)
#else
#define NKENTSEU_WATCHOS_ONLY(...)
#define NKENTSEU_NOT_WATCHOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_VISIONOS
/** @brief Garde son argument quand la cible est visionOS, le retire sinon. */
#define NKENTSEU_VISIONOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est visionOS, le garde sinon. */
#define NKENTSEU_NOT_VISIONOS(...)
#else
#define NKENTSEU_VISIONOS_ONLY(...)
#define NKENTSEU_NOT_VISIONOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_IPADOS
/** @brief Garde son argument quand la cible est iPadOS, le retire sinon. */
#define NKENTSEU_IPADOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est iPadOS, le garde sinon. */
#define NKENTSEU_NOT_IPADOS(...)
#else
#define NKENTSEU_IPADOS_ONLY(...)
#define NKENTSEU_NOT_IPADOS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_FREEBSD
/** @brief Garde son argument quand la cible est FreeBSD, le retire sinon. */
#define NKENTSEU_FREEBSD_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est FreeBSD, le garde sinon. */
#define NKENTSEU_NOT_FREEBSD(...)
#else
#define NKENTSEU_FREEBSD_ONLY(...)
#define NKENTSEU_NOT_FREEBSD(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_HARMONYOS
/** @brief Garde son argument quand la cible est HarmonyOS, le retire sinon. */
#define NKENTSEU_HARMONYOS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est HarmonyOS, le garde sinon. */
#define NKENTSEU_NOT_HARMONYOS(...)
#else
#define NKENTSEU_HARMONYOS_ONLY(...)
#define NKENTSEU_NOT_HARMONYOS(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour consoles PlayStation
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_PS5
/** @brief Garde son argument quand la cible est la PlayStation 5, le retire sinon. */
#define NKENTSEU_PS5_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la PlayStation 5, le garde sinon. */
#define NKENTSEU_NOT_PS5(...)
#else
#define NKENTSEU_PS5_ONLY(...)
#define NKENTSEU_NOT_PS5(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS4
/** @brief Garde son argument quand la cible est la PlayStation 4, le retire sinon. */
#define NKENTSEU_PS4_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la PlayStation 4, le garde sinon. */
#define NKENTSEU_NOT_PS4(...)
#else
#define NKENTSEU_PS4_ONLY(...)
#define NKENTSEU_NOT_PS4(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS3
/** @brief Garde son argument quand la cible est la PlayStation 3, le retire sinon. */
#define NKENTSEU_PS3_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la PlayStation 3, le garde sinon. */
#define NKENTSEU_NOT_PS3(...)
#else
#define NKENTSEU_PS3_ONLY(...)
#define NKENTSEU_NOT_PS3(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS2
/** @brief Garde son argument quand la cible est la PlayStation 2, le retire sinon. */
#define NKENTSEU_PS2_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la PlayStation 2, le garde sinon. */
#define NKENTSEU_NOT_PS2(...)
#else
#define NKENTSEU_PS2_ONLY(...)
#define NKENTSEU_NOT_PS2(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PS1
/** @brief Garde son argument quand la cible est la première PlayStation, le retire sinon. */
#define NKENTSEU_PS1_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la première PlayStation, le garde sinon. */
#define NKENTSEU_NOT_PS1(...)
#else
#define NKENTSEU_PS1_ONLY(...)
#define NKENTSEU_NOT_PS1(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PSP
/** @brief Garde son argument quand la cible est la PSP, le retire sinon. */
#define NKENTSEU_PSP_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la PSP, le garde sinon. */
#define NKENTSEU_NOT_PSP(...)
#else
#define NKENTSEU_PSP_ONLY(...)
#define NKENTSEU_NOT_PSP(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PSVITA
/** @brief Garde son argument quand la cible est la PS Vita, le retire sinon. */
#define NKENTSEU_PSVITA_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la PS Vita, le garde sinon. */
#define NKENTSEU_NOT_PSVITA(...)
#else
#define NKENTSEU_PSVITA_ONLY(...)
#define NKENTSEU_NOT_PSVITA(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour consoles Xbox
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_XBOX_SERIES
/** @brief Garde son argument quand la cible est la Xbox Series, le retire sinon. */
#define NKENTSEU_XBOX_SERIES_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Xbox Series, le garde sinon. */
#define NKENTSEU_NOT_XBOX_SERIES(...)
#else
#define NKENTSEU_XBOX_SERIES_ONLY(...)
#define NKENTSEU_NOT_XBOX_SERIES(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOXONE
/** @brief Garde son argument quand la cible est la Xbox One, le retire sinon. */
#define NKENTSEU_XBOXONE_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Xbox One, le garde sinon. */
#define NKENTSEU_NOT_XBOXONE(...)
#else
#define NKENTSEU_XBOXONE_ONLY(...)
#define NKENTSEU_NOT_XBOXONE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOX360
/** @brief Garde son argument quand la cible est la Xbox 360, le retire sinon. */
#define NKENTSEU_XBOX360_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Xbox 360, le garde sinon. */
#define NKENTSEU_NOT_XBOX360(...)
#else
#define NKENTSEU_XBOX360_ONLY(...)
#define NKENTSEU_NOT_XBOX360(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOX_ORIGINAL
/** @brief Garde son argument quand la cible est la première Xbox, le retire sinon. */
#define NKENTSEU_XBOX_ORIGINAL_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la première Xbox, le garde sinon. */
#define NKENTSEU_NOT_XBOX_ORIGINAL(...)
#else
#define NKENTSEU_XBOX_ORIGINAL_ONLY(...)
#define NKENTSEU_NOT_XBOX_ORIGINAL(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour consoles Nintendo
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_SWITCH
/** @brief Garde son argument quand la cible est la Nintendo Switch, le retire sinon. */
#define NKENTSEU_SWITCH_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Nintendo Switch, le garde sinon. */
#define NKENTSEU_NOT_SWITCH(...)
#else
#define NKENTSEU_SWITCH_ONLY(...)
#define NKENTSEU_NOT_SWITCH(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_WIIU
/** @brief Garde son argument quand la cible est la Wii U, le retire sinon. */
#define NKENTSEU_WIIU_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Wii U, le garde sinon. */
#define NKENTSEU_NOT_WIIU(...)
#else
#define NKENTSEU_WIIU_ONLY(...)
#define NKENTSEU_NOT_WIIU(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_WII
/** @brief Garde son argument quand la cible est la Wii, le retire sinon. */
#define NKENTSEU_WII_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Wii, le garde sinon. */
#define NKENTSEU_NOT_WII(...)
#else
#define NKENTSEU_WII_ONLY(...)
#define NKENTSEU_NOT_WII(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GAMECUBE
/** @brief Garde son argument quand la cible est la GameCube, le retire sinon. */
#define NKENTSEU_GAMECUBE_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la GameCube, le garde sinon. */
#define NKENTSEU_NOT_GAMECUBE(...)
#else
#define NKENTSEU_GAMECUBE_ONLY(...)
#define NKENTSEU_NOT_GAMECUBE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_N64
/** @brief Garde son argument quand la cible est la Nintendo 64, le retire sinon. */
#define NKENTSEU_N64_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Nintendo 64, le garde sinon. */
#define NKENTSEU_NOT_N64(...)
#else
#define NKENTSEU_N64_ONLY(...)
#define NKENTSEU_NOT_N64(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_3DS
/** @brief Garde son argument quand la cible est la Nintendo 3DS, le retire sinon. */
#define NKENTSEU_3DS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Nintendo 3DS, le garde sinon. */
#define NKENTSEU_NOT_3DS(...)
#else
#define NKENTSEU_3DS_ONLY(...)
#define NKENTSEU_NOT_3DS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_NDS
/** @brief Garde son argument quand la cible est la Nintendo DS, le retire sinon. */
#define NKENTSEU_NDS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Nintendo DS, le garde sinon. */
#define NKENTSEU_NOT_NDS(...)
#else
#define NKENTSEU_NDS_ONLY(...)
#define NKENTSEU_NOT_NDS(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GBA
/** @brief Garde son argument quand la cible est la Game Boy Advance, le retire sinon. */
#define NKENTSEU_GBA_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Game Boy Advance, le garde sinon. */
#define NKENTSEU_NOT_GBA(...)
#else
#define NKENTSEU_GBA_ONLY(...)
#define NKENTSEU_NOT_GBA(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GAMEBOY
/** @brief Garde son argument quand la cible est la Game Boy, le retire sinon. */
#define NKENTSEU_GAMEBOY_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Game Boy, le garde sinon. */
#define NKENTSEU_NOT_GAMEBOY(...)
#else
#define NKENTSEU_GAMEBOY_ONLY(...)
#define NKENTSEU_NOT_GAMEBOY(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour consoles Sega
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_DREAMCAST
/** @brief Garde son argument quand la cible est la Dreamcast, le retire sinon. */
#define NKENTSEU_DREAMCAST_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Dreamcast, le garde sinon. */
#define NKENTSEU_NOT_DREAMCAST(...)
#else
#define NKENTSEU_DREAMCAST_ONLY(...)
#define NKENTSEU_NOT_DREAMCAST(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_SATURN
/** @brief Garde son argument quand la cible est la Saturn, le retire sinon. */
#define NKENTSEU_SATURN_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Saturn, le garde sinon. */
#define NKENTSEU_NOT_SATURN(...)
#else
#define NKENTSEU_SATURN_ONLY(...)
#define NKENTSEU_NOT_SATURN(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_GENESIS
/** @brief Garde son argument quand la cible est la Mega Drive (Genesis), le retire sinon. */
#define NKENTSEU_GENESIS_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est la Mega Drive (Genesis), le garde sinon. */
#define NKENTSEU_NOT_GENESIS(...)
#else
#define NKENTSEU_GENESIS_ONLY(...)
#define NKENTSEU_NOT_GENESIS(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour Web et systèmes embarqués
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_EMSCRIPTEN
/** @brief Garde son argument quand la cible est le web, compilé par Emscripten, le retire sinon. */
#define NKENTSEU_WEB_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est le web, compilé par Emscripten, le garde sinon. */
#define NKENTSEU_NOT_WEB(...)
/** @brief Garde son argument quand la cible est le web, compilé par Emscripten, le retire sinon. */
#define NKENTSEU_EMSCRIPTEN_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est le web, compilé par Emscripten, le garde sinon. */
#define NKENTSEU_NOT_EMSCRIPTEN(...)
#else
#define NKENTSEU_WEB_ONLY(...)
#define NKENTSEU_NOT_WEB(...) __VA_ARGS__
#define NKENTSEU_EMSCRIPTEN_ONLY(...)
#define NKENTSEU_NOT_EMSCRIPTEN(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_ESP32
/** @brief Garde son argument quand la cible est un ESP32, le retire sinon. */
#define NKENTSEU_ESP32_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est un ESP32, le garde sinon. */
#define NKENTSEU_NOT_ESP32(...)
#else
#define NKENTSEU_ESP32_ONLY(...)
#define NKENTSEU_NOT_ESP32(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_ARDUINO
/** @brief Garde son argument quand la cible est une carte Arduino, le retire sinon. */
#define NKENTSEU_ARDUINO_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est une carte Arduino, le garde sinon. */
#define NKENTSEU_NOT_ARDUINO(...)
#else
#define NKENTSEU_ARDUINO_ONLY(...)
#define NKENTSEU_NOT_ARDUINO(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_STM32
/** @brief Garde son argument quand la cible est un microcontrôleur STM32, le retire sinon. */
#define NKENTSEU_STM32_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est un microcontrôleur STM32, le garde sinon. */
#define NKENTSEU_NOT_STM32(...)
#else
#define NKENTSEU_STM32_ONLY(...)
#define NKENTSEU_NOT_STM32(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_RASPBERRY_PI
/** @brief Garde son argument quand la cible est probablement un Raspberry Pi (ARM 32 bits sous Linux, ARMv6, v7-A ou v8-A ; détection approximative), le retire sinon. */
#define NKENTSEU_RASPBERRY_PI_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est probablement un Raspberry Pi (ARM 32 bits sous Linux, ARMv6, v7-A ou v8-A ; détection approximative), le garde sinon. */
#define NKENTSEU_NOT_RASPBERRY_PI(...)
#else
#define NKENTSEU_RASPBERRY_PI_ONLY(...)
#define NKENTSEU_NOT_RASPBERRY_PI(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_STEAM_DECK
/** @brief Garde son argument quand la cible est le Steam Deck (Linux x86-64 avec STEAM_DECK ou le runtime Steam), le retire sinon. */
#define NKENTSEU_STEAM_DECK_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est le Steam Deck (Linux x86-64 avec STEAM_DECK ou le runtime Steam), le garde sinon. */
#define NKENTSEU_NOT_STEAM_DECK(...)
#else
#define NKENTSEU_STEAM_DECK_ONLY(...)
#define NKENTSEU_NOT_STEAM_DECK(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour systèmes de fenêtrage (X11/Wayland)
// -------------------------------------------------------------------------

#ifdef NKENTSEU_WINDOWING_XCB
/** @brief Garde son argument quand le fenêtrage choisi est XCB, le retire sinon. */
#define NKENTSEU_XCB_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand le fenêtrage choisi est XCB, le garde sinon. */
#define NKENTSEU_NOT_XCB(...)
#else
#define NKENTSEU_XCB_ONLY(...)
#define NKENTSEU_NOT_XCB(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_WINDOWING_XLIB
/** @brief Garde son argument quand le fenêtrage choisi est Xlib, le retire sinon. */
#define NKENTSEU_XLIB_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand le fenêtrage choisi est Xlib, le garde sinon. */
#define NKENTSEU_NOT_XLIB(...)
#else
#define NKENTSEU_XLIB_ONLY(...)
#define NKENTSEU_NOT_XLIB(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_WINDOWING_WAYLAND
/** @brief Garde son argument quand le fenêtrage choisi est Wayland, le retire sinon. */
#define NKENTSEU_WAYLAND_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand le fenêtrage choisi est Wayland, le garde sinon. */
#define NKENTSEU_NOT_WAYLAND(...)
#else
#define NKENTSEU_WAYLAND_ONLY(...)
#define NKENTSEU_NOT_WAYLAND(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_WINDOWING_X11
/** @brief Garde son argument quand le fenêtrage choisi est X11 (Xlib ou XCB), le retire sinon. */
#define NKENTSEU_X11_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand le fenêtrage choisi est X11 (Xlib ou XCB), le garde sinon. */
#define NKENTSEU_NOT_X11(...)
#else
#define NKENTSEU_X11_ONLY(...)
#define NKENTSEU_NOT_X11(...) __VA_ARGS__
#endif

// -------------------------------------------------------------------------
// Sous-section : Macros pour catégories de plateformes
// -------------------------------------------------------------------------

#ifdef NKENTSEU_PLATFORM_DESKTOP
/** @brief Garde son argument quand la cible est un système de bureau (Windows hors UWP et Xbox, macOS, Linux hors Android), le retire sinon. */
#define NKENTSEU_DESKTOP_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est un système de bureau (Windows hors UWP et Xbox, macOS, Linux hors Android), le garde sinon. */
#define NKENTSEU_NOT_DESKTOP(...)
#else
#define NKENTSEU_DESKTOP_ONLY(...)
#define NKENTSEU_NOT_DESKTOP(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_MOBILE
/** @brief Garde son argument quand la cible est un système mobile, le retire sinon. */
#define NKENTSEU_MOBILE_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est un système mobile, le garde sinon. */
#define NKENTSEU_NOT_MOBILE(...)
#else
#define NKENTSEU_MOBILE_ONLY(...)
#define NKENTSEU_NOT_MOBILE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_CONSOLE
/** @brief Garde son argument quand la cible est une console de jeu, le retire sinon. */
#define NKENTSEU_CONSOLE_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est une console de jeu, le garde sinon. */
#define NKENTSEU_NOT_CONSOLE(...)
#else
#define NKENTSEU_CONSOLE_ONLY(...)
#define NKENTSEU_NOT_CONSOLE(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_HANDHELD
/** @brief Garde son argument quand la cible est une console portable, le retire sinon. */
#define NKENTSEU_HANDHELD_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est une console portable, le garde sinon. */
#define NKENTSEU_NOT_HANDHELD(...)
#else
#define NKENTSEU_HANDHELD_ONLY(...)
#define NKENTSEU_NOT_HANDHELD(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_EMBEDDED
/** @brief Garde son argument quand la cible est un système embarqué, le retire sinon. */
#define NKENTSEU_EMBEDDED_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est un système embarqué, le garde sinon. */
#define NKENTSEU_NOT_EMBEDDED(...)
#else
#define NKENTSEU_EMBEDDED_ONLY(...)
#define NKENTSEU_NOT_EMBEDDED(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_UNIX_LIKE
/** @brief Garde son argument quand la cible est un système POSIX de la famille Unix, le retire sinon. */
#define NKENTSEU_UNIX_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est un système POSIX de la famille Unix, le garde sinon. */
#define NKENTSEU_NOT_UNIX(...)
#else
#define NKENTSEU_UNIX_ONLY(...)
#define NKENTSEU_NOT_UNIX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_PLAYSTATION
/** @brief Garde son argument quand la cible est une PlayStation, quel que soit le modèle, le retire sinon. */
#define NKENTSEU_PLAYSTATION_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est une PlayStation, quel que soit le modèle, le garde sinon. */
#define NKENTSEU_NOT_PLAYSTATION(...)
#else
#define NKENTSEU_PLAYSTATION_ONLY(...)
#define NKENTSEU_NOT_PLAYSTATION(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_XBOX
/** @brief Garde son argument quand la cible est une Xbox, quel que soit le modèle, le retire sinon. */
#define NKENTSEU_XBOX_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est une Xbox, quel que soit le modèle, le garde sinon. */
#define NKENTSEU_NOT_XBOX(...)
#else
#define NKENTSEU_XBOX_ONLY(...)
#define NKENTSEU_NOT_XBOX(...) __VA_ARGS__
#endif

#ifdef NKENTSEU_PLATFORM_NINTENDO
/** @brief Garde son argument quand la cible est une console Nintendo de salon (NES à Switch), le retire sinon. */
#define NKENTSEU_NINTENDO_ONLY(...) __VA_ARGS__
/** @brief Retire son argument quand la cible est une console Nintendo de salon (NES à Switch), le garde sinon. */
#define NKENTSEU_NOT_NINTENDO(...)
#else
#define NKENTSEU_NINTENDO_ONLY(...)
#define NKENTSEU_NOT_NINTENDO(...) __VA_ARGS__
#endif

#endif // NKENTSEU_PLATFORM_NKPLATFORMDETECT_H

// =============================================================================
// Exemples : les exemples COMPILÉS et exécutés sont dans Docs/exemples/Foundation/NKPlatform/. Ceux qui étaient
// ici en commentaire sont gardés tels quels dans Docs/archives/NKPlatform/exemples_des_entetes.md (à convertir).

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================