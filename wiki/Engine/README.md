# Couche Engine

La couche **Engine** est le **framework applicatif** du moteur : elle assemble les sous-systèmes
[Runtime](../Runtime/README.md) en un moteur de jeu et une suite de création cohérents. Son module
central est **Noge** (boucle applicative, LayerStack, EventBus, ECS gameplay, et une suite
modélisation / animation / design 2D / IO / éditeur). C'est sur cette couche que reposent les
applications et l'éditeur **Nogee**.

## Modules

| Module | Rôle | Doc |
|--------|------|-----|
| **Noge** | Framework applicatif : Core (boucle/layers/bus), ECS gameplay, Modeling, Animation, Design2D, IO, Viewport, Physics | [Noge.md](Noge.md) |

> **Nogee** (l'éditeur) est une application bâtie sur Noge ; elle est planifiée (coquille actuelle
> sous `Applications/Nogee`) et sera documentée avec la couche Application.

## Statut

Noge livre son **framework applicatif** (Core) et la **surface ECS gameplay**. Plusieurs
sous-systèmes de création (Modeling, Animation, Design2D, certaines parties IO/Viewport) sont à ce
stade des **headers de spécification** : leur API publique est déclarée et documentée, mais
l'implémentation (`.cpp`) est partielle ou à venir. Les pages détaillées signalent ce statut.

## Dépendances

```
Foundation + System + Runtime
   ▲
Engine — Noge
   ├── Core            (NkApplication, LayerStack, EventBus, profiler)
   ├── ECS gameplay    (Components, Entities, Scene, Systems, Scripting, VisualScript)
   └── Suite création  (Modeling, Animation, Design2D, IO, Viewport, Physics)  [en partie spec]
        ▲
   Applications (Nogee, PV3DE, Pong, Sandbox…)
```

[← Index du wiki](../README.md) · [← Couche Runtime](../Runtime/README.md)
