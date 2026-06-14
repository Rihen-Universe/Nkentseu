# NKAutograd — différentiation automatique

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKAutograd` est le **moteur de l'apprentissage**. Apprendre, pour un réseau, c'est ajuster ses
poids dans la direction qui réduit l'erreur — et cette direction, c'est le **gradient**. Calculer
ces gradients à la main pour des milliers de paramètres est impossible ; `NKAutograd` le fait
**automatiquement**.

Son principe : pendant le calcul « avant » (forward), il **enregistre** les opérations effectuées
sur les tenseurs dans un graphe. Puis, en partant de l'erreur finale, il **remonte** ce graphe
(*backpropagation*, mode inverse) et applique la règle de dérivation en chaîne pour obtenir le
gradient de chaque paramètre. C'est ce que font PyTorch ou TensorFlow sous le capot ; ici on
l'écrit nous-mêmes, au-dessus de NKTensor. C'est la brique qui rend NKNN et NKOptim possibles.

## Responsabilités

- Suivre les opérations sur tenseurs dans un **graphe de calcul** (tape).
- **Rétropropagation** (mode inverse) : dérivées en chaîne, du résultat vers les entrées.
- Accumulation des gradients ; mise à zéro entre deux pas.
- Mode « sans gradient » (inférence) pour économiser mémoire et temps.

## Place dans la couche

- **Dépend de** : [NKTensor](../NKTensor/README.md).
- **Socle de** : [NKNN](../NKNN/README.md), [NKOptim](../NKOptim/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
