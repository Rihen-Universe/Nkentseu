# NKRL — Roadmap

> Apprendre par l'expérience (Phase 4). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — Q-learning tabulaire (Phase 4) ✅
- ✅ **Interface d'environnement** `rl::NkEnvironment` (`src/NKRL/NkEnv.h`) : Reset,
  Step(action)->(état suivant, récompense, terminal), NumStates/NumActions.
- ✅ **Environnement jouet** `rl::NkGridWorld` (grille N×N + trous + but + coût par pas).
- ✅ **Q-learning tabulaire** `rl::NkQLearning` : mise à jour TD
  `Q(s,a) += α(r + γ·max Q(s',·) − Q(s,a))` + exploration **ε-greedy** (RNG LCG).
  Helper `rl::RunEpisode` (apprentissage ou évaluation gloutonne).
- 🎯 ✅ **Jalon « ça vit »** (`NKRLTest`, `jenga run`) : l'agent part de **7.9%** de
  réussite et **apprend seul** à traverser la grille → **100% (200/200)** en évaluation
  gloutonne ; politique optimale contournant les trous affichée.

## Jalon 2 — deep RL ✅ (DQN de base)
- ✅ **Replay buffer** `rl::NkReplayBuffer` (`src/NKRL/NkReplayBuffer.h/.cpp`) : buffer
  CIRCULAIRE à capacité fixe (une seule allocation `NkVector::Resize` à la
  construction, jamais de réallocation ensuite), oubli **FIFO** (écrase le plus
  ancien slot quand plein), échantillonnage **uniforme avec remise** (RNG LCG).
- ✅ **DQN** `rl::NkDQN` (`src/NKRL/NkDQN.h/.cpp`), construit au-dessus de NKNN
  (`nn::NkDense`) + NKAutograd (`NkVar`) + NKOptim (`optim::NkAdam`) :
  - Réseau Q **en ligne** (Dense→Relu→Dense, taille cachée configurable) +
    réseau **CIBLE** de même architecture, synchronisé **durement** (copie
    profonde des poids) tous les `targetSyncInterval` pas de gradient — pas de
    gradient ne traverse jamais la cible.
  - Cible de Bellman `y = r + γ·max_a' Qcible(s',a')·(1−terminé)` calculée à
    partir du réseau cible (valeurs brutes, hors graphe autograd).
  - Sélection différentiable de `Q(s,a)` pour l'action JOUÉE : NKAutograd
    n'expose pas de « gather » par indices, donc on masque `Q(s,·)` par un
    one-hot de l'action (Mul élémentaire) puis on réduit la ligne par un
    `Matmul` avec un vecteur colonne de 1 ([numActions,1]) → [batch,1] ;
    `Matmul` étant différentiable, le gradient ne remonte que dans l'action
    jouée — effet d'un gather sans qu'il existe en tant que tel dans NKAutograd.
  - Politique **ε-greedy** à décroissance pilotée par l'appelant (`SetEpsilon`,
    même convention que `rl::NkQLearning`).
  - Perte **MSE** + **Adam** (NKOptim) sur des mini-lots échantillonnés du
    replay buffer. Helper `rl::RunDQNEpisode` (apprentissage ou évaluation
    gloutonne), symétrique de `rl::RunEpisode`.
  - Encodage d'état : **one-hot** `[1,numStates]`, générique (fonctionne pour
    tout `rl::NkEnvironment` discret sans encodeur dédié) — limite documentée
    ci-dessous.
- 🎯 ✅ **Preuve réelle** (`NKRLTest`, Test 2, `jenga run`) : DQN entraîné sur
  `rl::NkKeyDoorGridWorld` (grille 5×5 clé-puis-porte, chemin optimal 8 pas,
  sans trous), 600 épisodes d'entraînement (ε : 1.0→0.05) + 100 épisodes
  d'évaluation gloutonne, moyenné sur **5 graines** (11/22/33/44/55) :
  politique **ALÉATOIRE** (avant entraînement) = **13.6%** de réussite
  moyenne (récompense moyenne **−0.245**) → DQN entraîné (réseau + replay +
  cible) = **100.0%** de réussite sur les 5 graines (récompense moyenne
  **+0.930**). Chiffres réels d'une exécution complète (build Debug/Windows,
  `jenga build --target NKRLTest`), pas de valeurs inventées.
- ⬜ Courbes de récompense (suivi/export au fil de l'entraînement) — non fait,
  seuls les points d'étape sont affichés en console.
- ⬜ **Prioritized Experience Replay (PER)** — le replay est uniforme (limite
  assumée, cf commentaire `NkReplayBuffer.h`).
- ⬜ **Double DQN** (découpler sélection/évaluation de l'action cible pour
  réduire le biais de surestimation) — non fait, DQN « vanille ».
- ⬜ Synchronisation cible **douce** (Polyak averaging) — seule la synchronisation
  **dure** périodique est implémentée.
- ⬜ Encodage d'état riche (features continues type position/attributs) — l'entrée
  du réseau est un one-hot brut de l'état, donc pas de partage de
  représentation entre états proches (le réseau reste proche d'une table dans
  son expressivité d'ENTRÉE, même si l'architecture — réseau + replay + cible
  + Bellman + Adam — est un DQN complet).

## Jalon 3 — politiques ✅ (PPO, actions continues, preuve multi-agent)
- ✅ **Nouvel opérateur NKAutograd** `autograd::Log` (`NK_LOG`, cf `NKAutograd/NkVar.h/.cpp` et
  `NKTensor/NkTensorOps.h/.cpp`) : NKAutograd n'avait ni `Log` ni `Div` différentiables ; `Log`
  était nécessaire pour une log-probabilité discrète EXACTE (`log softmax(logits)[a] =
  Log(Softmax(logits))`, masqué puis réduit — même esprit que le « gather par masque+Matmul » déjà
  documenté pour `NkDQN`). Backward `d/dx log(max(x,eps)) = 1/max(x,eps)`, eps=1e-8. La version
  continue (gaussienne) n'en a pas eu besoin (forme fermée en `(a-μ)²·exp(-2logσ)`).
- ✅ **Réseau de politique** `rl::NkPolicyNet` (`src/NKRL/NkPolicyNet.h/.cpp`), MLP (NKNN
  Dense→Relu→Dense) à deux modes configurables à la construction :
  - **Discrete** : logits `[1,numActions]`, log-prob = `Sum(Mul(Log(Softmax(logits)), onehot))`.
  - **ContinuousGaussian** : moyenne `μ = actionScale·tanh(sortie MLP)` + écart-type `σ=exp(logσ)`
    appris comme paramètre SÉPARÉ indépendant de l'état (convention standard, cf Spinning Up/
    Baselines). Log-densité et entropie en forme fermée (algèbre directe, pas de round-trip
    Log/Exp). `SampleAction` (Box-Muller pour le bruit gaussien) et `GreedyAction` (évaluation).
- ✅ **PPO** `rl::NkPPO` (`src/NKRL/NkPPO.h/.cpp`) — Schulman et al. 2017,
  arXiv:1707.06347 (objectif clippé `L^CLIP = E[min(rₜAₜ, clip(rₜ,1-ε,1+ε)Aₜ)]` + bonus d'entropie
  `S`) : rollout ON-POLICY borné (`rolloutSize`, vidé après chaque mise à jour), critique MLP
  séparé (poids + Adam indépendants), **GAE COMPLET** (Schulman et al. 2016, arXiv:1506.02438,
  récursion arrière `Aₜ = δₜ + γλ(1-doneₜ)Aₜ₊₁`, PAS de simplification par rapport au papier),
  avantages normalisés (moyenne 0, écart-type 1), `epochs` passes d'optimisation par rollout sur
  les MÊMES avantages/retours (convention standard). Pas de version REINFORCE séparée : PPO EST
  l'algorithme de ce Jalon (cf en-tête `NkPPO.h` pour la justification).
- ✅ **Espace d'actions continues** : `rl::NkReach2D` (`src/NKRL/NkReach2D.h/.cpp`) — point 2D,
  déplacement `(dx,dy)` borné `[-maxStep,maxStep]`, cible tirée au hasard à CHAQUE épisode
  (généralisation, pas une trajectoire mémorisée). Observation NORMALISÉE dans `[-1,1]`
  (coordonnées brutes jusqu'à `worldSize` déséquilibraient l'apprentissage d'un MLP fraîchement
  initialisé — cf bug ci-dessous). `rl::NkContinuousEnv` = interface généralisant `rl::NkEnvironment`
  aux observations/actions continues.
- ✅ **Preuve multi-agent minimale** `rl::NkReach2DMulti` (`src/NKRL/NkReach2DMulti.h/.cpp`) : N
  agents-points dans le MÊME monde, avancés SIMULTANÉMENT par un seul `Step()` (co-présence réelle,
  pas N environnements indépendants), couplés par une pénalité de collision UNIQUEMENT. Chaque
  agent a sa PROPRE politique PPO indépendante (pas de politique partagée, pas de coordination, pas
  de récompense partagée, pas d'observation des actions d'autrui, pas d'apprentissage centralisé) —
  **PAS du MARL coopératif/compétitif avancé**, honnêtement documenté dans l'en-tête du fichier.
- 🎯 ✅ **Preuve réelle** (`NKRLTest`, Tests 3/4/5, `jenga build --target NKRLTest --config Debug
  --platform Windows` + `jenga run`, build et exécution RÉELS) :
  - Test 3 (sanity mode Discrete, monde-grille du Jalon 1, 1 graine) : politique aléatoire **1%**
    de réussite (récompense **−1.165**) → PPO discret entraîné **100%** (récompense **+0.86**).
  - Test 4 (mode ContinuousGaussian, `NkReach2D`, moyenne sur **5 graines** 11/22/33/44/55, 500
    épisodes d'entraînement, 100 épisodes d'évaluation gloutonne) : politique aléatoire continue
    **10.0%** de réussite (récompense moyenne **−11.20**) → PPO continu entraîné (gaussienne +
    critique + GAE + clip) **26.0%** de réussite (récompense moyenne **−4.98**).
  - Test 5 (preuve multi-agent, 2 PPO indépendants dans `NkReach2DMulti`, moyenne sur **3 graines**
    11/22/33, 350 épisodes) : AVANT (aléatoire) succès moyen (2 agents) **9.83%**, récompense
    moyenne **−11.10** → APRÈS (2 PPO entraînés simultanément) succès moyen **10.0%**, récompense
    moyenne **−7.34**. Progrès net en récompense, marginal en taux de réussite (tâche plus dure :
    observation 6D avec position relative de l'autre agent, budget d'entraînement plus modeste par
    agent) — chiffres réels, pas inventés, honnêtes sur leur modestie.
  - Chiffres réels d'exécutions complètes, aucune valeur inventée.
- 🐛 **Bug réel rencontré et corrigé pendant ce Jalon** (documenté pour traçabilité) : la première
  version de `NkPolicyNet` (mode continu) laissait la moyenne μ NON bornée en sortie du MLP. Dans un
  espace d'actions saturé par l'environnement, tout μ suffisamment extrême produit le MÊME
  comportement une fois clampé : aucune force de rappel ne s'oppose à sa dérive, et μ divergeait
  vers ±∞ au fil de l'entraînement (observé : de `(-3.89,0.14)` à 59 mises à jour à `(-4.97,-2.56)`
  à 195 mises à jour, sur le MÊME état-sonde) — la politique gloutonne finissait par foncer dans un
  coin fixe du monde indépendamment de la cible, PIRE qu'une politique aléatoire. Corrigé par
  `μ = actionScale·tanh(sortie brute)` (`NkPolicyNetConfig::actionScale`) + normalisation des
  observations dans `[-1,1]` (les coordonnées brutes jusqu'à `worldSize=10` déséquilibraient aussi
  l'apprentissage). Limite HONNÊTE : ceci ne borne que la MOYENNE, pas la densité complète — une
  vraie politique « squashée » façon SAC corrigerait aussi la log-densité par le jacobien de tanh,
  ce qui n'est PAS fait ici (documenté dans `NkPolicyNet.h`).
- ⬜ **Minibatching** à l'intérieur d'une mise à jour PPO — chaque époque utilise TOUT le rollout,
  pas de sous-échantillonnage.
- ⬜ **Collecte parallèle multi-environnements** — un seul rollout séquentiel (pas de vecteur
  d'environnements).
- ⬜ **Traitement batché de la perte clippée** — faute d'opérateur « gather »/tranche
  différentiable dans NKAutograd pour indexer un lot par échantillon (même contrainte que
  `NkDQN`, cf son en-tête), chaque transition est traitée par un forward/backward INDIVIDUEL
  (batch=1) ; coût CPU plus élevé qu'un PPO batché de référence, acceptable à l'échelle de ces
  environnements-jouets mais pas industriel.
- ⬜ **Vraie politique squashée** (SAC-style, jacobien de tanh dans la log-densité) — limite
  documentée ci-dessus.
- ⬜ **MARL coopératif/compétitif avancé** (récompense partagée, communication, apprentissage
  centralisé/CTDE) — la preuve multi-agent de ce Jalon est volontairement minimale (cf
  `NkReach2DMulti.h`).

## Plus tard
- ⬜ Récompense intrinsèque / curiosité (exploration émergente).
- ⬜ Apprentissage hors ligne (à partir de traces).
- ⬜ Intégration directe aux environnements de [NKCivilization](../NKCivilization/README.md).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
