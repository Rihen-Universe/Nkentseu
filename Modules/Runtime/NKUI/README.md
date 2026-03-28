# NKUI - Guide Complet de Maintenance et Integration

Ce document explique l architecture de `NKUI`, les responsabilites de chaque fichier, le cycle frame par frame, les points sensibles (focus, input, clipping, scroll, rendu), et une methode de debug pour corriger les bugs sans casser ce qui fonctionne.

## 1. Objectif de NKUI

`NKUI` est une UI immediate-mode:
- Le code UI est execute a chaque frame.
- L etat persistant est conserve dans des stores (ids, focus, animation, scroll, window states).
- Le rendu est sorti en `NkUIDrawList` (vertices + indices + draw commands) puis consomme par un renderer/backend.

Principes de separation:
- `NKUI` ne doit pas connaitre `NKFont` interne.
- `NKUI` ne doit pas connaitre `NKRHI` interne.
- Les adapters (comme `NkUINKRHIBackend`) font la traduction vers le GPU.

## 2. Architecture globale

Flux principal:
1. Remplir `NkUIInputState` depuis les events plateforme.
2. `ctx.BeginFrame(input, dt)`
3. Construire UI (`NkUIWidgets`, `NkUIWindow`, `NkUIMenu`, `NkUIDock`, etc.)
4. `ctx.EndFrame()`
5. Renderer/backend lit `ctx.layers[]` et emet les draw calls.

### Layers de draw

Ordre de rendu attendu:
- `LAYER_BG`
- `LAYER_WINDOWS`
- `LAYER_POPUPS`
- `LAYER_OVERLAY`

Regle critique:
- Un element interactif qui doit rester au dessus d un autre doit etre dans un layer plus haut.
- Ne pas dessiner des widgets normaux dans `OVERLAY` si cela masque popups/menus.

## 3. Etats UI importants

`NkUIContext` porte les etats critiques:
- `hotId`: widget survole.
- `activeId`: widget en interaction (drag/click maintenu).
- `focusId`: focus clavier.
- `currentWindowId`: fenetre en cours.
- `layers[]`: draw lists par couche.

Regles:
- `activeId` est prioritaire pendant un drag.
- Un clic ne doit pas activer plusieurs widgets.
- Les ids doivent etre stables frame a frame.

## 4. IDs et stabilite

Bonnes pratiques:
- Utiliser `"Label##id_unique"` si labels repetes.
- Les layouts doivent etre reinitialises correctement a chaque `Push`.
- Eviter toute dependance a un compteur stale d une frame precedente.

Symptomes d ids instables:
- un click agit sur un autre widget.
- combo/menu qui clignote ou se ferme instant.
- focus qui saute.

## 5. Input capture

Pipeline recommande:
- Widget actif d abord (capture stricte).
- Puis widget hovered.
- Puis fenetre ou background.

Fenetre top-most:
- Les inputs souris doivent aller a la fenetre au front (sauf capture active).
- Eviter propagation sur fenetres derriere.

## 6. Clipping et scissor

Deux niveaux:
1. Clip rect logique dans `NkUIDrawList` (`PushClipRect` / `PopClipRect`).
2. Scissor GPU applique par draw command `NK_CLIP_RECT`.

Regles:
- Toujours equilibrer Push/Pop.
- Les popups doivent avoir leur clip propre.
- Le clip d une fenetre ne doit pas fuir vers la suivante.

## 7. Scroll

Sources de scroll:
- molette (meme sans scrollbar visible si autorise).
- drag du thumb.

Regles:
- clamp sur `[0, maxScroll]`.
- nested scroll: le scope interne consomme d abord la molette.
- espace horizontal avec scrollbar:
  - sans scrollbar: marge gauche == marge droite
  - avec scrollbar verticale: marge droite = marge gauche + largeur scrollbar

## 8. Rendu backend (adapter)

Le backend demo `NkUINKRHIBackend`:
- prend `NkUIContext` en entree,
- convertit en buffers GPU,
- applique viewport + scissor + texture binding,
- emet draw indexed.

Regle anti artefact importante:
- Eviter d ecraser un meme VBO/IBO plusieurs fois par layer dans une frame.
- Preferer upload unique par frame (geometrie concatenee), puis draw avec offsets.

## 9. Fonts, unicode, emoji

Chemin standard:
- UI demande rendu texte via `NkUIFont` abstrait.
- atlas glyphes upload via callback adapter.

Pour unicode/emoji:
- prevoir fallback fonts.
- definir ordre de fallback.
- gerer atlas dynamique et invalidation propre.

## 10. Textures UI

Deux usages:
- primitives UI (solid/gradient).
- images utilisateur (`texId` par draw command texturee).

Bonnes pratiques:
- garder `texId=0` pour la texture blanche fallback.
- binder texture seulement si `texId` change.
- sur Vulkan/DX12, ne pas detruire descriptor set tant que GPU peut encore l utiliser (retire queue/deferred free).

## 11. Debug checklist rapide

Quand un bug UI apparait:
1. Verifier IDs (`label##id`, collisions).
2. Verifier input capture (`activeId`, `focusId`, `hotId`).
3. Verifier draw command count par layer.
4. Verifier clip/scissor effectif.
5. Verifier ordre layers.
6. Verifier upload geometrie backend (pas d ecrasement inter-layer).
7. Verifier texture binding (`texId`, descriptor sets).

## 12. Checklist de validation multi backend

A executer sur OpenGL, Vulkan, DX11, DX12, Software:
- menus ouvrent/ferment correctement.
- combo selection unique.
- scroll vertical/horizontal.
- drag/resize windows.
- clipping contenu fenetre.
- texte lisible et aligne.
- aucun triangle parasite.

## 13. Carte des fichiers NKUI

### Noyau
- `NkUI.h`: include central public.
- `NkUIExport.h`: macros export/import.
- `NkUIMath.h/.cpp`: types et helpers math UI.
- `NkUITheme.h`: palettes, metrics, animation params.

### Etat et input
- `NkUIInput.h`: capture input frame.
- `NkUIContext.h/.cpp`: etat global frame + ids + styles + layers.
- `NkUIAnimation.h/.cpp`: helpers animation.

### Draw et rendu
- `NkUIDrawList.h/.cpp`: primitives, vertices, indices, draw commands.
- `NkUIRenderer.h/.cpp`: renderer CPU/reference.
- `NkUILayout2.h/.cpp`: renderer OpenGL de reference.

### Layout et widgets
- `NkUILayout.h/.cpp`: row/column/grid/scroll/split logic.
- `NkUIWidgets.h/.cpp`: widgets (button, slider, combo, table, etc.).

### Fenetres, menu, docking
- `NkUIWindow.h/.cpp`: window manager, z-order, move/resize, client clipping.
- `NkUIMenu.h/.cpp`: menubar, menu, menu items, popup/modal.
- `NkUIDock.h/.cpp`: docking nodes and dock operations.

### Adapter demo (hors module NKUI)
- `Applications/Sandbox/src/DemoNkentseu/Base04/NkUINKRHIBackend.h/.cpp`
  - backend RHI de demo,
  - interface de texture upload,
  - pipeline/scissor/descriptor/bind.

## 14. Strategie de modification sans regression

Toujours suivre:
1. Changer un seul axe a la fois.
2. Compiler `NKUI` puis `NkUIDemoNKEngine`.
3. Verifier menu + combo + scroll + drag window.
4. Verifier au moins 2 backends avant de continuer.

Commande build utilisee:
```powershell
jenga build --target NkUIDemoNKEngine
```

## 15. Notes pratiques pour futur debug

- Si un element disparait apres clic: verifier layer + clip + scissor.
- Si plusieurs items cliquent en meme temps: verifier id collision et capture.
- Si triangles parasites: verifier backend geometry upload/index offsets.
- Si texte carre blanc: verifier atlas/texture/font fallback.

---

Ce README est volontairement operationnel: il sert de reference de maintenance, pas seulement de presentation.