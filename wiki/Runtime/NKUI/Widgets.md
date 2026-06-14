# Widgets, menus et fenêtres

> Couche **Runtime** · NKUI · Construire une interface en **mode immédiat** : les widgets
> (`NkUI`), les menus déroulants et popups (`NkUIMenu`), les fenêtres flottantes (`NkUIWindow`).

Une interface d'éditeur ou d'outil, c'est des **boutons, des champs, des listes, des fenêtres** —
et la question n'est pas tant « comment les dessiner » que « comment décrire en code, chaque frame,
ce qu'on veut voir ». NKUI répond par le paradigme **immédiat-mode** : on ne construit **pas** un
arbre d'objets `Button`/`Window` qu'on garderait en mémoire et qu'on synchroniserait à la main.
À la place, on **appelle une fonction par widget, à chaque frame**, dans l'ordre où on veut le voir,
et cette fonction fait tout d'un coup : elle calcule son rectangle (via le `NkUILayout`), gère
l'interaction souris/clavier, dessine via le `NkUIDrawList`, et **retourne le résultat** (le bouton
a-t-il été cliqué ? la valeur a-t-elle changé ?). L'état que vous gardez, c'est **votre état métier**
— le `bool` coché, le `float` du volume, le `char[]` du nom — pas l'UI elle-même.

Ce n'est **pas** un toolkit retenu (retained-mode) à la Qt ou WinForms : il n'y a pas de
`widget.SetVisible(true)`, pas de callbacks à brancher, pas d'objets à détruire. Si vous n'appelez
plus `NkUI::Button(...)` cette frame, le bouton **n'existe plus** — il suffit de ne pas l'appeler.
C'est le même esprit que Dear ImGui : décrire l'UI comme on écrit du texte, du haut vers le bas.

Toutes les méthodes des trois structs d'API (`NkUI`, `NkUIMenu`, `NkUIWindow`) sont **`static` et
`noexcept`** : ce sont des **namespaces de fonctions**, pas des objets à instancier. Presque tout
widget reçoit en tête le **contexte** `NkUIContext& ctx` (l'état partagé de l'UI : souris, focus,
thème, frame courante) et la **pile de layout** `NkUILayoutStack& ls` (qui place chaque item), et le
plus souvent aussi le **draw list** `NkUIDrawList& dl` (où s'accumulent les commandes de dessin) et
la **police** `NkUIFont& font`.

- **Namespace** : `nkentseu::nkui`
- **Headers** : `#include "NKUI/NkUIWidgets.h"`, `"NKUI/NkUIMenu.h"`, `"NKUI/NkUIWindow.h"`

---

## Décrire un widget : la boucle immédiate

Le geste fondamental se répète à l'identique pour tous les widgets : on appelle, on lit le retour,
on agit. Un bouton renvoie `true` **la frame où il est pressé** ; une checkbox prend une référence
`bool&` qu'elle modifie et renvoie `true` **si elle a changé** ; un slider prend une référence
`float&`/`int32&` qu'il édite en place. Vous n'« écoutez » jamais un événement : vous **regardez la
valeur de retour** dans le flot normal du code.

```cpp
if (NkUI::Button(ctx, ls, dl, font, "Lancer la simulation")) {
    sim.Start();                       // true seulement la frame du clic
}

bool wireframe = renderer.wireframe;
if (NkUI::Checkbox(ctx, ls, dl, font, "Fil de fer", wireframe))
    renderer.SetWireframe(wireframe);  // true seulement si l'utilisateur a basculé

NkUI::SliderFloat(ctx, ls, dl, font, "Exposition", &camera.exposure, 0.f, 4.f);
// camera.exposure est édité en place, frame après frame
```

Ce n'est **pas** un système de signaux : il n'y a rien à connecter ni à déconnecter, et l'ordre des
appels **est** l'ordre d'affichage. C'est aussi pourquoi l'**identité** d'un widget se déduit de son
**label** : deux boutons « OK » dans le même conteneur seraient confondus. On lève l'ambiguïté avec
la convention `"Label##id"` — seul « Label » s'affiche, « id » sert d'identité unique — et `"##id"`
seul donne un widget **sans étiquette visible**. Les helpers `NkParseLabel` (qui sépare les deux
moitiés) et `NkLabelToID` (qui calcule l'ID) formalisent cette règle.

> **En résumé.** Un widget = un appel par frame qui calcule, interagit, dessine et **retourne** le
> résultat. On garde son **état métier** (`bool&`, `float&`, `char*`), jamais l'UI. L'identité vient
> du label ; `"Texte##id"` montre « Texte » et identifie par « id », `"##id"` masque l'étiquette.

---

## Empiler, aligner, contraindre : le layout

Par défaut, les items se posent **les uns sous les autres** : NKUI gère un curseur de placement dans
la `NkUILayoutStack`. Pour sortir de cet empilement vertical, on ouvre des **régions** par paires
`Begin…/End…` : `BeginRow`/`EndRow` aligne horizontalement, `BeginColumn`/`EndColumn` reprend la
verticale dans une largeur donnée, `BeginGroup`/`EndGroup` encadre un bloc (avec bordure et titre
optionnels), `BeginGrid`/`EndGrid` dispose en cellules régulières. Pour le défilement et le
redimensionnement, `BeginScrollRegion`/`EndScrollRegion` (avec des `float*` de scroll optionnels) et
`BeginSplit…`/`EndSplit` (panneaux séparés par une poignée, ratio ajustable).

```cpp
NkUI::BeginRow(ctx, ls);                                   // aligner sur une ligne
  if (NkUI::Button(ctx, ls, dl, font, "Ouvrir")) { /* … */ }
  NkUI::SameLine(ctx, ls);                                 // coller le suivant à droite
  if (NkUI::Button(ctx, ls, dl, font, "Fermer")) { /* … */ }
NkUI::EndRow(ctx, ls);
```

La position fine se règle avec les **contraintes du prochain item** : `SetNextWidth`/`SetNextHeight`
en pixels, `SetNextWidthPct` en pourcentage, `SetNextGrow` pour remplir l'espace restant ;
`SameLine` remet le curseur sur la même ligne, `NewLine` force une nouvelle, `Spacing` insère un vide
et `Separator` trace un trait. Ces appels n'affectent **que l'item suivant**.

> **En résumé.** Le layout empile verticalement par défaut. On compose avec les paires
> `BeginRow/EndRow`, `BeginColumn`, `BeginGroup`, `BeginGrid`, `BeginScrollRegion`, `BeginSplit…`,
> et on ajuste au cas par cas via `SetNext…`, `SameLine`, `NewLine`, `Spacing`, `Separator`.

---

## Menus, popups, modaux

Les menus sont une famille à part (`NkUIMenu`) car ils s'affichent **par-dessus tout** (layer
popups) et persistent entre les frames : un menu reste ouvert tant qu'on n'a pas cliqué ailleurs.
On retrouve la barre de menus (`BeginMenuBar`/`EndMenuBar`), les menus déroulants
(`BeginMenu`/`EndMenu`), les items (`MenuItem`, avec raccourci et coche optionnels), les menus
contextuels au clic droit (`OpenContextMenu` puis `BeginContextMenu`/`EndContextMenu`), les popups
programmatiques (`OpenPopup`/`BeginPopup`/`EndPopup`) et les boîtes **modales** qui bloquent le reste
de l'UI (`OpenModal`/`BeginModal`/`EndModal`).

```cpp
if (NkUIMenu::BeginMenuBar(ctx, dl, font, {0, 0, W, 25})) {
    if (NkUIMenu::BeginMenu(ctx, dl, font, "Fichier")) {
        if (NkUIMenu::MenuItem(ctx, dl, font, "Ouvrir", "Ctrl+O")) Open();
        NkUIMenu::Separator(ctx, dl);
        if (NkUIMenu::MenuItem(ctx, dl, font, "Quitter")) Quit();
        NkUIMenu::EndMenu(ctx);
    }
    NkUIMenu::EndMenuBar(ctx);
}
```

> **En résumé.** `NkUIMenu` couvre barre, menus déroulants, items, menus contextuels, popups et
> modaux ; tout s'affiche au-dessus de l'UI et chaque `Begin…` ouvert se referme par son `End…`.

---

## Les fenêtres flottantes

`NkUIWindow` ajoute les **fenêtres** déplaçables, redimensionnables, repliables — l'ossature d'un
éditeur à panneaux. Contrairement aux widgets purement immédiats, une fenêtre a un **état
persistant** (position, taille, ordre Z, repli) stocké dans un `NkUIWindowState`, lui-même géré par
un `NkUIWindowManager`. Ce gestionnaire est le **seul objet à durée de vie** de NKUI : on
l'initialise une fois (`Init()`) et on le détruit en fin de vie (`Destroy()`).

```cpp
NkUIWindowManager wm;
wm.Init();                                 // une fois
// chaque frame :
if (NkUIWindow::Begin(ctx, wm, dl, font, ls, "Inspecteur")) {
    NkUI::Text(ctx, ls, dl, font, "Propriétés de l'objet");
    // … widgets du panneau …
}
NkUIWindow::End(ctx, wm, dl, ls);
// fin de vie :
wm.Destroy();                              // paire de Init()
```

Le comportement d'une fenêtre se règle par des **flags binaires** combinables (`NkUIWindowFlags`) :
pas de barre de titre, pas de redimensionnement, toujours au-dessus, modale, auto-dimensionnée, etc.

> **En résumé.** `NkUIWindow` gère des fenêtres à état persistant via un `NkUIWindowManager` (paire
> `Init()`/`Destroy()`). On encadre le contenu par `Begin`/`End` et on règle le comportement par les
> flags `NkUIWindowFlags` combinés au `|`.

---

## Aperçu de l'API

Tous les éléments publics des trois headers, en un coup d'œil. Chacun est détaillé dans la
« Référence complète » qui suit.

### `NkUIWidgets.h` — helpers, options, widgets

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Helpers label | `NkUILabelParts`, `NkParseLabel`, `NkLabelToID` | Séparer `"Label##ID"` ; calculer l'ID unique |
| Enum | `NkUISliderValuePlacement` | Où afficher la valeur d'un slider |
| Options | `NkUISliderValueOptions`, `NkUITextInputOptions`, `NkUIListBoxOptions` | Réglages fins slider / saisie texte / liste |
| Layout | `BeginRow`/`EndRow`, `BeginColumn`/`EndColumn`, `BeginGroup`/`EndGroup`, `BeginGrid`/`EndGrid`, `BeginScrollRegion`/`EndScrollRegion`, `BeginSplit`/`BeginSplitPane2`/`EndSplit` | Régions de placement par paires |
| Contrainte | `SetNextWidth`, `SetNextHeight`, `SetNextWidthPct`, `SetNextGrow`, `SameLine`, `NewLine`, `Spacing`, `Separator` | Régler le prochain item / la ligne |
| Texte | `Text`, `TextSmall`, `TextColored`, `TextWrapped`, `LabelValue` | Étiquettes et libellés |
| Boutons | `Button`, `ButtonSmall`, `ButtonImage`, `InvisibleButton` | Actions ponctuelles |
| Bascules | `Checkbox`, `RadioButton`, `Toggle` | Booléen / choix exclusif / interrupteur |
| Sliders | `SliderFloat`, `SliderInt`, `SliderFloat2`, `DragFloat`, `DragInt` | Réglage continu par glissement |
| Saisie | `InputText`, `InputInt`, `InputFloat`, `InputMultiline` | Champs éditables |
| Listes | `Combo`, `ListBox` | Sélection dans une liste d'items |
| Progression | `ProgressBar`, `ProgressBarVertical`, `ProgressBarCircular` | Barres / cercle d'avancement |
| Couleur | `ColorButton`, `ColorPicker` | Pastille / sélecteur de couleur |
| Arbre | `TreeNode`, `TreePop` | Nœuds dépliables |
| Table | `BeginTable`, `TableHeader`, `TableNextRow`, `TableNextCell`, `EndTable` | Tableaux à colonnes |
| Tooltip | `Tooltip`, `SetTooltip` | Info-bulle immédiate / différée |
| Dessin | `DrawWidgetBg`, `DrawLabel`, `CalcState`, `FlushPendingTooltip` | Helpers bas niveau de rendu/état |

### `NkUISliderValuePlacement` — placement de la valeur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeurs | `NK_ABOVE_THUMB`, `NK_BELOW_THUMB` | Au-dessus / en dessous du curseur |
| Valeurs | `NK_LEFT_OF_SLIDER`, `NK_RIGHT_OF_SLIDER` | À gauche / à droite du slider |
| Valeurs | `NK_IN_LABEL`, `NK_HIDDEN` | Dans l'étiquette / masquée |

### `NkUIMenu.h` — menus, popups, modaux

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| État | `NkUIMenuState` | État persistant d'un menu déroulant |
| MenuBar | `BeginMenuBar`, `EndMenuBar` | Barre de menus |
| Menu | `BeginMenu`, `EndMenu` | Menu déroulant |
| Items | `MenuItem`, `Separator` | Entrée (raccourci/coche) / trait |
| Contextuel | `OpenContextMenu`, `BeginContextMenu`, `EndContextMenu` | Menu au clic droit |
| Popup | `OpenPopup`, `ClosePopup`, `BeginPopup`, `EndPopup` | Popup par ID |
| Modal | `OpenModal`, `BeginModal`, `EndModal` | Dialogue bloquant |
| État interne | `CloseAllMenus`, `IsAnyMenuOpen` | Fermer tout / interroger |

### `NkUIWindow.h` — fenêtres

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkUIWindowFlags` (16 flags), `operator\|`, `HasFlag` | Comportement de fenêtre, combinable |
| État | `NkUIWindowState` | État d'une fenêtre (pos, taille, Z, repli, dock) |
| Gestionnaire | `NkUIWindowManager` (`Init`, `Destroy`, `FindOrCreate`, `Find`, `BringToFront`, `SortZOrder`, `HitTest`, `BeginFrame`, `EndFrame`) | Stocke et ordonne jusqu'à 64 fenêtres |
| API principale | `Begin`, `End` | Encadrer le contenu d'une fenêtre |
| Helpers | `SetNextWindowPos/Size`, `GetWindowPos/Size`, `IsWindowFocused/Hovered`, `SetWindowPos/Size`, `SetWindowCollapsed`, `CloseWindow` | Interroger et piloter une fenêtre |
| Rendu | `RenderAll` | Afficher toutes les fenêtres d'un coup |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines du temps réel.

### Helpers de label : `NkUILabelParts`, `NkParseLabel`, `NkLabelToID`

En immédiat-mode, **l'identité d'un widget vient de son label** — c'est ainsi que NKUI sait, d'une
frame à l'autre, que « ce » bouton est bien le même (pour suivre le survol, le focus, l'état pressé).
La convention `"Label##ID"` règle deux besoins en un : ce qui précède `##` est **affiché**, ce qui
suit sert d'**identité** invisible. `NkParseLabel(raw)` éclate la chaîne brute en un `NkUILabelParts`
(`char label[256]` visible + `char id[128]` caché) ; `NkLabelToID(ctx, raw)` calcule directement le
`NkUIID` unique. Ce sont des **fonctions libres** du namespace, pas des méthodes de `NkUI`.

- **Outils / éditeur** — afficher la même étiquette « Position » sur trois axes (X, Y, Z) sans
  collision : `"Position##posX"`, `"Position##posY"`, `"Position##posZ"`.
- **UI générée** — quand on instancie des widgets dans une boucle (une ligne par entité), suffixer
  l'index garantit une identité distincte : `"Supprimer##" + i`.
- **Sans étiquette** — `"##slider"` rend un widget muet (utile sous un libellé déjà dessiné à part).

### `NkUISliderValuePlacement` et `NkUISliderValueOptions` — afficher la valeur d'un slider

Un slider montre une **valeur courante** ; reste à décider **où**. L'enum `NkUISliderValuePlacement`
(scope `NkUISliderValuePlacement::`) propose six emplacements : `NK_ABOVE_THUMB` (au-dessus du
curseur), `NK_BELOW_THUMB` (dessous), `NK_LEFT_OF_SLIDER` / `NK_RIGHT_OF_SLIDER` (sur les côtés),
`NK_IN_LABEL` (intégrée à l'étiquette) et `NK_HIDDEN` (cachée). On passe le tout via un
`NkUISliderValueOptions` optionnel : `placement`, un `offset` final en pixels, un `gap` d'espacement,
et `followThumb` (si vrai, les modes above/below **suivent le curseur** ; sinon la valeur reste
centrée).

- **Éditeur de paramètres** — un panneau de réglages compact préfère `NK_IN_LABEL` (« Volume : 0.8 »)
  pour économiser la place verticale.
- **Mixage audio / gameplay** — une console de mixage met la valeur `NK_ABOVE_THUMB` avec
  `followThumb = true` : le chiffre colle au doigt, comme sur une vraie table.
- **Affichage minimal** — `NK_HIDDEN` quand seule la position importe (une barre de teinte, un réglage
  purement visuel).

### `NkUITextInputOptions` — régler la saisie texte

Tous les champs texte (`InputText`, `InputInt`, `InputFloat`, `InputMultiline`) acceptent un
`NkUITextInputOptions*` optionnel qui gouverne le **comportement clavier**. On y trouve les bascules
`allowTextInput`, `repeatBackspace`, `repeatDelete`, `allowEnterNewLine`, le **rythme de répétition**
des touches maintenues (`repeatDelay` 0.35 s, `repeatRate` 0.05 s), et pour le multiligne
l'interlignage (`multilineLineSpacing`), l'espacement des caractères (`multilineCharSpacing`) et le
retour à la ligne automatique (`multilineWrap`).

- **Champ numérique** — couper `allowEnterNewLine` et garder la répétition de `Backspace` pour une
  saisie rapide de valeurs.
- **Console / chat in-game** — un champ d'une ligne où Entrée **valide** (pas de nouvelle ligne).
- **Éditeur de script / notes** — un `InputMultiline` avec `multilineWrap = true` et l'interlignage
  ajusté pour la lisibilité d'un bloc de texte long.

### `NkUIListBoxOptions` — la sélection multiple

`ListBox` accepte un `NkUIListBoxOptions*` qui débride la **sélection multiple** : `multiSelect`
l'active, `toggleSelection` fait basculer un item au clic, `requireCtrlForMulti` impose la touche Ctrl
pour étendre la sélection (comportement « explorateur de fichiers »), et le masque externe
`bool* selectedMask` + `int32 selectedMaskCount` permet à l'appelant de **posséder** le tableau de
sélection (un booléen par item).

- **Gestionnaire d'assets** — sélectionner plusieurs textures pour une opération groupée (suppression,
  ré-import) via `selectedMask`.
- **Éditeur de scène** — choisir un sous-ensemble d'entités, avec `requireCtrlForMulti` pour coller
  aux habitudes des outils desktop.
- **Liste à choix unique** — laisser `opts = nullptr` (le `int32& selected` standard suffit).

### Layout : régions `Begin…/End…`

Le layout pose les items et délimite des **régions**. Chaque `Begin…` ouvre une zone et **doit** être
fermé par son `End…` correspondant.

- `BeginRow`/`EndRow` (hauteur optionnelle) — aligne les items **horizontalement** : une barre
  d'outils, une rangée de boutons d'action, un trio de champs X/Y/Z.
- `BeginColumn`/`EndColumn` (largeur optionnelle) — une **colonne** de largeur fixe : un volet de
  hiérarchie à gauche d'un éditeur.
- `BeginGroup`/`EndGroup` (label et bordure optionnels) — encadre un **bloc logique** : un groupe de
  réglages « Éclairage » avec son titre et son cadre.
- `BeginGrid`/`EndGrid` (nombre de colonnes, hauteur de cellule) — une **grille régulière** : une
  palette de tuiles, une galerie de vignettes d'assets.
- `BeginScrollRegion`/`EndScrollRegion` (`scrollX`/`scrollY` optionnels) — une zone **défilable** dont
  le contenu dépasse : une liste de logs, un long inspecteur de composants.
- `BeginSplit`/`BeginSplitPane2`/`EndSplit` (ratio ajustable, orientation verticale par défaut) — deux
  panneaux séparés par une **poignée** : viewport + propriétés, code + aperçu.

### Contrainte du prochain item et espacement

Ces appels n'agissent que sur l'item **suivant** ou sur le **curseur de ligne** : `SetNextWidth(w)` /
`SetNextHeight(h)` fixent une taille en pixels, `SetNextWidthPct(p)` une largeur en pourcentage du
conteneur, `SetNextGrow()` fait **remplir l'espace disponible** (un champ qui s'étire jusqu'au bord).
`SameLine(offset, spacing)` ramène le curseur sur la **même ligne** (pour poser deux widgets côte à
côte), `NewLine()` force une nouvelle ligne, `Spacing(px)` insère un **vide** vertical, et
`Separator(dl)` trace un **trait** de séparation. C'est l'outillage de mise en page fine d'un
panneau : aérer des groupes, aligner des paires libellé/valeur, étirer un champ de recherche.

### Texte et libellés

- `Text(…, text, col)` — du texte simple ; `col = {0,0,0,0}` (sentinelle alpha 0) signifie
  « **couleur du thème** ». L'affichage par défaut de toute valeur, de tout libellé statique.
- `TextSmall` — une variante plus petite, pour des annotations discrètes (unités, aides).
- `TextColored(col, text)` — du texte d'une couleur imposée : un message d'erreur en rouge, un statut
  « OK » en vert.
- `TextWrapped(text)` — du texte qui **revient à la ligne** dans la largeur disponible : une
  description, un message d'aide, le contenu d'une boîte d'information.
- `LabelValue(label, value)` — une paire « libellé : valeur » alignée, idiome universel des
  inspecteurs (« FPS : 144 », « Triangles : 1.2M », « Position : 0, 1, 0 »).

### Boutons : `Button`, `ButtonSmall`, `ButtonImage`, `InvisibleButton`

Le bouton est l'**action ponctuelle** par excellence : il renvoie `true` la frame où il est pressé,
et on agit aussitôt dans le `if`.

- `Button(label, size)` — le bouton standard (taille `{0,0}` = auto). Toute commande : « Charger »,
  « Compiler », « Réinitialiser la caméra ».
- `ButtonSmall(label)` — version compacte pour les barres d'outils denses.
- `ButtonImage(texId, size, tint)` — un bouton **dessiné depuis une texture** GPU (`texId`), teintable
  (`tint`). Idéal pour une **palette d'icônes** d'éditeur (déplacer, tourner, échelle), une vignette
  d'asset cliquable, un bouton de gamepad illustré. Note : il ne prend **pas** de `font`.
- `InvisibleButton(id, size)` — une **zone cliquable sans rendu** : capter un clic sur une région
  qu'on dessine soi-même (un canevas, une timeline, une poignée custom). Ne prend ni `dl` ni `font`.

### Bascules : `Checkbox`, `RadioButton`, `Toggle`

Toutes éditent un état en place et renvoient `true` **si la valeur a changé** cette frame.

- `Checkbox(label, bool& value)` — un **booléen** : activer le fil de fer, montrer les colliders,
  couper un son, basculer une option de build.
- `RadioButton(label, int32& selected, int32 value)` — un **choix exclusif** : le bouton se coche si
  `selected == value`, et son clic écrit `value`. Plusieurs radios partageant le même `selected`
  forment un groupe : mode de rendu (Solide / Fil de fer / Normales), type de projection
  (Perspective / Orthographique).
- `Toggle(label, bool& value)` — sémantiquement comme une checkbox, présenté en **interrupteur**
  (style on/off) : activer un panneau, un système, un mode debug.

### Sliders et drags : `SliderFloat`, `SliderInt`, `SliderFloat2`, `DragFloat`, `DragInt`

Le **réglage continu** par glissement. Les sliders bornent entre `vmin` et `vmax` ; les drags
ajustent par déplacement relatif (paramètre `speed`) sur une plage très large par défaut.

- `SliderFloat(label, float& value, vmin, vmax, fmt, width, valueOpts)` — un flottant borné :
  exposition de caméra, intensité d'une lumière, gain audio, vitesse d'un agent, force de gravité.
  `fmt` formate l'affichage (`"%.2f"`), `valueOpts` règle le placement de la valeur (voir plus haut).
- `SliderInt(label, int32& value, vmin, vmax, …)` — un entier borné : nombre d'échantillons, niveau de
  LOD, taille de brosse, nombre d'ennemis.
- `SliderFloat2(label, float* values, vmin, vmax, …)` — **deux composantes** d'un coup (`values`
  pointe sur ≥ 2 flottants) : un offset 2D, une plage min/max, des coordonnées UV.
- `DragFloat(label, float& value, speed, vmin, vmax)` — édite un flottant en **glissant** sur le champ
  (sans rail) : ajuster finement une position dans un inspecteur, un poids de blend d'animation.
- `DragInt(label, int32& value, speed, vmin, vmax)` — l'équivalent entier : un index de frame, un
  compteur, un identifiant.

### Saisie texte : `InputText`, `InputInt`, `InputFloat`, `InputMultiline`

Les champs **éditables**. `InputText` et `InputMultiline` écrivent dans un **buffer fourni par
l'appelant** (`char* buf` de capacité `bufSize`) — c'est vous qui possédez la mémoire. Tous acceptent
un `NkUITextInputOptions*` optionnel (voir plus haut).

- `InputText(label, char* buf, bufSize, width, opts)` — une ligne de texte : nom d'un objet, chemin de
  fichier, champ de recherche, pseudo de joueur.
- `InputInt(label, int32& value, opts)` / `InputFloat(label, float& value, fmt, opts)` — saisir un
  nombre **au clavier** plutôt qu'au slider (valeur précise : une seed, une coordonnée exacte, un
  framerate cible).
- `InputMultiline(label, char* buf, bufSize, size, opts)` — un bloc **multiligne** : un script, une
  description d'asset, des notes de niveau, le corps d'un message.

### Listes : `Combo`, `ListBox`

Choisir un item dans une liste. Les deux prennent un tableau de chaînes
(`const char* const* items`, `numItems`) et écrivent l'index choisi dans `int32& selected`.

- `Combo(label, int32& selected, items, numItems, width)` — une **liste déroulante** compacte :
  choisir un backend de rendu (OpenGL / Vulkan / DX12), une qualité d'ombres, un preset.
- `ListBox(label, int32& selected, items, numItems, visibleCount, opts)` — une liste **toujours
  visible** (`visibleCount` lignes), avec sélection **multiple** possible via `NkUIListBoxOptions` :
  un navigateur d'assets, une liste de calques, une sélection d'entités.

### Progression : `ProgressBar`, `ProgressBarVertical`, `ProgressBarCircular`

Visualiser un **avancement** (`fraction` de 0 à 1), avec un texte `overlay` optionnel.

- `ProgressBar(fraction, size, overlay)` — la barre **horizontale** classique : chargement d'assets,
  cuisson de lightmaps, import d'un modèle, barre de vie. Ne prend pas de `font`.
- `ProgressBarVertical(fraction, size, overlay)` — la même en **vertical** : une jauge de carburant,
  un VU-mètre audio, une barre d'énergie. Pas de `font`.
- `ProgressBarCircular(font, fraction, diameter, overlay)` — un **anneau** d'avancement : un spinner de
  chargement, un cooldown de compétence, un minuteur. Celle-ci **prend** une `font` (pour l'overlay
  centré).

### Couleur : `ColorButton`, `ColorPicker`

Éditer une `NkColor&` en place.

- `ColorButton(label, NkColor& col, size)` — une **pastille** cliquable montrant la couleur courante :
  parfaite en ligne dans un inspecteur (couleur d'une lumière, teinte d'un matériau), ouvrant le
  sélecteur au clic.
- `ColorPicker(label, NkColor& col)` — le **sélecteur complet** (teinte, saturation, valeur) : régler
  une couleur ambiante, la couleur d'un fog, une palette d'éditeur.

### Arbre : `TreeNode`, `TreePop`

`TreeNode(label, bool* open)` rend un **nœud dépliable** et renvoie `true` s'il est **déplié** — auquel
cas, et **seulement** dans ce cas, il faut appeler `TreePop()` après avoir dessiné les enfants (qui ne
prend ni `dl` ni `font`). Le pointeur `open` optionnel permet de piloter l'état d'ouverture de
l'extérieur.

```cpp
if (NkUI::TreeNode(ctx, ls, dl, font, "Transform")) {
    NkUI::SliderFloat(ctx, ls, dl, font, "X", &t.x, -10.f, 10.f);
    // … enfants …
    NkUI::TreePop(ctx, ls);
}
```

- **Hiérarchie de scène** — l'arbre des objets et de leurs enfants, plié/déplié à volonté.
- **Inspecteur de composants** — chaque composant ECS comme un nœud repliable.
- **Explorateur de fichiers** — dossiers et sous-dossiers d'un projet.

### Table : `BeginTable`, `TableHeader`, `TableNextRow`, `TableNextCell`, `EndTable`

Un **tableau à colonnes** suivant l'idiome `Begin/…/End`. On ouvre avec `BeginTable(id, columns,
width)`, on déclare les en-têtes avec `TableHeader(label)`, puis pour chaque ligne `TableNextRow(striped)`
(`striped` = alternance de teintes ; ne prend pas de `font`) et pour chaque cellule `TableNextCell()`
(ni `dl` ni `font`) avant d'y dessiner les widgets, et on referme par `EndTable(dl)` (sans `font`).

- **Profilage / debug** — une table « Système | Temps ms | % » des coûts par frame.
- **Éditeur de données** — une grille de propriétés, une feuille de stats d'unités, une matrice de
  collisions.
- **Réseau / outils** — la liste des joueurs connectés (nom, ping, état) ou des assets (nom, taille,
  type).

### Tooltip : `Tooltip`, `SetTooltip`

L'**info-bulle**. Attention à la signature : ni l'un ni l'autre ne prend `ls`. `Tooltip(ctx, dl, font,
text)` affiche **immédiatement** une bulle (typiquement quand le widget précédent est survolé) ;
`SetTooltip(ctx, text)` **diffère** l'affichage — la bulle n'apparaît qu'en fin de frame, vidée par
`FlushPendingTooltip`. Usage universel : expliquer un bouton d'icône, donner le nom complet d'une
valeur tronquée, afficher l'aide d'un réglage.

### Helpers de dessin : `DrawWidgetBg`, `DrawLabel`, `CalcState`, `FlushPendingTooltip`

Le **bas niveau** sur lequel s'appuient les widgets — utile quand on dessine un widget custom à la
main. `DrawWidgetBg(dl, theme, rect, type, state)` peint le fond d'un widget selon son **type**
(`NkUIWidgetType`) et son **état** (`NkUIWidgetState`) ; `DrawLabel(dl, font, theme, rect, text,
centered)` écrit une étiquette ; `CalcState(ctx, id, rect, enabled)` calcule l'état d'interaction
(survol, actif…) d'une zone à partir de la souris et du focus ; `FlushPendingTooltip(ctx, font)` vide
le tooltip différé par `SetTooltip` en fin de frame.

- **Widget custom** — une jauge, un graphe, une timeline : on calcule soi-même le rect, on appelle
  `CalcState` pour réagir au survol/clic, on dessine avec `DrawWidgetBg`/`DrawLabel` pour rester
  cohérent avec le thème.

### Menus : `NkUIMenu`

Tous les menus s'affichent **par-dessus l'UI** (layer popups) et tiennent un état persistant entre les
frames. `NkUIMenuState` mémorise un menu déroulant : son `id`, le `rect` du bouton déclencheur, le
`panelRect` du panneau, s'il est `open`, l'animation `openAnim`, sa `depth` (0 = barre, 1 = sous-menu…)
et son `itemCount`.

- **MenuBar** — `BeginMenuBar(ctx, dl, font, rect)` ouvre la barre (ex. `{0,0,W,25}`),
  `EndMenuBar(ctx)` la ferme. L'ossature de tout éditeur (Fichier, Édition, Affichage, Aide).
- **Menu déroulant** — `BeginMenu(ctx, dl, font, label, enabled)` / `EndMenu(ctx)` : un menu de la
  barre, ou un **sous-menu** quand on l'imbrique.
- **Items** — `MenuItem(ctx, dl, font, label, shortcut, selected, enabled)` renvoie `true` à
  l'activation ; `shortcut` affiche un raccourci (« Ctrl+S »), et `selected` (optionnel) en fait un
  **item à coche** (afficher/masquer un panneau). `Separator(ctx, dl)` insère un trait entre groupes
  d'items.
- **Menu contextuel** — `OpenContextMenu(ctx, id)` l'ouvre **à la position souris** (sur clic droit),
  puis `BeginContextMenu`/`EndContextMenu` le dessine : clic droit sur une entité, sur un asset, dans
  le viewport.
- **Popup** — `OpenPopup(ctx, id)` / `ClosePopup(ctx)` pilotent par **ID** un panneau flottant
  (`BeginPopup(ctx, dl, font, id, size)` renvoie `true` s'il est ouvert, `EndPopup(ctx, dl)` ferme) :
  un mini-formulaire, une palette de couleurs, une confirmation rapide.
- **Modal** — `OpenModal(ctx, title)` puis `BeginModal(ctx, dl, font, title, open, size)` /
  `EndModal(ctx, dl)` : un dialogue **bloquant** avec overlay (quitter sans sauvegarder ?, réglages de
  projet, import). Le `bool* open` permet une croix de fermeture.
- **État interne** — `CloseAllMenus(ctx)` referme tout (utile sur Échap ou perte de focus),
  `IsAnyMenuOpen(ctx)` indique si un menu capte l'entrée (pour suspendre les raccourcis du viewport).

### Fenêtres : `NkUIWindowFlags`, `NkUIWindowState`, `NkUIWindowManager`, `NkUIWindow`

**`NkUIWindowFlags`** (scope `NkUIWindowFlags::`) est un jeu de **16 flags binaires combinables** au
`|` : `NK_NONE`, `NK_NO_TITLE_BAR`, `NK_NO_RESIZE`, `NK_NO_MOVE`, `NK_NO_SCROLLBAR`,
`NK_NO_SCROLL_WITH_MOUSE`, `NK_NO_COLLAPSE`, `NK_NO_CLOSE`, `NK_NO_BACKGROUND`,
`NK_NO_BRING_TO_FRONT_ON_FOCUS`, `NK_ALWAYS_ON_TOP`, `NK_MODAL`, `NK_AUTO_SIZE`, `NK_CHILD_WINDOW`,
`NK_NO_TABS`, `NK_ALLOW_DOCK_CHILD`, `NK_NO_DOCK`. Les fonctions libres `operator|(a, b)` (combine) et
`HasFlag(flags, f)` (teste) accompagnent l'enum. On compose par exemple un HUD fixe avec
`NK_NO_TITLE_BAR | NK_NO_RESIZE | NK_NO_MOVE | NK_NO_BACKGROUND`.

**`NkUIWindowState`** porte l'état persistant d'une fenêtre : `name`, `id`, `pos`, `size`, contraintes
`minSize`/`maxSize`, `contentSize`, défilement `scrollX`/`scrollY`, `zOrder`, ses `flags`, et les
booléens `isOpen`, `isCollapsed`, `isDocked` (+ `dockNodeId`, `childDockRoot` = -1 si aucun sous-dock,
`isActiveTab`). C'est l'objet qui **survit** entre les frames — il mémorise où l'utilisateur a placé et
dimensionné chaque panneau.

**`NkUIWindowManager`** est le **gestionnaire** : un agrégat à durée de vie utilisateur, stockage fixe
de `MAX_WINDOWS = 64` fenêtres. Il expose ses fenêtres (`windows[]`, `numWindows`), l'ordre Z
(`zOrder[]`), les ID actif/survolé/déplacé/redimensionné, et l'état de drag/resize en cours. Ses
méthodes : `Init()` / `Destroy()` (**paire Create/Destroy obligatoire** — initialiser avant usage,
libérer après), `FindOrCreate(name, pos, size, flags)`, `Find(id)` / `Find(name)`, `BringToFront(id)`,
`SortZOrder()` (plus grand Z = devant), `HitTest(pos, titleBarHeight)` (fenêtre sous le curseur),
`BeginFrame(ctx)` / `EndFrame(ctx)` (réinitialise survol/état en début et fin de frame). À noter : le
manager est passé **explicitement** à l'API (il n'est **pas** rangé dans `NkUIContext` — design
non-intrusif).

**`NkUIWindow`** est l'API `Begin`/`End`. `Begin(ctx, wm, dl, font, ls, name, pOpen, flags)` renvoie
`true` si le contenu doit être dessiné (faux si la fenêtre est repliée/fermée — on saute alors le
contenu mais on appelle quand même `End`). Le `bool* pOpen` câble la **croix de fermeture**.
Les helpers pilotent les fenêtres par nom : `SetNextWindowPos`/`SetNextWindowSize` (avant `Begin`),
`GetWindowPos`/`GetWindowSize`, `IsWindowFocused`/`IsWindowHovered`,
`SetWindowPos`/`SetWindowSize`/`SetWindowCollapsed`, `CloseWindow`. Enfin, `RenderAll(ctx, wm, dl,
font)` affiche **toutes** les fenêtres d'un coup, quand on ne gère pas chaque `Begin`/`End`
manuellement.

- **Éditeur à panneaux** — Hiérarchie, Inspecteur, Console, Assets, chacun dans sa fenêtre repliable
  et redimensionnable ; le manager mémorise la disposition.
- **HUD de jeu** — un panneau de debug `NK_ALWAYS_ON_TOP | NK_NO_BACKGROUND` superposé au rendu.
- **Dialogue modal** — une fenêtre `NK_MODAL` qui bloque l'arrière-plan le temps d'une confirmation.

### Idiomes et pièges

- **Tout est immédiat-mode** : appeler les widgets **chaque frame**, dans l'ordre d'affichage ; ne rien
  retenir hormis l'état métier que vous passez par référence/pointeur.
- **Convention d'ID** : `"Texte##id"` affiche « Texte » et identifie par « id » ; `"##id"` masque
  l'étiquette. Indispensable dès qu'un même libellé se répète.
- **Paires `Begin/End` obligatoires** : layout (`BeginRow`, `BeginColumn`, `BeginGroup`, `BeginGrid`,
  `BeginScrollRegion`, `BeginSplit…`), `BeginTable`/`EndTable`, `TreeNode`(si `true`)/`TreePop`, menus
  (`BeginMenuBar`, `BeginMenu`, `BeginContextMenu`, `BeginPopup`, `BeginModal`) et
  `NkUIWindow::Begin`/`End`.
- **Create/Destroy** : `NkUIWindowManager` est le seul objet à durée de vie — `Init()` avant, `Destroy()`
  après (paire). Stockage **statique fixe** : max 64 fenêtres, 16 menus, 8 popups, 8 fenêtres imbriquées
  — au-delà, comportement non garanti.
- **Manager explicite** : `NkUIWindowManager` se passe à chaque appel (pas rangé dans `NkUIContext`).
- **Buffers texte** : `InputText`/`InputMultiline` écrivent dans **votre** `char* buf` de capacité
  `bufSize` — vous possédez la mémoire.
- **Pointeurs optionnels** : `scrollX`/`scrollY`, `ratio`, `open`, `selected`, `pOpen`, `valueOpts`,
  `opts` peuvent être `nullptr` (valeurs par défaut internes).
- **Signatures spéciales** : `Tooltip`/`SetTooltip` ne prennent **pas** `ls` ; `ButtonImage`,
  `InvisibleButton`, `ProgressBar`, `ProgressBarVertical`, `TreePop`, `TableNextRow`, `TableNextCell`,
  `EndTable` se passent de `font` et/ou `dl` (voir leur fiche).
- **Couleur de thème** : pour `Text`, `col = {0,0,0,0}` (alpha 0) est la sentinelle « couleur du
  thème ».
- **Mono-thread** : l'immédiat-mode est mono-thread implicite ; le rendu passe par `NkUIDrawList`.

---

### Exemple

```cpp
#include "NKUI/NkUIWidgets.h"
#include "NKUI/NkUIMenu.h"
#include "NKUI/NkUIWindow.h"
using namespace nkentseu::nkui;

NkUIWindowManager wm;
wm.Init();                                  // une fois, paire de Destroy()

// --- chaque frame ---
wm.BeginFrame(ctx);

// Barre de menus
if (NkUIMenu::BeginMenuBar(ctx, dl, font, {0, 0, screenW, 25})) {
    if (NkUIMenu::BeginMenu(ctx, dl, font, "Fichier")) {
        if (NkUIMenu::MenuItem(ctx, dl, font, "Ouvrir", "Ctrl+O")) Open();
        if (NkUIMenu::MenuItem(ctx, dl, font, "Quitter")) running = false;
        NkUIMenu::EndMenu(ctx);
    }
    NkUIMenu::EndMenuBar(ctx);
}

// Une fenêtre d'inspecteur
if (NkUIWindow::Begin(ctx, wm, dl, font, ls, "Inspecteur")) {
    NkUI::Text(ctx, ls, dl, font, "Lumière directionnelle");

    NkUI::SliderFloat(ctx, ls, dl, font, "Intensité", &light.intensity, 0.f, 10.f);
    NkUI::ColorButton(ctx, ls, dl, font, "Couleur", light.color);

    bool shadows = light.castShadows;
    if (NkUI::Checkbox(ctx, ls, dl, font, "Ombres", shadows))
        light.SetShadows(shadows);

    if (NkUI::TreeNode(ctx, ls, dl, font, "Avancé")) {
        NkUI::SliderInt(ctx, ls, dl, font, "Cascades", &light.cascades, 1, 4);
        NkUI::TreePop(ctx, ls);
    }
}
NkUIWindow::End(ctx, wm, dl, ls);

wm.EndFrame(ctx);
// --- fin de frame ---

wm.Destroy();                               // fin de vie, paire de Init()
```

---

[← Index NKUI](README.md) · [Récap NKUI](../NKUI.md) · [Couche Runtime](../README.md)
