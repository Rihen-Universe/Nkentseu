# NKSimulation — état simulé (eau, océan, brouillard volumétrique, fumée, feu)

> **Statut : SPÉCIFICATION.** Ce dossier ne contient AUCUN code et n'est PAS
> enregistré dans `Nkentseu.jenga`. Créer une cible de build qui ne compile rien
> ajouterait du bruit à chaque build sans rien apporter. Le module naîtra avec sa
> première simulation réelle — voir `docs/ROADMAP.md`, phase S1.

## Pourquoi un module séparé de NKRenderer

La ligne de partage n'est pas le sujet (« l'eau »), c'est la **nature du code** :

| nature | exemple | où ça vit |
|---|---|---|
| **Apparence** — dessine, sans mémoire d'une frame à l'autre | brouillard de hauteur, réfraction, caustiques, post-process sous-marin | **NKRenderer** |
| **État** — évolue dans le temps, interrogeable | houle FFT, particules de fluide, grille de fumée, flottabilité | **NKSimulation** |

Le critère qui tranche, et qui doit rester la règle du module :

> **Si le code doit tourner alors que rien n'est affiché, il n'appartient pas au
> moteur de rendu.**

Un bateau qui flotte demande la hauteur de vague à `NKPhysics`. Le système de
civilisation peut vouloir un modèle hydrique. Une animation peut précalculer une
simulation sans fenêtre ouverte. Si l'état vit dans NKRenderer, chacun de ces cas
doit lier le moteur de rendu entier pour rien.

## Chaînage des dépendances

```
NKRHI            périphérique + compute (abstraction matérielle)
   ↑
NKSimulation     état + calcul  ← CE MODULE
   ↑                    ↑
NKRenderer        NKPhysics / NKAI / animation hors-ligne
(dessine le        (interrogent l'état sans rien afficher)
 résultat)
```

**NKSimulation dépend de NKRHI, jamais de NKRenderer.** La simulation a besoin
d'un GPU pour le compute, mais NKRHI est l'abstraction du *matériel* alors que
NKRenderer est la *politique de rendu*. C'est cette distinction qui permet à la
physique d'interroger la houle sans tirer les ombres et le post-process.

Le module produit des **données** : carte de déplacement et de normales pour
l'océan, grille de densité pour la fumée, tampon de particules pour un fluide.
NKRenderer les consomme comme il consomme déjà une texture — il n'a pas à savoir
comment elles ont été calculées.

## Ce que ce module ne fera PAS

- Aucun `NkDrawCall`, aucun pipeline graphique, aucun shader de fragment.
  Le rendu de l'eau (réfraction, reflets, caustiques) reste dans NKRenderer, qui
  dispose déjà de `NkPlanarReflectionSystem`.
- Aucune dépendance à `NKWindow` : tout doit tourner sans fenêtre, et donc être
  testable en console (voir la leçon `NkEditMesh` dans la ROADMAP).

## Preuve que la séparation est nécessaire — coût déjà payé

Ce n'est pas une préférence de style. Deux cas mesurés dans ce dépôt :

1. **`NkEditMesh` est du CPU pur** (pas une ligne de GPU) mais vit dans
   NKRenderer. Conséquence : `NKEditMeshHarness`, qui ne teste que de la
   géométrie, doit lier NKRenderer + NKRHI + NKSL + NKWindow + glslang. Chaque
   build du harnais coûte plusieurs minutes au lieu de quelques secondes.
2. **La retopologie existe en double** (Noge et NkEditMesh) : la même question
   « où ça vit » a déjà créé une duplication non résorbée.

Et un troisième cas s'annonce : `NkVFXSystem` (NKRenderer/Tools/VFX) tient déjà
un état de particules (vélocité, gravité, durée de vie). Le jour où ces
particules auront une vraie physique, le problème recommence.
