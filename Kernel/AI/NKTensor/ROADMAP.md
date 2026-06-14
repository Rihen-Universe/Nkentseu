# NKTensor — Roadmap

> La pierre angulaire du framework (Phase 1). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — tenseurs CPU (Phase 1)
- ⬜ Structure de tenseur (forme, type, strides, device) + allocation via NKMemory.
- ⬜ Création, indexation, *slicing*, reshape/transpose.
- ⬜ Opérations élémentaires + **broadcasting** (CPU, via NKMath/SIMD).

## Jalon 2 — algèbre linéaire
- ⬜ **Produit de matrices** (matmul) optimisé SIMD.
- ⬜ Réductions (somme, moyenne, max, argmax…).
- 🎯 On multiplie deux grandes matrices sur CPU.

## Jalon 3 — backend GPU
- ⬜ Confirmer la voie **NKRHI compute** (shaders de calcul).
- ⬜ Tenseur GPU + transferts CPU↔GPU.
- ⬜ matmul + ops élémentaires en compute, **même API** que le CPU.
- 🎯 **Jalon Phase 1** : même calcul CPU vs GPU, accélération mesurée.

## Plus tard
- ⬜ Types réduits (fp16/bf16/int8) pour la quantization.
- ⬜ Opérations fusionnées (perf).
- ⬜ Multi-GPU.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
