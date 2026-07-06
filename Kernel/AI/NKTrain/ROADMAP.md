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

## Jalon 2 — durabilité
- ⬜ **Checkpoints** (modèle + optimiseur) : sauver / reprendre.
- ⬜ Métriques (précision…) + journalisation.
- ⬜ Boucle de **validation**.

## Jalon 3 — confort
- ⬜ Callbacks (early-stopping, planning, logs).
- ⬜ Reprise après interruption.
- ⬜ Visualisation des courbes (via le moteur).

## Plus tard
- ⬜ Entraînement distribué / multi-GPU.
- ⬜ Boucles spécialisées (RL, génératif).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
