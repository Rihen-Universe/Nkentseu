# NkPlatformDetect.h

`#include "NKPlatform/NkPlatformDetect.h"`

## Macros

### NKENTSEU_PLATFORM_WINDOWS

```cpp
#define NKENTSEU_PLATFORM_WINDOWS
```

Detection macro for the Windows platform

### NKENTSEU_PLATFORM_NAME

```cpp
#define NKENTSEU_PLATFORM_NAME
```

Readable name of the target platform, as a string (for example "Windows").

### NKENTSEU_PLATFORM_WINDOWS_64

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_64
```

Defined, with no value, when the target is 64-bit Windows (_WIN64).

### NKENTSEU_PLATFORM_VERSION

```cpp
#define NKENTSEU_PLATFORM_VERSION
```

Readable detail about the target, as a string (for example "Windows 64-bit").

### NKENTSEU_PLATFORM_WINDOWS_32

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_32
```

Defined, with no value, when the target is 32-bit Windows.

### NKENTSEU_PLATFORM_WINDOWS_10_OR_LATER

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_10_OR_LATER
```

Defined, with no value, when the minimum Windows version targeted (_WIN32_WINNT) is Windows 10 or later.

### NKENTSEU_PLATFORM_WINDOWS_8_1

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_8_1
```

Defined, with no value, when the minimum Windows version targeted (_WIN32_WINNT) is Windows 8.1.

### NKENTSEU_PLATFORM_WINDOWS_8

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_8
```

Defined, with no value, when the minimum Windows version targeted (_WIN32_WINNT) is Windows 8.

### NKENTSEU_PLATFORM_WINDOWS_7

```cpp
#define NKENTSEU_PLATFORM_WINDOWS_7
```

Defined, with no value, when the minimum Windows version targeted (_WIN32_WINNT) is Windows 7.

### NKENTSEU_PLATFORM_UWP

```cpp
#define NKENTSEU_PLATFORM_UWP
```

Defined, with no value, when the target is a UWP (Windows Runtime) application.

### NKENTSEU_PLATFORM_MACOS

```cpp
#define NKENTSEU_PLATFORM_MACOS
```

Detection macro for the macOS platform

### NKENTSEU_PLATFORM_IOS

```cpp
#define NKENTSEU_PLATFORM_IOS
```

Detection macro for the iOS platform

### NKENTSEU_PLATFORM_IOS_SIMULATOR

```cpp
#define NKENTSEU_PLATFORM_IOS_SIMULATOR
```

Defined, with no value, when the target is the iOS simulator.

### NKENTSEU_PLATFORM_TVOS

```cpp
#define NKENTSEU_PLATFORM_TVOS
```

Defined, with no value, when the target is tvOS.

### NKENTSEU_PLATFORM_WATCHOS

```cpp
#define NKENTSEU_PLATFORM_WATCHOS
```

Defined, with no value, when the target is watchOS.

### NKENTSEU_PLATFORM_VISIONOS

```cpp
#define NKENTSEU_PLATFORM_VISIONOS
```

Defined, with no value, when the target is visionOS.

### NKENTSEU_PLATFORM_VISIONOS_SIMULATOR

```cpp
#define NKENTSEU_PLATFORM_VISIONOS_SIMULATOR
```

Defined, with no value, when the target is the visionOS simulator.

### NKENTSEU_PLATFORM_MACCATALYST

```cpp
#define NKENTSEU_PLATFORM_MACCATALYST
```

Defined, with no value, when the target is Mac Catalyst (an iPad app on macOS).

### NKENTSEU_PLATFORM_ANDROID

```cpp
#define NKENTSEU_PLATFORM_ANDROID
```

Detection macro for the Android platform

### NKENTSEU_PLATFORM_LINUX

```cpp
#define NKENTSEU_PLATFORM_LINUX
```

Detection macro for the Linux platform

### NKENTSEU_PLATFORM_FREEBSD

```cpp
#define NKENTSEU_PLATFORM_FREEBSD
```

Defined, with no value, when the target is FreeBSD.

### NKENTSEU_PLATFORM_OPENBSD

```cpp
#define NKENTSEU_PLATFORM_OPENBSD
```

Defined, with no value, when the target is OpenBSD.

### NKENTSEU_PLATFORM_NETBSD

```cpp
#define NKENTSEU_PLATFORM_NETBSD
```

Defined, with no value, when the target is NetBSD.

### NKENTSEU_PLATFORM_SOLARIS

```cpp
#define NKENTSEU_PLATFORM_SOLARIS
```

Defined, with no value, when the target is Solaris.

### NKENTSEU_PLATFORM_UNIX

```cpp
#define NKENTSEU_PLATFORM_UNIX
```

Defined, with no value, when the target is another Unix that detection does not recognise more precisely.

### NKENTSEU_PLATFORM_EMSCRIPTEN

```cpp
#define NKENTSEU_PLATFORM_EMSCRIPTEN
```

Detection macro for the Web platform (Emscripten/WebAssembly)

### NKENTSEU_PLATFORM_WEB

```cpp
#define NKENTSEU_PLATFORM_WEB
```

Defined, with no value, when the target is the web, compiled by Emscripten.

### NKENTSEU_FORCE_WINDOWING_XCB_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_XCB_ONLY
```

Forces the use of XCB only

### NKENTSEU_FORCE_WINDOWING_XLIB_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_XLIB_ONLY
```

Forces the use of Xlib only

### NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
```

Forces the use of Wayland only

### NKENTSEU_FORCE_WINDOWING_NOOP_ONLY

```cpp
#define NKENTSEU_FORCE_WINDOWING_NOOP_ONLY
```

Forces the headless/noop backend only

### NKENTSEU_WINDOWING_XCB

```cpp
#define NKENTSEU_WINDOWING_XCB
```

Detection macro for the XCB windowing system

### NKENTSEU_WINDOWING_XLIB

```cpp
#define NKENTSEU_WINDOWING_XLIB
```

Detection macro for the Xlib windowing system

### NKENTSEU_WINDOWING_WAYLAND

```cpp
#define NKENTSEU_WINDOWING_WAYLAND
```

Detection macro for the Wayland windowing system

### NKENTSEU_WINDOWING_X11

```cpp
#define NKENTSEU_WINDOWING_X11
```

Defined, with no value, when the chosen windowing system is X11 (Xlib or XCB).

### NKENTSEU_WINDOWING_PREFERRED

```cpp
#define NKENTSEU_WINDOWING_PREFERRED
```

Name of the chosen Linux windowing system, as a string ("Wayland", "XCB", "Xlib" or "Noop").

### NKENTSEU_PLATFORM_HARMONYOS

```cpp
#define NKENTSEU_PLATFORM_HARMONYOS
```

Defined, with no value, when the target is HarmonyOS.

### NKENTSEU_PLATFORM_MOBILE

```cpp
#define NKENTSEU_PLATFORM_MOBILE
```

Mobile category macro

### NKENTSEU_PLATFORM_PS3

```cpp
#define NKENTSEU_PLATFORM_PS3
```

Defined, with no value, when the target is the PlayStation 3.

### NKENTSEU_PLATFORM_PLAYSTATION

```cpp
#define NKENTSEU_PLATFORM_PLAYSTATION
```

Defined, with no value, when the target is a PlayStation, whatever the model.

### NKENTSEU_PLATFORM_PS4

```cpp
#define NKENTSEU_PLATFORM_PS4
```

Defined, with no value, when the target is the PlayStation 4.

### NKENTSEU_PLATFORM_PS5

```cpp
#define NKENTSEU_PLATFORM_PS5
```

Defined, with no value, when the target is the PlayStation 5.

### NKENTSEU_PLATFORM_PSP

```cpp
#define NKENTSEU_PLATFORM_PSP
```

Defined, with no value, when the target is the PSP.

### NKENTSEU_PLATFORM_PSVITA

```cpp
#define NKENTSEU_PLATFORM_PSVITA
```

Defined, with no value, when the target is the PS Vita.

### NKENTSEU_PLATFORM_XBOX_ORIGINAL

```cpp
#define NKENTSEU_PLATFORM_XBOX_ORIGINAL
```

Defined, with no value, when the target is the original Xbox.

### NKENTSEU_PLATFORM_XBOX

```cpp
#define NKENTSEU_PLATFORM_XBOX
```

Defined, with no value, when the target is an Xbox, whatever the model.

### NKENTSEU_PLATFORM_XBOX360

```cpp
#define NKENTSEU_PLATFORM_XBOX360
```

Defined, with no value, when the target is the Xbox 360.

### NKENTSEU_PLATFORM_XBOXONE

```cpp
#define NKENTSEU_PLATFORM_XBOXONE
```

Defined, with no value, when the target is the Xbox One.

### NKENTSEU_PLATFORM_XBOX_SERIES

```cpp
#define NKENTSEU_PLATFORM_XBOX_SERIES
```

Defined, with no value, when the target is the Xbox Series.

### NKENTSEU_PLATFORM_NES

```cpp
#define NKENTSEU_PLATFORM_NES
```

Defined, with no value, when the target is the NES.

### NKENTSEU_PLATFORM_NINTENDO

```cpp
#define NKENTSEU_PLATFORM_NINTENDO
```

Defined, with no value, when the target is a Nintendo home console (NES to Switch).

### NKENTSEU_PLATFORM_SNES

```cpp
#define NKENTSEU_PLATFORM_SNES
```

Defined, with no value, when the target is the Super Nintendo.

### NKENTSEU_PLATFORM_N64

```cpp
#define NKENTSEU_PLATFORM_N64
```

Defined, with no value, when the target is the Nintendo 64.

### NKENTSEU_PLATFORM_GAMECUBE

```cpp
#define NKENTSEU_PLATFORM_GAMECUBE
```

Defined, with no value, when the target is the GameCube.

### NKENTSEU_PLATFORM_WII

```cpp
#define NKENTSEU_PLATFORM_WII
```

Defined, with no value, when the target is the Wii.

### NKENTSEU_PLATFORM_WIIU

```cpp
#define NKENTSEU_PLATFORM_WIIU
```

Defined, with no value, when the target is the Wii U.

### NKENTSEU_PLATFORM_SWITCH

```cpp
#define NKENTSEU_PLATFORM_SWITCH
```

Defined, with no value, when the target is the Nintendo Switch.

### NKENTSEU_PLATFORM_GAMEBOY

```cpp
#define NKENTSEU_PLATFORM_GAMEBOY
```

Defined, with no value, when the target is the Game Boy.

### NKENTSEU_PLATFORM_GAMEBOY_COLOR

```cpp
#define NKENTSEU_PLATFORM_GAMEBOY_COLOR
```

Defined, with no value, when the target is the Game Boy Color.

### NKENTSEU_PLATFORM_GBA

```cpp
#define NKENTSEU_PLATFORM_GBA
```

Defined, with no value, when the target is the Game Boy Advance.

### NKENTSEU_PLATFORM_NDS

```cpp
#define NKENTSEU_PLATFORM_NDS
```

Defined, with no value, when the target is the Nintendo DS.

### NKENTSEU_PLATFORM_3DS

```cpp
#define NKENTSEU_PLATFORM_3DS
```

Defined, with no value, when the target is the Nintendo 3DS.

### NKENTSEU_PLATFORM_MASTER_SYSTEM

```cpp
#define NKENTSEU_PLATFORM_MASTER_SYSTEM
```

Defined, with no value, when the target is the Master System.

### NKENTSEU_PLATFORM_SEGA

```cpp
#define NKENTSEU_PLATFORM_SEGA
```

Defined, with no value, when the target is a Sega home console.

### NKENTSEU_PLATFORM_GENESIS

```cpp
#define NKENTSEU_PLATFORM_GENESIS
```

Defined, with no value, when the target is the Genesis (Mega Drive).

### NKENTSEU_PLATFORM_SATURN

```cpp
#define NKENTSEU_PLATFORM_SATURN
```

Defined, with no value, when the target is the Saturn.

### NKENTSEU_PLATFORM_DREAMCAST

```cpp
#define NKENTSEU_PLATFORM_DREAMCAST
```

Defined, with no value, when the target is the Dreamcast.

### NKENTSEU_PLATFORM_GAME_GEAR

```cpp
#define NKENTSEU_PLATFORM_GAME_GEAR
```

Defined, with no value, when the target is the Game Gear.

### NKENTSEU_PLATFORM_ATARI2600

```cpp
#define NKENTSEU_PLATFORM_ATARI2600
```

Defined, with no value, when the target is the Atari 2600.

### NKENTSEU_PLATFORM_ATARI_JAGUAR

```cpp
#define NKENTSEU_PLATFORM_ATARI_JAGUAR
```

Defined, with no value, when the target is the Atari Jaguar.

### NKENTSEU_PLATFORM_NEOGEO

```cpp
#define NKENTSEU_PLATFORM_NEOGEO
```

Defined, with no value, when the target is the Neo Geo.

### NKENTSEU_PLATFORM_3DO

```cpp
#define NKENTSEU_PLATFORM_3DO
```

Defined, with no value, when the target is the 3DO.

### NKENTSEU_PLATFORM_ARDUINO

```cpp
#define NKENTSEU_PLATFORM_ARDUINO
```

Defined, with no value, when the target is an Arduino board.

### NKENTSEU_PLATFORM_EMBEDDED

```cpp
#define NKENTSEU_PLATFORM_EMBEDDED
```

Defined, with no value, when the target is an embedded system.

### NKENTSEU_PLATFORM_ESP32

```cpp
#define NKENTSEU_PLATFORM_ESP32
```

Defined, with no value, when the target is an ESP32.

### NKENTSEU_PLATFORM_ESP8266

```cpp
#define NKENTSEU_PLATFORM_ESP8266
```

Defined, with no value, when the target is an ESP8266.

### NKENTSEU_PLATFORM_STM32

```cpp
#define NKENTSEU_PLATFORM_STM32
```

Defined, with no value, when the target is an STM32 microcontroller.

### NKENTSEU_PLATFORM_RASPBERRY_PI

```cpp
#define NKENTSEU_PLATFORM_RASPBERRY_PI
```

Defined, with no value, when the target is probably a Raspberry Pi (32-bit ARM on Linux, ARMv6, v7-A or v8-A; approximate detection).

### NKENTSEU_PLATFORM_TEENSY

```cpp
#define NKENTSEU_PLATFORM_TEENSY
```

Defined, with no value, when the target is a Teensy board.

### NKENTSEU_PLATFORM_STEAM_DECK

```cpp
#define NKENTSEU_PLATFORM_STEAM_DECK
```

Defined, with no value, when the target is the Steam Deck (x86-64 Linux with STEAM_DECK or the Steam runtime).

### NKENTSEU_PLATFORM_STEAM

```cpp
#define NKENTSEU_PLATFORM_STEAM
```

Defined, with no value, when the target is the Steam Deck or the Steam runtime.

### NKENTSEU_PLATFORM_STEAM_RUNTIME

```cpp
#define NKENTSEU_PLATFORM_STEAM_RUNTIME
```

Defined, with no value, when the target is the Steam runtime (__STEAM_RUNTIME__).

### NKENTSEU_PLATFORM_DESKTOP

```cpp
#define NKENTSEU_PLATFORM_DESKTOP
```

Desktop category macro

### NKENTSEU_PLATFORM_HANDHELD

```cpp
#define NKENTSEU_PLATFORM_HANDHELD
```

Defined, with no value, when the target is a handheld console.

### NKENTSEU_PLATFORM_CONSOLE

```cpp
#define NKENTSEU_PLATFORM_CONSOLE
```

Console category macro

### NKENTSEU_PLATFORM_EMSCRIPTEN_BROWSER

```cpp
#define NKENTSEU_PLATFORM_EMSCRIPTEN_BROWSER
```

Defined, with no value, when the target is the web (Emscripten build for a browser).

### NKENTSEU_PLATFORM_POSIX

```cpp
#define NKENTSEU_PLATFORM_POSIX
```

Defined, with no value, when the target is a POSIX system (Linux, macOS, the BSDs, Android, iOS and their relatives).

### NKENTSEU_PLATFORM_UNIX_LIKE

```cpp
#define NKENTSEU_PLATFORM_UNIX_LIKE
```

Defined, with no value, when the target is a POSIX system of the Unix family.

### NKENTSEU_PLATFORM_UNKNOWN

```cpp
#define NKENTSEU_PLATFORM_UNKNOWN
```

Defined, with no value, when the target is no platform was recognised (the name is then "Unknown").

### NKENTSEU_WINDOWS_ONLY

```cpp
#define NKENTSEU_WINDOWS_ONLY(...)
```

Keeps its argument when the target is Windows, removes it otherwise.

### NKENTSEU_NOT_WINDOWS

```cpp
#define NKENTSEU_NOT_WINDOWS(...)
```

Removes its argument when the target is Windows, keeps it otherwise.

### NKENTSEU_LINUX_ONLY

```cpp
#define NKENTSEU_LINUX_ONLY(...)
```

Keeps its argument when the target is Linux, removes it otherwise.

### NKENTSEU_NOT_LINUX

```cpp
#define NKENTSEU_NOT_LINUX(...)
```

Removes its argument when the target is Linux, keeps it otherwise.

### NKENTSEU_MACOS_ONLY

```cpp
#define NKENTSEU_MACOS_ONLY(...)
```

Keeps its argument when the target is macOS, removes it otherwise.

### NKENTSEU_NOT_MACOS

```cpp
#define NKENTSEU_NOT_MACOS(...)
```

Removes its argument when the target is macOS, keeps it otherwise.

### NKENTSEU_IOS_ONLY

```cpp
#define NKENTSEU_IOS_ONLY(...)
```

Keeps its argument when the target is iOS, removes it otherwise.

### NKENTSEU_NOT_IOS

```cpp
#define NKENTSEU_NOT_IOS(...)
```

Removes its argument when the target is iOS, keeps it otherwise.

### NKENTSEU_ANDROID_ONLY

```cpp
#define NKENTSEU_ANDROID_ONLY(...)
```

Keeps its argument when the target is Android, removes it otherwise.

### NKENTSEU_NOT_ANDROID

```cpp
#define NKENTSEU_NOT_ANDROID(...)
```

Removes its argument when the target is Android, keeps it otherwise.

### NKENTSEU_TVOS_ONLY

```cpp
#define NKENTSEU_TVOS_ONLY(...)
```

Keeps its argument when the target is tvOS, removes it otherwise.

### NKENTSEU_NOT_TVOS

```cpp
#define NKENTSEU_NOT_TVOS(...)
```

Removes its argument when the target is tvOS, keeps it otherwise.

### NKENTSEU_WATCHOS_ONLY

```cpp
#define NKENTSEU_WATCHOS_ONLY(...)
```

Keeps its argument when the target is watchOS, removes it otherwise.

### NKENTSEU_NOT_WATCHOS

```cpp
#define NKENTSEU_NOT_WATCHOS(...)
```

Removes its argument when the target is watchOS, keeps it otherwise.

### NKENTSEU_VISIONOS_ONLY

```cpp
#define NKENTSEU_VISIONOS_ONLY(...)
```

Keeps its argument when the target is visionOS, removes it otherwise.

### NKENTSEU_NOT_VISIONOS

```cpp
#define NKENTSEU_NOT_VISIONOS(...)
```

Removes its argument when the target is visionOS, keeps it otherwise.

### NKENTSEU_IPADOS_ONLY

```cpp
#define NKENTSEU_IPADOS_ONLY(...)
```

Keeps its argument when the target is iPadOS, removes it otherwise.

### NKENTSEU_NOT_IPADOS

```cpp
#define NKENTSEU_NOT_IPADOS(...)
```

Removes its argument when the target is iPadOS, keeps it otherwise.

### NKENTSEU_FREEBSD_ONLY

```cpp
#define NKENTSEU_FREEBSD_ONLY(...)
```

Keeps its argument when the target is FreeBSD, removes it otherwise.

### NKENTSEU_NOT_FREEBSD

```cpp
#define NKENTSEU_NOT_FREEBSD(...)
```

Removes its argument when the target is FreeBSD, keeps it otherwise.

### NKENTSEU_HARMONYOS_ONLY

```cpp
#define NKENTSEU_HARMONYOS_ONLY(...)
```

Keeps its argument when the target is HarmonyOS, removes it otherwise.

### NKENTSEU_NOT_HARMONYOS

```cpp
#define NKENTSEU_NOT_HARMONYOS(...)
```

Removes its argument when the target is HarmonyOS, keeps it otherwise.

### NKENTSEU_PS5_ONLY

```cpp
#define NKENTSEU_PS5_ONLY(...)
```

Keeps its argument when the target is the PlayStation 5, removes it otherwise.

### NKENTSEU_NOT_PS5

```cpp
#define NKENTSEU_NOT_PS5(...)
```

Removes its argument when the target is the PlayStation 5, keeps it otherwise.

### NKENTSEU_PS4_ONLY

```cpp
#define NKENTSEU_PS4_ONLY(...)
```

Keeps its argument when the target is the PlayStation 4, removes it otherwise.

### NKENTSEU_NOT_PS4

```cpp
#define NKENTSEU_NOT_PS4(...)
```

Removes its argument when the target is the PlayStation 4, keeps it otherwise.

### NKENTSEU_PS3_ONLY

```cpp
#define NKENTSEU_PS3_ONLY(...)
```

Keeps its argument when the target is the PlayStation 3, removes it otherwise.

### NKENTSEU_NOT_PS3

```cpp
#define NKENTSEU_NOT_PS3(...)
```

Removes its argument when the target is the PlayStation 3, keeps it otherwise.

### NKENTSEU_PS2_ONLY

```cpp
#define NKENTSEU_PS2_ONLY(...)
```

Keeps its argument when the target is the PlayStation 2, removes it otherwise.

### NKENTSEU_NOT_PS2

```cpp
#define NKENTSEU_NOT_PS2(...)
```

Removes its argument when the target is the PlayStation 2, keeps it otherwise.

### NKENTSEU_PS1_ONLY

```cpp
#define NKENTSEU_PS1_ONLY(...)
```

Keeps its argument when the target is the original PlayStation, removes it otherwise.

### NKENTSEU_NOT_PS1

```cpp
#define NKENTSEU_NOT_PS1(...)
```

Removes its argument when the target is the original PlayStation, keeps it otherwise.

### NKENTSEU_PSP_ONLY

```cpp
#define NKENTSEU_PSP_ONLY(...)
```

Keeps its argument when the target is the PSP, removes it otherwise.

### NKENTSEU_NOT_PSP

```cpp
#define NKENTSEU_NOT_PSP(...)
```

Removes its argument when the target is the PSP, keeps it otherwise.

### NKENTSEU_PSVITA_ONLY

```cpp
#define NKENTSEU_PSVITA_ONLY(...)
```

Keeps its argument when the target is the PS Vita, removes it otherwise.

### NKENTSEU_NOT_PSVITA

```cpp
#define NKENTSEU_NOT_PSVITA(...)
```

Removes its argument when the target is the PS Vita, keeps it otherwise.

### NKENTSEU_XBOX_SERIES_ONLY

```cpp
#define NKENTSEU_XBOX_SERIES_ONLY(...)
```

Keeps its argument when the target is the Xbox Series, removes it otherwise.

### NKENTSEU_NOT_XBOX_SERIES

```cpp
#define NKENTSEU_NOT_XBOX_SERIES(...)
```

Removes its argument when the target is the Xbox Series, keeps it otherwise.

### NKENTSEU_XBOXONE_ONLY

```cpp
#define NKENTSEU_XBOXONE_ONLY(...)
```

Keeps its argument when the target is the Xbox One, removes it otherwise.

### NKENTSEU_NOT_XBOXONE

```cpp
#define NKENTSEU_NOT_XBOXONE(...)
```

Removes its argument when the target is the Xbox One, keeps it otherwise.

### NKENTSEU_XBOX360_ONLY

```cpp
#define NKENTSEU_XBOX360_ONLY(...)
```

Keeps its argument when the target is the Xbox 360, removes it otherwise.

### NKENTSEU_NOT_XBOX360

```cpp
#define NKENTSEU_NOT_XBOX360(...)
```

Removes its argument when the target is the Xbox 360, keeps it otherwise.

### NKENTSEU_XBOX_ORIGINAL_ONLY

```cpp
#define NKENTSEU_XBOX_ORIGINAL_ONLY(...)
```

Keeps its argument when the target is the original Xbox, removes it otherwise.

### NKENTSEU_NOT_XBOX_ORIGINAL

```cpp
#define NKENTSEU_NOT_XBOX_ORIGINAL(...)
```

Removes its argument when the target is the original Xbox, keeps it otherwise.

### NKENTSEU_SWITCH_ONLY

```cpp
#define NKENTSEU_SWITCH_ONLY(...)
```

Keeps its argument when the target is the Nintendo Switch, removes it otherwise.

### NKENTSEU_NOT_SWITCH

```cpp
#define NKENTSEU_NOT_SWITCH(...)
```

Removes its argument when the target is the Nintendo Switch, keeps it otherwise.

### NKENTSEU_WIIU_ONLY

```cpp
#define NKENTSEU_WIIU_ONLY(...)
```

Keeps its argument when the target is the Wii U, removes it otherwise.

### NKENTSEU_NOT_WIIU

```cpp
#define NKENTSEU_NOT_WIIU(...)
```

Removes its argument when the target is the Wii U, keeps it otherwise.

### NKENTSEU_WII_ONLY

```cpp
#define NKENTSEU_WII_ONLY(...)
```

Keeps its argument when the target is the Wii, removes it otherwise.

### NKENTSEU_NOT_WII

```cpp
#define NKENTSEU_NOT_WII(...)
```

Removes its argument when the target is the Wii, keeps it otherwise.

### NKENTSEU_GAMECUBE_ONLY

```cpp
#define NKENTSEU_GAMECUBE_ONLY(...)
```

Keeps its argument when the target is the GameCube, removes it otherwise.

### NKENTSEU_NOT_GAMECUBE

```cpp
#define NKENTSEU_NOT_GAMECUBE(...)
```

Removes its argument when the target is the GameCube, keeps it otherwise.

### NKENTSEU_N64_ONLY

```cpp
#define NKENTSEU_N64_ONLY(...)
```

Keeps its argument when the target is the Nintendo 64, removes it otherwise.

### NKENTSEU_NOT_N64

```cpp
#define NKENTSEU_NOT_N64(...)
```

Removes its argument when the target is the Nintendo 64, keeps it otherwise.

### NKENTSEU_3DS_ONLY

```cpp
#define NKENTSEU_3DS_ONLY(...)
```

Keeps its argument when the target is the Nintendo 3DS, removes it otherwise.

### NKENTSEU_NOT_3DS

```cpp
#define NKENTSEU_NOT_3DS(...)
```

Removes its argument when the target is the Nintendo 3DS, keeps it otherwise.

### NKENTSEU_NDS_ONLY

```cpp
#define NKENTSEU_NDS_ONLY(...)
```

Keeps its argument when the target is the Nintendo DS, removes it otherwise.

### NKENTSEU_NOT_NDS

```cpp
#define NKENTSEU_NOT_NDS(...)
```

Removes its argument when the target is the Nintendo DS, keeps it otherwise.

### NKENTSEU_GBA_ONLY

```cpp
#define NKENTSEU_GBA_ONLY(...)
```

Keeps its argument when the target is the Game Boy Advance, removes it otherwise.

### NKENTSEU_NOT_GBA

```cpp
#define NKENTSEU_NOT_GBA(...)
```

Removes its argument when the target is the Game Boy Advance, keeps it otherwise.

### NKENTSEU_GAMEBOY_ONLY

```cpp
#define NKENTSEU_GAMEBOY_ONLY(...)
```

Keeps its argument when the target is the Game Boy, removes it otherwise.

### NKENTSEU_NOT_GAMEBOY

```cpp
#define NKENTSEU_NOT_GAMEBOY(...)
```

Removes its argument when the target is the Game Boy, keeps it otherwise.

### NKENTSEU_DREAMCAST_ONLY

```cpp
#define NKENTSEU_DREAMCAST_ONLY(...)
```

Keeps its argument when the target is the Dreamcast, removes it otherwise.

### NKENTSEU_NOT_DREAMCAST

```cpp
#define NKENTSEU_NOT_DREAMCAST(...)
```

Removes its argument when the target is the Dreamcast, keeps it otherwise.

### NKENTSEU_SATURN_ONLY

```cpp
#define NKENTSEU_SATURN_ONLY(...)
```

Keeps its argument when the target is the Saturn, removes it otherwise.

### NKENTSEU_NOT_SATURN

```cpp
#define NKENTSEU_NOT_SATURN(...)
```

Removes its argument when the target is the Saturn, keeps it otherwise.

### NKENTSEU_GENESIS_ONLY

```cpp
#define NKENTSEU_GENESIS_ONLY(...)
```

Keeps its argument when the target is the Genesis (Mega Drive), removes it otherwise.

### NKENTSEU_NOT_GENESIS

```cpp
#define NKENTSEU_NOT_GENESIS(...)
```

Removes its argument when the target is the Genesis (Mega Drive), keeps it otherwise.

### NKENTSEU_WEB_ONLY

```cpp
#define NKENTSEU_WEB_ONLY(...)
```

Keeps its argument when the target is the web, compiled by Emscripten, removes it otherwise.

### NKENTSEU_NOT_WEB

```cpp
#define NKENTSEU_NOT_WEB(...)
```

Removes its argument when the target is the web, compiled by Emscripten, keeps it otherwise.

### NKENTSEU_EMSCRIPTEN_ONLY

```cpp
#define NKENTSEU_EMSCRIPTEN_ONLY(...)
```

Keeps its argument when the target is the web, compiled by Emscripten, removes it otherwise.

### NKENTSEU_NOT_EMSCRIPTEN

```cpp
#define NKENTSEU_NOT_EMSCRIPTEN(...)
```

Removes its argument when the target is the web, compiled by Emscripten, keeps it otherwise.

### NKENTSEU_ESP32_ONLY

```cpp
#define NKENTSEU_ESP32_ONLY(...)
```

Keeps its argument when the target is an ESP32, removes it otherwise.

### NKENTSEU_NOT_ESP32

```cpp
#define NKENTSEU_NOT_ESP32(...)
```

Removes its argument when the target is an ESP32, keeps it otherwise.

### NKENTSEU_ARDUINO_ONLY

```cpp
#define NKENTSEU_ARDUINO_ONLY(...)
```

Keeps its argument when the target is an Arduino board, removes it otherwise.

### NKENTSEU_NOT_ARDUINO

```cpp
#define NKENTSEU_NOT_ARDUINO(...)
```

Removes its argument when the target is an Arduino board, keeps it otherwise.

### NKENTSEU_STM32_ONLY

```cpp
#define NKENTSEU_STM32_ONLY(...)
```

Keeps its argument when the target is an STM32 microcontroller, removes it otherwise.

### NKENTSEU_NOT_STM32

```cpp
#define NKENTSEU_NOT_STM32(...)
```

Removes its argument when the target is an STM32 microcontroller, keeps it otherwise.

### NKENTSEU_RASPBERRY_PI_ONLY

```cpp
#define NKENTSEU_RASPBERRY_PI_ONLY(...)
```

Keeps its argument when the target is probably a Raspberry Pi (32-bit ARM on Linux, ARMv6, v7-A or v8-A; approximate detection), removes it otherwise.

### NKENTSEU_NOT_RASPBERRY_PI

```cpp
#define NKENTSEU_NOT_RASPBERRY_PI(...)
```

Removes its argument when the target is probably a Raspberry Pi (32-bit ARM on Linux, ARMv6, v7-A or v8-A; approximate detection), keeps it otherwise.

### NKENTSEU_STEAM_DECK_ONLY

```cpp
#define NKENTSEU_STEAM_DECK_ONLY(...)
```

Keeps its argument when the target is the Steam Deck (x86-64 Linux with STEAM_DECK or the Steam runtime), removes it otherwise.

### NKENTSEU_NOT_STEAM_DECK

```cpp
#define NKENTSEU_NOT_STEAM_DECK(...)
```

Removes its argument when the target is the Steam Deck (x86-64 Linux with STEAM_DECK or the Steam runtime), keeps it otherwise.

### NKENTSEU_XCB_ONLY

```cpp
#define NKENTSEU_XCB_ONLY(...)
```

Keeps its argument when the chosen windowing system is XCB, removes it otherwise.

### NKENTSEU_NOT_XCB

```cpp
#define NKENTSEU_NOT_XCB(...)
```

Removes its argument when the chosen windowing system is XCB, keeps it otherwise.

### NKENTSEU_XLIB_ONLY

```cpp
#define NKENTSEU_XLIB_ONLY(...)
```

Keeps its argument when the chosen windowing system is Xlib, removes it otherwise.

### NKENTSEU_NOT_XLIB

```cpp
#define NKENTSEU_NOT_XLIB(...)
```

Removes its argument when the chosen windowing system is Xlib, keeps it otherwise.

### NKENTSEU_WAYLAND_ONLY

```cpp
#define NKENTSEU_WAYLAND_ONLY(...)
```

Keeps its argument when the chosen windowing system is Wayland, removes it otherwise.

### NKENTSEU_NOT_WAYLAND

```cpp
#define NKENTSEU_NOT_WAYLAND(...)
```

Removes its argument when the chosen windowing system is Wayland, keeps it otherwise.

### NKENTSEU_X11_ONLY

```cpp
#define NKENTSEU_X11_ONLY(...)
```

Keeps its argument when the chosen windowing system is X11 (Xlib or XCB), removes it otherwise.

### NKENTSEU_NOT_X11

```cpp
#define NKENTSEU_NOT_X11(...)
```

Removes its argument when the chosen windowing system is X11 (Xlib or XCB), keeps it otherwise.

### NKENTSEU_DESKTOP_ONLY

```cpp
#define NKENTSEU_DESKTOP_ONLY(...)
```

Keeps its argument when the target is a desktop system (Windows except UWP and Xbox, macOS, Linux except Android), removes it otherwise.

### NKENTSEU_NOT_DESKTOP

```cpp
#define NKENTSEU_NOT_DESKTOP(...)
```

Removes its argument when the target is a desktop system (Windows except UWP and Xbox, macOS, Linux except Android), keeps it otherwise.

### NKENTSEU_MOBILE_ONLY

```cpp
#define NKENTSEU_MOBILE_ONLY(...)
```

Keeps its argument when the target is a mobile system, removes it otherwise.

### NKENTSEU_NOT_MOBILE

```cpp
#define NKENTSEU_NOT_MOBILE(...)
```

Removes its argument when the target is a mobile system, keeps it otherwise.

### NKENTSEU_CONSOLE_ONLY

```cpp
#define NKENTSEU_CONSOLE_ONLY(...)
```

Keeps its argument when the target is a game console, removes it otherwise.

### NKENTSEU_NOT_CONSOLE

```cpp
#define NKENTSEU_NOT_CONSOLE(...)
```

Removes its argument when the target is a game console, keeps it otherwise.

### NKENTSEU_HANDHELD_ONLY

```cpp
#define NKENTSEU_HANDHELD_ONLY(...)
```

Keeps its argument when the target is a handheld console, removes it otherwise.

### NKENTSEU_NOT_HANDHELD

```cpp
#define NKENTSEU_NOT_HANDHELD(...)
```

Removes its argument when the target is a handheld console, keeps it otherwise.

### NKENTSEU_EMBEDDED_ONLY

```cpp
#define NKENTSEU_EMBEDDED_ONLY(...)
```

Keeps its argument when the target is an embedded system, removes it otherwise.

### NKENTSEU_NOT_EMBEDDED

```cpp
#define NKENTSEU_NOT_EMBEDDED(...)
```

Removes its argument when the target is an embedded system, keeps it otherwise.

### NKENTSEU_UNIX_ONLY

```cpp
#define NKENTSEU_UNIX_ONLY(...)
```

Keeps its argument when the target is a POSIX system of the Unix family, removes it otherwise.

### NKENTSEU_NOT_UNIX

```cpp
#define NKENTSEU_NOT_UNIX(...)
```

Removes its argument when the target is a POSIX system of the Unix family, keeps it otherwise.

### NKENTSEU_PLAYSTATION_ONLY

```cpp
#define NKENTSEU_PLAYSTATION_ONLY(...)
```

Keeps its argument when the target is a PlayStation, whatever the model, removes it otherwise.

### NKENTSEU_NOT_PLAYSTATION

```cpp
#define NKENTSEU_NOT_PLAYSTATION(...)
```

Removes its argument when the target is a PlayStation, whatever the model, keeps it otherwise.

### NKENTSEU_XBOX_ONLY

```cpp
#define NKENTSEU_XBOX_ONLY(...)
```

Keeps its argument when the target is an Xbox, whatever the model, removes it otherwise.

### NKENTSEU_NOT_XBOX

```cpp
#define NKENTSEU_NOT_XBOX(...)
```

Removes its argument when the target is an Xbox, whatever the model, keeps it otherwise.

### NKENTSEU_NINTENDO_ONLY

```cpp
#define NKENTSEU_NINTENDO_ONLY(...)
```

Keeps its argument when the target is a Nintendo home console (NES to Switch), removes it otherwise.

### NKENTSEU_NOT_NINTENDO

```cpp
#define NKENTSEU_NOT_NINTENDO(...)
```

Removes its argument when the target is a Nintendo home console (NES to Switch), keeps it otherwise.

