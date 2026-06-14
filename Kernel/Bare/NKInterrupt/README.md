# NKInterrupt — interruptions et exceptions

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKInterrupt` gère les deux façons dont le CPU **interrompt** le flux normal du programme : les
**exceptions** (le processeur signale une erreur — division par zéro, faute de page, accès
illégal) et les **interruptions matérielles** (un périphérique réclame de l'attention — le timer
tique, la manette a un nouvel état, l'UART a reçu un octet).

Son travail : installer la **table des vecteurs**, capter ces événements, sauvegarder le
contexte (via NKArch), puis **router** chaque interruption vers le bon gestionnaire. C'est la
colonne vertébrale de tout ce qui est *réactif* dans le système : sans lui, pas d'ordonnanceur
préemptif, pas de pilotes pilotés par IRQ, pas de base de temps fiable. Les **exceptions** non
gérées deviennent une **panique** lisible (dump des registres sur NKSerial) au lieu d'un gel
silencieux.

## Responsabilités

- Installer la table des vecteurs (IDT sur x86, table d'exceptions sur ARM).
- Point d'entrée commun : sauvegarder/restaurer le contexte (NKArch).
- **Dispatcher** les IRQ vers les gestionnaires enregistrés (par les pilotes).
- Gérer les **exceptions** → panique lisible.
- Masquer/démasquer et acquitter les IRQ (contrôleur d'interruptions : PIC/APIC, GIC).

## Place dans la couche

- **Dépend de** : [NKArch](../NKArch/README.md).
- **Utilisé par** : [NKTimerHW](../NKTimerHW/README.md), [NKDriver](../NKDriver/README.md),
  [NKInput](../NKInput/README.md), [NKAudioHW](../NKAudioHW/README.md), [NKScheduler](../NKScheduler/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
