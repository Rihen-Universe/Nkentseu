# NKScheduler — ordonnanceur de tâches

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKScheduler` permet à plusieurs **tâches** de s'exécuter sur le même processeur en se partageant
le temps. Sans lui, le noyau ne fait qu'une chose à la fois, de bout en bout. Avec lui, on peut
séparer proprement le travail : la boucle de jeu, le mixage audio, le chargement d'assets en
fond, la mise à jour du dashboard — chacun dans sa tâche.

Il s'appuie sur deux briques : le **contexte** de NKArch (sauvegarder/restaurer les registres
d'une tâche pour passer à une autre) et le **tick** de NKTimerHW (pour reprendre la main
régulièrement). On commence **coopératif** (une tâche rend la main explicitement — simple et
prévisible), puis on passe **préemptif** (le tick interrompt la tâche courante), qui est ce que
font les vrais OS. C'est aussi lui qui implémente l'**attente** (un `Sleep` qui ne brûle pas le
CPU) et la synchronisation entre tâches.

## Responsabilités

- Représenter une **tâche** (pile, contexte, état).
- Changement de contexte (via NKArch) ; files d'exécution.
- Ordonnancement **coopératif** puis **préemptif** (tick NKTimerHW).
- Primitives d'attente / réveil et synchronisation entre tâches.

## Place dans la couche

- **Dépend de** : [NKArch](../NKArch/README.md), [NKInterrupt](../NKInterrupt/README.md),
  [NKTimerHW](../NKTimerHW/README.md).
- **Utilisé par** : [NKConsoleRT](../NKConsoleRT/README.md) et le moteur (tâches de fond).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
