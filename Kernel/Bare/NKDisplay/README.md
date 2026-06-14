# NKDisplay — affichage / framebuffer

> ⚠️ Squelette : pas encore de code. Voir la [ROADMAP](ROADMAP.md) et l'[architecture de la
> couche](../ARCHITECTURE.md).

## Rôle

`NKDisplay` est le pilote qui transforme une zone de mémoire en **image à l'écran**. Sur notre
console, on commence **sans pile GPU** : l'écran est un **framebuffer**, un grand tableau de
pixels que l'on écrit directement en mémoire et que le matériel affiche. NKDisplay obtient ce
framebuffer (taille, format, pas/stride, adresse — via le firmware et NKPMM) et l'expose comme
une **surface dessinable**.

C'est le partenaire naturel du **renderer logiciel de NKCanvas** : tout le moteur 2D (formes,
sprites, texte via NKUI, scènes animées comme les démos NKCanvas) dessine dans ce framebuffer,
**sans pilote GPU**. C'est notre voie d'affichage par défaut, et elle suffit pour un dashboard
et des jeux 2D. Un vrai backend GPU (nouveau backend NKRHI) reste une option **bien plus tard**.

## Responsabilités

- Obtenir le framebuffer (résolution, format pixel, stride, adresse).
- L'exposer comme surface : accès pixel, effacement, présentation (swap/vsync si dispo).
- Servir de **cible au renderer logiciel NKCanvas**.
- Alimenter le backend Bare de `NKWindow` (la « fenêtre » = le framebuffer plein écran).

## Place dans la couche

- **Dépend de** : [NKDriver](../NKDriver/README.md), [NKPMM](../NKPMM/README.md).
- **Alimente** : `NKCanvas` (renderer logiciel) et le backend Bare de `NKWindow`.

[Roadmap du module](ROADMAP.md) · [Architecture](../ARCHITECTURE.md) · [Modules](../README.md)
