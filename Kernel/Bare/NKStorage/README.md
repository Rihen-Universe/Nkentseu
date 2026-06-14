# NKStorage — stockage (SD / flash)

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKStorage` est le pilote du **support de stockage** de la console : carte SD, mémoire flash, ou
disque virtuel sous QEMU. Il travaille au niveau le plus brut — des **blocs** d'octets que l'on
lit et écrit par adresse. Il ne connaît rien aux fichiers ni aux dossiers : c'est uniquement un
canal fiable vers les octets persistants.

Au-dessus de lui, [NKFSBare](../NKFSBare/README.md) construit la notion de fichiers. NKStorage se
concentre sur la mécanique du périphérique : initialisation, lecture/écriture de blocs, gestion
des erreurs et, plus tard, transferts efficaces (DMA). C'est la base de tout ce qui doit
**survivre à l'extinction** : les assets des jeux, les sauvegardes, la configuration.

## Responsabilités

- Initialiser le contrôleur de stockage (SD/flash/virtio sous QEMU).
- Lire / écrire des **blocs** par adresse (LBA).
- Gestion des erreurs et nouvelles tentatives.
- Transferts efficaces (DMA) plus tard.

## Place dans la couche

- **Dépend de** : [NKDriver](../NKDriver/README.md).
- **Socle de** : [NKFSBare](../NKFSBare/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
