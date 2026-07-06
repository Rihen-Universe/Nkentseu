# NKOptim — Roadmap

> La règle de mise à jour des poids (Phase 2). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — SGD (Phase 2) ✅
- ✅ **SGD** (`optim::NkSGD`, `src/NKOptim/NkOptim.{h,cpp}`) : détient les paramètres
  (feuilles NkVar), `Step()` applique `p -= lr·g` **en place** (`NkVar::SetValue`, pour
  que le prochain forward réutilise les poids), `ZeroGrad()`.
- ✅ Application du pas + remise à zéro des gradients.
- 🎯 ✅ **Un réseau s'entraîne avec SGD** (`NKNNTest` : XOR → perte 1.8e-5, 4/4).

## Jalon 2 — optimiseurs modernes 🟡
- ✅ **Momentum** (`p ← p − lr·v`, `v ← μ·v + g` ; état par paramètre).
- ✅ **Adam** (`optim::NkAdam`) : moments m/v par paramètre + correction de biais
  (`m̂=m/(1−β1ᵗ)`, `v̂=v/(1−β2ᵗ)`), `p ← p − lr·m̂/(√v̂+ε)`. Utilise `ops::Sqrt`
  (ajouté à NKTensor). **Prouvé** : classification 3-classes → 100% en ~400 époques.
- ⬜ AdamW (weight decay découplé).
- ⬜ Clipping de gradient.

## Jalon 3 — plannings
- ⬜ Décroissance du taux d'apprentissage (step, cosine).
- ⬜ Warmup.

## Plus tard
- ⬜ Optimiseurs spécialisés (RL, grands modèles).
- ⬜ État d'optimiseur sérialisable (reprise d'entraînement).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
