# NKTrain — Roadmap

> La boucle qui fait apprendre (Phase 3). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — boucle de base (Phase 3) ✅
- ✅ **`train::TrainEpoch`** (`src/NKTrain/NkTrain.{h,cpp}`) : cycle forward → perte
  (entropie croisée) → backward → `opt.Step()` sur tous les lots d'une époque, puis
  remélange. Générique par **template** sur le forward (callable `NkVar(NkVar)`) et
  l'optimiseur (`Step()`) — sans std::function.
- ✅ Suivi **perte moyenne + exactitude** par époque (`EpochStats`) ; `train::Accuracy`
  évalue un loader sans mise à jour (helper compilé `CountCorrect`, argmax).
- 🎯 ✅ **Un modèle s'entraîne de bout en bout** (`NKTrainTest`, `jenga run`) :
  Dense(2→32)+relu+Dense(32→4), Adam+CE sur 4 amas → **train 100%, TEST 100%**
  (perte 1.58 → 7e-4). Chemin MNIST prêt (`NK_MNIST_DIR` → MLP 784→64→10).

## Jalon 2 — durabilité ✅ (2026-07-26)
- ✅ **Checkpoints** (modèle + optimiseur) : `NkCheckpoint.h/.cpp` — format « NKTC » v1
  (poids + état Adam optionnel {moments m/v, pas} + état de boucle optionnel
  {époque, pas global, meilleure métrique, patience}). GÉNÉRALISE ce qui n'existait
  QUE dans `NkGptTrainer`/`NkGptCore` (format « NKGP » v4, spécifique GPT) : ici
  fonctionne pour N'IMPORTE QUEL modèle NKNN (`NkVector<NkVar>`) + `optim::NkAdam`.
  Preuve réelle (`NKTrainTest`) : sauvegarde/rechargement **octet pour octet** des
  poids, pas Adam repris exact, et **reprise EXACTE** vérifiée en comparant un pas
  Adam identique sur le modèle continué en mémoire vs le modèle rechargé depuis le
  checkpoint -> **écart max = 0**.
- ✅ Métriques (perte + exactitude) + journalisation : `train::EvalMetrics`/`Evaluate`
  (un seul passage, au lieu de `EvalLoss`+`Accuracy` séparés) ; `NkLoggingCallback`
  (NKLogger, jamais printf).
- ✅ Boucle de **validation** : `Evaluate` (générique, template Forward/Loss) +
  intégrée à `train::Fit` (validation forward-only après chaque époque si un
  `valLoader` est fourni).

## Jalon 3 — confort ✅ (2026-07-26)
- ✅ **Callbacks** (`NkCallback.h/.cpp`) : interface `train::NkTrainCallback` (points
  fixes : avant/après entraînement, avant/après époque, avant/après lot) + boucle
  générique **`train::Fit`** qui les appelle. Implémentations réelles :
  - `NkEarlyStopping` : arrête si la métrique surveillée (perte val, ou perte train
    si pas de val) ne s'améliore pas de plus de `minDelta` après `patience` époques.
  - `NkLRSchedulerCallback<Opt>` : pilote `opt.SetLearningRate()` à chaque LOT via
    `NkLRSchedule` (warmup + cosine, déjà prouvé par `SelfTest`).
  - `NkStepDecayCallback<Opt>` (+ `NkStepDecaySchedule`) : décroissance PAR PALIERS
    (alternative au cosine), pilotée par époque.
  - `NkLoggingCallback` : journalise perte/exactitude/perte-val (NKLogger).
  Preuve réelle (`NKTrainTest`, 14/14 OK) : early-stopping logique pure déterministe
  (arrêt à l'époque attendue exacte), early-stopping arrêtant RÉELLEMENT `Fit()`
  avant la fin programmée, LR cosine ET par-paliers pilotant réellement Adam (valeur
  finale == valeur analytique attendue).
- ✅ **Reprise après interruption** : `train::Fit(forward, opt, trainLoader, valLoader,
  fromEpoch, toEpoch, callbacks, &globalStep)` accepte un `fromEpoch` > 1 ; combiné à
  `NkCheckpoint` (état de boucle) et `NkEarlyStopping::RestoreState`, une boucle
  interrompue reprend SANS perdre le compteur de patience ni le pas global (donc sans
  re-warmup du LR). Preuve réelle (`NKTrainTest`) : entraînement 1..5, checkpoint,
  reconstruction complète depuis zéro (« interruption » simulée), reprise 6..12 ->
  exactitude finale 100 %, état repris vérifié exact (époque/pas/meilleure métrique).
- ⬜ Visualisation des courbes (via le moteur) — reste à faire (hors périmètre de
  cette itération, nécessite NKRenderer/UI).

## Plus tard
- ⬜ Entraînement distribué / multi-GPU.
- ⬜ Boucles spécialisées (RL, génératif).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
