# NKCivilization — simulation de civilisation virtuelle

> 🟡 Jalon 1 ✅ sur le **vrai substrat NKECS** (2026-07-25) : la lib `NKCivilization`
> (`src/NKCivilization/`) fournit composants (`NkCivPosition`, `NkCivAgentRef`), substrat
> partagé (`NkCivGridState` : grille, trous, but commun, ressources consommables) et system
> (`NkCivAgentSystem`, un `ecs::NkSystem`). Prouvé par `Applications/NKCivilizationTest/`
> (build+run réels) : 3 agents = 3 entités ECS, collision déclenchée (mesurée), compétition de
> ressources premier-arrivé (répartition divergente), 3/3 au but via politiques apprises. Voir
> la [ROADMAP](ROADMAP.md) et l'[architecture de la couche](../ARCHITECTURE.md).

## Rôle

`NKCivilization` est l'**aboutissement** de NKAI : le monde où des centaines d'agents vivent,
interagissent, et forment une **société** qu'on regarde **évoluer**. C'est ici que tout converge —
les tenseurs calculent, les réseaux apprennent (NKNN), les agents décident (NKAgent), les
populations évoluent (NKEvolve) — pour produire une **civilisation virtuelle** émergente.

Le module fournit le **substrat** : un monde (espace, ressources), un **temps** qui s'écoule (donc
un passé qui s'accumule et un futur qui se construit), et le tissu des **interactions** (échanges,
coopération, conflit, transmission). Chaque être est une entité **NKECS** ; NKCivilization
orchestre leur cohabitation. On peut alors l'**observer** : enregistrer l'histoire, la rejouer,
mesurer des tendances, et explorer des scénarios **« what-if »**.

Deux garde-fous rappelés (cf. [architecture §1](../ARCHITECTURE.md#1-pourquoi-cette-couche-existe)) :
le comportement « libre » des êtres est de l'**émergence** (il naît des politiques et des
interactions, pas de scripts), pas du libre arbitre littéral ; et la **prospective** est un bac à
sable de trajectoires plausibles, **pas un oracle** du futur réel.

## Responsabilités

- **Substrat** : monde (espace, ressources), **temps** (histoire qui s'accumule), règles.
- Population d'agents (entités **NKECS**) + leurs **interactions** (social, économie, conflit).
- **Observation** : enregistrement, rejeu, mesures, visualisation (via le moteur).
- **Émergence** : laisser apparaître des structures non scriptées (groupes, cultures, cycles).
- **Prospective** : scénarios « what-if », analyse de tendances.

## Place dans la couche

- **Dépend de** : [NKAgent](../NKAgent/README.md), [NKEvolve](../NKEvolve/README.md), `NKECS` (Runtime), le moteur (rendu/audio) pour visualiser.
- **C'est** : la *killer app* qui relie tout NKAI (cf. [architecture §6](../ARCHITECTURE.md#6-synergie-avec-le-reste-de-nkentseu)).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
