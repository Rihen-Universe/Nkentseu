---
titre: NKPlatform: know what you run on
resume: The lowest module of the engine. At compile time, it tells you which system, processor and compiler you build for; at run time, what the processor can do and what the environment holds.
ordre: 10
cles: platform system processor compiler bytes endianness environment macro detection
---
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

## What it is for

NKPlatform is the foundation of every other layer: it depends on nothing, and almost the whole engine depends on it. It answers three questions.

- **What am I compiling for?** Macros tell you the system (`NKENTSEU_PLATFORM_WINDOWS`, `NKENTSEU_PLATFORM_LINUX`, `NKENTSEU_PLATFORM_ANDROID`...), the architecture ([[NKENTSEU_ARCH_64BIT]]) and the compiler (`NKENTSEU_COMPILER_CLANG`, `NKENTSEU_COMPILER_MSVC`...). The preprocessor sets them: they describe the **target** machine, not the one running the program.
- **How are bytes ordered?** [[NkIsLittleEndian]] tells you the machine's order, [[NkByteSwap32]] reverses the bytes of an integer, [[NkHostToNetwork32]] and [[NkNetworkToHost32]] go to network order and back.
- **What can this machine do, here and now?** [[NkGetCPUFeatures]] reads the processor at run time (SSE2, AVX, NEON, cache line size, core count). [[GetEnvVar]] reads an environment variable of the process.

## When to use it

- Your code must behave differently depending on the system: test an `NKENTSEU_PLATFORM_*` macro, never `_WIN32` or `__linux__` directly. When a platform is added, only one detection changes.
- You write or read a binary format (file, network): fix the format's byte order, and convert with the functions of `NkEndianness.h`.
- You want to pick the fastest path at run time (a vectorised loop, for example): ask [[NkGetCPUFeatures]], not the compiler.
- You read a setting passed through the environment (a path, a test mode): `NkEnv.h`.
- You write a Foundation module, below NKLogger: the `NK_FOUNDATION_LOG_*` macros log, and [[NkFoundationSetLogSink]] sends the messages wherever you want.

To link it, one line in your `.jenga` file:

```jenga
nkentseudependson(["NKPlatform"])
```

`#include "NKPlatform/NKPlatform.h"` brings detection, byte order, export and inlining macros at once. `NkEnv.h` and `NkCPUFeatures.h` are included separately.

## A complete example

{{exemple:Foundation/NKPlatform/ordre_des_octets}}

## Compile time, then run time

Macros answer at compile time:

{{exemple:Foundation/NKPlatform/detection}}

The processor answers at run time:

{{exemple:Foundation/NKPlatform/processeur}}

## Pitfalls

> [!attention] Compile time versus run time
> Detection macros are frozen at compile time. A program compiled for x86_64 and run under emulation on an ARM processor still says x86_64. To know what the processor really supports, use [[NkGetCPUFeatures]], at run time.

> [!attention] A missing variable gives nullptr
> [[GetEnvVar]] returns a pointer to the value, or `nullptr` if the variable does not exist. Always test the pointer before reading it.

{{si-version:0.1}}
> [!attention] Known issue in 0.1.0-beta.11: NkSet, NkGet, NkExists and NkUnset do not link
> These four functions are declared `inline` in `NkEnv.h`, but their body is in `NkEnv.cpp`. A program outside the module that calls them gets the warning "inline function is not defined", then linking fails (seen while building the example on this page). In this version, read the environment with [[GetEnvVar]]. Fixed after this version (see the work-in-progress documentation).
{{fin-si}}

{{si-version:dev}}
> [!attention] An environment variable lives in the process
> [[NkSet]] changes the environment of YOUR process (and of the processes it starts afterwards). It changes nothing for the user's session, and everything is gone when your program stops.

> [!attention] A missing variable and an empty one look alike
> [[NkGet]] returns an empty string in both cases. If the difference matters, ask [[NkExists]] first.

{{exemple:Foundation/NKPlatform/environnement_ecrire}}
{{fin-si}}

> [!attention] X11 macros
> X11 headers define macros with very common names (`None`, `Bool`, `Status`...). If you include X11 before a Nkentseu header that uses these names, the preprocessor replaces them. `NkX11Clean.h` removes them: include it right after X11.

> [!pourquoi] Why a whole layer for this
> The rest of the engine depends on it: if every module detected the system on its own, there would be as many ways to get it wrong as there are modules. One detection, at the bottom of the stack, and everyone reads it.

## Going further

- The tutorial [Your first program with NKPlatform](@/apprendre/nkplatform-premiers-pas) builds and runs two small programs.
- The [NKPlatform reference](@/reference/foundation/nkplatform) lists every function, type and macro, with its signature.
