# NKArch — abstraction d'architecture CPU

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKArch` est la **seule** brique qui connaît les détails du **processeur**. Tout ce qui dépend
de l'architecture (et qui n'est pas un périphérique) vit ici : accès aux registres, sauvegarde
et restauration de **contexte** (pour les interruptions et l'ordonnanceur), contrôle bas niveau
de la **MMU** (tables de pages), mise en place du mode CPU, barrières mémoire et primitives
atomiques au niveau du métal, et les instructions de mise en veille (`halt`/`wfi`).

C'est aussi le seul module qui contient de l'**assembleur** spécifique à une architecture.
Cibles prévues : **x86_64** (pour itérer vite sous QEMU) et **ARM64** (la console). Les couches
au-dessus ne voient qu'une **interface unifiée** : changer d'architecture = fournir une nouvelle
implémentation de NKArch, sans toucher au reste.

## Responsabilités

- Activer/désactiver les interruptions, mettre le CPU en veille.
- Lire/écrire les registres de contrôle et le contexte d'exécution.
- Définir la structure de **contexte** (registres sauvegardés) utilisée par NKInterrupt et NKScheduler.
- Primitives MMU : charger la base des tables de pages, activer la pagination, invalider le TLB.
- Barrières mémoire et opérations atomiques au niveau matériel.

## Place dans la couche

- **Dépend de** : rien (c'est le socle).
- **Utilisé par** : [NKBoot](../NKBoot/README.md), [NKInterrupt](../NKInterrupt/README.md),
  [NKPMM](../NKPMM/README.md), [NKScheduler](../NKScheduler/README.md).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
