# NKFSBare — système de fichiers minimal

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKFSBare` transforme les **blocs bruts** de NKStorage en **fichiers et dossiers**. C'est la
couche qui donne un sens à « ouvrir `level1.dat` » : elle interprète une structure sur le disque
(table d'allocation, répertoire), retrouve où sont les octets d'un fichier, et permet de les lire
(et écrire) par nom plutôt que par adresse de bloc.

On vise d'abord la **lecture** d'un format **simple et répandu** (FAT, lisible depuis un PC pour
préparer la carte), suffisant pour charger les **assets des jeux**. Au-dessus, NKFSBare alimente
le **backend Bare de NKFileSystem** : le moteur ouvre ses fichiers **exactement comme** sur un OS
classique, sans savoir qu'il parle à notre propre système de fichiers. L'écriture (sauvegardes,
config) vient ensuite.

## Responsabilités

- Monter un volume depuis NKStorage.
- Parcourir les répertoires ; ouvrir un fichier par chemin.
- Lire le contenu d'un fichier (puis écriture).
- Alimenter le **backend Bare de NKFileSystem**.

## Place dans la couche

- **Dépend de** : [NKStorage](../NKStorage/README.md).
- **Alimente** : le backend Bare de `NKFileSystem` (→ assets, sauvegardes, config).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
