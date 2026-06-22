# Blocks — le langage par blocs façon Scratch

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Blocks` est le langage visuel **accessible**, calqué sur Scratch. Là où [Blueprint](../Blueprint/README.md)
vise la logique typée et sérieuse, Blocks vise la **prise en main immédiate** et le prototypage :
on assemble des **blocs colorés qui s'emboîtent** comme des pièces de puzzle, sans broches à relier
ni types à gérer. C'est la porte d'entrée idéale pour débuter, pour enseigner, ou pour écrire vite
une petite logique.

Il s'appuie sur le même substrat [Graph](../Graph/README.md) (un bloc = un nœud, l'emboîtement = un
lien d'exécution), mais avec une **présentation et une interaction différentes** : une **palette de
blocs** par catégories (mouvement, contrôle, événements, variables…), un **emboîtement** par
magnétisme (snapping) plutôt que des fils, et des formes qui disent visuellement ce qui se connecte
à quoi. Comme Blueprint, il se **compile en code** via [Codegen](../Codegen/README.md).

## Responsabilités
- **Blocs emboîtables** (snapping) + formes (chapeau d'événement, blocs C, rapports…).
- **Palette** par catégories ; glisser-déposer depuis la palette.
- Variables, événements, structures de contrôle sous forme de blocs.
- Compilation via [Codegen](../Codegen/README.md) (Blocks → script lisible).

## Dépend de
[Graph](../Graph/README.md), [Codegen](../Codegen/README.md), NKUI/NKCanvas, NKContainers.

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
