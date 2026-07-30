# NKEvolve — vie artificielle & évolution

> 🟡 Jalon 1 (algorithme génétique) livré et prouvé — cf. [ROADMAP](ROADMAP.md) et
> l'[architecture de la couche](../ARCHITECTURE.md).

## Rôle

`NKEvolve` apporte l'autre grande façon d'obtenir de l'intelligence : non pas en l'**entraînant**,
mais en la **faisant évoluer**. Plutôt qu'un gradient, on s'inspire de la sélection naturelle —
une **population** d'individus, chacun décrit par un **génome**, est évaluée ; les meilleurs se
**reproduisent** (croisement), avec des **mutations**, et la génération suivante est, en moyenne,
un peu meilleure. Sur de nombreuses générations, des comportements complexes **émergent** sans
qu'on les ait conçus.

C'est l'ingrédient clé pour une civilisation **vivante** : on n'écrit pas le comportement de
chaque être, on laisse la pression de sélection le **façonner**. NKEvolve sert aussi à optimiser
ce qui se dérive mal (structures de réseaux, hyperparamètres, stratégies). C'est la brique
« artificial life » qui, combinée à NKRL et NKAgent, donne des populations qui s'adaptent.

## Responsabilités

- Représentation d'un **génome** (paramètres, structure) et d'une **population**.
- **Sélection** (selon une fonction d'adaptation / *fitness*).
- **Croisement** et **mutation** → nouvelle génération.
- Boucle évolutionnaire ; suivi de la diversité et de l'adaptation.
- Neuroévolution (faire évoluer des réseaux NKNN).

## État livré (2026-07-23)

- `NkGenome` (`src/NKEvolve/NkGenome.h`) : gènes réels (`NkVector<float>`) + fitness.
- `NkPopulation` (`NkPopulation.h/.cpp`) : population initialisée aléatoirement (LCG
  déterministe, même schéma que `rl::NkQLearning`/`nn::NkDense`), `BestIndex`/`BestFitness`/
  `MeanFitness`.
- `NkEvolution` (`NkEvolution.h/.cpp`) : moteur générationnel — élitisme, **sélection par
  tournoi**, **croisement arithmétique** (recombinaison pondérée par un `alpha` aléatoire),
  **mutation gaussienne** (Box-Muller sur NKMath, bornée aux limites du génome). La fonction
  de fitness est un **pointeur de fonction** (`NkFitnessFn`, pas de `std::function` —
  convention zéro-STL) : le moteur est générique, indépendant du problème.
- Prouvé par `NKEvolveTest` (`Applications/NKEvolveTest/`, build + run réels Debug/Windows) :
  population de 80 individus / 6 gènes faisant évoluer un vecteur vers une cible fixe.
  **Fitness moyen de la population 0,013 → 0,91** sur 200 générations, **meilleur génome
  jamais vu → fitness 1,0** (erreur max par gène 0,0005). Preuve que la boucle
  sélection+croisement+mutation fait *réellement* progresser une population, sans gradient.
- Reste (Jalon 1 → 2) : neuroévolution (faire évoluer des poids `nn::NkDense`/topologies),
  suivi de diversité, couplage à un problème d'agent (ex. hyperparamètres `NkAgent`/
  `NkGridWorld`) plutôt que la fonction jouet actuelle.

## Place dans la couche

- **Dépend de** : [NKTensor](../NKTensor/README.md) (et [NKNN](../NKNN/README.md) pour la neuroévolution).
- **Utilisé par** : [NKCivilization](../NKCivilization/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
