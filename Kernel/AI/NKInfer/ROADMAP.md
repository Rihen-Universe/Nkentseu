# NKInfer — Roadmap

> Faire tourner un modèle entraîné (Phase 3, LLM en Phase 7).
> ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — inférence de base (Phase 3) 🟡
- ✅ **Persistance des poids** (`src/NKInfer/NkInfer.{h,cpp}`) : `SaveParams`/`LoadParams`
  (format binaire **NKMD** : magic+version+count+[rank,dims,data] par tenseur),
  rechargement en place via `NkVar::SetValue`, validation des formes.
- ✅ **Prédiction** : `Predict` (argmax par ligne des logits [B,C]) + `PredictOne`.
- ✅ **Round-trip prouvé** (`NKInferTest`, `jenga run`) : modèle A entraîné 100% → sauvé
  → modèle B neuf 0% → rechargé **100%** (exact) ; prédictions A == B. **5/5 OK**.
- ⬜ Mode **sans gradient** explicite (NKAutograd) pour l'inférence pure.
- ⬜ Inférence par lots optimisée ; choix CPU/GPU.
- 🎯 **Classer un chiffre MNIST** : pipeline prêt ; brancher `NK_MNIST_DIR` (le forward
  MLP 784→64→10 + Save/Load est déjà câblé dans `NKInferTest`/`NKTrainTest`).

## Jalon 2 — efficacité
- ⬜ **Quantization** (fp16 puis int8).
- ⬜ Réutilisation mémoire / réduction des allocations.

## Jalon 3 — LLM (Phase 7)
- ⬜ Exécuter une architecture **transformer**.
- ⬜ Génération **token par token** + **KV-cache**.
- ⬜ Stratégies d'échantillonnage (greedy, top-k, température).
- ⬜ Fine-tune de **petits** modèles (pas d'entraînement frontière — cf. architecture §1).

## Plus tard
- ⬜ Modèles compilés / fusionnés pour la vitesse.
- ⬜ Inférence embarquée sur **Kernel/Bare** (objets intelligents).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
