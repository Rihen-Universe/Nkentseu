# Document 7 — Le vocabulaire des rôles `.nkgui`

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Ouvert le 21/08/2026 comme proposition ; **adopté** par le validateur
> (`NkGuiValidate.h` valide contre ce vocabulaire depuis la 0.3, doc 9 §8.1).
> **Réécrit le 27/09/2026** : réconcilié mot pour mot avec le catalogue du
> validateur (41 rôles), avec l'état de montage de chacun, et complété des **rôles
> proposés** par les spécifications de la famille (§3.10).
>
> Les numéros de section sont gardés : le code cite §3, §3.6, §3.7, §4, §5.
> En cas de désaccord : **le validateur fait foi pour ce qui est implémenté**, ce
> document pour ce qui est proposé ; un écart entre les deux est un défaut à corriger
> des deux côtés, pas une préférence.

---

## 1. Pourquoi un vocabulaire à nous

L'ancienne table du doc 2 §8 était un **miroir de l'API** (`Button()`, `BeginCombo()`,
`ColorEdit4()`) : elle couplait le format au code. *Un `.nkgui` doit s'ouvrir en 2030
même si le moteur a tout renommé en 2028.* Le vocabulaire est un **contrat**, pas un
reflet ; il sert aussi l'export, le corpus de l'IA, et la règle de NKGui : des noms
100 % Nkentseu.

---

## 2. Les règles de nommage

| # | règle | exemple rejeté |
|---|---|---|
| R1 | anglais, `PascalCase` ; **le fichier est anglais, l'interface française** | `Bouton` |
| R2 | pas de `Begin` / `End` | `BeginListBox` |
| R3 | pas de type ni d'arité dans le nom | `SliderFloat`, `DragInt`, `ColorEdit4`, `CheckBox3` |
| R4 | pas de suffixe de commodité : une variante est une **propriété** | `ButtonEx`, `TabBarEditable` |
| R5 | nommer **ce que c'est** | `DockSpaceOverViewport` |
| R6 🆕 | un rôle **décrit une forme d'interaction**, pas un usage : l'usage est l'**identifiant** (le nom de l'action) | `SaveButton` |

---

## 3. Le vocabulaire

Les propriétés sont celles du **validateur**, avec leur type (`s` chaîne · `b`
booléen · `n` nombre · `v` Vec2 · `c` couleur ou jeton · `e` énumération · `i`
drapeaux · `l` liste · `r` liaison · `a` quelconque). **Monté** : ✅ posé par
`NkGuiMonteur` · ◐ lu, validé, compté, non posé.

### 3.1 Actions

| rôle | propriétés | monté | remplace |
|---|---|---|---|
| `Button` | `label:s icon:e flags:i repeat:b repeatDelay:n repeatRate:n` | ✅ | `Button`, `ButtonEx`, `RepeatButton` |
| `ImageButton` | `image:e source:e size:a tint:c` | ◐ | `ImageButton` |
| `MenuItem` | `label:s shortcut:s checked:b` | ✅ dans un `Menu` | `MenuItem` |

⚠️ `RepeatButton` est **alias** de `Button { repeat = true }` pour le validateur ; le
monteur le connaît encore comme rôle propre (doc 2 §8.2). NKUIDesign écrit la forme
canonique.

### 3.2 Saisie

| rôle | propriétés | monté | remplace |
|---|---|---|---|
| `TextField` | `bind:r multiline:b secret:b maxChars:n wrap:b placeholder:s flags:i valueType:e` | ✅ | `InputText`, `InputTextEx`, `InputTextMultiline` |
| `NumberField` | `bind:r valueType:e step:n min:n max:n` | ◐ | `InputInt`, `InputFloat` |
| `Slider` | `bind:r valueType:e min:n max:n step:n` | ✅ | `SliderFloat` |
| `Drag` | `bind:r valueType:e speed:n min:n max:n dir:e` | ◐ | `DragFloat`, `DragInt` |
| `ColorField` | `bind:r alpha:b mode:e(Button, Field, Picker)` | ◐ | `ColorEdit4`, `ColorPicker4`, `ColorButton` |

`secret = true` est le champ de mot de passe (il manquait au doc 3 §14ter.4).

### 3.3 Booléens et sélection

| rôle | propriétés | monté | remplace |
|---|---|---|---|
| `Checkbox` | `bind:r tristate:b label:s` | ✅ | `Checkbox`, `CheckboxTristate`, `CheckBox3` |
| `Switch` | `bind:r label:s` | ◐ | — (nouveau) |
| `RadioGroup` | `bind:r options:l orientation:e` | ◐ | — (nouveau) ; l'exporter vers un moteur sans exclusivité **échoue bruyamment** |
| `Dropdown` | `bind:r items:l editable:b` | ✅ | `Combo` |
| `ListBox` | `bind:r items:l multiSelect:b` | ✅ | `BeginListBox` |
| `Item` | `label:s selected:b editable:b text:s` | ✅ | `Selectable`, `SelectableEditable`, `SelectItem` |
| `TreeItem` | `label:s expanded:b editable:b` | ✅ | `TreeNode`, `TreeNodeEditable` |

### 3.4 Texte et affichage

| rôle | propriétés | monté | remplace |
|---|---|---|---|
| `Text` | `text:s wrap:b align:e maxLines:n overflow:e for:s` | ✅ | — (décidé le 21/08 ; **pas de `Label`**, qui doublerait la propriété `label`) |
| `Image` | `source:e size:a tint:c uv0:v uv1:v texId:a` | ✅ | `Image` |
| `Progress` | `bind:r overlay:s indeterminate:b` | ✅ | `ProgressBar` |
| `Chart` | `values:l kind:e(Line, Histogram) min:n max:n height:n` | ✅ | `PlotLines`, `PlotHistogram` |
| `Separator` | `orientation:e` | ✅ | `Separator` |
| `Spacer` | `size:a` | ✅ | — (décidé le 21/08) |

### 3.5 Navigation

| rôle | propriétés | monté | remplace |
|---|---|---|---|
| `TabBar` | `tabs:l bind:r editable:b closable:b` | ✅ | `TabBar`, `TabBarEx`, `TabBarEditable` |
| `MenuBar` | — | ✅ | `BeginMenuBar` |
| `Menu` | `label:s` | ✅ | `BeginMenu` |
| `ContextMenu` | — | ◐ | `BeginPopupMenu` |
| `Expander` | `label:s expanded:b` | ✅ | `CollapsingHeader` |
| `DockSpace` | `overViewport:b topMargin:n` | ✅ | `DockSpace`, `DockSpaceOverViewport` |

### 3.6 Conteneurs

| rôle | propriétés | monté | note |
|---|---|---|---|
| `Window` | `title:s flags:i modal:b placement:e` | ✅ | `modal` est une propriété, pas un rôle |
| `Panel` | `title:s placement:e` | ✅ | |
| `Group` | `placement:e` | ✅ | |
| `VBox` · `HBox` | `gap:n align:e justify:e` | ✅ | |
| `Row` · `Column` | idem | ✅ | synonymes de `HBox` · `VBox`, gardés parce que le corpus les emploie |
| `Grid` | `columns:n gap:n sizes:l` | ✅ | `sizes` compté, non honoré (NKGui ne prend que le nombre de colonnes) |
| `Flow` | `gap:n` | ✅ | |
| `Stack` | `anchor:e` | ◐ | |
| `Table` | `columns:l flags:i` | ◐ | |
| `Scroll` | `axis:e always:b` | ✅ | était un drapeau de `BeginChild` ; c'est un conteneur explicite |
| `Splitter` | `bind:r min:n max:n orientation:e ratio:n` | ✅ | `ratio` (17/09) : la position **par défaut** ; le geste de l'utilisateur n'est jamais réécrit dans le fichier |

`placement = absolute` n'existe que sur les trois conteneurs **neutres** (`Window`,
`Panel`, `Group`) : les autres **nomment** leur agencement (doc 2 §4.3).

### 3.7 `Host` — la zone que l'application remplit

`Host { hint:s }` — ✅ monté. Un **rectangle** que l'application remplit : le viseur 3D
de NKCraft, la toile de NKUIDesign, l'éditeur de NKCode. *Le document dit OÙ et QUOI ;
l'application garde QUAND et COMMENT.*

- **Ce n'est pas `Callback`** : ce mot appartient déjà deux fois au format (la section
  `callback`, et l'appel `Callback "nom"(…)` d'un comportement). Un rôle du même nom a
  été **retiré** du monteur, pas légalisé.
- Non rempli : dessiné **hachuré** avec son nom (ou `hint`), compté `hotesNonRemplis`.
- Hôtes d'extension des panneaux communs : doc 2 §8.3.

### 3.8 Les propriétés universelles

`tooltip:s enabled:b visible:b id:s pos:v size:v sizeRel:v minSize:v maxSize:v` —
✅ ; `hitShape:b` 🟢 (0.4) ; `shortcut:s` proposé (P8, §3.10).

- `pos` **est l'interrupteur** du placement ; `size` seul reste le `size` du rôle
  (`Spacer.size` est un nombre).
- `sizeRel` = fraction du parent par axe ; `minSize` / `maxSize` bornent en pixels ;
  **strictement additif** (une composante `≤ 0` = axe non contraint ; `sizeRel` gagne
  sur son axe, puis les bornes).
- **Preuve** (`valides/13_tailles_relatives.nkgui`) : à 1200×800, gauche 192 px (16 %)
  et droite 348 px (29 %) ; à 800×600, gauche **180** px (le plancher relève 128 → 180)
  et droite 232 px ; un `pos`+`size` reste 200 px dans les deux fenêtres.
- **Limite nommée** : conteneurs et `Host` seulement ; une feuille dans un flux garde
  sa taille naturelle.

### 3.9 `DockSpace` — qui dit quoi

| qui | dit |
|---|---|
| le **document** | quelles zones existent, leur ordre, leurs proportions par défaut — zones désignées par l'**identifiant** du panneau (`hierarchie`), jamais par son libellé |
| la **coquille** (`NkEditorShell::LoadUiState`) | l'arbre vivant (`dockroot=`, `node=…`, `nwin=`, `float=`) — jamais réécrit dans le document |
| le **monteur** | demande chaque zone par `NkGuiMonteHooks::ZoneAncree(nom, rect)` |

- Zone nommée non fournie → hachurée avec son nom (`NKGuiMonteTest` m12).
- Panneau fourni non nommé → non touché, le crochet n'est pas appelé pour lui.
- Disposition enregistrée à laquelle manque un panneau → panneau **fermé** (politique
  inverse de celle du document, et voulue : c'est l'utilisateur qui l'a fermé).
- Largeur : la fraction de la zone, sinon le **reste** partagé (pas des parts égales,
  qui laissaient une bande orpheline de 197 px).

### 3.10 Les rôles proposés 🆕

Les spécifications des applications de la famille (NkAnima 09, NKScena 03, Nogee 06,
NKCraft 03, PV3DE 03, NkAntenne 03) ont **proposé** ces rôles, avec l'accord de Rodolf
(*« si tu juges que des rôles manquent, propose-les, mais décris-les »*). **Aucun
n'est au validateur** : un document écrit leur **doublure** (colonne « en attendant »)
et un commentaire `// P4 Tile` pour permettre le remplacement mécanique. NKUIDesign
les pose depuis « Poser un rôle proposé » et les dessine avec un badge « proposé »
(doc 3 §14ter).

Statut : **proposé** = décrit, pas tranché. Les propositions P1, P24 à P27 sont des
**extensions du format**, pas des rôles : elles sont **décidées** (27/09) et décrites
au doc 2 (§20, §15, §5.4, §18, §19).

| # | rôle | propriétés | événements | pourquoi aucun rôle existant ne suffit | en attendant | vient de |
|---|---|---|---|---|---|---|
| P2 | `ToggleButton` | `label icon bind group` (même groupe = exclusifs) | Changed | un outil actif parmi plusieurs : `Checkbox` n'a pas la forme d'un bouton, `Button` n'a pas d'état | `Button` + `appearance` d'actif (compté) | NkAnima |
| P3 | `Badge` | `text` (1–3 car. ou nombre), `tone` (nom de jeton) | — | pastille non interactive ; `Text` n'a pas de fond peint | `Text` + `text { color }` | NkAnima |
| P4 | `Tile` | `image label caption size` | Click, Hover, DragStart | vignette cliquable avec légende ; `ImageButton` n'en a pas et n'est pas monté | `Button` (`label`, `tooltip` = légende) | NkAnima |
| P5 | `VectorField` | `bind components(2–4) speed min max axisColors` | Changed | le champ X/Y/Z à liseré d'axe du panneau Détails ; la couleur porte l'axe | composant de trois `Slider` | NkAnima |
| P6 | `KeyDiamond` | `bind` (chemin animable) | Click → `commun.cle_propriete` | trois états (vide, plein, demi) pour un geste | `Button` icône losange | NkAnima |
| P7 | `CurveField` | `bind min max height xLabel yLabel` | Changed | courbe éditable **en place** ; `Chart` n'est pas éditable | `Host` + `hint` | NkAnima |
| P8 | `shortcut` universel | `shortcut:s` sur tout rôle interactif — **affichage seulement** | — | le raccourci se lit au même endroit qu'au menu | dans le `tooltip` : `"Déplacer (W)"` | NkAnima |
| P9 | `Slot` dans un composant | `Slot "nom"` | — | une ligne de propriété dont la **valeur** change de rôle | un composant par type de valeur | NkAnima |
| P10 | section `layout` | `layout "nom" { dock "id" left 0.16 … }` | — | un espace de travail **est** une disposition | section écrite, disposition posée par l'application | NkAnima |
| P11 | `pattern = Hatch` | dans `appearance` | — | une proposition non appliquée doit se voir sans la couleur (daltonisme) | écrit, compté | NkAnima |
| P12 | `icon` sur `TextField` | `icon:e` | — | la loupe d'un champ de recherche | `placeholder` « Rechercher… » | NkAnima |
| P13 | `TimecodeField` | `bind fps format(Timecode, Images, Secondes)` | Changed, Submit | saisie `+1:00`, `3-040` ; affichage HH:MM:SS:II | `TextField` + `placeholder` | NKScena |
| P14 | `Meter` | `bind peak min max channels orientation` | — | échelle en dB, zones, crête, voyant d'écrêtage | `Progress` + `Badge` | NKScena |
| P15 | `ColorWheel` | `bind luminance label` | Changed, DoubleClick | roue de décalage d'étalonnage autour du neutre | `Host` + `Slider` | NKScena |
| P16 | `AssetField` | `bind type allowNone` | Changed, Drop, DoubleClick | vignette, glisser-déposer typé, prendre la sélection, ouvrir | composant `ChampAsset` | Nogee |
| P17 | `KeyCaptureField` | `bind` | Changed | capturer une touche au lieu de la taper ; conflit signalé | `TextField` + `placeholder` | Nogee |
| P18 | `SplitButton` | `label icon` + enfants `MenuItem` | Click | ▶ Jouer ▾ : la flèche règle le bouton | `HBox` de deux `Button` + `ContextMenu` | Nogee |
| P19 | `PieMenu` | `label` + 2 à 8 `MenuItem` (ordre : droite, gauche, bas, haut, diagonales) | — | un geste directionnel mémorisable | `ContextMenu` | NKCraft |
| P20 | groupe **combinable** de `ToggleButton` | `group combine: Shift` | Changed | sommet / arête / face : clic remplace, Maj+clic ajoute | trois `ToggleButton` sans groupe | NKCraft |
| P21 | `HoldButton` | `key` | Press, Release | « maintenir pour parler » | `ToggleButton` | PV3DE |
| P22 | `Stepper` | `steps bind states(Done, Current, Todo, Hidden) free` | Changed | étapes faites / à venir / cachées | `TabBar` | PV3DE |
| P23 | `TokenField` | `bind families allowFree` | Changed | choisir **un jeton** du thème (nom + sens), pas une valeur | `Dropdown` de jetons + `ColorField` | NKUIDesign |
| P28 | `Drawer` | `edge(Left, Right, Bottom) size modal bind` | Changed | panneau qui glisse depuis un bord et se referme au clic extérieur | `Panel` absolu + `visible` lié | NkAntenne |

📌 **Priorité d'implémentation recommandée**, par nombre de spécifications qui les
emploient (relevé du 27/09 sur les sept documents « agent » : NkAnima, NKScena, Nogee,
NKCraft, PV3DE, NkAntenne, NKUIDesign) : P2 `ToggleButton` (7) · P3 `Badge` (7) ·
P4 `Tile` (7) · P6 `KeyDiamond` (4) · P5 `VectorField` (3) · P14 `Meter` (3) ·
P22 `Stepper` (2) ; puis P23 `TokenField`, dont NKUIDesign a besoin pour son propre
inspecteur. Les autres suivent la demande.

---

## 4. Ce qui n'est pas un rôle

| notion | ce qu'elle est | où |
|---|---|---|
| infobulle | une **propriété** (`tooltip`) | doc 3 §14quinquies |
| glisser-déposer | une **capacité** (événements `DragStart` / `Drop`) | doc 3 §14ter.4 |
| modalité | une **propriété** de `Window` (`modal`) ou un **type d'écran** (`Dialogue`) | doc 2 §18 |
| dialogue de fichier natif | un **service** (`call "fichier.ouvrir"()`) | doc 2 §5.6 |
| actif / sélectionné | une **donnée** (`checked`, `selected`), pas un état d'apparence | doc 2 §13.2 |
| thème | une **section** (`theme`) et des jetons | doc 2 §20 |

---

## 5. Versions et alias

Un rôle renommé garde son ancien nom comme **alias**, avec la version du changement :
le lecteur accepte les deux, le validateur signale l'ancien (`W-ROLE-ALIAS`),
l'écrivain de NKUIDesign émet le nouveau **seulement sur demande** (« Moderniser le
document ») — un fichier écrit par quelqu'un d'autre garde ses mots.

| alias | rôle | depuis |
|---|---|---|
| `RepeatButton`, `ButtonEx` | `Button` (`repeat = true` pour le premier) | 0.3 |
| `InputText`, `InputTextEx`, `InputTextMultiline` | `TextField` | 0.3 |
| `InputInt`, `InputFloat` | `NumberField` | 0.3 |
| `SliderFloat` | `Slider` | 0.3 |
| `DragFloat`, `DragInt` | `Drag` | 0.3 |
| `ColorEdit4`, `ColorPicker4`, `ColorButton` | `ColorField` | 0.3 |
| `CheckboxTristate`, `CheckBox3` | `Checkbox` | 0.3 |
| `Combo` | `Dropdown` | 0.3 |
| `BeginListBox` | `ListBox` | 0.3 |
| `CollapsingHeader` | `Expander` | 0.3 |
| `Selectable`, `SelectableEditable`, `SelectItem` | `Item` | 0.3 |
| `TreeNode`, `TreeNodeEditable` | `TreeItem` | 0.3 |
| `ProgressBar` | `Progress` | 0.3 |
| `PlotLines`, `PlotHistogram` | `Chart` | 0.3 |
| `TabBarEx`, `TabBarEditable` | `TabBar` | 0.3 |
| `DockSpaceOverViewport` | `DockSpace` | 0.3 |

Quand un rôle proposé (§3.10) entrera au vocabulaire, sa doublure **ne devient pas**
un alias : c'est une composition, pas un nom. NKUIDesign propose le remplacement
(« 14 `Button` marqués `// P2 ToggleButton` peuvent devenir des `ToggleButton` »).

---

## 6. L'export

`.nkgui` → vocabulaire NkUI (le pivot, et le seul chemin) → cible (NKGui, HTML,
code). Chaque traduction **déclare** ce qu'elle ne sait pas rendre, avec un diagnostic
nommé et **aucune approximation silencieuse**.

---

## 7. Questions — état au 27/09

| # | question | état |
|---|---|---|
| 1 | adopter ce vocabulaire au doc 2 | ✅ fait : le doc 2 réécrit y renvoie (§8) |
| 2 | les noms sont-ils les bons | ✅ en usage dans tout le corpus et les six applications |
| 3 | `Switch` et `RadioGroup` entrent-ils | ✅ au vocabulaire ; **montage** à faire (◐) — `RadioGroup` attend la primitive d'**exclusivité** de NKGui, qui débloque aussi `ToggleButton` en groupe (P2) |
| 4 🆕 | l'ordre d'entrée des rôles proposés | 🔴 recommandation au §3.10 |
| 5 🆕 | `Host` : garder ce nom | 🔴 ouvert depuis le 17/09 ; renommer ne coûterait qu'un alias |
