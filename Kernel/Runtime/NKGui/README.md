# NKGui

**La couche d'interface de Nkentseu.** Remplace `NKUI` (dépréciée le 16/08/2026),
avec des **noms 100 % Nkentseu** (aucun identifiant dérivé d'ImGui) et un
paradigme **immédiat**.

> Construit à partir de l'**étalon vivant** `Applications/ImGuiRef` (le vrai Dear
> ImGui qui tourne sur Nkentseu) : on observe le comportement de référence, puis on
> le **réécrit à notre manière**.

- Conception : [`ARCHITECTURE.md`](ARCHITECTURE.md) · Chantier en cours et
  inventaire mesuré : [`ROADMAP.md`](ROADMAP.md).

## État au 2026-08-18 : **en service**, pas un squelette

> ⚠️ **Ce README a menti pendant deux mois.** Il annonçait « Phase 1 — squelette,
> widgets ⏳ » alors que `Splitter` existait depuis le **26/06**. Un agent qui a
> fait la vérification de deux minutes que le corpus prescrit s'est vu répondre
> *que la chose n'existait pas* — et l'a réécrite de son côté. **Une documentation
> périmée ne ralentit pas l'adoption : elle la refuse.** D'où les chiffres datés
> ci-dessous plutôt qu'un état d'avancement.

Mesuré sur cette branche : **9 051 lignes**, **116 fonctions** déclarées dans
`NkGuiWidgets.h`, **15 projets dépendants**.

| Domaine | État |
|---|---|
| Liste de dessin (`NkGuiDrawList`) | rect plein/contour **arrondis**, cercle plein/contour, ligne, polyligne, polygone convexe, dégradés, image, texte |
| Widgets | boutons, cases, saisie (mono/multi-ligne), listes, arbres, tables, onglets, menus, popups, sélecteur de couleur, sliders, séparateurs… |
| Fenêtres | ancrage (docking), onglets, redimensionnement, **routeur d'entrée par couches** (`NkInputLayerScope`) |
| Glisser-déposer | charge typée, fantôme dessiné par la bibliothèque (`BeginDragSource`, `AcceptDragPayload`) |
| Mise en page | curseur immédiat, HBox, grille, flex (`Row`/`Column`), pile, **et placement explicite** (`SetNextItemRect`) |
| Thème | **35 jetons énumérables** par nom (`NkGuiThemeTokens`), hook de re-skin par widget (`styleFn`) |
| Icônes | jeu à **poignée opaque**, deux sources : atlas rasterisé **et** contours vectoriels (`NkGuiIcons.h`) |
| Backends | découplés — `NkGuiRHIBackend` (3D) / `NkGuiCanvasBackend` (2D) |

**Ce qui n'existe pas** : le mode **retenu** (façon Qt/Unity), annoncé un temps
comme un second paradigme. Aujourd'hui NKGui est **immédiat seul**.

## Deux règles de conception, à connaître avant d'ajouter quoi que ce soit

**1. NKGui ne connaît aucun vocabulaire d'application.** Pas d'`enum` d'icônes,
pas de nom de panneau. Un jeu d'icônes se remplit par l'application et se consomme
par **poignée opaque** — parce que ce vocabulaire devra survivre à la déclaration
de **NkUIDesign**, qui dessinera et éditera les icônes.

**2. Un composant doit pouvoir être DÉCRIT, pas seulement appelé** — sinon aucun
éditeur ne pourra le composer ni le sauver. Premier étage livré : le thème
s'énumère (nom, groupe, type, décalage) sans qu'on connaisse un seul nom de champ
à la compilation. Ce qu'on ajoute ici doit rester compatible avec ça.

## Vérifier sans GPU

`Applications/NKGuiDrawTest` — application **console**, aucune fenêtre, aucun
device : elle appelle l'API et **recalcule la géométrie produite**.

```
jenga build --target NKGuiDrawTest --config Debug
./Build/Bin/Debug-Windows/NKGuiDrawTest/NKGuiDrawTest.exe    # 108/108, code 0
```

C'est une application et non un `unittest()` parce que **l'exécution des tests
unitaires est désactivée par la politique du workspace**
(`disableunittestexecution`) : un test Unitest compilerait sans jamais tourner.

## Idiomes

Zéro-STL, mémoire **NKMemory** uniquement (jamais `new`/`delete`), `NkPascalCase`,
rendu via backends (NKCanvas 2D / NKRHI 3D), polices NKFont, images NKImage,
entrée NKEvent, curseur `NkWindow::SetCursor`. Commentaires en français.
