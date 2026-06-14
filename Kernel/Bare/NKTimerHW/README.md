# NKTimerHW — timer matériel

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKTimerHW` donne au système une **notion du temps**. Sans lui, le noyau ne sait ni mesurer une
durée, ni se réveiller périodiquement. Il pilote les **timers matériels** pour produire deux
choses : un **compteur monotone** (« combien de temps depuis le boot ? », pour mesurer et pour
le `dt` des jeux) et un **tick périodique** (une interruption à intervalle régulier).

Ce tick est vital pour deux clients : l'**ordonnanceur** (NKScheduler s'en sert pour préempter
les tâches) et la couche **NKTime** du moteur (qui, via son backend Bare, lit ce temps au lieu
de l'horloge de l'OS). C'est donc NKTimerHW qui, en bout de chaîne, fait avancer la boucle de jeu.

Matériel visé : **PIT/HPET/TSC** (x86), **timer générique ARM** (ARM64).

## Responsabilités

- Compteur monotone haute résolution (temps depuis le boot).
- Tick périodique configurable (IRQ via NKInterrupt).
- Attente active courte (`busy-wait` calibré) pour les pilotes.
- Source de temps pour le **backend NKTime** et pour **NKScheduler**.

## Place dans la couche

- **Dépend de** : [NKDriver](../NKDriver/README.md), [NKInterrupt](../NKInterrupt/README.md).
- **Utilisé par** : [NKScheduler](../NKScheduler/README.md), et le backend Bare de `NKTime`.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
