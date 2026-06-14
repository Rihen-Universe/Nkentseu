# AI — Roadmap globale

> Feuille de route du sous-système NKAI. Esprit : **from scratch, bottom-up, un résultat
> observable par phase**. On préfère un système simple qui *apprend* à un système complet qui ne
> tourne pas. Chaque tier repose sur le précédent.

Légende : ⬜ à faire · 🟡 en cours · ✅ fait.

---

## Phase 0 — Mise en place

- ⬜ Intégration Jenga (`.jenga` par module), namespace `nkentseu::ai`.
- ⬜ Vérifier la voie **GPU** : confirmer que NKRHI expose le **compute** (shaders de calcul) utilisable par NKTensor.
- ⬜ Décider le format des tenseurs (types, layout mémoire, alignement via NKMemory).

## Phase 1 — Le calcul (la pierre angulaire)

**Module : NKTensor.**
- ⬜ Tenseurs n-dim sur **CPU** (NKMath/SIMD) : création, indexation, broadcasting, ops élémentaires.
- ⬜ Produit de matrices + ops de réduction.
- ⬜ Backend **GPU** (NKRHI compute) derrière la **même API** ; choix du device à la création.
- 🎯 **Jalon : multiplier deux grandes matrices, CPU puis GPU, et mesurer l'accélération.**

## Phase 2 — L'apprentissage

**Modules : NKAutograd, NKNN, NKOptim.**
- ⬜ NKAutograd : graphe de calcul + rétropropagation (mode inverse).
- ⬜ NKNN : couche dense, activations, fonction de perte.
- ⬜ NKOptim : SGD puis Adam.
- 🎯 **Jalon : entraîner un mini-réseau (ex. XOR) qui converge.**

## Phase 3 — Données, entraînement, inférence

**Modules : NKData, NKTrain, NKInfer.**
- ⬜ NKData : chargeur de jeu de données + batchs (ex. MNIST).
- ⬜ NKTrain : boucle d'entraînement + checkpoints + métriques.
- ⬜ NKInfer : charger un modèle entraîné et l'exécuter.
- 🎯 **Jalon : entraîner puis inférer un vrai petit modèle (classer des chiffres MNIST).**

## Phase 4 — La décision

**Modules : NKRL, NKAgent.**
- ⬜ NKRL : interface d'environnement + Q-learning tabulaire, puis DQN.
- ⬜ NKAgent : agent avec mémoire (passé), perception (présent), politique de décision.
- 🎯 **Jalon « ça vit » : un agent apprend tout seul à résoudre un environnement simple.**

## Phase 5 — La vie et l'émergence

**Modules : NKEvolve, NKCivilization.**
- ⬜ NKEvolve : génomes, sélection, mutation, croisement → population qui évolue.
- ⬜ NKCivilization : monde + temps + plusieurs agents qui interagissent (sur NKECS).
- ⬜ Outils d'observation : enregistrer, rejouer, analyser les trajectoires.
- 🎯 **Jalon « ça émerge » : des comportements de société non scriptés apparaissent.**

## Phase 6 — Génération & incarnation

**Modules : NKGen, NKEmbodied.**
- ⬜ NKGen : un petit modèle génératif (image 2D) ; brancher la génération d'assets au moteur.
- ⬜ NKEmbodied : relier une politique à un corps simulé (puis réel via Kernel/Bare).
- 🎯 **Jalon : générer un asset dans le moteur ; piloter un corps par une IA.**

## Phase 7 — Montée en échelle (plus tard)

- ⬜ **LLM** dans NKInfer : architecture transformer, inférence, fine-tune de petits modèles, quantization.
- ⬜ Grande civilisation : plus d'agents, mémoire/réflexion/planification riches (style *generative agents*), prospective.
- ⬜ Robotique réelle / objets intelligents sur **Kernel/Bare**.
- ⬜ Modèles génératifs 3D / animation.

---

[Architecture](ARCHITECTURE.md) · [Modules](README.md)
