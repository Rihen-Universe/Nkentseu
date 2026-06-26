# NKGui

**Framework UI nouvelle génération de Nkentseu** — réécriture complète de l'UI
(remplace `NKUI`), avec **noms 100 % Nkentseu** (aucun identifiant dérivé d'ImGui)
et **deux paradigmes** : **immédiat** (façon ImGui) **et retenu** (façon Qt/Unity).

> Construit à partir de l'**étalon vivant** `Applications/ImGuiRef` (le vrai Dear
> ImGui qui tourne sur Nkentseu) : on observe le comportement de référence, puis on
> le **réécrit à notre manière**, plus modulaire et complet.

- Conception : [`ARCHITECTURE.md`](ARCHITECTURE.md).
- Roadmap (locale, non poussée) : `Kernel/Runtime/NKUI/ROADMAP_UI_REWRITE.private.md`.

## État : Phase 1 — squelette
- ✅ Macros d'export (défaut statique), types fondamentaux (`NkGuiId`, alias NKMath,
  **machine à états `NkGuiInteract`**), `NkGuiContext` (stub), parapluie `NKGui.h`.
- ⏳ Phase 2 : cœur (DrawList, Context complet, Input, Interaction).
- ⏳ Phases 3-6 : widgets, fenêtres/interaction, docking, mode retenu.
- ⏳ Phases 7-9 : backends (NKCanvas/NKRHI/NKEvent), thèmes/tests, bascule
  (suppression NKUI + réécriture des dépendants).

## Idiomes
Zéro-STL, mémoire **NKMemory** uniquement (jamais new/delete), `NkPascalCase`,
rendu via backends (NKCanvas 2D / NKRHI 3D), polices NKFont, images NKImage,
entrée NKEvent, curseur `NkWindow::SetCursor`.
