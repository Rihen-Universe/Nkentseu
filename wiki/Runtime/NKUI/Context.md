# Le contexte, les entrées et la math UI

> Couche **Runtime** · NKUI · Le cœur **immediate-mode** : l'objet `NkUIContext` qui pilote une
> frame d'interface, l'état d'entrée `NkUIInputState` qu'on lui fournit, et le petit socle
> géométrique (`NkRect`, hachage d'ID, easing) sur lequel tout repose.

Une interface en **immediate-mode** ne ressemble pas à une interface classique. Il n'y a pas
d'arbre de widgets persistant qu'on construit une fois puis qu'on met à jour : à **chaque frame**,
on **redéclare** toute l'UI en appelant des fonctions (`Button("OK")`, `Slider(...)`), et le système
décide à la volée ce qui est survolé, cliqué, actif. Pour que cela fonctionne, il faut un endroit
où ranger le peu d'**état** qui doit survivre d'une frame à l'autre — qui est sous la souris, qui a
le focus clavier, où en est telle animation. Cet endroit, c'est le **contexte**. Tout le reste de
NKUI (widgets, fenêtres, docking, menus) n'est qu'une couche au-dessus de lui.

Cette page couvre les trois briques fondatrices : le **contexte** (`NkUIContext`), l'**entrée**
(`NkUIInputState`, ce que vous remplissez depuis votre backend clavier/souris) et la **math UI**
(rectangles, couleurs, hachage d'ID, easing). Ce n'est **pas** une page sur les widgets eux-mêmes
ni sur le rendu — ces sujets vivent ailleurs ; ici on pose les fondations.

- **Namespace** : `nkentseu::nkui`
- **Header parapluie** : `#include "NKUI/NKUI.h"`
- **Headers réels couverts** : `NKUI.h`, `NkUIExport.h`, `NkUIMath.h`, `NkUIInput.h`, `NkUIContext.h`

---

## Le contexte : `NkUIContext`

`NkUIContext` est l'objet central. Contrairement à Dear ImGui, ce **n'est pas un singleton global**
caché derrière des fonctions libres : c'est un objet **explicite et multi-instanciable** — vous le
créez, vous le passez, et vous pouvez en avoir plusieurs (un par fenêtre native, par exemple). Tout
son état est public (un seul membre privé, `mInitialized`), ce qui le rend transparent et facile à
inspecter ; en contrepartie, c'est à vous de respecter ses invariants.

Sa raison d'être est de porter l'**état transverse à une frame** : trois identifiants clés —
`hotId` (le widget survolé), `activeId` (celui en cours d'interaction : clic, drag, édition) et
`focusId` (celui qui a le focus clavier) — plus `lastId` (le dernier widget créé) et
`currentWindowId`. À cela s'ajoutent l'état d'entrée (`input`), le temps (`time`, `dt`, `frameNum`),
le thème, les *draw lists* par couche, le curseur de layout, et plusieurs piles (IDs, style,
animations).

Le rythme de vie est immuable : **`BeginFrame` → vos appels de widgets → `EndFrame`**, puis on
soumet le contexte au renderer.

```cpp
NkUIContext ctx;
ctx.Init(1280, 720);                 // une fois

// ... chaque frame :
ctx.BeginFrame(input, dt);           // input = NkUIInputState rempli par votre backend
//   ... ici les appels widgets (Button, Slider, ...) lisent/écrivent hotId/activeId ...
ctx.EndFrame();                      // finalise les DrawLists
renderer.Submit(ctx);               // (couche rendu, hors de cette page)
```

Ce n'est **pas** un conteneur de widgets : `NkUIContext` ne garde aucune liste d'objets « bouton »
ou « slot ». Il ne mémorise que des **identifiants** et des valeurs scalaires. Un widget « existe »
le temps d'un appel de fonction ; sa seule trace persistante est, éventuellement, son ID dans
`hotId`/`activeId`/`focusId` ou une entrée d'animation.

> **En résumé.** `NkUIContext` = l'objet immediate-mode, explicite et multi-instance (pas de
> singleton). Il porte l'état d'une frame : `hotId`/`activeId`/`focusId`, l'entrée, le temps, le
> thème, les *draw lists*, le layout et les piles. Cycle : `Init` une fois, puis
> `BeginFrame`/widgets/`EndFrame` à chaque frame.

---

## L'entrée : `NkUIInputState`

Le contexte ne lit **pas** le clavier ni la souris lui-même : il ne sait rien de NKEvent, SDL, GLFW
ou Win32. C'est **vous** qui traduisez les événements de votre backend dans un `NkUIInputState`, que
vous passez ensuite à `BeginFrame`. Cette séparation est délibérée : NKUI reste indépendant de la
source d'entrée, et la même UI tourne quel que soit le système de fenêtrage.

Un `NkUIInputState` décrit l'entrée d'**une seule frame** : position et boutons souris (indexés 0 à
4, soit cinq boutons), molette (verticale et horizontale), état des touches, texte saisi (codepoints
UTF-32), points tactiles (jusqu'à dix), et le `dt` de la frame. Tous ses champs sont publics.

Le piège central est l'**ordre des opérations**. On appelle d'abord `BeginFrame()` sur l'**input**
(qui recalcule le delta souris et **remet à zéro** les transitions de la frame précédente : clics,
relâchements, touches pressées, molette, texte), **puis** on injecte les événements de la frame :

```cpp
input.BeginFrame();                       // 1) reset deltas + flags transitoires
input.SetMousePos(mx, my);               // 2) puis on injecte les événements
input.SetMouseButton(0, leftDown);
input.SetKey(NkKey::NK_TAB, tabDown);
input.AddInputChar(codepoint);
input.AddMouseWheel(scrollY);

ctx.BeginFrame(input, dt);               // 3) on remet l'input au contexte
```

Si on remplit **avant** `BeginFrame()`, tout est effacé. C'est exactement l'inverse de l'intuition.

Attention aussi : `BeginFrame()` ne **consomme pas** la molette ni le texte par soustraction — il
les **remet à zéro**. Donc `AddMouseWheel` et `AddInputChar` **accumulent** sur la frame courante et
repartent de zéro à la frame suivante. Deux champs publics, `mouseDblClick` et `mouseDownDuration`,
ne sont **jamais écrits** par les méthodes du header : si vous en avez besoin, c'est à vous de les
maintenir.

Dernière subtilité, asymétrique : les **setters** bornent l'index (`SetKey` rejette les clés hors
plage, `SetMouseButton` les boutons hors `[0,5[`), mais les **getters** `IsKeyDown` / `IsKeyPressed`
**ne bornent pas** — ils indexent directement le tableau. Passez toujours une `NkKey` valide.

> **En résumé.** Vous remplissez `NkUIInputState` depuis votre backend, le contexte ne lit rien
> lui-même. Sur l'input : `BeginFrame()` (reset) **PUIS** `SetMousePos`/`SetMouseButton`/`SetKey`/
> `AddInputChar`/`AddMouseWheel`. Molette et texte **accumulent** ; `mouseDblClick`/
> `mouseDownDuration` sont à vous. Les getters de clé ne bornent pas l'index.

---

## La math UI : rectangles, ID, easing

NKUI s'appuie sur un socle géométrique minuscule mais omniprésent. Le type le plus important est
`NkRect` — qui est en réalité un **alias de `math::NkFloatRect`**, donc des champs **`.x .y .w .h`**
(origine + dimensions), **pas** un couple min/max. Deux helpers libres l'accompagnent :
`NkRectContains` (test d'inclusion **demi-ouvert** : le bord droit/bas est exclu) et
`NkRectIntersect` (intersection, dont `w`/`h` sont clampés à zéro s'il n'y a pas de recouvrement —
jamais de dimension négative).

Le second pilier est le **hachage d'ID**. En immediate-mode, chaque widget a besoin d'un identifiant
stable d'une frame à l'autre, dérivé de quelque chose qu'on connaît : son libellé, un indice de
boucle, un pointeur. Les fonctions `NkHash` (chaîne), `NkHashInt` (entier) et `NkHashPtr` (pointeur)
calculent un `NkUIID` en **FNV-1a 32 bits**, avec une garantie précieuse : le résultat n'est
**jamais zéro** (zéro est réservé à `NKUI_ID_NONE`, l'« absence d'ID »).

Enfin, un peu d'**easing** pour les animations : `NkEaseOut` et `NkEaseInOut` adoucissent un
paramètre `t ∈ [0,1]`, et le `struct NkUIAnim` (POD : `value`/`target`/`speed`) sert de support aux
animations du contexte.

```cpp
NkRect r{ 10.f, 10.f, 120.f, 32.f };          // x, y, largeur, hauteur
bool over = NkRectContains(r, mousePos);       // bord droit/bas exclu

NkUIID id = NkHash("save_button");             // ID stable, jamais 0
```

Ce n'est **pas** un module de math complet : `NkUIMath.h` ne déclare **rien** en propre, il ne fait
que relayer des includes. Toute la « math UI » (vecteurs, couleurs, rectangle, `NKUI_PI`) provient
en réalité de `NkUIExport.h` et de **NKMath** — `NkVec2`, `NkColor`, `NkRect` sont des alias des
types NKMath.

> **En résumé.** `NkRect` = un `NkFloatRect` (`.x .y .w .h`, origine+taille), inclusion
> **demi-ouverte**. Les `NkHash*` produisent des `NkUIID` FNV-1a **jamais nuls** (0 = `NKUI_ID_NONE`).
> Easing `NkEaseOut`/`NkEaseInOut` + `NkUIAnim` pour animer. `NkUIMath.h` ne déclare rien : tout vient
> de `NkUIExport.h` + NKMath.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Le détail (sémantique, pièges, cas d'usage) suit dans la
« Référence complète ».

### Socle (`NkUIExport.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Macros | `NKUI_API`, `NKUI_INLINE`, `NKUI_NODISCARD`, `NKUI_NORETURN` | Export DLL, inline forcé, `[[nodiscard]]`, `[[noreturn]]`. |
| Alias | `NkUIID` (= `uint32`) · `NKUI_ID_NONE` (= 0) | Identifiant de widget ; valeur « aucun ID ». |
| Alias | `NkVec2`, `NkColor`, `NkRect` (= `NkFloatRect`) | Re-export NKMath ; `NkRect` a `.x .y .w .h`. |
| Constante | `NKUI_PI` | π en `float32`. |
| Géométrie | `NkRectContains(r, p)` | Inclusion **demi-ouverte** (bord droit/bas exclu). |
| Géométrie | `NkRectIntersect(a, b)` | Intersection ; `w`/`h` clampés à 0 si disjoint. |

### Entrée (`NkUIInputState`, `NkUIInput.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `NkKey`, `NkMouseButton` | Re-export des enums NKEvent. |
| Cycle | `BeginFrame()` | Recalcule deltas, **reset** clics/touches/molette/texte. |
| Souris (set) | `SetMousePos`, `SetMouseButton`, `AddMouseWheel` | Injecter position, bouton (0–4), molette (**accumule**). |
| Clavier (set) | `SetKey`, `AddInputChar` | Injecter touche (synchronise ctrl/shift/alt), codepoint UTF-32. |
| Souris (get) | `IsMouseDown/Clicked/Released(b=0)` | État brut d'un bouton (garde `b<5`). |
| Clavier (get) | `IsKeyDown`, `IsKeyPressed` | État d'une touche (⚠️ **pas de borne**). |
| Champs | `mousePos`, `mouseWheel`, `keyDown[]`, `inputChars[32]`, `touches[10]`, `dt`… | État public d'une frame. |

### Contexte (`NkUIContext.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Hachage | `NkHash`, `NkHashInt`, `NkHashPtr` | `NkUIID` FNV-1a 32 bits, **jamais 0**. |
| Animation | `NkUIAnim` · `NkEaseOut` · `NkEaseInOut` | POD d'animation + courbes d'easing. |
| Override | `NkUIShapeOverrideCtx` · `NkUIShapeOverrideFn` | Contexte + type de callback de dessin custom. |
| Enums | `NkStyleVar` · `NkUIMouseCursor` | Variables de style empilables · curseur suggéré. |
| Style | `NkStyleVarEntry` | Entrée de pile de style (union float/couleur). |
| Cycle de vie | `Init`, `Destroy`, `BeginFrame`, `EndFrame` | Initialiser / détruire / borner une frame. |
| IDs | `GetID(str/int/ptr)`, `PushID`, `PopID` | Calcul et pile d'IDs imbriqués. |
| États widget | `IsHot/IsActive/IsFocused`, `SetHot/SetActive/SetFocus`, `ClearHot/ClearActive` | Lire/écrire hot/active/focus (⚠️ pas de `ClearFocus`). |
| Hit testing | `IsHovered(rect)`, `IsHoveredCircle(c, r)` | La souris est-elle dans une zone ? |
| Animations | `Animate`, `AnimateToward`, `StepAnimations` | Animer une valeur par ID. |
| Style | `PushStyleColor`, `PushStyleVar`, `PopStyle` | Empiler/dépiler des surcharges de thème. |
| Overrides | `SetShapeOverride`, `ClearShapeOverride`, `CallShapeOverride` | Dessin custom par type de widget. |
| Layout | `SetCursor`, `GetCursor`, `AdvanceCursor`, `SameLine`, `NewLine`, `Spacing` | Curseur de placement automatique. |
| Souris/consommation | `IsMouseClicked/Released`, `ConsumeMouseClick/Release` | Clic respectant la consommation. |
| Curseur/police | `SetMouseCursor`, `GetMouseCursor`, `SetActiveFont`, `GetActiveFont` | Curseur suggéré ; police active. |
| Thème | `SetTheme`, `GetTheme`, `LoadThemeJSON`, `ParseThemeJSON`, `SaveThemeJSON` | Lire/écrire/sérialiser le thème. |
| Couleurs/métriques | `GetColor`, `GetCornerRadius`, `GetItemHeight`, `GetPaddingX`, `GetPaddingY` | Couleur effective + métriques du thème. |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses cas d'usage. Les enums donnent leur **scope exact**.

### Macros et types de base (`NkUIExport.h`)

`NKUI_API` gère l'export/import DLL : `__declspec(dllexport)` si `NKUI_BUILD_DLL`,
`__declspec(dllimport)` si `NKUI_USE_DLL`, `__attribute__((visibility("default")))` sous GCC/Clang,
vide sinon. `NKUI_INLINE` force l'inlining (`__forceinline` sur MSVC, `inline
__attribute__((always_inline))` ailleurs). `NKUI_NODISCARD` et `NKUI_NORETURN` mappent
`[[nodiscard]]` et `[[noreturn]]`.

Côté types : `NkUIID` est un `uint32`, l'identifiant de chaque widget ; `NKUI_ID_NONE` vaut `0` et
signifie « aucun ID ». `NkVec2`, `NkColor`, `NkRect` sont des **alias NKMath** — et il faut bien
intégrer que `NkRect = math::NkFloatRect` : c'est un rectangle **origine + taille** (`.x .y .w .h`),
jamais un min/max. `NKUI_PI` fournit π en `float32`.

Les deux helpers géométriques sont libres, `NKUI_INLINE`, `noexcept`, et prennent leurs arguments
**par valeur** :

- `NkRectContains(r, p)` — teste si le point `p` est dans le rectangle `r`, en **demi-ouvert** :
  `p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h`. Le bord droit et le bord bas sont
  **exclus** — propriété cruciale pour que des rectangles jointifs ne se chevauchent pas sur leur
  arête commune (un pixel ne « touche » qu'un seul des deux).
- `NkRectIntersect(a, b)` — renvoie le rectangle d'intersection ; si les deux ne se recouvrent pas,
  `w` et `h` sont **clampés à `0.f`** (jamais de valeur négative). C'est la base du **clipping** :
  intersecter la zone d'un widget avec celle de son parent pour ne dessiner que le visible.

### `NkUIInputState` à fond (`NkUIInput.h`)

Cette structure est le **pont** entre votre backend d'entrée et NKUI. Les alias `NkKey` et
`NkMouseButton` re-exportent les enums de NKEvent (les API souris de NKUI utilisent en fait des
`int32` 0–4, pas `NkMouseButton`). Tous les champs sont publics : souris (`mousePos`,
`mousePosLast`, `mouseDelta`, `mouseWheel`, `mouseWheelH`, les tableaux `mouseDown/Clicked/Released/
DblClick[5]`, `mouseDownDuration[5]`), clavier (`keyDown/Pressed/Released[NK_KEY_MAX]`, plus les
booléens `ctrl/shift/alt`), texte (`inputChars[32]` en codepoints UTF-32, `numInputChars`), tactile
(`Touch touches[10]` avec `numTouches`, `Touch` étant `{ NkVec2 pos; bool down; }`) et `dt`.

Toutes les méthodes sont inline dans le header et `noexcept` :

- `BeginFrame()` — recalcule `mouseDelta` (= `mousePos − mousePosLast`), **remet à false** tous les
  flags transitoires (`mouseClicked/Released/DblClick` ×5, `keyPressed/Released` ×`NK_KEY_MAX`),
  remet `numInputChars = 0` et `mouseWheel = mouseWheelH = 0`. Il **ne touche pas** à `mouseDown`,
  `keyDown` ni `mousePosLast`. À appeler **en premier**, avant d'injecter les événements.
- `SetMousePos(x, y)` — sauve l'ancienne position dans `mousePosLast`, pose la nouvelle, recalcule le
  delta.
- `SetMouseButton(btn, down)` — vérifie `btn ∈ [0,5[` (sinon ne fait rien), détecte les transitions
  (appui → `mouseClicked`, relâche → `mouseReleased`) et met à jour `mouseDown[btn]`.
- `AddMouseWheel(dy, dx=0)` — **accumule** : `mouseWheel += dy`, `mouseWheelH += dx`. À appeler une
  fois par événement molette de la frame.
- `SetKey(key, down)` — borne stricte `k ∈ ]0, NK_KEY_MAX[` (⚠️ `k = 0` est **rejeté**), gère les
  transitions `keyPressed`/`keyReleased`, pose `keyDown[k]`, et **synchronise** automatiquement
  `ctrl`/`shift`/`alt` à partir des touches gauche/droite correspondantes — vous n'avez pas à gérer
  les modificateurs à la main.
- `AddInputChar(c)` — ajoute un codepoint dans `inputChars` si `numInputChars < 32`, **sinon ignore
  silencieusement** (le tampon de texte d'une frame est plafonné à 32 caractères).
- Getters : `IsMouseDown/Clicked/Released(b=0)` testent `b < 5 && tableau[b]` (garde le haut, **pas**
  le bas). `IsKeyDown(k)` / `IsKeyPressed(k)` indexent **directement** `keyDown[(int32)k]` /
  `keyPressed[...]` **sans aucune borne** : une clé hors plage = comportement indéfini.

**Cas d'usage par domaine** :
- **Éditeur / outils** — saisie de texte dans des champs (codepoints UTF-32 via `AddInputChar`),
  raccourcis clavier (Ctrl+Z détecté via `ctrl` synchronisé + `IsKeyPressed`).
- **Jeu (HUD/menus)** — navigation souris, molette pour faire défiler un inventaire (`AddMouseWheel`
  accumulé puis lu), clic pour valider.
- **Mobile / tactile** — jusqu'à dix doigts via `touches[10]`, à mapper sur les zones de widgets.
- **Backend-agnostique** — la même boucle remplit le même `NkUIInputState` que la source soit NKEvent,
  SDL, GLFW ou Win32.

**Pièges** : remplir **après** `BeginFrame()`, sinon tout est effacé ; molette et texte
**accumulent** (pas de soustraction) ; `mouseDblClick`/`mouseDownDuration` sont publics mais **jamais
maintenus** par le header — à votre charge ; ne passez jamais une `NkKey` hors plage aux getters.
Note historique : le commentaire du header évoque un `ctx.Feed(state)` qui **n'existe pas** — la vraie
porte d'entrée est `ctx.BeginFrame(input, dt)`.

### Hachage d'ID : `NkHash`, `NkHashInt`, `NkHashPtr`

Trois fonctions libres (`NKUI_INLINE`, `noexcept`) calculant un `NkUIID` en **FNV-1a 32 bits**
(graine par défaut `2166136261u`, multiplicateur `16777619u`) :

- `NkHash(str, seed)` — hache une chaîne C ; sert à dériver l'ID d'un widget depuis son **libellé**
  (`NkHash("Save")`).
- `NkHashInt(v, seed)` — hache les 4 octets d'un entier ; idéal pour donner un ID **stable** à chaque
  élément d'une boucle (l'indice `i`).
- `NkHashPtr(p, seed)` — hache les 32 bits bas d'un pointeur (via `NkHashInt`) ; pour identifier un
  widget par l'**objet** qu'il représente.

Tous **garantissent un résultat non nul** : si le hash tombe sur 0, ils renvoient `1u`. C'est
essentiel car `0 = NKUI_ID_NONE`. La graine sert à **combiner** des IDs (préfixe de fenêtre + libellé)
pour distinguer deux boutons « OK » dans deux panneaux différents — c'est précisément le rôle de la
pile d'ID (`PushID`/`PopID`).

### `NkUIAnim` et l'easing : `NkEaseOut`, `NkEaseInOut`

`NkUIAnim` est un POD d'animation : `id` (le widget animé), `value` (valeur courante dans `[0,1]`),
`target` (cible dans `[0,1]`), `speed` (unités/seconde, défaut `8.f`). Le contexte tient un tableau de
512 de ces animations et les fait converger.

`NkEaseOut(t)` renvoie `1 − (1 − t)²` (décélération en fin de course), `NkEaseInOut(t)` une courbe
quadratique douce aux deux bouts. On les applique à la `value` d'une animation pour des transitions
naturelles : un bouton qui s'illumine progressivement au survol, un panneau qui se déplie, un slider
dont le manche rattrape sa position.

### `NkUIShapeOverrideCtx` et `NkUIShapeOverrideFn`

Le **shape override** permet de remplacer le dessin par défaut d'un type de widget par votre propre
rendu. `NkUIShapeOverrideFn` est un **pointeur de fonction libre** `void(*)(NkUIShapeOverrideCtx&)` —
**pas** un `std::function`, donc **pas de capture** : tout contexte utilisateur passe par le champ
`userData`. Le `NkUIShapeOverrideCtx` reçu contient `dl` (la `NkUIDrawList` où dessiner), `rect` (la
zone), `state` (un `NkUIWidgetState`, type de `NkUITheme`), `theme`, `userData`, `label` (peut être
`nullptr`) et `value` (normalisé `[0,1]`, utile pour les sliders).

Cas d'usage : un thème « gaming » avec des boutons biseautés dessinés à la main, des jauges de vie
custom, des sliders en forme d'arc — tout en laissant NKUI gérer le hit-testing et la logique
immediate-mode.

### `NkStyleVar` et `NkStyleVarEntry`

`enum class NkStyleVar : uint8` énumère les variables de style **empilables**, dans l'ordre :
`NkStyleVar::NK_ITEM_SPACING`, `NK_PADDING_X`, `NK_PADDING_Y`, `NK_CORNER_RADIUS`, `NK_BORDER_WIDTH`,
`NK_ALPHA`, `NK_BUTTON_BG`, `NK_BUTTON_TEXT`, `NK_TEXT_COLOR`, et la sentinelle `NK_COUNT`. Les six
premières sont des scalaires, les trois suivantes des couleurs.

`NkStyleVarEntry` est ce que la pile de style stocke : la `var` concernée et une **union anonyme**
`prev` qui garde la valeur **précédente** — soit un `float32 f`, soit un `NkColor col` selon la
nature de la variable. C'est ce qui permet à `PopStyle` de restaurer exactement l'état d'avant.

### `NkUIMouseCursor`

`enum class NkUIMouseCursor : uint8` décrit le **curseur suggéré** par l'UI, que l'adaptateur
plateforme va appliquer. Valeurs (scope `NkUIMouseCursor::`) : `NK_ARROW = 0`, `NK_TEXT_INPUT`,
`NK_HAND`, `NK_RESIZE_NS`, `NK_RESIZE_WE`, `NK_RESIZE_NWSE`, `NK_RESIZE_NESW`. **Pas de sentinelle
`COUNT`.** Un champ de texte demandera `NK_TEXT_INPUT`, un lien `NK_HAND`, une poignée de
redimensionnement la flèche diagonale appropriée.

### `NkUIContext` : les champs

Tous publics (seul `mInitialized` est privé). Les principaux groupes :

- **IDs** : `hotId` (survolé), `activeId` (en interaction), `focusId` (focus clavier), `lastId`
  (dernier widget créé), `currentWindowId` — tous initialisés à `NKUI_ID_NONE`.
- **Polices** : `fontManager` (`NkUIFontManager` par valeur, partagé).
- **Entrées/temps** : `input` (`NkUIInputState`), `time`, `dt`, `frameNum`, les drapeaux de
  consommation `wheelConsumed`/`wheelHConsumed`, `mouseClickConsumed[5]`/`mouseReleaseConsumed[5]`, et
  `mouseCursor`.
- **Thème** : `theme` (`NkUITheme`) et `globalAlpha`.
- **DrawLists** : constantes `LAYER_BG=0`, `LAYER_WINDOWS=1`, `LAYER_POPUPS=2`, `LAYER_OVERLAY=3`,
  `LAYER_COUNT=4` ; le tableau `layers[LAYER_COUNT]` (une *draw list* par couche, pour que les popups
  passent au-dessus des fenêtres, etc.) et `dl` (pointeur vers la *draw list* courante).
- **Viewport** : `viewW=1280`, `viewH=720`.
- **Piles** : `idStack[64]`/`idDepth`, `styleStack[128]`/`styleDepth`, `anims[512]`/`numAnims`.
- **Layout** : `cursor`, `cursorStart`, `lineHeight`, `sameLine`.
- **Overrides** : `overrides[NkUIWidgetType::NK_COUNT]`, `overrideUD[...]`, `activeFont`.
- **WindowManager** : `wm` (défini par la couche fenêtre, externe).

Ces tableaux sont à **taille fixe** sans garde de débordement visible : respectez les limites (`idStack`
64, `styleStack` 128, `anims` 512). Au passage, le nombre d'animations simultanées et la profondeur
d'imbrication d'IDs/styles sont donc bornés par construction.

### Cycle de vie : `Init`, `Destroy`, `BeginFrame`, `EndFrame`

- `Init(viewportW=1280, viewportH=720, fontConfig={})` — initialise le contexte (viewport, polices).
  À appeler **une fois**.
- `Destroy()` — libère les ressources.
- `BeginFrame(input, dt)` — ouvre une frame avec l'état d'entrée fourni ; copie l'input, avance le
  temps.
- `EndFrame()` — finalise les *draw lists*, prêtes pour le renderer.

C'est l'ossature de toute boucle UI : `Init` au démarrage, `BeginFrame`/widgets/`EndFrame` chaque
frame, `Destroy` à l'arrêt.

### IDs : `GetID`, `PushID`, `PopID`

`GetID(str)` / `GetID(int)` / `GetID(ptr)` calculent l'ID d'un widget **en tenant compte de la pile
d'ID courante** (le dernier `PushID` sert de graine). `PushID(str)` / `PushID(int)` empilent un
préfixe, `PopID()` le retire. C'est le mécanisme qui résout les collisions : deux boutons de même
libellé dans deux panneaux différents obtiennent des IDs distincts si chaque panneau a fait son
`PushID`.

```cpp
for (int32 i = 0; i < count; ++i) {
    ctx.PushID(i);                 // distingue les itérations
    // ... un widget "Delete" par ligne, ID unique grâce au PushID(i)
    ctx.PopID();
}
```

### États widget : hot / active / focus

Les lecteurs `IsHot(id)` / `IsActive(id)` / `IsFocused(id)` comparent l'ID au champ correspondant ;
les mutateurs `SetHot` / `SetActive` / `SetFocus` le posent. `ClearHot()` et `ClearActive()` remettent
à `NKUI_ID_NONE`. **Il n'existe pas de `ClearFocus()`** : pour retirer le focus clavier, posez
`focusId = NKUI_ID_NONE` directement (le champ est public). Ce trio est le cœur de la logique
immediate-mode : un widget devient `hot` s'il est survolé, `active` au clic maintenu, et garde le
`focus` clavier après un clic.

### Hit testing : `IsHovered`, `IsHoveredCircle`

`IsHovered(rect)` est un raccourci de `NkRectContains(rect, input.mousePos)` : la souris est-elle dans
ce rectangle ? `IsHoveredCircle(centre, rayon)` teste l'appartenance à un **disque** (distance² ≤
rayon²) — pour des boutons ronds, des poignées circulaires, des nœuds de graphe.

### Animations : `Animate`, `AnimateToward`, `StepAnimations`

`Animate(id, target, speed=8.f)` enregistre/ajuste l'animation d'un ID vers `target` et **renvoie la
valeur courante animée** — pratique à lire directement pour piloter une couleur ou une taille.
`AnimateToward(id, target)` fixe la cible sans renvoyer. `StepAnimations()` fait avancer toutes les
animations d'un pas (selon `dt`). Usage typique : `float a = ctx.Animate(id, isHovered ? 1.f : 0.f);`
puis interpoler la couleur du widget avec `a`.

### Pile de style : `PushStyleColor`, `PushStyleVar`, `PopStyle`

`PushStyleColor(var, col)` empile une surcharge de **couleur**, `PushStyleVar(var, val)` une surcharge
**scalaire** ; chacun pousse un `NkStyleVarEntry` qui mémorise l'ancienne valeur. `PopStyle(count=1)`
dépile et restaure. C'est le moyen de modifier localement l'apparence — élargir l'espacement d'un
groupe, teinter un bouton en rouge — sans toucher le thème global :

```cpp
ctx.PushStyleColor(NkStyleVar::NK_BUTTON_BG, dangerRed);
// ... bouton "Supprimer" en rouge ...
ctx.PopStyle();                    // restaure la couleur du thème
```

### Shape overrides : `SetShapeOverride`, `ClearShapeOverride`, `CallShapeOverride`

`SetShapeOverride(type, fn, ud=nullptr)` associe un callback de dessin custom à un
`NkUIWidgetType`, avec un `userData` optionnel. `ClearShapeOverride(type)` le retire.
`CallShapeOverride(type, rect, state, label=nullptr, val=0.f)` invoque le callback s'il existe (et
renvoie `true` dans ce cas) — appelé en interne par les widgets pour laisser votre code dessiner.
Comme le callback est un pointeur de fonction sans capture, faites passer tout état par `userData`.

### Layout : `SetCursor`, `GetCursor`, `AdvanceCursor`, `SameLine`, `NewLine`, `Spacing`

NKUI place les widgets automatiquement via un **curseur**. `SetCursor(pos)` le positionne,
`GetCursor()` / `GetCursorStart()` le lisent. `AdvanceCursor(size)` avance après un widget de la
taille donnée. `SameLine(offsetX=0, spacing=-1.f)` garde le prochain widget **sur la même ligne**
(`spacing = -1` ⇒ espacement du thème), `NewLine()` passe à la ligne, `Spacing(pixels=-1.f)` insère un
blanc (`-1` ⇒ valeur par défaut). C'est le flux de mise en page « de haut en bas » classique des
panneaux d'outils, avec `SameLine` pour aligner horizontalement (un libellé suivi de son champ).

### Souris et consommation : `IsMouseClicked`, `ConsumeMouseClick`…

Ces méthodes du **contexte** diffèrent de celles de l'input brut : elles respectent les drapeaux de
**consommation**. `IsMouseClicked(button=0)` est vrai si `button ∈ [0,5[`, que `input` signale un clic
**et** qu'il n'a pas déjà été consommé (`!mouseClickConsumed[button]`) ; `IsMouseReleased` est
l'équivalent au relâchement. `ConsumeMouseClick(button=0)` **marque** le clic comme consommé et renvoie
`true` (ou `false` si pas de clic / déjà consommé) ; `ConsumeMouseRelease` de même. La consommation
empêche un clic d'être traité par **deux** widgets superposés (un bouton au-dessus d'une fenêtre). En
immediate-mode, **préférez toujours** ces méthodes aux versions brutes `input.IsMouseClicked()`.

### Curseur souris et police active

`SetMouseCursor(cursor)` pose le curseur suggéré (`mouseCursor`), `GetMouseCursor()` le lit — votre
adaptateur plateforme lira ce champ en fin de frame pour appeler l'API système. `SetActiveFont(font)`
/ `GetActiveFont()` gèrent la `NkUIFont*` courante utilisée pour le texte.

### Thème : `SetTheme`, `GetTheme`, `LoadThemeJSON`, `ParseThemeJSON`, `SaveThemeJSON`

`SetTheme(t)` / `GetTheme()` accèdent au thème en mémoire. `LoadThemeJSON(path)` charge un thème
depuis un fichier, `ParseThemeJSON(json, len)` depuis une chaîne en mémoire, `SaveThemeJSON(path)`
sérialise le thème courant — de quoi proposer des thèmes interchangeables (clair/sombre, par projet)
éditables hors code.

### Couleurs et métriques : `GetColor`, `GetCornerRadius`…

`GetColor(type, state)` renvoie la couleur **effective** d'un widget pour un état donné, **en tenant
compte de la pile de style** courante (donc différente de la couleur brute du thème si un
`PushStyleColor` est actif). `GetCornerRadius()`, `GetItemHeight()`, `GetPaddingX()`,
`GetPaddingY()` exposent les métriques du thème (`theme.metrics.*`) — utiles pour qu'un dessin custom
reste cohérent avec le reste de l'UI.

### Note de scope

`NkUIMath.h` ne déclare **rien** en propre : il ne fait que relayer `<cmath>`, `<cstring>`,
`NkUIExport.h` et `NKMath`. Toute la « math UI » vit dans `NkUIExport.h` + NKMath. De même,
`NKUI.h` est une **umbrella** sans déclaration : elle inclut les sous-headers (Input, Theme, DrawList,
Context, Renderer, Layout, Font, Widgets, Window, Dock, Menu, Animation…). Les types `NkUIWidgetType`
et `NkUIWidgetState` (référencés par les overrides et `GetColor`) sont déclarés dans `NkUITheme.h`,
hors de cette page.

---

### Exemple

```cpp
#include "NKUI/NKUI.h"
using namespace nkentseu::nkui;

NkUIContext ctx;
ctx.Init(1280, 720);                       // une fois

NkUIInputState input;

// --- chaque frame ---
input.BeginFrame();                        // 1) reset deltas + transitions
input.SetMousePos(mouseX, mouseY);        // 2) injecter les événements backend
input.SetMouseButton(0, leftButtonDown);
input.AddMouseWheel(scrollDelta);
input.SetKey(NkKey::NK_TAB, tabDown);      // synchronise ctrl/shift/alt tout seul

ctx.BeginFrame(input, dt);                 // 3) ouvrir la frame UI

NkRect r{ 20.f, 20.f, 120.f, 32.f };       // x, y, w, h (NkRect = NkFloatRect)
NkUIID id = ctx.GetID("ok_button");        // ID stable, jamais 0

if (ctx.IsHovered(r)) {
    ctx.SetHot(id);
    ctx.SetMouseCursor(NkUIMouseCursor::NK_HAND);
    float glow = ctx.Animate(id, 1.f);     // anime le survol, lit la valeur
    (void)glow;                            // ... utilisé pour teinter le bouton
    if (ctx.ConsumeMouseClick(0)) {        // respecte la consommation
        // action du bouton
    }
}

ctx.EndFrame();
// renderer.Submit(ctx);  (couche rendu, hors de cette page)
```

---

[← Index NKUI](README.md) · [Récap NKUI](../NKUI.md) · [Couche Runtime](../README.md)
