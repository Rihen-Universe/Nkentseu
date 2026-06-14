# NKPMM — gestion de la mémoire physique + MMU

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKPMM` (*Physical Memory Manager*) prend la **RAM brute** de la machine et la rend utilisable.
Au démarrage, la mémoire est un grand espace non organisé décrit par la carte mémoire que NKBoot
a récupérée. NKPMM la **cartographie**, met en place la **pagination** (la MMU, via NKArch), et
distribue des pages physiques à qui en demande.

C'est le module **charnière** avec le reste de Nkentseu : une fois la mémoire en place, NKPMM
expose une **grande zone** qui sert de réserve à **NKMemory**. À partir de cet instant précis,
toute la **Foundation** (NKMemory, NKContainers, NKMath, NKCore) fonctionne **exactement comme
sur un OS classique** — c'est le moment où le moteur « prend vie » sur le métal. NKMemory garde
son rôle (allocateurs, smart pointers, suivi) ; NKPMM se contente de lui fournir le terrain.

## Responsabilités

- Lire la carte mémoire (de NKBoot) ; recenser la RAM utilisable.
- Allocateur de **pages physiques** (frame allocator).
- Pagination via la MMU (NKArch) : mappage virtuel↔physique, droits (R/W/X).
- Exposer une (ou des) **régions** à NKMemory comme réserve d'allocation.
- Régions mémoire des périphériques (MMIO) pour les pilotes.

## Place dans la couche

- **Dépend de** : [NKArch](../NKArch/README.md) (MMU), [NKBoot](../NKBoot/README.md) (carte mémoire).
- **Alimente** : `NKMemory` (Foundation) — le pont vers tout le moteur. Utilisé aussi par
  [NKDisplay](../NKDisplay/README.md) (framebuffer) et les pilotes (MMIO).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
