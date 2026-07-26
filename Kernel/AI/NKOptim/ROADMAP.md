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
- ✅ **AdamW (weight decay découplé)** — vérifié dans le code (audit 2026-07-26) :
  `NkAdam(..., float weightDecay = 0.0f)` (`NkOptim.h`) — commentaire explicite
  « `weightDecay` > 0 -> AdamW » ; `Step()` (`NkOptim.cpp`) applique
  `p ← p − lr·(m̂/(√v̂+ε) + wd·p)` sur le chemin CPU **et** passe `wd` au kernel GPU
  fusé (`NkGpuAdamStep(..., mWd)`). Correspond à l'item « 8. AdamW (2026-07-06) »
  de `Kernel/AI/ROADMAP.md`.
- ⬜ **Clipping de gradient** — toujours absent : recherche de `clip`/`Clip` dans
  `NKOptim/src/` (0 résultat) et dans tout `Kernel/AI/` (seules occurrences :
  `NKRL/NkPPO.*` = clipping de l'objectif PPO, et `NKEmbodied` = limites
  d'actionneur — aucun rapport avec le clipping de norme/valeur de gradient d'un
  optimiseur). `Kernel/AI/ROADMAP.md` ne prétend d'ailleurs pas non plus que ce
  point est livré : pas de désaccord avec la roadmap globale ici, l'item reste
  légitimement ⬜.

## Jalon 3 — plannings
- ✅ **Décroissance du taux d'apprentissage (step, cosine) + warmup** — la
  *formule* n'est **pas** dans `NKOptim` (ni `NkAdam` ni `NkSGD` n'ont de logique
  de schedule interne), mais la fonctionnalité est réellement livrée **ailleurs**
  dans la pile et pilote ces optimiseurs depuis l'extérieur via
  `NkAdam::SetLearningRate()`/`NkSGD::SetLearningRate()` (les deux seuls points
  d'entrée exposés ici, bien présents dans `NkOptim.h`) :
  - `Kernel/AI/NKTrain/src/NKTrain/NkTrain.h` : `NkLRSchedule` (warmup linéaire →
    cosine → plancher), `NkLRSchedulerCallback`/`NkStepDecayCallback` (callbacks
    `train::Fit`).
  - `Kernel/AI/NKGpt` (`NkGptTrainer`) : `NK_GPT_LR`/`NK_GPT_WARMUP`, schedule
    repris exactement à la reprise de checkpoint (pas de re-warmup).
  - Correspond à `Kernel/AI/ROADMAP.md` Phase 3 « Option A.1 livrée (2026-07-12) »
    et à l'item « LR schedule warmup + cosine (2026-07-06) » de la section GPT.
  - **Nuance à conserver** : si on juge cet item au périmètre STRICT de la classe
    `NkOptim`/`NkAdam` elle-même (sans schedule interne), il resterait ⬜. On le
    marque ✅ ici parce que le comportement demandé (LR qui varie dans le temps,
    warmup+cosine) existe et fonctionne réellement dans le dépôt, au prix d'être
    porté par `NKTrain`/`NKGpt` plutôt que par `NKOptim`.

## Plus tard
- ⬜ Optimiseurs spécialisés (RL, grands modèles).
- ✅ **État d'optimiseur sérialisable (reprise d'entraînement)** — vérifié dans le
  code (audit 2026-07-26) : `NkAdam` expose `StepCount()/SetStepCount()` et
  `FirstMoments()/SecondMoments()/SetMoments()` (`NkOptim.h`, implémentés dans
  `NkOptim.cpp`), l'API exacte utilisée pour sérialiser/restaurer l'état Adam.
  Consommateurs réels : `Kernel/AI/NKTrain/src/NKTrain/NkCheckpoint.{h,cpp}`
  (format « NKTC » v1, généralisé à **n'importe quel** modèle NKNN + `NkAdam`,
  livré 2026-07-26 par `Kernel/AI/ROADMAP.md` Phase 3) et `NKGpt/NkGptCore`
  (format « NKGP » v4, spécifique GPT, checkpoint+reprise avec état Adam complet
  et LR schedule repris sans warmup). Comme pour l'item précédent, la
  *sérialisation elle-même* (lecture/écriture fichier) vit dans `NKTrain`/`NKGpt`,
  mais `NKOptim` fournit bien les accesseurs d'état qui la rendent possible — la
  reprise d'entraînement décrite par cet item est réellement livrée.

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
