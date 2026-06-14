# NKOptim — optimiseurs

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKOptim` est la **règle de mise à jour** des paramètres. NKAutograd fournit les gradients (la
direction de descente) ; NKOptim décide **comment** s'en servir pour modifier les poids à chaque
pas d'entraînement. Le choix de l'optimiseur change radicalement la vitesse et la stabilité de
l'apprentissage.

On commence par la **descente de gradient stochastique** (SGD) — simple et fondatrice — puis le
**momentum** (qui lisse la trajectoire) et **Adam** (qui adapte le pas à chaque paramètre, le
choix par défaut moderne). On y ajoute les **plannings de taux d'apprentissage** (faire décroître
le pas au fil de l'entraînement) et le **clipping** de gradient (éviter les explosions). C'est un
module compact mais décisif : un bon optimiseur fait converger un réseau que SGD seul laisserait
stagner.

## Responsabilités

- Optimiseurs : **SGD**, **momentum**, **Adam** (+ variantes).
- Plannings de taux d'apprentissage (décroissance, warmup).
- **Clipping** de gradient.
- Application du pas sur les paramètres d'un modèle (depuis NKNN/NKAutograd).

## Place dans la couche

- **Dépend de** : [NKAutograd](../NKAutograd/README.md) (gradients), [NKTensor](../NKTensor/README.md).
- **Utilisé par** : [NKTrain](../NKTrain/README.md), [NKRL](../NKRL/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
