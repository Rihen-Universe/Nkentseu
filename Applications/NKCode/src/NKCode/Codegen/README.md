# Codegen — le visuel devient du code exécutable

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Codegen` est la pièce qui rend la programmation visuelle **réelle** : il **compile un graphe en
code texte**, que **Jenga** construit ensuite. C'est exactement le pipeline d'Unreal (Blueprint →
C++ → build) : le graphe n'est pas interprété par magie, il est **traduit** en un code lisible et
compilable.

Concrètement : il parcourt le graphe ([Graph](../Graph/README.md)) en suivant le **flux
d'exécution**, résout le **flux de données**, et émet du code — **C++** pour Blueprint (logique de
jeu performante), un **script lisible** pour Blocks (accessibilité). Le code généré est déposé dans
un projet Jenga, puis **buildé et lancé** via le sous-système [Project](../Project/README.md). Une
alternative pour le prototypage rapide est une **petite VM** qui interprète le graphe sans passer
par la compilation — utile pendant l'édition.

## Responsabilités
- **Parcours** du graphe (ordre d'exécution, résolution des données).
- **Émission de code** : Blueprint → C++ ; Blocks → script lisible.
- Intégration au build : injecter le code généré dans un projet Jenga → **build & run**.
- (Option) **VM d'interprétation** pour exécuter un graphe sans compiler (aperçu rapide).

## Dépend de
[Graph](../Graph/README.md), [Blueprint](../Blueprint/README.md) & [Blocks](../Blocks/README.md),
[Project](../Project/README.md) (build via **Jenga**), NKContainers/NKMath.

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
