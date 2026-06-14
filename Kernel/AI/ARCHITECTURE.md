# AI — Architecture du sous-système d'intelligence artificielle (NKAI)

> ⚠️ **Statut : squelette documentaire.** Cette couche ne contient pour l'instant **aucun
> code** — uniquement l'architecture, et un `README` + une `ROADMAP` par module. On la
> construit **from scratch, pour apprendre en grandissant avec**, comme [Kernel/Bare](../Bare/ARCHITECTURE.md).

---

## 1. Pourquoi cette couche existe

On veut, dans Nkentseu, pouvoir **créer des intelligences** : des modèles de machine learning,
deep learning, apprentissage par renforcement (Q-learning, deep RL), des LLM, des objets
intelligents et de la robotique — et, surtout, des **agents** que l'on peut assembler en une
**civilisation virtuelle** qu'on regarde **évoluer**, où chaque être a un passé, un présent, un
futur, et prend ses **propres décisions**, comme s'il n'était pas programmé.

`NKAI` est la couche qui rend ça possible. Elle fournit, du plus bas au plus haut : le **calcul**
(tenseurs sur CPU et GPU), l'**apprentissage** (réseaux de neurones, différentiation automatique,
optimiseurs), la **décision** (RL, agents cognitifs), la **vie et l'émergence** (évolution,
simulation de société), et la **génération** (images 2D, formes 3D, animation).

Comme tout le reste de Nkentseu, on l'écrit **from scratch**. L'objectif n'est pas de battre
PyTorch ou GPT — c'est de **comprendre** l'IA en la construisant, et d'avoir un système **intégré
au moteur** : les agents vivent dans nos mondes 3D, apprennent avec notre RHI, et peuvent un jour
tourner sur notre propre console ([Kernel/Bare](../Bare/ARCHITECTURE.md)) ou dans des objets réels.

### Trois honnêtetés posées d'emblée

- **LLM** : on peut implémenter l'architecture (transformer), faire de l'**inférence** et du
  **fine-tuning de petits modèles** — c'est faisable et formateur. **Entraîner un LLM frontière
  from-scratch** (données massives, milliers d'heures GPU) n'est **pas** une cible solo. NKAI
  *supporte* les LLM ; il ne prétend pas en entraîner un comme OpenAI.
- **« Libre arbitre / comme non programmé »** : techniquement, un agent simulé est toujours
  programmé. Ce qu'on vise, c'est l'**émergence** : un comportement qui **naît** de politiques
  apprises + d'un état interne + d'interactions + d'aléatoire, et non de règles `if/else`
  scriptées. Ça *donne* l'autonomie, sans la prétendre littérale. C'est le bon cadrage, et c'est
  déjà profond.
- **« Prédire le futur »** : une simulation calibrée est un **bac à sable de scénarios
  « what-if »** (comme l'épidémiologie ou l'économie), **pas un oracle**. Le chaos et l'erreur de
  modèle empêchent une prédiction exacte ; on **explore des trajectoires plausibles** et des
  **motifs émergents**.

---

## 2. Place dans la pile Nkentseu

```
┌──────────────────────────────────────────────────────────────┐
│  Application   civilisation virtuelle, jeux à PNJ vivants,    │
│                outils de design assistés par IA, robots       │
├──────────────────────────────────────────────────────────────┤
│  AI (CETTE COUCHE)  le sous-système NKAI, en 3 tiers :        │
│    Génération/Sim  NKCivilization · NKGen                     │
│    Cognition       NKRL · NKEvolve · NKAgent · NKEmbodied     │
│    Infrastructure  NKTensor · NKAutograd · NKNN · NKOptim ·   │
│                    NKData · NKTrain · NKInfer                 │
├──────────────────────────────────────────────────────────────┤
│  Runtime       NKRHI (compute GPU) · NKCanvas · NKECS · NKAudio│
│                (NKAI s'appuie sur NKRHI pour le calcul massif) │
├──────────────────────────────────────────────────────────────┤
│  Foundation    NKMemory · NKContainers · NKMath (+ SIMD)      │
│                (NKAI s'appuie sur NKMath/SIMD pour le CPU)     │
└──────────────────────────────────────────────────────────────┘
```

- **Dépend de** : `Foundation` (NKMath/SIMD pour le CPU, NKMemory, NKContainers) et `Runtime`
  (**NKRHI compute** pour le GPU, NKECS pour les agents-entités, NKCanvas/NKAudio pour visualiser).
- **Emplacement** : `Kernel/AI` — une **couche bas niveau réutilisable, façon `Foundation`** :
  une boîte à outils dont tout le reste (jeux, console, robots, outils) peut se servir, y compris
  sur la console pour la robotique. Le tier haut (Sim/Génération) pourra plus tard migrer vers
  `Engine/` ou `Applications/` s'il devient surtout applicatif — c'est volontairement modulaire.

---

## 3. La pierre angulaire : CPU **et** GPU derrière une interface unique

Tout, en IA, se ramène à des **tenseurs** (tableaux n-dimensionnels) et à des opérations
**massivement parallèles** dessus (produits de matrices, convolutions, attention). C'est
exactement le découpage que Nkentseu sait déjà faire pour le rendu : **une interface, deux
backends**.

- **GPU = NKRHI compute** → pour la **rapidité** et le **calcul massif en parallèle**
  (entraînement, gros modèles, batchs). C'est la voie par défaut quand le matériel le permet.
- **CPU = NKMath + SIMD** → repli portable, petits modèles, debug, plateformes sans GPU.

`NKTensor` expose **la même API** quel que soit le backend ; on choisit le device (`CPU`/`GPU`)
à la création, et tout le reste de NKAI (NN, optimiseurs, RL…) est écrit **une seule fois**,
indépendamment du matériel. C'est le même principe d'abstraction que le RHI — un terrain connu.

---

## 4. Les modules (3 tiers)

### Tier 1 — Infrastructure (le framework ML from-scratch)

| Module | Rôle | Dépend de |
|--------|------|-----------|
| **NKTensor** | Tableaux n-dim + opérations. Backends **CPU SIMD** et **GPU NKRHI compute**. La brique de base. | NKMath, NKRHI |
| **NKAutograd** | Différentiation automatique (mode inverse) : le moteur du *gradient*, donc de l'apprentissage. | NKTensor |
| **NKNN** | Couches de réseaux de neurones (dense, convolution, normalisation, **attention/transformer**), activations, fonctions de perte. | NKTensor, NKAutograd |
| **NKOptim** | Optimiseurs (SGD, Adam…) et plannings de taux d'apprentissage. | NKAutograd |
| **NKData** | Jeux de données, chargeurs, **tokenizers** (texte), augmentation. | NKTensor |
| **NKTrain** | Boucle d'entraînement, points de reprise (checkpoints), métriques. | NKNN, NKOptim, NKData |
| **NKInfer** | Exécution en **inférence** : charger un modèle, le faire tourner. **LLM**, quantization. | NKNN, NKTensor |

### Tier 2 — Cognition & apprentissage

| Module | Rôle | Dépend de |
|--------|------|-----------|
| **NKRL** | Apprentissage par **renforcement** : environnements, agents, Q-learning, DQN, policy-gradient, PPO. | NKNN, NKOptim |
| **NKEvolve** | **Vie artificielle** / évolution : génomes, sélection, mutation, croisement → populations émergentes. | NKTensor |
| **NKAgent** | Agent **cognitif** : mémoire (passé), perception (présent), buts & planification (futur), politique de décision. | NKInfer, NKRL |
| **NKEmbodied** | **Incarnation** : relier une politique/agent à un corps — robot, objet intelligent — réel ou simulé (tourne aussi sur [Kernel/Bare](../Bare/README.md)). | NKAgent |

### Tier 3 — Simulation & génération

| Module | Rôle | Dépend de |
|--------|------|-----------|
| **NKCivilization** | La **simulation de civilisation virtuelle** : monde, temps, société, économie, interactions, émergence + outils d'**analyse et de prospective**. | NKAgent, NKEvolve, NKECS |
| **NKGen** | Modèles **génératifs** : images 2D (diffusion), formes 3D, animation/motion → générer des assets dans le moteur. | NKNN, NKInfer |

Détail de chaque module dans son dossier (`<Module>/README.md` + `<Module>/ROADMAP.md`).

---

## 5. Ordre de construction (bottom-up)

On bâtit **du bas vers le haut** : chaque tier repose sur le précédent, et chaque étape produit un
**résultat observable**.

```
1. NKTensor                      → multiplier deux matrices, CPU puis GPU, et mesurer l'accélération
2. NKAutograd + NKNN + NKOptim   → entraîner un mini-réseau (ex. XOR) qui converge
3. NKData + NKTrain + NKInfer    → entraîner puis inférer un vrai petit modèle (ex. MNIST)
4. NKRL + NKAgent                → un agent apprend à résoudre un environnement simple
5. NKEvolve + NKCivilization     → une petite population évolue ; une micro-société tourne
6. NKGen + NKEmbodied            → générer une image/un asset ; piloter un corps simulé
7. (plus tard) LLM dans NKInfer, montée en échelle, grande civilisation, robotique sur Kernel/Bare
```

Le premier jalon « ça vit » : **étape 4** — voir un agent **apprendre** par lui-même. Le jalon
« ça émerge » : **étape 5** — voir des comportements de société non scriptés apparaître.

---

## 6. Synergie avec le reste de Nkentseu

- **NKECS = les agents** : chaque être de la civilisation est une entité ECS ; NKCivilization
  orchestre, NKAgent décide.
- **Le moteur = leur monde** : NKCanvas/NKRenderer pour **voir** la civilisation évoluer, NKAudio
  pour l'entendre, NKMath pour l'espace et le temps.
- **NKRHI = le muscle** : le même GPU qui rend les images entraîne les modèles (compute).
- **Kernel/Bare = l'incarnation** : faire tourner une IA embarquée sur la console ou un objet
  intelligent, sans OS hôte.

La **civilisation virtuelle** est la *killer app* qui relie tout : tenseurs + apprentissage +
agents + monde 3D + génération, dans un seul système vivant.

---

## 7. Philosophie et conventions

- **From scratch, incrémental.** On préfère un système simple qui *apprend* à un système complet
  qui ne tourne pas. Un résultat observable par étape.
- **From-scratch ≠ SOTA.** On écrit le cœur pour comprendre et pour l'intégration moteur. On
  connaît la frontière : « pédagogique/fonctionnel » (notre cible) vs « concurrencer PyTorch/GPT »
  (hors périmètre solo).
- **CPU et GPU, une seule API** (cf. §3). Le code haut niveau ignore le matériel.
- **Conventions Nkentseu.** Préfixe `NK`, `NkPascalCase`, namespace prévu `nkentseu::ai`. Chaque
  module : `src/`, `README.md`, `ROADMAP.md`.

---

[Modules](README.md) · [Roadmap globale](ROADMAP.md)
