# NKInput — entrées (manette, boutons)

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKInput` est le pilote qui écoute les **entrées du joueur** : la manette, les boutons, et tout
contrôleur que la console expose. Il lit l'état du matériel (par interruption quand le périphérique
change, ou par scrutation), le transforme en événements propres (bouton pressé/relâché, axe
déplacé), et les pousse vers le haut.

C'est la source du **backend Bare de NKEvent** : le moteur (et donc les jeux Nkoung) reçoit ces
entrées **exactement comme** il recevrait celles d'un clavier/souris sur PC — il ne sait pas
qu'elles viennent d'un GPIO ou d'un contrôleur maison. C'est ce qui rend les jeux portables
sans modification entre le PC de dev et la console.

## Responsabilités

- Lire l'état des contrôleurs (IRQ via NKInterrupt, ou scrutation).
- Anti-rebond (debounce) des boutons, normalisation des axes.
- Produire des **événements d'entrée** propres.
- Alimenter le **backend Bare de NKEvent**.

## Place dans la couche

- **Dépend de** : [NKDriver](../NKDriver/README.md), [NKInterrupt](../NKInterrupt/README.md).
- **Alimente** : le backend Bare de `NKEvent` (→ le moteur et les jeux).

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
