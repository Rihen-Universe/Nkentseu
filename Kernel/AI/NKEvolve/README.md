# NKEvolve — vie artificielle & évolution

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

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

## Place dans la couche

- **Dépend de** : [NKTensor](../NKTensor/README.md) (et [NKNN](../NKNN/README.md) pour la neuroévolution).
- **Utilisé par** : [NKCivilization](../NKCivilization/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
