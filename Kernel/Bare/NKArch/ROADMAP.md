# NKArch — Roadmap

> Module fondateur (Phase 1). Objectif : une **interface CPU unifiée** au-dessus de x86_64
> puis ARM64. ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — primitives minimales (Phase 1)
- ⬜ `halt` / mise en veille (`hlt` x86, `wfi` ARM).
- ⬜ Activer / désactiver les interruptions globales.
- ⬜ Lire/écrire les registres de contrôle de base.
- ⬜ Squelette d'interface commune (x86_64 vs ARM64) + sélection à la compilation.

## Jalon 2 — contexte (pour interruptions & ordonnanceur)
- ⬜ Structure de **contexte** (jeu de registres sauvegardés).
- ⬜ Sauvegarde / restauration de contexte (assembleur).
- ⬜ Bascule de pile (kernel/exception).

## Jalon 3 — MMU bas niveau (pour NKPMM)
- ⬜ Charger la base des tables de pages.
- ⬜ Activer la pagination / la traduction d'adresses.
- ⬜ Invalidation TLB.

## Jalon 4 — concurrence métal
- ⬜ Barrières mémoire (acquire/release/full).
- ⬜ Opérations atomiques (CAS, fetch-add) au niveau matériel.

## Plus tard
- ⬜ Multi-cœur (SMP) : démarrage des cœurs secondaires.
- ⬜ Extensions FPU/SIMD : sauvegarde d'état étendu.
- ⬜ Portage de la 2ᵉ architecture (ARM64 si on a commencé x86_64, ou l'inverse).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
