# NKRL — apprentissage par renforcement

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKRL` permet à un agent d'**apprendre par l'expérience**, sans qu'on lui montre les bonnes
réponses. Contrairement à l'apprentissage supervisé (où l'on donne des exemples étiquetés), ici
l'agent **agit** dans un **environnement**, reçoit une **récompense** (positive ou négative), et
ajuste son comportement pour maximiser la récompense sur le long terme. C'est ainsi qu'on apprend
à un programme à jouer, à un robot à marcher, à un PNJ à survivre.

Le module fournit les deux faces : l'**environnement** (l'état du monde, les actions possibles,
la récompense, la transition) et l'**agent** (sa *politique* : quelle action choisir dans quel
état). On commence par le **Q-learning tabulaire** (simple, on comprend tout), puis le **DQN**
(un réseau approxime la valeur des actions), puis les méthodes de **gradient de politique** (PPO).
C'est le cœur des êtres « décideurs » de la civilisation : chaque agent y apprend à vivre.

## Responsabilités

- Interface d'**environnement** (état, actions, récompense, transition, fin d'épisode).
- **Politiques** et exploration (ε-greedy…).
- Algorithmes : **Q-learning** tabulaire, **DQN**, **policy-gradient / PPO**.
- Mémoire de rejeu (replay buffer), collecte d'épisodes.

## Place dans la couche

- **Dépend de** : [NKNN](../NKNN/README.md), [NKOptim](../NKOptim/README.md), [NKTensor](../NKTensor/README.md).
- **Utilisé par** : [NKAgent](../NKAgent/README.md), [NKCivilization](../NKCivilization/README.md), [NKEmbodied](../NKEmbodied/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
