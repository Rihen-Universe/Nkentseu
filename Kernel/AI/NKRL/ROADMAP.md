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

## Jalon 2 — deep RL
- ⬜ **DQN** (réseau de valeur) + replay buffer + réseau cible.
- ⬜ Gestion d'épisodes, courbes de récompense.

## Jalon 3 — politiques
- ⬜ **Policy-gradient** ; **PPO**.
- ⬜ Espaces d'actions continus.
- ⬜ Multi-agent (plusieurs apprenants dans le même monde).

## Plus tard
- ⬜ Récompense intrinsèque / curiosité (exploration émergente).
- ⬜ Apprentissage hors ligne (à partir de traces).
- ⬜ Intégration directe aux environnements de [NKCivilization](../NKCivilization/README.md).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
