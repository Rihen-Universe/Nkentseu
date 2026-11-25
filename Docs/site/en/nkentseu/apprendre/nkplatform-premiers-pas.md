---
titre: Your first program with NKPlatform
resume: In five steps, you declare a console program in Jenga, read your machine's byte order, convert an integer to network order, then read an environment variable.
ordre: 10
cles: tutorial first steps jenga console bytes environment
---
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

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

{{extrait:Foundation/NKPlatform/ordre_des_octets#sens}}

## Step 3: reverse the bytes

[[NkByteSwap32]] reverses the four bytes of a 32-bit integer. It is the basic tool to read a file written by a machine of the other order.

{{extrait:Foundation/NKPlatform/ordre_des_octets#echange}}

## Step 4: talk to the network

On the network, the order is big-endian. Before sending, [[NkHostToNetwork32]]; when receiving, [[NkNetworkToHost32]]. On a big-endian machine, both functions do nothing: your code stays the same everywhere.

{{extrait:Foundation/NKPlatform/ordre_des_octets#reseau}}

The complete program, with what it prints:

{{exemple:Foundation/NKPlatform/ordre_des_octets}}

## Step 5: the process environment

A program receives environment variables from whoever starts it. [[GetEnvVar]] reads one; a missing variable gives `nullptr`, not an empty string:

{{exemple:Foundation/NKPlatform/variables_environnement}}

> [!attention] Test the pointer
> Reading a `nullptr` crashes your program. The output above shows both cases: a variable that exists, and one that does not.

## What next

- Read [NKPlatform: know what you run on](@/manuel/foundation/nkplatform) for the detection pitfalls.
- Look up a function in the [NKPlatform reference](@/reference/foundation/nkplatform) (shortcut: `Ctrl` `K`).
