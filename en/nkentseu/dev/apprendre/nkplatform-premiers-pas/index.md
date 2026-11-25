# Your first program with NKPlatform

In five steps, you declare a console program in Jenga, read your machine's byte order, convert an integer to network order, then read an environment variable.


All the code in this tutorial is real: these are two programs from `Docs/exemples/`, compiled and run each time this documentation is built. What you read under "Output" is what they printed.

## Step 1: declare the program

A Nkentseu console program is declared in a `.jenga` file. It says which module it uses: here NKPlatform, plus NKContainers to print text with `NkPrintln`.

```jenga
with project("MyFirstProgram"):
    consoleapp()
    language("C++")
    cppdialect("C++20")
    files(["main.cpp"])
    nkentseudependson(["NKPlatform", "NKContainers", "NKMemory", "NKCore"])
```

That is the essence. The complete file that builds the examples on this page, with the toolchain choice per system, is `Docs/exemples/NkDocExemples.jenga`.

## Step 2: know your machine's byte order

A machine stores the bytes of an integer in one order: smallest first (little-endian, almost every current processor), or the reverse. [[NkIsLittleEndian]] tells you, without measuring anything at run time:

```cpp
// Le sens de la machine est connu dès la compilation : aucun test n'est fait à l'exécution.
const bool petitBoutiste = platform::NkIsLittleEndian();
NkPrintln("Cette machine est {}.", petitBoutiste ? "petit-boutiste" : "gros-boutiste");
```


## Step 3: reverse the bytes

[[NkByteSwap32]] reverses the four bytes of a 32-bit integer. It is the basic tool to read a file written by a machine of the other order.

```cpp
// Inverser les octets d'un entier de 32 bits : 11 22 33 44 devient 44 33 22 11.
const uint32_t valeur = 0x11223344u;
const uint32_t inverse = platform::NkByteSwap32(valeur);
NkPrintln("0x{:08X} inversé donne 0x{:08X}", valeur, inverse);
```


## Step 4: talk to the network

On the network, the order is big-endian. Before sending, [[NkHostToNetwork32]]; when receiving, [[NkNetworkToHost32]]. On a big-endian machine, both functions do nothing: your code stays the same everywhere.

```cpp
// L'ordre du réseau est gros-boutiste. HostToNetwork n'inverse que si la machine est petit-boutiste,
// et NetworkToHost défait exactement ce que HostToNetwork a fait.
const uint32_t envoye = platform::NkHostToNetwork32(valeur);
const uint32_t recu = platform::NkNetworkToHost32(envoye);
NkPrintln("Envoyé sur le réseau : 0x{:08X}, relu : 0x{:08X}", envoye, recu);
```


The complete program, with what it prints:

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


## Step 5: the process environment

A program receives environment variables from whoever starts it. [[GetEnvVar]] reads one; a missing variable gives `nullptr`, not an empty string:

```cpp
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Exemple de la documentation (NKPlatform) : lire une variable d'environnement, et reconnaître une variable absente.
// Compilé et exécuté à chaque construction de la documentation ; sa sortie est comparée à
// variables_environnement.attendu. N'utilise que l'API présente dans la version publiée 0.1.0-beta.11 et
// utilisable hors du module : GetEnvVar (NkSet, NkGet, NkExists et NkUnset y sont déclarées « inline » sans
// définition dans l'en-tête : un programme qui les appelle ne se lie pas ; voir la page du manuel).

#include "NKPlatform/NkEnv.h"
#include "NKContainers/String/NkPrint.h"

int main() {
	using namespace nkentseu;

	// GetEnvVar rend un pointeur vers la valeur, ou nullptr si la variable n'existe pas.
	// PATH existe partout où un programme se lance : on ne montre pas sa valeur, qui dépend de la machine.
	const char *chemin = env::GetEnvVar("PATH");
	NkPrintln("PATH est définie : {}", chemin ? "oui" : "non");

	// Teste TOUJOURS le pointeur avant de le lire : une variable absente ne rend pas une chaîne vide.
	const char *absente = env::GetEnvVar("NK_DOC_VARIABLE_QUI_N_EXISTE_PAS");
	NkPrintln("NK_DOC_VARIABLE_QUI_N_EXISTE_PAS : {}", absente ? absente : "(absente)");

	return (chemin && !absente) ? 0 : 1;
}
```

Sortie :

```text
PATH est définie : oui
NK_DOC_VARIABLE_QUI_N_EXISTE_PAS : (absente)
```


> [!attention] Test the pointer
> Reading a `nullptr` crashes your program. The output above shows both cases: a variable that exists, and one that does not.

## What next

- Read [NKPlatform: know what you run on](@/manuel/foundation/nkplatform) for the detection pitfalls.
- Look up a function in the [NKPlatform reference](@/reference/foundation/nkplatform) (shortcut: `Ctrl` `K`).
