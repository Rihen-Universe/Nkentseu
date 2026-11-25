# NKPlatform: know what you run on

The lowest module of the engine. At compile time, it tells you which system, processor and compiler you build for; at run time, what the processor can do and what the environment holds.


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

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : l'ordre des octets d'un entier, et son passage à l'ordre du réseau.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à ordre_des_octets.attendu.

#include "NKPlatform/NkEndianness.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// Le sens de la machine est connu dès la compilation : aucun test n'est fait à l'exécution.
	const bool petitBoutiste = platform::NkIsLittleEndian();
	NkPrintln("Cette machine est {}.", petitBoutiste ? "petit-boutiste" : "gros-boutiste");

	// Inverser les octets d'un entier de 32 bits : 11 22 33 44 devient 44 33 22 11.
	const uint32_t valeur = 0x11223344u;
	const uint32_t inverse = platform::NkByteSwap32(valeur);
	NkPrintln("0x{:08X} inversé donne 0x{:08X}", valeur, inverse);

	// L'ordre du réseau est gros-boutiste. HostToNetwork n'inverse que si la machine est petit-boutiste,
	// et NetworkToHost défait exactement ce que HostToNetwork a fait.
	const uint32_t envoye = platform::NkHostToNetwork32(valeur);
	const uint32_t recu = platform::NkNetworkToHost32(envoye);
	NkPrintln("Envoyé sur le réseau : 0x{:08X}, relu : 0x{:08X}", envoye, recu);

	return recu == valeur ? 0 : 1;
}
```

Sortie :

```text
Cette machine est petit-boutiste.
0x11223344 inversé donne 0x44332211
Envoyé sur le réseau : 0x44332211, relu : 0x11223344
```


## Compile time, then run time

Macros answer at compile time:

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : les macros de détection, fixées À LA COMPILATION.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à detection.attendu.
// Il n'affiche que des faits vrais sur toutes les machines où la documentation se construit (Windows et Linux,
// 64 bits, C++20) : le nom exact de la plateforme change d'une machine à l'autre.

#include "NKPlatform/NKPlatform.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// Une macro de détection est DÉFINIE ou non : on la teste avec #if defined(...), jamais par sa valeur.
#if defined(NKENTSEU_ARCH_64BIT)
	const bool bits64 = true;
#else
	const bool bits64 = false;
#endif
	NkPrintln("Architecture 64 bits : {}", bits64 ? "oui" : "non");

#if defined(NKENTSEU_PLATFORM_DESKTOP)
	const bool bureau = true;
#else
	const bool bureau = false;
#endif
	NkPrintln("Système de bureau : {}", bureau ? "oui" : "non");

	// NKENTSEU_PLATFORM_NAME est une chaîne lisible ("Windows", "Linux"...), pour un journal ou un rapport.
	const char *nom = NKENTSEU_PLATFORM_NAME;
	NkPrintln("Nom de la plateforme connu : {}", (nom && nom[0] != 0) ? "oui" : "non");

	NkPrintln("C++20 ou plus récent : {}", NKENTSEU_CPP_VERSION >= 20 ? "oui" : "non");

	return (bits64 && bureau) ? 0 : 1;
}
```

Sortie :

```text
Architecture 64 bits : oui
Système de bureau : oui
Nom de la plateforme connu : oui
C++20 ou plus récent : oui
```


The processor answers at run time:

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : ce que le processeur sait faire, lu À L'EXÉCUTION.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à processeur.attendu.
// Il n'affiche que des faits vrais sur toutes les machines où la documentation se construit (x86-64 et ARM 64) :
// les valeurs exactes (nombre de cœurs, nom du processeur) changent d'une machine à l'autre.

#include "NKPlatform/NkCPUFeatures.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// NkGetCPUFeatures interroge le processeur une seule fois (au premier appel), puis garde le résultat.
	const platform::NkCPUFeatures &cpu = platform::NkGetCPUFeatures();

	const int32_t physiques = platform::NkGetPhysicalCoreCount();
	const int32_t logiques = platform::NkGetLogicalCoreCount();
	NkPrintln("Au moins un cœur physique : {}", physiques >= 1 ? "oui" : "non");
	NkPrintln("Autant ou plus de cœurs logiques que physiques : {}", logiques >= physiques ? "oui" : "non");

	// x86-64 garantit SSE2, ARM 64 bits garantit NEON : l'un des deux répond toujours oui sur nos cibles.
	const bool simd = platform::NkHasSSE2() || platform::NkHasNEON();
	NkPrintln("SSE2 ou NEON disponible : {}", simd ? "oui" : "non");

	// Une ligne de cache fait une puissance de deux octets (64 le plus souvent).
	const int32_t ligne = platform::NkGetCacheLineSize();
	const bool puissanceDeDeux = ligne > 0 && (ligne & (ligne - 1)) == 0;
	NkPrintln("Ligne de cache en puissance de deux : {}", puissanceDeDeux ? "oui" : "non");

	// Un deuxième appel rend le MÊME objet : la détection n'est pas refaite.
	const bool memeObjet = &cpu == &platform::NkGetCPUFeatures();
	NkPrintln("Le même résultat à chaque appel : {}", memeObjet ? "oui" : "non");

	return (physiques >= 1 && simd && puissanceDeDeux && memeObjet) ? 0 : 1;
}
```

Sortie :

```text
Au moins un cœur physique : oui
Autant ou plus de cœurs logiques que physiques : oui
SSE2 ou NEON disponible : oui
Ligne de cache en puissance de deux : oui
Le même résultat à chaque appel : oui
```


## Pitfalls

> [!attention] Compile time versus run time
> Detection macros are frozen at compile time. A program compiled for x86_64 and run under emulation on an ARM processor still says x86_64. To know what the processor really supports, use [[NkGetCPUFeatures]], at run time.

> [!attention] A missing variable gives nullptr
> [[GetEnvVar]] returns a pointer to the value, or `nullptr` if the variable does not exist. Always test the pointer before reading it.

> [!attention] Known issue in 0.1.0-beta.11: NkSet, NkGet, NkExists and NkUnset do not link
> These four functions are declared `inline` in `NkEnv.h`, but their body is in `NkEnv.cpp`. A program outside the module that calls them gets the warning "inline function is not defined", then linking fails (seen while building the example on this page). In this version, read the environment with [[GetEnvVar]]. Fixed after this version (see the work-in-progress documentation).


> [!attention] X11 macros
> X11 headers define macros with very common names (`None`, `Bool`, `Status`...). If you include X11 before a Nkentseu header that uses these names, the preprocessor replaces them. `NkX11Clean.h` removes them: include it right after X11.

> [!pourquoi] Why a whole layer for this
> The rest of the engine depends on it: if every module detected the system on its own, there would be as many ways to get it wrong as there are modules. One detection, at the bottom of the stack, and everyone reads it.

## Going further

- The tutorial [Your first program with NKPlatform](@/apprendre/nkplatform-premiers-pas) builds and runs two small programs.
- The [NKPlatform reference](@/reference/foundation/nkplatform) lists every function, type and macro, with its signature.
