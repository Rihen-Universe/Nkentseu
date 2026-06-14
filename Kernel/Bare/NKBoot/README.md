# NKBoot — démarrage bas niveau

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKBoot` est le **tout premier code** qui s'exécute quand la machine démarre. Avant lui, rien
n'est prêt : pas de pile utilisable, pas de mémoire initialisée, pas de C++ exploitable. Son
travail est d'amener le processeur dans un état où le **code C++ du noyau peut tourner**, puis
de lui passer la main.

Concrètement : il installe une **pile**, met à zéro la section **BSS**, copie les données
initialisées si nécessaire, lit la **carte mémoire** fournie par le firmware/bootloader, puis
appelle le point d'entrée du noyau (qui, en bout de chaîne, atteint `NKConsoleRT` / `main`). Il
coordonne aussi le **linker script** (placement des sections, adresse de chargement) et l'en-tête
du **protocole de boot** (multiboot2 sur x86, device tree sur ARM).

## Responsabilités

- Point d'entrée (assembleur) : initialiser le pointeur de pile.
- Mettre à zéro le BSS, copier `.data` si besoin.
- Récupérer la carte mémoire et les infos du firmware → les transmettre à NKPMM.
- Appeler l'entrée C++ du noyau ; au retour, arrêter proprement la machine.

## Place dans la couche

- **Dépend de** : [NKArch](../NKArch/README.md) (mise en place CPU).
- **Précède** : tout le reste — c'est le point de départ de la [séquence de démarrage](../ARCHITECTURE.md#4-séquence-de-démarrage-vision).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
