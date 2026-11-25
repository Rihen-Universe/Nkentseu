# NkPlatformDetect.h

`#include "NKPlatform/NkPlatformDetect.h"`

## Macros

### NKENTSEU_PLATFORM_WINDOWS

```cpp
#define NKENTSEU_PLATFORM_WINDOWS
```

Macro de détection de plateforme Windows

### NKENTSEU_PLATFORM_NAME

```cpp
#define NKENTSEU_PLATFORM_NAME
```

Nom lisible de la plateforme cible, en chaîne (par exemple "Windows").

### NKENTSEU_PLATFORM_WINDOWS_64

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_64
```

Définie, sans valeur, quand la cible est Windows 64 bits (_WIN64).

### NKENTSEU_PLATFORM_VERSION

```cpp
#define NKENTSEU_PLATFORM_VERSION
```

Précision lisible sur la cible, en chaîne (par exemple "Windows 64-bit").

### NKENTSEU_PLATFORM_WINDOWS_32

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_32
```

Définie, sans valeur, quand la cible est Windows 32 bits.

### NKENTSEU_PLATFORM_WINDOWS_10_OR_LATER

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_10_OR_LATER
```

Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 10 ou plus récent.

### NKENTSEU_PLATFORM_WINDOWS_8_1

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_8_1
```

Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 8.1.

### NKENTSEU_PLATFORM_WINDOWS_8

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_8
```

Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 8.

### NKENTSEU_PLATFORM_WINDOWS_7

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_7
```

Définie, sans valeur, quand la version minimale de Windows visée (_WIN32_WINNT) est Windows 7.

### NKENTSEU_PLATFORM_UWP

```cpp
#define NKENTSEU_PLATFORM_UWP
```

Définie, sans valeur, quand la cible est une application UWP (Windows Runtime).

### NKENTSEU_PLATFORM_MACOS

```cpp
#define NKENTSEU_PLATFORM_MACOS
```

Macro de détection de plateforme macOS

### NKENTSEU_PLATFORM_IOS

```cpp
#define NKENTSEU_PLATFORM_IOS
```

Macro de détection de plateforme iOS

### NKENTSEU_PLATFORM_IOS_SIMULATOR

```cpp
#define NKENTSEU_PLATFORM_IOS_SIMULATOR
```

Définie, sans valeur, quand la cible est le simulateur iOS.

### NKENTSEU_PLATFORM_TVOS

```cpp
#define NKENTSEU_PLATFORM_TVOS
```

Définie, sans valeur, quand la cible est tvOS.

### NKENTSEU_PLATFORM_WATCHOS

```cpp
#define NKENTSEU_PLATFORM_WATCHOS
```

Définie, sans valeur, quand la cible est watchOS.

### NKENTSEU_PLATFORM_VISIONOS

```cpp
#define NKENTSEU_PLATFORM_VISIONOS
```

Définie, sans valeur, quand la cible est visionOS.

### NKENTSEU_PLATFORM_VISIONOS_SIMULATOR

```cpp
#define NKENTSEU_PLATFORM_VISIONOS_SIMULATOR
```

Définie, sans valeur, quand la cible est le simulateur visionOS.

### NKENTSEU_PLATFORM_MACCATALYST

```cpp
#define NKENTSEU_PLATFORM_MACCATALYST
```

Définie, sans valeur, quand la cible est Mac Catalyst (une application iPad sur macOS).

### NKENTSEU_PLATFORM_ANDROID

```cpp
#define NKENTSEU_PLATFORM_ANDROID
```

Macro de détection de plateforme Android

### NKENTSEU_PLATFORM_LINUX

```cpp
#define NKENTSEU_PLATFORM_LINUX
```

Macro de détection de plateforme Linux

### NKENTSEU_PLATFORM_FREEBSD

```cpp
#define NKENTSEU_PLATFORM_FREEBSD
```

Définie, sans valeur, quand la cible est FreeBSD.

### NKENTSEU_PLATFORM_OPENBSD

```cpp
#define NKENTSEU_PLATFORM_OPENBSD
```

Définie, sans valeur, quand la cible est OpenBSD.

### NKENTSEU_PLATFORM_NETBSD

```cpp
#define NKENTSEU_PLATFORM_NETBSD
```

Définie, sans valeur, quand la cible est NetBSD.

### NKENTSEU_PLATFORM_SOLARIS

```cpp
#define NKENTSEU_PLATFORM_SOLARIS
```

Définie, sans valeur, quand la cible est Solaris.

### NKENTSEU_PLATFORM_UNIX

```cpp
#define NKENTSEU_PLATFORM_UNIX
```

Définie, sans valeur, quand la cible est un autre Unix, que la détection ne reconnaît pas plus précisément.

### NKENTSEU_PLATFORM_EMSCRIPTEN

```cpp
#define NKENTSEU_PLATFORM_EMSCRIPTEN
```

Macro de détection de plateforme Web (Emscripten/WebAssembly)

### NKENTSEU_PLATFORM_WEB

```cpp
#define NKENTSEU_PLATFORM_WEB
```

Définie, sans valeur, quand la cible est le web, compilé par Emscripten.

### NKENTSEU_FORCE_WINDOWING_XCB_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_XCB_ONLY
```

Forcer l'utilisation de XCB uniquement

### NKENTSEU_FORCE_WINDOWING_XLIB_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
```

Forcer l'utilisation de Xlib uniquement

### NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
```

Forcer l'utilisation de Wayland uniquement

### NKENTSEU_FORCE_WINDOWING_NOOP_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
```

Forcer le backend headless/noop uniquement

### NKENTSEU_WINDOWING_XCB

```cpp
#define NKENTSEU_WINDOWING_XCB
```

Macro de détection du système de fenêtres XCB

### NKENTSEU_WINDOWING_XLIB

```cpp
#define NKENTSEU_WINDOWING_XLIB
```

Macro de détection du système de fenêtres Xlib

### NKENTSEU_WINDOWING_WAYLAND

```cpp
#define NKENTSEU_WINDOWING_WAYLAND
```

Macro de détection du système de fenêtres Wayland

### NKENTSEU_WINDOWING_X11

```cpp
#define NKENTSEU_WINDOWING_X11
```

Définie, sans valeur, quand le fenêtrage choisi est X11 (Xlib ou XCB).

### NKENTSEU_WINDOWING_PREFERRED

```cpp
#define NKENTSEU_WINDOWING_PREFERRED
```

Nom du fenêtrage Linux choisi, en chaîne ("Wayland", "XCB", "Xlib" ou "Noop").

### NKENTSEU_PLATFORM_HARMONYOS

```cpp
#define NKENTSEU_PLATFORM_HARMONYOS
```

Définie, sans valeur, quand la cible est HarmonyOS.

### NKENTSEU_PLATFORM_MOBILE

```cpp
#define NKENTSEU_PLATFORM_MOBILE
```

Macro de catégorie Mobile

### NKENTSEU_PLATFORM_PS3

```cpp
#define NKENTSEU_PLATFORM_PS3
```

Définie, sans valeur, quand la cible est la PlayStation 3.

### NKENTSEU_PLATFORM_PLAYSTATION

```cpp
#define NKENTSEU_PLATFORM_PLAYSTATION
```

Définie, sans valeur, quand la cible est une PlayStation, quel que soit le modèle.

### NKENTSEU_PLATFORM_PS4

```cpp
#define NKENTSEU_PLATFORM_PS4
```

Définie, sans valeur, quand la cible est la PlayStation 4.

### NKENTSEU_PLATFORM_PS5

```cpp
#define NKENTSEU_PLATFORM_PS5
```

Définie, sans valeur, quand la cible est la PlayStation 5.

### NKENTSEU_PLATFORM_PSP

```cpp
#define NKENTSEU_PLATFORM_PSP
```

Définie, sans valeur, quand la cible est la PSP.

### NKENTSEU_PLATFORM_PSVITA

```cpp
#define NKENTSEU_PLATFORM_PSVITA
```

Définie, sans valeur, quand la cible est la PS Vita.

### NKENTSEU_PLATFORM_XBOX_ORIGINAL

```cpp
#define NKENTSEU_PLATFORM_XBOX_ORIGINAL
```

Définie, sans valeur, quand la cible est la première Xbox.

### NKENTSEU_PLATFORM_XBOX

```cpp
#define NKENTSEU_PLATFORM_XBOX
```

Définie, sans valeur, quand la cible est une Xbox, quel que soit le modèle.

### NKENTSEU_PLATFORM_XBOX360

```cpp
#define NKENTSEU_PLATFORM_XBOX360
```

Définie, sans valeur, quand la cible est la Xbox 360.

### NKENTSEU_PLATFORM_XBOXONE

```cpp
#define NKENTSEU_PLATFORM_XBOXONE
```

Définie, sans valeur, quand la cible est la Xbox One.

### NKENTSEU_PLATFORM_XBOX_SERIES

```cpp
#define NKENTSEU_PLATFORM_XBOX_SERIES
```

Définie, sans valeur, quand la cible est la Xbox Series.

### NKENTSEU_PLATFORM_NES

```cpp
#define NKENTSEU_PLATFORM_NES
```

Définie, sans valeur, quand la cible est la NES.

### NKENTSEU_PLATFORM_NINTENDO

```cpp
#define NKENTSEU_PLATFORM_NINTENDO
```

Définie, sans valeur, quand la cible est une console Nintendo de salon (NES à Switch).

### NKENTSEU_PLATFORM_SNES

```cpp
#define NKENTSEU_PLATFORM_SNES
```

Définie, sans valeur, quand la cible est la Super Nintendo.

### NKENTSEU_PLATFORM_N64

```cpp
#define NKENTSEU_PLATFORM_N64
```

Définie, sans valeur, quand la cible est la Nintendo 64.

### NKENTSEU_PLATFORM_GAMECUBE

```cpp
#define NKENTSEU_PLATFORM_GAMECUBE
```

Définie, sans valeur, quand la cible est la GameCube.

### NKENTSEU_PLATFORM_WII

```cpp
#define NKENTSEU_PLATFORM_WII
```

Définie, sans valeur, quand la cible est la Wii.

### NKENTSEU_PLATFORM_WIIU

```cpp
#define NKENTSEU_PLATFORM_WIIU
```

Définie, sans valeur, quand la cible est la Wii U.

### NKENTSEU_PLATFORM_SWITCH

```cpp
#define NKENTSEU_PLATFORM_SWITCH
```

Définie, sans valeur, quand la cible est la Nintendo Switch.

### NKENTSEU_PLATFORM_GAMEBOY

```cpp
#define NKENTSEU_PLATFORM_GAMEBOY
```

Définie, sans valeur, quand la cible est la Game Boy.

### NKENTSEU_PLATFORM_GAMEBOY_COLOR

```cpp
#define NKENTSEU_PLATFORM_GAMEBOY_COLOR
```

Définie, sans valeur, quand la cible est la Game Boy Color.

### NKENTSEU_PLATFORM_GBA

```cpp
#define NKENTSEU_PLATFORM_GBA
```

Définie, sans valeur, quand la cible est la Game Boy Advance.

### NKENTSEU_PLATFORM_NDS

```cpp
#define NKENTSEU_PLATFORM_NDS
```

Définie, sans valeur, quand la cible est la Nintendo DS.

### NKENTSEU_PLATFORM_3DS

```cpp
#define NKENTSEU_PLATFORM_3DS
```

Définie, sans valeur, quand la cible est la Nintendo 3DS.

### NKENTSEU_PLATFORM_MASTER_SYSTEM

```cpp
#define NKENTSEU_PLATFORM_MASTER_SYSTEM
```

Définie, sans valeur, quand la cible est la Master System.

### NKENTSEU_PLATFORM_SEGA

```cpp
#define NKENTSEU_PLATFORM_SEGA
```

Définie, sans valeur, quand la cible est une console Sega de salon.

### NKENTSEU_PLATFORM_GENESIS

```cpp
#define NKENTSEU_PLATFORM_GENESIS
```

Définie, sans valeur, quand la cible est la Mega Drive (Genesis).

### NKENTSEU_PLATFORM_SATURN

```cpp
#define NKENTSEU_PLATFORM_SATURN
```

Définie, sans valeur, quand la cible est la Saturn.

### NKENTSEU_PLATFORM_DREAMCAST

```cpp
#define NKENTSEU_PLATFORM_DREAMCAST
```

Définie, sans valeur, quand la cible est la Dreamcast.

### NKENTSEU_PLATFORM_GAME_GEAR

```cpp
#define NKENTSEU_PLATFORM_GAME_GEAR
```

Définie, sans valeur, quand la cible est la Game Gear.

### NKENTSEU_PLATFORM_ATARI2600

```cpp
#define NKENTSEU_PLATFORM_ATARI2600
```

Définie, sans valeur, quand la cible est l'Atari 2600.

### NKENTSEU_PLATFORM_ATARI_JAGUAR

```cpp
#define NKENTSEU_PLATFORM_ATARI_JAGUAR
```

Définie, sans valeur, quand la cible est l'Atari Jaguar.

### NKENTSEU_PLATFORM_NEOGEO

```cpp
#define NKENTSEU_PLATFORM_NEOGEO
```

Définie, sans valeur, quand la cible est la Neo Geo.

### NKENTSEU_PLATFORM_3DO

```cpp
#define NKENTSEU_PLATFORM_3DO
```

Définie, sans valeur, quand la cible est la 3DO.

### NKENTSEU_PLATFORM_ARDUINO

```cpp
#define NKENTSEU_PLATFORM_ARDUINO
```

Définie, sans valeur, quand la cible est une carte Arduino.

### NKENTSEU_PLATFORM_EMBEDDED

```cpp
#define NKENTSEU_PLATFORM_EMBEDDED
```

Définie, sans valeur, quand la cible est un système embarqué.

### NKENTSEU_PLATFORM_ESP32

```cpp
#define NKENTSEU_PLATFORM_ESP32
```

Définie, sans valeur, quand la cible est un ESP32.

### NKENTSEU_PLATFORM_ESP8266

```cpp
#define NKENTSEU_PLATFORM_ESP8266
```

Définie, sans valeur, quand la cible est un ESP8266.

### NKENTSEU_PLATFORM_STM32

```cpp
#define NKENTSEU_PLATFORM_STM32
```

Définie, sans valeur, quand la cible est un microcontrôleur STM32.

### NKENTSEU_PLATFORM_RASPBERRY_PI

```cpp
#define NKENTSEU_PLATFORM_RASPBERRY_PI
```

Définie, sans valeur, quand la cible est probablement un Raspberry Pi (ARM 32 bits sous Linux, ARMv6, v7-A ou v8-A ; détection approximative).

### NKENTSEU_PLATFORM_TEENSY

```cpp
#define NKENTSEU_PLATFORM_TEENSY
```

Définie, sans valeur, quand la cible est une carte Teensy.

### NKENTSEU_PLATFORM_STEAM_DECK

```cpp
#define NKENTSEU_PLATFORM_STEAM_DECK
```

Définie, sans valeur, quand la cible est le Steam Deck (Linux x86-64 avec STEAM_DECK ou le runtime Steam).

### NKENTSEU_PLATFORM_STEAM

```cpp
#define NKENTSEU_PLATFORM_STEAM
```

Définie, sans valeur, quand la cible est le Steam Deck ou le runtime Steam.

### NKENTSEU_PLATFORM_STEAM_RUNTIME

```cpp
#define NKENTSEU_PLATFORM_STEAM_RUNTIME
```

Définie, sans valeur, quand la cible est le runtime Steam (__STEAM_RUNTIME__).

### NKENTSEU_PLATFORM_DESKTOP

```cpp
#define NKENTSEU_PLATFORM_DESKTOP
```

Macro de catégorie Desktop

### NKENTSEU_PLATFORM_HANDHELD

```cpp
#define NKENTSEU_PLATFORM_HANDHELD
```

Définie, sans valeur, quand la cible est une console portable.

### NKENTSEU_PLATFORM_CONSOLE

```cpp
#define NKENTSEU_PLATFORM_CONSOLE
```

Macro de catégorie Console

### NKENTSEU_PLATFORM_EMSCRIPTEN_BROWSER

```cpp
#define NKENTSEU_PLATFORM_EMSCRIPTEN_BROWSER
```

Définie, sans valeur, quand la cible est le web (compilation Emscripten pour navigateur).

### NKENTSEU_PLATFORM_POSIX

```cpp
#define NKENTSEU_PLATFORM_POSIX
```

Définie, sans valeur, quand la cible est un système POSIX (Linux, macOS, les BSD, Android, iOS et leurs dérivés).

### NKENTSEU_PLATFORM_UNIX_LIKE

```cpp
#define NKENTSEU_PLATFORM_UNIX_LIKE
```

Définie, sans valeur, quand la cible est un système POSIX de la famille Unix.

### NKENTSEU_PLATFORM_UNKNOWN

```cpp
#define NKENTSEU_PLATFORM_UNKNOWN
```

Définie, sans valeur, quand la cible est aucune plateforme n'a été reconnue (le nom vaut alors "Unknown").

### NKENTSEU_WINDOWS_ONLY

```cpp
#define NKENTSEU_WINDOWS_ONLY(...)
```

Garde son argument quand la cible est Windows, le retire sinon.

### NKENTSEU_NOT_WINDOWS

```cpp
#define NKENTSEU_NOT_WINDOWS(...)
```

Retire son argument quand la cible est Windows, le garde sinon.

### NKENTSEU_LINUX_ONLY

```cpp
#define NKENTSEU_LINUX_ONLY(...)
```

Garde son argument quand la cible est Linux, le retire sinon.

### NKENTSEU_NOT_LINUX

```cpp
#define NKENTSEU_NOT_LINUX(...)
```

Retire son argument quand la cible est Linux, le garde sinon.

### NKENTSEU_MACOS_ONLY

```cpp
#define NKENTSEU_MACOS_ONLY(...)
```

Garde son argument quand la cible est macOS, le retire sinon.

### NKENTSEU_NOT_MACOS

```cpp
#define NKENTSEU_NOT_MACOS(...)
```

Retire son argument quand la cible est macOS, le garde sinon.

### NKENTSEU_IOS_ONLY

```cpp
#define NKENTSEU_IOS_ONLY(...)
```

Garde son argument quand la cible est iOS, le retire sinon.

### NKENTSEU_NOT_IOS

```cpp
#define NKENTSEU_NOT_IOS(...)
```

Retire son argument quand la cible est iOS, le garde sinon.

### NKENTSEU_ANDROID_ONLY

```cpp
#define NKENTSEU_ANDROID_ONLY(...)
```

Garde son argument quand la cible est Android, le retire sinon.

### NKENTSEU_NOT_ANDROID

```cpp
#define NKENTSEU_NOT_ANDROID(...)
```

Retire son argument quand la cible est Android, le garde sinon.

### NKENTSEU_TVOS_ONLY

```cpp
#define NKENTSEU_TVOS_ONLY(...)
```

Garde son argument quand la cible est tvOS, le retire sinon.

### NKENTSEU_NOT_TVOS

```cpp
#define NKENTSEU_NOT_TVOS(...)
```

Retire son argument quand la cible est tvOS, le garde sinon.

### NKENTSEU_WATCHOS_ONLY

```cpp
#define NKENTSEU_WATCHOS_ONLY(...)
```

Garde son argument quand la cible est watchOS, le retire sinon.

### NKENTSEU_NOT_WATCHOS

```cpp
#define NKENTSEU_NOT_WATCHOS(...)
```

Retire son argument quand la cible est watchOS, le garde sinon.

### NKENTSEU_VISIONOS_ONLY

```cpp
#define NKENTSEU_VISIONOS_ONLY(...)
```

Garde son argument quand la cible est visionOS, le retire sinon.

### NKENTSEU_NOT_VISIONOS

```cpp
#define NKENTSEU_NOT_VISIONOS(...)
```

Retire son argument quand la cible est visionOS, le garde sinon.

### NKENTSEU_IPADOS_ONLY

```cpp
#define NKENTSEU_IPADOS_ONLY(...)
```

Garde son argument quand la cible est iPadOS, le retire sinon.

### NKENTSEU_NOT_IPADOS

```cpp
#define NKENTSEU_NOT_IPADOS(...)
```

Retire son argument quand la cible est iPadOS, le garde sinon.

### NKENTSEU_FREEBSD_ONLY

```cpp
#define NKENTSEU_FREEBSD_ONLY(...)
```

Garde son argument quand la cible est FreeBSD, le retire sinon.

### NKENTSEU_NOT_FREEBSD

```cpp
#define NKENTSEU_NOT_FREEBSD(...)
```

Retire son argument quand la cible est FreeBSD, le garde sinon.

### NKENTSEU_HARMONYOS_ONLY

```cpp
#define NKENTSEU_HARMONYOS_ONLY(...)
```

Garde son argument quand la cible est HarmonyOS, le retire sinon.

### NKENTSEU_NOT_HARMONYOS

```cpp
#define NKENTSEU_NOT_HARMONYOS(...)
```

Retire son argument quand la cible est HarmonyOS, le garde sinon.

### NKENTSEU_PS5_ONLY

```cpp
#define NKENTSEU_PS5_ONLY(...)
```

Garde son argument quand la cible est la PlayStation 5, le retire sinon.

### NKENTSEU_NOT_PS5

```cpp
#define NKENTSEU_NOT_PS5(...)
```

Retire son argument quand la cible est la PlayStation 5, le garde sinon.

### NKENTSEU_PS4_ONLY

```cpp
#define NKENTSEU_PS4_ONLY(...)
```

Garde son argument quand la cible est la PlayStation 4, le retire sinon.

### NKENTSEU_NOT_PS4

```cpp
#define NKENTSEU_NOT_PS4(...)
```

Retire son argument quand la cible est la PlayStation 4, le garde sinon.

### NKENTSEU_PS3_ONLY

```cpp
#define NKENTSEU_PS3_ONLY(...)
```

Garde son argument quand la cible est la PlayStation 3, le retire sinon.

### NKENTSEU_NOT_PS3

```cpp
#define NKENTSEU_NOT_PS3(...)
```

Retire son argument quand la cible est la PlayStation 3, le garde sinon.

### NKENTSEU_PS2_ONLY

```cpp
#define NKENTSEU_PS2_ONLY(...)
```

Garde son argument quand la cible est la PlayStation 2, le retire sinon.

### NKENTSEU_NOT_PS2

```cpp
#define NKENTSEU_NOT_PS2(...)
```

Retire son argument quand la cible est la PlayStation 2, le garde sinon.

### NKENTSEU_PS1_ONLY

```cpp
#define NKENTSEU_PS1_ONLY(...)
```

Garde son argument quand la cible est la première PlayStation, le retire sinon.

### NKENTSEU_NOT_PS1

```cpp
#define NKENTSEU_NOT_PS1(...)
```

Retire son argument quand la cible est la première PlayStation, le garde sinon.

### NKENTSEU_PSP_ONLY

```cpp
#define NKENTSEU_PSP_ONLY(...)
```

Garde son argument quand la cible est la PSP, le retire sinon.

### NKENTSEU_NOT_PSP

```cpp
#define NKENTSEU_NOT_PSP(...)
```

Retire son argument quand la cible est la PSP, le garde sinon.

### NKENTSEU_PSVITA_ONLY

```cpp
#define NKENTSEU_PSVITA_ONLY(...)
```

Garde son argument quand la cible est la PS Vita, le retire sinon.

### NKENTSEU_NOT_PSVITA

```cpp
#define NKENTSEU_NOT_PSVITA(...)
```

Retire son argument quand la cible est la PS Vita, le garde sinon.

### NKENTSEU_XBOX_SERIES_ONLY

```cpp
#define NKENTSEU_XBOX_SERIES_ONLY(...)
```

Garde son argument quand la cible est la Xbox Series, le retire sinon.

### NKENTSEU_NOT_XBOX_SERIES

```cpp
#define NKENTSEU_NOT_XBOX_SERIES(...)
```

Retire son argument quand la cible est la Xbox Series, le garde sinon.

### NKENTSEU_XBOXONE_ONLY

```cpp
#define NKENTSEU_XBOXONE_ONLY(...)
```

Garde son argument quand la cible est la Xbox One, le retire sinon.

### NKENTSEU_NOT_XBOXONE

```cpp
#define NKENTSEU_NOT_XBOXONE(...)
```

Retire son argument quand la cible est la Xbox One, le garde sinon.

### NKENTSEU_XBOX360_ONLY

```cpp
#define NKENTSEU_XBOX360_ONLY(...)
```

Garde son argument quand la cible est la Xbox 360, le retire sinon.

### NKENTSEU_NOT_XBOX360

```cpp
#define NKENTSEU_NOT_XBOX360(...)
```

Retire son argument quand la cible est la Xbox 360, le garde sinon.

### NKENTSEU_XBOX_ORIGINAL_ONLY

```cpp
#define NKENTSEU_XBOX_ORIGINAL_ONLY(...)
```

Garde son argument quand la cible est la première Xbox, le retire sinon.

### NKENTSEU_NOT_XBOX_ORIGINAL

```cpp
#define NKENTSEU_NOT_XBOX_ORIGINAL(...)
```

Retire son argument quand la cible est la première Xbox, le garde sinon.

### NKENTSEU_SWITCH_ONLY

```cpp
#define NKENTSEU_SWITCH_ONLY(...)
```

Garde son argument quand la cible est la Nintendo Switch, le retire sinon.

### NKENTSEU_NOT_SWITCH

```cpp
#define NKENTSEU_NOT_SWITCH(...)
```

Retire son argument quand la cible est la Nintendo Switch, le garde sinon.

### NKENTSEU_WIIU_ONLY

```cpp
#define NKENTSEU_WIIU_ONLY(...)
```

Garde son argument quand la cible est la Wii U, le retire sinon.

### NKENTSEU_NOT_WIIU

```cpp
#define NKENTSEU_NOT_WIIU(...)
```

Retire son argument quand la cible est la Wii U, le garde sinon.

### NKENTSEU_WII_ONLY

```cpp
#define NKENTSEU_WII_ONLY(...)
```

Garde son argument quand la cible est la Wii, le retire sinon.

### NKENTSEU_NOT_WII

```cpp
#define NKENTSEU_NOT_WII(...)
```

Retire son argument quand la cible est la Wii, le garde sinon.

### NKENTSEU_GAMECUBE_ONLY

```cpp
#define NKENTSEU_GAMECUBE_ONLY(...)
```

Garde son argument quand la cible est la GameCube, le retire sinon.

### NKENTSEU_NOT_GAMECUBE

```cpp
#define NKENTSEU_NOT_GAMECUBE(...)
```

Retire son argument quand la cible est la GameCube, le garde sinon.

### NKENTSEU_N64_ONLY

```cpp
#define NKENTSEU_N64_ONLY(...)
```

Garde son argument quand la cible est la Nintendo 64, le retire sinon.

### NKENTSEU_NOT_N64

```cpp
#define NKENTSEU_NOT_N64(...)
```

Retire son argument quand la cible est la Nintendo 64, le garde sinon.

### NKENTSEU_3DS_ONLY

```cpp
#define NKENTSEU_3DS_ONLY(...)
```

Garde son argument quand la cible est la Nintendo 3DS, le retire sinon.

### NKENTSEU_NOT_3DS

```cpp
#define NKENTSEU_NOT_3DS(...)
```

Retire son argument quand la cible est la Nintendo 3DS, le garde sinon.

### NKENTSEU_NDS_ONLY

```cpp
#define NKENTSEU_NDS_ONLY(...)
```

Garde son argument quand la cible est la Nintendo DS, le retire sinon.

### NKENTSEU_NOT_NDS

```cpp
#define NKENTSEU_NOT_NDS(...)
```

Retire son argument quand la cible est la Nintendo DS, le garde sinon.

### NKENTSEU_GBA_ONLY

```cpp
#define NKENTSEU_GBA_ONLY(...)
```

Garde son argument quand la cible est la Game Boy Advance, le retire sinon.

### NKENTSEU_NOT_GBA

```cpp
#define NKENTSEU_NOT_GBA(...)
```

Retire son argument quand la cible est la Game Boy Advance, le garde sinon.

### NKENTSEU_GAMEBOY_ONLY

```cpp
#define NKENTSEU_GAMEBOY_ONLY(...)
```

Garde son argument quand la cible est la Game Boy, le retire sinon.

### NKENTSEU_NOT_GAMEBOY

```cpp
#define NKENTSEU_NOT_GAMEBOY(...)
```

Retire son argument quand la cible est la Game Boy, le garde sinon.

### NKENTSEU_DREAMCAST_ONLY

```cpp
#define NKENTSEU_DREAMCAST_ONLY(...)
```

Garde son argument quand la cible est la Dreamcast, le retire sinon.

### NKENTSEU_NOT_DREAMCAST

```cpp
#define NKENTSEU_NOT_DREAMCAST(...)
```

Retire son argument quand la cible est la Dreamcast, le garde sinon.

### NKENTSEU_SATURN_ONLY

```cpp
#define NKENTSEU_SATURN_ONLY(...)
```

Garde son argument quand la cible est la Saturn, le retire sinon.

### NKENTSEU_NOT_SATURN

```cpp
#define NKENTSEU_NOT_SATURN(...)
```

Retire son argument quand la cible est la Saturn, le garde sinon.

### NKENTSEU_GENESIS_ONLY

```cpp
#define NKENTSEU_GENESIS_ONLY(...)
```

Garde son argument quand la cible est la Mega Drive (Genesis), le retire sinon.

### NKENTSEU_NOT_GENESIS

```cpp
#define NKENTSEU_NOT_GENESIS(...)
```

Retire son argument quand la cible est la Mega Drive (Genesis), le garde sinon.

### NKENTSEU_WEB_ONLY

```cpp
#define NKENTSEU_WEB_ONLY(...)
```

Garde son argument quand la cible est le web, compilé par Emscripten, le retire sinon.

### NKENTSEU_NOT_WEB

```cpp
#define NKENTSEU_NOT_WEB(...)
```

Retire son argument quand la cible est le web, compilé par Emscripten, le garde sinon.

### NKENTSEU_EMSCRIPTEN_ONLY

```cpp
#define NKENTSEU_EMSCRIPTEN_ONLY(...)
```

Garde son argument quand la cible est le web, compilé par Emscripten, le retire sinon.

### NKENTSEU_NOT_EMSCRIPTEN

```cpp
#define NKENTSEU_NOT_EMSCRIPTEN(...)
```

Retire son argument quand la cible est le web, compilé par Emscripten, le garde sinon.

### NKENTSEU_ESP32_ONLY

```cpp
#define NKENTSEU_ESP32_ONLY(...)
```

Garde son argument quand la cible est un ESP32, le retire sinon.

### NKENTSEU_NOT_ESP32

```cpp
#define NKENTSEU_NOT_ESP32(...)
```

Retire son argument quand la cible est un ESP32, le garde sinon.

### NKENTSEU_ARDUINO_ONLY

```cpp
#define NKENTSEU_ARDUINO_ONLY(...)
```

Garde son argument quand la cible est une carte Arduino, le retire sinon.

### NKENTSEU_NOT_ARDUINO

```cpp
#define NKENTSEU_NOT_ARDUINO(...)
```

Retire son argument quand la cible est une carte Arduino, le garde sinon.

### NKENTSEU_STM32_ONLY

```cpp
#define NKENTSEU_STM32_ONLY(...)
```

Garde son argument quand la cible est un microcontrôleur STM32, le retire sinon.

### NKENTSEU_NOT_STM32

```cpp
#define NKENTSEU_NOT_STM32(...)
```

Retire son argument quand la cible est un microcontrôleur STM32, le garde sinon.

### NKENTSEU_RASPBERRY_PI_ONLY

```cpp
#define NKENTSEU_RASPBERRY_PI_ONLY(...)
```

Garde son argument quand la cible est probablement un Raspberry Pi (ARM 32 bits sous Linux, ARMv6, v7-A ou v8-A ; détection approximative), le retire sinon.

### NKENTSEU_NOT_RASPBERRY_PI

```cpp
#define NKENTSEU_NOT_RASPBERRY_PI(...)
```

Retire son argument quand la cible est probablement un Raspberry Pi (ARM 32 bits sous Linux, ARMv6, v7-A ou v8-A ; détection approximative), le garde sinon.

### NKENTSEU_STEAM_DECK_ONLY

```cpp
#define NKENTSEU_STEAM_DECK_ONLY(...)
```

Garde son argument quand la cible est le Steam Deck (Linux x86-64 avec STEAM_DECK ou le runtime Steam), le retire sinon.

### NKENTSEU_NOT_STEAM_DECK

```cpp
#define NKENTSEU_NOT_STEAM_DECK(...)
```

Retire son argument quand la cible est le Steam Deck (Linux x86-64 avec STEAM_DECK ou le runtime Steam), le garde sinon.

### NKENTSEU_XCB_ONLY

```cpp
#define NKENTSEU_XCB_ONLY(...)
```

Garde son argument quand le fenêtrage choisi est XCB, le retire sinon.

### NKENTSEU_NOT_XCB

```cpp
#define NKENTSEU_NOT_XCB(...)
```

Retire son argument quand le fenêtrage choisi est XCB, le garde sinon.

### NKENTSEU_XLIB_ONLY

```cpp
#define NKENTSEU_XLIB_ONLY(...)
```

Garde son argument quand le fenêtrage choisi est Xlib, le retire sinon.

### NKENTSEU_NOT_XLIB

```cpp
#define NKENTSEU_NOT_XLIB(...)
```

Retire son argument quand le fenêtrage choisi est Xlib, le garde sinon.

### NKENTSEU_WAYLAND_ONLY

```cpp
#define NKENTSEU_WAYLAND_ONLY(...)
```

Garde son argument quand le fenêtrage choisi est Wayland, le retire sinon.

### NKENTSEU_NOT_WAYLAND

```cpp
#define NKENTSEU_NOT_WAYLAND(...)
```

Retire son argument quand le fenêtrage choisi est Wayland, le garde sinon.

### NKENTSEU_X11_ONLY

```cpp
#define NKENTSEU_X11_ONLY(...)
```

Garde son argument quand le fenêtrage choisi est X11 (Xlib ou XCB), le retire sinon.

### NKENTSEU_NOT_X11

```cpp
#define NKENTSEU_NOT_X11(...)
```

Retire son argument quand le fenêtrage choisi est X11 (Xlib ou XCB), le garde sinon.

### NKENTSEU_DESKTOP_ONLY

```cpp
#define NKENTSEU_DESKTOP_ONLY(...)
```

Garde son argument quand la cible est un système de bureau (Windows hors UWP et Xbox, macOS, Linux hors Android), le retire sinon.

### NKENTSEU_NOT_DESKTOP

```cpp
#define NKENTSEU_NOT_DESKTOP(...)
```

Retire son argument quand la cible est un système de bureau (Windows hors UWP et Xbox, macOS, Linux hors Android), le garde sinon.

### NKENTSEU_MOBILE_ONLY

```cpp
#define NKENTSEU_MOBILE_ONLY(...)
```

Garde son argument quand la cible est un système mobile, le retire sinon.

### NKENTSEU_NOT_MOBILE

```cpp
#define NKENTSEU_NOT_MOBILE(...)
```

Retire son argument quand la cible est un système mobile, le garde sinon.

### NKENTSEU_CONSOLE_ONLY

```cpp
#define NKENTSEU_CONSOLE_ONLY(...)
```

Garde son argument quand la cible est une console de jeu, le retire sinon.

### NKENTSEU_NOT_CONSOLE

```cpp
#define NKENTSEU_NOT_CONSOLE(...)
```

Retire son argument quand la cible est une console de jeu, le garde sinon.

### NKENTSEU_HANDHELD_ONLY

```cpp
#define NKENTSEU_HANDHELD_ONLY(...)
```

Garde son argument quand la cible est une console portable, le retire sinon.

### NKENTSEU_NOT_HANDHELD

```cpp
#define NKENTSEU_NOT_HANDHELD(...)
```

Retire son argument quand la cible est une console portable, le garde sinon.

### NKENTSEU_EMBEDDED_ONLY

```cpp
#define NKENTSEU_EMBEDDED_ONLY(...)
```

Garde son argument quand la cible est un système embarqué, le retire sinon.

### NKENTSEU_NOT_EMBEDDED

```cpp
#define NKENTSEU_NOT_EMBEDDED(...)
```

Retire son argument quand la cible est un système embarqué, le garde sinon.

### NKENTSEU_UNIX_ONLY

```cpp
#define NKENTSEU_UNIX_ONLY(...)
```

Garde son argument quand la cible est un système POSIX de la famille Unix, le retire sinon.

### NKENTSEU_NOT_UNIX

```cpp
#define NKENTSEU_NOT_UNIX(...)
```

Retire son argument quand la cible est un système POSIX de la famille Unix, le garde sinon.

### NKENTSEU_PLAYSTATION_ONLY

```cpp
#define NKENTSEU_PLAYSTATION_ONLY(...)
```

Garde son argument quand la cible est une PlayStation, quel que soit le modèle, le retire sinon.

### NKENTSEU_NOT_PLAYSTATION

```cpp
#define NKENTSEU_NOT_PLAYSTATION(...)
```

Retire son argument quand la cible est une PlayStation, quel que soit le modèle, le garde sinon.

### NKENTSEU_XBOX_ONLY

```cpp
#define NKENTSEU_XBOX_ONLY(...)
```

Garde son argument quand la cible est une Xbox, quel que soit le modèle, le retire sinon.

### NKENTSEU_NOT_XBOX

```cpp
#define NKENTSEU_NOT_XBOX(...)
```

Retire son argument quand la cible est une Xbox, quel que soit le modèle, le garde sinon.

### NKENTSEU_NINTENDO_ONLY

```cpp
#define NKENTSEU_NINTENDO_ONLY(...)
```

Garde son argument quand la cible est une console Nintendo de salon (NES à Switch), le retire sinon.

### NKENTSEU_NOT_NINTENDO

```cpp
#define NKENTSEU_NOT_NINTENDO(...)
```

Retire son argument quand la cible est une console Nintendo de salon (NES à Switch), le garde sinon.

