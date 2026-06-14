# NKDisplay — Roadmap

> Le module qui met enfin une image à l'écran (Phase 3). ⬜ à faire · 🟡 en cours · ✅ fait.

## Jalon 1 — framebuffer brut (Phase 3)
- ⬜ Obtenir le framebuffer (résolution, format, stride, adresse) via firmware + NKPMM.
- ⬜ Écrire un pixel ; effacer l'écran d'une couleur.
- ⬜ Dessiner un rectangle plein.
- 🎯 Premier visuel : un rectangle de couleur à l'écran.

## Jalon 2 — surface pour NKCanvas
- ⬜ Exposer le framebuffer comme **surface dessinable** standard.
- ⬜ Brancher le **renderer logiciel NKCanvas** dessus.
- ⬜ Présentation (double buffering si la mémoire le permet).
- 🎯 Une scène 2D NKCanvas (formes + texte NKUI) s'affiche.

## Jalon 3 — intégration System
- ⬜ Alimenter le **backend Bare de NKWindow** (fenêtre = plein écran).
- ⬜ Gestion de la résolution / du redimensionnement logique.
- ⬜ Synchronisation verticale (vsync) si disponible.

## Plus tard
- ⬜ Plusieurs résolutions / sorties.
- ⬜ Accélération matérielle (option : futur backend NKRHI GPU).
- ⬜ Affichage de la vraie carte (vs framebuffer QEMU).

[← Module](README.md) · [Roadmap globale](../ROADMAP.md)
