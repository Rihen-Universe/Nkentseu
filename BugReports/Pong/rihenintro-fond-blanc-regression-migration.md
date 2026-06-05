# Pong — Fond de RihenIntro plus blanc (régression migration NKCanvas)

- **Catégorie** : Pong (application) — régression
- **Sévérité** : moyenne (cosmétique, tous backends)
- **Date** : 2026-06-05
- **Statut** : **ouvert**

## Symptôme
La scène d'intro **RihenIntro** a un fond **sombre** (gris ~0.1) alors qu'il
devrait être **blanc** (le logo Rihen est conçu sur fond blanc). Visible sur
**tous les backends** (constaté sur Vulkan une fois le rendu réparé).

## Cause racine
**Régression de la migration directe Pong → NKCanvas.** Le modèle de frame a été
nettoyé : chaque scène ne fait plus `r.Clear(couleur)` ; c'est désormais
**`PongApp::Render`** qui clear **une seule fois** :
```cpp
mTarget->Clear(theme::Dark());   // pour TOUTES les scènes
```
Or, avant la migration, **RihenIntro faisait son propre `r.Clear(blanc)`**. Ce
clear par-scène a été retiré → RihenIntro hérite du `theme::Dark()` global.

## Solutions possibles
1. **(Recommandé) Couleur de fond par scène.** Ajouter
   `virtual NkColor Scene::BackgroundColor() const { return theme::Dark(); }`,
   override dans `RihenIntroScene` (→ blanc). Dans `PongApp::Render` :
   ```cpp
   NkColor bg = theme::Dark();
   if (Scene* top = mScenes.Top()) bg = top->BackgroundColor();
   mTarget->Clear(bg);
   ```
   Propre, extensible (NogeIntro etc. peuvent aussi personnaliser).
2. **(Rapide)** RihenIntro dessine un quad plein-écran blanc en 1er dans
   `OnRender` (`r.DrawFilledRect({0,0,W,H}, blanc)`). Localisé mais moins propre.

## Liens
- `Applications/Pong/src/Pong/Game/PongApp.cpp` (Render → Clear centralisé)
- `Applications/Pong/src/Pong/UI/Scenes/RihenIntroScene.cpp`
- `Applications/Pong/src/Pong/UI/Scene.h` (ajout BackgroundColor)
