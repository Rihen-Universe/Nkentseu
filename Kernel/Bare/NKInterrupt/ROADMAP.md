# NKInterrupt — Roadmap

> La colonne vertébrale réactive du système (Phase 2). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — exceptions (Phase 2)
- ⬜ Installer la table des vecteurs (IDT x86 / vecteurs ARM).
- ⬜ Point d'entrée commun : sauvegarde/restauration du contexte (via NKArch).
- ⬜ Handler d'exception générique → **panique lisible** sur NKSerial (dump registres).
- 🎯 Une faute CPU produit un message clair au lieu d'un gel.

## Jalon 2 — IRQ matérielles
- ⬜ Initialiser le contrôleur d'interruptions (PIC/APIC sur x86, GIC sur ARM).
- ⬜ Enregistrement de handlers d'IRQ (table de callbacks).
- ⬜ Masquage / démasquage / acquittement d'IRQ.
- ⬜ Premier client : [NKTimerHW](../NKTimerHW/README.md) (tick périodique).

## Jalon 3 — robustesse
- ⬜ Imbrication / priorités d'interruptions.
- ⬜ Compteurs par vecteur (diagnostic).
- ⬜ Distinction contexte IRQ / contexte tâche (pour NKScheduler).

## Plus tard
- ⬜ Routage multi-cœur (IPI, affinité).
- ⬜ MSI/MSI-X si bus PCIe sur la vraie carte.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
