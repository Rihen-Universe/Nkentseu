# NKTrain — entraînement

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKTrain` **assemble** les briques précédentes en une **boucle d'apprentissage** qui marche. Seul,
aucun des modules n'entraîne un modèle : il faut orchestrer le cycle — présenter un lot (NKData),
calculer la sortie et l'erreur (NKNN), rétropropager les gradients (NKAutograd), mettre à jour les
poids (NKOptim), recommencer. C'est ce cycle que NKTrain encapsule.

Au-delà de la boucle, il fournit ce qui rend un entraînement **utilisable sur la durée** : la
sauvegarde de **points de reprise** (pour ne pas tout perdre et reprendre plus tard), le suivi de
**métriques** (perte, précision — pour savoir si ça progresse), et l'évaluation sur un jeu de
**validation** (pour détecter le surapprentissage). C'est le module qu'on lance quand on veut
réellement « entraîner un modèle ».

## Responsabilités

- Boucle d'entraînement : forward → perte → backward → pas d'optimiseur, sur les époques.
- **Checkpoints** : sauver/charger l'état (modèle + optimiseur) pour reprendre.
- **Métriques** et journalisation (perte, précision…).
- Boucle de **validation** / évaluation.
- Points d'accroche (callbacks) : early-stopping, planning, logs.

## Place dans la couche

- **Dépend de** : [NKNN](../NKNN/README.md), [NKOptim](../NKOptim/README.md), [NKData](../NKData/README.md).
- **Produit** : des modèles entraînés, consommés par [NKInfer](../NKInfer/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
