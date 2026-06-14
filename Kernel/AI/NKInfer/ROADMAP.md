# NKInfer — Roadmap

> Faire tourner un modèle entraîné (Phase 3, LLM en Phase 7).
> ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — inférence de base (Phase 3)
- ⬜ Charger un modèle entraîné (poids depuis NKTrain).
- ⬜ Exécution **forward** sans gradient (mode inférence de NKAutograd).
- ⬜ Inférence par lots ; choix CPU/GPU.
- 🎯 Classer un chiffre MNIST avec le modèle entraîné.

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
