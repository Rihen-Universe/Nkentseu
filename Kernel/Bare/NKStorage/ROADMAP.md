# NKStorage — Roadmap

> L'accès aux octets persistants (Phase 4). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — lecture de blocs (Phase 4)
- ⬜ Initialiser le contrôleur (virtio-blk sous QEMU pour commencer).
- ⬜ Lire un bloc par adresse (LBA).
- 🎯 On lit le premier secteur du support et on l'affiche (debug).

## Jalon 2 — lecture/écriture complète
- ⬜ Écriture de blocs.
- ⬜ Lecture/écriture de plusieurs blocs contigus.
- ⬜ Gestion d'erreurs + nouvelles tentatives.

## Jalon 3 — performance
- ⬜ Transferts **DMA** (alloc contiguë via NKPMM).
- ⬜ Cache de blocs simple.

## Plus tard
- ⬜ Vrai contrôleur SD/flash de la carte.
- ⬜ Plusieurs supports.
- ⬜ Usure / wear-leveling (flash).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
