# NKPMM — Roadmap

> Le module charnière qui « allume » la Foundation sur le métal (Phase 2).
> ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — mémoire physique (Phase 2)
- ⬜ Lire la carte mémoire transmise par NKBoot ; recenser la RAM libre.
- ⬜ Allocateur de pages physiques (bitmap ou liste libre).
- ⬜ Réserver les zones occupées (noyau, framebuffer, firmware).

## Jalon 2 — pagination (MMU)
- ⬜ Construire les tables de pages initiales (via NKArch).
- ⬜ Activer la pagination ; mappage identité du noyau.
- ⬜ Mapper/démapper une plage virtuelle avec droits R/W/X.

## Jalon 3 — pont vers NKMemory
- ⬜ Exposer une **grande région** comme réserve à NKMemory.
- ⬜ Brancher la réserve sur les allocateurs NKMemory.
- 🎯 **Jalon Phase 2** : NKMemory/NKContainers/NKMath tournent sur le métal.

## Jalon 4 — services aux pilotes
- ⬜ Mappage **MMIO** (registres de périphériques) pour NKDriver.
- ⬜ Allocation contiguë / alignée (DMA, framebuffer).

## Plus tard
- ⬜ Espaces d'adressage séparés (si processus utilisateur).
- ⬜ Pagination à la demande, protection fine.
- ⬜ Statistiques mémoire exposées au dashboard.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
