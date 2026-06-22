# Shell — la coquille de l'IDE

> ⚠️ Squelette : pas encore de code. Voir l'[architecture](../../../ARCHITECTURE.md) et la
> [roadmap](../../../ROADMAP.md).

## Rôle
`Shell` est l'**ossature** de NKCode : la fenêtre, la boucle applicative, et la mise en page de
tout le reste. C'est lui qui ouvre la fenêtre (NKWindow), pilote la frame (rendu NKUI/NKCanvas,
entrées NKEvent), et organise les **panneaux** (éditeur, explorateur, sortie, graphe) dans un
**système de docking** redimensionnable. Il fournit aussi les éléments transverses d'un IDE
moderne : la **palette de commandes** (recherche d'actions), les **raccourcis clavier**, le
**thème** et les **réglages**.

Tout sous-système s'« accroche » au Shell : il déclare des panneaux, des commandes et des
raccourcis, que le Shell affiche et route.

## Responsabilités
- Fenêtre + boucle + thème (réutilise le pattern GUI de Nkoung : `NkoungFrame`-like).
- **Docking / layout** des panneaux ; onglets ; barre de statut.
- **Palette de commandes** + registre d'actions + raccourcis.
- Réglages utilisateur + persistance de session (panneaux/onglets ouverts).

## Dépend de
NKWindow, NKEvent, **NKUI**, **NKCanvas**, NKFont, NKFileSystem (réglages).

[Architecture](../../../ARCHITECTURE.md) · [Roadmap](../../../ROADMAP.md)
