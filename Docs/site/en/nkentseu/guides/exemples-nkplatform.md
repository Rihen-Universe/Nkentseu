---
titre: NKPlatform examples
resume: Complete programs, compiled and run each time the documentation is built, with what they print.
ordre: 10
cles: examples complete programs bytes network environment
---
<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

Each example is a file in `Docs/exemples/Foundation/NKPlatform/`. Jenga builds it as a console program, then `jenga docs exemples` runs it and compares its output with what is expected. An example that no longer compiles, or no longer prints the same thing, blocks the site build.

## Know what the program was compiled for

{{exemple:Foundation/NKPlatform/detection}}

## Query the processor at run time

{{exemple:Foundation/NKPlatform/processeur}}

## Redirect the foundation log

{{exemple:Foundation/NKPlatform/journal_fondation}}

## Read and convert byte order

{{exemple:Foundation/NKPlatform/ordre_des_octets}}

## Read an environment variable, recognise a missing one

{{exemple:Foundation/NKPlatform/variables_environnement}}

{{si-version:dev}}
## Write, read and remove a variable (work in progress)

{{exemple:Foundation/NKPlatform/environnement_ecrire}}
{{fin-si}}

{{si-version:dev}}
## The readable processor summary (work in progress)

{{exemple:Foundation/NKPlatform/processeur_resume}}
{{fin-si}}
