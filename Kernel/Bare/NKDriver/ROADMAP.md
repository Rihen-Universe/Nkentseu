# NKDriver — Roadmap

> Le cadre commun des pilotes (Phase 3). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — modèle minimal (Phase 3)
- ⬜ Interface de pilote (probe / init / start / stop).
- ⬜ Table d'enregistrement des pilotes.
- ⬜ Attribution de ressources : MMIO (NKPMM) + IRQ (NKInterrupt).
- 🎯 Un premier pilote (NKDisplay) s'enregistre et s'initialise via le framework.

## Jalon 2 — découverte
- ⬜ Énumération des périphériques (ports connus sur QEMU).
- ⬜ Association périphérique ↔ pilote (matching).
- ⬜ Arbre des périphériques + cycle de vie.

## Jalon 3 — robustesse
- ⬜ Gestion d'erreurs / périphérique absent.
- ⬜ Ordre d'initialisation et dépendances entre pilotes.
- ⬜ Journalisation des pilotes (via NKSerial/NKLogger).

## Plus tard
- ⬜ Bus PCIe / device tree sur la vraie carte.
- ⬜ Chargement/déchargement à chaud.
- ⬜ Gestion d'énergie par périphérique.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
