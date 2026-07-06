# NKData — Roadmap

> Ce qui nourrit l'apprentissage (Phase 3). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — jeu de données + lots (Phase 3) ✅
- ✅ **`data::NkDataset`** (`src/NKData/NkData.{h,cpp}`) : features X [N,D] (stockage
  partagé) + étiquettes [N] + nb de classes ; `Size/FeatureDim/NumClasses`.
- ✅ **`data::NkDataLoader`** : mélange **Fisher-Yates** (LCG déterministe, `Shuffle()`
  par époque) + regroupement en **lots** → `NkBatch{ inputs[B,D], targets[B,C] one-hot,
  labels[B] }`. Dernier lot partiel géré.
- ✅ **Chargeur MNIST** `data::LoadMnist` (format **IDX** big-endian) → [N,784]
  normalisé [0,1] + labels 0..9. Générateur synthétique `data::MakeBlobs` (amas 2D).
- 🎯 ✅ **On itère sur des lots prêts pour l'entraînement** (`NKDataTest`, `jenga run`)
  → **7/7 OK** : couverture complète, histogramme préservé, formes `[B,D]/[B,C]`,
  one-hot correct, shuffle effectif. (MNIST : brancher `NK_MNIST_DIR` vers les fichiers
  IDX décompressés pour l'exercer sur données réelles.)

## Jalon 2 — texte
- ⬜ **Tokenizer** (texte → identifiants) + détokenizer.
- ⬜ Vocabulaire, séquences, padding.

## Jalon 3 — robustesse
- ⬜ **Augmentation** de données (images, texte).
- ⬜ Découpage entraînement / validation / test.
- ⬜ Chargement en flux (gros jeux).

## Plus tard
- ⬜ Observations d'agents (civilisation) comme données.
- ⬜ Préchargement asynchrone (via NKScheduler / threads).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
