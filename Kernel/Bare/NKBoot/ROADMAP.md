# NKBoot — Roadmap

> Le point d'entrée de la machine (Phase 1). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — entrée minimale (Phase 1)
- ⬜ Linker script : sections, point d'entrée, adresse de chargement.
- ⬜ Stub d'entrée (assembleur) : installer le pointeur de pile.
- ⬜ Mettre à zéro le BSS ; copier `.data` si nécessaire.
- ⬜ Appeler l'entrée C++ du noyau ; au retour → `halt` (via NKArch).
- 🎯 On atteint une fonction C++ depuis le métal.

## Jalon 2 — protocole de boot
- ⬜ En-tête multiboot2 (x86) / lecture device tree (ARM).
- ⬜ Récupérer la **carte mémoire** + infos firmware.
- ⬜ Transmettre la carte mémoire à [NKPMM](../NKPMM/README.md).

## Jalon 3 — robustesse
- ⬜ Constructeurs globaux C++ (`.init_array`) appelés.
- ⬜ Garde-fous : pile de secours, message si panique très précoce (via NKSerial).
- ⬜ Bannière de boot (version, build, architecture).

## Plus tard
- ⬜ Démarrage des cœurs secondaires (avec NKArch SMP).
- ⬜ Adaptation à l'amorçage de la vraie carte (vs QEMU).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
