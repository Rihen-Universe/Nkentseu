# NKNN — réseaux de neurones

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKNN` fournit les **briques avec lesquelles on assemble un réseau de neurones** : les *couches*
(la transformation élémentaire), les *activations* (la non-linéarité qui donne sa puissance au
réseau) et les *fonctions de perte* (la mesure d'erreur qu'on cherche à réduire).

On part des couches simples — une couche **dense** (entièrement connectée) — puis on monte vers
les couches de **convolution** (vision), de **normalisation** (stabilité de l'entraînement), et
surtout le bloc d'**attention / transformer**, qui est l'architecture au cœur des LLM modernes.
Chaque couche est écrite au-dessus de NKTensor et **différentiable** grâce à NKAutograd : on les
empile, on calcule l'erreur, et l'apprentissage se propage automatiquement. C'est ici qu'on
*définit* un modèle.

## Responsabilités

- Couches : **dense**, **convolution**, **normalisation**, **attention/transformer**, embeddings.
- Activations (ReLU, GELU, sigmoïde, softmax…) et fonctions de **perte** (MSE, entropie croisée…).
- Conteneur de **modèle** (empiler des couches) + accès aux paramètres.
- Sérialisation des poids (sauver/charger un modèle).

## Place dans la couche

- **Dépend de** : [NKTensor](../NKTensor/README.md), [NKAutograd](../NKAutograd/README.md).
- **Utilisé par** : [NKTrain](../NKTrain/README.md), [NKInfer](../NKInfer/README.md), [NKRL](../NKRL/README.md), [NKGen](../NKGen/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
