# NKSerial — port série (UART)

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKSerial` est notre **premier moyen de communiquer avec l'extérieur**. Tant que l'écran n'est
pas piloté (ça vient bien plus tard, en Phase 3), le **port série** est le seul endroit où l'on
peut afficher quoi que ce soit. C'est l'outil de débogage n°1 du bas niveau : sans lui, un noyau
qui plante est totalement muet.

Il commence **minimal et en polling** (on écrit un octet dans le registre de l'UART et on
attend qu'il parte), ce qui suffit pour logger pendant tout le bring-up. Plus tard, il devient un
**puits (sink) pour NKLogger** : tous les `NK_LOG_*` du moteur ressortent sur le série. La
lecture (RX) permet aussi une **console de debug** précoce. La version pilotée par interruptions
viendra une fois NKInterrupt prêt.

Matériel visé : **16550** (x86/QEMU), **PL011** (ARM).

## Responsabilités

- Initialiser l'UART (baud, format).
- Émettre un octet / une chaîne (polling d'abord).
- Servir de **sink NKLogger** sur le métal.
- Lire des octets en entrée (console série de debug).

## Place dans la couche

- **Dépend de** : [NKArch](../NKArch/README.md) (accès registres) ; plus tard [NKDriver](../NKDriver/README.md) et [NKInterrupt](../NKInterrupt/README.md) (mode IRQ).
- **Utilisé par** : tout le noyau, pour le log précoce.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
