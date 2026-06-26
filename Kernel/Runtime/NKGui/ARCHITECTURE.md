# NKGui — Architecture

> Framework UI de nouvelle génération de Nkentseu. **Réécriture complète** (noms,
> fichiers, structures **neufs**, aucun identifiant dérivé d'ImGui) à partir de
> l'étalon `Applications/ImGuiRef` (Dear ImGui qui tourne sur Nkentseu). Deux
> paradigmes : **immédiat** (façon ImGui) **et retenu** (façon Qt/Unity).
> Roadmap complète : `Kernel/Runtime/NKUI/ROADMAP_UI_REWRITE.private.md`.

---

## 1. Objectifs / non-objectifs

**Objectifs**
- UX « niveau ImGui/Unity » : déplacer, redimensionner (bords+coins, curseurs),
  replier, agrandir, fermer, docker, onglets — avec une **machine à états
  d'interaction UNIQUE et explicite** (le défaut majeur de l'ancien NKUI).
- **Modulaire** : couches découplées, testables séparément.
- **Deux API sur un seul cœur** : immédiat + retenu (« le retenu rejoue l'immédiat »).
- **Idiomes Nkentseu** : zéro-STL, **NKMemory** (jamais new/delete), `NkPascalCase`,
  rendu via backends (NKCanvas 2D / NKRHI 3D), polices NKFont, images NKImage,
  entrée NKEvent, cross-plateforme.

**Non-objectifs (pour l'instant)**
- Multi-viewport OS (fenêtres ImGui hors de la fenêtre principale) — plus tard.
- Compat avec l'ancien NKUI (on le supprime et on réécrit les dépendants).

---

## 2. Règle de nommage (zéro lien ImGui)

Tout identifiant final est **à nous**. Aucun `Im*`, `ImGui*`, `ImVec*` dans NKGui.
Table de correspondance (concept ImGui → nom NKGui), pour guider la réécriture :

| Concept ImGui            | NKGui                              |
|--------------------------|------------------------------------|
| `ImGuiContext`           | `NkGuiContext`                     |
| `ImGuiID`                | `NkGuiId`                          |
| `ImDrawList`             | `NkGuiDrawList`                    |
| `ImDrawData`             | `NkGuiDrawData`                    |
| `ImGuiIO` / input        | `NkGuiInput`                       |
| `ImGuiStyle`             | `NkGuiStyle` / `NkGuiTheme`        |
| `ImGuiWindow`            | `NkGuiWindow`                      |
| `ImGuiDockNode`          | `NkGuiDockNode`                    |
| `ImGuiTabBar`            | `NkGuiTabBar`                      |
| `ImVec2` / `ImVec4`      | `math::NkVec2` / `math::NkVec4`    |
| `ImU32` couleur          | `math::NkColor`                    |
| `ImGui::Begin/End`       | `nkgui::Begin/End`                 |
| `ImGui::Button`          | `nkgui::Button`                    |
| `GImGui` (ctx courant)   | `nkgui::GetCurrentContext()`       |

> Les **types géométriques** réutilisent **NKMath** (`NkVec2`, `NkColor`,
> `NkFloatRect`) — c'est l'idiome Nkentseu (NKUI faisait pareil), pas un lien ImGui.

---

## 3. Architecture en couches

```
┌──────────────────────────────────────────────────────────────┐
│  Backends      Render (NKCanvas 2D / NKRHI 3D) · Input (NKEvent)│  Phase 7
│                Font (NKFont) · Image (NKImage) · Curseur (NKWindow)│
├──────────────────────────────────────────────────────────────┤
│  Retained      arbre de widgets persistant + data-binding +    │  Phase 6
│                événements → REJOUE l'immédiat                   │
├──────────────────────────────────────────────────────────────┤
│  Immediate     Begin/End, fenêtres, widgets, layout, docking   │  Phases 3-5
├──────────────────────────────────────────────────────────────┤
│  Window/Dock   NkGuiWindow, NkGuiDockNode, NkGuiTabBar         │  Phases 4-5
├──────────────────────────────────────────────────────────────┤
│  Widgets/Layout  boutons, sliders, input, table, menu, …       │  Phase 3
├──────────────────────────────────────────────────────────────┤
│  Core          NkGuiContext · NkGuiDrawList · NkGuiInput ·      │  Phase 2
│                NkGuiId · NkGuiInteraction (machine à états)     │
├──────────────────────────────────────────────────────────────┤
│  Foundation    NKMath · NKMemory · NKContainers · NKCore       │
└──────────────────────────────────────────────────────────────┘
```

Arborescence du module :
```
Kernel/Runtime/NKGui/
  NKGui.jenga · ARCHITECTURE.md · README.md
  src/NKGui/
    NKGui.h                 ← parapluie (API publique)
    NkGuiExport.h           ← macros export (défaut STATIQUE)
    Core/      NkGuiTypes.h NkGuiId.h NkGuiDrawList.* NkGuiContext.* NkGuiInput.h NkGuiInteraction.h
    Layout/    (Phase 3)
    Widgets/   (Phase 3)
    Window/    (Phase 4)
    Dock/      (Phase 5)
    Retained/  (Phase 6)
    Backend/   (Phase 7 : interfaces de rendu/entrée)
```

---

## 4. Cœur (Core) — primitives PROPRES

- **`NkGuiId`** : identifiant de widget (hash FNV-1a), pile d'ID avec scoping
  (`PushId`/`PopId`), dérivation par chaîne/pointeur/entier.
- **`NkGuiDrawList`** : liste de dessin vectorielle (rect, ligne, triangle,
  polyligne, cercle, arc, bézier, texte, image, **clip stack**, coins arrondis).
  Sortie = sommets/indices indépendants du backend.
- **`NkGuiContext`** : état par instance (multi-contexte possible). Contient les
  IDs (hot/active/focus), les couches de draw lists, le thème, l'input, l'état
  des fenêtres/dock, les piles (id/clip/style/layout). Un **contexte courant**
  (`SetCurrentContext`/`GetCurrentContext`) permet l'API immédiate terse.
- **`NkGuiInput`** : état d'entrée par frame (souris pos/boutons/molette, clavier,
  texte, modificateurs, dt) — alimenté par le backend NKEvent.
- **`NkGuiInteraction`** : ⭐ **LA machine à états** qui résout l'ambiguïté
  actuelle. À chaque frame, un **seul** mode actif, explicite :

  ```
  enum class NkGuiInteract {
      None, HoverWidget, EditWidget,
      MoveWindow, ResizeWindow,   // + bord/coin concerné
      DragSplitter,               // + quel séparateur
      DragTab, DockTarget         // + nœud/direction visés
  };
  ```
  Règle d'or : **les zones de préhension sont disjointes et priorisées** (bord/coin
  de resize > barre de titre/onglet pour move > contenu), chacune avec son
  **curseur** (`NkWindow::SetCursor`, déjà en place) et son **affordance visuelle**.
  → Plus jamais « je voulais redimensionner et ça a docké ».

---

## 5. API immédiate (paradigme 1)

Fonctions libres dans `nkgui`, sur le **contexte courant** (terse) ou explicite :

```cpp
nkgui::SetCurrentContext(&ctx);
nkgui::NewFrame(input, dt);
  nkgui::DockSpaceFullscreen();                 // dockspace plein écran
  if (nkgui::Begin("Inspecteur", &open)) {
      nkgui::Text("Position");
      nkgui::SliderFloat("X", &x, -100, 100);
      if (nkgui::Button("Reset")) x = 0;
  }
  nkgui::End();
nkgui::Render();                                // produit NkGuiDrawData
backend.Submit(nkgui::GetDrawData());           // NKCanvas / NKRHI
```

Performant, sans allocation par frame (NKMemory + buffers réutilisés).

---

## 6. API retenue (paradigme 2) — « le retenu rejoue l'immédiat »

Un **arbre de widgets persistants** (objets avec propriétés, styles, data-binding,
callbacks). Chaque frame, l'arbre **émet les appels immédiats** correspondants :

```cpp
auto* panel = root.Add<NkGuiPanel>("Inspecteur");
auto* sx    = panel->Add<NkGuiSlider>("X")->Range(-100,100)->Bind(&x);
panel->Add<NkGuiButton>("Reset")->OnClick([&]{ x = 0; });
// boucle :
retained.Render();   // parcourt l'arbre -> appels nkgui::* immédiats
```

- **Un seul moteur** (l'immédiat) → zéro duplication de widgets.
- Le retenu gère : **cycle de vie** (NKMemory), **diff d'événements**, **layout
  déclaratif**, **data-binding** (propriété ↔ variable), **styles persistants**.
- Variante future possible : retenu pur avec reconciliation (option, §7 roadmap).

> Avantage : un éditeur (NKCode/Nogee) peut mélanger panneaux immédiats (outils
> rapides) et UI retenue (formulaires/inspecteurs data-bindés).

---

## 7. Backends (Phase 7)
- **Rendu** : interface `NkGuiRenderer` ; impls `NkGuiCanvasBackend` (NKCanvas 2D,
  réutilise la logique de NkImGuiCanvasBackend) et `NkGuiRHIBackend` (NKRHI 3D).
- **Entrée** : `NkGuiNkEventBackend` (NKEvent → NkGuiInput), réutilise la glue
  écrite pour ImGuiRef (mouse/clé/texte/molette/curseur).
- **Polices** : NKFont (atlas) ; **images** : NKImage ; **curseur** :
  `NkWindow::SetCursor` (déjà ajouté).

---

## 8. Phases (résumé ; détail dans la roadmap)
0. ✅ Étalon ImGui vivant (`ImGuiRef`).
1. **Archi + squelette NKGui (CE DOC).**
2. Core : Id, DrawList, Context, Input, **Interaction (machine à états)**.
3. Layout + **tous** les widgets (parité ImGui).
4. Fenêtres + interaction (move/resize/collapse/maximize/close, curseurs).
5. Docking complet (dock tree, central node, splitters, onglets, boussole
   compacte intérieure + bords, persistance).
6. **Mode retenu**.
7. Backends (NKCanvas/NKRHI/NKEvent/NKFont/NKImage).
8. Thèmes, animations, nav clavier/manette, perf, tests golden-image.
9. Bascule : suppression de NKUI + réécriture des dépendants (NKEditorKit, Nogee,
   Nkoung) sur NKGui.

Validation à chaque phase : **build vert + capture/vidéo + comparaison à `ImGuiRef`**.

---

## 9. Décisions actées / ouvertes
- **Actées** : nom `NKGui`/`nkgui::` ; cœur immédiat + couche retenue qui le rejoue ;
  types géométriques via NKMath ; backends séparés du cœur.
- **Ouvertes** : périmètre parité (tables/plots avancés tout de suite ?), priorité
  rendu (NKCanvas d'abord), nav manette, persistance layout (format JSON ?).
