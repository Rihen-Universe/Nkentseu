# NKEvolve — Roadmap

> Faire émerger l'intelligence par sélection (Phase 5). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — algorithme génétique (Phase 5) — 🟡 livré et prouvé (2026-07-23)
- ✅ Génome (`NkGenome` : gènes réels + fitness) + population (`NkPopulation`, init aléatoire
  LCG déterministe) + fonction d'adaptation (*fitness* = pointeur de fonction fourni par
  l'appelant, `NkFitnessFn`, sans `std::function`).
- ✅ Sélection **par tournoi**, **croisement arithmétique** (recombinaison pondérée), **mutation
  gaussienne** (Box-Muller, bornée) + **élitisme** → nouvelle génération (`NkEvolution::RunGeneration`).
- 🎯 ✅ **Atteint** : `NKEvolveTest` (build+run réels) — population 80 individus / 6 gènes,
  problème « approcher un vecteur cible » : **fitness moyen 0,013 → 0,91** en 200 générations,
  **meilleur génome jamais vu → fitness 1,0** (erreur max/gène 0,0005). Le fitness moyen
  progresse nettement génération après génération (bruité par la mutation continue, comme
  attendu d'un GA réel — pas monotone strict, mais tendance nette et meilleur-jamais-vu
  monotone grâce à l'élitisme + suivi explicite).
- ⬜ Reste du Jalon 1 : suivi de diversité (mesurer la convergence prématurée), problème
  d'évaluation plus riche (aujourd'hui une fonction jouet, pas encore un agent/environnement).

## Jalon 2 — neuroévolution
- ⬜ Faire évoluer les poids d'un réseau NKNN.
- ⬜ Faire évoluer la **structure** (topologie) — style NEAT.
- ⬜ Suivi de la **diversité** (éviter la convergence prématurée).

## Jalon 3 — vie artificielle
- ⬜ Reproduction *située* (les agents se reproduisent dans le monde, pas par lots).
- ⬜ Pression de sélection émergente (survie, ressources).
- ⬜ Hérédité de traits + variation continue.

## Plus tard
- ⬜ Co-évolution (proies/prédateurs, compétition).
- ⬜ Spéciation, niches écologiques.
- ⬜ Couplage fin avec [NKCivilization](../NKCivilization/README.md).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
