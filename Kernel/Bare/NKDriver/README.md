# NKDriver — framework de pilotes

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKDriver` est le **cadre commun** dans lequel s'écrivent tous les pilotes de périphériques.
Plutôt que chaque pilote (écran, manette, stockage, audio) réinvente sa façon d'être découvert,
initialisé et branché aux interruptions, NKDriver fixe un **modèle unique** : un pilote
s'enregistre, déclare le matériel qu'il sait gérer, reçoit ses ressources (régions MMIO via
NKPMM, lignes d'IRQ via NKInterrupt), et expose une interface stable au reste du système.

Il fournit aussi la **découverte** des périphériques présents (énumération du bus : ports
connus sur QEMU au début, PCIe/arbre de périphériques sur la vraie carte plus tard) et le
**cycle de vie** (init, démarrage, arrêt). C'est la fondation sur laquelle reposent NKDisplay,
NKInput, NKStorage et NKAudioHW — tous sont des *clients* de ce framework.

## Responsabilités

- Modèle de pilote : interface commune (probe, init, start, stop).
- **Enregistrement** des pilotes et **découverte** des périphériques.
- Attribution des ressources : MMIO (NKPMM), IRQ (NKInterrupt).
- Cycle de vie et arbre des périphériques.

## Place dans la couche

- **Dépend de** : [NKInterrupt](../NKInterrupt/README.md), [NKPMM](../NKPMM/README.md).
- **Socle de** : [NKSerial](../NKSerial/README.md) (mode IRQ), [NKTimerHW](../NKTimerHW/README.md),
  [NKDisplay](../NKDisplay/README.md), [NKInput](../NKInput/README.md),
  [NKStorage](../NKStorage/README.md), [NKAudioHW](../NKAudioHW/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
