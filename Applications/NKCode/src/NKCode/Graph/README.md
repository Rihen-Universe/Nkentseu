# Graph — le substrat de programmation visuelle

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Graph` est le **moteur commun** des deux langages visuels. Plutôt que d'écrire deux éditeurs
séparés (un pour Scratch, un pour Blueprint), NKCode pose **un seul modèle** — **nœuds**, **broches**
(les points de connexion) et **liens** — et un **canvas** pour le manipuler. Blocks et Blueprint
ne sont alors que **deux présentations** de ce même substrat.

Le canvas fournit l'interaction attendue d'un éditeur de nœuds : **pan/zoom**, **sélection**,
**déplacement** des nœuds, et surtout **tirer un lien** d'une broche à une autre (avec
prévisualisation et validation). Il gère l'**undo/redo** et la **sérialisation** des graphes
(sauver/recharger), via NKSerialization avec un schéma décrit par NKReflection. C'est la fondation
sur laquelle Blocks, Blueprint et Codegen s'appuient tous.

## Responsabilités
- Modèle **nœud / broche / lien** (typage des broches délégué aux langages).
- **Canvas** : pan/zoom, sélection, déplacement, **connexion** par glisser, alignement.
- Undo/redo ; copier/coller ; commentaires/regroupements.
- **Sérialisation** des graphes (NKSerialization / NKReflection).

## Dépend de
**NKCanvas**/**NKUI**, NKEvent, NKMath (géométrie du canvas), **NKSerialization** + **NKReflection**,
NKContainers.

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
