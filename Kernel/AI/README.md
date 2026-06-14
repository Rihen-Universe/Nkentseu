# AI — sous-système d'intelligence artificielle (NKAI)

> ⚠️ **Squelette documentaire — aucun code pour l'instant.** Chaque module ne contient que son
> `README.md` et sa `ROADMAP.md`. On construit cette couche **from scratch, pour apprendre en
> grandissant avec**, comme [Kernel/Bare](../Bare/README.md).

`NKAI` permet de **créer des intelligences** dans Nkentseu : machine learning, deep learning,
apprentissage par renforcement (Q-learning, deep RL), LLM, objets intelligents et robotique — et
surtout des **agents** assemblés en une **civilisation virtuelle** qu'on regarde **évoluer**, où
chaque être a un passé, un présent, un futur, et prend ses propres décisions.

Couche **bas niveau réutilisable, façon `Foundation`** : du **calcul** (tenseurs CPU/GPU) jusqu'à
la **simulation** et la **génération**. Tout est écrit from-scratch, intégré au moteur (les agents
vivent dans nos mondes 3D, apprennent via NKRHI, peuvent tourner sur notre console).

Pour la vision, le découpage en tiers, le principe CPU/GPU et l'ordre de construction : **lire
d'abord [ARCHITECTURE.md](ARCHITECTURE.md)**. La feuille de route : [ROADMAP.md](ROADMAP.md).

> **À garder en tête** (cf. architecture §1) : on *supporte* les LLM (inférence + fine-tune de
> petits modèles), on n'en *entraîne* pas un comme OpenAI ; le « libre arbitre » des agents est de
> l'**émergence**, pas du libre arbitre littéral ; « prédire le futur » = bac à sable de scénarios,
> pas un oracle.

---

## Les modules

### Tier 1 — Infrastructure (le framework ML)

| Module | Rôle | Phase |
|--------|------|-------|
| [NKTensor](NKTensor/README.md) | Tableaux n-dim + opérations. Backends **CPU SIMD** et **GPU NKRHI compute**. La brique de base. | 1 |
| [NKAutograd](NKAutograd/README.md) | Différentiation automatique (mode inverse) : le moteur du gradient. | 2 |
| [NKNN](NKNN/README.md) | Couches de réseaux (dense, conv, norm, attention/transformer), activations, pertes. | 2 |
| [NKOptim](NKOptim/README.md) | Optimiseurs (SGD, Adam…) et plannings. | 2 |
| [NKData](NKData/README.md) | Jeux de données, chargeurs, tokenizers, augmentation. | 3 |
| [NKTrain](NKTrain/README.md) | Boucle d'entraînement, checkpoints, métriques. | 3 |
| [NKInfer](NKInfer/README.md) | Inférence : charger et exécuter un modèle. LLM, quantization. | 3 |

### Tier 2 — Cognition & apprentissage

| Module | Rôle | Phase |
|--------|------|-------|
| [NKRL](NKRL/README.md) | Renforcement : environnements, agents, Q-learning, DQN, PPO. | 4 |
| [NKEvolve](NKEvolve/README.md) | Vie artificielle / évolution : génomes, sélection, mutation. | 5 |
| [NKAgent](NKAgent/README.md) | Agent cognitif : mémoire, perception, planification, décision. | 4 |
| [NKEmbodied](NKEmbodied/README.md) | Incarnation : relier une politique à un corps (robot, objet) réel ou simulé. | 6 |

### Tier 3 — Simulation & génération

| Module | Rôle | Phase |
|--------|------|-------|
| [NKCivilization](NKCivilization/README.md) | La simulation de civilisation virtuelle : société, temps, émergence, prospective. | 5 |
| [NKGen](NKGen/README.md) | Génératif : images 2D (diffusion), formes 3D, animation. | 6 |

Les phases renvoient à la [ROADMAP globale](ROADMAP.md). On bâtit **bottom-up** : chaque tier
repose sur le précédent.

---

## Conventions

- Préfixe `NK`, `NkPascalCase`, namespace prévu `nkentseu::ai`.
- Chaque module, une fois démarré : `src/`, `README.md`, `ROADMAP.md` (+ `.jenga` à terme).
- **CPU et GPU derrière une interface unique** (`NKTensor`) : le code haut niveau ignore le matériel.
- **From-scratch, pour apprendre** — pas pour battre PyTorch/GPT.

[Architecture complète](ARCHITECTURE.md) · [Roadmap globale](ROADMAP.md)
